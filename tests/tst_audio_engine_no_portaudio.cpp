// =================================================================
// tests/tst_audio_engine_no_portaudio.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  A test run never initialises
// PortAudio (R-R3-21, R-R3-23). Pa_Initialize on Linux opens every ALSA
// PCM to probe it and on macOS walks CoreAudio, so the device guard in
// AudioEngine::makeBus alone still let a test touch the real audio
// devices. Constructing an AudioEngine, and a RadioModel (which builds its
// own AudioEngine), must make no Pa_Initialize or Pa_Terminate call, and
// the device lists come back empty.
//
// Modification history (NereusSDR):
//   2026-09-24: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QStandardPaths>

#include <portaudio.h>

#include "core/AudioEngine.h"
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
};

QTEST_MAIN(TstAudioEngineNoPortAudio)
#include "tst_audio_engine_no_portaudio.moc"
