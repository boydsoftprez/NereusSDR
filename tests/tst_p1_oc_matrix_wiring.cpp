// no-port-check: smoke test for P1RadioConnection ↔ OcMatrix wiring.
// Verifies that ctx.ocByte is sourced from OcMatrix::maskFor(currentBand, mox)
// when an OcMatrix is wired, and falls through to legacy m_ocOutput=0 otherwise.
// Phase 3P-D Task 3.

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/OcMatrix.h"
#include "models/Band.h"

using namespace NereusSDR;

class TestP1OcMatrixWiring : public QObject {
    Q_OBJECT

private slots:
    // Without an OcMatrix wired, P1 falls through to legacy m_ocOutput=0.
    // Bank 0 C2 = (m_ocOutput << 1) & 0xFE — with m_ocOutput=0, byte 2 = 0.
    void no_matrix_falls_through_to_zero()
    {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setReceiverFrequency(0, 14'100'000ULL);  // 20m
        conn.setMox(false);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // Bank 0 C2: (m_ocOutput << 1) & 0xFE — with m_ocOutput=0 → 0x00
        QCOMPARE(int(buf[2]), 0);
    }

    // With an OcMatrix that has pin 0 set for 20m RX, the OC byte is 0x01
    // → C2 = (0x01 << 1) & 0xFE = 0x02.
    void matrix_routes_pin_to_oc_byte()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 14'100'000ULL);  // 20m
        conn.setMox(false);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // maskFor(Band20m, false) = 0x01 (pin 0)
        // Bank 0 C2: (0x01 << 1) & 0xFE = 0x02
        QCOMPARE(int(buf[2]), 0x02);
    }

    // MOX state selects TX matrix path: pin 1 set for 40m TX → mask = 0x02
    // → C2 = (0x02 << 1) & 0xFE = 0x04.
    void mox_uses_tx_matrix()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band40m, /*pin=*/1, /*tx=*/true, true);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 7'100'000ULL);  // 40m
        conn.setMox(true);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // maskFor(Band40m, true) = 0x02 (pin 1)
        // Bank 0 C2: (0x02 << 1) & 0xFE = 0x04
        QCOMPARE(int(buf[2]), 0x04);
    }

    // Detaching the matrix (setOcMatrix(nullptr)) falls back to legacy 0.
    void matrix_detach_falls_back_to_zero()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band20m, 0, false, true);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setReceiverFrequency(0, 14'100'000ULL);  // 20m
        conn.setMox(false);

        // First with matrix — should be 0x02
        conn.setOcMatrix(&matrix);
        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);
        QCOMPARE(int(buf[2]), 0x02);

        // Now detach — should fall back to 0
        conn.setOcMatrix(nullptr);
        memset(buf, 0, sizeof(buf));
        conn.composeCcForBankForTest(0, buf);
        QCOMPARE(int(buf[2]), 0x00);
    }

    // ── HL2 N2ADR bypass routing (2026-08-01) ──────────────────────────────
    //
    // The HL2 has no Alex board (hasAlexFilters=false): its RX preselector
    // is the N2ADR board driven through these same OC pins, so
    // AlexController's per-ADC BYPASS / WidebandLocked decision has nowhere
    // else to reach the wire: it otherwise lands only in ctx.alexHpfBits,
    // which P1CodecHl2 never reads. P1RadioConnection watches
    // m_alexRxHpfOverride (set by setAlexRxBpf) for the 0x20 bypass
    // sentinel documented on AlexRxBpf in RadioConnection.h, and forces
    // ctx.ocByte to 0x00 when it sees it, scoped to hasIoBoardHl2 so
    // Alex-equipped boards (which already carry this decision on bank 10)
    // are untouched.
    //
    // maintainer-supplied hardware knowledge (J.J. Boyd / KG4VCF,
    // 2026-08-01): driving every OC pin low (ocByte == 0x00) disables /
    // bypasses the N2ADR board. Not documented in any upstream source;
    // mi0bot never commands 0x00.

    void hl2_bypass_override_forces_ocbyte_to_zero()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);  // would be 0x01

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 14'200'000ULL);  // 20m
        conn.setMox(false);
        conn.setAlexRxBpf(AlexRxBpf{.hpfBitsAdc0 = 0x20});  // AlexController: Bypass

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // Forced to 0x00 regardless of what maskFor(Band20m, false) held.
        QCOMPARE(int(buf[2]), 0x00);
    }

    void hl2_non_bypass_override_keeps_band_mask()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);  // mask 0x01

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 14'200'000ULL);  // 20m
        conn.setMox(false);
        // A real Filtered decision: any value other than the 0x20 sentinel.
        conn.setAlexRxBpf(AlexRxBpf{.hpfBitsAdc0 = 0x01});

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // maskFor(Band20m, false) = 0x01 -> C2 = (0x01 << 1) & 0xFE = 0x02
        QCOMPARE(int(buf[2]), 0x02);
    }

    void hl2_no_override_keeps_band_mask()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);  // mask 0x01

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 14'200'000ULL);  // 20m
        conn.setMox(false);
        // No setAlexRxBpf() call at all: m_alexRxHpfOverride stays at its
        // -1 "no decision yet" default (e.g. before the first republish).

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        QCOMPARE(int(buf[2]), 0x02);
    }

    void non_hl2_board_ignores_bypass_override()
    {
        OcMatrix matrix;
        matrix.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);  // mask 0x01

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);  // has Alex; not the OC-driven path
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 14'200'000ULL);
        conn.setMox(false);
        conn.setAlexRxBpf(AlexRxBpf{.hpfBitsAdc0 = 0x20});  // bypass decision present

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // hasIoBoardHl2 is false for Hermes: the HL2 override must not fire,
        // even though the same bypass sentinel arrived. Alex boards already
        // carry this decision on their own bank-10 word.
        QCOMPARE(int(buf[2]), 0x02);
    }

    void hl2_bypass_override_does_not_apply_during_mox()
    {
        OcMatrix matrix;
        // 40m TX pin, matching N2adrPreset.cpp's real mapping (pin 3 = bit 2).
        matrix.setPin(Band::Band40m, /*pin=*/2, /*tx=*/true, true);  // mask 0x04

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setOcMatrix(&matrix);
        conn.setReceiverFrequency(0, 7'150'000ULL);  // 40m
        conn.setMox(true);
        conn.setAlexRxBpf(AlexRxBpf{.hpfBitsAdc0 = 0x20});  // some OTHER RX slice conflicts

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // The N2ADR pins double as the TX low-pass bank. A receive-side
        // bypass decision about some other slice must not strip TX
        // filtering for the band actually being transmitted.
        // maskFor(Band40m, true) = 0x04 -> C2 = (0x04 << 1) & 0xFE = 0x08
        QCOMPARE(int(buf[2]), 0x08);
    }
};

QTEST_APPLESS_MAIN(TestP1OcMatrixWiring)
#include "tst_p1_oc_matrix_wiring.moc"
