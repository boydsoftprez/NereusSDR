// no-port-check: NereusSDR-original. No upstream port.
#include <QtTest/QtTest>
#include "core/PaTempUnit.h"
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
        // Exact match via PaTempUnitNotifier::format(), like
        // bothReadingsShareRowOne below: a literal "42.5" .contains()
        // check assumes Celsius is the ambient persisted unit and
        // silently stops meaning what it says if that unit is ever
        // Fahrenheit (42.5C formats as "108.5F"), instead of proving
        // refreshPaRow()'s temp-only branch (SystemTile.cpp) produces the
        // unprefixed PaTempUnitNotifier::format(m_celsius) it is supposed
        // to (final-fix-wave finding 9).
        QCOMPARE(t.paRowText(), PaTempUnitNotifier::format(42.5));
    }

    void bothReadingsShareRowOne() {
        SystemTile t;
        t.setPaVolts(13.8);
        t.setPaTempCelsius(42.5);
        QVERIFY(t.hasPaRow());
        // Composed-string comparison, not two independent .contains() calls:
        // a regression that dropped the separator ("13.8V42.5C") would still
        // satisfy two independent .contains() checks but must fail this one.
        const QString expected =
            QStringLiteral("13.8V ") + PaTempUnitNotifier::format(42.5);
        QCOMPARE(t.paRowText(), expected);
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

    void clearingBothResetsCursorToArrow() {
        SystemTile t;
        t.setPaTempCelsius(42.5);
        QCOMPARE(t.cursor().shape(), Qt::PointingHandCursor);
        t.clearPaVolts();
        t.clearPaTemp();
        QCOMPARE(t.cursor().shape(), Qt::ArrowCursor);
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
