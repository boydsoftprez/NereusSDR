// =================================================================
// tests/tst_fft_router_topology.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Task 8-9: per-stream FFT dispatch topology.
//
// Renamed from tst_fft_engine_pool.cpp (R1 Task 6, 2026-08-02): that
// filename now belongs to tests/tst_fft_engine_pool.cpp, which tests the
// real FftEnginePool class (src/core/spectrum/FftEnginePool.h). This file
// was never about that class -- despite the old filename, every test
// below exercises FFTRouter's pan/receiver subscription mapping, which is
// what MainWindow's per-stream FFTEngine pool depended on before
// FftEnginePool existed as a class. Content is otherwise unchanged.
// =================================================================
#include <QtTest/QtTest>
#include "core/FFTRouter.h"

using namespace NereusSDR;

class TestFftRouterTopology : public QObject {
    Q_OBJECT
private slots:
    void distinct_streams_reach_distinct_pans()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-1"), 1);

        QCOMPARE(router.pansForReceiver(0),
                 QList<QString>{QStringLiteral("pan-0")});
        QCOMPARE(router.pansForReceiver(1),
                 QList<QString>{QStringLiteral("pan-1")});
    }

    void slices_sharing_a_stream_share_one_pan_subscription()
    {
        FFTRouter router;
        // Slices A and B both on stream 0, both shown on pan-0. The pan
        // must subscribe once, not twice, or it paints each frame twice.
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);

        QCOMPARE(router.pansForReceiver(0).size(), 1);
    }

    void one_stream_can_feed_several_pans_at_different_zooms()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-1"), 0);

        QCOMPARE(router.pansForReceiver(0).size(), 2);
    }

    void rebuild_drops_stale_subscriptions()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-0"), 1);
        QCOMPARE(router.receiversForPan(QStringLiteral("pan-0")).size(), 2);

        // A full rebuild clears then re-adds only what the slice set says.
        router.removePan(QStringLiteral("pan-0"));
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);

        QCOMPARE(router.receiversForPan(QStringLiteral("pan-0")),
                 QList<int>{0});
    }
};

QTEST_MAIN(TestFftRouterTopology)
#include "tst_fft_router_topology.moc"
