// no-port-check: test fixture cites deskhpsdr/Thetis source for expected
// values only; no Thetis logic is ported here.  NereusSDR-original test.
//
// Wire-byte snapshot tests for P2RadioConnection::sendTxIq() (3M-1a Task E.6).
//
// P2 TX I/Q frame layout (1444 bytes total):
//   [0..3]      4-byte BE sequence number
//   [4..1443]   1440-byte payload: 240 samples × 6 bytes each
//                 per sample: 3-byte BE signed int24 I, 3-byte BE signed int24 Q
//
// Float→int24 gain: 8388607.0 (0x7FFFFF — no rounding offset; deskhpsdr
// passes pre-rounded integer pairs from WDSP; we clamp to ±8388607).
//
// Cite: deskhpsdr/src/new_protocol.c:2795-2837 [@120188f]
//       deskhpsdr/src/new_protocol.h:37 [@120188f]  (#define TX_IQ_FROM_HOST_PORT 1029)
//       deskhpsdr/src/new_protocol.c:1476,1596 [@120188f] (24-bit width assertions)
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2TxIqWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Frame size ─────────────────────────────────────────────────────────
    // Every call to txIqFrameForTest must return exactly 1444 bytes.
    // Cite: deskhpsdr/src/new_protocol.c:1929 [@120188f]
    //   "a TX IQ buffer with 240 sample is sent every 1250 usecs"
    //   Frame = 4-byte seq + 240×6 = 1444.
    void frameSize_is1444() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);
        QCOMPARE(frame.size(), 1444);
    }

    // ── 2. Payload size ───────────────────────────────────────────────────────
    // 240 samples × 6 bytes = 1440-byte payload after the 4-byte sequence.
    // Cite: deskhpsdr/src/new_protocol.c:1820 [@120188f]
    //   if (txiq_count >= 240) { ... nptr = txiq_inptr + 1440; }
    void payloadSize_is1440() {
        P2RadioConnection conn;
        QByteArray frame = conn.txIqFrameForTest(nullptr, 0);
        QCOMPARE(frame.size() - 4, 1440);
    }

    // ── 3. Sequence number starts at zero, increments per frame ──────────────
    // Cite: deskhpsdr/src/new_protocol.c:1945-1948 [@120188f]
    //   iqbuffer[0] = (tx_iq_sequence >> 24) & 0xFF; … tx_iq_sequence++;
    void sequenceNumber_startsAtZeroAndIncrements() {
        P2RadioConnection conn;

        auto readSeq = [](const QByteArray& f) -> quint32 {
            return (quint32(quint8(f[0])) << 24)
                 | (quint32(quint8(f[1])) << 16)
                 | (quint32(quint8(f[2])) <<  8)
                 |  quint32(quint8(f[3]));
        };

        QByteArray f0 = conn.txIqFrameForTest(nullptr, 0);
        QByteArray f1 = conn.txIqFrameForTest(nullptr, 0);
        QByteArray f2 = conn.txIqFrameForTest(nullptr, 0);

        QCOMPARE(readSeq(f0), quint32(0));
        QCOMPARE(readSeq(f1), quint32(1));
        QCOMPARE(readSeq(f2), quint32(2));
    }

    // ── 4. Positive full-scale I sample → 0x7F 0xFF 0xFF ────────────────────
    // Feed I=+1.0, Q=0.0 as sample 0.
    // +1.0 × 8388607 = 8388607 = 0x7FFFFF.
    // Wire bytes at offset 4: 0x7F 0xFF 0xFF.
    // Cite: deskhpsdr/src/new_protocol.c:2811-2813 [@120188f]
    //   TXIQRINGBUF[iptr++] = (isample >> 16) & 0xFF;  → 0x7F
    //   TXIQRINGBUF[iptr++] = (isample >>  8) & 0xFF;  → 0xFF
    //   TXIQRINGBUF[iptr++] = (isample      ) & 0xFF;  → 0xFF
    void positiveFullScale_IIs_7F_FF_FF() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        iq[0] = 1.0f;   // I0 = +1.0
        iq[1] = 0.0f;   // Q0 = 0.0

        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        // Payload starts at byte 4; sample 0 I at bytes 4..6.
        QCOMPARE(quint8(frame[4]), quint8(0x7F));
        QCOMPARE(quint8(frame[5]), quint8(0xFF));
        QCOMPARE(quint8(frame[6]), quint8(0xFF));

        // Sample 0 Q (zero) at bytes 7..9.
        QCOMPARE(quint8(frame[7]),  quint8(0x00));
        QCOMPARE(quint8(frame[8]),  quint8(0x00));
        QCOMPARE(quint8(frame[9]),  quint8(0x00));
    }

    // ── 5. Negative full-scale Q sample → 0x80 0x00 0x01 ────────────────────
    // Feed I=0.0, Q=-1.0 as sample 0.
    // -1.0 × 8388607 = -8388607 = 0xFF800001 in two's-complement int32.
    // High 3 bytes: 0xFF, 0x80, 0x01 — but we clamp to -8388607 and cast to
    // quint32: 0xFFFFFFFF - 8388607 + 1 = 0xFF800001.
    //   (0xFF800001 >> 16) & 0xFF = 0xFF → wait — let's verify:
    //   -8388607 as int32 = 0xFF800001
    //   bits 23..16 of int32(-8388607) = 0xFF800001 >> 16 = 0xFF80 → & 0xFF = 0x80
    //   Wait: 0xFF800001 >> 16 = 0xFF80; & 0xFF = 0x80.
    //   bits 15..8: (0xFF800001 >> 8) & 0xFF = 0x00.
    //   bits 7..0: 0xFF800001 & 0xFF = 0x01.
    // So Q bytes: 0x80 0x00 0x01.
    // Cite: deskhpsdr/src/new_protocol.c:2814-2816 [@120188f]
    //   TXIQRINGBUF[iptr++] = (qsample >> 16) & 0xFF;
    //   TXIQRINGBUF[iptr++] = (qsample >>  8) & 0xFF;
    //   TXIQRINGBUF[iptr++] = (qsample      ) & 0xFF;
    void negativeFullScale_QIs_80_00_01() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        iq[0] = 0.0f;   // I0 = 0.0
        iq[1] = -1.0f;  // Q0 = -1.0

        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        // Sample 0 I (zero) at bytes 4..6.
        QCOMPARE(quint8(frame[4]), quint8(0x00));
        QCOMPARE(quint8(frame[5]), quint8(0x00));
        QCOMPARE(quint8(frame[6]), quint8(0x00));

        // Sample 0 Q at bytes 7..9.
        QCOMPARE(quint8(frame[7]),  quint8(0x80));
        QCOMPARE(quint8(frame[8]),  quint8(0x00));
        QCOMPARE(quint8(frame[9]),  quint8(0x01));
    }

    // ── 6. Zero sample → 0x00 0x00 0x00 0x00 0x00 0x00 ──────────────────────
    void zeroSample_allPayloadBytesAreZero() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        for (int i = 4; i < 1444; ++i) {
            QCOMPARE(quint8(frame[i]), quint8(0x00));
        }
    }

    // ── 7. Over-range positive clips to 0x7FFFFF ─────────────────────────────
    // Feed I=+2.0 → clamps to +8388607 = 0x7FFFFF.
    void overRangePositive_clipsToMaxInt24() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        iq[0] = 2.0f;   // I0 = +2.0 → clamp to +8388607

        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        QCOMPARE(quint8(frame[4]), quint8(0x7F));
        QCOMPARE(quint8(frame[5]), quint8(0xFF));
        QCOMPARE(quint8(frame[6]), quint8(0xFF));
    }

    // ── 8. Over-range negative clips to 0x800001 ─────────────────────────────
    // Feed Q=-2.0 → clamps to -8388607 = 0xFF800001 (bytes: 0x80 0x00 0x01).
    void overRangeNegative_clipsToMinInt24() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        iq[1] = -2.0f;  // Q0 = -2.0 → clamp to -8388607

        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        // Q bytes (offset 7..9 for sample 0)
        QCOMPARE(quint8(frame[7]),  quint8(0x80));
        QCOMPARE(quint8(frame[8]),  quint8(0x00));
        QCOMPARE(quint8(frame[9]),  quint8(0x01));
    }

    // ── 9. Underrun path — ring empty → silence ───────────────────────────────
    // Call txIqFrameForTest with no samples fed; entire payload must be zero.
    // Matches deskhpsdr zero-fill of unused payload bytes when ring underruns.
    void underrun_payloadIsAllZeros() {
        P2RadioConnection conn;
        QByteArray frame = conn.txIqFrameForTest(nullptr, 0);

        QCOMPARE(frame.size(), 1444);
        for (int i = 4; i < 1444; ++i) {
            QCOMPARE(quint8(frame[i]), quint8(0x00));
        }
    }

    // ── 10. Partial fill — ring has <240 samples; remaining slots are zeros ───
    // Feed 60 samples (I=0.5, rest=0). Frame should have sample 0 with correct
    // bytes; samples 60..239 should be zero (silence padding).
    // Cite: deskhpsdr underrun path: when ring < 240, remaining bytes stay 0.
    void partialFill_remainingSlotsAreZeroPadded() {
        P2RadioConnection conn;
        std::vector<float> iq(60 * 2, 0.0f);
        iq[0] = 0.5f;   // I0 = 0.5
        conn.sendTxIq(iq.data(), 60);

        // Get frame WITHOUT feeding more — triggers partial drain + zero-fill.
        QByteArray frame = conn.txIqFrameForTest(nullptr, 0);

        QCOMPARE(frame.size(), 1444);

        // Sample 0 I: 0.5 × 8388607 = 4194303.5 → (int) = 4194303 = 0x3FFFFF
        QCOMPARE(quint8(frame[4]), quint8(0x3F));
        QCOMPARE(quint8(frame[5]), quint8(0xFF));
        QCOMPARE(quint8(frame[6]), quint8(0xFF));

        // Sample 60 should be zero (ring exhausted after 60 samples).
        // Sample 60 offset: 4 + 60×6 = 364.
        QCOMPARE(quint8(frame[364]), quint8(0x00));
        QCOMPARE(quint8(frame[365]), quint8(0x00));
        QCOMPARE(quint8(frame[366]), quint8(0x00));
    }

    // ── 11. Ring saturation — excess samples beyond capacity are dropped ───────
    // Push kTxIqRingCapacityFloats floats + extra; ring should not overflow.
    // After saturation, buffered count must not exceed capacity.
    //
    // 3M-1a bench fix (2026-04-26): ring capacity increased from 2048 to 16384
    // floats to accommodate one full fexchange2 block (kTxDspBufferSize = 4096
    // samples = 8192 floats).  Test updated to match new capacity.
    void ringSaturation_excessSamplesDropped() {
        P2RadioConnection conn;

        // Slightly more than the ring capacity (in I/Q pairs).
        // kTxIqRingCapacityFloats = 16384 floats = 8192 pairs.
        const int overCount = 8192 + 64;  // 8256 pairs = 16512 floats
        std::vector<float> iq(overCount * 2, 0.25f);
        conn.sendTxIq(iq.data(), overCount);

        // Buffer must not exceed ring capacity (16384 floats).
        QVERIFY(conn.txIqRingCountForTest() <= 16384);
        QVERIFY(conn.txIqRingCountForTest() > 0);
    }

    // ── 12. Sequence number does not reset across calls ───────────────────────
    // Three frames: seq 0, 1, 2.
    void sequenceNumber_doesNotResetAcrossCalls() {
        P2RadioConnection conn;

        auto readSeq = [](const QByteArray& f) -> quint32 {
            return (quint32(quint8(f[0])) << 24)
                 | (quint32(quint8(f[1])) << 16)
                 | (quint32(quint8(f[2])) <<  8)
                 |  quint32(quint8(f[3]));
        };

        // Feed 240 samples each time so the drain is deterministic.
        std::vector<float> iq(240 * 2, 0.0f);
        const quint32 s0 = readSeq(conn.txIqFrameForTest(iq.data(), 240));
        const quint32 s1 = readSeq(conn.txIqFrameForTest(iq.data(), 240));
        const quint32 s2 = readSeq(conn.txIqFrameForTest(iq.data(), 240));

        QCOMPARE(s1, s0 + 1);
        QCOMPARE(s2, s0 + 2);
    }

    // ── 13. Last sample boundary — sample 239 lands at correct offset ─────────
    // Sample 239 (0-indexed) starts at byte 4 + 239×6 = 1438; 3 bytes I + 3 bytes Q.
    void lastSample_atCorrectOffset() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        // Set only sample 239: I=+1.0, Q=0.0.
        iq[239 * 2]     = 1.0f;
        iq[239 * 2 + 1] = 0.0f;

        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        const int base = 4 + 239 * 6;  // = 1438
        QCOMPARE(frame.size(), 1444);
        QCOMPARE(base + 5, 1443);  // Last byte is frame[1443]

        QCOMPARE(quint8(frame[base + 0]), quint8(0x7F));
        QCOMPARE(quint8(frame[base + 1]), quint8(0xFF));
        QCOMPARE(quint8(frame[base + 2]), quint8(0xFF));
        QCOMPARE(quint8(frame[base + 3]), quint8(0x00));
        QCOMPARE(quint8(frame[base + 4]), quint8(0x00));
        QCOMPARE(quint8(frame[base + 5]), quint8(0x00));
    }

    // ── 14. No HL2 CWX LSB-clear quirk in P2 ────────────────────────────────
    // The HL2 CWX LSB-clear workaround applies to P1 old_protocol only.
    // Verify that an odd I/Q value with LSB=1 is NOT masked in P2.
    // Feed I = (1.0 / 8388607.0) so int24 = 1 = 0x000001 (LSB = 1).
    // deskhpsdr/src/new_protocol.c has no &=~1 pattern near new_protocol_iq_samples.
    void noHl2LsbClearWorkaround_inP2() {
        P2RadioConnection conn;
        std::vector<float> iq(240 * 2, 0.0f);
        // Smallest positive int24 representable: 1/8388607.0 ≈ 1.19e-7.
        iq[0] = 1.0f / 8388607.0f;  // I = exactly +1 after floor
        // Q = 0.

        QByteArray frame = conn.txIqFrameForTest(iq.data(), 240);

        // I = 1 → bytes: 0x00 0x00 0x01
        QCOMPARE(quint8(frame[4]), quint8(0x00));
        QCOMPARE(quint8(frame[5]), quint8(0x00));
        QCOMPARE(quint8(frame[6]), quint8(0x01));  // LSB NOT cleared
    }
};

QTEST_MAIN(TestP2TxIqWire)
#include "tst_p2_tx_iq_wire.moc"
