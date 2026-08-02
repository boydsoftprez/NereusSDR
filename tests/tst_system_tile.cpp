// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include "gui/widgets/SystemTile.h"

using namespace NereusSDR;

class TstSystemTile : public QObject {
    Q_OBJECT
private slots:
    void cpuOnlyWhenBoardHasNoPaTelemetry() {
        SystemTile t;
        t.setCpuPercent(19.0);
        QVERIFY(!t.hasPaRow());
        QCOMPARE(t.cpuRowText(), QStringLiteral("19%"));
    }

    void voltsOnlyOnMkiiClass() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setCpuPercent(19.0);
        QVERIFY(t.hasPaRow());
        QCOMPARE(t.paRowText(), QStringLiteral("13.8V"));
    }

    void tempOnlyOnHl2() {
        SystemTile t;
        t.setPaTempCelsius(42.5);
        t.setCpuPercent(19.0);
        QVERIFY(t.hasPaRow());
        QVERIFY(t.paRowText().contains(QStringLiteral("42.5")));
    }

    void bothReadingsShareRowOne() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        QVERIFY(t.hasPaRow());
        QVERIFY2(t.paRowText().contains(QStringLiteral("13.8V")),
                 qPrintable(t.paRowText()));
        QVERIFY2(t.paRowText().contains(QStringLiteral("42.5")),
                 qPrintable(t.paRowText()));
    }

    void cpuSurvivesWhenBothPaReadingsArrive() {
        SystemTile t;
        t.setCpuPercent(19.0);
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        QCOMPARE(t.cpuRowText(), QStringLiteral("19%"));
    }

    void clearingBothHidesRowOne() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        t.clearPaVolts();
        t.clearPaTemp();
        QVERIFY(!t.hasPaRow());
    }

    void paLabelIsSettableForG2ePsu() {
        SystemTile t;
        t.setPaLabel(QStringLiteral("PSU"));
        t.setPaVolts(13.4);
        QCOMPARE(t.paLabel(), QStringLiteral("PSU"));
    }
};
QTEST_MAIN(TstSystemTile)
#include "tst_system_tile.moc"
