// =================================================================
// tests/tst_remote_display_allocator.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original tests for GUI display-quality allocation.
//
// =================================================================

#include <QtTest>

#include "core/session/media/DisplayBudget.h"
#include "gui/RemoteDisplayAllocator.h"

#include <utility>

using namespace NereusSDR;

namespace {

RemoteDisplayIntent pan(QString id, int pixels, int fps, bool active = false,
                        bool wide = false, int periodMs = 1'000)
{
    return {std::move(id), pixels, fps, wide, periodMs, active};
}

DisplayBudgetLimits limitsFor(const QList<RemoteDisplayIntent>& intents)
{
    QList<DisplayBudgetCharge> charges;
    for (const RemoteDisplayIntent& intent : intents) {
        charges.append(spectrumDisplayCost(intent.pixels, intent.fps,
                                           intent.includeWidePlane)->charge);
    }
    const DisplayBudgetCharge total = *sumDisplayCharges(charges);
    return {total.applicationBytesPerSecond, total.spectrumSampleUnitsPerSecond, 1};
}

const RemoteDisplayQuality& quality(const RemoteDisplayAllocation& allocation, const QString& id)
{
    for (const RemoteDisplayQuality& item : allocation.pans) {
        if (item.panId == id) {
            return item;
        }
    }
    Q_UNREACHABLE();
    return allocation.pans.first();
}

} // namespace

class TestRemoteDisplayAllocator : public QObject {
    Q_OBJECT

private slots:
    void mini_yields_endpoint_and_budget_to_pans()
    {
        QList<RemoteDisplayIntent> intents;
        for (int i = 0; i < 8; ++i) {
            intents.append(pan(QStringLiteral("pan-%1").arg(i), 512, 30, i == 0));
        }
        RemoteDisplayIntent mini = pan(QStringLiteral("a-mini"), 1024, 30);
        mini.kind = RemoteDisplayIntent::Kind::Mini;
        intents.append(mini);
        const auto endpointBound = allocateRemoteDisplay(
            {100'000'000, 100'000'000, 1}, intents, false);
        QVERIFY(endpointBound);
        QVERIFY(quality(*endpointBound, QStringLiteral("a-mini")).suspended);
        for (int i = 0; i < 8; ++i) {
            QVERIFY(!quality(*endpointBound, QStringLiteral("pan-%1").arg(i)).suspended);
        }

        const auto onePanCost = spectrumDisplayCost(512, 30, false)->charge;
        const auto budgetBound = allocateRemoteDisplay(
            {onePanCost.applicationBytesPerSecond,
             onePanCost.spectrumSampleUnitsPerSecond, 1},
            {pan(QStringLiteral("pan"), 512, 30, true), mini}, false);
        QVERIFY(budgetBound);
        QCOMPARE(quality(*budgetBound, QStringLiteral("pan")).fps, 30);
        QCOMPARE(quality(*budgetBound, QStringLiteral("pan")).pixels, 512);
        QVERIFY(quality(*budgetBound, QStringLiteral("a-mini")).suspended);
    }

    void preservesRequestedQualityWhenItFits()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("z"), 1'024, 30),
            pan(QStringLiteral("a"), 512, 20, true, true, 333),
        };
        const auto allocation = allocateRemoteDisplay(limitsFor(intents), intents, false);
        QVERIFY(allocation);
        QCOMPARE(allocation->pans.size(), 2);
        QCOMPARE(allocation->pans[0].panId, QStringLiteral("a"));
        QCOMPARE(quality(*allocation, QStringLiteral("a")).pixels, 512);
        QCOMPARE(quality(*allocation, QStringLiteral("a")).fps, 20);
        QCOMPARE(quality(*allocation, QStringLiteral("a")).framesPerLine, 7);
        QCOMPARE(quality(*allocation, QStringLiteral("z")).pixels, 1'024);
        QCOMPARE(quality(*allocation, QStringLiteral("z")).fps, 30);
    }

    void backgroundFpsUsesStableFairRoundsBeforePixels()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("b"), 1'000, 20),
            pan(QStringLiteral("a"), 1'000, 20),
            pan(QStringLiteral("active"), 1'000, 20, true),
        };
        const DisplayBudgetCharge oneLess = spectrumDisplayCost(1'000, 19, false)->charge;
        const DisplayBudgetCharge full = spectrumDisplayCost(1'000, 20, false)->charge;
        const DisplayBudgetLimits cap{full.applicationBytesPerSecond
                                          + oneLess.applicationBytesPerSecond * 2,
                                      full.spectrumSampleUnitsPerSecond
                                          + oneLess.spectrumSampleUnitsPerSecond * 2, 1};
        const auto allocation = allocateRemoteDisplay(cap, intents, false);
        QVERIFY(allocation);
        QCOMPARE(quality(*allocation, QStringLiteral("a")).fps, 19);
        QCOMPARE(quality(*allocation, QStringLiteral("b")).fps, 19);
        QCOMPARE(quality(*allocation, QStringLiteral("active")).fps, 20);
        QCOMPARE(quality(*allocation, QStringLiteral("a")).pixels, 1'000);
        QCOMPARE(quality(*allocation, QStringLiteral("b")).pixels, 1'000);
    }

    void reducesBackgroundPixelsBeforeActiveFps()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("background"), 300, 10),
            pan(QStringLiteral("active"), 300, 20, true),
        };
        const DisplayBudgetCharge backgroundReduced = spectrumDisplayCost(299, 10, false)->charge;
        const DisplayBudgetCharge activeFull = spectrumDisplayCost(300, 20, false)->charge;
        const DisplayBudgetLimits cap{backgroundReduced.applicationBytesPerSecond
                                          + activeFull.applicationBytesPerSecond,
                                      backgroundReduced.spectrumSampleUnitsPerSecond
                                          + activeFull.spectrumSampleUnitsPerSecond, 1};
        const auto allocation = allocateRemoteDisplay(cap, intents, false);
        QVERIFY(allocation);
        QCOMPARE(quality(*allocation, QStringLiteral("background")).pixels, 299);
        QCOMPARE(quality(*allocation, QStringLiteral("background")).fps, 10);
        QCOMPARE(quality(*allocation, QStringLiteral("active")).pixels, 300);
        QCOMPARE(quality(*allocation, QStringLiteral("active")).fps, 20);
    }

    void suspendsHighestBackgroundThenRecalculatesOriginalQuality()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("a"), 256, 10),
            pan(QStringLiteral("b"), 256, 10),
            pan(QStringLiteral("active"), 256, 10, true),
        };
        const DisplayBudgetCharge one = spectrumDisplayCost(256, 10, false)->charge;
        const DisplayBudgetLimits cap{one.applicationBytesPerSecond * 2,
                                      one.spectrumSampleUnitsPerSecond * 2, 1};
        const auto allocation = allocateRemoteDisplay(cap, intents, false);
        QVERIFY(allocation);
        QVERIFY(!quality(*allocation, QStringLiteral("a")).suspended);
        QVERIFY(quality(*allocation, QStringLiteral("b")).suspended);
        QVERIFY(!quality(*allocation, QStringLiteral("active")).suspended);
        QCOMPARE(quality(*allocation, QStringLiteral("a")).pixels, 256);
        QCOMPARE(quality(*allocation, QStringLiteral("active")).fps, 10);
    }

    void recomputesOriginalIntentWhenHeadroomReturns()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("background"), 1'000, 30),
            pan(QStringLiteral("active"), 1'000, 30, true),
        };
        const DisplayBudgetCharge floor = spectrumDisplayCost(256, 10, false)->charge;
        const auto constrained = allocateRemoteDisplay(
            {floor.applicationBytesPerSecond * 2, floor.spectrumSampleUnitsPerSecond * 2, 1},
            intents, false);
        QVERIFY(constrained);
        QVERIFY(quality(*constrained, QStringLiteral("background")).pixels < 1'000);

        const auto recovered = allocateRemoteDisplay(limitsFor(intents), intents, false);
        QVERIFY(recovered);
        QCOMPARE(quality(*recovered, QStringLiteral("background")).pixels, 1'000);
        QCOMPARE(quality(*recovered, QStringLiteral("background")).fps, 30);
    }

    void suspension_recalculates_survivors_and_impossible_active_pan_pauses()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("background"), 256, 10),
            pan(QStringLiteral("active"), 300, 20, true),
        };
        // Both floors need 10,240 sample units/s. Only one pan fits; the
        // remaining active pan must regain 300 pixels and 15 FPS from its
        // original intent, instead of retaining the earlier 256 x 10 floor.
        const DisplayBudgetLimits cap{100000, 9000, 1};
        const auto allocation = allocateRemoteDisplay(cap, intents, false);
        QVERIFY(allocation);
        QVERIFY(quality(*allocation, QStringLiteral("background")).suspended);
        QCOMPARE(quality(*allocation, QStringLiteral("active")).pixels, 300);
        QCOMPARE(quality(*allocation, QStringLiteral("active")).fps, 15);
        const auto paused = allocateRemoteDisplay({1, 1, 1}, intents, false);
        QVERIFY(paused);
        for (const auto& item : paused->pans) {
            QVERIFY(item.suspended);
            QCOMPARE(item.charge, DisplayBudgetCharge{});
        }
    }

    void structural_message_limit_applies_even_with_abundant_bytes_and_samples()
    {
        QList<RemoteDisplayIntent> intents;
        for (int i = 0; i < 8; ++i) {
            intents.append(pan(QString::number(i), 512, 30, i == 0));
        }
        const auto allocation = allocateRemoteDisplay({10000000, 10000000, 1}, intents, false);
        QVERIFY(allocation);
        QCOMPARE(allocation->total.messagesPerSecond, quint32{200});
        QCOMPARE(quality(*allocation, QStringLiteral("0")).fps, 30);
        for (const auto& item : allocation->pans) { QCOMPARE(item.pixels, 512); }
    }

    void reservesPs3AndRefusesItWhenItCannotFit()
    {
        const RemoteDisplayIntent intent = pan(QStringLiteral("a"), 256, 10);
        const DisplayBudgetCharge spectrum = spectrumDisplayCost(256, 10, false)->charge;
        const DisplayBudgetCharge ps3 = ps3DisplayCharge();
        const DisplayBudgetLimits coexist{ps3.applicationBytesPerSecond
                                             + spectrum.applicationBytesPerSecond,
                                          spectrum.spectrumSampleUnitsPerSecond, 1};
        const auto allocation = allocateRemoteDisplay(coexist, {intent}, true);
        QVERIFY(allocation);
        QCOMPARE(allocation->total.applicationBytesPerSecond,
                 coexist.applicationBytesPerSecond);

        QString error;
        QVERIFY(!allocateRemoteDisplay({ps3.applicationBytesPerSecond - 1, 1, 1},
                                       {}, true, &error));
        QVERIFY(error.contains(QStringLiteral("PureSignal")));
    }

    void chargesWidePlaneAndRejectsInvalidInput()
    {
        const RemoteDisplayIntent narrow = pan(QStringLiteral("narrow"), 256, 10);
        const RemoteDisplayIntent wide = pan(QStringLiteral("wide"), 256, 10, false, true);
        const DisplayBudgetCharge narrowCharge = spectrumDisplayCost(256, 10, false)->charge;
        const DisplayBudgetCharge wideCharge = spectrumDisplayCost(256, 10, true)->charge;
        const auto allocation = allocateRemoteDisplay(
            {wideCharge.applicationBytesPerSecond, wideCharge.spectrumSampleUnitsPerSecond, 1},
            {wide}, false);
        QVERIFY(allocation);
        QCOMPARE(allocation->total, wideCharge);
        QVERIFY(allocation->total.applicationBytesPerSecond > narrowCharge.applicationBytesPerSecond);

        QString error;
        QVERIFY(!allocateRemoteDisplay(limitsFor({narrow}),
                                       {pan(QStringLiteral("bad"), 256, 10, false, false, 65'536)},
                                       false, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!allocateRemoteDisplay(limitsFor({narrow}),
                                       {narrow, pan(QStringLiteral("narrow"), 256, 10)}, false));
        QVERIFY(!allocateRemoteDisplay(limitsFor({narrow}),
                                       {pan(QStringLiteral("active-a"), 256, 10, true),
                                        pan(QStringLiteral("active-b"), 256, 10, true)}, false));

        const RemoteDisplayIntent longPeriod = pan(QStringLiteral("long-period"), 256, 60,
                                                    false, false, 65'535);
        const auto longPeriodAllocation = allocateRemoteDisplay(limitsFor({longPeriod}),
                                                                 {longPeriod}, false);
        QVERIFY(longPeriodAllocation);
        QCOMPARE(quality(*longPeriodAllocation, QStringLiteral("long-period")).framesPerLine,
                 3'933);
    }

    void rawIqKeepsVisiblePanFloorAndReducesFpsBeforePixels()
    {
        const QList<RemoteDisplayIntent> intents{
            pan(QStringLiteral("background"), 1024, 30),
            pan(QStringLiteral("active"), 1024, 30, true),
        };
        const auto once = spectrumDisplayCost(1024, 1, false)->charge;
        const DisplayBudgetLimits cap{2 * once.applicationBytesPerSecond,
                                      2 * once.spectrumSampleUnitsPerSecond, 1};
        const auto allocation = allocateRemoteDisplay(cap, intents, false, nullptr, true);
        QVERIFY(allocation);
        for (const auto& item : allocation->pans) {
            QVERIFY(!item.suspended);
            QCOMPARE(item.fps, 1);
            QCOMPARE(item.pixels, 1024);
        }
        const auto floor = spectrumDisplayCost(1, 1, false)->charge;
        QString error;
        QVERIFY(!allocateRemoteDisplay(
            {2 * floor.applicationBytesPerSecond - 1,
             2 * floor.spectrumSampleUnitsPerSecond, 1},
            intents, false, &error, true));
        QVERIFY(error.contains(QStringLiteral("one frame per second")));
    }
};

QTEST_GUILESS_MAIN(TestRemoteDisplayAllocator)

#include "tst_remote_display_allocator.moc"
