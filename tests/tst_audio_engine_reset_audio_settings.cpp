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
// does not require a real PortAudio or CoreAudio backend.
//
// Cross-platform. No radioModel required (AudioEngine standalone
// construction with a dedicated AppSettings instance).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>

using namespace NereusSDR;

class TstAudioEngineResetAudioSettings : public QObject {
    Q_OBJECT

private slots:
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

private:
    // Seed AppSettings with a known set of keys covering all §2.5 clear
    // categories plus the two preservation keys.
    void seedSettings(AppSettings& s);
};

// ---------------------------------------------------------------------------
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
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/DspRate"),      QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/DspBlockSize"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioVacFeedbackKeys()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/VacFeedback/1/Gain"), QString()).toString(), QString());
    QCOMPARE(s.value(QStringLiteral("audio/VacFeedback/2/Gain"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioFeatureFlagKeys()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
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
    radio.audioEngine()->resetAudioSettings();

    QCOMPARE(s.value(QStringLiteral("audio/FirstRunComplete"), QString()).toString(), QString());
}

void TstAudioEngineResetAudioSettings::clearsAudioLastDetectedCables()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
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

    QSignalSpy spy(engine, &AudioEngine::speakersConfigChanged);
    engine->resetAudioSettings();

    // ensureSpeakersOpen() emits speakersConfigChanged after rebuilding the
    // default bus (or on the fake path if no PortAudio backend is available).
    QVERIFY(spy.count() >= 1);
}

void TstAudioEngineResetAudioSettings::emitsVaxConfigChangedForAllChannels()
{
    auto& s = AppSettings::instance();
    seedSettings(s);

    RadioModel radio;
    AudioEngine* engine = radio.audioEngine();

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

    QSignalSpy spy(engine, &AudioEngine::audioSettingsReset);
    engine->resetAudioSettings();

    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TstAudioEngineResetAudioSettings)
#include "tst_audio_engine_reset_audio_settings.moc"
