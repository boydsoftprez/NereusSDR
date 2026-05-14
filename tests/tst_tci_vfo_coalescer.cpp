// no-port-check: NereusSDR-original TDD test for TciVfoCoalescer.
// Phase 3J-1 Task 15.1.
//
// Verifies:
//   - update() replaces the previous frame for the same key (latest-wins)
//   - drain emits keys in original arrival order
//   - burst of 200 updates on 2 keys collapses to exactly 2 frames
//   - clear() drops all pending frames without emitting

#include <QtTest>
#include "core/TciVfoCoalescer.h"

using namespace NereusSDR;

class TestTciVfoCoalescer : public QObject {
    Q_OBJECT
private slots:
    void update_replaces_previous_frame_for_same_key();
    void drain_emits_in_arrival_order();
    void burst_of_200_collapses_to_one_per_key();
    void clear_drops_pending_frames();
};

void TestTciVfoCoalescer::update_replaces_previous_frame_for_same_key()
{
    TciVfoCoalescer c;
    c.update(QStringLiteral("vfo:0,0"), QStringLiteral("vfo:0,0,14000000;"));
    c.update(QStringLiteral("vfo:0,0"), QStringLiteral("vfo:0,0,14250000;"));
    c.update(QStringLiteral("vfo:0,0"), QStringLiteral("vfo:0,0,14300000;"));
    QCOMPARE(c.pending(), 1);
    QStringList out;
    c.drainAll(&out);
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first(), QStringLiteral("vfo:0,0,14300000;"));  // latest wins
}

void TestTciVfoCoalescer::drain_emits_in_arrival_order()
{
    TciVfoCoalescer c;
    c.update(QStringLiteral("vfo:1,0"), QStringLiteral("vfo:1,0,7100000;"));
    c.update(QStringLiteral("vfo:0,0"), QStringLiteral("vfo:0,0,14250000;"));
    c.update(QStringLiteral("if:0,0"),  QStringLiteral("if:0,0,0;"));
    QStringList out;
    c.drainAll(&out);
    QCOMPARE(out.size(), 3);
    QCOMPARE(out.at(0), QStringLiteral("vfo:1,0,7100000;"));   // first inserted
    QCOMPARE(out.at(1), QStringLiteral("vfo:0,0,14250000;"));  // second
    QCOMPARE(out.at(2), QStringLiteral("if:0,0,0;"));          // third
}

void TestTciVfoCoalescer::burst_of_200_collapses_to_one_per_key()
{
    // Plan spec: 200 VFO updates → ≤ 1 outbound per coalesce interval.
    // Two distinct keys (rx0/chan0 and rx0/chan1) — should collapse to 2 frames.
    TciVfoCoalescer c;
    for (int i = 0; i < 100; ++i) {
        c.update(QStringLiteral("vfo:0,0"),
                 QStringLiteral("vfo:0,0,%1;").arg(14000000 + i));
        c.update(QStringLiteral("vfo:0,1"),
                 QStringLiteral("vfo:0,1,%1;").arg(7000000 + i));
    }
    QCOMPARE(c.pending(), 2);
    QStringList out;
    c.drainAll(&out);
    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0), QStringLiteral("vfo:0,0,14000099;"));  // 100th update (i=99)
    QCOMPARE(out.at(1), QStringLiteral("vfo:0,1,7000099;"));
}

void TestTciVfoCoalescer::clear_drops_pending_frames()
{
    TciVfoCoalescer c;
    c.update(QStringLiteral("vfo:0,0"), QStringLiteral("vfo:0,0,14250000;"));
    c.update(QStringLiteral("vfo:1,0"), QStringLiteral("vfo:1,0,7100000;"));
    QCOMPARE(c.pending(), 2);
    c.clear();
    QCOMPARE(c.pending(), 0);
    QStringList out;
    c.drainAll(&out);
    QCOMPARE(out.size(), 0);
}

QTEST_GUILESS_MAIN(TestTciVfoCoalescer)
#include "tst_tci_vfo_coalescer.moc"
