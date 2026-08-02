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

    // Re-subscribing a consumer to a different stream must move it, not clone it.
    void resubscribeMovesTheConsumer()
    {
        FftTopology t;
        t.subscribe("a", 0);
        t.subscribe("a", 1);
        FFTRouter r;
        t.applyTo(r);
        QVERIFY(r.pansForReceiver(0).isEmpty());
        QCOMPARE(r.pansForReceiver(1), QList<QString>{"a"});
    }
};

QTEST_MAIN(TstFftTopology)
#include "tst_fft_topology.moc"
