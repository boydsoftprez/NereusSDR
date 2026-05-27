// =================================================================
// tests/tst_p2_radio_connection_apply_ddc_assignment.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Task 15: verify P2RadioConnection accepts a
// codec-emitted DdcAssignment and writes the corresponding per-DDC
// state into the CmdRx packet.
//
// Design: docs/architecture/2026-05-26-phase3f-sub-epic-b-codec-chain-plan.md
//         Task 15.
// Source: NereusSDR-original (no Thetis upstream).
// =================================================================

#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"
#include "core/DdcAssignment.h"

using namespace NereusSDR;

class TestP2RadioConnectionApplyDdcAssignment : public QObject {
    Q_OBJECT
private slots:
    void apply_ddc_assignment_populates_rx_state()
    {
        P2RadioConnection conn(nullptr);

        DdcAssignment a{};
        a.rate[2]    = 192000;  // DDC2 at 192 kHz
        a.rate[3]    = 96000;   // DDC3 at 96 kHz
        a.ddcEnable  = 0x0c;    // bits 2 and 3 set: DDC2 + DDC3 enabled
        // adcCtrl1: bits 4-5 for DDC2 = 00 (ADC0), bits 6-7 for DDC3 = 01 (ADC1)
        // value = 0b_01_00_00_00 = 0x40
        a.adcCtrl1   = 0x40;
        a.syncEnable = 0x04;    // DDC2 syncs to DDC0
        a.nDdc       = 2;

        conn.applyDdcAssignment(a);

        // Snapshot CmdRx via the test-seam accessor.
        static constexpr int kBufLen = 1444;
        quint8 buf[kBufLen] = {};
        conn.composeCmdRxForTest(buf);

        // Sanity check: at least one byte in the per-DDC region (bytes 50-200)
        // must be non-zero after the assignment was applied.
        bool anyNonzero = false;
        for (int i = 50; i < 200; ++i) {
            if (buf[i] != 0) {
                anyNonzero = true;
                break;
            }
        }
        QVERIFY2(anyNonzero, "Expected at least one non-zero byte in per-DDC CmdRx region after applyDdcAssignment");
    }
};

QTEST_MAIN(TestP2RadioConnectionApplyDdcAssignment)
#include "tst_p2_radio_connection_apply_ddc_assignment.moc"
