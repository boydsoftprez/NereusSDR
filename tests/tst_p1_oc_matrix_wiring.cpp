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
};

QTEST_APPLESS_MAIN(TestP1OcMatrixWiring)
#include "tst_p1_oc_matrix_wiring.moc"
