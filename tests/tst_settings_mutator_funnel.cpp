// Remote Daemon R2, Task 13 -- AppSettings mutator funnel.
//
// Before this task, AppSettings.cpp had 23 direct QMap<QString,QString>
// m_settings.insert()/remove()/clear() call sites; only setValue(),
// remove(), and clear() itself were "canonical". Everything else --
// setHardwareValue/hardwareValue/hardwareValues/clearHardwareValues and
// the whole radios/* family (saveRadio/forgetRadio/clearSavedRadios/
// savedRadios/savedRadio/lastConnected/setLastConnected/discoveryProfile/
// setDiscoveryProfile/modelOverride/setModelOverride) -- touched
// m_settings directly. R2's SettingsProxy (Task 15) can only delegate
// reads/writes at a single seam, so this task rewrites every one of
// those to call setValue()/value()/remove()/contains()/allKeys()
// internally, and adds a std::function<void(const QString&)> change
// hook that fires from inside setValue()/remove() -- which, because of
// the funnel, now covers every mutator in the list above for free.
//
// This file pins:
//   - the hook fires for exactly the ten operations the brief names
//     (setValue, remove, setHardwareValue, clearHardwareValues,
//     saveRadio, forgetRadio, clearSavedRadios, setLastConnected,
//     setDiscoveryProfile, setModelOverride);
//   - it does NOT fire for clear() (deliberately excluded -- a bulk
//     test-isolation wipe with no single key to report; see
//     AppSettings.h's doc comment on setChangeHook for the reasoning,
//     which also covers load()'s corrupt-file recovery fallback);
//   - it does NOT fire for a plain read (value() on its own);
//   - it is instance-scoped, not global/static;
//   - the two semantics later tasks (15, 16) depend on survive the
//     routing unchanged: value() still returns the caller's default for
//     a key that was never written, and hardwareValues() still strips
//     its "hardware/<mac>/" prefix down to bare keys.
//
// Uses the direct-construction path (AppSettings(filePath)) exclusively,
// same as tst_app_settings_migration.cpp -- no singleton, no disk I/O
// required.

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TstSettingsMutatorFunnel : public QObject {
    Q_OBJECT

private:
    // Build a fully-populated RadioInfo so saveRadio() writes every field
    // it normally would except modelOverride, which stays at the FIRST /
    // no-override sentinel here -- setModelOverride() is pinned by its
    // own dedicated test below.
    static RadioInfo makeRadioInfo(const QString& mac)
    {
        RadioInfo info;
        info.name          = QStringLiteral("Bench ANAN-G2");
        info.macAddress    = mac;
        info.address       = QHostAddress(QStringLiteral("192.168.1.50"));
        info.port          = 1024;
        info.boardType     = HPSDRHW::Orion;
        info.protocol      = ProtocolVersion::Protocol2;
        info.firmwareVersion = 42;
        return info;
    }

    // Installs a recording hook on s and returns the shared log it
    // appends to. Caller owns the log's lifetime (a local QStringList).
    static void installRecorder(AppSettings& s, QStringList& log)
    {
        s.setChangeHook([&log](const QString& key) {
            log.append(key);
        });
    }

private slots:
    // ── The ten required "fires for" operations ─────────────────────────

    void hookFiresOnSetValue()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        s.setValue(QStringLiteral("SomeKey"), QStringLiteral("SomeValue"));

        QCOMPARE(log, QStringList{QStringLiteral("SomeKey")});
    }

    void hookFiresOnRemove()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("SomeKey"), QStringLiteral("SomeValue"));

        QStringList log;
        installRecorder(s, log);
        s.remove(QStringLiteral("SomeKey"));

        QCOMPARE(log, QStringList{QStringLiteral("SomeKey")});
    }

    void hookFiresOnSetHardwareValue()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:FF");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 192000);

        // The hook observes the fully-qualified underlying key, not the
        // bare per-mac key hardwareValues() later strips down to.
        QCOMPARE(log, QStringList{QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/radioInfo/sampleRate")});
    }

    void hookFiresOnClearHardwareValues()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:FF");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 192000);
        s.setHardwareValue(mac, QStringLiteral("radioInfo/activeRxCount"), 2);

        QStringList log;
        installRecorder(s, log);
        s.clearHardwareValues(mac);

        QCOMPARE(log.size(), 2);
        for (const QString& key : log) {
            QVERIFY2(key.startsWith(QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/")),
                     qPrintable(key));
        }
        QVERIFY(s.hardwareValues(mac).isEmpty());
    }

    void hookFiresOnSaveRadio()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        const QString mac = QStringLiteral("11:22:33:44:55:66");
        s.saveRadio(makeRadioInfo(mac), /*pinToMac=*/true, /*autoConnect=*/false);

        // 10 fields with a default (FIRST/no-override) modelOverride --
        // that field is conditional; the non-default case (an 11th fire)
        // is covered by the second phase below.
        const QStringList expectedSuffixes = {
            QStringLiteral("name"), QStringLiteral("ipAddress"),
            QStringLiteral("port"), QStringLiteral("macAddress"),
            QStringLiteral("boardType"), QStringLiteral("protocol"),
            QStringLiteral("firmwareVersion"), QStringLiteral("pinToMac"),
            QStringLiteral("autoConnect"), QStringLiteral("lastSeen"),
        };
        QCOMPARE(log.size(), expectedSuffixes.size());
        const QString prefix = QStringLiteral("radios/%1/").arg(mac);
        for (const QString& suffix : expectedSuffixes) {
            QVERIFY2(log.contains(prefix + suffix), qPrintable(prefix + suffix));
        }

        // Second phase: a non-default modelOverride makes saveRadio()'s
        // conditional 11th field write fire too. Separate mac + a
        // cleared log so this doesn't disturb the 10-field assertion
        // above. hookFiresOnSetModelOverride (below) pins the standalone
        // setModelOverride() path; this pins saveRadio()'s own
        // conditional branch specifically, which is a different call
        // site even though it writes the same key shape.
        log.clear();
        const QString macWithOverride = QStringLiteral("77:88:99:AA:BB:CC");
        RadioInfo infoWithOverride = makeRadioInfo(macWithOverride);
        infoWithOverride.modelOverride = HPSDRModel::ANAN_G2;
        s.saveRadio(infoWithOverride, /*pinToMac=*/false, /*autoConnect=*/false);

        QCOMPARE(log.size(), 11);
        const QString overridePrefix = QStringLiteral("radios/%1/").arg(macWithOverride);
        QVERIFY2(log.contains(overridePrefix + QStringLiteral("modelOverride")),
                 qPrintable(overridePrefix + QStringLiteral("modelOverride")));

        auto sr = s.savedRadio(macWithOverride);
        QVERIFY(sr.has_value());
        QCOMPARE(sr->info.modelOverride, HPSDRModel::ANAN_G2);
    }

    void hookFiresOnForgetRadio()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("11:22:33:44:55:66");
        s.saveRadio(makeRadioInfo(mac), false, false);

        QStringList log;
        installRecorder(s, log);
        s.forgetRadio(mac);

        QCOMPARE(log.size(), 10); // same 10 fields saveRadio wrote above
        const QString prefix = QStringLiteral("radios/%1/").arg(mac);
        for (const QString& key : log) {
            QVERIFY2(key.startsWith(prefix), qPrintable(key));
        }
        QVERIFY(!s.savedRadio(mac).has_value());
    }

    void hookFiresOnClearSavedRadios()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac1 = QStringLiteral("11:22:33:44:55:66");
        const QString mac2 = QStringLiteral("AA:BB:CC:DD:EE:FF");
        s.saveRadio(makeRadioInfo(mac1), false, false);
        s.saveRadio(makeRadioInfo(mac2), false, false);
        s.setLastConnected(mac1); // must survive clearSavedRadios()

        QStringList log;
        installRecorder(s, log);
        s.clearSavedRadios();

        QCOMPARE(log.size(), 20); // 10 fields x 2 radios
        for (const QString& key : log) {
            QVERIFY2(key.startsWith(QStringLiteral("radios/")), qPrintable(key));
            QVERIFY2(key.mid(7).contains(QLatin1Char('/')), qPrintable(key));
        }
        QVERIFY(!s.savedRadio(mac1).has_value());
        QVERIFY(!s.savedRadio(mac2).has_value());
        // Pre-existing invariant (not new to this task): lastConnected is
        // scoped separately and survives a clearSavedRadios() call.
        QCOMPARE(s.lastConnected(), mac1);
    }

    void hookFiresOnSetLastConnected()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        // Non-empty branch routes through setValue().
        s.setLastConnected(QStringLiteral("11:22:33:44:55:66"));
        QCOMPARE(log, QStringList{QStringLiteral("radios/lastConnected")});

        // Empty branch routes through remove() -- also fires.
        log.clear();
        s.setLastConnected(QString());
        QCOMPARE(log, QStringList{QStringLiteral("radios/lastConnected")});
    }

    void hookFiresOnSetDiscoveryProfile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        s.setDiscoveryProfile(DiscoveryProfile::Fast);

        QCOMPARE(log, QStringList{QStringLiteral("radios/discoveryProfile")});
        QCOMPARE(s.discoveryProfile(), DiscoveryProfile::Fast);
    }

    void hookFiresOnSetModelOverride()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        const QString mac = QStringLiteral("11:22:33:44:55:66");
        s.setModelOverride(mac, HPSDRModel::ANAN_G2);

        QCOMPARE(log, QStringList{QStringLiteral("radios/11:22:33:44:55:66/modelOverride")});
        QCOMPARE(s.modelOverride(mac), HPSDRModel::ANAN_G2);
    }

    // ── Deliberate non-firing cases ──────────────────────────────────────

    void hookDoesNotFireOnClear()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("Key1"), QStringLiteral("Val1"));
        s.setValue(QStringLiteral("Key2"), QStringLiteral("Val2"));

        QStringList log;
        installRecorder(s, log);
        s.clear();

        QVERIFY2(log.isEmpty(),
                 "clear() is a bulk test-isolation wipe with no single "
                 "key to report and must not fire the change hook");
        QVERIFY(s.allKeys().isEmpty());
    }

    void hookDoesNotFireOnPlainRead()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("Key1"), QStringLiteral("Val1"));

        QStringList log;
        installRecorder(s, log);
        const QVariant v = s.value(QStringLiteral("Key1"));
        Q_UNUSED(v);

        QVERIFY(log.isEmpty());
    }

    // ── Hook shape: instance-scoped, clearable ───────────────────────────

    void hookIsInstanceScoped()
    {
        QTemporaryDir tmpA;
        QTemporaryDir tmpB;
        QVERIFY(tmpA.isValid());
        QVERIFY(tmpB.isValid());
        AppSettings a(tmpA.filePath(QStringLiteral("NereusSDR.settings")));
        AppSettings b(tmpB.filePath(QStringLiteral("NereusSDR.settings")));

        QStringList log;
        installRecorder(a, log); // only installed on 'a'

        b.setValue(QStringLiteral("OnB"), QStringLiteral("x"));
        a.setValue(QStringLiteral("OnA"), QStringLiteral("x"));

        // Only a's own mutation shows up -- b's hook (unset) never
        // touches a's log, and a's hook never fires for b's mutation.
        QCOMPARE(log, QStringList{QStringLiteral("OnA")});
    }

    void hookCanBeCleared()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QStringList log;
        installRecorder(s, log);

        s.setValue(QStringLiteral("Key1"), QStringLiteral("Val1"));
        QCOMPARE(log.size(), 1);

        s.setChangeHook(nullptr);
        s.setValue(QStringLiteral("Key2"), QStringLiteral("Val2"));

        QCOMPARE(log.size(), 1); // unchanged -- no crash, no new entry
        QCOMPARE(s.value(QStringLiteral("Key2")).toString(), QStringLiteral("Val2"));
    }

    // ── The two semantics this task must not break ───────────────────────

    void unwrittenKeyStillReturnsCallerDefault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        // The exact Setup-page scenario the controller notes name: Region
        // was never set, and value() must hand back the caller's default,
        // not an empty string.
        QVERIFY(!s.contains(QStringLiteral("Region")));
        QCOMPARE(s.value(QStringLiteral("Region"), QStringLiteral("United States")).toString(),
                 QStringLiteral("United States"));
    }

    void hardwareValuesStillStripsPrefix()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:FF");
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 192000);

        const QMap<QString, QVariant> values = s.hardwareValues(mac);
        QVERIFY(values.contains(QStringLiteral("radioInfo/sampleRate")));
        QCOMPARE(values.value(QStringLiteral("radioInfo/sampleRate")).toInt(), 192000);
        // Prefix must be gone, not merely present as an additional key.
        QVERIFY(!values.contains(QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/radioInfo/sampleRate")));
        QCOMPARE(values.size(), 1);
    }

    // ── Routed reads still round-trip through save()/load() ──────────────
    // (belt-and-suspenders: the wider suite -- tst_app_settings_migration,
    // tst_connection_panel_saved_radios, tst_settings_hygiene -- is the
    // real corruption backstop; this just confirms the hook plumbing
    // doesn't interfere with a real save/load cycle.)

    void savedRadioSurvivesSaveAndLoadWithHookInstalled()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));
        const QString mac = QStringLiteral("11:22:33:44:55:66");

        {
            AppSettings s(path);
            QStringList log;
            installRecorder(s, log);
            s.saveRadio(makeRadioInfo(mac), true, true);
            s.save();
            QVERIFY(!log.isEmpty());
        }
        {
            AppSettings s(path);
            s.load();
            auto sr = s.savedRadio(mac);
            QVERIFY2(sr.has_value(), "Radio must survive a save/load cycle with the hook installed");
            QCOMPARE(sr->info.name, QStringLiteral("Bench ANAN-G2"));
            QVERIFY(sr->pinToMac);
            QVERIFY(sr->autoConnect);
        }
    }
};

QTEST_APPLESS_MAIN(TstSettingsMutatorFunnel)
#include "tst_settings_mutator_funnel.moc"
