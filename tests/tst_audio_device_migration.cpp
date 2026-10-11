// =================================================================
// tests/tst_audio_device_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// V-SW-3: the one-time migration of saved audio devices to the native
// engines (R-AUD-05).  One profile per row of the table produces the row's
// result, once; the old keys stay in the file; a profile that already has
// Engine is never touched.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 4. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/audio/AudioDeviceMatching.h"

#include <QMap>

using namespace NereusSDR;

namespace {

const QString kPrefix = QStringLiteral("audio/Speakers");
const QString kRealtek = QStringLiteral("Speakers (Realtek(R) Audio)");

AudioMigrationContext windowsContext()
{
    AudioMigrationContext c;
    c.nativeEngine = AudioEngineKind::WindowsShared;
    c.nativeRunning = true;
    c.windows = true;
    return c;
}

AudioMigrationContext macContext()
{
    AudioMigrationContext c;
    c.nativeEngine = AudioEngineKind::CoreAudio;
    c.nativeRunning = true;
    return c;
}

AudioMigrationContext linuxContext(bool running)
{
    AudioMigrationContext c;
    c.nativeEngine = AudioEngineKind::PipeWire;
    c.nativeRunning = running;
    c.onLinux = true;
    return c;
}

QString key(const QString& prefix, const char* name)
{
    return prefix + QLatin1Char('/') + QLatin1String(name);
}

QString value(const QString& prefix, const char* name)
{
    return AppSettings::instance().value(key(prefix, name), QString()).toString();
}

bool has(const QString& prefix, const char* name)
{
    return AppSettings::instance().contains(key(prefix, name));
}

void write(const QString& prefix, const char* name, const QString& v)
{
    AppSettings::instance().setValue(key(prefix, name), v);
}

// Every audio/* key and its value.
QMap<QString, QString> audioSnapshot()
{
    QMap<QString, QString> out;
    auto& s = AppSettings::instance();
    for (const QString& k : s.allKeys()) {
        if (k.startsWith(QStringLiteral("audio/"))) {
            out.insert(k, s.value(k).toString());
        }
    }
    return out;
}

// A profile as the old Setup page saved it: all ten keys.
void writeOldProfile(const QString& prefix, const QString& driverApi, const QString& deviceName,
                     bool exclusive = false)
{
    AudioDeviceConfig cfg;
    cfg.driverApi = driverApi;
    cfg.deviceName = deviceName;
    cfg.exclusiveMode = exclusive;
    cfg.sampleRate = 96000;
    cfg.bufferSamples = 256;
    cfg.saveToSettings(prefix);
}

} // namespace

class TstAudioDeviceMigration : public QObject {
    Q_OBJECT

private:
    void clearAudioKeys()
    {
        auto& s = AppSettings::instance();
        for (const QString& k : s.allKeys()) {
            if (k.startsWith(QStringLiteral("audio/"))) {
                s.remove(k);
            }
        }
    }

    // The old keys are as written, apart from the ones the row names.
    void verifyOldKeysKept(const QMap<QString, QString>& before, const QMap<QString, QString>& after,
                           const QStringList& changed)
    {
        for (auto it = before.cbegin(); it != before.cend(); ++it) {
            QVERIFY2(after.contains(it.key()), qPrintable(it.key()));
            if (!changed.contains(it.key())) {
                QCOMPARE(after.value(it.key()), it.value());
            }
        }
    }

private slots:

    void init() { clearAudioKeys(); }
    void cleanup() { clearAudioKeys(); }

    // Row 1: no DriverApi, Windows, native Windows audio shared running.
    void noDriverApiMovesToNativeSameDevice()
    {
        writeOldProfile(kPrefix, QString(), kRealtek);
        const auto before = audioSnapshot();
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, windowsContext()), AudioMigrationResult::Migrated);
        QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("WindowsShared"));
        QCOMPARE(value(kPrefix, "DeviceName"), kRealtek);
        QVERIFY(value(kPrefix, "DeviceId").isEmpty());
        verifyOldKeysKept(before, audioSnapshot(), {});

        const AudioDeviceConfig loaded = AudioDeviceConfig::loadFromSettings(kPrefix);
        QVERIFY(loaded.engine == AudioEngineKind::WindowsShared);
        QCOMPARE(loaded.deviceName, kRealtek);
        QVERIFY(loaded.deviceId.isEmpty());
    }

    // Rows 2 and 3: WASAPI by ExclusiveMode.
    void wasapiMovesByExclusiveMode()
    {
        writeOldProfile(QStringLiteral("audio/Speakers"), QStringLiteral("Windows WASAPI"), kRealtek, false);
        writeOldProfile(QStringLiteral("audio/Headphones"), QStringLiteral("Windows WASAPI"), kRealtek, true);
        const auto before = audioSnapshot();
        QCOMPARE(migrateAudioDeviceKeys(QStringLiteral("audio/Speakers"), windowsContext()),
                 AudioMigrationResult::Migrated);
        QCOMPARE(migrateAudioDeviceKeys(QStringLiteral("audio/Headphones"), windowsContext()),
                 AudioMigrationResult::Migrated);
        QCOMPARE(value(QStringLiteral("audio/Speakers"), "Engine"), QStringLiteral("WindowsShared"));
        QCOMPARE(value(QStringLiteral("audio/Headphones"), "Engine"), QStringLiteral("WindowsExclusive"));
        QCOMPARE(value(QStringLiteral("audio/Speakers"), "DeviceName"), kRealtek);
        QCOMPARE(value(QStringLiteral("audio/Headphones"), "DeviceName"), kRealtek);
        verifyOldKeysKept(before, audioSnapshot(), {});
    }

    // The short form the VAX schema migration wrote.
    void shortWasapiFormMovesToo()
    {
        write(kPrefix, "DriverApi", QStringLiteral("WASAPI"));
        write(kPrefix, "DeviceName", kRealtek);
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, windowsContext()), AudioMigrationResult::Migrated);
        QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("WindowsShared"));
    }

    // Row 4: Core Audio on the Mac.
    void coreAudioOnTheMac()
    {
        const QString name = QStringLiteral("MacBook Pro Speakers");
        writeOldProfile(kPrefix, QStringLiteral("Core Audio"), name);
        const auto before = audioSnapshot();
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, macContext()), AudioMigrationResult::Migrated);
        QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("CoreAudio"));
        QCOMPARE(value(kPrefix, "DeviceName"), name);
        verifyOldKeysKept(before, audioSnapshot(), {});
    }

    // Row 5: ALSA on the sound server moves to "(platform default)".
    void alsaServerNamesMoveToPlatformDefault()
    {
        const QStringList names = {QString(), QStringLiteral("default"), QStringLiteral("pulse"),
                                   QStringLiteral("pipewire")};
        for (const QString& name : names) {
            clearAudioKeys();
            writeOldProfile(kPrefix, QStringLiteral("ALSA"), name);
            const auto before = audioSnapshot();
            QCOMPARE(migrateAudioDeviceKeys(kPrefix, linuxContext(true)), AudioMigrationResult::Migrated);
            QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("PipeWire"));
            QVERIFY(has(kPrefix, "DeviceName"));
            QVERIFY(value(kPrefix, "DeviceName").isEmpty());
            QVERIFY(value(kPrefix, "DeviceId").isEmpty());
            QCOMPARE(value(kPrefix, "DriverApi"), QStringLiteral("ALSA"));
            verifyOldKeysKept(before, audioSnapshot(), {key(kPrefix, "DeviceName")});
            QVERIFY(AudioDeviceConfig::loadFromSettings(kPrefix).isPlatformDefault());
        }
    }

    // Row 6: an older driver picked by name stays, every old key unchanged.
    void olderDriversPickedByNameStay()
    {
        struct Row { QString api; QString name; AudioMigrationContext ctx; };
        const QList<Row> rows = {
            {QStringLiteral("MME"), kRealtek, windowsContext()},
            {QStringLiteral("Windows DirectSound"), kRealtek, windowsContext()},
            {QStringLiteral("Windows WDM-KS"), kRealtek, windowsContext()},
            {QStringLiteral("JACK Audio Connection Kit"), QStringLiteral("system"), linuxContext(true)},
            {QStringLiteral("ALSA"), QStringLiteral("USB Audio Device: - (hw:1,0)"), linuxContext(true)},
        };
        for (const Row& row : rows) {
            clearAudioKeys();
            writeOldProfile(kPrefix, row.api, row.name);
            const auto before = audioSnapshot();
            QCOMPARE(migrateAudioDeviceKeys(kPrefix, row.ctx), AudioMigrationResult::Unchanged);
            QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("PortAudio"));
            const auto after = audioSnapshot();
            QCOMPARE(after.size(), before.size() + 1);
            verifyOldKeysKept(before, after, {});
        }
    }

    // Linux with the sound server not answering: postponed, then migrated.
    void linuxNotRunningPostponesThenMigrates()
    {
        writeOldProfile(kPrefix, QString(), QStringLiteral("USB Audio Device Analog Stereo"));
        const auto before = audioSnapshot();
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, linuxContext(false)), AudioMigrationResult::Postponed);
        QCOMPARE(audioSnapshot(), before);

        QCOMPARE(migrateAudioDeviceKeys(kPrefix, linuxContext(true)), AudioMigrationResult::Migrated);
        QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("PipeWire"));
        QCOMPARE(value(kPrefix, "DeviceName"), QStringLiteral("USB Audio Device Analog Stereo"));
    }

    // No native engine registered yet: everything that would move waits.
    void noNativeEnginePostponesEveryMove()
    {
        struct Row { QString api; QString name; bool exclusive; AudioMigrationContext ctx; };
        QList<Row> rows = {
            {QString(), kRealtek, false, windowsContext()},
            {QStringLiteral("Windows WASAPI"), kRealtek, true, windowsContext()},
            {QStringLiteral("Core Audio"), QStringLiteral("MacBook Pro Speakers"), false, macContext()},
            {QStringLiteral("ALSA"), QStringLiteral("pulse"), false, linuxContext(true)},
        };
        for (Row& row : rows) {
            clearAudioKeys();
            row.ctx.nativeEngine.reset();
            writeOldProfile(kPrefix, row.api, row.name, row.exclusive);
            const auto before = audioSnapshot();
            QCOMPARE(migrateAudioDeviceKeys(kPrefix, row.ctx), AudioMigrationResult::Postponed);
            QCOMPARE(audioSnapshot(), before);
        }
    }

    // The Linux Core: every profile moves to ALSA direct, name kept.
    void alsaDirectOnlyMovesEveryProfile()
    {
        const QList<QPair<QString, QString>> rows = {
            {QString(), QString()},
            {QStringLiteral("ALSA"), QStringLiteral("USB Audio Device: - (hw:2,0)")},
            {QStringLiteral("ALSA"), QStringLiteral("default")},
            {QStringLiteral("JACK Audio Connection Kit"), QStringLiteral("system")},
            {QStringLiteral("Windows WASAPI"), kRealtek},
        };
        for (const auto& row : rows) {
            clearAudioKeys();
            writeOldProfile(kPrefix, row.first, row.second);
            const auto before = audioSnapshot();
            AudioMigrationContext ctx = linuxContext(false);
            ctx.nativeEngine.reset();
            ctx.alsaDirectOnly = true;
            QCOMPARE(migrateAudioDeviceKeys(kPrefix, ctx), AudioMigrationResult::Migrated);
            QCOMPARE(value(kPrefix, "Engine"), QStringLiteral("AlsaDirect"));
            QCOMPARE(value(kPrefix, "DeviceName"), row.second);
            verifyOldKeysKept(before, audioSnapshot(), {});
        }
    }

    // A profile that has Engine is never touched; twice is the same as once.
    void existingEngineIsNeverTouched()
    {
        writeOldProfile(kPrefix, QStringLiteral("Windows WASAPI"), kRealtek, true);
        write(kPrefix, "Engine", QStringLiteral("PortAudio"));
        const auto before = audioSnapshot();
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, windowsContext()), AudioMigrationResult::Unchanged);
        QCOMPARE(audioSnapshot(), before);
    }

    void runningTwiceGivesTheSameKeysAsOnce()
    {
        writeOldProfile(QStringLiteral("audio/Speakers"), QString(), kRealtek);
        writeOldProfile(QStringLiteral("audio/Headphones"), QStringLiteral("Windows WASAPI"), kRealtek, true);
        writeOldProfile(QStringLiteral("audio/TxInput"), QStringLiteral("MME"), QStringLiteral("Microphone (USB)"));
        writeOldProfile(QStringLiteral("audio/Vax1"), QStringLiteral("Windows WASAPI"), QStringLiteral("CABLE Input"));
        migrateAllAudioDeviceKeys(windowsContext());
        const auto once = audioSnapshot();
        QCOMPARE(value(QStringLiteral("audio/Speakers"), "Engine"), QStringLiteral("WindowsShared"));
        QCOMPARE(value(QStringLiteral("audio/Headphones"), "Engine"), QStringLiteral("WindowsExclusive"));
        QCOMPARE(value(QStringLiteral("audio/TxInput"), "Engine"), QStringLiteral("PortAudio"));
        QCOMPARE(value(QStringLiteral("audio/Vax1"), "Engine"), QStringLiteral("WindowsShared"));
        // Nothing saved for these, so nothing is written.
        QVERIFY(!has(QStringLiteral("audio/Vax2"), "Engine"));
        QVERIFY(!has(QStringLiteral("audio/Vax4"), "Engine"));

        migrateAllAudioDeviceKeys(windowsContext());
        QCOMPARE(audioSnapshot(), once);
    }

    void allPrefixesAreMigrated()
    {
        const QStringList prefixes = {
            QStringLiteral("audio/Speakers"), QStringLiteral("audio/Headphones"),
            QStringLiteral("audio/TxInput"),  QStringLiteral("audio/Vax1"),
            QStringLiteral("audio/Vax2"),     QStringLiteral("audio/Vax3"),
            QStringLiteral("audio/Vax4"),
        };
        for (const QString& p : prefixes) {
            writeOldProfile(p, QStringLiteral("Core Audio"), QStringLiteral("Device ") + p);
        }
        migrateAllAudioDeviceKeys(macContext());
        for (const QString& p : prefixes) {
            QCOMPARE(value(p, "Engine"), QStringLiteral("CoreAudio"));
            QCOMPARE(value(p, "DeviceName"), QStringLiteral("Device ") + p);
        }
    }

    void nothingSavedWritesNothing()
    {
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, windowsContext()), AudioMigrationResult::Unchanged);
        QVERIFY(audioSnapshot().isEmpty());
    }

    // A save of an unmigrated profile does not mark it migrated.
    void saveOfUnmigratedProfileKeepsItUnmigrated()
    {
        writeOldProfile(kPrefix, QString(), kRealtek);
        AudioDeviceConfig loaded = AudioDeviceConfig::loadFromSettings(kPrefix);
        QVERIFY(!loaded.engine.has_value());
        loaded.saveToSettings(kPrefix);
        QVERIFY(!has(kPrefix, "Engine"));
        QCOMPARE(migrateAudioDeviceKeys(kPrefix, windowsContext()), AudioMigrationResult::Migrated);
    }
};

QTEST_MAIN(TstAudioDeviceMigration)
#include "tst_audio_device_migration.moc"
