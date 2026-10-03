// =================================================================
// tests/tst_pan_actions_per_pan.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Asserts that a control drawn on a
// pan acts on that pan, in a local window and in a remote one.
//
// Remote-window parity plan, Task 18 (R-R3-09, R-R3-24, R-R3-34, R-R3-49):
//   B3.1  the overlay BAND flyout on a pan whose slice is not the active
//         one changes that pan's slice, local and remote.
//   C8    a restored pan with no slice says so and how to add one, instead
//         of standing blank.
//   and the passing items: the Display flyout (Grid Lines included) and
//   the Clarity badge on every pan, each acting on its own pan; a spot's
//   left-click doing what AetherSDR's does; Pan Layout and +PAN using the
//   Core's advertised slice limit in a remote window.
//
// Loopback WebSocket and test radio models only: no RF, no radio, no audio
// device. Every run is offscreen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF. Unkeyed TX-letter shared-pan history
//                 lifecycle regression. AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Primary empty-key Max Bin regression
//                                    and coverage guards. AI-assisted via
//                                    OpenAI Codex.
//   2026-09-26  J.J. Boyd / KG4VCF  Remote-window parity Task 18.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/ClarityController.h"
#include "core/ConnectionState.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/SMeterWidget.h"
#include "gui/applets/TxApplet.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/TransmitHolder.h"
#include "gui/meters/MeterPoller.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModeResolver.h"
#include "models/SpotModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

constexpr int kSettleMs = 300;

PanadapterApplet* appletFor(MainWindow* window, const QString& panId)
{
    if (!window) { return nullptr; }
    for (PanadapterApplet* applet : window->findChildren<PanadapterApplet*>()) {
        if (applet->panId() == panId) { return applet; }
    }
    return nullptr;
}

SpectrumOverlayPanel* stripFor(MainWindow* window, const QString& panId)
{
    PanadapterApplet* applet = appletFor(window, panId);
    return applet && applet->spectrumWidget()
        ? applet->spectrumWidget()->findChild<SpectrumOverlayPanel*>()
        : nullptr;
}

// The slice a pan shows: its own active slice.
SliceModel* sliceOnPan(MainWindow* window, const QString& panId)
{
    PanadapterApplet* applet = appletFor(window, panId);
    if (!applet || !window->radioModel()) { return nullptr; }
    return window->radioModel()->sliceById(applet->activeSliceIndex());
}

// A local window on a test radio with the saved layout `layout`, connected,
// every pan given its slice as a local window does at connect.
std::unique_ptr<MainWindow> openLocalWindow(const QString& layout)
{
    AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), layout);
    auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                               MainWindow::ConnectionStartup::Deferred);
    RadioModel* model = window->radioModel();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                               /*defaultRateHz=*/192000);
    window->resize(1280, 800);
    window->show();
    model->setConnectionStateForTest(ConnectionState::Connected);
    return window;
}

} // namespace

class TestPanActionsPerPan final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("pan-actions-per-pan")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // Core's primary slice can have no panKey. Its associated live pan is
    // still its Max Bin source, shared by the S-meter and VFO meter.
    void maxBinUsesAssociatedPanForEmptyCoreKey()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        SliceModel* coreSlice = h.station().slices().first();
        coreSlice->setPanKey(QString());
        coreSlice->setFrequency(14002000.0);
        coreSlice->setFilter(500, 2500);
        h.startStartupConnection();
        QTRY_VERIFY(h.client()->isHandshakeComplete());
        SliceModel* slice = h.remoteModel()->sliceById(coreSlice->sliceIndex());
        QVERIFY(slice);
        QVERIFY(slice->panKey().isEmpty());
        QVERIFY(slice->streamIndex() >= 0);
        PanadapterApplet* pan = appletFor(h.window(), QStringLiteral("pan-0"));
        QVERIFY(pan && pan->associatedSlices().contains(slice->sliceIndex()));
        SpectrumWidget* sw = pan->spectrumWidget();
        QVERIFY(sw);
        SpectrumEndpointContext context;
        context.codec = {29, 1, -180, 0, 11, 11, 0};
        context.exactCentreHz = 14000000;
        context.exactSpanHz = 10000;
        sw->setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(11, -120);
        frame.traceDbm[8] = -71;
        frame.waterfallDbm = QVector<float>(11, -110);
        QVERIFY(sw->updateRemoteSpectrum(frame));
        QCOMPARE(sw->peakDbmInPassband(14002500, 14004500), -71.0);
        MeterPoller* poller = h.window()->findChild<MeterPoller*>();
        SMeterWidget* meter = h.window()->findChild<SMeterWidget*>();
        QVERIFY(poller && meter);
        meter->setRxMode(QStringLiteral("Max Bin"));
        QSignalSpy vfo(poller, &MeterPoller::remoteSliceLevelUpdated);
        QVERIFY(QMetaObject::invokeMethod(poller, "poll", Qt::DirectConnection));
        QCOMPARE(meter->levelDbm(), -71.0f);
        QVERIFY(!vfo.isEmpty());
        QCOMPARE(vfo.last().at(0).toInt(), slice->sliceIndex());
        QCOMPARE(vfo.last().at(1).toDouble(), -71.0);

        const auto noReading = [&]() {
            vfo.clear();
            QVERIFY(QMetaObject::invokeMethod(poller, "poll", Qt::DirectConnection));
            QCOMPARE(meter->sUnitsText(), QStringLiteral("--"));
            QVERIFY(!vfo.isEmpty());
            QCOMPARE(vfo.last().at(1).toDouble(), -400.0);
        };
        // An unbound slice cannot borrow a retained display frame.
        const int stream = slice->streamIndex();
        slice->setStreamIndex(-1);
        noReading();
        slice->setStreamIndex(stream);
        // A marker association with another receiver does not lend its peak.
        h.remoteModel()->addSliceWithStationId(99);
        SliceModel* foreign = h.remoteModel()->sliceById(99);
        QVERIFY(foreign);
        foreign->setStreamIndex(stream + 1);
        pan->addSlice(99);
        pan->setActiveSliceIndex(99);
        h.remoteModel()->setActiveSlice(slice->sliceIndex());
        // Focus the slice again but explicitly leave the test pan displaying
        // the other receiver, without processing a subscription renewal.
        pan->setActiveSliceIndex(99);
        vfo.clear();
        QVERIFY(QMetaObject::invokeMethod(poller, "poll", Qt::DirectConnection));
        bool foundPrimary = false;
        for (const QList<QVariant>& reading : vfo) {
            if (reading.at(0).toInt() == slice->sliceIndex()) {
                QCOMPARE(reading.at(1).toDouble(), -400.0);
                foundPrimary = true;
            }
        }
        QVERIFY(foundPrimary);
        // Selecting a co-host on the same receiver keeps the primary's
        // passband measurement available, matching a live shared pan.
        foreign->setStreamIndex(stream);
        foreign->setStreamEpoch(slice->streamEpoch());
        vfo.clear();
        QVERIFY(QMetaObject::invokeMethod(poller, "poll", Qt::DirectConnection));
        foundPrimary = false;
        for (const QList<QVariant>& reading : vfo) {
            if (reading.at(0).toInt() == slice->sliceIndex()) {
                QCOMPARE(reading.at(1).toDouble(), -71.0);
                foundPrimary = true;
            }
        }
        QVERIFY(foundPrimary);
        pan->removeSlice(99);
        pan->setActiveSliceIndex(slice->sliceIndex());
        h.remoteModel()->setActiveSlice(slice->sliceIndex());
        // Rejected/retired coverage clears the trace, so no old peak remains.
        sw->clearRemoteSpectrum();
        noReading();
        // No actual host is not replaced by an arbitrary active pan.
        pan->removeSlice(slice->sliceIndex());
        noReading();
    }

    void unkeyedTxAppletSliceChoiceKeepsSharedPanHistory()
    {
        RemoteWindowHarness::Options options;
        options.stationSlices = 1;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        SliceModel* first = h.station().sliceById(0);
        QVERIFY(first);
        first->setPanKey(QString());
        first->setFrequency(3650000);
        QCOMPARE(h.station().addSlice(QStringLiteral("pan-0")), 1);
        SliceModel* second = h.station().sliceById(1);
        QVERIFY(second);
        second->setFrequency(3651000);
        QVERIFY(h.station().moveSlicesToStream({0, 1}, first->streamIndex(), 3650000));
        QCOMPARE(second->streamIndex(), first->streamIndex());
        const int stream = first->streamIndex();
        const quint64 epoch = first->streamEpoch();
        h.startStartupConnection();
        QTRY_VERIFY(h.client()->stationLinkReady());
        TransmitHolder::Holder self;
        self.deviceId = QByteArrayLiteral("token:1");
        self.name = QStringLiteral("History bench window");
        h.server().transmitHolder()->transferTo(self, QStringLiteral("test"));
        QTRY_VERIFY(h.client()->holdsTransmitHere());
        QTRY_VERIFY(h.remoteModel()->sliceById(1));
        PanadapterApplet* pan = appletFor(h.window(), QStringLiteral("pan-0"));
        QVERIFY(pan && pan->associatedSlices().contains(0)
                    && pan->associatedSlices().contains(1));
        SpectrumWidget* sw = pan->spectrumWidget();
        QVERIFY(sw);
        sw->setWaterfallTickerPausedForTest(true);
        // Let the initial shown-window resize/history debounce settle before
        // the trigger; viewport rebuilds can change cursor representation.
        QTest::qWait(300);
        sw->setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        sw->setDssRowDivider(1); // Explicit direct-row fixture; main folds remote rows.
        SpectrumEndpointContext context;
        context.codec = {29, 1, -180, 0, 11, 11, 0};
        context.exactCentreHz = 3650000;
        context.exactSpanHz = 10000;
        sw->setRemoteSpectrumContext(context, context.exactCentreHz, 192000);
        DisplayCodecFrame frame;
        frame.context = context.codec;
        frame.traceDbm = QVector<float>(11, -100);
        frame.waterfallDbm = QVector<float>(11, -110);
        QVERIFY(sw->updateRemoteSpectrum(frame));
        for (int i = 0; i < 6; ++i) { sw->pushWaterfallRowForTest(frame.waterfallDbm); }
        const int rows = sw->dssRowsPushedForTest();
        const int history = sw->waterfallHistoryRowsForTest();
        QVERIFY(rows >= 6 && history >= 6);
        const auto colouredPixels = [sw]() {
            const QImage& image = sw->liveWaterfallForTest();
            int coloured = 0;
            const QRgb empty = QColor(0x0f, 0x0f, 0x1a).rgb();
            for (int y = 0; y < image.height(); ++y) {
                const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    if (line[x] != empty && line[x] != qRgb(0, 0, 0)) { ++coloured; }
                }
            }
            return coloured;
        };
        QVERIFY(colouredPixels() > 0);
        const QSize originalImageSize = sw->liveWaterfallForTest().size();
        const int active = pan->activeSliceIndex();
        const double centre = sw->centerFrequency();
        const double span = sw->bandwidth();
        TxApplet* applet = h.window()->findChild<TxApplet*>();
        QVERIFY(applet);
        QSignalSpy finished(h.client(), &StationClient::deviceCommandFinished);
        int minimumRows = rows;
        int minimumHistory = history;
        int minimumColoured = colouredPixels();
        QTimer sampler;
        QObject::connect(&sampler, &QTimer::timeout, h.window(), [&]() {
            minimumRows = std::min(minimumRows, sw->dssRowsPushedForTest());
            minimumHistory = std::min(minimumHistory, sw->waterfallHistoryRowsForTest());
            minimumColoured = std::min(minimumColoured, colouredPixels());
        });
        sampler.start(1);
        for (int id : {0, 1, 0}) {
            QPushButton* letter = nullptr;
            for (QPushButton* button : applet->transmitSliceButtons()) {
                if (button->property("sliceId").toInt() == id) { letter = button; }
            }
            QVERIFY(letter && letter->isEnabled());
            const int writeRow = sw->liveWaterfallWriteRowForTest();
            finished.clear();
            letter->click();
            QTRY_VERIFY(!finished.isEmpty());
            QCOMPARE(finished.last().at(0).toByteArray(), QByteArrayLiteral("tx.setTxSlice"));
            QVERIFY2(finished.last().at(2).toBool(),
                     qPrintable(finished.last().at(3).toString()));
            QTRY_VERIFY(h.remoteModel()->sliceById(id)->isTxSlice());
            QCOMPARE(h.station().txSliceArbiter()->txBoundSliceId(), id);
            QVERIFY(!h.station().mox());
            QVERIFY(!h.remoteModel()->mox());
            QCOMPARE(first->streamIndex(), stream);
            QCOMPARE(second->streamIndex(), stream);
            QCOMPARE(first->streamEpoch(), epoch);
            QCOMPARE(second->streamEpoch(), epoch);
            QCOMPARE(pan->activeSliceIndex(), active);
            QCOMPARE(sw->centerFrequency(), centre);
            QCOMPARE(sw->bandwidth(), span);
            QTest::qWait(120);
            QCOMPARE(minimumRows, rows);
            QCOMPARE(minimumHistory, history);
            QVERIFY(minimumColoured > 0);
            QCOMPARE(sw->liveWaterfallForTest().size(), originalImageSize);
            if (sw->liveWaterfallWriteRowForTest() != writeRow) {
                qInfo() << "History fixture cursor moved" << writeRow
                        << sw->liveWaterfallWriteRowForTest()
                        << "image" << sw->liveWaterfallForTest().size()
                        << "coloured" << colouredPixels();
            }
            QVERIFY(sw->updateRemoteSpectrum(frame));
            sw->pushWaterfallRowForTest(frame.waterfallDbm);
            QVERIFY(sw->dssRowsPushedForTest() > rows);
        }
        sampler.stop();
    }

    // ── B3.1 ────────────────────────────────────────────────────────────

    // A remote window: the second pan's BAND flyout changes the second
    // pan's slice on the Core, not the active slice on the first pan.
    void bandFlyoutChangesItsOwnPansSliceRemote()
    {
        RemoteWindowHarness::Options options;
        options.stationSlices = 2;
        options.panLayout = QStringLiteral("2v");
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect && connect->isEnabled());
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        QCOMPARE(h.client()->capabilities().bandSelectVersion, 1);

        const QList<SliceModel*> coreSlices = h.station().slices();
        QCOMPARE(coreSlices.size(), 2);
        SliceModel* const first = coreSlices.at(0);
        SliceModel* const second = coreSlices.at(1);
        QCOMPARE(first->panKey(), QStringLiteral("pan-0"));
        QCOMPARE(second->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(h.station().activeSlice(), first);
        const double firstHz = first->frequency();
        QVERIFY(bandFromFrequency(second->frequency()) != Band::Band40m);

        MainWindow* const window = h.window();
        QTRY_VERIFY(sliceOnPan(window, QStringLiteral("pan-1"))
                    && sliceOnPan(window, QStringLiteral("pan-1"))->sliceIndex()
                           == second->sliceIndex());
        SpectrumOverlayPanel* const strip = stripFor(window, QStringLiteral("pan-1"));
        QVERIFY(strip);
        // The Core runs its own band change for that slice (slice.selectBand),
        // with its own band memory, as a band button at the Core does.
        QStringList bandVerbs;
        QObject::connect(h.client(), &StationClient::commandResponse, h.window(),
            [&bandVerbs](const SessionMessage& message) {
                if (message.commandVerb == "slice.selectBand") {
                    bandVerbs << (message.accepted ? QStringLiteral("accepted")
                                                   : message.reason);
                }
            });

        emit strip->bandSelected(QStringLiteral("40m"), 7.0e6, QStringLiteral("LSB"));

        QTRY_COMPARE(bandVerbs, QStringList{QStringLiteral("accepted")});
        QTRY_COMPARE(bandFromFrequency(second->frequency()), Band::Band40m);
        QTest::qWait(kSettleMs);
        QCOMPARE(first->frequency(), firstHz);
        // The window shows the Core's change.
        QTRY_COMPARE(bandFromFrequency(
                         h.remoteModel()->sliceById(second->sliceIndex())->frequency()),
                     Band::Band40m);
    }

    // A local window: the same click, the same result.
    void bandFlyoutChangesItsOwnPansSliceLocal()
    {
        std::unique_ptr<MainWindow> window = openLocalWindow(QStringLiteral("2v"));
        RadioModel* const model = window->radioModel();
        QTRY_VERIFY(sliceOnPan(window.get(), QStringLiteral("pan-0"))
                    && sliceOnPan(window.get(), QStringLiteral("pan-1")));
        SliceModel* const first = sliceOnPan(window.get(), QStringLiteral("pan-0"));
        SliceModel* const second = sliceOnPan(window.get(), QStringLiteral("pan-1"));
        QVERIFY(first != second);
        model->setActiveSliceById(first->sliceIndex());
        QCOMPARE(model->activeSlice(), first);
        const double firstHz = first->frequency();
        QVERIFY(bandFromFrequency(second->frequency()) != Band::Band40m);

        SpectrumOverlayPanel* const strip = stripFor(window.get(), QStringLiteral("pan-1"));
        QVERIFY(strip);
        emit strip->bandSelected(QStringLiteral("40m"), 7.0e6, QStringLiteral("LSB"));

        QCOMPARE(bandFromFrequency(second->frequency()), Band::Band40m);
        QCOMPARE(first->frequency(), firstHz);
    }

    // ── C8 ──────────────────────────────────────────────────────────────

    // A remote window restores two pans while the Core has one slice: the
    // empty pan says why and how to fill it; a slice arriving clears it.
    void restoredPanWithNoSliceSaysSoRemote()
    {
        RemoteWindowHarness::Options options;
        options.stationSlices = 1;
        options.panLayout = QStringLiteral("2v");
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        PanadapterApplet* const pan0 = appletFor(h.window(), QStringLiteral("pan-0"));
        PanadapterApplet* const pan1 = appletFor(h.window(), QStringLiteral("pan-1"));
        QVERIFY(pan0 && pan1);
        // Not connected: an empty pan is not a missing slice yet.
        QCOMPARE(pan1->visibleNoSliceHint(), QString());

        h.holdNextSnapshot();
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(h.snapshotHeld() && h.remoteModel()->isConnected(), 10000);
        QTest::qWait(kSettleMs);
        // The Core's slices have not arrived: nothing to say yet.
        QCOMPARE(pan1->visibleNoSliceHint(), QString());

        h.releaseSnapshot();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        QTRY_COMPARE(pan1->visibleNoSliceHint(), PanadapterApplet::noSliceHintText());
        QCOMPARE(pan0->visibleNoSliceHint(), QString());
        QVERIFY(h.addSliceCommands().isEmpty());

        // A slice on that pan clears it; closing it brings it back.
        h.station().addSlice(QStringLiteral("pan-1"));
        QTRY_COMPARE(pan1->visibleNoSliceHint(), QString());
        const QVector<SliceModel*> onPan1 = h.station().slicesOnPan(QStringLiteral("pan-1"));
        QCOMPARE(onPan1.size(), 1);
        h.station().removeSlice(onPan1.first()->sliceIndex());
        QTRY_COMPARE(pan1->visibleNoSliceHint(), PanadapterApplet::noSliceHintText());

        // The radio going offline at the Core ends it.
        h.reportRadioOffline();
        QTRY_COMPARE(pan1->visibleNoSliceHint(), QString());
    }

    // A local window: a pan whose slice is closed says the same.
    void emptyPanSaysSoLocal()
    {
        std::unique_ptr<MainWindow> window = openLocalWindow(QStringLiteral("2v"));
        RadioModel* const model = window->radioModel();
        QTRY_VERIFY(sliceOnPan(window.get(), QStringLiteral("pan-1")));
        PanadapterApplet* const pan1 = appletFor(window.get(), QStringLiteral("pan-1"));
        QCOMPARE(pan1->visibleNoSliceHint(), QString());
        model->removeSlice(sliceOnPan(window.get(), QStringLiteral("pan-1"))->sliceIndex());
        QTRY_COMPARE(pan1->visibleNoSliceHint(), PanadapterApplet::noSliceHintText());
        model->setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_COMPARE(pan1->visibleNoSliceHint(), QString());
    }

    // ── Passing items ───────────────────────────────────────────────────

    // Pan Layout and +PAN offer as many pans as the Core says it can hold
    // in a remote window, and the board's own limit locally.
    void panLayoutLimitFollowsTheCoresSliceLimit()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect);
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        RadioModel* const remote = h.remoteModel();
        QCOMPARE(MainWindow::panLayoutLimitFor(remote), 5);

        StationCapabilities narrowed = h.client()->capabilities();
        QCOMPARE(narrowed.boardMaxSlices, 5);
        narrowed.effectiveMaxSlices = 2;
        h.pushCapabilities(narrowed);
        QTRY_COMPARE(remote->maxSlices(), 2);
        QCOMPARE(MainWindow::panLayoutLimitFor(remote), 2);
        // The board is unchanged: the limit is the Core's, not the board's.
        QCOMPARE(remote->boardCapabilities().maxSlices, 5);
    }

    void panLayoutLimitIsTheBoardsLocally()
    {
        std::unique_ptr<MainWindow> window = openLocalWindow(QStringLiteral("1"));
        RadioModel* const model = window->radioModel();
        QTRY_VERIFY(model->isConnected());
        QCOMPARE(MainWindow::panLayoutLimitFor(model),
                 qMin(model->boardCapabilities().maxSlices, model->userStreamCount()));
        QCOMPARE(MainWindow::panLayoutLimitFor(nullptr), 1);
    }

    // The Display flyout on the second pan changes the second pan, Grid
    // Lines included, and leaves the first alone.
    void displayFlyoutActsOnItsOwnPan()
    {
        std::unique_ptr<MainWindow> window = openLocalWindow(QStringLiteral("2v"));
        QTRY_VERIFY(stripFor(window.get(), QStringLiteral("pan-1")));
        SpectrumWidget* const sw0 = appletFor(window.get(), QStringLiteral("pan-0"))->spectrumWidget();
        SpectrumWidget* const sw1 = appletFor(window.get(), QStringLiteral("pan-1"))->spectrumWidget();
        SpectrumOverlayPanel* const strip1 = stripFor(window.get(), QStringLiteral("pan-1"));

        // Grid Lines: the toggle in the second pan's flyout, clicked.
        QPushButton* grid = nullptr;
        for (QPushButton* button : sw1->findChildren<QPushButton*>()) {
            if (button->toolTip() == QStringLiteral("Show or hide frequency and dB grid lines")) {
                QVERIFY(grid == nullptr);
                grid = button;
            }
        }
        QVERIFY(grid);
        QVERIFY(sw1->gridEnabled());
        QCOMPARE(grid->isChecked(), true);
        grid->click();
        QCOMPARE(sw1->gridEnabled(), false);
        QCOMPARE(sw0->gridEnabled(), true);
        grid->click();
        QCOMPARE(sw1->gridEnabled(), true);

        const int black0 = sw0->wfBlackLevel();
        emit strip1->wfBlackLevelChanged(black0 == 42 ? 43 : 42);
        QCOMPARE(sw1->wfBlackLevel(), black0 == 42 ? 43 : 42);
        QCOMPARE(sw0->wfBlackLevel(), black0);

        const float alpha0 = sw0->fillAlpha();
        emit strip1->fillAlphaChanged(0.25f);
        QCOMPARE(sw1->fillAlpha(), 0.25f);
        QCOMPARE(sw0->fillAlpha(), alpha0);

        const bool cursor0 = sw0->cursorFreqVisible();
        emit strip1->cursorFreqVisibleChanged(!cursor0);
        QCOMPARE(sw1->cursorFreqVisible(), !cursor0);
        QCOMPARE(sw0->cursorFreqVisible(), cursor0);

        const WfColorScheme scheme0 = sw0->wfColorScheme();
        const int other = static_cast<int>(scheme0) == 0 ? 1 : 0;
        emit strip1->colorSchemeChanged(other);
        QCOMPARE(static_cast<int>(sw1->wfColorScheme()), other);
        QCOMPARE(sw0->wfColorScheme(), scheme0);
    }

    // Clarity tunes the active pan. Re-tune on the second pan's strip makes
    // it the pan Clarity tunes; its badge shows, the first pan's does not,
    // and the first pan goes back to its own waterfall levels.
    void clarityBadgeAndRetuneActOnTheirOwnPan()
    {
        std::unique_ptr<MainWindow> window = openLocalWindow(QStringLiteral("2v"));
        QTRY_VERIFY(stripFor(window.get(), QStringLiteral("pan-1")));
        ClarityController* const clarity = window->radioModel()->clarityController();
        QVERIFY(clarity && clarity->isEnabled());
        SpectrumWidget* const sw0 = appletFor(window.get(), QStringLiteral("pan-0"))->spectrumWidget();
        SpectrumWidget* const sw1 = appletFor(window.get(), QStringLiteral("pan-1"))->spectrumWidget();
        const auto badge = [&window](const QString& panId) -> QLabel* {
            SpectrumOverlayPanel* strip = stripFor(window.get(), panId);
            if (!strip) { return nullptr; }
            for (QLabel* label : strip->parentWidget()->findChildren<QLabel*>()) {
                if (label->toolTip()
                    == QStringLiteral("Clarity status: green = active, amber = paused")) {
                    return label;
                }
            }
            return nullptr;
        };
        QVERIFY(badge(QStringLiteral("pan-0")) && badge(QStringLiteral("pan-1")));
        auto* stack = window->findChild<PanadapterStack*>();
        QVERIFY(stack);
        stack->setActivePan(QStringLiteral("pan-0"));
        emit clarity->waterfallThresholdsChanged(-125.0f, -85.0f);
        QVERIFY(sw0->clarityActive());
        QVERIFY(!badge(QStringLiteral("pan-0"))->isHidden());
        QVERIFY(badge(QStringLiteral("pan-1"))->isHidden());

        emit stripFor(window.get(), QStringLiteral("pan-1"))->clarityRetuneRequested();
        QCOMPARE(stack->activePanId(), QStringLiteral("pan-1"));
        QVERIFY(!sw0->clarityActive());
        emit clarity->waterfallThresholdsChanged(-120.0f, -80.0f);
        QVERIFY(sw1->clarityActive());
        QVERIFY(!sw0->clarityActive());
        QVERIFY(badge(QStringLiteral("pan-0"))->isHidden());
        QVERIFY(!badge(QStringLiteral("pan-1"))->isHidden());
    }

    // A left-click on a spot: the widget tunes the pan's slice, and the
    // slice takes the spot's mode (AetherSDR's spot click), on that pan only.
    void spotClickSetsItsOwnPansSliceMode()
    {
        std::unique_ptr<MainWindow> window = openLocalWindow(QStringLiteral("2v"));
        QTRY_VERIFY(sliceOnPan(window.get(), QStringLiteral("pan-0"))
                    && sliceOnPan(window.get(), QStringLiteral("pan-1")));
        RadioModel* const model = window->radioModel();
        SliceModel* const first = sliceOnPan(window.get(), QStringLiteral("pan-0"));
        SliceModel* const second = sliceOnPan(window.get(), QStringLiteral("pan-1"));
        first->setDspMode(DSPMode::USB);
        second->setDspMode(DSPMode::USB);
        model->spotModel()->applySpotStatus(7, {
            {QStringLiteral("callsign"), QStringLiteral("K1ABC")},
            {QStringLiteral("rx_freq"), QStringLiteral("7.025")},
            {QStringLiteral("mode"), QStringLiteral("CW")},
            {QStringLiteral("source"), QStringLiteral("Cluster")}});
        model->spotModel()->applySpotStatus(8, {
            {QStringLiteral("callsign"), QStringLiteral("VK2XYZ")},
            {QStringLiteral("rx_freq"), QStringLiteral("14.236")},
            {QStringLiteral("source"), QStringLiteral("FreeDV")}});
        SpectrumWidget* const sw1 = appletFor(window.get(), QStringLiteral("pan-1"))->spectrumWidget();

        emit sw1->spotTriggered(7);
        QCOMPARE(second->dspMode(), DSPMode::CWL);
        QCOMPARE(first->dspMode(), DSPMode::USB);

        emit sw1->spotTriggered(8);
        QCOMPARE(second->dspMode(), DSPMode::RADE_U);

        // Auto mode off in the Spot Hub: the click only tunes.
        second->setDspMode(DSPMode::USB);
        AppSettings::instance().setValue(QStringLiteral("SpotAutoSwitchMode"),
                                         QStringLiteral("False"));
        emit sw1->spotTriggered(7);
        QCOMPARE(second->dspMode(), DSPMode::USB);
        AppSettings::instance().remove(QStringLiteral("SpotAutoSwitchMode"));

        // A spot that is gone changes nothing.
        emit sw1->spotTriggered(99);
        QCOMPARE(second->dspMode(), DSPMode::USB);
    }

    // The same click in a remote window changes the Core's slice.
    void spotClickSetsItsOwnPansSliceModeRemote()
    {
        RemoteWindowHarness::Options options;
        options.stationSlices = 2;
        options.panLayout = QStringLiteral("2v");
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect);
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        SliceModel* const coreFirst = h.station().slices().at(0);
        SliceModel* const coreSecond = h.station().slices().at(1);
        coreFirst->setDspMode(DSPMode::USB);
        coreSecond->setDspMode(DSPMode::USB);
        QTRY_VERIFY(sliceOnPan(h.window(), QStringLiteral("pan-1"))
                    && sliceOnPan(h.window(), QStringLiteral("pan-1"))->dspMode() == DSPMode::USB);
        h.remoteModel()->spotModel()->applySpotStatus(7, {
            {QStringLiteral("callsign"), QStringLiteral("K1ABC")},
            {QStringLiteral("rx_freq"), QStringLiteral("14.025")},
            {QStringLiteral("mode"), QStringLiteral("CW")},
            {QStringLiteral("source"), QStringLiteral("Cluster")}});
        emit h.panSpectrum(QStringLiteral("pan-1"))->spotTriggered(7);
        QTRY_COMPARE(coreSecond->dspMode(), DSPMode::CWU);
        QTest::qWait(kSettleMs);
        QCOMPARE(coreFirst->dspMode(), DSPMode::USB);
    }

    void spotModesFollowAetherSdr()
    {
        using namespace SpotModeResolver;
        // AetherSDR's resolver, unchanged.
        QCOMPARE(extractSpotModeFromComment(QStringLiteral("CW  6 dB 28 WPM CQ")),
                 QStringLiteral("CW"));
        QCOMPARE(extractSpotModeFromComment(QStringLiteral("JP-1277 Higashimurayama FT8")),
                 QStringLiteral("FT8"));
        QCOMPARE(inferSpotModeFromBand(14.030), QStringLiteral("CW"));
        QCOMPARE(inferSpotModeFromBand(14.074), QStringLiteral("DIGU"));
        QCOMPARE(inferSpotModeFromBand(14.250), QStringLiteral("USB"));
        QCOMPARE(inferSpotModeFromBand(3.800), QStringLiteral("LSB"));
        QCOMPARE(mapSpotModeToRadioMode(QStringLiteral("SSB"), 7.2), QStringLiteral("LSB"));
        QCOMPARE(mapSpotModeToRadioMode(QStringLiteral("RTTY"), 14.08), QStringLiteral("DIGL"));
        QCOMPARE(resolveSpotRadioMode({}, {}, 0.5), QString());
        // NereusSDR's modes for them.
        SpotData spot;
        spot.rxFreqMhz = 7.030;
        spot.mode = QStringLiteral("CW");
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>(DSPMode::CWL));
        spot.rxFreqMhz = 21.030;
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>(DSPMode::CWU));
        spot.mode = QStringLiteral("NFM");
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>(DSPMode::FM));
        spot.mode = QStringLiteral("FT8");
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>(DSPMode::DIGU));
        spot.mode = QStringLiteral("OTHER");
        spot.rxFreqMhz = 0.5;
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>());
        spot.mode.clear();
        spot.source = QStringLiteral("FreeDV");
        spot.rxFreqMhz = 7.177;
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>(DSPMode::RADE_L));
        spot.rxFreqMhz = 5.354;
        QCOMPARE(dspModeForSpot(spot), std::optional<DSPMode>(DSPMode::RADE_U));
    }

    void noSliceHintIsPlain()
    {
        QVERIFY(OperatorWording::isPlain(PanadapterApplet::noSliceHintText()));
        QVERIFY(!PanadapterApplet::noSliceHintText().contains(QChar(0x2014)));
    }
};

QTEST_MAIN(TestPanActionsPerPan)
#include "tst_pan_actions_per_pan.moc"
