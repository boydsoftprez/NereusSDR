// =================================================================
// tests/tst_fft_topology.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R1 Task 7: FftTopology's own subscription algebra (subscribe /
// unsubscribe / applyTo), extracted from the router-mutation half of
// MainWindow::rebuildFftRouting. Distinct from tst_fft_router_fanout.cpp
// and tst_fft_router_topology.cpp, which pin FFTRouter's own pan/receiver
// mapping contract directly and do not exercise FftTopology at all.
//
// Fix round 1 (coordinator spec review, finding 1): FftTopology widened
// from one stream per consumer to many. subscribingToASecondStreamAdds
// replaces the original resubscribeMovesTheConsumer, which asserted the
// now-superseded one-stream-per-consumer "moves, not clones" behavior;
// the three tests after it are new coverage for the widened contract.
// =================================================================
#include <QtTest>
#include "core/spectrum/FftTopology.h"
#include "core/FFTRouter.h"

using namespace NereusSDR;

class TstFftTopology : public QObject {
    Q_OBJECT
private slots:
    void manyConsumersOnOneStream()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("b", 0);
        t.subscribe("c", 1);
        FFTRouter r;
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0).size(), 2);
        QCOMPARE(r.pansForReceiver(1).size(), 1);
    }

    void unsubscribeRemovesOnlyThatConsumer()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("b", 0);
        t.unsubscribe("a");
        FFTRouter r;
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0), QList<QString>{"b"});
    }

    // applyTo must be a full rebuild, so a stale mapping cannot survive.
    void applyToIsIdempotentAndAuthoritative()
    {
        FftTopology t;
        t.subscribe("a", 0);
        FFTRouter r;
        t.applyTo(r);
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0), QList<QString>{"a"});
        t.unsubscribe("a");
        t.applyTo(r);
        QVERIFY(r.pansForReceiver(0).isEmpty());
    }

    // Fix round 1, finding 1: a consumer can hold more than one stream at
    // once. Subscribing a second, different stream adds to the set rather
    // than replacing what was there -- superseding this file's original
    // resubscribeMovesTheConsumer, which asserted the opposite under the
    // one-stream-per-consumer design finding 1 widened away from.
    void subscribingToASecondStreamAdds()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("a", 1);
        FFTRouter r;
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0), QList<QString>{"a"});
        QCOMPARE(r.pansForReceiver(1), QList<QString>{"a"});
    }

    // Fix round 1: unsubscribe(consumer, oneStream) drops only that one
    // stream, leaving any other stream the consumer holds untouched.
    void unsubscribeOneStreamLeavesTheOther()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("a", 1);
        t.unsubscribe("a", 0);
        FFTRouter r;
        t.applyTo(r);
        QVERIFY(r.pansForReceiver(0).isEmpty());
        QCOMPARE(r.pansForReceiver(1), QList<QString>{"a"});
    }

    // Fix round 1: the no-stream-argument unsubscribe(consumer) still
    // drops every stream a consumer holds, not just one of them.
    void unsubscribeConsumerDropsAllItsStreams()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("a", 1);
        t.unsubscribe("a");
        FFTRouter r;
        t.applyTo(r);
        QVERIFY(r.pansForReceiver(0).isEmpty());
        QVERIFY(r.pansForReceiver(1).isEmpty());
    }

    // Fix round 1: subscribing an already-held (consumer, stream) pair a
    // second time must not duplicate it.
    void resubscribingTheSamePairDoesNotDuplicate()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("a", 0);
        FFTRouter r;
        t.applyTo(r);
        QCOMPARE(r.pansForReceiver(0).size(), 1);
    }
};

QTEST_MAIN(TstFftTopology)
#include "tst_fft_topology.moc"
