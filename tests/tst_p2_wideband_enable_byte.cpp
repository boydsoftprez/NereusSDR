// =================================================================
// tests/tst_p2_wideband_enable_byte.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic F Task 1: P2 composeCmdGeneral writes packetbuf[23]
// wideband per-ADC enable mask per Thetis network.c:879 [v2.10.3.15].
//
// Source-first correction note: the original Sub-Epic F plan targeted
// composeCmdRx byte 23. That was wrong: CmdRx byte 23 is rx[1].rx_adc
// (RX1 ADC selector) per Thetis network.c:1118, not the wideband enable
// mask. The mask lives in CmdGeneral byte 23 per Thetis network.c:879.
// See plan revision note at
// docs/architecture/2026-05-26-phase3f-sub-epic-f-wideband-plan.md
// (Task 1) for full rationale.
// =================================================================
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2WidebandEnableByte : public QObject {
    Q_OBJECT
private slots:
    void compose_cmd_general_writes_packetbuf_23_when_wideband_enabled()
    {
        P2RadioConnection conn;
        conn.setWidebandEnabled(0, true);  // enable ADC0 wideband

        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);

        // packetbuf[23] should have bit 0 set (ADC0 enabled).
        QCOMPARE(quint8(buf[23] & 0x01), quint8(0x01));
    }

    void compose_cmd_general_writes_0_when_no_wideband()
    {
        P2RadioConnection conn;
        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);
        QCOMPARE(quint8(buf[23]), quint8(0x00));
    }

    void per_adc_enable_independent()
    {
        P2RadioConnection conn;
        conn.setWidebandEnabled(0, true);
        conn.setWidebandEnabled(1, true);
        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);
        QCOMPARE(quint8(buf[23] & 0x03), quint8(0x03));  // both bits set
    }
};

QTEST_MAIN(TestP2WidebandEnableByte)
#include "tst_p2_wideband_enable_byte.moc"
