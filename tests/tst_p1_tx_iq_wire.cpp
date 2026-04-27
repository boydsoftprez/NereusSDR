// no-port-check: test-only — deskhpsdr/Thetis file names appear only in
// source-cite comments that document which upstream line each assertion
// verifies. No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::sendTxIq() (3M-1a Task E.2).
//
// Layout under test (per-sample, 8 bytes, 63 samples = 504 bytes per zone):
//   [0-1] mic_L int16 big-endian (zero — NullMicSource)
//   [2-3] mic_R int16 big-endian (zero — NullMicSource)
//   [4-5] I     int16 big-endian
//   [6-7] Q     int16 big-endian
//
// Cite: deskhpsdr/src/old_protocol.c:2429-2458 [@120188f]
//       deskhpsdr/src/transmitter.c:1541 [@120188f] (gain = 32767.0)
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1TxIqWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Unit-pulse test ─────────────────────────────────────────────────
    // Feed 126 samples: sample 0 = (1.0, 0.0), rest = (0.0, 0.0).
    // After sendTxIq, call sendTxIqAndCapture to get a full Metis frame.
    // Verify:
    //   frame[16..19] = mic zeros (bytes 0-3 of sample 0)
    //   frame[20..21] = 0x7F 0xFF  (I = max-positive int16 = 32767 = 0x7FFF)
    //   frame[22..23] = 0x00 0x00  (Q = 0)
    //
    // float→int16: gain=32767.0, v=1.0 → (long)(1.0*32767+0.5)=32767=0x7FFF
    // Cite: deskhpsdr/src/transmitter.c:1541 [@120188f]
    void unitPulse_firstSampleIsMaxI() {
        P1RadioConnection conn;

        // Build 126-sample buffer: sample 0 = (1.0, 0.0)
        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] = 1.0f;  // I0
        iq[1] = 0.0f;  // Q0

        // Feed and capture the Metis frame.
        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);
        QCOMPARE(frame.size(), 1032);

        // Metis header sanity
        QCOMPARE(quint8(frame[0]), quint8(0xEF));
        QCOMPARE(quint8(frame[1]), quint8(0xFE));
        QCOMPARE(quint8(frame[2]), quint8(0x01));
        QCOMPARE(quint8(frame[3]), quint8(0x02));

        // Subframe 0 sync
        QCOMPARE(quint8(frame[8]),  quint8(0x7F));
        QCOMPARE(quint8(frame[9]),  quint8(0x7F));
        QCOMPARE(quint8(frame[10]), quint8(0x7F));

        // TX data zone starts at frame[16].
        // Sample 0, bytes 0-3: mic_L + mic_R = zeros.
        QCOMPARE(quint8(frame[16]), quint8(0x00));  // mic_L hi
        QCOMPARE(quint8(frame[17]), quint8(0x00));  // mic_L lo
        QCOMPARE(quint8(frame[18]), quint8(0x00));  // mic_R hi
        QCOMPARE(quint8(frame[19]), quint8(0x00));  // mic_R lo

        // Sample 0, bytes 4-5: I = 0x7FFF (32767).
        // Cite: deskhpsdr/src/transmitter.c:1541,1787 [@120188f]
        //   gain=32767.0; isample=(long)(1.0*32767.0+0.5)=32767=0x7FFF
        QCOMPARE(quint8(frame[20]), quint8(0x7F));  // I hi
        QCOMPARE(quint8(frame[21]), quint8(0xFF));  // I lo

        // Sample 0, bytes 6-7: Q = 0x0000.
        QCOMPARE(quint8(frame[22]), quint8(0x00));  // Q hi
        QCOMPARE(quint8(frame[23]), quint8(0x00));  // Q lo
    }

    // ── 2. Subframe 1 zone populated correctly ────────────────────────────
    // Sample 63 (first sample of subframe 1) should be all-zero (silence).
    // The second zone starts at frame[528] after the 3-byte sync + 5-byte C&C.
    // With sample 0=(1,0) and 1..125=(0,0): sample 63 in zone 2 is (0,0).
    void subframe1ZoneCorrectOffset() {
        P1RadioConnection conn;

        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] = 1.0f;  // I0

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);

        // Subframe 1 sync at [520..522]
        QCOMPARE(quint8(frame[520]), quint8(0x7F));
        QCOMPARE(quint8(frame[521]), quint8(0x7F));
        QCOMPARE(quint8(frame[522]), quint8(0x7F));

        // TX data zone 2 at frame[528]. Sample 63 = (0,0).
        QCOMPARE(quint8(frame[528]), quint8(0x00));  // mic_L hi
        QCOMPARE(quint8(frame[529]), quint8(0x00));  // mic_L lo
        QCOMPARE(quint8(frame[530]), quint8(0x00));  // mic_R hi
        QCOMPARE(quint8(frame[531]), quint8(0x00));  // mic_R lo
        QCOMPARE(quint8(frame[532]), quint8(0x00));  // I hi
        QCOMPARE(quint8(frame[533]), quint8(0x00));  // I lo
        QCOMPARE(quint8(frame[534]), quint8(0x00));  // Q hi
        QCOMPARE(quint8(frame[535]), quint8(0x00));  // Q lo
    }

    // ── 3. Partial-fill test ──────────────────────────────────────────────
    // Feed 60 samples → buffer has 60; capture should underrun (zeros in TX zone).
    // Feed 66 more → buffer has 126; capture should drain exactly 126.
    void partialFill_buffersDontSendUntilFull() {
        P1RadioConnection conn;

        std::vector<float> partial(60 * 2, 0.0f);
        partial[0] = 0.5f;  // I0 = 0.5

        // Feed 60: buffer should have 60, no frame drained yet.
        conn.sendTxIq(partial.data(), 60);
        QCOMPARE(conn.txIqBufferedSamplesForTest(), 60);

        // Now feed 66 more (all silence except first I = 0.25).
        std::vector<float> rest(66 * 2, 0.0f);
        rest[0] = 0.25f;
        conn.sendTxIq(rest.data(), 66);
        QCOMPARE(conn.txIqBufferedSamplesForTest(), 126);

        // Capture drains 126 → buffer should be empty.
        std::vector<float> zeros(0 * 2, 0.0f);  // empty — capture triggers drain
        QByteArray frame = conn.sendTxIqAndCapture(zeros.data(), 0);
        QCOMPARE(frame.size(), 1032);
        QCOMPARE(conn.txIqBufferedSamplesForTest(), 0);
    }

    // ── 4. Multi-frame test ────────────────────────────────────────────────
    // Feed 252 samples (2 frame's worth). Capture twice; verify sequence
    // numbers increment by 1 each capture (our test seam increments m_epSendSeq).
    void multiFrame_sequenceNumberIncrements() {
        P1RadioConnection conn;

        std::vector<float> iq(252 * 2, 0.0f);
        conn.sendTxIq(iq.data(), 252);
        QCOMPARE(conn.txIqBufferedSamplesForTest(), 252);

        std::vector<float> empty;
        QByteArray frame1 = conn.sendTxIqAndCapture(empty.data(), 0);
        QByteArray frame2 = conn.sendTxIqAndCapture(empty.data(), 0);

        QCOMPARE(frame1.size(), 1032);
        QCOMPARE(frame2.size(), 1032);

        // Sequence numbers are at bytes [4..7], big-endian.
        auto readSeq = [](const QByteArray& f) -> quint32 {
            return (quint32(quint8(f[4])) << 24)
                 | (quint32(quint8(f[5])) << 16)
                 | (quint32(quint8(f[6])) <<  8)
                 |  quint32(quint8(f[7]));
        };
        const quint32 seq1 = readSeq(frame1);
        const quint32 seq2 = readSeq(frame2);
        QCOMPARE(seq2, seq1 + 1);

        // Both frames drained — buffer should be empty.
        QCOMPARE(conn.txIqBufferedSamplesForTest(), 0);
    }

    // ── 5. Sign + clamp test ───────────────────────────────────────────────
    // Feed v = -1.0: expected int16 = -32767 = 0x8001.
    // Cite: deskhpsdr/src/transmitter.c:1787-1788 [@120188f]
    //   qsample = (long)(qs * gain + (qs >= 0.0 ? 0.5 : -0.5))
    //   qs=-1.0: (-1.0*32767.0 - 0.5) = -32767.5 → (long) = -32767 → 0x8001
    void negativeFullScale_encodedAsNegativeInt16() {
        P1RadioConnection conn;

        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] = 0.0f;   // I0 = 0
        iq[1] = -1.0f;  // Q0 = -1.0

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);

        // Sample 0 Q bytes at frame[22..23].
        // -32767 = 0x8001
        QCOMPARE(quint8(frame[22]), quint8(0x80));  // Q hi
        QCOMPARE(quint8(frame[23]), quint8(0x01));  // Q lo
    }

    // ── 6. Over-range clamp test ───────────────────────────────────────────
    // Feed I = 2.0 (clips to 32767 = 0x7FFF).
    // Feed Q = -2.0 (clips to -32767 = 0x8001).
    void overRangePositive_clampsToMaxInt16() {
        P1RadioConnection conn;

        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] = 2.0f;   // I0 = 2.0 → clamp to 32767
        iq[1] = -2.0f;  // Q0 = -2.0 → clamp to -32767

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);

        // I: 0x7FFF
        QCOMPARE(quint8(frame[20]), quint8(0x7F));
        QCOMPARE(quint8(frame[21]), quint8(0xFF));

        // Q: 0x8001
        QCOMPARE(quint8(frame[22]), quint8(0x80));
        QCOMPARE(quint8(frame[23]), quint8(0x01));
    }

    // ── 7. Underrun test ──────────────────────────────────────────────────
    // Feed 0 samples; capture should give zeros in TX zones (silence).
    void underrun_txZonesAreZero() {
        P1RadioConnection conn;

        std::vector<float> empty;
        QByteArray frame = conn.sendTxIqAndCapture(empty.data(), 0);
        QCOMPARE(frame.size(), 1032);

        // TX zone 0 first sample: all zeros.
        for (int i = 16; i < 24; ++i) {
            QCOMPARE(quint8(frame[i]), quint8(0x00));
        }
        // TX zone 1 first sample: all zeros.
        for (int i = 528; i < 536; ++i) {
            QCOMPARE(quint8(frame[i]), quint8(0x00));
        }
    }

    // ── 8. Mic bytes are zero (NullMicSource) ─────────────────────────────
    // Even when I/Q are non-zero, mic bytes [0..3] per sample must be zero.
    // Cite: deskhpsdr/src/old_protocol.c:2429-2434 [@120188f]
    //   (HL2 path writes zeros; NereusSDR 3M-1a uses NullMicSource)
    void micBytesAreZeroForNullMicSource() {
        P1RadioConnection conn;

        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] = 0.75f;  // I0
        iq[1] = 0.5f;   // Q0

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);

        // Bytes 16-19 = mic_L hi, mic_L lo, mic_R hi, mic_R lo = zeros.
        QCOMPARE(quint8(frame[16]), quint8(0x00));
        QCOMPARE(quint8(frame[17]), quint8(0x00));
        QCOMPARE(quint8(frame[18]), quint8(0x00));
        QCOMPARE(quint8(frame[19]), quint8(0x00));
    }

    // ── 9. Zone boundary: last sample of zone 0 ───────────────────────────
    // Sample 62 (index 0-based) is the last in zone 0 (frame[16 + 62×8] = frame[512]).
    void zone0LastSample_atCorrectOffset() {
        P1RadioConnection conn;

        // All zeros except sample 62 which has I=0.5, Q=-0.5.
        std::vector<float> iq(126 * 2, 0.0f);
        iq[62 * 2]     = 0.5f;   // I62
        iq[62 * 2 + 1] = -0.5f;  // Q62

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);

        // Sample 62 starts at frame[16 + 62×8] = frame[512].
        // I = (long)(0.5*32767+0.5) = (long)16383.5 + 0.5 = 16384 = 0x4000
        const int base = 16 + 62 * 8;  // = 512
        QCOMPARE(quint8(frame[base + 4]), quint8(0x40));  // I hi
        QCOMPARE(quint8(frame[base + 5]), quint8(0x00));  // I lo

        // Q = (long)(-0.5*32767 - 0.5) = (long)(-16383.5 - 0.5) = -16384 = 0xC000
        QCOMPARE(quint8(frame[base + 6]), quint8(0xC0));  // Q hi
        QCOMPARE(quint8(frame[base + 7]), quint8(0x00));  // Q lo
    }

    // ── 10. HL2 CWX LSB-clear workaround ─────────────────────────────────────
    // When HPSDRModel == HERMESLITE, I-low and Q-low bytes must have LSB = 0
    // (masked with 0xFE). Non-HL2 path must NOT mask.
    // Cite: deskhpsdr/src/old_protocol.c:2441-2453 [@120188f]
    //   TXRINGBUF[iptr++] = isample & 0xFE;  // I lo — LSB cleared on HL2
    //   TXRINGBUF[iptr++] = qsample & 0xFE;  // Q lo — LSB cleared on HL2
    void hl2_iLoAndQLoLsbAreCleared() {
        P1RadioConnection conn;

        // Set HPSDRModel to HERMESLITE so the workaround activates.
        HardwareProfile hl2Profile;
        hl2Profile.model = HPSDRModel::HERMESLITE;
        conn.setHardwareProfile(hl2Profile);

        // Use I = 0x7FFF (32767) and Q = 0x8001 (-32767).
        // I lo without mask = 0xFF; with 0xFE mask → 0xFE.
        // Q lo without mask = 0x01; with 0xFE mask → 0x00.
        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] =  1.0f;   // I0 → 32767 = 0x7FFF; I lo = 0xFF → 0xFE after mask
        iq[1] = -1.0f;   // Q0 → -32767 = 0x8001; Q lo = 0x01 → 0x00 after mask

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);
        QCOMPARE(frame.size(), 1032);

        // I hi unchanged: 0x7F
        QCOMPARE(quint8(frame[20]), quint8(0x7F));
        // I lo LSB cleared: 0xFF & 0xFE = 0xFE
        QCOMPARE(quint8(frame[21]), quint8(0xFE));

        // Q hi unchanged: 0x80
        QCOMPARE(quint8(frame[22]), quint8(0x80));
        // Q lo LSB cleared: 0x01 & 0xFE = 0x00
        QCOMPARE(quint8(frame[23]), quint8(0x00));
    }

    // ── 11. Non-HL2 LSB is NOT masked ────────────────────────────────────────
    // On a non-HL2 board the I lo and Q lo bytes must be unmodified.
    void nonHl2_iLoAndQLoLsbPreserved() {
        P1RadioConnection conn;
        // Default model is HPSDRModel::HERMES — no mask applied.

        // I = 0x7FFF, Q = 0x8001 same as above.
        std::vector<float> iq(126 * 2, 0.0f);
        iq[0] =  1.0f;   // I0 → 32767 = 0x7FFF; I lo = 0xFF (no mask)
        iq[1] = -1.0f;   // Q0 → -32767 = 0x8001; Q lo = 0x01 (no mask)

        QByteArray frame = conn.sendTxIqAndCapture(iq.data(), 126);
        QCOMPARE(frame.size(), 1032);

        // I lo must be 0xFF (unmasked)
        QCOMPARE(quint8(frame[21]), quint8(0xFF));
        // Q lo must be 0x01 (unmasked)
        QCOMPARE(quint8(frame[23]), quint8(0x01));
    }
};

QTEST_APPLESS_MAIN(TestP1TxIqWire)
#include "tst_p1_tx_iq_wire.moc"
