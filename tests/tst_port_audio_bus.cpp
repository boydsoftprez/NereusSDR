#include <QtTest/QtTest>
#include "core/audio/PortAudioBus.h"
#include <portaudio.h>

#include <QStandardPaths>

#include <cmath>

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

    void outputPacingTracksActualCallbackDrainAndUnderrun() {
        PortAudioBus bus;
        bus.m_cfg.direction = AudioDirection::Output;
        bus.m_negFormat = AudioFormat{48000, 2, AudioFormat::Sample::Float32};
        bus.m_ring[0] = 0.1f;
        bus.m_ring[1] = 0.2f;
        bus.m_ring[2] = 0.3f;
        bus.m_ring[3] = 0.4f;
        bus.m_ring[4] = 0.5f;
        bus.m_ring[5] = 0.6f;
        bus.m_ring[6] = 0.7f;
        bus.m_ring[7] = 0.8f;
        bus.m_ringRead.store(0, std::memory_order_relaxed);
        bus.m_ringWrite.store(8, std::memory_order_relaxed);

        auto pacing = bus.outputPacing();
        QVERIFY(pacing.has_value());
        QCOMPARE(pacing->consumedFrames, quint64(0));
        QCOMPARE(pacing->queuedFrames, 4);
        QCOMPARE(pacing->capacityFrames, static_cast<int>(bus.m_ring.size() / 2));

        float first[4] = {};
        QCOMPARE(PortAudioBus::paCallback(nullptr, first, 2, nullptr, 0, &bus), paContinue);
        QCOMPARE(first[0], 0.1f);
        QCOMPARE(first[3], 0.4f);
        pacing = bus.outputPacing();
        QCOMPARE(pacing->consumedFrames, quint64(2));
        QCOMPARE(pacing->queuedFrames, 2);

        float second[4] = {};
        QCOMPARE(PortAudioBus::paCallback(nullptr, second, 2, nullptr, 0, &bus), paContinue);
        QCOMPARE(second[0], 0.5f);
        QCOMPARE(second[3], 0.8f);

        float underrun[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        QCOMPARE(PortAudioBus::paCallback(nullptr, underrun, 2, nullptr, 0, &bus), paContinue);
        for (float sample : underrun) {
            QCOMPARE(sample, 0.0f);
        }
        pacing = bus.outputPacing();
        QCOMPARE(pacing->consumedFrames, quint64(6));
        QCOMPARE(pacing->queuedFrames, 0);
        QCOMPARE(pacing->capacityFrames, static_cast<int>(bus.m_ring.size() / 2));
    }

    void outputFlushFloorSurvivesStaleCallbackPublication() {
        PortAudioBus bus;
        bus.m_cfg.direction = AudioDirection::Output;
        bus.m_negFormat = AudioFormat{48000, 2, AudioFormat::Sample::Float32};
        bus.m_ring[0] = 0.25f;
        bus.m_ring[1] = -0.25f;
        bus.m_ring[2] = 0.25f;
        bus.m_ring[3] = -0.25f;
        bus.m_ring[4] = 0.25f;
        bus.m_ring[5] = -0.25f;
        bus.m_ring[6] = 0.25f;
        bus.m_ring[7] = -0.25f;
        bus.m_ringRead.store(0, std::memory_order_relaxed);
        bus.m_ringWrite.store(8, std::memory_order_relaxed);

        bus.flush();
        auto pacing = bus.outputPacing();
        QVERIFY(pacing.has_value());
        QCOMPARE(pacing->queuedFrames, 0);

        // Models a callback that loaded read=0 before flush and publishes
        // that stale position later. The discard floor remains authoritative.
        bus.m_ringRead.store(0, std::memory_order_release);
        pacing = bus.outputPacing();
        QCOMPARE(pacing->queuedFrames, 0);

        float discarded[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        QCOMPARE(PortAudioBus::paCallback(nullptr, discarded, 2, nullptr, 0, &bus), paContinue);
        for (float sample : discarded) {
            QCOMPARE(sample, 0.0f);
        }
        QCOMPARE(bus.m_ringRead.load(std::memory_order_acquire), qint64(8));

        // New audio after the flush may ramp from silence, but none of the
        // discarded 0.25/-0.25 samples may reappear.
        bus.m_ring[8] = 0.75f;
        bus.m_ring[9] = -0.75f;
        bus.m_ring[10] = 0.75f;
        bus.m_ring[11] = -0.75f;
        bus.m_ringWrite.store(12, std::memory_order_release);
        float fresh[4] = {};
        QCOMPARE(PortAudioBus::paCallback(nullptr, fresh, 2, nullptr, 0, &bus), paContinue);
        for (float sample : fresh) {
            QVERIFY(std::abs(sample) < 0.02f);
            QVERIFY(std::abs(std::abs(sample) - 0.25f) > 0.10f);
        }
        pacing = bus.outputPacing();
        QCOMPARE(pacing->consumedFrames, quint64(4));
        QCOMPARE(pacing->queuedFrames, 0);
    }
};

QTEST_APPLESS_MAIN(TstPortAudioBus)
#include "tst_port_audio_bus.moc"
