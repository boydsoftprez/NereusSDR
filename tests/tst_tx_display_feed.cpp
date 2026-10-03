// =================================================================
// tests/tst_tx_display_feed.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// Remote-window parity Task 28 (R-R3-49, A11): TxDisplayFeed, the one owner
// of the transmit analyzer's view, and TxAnalyzer::clampViewToBaseband, the
// rule every viewer's view goes through. The feed runs the analyzer on every
// key (viewers or not); the governing viewer (the local one, else the lowest
// id) sets its window and pixel count; while keyed the carrier follows the
// transmit slice's frequency and XIT (Thetis console.cs:22138-22150
// [v2.10.3.15], "xit, only when txing").
//
// The Core keys through its own MoxController with the receive-only MOX
// pre-check lifted, as tst_remote_peripherals does; no radio is attached and
// nothing reaches one.
//
// Modification history (NereusSDR):
//   2026-09-26 : Created for parity Task 28 by J.J. Boyd (KG4VCF).
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TxAnalyzer.h"
#include "core/TxDisplayFeed.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QLoggingCategory>
#include <QSignalSpy>

using namespace NereusSDR;

namespace {

constexpr double kDialHz = 14'200'000.0;

struct Station {
    RadioModel radio;
    TxAnalyzer analyzer{TxAnalyzer::kTxDispId};
    int sliceId{-1};

    Station()
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        sliceId = radio.addSlice();
        if (SliceModel* slice = radio.sliceById(sliceId)) {
            slice->setFrequency(kDialHz);
        }
        radio.setTxAnalyzer(&analyzer);
    }
    ~Station() { radio.setTxAnalyzer(nullptr); }

    TxDisplayFeed* feed() const { return radio.txDisplayFeed(); }
    SliceModel* txSlice() const { return radio.txBoundSlice(); }

    bool key(bool on)
    {
        MoxController* mox = radio.moxController();
        if (mox == nullptr) {
            return false;
        }
        // Today's Core is receive-only; lifting the pre-check stands in for
        // one that can transmit. The keying goes through the same path.
        mox->setMoxCheck({});
        mox->setMox(on);
        return true;
    }
};

} // namespace

class TstTxDisplayFeed : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // No radio is attached: the model's own not-connected notes (the TX
        // frequency and antennas it cannot push) are expected here.
        QLoggingCategory::setFilterRules(QStringLiteral(
            "nereus.*.debug=false\nnereus.*.info=false\nnereussdr.*.info=false\n"
            "nereus.connection.warning=false"));
    }
    void clampHoldsTheViewInsideTheBaseband();
    void feedExistsOnlyWithAnAnalyzer();
    void runsTheAnalyzerOnEveryKeyWithoutViewers();
    void governingViewerSetsTheAnalyzerView();
    void lowerIdGovernsUntilItLeaves();
    void localViewerGoverns();
    void carrierFollowsXitAndTheSliceWhileKeyed();
    void spanUnder1000KeepsTheLastGoodView();
    void planesPassOnlyWhileKeyed();
    void mini_without_tx_channel_stays_unavailable();
    void mini_does_not_attach_to_an_unopened_tx_channel();
};

void TstTxDisplayFeed::mini_without_tx_channel_stays_unavailable()
{
    Station station;
    TxDisplayFeed* feed = station.feed();
    QVERIFY(feed);
    const int viewer = feed->addViewer(kDialHz, 40'000.0, 1024,
                                       /*local=*/false, /*mini=*/true);
    QCOMPARE(feed->miniView().pixels, 1024);
    QCOMPARE(feed->miniView().spanHz(), 40'000.0);
    QVERIFY(station.key(true));
    QVERIFY(!feed->miniReady());
    QCOMPARE(feed->viewerCount(), 0); // mini never perturbs pan governor
    feed->removeViewer(viewer);
    QVERIFY(station.key(false));
}

void TstTxDisplayFeed::mini_does_not_attach_to_an_unopened_tx_channel()
{
    Station station;
    TxChannel unopened(WdspEngine::kTxChannelId, 256, 256,
                       station.radio.transmitLane(), nullptr);
    station.radio.injectTxChannelForTest(&unopened);
    TxDisplayFeed* feed = station.feed();
    QVERIFY(feed);
    const int viewer = feed->addViewer(kDialHz, 40'000.0, 1024,
                                       /*local=*/false, /*mini=*/true);
    QVERIFY(station.key(true));
    // Creation and SetAnalyzer may succeed for display 6, but the lane
    // guard must refuse TXASetSipAllocDisps when OpenChannel never ran.
    QTRY_VERIFY_WITH_TIMEOUT(feed->miniAttachmentSettledForTest(), 5'000);
    QVERIFY(!feed->miniReady());
    QVERIFY(!unopened.isWdspReady());
    feed->removeViewer(viewer);
    QVERIFY(station.key(false));
    station.radio.injectTxChannelForTest(nullptr);
}

void TstTxDisplayFeed::clampHoldsTheViewInsideTheBaseband()
{
    // Centred on the carrier: +/- half the span, in 100 Hz steps.
    TxDisplayView view = TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz, 8000.0, 1200);
    QCOMPARE(view.carrierHz, kDialHz);
    QCOMPARE(view.lowHz, -4000);
    QCOMPARE(view.highHz, 4000);
    QCOMPARE(view.pixels, 1200);
    QCOMPARE(view.centreHz(), kDialHz);
    QCOMPARE(view.spanHz(), 8000.0);

    // Panned: the edges are relative to the carrier, quantised to 100 Hz.
    view = TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz + 1234.0, 10000.0, 800);
    QCOMPARE(view.lowHz, -3800);   // -3766 rounds to -3800
    QCOMPARE(view.highHz, 6200);   //  6234 rounds to  6200

    // Past +48 kHz: pulled back inside, span kept.
    view = TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz + 60000.0, 20000.0, 800);
    QCOMPARE(view.lowHz, 28000);
    QCOMPARE(view.highHz, 48000);
    // Past -48 kHz likewise.
    view = TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz - 90000.0, 20000.0, 800);
    QCOMPARE(view.lowHz, -48000);
    QCOMPARE(view.highHz, -28000);
    // Wider than the baseband: the whole 96 kHz and no more.
    view = TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz + 5000.0, 200000.0, 800);
    QCOMPARE(view.lowHz, -48000);
    QCOMPARE(view.highHz, 48000);

    // A span under 1000 Hz comes back empty: keep the last good view.
    view = TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz, 999.0, 800);
    QVERIFY(view.empty());
    QCOMPARE(view.lowHz, 0);
    QCOMPARE(view.highHz, 0);
    QVERIFY(TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz, 0.0, 800).empty());
    QVERIFY(!TxAnalyzer::clampViewToBaseband(kDialHz, kDialHz, 1000.0, 800).empty());
}

void TstTxDisplayFeed::feedExistsOnlyWithAnAnalyzer()
{
    RadioModel radio;
    QVERIFY(radio.txDisplayFeed() == nullptr);
    TxAnalyzer analyzer(TxAnalyzer::kTxDispId);
    radio.setTxAnalyzer(&analyzer);
    QVERIFY(radio.txDisplayFeed() != nullptr);
    QCOMPARE(radio.txDisplayFeed()->analyzer(), &analyzer);
    radio.setTxAnalyzer(nullptr);
    QVERIFY(radio.txDisplayFeed() == nullptr);
}

void TstTxDisplayFeed::runsTheAnalyzerOnEveryKeyWithoutViewers()
{
    Station s;
    QVERIFY(s.feed());
    QVERIFY(s.txSlice());
    QSignalSpy keyed(s.feed(), &TxDisplayFeed::keyedChanged);
    s.analyzer.setSpectrumWindow(-2000, 2000);
    s.analyzer.setNumPixels(640);

    QVERIFY(s.key(true));
    QTRY_VERIFY(s.feed()->isKeyed());
    QVERIFY(s.analyzer.isRunning());
    QCOMPARE(keyed.count(), 1);
    QCOMPARE(keyed.last().at(0).toBool(), true);
    // Nobody asked: the window and pixel count are left as they were.
    QCOMPARE(s.analyzer.spectrumWindowLowHz(), -2000);
    QCOMPARE(s.analyzer.spectrumWindowHighHz(), 2000);
    QCOMPARE(s.analyzer.numPixels(), 640);
    QCOMPARE(s.feed()->currentView().carrierHz, kDialHz);
    QCOMPARE(s.feed()->governingViewer(), 0);

    QVERIFY(s.key(false));
    QTRY_VERIFY(!s.feed()->isKeyed());
    QVERIFY(!s.analyzer.isRunning());
    QCOMPARE(keyed.count(), 2);
    QCOMPARE(keyed.last().at(0).toBool(), false);
    // The fall clears the clip, as the local window's fall does.
    QCOMPARE(s.analyzer.spectrumWindowLowHz(), 0);
    QCOMPARE(s.analyzer.spectrumWindowHighHz(), 0);
}

void TstTxDisplayFeed::governingViewerSetsTheAnalyzerView()
{
    Station s;
    TxDisplayFeed* feed = s.feed();
    QVERIFY(feed);
    const int id = feed->addViewer(kDialHz + 1000.0, 10000.0, 800, /*local=*/false);
    QVERIFY(id > 0);
    QVERIFY(feed->isGoverning(id));
    // Not keyed: nothing reaches the analyzer yet.
    QCOMPARE(s.analyzer.spectrumWindowLowHz(), 0);
    QSignalSpy viewChanged(feed, &TxDisplayFeed::viewChanged);

    QVERIFY(s.key(true));
    QTRY_VERIFY(feed->isKeyed());
    const TxDisplayView view = feed->currentView();
    QCOMPARE(view.carrierHz, kDialHz);
    QCOMPARE(view.lowHz, -4000);
    QCOMPARE(view.highHz, 6000);
    QCOMPARE(view.pixels, 800);
    QCOMPARE(s.analyzer.spectrumWindowLowHz(), -4000);
    QCOMPARE(s.analyzer.spectrumWindowHighHz(), 6000);
    QCOMPARE(s.analyzer.numPixels(), 800);

    // A pan or zoom of the governing viewer moves the analyzer while keyed.
    feed->updateViewer(id, kDialHz, 20000.0, 1000);
    QCOMPARE(feed->currentView().lowHz, -10000);
    QCOMPARE(feed->currentView().highHz, 10000);
    QCOMPARE(s.analyzer.spectrumWindowLowHz(), -10000);
    QCOMPARE(s.analyzer.numPixels(), 1000);
    QVERIFY(!viewChanged.isEmpty());
    QCOMPARE(viewChanged.last().at(0).value<TxDisplayView>(), feed->currentView());

    QVERIFY(s.key(false));
    QTRY_VERIFY(!feed->isKeyed());
    feed->removeViewer(id);
    QCOMPARE(feed->viewerCount(), 0);
}

void TstTxDisplayFeed::lowerIdGovernsUntilItLeaves()
{
    Station s;
    TxDisplayFeed* feed = s.feed();
    QVERIFY(feed);
    QVERIFY(s.key(true));
    QTRY_VERIFY(feed->isKeyed());
    QSignalSpy governor(feed, &TxDisplayFeed::governorChanged);
    const int first = feed->addViewer(kDialHz, 8000.0, 600, false);
    const int second = feed->addViewer(kDialHz, 30000.0, 900, false);
    QVERIFY(first < second);
    QVERIFY(feed->isGoverning(first));
    QVERIFY(!feed->isGoverning(second));
    QCOMPARE(feed->currentView().spanHz(), 8000.0);
    QCOMPARE(s.analyzer.numPixels(), 600);

    // The governing viewer leaves: the other governs, with its own view.
    feed->removeViewer(first);
    QVERIFY(feed->isGoverning(second));
    QCOMPARE(feed->currentView().spanHz(), 30000.0);
    QCOMPARE(s.analyzer.numPixels(), 900);
    QVERIFY(!governor.isEmpty());
    QCOMPARE(governor.last().at(0).toInt(), second);

    QVERIFY(s.key(false));
    QTRY_VERIFY(!feed->isKeyed());
}

void TstTxDisplayFeed::localViewerGoverns()
{
    Station s;
    TxDisplayFeed* feed = s.feed();
    QVERIFY(feed);
    const int remote = feed->addViewer(kDialHz, 8000.0, 600, false);
    const int local = feed->addViewer(kDialHz, 16000.0, 1400, /*local=*/true);
    QVERIFY(remote < local);
    // A desktop window hosting the Core governs over a lower remote id.
    QVERIFY(feed->isGoverning(local));
    QVERIFY(!feed->isGoverning(remote));
    QVERIFY(s.key(true));
    QTRY_VERIFY(feed->isKeyed());
    QCOMPARE(feed->currentView().spanHz(), 16000.0);
    QCOMPARE(s.analyzer.numPixels(), 1400);
    feed->removeViewer(local);
    QVERIFY(feed->isGoverning(remote));
    QCOMPARE(feed->currentView().spanHz(), 8000.0);
    QVERIFY(s.key(false));
    QTRY_VERIFY(!feed->isKeyed());
}

void TstTxDisplayFeed::carrierFollowsXitAndTheSliceWhileKeyed()
{
    Station s;
    TxDisplayFeed* feed = s.feed();
    SliceModel* slice = s.txSlice();
    QVERIFY(feed && slice);
    const int id = feed->addViewer(kDialHz, 8000.0, 800, false);
    QVERIFY(s.key(true));
    QTRY_VERIFY(feed->isKeyed());
    QCOMPARE(feed->currentView().carrierHz, kDialHz);
    QSignalSpy viewChanged(feed, &TxDisplayFeed::viewChanged);

    // Row 16: XIT on while keyed. The carrier is the transmitter's
    // (txFrequencyForSlice: dial plus XIT), and the view moves with it.
    slice->setXitHz(500);
    slice->setXitEnabled(true);
    const double shifted = static_cast<double>(s.radio.txFrequencyForSlice(slice));
    QCOMPARE(shifted, kDialHz + 500.0);
    QCOMPARE(feed->currentView().carrierHz, shifted);
    QCOMPARE(feed->currentView().centreHz(), shifted);
    QCOMPARE(feed->currentView().lowHz, -4000);
    QCOMPARE(feed->currentView().highHz, 4000);
    QVERIFY(!viewChanged.isEmpty());
    QCOMPARE(viewChanged.last().at(0).value<TxDisplayView>().carrierHz, shifted);

    // The transmit slice retuned while keyed.
    const int before = viewChanged.count();
    slice->setFrequency(kDialHz + 3000.0);
    QCOMPARE(feed->currentView().carrierHz, kDialHz + 3500.0);
    QVERIFY(viewChanged.count() > before);

    // Unkeyed, a change is not followed until the next key.
    QVERIFY(s.key(false));
    QTRY_VERIFY(!feed->isKeyed());
    const int unkeyed = viewChanged.count();
    slice->setXitEnabled(false);
    QCOMPARE(viewChanged.count(), unkeyed);
    feed->removeViewer(id);
}

void TstTxDisplayFeed::spanUnder1000KeepsTheLastGoodView()
{
    Station s;
    TxDisplayFeed* feed = s.feed();
    QVERIFY(feed);
    const int id = feed->addViewer(kDialHz, 6000.0, 700, false);
    QVERIFY(s.key(true));
    QTRY_VERIFY(feed->isKeyed());
    QCOMPARE(feed->currentView().lowHz, -3000);
    feed->updateViewer(id, kDialHz, 500.0, 700);
    QCOMPARE(feed->currentView().lowHz, -3000);
    QCOMPARE(feed->currentView().highHz, 3000);
    QCOMPARE(s.analyzer.spectrumWindowLowHz(), -3000);
    QVERIFY(s.key(false));
    QTRY_VERIFY(!feed->isKeyed());
}

void TstTxDisplayFeed::planesPassOnlyWhileKeyed()
{
    Station s;
    TxDisplayFeed* feed = s.feed();
    QVERIFY(feed);
    QSignalSpy trace(feed, &TxDisplayFeed::traceReady);
    QSignalSpy waterfall(feed, &TxDisplayFeed::waterfallReady);
    const QVector<float> pixels{-60.0f, -20.0f, -60.0f};
    // The analyzer's own signals, as its poll emits them.
    emit s.analyzer.txFftReady(-1, pixels);
    emit s.analyzer.txWaterfallReady(-1, pixels);
    QCOMPARE(trace.count(), 0);
    QCOMPARE(waterfall.count(), 0);

    QVERIFY(s.key(true));
    QTRY_VERIFY(feed->isKeyed());
    emit s.analyzer.txFftReady(-1, pixels);
    emit s.analyzer.txWaterfallReady(-1, pixels);
    QCOMPARE(trace.count(), 1);
    QCOMPARE(waterfall.count(), 1);
    QCOMPARE(trace.last().at(0).value<QVector<float>>(), pixels);

    QVERIFY(s.key(false));
    QTRY_VERIFY(!feed->isKeyed());
    emit s.analyzer.txFftReady(-1, pixels);
    QCOMPARE(trace.count(), 1);
}

QTEST_MAIN(TstTxDisplayFeed)
#include "tst_tx_display_feed.moc"
