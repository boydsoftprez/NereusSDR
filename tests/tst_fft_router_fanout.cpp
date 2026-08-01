// =================================================================
// tests/tst_fft_router_fanout.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 7: FFTRouter receiver-to-pan fan-out.
// =================================================================
#include <QtTest/QtTest>
#include "core/FFTRouter.h"

using namespace NereusSDR;

class TestFFTRouterFanout : public QObject {
    Q_OBJECT
private slots:
    void map_pan_to_receiver_round_trips()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 2);
        QList<QString> pans = router.pansForReceiver(2);
        QVERIFY(pans.contains(QStringLiteral("pan-0")));
    }

    void multiple_pans_can_subscribe_same_receiver()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 2);
        router.mapPanToReceiver(QStringLiteral("pan-1"), 2);
        QList<QString> pans = router.pansForReceiver(2);
        QCOMPARE(pans.size(), 2);
    }

    void remove_pan_unsubscribes_from_all_receivers()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 2);
        router.removePan(QStringLiteral("pan-0"));
        QList<QString> pans = router.pansForReceiver(2);
        QVERIFY(pans.isEmpty());
    }
};

QTEST_MAIN(TestFFTRouterFanout)
#include "tst_fft_router_fanout.moc"
