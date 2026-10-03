// =================================================================
// tests/tst_display_budget_split.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. iPhone app plan Task 76 (R-IOS-31,
// R-MC-17; the several-devices design, ruling 9.3 and design ruling
// 9.3a): the Core's display budget shared among the devices signed in to
// it. Table-driven: the holder whole up to the total; the rest shared
// max-min fair; equal shares with no holder, with the station device
// holding and with an away holder; PureSignal charged once;
// sharedConnection without a governor cut and sharedProcessing with one,
// only while another device is admitted; generations.
//
// Modification history (NereusSDR):
//   2026-09-25 - Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code (R-IOS-31, R-MC-17).
//
// =================================================================

#include <QtTest>

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayBudgetSplit.h"

using namespace NereusSDR;

namespace {

struct Wanted {
    quint64 bytes = 0;
    quint64 samples = 0;
    DisplayBudgetReason reason = DisplayBudgetReason::None;
};

DisplayBudgetSplitDevice device(const char* id, quint64 bytes, quint64 samples)
{
    DisplayBudgetSplitDevice d;
    d.id = QByteArray(id);
    d.request = {bytes, samples, 0};
    return d;
}

const DisplayBudgetLimits kTotal{1'000'000, 100'000, 7};

} // namespace

Q_DECLARE_METATYPE(DisplayBudgetSplitInput)
Q_DECLARE_METATYPE(QList<Wanted>)

class TstDisplayBudgetSplit : public QObject {
    Q_OBJECT

private slots:
    void shares_data();
    void shares();
    void reasonNamesWhichLimitIsShort();
    void anUnchangedShareKeepsItsGeneration();
    void aChangedShareMovesItsGenerationOn();
    void aLoneDeviceFollowsTheTotalsGenerations();
    void pureSignalIsChargedOnceToItsSubscriber();
    void aShareNeverReachesZero();
    void olderDevicesHearTheReasonsTheyKnow();
    void theNewReasonsCrossTheWire();
    void everyDeviceAsksForAtLeastOneUsefulPan();
};

void TstDisplayBudgetSplit::shares_data()
{
    QTest::addColumn<DisplayBudgetSplitInput>("input");
    QTest::addColumn<QList<Wanted>>("wanted");

    const auto sc = DisplayBudgetReason::SharedConnection;
    const auto none = DisplayBudgetReason::None;

    {
        DisplayBudgetSplitInput in{kTotal, false, {device("a", 1'000'000, 100'000)}};
        QTest::newRow("alone: the whole total") << in
            << QList<Wanted>{{1'000'000, 100'000, none}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 1'000'000, 100'000),
                                    device("b", 1'000'000, 100'000)}};
        QTest::newRow("no holder: equal halves") << in
            << QList<Wanted>{{500'000, 50'000, sc}, {500'000, 50'000, sc}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 1'000'000, 100'000),
                                    device("b", 1'000'000, 100'000),
                                    device("c", 1'000'000, 100'000)}};
        // 1'000'000 / 3: the remainder falls to the device served last.
        QTest::newRow("no holder: equal thirds") << in
            << QList<Wanted>{{333'333, 33'333, sc}, {333'333, 33'333, sc},
                             {333'334, 33'334, sc}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 100'000, 10'000),
                                    device("b", 1'000'000, 100'000),
                                    device("c", 1'000'000, 100'000)}};
        // a asks for less than a third, so what it leaves goes to b and c.
        QTest::newRow("max-min fair: a small request leaves the rest") << in
            << QList<Wanted>{{100'000, 10'000, none}, {450'000, 45'000, sc},
                             {450'000, 45'000, sc}};
    }
    {
        // Fix wave I5: what nobody asks for is shared equally as headroom.
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 100'000, 10'000), device("b", 200'000, 30'000)}};
        QTest::newRow("headroom: what nobody asks for is shared") << in
            << QList<Wanted>{{450'000, 40'000, none}, {550'000, 60'000, none}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false, {device("a", 0, 0)}};
        QTest::newRow("alone asking for nothing: the whole total") << in
            << QList<Wanted>{{1'000'000, 100'000, none}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 1'000'000, 100'000),
                                    device("h", 600'000, 40'000),
                                    device("c", 1'000'000, 100'000)},
                                   DisplayBudgetHolderKind::Device, QByteArray("h")};
        QTest::newRow("a network holder whole, the rest shared") << in
            << QList<Wanted>{{200'000, 30'000, sc}, {600'000, 40'000, none},
                             {200'000, 30'000, sc}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("h", 5'000'000, 500'000),
                                    device("b", 1'000'000, 100'000)},
                                   DisplayBudgetHolderKind::Device, QByteArray("h")};
        // Up to the total: the other is left with the floor of 1.
        QTest::newRow("a holder asking past the total gets the total") << in
            << QList<Wanted>{{1'000'000, 100'000, sc}, {1, 1, sc}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 1'000'000, 100'000),
                                    device("b", 1'000'000, 100'000)},
                                   DisplayBudgetHolderKind::Station};
        QTest::newRow("the station device holding: equal shares") << in
            << QList<Wanted>{{500'000, 50'000, sc}, {500'000, 50'000, sc}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("h", 600'000, 40'000),
                                    device("b", 1'000'000, 100'000)},
                                   DisplayBudgetHolderKind::Device, QByteArray("h"), true};
        QTest::newRow("an away holder: equal max-min fair shares") << in
            << QList<Wanted>{{500'000, 40'000, sc}, {500'000, 60'000, sc}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("h", 1'000'000, 100'000)},
                                   DisplayBudgetHolderKind::Device, QByteArray("h")};
        QTest::newRow("a holder alone") << in
            << QList<Wanted>{{1'000'000, 100'000, none}};
    }
    {
        DisplayBudgetSplitInput in{kTotal, false,
                                   {device("a", 1'000'000, 100'000),
                                    device("b", 1'000'000, 100'000)},
                                   DisplayBudgetHolderKind::Device, QByteArray("gone")};
        QTest::newRow("a holder not among the devices counts as none") << in
            << QList<Wanted>{{500'000, 50'000, sc}, {500'000, 50'000, sc}};
    }
}

void TstDisplayBudgetSplit::shares()
{
    QFETCH(DisplayBudgetSplitInput, input);
    QFETCH(QList<Wanted>, wanted);
    const QList<DisplayBudgetShare> shares = DisplayBudgetSplit::split(input);
    QCOMPARE(shares.size(), wanted.size());
    quint64 bytes = 0;
    quint64 samples = 0;
    for (qsizetype i = 0; i < shares.size(); ++i) {
        QCOMPARE(shares.at(i).id, input.devices.at(i).id);
        QCOMPARE(shares.at(i).limits.applicationBytesPerSecond, wanted.at(i).bytes);
        QCOMPARE(shares.at(i).limits.spectrumSampleUnitsPerSecond, wanted.at(i).samples);
        QCOMPARE(shares.at(i).reason, wanted.at(i).reason);
        QVERIFY(shares.at(i).limits.isValid());
        bytes += shares.at(i).limits.applicationBytesPerSecond;
        samples += shares.at(i).limits.spectrumSampleUnitsPerSecond;
    }
    // Never more than the total, apart from the floor of 1 a starved device
    // keeps so its budget stays a budget.
    QVERIFY(bytes <= input.total.applicationBytesPerSecond + static_cast<quint64>(shares.size()));
    QVERIFY(samples
            <= input.total.spectrumSampleUnitsPerSecond + static_cast<quint64>(shares.size()));
}

void TstDisplayBudgetSplit::reasonNamesWhichLimitIsShort()
{
    // Alone: the total's own reason, never a shared one.
    DisplayBudgetSplitInput alone{kTotal, false, {device("a", 2'000'000, 200'000)}};
    QCOMPARE(DisplayBudgetSplit::split(alone).first().reason, DisplayBudgetReason::None);
    alone.governorCut = true;
    QCOMPARE(DisplayBudgetSplit::split(alone).first().reason, DisplayBudgetReason::CoreBusy);

    // Another device admitted and the share below the request.
    DisplayBudgetSplitInput two{kTotal, false,
                                {device("a", 1'000'000, 100'000),
                                 device("b", 1'000'000, 100'000)}};
    for (const DisplayBudgetShare& share : DisplayBudgetSplit::split(two)) {
        QCOMPARE(share.reason, DisplayBudgetReason::SharedConnection);
    }
    two.governorCut = true;
    for (const DisplayBudgetShare& share : DisplayBudgetSplit::split(two)) {
        QCOMPARE(share.reason, DisplayBudgetReason::SharedProcessing);
    }

    // Another device admitted but the share covers the request: the total's
    // own reason.
    DisplayBudgetSplitInput small{kTotal, true,
                                  {device("a", 100, 10), device("b", 100, 10)}};
    for (const DisplayBudgetShare& share : DisplayBudgetSplit::split(small)) {
        QCOMPARE(share.reason, DisplayBudgetReason::CoreBusy);
    }
    small.governorCut = false;
    for (const DisplayBudgetShare& share : DisplayBudgetSplit::split(small)) {
        QCOMPARE(share.reason, DisplayBudgetReason::None);
    }
}

void TstDisplayBudgetSplit::anUnchangedShareKeepsItsGeneration()
{
    DisplayBudgetSplitInput in{DisplayBudgetLimits{1'000'000, 100'000, 40},
                               false,
                               {device("a", 1'000'000, 100'000),
                                device("b", 1'000'000, 100'000)}};
    in.devices[0].previous = DisplayBudgetLimits{500'000, 50'000, 12};
    in.devices[0].previousReason = DisplayBudgetReason::SharedConnection;
    const QList<DisplayBudgetShare> shares = DisplayBudgetSplit::split(in);
    QCOMPARE(shares.at(0).limits.generation, 12u);
    // b was never published: the total's generation.
    QCOMPARE(shares.at(1).limits.generation, 40u);
}

void TstDisplayBudgetSplit::aChangedShareMovesItsGenerationOn()
{
    // The total's generation is not newer than a's last: a moves on by one.
    DisplayBudgetSplitInput in{DisplayBudgetLimits{1'000'000, 100'000, 3},
                               false,
                               {device("a", 1'000'000, 100'000),
                                device("b", 1'000'000, 100'000)}};
    in.devices[0].previous = DisplayBudgetLimits{1'000'000, 100'000, 9};
    in.devices[0].previousReason = DisplayBudgetReason::None;
    QCOMPARE(DisplayBudgetSplit::split(in).at(0).limits.generation, 10u);

    // A reason change alone is a change.
    in.devices[0].previous = DisplayBudgetLimits{500'000, 50'000, 9};
    in.devices[0].previousReason = DisplayBudgetReason::SharedProcessing;
    QCOMPARE(DisplayBudgetSplit::split(in).at(0).limits.generation, 10u);

    // A newer total's generation is taken as it is.
    in.total.generation = 12;
    QCOMPARE(DisplayBudgetSplit::split(in).at(0).limits.generation, 12u);

    // Across the wrap of the number space, generation 0 is skipped.
    in.total.generation = 0xFFFFFF00u;
    in.devices[0].previous = DisplayBudgetLimits{1, 1, 0xFFFFFFFFu};
    QCOMPARE(DisplayBudgetSplit::split(in).at(0).limits.generation, 1u);
}

void TstDisplayBudgetSplit::aLoneDeviceFollowsTheTotalsGenerations()
{
    // Before shares existed a device saw the total's generations; alone on
    // the Core it still does, step for step.
    DisplayBudgetSplitInput in{DisplayBudgetLimits{1'000'000, 100'000, 5}, false,
                               {device("a", 1'000'000, 100'000)}};
    DisplayBudgetShare share = DisplayBudgetSplit::split(in).first();
    QCOMPARE(share.limits, (DisplayBudgetLimits{1'000'000, 100'000, 5}));
    for (quint32 generation = 6; generation < 10; ++generation) {
        in.total = {in.total.applicationBytesPerSecond / 2,
                    in.total.spectrumSampleUnitsPerSecond / 2, generation};
        in.governorCut = true;
        in.devices[0].previous = share.limits;
        in.devices[0].previousReason = share.reason;
        in.devices[0].request = {in.total.applicationBytesPerSecond,
                                 in.total.spectrumSampleUnitsPerSecond, 0};
        share = DisplayBudgetSplit::split(in).first();
        QCOMPARE(share.limits, in.total);
        QCOMPARE(share.reason, DisplayBudgetReason::CoreBusy);
    }
}

void TstDisplayBudgetSplit::pureSignalIsChargedOnceToItsSubscriber()
{
    const DisplayBudgetCharge ps3 = ps3DisplayCharge();
    QVERIFY(ps3.applicationBytesPerSecond > 0);
    const DisplayBudgetLimits total{ps3.applicationBytesPerSecond + 800'000, 100'000, 1};
    DisplayBudgetSplitInput in{total, false,
                               {device("a", total.applicationBytesPerSecond, 100'000),
                                device("b", total.applicationBytesPerSecond, 100'000)}};
    in.ps3Subscriber = QByteArray("b");
    const QList<DisplayBudgetShare> shares = DisplayBudgetSplit::split(in);
    // Its charge comes off the top once and goes to b alone; the rest is
    // shared equally.
    QCOMPARE(shares.at(0).limits.applicationBytesPerSecond, quint64{400'000});
    QCOMPARE(shares.at(1).limits.applicationBytesPerSecond,
             ps3.applicationBytesPerSecond + 400'000);
    QCOMPARE(shares.at(0).limits.applicationBytesPerSecond
                 + shares.at(1).limits.applicationBytesPerSecond,
             total.applicationBytesPerSecond);

    // With the subscriber alone it is the whole total, as before shares.
    DisplayBudgetSplitInput alone{total, false,
                                  {device("b", total.applicationBytesPerSecond, 100'000)}};
    alone.ps3Subscriber = QByteArray("b");
    QCOMPARE(DisplayBudgetSplit::split(alone).first().limits.applicationBytesPerSecond,
             total.applicationBytesPerSecond);
}

void TstDisplayBudgetSplit::aShareNeverReachesZero()
{
    DisplayBudgetSplitInput in{DisplayBudgetLimits{3, 3, 1}, false,
                               {device("a", 3, 3), device("b", 3, 3), device("c", 3, 3),
                                device("d", 3, 3)}};
    for (const DisplayBudgetShare& share : DisplayBudgetSplit::split(in)) {
        QVERIFY(share.limits.isValid());
    }
    QVERIFY(DisplayBudgetSplit::split({DisplayBudgetLimits{}, false, {device("a", 1, 1)}})
                .isEmpty());
    QVERIFY(DisplayBudgetSplit::split({kTotal, false, {}}).isEmpty());
}

void TstDisplayBudgetSplit::olderDevicesHearTheReasonsTheyKnow()
{
    QCOMPARE(displayBudgetReasonForOlderDevice(DisplayBudgetReason::SharedProcessing),
             DisplayBudgetReason::CoreBusy);
    QCOMPARE(displayBudgetReasonForOlderDevice(DisplayBudgetReason::SharedConnection),
             DisplayBudgetReason::None);
    QCOMPARE(displayBudgetReasonForOlderDevice(DisplayBudgetReason::CoreBusy),
             DisplayBudgetReason::CoreBusy);
    QCOMPARE(displayBudgetReasonForOlderDevice(DisplayBudgetReason::None),
             DisplayBudgetReason::None);
}

void TstDisplayBudgetSplit::theNewReasonsCrossTheWire()
{
    for (DisplayBudgetReason reason :
         {DisplayBudgetReason::None, DisplayBudgetReason::CoreBusy,
          DisplayBudgetReason::SharedConnection, DisplayBudgetReason::SharedProcessing}) {
        QCOMPARE(displayBudgetReasonFromWireName(displayBudgetReasonWireName(reason)), reason);
    }
    QCOMPARE(displayBudgetReasonWireName(DisplayBudgetReason::SharedConnection),
             QStringLiteral("sharedConnection"));
    QCOMPARE(displayBudgetReasonWireName(DisplayBudgetReason::SharedProcessing),
             QStringLiteral("sharedProcessing"));
}

void TstDisplayBudgetSplit::everyDeviceAsksForAtLeastOneUsefulPan()
{
    // Fix wave 2 (Critical 1): a newcomer that has not subscribed yet is
    // counted as asking for one useful pan, beside a device asking for the
    // whole total.
    const DisplayBudgetCharge pan{20'000, 2'000, 0};
    DisplayBudgetSplitInput in{kTotal, false,
                               {device("a", 1'000'000, 100'000), device("b", 0, 0)}};
    in.minimumRequest = pan;
    QList<DisplayBudgetShare> shares = DisplayBudgetSplit::split(in);
    QCOMPARE(shares.at(0).limits.applicationBytesPerSecond, quint64{980'000});
    QCOMPARE(shares.at(0).limits.spectrumSampleUnitsPerSecond, quint64{98'000});
    QCOMPARE(shares.at(0).reason, DisplayBudgetReason::SharedConnection);
    QCOMPARE(shares.at(1).limits.applicationBytesPerSecond, quint64{20'000});
    QCOMPARE(shares.at(1).limits.spectrumSampleUnitsPerSecond, quint64{2'000});
    QCOMPARE(shares.at(1).reason, DisplayBudgetReason::None);

    // Once it asks for the whole total too: equal halves.
    in.devices[1].request = {1'000'000, 100'000, 0};
    shares = DisplayBudgetSplit::split(in);
    QCOMPARE(shares.at(1).limits.applicationBytesPerSecond, quint64{500'000});
    QCOMPARE(shares.at(0).limits.applicationBytesPerSecond, quint64{500'000});

    // Alone, asking for nothing: still the whole total.
    DisplayBudgetSplitInput alone{kTotal, false, {device("a", 0, 0)}};
    alone.minimumRequest = pan;
    QCOMPARE(DisplayBudgetSplit::split(alone).first().limits, kTotal);

    // Rule 1 still keeps a present holder whole: the newcomer's floor comes
    // from what the holder leaves.
    DisplayBudgetSplitInput held{kTotal, false,
                                 {device("h", 1'000'000, 100'000), device("b", 0, 0)},
                                 DisplayBudgetHolderKind::Device, QByteArray("h")};
    held.minimumRequest = pan;
    shares = DisplayBudgetSplit::split(held);
    QCOMPARE(shares.at(0).limits.applicationBytesPerSecond, quint64{1'000'000});
    QCOMPARE(shares.at(0).limits.spectrumSampleUnitsPerSecond, quint64{100'000});
    QCOMPARE(shares.at(0).reason, DisplayBudgetReason::None);
    QCOMPARE(shares.at(1).limits.applicationBytesPerSecond, quint64{1});
    QCOMPARE(shares.at(1).reason, DisplayBudgetReason::SharedConnection);
    // A holder asking for less leaves the newcomer at least its floor.
    held.devices[0].request = {900'000, 90'000, 0};
    shares = DisplayBudgetSplit::split(held);
    QVERIFY(shares.at(0).limits.applicationBytesPerSecond >= 900'000);
    QVERIFY(shares.at(1).limits.applicationBytesPerSecond >= pan.applicationBytesPerSecond);
    QVERIFY(shares.at(1).limits.spectrumSampleUnitsPerSecond >= pan.spectrumSampleUnitsPerSecond);
}

QTEST_APPLESS_MAIN(TstDisplayBudgetSplit)
#include "tst_display_budget_split.moc"
