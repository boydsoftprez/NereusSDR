// no-port-check: NereusSDR-original offscreen regression for the meter poller during MainWindow teardown.
// Modification history (NereusSDR):
//   2026-10-09 - The meter poller publishes nothing while the window's
//                RadioModel is being destroyed: its destroyed hook reached
//                the window's source lambdas, which read the dying model's
//                deleted slices (exit -11 in the visual preview's
//                dial-addable--add-content). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QLoggingCategory>
#include <QPointer>
#include <chrono>
#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

} // namespace

class TstMainWindowMeterTeardown : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("mainwindow-meter-teardown")));
        qApp->setStyle(QStringLiteral("Fusion"));
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{30});
    }
    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    // The saved polling delay applies when the window starts, not when
    // Setup's Multimeter page first opens.
    void theSavedPollingDelayAppliesAtStartup()
    {
        AppSettings::instance().setValue(QStringLiteral("MultimeterDelayMs"), 500);
        auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                                   MainWindow::ConnectionStartup::Deferred);
        QVERIFY(window->radioModel()->meterPoller());
        QCOMPARE(window->radioModel()->meterPoller()->intervalMs(), 500);
    }

    // A posed radio (no connection is made): the poller knows the board, so
    // losing the model changes what it publishes. Before the fix that change
    // was published from QObject::destroyed, inside the model's teardown.
    void pollerPublishesNothingWhileTheModelGoes()
    {
        auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                                   MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window->radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        RadioInfo info;
        info.boardType = HPSDRHW::Saturn;
        info.macAddress = QStringLiteral("02:00:00:00:00:32");
        model->setLastRadioInfoForTest(info);
        model->setConnectionStateForTest(ConnectionState::Connected);
        window->resize(1280, 800);
        window->show();
        settle();

        auto* poller = window->findChild<MeterPoller*>();
        QVERIFY(poller != nullptr);
        QCOMPARE(poller->bindingSupport(MeterBinding::HwAmps) == MeterItem::BindingSupport::Unknown, false);

        const QPointer<RadioModel> alive(model);
        int whileDying = 0;
        QStringList seen;
        connect(poller, &MeterPoller::bindingSupportChanged, this,
                [&alive, &whileDying, &seen](int binding, MeterItem::BindingSupport) {
            if (alive.isNull()) {
                ++whileDying;
                seen << QString::number(binding);
            }
        });
        window.reset();
        QVERIFY2(whileDying == 0,
                 qPrintable(QStringLiteral("%1 binding support change(s) published while the RadioModel was "
                                           "being destroyed (bindings %2)")
                                .arg(whileDying).arg(seen.join(QLatin1Char(',')))));
    }
};

QTEST_MAIN(TstMainWindowMeterTeardown)
#include "tst_mainwindow_meter_teardown.moc"
