// no-port-check: NereusSDR-original offscreen container composition test.
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
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/MeterWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

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
        const auto addMini = [manager](int rxSource) {
            ContainerWidget* container = manager->createContainer(rxSource, DockMode::Floating);
            auto* meter = new MeterWidget();
            container->setContent(meter);
            auto* item = new FilterDisplayItem();
            meter->addItem(item);
            manager->setContainerVisible(container->id(), true);
            meter->show();
            return std::tuple{container, meter, item};
        };
        const auto [aContainer, aMeter, aItem] = addMini(1);
        const auto [aDuplicate, duplicateMeter, duplicateItem] = addMini(1);
        const auto [bContainer, bMeter, bItem] = addMini(2);
        ContainerWidget* headerContainer = manager->createContainer(1, DockMode::Floating);
        auto* panel = new AppletPanelWidget();
        headerContainer->setContent(panel);
        manager->setContainerVisible(headerContainer->id(), true);
        auto* headerMeter = new MeterWidget();
        auto* headerItem = new FilterDisplayItem();
        headerMeter->addItem(headerItem);
        panel->setHeaderWidget(headerMeter, QStringLiteral("Filter"));
        auto* bodyMeter = new MeterWidget();
        auto* bodyItem = new FilterDisplayItem();
        bodyMeter->addItem(bodyItem);
        panel->addWidget(bodyMeter, QStringLiteral("Second filter"));
        QVERIFY(aMeter->shouldRender(aItem));
        QVERIFY(bMeter->shouldRender(bItem));
        QVERIFY(!aContainer->isHidden());
        QVERIFY(!aMeter->isHidden());
        QCoreApplication::processEvents();
        QTRY_COMPARE(window.miniProducerCountForTest(), 2);
        QVERIFY(headerContainer->isVisible());
        QVERIFY(headerMeter->isVisible());
        QVERIFY(bodyMeter->isVisible());

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
        bContainer->setRxSource(1);
        QTRY_COMPARE(window.miniProducerCountForTest(), 1);
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
};

QTEST_MAIN(TestMiniDisplayComposition)
#include "tst_mini_display_composition.moc"
