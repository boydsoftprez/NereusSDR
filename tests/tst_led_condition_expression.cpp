// no-port-check: NereusSDR-original test.
//
// Container item parity Task 26: the LED condition. evaluateLedCondition over
// a table of expressions (the defined subset, its errors, malformed input
// that must neither crash nor hang), then LEDItem: names from the hosting
// meter's slice readings, clock values and a meter data source; the error in
// the editor with the LED false; no condition keeps the thresholds; the
// condition survives a save and load.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.

#include <QtTest/QtTest>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPointF>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/LedConditionExpression.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/meter_property_editors/LedItemEditor.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {

using Vars = QHash<QString, QVariant>;

// Variables keyed in lowercase; a name not in the table is unknown.
LedConditionLookup lookupIn(const Vars& vars)
{
    return [vars](const QString& name) -> std::optional<QVariant> {
        const auto it = vars.constFind(name.trimmed().toLower());
        if (it == vars.cend()) {
            return std::nullopt;
        }
        return it.value();
    };
}

const Vars kStation{
    {QStringLiteral("signal_strength"), -60.0},
    {QStringLiteral("swr"), 2.5},
    {QStringLiteral("mic"), -10.0},
    {QStringLiteral("count"), 3},
    {QStringLiteral("time_utc"), QStringLiteral("12:00:00")},
    {QStringLiteral("flag"), true},
    {QStringLiteral("novalue"), QVariant()},
    {QStringLiteral("point"), QPointF(1.0, 2.0)},
};

const QUuid kSourceGuid(QStringLiteral("6a0c1d2e-3f40-4a5b-8c6d-7e8f90a1b2c3"));

// A UDP listener data source on loopback (takes any free port).
void writeSource(const QUuid& guid)
{
    auto& s = AppSettings::instance();
    const auto key = [&guid](const QString& prop) {
        return QStringLiteral("MmioEndpoints/") + guid.toString(QUuid::WithoutBraces) + QLatin1Char('/') + prop;
    };
    s.setValue(key(QStringLiteral("Name")), QStringLiteral("Sensors"));
    s.setValue(key(QStringLiteral("Transport")), QStringLiteral("UdpListener"));
    s.setValue(key(QStringLiteral("Format")), QStringLiteral("Json"));
    s.setValue(key(QStringLiteral("Host")), QStringLiteral("127.0.0.1"));
    s.setValue(key(QStringLiteral("Port")), QStringLiteral("0"));
    s.setValue(key(QStringLiteral("Device")), QString());
    s.setValue(key(QStringLiteral("Baud")), QStringLiteral("9600"));
    s.setValue(key(QStringLiteral("Direction")), QStringLiteral("In"));
}

QLabel* conditionErrorLabel(QWidget& editor)
{
    for (QLabel* label : editor.findChildren<QLabel*>()) {
        if (label->toolTip().startsWith(QStringLiteral("While the condition has a mistake"))) {
            return label;
        }
    }
    return nullptr;
}

QDoubleSpinBox* thresholdSpin(QWidget& editor)
{
    for (QDoubleSpinBox* spin : editor.findChildren<QDoubleSpinBox*>()) {
        if (spin->toolTip() == QStringLiteral("Used while the condition is empty.")) {
            return spin;
        }
    }
    return nullptr;
}

} // namespace

class TstLedConditionExpression : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        ExternalVariableEngine::instance().shutdown();
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        ExternalVariableEngine::instance().shutdown();
        AppSettings::instance().clear();
    }

    void evaluates_data()
    {
        QTest::addColumn<QString>("expression");
        QTest::addColumn<bool>("expected");

        // Acceptance: the slice reading against a threshold.
        QTest::newRow("signal above -73") << QStringLiteral("%SIGNAL_STRENGTH% > -73") << true;
        QTest::newRow("signal below -50") << QStringLiteral("%signal_strength% < -50") << true;
        // Acceptance: brackets, && and !.
        QTest::newRow("swr and mic") << QStringLiteral("(%swr% >= 2) && !(%mic% < -20)") << true;
        QTest::newRow("swr or") << QStringLiteral("%swr% < 2 || %mic% == -10") << true;
        QTest::newRow("both false") << QStringLiteral("%swr% < 2 || %mic% != -10") << false;
        // Precedence: * / before + -, comparison before ==, && before ||.
        QTest::newRow("mul before add") << QStringLiteral("1 + 2 * 3 == 7") << true;
        QTest::newRow("brackets first") << QStringLiteral("(1 + 2) * 3 == 9") << true;
        QTest::newRow("division") << QStringLiteral("10 / 4 == 2.5") << true;
        QTest::newRow("left to right") << QStringLiteral("8 - 2 - 1 == 5") << true;
        QTest::newRow("and before or") << QStringLiteral("1 > 2 && 1 > 2 || 2 > 1") << true;
        QTest::newRow("unary minus") << QStringLiteral("-%swr% < 0") << true;
        QTest::newRow("minus minus") << QStringLiteral("2 - -3 == 5") << true;
        QTest::newRow("not not") << QStringLiteral("!!(1 < 2)") << true;
        QTest::newRow("le ge") << QStringLiteral("2 <= 2 && 3 >= 3") << true;
        QTest::newRow("exponent") << QStringLiteral("1e3 == 1000 && .5 == 0.5") << true;
        QTest::newRow("divide by zero") << QStringLiteral("1 / 0 > 1000000") << true;
        QTest::newRow("int variable") << QStringLiteral("%count% == 3") << true;
        // Text and true / false values.
        QTest::newRow("text equal") << QStringLiteral("%time_utc% == \"12:00:00\"") << true;
        QTest::newRow("text join") << QStringLiteral("\"a\" + \"b\" == \"ab\"") << true;
        QTest::newRow("text escape") << QStringLiteral("\"a\\\"b|c\" != \"\"") << true;
        QTest::newRow("text percent") << QStringLiteral("\"%1%%\" == \"%1\" + \"%%\"") << true;
        QTest::newRow("flag") << QStringLiteral("%flag%") << true;
        QTest::newRow("not flag") << QStringLiteral("!%flag%") << false;
        QTest::newRow("flag equal") << QStringLiteral("%flag% == (1 < 2)") << true;
        QTest::newRow("spaces in name") << QStringLiteral("% swr % > 2") << true;
    }

    void evaluates()
    {
        QFETCH(QString, expression);
        QFETCH(bool, expected);
        const LedConditionResult r = evaluateLedCondition(expression, lookupIn(kStation));
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.value, expected);
    }

    void signalThresholdFollowsTheReading()
    {
        Vars vars = kStation;
        const QString expr = QStringLiteral("%SIGNAL_STRENGTH% > -73");
        vars[QStringLiteral("signal_strength")] = -60.0;
        QVERIFY(evaluateLedCondition(expr, lookupIn(vars)).value);
        vars[QStringLiteral("signal_strength")] = -80.0;
        const LedConditionResult r = evaluateLedCondition(expr, lookupIn(vars));
        QVERIFY(r.ok);
        QVERIFY(!r.value);
    }

    void operandsDecide()
    {
        const QString expr = QStringLiteral("(%swr% >= 2) && !(%mic% < -20)");
        const struct { double swr; double mic; bool expected; } cases[] = {
            {2.5, -10.0, true}, {2.0, -20.0, true}, {2.5, -30.0, false}, {1.5, -10.0, false},
            {1.5, -30.0, false},
        };
        for (const auto& c : cases) {
            Vars vars = kStation;
            vars[QStringLiteral("swr")] = c.swr;
            vars[QStringLiteral("mic")] = c.mic;
            const LedConditionResult r = evaluateLedCondition(expr, lookupIn(vars));
            QVERIFY(r.ok);
            QCOMPARE(r.value, c.expected);
        }
    }

    void errors_data()
    {
        QTest::addColumn<QString>("expression");
        QTest::addColumn<QString>("mentions");

        // Acceptance: C# outside the subset.
        QTest::newRow("Math.Abs") << QStringLiteral("Math.Abs(%swr%)") << QStringLiteral("\"Math.Abs\"");
        // Acceptance: an unknown placeholder names it.
        QTest::newRow("unknown name") << QStringLiteral("%nope% > 1") << QStringLiteral("%nope%");
        QTest::newRow("no value yet") << QStringLiteral("%novalue% > 1") << QStringLiteral("%novalue% has no value");
        QTest::newRow("odd value") << QStringLiteral("%point% > 1") << QStringLiteral("%point%");
        QTest::newRow("empty") << QString() << QStringLiteral("empty");
        QTest::newRow("spaces") << QStringLiteral("   ") << QStringLiteral("empty");
        QTest::newRow("ends early") << QStringLiteral("%swr% >") << QStringLiteral("ends");
        QTest::newRow("open bracket") << QStringLiteral("(%swr% > 1") << QStringLiteral("\"(\"");
        QTest::newRow("close bracket") << QStringLiteral("%swr% > 1)") << QStringLiteral("\")\"");
        QTest::newRow("lone close") << QStringLiteral(")") << QStringLiteral("\")\"");
        QTest::newRow("empty brackets") << QStringLiteral("()") << QStringLiteral("nothing");
        QTest::newRow("open percent") << QStringLiteral("%swr > 1") << QStringLiteral("closing %");
        QTest::newRow("empty percent") << QStringLiteral("%% > 1") << QStringLiteral("%%");
        QTest::newRow("open quote") << QStringLiteral("\"abc == 1") << QStringLiteral("closing");
        QTest::newRow("bad escape") << QStringLiteral("\"a\\q\" == \"a\"") << QStringLiteral("\\q");
        QTest::newRow("single equals") << QStringLiteral("%swr% = 2") << QStringLiteral("\"==\"");
        QTest::newRow("single and") << QStringLiteral("%flag% & %flag%") << QStringLiteral("\"&&\"");
        QTest::newRow("single or") << QStringLiteral("%flag% | %flag%") << QStringLiteral("\"||\"");
        QTest::newRow("C# float") << QStringLiteral("5f > 1") << QStringLiteral("\"5f\"");
        QTest::newRow("two dots") << QStringLiteral("1.2.3 > 0") << QStringLiteral("\"1.2.3\"");
        QTest::newRow("bare e") << QStringLiteral("1e > 0") << QStringLiteral("\"1e\"");
        QTest::newRow("missing operator") << QStringLiteral("%swr% 3") << QStringLiteral("\"3\"");
        QTest::newRow("leading operator") << QStringLiteral("> 3") << QStringLiteral("\">\"");
        QTest::newRow("strange character") << QStringLiteral("%swr% > 1 @") << QStringLiteral("\"@\"");
        QTest::newRow("true word") << QStringLiteral("true") << QStringLiteral("\"true\"");
        QTest::newRow("semicolon") << QStringLiteral("%swr% > 1;") << QStringLiteral("\";\"");
        QTest::newRow("number result") << QStringLiteral("%swr%") << QStringLiteral("number");
        QTest::newRow("text result") << QStringLiteral("\"a\"") << QStringLiteral("text");
        QTest::newRow("text order") << QStringLiteral("\"a\" < \"b\"") << QStringLiteral("\"<\"");
        QTest::newRow("mixed equal") << QStringLiteral("1 == \"1\"") << QStringLiteral("\"==\"");
        QTest::newRow("not number") << QStringLiteral("!1") << QStringLiteral("\"!\"");
        QTest::newRow("minus text") << QStringLiteral("-\"a\" == 1") << QStringLiteral("\"-\"");
        QTest::newRow("and numbers") << QStringLiteral("1 && 2") << QStringLiteral("\"&&\"");
        QTest::newRow("text plus number") << QStringLiteral("\"a\" + 1 == \"a1\"") << QStringLiteral("\"+\"");
        QTest::newRow("times flag") << QStringLiteral("%flag% * 2 > 1") << QStringLiteral("\"*\"");
        QTest::newRow("C# ternary") << QStringLiteral("%swr% > 2 ? 1 : 0") << QStringLiteral("\"?\"");
        QTest::newRow("C# method") << QStringLiteral("%time_utc%.StartsWith(\"1\")")
                                   << QStringLiteral("\".\"");
        QTest::newRow("unicode digit") << QStringLiteral("٣ > 1") << QStringLiteral("number");
    }

    void errors()
    {
        QFETCH(QString, expression);
        QFETCH(QString, mentions);
        const LedConditionResult r = evaluateLedCondition(expression, lookupIn(kStation));
        QVERIFY(!r.ok);
        QVERIFY(!r.value);
        QVERIFY2(r.error.contains(mentions), qPrintable(r.error));
        // Plain words a user can act on.
        QVERIFY2(OperatorWording::isPlain(r.error), qPrintable(r.error));
        QVERIFY2(!r.error.contains(QStringLiteral("variable"), Qt::CaseInsensitive), qPrintable(r.error));
        QVERIFY2(!r.error.contains(QStringLiteral("token"), Qt::CaseInsensitive), qPrintable(r.error));
    }

    void noLookupIsUnknownNotACrash()
    {
        const LedConditionResult r = evaluateLedCondition(QStringLiteral("%swr% > 1"), LedConditionLookup{});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("%swr%")));
    }

    // Deep or long input ends with an answer: nesting is capped, long
    // chains are worked out without recursion.
    void hostileInputEnds()
    {
        QElapsedTimer timer;
        timer.start();

        const QString deepBrackets = QString(100000, QLatin1Char('(')) + QStringLiteral("1 > 0")
            + QString(100000, QLatin1Char(')'));
        LedConditionResult r = evaluateLedCondition(deepBrackets, lookupIn(kStation));
        QVERIFY(!r.ok);
        QVERIFY2(r.error.contains(QStringLiteral("deeply")), qPrintable(r.error));

        r = evaluateLedCondition(QString(100000, QLatin1Char('!')) + QStringLiteral("%flag%"), lookupIn(kStation));
        QVERIFY(!r.ok);
        QVERIFY2(r.error.contains(QStringLiteral("deeply")), qPrintable(r.error));

        r = evaluateLedCondition(QString(100000, QLatin1Char('(')), lookupIn(kStation));
        QVERIFY(!r.ok);

        // At the cap, nesting still works.
        const QString atCap = QString(kLedConditionMaxDepth, QLatin1Char('(')) + QStringLiteral("1 > 0")
            + QString(kLedConditionMaxDepth, QLatin1Char(')'));
        r = evaluateLedCondition(atCap, lookupIn(kStation));
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(r.value);

        QString chain = QStringLiteral("0");
        for (int i = 0; i < 20000; ++i) {
            chain += QStringLiteral(" + 1");
        }
        r = evaluateLedCondition(chain + QStringLiteral(" == 20000"), lookupIn(kStation));
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(r.value);

        QString junk;
        for (int i = 0; i < 5000; ++i) {
            junk += QStringLiteral("%\"(|&=!<>");
        }
        r = evaluateLedCondition(junk, lookupIn(kStation));
        QVERIFY(!r.ok);

        QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
    }

    // ---- LEDItem -------------------------------------------------------

    void ledReadsTheSliceReading()
    {
        MeterWidget meter;
        auto* led = new LEDItem(&meter);
        meter.addItem(led);
        led->setCondition(QStringLiteral("%SIGNAL_STRENGTH% > -73"));
        // No reading yet: false, with the reason.
        QVERIFY(!led->isTrue());
        QVERIFY(led->conditionError().contains(QStringLiteral("has no value now")));

        meter.updateMeterValue(MeterBinding::SignalPeak, -60.0);
        QVERIFY(led->evaluateCondition());
        QVERIFY(led->isTrue());
        QVERIFY(led->conditionError().isEmpty());

        meter.updateMeterValue(MeterBinding::SignalPeak, -80.0);
        // The update interval decides when advanceMeter works it out again.
        led->setUpdateIntervalMs(100);
        QVERIFY(led->advanceMeter(1000));
        QVERIFY(!led->isTrue());
        meter.updateMeterValue(MeterBinding::SignalPeak, -60.0);
        QVERIFY(!led->advanceMeter(1050));
        QVERIFY(!led->isTrue());
        QVERIFY(led->advanceMeter(1100));
        QVERIFY(led->isTrue());
    }

    void ledReadsSeveralReadings()
    {
        MeterWidget meter;
        auto* led = new LEDItem(&meter);
        led->setBindingId(MeterBinding::TxSwr);
        meter.addItem(led);
        led->setCondition(QStringLiteral("(%swr% >= 2) && !(%mic% < -20)"));
        meter.updateMeterValue(MeterBinding::TxMic, -10.0);
        // The bound reading arriving works the condition out.
        meter.updateMeterValue(MeterBinding::TxSwr, 2.5);
        QVERIFY(led->isTrue());
        meter.updateMeterValue(MeterBinding::TxMic, -30.0);
        meter.updateMeterValue(MeterBinding::TxSwr, 2.4);
        QVERIFY(!led->isTrue());
    }

    void ledReadsClockValues()
    {
        MeterWidget meter;
        auto* clock = new ClockItem(&meter, false);
        clock->setTimeSource([] { return QDateTime(QDate(2026, 10, 26), QTime(12, 34, 56)); });
        meter.addItem(clock);
        auto* led = new LEDItem(&meter);
        meter.addItem(led);
        led->setCondition(QStringLiteral("%TIME_LOC% == \"12:34:56\" && %date_loc% == \"Mon 26 Oct 2026\""));
        QVERIFY2(led->isTrue(), qPrintable(led->conditionError()));

        // Without a clock the names are still known.
        LEDItem alone;
        alone.setCondition(QStringLiteral("%time_utc% != \"\""));
        QVERIFY2(alone.isTrue(), qPrintable(alone.conditionError()));
    }

    void ledReadsADataSourceValue()
    {
        writeSource(kSourceGuid);
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kSourceGuid);
        QVERIFY(ep);
        const QString prefix = kSourceGuid.toString(QUuid::WithoutBraces) + QLatin1Char('.');
        ep->mergeBatch({{prefix + QStringLiteral("temperature"), 42.0},
                        {prefix + QStringLiteral("state"), QStringLiteral("ok")}});

        LEDItem led;
        const QString id = ep->fourChar();
        led.setCondition(QLatin1Char('%') + id + QStringLiteral(".temperature% > 40 && %") + id
                         + QStringLiteral(".state% == \"ok\""));
        QVERIFY2(led.isTrue(), qPrintable(led.conditionError()));

        // A value the source has not sent yet; a source that does not exist.
        led.setCondition(QLatin1Char('%') + id + QStringLiteral(".humidity% > 1"));
        QVERIFY(!led.isTrue());
        QVERIFY(led.conditionError().contains(QStringLiteral("has no value now")));
        led.setCondition(QStringLiteral("%QQQQ.temperature% > 1"));
        QVERIFY(!led.isTrue());
        QVERIFY(led.conditionError().contains(QStringLiteral("%QQQQ.temperature%")));
    }

    void badConditionShowsInTheEditorAndStaysFalse()
    {
        MeterWidget meter;
        auto* led = new LEDItem(&meter);
        led->setBindingId(MeterBinding::TxSwr);
        meter.addItem(led);
        meter.updateMeterValue(MeterBinding::TxSwr, 5.0);
        // The thresholds would light it.
        QVERIFY(led->isTrue());

        led->setCondition(QStringLiteral("Math.Abs(%swr%)"));
        QVERIFY(!led->isTrue());
        QVERIFY(led->conditionError().contains(QStringLiteral("\"Math.Abs\"")));
        meter.updateMeterValue(MeterBinding::TxSwr, 6.0);
        led->advanceMeter(10000);
        QVERIFY(!led->isTrue());

        LedItemEditor editor;
        editor.setItem(led);
        auto* edit = editor.findChild<QLineEdit*>(QStringLiteral("ledCondition"));
        QLabel* error = conditionErrorLabel(editor);
        QDoubleSpinBox* threshold = thresholdSpin(editor);
        QVERIFY(edit);
        QVERIFY(error);
        QVERIFY(threshold);
        QCOMPARE(edit->text(), QStringLiteral("Math.Abs(%swr%)"));
        QVERIFY(error->text().contains(QStringLiteral("\"Math.Abs\"")));
        QVERIFY(!threshold->isEnabled());

        // Typing a good condition clears the error; a name with no value
        // yet is not a mistake in the editor.
        edit->clear();
        QTest::keyClicks(edit, QStringLiteral("%swr% > 2 && %mic% < 0"));
        QCOMPARE(led->condition(), QStringLiteral("%swr% > 2 && %mic% < 0"));
        QVERIFY2(error->text().isEmpty(), qPrintable(error->text()));
        QTest::keyClicks(edit, QStringLiteral(" && %nope%"));
        QVERIFY(error->text().contains(QStringLiteral("%nope%")));

        // Cleared: the thresholds decide again.
        edit->selectAll();
        QTest::keyClick(edit, Qt::Key_Backspace);
        QVERIFY(led->condition().isEmpty());
        QVERIFY(!led->hasCondition());
        QVERIFY(error->text().isEmpty());
        QVERIFY(threshold->isEnabled());
        QVERIFY(led->isTrue());
    }

    void noConditionKeepsTheThresholds()
    {
        LEDItem led;
        QVERIFY(!led.hasCondition());
        led.setValue(5.0);
        QVERIFY(led.isTrue());
        led.setValue(-5.0);
        QVERIFY(!led.isTrue());
        QVERIFY(led.conditionError().isEmpty());
        // The record is the one written before the condition existed.
        QCOMPARE(led.serialize().split(QLatin1Char('|')).size(), 28);
        LEDItem back;
        QVERIFY(back.deserialize(led.serialize()));
        QVERIFY(!back.hasCondition());
        // A whitespace condition is no condition.
        led.setCondition(QStringLiteral("  "));
        QVERIFY(!led.hasCondition());
        led.setValue(5.0);
        QVERIFY(led.isTrue());
        QCOMPARE(led.serialize().split(QLatin1Char('|')).size(), 28);
    }

    void conditionSurvivesSaveAndLoad()
    {
        LEDItem led;
        const QString cond = QStringLiteral("%swr% > 1 || \"a|b%1%%\" == \"a|b%1%%\"");
        led.setCondition(cond);
        const QString record = led.serialize();
        QCOMPARE(record.split(QLatin1Char('|')).size(), 29);
        LEDItem back;
        QVERIFY(back.deserialize(record));
        QCOMPARE(back.condition(), cond);
        QCOMPARE(back.serialize(), record);

        // After the old unfilled placeholders, the condition is at opt + 8.
        const QStringList parts = record.split(QLatin1Char('|'));
        QStringList shifted = parts.mid(0, 20);
        shifted << QStringLiteral("%15") << QStringLiteral("%16") << QStringLiteral("%17") << parts.mid(20);
        LEDItem old;
        QVERIFY(old.deserialize(shifted.join(QLatin1Char('|'))));
        QCOMPARE(old.condition(), cond);

        // An empty field is no condition; a damaged one is refused.
        LEDItem empty;
        QVERIFY(empty.deserialize(record.left(record.lastIndexOf(QLatin1Char('|')) + 1)));
        QVERIFY(!empty.hasCondition());
        LEDItem damaged;
        QVERIFY(!damaged.deserialize(record + QStringLiteral("*!")));
    }
};

QTEST_MAIN(TstLedConditionExpression)
#include "tst_led_condition_expression.moc"
