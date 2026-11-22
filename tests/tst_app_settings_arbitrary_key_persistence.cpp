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
//
// Modification history (NereusSDR):
//   2026-11-22  J.J. Boyd (KG4VCF)  A key with characters an XML element
//                                    name cannot hold (a tab, a line break,
//                                    a leading digit, a lone surrogate)
//                                    saves, reloads and comes back, and the
//                                    older escapes keep their spelling.
//                                    AI-assisted via Claude Code.

#include <QtTest/QtTest>
#include <QFile>
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

    // A TX profile name from a file becomes part of its keys. No key may
    // make the file unreadable on the next launch (the loader would treat
    // the whole file as corrupt), and the older escapes keep their spelling
    // so files written before them still load.
    void odd_keys_round_trip_and_older_escapes_are_unchanged() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));
        const QString prefix = QStringLiteral("hardware/aa:bb:cc:11:22:33/tx/profile/");
        const QStringList keys{
            prefix + QStringLiteral("Tab\there/MicGain"),
            prefix + QStringLiteral("Line\nbreak/MicGain"),
            prefix + QStringLiteral("Bell\x07" "and\x7f" "/MicGain"),
            prefix + QString(QChar(0x0001)) + QStringLiteral("start/MicGain"),
            QStringLiteral("9LeadingDigit"),
            QStringLiteral("-LeadingDash"),
            QStringLiteral(".LeadingDot"),
            prefix + QStringLiteral("Caf\u00e9 \u00b7 \u0301/MicGain"),
            prefix + QString::fromUcs4(U"Emoji \U0001F4FB/MicGain"),
            prefix + QString(QChar(0xD800)) + QStringLiteral("lone/MicGain"),
            prefix + QString(QChar(0xFFFE)) + QStringLiteral("nonchar/MicGain"),
            prefix + QStringLiteral("D-104+CPDR/MicGain"),
        };
        {
            AppSettings s(path);
            for (int i = 0; i < keys.size(); ++i) {
                s.setValue(keys.at(i), QString::number(i));
            }
            s.save();
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());
        file.close();
        // The older escapes, byte for byte.
        QVERIFY(text.contains(QStringLiteral(
            "hardware__s__aa__c__bb__c__cc__c__11__c__22__c__33__s__tx__s__profile__s__"
            "D-104__plus__CPDR__s__MicGain")));
        QVERIFY(text.contains(QStringLiteral("Tab__u0009__here")));
        QVERIFY(text.contains(QStringLiteral("__u0039__LeadingDigit")));
        QVERIFY(text.contains(QStringLiteral("Emoji__sp____u01F4FB__")));
        {
            AppSettings s(path);
            s.load();
            QVERIFY(!s.wasCorruptedOnLoad());
            for (int i = 0; i < keys.size(); ++i) {
                QVERIFY2(s.contains(keys.at(i)), qPrintable(QString::number(i)));
                QCOMPARE(s.value(keys.at(i)).toString(), QString::number(i));
            }
        }
    }
};

QTEST_MAIN(TstAppSettingsArbitraryKeyPersistence)
#include "tst_app_settings_arbitrary_key_persistence.moc"
