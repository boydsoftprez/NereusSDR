// =================================================================
// tests/tst_legacy_iq_tap_is_stream_zero.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The two-tap fork is a NereusSDR
// construct (Phase 3F Sub-Epic I Task 8); there is no upstream, because
// Thetis has no multi-stream I/Q publisher to port from.
//
// Codex review round 7, PR #293.
//
// RadioModel publishes each I/Q frame twice: once tagged with its stream
// (rawIqDataForStream, added by Sub-Epic I for MainWindow's per-stream
// FFTEngine pool) and once untagged (rawIqData, which predates all of
// this). The untagged signal has exactly one subscriber, TciServer, and
// that subscriber still labels everything it receives as receiver 0:
//
//     // Phase 18: RadioModel::rawIqData fires only for RX1 (slice 0).
//     constexpr int kReceiver = 0;   // TciServer.cpp onRawIqDataReceived
//
// Sub-Epic I made that comment false. ReceiverManager gained more than
// one stream, the fork forwarded all of them untagged, and a TCI client
// running iq_start:0 received consecutive blocks from different DDCs on
// different bands under a single receiver header. Every frame is
// individually well-formed, so nothing errors; the time series is just
// quietly spliced from unrelated signals.
//
// This is the recurring shape on this branch: a consumer states its
// precondition in a comment, the producer changes, and nothing connects
// the two. The case below asserts the precondition instead of narrating
// it.
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "models/RadioModel.h"

using namespace NereusSDR;

class TestLegacyIqTapIsStreamZero : public QObject
{
    Q_OBJECT

private slots:

    // ── 1. Stream zero reaches both taps ─────────────────────────────────
    //
    // The single-stream case has to keep working exactly as it did, or
    // this fix trades a multi-slice defect for a single-slice one. Slice A
    // alone is the path most operators are on.
    void stream_zero_reaches_both_taps()
    {
        RadioModel model;
        QSignalSpy untagged(&model, &RadioModel::rawIqData);
        QSignalSpy tagged(&model, &RadioModel::rawIqDataForStream);

        model.forkIqToTapsForTest(0, QVector<float>{1.0F, 2.0F, 3.0F, 4.0F});

        QCOMPARE(untagged.count(), 1);
        QCOMPARE(tagged.count(), 1);
        QCOMPARE(tagged.at(0).at(0).toInt(), 0);
    }

    // ── 2. Other streams reach only the tagged tap ───────────────────────
    //
    // The finding itself. Before the fix both counts were 1 here, which
    // is what corrupted the TCI series.
    void a_nonzero_stream_does_not_reach_the_untagged_tap()
    {
        RadioModel model;
        QSignalSpy untagged(&model, &RadioModel::rawIqData);
        QSignalSpy tagged(&model, &RadioModel::rawIqDataForStream);

        model.forkIqToTapsForTest(1, QVector<float>{5.0F, 6.0F});

        QVERIFY2(untagged.count() == 0,
                 "stream 1 must not reach the untagged tap: TciServer "
                 "labels everything it receives as receiver 0");
        QCOMPARE(tagged.count(), 1);
        QCOMPARE(tagged.at(0).at(0).toInt(), 1);
    }

    // ── 3. A full five-stream radio publishes one untagged frame ─────────
    //
    // The G2 and G2E both carry five user streams. Driving all of them
    // is the shape the bench will actually produce, and it pins the count
    // rather than just the single off-by-one case above.
    void five_live_streams_yield_one_untagged_frame_each_round()
    {
        RadioModel model;
        QSignalSpy untagged(&model, &RadioModel::rawIqData);
        QSignalSpy tagged(&model, &RadioModel::rawIqDataForStream);

        for (int stream = 0; stream < 5; ++stream) {
            model.forkIqToTapsForTest(stream, QVector<float>{1.0F, 1.0F});
        }

        QVERIFY2(untagged.count() == 1,
                 "one untagged frame per round regardless of stream count");
        QCOMPARE(tagged.count(), 5);
    }

    // ── 4. The payload is not altered on its way through ─────────────────
    //
    // The fix gates which taps fire, nothing else. If it ever starts
    // reshaping samples, that is a different and much worse bug.
    void the_samples_pass_through_unchanged()
    {
        RadioModel model;
        QSignalSpy untagged(&model, &RadioModel::rawIqData);

        const QVector<float> payload{0.25F, -0.5F, 0.75F, -1.0F};
        model.forkIqToTapsForTest(0, payload);

        QCOMPARE(untagged.count(), 1);
        QCOMPARE(untagged.at(0).at(0).value<QVector<float>>(), payload);
    }

    // ── 5. A negative index is not stream zero ───────────────────────────
    //
    // -1 is the "no DDC" sentinel used throughout DdcAssignment. It must
    // not fall through an `index != 0` style test into the untagged tap.
    void a_negative_stream_index_reaches_neither_as_stream_zero()
    {
        RadioModel model;
        QSignalSpy untagged(&model, &RadioModel::rawIqData);

        model.forkIqToTapsForTest(-1, QVector<float>{1.0F, 1.0F});

        QVERIFY2(untagged.count() == 0,
                 "-1 is the no-DDC sentinel, not stream zero");
    }
};

QTEST_MAIN(TestLegacyIqTapIsStreamZero)
#include "tst_legacy_iq_tap_is_stream_zero.moc"
