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
//   2026-11-25  J.J. Boyd (KG4VCF)  A key whose text between two escapes is
//                                    itself an escape's word comes back;
//                                    every fixed escape still reads, as do
//                                    files older builds wrote; a key that
//                                    cannot come back is known.
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

    // Ordinary names whose text meets an escape on both sides: saved,
    // reloaded and the same key, by the launch load and by the settings
    // backup's import. The spelling in the file is what it always was.
    void names_between_escapes_round_trip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));
        const QString prefix = QStringLiteral("hardware/aa:bb:cc:11:22:33/tx/profile/");
        QStringList keys;
        for (const char* name : {"plan c", "c", "Rig s", "A lb B", "Heil plus gate", "_c_", "_",
                                 "c_", "_s", "u0042", "a u0042 b", "x_ c", "+_c_+", "My__Profile",
                                 "a__b", "__", "s c s"}) {
            keys.append(prefix + QString::fromLatin1(name) + QStringLiteral("/MicGain"));
        }
        for (const QString& key : keys) {
            QVERIFY2(AppSettings::keySurvivesFile(key), qPrintable(key));
        }
        {
            AppSettings s(path);
            for (int i = 0; i < keys.size(); ++i) {
                s.setValue(keys.at(i), QString::number(i));
            }
            QVERIFY(s.save());
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray xml = file.readAll();
        file.close();
        QVERIFY(xml.contains("__s__plan__sp__c__s__MicGain>"));
        AppSettings loaded(path);
        loaded.load();
        QVERIFY(!loaded.wasCorruptedOnLoad());
        AppSettings imported(tmp.filePath(QStringLiteral("imported.settings")));
        QString error;
        QVERIFY2(imported.importLocalXml(xml, &error), qPrintable(error));
        for (int i = 0; i < keys.size(); ++i) {
            QVERIFY2(loaded.contains(keys.at(i)), qPrintable(keys.at(i)));
            QCOMPARE(loaded.value(keys.at(i)).toString(), QString::number(i));
            QCOMPARE(imported.value(keys.at(i)).toString(), QString::number(i));
        }
    }

    // Every fixed escape reads as its character, alone and beside others,
    // as files written by every earlier build spell them.
    void every_fixed_escape_reads() {
        const struct { const char* word; char character; } escapes[] = {
            {"c", ':'}, {"s", '/'}, {"lb", '['}, {"rb", ']'}, {"plus", '+'}, {"sp", ' '},
            {"qm", '?'}, {"amp", '&'}, {"hash", '#'}, {"lp", '('}, {"rp", ')'}, {"cm", ','},
            {"at", '@'}, {"excl", '!'}, {"ast", '*'}, {"sq", '\''}, {"dq", '"'}, {"eq", '='},
            {"sc", ';'}, {"lt", '<'}, {"gt", '>'}, {"lc", '{'}, {"rc", '}'}, {"pipe", '|'},
            {"dlr", '$'}, {"pct", '%'}, {"caret", '^'}, {"tilde", '~'}, {"bt", '`'}, {"bs", '\\'},
        };
        QString all;
        QString allTags;
        for (const auto& e : escapes) {
            const QString tag = QStringLiteral("__%1__").arg(QLatin1String(e.word));
            const QString character(QLatin1Char(e.character));
            QCOMPARE(AppSettings::keyFromXmlTag(QStringLiteral("a") + tag + QStringLiteral("b")),
                     QStringLiteral("a") + character + QStringLiteral("b"));
            QVERIFY(AppSettings::keySurvivesFile(QStringLiteral("a") + character));
            all += character;
            allTags += tag;
        }
        QCOMPARE(AppSettings::keyFromXmlTag(allTags), all);
        QVERIFY(AppSettings::keySurvivesFile(all));
        QCOMPARE(AppSettings::keyFromXmlTag(QStringLiteral("a__u0009__b__u01F4FB__")),
                 QString::fromUcs4(U"a\tb\U0001F4FB"));
        QCOMPARE(AppSettings::keyFromXmlTag(QStringLiteral("My__Profile__s___x____s__")),
                 QStringLiteral("My__Profile/_x__/"));
    }

    // A key that already holds an escape's text, or ends so that the next
    // character makes one, is known not to come back.
    void a_key_that_cannot_come_back_is_known() {
        for (const char* name : {"D-104__plus__CPDR", "__u0042__", "a__c", "Rig__sp", "a__c__b"}) {
            QVERIFY2(!AppSettings::keySurvivesFile(
                         QStringLiteral("profile/%1/MicGain").arg(QLatin1String(name))), name);
        }
    }

    // A file an older build wrote, its names as it wrote them, and one a
    // pre-2026-04-30 build wrote with a literal '+' (the load repairs it).
    void older_files_load_the_keys_they_meant() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray profile = "hardware__s__aa__c__bb__c__cc__s__tx__s__profile__s__";
        file.write("<?xml version=\"1.0\"?>\n<NereusSDR>\n"
                   "<audio__s__DspRate>48000</audio__s__DspRate>\n<Plain_Key>1</Plain_Key>\n<"
                   + profile + "SSB__sp__2.8k__sp__CFC__s__MicGain>-5</" + profile
                   + "SSB__sp__2.8k__sp__CFC__s__MicGain>\n<" + profile
                   + "plan__sp__c__s__MicGain>-6</" + profile + "plan__sp__c__s__MicGain>\n<"
                   + profile + "D-104+c+CPDR__s__MicGain>-7</" + profile
                   + "D-104+c+CPDR__s__MicGain>\n</NereusSDR>\n");
        file.close();
        AppSettings s(path);
        s.load();
        QVERIFY(!s.wasCorruptedOnLoad());
        const QString prefix = QStringLiteral("hardware/aa:bb:cc/tx/profile/");
        QCOMPARE(s.value(QStringLiteral("audio/DspRate")).toString(), QStringLiteral("48000"));
        QCOMPARE(s.value(QStringLiteral("Plain_Key")).toString(), QStringLiteral("1"));
        QCOMPARE(s.value(prefix + QStringLiteral("SSB 2.8k CFC/MicGain")).toString(),
                 QStringLiteral("-5"));
        QCOMPARE(s.value(prefix + QStringLiteral("plan c/MicGain")).toString(),
                 QStringLiteral("-6"));
        QCOMPARE(s.value(prefix + QStringLiteral("D-104+c+CPDR/MicGain")).toString(),
                 QStringLiteral("-7"));
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
