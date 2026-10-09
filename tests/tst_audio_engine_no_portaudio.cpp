// =================================================================
// tests/tst_audio_engine_no_portaudio.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  A test run never initialises
// PortAudio (R-R3-21, R-R3-23). Pa_Initialize on Linux opens every ALSA
// PCM to probe it and on macOS walks CoreAudio, so the device guard in
// AudioEngine::makeBus alone still let a test touch the real audio
// devices. Constructing an AudioEngine, and a RadioModel (which builds its
// own AudioEngine), must make no Pa_Initialize or Pa_Terminate call, and
// the device lists come back empty.  Native audio plan Task 7 (R-AUD-32):
// the registry's PortAudio backend lists nothing and its Rescan calls
// nothing either, with the engine running on the system's own backends.
//
// Modification history (NereusSDR):
//   2026-09-24: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-32): the system backends
//               and Rescan make no PortAudio call. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-32): on the Mac
//               the system's engine is Core Audio, which lists nothing in
//               a test run either. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include <portaudio.h>

#include "core/AudioEngine.h"
#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "core/audio/PortAudioBus.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstAudioEngineNoPortAudio : public QObject {
    Q_OBJECT
private slots:
    void testRunIsDecidedBeforeAnyEngine()
    {
        // TestSandboxInit.cpp enables test mode from a static constructor,
        // before main, so it is in force before any AudioEngine exists.
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(PortAudioBus::portAudioBarredForTestRun());
    }

    void engineAndRadioModelNeverInitialisePortAudio()
    {
        {
            AudioEngine engine;
            RadioModel radio;
            QVERIFY(radio.audioEngine() != nullptr);
            QCOMPARE(AudioEngine::paInitializeCallsForTest(), 0);
            QVERIFY(PortAudioBus::hostApis().isEmpty());
            QVERIFY(PortAudioBus::outputDevicesFor(0).isEmpty());
            QVERIFY(PortAudioBus::inputDevicesFor(0).isEmpty());
        }
        QCOMPARE(AudioEngine::paInitializeCallsForTest(), 0);
        QCOMPARE(AudioEngine::paTerminateCallsForTest(), 0);
        // PortAudio's own view, independent of the counter: it answers
        // "not initialised" from its initialisation count alone and touches
        // no device doing so.
        QCOMPARE(Pa_GetDeviceCount(), PaDeviceIndex(paNotInitialized));
    }

    // R-AUD-32: the engine on the system's own backends (PortAudio here,
    // until the native engine tasks) lists and rescans without a
    // PortAudio call.
    void systemBackendsAndRescanNeverInitialisePortAudio()
    {
        QVERIFY(listPortAudioDevices().isEmpty());
        {
            AudioEngine engine;
            engine.setAudioBackendsForTest(makeSystemAudioBackends(AudioBackendContext{}));
            engine.start();
            QVERIFY(engine.catalogue() != nullptr);
#ifdef Q_OS_MAC
            // R-AUD-01: Core Audio alone, and in a test run it walks no
            // device either.
            QCOMPARE(engine.defaultEngine(), AudioEngineKind::CoreAudio);
            QVERIFY(engine.catalogue()
                        ->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output)
                        .isEmpty());
#else
            QCOMPARE(engine.defaultEngine(), AudioEngineKind::PortAudio);
#endif
            QVERIFY(engine.catalogue()
                        ->devices(AudioBackendId::PortAudio, AudioDeviceDirection::Output)
                        .isEmpty());
            // Nothing is listed, so the speakers have no system default.
            QCOMPARE(engine.roleStatus(AudioRole::Speakers).state, AudioRoleState::Silent);
            QCOMPARE(engine.roleStatus(AudioRole::Speakers).reason, AudioRoleReason::NoDevice);

            QSignalSpy rescanned(engine.catalogue(), &IAudioDeviceCatalog::olderDriversRescanned);
            engine.rescanOlderDrivers();
            QVERIFY(rescanned.wait(5000));
            engine.stop();
        }
        QCOMPARE(AudioEngine::paInitializeCallsForTest(), 0);
        QCOMPARE(AudioEngine::paTerminateCallsForTest(), 0);
        QCOMPARE(Pa_GetDeviceCount(), PaDeviceIndex(paNotInitialized));
    }
};

QTEST_MAIN(TstAudioEngineNoPortAudio)
#include "tst_audio_engine_no_portaudio.moc"
