#include <QtTest/QtTest>
#include "core/audio/PortAudioBus.h"
#include <portaudio.h>

using namespace NereusSDR;

class TstPortAudioBus : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        Pa_Initialize();
    }
    void cleanupTestCase() {
        Pa_Terminate();
    }

    void constructsClosed() {
        PortAudioBus bus;
        QVERIFY(!bus.isOpen());
    }

    void openSucceedsOnDefaultDevice() {
        PortAudioBus bus;
        AudioFormat f;
        QVERIFY2(bus.open(f), qPrintable(bus.errorString()));
        QVERIFY(bus.isOpen());
        bus.close();
        QVERIFY(!bus.isOpen());
    }

    void negotiatedFormatReflectsDevice() {
        PortAudioBus bus;
        AudioFormat f;
        f.sampleRate = 48000;
        f.channels = 2;
        bus.open(f);
        QVERIFY(bus.negotiatedFormat().sampleRate > 0);
        bus.close();
    }

    void backendNameIdentifiesAPI() {
        PortAudioBus bus;
        bus.open(AudioFormat{});
        const QString n = bus.backendName();
        QVERIFY(!n.isEmpty());
        bus.close();
    }

    void hostApisEnumerateNonEmpty() {
        const auto apis = PortAudioBus::hostApis();
        QVERIFY(!apis.isEmpty());
    }

    void outputDevicesEnumerateForFirstApi() {
        const auto apis = PortAudioBus::hostApis();
        QVERIFY(!apis.isEmpty());
        const auto devices = PortAudioBus::outputDevicesFor(apis.first().index);
        // Host system should have at least one output; skip if headless CI.
        if (devices.isEmpty()) { QSKIP("No output devices on test host"); }
    }

    void openInputSucceedsOnDefaultDevice() {
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
};

QTEST_APPLESS_MAIN(TstPortAudioBus)
#include "tst_port_audio_bus.moc"
