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

    // A PS edge changes DDC0/DDC1, but it is still a complete assignment.
    // Every unrelated DDC must retain its enable, rate, and ADC selection.
    //
    // Mutation caught: replacing the second applyDdcAssignment with the
    // legacy four-rate PsDdcConfig writer drops or rewrites DDC4-6.
    void full_seven_ddc_state_survives_ps_transition()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);

        DdcAssignment before{};
        before.ddcEnable = 0x7f;
        before.syncEnable = 0;
        before.adcCtrl1 = 0xe4; // DDC0..3 -> ADC0, ADC1, ADC2, ADC3
        before.adcCtrl2 = 0x24; // DDC4..6 -> ADC0, ADC1, ADC2
        before.nDdc = 7;
        const std::array<int, 7> rateHz{
            48000, 96000, 192000, 384000, 768000, 1536000, 3072000
        };
        for (int ddc = 0; ddc < 7; ++ddc) {
            before.rate[ddc] = rateHz[ddc];
        }
        conn.applyDdcAssignment(before);

        DdcAssignment ps = before;
        ps.syncEnable = 0x02;
        ps.rate[0] = 192000;
        ps.rate[1] = 192000;
        ps.adcCtrl1 = (ps.adcCtrl1 & 0xf3) | 0x08;
        ps.psFwdDdc = 0;
        ps.psRevDdc = 1;
        conn.applyDdcAssignment(ps);

        quint8 buf[1444] = {};
        conn.composeCmdRxForTest(buf);
        QCOMPARE(buf[7], quint8{0x7f});
        QCOMPARE(buf[1363], quint8{0x02});

        for (int ddc = 0; ddc < 7; ++ddc) {
            const int base = 17 + (ddc * 6);
            const int actualRateKhz =
                (static_cast<int>(buf[base + 1]) << 8)
                | static_cast<int>(buf[base + 2]);
            const int expectedRateKhz =
                ((ddc < 2) ? 192000 : rateHz[ddc]) / 1000;
            QCOMPARE(actualRateKhz, expectedRateKhz);

            const int expectedAdc =
                ddc < 4 ? ((ps.adcCtrl1 >> (ddc * 2)) & 0x3)
                        : ((ps.adcCtrl2 >> ((ddc - 4) * 2)) & 0x3);
            QCOMPARE(static_cast<int>(buf[base]), expectedAdc);
        }
    }
};

QTEST_MAIN(TestP2RadioConnectionApplyDdcAssignment)
#include "tst_p2_radio_connection_apply_ddc_assignment.moc"
