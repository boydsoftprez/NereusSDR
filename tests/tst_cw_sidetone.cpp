// no-port-check: NereusSDR-original test. The values it checks are the
// sidetone.c port's (src/core/CwSidetone.cpp); no upstream logic here.
// =================================================================
// tests/tst_cw_sidetone.cpp  (NereusSDR)
// =================================================================
//
// CW transmit Task 11 (R-CW-39, V-SW-12): the computer sidetone. The
// generator alone (tone, raised-cosine edges, run, pitch and volume), then
// in the audio engine on a RadioModel: it reaches the transmit monitor's
// slot at MON's level only while the transmit slice is in CW with both
// sidetone switches on, the CWX sender's key keys it, and the radio's
// paddle status never does. No radio socket, no audio device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the computer sidetone tests (R-CW-39).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QStandardPaths>

#include <cmath>
#include <complex>
#include <memory>
#include <numbers>
#include <vector>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/CwSidetone.h"
#include "core/CwxSender.h"
#include "core/IAudioBus.h"
#include "core/RadioConnection.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/FakeAudioBus.h"

using namespace NereusSDR;

namespace {

constexpr int kRate = 48000;
constexpr double kPitch = 600.0;
constexpr double kVolume = 0.5;
// 0.005 s at 48 kHz.
constexpr int kEdge = 240;

std::vector<float> render(CwSidetone& st, int frames)
{
    std::vector<float> out(static_cast<size_t>(frames) * 2, 0.0f);
    if (!st.render(out.data(), frames)) {
        out.clear();
    }
    return out;
}

double magnitude(const std::vector<float>& buf, int frame)
{
    return std::hypot(double(buf[size_t(frame) * 2]), double(buf[size_t(frame) * 2 + 1]));
}

// The raised-cosine edge's value at step k of kEdge (sidetone.c's rise).
double riseAt(int k)
{
    return 0.5 * (1.0 - std::cos(double(k) * std::numbers::pi / double(kEdge)));
}

// The frequency of the strongest DFT bin of the left channel over n frames.
double peakHz(const std::vector<float>& buf, int start, int n)
{
    int best = 0;
    double bestMag = -1.0;
    for (int k = 1; k < n / 2; ++k) {
        std::complex<double> acc{0.0, 0.0};
        const double w = -2.0 * std::numbers::pi * double(k) / double(n);
        for (int i = 0; i < n; ++i) {
            acc += double(buf[size_t(start + i) * 2]) * std::polar(1.0, w * double(i));
        }
        if (std::abs(acc) > bestMag) {
            bestMag = std::abs(acc);
            best = k;
        }
    }
    return double(best) * double(kRate) / double(n);
}

void configured(CwSidetone& st)
{
    st.setEdge(CwSidetone::kRaisedCosineEdge, CwSidetone::kDefaultEdgeSeconds);
    st.configure(kRate, 1024);
    st.setPitchHz(kPitch);
    st.setVolume(kVolume);
    st.setRun(true);
}

class CwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

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
    void sendTxIq(const float*, int) override {}
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
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void reportKey(bool dot, bool dash, bool keyOut) { emit radioKeyStatus(dot, dash, keyOut); }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

constexpr int kFrames = 64;
constexpr float kMonLevel = 0.5f;

// A local model with one CW slice and a fake speakers bus; each period
// is one receiver block, whose arrival drains the mix.
struct EngineRig {
    std::unique_ptr<RadioModel> radio = std::make_unique<RadioModel>();
    AudioEngine* engine = nullptr;
    FakeAudioBus* speakers = nullptr;
    SliceModel* slice = nullptr;
    int sliceId = -1;

    EngineRig()
    {
        engine = radio->audioEngine();
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        auto spk = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        spk->open(format);
        speakers = spk.get();
        engine->setSpeakersBusForTest(std::move(spk));
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        engine->setVolume(1.0f);
        // MON's level is the monitor slot's gain. RadioModel feeds it from
        // monitorVolume once the transmit chain is wired at connect
        // (wireMicAndMonitorToTransmit), which this rig does not run.
        engine->setTxMonitorVolume(kMonLevel);
        sliceId = radio->addSlice();
        slice = radio->sliceById(sliceId);
        slice->setDspMode(DSPMode::CWU);
        engine->setSliceStreaming(sliceId, true);
    }

    void periods(int n)
    {
        const std::vector<float> silence(static_cast<size_t>(kFrames) * 2, 0.0f);
        for (int p = 0; p < n; ++p) {
            engine->rxBlockReady(sliceId, silence.data(), kFrames);
        }
    }

    // The last block's frame magnitudes on the speakers bus.
    std::vector<double> lastMagnitudes() const
    {
        const QByteArray bytes = speakers->buffer();
        const auto* f = reinterpret_cast<const float*>(bytes.constData());
        const int n = static_cast<int>(bytes.size() / qsizetype(sizeof(float)));
        std::vector<double> mags;
        for (int i = std::max(0, n - kFrames * 2); i + 1 < n; i += 2) {
            mags.push_back(std::hypot(double(f[i]), double(f[i + 1])));
        }
        return mags;
    }

    double lastPeak() const
    {
        double p = 0.0;
        for (const double m : lastMagnitudes()) {
            p = std::max(p, m);
        }
        return p;
    }
};

} // namespace

class TstCwSidetone : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void remoteHoldSilencesAnOldKeyListenerUntilResume()
    {
        CwSidetone st;
        configured(st);
        st.setKey(true);
        QVERIFY(!render(st, 1024).empty());
        st.setRemoteHeld(true);
        st.setKey(true); // late old listener store must not sustain tone
        const std::vector<float> held = render(st, 1024);
        for (size_t i = size_t(kEdge * 2 + 2); i < held.size(); ++i) {
            QCOMPARE(held[i], 0.0f);
        }
        st.setKey(false);
        st.setRemoteHeld(false);
        const std::vector<float> low = render(st, 1024);
        for (float sample : low) { QCOMPARE(sample, 0.0f); }
    }

    // ── The generator ───────────────────────────────────────────────────

    void notRunningRendersFalseAndWritesNothing()
    {
        CwSidetone st;
        configured(st);
        st.setRun(false);
        st.setKey(true);
        std::vector<float> out(256, 7.0f);
        QVERIFY(!st.render(out.data(), 128));
        for (const float v : out) {
            QCOMPARE(v, 7.0f);
        }
    }

    void theEdgeIsFiveMillisecondsAtTheMixRate()
    {
        CwSidetone st;
        configured(st);
        QCOMPARE(st.edgeSamples(), kEdge);
    }

    // Key down: one LOW sample, then the raised-cosine rise over 5 ms to
    // the volume, and the volume from there on.
    void keyDownRisesInFiveMillisecondsToTheVolume()
    {
        CwSidetone st;
        configured(st);
        st.setKey(true);
        const std::vector<float> out = render(st, 4800);
        QCOMPARE(int(out.size()), 4800 * 2);
        QCOMPARE(magnitude(out, 0), 0.0);
        for (int k = 0; k <= kEdge; ++k) {
            const double expected = kVolume * riseAt(k);
            QVERIFY2(std::fabs(magnitude(out, 1 + k) - expected) < 1e-5,
                     qPrintable(QStringLiteral("rise %1: %2 against %3")
                                    .arg(k).arg(magnitude(out, 1 + k)).arg(expected)));
        }
        for (int k = 0; k < kEdge; ++k) {
            QVERIFY(magnitude(out, 1 + k) < kVolume - 1e-6);
        }
        for (int i = 1 + kEdge; i < 4800; ++i) {
            QVERIFY2(std::fabs(magnitude(out, i) - kVolume) < 1e-5,
                     qPrintable(QStringLiteral("frame %1: %2").arg(i).arg(magnitude(out, i))));
        }
    }

    // The tone is at the pitch: the strongest bin is within one bin.
    void theToneIsAtThePitch()
    {
        CwSidetone st;
        configured(st);
        st.setKey(true);
        const std::vector<float> out = render(st, 4096 + 512);
        constexpr int kN = 4096;
        const double binHz = double(kRate) / double(kN);
        const double hz = peakHz(out, 512, kN);
        QVERIFY2(std::fabs(hz - kPitch) <= binHz,
                 qPrintable(QStringLiteral("peak %1 Hz").arg(hz)));
    }

    // Key up: the full tone for the sample that sees it, then the fall over
    // 5 ms, then exact zero.
    void keyUpFallsInFiveMillisecondsToExactZero()
    {
        CwSidetone st;
        configured(st);
        st.setKey(true);
        (void)render(st, 4800);
        st.setKey(false);
        const std::vector<float> out = render(st, 960);
        QVERIFY(std::fabs(magnitude(out, 0) - kVolume) < 1e-5);
        for (int k = 0; k <= kEdge; ++k) {
            const double expected = kVolume * riseAt(kEdge - k);
            QVERIFY2(std::fabs(magnitude(out, 1 + k) - expected) < 1e-5,
                     qPrintable(QStringLiteral("fall %1: %2 against %3")
                                    .arg(k).arg(magnitude(out, 1 + k)).arg(expected)));
        }
        for (int i = 2 + kEdge; i < 960; ++i) {
            QCOMPARE(out[size_t(i) * 2], 0.0f);
            QCOMPARE(out[size_t(i) * 2 + 1], 0.0f);
        }
        // And it stays silent while the key is up.
        const std::vector<float> after = render(st, 480);
        for (const float v : after) {
            QCOMPARE(v, 0.0f);
        }
    }

    // A pitch or volume change reaches the next render.
    void pitchAndVolumeReachTheNextRender()
    {
        CwSidetone st;
        configured(st);
        st.setKey(true);
        (void)render(st, 960);
        st.setPitchHz(800.0);
        st.setVolume(0.25);
        const std::vector<float> out = render(st, 4096);
        const double binHz = double(kRate) / 4096.0;
        QVERIFY(std::fabs(peakHz(out, 0, 4096) - 800.0) <= binHz);
        for (int i = 0; i < 4096; ++i) {
            QVERIFY(std::fabs(magnitude(out, i) - 0.25) < 1e-5);
        }
    }

    // ── The CWX sender's key ────────────────────────────────────────────

    // The TCI key (setKeyHeld) and the straight key (setHandKey) both reach
    // the listener, with no key line and outside break-in or MOX; stop()
    // releases it.
    void theSendersKeyReachesTheListener()
    {
        CwxSender sender;
        std::atomic<int> changes{0};
        std::atomic<bool> key{true};
        sender.setKeyListener([&](bool down) {
            key.store(down);
            ++changes;
        });
        QCOMPARE(key.load(), false);
        QVERIFY(sender.setHandKey(true));
        QCOMPARE(key.load(), true);
        sender.setHandKey(false);
        QCOMPARE(key.load(), false);
        sender.setKeyHeld(true);
        QTRY_COMPARE_WITH_TIMEOUT(key.load(), true, 2000);
        sender.stop();
        QTRY_COMPARE_WITH_TIMEOUT(key.load(), false, 2000);
    }

    // ── In the engine ───────────────────────────────────────────────────

    // CW with both switches on: a keyed sidetone reaches the monitor slot,
    // and the speakers, at MON's level.
    void keyedItReachesTheMonitorSlotAtTheMonLevel()
    {
        AppSettings::instance().clear();
        EngineRig rig;
        QVERIFY(rig.radio->transmitModel().cwSidetone());
        QVERIFY(rig.radio->transmitModel().cwComputerSidetone());
        QVERIFY(rig.engine->cwSidetoneRun());
        QCOMPARE(rig.engine->cwSidetonePitchHz(), double(rig.radio->cwPitchHz()));
        QVERIFY(rig.radio->cwxSender()->setHandKey(true));
        QVERIFY(rig.engine->cwSidetoneKeyed());
        rig.periods(20);
        const std::vector<double> mags = rig.lastMagnitudes();
        QCOMPARE(int(mags.size()), kFrames);
        for (const double m : mags) {
            QVERIFY2(std::fabs(m - double(kMonLevel)) < 1e-4,
                     qPrintable(QStringLiteral("magnitude %1").arg(m)));
        }
        // A MON level change reaches it at the next drain.
        rig.engine->setTxMonitorVolume(0.25f);
        rig.periods(20);
        for (const double m : rig.lastMagnitudes()) {
            QVERIFY2(std::fabs(m - 0.25) < 1e-4, qPrintable(QStringLiteral("magnitude %1").arg(m)));
        }
        rig.radio->cwxSender()->setHandKey(false);
        rig.periods(20);
        QCOMPARE(rig.lastPeak(), 0.0);
        AppSettings::instance().clear();
    }

    // Either switch off, or a mode that is not CW: it does not run and the
    // slot gets nothing.
    void eitherSwitchOffOrNotCwIsSilence()
    {
        AppSettings::instance().clear();
        EngineRig rig;
        TransmitModel& tm = rig.radio->transmitModel();
        QVERIFY(rig.radio->cwxSender()->setHandKey(true));

        tm.setCwComputerSidetone(false);
        QVERIFY(!rig.engine->cwSidetoneRun());
        rig.periods(20);
        QCOMPARE(rig.lastPeak(), 0.0);

        tm.setCwComputerSidetone(true);
        tm.setCwSidetone(false);
        QVERIFY(!rig.engine->cwSidetoneRun());
        rig.periods(20);
        QCOMPARE(rig.lastPeak(), 0.0);

        tm.setCwSidetone(true);
        QVERIFY(rig.engine->cwSidetoneRun());
        rig.periods(20);
        QVERIFY(rig.lastPeak() > 0.4);

        rig.slice->setDspMode(DSPMode::USB);
        QVERIFY(!rig.engine->cwSidetoneRun());
        rig.periods(20);
        QCOMPARE(rig.lastPeak(), 0.0);
        rig.radio->cwxSender()->setHandKey(false);
        AppSettings::instance().clear();
    }

    // The CW pitch is the sidetone's pitch.
    void itFollowsTheCwPitch()
    {
        AppSettings::instance().clear();
        EngineRig rig;
        rig.radio->setCwPitchHz(700);
        QCOMPARE(rig.engine->cwSidetonePitchHz(), 700.0);
        AppSettings::instance().clear();
    }

    // The radio's paddle status never keys it (spec C8).
    void thePaddleNeverKeysIt()
    {
        AppSettings::instance().clear();
        EngineRig rig;
        CwMockConnection conn;
        rig.radio->injectConnectionForTest(&conn);
        rig.radio->wireCwKeyerConnectionForTest();
        pump();
        conn.reportKey(/*dot=*/true, /*dash=*/false, /*keyOut=*/true);
        pump();
        QVERIFY(!rig.engine->cwSidetoneKeyed());
        conn.reportKey(/*dot=*/false, /*dash=*/true, /*keyOut=*/true);
        pump();
        QVERIFY(!rig.engine->cwSidetoneKeyed());
        rig.periods(20);
        QCOMPARE(rig.lastPeak(), 0.0);
        conn.reportKey(false, false, false);
        pump();
        rig.radio->injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    // A remote window never runs one: the Core's plays in its owner mix.
    void aRemoteWindowNeverRunsIt()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(!remote.computeComputerSidetoneRun());
        QVERIFY(!remote.audioEngine()->cwSidetoneRun());
    }
};

QTEST_MAIN(TstCwSidetone)
#include "tst_cw_sidetone.moc"
