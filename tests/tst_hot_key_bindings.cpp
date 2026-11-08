// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-07 - J.J. Boyd (KG4VCF), hot keys: VOX on or off (vox.toggle).
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// The action list, chord text and bindings, against the plan's Action ids table
// and rebinding rules (R-HK-01, R-HK-12, R-HK-18).
#include <QtTest>

#include <QKeySequence>
#include <QSet>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyChord.h"

using namespace NereusSDR;

namespace {

struct ExpectedAction {
    const char* id;
    const char* group;
    HotKeyGesture gesture;
    bool transmits;
    QStringList keys;
};

// The plan's Action ids table, written out by hand so the two must agree.
QList<ExpectedAction> expectedActions()
{
    using G = HotKeyGesture;
    QList<ExpectedAction> e = {
        {"tune.up", "Tuning", G::Turn, false, {"Right"}},
        {"tune.down", "Tuning", G::Turn, false, {"Left"}},
        {"tune.up10", "Tuning", G::Turn, false, {"Shift+Right"}},
        {"tune.down10", "Tuning", G::Turn, false, {"Shift+Left"}},
        {"step.larger", "Tuning", G::Press, false, {"]"}},
        {"step.smaller", "Tuning", G::Press, false, {"["}},
        {"slice.lock", "Tuning", G::Press, false, {"L"}},
        {"tune.goto", "Tuning", G::Press, false, {"G"}},
        {"slice.next", "Tuning", G::Press, false, {"`"}},
        {"filter.next", "Filter", G::Press, false, {"B"}},
        {"filter.previous", "Filter", G::Press, false, {"V"}},
        {"volume.up", "Audio", G::Turn, false, {"Up", "Num++"}},
        {"volume.down", "Audio", G::Turn, false, {"Down", "Num+-"}},
        {"mute", "Audio", G::Press, false, {"U", "Num+*"}},
        {"bandstack.next", "Band and mode", G::Press, false, {"M"}},
        {"bandstack.previous", "Band and mode", G::Press, false, {"N"}},
        {"band.next", "Band and mode", G::Press, false, {"J"}},
        {"band.previous", "Band and mode", G::Press, false, {"H"}},
        {"bandstack.add", "Band and mode", G::Press, false, {"Shift+M"}},
        {"bandstack.panel", "Band and mode", G::Press, false, {"Shift+N"}},
        {"mode.next", "Band and mode", G::Press, false, {"X"}},
        {"mode.previous", "Band and mode", G::Press, false, {"Z"}},
        {"zoom.in", "Display", G::Press, false, {"="}},
        {"zoom.out", "Display", G::Press, false, {"-"}},
        {"nr.toggle", "DSP", G::Press, false, {"R"}},
        {"nb.toggle", "DSP", G::Press, false, {"E"}},
        {"anf.toggle", "DSP", G::Press, false, {"A"}},
        {"rit.toggle", "DSP", G::Press, false, {"I"}},
        {"xit.toggle", "DSP", G::Press, false, {"O"}},
        {"tx.hold", "Transmit", G::Hold, true, {"Space"}},
        {"tx.mox", "Transmit", G::Press, true, {"Ctrl+Shift+M"}},
        {"tx.tune", "Transmit", G::Press, true, {"Ctrl+T"}},
        {"tx.twotone", "Transmit", G::Press, true, {"Ctrl+Shift+T"}},
        // JJ, 2026-11-07: VOX on or off, with no default key.
        {"vox.toggle", "Transmit", G::Press, true, {}},
        {"cw.msg1", "CW", G::Press, true, {"Ctrl+F1"}},
        {"cw.msg2", "CW", G::Press, true, {"Ctrl+F2"}},
        {"cw.msg3", "CW", G::Press, true, {"Ctrl+F3"}},
        {"cw.msg4", "CW", G::Press, true, {"Ctrl+F4"}},
        {"cw.msg5", "CW", G::Press, true, {"Ctrl+F5"}},
        {"cw.msg6", "CW", G::Press, true, {"Ctrl+F6"}},
        {"cw.msg7", "CW", G::Press, true, {"Ctrl+F7"}},
        {"cw.msg8", "CW", G::Press, true, {"Ctrl+F8"}},
        {"cw.stop", "CW", G::Press, false, {"Ctrl+F10"}},
        {"cw.dot", "CW", G::Hold, true, {","}},
        {"cw.dash", "CW", G::Hold, true, {"."}},
        {"cw.straight", "CW", G::Hold, true, {"/"}},
        {"voice.msg1", "Voice", G::Press, true, {"Ctrl+Shift+F1"}},
        {"voice.msg2", "Voice", G::Press, true, {"Ctrl+Shift+F2"}},
        {"voice.msg3", "Voice", G::Press, true, {"Ctrl+Shift+F3"}},
        {"voice.msg4", "Voice", G::Press, true, {"Ctrl+Shift+F4"}},
        {"voice.stop", "Voice", G::Press, false, {"Ctrl+Shift+F10"}},
    };
    return e;
}

QStringList storedHotKeySettings()
{
    QStringList keys;
    const QStringList all = AppSettings::instance().allKeys();
    for (const QString& key : all) {
        if (key.startsWith(QLatin1String("HotKeys/"))) {
            keys.append(key);
        }
    }
    keys.sort();
    return keys;
}

} // namespace

class TstHotKeyBindings : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void listMatchesThePlanTable()
    {
        const QList<ExpectedAction> expected = expectedActions();
        const QList<HotKeyAction>& actual = HotKeyActionList::all();
        QCOMPARE(actual.size(), 51);
        QCOMPARE(actual.size(), expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            const ExpectedAction& e = expected.at(i);
            const HotKeyAction& a = actual.at(i);
            QCOMPARE(a.id, QString::fromLatin1(e.id));
            QCOMPARE(a.group, QString::fromLatin1(e.group));
            QCOMPARE(a.gesture, e.gesture);
            QCOMPARE(a.transmits, e.transmits);
            QCOMPARE(a.defaultKeys, e.keys);
            QVERIFY2(!a.name.isEmpty(), e.id);
            QCOMPARE(HotKeyActionList::find(a.id), &a);
        }
        QCOMPARE(HotKeyActionList::find(QStringLiteral("no.such")), nullptr);
    }

    void scopesFollowTheDesign()
    {
        const auto scopeOf = [](const char* id) {
            return HotKeyActionList::find(QString::fromLatin1(id))->scope;
        };
        QCOMPARE(scopeOf("tune.up"), HotKeyScope::Slice);
        QCOMPARE(scopeOf("volume.up"), HotKeyScope::Slice);
        QCOMPARE(scopeOf("band.next"), HotKeyScope::Slice);
        QCOMPARE(scopeOf("tx.hold"), HotKeyScope::TransmitSlice);
        QCOMPARE(scopeOf("cw.stop"), HotKeyScope::TransmitSlice);
        QCOMPARE(scopeOf("voice.msg1"), HotKeyScope::TransmitSlice);
        QCOMPARE(scopeOf("zoom.in"), HotKeyScope::Pan);
        QCOMPARE(scopeOf("zoom.out"), HotKeyScope::Pan);
        QCOMPARE(scopeOf("slice.next"), HotKeyScope::Window);
        QCOMPARE(scopeOf("bandstack.panel"), HotKeyScope::Window);
    }

    void defaultsAreSharedByNoOneAndBreakNoRule()
    {
        QSet<QString> seen;
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            for (const QString& key : action.defaultKeys) {
                QVERIFY2(!seen.contains(key), qPrintable(key));
                seen.insert(key);
                QVERIFY2(HotKeyBindings::reservedBy(key).isEmpty(), qPrintable(key));
                const auto chord = HotKeyChord::parse(key);
                QVERIFY2(chord.has_value(), qPrintable(key));
                QCOMPARE(chord->toString(), key);
            }
        }
    }

    void freshSettingsUseTheDefaults()
    {
        HotKeyBindings b;
        b.load();
        QCOMPARE(b.actionForChord("Right"), QStringLiteral("tune.up"));
        QCOMPARE(b.actionForChord("Num++"), QStringLiteral("volume.up"));
        QCOMPARE(b.actionForChord("Up"), QStringLiteral("volume.up"));
        QCOMPARE(b.actionForChord("Ctrl+T"), QStringLiteral("tx.tune"));
        QCOMPARE(b.actionForChord("Ctrl+F8"), QStringLiteral("cw.msg8"));
        QCOMPARE(b.actionForChord("/"), QStringLiteral("cw.straight"));
        QCOMPARE(b.actionForChord("Ctrl+Y"), QString());
        QCOMPARE(b.actionForChord("not a key"), QString());
        QVERIFY(b.enabled());
        QVERIFY(b.pinFor("tune.up").isNull());
    }

    void usableBeforeLoad()
    {
        HotKeyBindings b;
        QCOMPARE(b.actionForChord("Right"), QStringLiteral("tune.up"));
    }

    void chordParsing_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("canonical");  // empty means refused

        QTest::newRow("modifier order") << "Shift+Ctrl+T" << "Ctrl+Shift+T";
        QTest::newRow("all modifiers") << "Num+Meta+Alt+Shift+Ctrl+F1" << "Ctrl+Shift+Alt+Meta+Num+F1";
        QTest::newRow("Qt order") << "Ctrl+Alt+Shift+X" << "Ctrl+Shift+Alt+X";
        QTest::newRow("case") << "ctrl+t" << "Ctrl+T";
        QTest::newRow("named key case") << "shift+PGUP" << "Shift+PgUp";
        QTest::newRow("keypad plus") << "Num++" << "Num++";
        QTest::newRow("keypad minus") << "Num+-" << "Num+-";
        QTest::newRow("keypad star") << "Num+*" << "Num+*";
        QTest::newRow("plus key") << "+" << "+";
        QTest::newRow("ctrl plus") << "Ctrl++" << "Ctrl++";
        QTest::newRow("comma") << "Ctrl+," << "Ctrl+,";
        QTest::newRow("backtick") << "`" << "`";
        QTest::newRow("F35") << "F35" << "F35";
        QTest::newRow("empty") << "" << "";
        QTest::newRow("no key") << "Ctrl+" << "";
        QTest::newRow("only a modifier") << "Ctrl" << "";
        QTest::newRow("keypad no key") << "Num+" << "";
        QTest::newRow("modifier twice") << "Ctrl+Ctrl+T" << "";
        QTest::newRow("two keys") << "Ctrl+T+Y" << "";
        QTest::newRow("unknown key") << "Banana" << "";
        QTest::newRow("unknown modifier") << "Hyper+T" << "";
        QTest::newRow("double plus") << "++" << "";
        QTest::newRow("literal space") << " " << "";
        QTest::newRow("F0") << "F0" << "";
        QTest::newRow("F36") << "F36" << "";
        QTest::newRow("spaces") << "Ctrl + T" << "";
    }

    void chordParsing()
    {
        QFETCH(QString, text);
        QFETCH(QString, canonical);
        const std::optional<HotKeyChord> chord = HotKeyChord::parse(text);
        if (canonical.isEmpty()) {
            QVERIFY(!chord.has_value());
        } else {
            QVERIFY(chord.has_value());
            QCOMPARE(chord->toString(), canonical);
        }
    }

    void chordClassification()
    {
        QVERIFY(HotKeyChord::parse("T")->hasNoModifier());
        QVERIFY(!HotKeyChord::parse("Shift+T")->hasNoModifier());
        QVERIFY(!HotKeyChord::parse("Num+5")->hasNoModifier());
        for (const char* key : {"T", "7", "Left", "Right", "Up", "Down"}) {
            QVERIFY2(HotKeyChord::parse(key)->isLetterOrNumberOrArrow(), key);
        }
        for (const char* key : {",", ";", "Space", "F1", "`", "Home"}) {
            QVERIFY2(!HotKeyChord::parse(key)->isLetterOrNumberOrArrow(), key);
        }
        QCOMPARE(*HotKeyChord::parse("Shift+Ctrl+T"), *HotKeyChord::parse("Ctrl+Shift+T"));
        QVERIFY(!(*HotKeyChord::parse("Ctrl+T") == *HotKeyChord::parse("Ctrl+Y")));
    }

    // Core code holds chords as text, so this is where the text is held against
    // Qt's own: every name parse() knows must be one QKeySequence reads and
    // writes, and whatever QKeySequence writes must parse to the same chord.
    void chordTextAgreesWithQt()
    {
        QStringList texts = {
            "Esc", "Tab", "Backtab", "Backspace", "Return", "Enter", "Ins", "Del", "Pause", "Print", "SysReq",
            "Home", "End", "Left", "Up", "Right", "Down", "PgUp", "PgDown", "CapsLock", "NumLock", "ScrollLock",
            "Menu", "Help", "Space", "F1", "F35", "Ctrl+Shift+Alt+Meta+Num+Q",
        };
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            texts += action.defaultKeys;
        }
        for (const QString& text : std::as_const(texts)) {
            const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
            QVERIFY2(sequence.count() == 1, qPrintable(text));
            const QString fromQt = sequence.toString(QKeySequence::PortableText);
            const std::optional<HotKeyChord> chord = HotKeyChord::parse(fromQt);
            QVERIFY2(chord.has_value(), qPrintable(fromQt));
            QCOMPARE(chord->toString(), text);
        }
    }

    void transmitActionsRefusePlainKeys_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("key");
        QTest::newRow("mox letter") << "tx.mox" << "T";
        QTest::newRow("hold number") << "tx.hold" << "7";
        QTest::newRow("cw arrow") << "cw.msg1" << "Left";
        QTest::newRow("voice letter") << "voice.msg1" << "Q";
        QTest::newRow("cw stop is in the CW group") << "cw.stop" << "S";
        QTest::newRow("voice stop is in the Voice group") << "voice.stop" << "S";
        QTest::newRow("keypad number is still a plain number") << "tx.mox" << "Num+5";
    }

    void transmitActionsRefusePlainKeys()
    {
        QFETCH(QString, id);
        QFETCH(QString, key);
        HotKeyBindings b;
        const QStringList before = b.keysFor(id);
        const HotKeyBindOutcome outcome = b.bind(id, key, false);
        QCOMPARE(outcome.result, HotKeyBindResult::RefusedTransmitPlainKey);
        QVERIFY(!outcome.reason.isEmpty());
        QCOMPARE(b.keysFor(id), before);
    }

    void latchedActionsRefuseSpace()
    {
        for (const char* id : {"tx.mox", "tx.tune", "tx.twotone"}) {
            HotKeyBindings b;
            const HotKeyBindOutcome outcome = b.bind(QString::fromLatin1(id), "Space", false);
            QCOMPARE(outcome.result, HotKeyBindResult::RefusedLatchedOnSpace);
            QVERIFY(!outcome.reason.isEmpty());
        }
    }

    void transmitActionsTakeOtherKeys()
    {
        HotKeyBindings b;
        QCOMPARE(b.bind("cw.dot", ",", false).result, HotKeyBindResult::Bound);
        QCOMPARE(b.bind("tx.mox", "Ctrl+Y", false).result, HotKeyBindResult::Bound);
        QCOMPARE(b.bind("cw.dash", ";", false).result, HotKeyBindResult::Bound);
        QCOMPARE(b.actionForChord("Ctrl+Y"), QStringLiteral("tx.mox"));
        QCOMPARE(b.actionForChord(";"), QStringLiteral("cw.dash"));
    }

    void reservedKeysNameTheMenuItem_data()
    {
        QTest::addColumn<QString>("chord");
        QTest::addColumn<QString>("item");
        QTest::newRow("comma") << "Ctrl+," << "Settings";
        QTest::newRow("Q") << "Ctrl+Q" << "Quit";
        QTest::newRow("K") << "Ctrl+K" << "Connect";
        QTest::newRow("L") << "Ctrl+L" << "Pan Layout";
        QTest::newRow("R") << "Ctrl+R" << "Add slice on active pan";
        QTest::newRow("M") << "Ctrl+M" << "Minimal Mode";
        QTest::newRow("X") << "Ctrl+X" << "CWX";
        for (int n = 1; n <= 9; ++n) {
            const QByteArray tag = QByteArray::number(n);
            QTest::newRow(tag.constData()) << QStringLiteral("Ctrl+%1").arg(n) << QStringLiteral("Profile %1").arg(n);
        }
        QTest::newRow("shift A") << "Ctrl+Shift+A" << "Arrange Workspace";
        QTest::newRow("shift S") << "Ctrl+Shift+S" << "Spot Hub";
        QTest::newRow("shift R") << "Ctrl+Shift+R" << "FreeDV Reporter";
        QTest::newRow("shift D") << "Ctrl+Shift+D" << "Diversity";
        QTest::newRow("shift K") << "Ctrl+Shift+K" << "Disconnect";
        QTest::newRow("shift N") << "Ctrl+Shift+N" << "TNF";
        QTest::newRow("shift X") << "Ctrl+Shift+X" << "Clear Spots";
        QTest::newRow("any modifier order") << "Shift+Ctrl+X" << "Clear Spots";
    }

    void reservedKeysNameTheMenuItem()
    {
        QFETCH(QString, chord);
        QFETCH(QString, item);
        QCOMPARE(HotKeyBindings::reservedBy(chord), item);
        HotKeyBindings b;
        const HotKeyBindOutcome outcome = b.bind("mute", chord, false);
        QCOMPARE(outcome.result, HotKeyBindResult::RefusedReserved);
        QVERIFY2(outcome.reason.contains(item), qPrintable(outcome.reason));
        QCOMPARE(b.keysFor("mute"), (QStringList{"U", "Num+*"}));
    }

    void unreservedKeysAreNotReserved()
    {
        QCOMPARE(HotKeyBindings::reservedBy("Ctrl+T"), QString());
        QCOMPARE(HotKeyBindings::reservedBy("Ctrl+0"), QString());
        QCOMPARE(HotKeyBindings::reservedBy("Q"), QString());
        QCOMPARE(HotKeyBindings::reservedBy("rubbish key"), QString());
    }

    void conflictChangesNothingUntilReplaced()
    {
        HotKeyBindings b;
        QSignalSpy changed(&b, &HotKeyBindings::bindingsChanged);

        const HotKeyBindOutcome refused = b.bind("mute", "B", false);
        QCOMPARE(refused.result, HotKeyBindResult::Conflict);
        QCOMPARE(refused.conflictId, QStringLiteral("filter.next"));
        QVERIFY(!refused.reason.isEmpty());
        QCOMPARE(b.keysFor("mute"), (QStringList{"U", "Num+*"}));
        QCOMPARE(b.keysFor("filter.next"), QStringList{"B"});
        QCOMPARE(changed.count(), 0);

        const HotKeyBindOutcome bound = b.bind("mute", "B", true);
        QCOMPARE(bound.result, HotKeyBindResult::Bound);
        QVERIFY(bound.reason.isEmpty());
        QCOMPARE(b.keysFor("mute"), QStringList{"B"});
        QVERIFY(b.keysFor("filter.next").isEmpty());
        QCOMPARE(b.actionForChord("B"), QStringLiteral("mute"));
        QCOMPARE(b.actionForChord("U"), QString());
        QCOMPARE(changed.count(), 1);
    }

    void replaceTakesOnlyTheOneKey()
    {
        HotKeyBindings b;
        QCOMPARE(b.bind("mute", "Up", true).result, HotKeyBindResult::Bound);
        QCOMPARE(b.keysFor("volume.up"), QStringList{"Num++"});
    }

    void rebindingAnOwnKeyIsNoConflict()
    {
        HotKeyBindings b;
        QCOMPARE(b.bind("mute", "U", false).result, HotKeyBindResult::Bound);
        QCOMPARE(b.keysFor("mute"), QStringList{"U"});
    }

    void invalidBindsAreRefused()
    {
        HotKeyBindings b;
        QCOMPARE(b.bind("no.such", "Ctrl+Y", false).result, HotKeyBindResult::RefusedInvalid);
        QCOMPARE(b.bind("mute", "Ctrl+", false).result, HotKeyBindResult::RefusedInvalid);
        QCOMPARE(b.keysFor("mute"), (QStringList{"U", "Num+*"}));
    }

    void clearKeyTakesOnlyThatKey()
    {
        HotKeyBindings b;
        QSignalSpy spy(&b, &HotKeyBindings::bindingsChanged);
        b.setPin("volume.up", QLatin1Char('B'));
        spy.clear();
        b.clearKey("volume.up", "Up");
        QCOMPARE(b.keysFor("volume.up"), QStringList{"Num++"});
        QVERIFY(b.actionForChord("Up").isEmpty());
        QCOMPARE(b.actionForChord("Num++"), QString("volume.up"));
        QCOMPARE(b.pinFor("volume.up"), QLatin1Char('B'));
        QCOMPARE(spy.size(), 1);

        // A key it doesn't have, an action or a chord that doesn't exist: nothing.
        b.clearKey("volume.up", "Up");
        b.clearKey("volume.up", "L");
        b.clearKey("no.such.action", "Num++");
        b.clearKey("volume.up", "not a key");
        QCOMPARE(b.keysFor("volume.up"), QStringList{"Num++"});
        QCOMPARE(spy.size(), 1);

        // Any spelling of the chord.
        b.clearKey("volume.up", "num++");
        QVERIFY(b.keysFor("volume.up").isEmpty());
    }

    void aStrayPressSaysWhatItWouldDo()
    {
        HotKeyBindings b;
        QCOMPARE(b.bind("cw.stop", "Q", false).reason,
                 QString("Q is a plain key, so a stray press would stop the message. "
                         "Use Space, a function key or a combination."));
        QCOMPARE(b.bind("voice.stop", "Q", false).reason,
                 QString("Q is a plain key, so a stray press would stop the message. "
                         "Use Space, a function key or a combination."));
        QCOMPARE(b.bind("tx.hold", "Q", false).reason,
                 QString("Q is a plain key, so a stray press would transmit. "
                         "Use Space, a function key or a combination."));
        QCOMPARE(b.bind("cw.msg1", "Q", false).reason,
                 QString("Q is a plain key, so a stray press would transmit. "
                         "Use Space, a function key or a combination."));
    }

    void clearAndReset()
    {
        HotKeyBindings b;
        b.clear("mute");
        QVERIFY(b.keysFor("mute").isEmpty());
        QCOMPARE(b.actionForChord("U"), QString());

        b.resetToDefault("mute");
        QCOMPARE(b.keysFor("mute"), (QStringList{"U", "Num+*"}));

        QCOMPARE(b.bind("mute", "B", true).result, HotKeyBindResult::Bound);
        b.resetToDefault("filter.next");
        QCOMPARE(b.actionForChord("B"), QStringLiteral("filter.next"));
        QVERIFY(b.keysFor("mute").isEmpty());

        b.setPin("mute", QLatin1Char('B'));
        b.setEnabled(false);
        b.resetAll();
        QCOMPARE(b.keysFor("mute"), (QStringList{"U", "Num+*"}));
        QVERIFY(b.pinFor("mute").isNull());
        QVERIFY(!b.enabled());
    }

    void pins()
    {
        HotKeyBindings b;
        b.setPin("tune.up", QLatin1Char('b'));
        QCOMPARE(b.pinFor("tune.up"), QLatin1Char('B'));
        b.setPin("tune.up", QChar());
        QVERIFY(b.pinFor("tune.up").isNull());

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Pin that is not allowed"));
        b.setPin("tune.up", QLatin1Char('E'));
        QVERIFY(b.pinFor("tune.up").isNull());

        // A transmit action uses the transmit slice; it can't be pinned.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Pin that is not allowed"));
        b.setPin("tx.mox", QLatin1Char('A'));
        QVERIFY(b.pinFor("tx.mox").isNull());
    }

    void enabledSignalsOnlyOnChange()
    {
        HotKeyBindings b;
        QSignalSpy spy(&b, &HotKeyBindings::enabledChanged);
        b.setEnabled(true);
        QCOMPARE(spy.count(), 0);
        b.setEnabled(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    void saveStoresOnlyWhatDiffers()
    {
        HotKeyBindings first;
        first.save();
        QVERIFY(storedHotKeySettings().isEmpty());

        QCOMPARE(first.bind("mute", "B", true).result, HotKeyBindResult::Bound);
        first.clear("zoom.in");
        first.setPin("tune.up", QLatin1Char('C'));
        first.setEnabled(false);
        first.save();

        QCOMPARE(storedHotKeySettings(),
                 (QStringList{"HotKeys/Binding/filter.next", "HotKeys/Binding/mute", "HotKeys/Binding/zoom.in",
                              "HotKeys/Enabled", "HotKeys/Pin/tune.up"}));
        AppSettings& s = AppSettings::instance();
        QCOMPARE(s.value("HotKeys/Binding/mute").toString(), QStringLiteral("B"));
        QCOMPARE(s.value("HotKeys/Binding/filter.next").toString(), QString());
        QCOMPARE(s.value("HotKeys/Enabled").toString(), QStringLiteral("False"));
        QCOMPARE(s.value("HotKeys/Pin/tune.up").toString(), QStringLiteral("C"));

        HotKeyBindings second;
        second.load();
        QCOMPARE(second.exportText(), first.exportText());
        QVERIFY(!second.enabled());
        QCOMPARE(second.pinFor("tune.up"), QLatin1Char('C'));
        QVERIFY(second.keysFor("zoom.in").isEmpty());
        QVERIFY(second.keysFor("filter.next").isEmpty());
        // Everything else was never stored, so it follows the table.
        QCOMPARE(second.keysFor("tx.mox"), QStringList{"Ctrl+Shift+M"});

        // Putting it all back leaves nothing stored.
        second.resetAll();
        second.setEnabled(true);
        second.save();
        QVERIFY(storedHotKeySettings().isEmpty());
    }

    void changedDefaultYieldsToASavedKey()
    {
        // A saved key on a default another action holds, the shape a later
        // change to the defaults leaves: the saved key stands, and the action
        // with no saved value keeps the rest of its defaults.
        AppSettings::instance().setValue("HotKeys/Binding/mute", "Num++");
        HotKeyBindings b;
        b.load();
        QCOMPARE(b.actionForChord("Num++"), QStringLiteral("mute"));
        QCOMPARE(b.keysFor("volume.up"), QStringList{"Up"});
    }

    void unknownSettingsAreDropped()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue("HotKeys/Binding/gone.action", "Ctrl+Y");
        s.setValue("HotKeys/Pin/gone.action", "A");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown action.*HotKeys/Binding/gone.action"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown action.*HotKeys/Pin/gone.action"));
        HotKeyBindings b;
        b.load();
        QCOMPARE(b.actionForChord("Ctrl+Y"), QString());
        QVERIFY(!s.contains("HotKeys/Binding/gone.action"));
        QVERIFY(!s.contains("HotKeys/Pin/gone.action"));
    }

    void savedKeysBreakingARuleAreDropped()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue("HotKeys/Binding/tx.mox", "T");
        s.setValue("HotKeys/Pin/tx.tune", "A");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("saved key that is not allowed"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("saved pin that is not allowed"));
        HotKeyBindings b;
        b.load();
        QCOMPARE(b.actionForChord("T"), QString());
        QVERIFY(b.pinFor("tx.tune").isNull());
    }

    void exportFormat()
    {
        HotKeyBindings b;
        b.setPin("tune.up", QLatin1Char('D'));
        const QStringList lines = b.exportText().split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 52);
        QCOMPARE(lines.at(0), QStringLiteral("# NereusSDR shortcuts 1"));
        QCOMPARE(lines.at(1), QStringLiteral("tune.up\tRight\tD"));
        QVERIFY(lines.contains(QStringLiteral("volume.up\tUp Num++\t")));
        QVERIFY(lines.contains(QStringLiteral("cw.dot\t,\t")));
        QVERIFY(lines.contains(QStringLiteral("tx.hold\tSpace\t")));
        QVERIFY(lines.contains(QStringLiteral("vox.toggle\t\t")));
    }

    void importRestoresAnExport()
    {
        HotKeyBindings source;
        QCOMPARE(source.bind("mute", "B", true).result, HotKeyBindResult::Bound);
        QCOMPARE(source.bind("filter.previous", "Ctrl+,", false).result, HotKeyBindResult::RefusedReserved);
        QCOMPARE(source.bind("filter.previous", "Shift+,", false).result, HotKeyBindResult::Bound);
        QCOMPARE(source.bind("slice.lock", "Shift+Space", false).result, HotKeyBindResult::Bound);
        source.clear("zoom.in");
        source.setPin("tune.down", QLatin1Char('A'));
        const QString text = source.exportText();

        HotKeyBindings target;
        QSignalSpy changed(&target, &HotKeyBindings::bindingsChanged);
        QCOMPARE(target.importText(text), QStringList());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(target.exportText(), text);
        QCOMPARE(target.actionForChord("Shift+,"), QStringLiteral("filter.previous"));
        QCOMPARE(target.actionForChord("Shift+Space"), QStringLiteral("slice.lock"));
        QCOMPARE(target.actionForChord("B"), QStringLiteral("mute"));
        QCOMPARE(target.pinFor("tune.down"), QLatin1Char('A'));
    }

    void importOrderDoesNotMatter()
    {
        // Two actions swap keys; each line names a key the other holds right now.
        HotKeyBindings b;
        const QStringList skipped = b.importText(QStringLiteral("filter.next\tV\t\nfilter.previous\tB\t\n"));
        QCOMPARE(skipped, QStringList());
        QCOMPARE(b.actionForChord("V"), QStringLiteral("filter.next"));
        QCOMPARE(b.actionForChord("B"), QStringLiteral("filter.previous"));
    }

    void importSkipsBadLinesAndAppliesGoodOnes()
    {
        HotKeyBindings b;
        const QStringList skipped = b.importText(QStringLiteral("# NereusSDR shortcuts 1\r\n"
                                                                "gone.action\tCtrl+Y\t\r\n"
                                                                "tx.mox\tT\t\r\n"
                                                                "\r\n"
                                                                "mute\tShift+U\t\r\n"));
        QCOMPARE(skipped.size(), 2);
        QVERIFY2(skipped.at(0).contains("gone.action") && skipped.at(0).contains("no action"),
                 qPrintable(skipped.at(0)));
        QVERIFY2(skipped.at(1).contains("tx.mox") && skipped.at(1).contains("plain key"),
                 qPrintable(skipped.at(1)));
        QCOMPARE(b.keysFor("mute"), QStringList{"Shift+U"});
        QCOMPARE(b.keysFor("tx.mox"), QStringList{"Ctrl+Shift+M"});
    }

    void importRefusesEachKindOfBadLine()
    {
        HotKeyBindings b;
        const QString before = b.exportText();
        const QStringList skipped = b.importText(QStringLiteral("just some words\n"
                                                                "mute\tNot+A+Key\t\n"
                                                                "mute\tShift+U\tE\n"
                                                                "tx.mox\tCtrl+Y\tA\n"
                                                                "mute\tCtrl+Q\t\n"
                                                                "tx.tune\tSpace\t\n"));
        QCOMPARE(skipped.size(), 6);
        QCOMPARE(b.exportText(), before);

        const QStringList twice = b.importText(QStringLiteral("mute\tShift+U\t\nmute\tShift+Y\t\n"
                                                              "zoom.in\tShift+U\t\n"));
        QCOMPARE(twice.size(), 2);
        QCOMPARE(b.keysFor("mute"), QStringList{"Shift+U"});
        QCOMPARE(b.keysFor("zoom.in"), QStringList{"="});
    }

    void importTakesAKeyFromAnActionItDoesNotList()
    {
        HotKeyBindings b;
        QCOMPARE(b.importText(QStringLiteral("mute\tB\t\n")), QStringList());
        QCOMPARE(b.actionForChord("B"), QStringLiteral("mute"));
        QVERIFY(b.keysFor("filter.next").isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstHotKeyBindings)
#include "tst_hot_key_bindings.moc"
