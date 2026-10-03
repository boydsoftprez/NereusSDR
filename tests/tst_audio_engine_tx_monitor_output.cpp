// =================================================================
// tests/tst_audio_engine_tx_monitor_output.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R-R3-45 (operator decision 2026-09-24): the transmit monitor (MON) plays
// on the speakers or the headphones, the operator's own choice beside the
// MON button, independent of the receivers' routes. Default speakers, as
// before; saved as audio/TxMonitor/Output, this computer's setting.
//
// Pins:
//   - the default is the speakers, and a saved choice is read back;
//   - setting it saves it and announces it once;
//   - MON on the speakers lands only in the speakers mix, on the
//     headphones only in the headphones mix;
//   - a change while MON is playing moves it at once with no doubling
//     (the two outputs together never carry more than MON's own level);
//   - the anti-VOX reference never carries MON, on either output.
//
// Harness as tst_audio_engine_multi_slice_mix: fake speakers and
// headphones buses through the NEREUS_BUILD_TESTS seams, start() never
// called, so no real sound device is touched. No radio is keyed: MON
// blocks are fed straight to txMonitorBlockReady, as TxChannel's siphon
// would during a transmission.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

using namespace NereusSDR;

namespace {

constexpr float kMonLevel = 0.5f;
constexpr int kFrames = 2;

AudioFormat stereoFloat48k()
{
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels = 2;
    fmt.sample = AudioFormat::Sample::Float32;
    return fmt;
}

const float* floatsOf(const QByteArray& bytes)
{
    return reinterpret_cast<const float*>(bytes.constData());
}

int floatCount(const QByteArray& bytes)
{
    return static_cast<int>(bytes.size() / static_cast<qsizetype>(sizeof(float)));
}

float peakOf(const QByteArray& bytes, int fromFloat = 0)
{
    float peak = 0.0f;
    const float* f = floatsOf(bytes);
    for (int i = fromFloat; i < floatCount(bytes); ++i) {
        peak = std::max(peak, std::fabs(f[i]));
    }
    return peak;
}

} // namespace

class TstAudioEngineTxMonitorOutput : public QObject {
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine* engine{nullptr};      // non-owning
        FakeAudioBus* speakers{nullptr};   // engine owns it
        FakeAudioBus* headphones{nullptr}; // engine owns it
        int slice{-1};
    };

    // One receiver on the speakers feeding silence paces the mix (the
    // monitor slot never joins the barrier); MON is on at full level and
    // master volume is 1.0 so the speakers and headphones compare directly.
    Harness makeHarness()
    {
        Harness h;
        h.radio = std::make_unique<RadioModel>();
        h.engine = h.radio->audioEngine();

        auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        speakers->open(stereoFloat48k());
        h.speakers = speakers.get();
        h.engine->setSpeakersBusForTest(std::move(speakers));

        auto phones = std::make_unique<FakeAudioBus>(QStringLiteral("FakeHeadphones"));
        phones->open(stereoFloat48k());
        h.headphones = phones.get();
        h.engine->setHeadphonesBusForTest(std::move(phones));

        h.radio->configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                     /*defaultRateHz*/ 192000);
        h.slice = h.radio->addSlice();
        h.engine->setVolume(1.0f);
        h.engine->setTxMonitorVolume(1.0f);
        h.engine->setTxMonitorEnabled(true);
        return h;
    }

    // One audio period: a MON block, then the receiver's block, whose
    // arrival drains the mix and pushes both outputs.
    static void runPeriods(Harness& h, int periods)
    {
        const std::array<float, kFrames> mon = {kMonLevel, kMonLevel};
        const std::array<float, kFrames * 2> silence = {0.0f, 0.0f, 0.0f, 0.0f};
        for (int p = 0; p < periods; ++p) {
            h.engine->txMonitorBlockReady(mon.data(), kFrames);
            h.engine->rxBlockReady(h.slice, silence.data(), kFrames);
        }
    }

private slots:
    // The test device guard: with no fake device supplied, the engine
    // opens nothing real.
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void defaultsToTheSpeakers()
    {
        AudioEngine engine;
        QCOMPARE(engine.txMonitorOutput(), TxMonitorOutput::Speakers);
    }

    void choiceIsSavedAndAnnouncedOnce()
    {
        AudioEngine engine;
        QSignalSpy spy(&engine, &AudioEngine::txMonitorOutputChanged);

        engine.setTxMonitorOutput(TxMonitorOutput::Headphones);
        QCOMPARE(engine.txMonitorOutput(), TxMonitorOutput::Headphones);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/TxMonitor/Output")).toString(),
                 QStringLiteral("Headphones"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<TxMonitorOutput>(), TxMonitorOutput::Headphones);

        engine.setTxMonitorOutput(TxMonitorOutput::Headphones);  // no change
        QCOMPARE(spy.count(), 1);

        engine.setTxMonitorOutput(TxMonitorOutput::Speakers);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/TxMonitor/Output")).toString(),
                 QStringLiteral("Speakers"));
        QCOMPARE(spy.count(), 2);
    }

    void savedChoiceIsReadBack()
    {
        AppSettings::instance().setValue(QStringLiteral("audio/TxMonitor/Output"),
                                         QStringLiteral("Headphones"));
        AudioEngine engine;
        QCOMPARE(engine.txMonitorOutput(), TxMonitorOutput::Headphones);
    }

    void onTheSpeakersOnlyTheSpeakersCarryIt()
    {
        Harness h = makeHarness();
        runPeriods(h, 400);

        QCOMPARE(h.speakers->pushCount(), h.headphones->pushCount());
        QVERIFY(peakOf(h.speakers->buffer()) > 0.4f);
        QCOMPARE(peakOf(h.headphones->buffer()), 0.0f);
    }

    void onTheHeadphonesOnlyTheHeadphonesCarryIt()
    {
        Harness h = makeHarness();
        h.engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
        runPeriods(h, 400);

        QCOMPARE(h.speakers->pushCount(), h.headphones->pushCount());
        QVERIFY(peakOf(h.headphones->buffer()) > 0.4f);
        QCOMPARE(peakOf(h.speakers->buffer()), 0.0f);
    }

    // A change while MON plays: it leaves the speakers and arrives on the
    // headphones within the mixer's ramp, and at no sample do the two
    // outputs together carry more than MON's own level. Then back again.
    void liveChangeMovesItAtOnceWithoutDoubling()
    {
        Harness h = makeHarness();
        runPeriods(h, 400);
        const int splitFloats = floatCount(h.speakers->buffer());
        QCOMPARE(floatCount(h.headphones->buffer()), splitFloats);

        h.engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
        runPeriods(h, 400);

        const QByteArray spk = h.speakers->buffer();
        const QByteArray hp = h.headphones->buffer();
        QCOMPARE(floatCount(spk), floatCount(hp));
        const float* s = floatsOf(spk);
        const float* p = floatsOf(hp);
        for (int i = 0; i < floatCount(spk); ++i) {
            QVERIFY2(std::fabs(s[i]) + std::fabs(p[i]) <= kMonLevel + 1e-4f,
                     qPrintable(QStringLiteral("doubled at float %1: %2 + %3")
                                    .arg(i).arg(double(s[i])).arg(double(p[i]))));
        }
        // Moved within the first block after the change: the headphones
        // carry MON right away, and the speakers are silent once the
        // 240-frame ramp has run (well inside 400 two-frame periods).
        QVERIFY(std::fabs(p[splitFloats]) > 0.0f);
        const int tailFloats = floatCount(spk) - 16;
        QCOMPARE(peakOf(spk, tailFloats), 0.0f);
        QVERIFY(peakOf(hp, tailFloats) > 0.4f);

        h.engine->setTxMonitorOutput(TxMonitorOutput::Speakers);
        runPeriods(h, 400);
        const QByteArray spk2 = h.speakers->buffer();
        const QByteArray hp2 = h.headphones->buffer();
        const int tail2 = floatCount(spk2) - 16;
        QVERIFY(peakOf(spk2, tail2) > 0.4f);
        QCOMPARE(peakOf(hp2, tail2), 0.0f);
    }

    // What the speakers play is the anti-VOX reference; MON is never in it,
    // wherever it plays.
    void antiVoxReferenceNeverCarriesIt()
    {
        Harness h = makeHarness();
        float referencePeak = 0.0f;
        int references = 0;
        connect(h.engine, &AudioEngine::antiVoxBlockReady, this,
                [&referencePeak, &references](const float* samples, int frames) {
            ++references;
            for (int i = 0; i < frames * 2; ++i) {
                referencePeak = std::max(referencePeak, std::fabs(samples[i]));
            }
        }, Qt::DirectConnection);

        runPeriods(h, 100);
        h.engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
        runPeriods(h, 100);

        QVERIFY(references > 0);
        QCOMPARE(referencePeak, 0.0f);
    }
};

QTEST_MAIN(TstAudioEngineTxMonitorOutput)
#include "tst_audio_engine_tx_monitor_output.moc"
