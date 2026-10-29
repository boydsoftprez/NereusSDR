// no-port-check: NereusSDR-original offscreen container composition test.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 — Final review fix wave (C2): with Slice B the transmit slice
//                 and MOX shown, a display on Slice A shows and edits Slice
//                 A's RX filter only. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <chrono>
#include <cmath>
#include <tuple>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/spectrum/FftEnginePool.h"
#include "core/MoxController.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/MeterWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include <QMouseEvent>
#include <QSignalSpy>

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

class TestMiniDisplayComposition final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("mini-display-composition")));
    }
    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    void sharedSliceFansOutAndIndependentSliceKeepsItsOwnCrop()
    {
        QVERIFY(UnbuiltFeatures::isBuilt(UnbuiltFeature::ContainerFilterDisplay));
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] { RadioDiscovery::clearHoldOffForTest(); });
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        window.resize(1280, 800);
        window.show();
        model->setConnectionStateForTest(ConnectionState::Connected);
        while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
        SliceModel* a = model->sliceById(0);
        SliceModel* b = model->sliceById(1);
        QVERIFY(a && b);
        const int stream = a->streamIndex();
        QVERIFY(stream >= 0);
        const double sourceHz = model->streamCentreHz(stream);
        a->setFrequency(sourceHz - 12'000.0);
        b->setFrequency(sourceHz + 12'000.0);
        QCOMPARE(b->streamIndex(), stream);
        QVERIFY(model->streamActive(stream));

        auto* manager = window.findChild<ContainerManager*>();
        QVERIFY(manager);
        // Production content is projected only after a structured commit.
        const auto addMini = [manager](int rxSource) {
            ContainerWidget* container = manager->createContainer(rxSource, DockMode::Floating);
            auto document=manager->workspaceStore()->snapshot();
            for(auto& c:document.containers) { if(c.id==container->id()) { c.contents={manager->contentRegistry()->makeEntry("FILTERDISPLAY")}; } }
            const auto result=manager->commitWorkspace(document,document.revision); Q_ASSERT(result.status==CommitStatus::Saved);
            auto* meter=manager->contentHost(container->id())->meterSurfaces().first();
            auto* item=qobject_cast<FilterDisplayItem*>(meter->items().first()); Q_ASSERT(item);
            return std::tuple{container,meter,item};
        };
        const auto [aContainer, aMeter, aItem] = addMini(1);
        const auto [aDuplicate, duplicateMeter, duplicateItem] = addMini(1);
        const auto [bContainer, bMeter, initialBItem] = addMini(2);
        QPointer<FilterDisplayItem> bItem=initialBItem;
        ContainerWidget* headerContainer = manager->createContainer(1, DockMode::Floating);
        // An existing applet may itself contain header/body meter descendants.
        // Explicitly detach the borrowed TCI view for this fixture; attaching
        // another widget while the live singleton exists is correctly refused.
        auto* panel=new AppletPanelWidget(&window);
        auto document=manager->workspaceStore()->snapshot();
        for(auto& c:document.containers) { for(int i=c.contents.size()-1;i>=0;--i) { if(c.contents[i].typeId=="applet:tci") { c.contents.removeAt(i); } } }
        for(auto& c:document.containers) { if(c.id==headerContainer->id()) { c.contents={manager->contentRegistry()->makeEntry("applet:tci")}; } }
        QCOMPARE(manager->commitWorkspace(document,document.revision).status,CommitStatus::Saved);
        manager->contentRegistry()->attachSingleton("applet:tci",nullptr);
        manager->contentRegistry()->attachSingleton("applet:tci",panel); manager->contentRegistry()->setAvailable("applet:tci",true);
        auto* headerMeter = new MeterWidget(); auto* headerItem = new FilterDisplayItem(); headerMeter->addItem(headerItem);
        panel->setHeaderWidget(headerMeter, QStringLiteral("Filter"));
        auto* bodyMeter = new MeterWidget(); auto* bodyItem = new FilterDisplayItem(); bodyMeter->addItem(bodyItem);
        panel->addWidget(bodyMeter, QStringLiteral("Second filter"));
        QVERIFY(aMeter->shouldRender(aItem));
        QVERIFY(bMeter->shouldRender(bItem));
        QVERIFY(!aContainer->isHidden());
        QTRY_VERIFY(!aMeter->isHidden());
        QCoreApplication::processEvents();
        QTRY_COMPARE(window.miniProducerCountForTest(), 2);
        QVERIFY(headerContainer->isVisible());
        QTRY_VERIFY(headerMeter->isVisible());
        QTRY_VERIFY(bodyMeter->isVisible());

        QVector<float> bins(4096, 1.0e-9f);
        const double binHz = 192'000.0 / bins.size();
        const double sourceLow = sourceHz - 96'000.0;
        bins[int(std::lround((sourceHz - 11'000.0 - sourceLow) / binHz))] = 1.0f;
        bins[int(std::lround((sourceHz + 14'000.0 - sourceLow) / binHz))] = 0.5f;
        auto feed = [&] { window.fftEnginePoolForTest()->fftFrameReady(stream, bins, 1.0, 0.0); };
        QTRY_VERIFY([&] {
            feed();
            return aItem->frameAvailable() && duplicateItem->frameAvailable()
                && bItem->frameAvailable() && headerItem->frameAvailable()
                && bodyItem->frameAvailable();
        }());
        QCOMPARE(aItem->framePixels(), 1024);
        QCOMPARE(aItem->frameCentreHz(), a->frequency());
        QCOMPARE(duplicateItem->frameCentreHz(), a->frequency());
        QCOMPARE(headerItem->frameCentreHz(), a->frequency());
        QCOMPARE(bodyItem->frameCentreHz(), a->frequency());
        QCOMPARE(bItem->frameCentreHz(), b->frequency());
        QCOMPARE(aItem->tracePeakPixelForTest(), duplicateItem->tracePeakPixelForTest());
        QVERIFY(aItem->tracePeakPixelForTest() != bItem->tracePeakPixelForTest());

        // A header installed after the panel became container content must
        // participate in the same producer, and group hiding must retire its
        // visible frame even while another A container remains active.
        headerItem->setDisplayGroup(2);
        headerMeter->setDisplayGroup(1);
        QTRY_VERIFY(!headerItem->frameAvailable());
        QCOMPARE(window.miniProducerCountForTest(), 2);
        headerMeter->setDisplayGroup(0);
        QTRY_VERIFY([&] { feed(); return headerItem->frameAvailable(); }());
        headerMeter->removeItem(headerItem);
        QTRY_VERIFY(!headerItem->frameAvailable());
        panel->clearHeaderWidget();
        bodyMeter->removeItem(bodyItem);
        QTRY_VERIFY(!bodyItem->frameAvailable());

        manager->setContainerVisible(aContainer->id(), false);
        QTRY_VERIFY(!aItem->frameAvailable());
        QCOMPARE(window.miniProducerCountForTest(), 2); // duplicate A still consumes
        duplicateMeter->removeItem(duplicateItem);
        QTRY_COMPARE(window.miniProducerCountForTest(), 1);
        QCOMPARE(window.miniProducerCountForTest(), 1); // B only
        document=manager->workspaceStore()->snapshot();
        for(auto& c:document.containers) { if(c.id==bContainer->id()) { c.config["rxSource"]=1; } }
        QCOMPARE(manager->commitWorkspace(document,document.revision).status,CommitStatus::Saved);
        QVERIFY(bItem.isNull()); // source replacement synchronously retires the old consumer
        bItem=qobject_cast<FilterDisplayItem*>(manager->contentHost(bContainer->id())->meterSurfaces().first()->items().first());
        QVERIFY(bItem); QTRY_COMPARE(window.miniProducerCountForTest(), 1);
        QTRY_VERIFY(!bItem->frameAvailable());
        QTest::qWait(35); // only the 30 fps local cadence, not a test deadline
        feed();
        QTRY_VERIFY(bItem->frameAvailable());
        QCOMPARE(bItem->frameCentreHz(), a->frequency());
        manager->destroyContainer(aContainer->id());
        manager->destroyContainer(aDuplicate->id());
        manager->destroyContainer(bContainer->id());
        manager->destroyContainer(headerContainer->id());
        QTRY_COMPARE(window.miniProducerCountForTest(), 0);
    }
    // C2: MOX reaches a filter display only on the transmit slice (Thetis
    // m.MOX = rx == m.RX && newMox, MeterManager.cs:5264). Slice B is the
    // transmit slice; a display on Slice A keeps showing its RX filter and a
    // drag changes Slice A's RX filter only. Nothing keys and TransmitModel
    // is unchanged.
    void nonTransmitSliceDisplayEditsItsOwnRxFilter()
    {
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] { RadioDiscovery::clearHoldOffForTest(); });
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        window.resize(1280, 800);
        window.show();
        model->setConnectionStateForTest(ConnectionState::Connected);
        while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
        SliceModel* a = model->sliceById(0);
        SliceModel* b = model->sliceById(1);
        QVERIFY(a && b);
        const int stream = a->streamIndex();
        QVERIFY(stream >= 0);
        const double sourceHz = model->streamCentreHz(stream);
        a->setFrequency(sourceHz - 12'000.0);
        b->setFrequency(sourceHz + 12'000.0);
        a->setDspMode(DSPMode::USB);
        b->setDspMode(DSPMode::USB);
        a->setFilter(200, 2600);
        b->setFilter(300, 2400);
        TransmitModel& tx = model->transmitModel();
        tx.setFilterLow(100);
        tx.setFilterHigh(2900);
        model->requestTxHandoffToSlice(1);
        QCOMPARE(model->txBoundSlice(), b);

        auto* manager = window.findChild<ContainerManager*>();
        QVERIFY(manager);
        const auto addMini = [manager](int rxSource) {
            ContainerWidget* container = manager->createContainer(rxSource, DockMode::Floating);
            auto document=manager->workspaceStore()->snapshot();
            for(auto& c:document.containers) { if(c.id==container->id()) { c.contents={manager->contentRegistry()->makeEntry("FILTERDISPLAY")}; } }
            const auto result=manager->commitWorkspace(document,document.revision); Q_ASSERT(result.status==CommitStatus::Saved);
            auto* meter=manager->contentHost(container->id())->meterSurfaces().first();
            auto* item=qobject_cast<FilterDisplayItem*>(meter->items().first()); Q_ASSERT(item);
            return std::tuple{container,meter,item};
        };
        const auto [aContainer, aMeter, aItem] = addMini(1);
        const auto [bContainer, bMeter, bItem] = addMini(2);
        QTRY_COMPARE(window.miniProducerCountForTest(), 2);
        QVector<float> bins(4096, 1.0e-9f);
        auto feed = [&] { window.fftEnginePoolForTest()->fftFrameReady(stream, bins, 1.0, 0.0); };
        QTRY_VERIFY([&] { feed(); return aItem->frameAvailable() && bItem->frameAvailable(); }());
        QVERIFY(!aItem->sliceFilterState().transmitSlice);
        QVERIFY(bItem->sliceFilterState().transmitSlice);
        QVERIFY(aItem->requestsEnabled());

        // The window's MOX shown on the containers (the display state only;
        // nothing is keyed here).
        QSignalSpy mox(&tx, &TransmitModel::moxChanged);
        QSignalSpy tune(&tx, &TransmitModel::tuneChanged);
        QSignalSpy txFilter(&tx, &TransmitModel::filterChanged);
        manager->setTransmitting(true);
        constexpr int kW = 800;
        constexpr int kH = 300;
        const auto la = aItem->thetisLayout(kW, kH);
        QVERIFY(la.valid);
        QCOMPARE(la.lowHz, 200.0f);
        QCOMPARE(la.highHz, 2600.0f);
        const auto lb = bItem->thetisLayout(kW, kH);
        QCOMPARE(lb.lowHz, 100.0f);
        QCOMPARE(lb.highHz, 2900.0f);

        const QPointF grab = (la.highTop + la.highBot) / 2.0;
        const float toX = la.centre + aItem->hzToPixels(2800.0, la.pixelSpan, la.hzSpan);
        const QPointF to(toX, grab.y());
        const auto send = [&](QEvent::Type type, const QPointF& pos, Qt::MouseButton button,
                              Qt::MouseButtons buttons) {
            QMouseEvent event(type, pos, pos, button, buttons, Qt::ControlModifier);
            if (type == QEvent::MouseButtonPress) { aItem->handleMousePress(&event, kW, kH); }
            else if (type == QEvent::MouseButtonRelease) { aItem->handleMouseRelease(&event, kW, kH); }
            else { aItem->handleMouseMove(&event, kW, kH); }
        };
        const int bLow = b->filterLow();
        const int bHigh = b->filterHigh();
        const auto expected = a->filterForRequest(200, 2800);
        send(QEvent::MouseMove, grab, Qt::NoButton, Qt::NoButton);
        send(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
        send(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
        QCOMPARE(a->filterLow(), expected.first);
        QCOMPARE(a->filterHigh(), expected.second);
        QCOMPARE(b->filterLow(), bLow);
        QCOMPARE(b->filterHigh(), bHigh);
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 2900);
        QCOMPARE(txFilter.count(), 0);
        QCOMPARE(mox.count(), 0);
        QCOMPARE(tune.count(), 0);
        QVERIFY(!model->isTransmitting());
        manager->setTransmitting(false);
        manager->destroyContainer(aContainer->id());
        manager->destroyContainer(bContainer->id());
        QTRY_COMPARE(window.miniProducerCountForTest(), 0);
    }
};

QTEST_MAIN(TestMiniDisplayComposition)
#include "tst_mini_display_composition.moc"
