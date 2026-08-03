// =================================================================
// tests/tst_notch_sideband_shift.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF: NotchModel::notchSidebandShift, ported from Thetis
// console.cs:40281-40307 notchSidebandShift(rx). Table-driven.
//
// The sign is the whole point. A dropped negation puts every LSB +TNF
// notch at VFO + 1500 instead of VFO - 1500: outside the passband,
// silent on LSB, and perfectly correct on USB, so it is invisible
// without this test.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.3.
// =================================================================

#include <QtTest/QtTest>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchSidebandShift : public QObject {
    Q_OBJECT

private slots:
    void shift_data()
    {
        QTest::addColumn<int>("filterLowHz");
        QTest::addColumn<int>("filterHighHz");
        QTest::addColumn<int>("expected");

        // Upper sideband: 200 + ((2800 - 200) / 2) = 200 + 1300 = 1500
        QTest::newRow("USB 200/2800")      << 200   << 2800  << 1500;
        // Lower sideband: -2800 + ((-200 - -2800) / 2) = -2800 + 1300 = -1500
        QTest::newRow("LSB -2800/-200")    << -2800 << -200  << -1500;
        // Symmetric filter (AM): middle == 0 hits the fallback at
        // console.cs:40294-40295, so highHz / 2 = 3000 / 2 = 1500
        QTest::newRow("AM -3000/+3000")    << -3000 << 3000  << 1500;
        // Narrow CW-width symmetric filter, same fallback: 250 / 2 = 125
        QTest::newRow("symmetric -250/250") << -250 << 250   << 125;
        // Narrow USB: 300 + ((1500 - 300) / 2) = 300 + 600 = 900
        QTest::newRow("USB 300/1500")      << 300   << 1500  << 900;
        // Narrow LSB: -1500 + ((-300 - -1500) / 2) = -1500 + 600 = -900
        QTest::newRow("LSB -1500/-300")    << -1500 << -300  << -900;
        // Wide DIGU: 0 + ((3000 - 0) / 2) = 1500
        QTest::newRow("DIGU 0/3000")       << 0     << 3000  << 1500;
        // C# integer division truncates toward zero; C++ matches.
        // 100 + ((2801 - 100) / 2) = 100 + 1350 = 1450
        QTest::newRow("odd span 100/2801") << 100   << 2801  << 1450;
    }

    void shift()
    {
        QFETCH(int, filterLowHz);
        QFETCH(int, filterHighHz);
        QFETCH(int, expected);
        QCOMPARE(NotchModel::notchSidebandShift(filterLowHz, filterHighHz),
                 expected);
    }

    void lsb_shift_is_negative()
    {
        // Guards the sign specifically, independent of the table above.
        QVERIFY(NotchModel::notchSidebandShift(-2800, -200) < 0);
        QVERIFY(NotchModel::notchSidebandShift(200, 2800) > 0);
    }

    void shift_is_callable_without_an_instance()
    {
        // Static by design: a global, slice-agnostic NotchModel cannot reach
        // per-slice filter edges, so the caller supplies them.
        QCOMPARE(NotchModel::notchSidebandShift(200, 2800), 1500);
    }
};

QTEST_MAIN(TestNotchSidebandShift)
#include "tst_notch_sideband_shift.moc"
