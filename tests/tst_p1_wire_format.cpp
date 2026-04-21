// no-port-check: test-only — Thetis file names (networkproto1.c) appear only
// in source-cite comments that document which upstream line each assertion
// verifies. No Thetis logic is ported here; this file is NereusSDR-original.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"
#include <vector>

using namespace NereusSDR;

class TestP1WireFormat : public QObject {
    Q_OBJECT
private slots:
    void ep2FrameMagicHeader() {
        quint8 frame[1032] = {};
        P1RadioConnection::composeEp2Frame(frame, /*seq=*/0, /*ccAddress=*/0,
                                           /*sampleRate=*/48000, /*mox=*/false);
        // Source: networkproto1.c:223-226 — MetisWriteFrame() magic 0xEFFE 0x01 0x02
        QCOMPARE(frame[0], quint8(0xEF));
        QCOMPARE(frame[1], quint8(0xFE));
        QCOMPARE(frame[2], quint8(0x01));
        QCOMPARE(frame[3], quint8(0x02));
    }

    void ep2FrameSequenceBigEndian32() {
        quint8 frame[1032] = {};
        P1RadioConnection::composeEp2Frame(frame, /*seq=*/0x12345678, 0, 48000, false);
        // Source: networkproto1.c:227-230 — MetisWriteFrame() sequence big-endian
        QCOMPARE(frame[4], quint8(0x12));
        QCOMPARE(frame[5], quint8(0x34));
        QCOMPARE(frame[6], quint8(0x56));
        QCOMPARE(frame[7], quint8(0x78));
    }

    void ep2FrameSyncBytesAtUsbSubframes() {
        quint8 frame[1032] = {};
        P1RadioConnection::composeEp2Frame(frame, 0, 0, 48000, false);
        // Source: networkproto1.c:600-602, 881-883 — WriteMainLoop() sync bytes
        QCOMPARE(frame[8],  quint8(0x7F));
        QCOMPARE(frame[9],  quint8(0x7F));
        QCOMPARE(frame[10], quint8(0x7F));
        // USB subframe 1 at offset 520: 0x7F 0x7F 0x7F
        QCOMPARE(frame[520], quint8(0x7F));
        QCOMPARE(frame[521], quint8(0x7F));
        QCOMPARE(frame[522], quint8(0x7F));
    }

    void ccBank0EncodesSampleRate48k() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, /*sampleRate=*/48000, /*mox=*/false);
        // Source: networkproto1.c:620 — C1 = (SampleRateIn2Bits & 3): 48k=00, 96k=01, 192k=10, 384k=11
        QCOMPARE(int(out[1] & 0x03), 0);
    }

    void ccBank0EncodesSampleRate96k() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, 96000, false);
        // Source: networkproto1.c:620 — SampleRateIn2Bits for 96k = 1
        QCOMPARE(int(out[1] & 0x03), 1);
    }

    void ccBank0EncodesSampleRate192k() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, 192000, false);
        // Source: networkproto1.c:620 — SampleRateIn2Bits for 192k = 2
        QCOMPARE(int(out[1] & 0x03), 2);
    }

    void ccBank0EncodesSampleRate384k() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, 384000, false);
        // Source: networkproto1.c:620 — SampleRateIn2Bits for 384k = 3
        QCOMPARE(int(out[1] & 0x03), 3);
    }

    void ccBank0SetsMoxBit() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, 48000, /*mox=*/true);
        // Source: networkproto1.c:615-616 — C0 = (unsigned char)XmitBit; MOX is bit 0 of C0
        QCOMPARE(int(out[0] & 0x01), 1);
    }

    void ccBank0C0AddressIs0x00() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, 48000, false);
        // Source: networkproto1.c:619 — case 0: no C0 |= address, so C0 bits 7..1 = 0
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0);
    }

    void rxFrequencyEncodedBigEndian32() {
        quint8 out[5] = {};
        quint64 freqHz = 14200000ULL;
        P1RadioConnection::composeCcBankRxFreq(out, /*rxIndex=*/0, freqHz);
        // Source: networkproto1.c:660-663 — C1..C4 = frequency bytes, big-endian
        quint32 readBack = (quint32(out[1]) << 24) | (quint32(out[2]) << 16)
                         | (quint32(out[3]) <<  8) |  quint32(out[4]);
        QCOMPARE(readBack, 14200000U);
    }

    void rxFrequencyC0AddressForRx0() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 0, 14200000);
        // Source: networkproto1.c:653-654 — case 2: C0 |= 4; RX1 DDC0 → address bits = 4/2 = 0x02
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x02);
    }

    void rxFrequencyC0AddressForRx1() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 1, 14200000);
        // Source: networkproto1.c:498 — case 3: C0 |= 6 → address = 6>>1 = 0x03
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x03);
    }

    void rxFrequencyC0AddressForRx2() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 2, 14200000);
        // Source: networkproto1.c:526 — case 5: C0 |= 0x08 → address = 0x08>>1 = 0x04
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x04);
    }

    void rxFrequencyC0AddressForRx3() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 3, 14200000);
        // Source: networkproto1.c:539 — case 6: C0 |= 0x0A → address = 0x0A>>1 = 0x05
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x05);
    }

    void rxFrequencyC0AddressForRx4() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 4, 14200000);
        // Source: networkproto1.c:549 — case 7: C0 |= 0x0C → address = 0x0C>>1 = 0x06
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x06);
    }

    void rxFrequencyC0AddressForRx5() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 5, 14200000);
        // Source: networkproto1.c:~560 — case 8: C0 |= 0x0E → address = 0x0E>>1 = 0x07
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x07);
    }

    void rxFrequencyC0AddressForRx6() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 6, 14200000);
        // Source: networkproto1.c:569 — case 9: C0 |= 0x10 → address = 0x10>>1 = 0x08
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x08);
    }

    void rxFrequencyAddressesAreAllDistinct() {
        // Bug guard: the 7 DDC NCO banks must each emit a different C0 byte,
        // else the round-robin collides two banks onto one bus slot.
        quint8 bytes[7] = {};
        for (int i = 0; i < 7; ++i) {
            quint8 out[5] = {};
            P1RadioConnection::composeCcBankRxFreq(out, i, 14200000);
            bytes[i] = out[0];
        }
        for (int a = 0; a < 7; ++a) {
            for (int b = a + 1; b < 7; ++b) {
                QVERIFY2(bytes[a] != bytes[b],
                    qPrintable(QString("kRxC0Address[%1]==kRxC0Address[%2]==0x%3")
                        .arg(a).arg(b).arg(bytes[a], 2, 16, QChar('0'))));
            }
        }
    }

    void txFrequencyEncodedBigEndian32() {
        quint8 out[5] = {};
        quint64 freqHz = 7100000ULL;
        P1RadioConnection::composeCcBankTxFreq(out, freqHz);
        // Source: networkproto1.c:647-650 — case 1: C0 |= 2; C1..C4 = TX freq big-endian
        quint32 readBack = (quint32(out[1]) << 24) | (quint32(out[2]) << 16)
                         | (quint32(out[3]) <<  8) |  quint32(out[4]);
        QCOMPARE(readBack, 7100000U);
    }

    void txFrequencyC0AddressIs0x01() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankTxFreq(out, 7100000);
        // Source: networkproto1.c:645-646 — case 1: C0 |= 2 → address = 2>>1 = 0x01
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x01);
    }

    void attenuatorEncodedInBank() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankAtten(out, /*dB=*/20);
        // Source: networkproto1.c:770 — case 11 (C0 |= 0x14): C4 = adc[0].rx_step_attn & 0b00011111 | 0b00100000
        // Low 5 bits of C4 hold the dB value; bit 5 is the "enable attenuation" flag
        QCOMPARE(int(out[4] & 0x1F), 20);
    }

    void attenuatorC0AddressIs0x0A() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankAtten(out, 0);
        // Source: networkproto1.c:763 — case 11: C0 |= 0x14 → address = 0x14>>1 = 0x0A
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x0A);
    }

    // --- Sample scaling ---
    // Source: networkproto1.c:367-374 — sign-extend 24-bit BE, scale by 1/2^23

    void scaleSample24PositiveHalfScale() {
        quint8 be24[3] = {0x40, 0x00, 0x00};  // +0x400000 = 4194304
        float f = P1RadioConnection::scaleSample24(be24);
        // 24-bit full scale = 2^23 = 8388608; 4194304 / 8388608 = 0.5
        QVERIFY(qAbs(f - 0.5f) < 0.0001f);
    }

    void scaleSample24Zero() {
        quint8 be24[3] = {0x00, 0x00, 0x00};
        float f = P1RadioConnection::scaleSample24(be24);
        QCOMPARE(f, 0.0f);
    }

    void scaleSample24NegativeHalfScale() {
        quint8 be24[3] = {0xC0, 0x00, 0x00};  // -0x400000 after sign-extend
        float f = P1RadioConnection::scaleSample24(be24);
        QVERIFY(qAbs(f - (-0.5f)) < 0.0001f);
    }

    void scaleSample24NegativeOne() {
        quint8 be24[3] = {0x80, 0x00, 0x00};  // -0x800000
        float f = P1RadioConnection::scaleSample24(be24);
        QVERIFY(qAbs(f - (-1.0f)) < 0.0001f);
    }

    // --- ep6 frame parse (1 RX) ---
    // Source: networkproto1.c:319-415 MetisReadThreadMainLoop

    void parseEp6Frame1RxEmitsCorrectSampleCount() {
        quint8 frame[1032] = {};
        // Metis ep6 header: EF FE 01 06 + 4-byte sequence
        frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x06;
        // Subframe 0 at offset 8: sync 7F 7F 7F + 5-byte C&C (zeros for test)
        frame[8] = 0x7F; frame[9] = 0x7F; frame[10] = 0x7F;
        // Subframe 1 at offset 520
        frame[520] = 0x7F; frame[521] = 0x7F; frame[522] = 0x7F;
        // Leave samples as zero — parser should emit them

        std::vector<std::vector<float>> perRx;
        const bool ok = P1RadioConnection::parseEp6Frame(frame, /*numRx=*/1, perRx);
        QVERIFY(ok);
        QCOMPARE(perRx.size(), size_t(1));
        // 1 RX: slot size = 6 (IQ) + 2 (mic) = 8 bytes. 504 / 8 = 63 slots per subframe.
        // Source: networkproto1.c:361 — spr = 504 / (6 * nddc + 2)
        // 2 subframes × 63 slots × 2 floats (I,Q) = 252 floats per RX.
        QCOMPARE(perRx[0].size(), size_t(252));
        // All samples are zero since frame body is zero
        for (float v : perRx[0]) { QCOMPARE(v, 0.0f); }
    }

    void parseEp6FrameRejectsWrongMagic() {
        quint8 frame[1032] = {};
        frame[0] = 0xDE; frame[1] = 0xAD;  // not EF FE
        std::vector<std::vector<float>> perRx;
        QVERIFY(!P1RadioConnection::parseEp6Frame(frame, 1, perRx));
    }

    void parseEp6FrameRejectsMissingSync() {
        quint8 frame[1032] = {};
        frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x06;
        // No sync bytes at offset 8 — subframe 0 sync check should fail
        std::vector<std::vector<float>> perRx;
        QVERIFY(!P1RadioConnection::parseEp6Frame(frame, 1, perRx));
    }

    void parseEp6Frame1RxWithKnownSample() {
        quint8 frame[1032] = {};
        frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x06;
        frame[8] = 0x7F; frame[9] = 0x7F; frame[10] = 0x7F;
        frame[520] = 0x7F; frame[521] = 0x7F; frame[522] = 0x7F;
        // First sample slot starts at offset 16 (frame+8 subframe base + 8 for sync+C&C).
        // Source: networkproto1.c:366 — k = 8 + isample*slotBytes + iddc*6 (relative to bptr)
        // Place I = +0.5, Q = -0.5 in first slot
        frame[16] = 0x40; frame[17] = 0x00; frame[18] = 0x00;  // I = +half scale
        frame[19] = 0xC0; frame[20] = 0x00; frame[21] = 0x00;  // Q = -half scale
        // frame[22..23] is mic16 (leave zero)

        std::vector<std::vector<float>> perRx;
        QVERIFY(P1RadioConnection::parseEp6Frame(frame, 1, perRx));
        QVERIFY(qAbs(perRx[0][0] - 0.5f) < 0.0001f);   // I
        QVERIFY(qAbs(perRx[0][1] - (-0.5f)) < 0.0001f); // Q
    }

    // --- Per-board quirks (Task 11) ---

    void hermesLiteAttenClampsTo63() {
        // HL2 range is 0..63 per BoardCapabilities.cpp kHermesLite (6-bit, Task 11).
        // Source: mi0bot WriteMainLoop_HL2 [@c26a8a4] — 6-bit range (0x3F mask).
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setAttenuator(80);  // above max
        QCOMPARE(conn.currentAttenForTest(), 63);
        conn.setAttenuator(-5);  // below min
        QCOMPARE(conn.currentAttenForTest(), 0);
    }

    void hermesAttenClampsTo31() {
        // Hermes range is 0..31 per BoardCapabilities.cpp kHermes.
        // Source: specHPSDR.cs per-HPSDRHW atten limits.
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setAttenuator(80);  // above max
        QCOMPARE(conn.currentAttenForTest(), 31);
    }

    // --- HL2-specific helpers (Task 12) ---
    // Task 12 hardware validation is covered by the hardware smoke checklist
    // (§7.3 of the Phase 3I plan). Unit tests here verify only the lightweight
    // state invariants that are observable without a live HL2.

    void hl2ThrottledStartsFalse() {
        // m_hl2Throttled must be false at construction — no spurious throttle
        // flag before the watchdog has ever fired.
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(!conn.hl2ThrottledForTest());
    }

    void hl2IoBoardInitNoopForNonHl2() {
        // hl2SendIoBoardInit() is only meaningful when hasIoBoardHl2 is set.
        // For a non-HL2 board (e.g. Hermes), setBoardForTest must not
        // accidentally set the throttle flag.
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::Hermes);
        QVERIFY(!conn.hl2ThrottledForTest());
    }

    // Phase 3P-A Task 13: setReceiverFrequency must update Alex HPF bits.
    // Source: console.cs:6830-6942 [@501e3f5]
    void setReceiverFrequency_updates_alex_hpf_bits() {
        P1RadioConnection conn(nullptr);
        conn.init();
        conn.setBoardForTest(HPSDRHW::Hermes);  // hasAlexFilters = true
        conn.setReceiverFrequency(0, 14'100'000ULL);  // 20m → 13 MHz HPF (0x01)
        quint8 out[5] = {};
        conn.composeCcForBankForTest(10, out);  // bank 10 carries Alex bits
        // C3 low 7 bits = HPF select
        QCOMPARE(int(out[3] & 0x7F), 0x01);
    }

    // Phase 3P-A Task 13: setTxFrequency must update Alex LPF bits.
    // Source: console.cs:7168-7234 [@501e3f5]
    void setTxFrequency_updates_alex_lpf_bits() {
        P1RadioConnection conn(nullptr);
        conn.init();
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setTxFrequency(14'100'000ULL);  // 30/20m LPF (0x01)
        quint8 out[5] = {};
        conn.composeCcForBankForTest(10, out);
        // C4 = LPF select
        QCOMPARE(int(out[4]), 0x01);
    }
};

QTEST_APPLESS_MAIN(TestP1WireFormat)
#include "tst_p1_wire_format.moc"
