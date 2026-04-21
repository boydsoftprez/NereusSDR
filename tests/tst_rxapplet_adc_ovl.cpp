// no-port-check: test fixture asserts UI behavior, no Thetis port
//
// Phase 3P-B Task 10: per-ADC OVL split + RX1 preamp wire test.
//
// Part A — OVL badge count:
//   Single-ADC boards (HL2, Hermes) → 1 badge labeled "OVL".
//   Dual-ADC boards (OrionMKII)     → 2 badges labeled "OVL₀" / "OVL₁".
//   Gate: BoardCapabilities::p2PreampPerAdc (added Task 6).
//
// Part B — RX1 preamp byte 1403 bit 1:
//   P2RadioConnection::setRx1Preamp(true)  → buf[1403] & 0x02 != 0.
//   P2RadioConnection::setRx1Preamp(false) → buf[1403] & 0x02 == 0.
//   Default (m_rx[1].preamp=0) must leave bit 1 clear — regression-freeze
//   gate from Task 7 must still PASS.

#include <QtTest/QtTest>
#include <QApplication>

#include "core/P2RadioConnection.h"
#include "core/HpsdrModel.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestRxAppletAdcOvl : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    // ── Part A: OVL badge count per board family ──────────────────────────

    // HL2 is single-ADC → one OVL badge.
    void hl2_single_badge()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.visibleOvlBadgeCountForTest(), 1);
    }

    // Hermes is single-ADC → one OVL badge.
    void hermes_single_badge()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.visibleOvlBadgeCountForTest(), 1);
    }

    // OrionMKII has p2PreampPerAdc=true → two OVL badges.
    void orionmkii_dual_badges()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.visibleOvlBadgeCountForTest(), 2);
    }

    // ── Part B: RX1 preamp wires to byte 1403 bit 1 ──────────────────────

    // setRx1Preamp(true) → byte 1403 bit 1 set.
    void orionmkii_setRx1Preamp_true_writes_byte_1403_bit1()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setRx1Preamp(true);

        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QVERIFY2((buf[1403] & 0x02) != 0,
                 "byte 1403 bit 1 must be set when RX1 preamp is enabled");
    }

    // setRx1Preamp(false) → byte 1403 bit 1 clear.
    void orionmkii_setRx1Preamp_false_clears_byte_1403_bit1()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setRx1Preamp(false);

        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[1403]) & 0x02, 0);
    }

    // Default state (m_rx[1].preamp=0) must leave bit 1 clear.
    // This confirms the regression-freeze baseline is preserved — no drift
    // from the Task 7 byte-for-byte freeze.
    void default_rx1Preamp_is_false_byte_1403_bit1_clear()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        // No setRx1Preamp() call — default state.

        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[1403]) & 0x02, 0);
    }
};

QTEST_MAIN(TestRxAppletAdcOvl)
#include "tst_rxapplet_adc_ovl.moc"
