// =================================================================
// tests/tst_audio_engine_pull_tx_mic.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine::pullTxMic — Phase 3M-1b Task E.1.
//
// pullTxMic wraps m_txInputBus->pull(), converts raw bytes to float32
// mono, and returns the sample count actually written. The test uses
// FakeAudioBus injected via the NEREUS_BUILD_TESTS-only
// AudioEngine::setTxInputBusForTest seam, so no real PortAudio device
// or CoreAudio HAL plugin is required. Cross-platform.
//
// Pre-code review cite: §0.3 (PcMicSource arch — pullTxMic is the
// foundational accessor PcMicSource will tap in Phase F.1).
//
// 2026-09-22: R-R3-36 prerequisite adds the source-intent/readiness
// distinction used by TxWorkerThread. J.J. Boyd (KG4VCF), AI-assisted via
// OpenAI Codex.
//
// 2026-09-22: R-R3-36 Task 5: with no injected bus the TX input is the
// capture supervisor's reader, which yields nothing until capture is Ready.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"

#include "fakes/FakeAudioBus.h"

#include <cstring>

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// Helper: build a QByteArray of Int16 interleaved stereo samples from a
// vector of (left, right) pairs, in the order pullTxMic expects.
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
// Helper: build a QByteArray of Float32 interleaved stereo samples.
// ---------------------------------------------------------------------------
static QByteArray makeFloat32StereoBytes(
    std::initializer_list<std::pair<float, float>> frames)
{
    QByteArray out;
    out.reserve(static_cast<int>(frames.size()) * 2 * 4);
    for (auto [l, r] : frames) {
        out.append(reinterpret_cast<const char*>(&l), 4);
        out.append(reinterpret_cast<const char*>(&r), 4);
    }
    return out;
}

class TstAudioEnginePullTxMic : public QObject {
    Q_OBJECT

private:
    // Build an AudioEngine with a FakeAudioBus injected into the
    // TX-input slot. Returns the engine (unique_ptr) and a raw view of
    // the fake bus so tests can call setPullData / setNegotiatedFormat.
    struct Harness {
        std::unique_ptr<AudioEngine> engine;
        FakeAudioBus* bus;  // non-owning; engine owns via m_txInputBus
    };

    static Harness makeHarness(AudioFormat fmt) {
        Harness h;
        h.engine = std::make_unique<AudioEngine>();

        auto fakeBus = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeTxInput"));
        fakeBus->open(fmt);
        h.bus = fakeBus.get();
        h.engine->setTxInputBusForTest(std::move(fakeBus));
        return h;
    }

    static AudioFormat int16StereoFmt() {
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Int16;
        return fmt;
    }

    static AudioFormat float32StereoFmt() {
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        return fmt;
    }

    static AudioFormat float32MonoFmt() {
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 1;
        fmt.sample     = AudioFormat::Sample::Float32;
        return fmt;
    }

private slots:

    void pcMicSelectionIntent_isIndependentOfCaptureReadiness()
    {
        AudioEngine engine;
        QVERIFY(!engine.isPcMicSelected());
        QVERIFY(!engine.isPcMicOverrideActive());

        engine.onMicSourceChanged(/*selectedSourceIsPc=*/true);
        QVERIFY(engine.isPcMicSelected());
        QVERIFY(!engine.isPcMicOverrideActive());

        AudioFormat fmt = float32StereoFmt();
        auto fakeBus = std::make_unique<FakeAudioBus>(
            QStringLiteral("ClosedFakeTxInput"));
        FakeAudioBus* bus = fakeBus.get();
        fakeBus->setNegotiatedFormat(fmt);
        engine.setTxInputBusForTest(std::move(fakeBus));
        QVERIFY(engine.isPcMicSelected());
        QVERIFY(!engine.isPcMicOverrideActive());

        QVERIFY(bus->open(fmt));
        QVERIFY(engine.isPcMicSelected());
        QVERIFY(engine.isPcMicOverrideActive());

        engine.onMicSourceChanged(/*selectedSourceIsPc=*/false);
        QVERIFY(!engine.isPcMicSelected());
        QVERIFY(!engine.isPcMicOverrideActive());
    }

    // ── 1. Null bus: returns 0 ─────────────────────────────────────────────
    // m_txInputBus is default-null — engine with no TX input configured.

    void nullBus_returnsZero()
    {
        AudioEngine engine;  // no bus injected
        std::array<float, 4> dst{};
        QCOMPARE(engine.pullTxMic(dst.data(), 4), 0);
    }

    // R-R3-36: PC mic selected but capture not Ready (no injected bus, no
    // demand): not active, no samples, no level, and start() opens nothing.
    void pcSelectedWithoutReadyCapture_isSilent()
    {
        AudioEngine engine;
        engine.onMicSourceChanged(/*selectedSourceIsPc=*/true);
        QVERIFY(engine.isPcMicSelected());
        QVERIFY(!engine.isPcMicOverrideActive());
        std::array<float, 64> dst{};
        QCOMPARE(engine.pullTxMic(dst.data(), 64), 0);
        QCOMPARE(engine.pcMicInputLevel(), 0.0f);
        QCOMPARE(engine.captureStatus().state, CaptureSupervisor::Status::State::Closed);
    }

    // ── 2. n <= 0: returns 0 ──────────────────────────────────────────────

    void zeroN_returnsZero()
    {
        Harness h = makeHarness(int16StereoFmt());
        std::array<float, 4> dst{};
        QCOMPARE(h.engine->pullTxMic(dst.data(), 0), 0);
        QCOMPARE(h.engine->pullTxMic(dst.data(), -1), 0);
    }

    // ── 3. Null dst: returns 0 ────────────────────────────────────────────

    void nullDst_returnsZero()
    {
        Harness h = makeHarness(int16StereoFmt());
        QCOMPARE(h.engine->pullTxMic(nullptr, 4), 0);
    }

    // ── 4. Empty bus (no pull data): returns 0 ────────────────────────────
    // Bus is open but has no data queued — pull() returns 0 bytes.

    void emptyBus_returnsZero()
    {
        Harness h = makeHarness(int16StereoFmt());
        // No setPullData call → m_pullData is empty → pull() returns 0.
        std::array<float, 4> dst{};
        QCOMPARE(h.engine->pullTxMic(dst.data(), 4), 0);
    }

    // ── 5. Int16 stereo → float32 mono: correct sample count ──────────────
    // Two stereo frames → 2 mono output samples; return value is 2.

    void int16Stereo_returnsSampleCount()
    {
        Harness h = makeHarness(int16StereoFmt());
        // 2 stereo frames of Int16 (4 int16_t values = 8 bytes).
        h.bus->setPullData(makeInt16StereoBytes({{1000, -500}, {2000, -1000}}));

        std::array<float, 4> dst{};
        const int got = h.engine->pullTxMic(dst.data(), 4);
        QCOMPARE(got, 2);
    }

    // ── 6. Int16 stereo → float32 mono: correct normalisation ─────────────
    // Left channel is taken; right is discarded. Int16 / 32768.0f normalises.
    // INT16_MAX (32767) → ~1.0f; value 16384 → 0.5f exactly.

    void int16StereoToFloat32Mono_correctConversion()
    {
        Harness h = makeHarness(int16StereoFmt());
        // Frame 0: left=16384 (0.5f), right=0 (discarded).
        // Frame 1: left=-16384 (-0.5f), right=32767 (discarded).
        h.bus->setPullData(makeInt16StereoBytes(
            {{16384, 0}, {-16384, 32767}}));

        std::array<float, 4> dst{0.0f, 0.0f, 0.0f, 0.0f};
        const int got = h.engine->pullTxMic(dst.data(), 4);

        QCOMPARE(got, 2);
        // 16384 / 32768.0f = 0.5f exactly (power-of-two; no rounding).
        QCOMPARE(dst[0], 16384.0f / 32768.0f);
        QCOMPARE(dst[1], -16384.0f / 32768.0f);
        // Samples beyond `got` are not touched.
        QCOMPARE(dst[2], 0.0f);
    }

    // ── 7. Float32 stereo → float32 mono: correct passthrough ─────────────
    // Left channel is passed directly; right is discarded.

    void float32StereoToMono_correctPassthrough()
    {
        Harness h = makeHarness(float32StereoFmt());
        h.bus->setPullData(makeFloat32StereoBytes(
            {{0.75f, -0.25f}, {-0.5f, 0.9f}}));

        std::array<float, 4> dst{};
        const int got = h.engine->pullTxMic(dst.data(), 4);

        QCOMPARE(got, 2);
        QCOMPARE(dst[0], 0.75f);
        QCOMPARE(dst[1], -0.5f);
    }

    // ── 8. Float32 mono → float32 mono: all samples used ──────────────────
    // Single-channel bus: no channel stride; channel 0 is the only sample
    // per frame, so all samples transfer directly.

    void float32Mono_fullTransfer()
    {
        Harness h = makeHarness(float32MonoFmt());

        QByteArray raw;
        const float v0 = 0.1f;
        const float v1 = -0.2f;
        const float v2 = 0.3f;
        raw.append(reinterpret_cast<const char*>(&v0), 4);
        raw.append(reinterpret_cast<const char*>(&v1), 4);
        raw.append(reinterpret_cast<const char*>(&v2), 4);
        h.bus->setPullData(raw);

        std::array<float, 4> dst{};
        const int got = h.engine->pullTxMic(dst.data(), 4);

        QCOMPARE(got, 3);
        QCOMPARE(dst[0], 0.1f);
        QCOMPARE(dst[1], -0.2f);
        QCOMPARE(dst[2], 0.3f);
    }

    // ── 9. Partial bus: returns actual available count ─────────────────────
    // Bus has only 1 stereo frame but caller asks for 4 mono samples.
    // pullTxMic must return 1, not 4.

    void partialBus_returnsActualCount()
    {
        Harness h = makeHarness(int16StereoFmt());
        // Inject 1 stereo frame (4 bytes Int16).
        h.bus->setPullData(makeInt16StereoBytes({{8192, 0}}));

        std::array<float, 4> dst{0.0f, 0.0f, 0.0f, 0.0f};
        const int got = h.engine->pullTxMic(dst.data(), 4);

        QCOMPARE(got, 1);
        QCOMPARE(dst[0], 8192.0f / 32768.0f);
        // Remaining dst slots are untouched.
        QCOMPARE(dst[1], 0.0f);
    }
};

QTEST_APPLESS_MAIN(TstAudioEnginePullTxMic)
#include "tst_audio_engine_pull_tx_mic.moc"
