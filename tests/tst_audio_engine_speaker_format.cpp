// =================================================================
// tests/tst_audio_engine_speaker_format.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Local speakers at any format;
// no upstream logic is ported here.
//
// R3 receiver audio fix wave (R-R3-23, R-R3-07): a local window pushed the
// 48 kHz stereo master mix to the speakers whatever format the speaker
// device was opened at, so a 96 kHz device played it at twice the speed
// and a mono device got interleaved stereo read as mono. The mix now
// reaches the speakers at the device's own rate and channel count: every
// rate the Devices page offers from 44.1 to 192 kHz, stereo and mono.
//
// A fake speaker device (FakeAudioBus, opened at the row's format) stands
// in for PortAudio; nothing opens this computer's real speakers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio fix wave.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slices set to AF 100: AF is the
//                                    mixer level now. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kLeftHz = 700.0;
constexpr double kRightHz = 1900.0;
constexpr double kLeftAmp = 0.2;
constexpr double kRightAmp = 0.1;
constexpr int kBlockFrames = 64;     // one DSP block at 48 kHz
constexpr int kSeconds = 2;

// A tone's amplitude in one channel of interleaved audio at `rateHz`.
double toneAmplitude(const float* samples, qsizetype count, int channels, int channel,
                     double hz, int rateHz, qsizetype firstFrame)
{
    double cosine = 0.0;
    double sine = 0.0;
    qsizetype frames = 0;
    for (qsizetype f = firstFrame; f * channels + channel < count; ++f) {
        const double phase = 2.0 * kPi * hz * double(f) / double(rateHz);
        const double v = samples[f * channels + channel];
        cosine += v * std::cos(phase);
        sine += v * std::sin(phase);
        ++frames;
    }
    return frames > 0 ? 2.0 * std::hypot(cosine, sine) / double(frames) : 0.0;
}

} // namespace

class TstAudioEngineSpeakerFormat : public QObject {
    Q_OBJECT

private slots:
    void playsAtTheSpeakersOwnFormat_data()
    {
        QTest::addColumn<int>("rate");
        QTest::addColumn<int>("channels");
        QTest::newRow("44.1 kHz stereo") << 44100 << 2;
        QTest::newRow("48 kHz stereo") << 48000 << 2;
        QTest::newRow("96 kHz stereo") << 96000 << 2;
        QTest::newRow("192 kHz stereo") << 192000 << 2;
        QTest::newRow("48 kHz mono") << 48000 << 1;
        QTest::newRow("44.1 kHz mono") << 44100 << 1;
    }

    void playsAtTheSpeakersOwnFormat()
    {
        QFETCH(int, rate);
        QFETCH(int, channels);

        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();
        auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = rate;
        format.channels = channels;
        format.sample = AudioFormat::Sample::Float32;
        QVERIFY(speakers->open(format));
        FakeAudioBus* device = speakers.get();
        engine->setSpeakersBusForTest(std::move(speakers));
        engine->setVolume(1.0f);
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        const int slice = radio->addSlice();
        QVERIFY(slice >= 0);
        // AF is the mixer level now; these tests measure unity gain.
        radio->sliceById(slice)->setAfGain(100);

        // Two seconds of the 48 kHz stereo mix: 700 Hz left, 1900 Hz right.
        std::vector<float> block(kBlockFrames * 2);
        const int totalFrames = kSeconds * 48000;
        for (int sent = 0; sent < totalFrames; sent += kBlockFrames) {
            for (int i = 0; i < kBlockFrames; ++i) {
                const double t = double(sent + i) / 48000.0;
                block[size_t(2 * i)] = float(kLeftAmp * std::sin(2.0 * kPi * kLeftHz * t));
                block[size_t(2 * i + 1)] = float(kRightAmp * std::sin(2.0 * kPi * kRightHz * t));
            }
            engine->rxBlockReady(slice, block.data(), kBlockFrames);
        }

        const auto* played = reinterpret_cast<const float*>(device->buffer().constData());
        const qsizetype count = device->buffer().size() / qsizetype(sizeof(float));
        QCOMPARE(count % channels, qsizetype(0));
        const qsizetype frames = count / channels;
        // Two seconds at the device's own rate, less the resampler's delay.
        QVERIFY2(frames <= qsizetype(kSeconds) * rate
                     && frames >= qsizetype(kSeconds) * rate - rate / 20,
                 qPrintable(QStringLiteral("%1 frames at %2 Hz").arg(frames).arg(rate)));

        // Past the start (the mix fades in, the resampler fills).
        const qsizetype skip = rate / 4;
        if (channels == 2) {
            const double leftLow = toneAmplitude(played, count, 2, 0, kLeftHz, rate, skip);
            const double leftHigh = toneAmplitude(played, count, 2, 0, kRightHz, rate, skip);
            const double rightHigh = toneAmplitude(played, count, 2, 1, kRightHz, rate, skip);
            const double rightLow = toneAmplitude(played, count, 2, 1, kLeftHz, rate, skip);
            QVERIFY2(std::abs(leftLow - kLeftAmp) < 0.005 && leftHigh < 0.002,
                     qPrintable(QStringLiteral("left: %1 Hz %2, %3 Hz %4")
                                    .arg(kLeftHz).arg(leftLow).arg(kRightHz).arg(leftHigh)));
            QVERIFY2(std::abs(rightHigh - kRightAmp) < 0.005 && rightLow < 0.002,
                     qPrintable(QStringLiteral("right: %1 Hz %2, %3 Hz %4")
                                    .arg(kRightHz).arg(rightHigh).arg(kLeftHz).arg(rightLow)));
        } else {
            // A mono speaker hears both channels, mixed: (L + R) / 2, as a
            // remote window's mono speaker does.
            const double low = toneAmplitude(played, count, 1, 0, kLeftHz, rate, skip);
            const double high = toneAmplitude(played, count, 1, 0, kRightHz, rate, skip);
            QVERIFY2(std::abs(low - kLeftAmp / 2.0) < 0.005
                         && std::abs(high - kRightAmp / 2.0) < 0.005,
                     qPrintable(QStringLiteral("mono: %1 Hz %2, %3 Hz %4")
                                    .arg(kLeftHz).arg(low).arg(kRightHz).arg(high)));
        }
    }

    // R-R3-45 fix wave: the headphones output converts too. A receiver on
    // the headphones plays at the headphones device's own rate and channel
    // count (its own converter), and nothing of it reaches the speakers.
    void headphonesPlayAtTheirOwnFormat_data()
    {
        QTest::addColumn<int>("rate");
        QTest::addColumn<int>("channels");
        QTest::newRow("44.1k-stereo") << 44100 << 2;
        QTest::newRow("48k-mono") << 48000 << 1;
    }

    void headphonesPlayAtTheirOwnFormat()
    {
        QFETCH(int, rate);
        QFETCH(int, channels);

        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();
        auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat speakerFormat;
        speakerFormat.sampleRate = 48000;
        speakerFormat.channels = 2;
        speakerFormat.sample = AudioFormat::Sample::Float32;
        QVERIFY(speakers->open(speakerFormat));
        FakeAudioBus* speakerDevice = speakers.get();
        engine->setSpeakersBusForTest(std::move(speakers));
        auto headphones = std::make_unique<FakeAudioBus>(QStringLiteral("FakeHeadphones"));
        AudioFormat format;
        format.sampleRate = rate;
        format.channels = channels;
        format.sample = AudioFormat::Sample::Float32;
        QVERIFY(headphones->open(format));
        FakeAudioBus* device = headphones.get();
        engine->setHeadphonesBusForTest(std::move(headphones));
        engine->setVolume(1.0f);
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        const int slice = radio->addSlice();
        QVERIFY(slice >= 0);
        // AF is the mixer level now; these tests measure unity gain.
        radio->sliceById(slice)->setAfGain(100);
        const auto restore = qScopeGuard([&] {
            radio->sliceById(slice)->setOutputRoute(SliceModel::OutputRoute::Speakers);
            AppSettings::instance().remove(QStringLiteral("Slice%1/OutputRoute").arg(slice));
        });
        radio->sliceById(slice)->setOutputRoute(SliceModel::OutputRoute::Headphones);

        std::vector<float> block(kBlockFrames * 2);
        const int totalFrames = kSeconds * 48000;
        for (int sent = 0; sent < totalFrames; sent += kBlockFrames) {
            for (int i = 0; i < kBlockFrames; ++i) {
                const double t = double(sent + i) / 48000.0;
                block[size_t(2 * i)] = float(kLeftAmp * std::sin(2.0 * kPi * kLeftHz * t));
                block[size_t(2 * i + 1)] = float(kRightAmp * std::sin(2.0 * kPi * kRightHz * t));
            }
            engine->rxBlockReady(slice, block.data(), kBlockFrames);
        }

        const auto* played = reinterpret_cast<const float*>(device->buffer().constData());
        const qsizetype count = device->buffer().size() / qsizetype(sizeof(float));
        QCOMPARE(count % channels, qsizetype(0));
        const qsizetype frames = count / channels;
        QVERIFY2(frames <= qsizetype(kSeconds) * rate
                     && frames >= qsizetype(kSeconds) * rate - rate / 20,
                 qPrintable(QStringLiteral("%1 frames at %2 Hz").arg(frames).arg(rate)));
        const qsizetype skip = rate / 4;
        if (channels == 2) {
            const double left = toneAmplitude(played, count, 2, 0, kLeftHz, rate, skip);
            const double right = toneAmplitude(played, count, 2, 1, kRightHz, rate, skip);
            QVERIFY2(std::abs(left - kLeftAmp) < 0.005 && std::abs(right - kRightAmp) < 0.005,
                     qPrintable(QStringLiteral("headphones: %1, %2").arg(left).arg(right)));
        } else {
            const double low = toneAmplitude(played, count, 1, 0, kLeftHz, rate, skip);
            const double high = toneAmplitude(played, count, 1, 0, kRightHz, rate, skip);
            QVERIFY2(std::abs(low - kLeftAmp / 2.0) < 0.005
                         && std::abs(high - kRightAmp / 2.0) < 0.005,
                     qPrintable(QStringLiteral("headphones mono: %1, %2").arg(low).arg(high)));
        }
        // The speakers heard none of it.
        const auto* spk = reinterpret_cast<const float*>(speakerDevice->buffer().constData());
        const qsizetype spkCount = speakerDevice->buffer().size() / qsizetype(sizeof(float));
        for (qsizetype i = 0; i < spkCount; ++i) {
            QVERIFY(std::abs(spk[i]) < 1.0e-6f);
        }
    }
};

QTEST_MAIN(TstAudioEngineSpeakerFormat)
#include "tst_audio_engine_speaker_format.moc"
