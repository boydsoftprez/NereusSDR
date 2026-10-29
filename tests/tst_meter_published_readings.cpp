// no-port-check: NereusSDR-original tests for Thetis reading names and
// the readings a clock publishes.
// Modification history (NereusSDR):
//   2026-10-25 - Reading names both ways, clock time and date readings
//                 under a fixed time source. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QDateTime>
#include <QTimeZone>

#include "gui/meters/ClockItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/SpacerItem.h"
#include "gui/meters/ThetisReadingNames.h"

using namespace NereusSDR;

class TestMeterPublishedReadings : public QObject {
    Q_OBJECT

private slots:
    void readingNameForBinding_data()
    {
        QTest::addColumn<int>("binding");
        QTest::addColumn<QString>("name");
        QTest::newRow("swr") << MeterBinding::TxSwr << QStringLiteral("swr");
        QTest::newRow("signal") << MeterBinding::SignalPeak << QStringLiteral("signal_strength");
        QTest::newRow("avg") << MeterBinding::SignalAvg << QStringLiteral("avg_signal_strength");
        QTest::newRow("pwr") << MeterBinding::TxPower << QStringLiteral("pwr");
        QTest::newRow("cfc") << MeterBinding::TxCfc << QStringLiteral("cfc_av");
        QTest::newRow("lvl gain") << MeterBinding::TxLevelerGain << QStringLiteral("lvl_g");
        QTest::newRow("volts") << MeterBinding::HwVolts << QStringLiteral("volts");
        QTest::newRow("azimuth") << MeterBinding::RotatorAz << QStringLiteral("az");
        QTest::newRow("no thetis reading") << MeterBinding::HwTemperature << QString();
        QTest::newRow("unknown id") << 9999 << QString();
    }

    void readingNameForBinding()
    {
        QFETCH(int, binding);
        QFETCH(QString, name);
        QCOMPARE(thetisReadingName(binding), name);
    }

    void bindingForName()
    {
        QCOMPARE(bindingForThetisReadingName(QStringLiteral("SWR")), MeterBinding::TxSwr);
        QCOMPARE(bindingForThetisReadingName(QStringLiteral("swr")), MeterBinding::TxSwr);
        QCOMPARE(bindingForThetisReadingName(QStringLiteral("Signal_Strength")),
                 MeterBinding::SignalPeak);
        QCOMPARE(bindingForThetisReadingName(QStringLiteral("nothing")), -1);
        QCOMPARE(bindingForThetisReadingName(QString()), -1);
    }

    void namesRoundTrip()
    {
        const int bindings[] = {
            MeterBinding::SignalPeak, MeterBinding::SignalAvg, MeterBinding::AdcPeak,
            MeterBinding::AdcAvg, MeterBinding::AgcGain, MeterBinding::AgcPeak,
            MeterBinding::AgcAvg, MeterBinding::SignalMaxBin, MeterBinding::PbSnr,
            MeterBinding::TxPower, MeterBinding::TxReversePower, MeterBinding::TxSwr,
            MeterBinding::TxMic, MeterBinding::TxComp, MeterBinding::TxAlc,
            MeterBinding::TxEq, MeterBinding::TxLeveler, MeterBinding::TxLevelerGain,
            MeterBinding::TxAlcGain, MeterBinding::TxAlcGroup, MeterBinding::TxCfc,
            MeterBinding::TxCfcGain, MeterBinding::TxMicPeak, MeterBinding::TxAlcPeak,
            MeterBinding::TxCompPeak, MeterBinding::TxEqPeak, MeterBinding::TxLevelerPeak,
            MeterBinding::TxCfcPeak, MeterBinding::HwVolts, MeterBinding::HwAmps,
            MeterBinding::RotatorAz, MeterBinding::RotatorEle,
        };
        QSet<QString> seen;
        for (int binding : bindings) {
            const QString name = thetisReadingName(binding);
            QVERIFY2(!name.isEmpty(), qPrintable(QString::number(binding)));
            QCOMPARE(name, name.toLower());
            QVERIFY2(!seen.contains(name), qPrintable(name));
            seen.insert(name);
            QCOMPARE(bindingForThetisReadingName(name), binding);
        }
    }

    void baseItemPublishesNothing()
    {
        SpacerItem spacer;
        QVERIFY(spacer.publishedReadings().isEmpty());
        ClockItem clock;
        const MeterItem& base = clock;
        QCOMPARE(base.publishedReadings().size(), 4);
    }

    void clockTwelveHour()
    {
        ClockItem clock;
        clock.setShow24Hour(false);
        const QDateTime fixed(QDate(2026, 10, 25), QTime(13, 5, 9));
        clock.setTimeSource([fixed] { return fixed; });
        const QHash<QString, QVariant> r = clock.publishedReadings();
        QCOMPARE(r.value(QStringLiteral("time_loc")).toString(), QStringLiteral("1:05:09pm"));
        QCOMPARE(r.value(QStringLiteral("date_loc")).toString(), QStringLiteral("Sun 25 Oct 2026"));
        QCOMPARE(r.size(), 4);
    }

    void clockTwelveHourMorningAndMidnight()
    {
        ClockItem clock;
        clock.setShow24Hour(false);
        QDateTime t(QDate(2026, 10, 25), QTime(0, 7, 3));
        clock.setTimeSource([&t] { return t; });
        QCOMPARE(clock.publishedReadings().value(QStringLiteral("time_loc")).toString(),
                 QStringLiteral("12:07:03am"));
        t = QDateTime(QDate(2026, 10, 25), QTime(9, 0, 0));
        QCOMPARE(clock.publishedReadings().value(QStringLiteral("time_loc")).toString(),
                 QStringLiteral("9:00:00am"));
        t = QDateTime(QDate(2026, 10, 25), QTime(12, 30, 0));
        QCOMPARE(clock.publishedReadings().value(QStringLiteral("time_loc")).toString(),
                 QStringLiteral("12:30:00pm"));
    }

    void clockTwentyFourHour()
    {
        ClockItem clock;
        clock.setShow24Hour(true);
        const QDateTime fixed(QDate(2026, 10, 25), QTime(13, 5, 9));
        clock.setTimeSource([fixed] { return fixed; });
        QCOMPARE(clock.publishedReadings().value(QStringLiteral("time_loc")).toString(),
                 QStringLiteral("13:05:09"));
    }

    void clockUtcReadings()
    {
        ClockItem clock;
        clock.setShow24Hour(true);
        // A UTC time source makes the UTC readings deterministic.
        const QDateTime fixed(QDate(2026, 10, 25), QTime(23, 59, 1), QTimeZone::UTC);
        clock.setTimeSource([fixed] { return fixed; });
        const QHash<QString, QVariant> r = clock.publishedReadings();
        QCOMPARE(r.value(QStringLiteral("time_utc")).toString(), QStringLiteral("23:59:01"));
        QCOMPARE(r.value(QStringLiteral("date_utc")).toString(), QStringLiteral("Sun 25 Oct 2026"));
        clock.setShow24Hour(false);
        QCOMPARE(clock.publishedReadings().value(QStringLiteral("time_utc")).toString(),
                 QStringLiteral("11:59:01pm"));
    }
};

QTEST_MAIN(TestMeterPublishedReadings)
#include "tst_meter_published_readings.moc"
