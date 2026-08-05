// no-port-check: NereusSDR-original regression test.
//
// Phase 3J-1 closeout Item 7 (2026-05-12): pins the contract that
// AppSettings does NOT purge top-level keys on save/reload, regardless
// of whether the key matches any "known schema".  The user's memory
// note "NereusSDR purges unknown keys on save" turned out to be
// inaccurate -- audit of AppSettings::load / save / setValue / remove
// found zero code paths that filter top-level keys.  This test prevents
// any future change from accidentally introducing one.
//
// Coverage:
//   - User-set arbitrary key persists across save → reload
//   - User-set TciEmulate* keys persist (the canonical TCI bench gripe)
//   - A key with "False" string value round-trips (catches "empty-string
//     means absent" parser quirks)
//   - A key set to "" round-trips as ""
//
// If this test ever fails, the regression is almost certainly a new
// filter inside AppSettings::save() or load() -- start the audit at
// src/core/AppSettings.cpp.

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"

using namespace NereusSDR;

class TstAppSettingsArbitraryKeyPersistence : public QObject {
    Q_OBJECT
private slots:
    void arbitrary_top_level_key_round_trips() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // Cycle 1: set a key that no NereusSDR code path knows about.
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("ArbitraryUserKey_42"),
                       QStringLiteral("hello"));
            s.save();
        }
        // Cycle 2: reload, key must still be present + correct value.
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("ArbitraryUserKey_42"))
                         .toString(),
                     QStringLiteral("hello"));
        }
    }

    // The exact scenario that triggered Item 7: the operator pre-edits
    // TciEmulate* keys to True (or False) outside the running app, and
    // expects them to survive a save cycle.
    void tci_emulate_keys_round_trip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings s(path);
            s.setValue(QStringLiteral("TciEmulateSunSDR2Pro"),
                       QStringLiteral("True"));
            s.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"),
                       QStringLiteral("False"));
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("TciEmulateSunSDR2Pro"))
                         .toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("TciEmulateExpertSDR3Protocol"))
                         .toString(),
                     QStringLiteral("False"));
            QVERIFY(s.contains(QStringLiteral("TciEmulateSunSDR2Pro")));
            QVERIFY(s.contains(QStringLiteral("TciEmulateExpertSDR3Protocol")));
        }
    }

    // A key with empty-string value is NOT the same as an absent key.
    // QHash::insert("", "") + save must round-trip as a present key
    // with empty value, not as removal.
    void empty_string_value_round_trips() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings s(path);
            s.setValue(QStringLiteral("EmptyValueKey"), QString());
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QVERIFY(s.contains(QStringLiteral("EmptyValueKey")));
            QCOMPARE(s.value(QStringLiteral("EmptyValueKey")).toString(),
                     QString());
        }
    }

    // Pin that the only paths that remove top-level keys are explicit:
    // remove(key), forgetRadio(mac), clearSavedRadios(), clearHardware
    // Values(mac).  None of those should touch an unrelated arbitrary
    // key.
    void scoped_removal_paths_do_not_touch_arbitrary_keys() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        AppSettings s(path);
        s.setValue(QStringLiteral("ArbitraryUserKey_42"),
                   QStringLiteral("survive"));
        s.setValue(QStringLiteral("radios/AA:BB:CC:DD:EE:FF/name"),
                   QStringLiteral("test radio"));
        s.setValue(QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/sampleRate"),
                   QStringLiteral("192000"));

        // forgetRadio + clearHardwareValues remove their scoped prefixes
        // but must leave the arbitrary key alone.
        s.forgetRadio(QStringLiteral("AA:BB:CC:DD:EE:FF"));
        s.clearHardwareValues(QStringLiteral("AA:BB:CC:DD:EE:FF"));

        QCOMPARE(s.value(QStringLiteral("ArbitraryUserKey_42")).toString(),
                 QStringLiteral("survive"));
        QVERIFY(!s.contains(
            QStringLiteral("radios/AA:BB:CC:DD:EE:FF/name")));
        QVERIFY(!s.contains(
            QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/sampleRate")));
    }
};

QTEST_MAIN(TstAppSettingsArbitraryKeyPersistence)
#include "tst_app_settings_arbitrary_key_persistence.moc"
