// =================================================================
// tests/tst_audio_engine_matcher_push.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The clock matcher on a local
// output (R-AUD-15); no upstream logic is ported here.
//
// A speakers bus that takes the stereo mix (FakeMatcherAudioBus, a real
// DeviceRateMatcher with no device) gets the 48 kHz stereo master mix
// itself, after every gain and mute, and never through the speaker format
// converter. Pumped as a device whose clock runs 200 ppm away from the
// mix for 60 simulated seconds of local playback, the matcher shows no
// dry run once it has sized itself. Nothing opens this computer's real
// speakers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09  J.J. Boyd / KG4VCF  Native audio plan Task 6 (R-AUD-15).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-09  J.J. Boyd / KG4VCF  Native audio plan Task 7 (R-AUD-15):
//                                    delayParts(AudioRole::Speakers)
//                                    replaces speakersDelayParts().
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/audio/IAudioEngineBackend.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"
#include "fakes/FakeMatcherAudioBus.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kBlockFrames = 64;      // one DSP block at 48 kHz
constexpr int kCallbackFrames = 128;  // the fake device's callback

struct Rig {
    std::unique_ptr<RadioModel> radio;
    AudioEngine* engine = nullptr;
    int slice = -1;
};

// A radio with one slice at AF 100 and the given speakers bus.
Rig makeRig(std::unique_ptr<IAudioBus> speakers)
{
    Rig rig;
    rig.radio = std::make_unique<RadioModel>();
    rig.engine = rig.radio->audioEngine();
    rig.engine->setSpeakersBusForTest(std::move(speakers));
    rig.engine->setVolume(1.0f);
    rig.radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
    rig.slice = rig.radio->addSlice();
    if (rig.slice >= 0) {
        // AF is the mixer level now; these tests measure unity gain.
        rig.radio->sliceById(rig.slice)->setAfGain(100);
    }
    return rig;
}

std::unique_ptr<FakeMatcherAudioBus> openMatcherBus(int rate, bool takesStereoMix,
                                                    FakeMatcherAudioBus** view)
{
    AudioStreamRequest request;
    request.sampleRate = rate;
    auto bus = std::make_unique<FakeMatcherAudioBus>(request, takesStereoMix, kCallbackFrames);
    AudioFormat format;
    format.sampleRate = rate;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    if (!bus->open(format)) {
        return nullptr;
    }
    *view = bus.get();
    return bus;
}

// One 64-frame block of a tone, both channels, continuing from `frame`.
void fillTone(std::vector<float>& block, qint64 frame, double hz, double amp)
{
    for (int i = 0; i < kBlockFrames; ++i) {
        const double t = double(frame + i) / 48000.0;
        const float v = float(amp * std::sin(2.0 * kPi * hz * t));
        block[size_t(2 * i)] = v;
        block[size_t(2 * i + 1)] = v;
    }
}

} // namespace

class TstAudioEngineMatcherPush : public QObject {
    Q_OBJECT

private slots:
    // 60 simulated seconds of local playback into a device 200 ppm fast or
    // slow: once sized, the matcher never runs dry (R-AUD-15, D2).
    void noDryRunAfterSizingAtTwoHundredPpm_data()
    {
        QTest::addColumn<double>("ppm");
        QTest::newRow("device 200 ppm fast") << 200.0;
        QTest::newRow("device 200 ppm slow") << -200.0;
    }

    void noDryRunAfterSizingAtTwoHundredPpm()
    {
        QFETCH(double, ppm);
        FakeMatcherAudioBus* device = nullptr;
        auto bus = openMatcherBus(48000, true, &device);
        QVERIFY(bus);
        Rig rig = makeRig(std::move(bus));
        QVERIFY(rig.slice >= 0);
        QVERIFY(rig.engine->remotePlaybackIntoMatcher());

        constexpr int kSeconds = 60;
        constexpr int kSizingSeconds = 15;
        const double devicePerMixFrame = 1.0 + ppm * 1e-6;
        std::vector<float> block(kBlockFrames * 2);
        double owed = 0.0;
        quint64 dryRunsAfterSizing = 0;
        quint64 overrunsAfterSizing = 0;
        const qint64 total = qint64(kSeconds) * 48000;
        for (qint64 sent = 0; sent < total; sent += kBlockFrames) {
            fillTone(block, sent, 700.0, 0.2);
            rig.engine->rxBlockReady(rig.slice, block.data(), kBlockFrames);
            owed += double(kBlockFrames) * devicePerMixFrame;
            while (owed >= double(kCallbackFrames)) {
                device->pumpForTest(kCallbackFrames);
                owed -= double(kCallbackFrames);
            }
            if (sent == qint64(kSizingSeconds) * 48000) {
                const auto stats = device->matcherStats();
                QVERIFY(stats.has_value());
                dryRunsAfterSizing = stats->dryRuns;
                overrunsAfterSizing = stats->overruns;
            }
        }

        const auto stats = device->matcherStats();
        QVERIFY(stats.has_value());
        QVERIFY2(stats->dryRuns == dryRunsAfterSizing,
                 qPrintable(QStringLiteral("dry runs %1 at %2 s, %3 at %4 s (ratio %5)")
                                .arg(dryRunsAfterSizing).arg(kSizingSeconds)
                                .arg(stats->dryRuns).arg(kSeconds).arg(stats->ratio, 0, 'f', 7)));
        QCOMPARE(stats->overruns, overrunsAfterSizing);
        QVERIFY(stats->controlActive);
        // The mix reached the device: the last read carries the tone.
        float peak = 0.0f;
        for (int i = 0; i < 2 * device->lastReadFrames(); ++i) {
            peak = std::max(peak, std::abs(device->lastRead()[i]));
        }
        QVERIFY2(peak > 0.1f, qPrintable(QString::number(peak)));
    }

    // A bus that takes the stereo mix gets the 48 kHz block itself, not the
    // speaker format converter's output: a 700 Hz tone played by a 96 kHz
    // device is still 700 Hz (the converter's 96 kHz block read as 48 kHz
    // would put it at 350 Hz). A bus without one keeps the converter.
    void matcherBusSkipsTheFormatConverter()
    {
        constexpr int kRate = 96000;
        {
            FakeMatcherAudioBus* device = nullptr;
            auto bus = openMatcherBus(kRate, true, &device);
            QVERIFY(bus);
            Rig rig = makeRig(std::move(bus));
            QVERIFY(rig.slice >= 0);
            std::vector<float> block(kBlockFrames * 2);
            std::vector<float> played;
            double owed = 0.0;
            for (qint64 sent = 0; sent < 48000; sent += kBlockFrames) {
                fillTone(block, sent, 700.0, 0.2);
                rig.engine->rxBlockReady(rig.slice, block.data(), kBlockFrames);
                owed += double(kBlockFrames) * 2.0;
                while (owed >= double(kCallbackFrames)) {
                    device->pumpForTest(kCallbackFrames);
                    owed -= double(kCallbackFrames);
                    if (sent >= 24000) {
                        for (int i = 0; i < kCallbackFrames; ++i) {
                            played.push_back(device->lastRead()[2 * i]);
                        }
                    }
                }
            }
            QCOMPARE(device->pushCount(), 48000 / kBlockFrames);
            auto amplitude = [&](double hz) {
                double c = 0.0;
                double s = 0.0;
                for (size_t f = 0; f < played.size(); ++f) {
                    const double phase = 2.0 * kPi * hz * double(f) / double(kRate);
                    c += played[f] * std::cos(phase);
                    s += played[f] * std::sin(phase);
                }
                return 2.0 * std::hypot(c, s) / double(played.size());
            };
            const double at700 = amplitude(700.0);
            const double at350 = amplitude(350.0);
            QVERIFY2(std::abs(at700 - 0.2) < 0.01 && at350 < 0.005,
                     qPrintable(QStringLiteral("700 Hz %1, 350 Hz %2").arg(at700).arg(at350)));
        }
        {
            FakeMatcherAudioBus* device = nullptr;
            auto bus = openMatcherBus(kRate, false, &device);
            QVERIFY(bus);
            Rig rig = makeRig(std::move(bus));
            QVERIFY(rig.slice >= 0);
            QVERIFY(!rig.engine->remotePlaybackIntoMatcher());
            std::vector<float> block(kBlockFrames * 2);
            for (qint64 sent = 0; sent < 48000; sent += kBlockFrames) {
                fillTone(block, sent, 700.0, 0.2);
                rig.engine->rxBlockReady(rig.slice, block.data(), kBlockFrames);
            }
            // One second at the device's own rate (less the converter's
            // delay), as the converter has always made it.
            const qsizetype frames = device->bytes().size() / qsizetype(2 * sizeof(float));
            QVERIFY2(frames <= kRate && frames >= kRate - kRate / 20,
                     qPrintable(QString::number(frames)));
        }
    }

    // Muted, a matcher bus is fed silence: the mute's flush drops what is
    // queued, the device plays the matcher's blend out of the tone for one
    // callback (128 frames, ntslew + 1) and then exact zeros, and the
    // matcher does not run dry for want of writes. A bus without a matcher
    // is pushed nothing, as always.
    void muteFeedsSilenceToAMatcherBus()
    {
        {
            FakeMatcherAudioBus* device = nullptr;
            auto bus = openMatcherBus(48000, true, &device);
            QVERIFY(bus);
            Rig rig = makeRig(std::move(bus));
            QVERIFY(rig.slice >= 0);
            std::vector<float> block(kBlockFrames * 2);
            qint64 sent = 0;
            auto run = [&](int blocks, float* peakAfter, int skipBlocks) {
                *peakAfter = 0.0f;
                for (int b = 0; b < blocks; ++b, sent += kBlockFrames) {
                    fillTone(block, sent, 700.0, 0.2);
                    rig.engine->rxBlockReady(rig.slice, block.data(), kBlockFrames);
                    if ((b & 1) == 1) {
                        device->pumpForTest(kCallbackFrames);
                        if (b >= skipBlocks) {
                            for (int i = 0; i < 2 * kCallbackFrames; ++i) {
                                *peakAfter = std::max(*peakAfter,
                                                      std::abs(device->lastRead()[i]));
                            }
                        }
                    }
                }
            };
            float peak = 0.0f;
            run(1500, &peak, 1000);   // two seconds playing
            QVERIFY(peak > 0.1f);
            const quint64 dryBefore = device->matcherStats()->dryRuns;
            const int pushesBefore = device->pushCount();
            rig.engine->setMasterMuted(true);
            run(750, &peak, 2);       // one second muted, from the second callback
            QVERIFY2(peak == 0.0f, qPrintable(QString::number(peak)));
            QCOMPARE(device->pushCount(), pushesBefore + 750);
            QCOMPARE(device->matcherStats()->dryRuns, dryBefore);
        }
        {
            FakeMatcherAudioBus* device = nullptr;
            auto bus = openMatcherBus(48000, false, &device);
            QVERIFY(bus);
            Rig rig = makeRig(std::move(bus));
            QVERIFY(rig.slice >= 0);
            rig.engine->setMasterMuted(true);
            std::vector<float> block(kBlockFrames * 2);
            for (qint64 sent = 0; sent < 4800; sent += kBlockFrames) {
                fillTone(block, sent, 700.0, 0.2);
                rig.engine->rxBlockReady(rig.slice, block.data(), kBlockFrames);
            }
            QCOMPARE(device->pushCount(), 0);
        }
    }

    // delayParts(Speakers) reports the matcher bus's parts, and -1 (no
    // matcher) for a bus without one.
    void speakersDelayPartsComeFromTheBus()
    {
        {
            FakeMatcherAudioBus* device = nullptr;
            auto bus = openMatcherBus(48000, true, &device);
            QVERIFY(bus);
            Rig rig = makeRig(std::move(bus));
            const AudioDelayParts parts = rig.engine->delayParts(AudioRole::Speakers);
            QVERIFY(parts.matcherFillMs >= 0.0);
            QCOMPARE(parts.deviceBufferMs, 1000.0 * kCallbackFrames / 48000.0);
            QVERIFY(rig.engine->remotePlaybackMatcherStats().has_value());
        }
        {
            auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
            AudioFormat format;
            format.sampleRate = 48000;
            format.channels = 2;
            format.sample = AudioFormat::Sample::Float32;
            QVERIFY(bus->open(format));
            Rig rig = makeRig(std::move(bus));
            QCOMPARE(rig.engine->delayParts(AudioRole::Speakers).matcherFillMs, -1.0);
            QVERIFY(!rig.engine->remotePlaybackMatcherStats().has_value());
            QVERIFY(!rig.engine->remotePlaybackIntoMatcher());
        }
    }
};

QTEST_MAIN(TstAudioEngineMatcherPush)
#include "tst_audio_engine_matcher_push.moc"
