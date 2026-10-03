// no-port-check: NereusSDR-original test. It cites Thetis obbuffs.c /
// network.c, deskhpsdr new_protocol.c and the P2 gateware only for the
// behaviour and sizes it expects; no upstream logic is ported here.
//
// R-IOS-13, R-R3-42: the Protocol 2 transmit I/Q send path never drops a
// sample on a transient stall.
//
// The Rock's Core, 2026-09-26: 7806 "P2 TX I/Q ring buffer overflow" lines in
// four keys. The drain was a 5 ms timer on the connection thread sending a
// fixed 4 frames a tick; a late tick was never made up, and the ring held
// 42.7 ms. These tests drive the send path on simulated time:
//   - the producer pushes a 256-sample block per radio mic packet
//     (64 mic samples at 48 kHz = 1.333 ms, x4 to 192 kHz), as the TX pump;
//   - the send thread makes one pass a millisecond (serviceTxIqSendForTest).
// A connection-thread stall holds the mic packets, so the producer stops and
// then bursts the backlog; a send-thread stall stops the passes.
//
// Invariants checked in every scenario:
//   - no sample is lost: every produced sample reaches the wire, in order;
//   - the radio's buffer (4096 samples in the P2 gateware, Tx1_IQ_fifo.vhd:106
//     [@8e86a61]) is never filled past the 15 ms target;
//   - after the stall the frame rate returns to 800 a second (192 kHz / 240).
//
// Modification history (NereusSDR):
//   2026-10-01: TX diagnostics lane: a key with a late first block, a send
//               thread stall and a half-second producer pause places its
//               silence (start, mid-key, tail), its ran dry and its
//               catch-up bursts in time. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane, review round: the unkey tail's start,
//               and a second key whose passes place its start afresh (the
//               send thread's own reset). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
#include <QtTest/QtTest>

#include "core/P2RadioConnection.h"

#include <vector>

using namespace NereusSDR;

namespace {

constexpr qint64 kMs = 1'000'000;
constexpr int kBlock = 256;                 // pairs per pump block at 192 kHz
constexpr int kTargetLead = 2880;           // TxIqPacer::kTargetLeadSamples

qint64 micPacketNs(qint64 k) { return k * 4'000'000 / 3; }  // 1.333 ms each

// The value a sample's I carries on the wire: 1-based index, so silence (0)
// is never mistaken for a sample.
float sampleValue(qint64 index) { return float(index + 2) / 8388608.0f; }

struct Run {
    P2RadioConnection conn;
    P2RadioConnection::TxIqCapture cap;
    qint64 produced{0};        // real samples pushed
    qint64 nextMic{0};         // next mic packet index
    qint64 maxLead{0};
    QList<int> passFrames;     // frames per send pass, in order
    QList<qint64> passAtNs;

    void pushBlock()
    {
        std::vector<float> iq(kBlock * 2, 0.0f);
        for (int i = 0; i < kBlock; ++i) {
            iq[2 * i] = sampleValue(produced + i);
        }
        conn.sendTxIq(iq.data(), kBlock);
        produced += kBlock;
    }

    void pass(qint64 nowNs)
    {
        const int n = conn.serviceTxIqSendForTest(nowNs, &cap);
        passFrames.append(n);
        passAtNs.append(nowNs);
        maxLead = std::max<qint64>(maxLead, qint64(conn.txIqRadioLeadForTest()));
    }

    // Simulates [fromNs, toNs). Producer blocks come with the mic packets
    // unless the producer is stalled; a stalled producer's packets arrive
    // as one burst when the stall ends. The send thread passes once a
    // millisecond unless it is stalled.
    using Windows = QList<QPair<qint64, qint64>>;
    static bool inside(const Windows& w, qint64 t)
    {
        for (const auto& p : w) {
            if (t >= p.first && t < p.second) {
                return true;
            }
        }
        return false;
    }
    void simulate(qint64 fromNs, qint64 toNs,
                  const Windows& producerStalls = {}, const Windows& senderStalls = {})
    {
        for (qint64 t = fromNs; t < toNs; t += kMs / 4) {
            while (!inside(producerStalls, t) && micPacketNs(nextMic) <= t) {
                pushBlock();
                ++nextMic;
            }
            if (!inside(senderStalls, t) && t % kMs == 0) {
                pass(t);
            }
        }
    }

    // Decodes the wire: the real samples in order, and the silent samples.
    void decode(QList<qint64>* real, qint64* silent) const
    {
        *silent = 0;
        for (const QByteArray& f : cap.frames) {
            for (int s = 0; s < 240; ++s) {
                const int o = 4 + s * 6;
                const qint32 v = (qint32(quint8(f[o])) << 24 | qint32(quint8(f[o + 1])) << 16
                                  | qint32(quint8(f[o + 2])) << 8) >> 8;
                if (v == 0) {
                    ++*silent;
                } else {
                    real->append(qint64(v) - 1);
                }
            }
        }
    }

    // Frames sent in [fromNs, toNs).
    int framesIn(qint64 fromNs, qint64 toNs) const
    {
        int n = 0;
        for (int i = 0; i < passAtNs.size(); ++i) {
            if (passAtNs[i] >= fromNs && passAtNs[i] < toNs) {
                n += passFrames[i];
            }
        }
        return n;
    }
};

// Every produced sample is on the wire exactly once, in order, apart from
// what is still queued in the ring.
void verifyNothingLost(const Run& r)
{
    QList<qint64> real;
    qint64 silent = 0;
    r.decode(&real, &silent);
    const qint64 queued = r.conn.txIqRingCountForTest() / 2;
    QCOMPARE(qint64(real.size()) + queued, r.produced);
    for (qint64 i = 0; i < real.size(); ++i) {
        if (real[i] != i) {
            QFAIL(qPrintable(QStringLiteral("sample %1 on the wire where %2 was due").arg(real[i]).arg(i)));
        }
    }
    QCOMPARE(r.conn.txSendStats().overflowSamples, quint64(0));
}

} // namespace

class TestP2TxIqSend : public QObject {
    Q_OBJECT

private slots:
    // Red on 68410c40: the ring held 16384 floats (42.7 ms), so a 200 ms
    // stall's worth of blocks on top of the 20 ms cushion overflowed.
    void ring_holdsTwoHundredMsStallWithCushion()
    {
        P2RadioConnection conn;
        conn.setMox(true);  // arms the key-on cushion
        std::vector<float> iq(kBlock * 2, 0.1f);
        const int blocks = (192000 / 5 + kBlock - 1) / kBlock;  // 200 ms
        for (int b = 0; b < blocks; ++b) {
            conn.sendTxIq(iq.data(), kBlock);
        }
        // R-IOS-13 (2026-09-27): the cushion is the target lead plus one
        // frame, 3120 pairs.
        QCOMPARE(conn.txIqRingCountForTest() / 2, kTargetLead + 240 + blocks * kBlock);
        QCOMPARE(conn.txSendStats().overflowSamples, quint64(0));
    }

    void steadyRun_matchesRadioRate()
    {
        Run r;
        r.conn.setMox(true);
        r.simulate(0, 10'000 * kMs);
        verifyNothingLost(r);
        // 192000 / 240 = 800 frames a second, every second after the first.
        for (int sec = 1; sec < 10; ++sec) {
            const int n = r.framesIn(sec * 1000 * kMs, (sec + 1) * 1000 * kMs);
            QVERIFY2(n >= 799 && n <= 801, qPrintable(QString::number(n)));
        }
        QVERIFY(r.maxLead <= kTargetLead);
        const auto st = r.conn.txSendStats();
        QCOMPARE(st.zeroPaddedSamples, quint64(0));
        QCOMPARE(st.radioRanDry, quint64(0));
        QCOMPARE(st.lateWakes, quint64(0));
        QVERIFY(st.framesSent >= 7990);
    }

    void connectionThreadStall_dropsNothing_data()
    {
        QTest::addColumn<int>("stallMs");
        QTest::newRow("50 ms") << 50;
        QTest::newRow("100 ms") << 100;
        QTest::newRow("200 ms") << 200;
    }
    // The radio's mic packets wait behind the stall, so the pump stops and
    // then bursts the backlog. The send thread keeps the radio fed (silence
    // once the cushion is gone) and sends every sample after.
    void connectionThreadStall_dropsNothing()
    {
        QFETCH(int, stallMs);
        Run r;
        r.conn.setMox(true);
        const qint64 stallAt = 2000 * kMs;
        r.simulate(0, 6000 * kMs, {{stallAt, stallAt + stallMs * kMs}});
        verifyNothingLost(r);
        QVERIFY(r.maxLead <= kTargetLead);
        // Cadence back to steady within a second of the stall.
        for (int sec = 4; sec < 6; ++sec) {
            const int n = r.framesIn(sec * 1000 * kMs, (sec + 1) * 1000 * kMs);
            QVERIFY2(n >= 799 && n <= 801, qPrintable(QString::number(n)));
        }
    }

    void sendThreadStall_dropsNothing_data()
    {
        QTest::addColumn<int>("stallMs");
        QTest::newRow("50 ms") << 50;
        QTest::newRow("100 ms") << 100;
        QTest::newRow("200 ms") << 200;
    }
    // The send thread misses its passes; the producer keeps going. The ring
    // holds the backlog, the first pass after refills the radio to its
    // target (a capped burst, never above it), and nothing is dropped.
    void sendThreadStall_dropsNothing()
    {
        QFETCH(int, stallMs);
        Run r;
        r.conn.setMox(true);
        const qint64 stallAt = 2000 * kMs;
        r.simulate(0, 6000 * kMs, {}, {{stallAt, stallAt + stallMs * kMs}});
        verifyNothingLost(r);
        QVERIFY(r.maxLead <= kTargetLead);
        // The first pass after the stall: at most the target lead's frames.
        int firstAfter = -1;
        for (int i = 0; i < r.passAtNs.size(); ++i) {
            if (r.passAtNs[i] >= stallAt + stallMs * kMs) {
                firstAfter = r.passFrames[i];
                break;
            }
        }
        QVERIFY(firstAfter > 4);
        QVERIFY(firstAfter <= kTargetLead / 240);
        const auto st = r.conn.txSendStats();
        QCOMPARE(st.zeroPaddedSamples, quint64(0));
        QCOMPARE(st.lateWakes, quint64(1));
        QCOMPARE(st.catchUpBursts, quint64(1));
        QCOMPARE(st.radioRanDry, quint64(1));
        for (int sec = 4; sec < 6; ++sec) {
            const int n = r.framesIn(sec * 1000 * kMs, (sec + 1) * 1000 * kMs);
            QVERIFY2(n >= 799 && n <= 801, qPrintable(QString::number(n)));
        }
    }

    // The old design's failure: both halves on one stalled thread (the
    // send thread starved as well as the connection thread).
    void bothStall_dropsNothing_data()
    {
        QTest::addColumn<int>("stallMs");
        QTest::newRow("50 ms") << 50;
        QTest::newRow("100 ms") << 100;
        QTest::newRow("200 ms") << 200;
    }
    void bothStall_dropsNothing()
    {
        QFETCH(int, stallMs);
        Run r;
        r.conn.setMox(true);
        const qint64 stallAt = 2000 * kMs;
        const Run::Windows stall{{stallAt, stallAt + stallMs * kMs}};
        r.simulate(0, 6000 * kMs, stall, stall);
        verifyNothingLost(r);
        QVERIFY(r.maxLead <= kTargetLead);
        for (int sec = 4; sec < 6; ++sec) {
            const int n = r.framesIn(sec * 1000 * kMs, (sec + 1) * 1000 * kMs);
            QVERIFY2(n >= 799 && n <= 801, qPrintable(QString::number(n)));
        }
    }

    // Past the cap. Each stall of the send thread longer than the radio's
    // 15 ms lead leaves that much more queued (the air time went by with
    // the radio empty), so repeated long stalls fill the 341 ms ring. Then
    // the ring refuses the newest samples: every one counted, none silent.
    void pastTheCap_lossIsCounted()
    {
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("P2 transmit I/Q buffer full")));
        Run r;
        r.conn.setMox(true);
        Run::Windows stalls;
        for (int k = 0; k < 6; ++k) {
            const qint64 from = 1000 * kMs + k * 500 * kMs;
            stalls.append({from, from + 200 * kMs});
        }
        r.simulate(0, 6000 * kMs, stalls, stalls);
        QList<qint64> real;
        qint64 silent = 0;
        r.decode(&real, &silent);
        const auto st = r.conn.txSendStats();
        QVERIFY(st.overflowSamples > 0);
        QCOMPARE(qint64(real.size()) + r.conn.txIqRingCountForTest() / 2
                     + qint64(st.overflowSamples),
                 r.produced);
        QVERIFY(st.maxRingMs >= 330);
        QVERIFY(r.maxLead <= kTargetLead);
    }

    // Unkeyed, the radio still gets its port-1029 stream: silence at the
    // same 800 frames a second, never counted as a hole.
    void unkeyed_sendsSilenceAtRadioRate()
    {
        Run r;
        for (qint64 t = 0; t < 3000 * kMs; t += kMs) {
            r.pass(t);
        }
        const int n = r.framesIn(1000 * kMs, 2000 * kMs);
        QVERIFY2(n >= 799 && n <= 801, qPrintable(QString::number(n)));
        QCOMPARE(r.conn.txSendStats().zeroPaddedSamples, quint64(0));
        QVERIFY(r.maxLead <= kTargetLead);
    }

    // TX diagnostics lane: the TX channel's first block comes 60 ms after
    // the key; the send thread stalls 30 ms at 1 s (the radio runs dry,
    // then a refill); the producer gives nothing for 500 ms at 2 s (its
    // blocks are not held for later, as while the TX channel does not
    // send); the producer stops at 3 s and the key runs on to 3.1 s (the
    // tail). Each is placed in time, and the three parts of the silence
    // add up to the whole.
    void dropoutsArePlacedInTheKey()
    {
        Run r;
        r.conn.setMox(true);
        const Run::Windows producerSilent{{0, 60 * kMs}, {2000 * kMs, 2500 * kMs},
                                          {3000 * kMs, 4000 * kMs}};
        const Run::Windows sender{{1000 * kMs, 1030 * kMs}};
        for (qint64 t = 0; t < 3100 * kMs; t += kMs / 4) {
            while (micPacketNs(r.nextMic) <= t) {
                if (!Run::inside(producerSilent, t)) {
                    r.pushBlock();
                }
                ++r.nextMic;
            }
            if (!Run::inside(sender, t) && t % kMs == 0) {
                r.pass(t);
            }
        }
        const auto st = r.conn.txSendStats();
        const QString text =
            QStringLiteral("start %1 mid %2 tail %3 total %4; first block %5 ms; longest mid "
                           "%6 at %7 ms; dry at %8 ms gap %9 ms; bursts %10")
                .arg(st.padStartSamples)
                .arg(st.padMidSamples)
                .arg(st.padTailSamples)
                .arg(st.zeroPaddedSamples)
                .arg(st.firstBlockAtMs)
                .arg(st.longestMidPadSamples)
                .arg(st.longestMidPadAtMs)
                .arg(st.firstDryAtMs)
                .arg(st.firstDryGapMs)
                .arg(st.burstEvents);
        qInfo().noquote() << text;
        QVERIFY(st.placed);
        QCOMPARE(st.keySteadyNs, qint64(0));
        QCOMPARE(st.padStartSamples + st.padMidSamples + st.padTailSamples,
                 st.zeroPaddedSamples);
        // The start: the radio's 15 ms lead filled with silence, then 192
        // samples a ms until the block at 60 ms.
        QVERIFY2(st.firstBlockAtMs >= 60.0 && st.firstBlockAtMs <= 61.0, qPrintable(text));
        QVERIFY2(st.padStartSamples >= quint64(55 * 192)
                     && st.padStartSamples <= quint64(kTargetLead + 61 * 192),
                 qPrintable(text));
        // Mid-key: the longest run is from when the radio's lead and what
        // the ring still held ran down after the pause began (the stall at
        // 1 s left about 15 ms queued), to the pause's end; a few short
        // runs follow while the lead climbs back over its low water.
        QVERIFY2(st.longestMidPadAtMs >= 2000.0 && st.longestMidPadAtMs <= 2040.0,
                 qPrintable(text));
        const double longestEndMs =
            st.longestMidPadAtMs + static_cast<double>(st.longestMidPadSamples) / 192.0;
        QVERIFY2(std::abs(longestEndMs - 2500.0) <= 1.5, qPrintable(text));
        QVERIFY2(st.padMidSamples >= st.longestMidPadSamples
                     && st.padMidSamples <= st.longestMidPadSamples + quint64(10 * 192),
                 qPrintable(text));
        // The tail: from the lead's low water after 3 s to 3.1 s, placed
        // at its start.
        QVERIFY2(st.padTailSamples >= quint64(80 * 192) && st.padTailSamples <= quint64(100 * 192),
                 qPrintable(text));
        QVERIFY2(st.padTailAtMs >= 3000.0 && st.padTailAtMs <= 3020.0,
                 qPrintable(QStringLiteral("tail from %1 ms").arg(st.padTailAtMs)));
        const double tailEndMs = st.padTailAtMs + static_cast<double>(st.padTailSamples) / 192.0;
        QVERIFY2(std::abs(tailEndMs - 3100.0) <= 1.5,
                 qPrintable(QStringLiteral("tail ends %1 ms").arg(tailEndMs)));
        // The send thread's stall: passes at 999 ms and 1030 ms.
        QCOMPARE(st.radioRanDry, quint64(1));
        QVERIFY2(std::abs(st.firstDryAtMs - 1030.0) < 0.01, qPrintable(text));
        QVERIFY2(std::abs(st.firstDryGapMs - 31.0) < 0.01, qPrintable(text));
        // Bursts: the first block's refill from its cushion, and the
        // stall's (nothing was held through the pause to refill with).
        QCOMPARE(st.catchUpBursts, quint64(2));
        QCOMPARE(st.burstEvents, 2);
        QVERIFY2(st.bursts[0].atMs >= 60.0 && st.bursts[0].atMs <= 61.0, qPrintable(text));
        QVERIFY2(std::abs(st.bursts[0].gapMs - 1.0) < 0.01, qPrintable(text));
        QVERIFY2(std::abs(st.bursts[1].atMs - 1030.0) < 0.01, qPrintable(text));
        QVERIFY2(std::abs(st.bursts[1].gapMs - 31.0) < 0.01, qPrintable(text));
        for (int i = 0; i < st.burstEvents; ++i) {
            QVERIFY(st.bursts[static_cast<size_t>(i)].frames > 4);
        }

        // A new key starts the placement afresh.
        r.conn.setMox(false);
        r.conn.setMox(true);
        const auto fresh = r.conn.txSendStats();
        QCOMPARE(fresh.padTailSamples, quint64(0));
        QCOMPARE(fresh.burstEvents, 0);
        QCOMPARE(fresh.firstDryAtMs, -1.0);
        QCOMPARE(fresh.firstBlockAtMs, -1.0);
        QCOMPARE(fresh.padTailAtMs, -1.0);

        // The send thread's own state starts afresh too: the second key's
        // passes place its start from its own first pass, with nothing of
        // the first key's open tail or blocks carried over.
        constexpr qint64 kSecondKeyNs = 4000 * kMs;
        r.nextMic = 0;
        while (micPacketNs(r.nextMic) < kSecondKeyNs) {
            ++r.nextMic;
        }
        for (qint64 t = kSecondKeyNs; t < kSecondKeyNs + 500 * kMs; t += kMs / 4) {
            while (micPacketNs(r.nextMic) <= t) {
                if (t >= kSecondKeyNs + 40 * kMs) {
                    r.pushBlock();
                }
                ++r.nextMic;
            }
            if (t % kMs == 0) {
                r.pass(t);
            }
        }
        const auto second = r.conn.txSendStats();
        const QString secondText =
            QStringLiteral("second key at %1 ns: start %2 mid %3 tail %4 total %5; first block "
                           "%6 ms; dry %7")
                .arg(second.keySteadyNs)
                .arg(second.padStartSamples)
                .arg(second.padMidSamples)
                .arg(second.padTailSamples)
                .arg(second.zeroPaddedSamples)
                .arg(second.firstBlockAtMs)
                .arg(second.radioRanDry);
        qInfo().noquote() << secondText;
        QCOMPARE(second.keySteadyNs, kSecondKeyNs);
        QVERIFY2(second.firstBlockAtMs >= 40.0 && second.firstBlockAtMs <= 41.0,
                 qPrintable(secondText));
        QVERIFY2(second.padStartSamples >= quint64(35 * 192)
                     && second.padStartSamples <= quint64(kTargetLead + 41 * 192),
                 qPrintable(secondText));
        QCOMPARE(second.padMidSamples, quint64(0));
        QCOMPARE(second.padTailSamples, quint64(0));
        QCOMPARE(second.zeroPaddedSamples, second.padStartSamples);
        QCOMPARE(second.radioRanDry, quint64(0));
    }

    // A socket that refuses a frame (full send buffer): the frame is kept
    // and sent next pass with its sequence number, nothing skipped.
    void refusedFrame_isRetriedInOrder()
    {
        Run r;
        r.conn.setMox(true);
        r.simulate(0, 100 * kMs);
        r.cap.refuse = 3;
        r.simulate(100 * kMs, 1000 * kMs);
        verifyNothingLost(r);
        quint32 expect = 0;
        for (const QByteArray& f : r.cap.frames) {
            const quint32 seq = quint32(quint8(f[0])) << 24 | quint32(quint8(f[1])) << 16
                              | quint32(quint8(f[2])) << 8 | quint32(quint8(f[3]));
            QCOMPARE(seq, expect++);
        }
        QCOMPARE(r.conn.txSendStats().sendErrors, quint64(3));
    }
};

QTEST_MAIN(TestP2TxIqSend)
#include "tst_p2_tx_iq_send.moc"
