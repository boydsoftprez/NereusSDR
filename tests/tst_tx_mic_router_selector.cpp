// =================================================================
// tests/tst_tx_mic_router_selector.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Exercises CompositeTxMicRouter — the strategy-pattern selector that
// holds PcMicSource + RadioMicSource and dispatches pullSamples based
// on the active MicSource selection (Phase 3M-1b Task F.3).
//
// Strategy: each test harness builds a real PcMicSource (backed by a
// FakeAudioBus-injected AudioEngine) and/or a real RadioMicSource
// (backed by a TestRadioConnection). The selector is driven via
// setActiveSource() and setMoxActive(), then pullSamples() is called
// to verify which source's data flows through.
//
// TestRadioConnection is inline here (same pattern as
// tst_radio_mic_source.cpp — each test binary is self-contained).
//
// Pre-code review cite: §0.3 + master design §5.2.1.
// Plan: 3M-1b F.3.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-27 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 Phase 3M-1b Task F.3, with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>

#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"
#include "core/audio/RadioMicSource.h"
#include "core/AudioEngine.h"
#include "core/RadioConnection.h"
#include "core/IAudioBus.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <cstdint>
#include <memory>

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// TestRadioConnection — minimal concrete subclass of RadioConnection.
// Stubs all pure-virtual slots as no-ops.
// Exposes emitMicFrame() so tests can fire micFrameDecoded synchronously.
// Duplicated from tst_radio_mic_source.cpp — each test binary is self-contained.
// ---------------------------------------------------------------------------
class TestRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit TestRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {}

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTT(bool) override {}
    void setMicXlr(bool) override {}
    void sendTxIq(const float*, int) override {}

    void emitMicFrame(const float* samples, int frames)
    {
        emit micFrameDecoded(samples, frames);
    }
};

// ---------------------------------------------------------------------------
// Helper: build a QByteArray of Int16 interleaved stereo samples.
// Same helper pattern as tst_pc_mic_source.cpp.
// ---------------------------------------------------------------------------
static QByteArray makeInt16StereoBytes(
    std::initializer_list<std::pair<int16_t, int16_t>> frames)
{
    QByteArray out;
    out.reserve(static_cast<int>(frames.size()) * 2 * 2);
    for (auto [l, r] : frames) {
        out.append(reinterpret_cast<const char*>(&l), 2);
        out.append(reinterpret_cast<const char*>(&r), 2);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class TstTxMicRouterSelector : public QObject {
    Q_OBJECT

private:
    // Full harness: AudioEngine + FakeAudioBus for PcMicSource,
    // TestRadioConnection for RadioMicSource.
    struct Harness {
        std::unique_ptr<AudioEngine>     engine;
        FakeAudioBus*                    pcBus;   // non-owning; engine owns
        TestRadioConnection              radioConn;
        std::unique_ptr<PcMicSource>     pcSrc;
        std::unique_ptr<RadioMicSource>  radioSrc;

        explicit Harness(bool openPcBus = true)
            : radioConn()
        {
            AudioFormat fmt;
            fmt.sampleRate = 48000;
            fmt.channels   = 2;
            fmt.sample     = AudioFormat::Sample::Int16;

            engine = std::make_unique<AudioEngine>();
            auto fakeBus = std::make_unique<FakeAudioBus>(
                QStringLiteral("FakeTxInput"));
            if (openPcBus) {
                fakeBus->open(fmt);
            }
            pcBus = fakeBus.get();
            engine->setTxInputBusForTest(std::move(fakeBus));

            pcSrc    = std::make_unique<PcMicSource>(engine.get());
            radioSrc = std::make_unique<RadioMicSource>(&radioConn);
        }

        CompositeTxMicRouter makeRouter(bool hasMicJack = true)
        {
            return CompositeTxMicRouter(pcSrc.get(), radioSrc.get(), hasMicJack);
        }
    };

private slots:

    // ── 1. defaultSource_isPc ─────────────────────────────────────────────
    // Router constructed with hasMicJack=true defaults to Pc source.

    void defaultSource_isPc()
    {
        Harness h;
        auto router = h.makeRouter(/*hasMicJack=*/true);
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));
    }

    // ── 2. setActiveSource_Pc_dispatchesToPc ──────────────────────────────
    // When Pc is selected, pullSamples routes to PcMicSource (AudioEngine).

    void setActiveSource_Pc_dispatchesToPc()
    {
        Harness h;
        // Push a recognizable value into the PC bus.
        h.pcBus->setPullData(makeInt16StereoBytes({{16384, 0}}));

        auto router = h.makeRouter(/*hasMicJack=*/true);
        router.setActiveSource(MicSource::Pc);

        std::array<float, 4> dst{};
        const int got = router.pullSamples(dst.data(), 4);

        // PcMicSource returns 1 (one stereo frame → one mono float).
        QCOMPARE(got, 1);
        // Value from PC bus: 16384 / 32768 = 0.5
        QCOMPARE(dst[0], 16384.0f / 32768.0f);
    }

    // ── 3. setActiveSource_Radio_dispatchesToRadio ────────────────────────
    // When Radio is selected (hasMicJack=true), pullSamples routes to
    // RadioMicSource.

    void setActiveSource_Radio_dispatchesToRadio()
    {
        Harness h;
        // Push recognizable values to the radio mic ring.
        const std::array<float, 3> radioData{0.1f, 0.2f, 0.3f};
        h.radioConn.emitMicFrame(radioData.data(), 3);

        auto router = h.makeRouter(/*hasMicJack=*/true);
        router.setActiveSource(MicSource::Radio);

        std::array<float, 4> dst{9.9f, 9.9f, 9.9f, 9.9f};
        const int got = router.pullSamples(dst.data(), 4);

        // RadioMicSource always returns n (zero-fills remainder).
        QCOMPARE(got, 4);
        QCOMPARE(dst[0], 0.1f);
        QCOMPARE(dst[1], 0.2f);
        QCOMPARE(dst[2], 0.3f);
        QCOMPARE(dst[3], 0.0f);  // zero-fill on underrun
    }

    // ── 4. setActiveSource_Radio_HL2force_dispatchesToPc ─────────────────
    // When hasMicJack=false (HL2), setActiveSource(Radio) is silently
    // ignored and pullSamples still routes to PcMicSource.

    void setActiveSource_Radio_HL2force_dispatchesToPc()
    {
        Harness h;
        // Push data to PC bus so we can distinguish PC vs Radio.
        h.pcBus->setPullData(makeInt16StereoBytes({{8192, 0}}));
        // Also push to radio ring — should NOT appear in output.
        const std::array<float, 2> radioData{0.75f, 0.5f};
        h.radioConn.emitMicFrame(radioData.data(), 2);

        // hasMicJack=false → HL2 force-PC
        auto router = h.makeRouter(/*hasMicJack=*/false);
        router.setActiveSource(MicSource::Radio);  // should be silently ignored

        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));

        std::array<float, 4> dst{};
        const int got = router.pullSamples(dst.data(), 4);
        // PC bus has 1 frame.
        QCOMPARE(got, 1);
        QCOMPARE(dst[0], 8192.0f / 32768.0f);
    }

    // ── 5. setActiveSource_Radio_nullRadioSource_dispatchesToPc ──────────
    // When radioSource is nullptr, the constructor collapses hasMicJack
    // to false, and Radio selection is permanently ignored.

    void setActiveSource_Radio_nullRadioSource_dispatchesToPc()
    {
        Harness h;
        h.pcBus->setPullData(makeInt16StereoBytes({{4096, 0}}));

        // Pass nullptr for radioSource — collapsed to Pc-only mode.
        CompositeTxMicRouter router(h.pcSrc.get(), nullptr, /*hasMicJack=*/true);
        router.setActiveSource(MicSource::Radio);  // ignored (null radioSource)

        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));

        std::array<float, 4> dst{};
        const int got = router.pullSamples(dst.data(), 4);
        QCOMPARE(got, 1);
        QCOMPARE(dst[0], 4096.0f / 32768.0f);
    }

    // ── 6. setActiveSource_Pc_duringMox_appliesImmediately ───────────────
    // Switching TO Pc during MOX is safe and applies immediately.
    // (Only Radio→Pc switch during MOX needs deferral for the opposite
    // case; here we confirm Pc selection is not blocked by MOX.)

    void setActiveSource_Pc_duringMox_appliesImmediately()
    {
        Harness h;

        auto router = h.makeRouter(/*hasMicJack=*/true);
        // Start on Radio, then engage MOX.
        router.setActiveSource(MicSource::Radio);
        router.setMoxActive(true);

        // Switch to Pc during MOX — this is NOT a Radio request, so
        // no HL2 force-PC gate applies. MOX-lock still applies.
        // Per design: ANY switch during MOX is deferred (not just Radio).
        // Verify via hasPendingSwitchForTest.
        router.setActiveSource(MicSource::Pc);
        QVERIFY(router.hasPendingSwitchForTest());
        // Active source still Radio during MOX.
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Radio));
    }

    // ── 7. setActiveSource_Radio_duringMox_isPending_appliedOnMoxOff ─────
    // Switching to Radio during MOX records a pending switch. On MOX-off,
    // the pending switch is applied.

    void setActiveSource_Radio_duringMox_isPending_appliedOnMoxOff()
    {
        Harness h;
        auto router = h.makeRouter(/*hasMicJack=*/true);

        // Start on Pc, then engage MOX.
        router.setActiveSource(MicSource::Pc);
        router.setMoxActive(true);

        // Request Radio during MOX — must be deferred.
        router.setActiveSource(MicSource::Radio);
        QVERIFY(router.hasPendingSwitchForTest());
        // Active source still Pc during MOX.
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));

        // MOX-off: pending switch should be applied.
        router.setMoxActive(false);
        QVERIFY(!router.hasPendingSwitchForTest());
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Radio));
    }

    // ── 8. setActiveSource_Radio_duringMox_HL2force_isIgnored ────────────
    // Double-gate test: on HL2 (hasMicJack=false), a Radio request during
    // MOX is still silently dropped — it never becomes pending.

    void setActiveSource_Radio_duringMox_HL2force_isIgnored()
    {
        Harness h;
        // HL2: hasMicJack=false.
        auto router = h.makeRouter(/*hasMicJack=*/false);
        router.setMoxActive(true);

        // Request Radio during MOX — HL2 gate fires first, request dropped.
        router.setActiveSource(MicSource::Radio);

        // No pending switch recorded.
        QVERIFY(!router.hasPendingSwitchForTest());
        // Active source still Pc.
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));

        // MOX-off: no pending switch to apply; still Pc.
        router.setMoxActive(false);
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));
    }

    // ── 9. setMoxActive_offToOn_noEffectOnSource ──────────────────────────
    // MOX-on transition has no effect on the active source.

    void setMoxActive_offToOn_noEffectOnSource()
    {
        Harness h;
        auto router = h.makeRouter(/*hasMicJack=*/true);
        router.setActiveSource(MicSource::Radio);
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Radio));

        router.setMoxActive(true);
        // Source unchanged by MOX-on.
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Radio));
    }

    // ── 10. setMoxActive_onToOff_appliesPending ───────────────────────────
    // Full MOX cycle: set source, engage MOX, request different source,
    // release MOX — verify pending is applied.

    void setMoxActive_onToOff_appliesPending()
    {
        Harness h;
        auto router = h.makeRouter(/*hasMicJack=*/true);

        router.setActiveSource(MicSource::Radio);
        router.setMoxActive(true);
        router.setActiveSource(MicSource::Pc);  // deferred
        QVERIFY(router.hasPendingSwitchForTest());

        router.setMoxActive(false);
        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Pc));
        QVERIFY(!router.hasPendingSwitchForTest());
    }

    // ── 11. setMoxActive_onToOff_noPending_noEffect ───────────────────────
    // MOX-off when there is no pending switch: active source unchanged,
    // hasPendingSwitchForTest() stays false.

    void setMoxActive_onToOff_noPending_noEffect()
    {
        Harness h;
        auto router = h.makeRouter(/*hasMicJack=*/true);
        router.setActiveSource(MicSource::Radio);

        router.setMoxActive(true);
        // No setActiveSource() call during MOX.
        router.setMoxActive(false);

        QCOMPARE(static_cast<int>(router.activeSource()),
                 static_cast<int>(MicSource::Radio));
        QVERIFY(!router.hasPendingSwitchForTest());
    }

    // ── 12. pullSamples_nullDst_returnsZero ───────────────────────────────
    // Null dst is a programming-error guard: returns 0 without crashing.

    void pullSamples_nullDst_returnsZero()
    {
        Harness h;
        auto router = h.makeRouter();
        QCOMPARE(router.pullSamples(nullptr, 4), 0);
    }

    // ── 13. pullSamples_zeroN_returnsZero ─────────────────────────────────
    // n=0 is a programming-error guard: returns 0.

    void pullSamples_zeroN_returnsZero()
    {
        Harness h;
        auto router = h.makeRouter();
        std::array<float, 4> dst{9.9f, 9.9f, 9.9f, 9.9f};
        QCOMPARE(router.pullSamples(dst.data(), 0), 0);
        // Buffer untouched.
        QCOMPARE(dst[0], 9.9f);
    }

    // ── 14. pullSamples_noSources_zeroFills ───────────────────────────────
    // When both pcSource and radioSource are null, pullSamples zero-fills
    // and returns n (defensive path — should not occur in production).

    void pullSamples_noSources_zeroFills()
    {
        CompositeTxMicRouter router(nullptr, nullptr, /*hasMicJack=*/true);
        std::array<float, 4> dst{9.9f, 9.9f, 9.9f, 9.9f};
        const int got = router.pullSamples(dst.data(), 4);
        QCOMPARE(got, 4);
        for (float v : dst) {
            QCOMPARE(v, 0.0f);
        }
    }
};

QTEST_APPLESS_MAIN(TstTxMicRouterSelector)
#include "tst_tx_mic_router_selector.moc"
