// =================================================================
// tests/tst_audio_engine_reset_audio_settings.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine::resetAudioSettings() — Sub-Phase 12 Task 12.4.
//
// Verifies the clear-vs-preserve boundary from addendum §2.5:
//   Clear: all audio/* keys.
//   Preserve: slice/<N>/VaxChannel, tx/OwnerSlot.
//
// Also verifies signal emission: speakersConfigChanged, vaxConfigChanged
// (channels 1–4), and audioSettingsReset.
//
// Uses NEREUS_BUILD_TESTS seam for fake bus injection so the test
// does not require a real PortAudio or CoreAudio backend. R3 receiver
// audio fix wave follow-up (2026-09-23, J.J. Boyd KG4VCF, AI-assisted via
// Anthropic Claude Code): the reset's rebuilt speakers and VAX devices are
// fakes and the run is in test mode; before this it opened this
// computer's real default output and VAX devices.
//
// Cross-platform. No radioModel required (AudioEngine standalone
// construction with a dedicated AppSettings instance).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>

using namespace NereusSDR;

namespace {

// R3 receiver audio fix wave follow-up: every device the engine opens is a
// fake, and test mode (initTestCase) stops anything else from reaching this
// computer's real speakers, microphone or VAX devices. `opened` counts the
// fake devices made.
void useFakeDevices(AudioEngine* engine, int* opened)
{
    engine->setDeviceBusFactoryForTest([opened](const AudioDeviceConfig&, bool) {
        if (opened) { ++*opened; }
        return std::make_unique<FakeAudioBus>(QStringLiteral("FakeDevice"));
    });
    engine->setVaxBusFactoryForTest([opened](int channel) -> std::unique_ptr<IAudioBus> {
        if (opened) { ++*opened; }
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax%1").arg(channel));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        bus->open(fmt);
        return bus;
    });
}

} // namespace

class TstAudioEngineResetAudioSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    // Clear boundary: all audio/* keys are removed after reset.
    void clearsAudioSpeakersKeys();
    void clearsAudioVaxKeys();
    void clearsAudioDspRateAndBlockSize();
    void clearsAudioVacFeedbackKeys();
    void clearsAudioFeatureFlagKeys();
    void clearsAudioFirstRunComplete();
    void clearsAudioLastDetectedCables();

    // Preserve boundary: slice/*/VaxChannel and tx/OwnerSlot survive.
    void preservesSliceVaxChannel();
    void preservesTxOwnerSlot();

    // Signals emitted on reset.
    void emitsSpeakersConfigChanged();
    void emitsVaxConfigChangedForAllChannels();
    void emitsAudioSettingsReset();
    // R-R3-45: the MON output choice goes with the rest.
    void putsTheMonitorBackOnTheSpeakers();
    void operatorLocalResetPreservesCoreAudioSettingsAndRebuildsOutputs();

private:
    int m_opened{0};   // fake devices made by the engines under test
    // Seed AppSettings with a known set of keys covering all §2.5 clear
    // categories plus the two preservation keys.
    void seedSettings(AppSettings& s);
};

// ---------------------------------------------------------------------------
void TstAudioEngineResetAudioSettings::initTestCase()
{
    // With no fake device supplied, the engine opens nothing real.
    QStandardPaths::setTestModeEnabled(true);
}

void TstAudioEngineResetAudioSettings::init()
{
    // Wipe the singleton's in-memory store before each test so previous
    // test runs don't bleed into each other.
    AppSettings::instance().clear();
}

void TstAudioEngineResetAudioSettings::seedSettings(AppSettings& s)
{
    // --- audio/* keys to be CLEARED ---
    s.setValue(QStringLiteral("audio/Speakers/DeviceName"),   QStringLiteral("BuiltIn"));
    s.setValue(QStringLiteral("audio/Speakers/SampleRate"),   QStringLiteral("48000"));
    s.setValue(QStringLiteral("audio/Headphones/DeviceName"), QStringLiteral("Phones"));
    s.setValue(QStringLiteral("audio/TxInput/DeviceName"),    QStringLiteral("Mic"));
    s.setValue(QStringLiteral("audio/Vax1/DeviceName"),       QStringLiteral("CABLE-A"));
    s.setValue(QStringLiteral("audio/Vax2/DeviceName"),       QStringLiteral("CABLE-B"));
    s.setValue(QStringLiteral("audio/Vax3/DeviceName"),       QStringLiteral("CABLE-C"));
    s.setValue(QStringLiteral("audio/Vax4/DeviceName"),       QStringLiteral("CABLE-D"));
    s.setValue(QStringLiteral("audio/DspRate"),               QStringLiteral("96000"));
    s.setValue(QStringLiteral("audio/DspBlockSize"),          QStringLiteral("512"));
    s.setValue(QStringLiteral("audio/VacFeedback/1/Gain"),    QStringLiteral("1.5000"));
    s.setValue(QStringLiteral("audio/VacFeedback/2/Gain"),    QStringLiteral("0.8000"));
    s.setValue(QStringLiteral("audio/SendIqToVax"),           QStringLiteral("True"));
    s.setValue(QStringLiteral("audio/TxMonitorToVax"),        QStringLiteral("True"));
    s.setValue(QStringLiteral("audio/MuteVaxDuringTxOnOtherSlice"), QStringLiteral("True"));
    s.setValue(QStringLiteral("audio/FirstRunComplete"),      QStringLiteral("True"));
    s.setValue(QStringLiteral("audio/LastDetectedCables"),    QStringLiteral("aaa,bbb"));

    // --- Keys to be PRESERVED ---
    s.setValue(QStringLiteral("slice/0/VaxChannel"), QStringLiteral("2"));
    s.setValue(QStringLiteral("slice/1/VaxChannel"), QStringLiteral("3"));
    s.setValue(QStringLiteral("tx/OwnerSlot"),       QStringLiteral("0"));
}

// ---------------------------------------------------------------------------
// Clear-boundary tests
// ---------------------------------------------------------------------------

void TstAudioEngineResetAudioSettings::clearsAudioSpeakersKeys()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/Speakers/DeviceName"),   QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/Speakers/SampleRate"),   QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/Headphones/DeviceName"), QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/TxInput/DeviceName"),    QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioVaxKeys()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/Vax1/DeviceName"), QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/Vax2/DeviceName"), QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/Vax3/DeviceName"), QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/Vax4/DeviceName"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioDspRateAndBlockSize()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/DspRate"),      QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/DspBlockSize"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioVacFeedbackKeys()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/VacFeedback/1/Gain"), QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/VacFeedback/2/Gain"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioFeatureFlagKeys()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/SendIqToVax"),                  QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/TxMonitorToVax"),               QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/MuteVaxDuringTxOnOtherSlice"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioFirstRunComplete()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/FirstRunComplete"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioLastDetectedCables()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/LastDetectedCables"), QString()).toString(), QString());
}

// ---------------------------------------------------------------------------
// Preserve-boundary tests
// ---------------------------------------------------------------------------

void TstAudioEngineResetAudioSettings::preservesSliceVaxChannel()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("slice/0/VaxChannel")).toString(),
             QStringLiteral("2"));
    QCOMPARE(s.value(QStringLiteral("slice/1/VaxChannel")).toString(),
             QStringLiteral("3"));
}

void TstAudioEngineResetAudioSettings::preservesTxOwnerSlot()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("tx/OwnerSlot")).toString(),
             QStringLiteral("0"));
}

// ---------------------------------------------------------------------------
// Signal-emission tests
// ---------------------------------------------------------------------------

void TstAudioEngineResetAudioSettings::emitsSpeakersConfigChanged()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    AudioEngine* engine = radio.audioEngine();
    m_opened = 0;
    useFakeDevices(engine, &m_opened);

    QSignalSpy spy(engine, &AudioEngine::speakersConfigChanged);
    engine->resetAudioSettings();

    // ensureSpeakersOpen() emits speakersConfigChanged after rebuilding the
    // default bus (or on the fake path if no PortAudio backend is available).
    QVERIFY(spy.count() >= 1);
    // Rebuilt on fakes: the speakers and the four VAX devices.
    QVERIFY(m_opened >= 5);
}

void TstAudioEngineResetAudioSettings::emitsVaxConfigChangedForAllChannels()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    AudioEngine* engine = radio.audioEngine();
    useFakeDevices(engine, &m_opened);

    QSignalSpy spy(engine, &AudioEngine::vaxConfigChanged);
    engine->resetAudioSettings();

    // One emission per VAX channel (4 total).
    QCOMPARE(spy.count(), 4);

    QSet<int> channels;
    for (int i = 0; i < spy.count(); ++i) {
        channels.insert(spy.at(i).at(0).toInt());
    }
    QVERIFY(channels.contains(1));
    QVERIFY(channels.contains(2));
    QVERIFY(channels.contains(3));
    QVERIFY(channels.contains(4));
}

void TstAudioEngineResetAudioSettings::emitsAudioSettingsReset()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    AudioEngine* engine = radio.audioEngine();
    useFakeDevices(engine, &m_opened);

    QSignalSpy spy(engine, &AudioEngine::audioSettingsReset);
    engine->resetAudioSettings();

    QCOMPARE(spy.count(), 1);
}

// ---------------------------------------------------------------------------
// R-R3-45: resetting the audio settings puts MON back on the speakers,
// announces it, and does not write the key straight back.
void TstAudioEngineResetAudioSettings::putsTheMonitorBackOnTheSpeakers()
{
    auto& s = AppSettings::instance();
    RadioModel radio;
    useFakeDevices(radio.audioEngine(), &m_opened);
    radio.audioEngine()->setTxMonitorOutput(TxMonitorOutput::Headphones);
    QCOMPARE(s.value(QStringLiteral("audio/TxMonitor/Output")).toString(),
             QStringLiteral("Headphones"));

    QSignalSpy spy(radio.audioEngine(), &AudioEngine::txMonitorOutputChanged);
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(radio.audioEngine()->txMonitorOutput(), TxMonitorOutput::Speakers);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!s.contains(QStringLiteral("audio/TxMonitor/Output")));
}

void TstAudioEngineResetAudioSettings::operatorLocalResetPreservesCoreAudioSettingsAndRebuildsOutputs()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("audio/DspRate"), QStringLiteral("96000"));
    s.setValue(QStringLiteral("audio/Speakers/DeviceName"), QStringLiteral("old-device"));
    RadioModel radio;
    AudioEngine* engine = radio.audioEngine();
    useFakeDevices(engine, &m_opened);
    engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
    QSignalSpy vax(engine, &AudioEngine::vaxConfigChanged);
    engine->resetAudioSettings(true);
    QCOMPARE(s.value(QStringLiteral("audio/DspRate")).toString(), QStringLiteral("96000"));
    QVERIFY(!s.contains(QStringLiteral("audio/Speakers/DeviceName")));
    QCOMPARE(engine->txMonitorOutput(), TxMonitorOutput::Speakers);
    QCOMPARE(vax.count(), 4);
    QVERIFY(m_opened >= 5);
}

QTEST_MAIN(TstAudioEngineResetAudioSettings)
#include "tst_audio_engine_reset_audio_settings.moc"
