#include <QtTest/QtTest>
#include "core/audio/PortAudioBus.h"
#include <portaudio.h>

#include <QStandardPaths>

#include <cmath>
#include <cstddef>
#include <vector>

using namespace NereusSDR;

namespace {

// R-R3-21: a test run never initialises PortAudio (it probes every ALSA
// PCM on Linux and walks CoreAudio on macOS) and never opens a real audio
// device. The cases below that need one run only when a developer opts in
// on their own machine:
//
//     NEREUS_TEST_REAL_AUDIO_DEVICES=1 ./tests/tst_port_audio_bus
//
// With the variable set, this test leaves test mode, initialises
// PortAudio and opens this computer's real output and input devices (on
// macOS the input case may ask for microphone access).
// Without it those cases skip and say how to run them.
bool realDevicesOptedIn()
{
    return qEnvironmentVariable("NEREUS_TEST_REAL_AUDIO_DEVICES") == QStringLiteral("1");
}

constexpr const char* kRealDevicesSkip =
    "Opens real audio devices: set NEREUS_TEST_REAL_AUDIO_DEVICES=1 to run";

} // namespace

class TstPortAudioBus : public QObject {
    Q_OBJECT
private:
    bool m_paInitialized{false};

private slots:
    void initTestCase() {
        if (!realDevicesOptedIn()) {
            return;
        }
        // Opted in: leave test mode so PortAudioBus stops barring PortAudio,
        // and own its lifetime here as the application does.
        QStandardPaths::setTestModeEnabled(false);
        QVERIFY(!PortAudioBus::portAudioBarredForTestRun());
        const PaError err = Pa_Initialize();
        QVERIFY2(err == paNoError, Pa_GetErrorText(err));
        m_paInitialized = true;
    }

    void cleanupTestCase() {
        if (m_paInitialized) {
            Pa_Terminate();
            m_paInitialized = false;
        }
    }

    void constructsClosed() {
        PortAudioBus bus;
        QVERIFY(!bus.isOpen());
    }

    void openSucceedsOnDefaultDevice() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        PortAudioBus bus;
        AudioFormat f;
        // 2026-05-12 (PR #238 follow-up): CI runner has no output
        // device, so bus.open() returns false with "No output device
        // found".  Skip rather than fail — the open contract is
        // exercised on dev machines + bench; CI just smoke-tests
        // that the code builds and links against PortAudio.
        if (!bus.open(f)) {
            QSKIP(qPrintable(QStringLiteral("No default output device on host: ")
                             + bus.errorString()));
        }
        QVERIFY(bus.isOpen());
        bus.close();
        QVERIFY(!bus.isOpen());
    }

    void negotiatedFormatReflectsDevice() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        PortAudioBus bus;
        AudioFormat f;
        f.sampleRate = 48000;
        f.channels = 2;
        bus.open(f);
        QVERIFY(bus.negotiatedFormat().sampleRate > 0);
        bus.close();
    }

    void backendNameIdentifiesAPI() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        PortAudioBus bus;
        // 2026-05-12 (PR #238 follow-up): same headless-CI caveat as
        // openSucceedsOnDefaultDevice — the backend name is only
        // populated after a successful open(), so skip when no
        // output device is available.
        if (!bus.open(AudioFormat{})) {
            QSKIP(qPrintable(QStringLiteral("No default output device on host: ")
                             + bus.errorString()));
        }
        const QString n = bus.backendName();
        QVERIFY(!n.isEmpty());
        bus.close();
    }

    // R-R3-21: in a test run the device lists are empty and PortAudio is
    // never asked.
    void deviceListsAreEmptyInATestRun() {
        if (realDevicesOptedIn()) {
            QSKIP("Real audio devices opted in: this run is not a test run");
        }
        QVERIFY(PortAudioBus::portAudioBarredForTestRun());
        QVERIFY(PortAudioBus::hostApis().isEmpty());
        QVERIFY(PortAudioBus::outputDevicesFor(0).isEmpty());
        QVERIFY(PortAudioBus::inputDevicesFor(0).isEmpty());
    }

    void openInputSucceedsOnDefaultDevice() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        PortAudioBus bus;
        PortAudioConfig cfg;
        cfg.direction = AudioDirection::Input;
        bus.setConfig(cfg);
        AudioFormat f;
        if (!bus.open(f)) {
            QSKIP(qPrintable(QStringLiteral("No default input device: ") + bus.errorString()));
        }
        QVERIFY(bus.isOpen());
        QVERIFY(bus.negotiatedFormat().sampleRate > 0);
        QVERIFY(!bus.backendName().isEmpty());
        bus.close();
        QVERIFY(!bus.isOpen());
    }

    // Issue #112: open() must honor cfg.deviceName. Pick the first
    // enumerated output device, feed its name through PortAudioConfig,
    // and verify the bus opens — previously open() always called
    // Pa_GetDefaultOutputDevice() and ignored the configured name.
    void openHonorsConfiguredDeviceName() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        const auto apis = PortAudioBus::hostApis();
        if (apis.isEmpty()) { QSKIP("No PortAudio host APIs on test host"); }

        PortAudioBus::DeviceInfo target{};
        bool found = false;
        for (const auto& api : apis) {
            const auto devs = PortAudioBus::outputDevicesFor(api.index);
            if (!devs.isEmpty()) { target = devs.first(); found = true; break; }
        }
        if (!found) { QSKIP("No output devices on test host"); }

        PortAudioBus bus;
        PortAudioConfig cfg;
        cfg.direction    = AudioDirection::Output;
        cfg.hostApiIndex = target.hostApiIndex;
        cfg.deviceName   = target.name;
        bus.setConfig(cfg);

        AudioFormat f;
        QVERIFY2(bus.open(f), qPrintable(bus.errorString()));
        QVERIFY(bus.isOpen());
        bus.close();
    }

    // Issue #112: when cfg.deviceName doesn't match anything, open() must
    // fall back through (host-api default → global default → first
    // enumerated output device) rather than erroring out — on systems
    // without a configured ALSA default, that final fallback is the only
    // thing that keeps audio reaching the user.
    void openFallsBackWhenNameMissing() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        PortAudioBus bus;
        PortAudioConfig cfg;
        cfg.direction  = AudioDirection::Output;
        cfg.deviceName = QStringLiteral("::no_such_device_name_12345::");
        bus.setConfig(cfg);

        AudioFormat f;
        if (!bus.open(f)) {
            QSKIP(qPrintable(QStringLiteral("No output devices on test host: ")
                             + bus.errorString()));
        }
        QVERIFY(bus.isOpen());
        bus.close();
    }

    // Issue #115 / Codex P2: step-4 fallback must respect the requested
    // channel count. If the host has at least one stereo-capable output
    // device, a stereo request must not end up on a mono device — even
    // if a mono device was enumerated first.
    void openFallbackRespectsChannelCapacity() {
        if (!realDevicesOptedIn()) { QSKIP(kRealDevicesSkip); }
        // Find any stereo-capable output; skip if the host has none.
        const auto apis = PortAudioBus::hostApis();
        bool hasStereoOutput = false;
        for (const auto& api : apis) {
            const auto devs = PortAudioBus::outputDevicesFor(api.index);
            for (const auto& d : devs) {
                if (d.maxOutputChannels >= 2) { hasStereoOutput = true; break; }
            }
            if (hasStereoOutput) { break; }
        }
        if (!hasStereoOutput) {
            QSKIP("No stereo-capable output device on test host");
        }

        PortAudioBus bus;
        PortAudioConfig cfg;
        cfg.direction  = AudioDirection::Output;
        cfg.deviceName = QStringLiteral("::force_fallback_::");
        bus.setConfig(cfg);

        AudioFormat f;
        f.channels = 2;
        QVERIFY2(bus.open(f), qPrintable(bus.errorString()));
        QVERIFY(bus.isOpen());
        // negotiatedFormat carries what we asked for; the device must
        // have had capacity for it or Pa_OpenStream would have failed.
        QCOMPARE(bus.negotiatedFormat().channels, 2);
        bus.close();
    }

    // R-AUD-15: an output stream queues in its clock matcher.  push()
    // takes 48 kHz stereo; the callback reads the matcher and writes the
    // stream's channels; outputPacing reports every frame the device asked
    // for, the matcher's fill and its automatic size.
    void outputPacingTracksMatcherReadsAndDryRun() {
        PortAudioBus bus;
        bus.m_cfg.direction = AudioDirection::Output;
        bus.m_cfg.bufferSamples = kCallback;
        QVERIFY(bus.takesStereoMix());
        QVERIFY(!bus.outputPacing().has_value());
        QVERIFY(bus.prepareOutputMatcher(48000, 2));

        auto pacing = bus.outputPacing();
        QVERIFY(pacing.has_value());
        QCOMPARE(pacing->consumedFrames, quint64(0));
        // A fresh matcher starts with its starting fill queued as silence.
        QVERIFY(pacing->queuedFrames >= 0);
        QVERIFY(pacing->queuedFrames <= pacing->capacityFrames);
        QCOMPARE(pacing->callbackFrames, kCallback);

        std::vector<float> block(std::size_t(2 * kCallback));
        for (std::size_t i = 0; i < block.size(); i += 2) {
            block[i] = 0.5f;
            block[i + 1] = -0.5f;
        }
        std::vector<float> out(std::size_t(2 * kCallback));
        constexpr int kBlocks = 400;   // a little over a second
        for (int i = 0; i < kBlocks; ++i) {
            QCOMPARE(bus.push(reinterpret_cast<const char*>(block.data()),
                              qint64(block.size() * sizeof(float))),
                     qint64(block.size() * sizeof(float)));
            QCOMPARE(PortAudioBus::paCallback(nullptr, out.data(), kCallback, nullptr, 0, &bus),
                     paContinue);
        }
        // Steady state: the device plays the mix as written.
        for (std::size_t i = 0; i < out.size(); i += 2) {
            QVERIFY2(std::abs(out[i] - 0.5f) < 1e-3f, qPrintable(QString::number(out[i])));
            QVERIFY2(std::abs(out[i + 1] + 0.5f) < 1e-3f, qPrintable(QString::number(out[i + 1])));
        }
        pacing = bus.outputPacing();
        QCOMPARE(pacing->consumedFrames, quint64(kBlocks) * quint64(kCallback));
        QVERIFY(pacing->capacityFrames > 0);
        QVERIFY(pacing->queuedFrames >= 0);
        QVERIFY(pacing->queuedFrames <= pacing->capacityFrames);

        const AudioDelayParts parts = bus.delayParts();
        QVERIFY(parts.matcherFillMs >= 0.0);
        QCOMPARE(parts.deviceBufferMs, 1000.0 * kCallback / 48000.0);
        QCOMPARE(parts.deviceLatencyMs, 0.0);
        QVERIFY(parts.totalMs() >= parts.deviceBufferMs);

        const auto before = bus.matcherStats();
        QVERIFY(before.has_value());

        // The writer stops: the device keeps asking, the matcher runs dry
        // and slews to silence, and every asked-for frame still counts.
        for (int i = 0; i < 20; ++i) {
            QCOMPARE(PortAudioBus::paCallback(nullptr, out.data(), kCallback, nullptr, 0, &bus),
                     paContinue);
        }
        for (float sample : out) {
            QCOMPARE(sample, 0.0f);
        }
        pacing = bus.outputPacing();
        QCOMPARE(pacing->consumedFrames, quint64(kBlocks + 20) * quint64(kCallback));
        QVERIFY(bus.matcherStats()->dryRuns > before->dryRuns);
    }

    // A one-channel device hears left plus right, halved; a stream opened
    // with more channels than the pair gets silence on the others.
    void outputWritesTheStreamsOwnChannels() {
        for (const int channels : {1, 4}) {
            PortAudioBus bus;
            bus.m_cfg.direction = AudioDirection::Output;
            bus.m_cfg.bufferSamples = kCallback;
            QVERIFY(bus.prepareOutputMatcher(48000, channels));
            std::vector<float> block(std::size_t(2 * kCallback));
            for (std::size_t i = 0; i < block.size(); i += 2) {
                block[i] = 0.2f;
                block[i + 1] = 0.6f;
            }
            std::vector<float> out(std::size_t(channels * kCallback));
            for (int i = 0; i < 100; ++i) {
                bus.push(reinterpret_cast<const char*>(block.data()),
                         qint64(block.size() * sizeof(float)));
                PortAudioBus::paCallback(nullptr, out.data(), kCallback, nullptr, 0, &bus);
            }
            for (int f = 0; f < kCallback; ++f) {
                const float* frame = out.data() + std::ptrdiff_t(f) * channels;
                if (channels == 1) {
                    QVERIFY(std::abs(frame[0] - 0.4f) < 1e-3f);
                } else {
                    QVERIFY(std::abs(frame[0] - 0.2f) < 1e-3f);
                    QVERIFY(std::abs(frame[1] - 0.6f) < 1e-3f);
                    QCOMPARE(frame[2], 0.0f);
                    QCOMPARE(frame[3], 0.0f);
                }
            }
        }
    }

    // flush() asks the matcher to drop what is queued at its next write:
    // after one more block none of the flushed audio reaches the device.
    void outputFlushDropsQueuedAudio() {
        PortAudioBus bus;
        bus.m_cfg.direction = AudioDirection::Output;
        bus.m_cfg.bufferSamples = kCallback;
        QVERIFY(bus.prepareOutputMatcher(48000, 2));
        std::vector<float> loud(std::size_t(2 * kCallback), 0.25f);
        std::vector<float> out(std::size_t(2 * kCallback));
        for (int i = 0; i < 200; ++i) {
            bus.push(reinterpret_cast<const char*>(loud.data()),
                     qint64(loud.size() * sizeof(float)));
            PortAudioBus::paCallback(nullptr, out.data(), kCallback, nullptr, 0, &bus);
        }
        QVERIFY(std::abs(out[0] - 0.25f) < 1e-3f);

        bus.flush();
        std::vector<float> quiet(std::size_t(2 * kCallback), 0.0f);
        for (int i = 0; i < 40; ++i) {
            bus.push(reinterpret_cast<const char*>(quiet.data()),
                     qint64(quiet.size() * sizeof(float)));
            PortAudioBus::paCallback(nullptr, out.data(), kCallback, nullptr, 0, &bus);
        }
        for (float sample : out) {
            QVERIFY2(std::abs(sample) < 1e-3f, qPrintable(QString::number(sample)));
        }
        // An input stream holds no matcher and reports none of it.
        PortAudioBus input;
        input.m_cfg.direction = AudioDirection::Input;
        QVERIFY(!input.takesStereoMix());
        QVERIFY(!input.matcherStats().has_value());
        QCOMPARE(input.delayParts().matcherFillMs, -1.0);
    }

    // restartClockMatch() reaches the matcher; a closed output takes no mix.
    void closedOutputTakesNoMix() {
        PortAudioBus bus;
        bus.m_cfg.direction = AudioDirection::Output;
        std::vector<float> block(std::size_t(2 * kCallback), 0.1f);
        QCOMPARE(bus.push(reinterpret_cast<const char*>(block.data()),
                          qint64(block.size() * sizeof(float))),
                 qint64(0));
        bus.restartClockMatch();   // no matcher: no effect
        QVERIFY(!bus.matcherStats().has_value());
    }

private:
    static constexpr int kCallback = 128;
};

QTEST_APPLESS_MAIN(TstPortAudioBus)
#include "tst_port_audio_bus.moc"
