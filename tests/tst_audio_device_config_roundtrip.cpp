// =================================================================
// tests/tst_audio_device_config_roundtrip.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioDeviceConfig::loadFromSettings / saveToSettings
// round-trip — Sub-Phase 12 Task 12.2 Step 0.
//
// Coverage:
//   1. Round-trip saves and loads all 10 fields and returns identical
//      values.
//   2. Fresh install (no keys): loadFromSettings returns defaults.
//   3. Edge case: empty driverApi round-trips to empty.
//   4. Edge case: manualLatencyMs=0 round-trips to 0.
//   5. Edge case: bitDepth=16/24/32 all round-trip correctly.
//   6. Boolean fields (exclusiveMode / eventDriven / bypassMixer) both
//      True and False round-trip correctly.
//   7. Two prefixes are independent (Speakers vs Headphones).
//   8. Native audio Task 4 (R-AUD-04): the saved identity keys Engine,
//      DeviceId, FirstChannel, MicChannel and DelayMs round-trip; an old
//      profile loads with engine nullopt; a save without engine writes
//      none of them.
//
// Design spec:
//   docs/architecture/2026-04-20-phase3o-subphase12-addendum.md §4
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"

using namespace NereusSDR;

class TstAudioDeviceConfigRoundtrip : public QObject {
    Q_OBJECT

private:
    void clearAudioKeys() {
        auto& s = AppSettings::instance();
        // Remove all audio/* keys that our tests might write.
        const QStringList keys = s.allKeys();
        for (const QString& k : keys) {
            if (k.startsWith(QStringLiteral("audio/"))) {
                s.remove(k);
            }
        }
    }

private slots:

    void init() { clearAudioKeys(); }
    void cleanup() { clearAudioKeys(); }

    // ── 1. Full round-trip: all 10 fields ─────────────────────────────────

    void roundTripAllFields() {
        AudioDeviceConfig orig;
        orig.driverApi      = QStringLiteral("Windows WASAPI");
        orig.deviceName     = QStringLiteral("Realtek Audio");
        orig.sampleRate     = 96000;
        orig.bitDepth       = 24;
        orig.channels       = 1;
        orig.bufferSamples  = 512;
        orig.exclusiveMode  = true;
        orig.eventDriven    = true;
        orig.bypassMixer    = false;
        orig.manualLatencyMs = 8;

        orig.saveToSettings(QStringLiteral("audio/Speakers"));
        AppSettings::instance().save();

        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));

        QCOMPARE(loaded.driverApi,       orig.driverApi);
        QCOMPARE(loaded.deviceName,      orig.deviceName);
        QCOMPARE(loaded.sampleRate,      orig.sampleRate);
        QCOMPARE(loaded.bitDepth,        orig.bitDepth);
        QCOMPARE(loaded.channels,        orig.channels);
        QCOMPARE(loaded.bufferSamples,   orig.bufferSamples);
        QCOMPARE(loaded.exclusiveMode,   orig.exclusiveMode);
        QCOMPARE(loaded.eventDriven,     orig.eventDriven);
        QCOMPARE(loaded.bypassMixer,     orig.bypassMixer);
        QCOMPARE(loaded.manualLatencyMs, orig.manualLatencyMs);
    }

    // ── 2. Fresh install — defaults returned ──────────────────────────────

    void freshInstallReturnsDefaults() {
        // No keys written — loadFromSettings must return the struct's
        // in-class defaults so makeBus treats it as "platform default".
        const AudioDeviceConfig cfg =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));

        QVERIFY(cfg.deviceName.isEmpty());
        QCOMPARE(cfg.sampleRate,     48000);
        QCOMPARE(cfg.channels,       2);
        QCOMPARE(cfg.bufferSamples,  128);  // PR #286 lowered default 256 -> 128
        QCOMPARE(cfg.exclusiveMode,  false);
        QCOMPARE(cfg.eventDriven,    false);
        QCOMPARE(cfg.bypassMixer,    false);
        QCOMPARE(cfg.manualLatencyMs, 0);
        QCOMPARE(cfg.bitDepth,       32);
        QVERIFY(cfg.driverApi.isEmpty());
        QCOMPARE(cfg.hostApiIndex,   -1);
    }

    // ── 3. Empty driverApi round-trips to empty ────────────────────────────

    void emptyDriverApiRoundTrips() {
        AudioDeviceConfig cfg;
        cfg.driverApi = QString();
        cfg.saveToSettings(QStringLiteral("audio/Test"));

        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Test"));

        QVERIFY(loaded.driverApi.isEmpty());
    }

    // ── 4. manualLatencyMs=0 round-trips to 0 ────────────────────────────

    void zeroLatencyRoundTrips() {
        AudioDeviceConfig cfg;
        cfg.manualLatencyMs = 0;
        cfg.saveToSettings(QStringLiteral("audio/Test"));

        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Test"));

        QCOMPARE(loaded.manualLatencyMs, 0);
    }

    // ── 5. bitDepth 16 / 24 / 32 all round-trip ──────────────────────────

    void bitDepthVariantsRoundTrip() {
        for (int depth : { 16, 24, 32 }) {
            clearAudioKeys();
            AudioDeviceConfig cfg;
            cfg.bitDepth = depth;
            cfg.saveToSettings(QStringLiteral("audio/Test"));

            const AudioDeviceConfig loaded =
                AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Test"));

            QCOMPARE(loaded.bitDepth, depth);
        }
    }

    // ── 6. Boolean field combinations ─────────────────────────────────────

    void booleanFieldsTrueRoundTrip() {
        AudioDeviceConfig cfg;
        cfg.exclusiveMode = true;
        cfg.eventDriven   = true;
        cfg.bypassMixer   = true;
        cfg.saveToSettings(QStringLiteral("audio/Test"));

        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Test"));

        QCOMPARE(loaded.exclusiveMode, true);
        QCOMPARE(loaded.eventDriven,   true);
        QCOMPARE(loaded.bypassMixer,   true);
    }

    void booleanFieldsFalseRoundTrip() {
        AudioDeviceConfig cfg;
        cfg.exclusiveMode = false;
        cfg.eventDriven   = false;
        cfg.bypassMixer   = false;
        cfg.saveToSettings(QStringLiteral("audio/Test"));

        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Test"));

        QCOMPARE(loaded.exclusiveMode, false);
        QCOMPARE(loaded.eventDriven,   false);
        QCOMPARE(loaded.bypassMixer,   false);
    }

    // ── 7. Two prefixes are independent ────────────────────────────────────

    void twoPrefixesAreIndependent() {
        AudioDeviceConfig speakers;
        speakers.deviceName = QStringLiteral("Speaker Device");
        speakers.sampleRate = 48000;
        speakers.saveToSettings(QStringLiteral("audio/Speakers"));

        AudioDeviceConfig headphones;
        headphones.deviceName = QStringLiteral("Headphone Device");
        headphones.sampleRate = 96000;
        headphones.saveToSettings(QStringLiteral("audio/Headphones"));

        const AudioDeviceConfig loadedSpeakers =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
        const AudioDeviceConfig loadedHeadphones =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Headphones"));

        QCOMPARE(loadedSpeakers.deviceName,   QStringLiteral("Speaker Device"));
        QCOMPARE(loadedSpeakers.sampleRate,   48000);
        QCOMPARE(loadedHeadphones.deviceName, QStringLiteral("Headphone Device"));
        QCOMPARE(loadedHeadphones.sampleRate, 96000);
    }

    // ── 8. Saved identity keys (R-AUD-04) ──────────────────────────────────

    void identityKeysRoundTrip() {
        struct Row { AudioEngineKind engine; const char* key; MicChannelPick mic; const char* micKey; };
        const Row rows[] = {
            {AudioEngineKind::PortAudio,        "PortAudio",        MicChannelPick::Left,  "Left"},
            {AudioEngineKind::CoreAudio,        "CoreAudio",        MicChannelPick::Right, "Right"},
            {AudioEngineKind::WindowsShared,    "WindowsShared",    MicChannelPick::Both,  "Both"},
            {AudioEngineKind::WindowsExclusive, "WindowsExclusive", MicChannelPick::Left,  "Left"},
            {AudioEngineKind::Asio,             "ASIO",             MicChannelPick::Right, "Right"},
            {AudioEngineKind::PipeWire,         "PipeWire",         MicChannelPick::Both,  "Both"},
            {AudioEngineKind::PulseAudio,       "PulseAudio",       MicChannelPick::Left,  "Left"},
            {AudioEngineKind::AlsaDirect,       "AlsaDirect",       MicChannelPick::Left,  "Left"},
        };
        auto& s = AppSettings::instance();
        for (const Row& row : rows) {
            clearAudioKeys();
            AudioDeviceConfig cfg;
            cfg.engine       = row.engine;
            cfg.deviceId     = QStringLiteral("id-") + QLatin1String(row.key);
            cfg.deviceName   = QStringLiteral("Focusrite USB ASIO");
            cfg.firstChannel = 3;
            cfg.micChannel   = row.mic;
            cfg.delayMs      = 10;
            cfg.saveToSettings(QStringLiteral("audio/TxInput"));

            QCOMPARE(s.value(QStringLiteral("audio/TxInput/Engine")).toString(), QLatin1String(row.key));
            QCOMPARE(s.value(QStringLiteral("audio/TxInput/MicChannel")).toString(), QLatin1String(row.micKey));
            QCOMPARE(s.value(QStringLiteral("audio/TxInput/FirstChannel")).toString(), QStringLiteral("3"));
            QCOMPARE(s.value(QStringLiteral("audio/TxInput/DelayMs")).toString(), QStringLiteral("10"));

            const AudioDeviceConfig loaded =
                AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput"));
            QVERIFY(loaded.engine == row.engine);
            QCOMPARE(loaded.deviceId, cfg.deviceId);
            QCOMPARE(loaded.deviceName, cfg.deviceName);
            QCOMPARE(loaded.firstChannel, 3);
            QVERIFY(loaded.micChannel == row.mic);
            QCOMPARE(loaded.delayMs, 10);
        }
    }

    void identityNoneAndDefaultRoundTrip() {
        AudioDeviceConfig none;
        none.engine = AudioEngineKind::CoreAudio;
        none.deviceId = QString::fromLatin1(kAudioDeviceNone);
        none.saveToSettings(QStringLiteral("audio/Headphones"));
        const AudioDeviceConfig loadedNone =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Headphones"));
        QCOMPARE(loadedNone.deviceId, QStringLiteral("(none)"));
        QVERIFY(loadedNone.isNone());
        QVERIFY(!loadedNone.isPlatformDefault());

        AudioDeviceConfig def;
        def.engine = AudioEngineKind::CoreAudio;
        def.saveToSettings(QStringLiteral("audio/Speakers"));
        const AudioDeviceConfig loadedDef =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
        QVERIFY(loadedDef.engine == AudioEngineKind::CoreAudio);
        QVERIFY(loadedDef.isPlatformDefault());
        QVERIFY(!loadedDef.isNone());
    }

    void oldProfileLoadsWithEngineNullopt() {
        // An old profile: the ten keys, no identity keys.
        AudioDeviceConfig old;
        old.driverApi  = QStringLiteral("MME");
        old.deviceName = QStringLiteral("Speakers (Realtek(R) Audio)");
        old.saveToSettings(QStringLiteral("audio/Speakers"));

        auto& s = AppSettings::instance();
        for (const char* k : {"Engine", "DeviceId", "FirstChannel", "MicChannel", "DelayMs"}) {
            QVERIFY2(!s.contains(QStringLiteral("audio/Speakers/") + QLatin1String(k)), k);
        }

        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
        QVERIFY(!loaded.engine.has_value());
        QVERIFY(loaded.deviceId.isEmpty());
        QCOMPARE(loaded.firstChannel, 1);
        QVERIFY(loaded.micChannel == MicChannelPick::Left);
        QCOMPARE(loaded.delayMs, 0);
        QCOMPARE(loaded.driverApi, QStringLiteral("MME"));
        QCOMPARE(loaded.deviceName, QStringLiteral("Speakers (Realtek(R) Audio)"));
    }

    void unknownIdentityValuesFallBackToDefaults() {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Speakers/Engine"),       QStringLiteral("coreaudio"));
        s.setValue(QStringLiteral("audio/Speakers/FirstChannel"), QStringLiteral("0"));
        s.setValue(QStringLiteral("audio/Speakers/MicChannel"),   QStringLiteral("Middle"));
        s.setValue(QStringLiteral("audio/Speakers/DelayMs"),      QStringLiteral("-5"));
        const AudioDeviceConfig loaded =
            AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));
        QVERIFY(!loaded.engine.has_value());
        QCOMPARE(loaded.firstChannel, 1);
        QVERIFY(loaded.micChannel == MicChannelPick::Left);
        QCOMPARE(loaded.delayMs, 0);
    }
};

QTEST_MAIN(TstAudioDeviceConfigRoundtrip)
#include "tst_audio_device_config_roundtrip.moc"
