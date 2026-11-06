// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_panel.cpp  (NereusSDR)
// =================================================================
//
// The band stack panel's shell and its place beside the pans, on a real
// MainWindow offscreen, in both looks: the pane's shape, free containers and
// saved sizes across an open and close, the slide, width, the keys, the dots
// and menu, the greyed reasons, and one toast per refusal or report.
//
// NEREUS_BANDSTACK_CAPTURE_DIR, when set, saves the panel closed, open and
// greyed in each look there, to read by eye.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>

#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/UnbuiltFeatureList.h"
#include "core/settings/SettingsScope.h"
#include "fakes/FakeBandStackAccess.h"
#include "gui/MainWindow.h"
#include "gui/bandstack/BandStackPanel.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr QSize kWindowSize(1400, 860);
// Wide enough that the status bar's fold ladder leaves the dots showing.
constexpr QSize kWideWindowSize(2200, 860);
constexpr QRect kLeftAnchored(180, 90, 300, 220);
constexpr QRect kRightAnchored(640, 420, 300, 200);
constexpr int kShownMs = 400;
constexpr int kSettleTimeoutMs = 10000;

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

std::unique_ptr<MainWindow> buildWindow(bool workspaceLook, QSize size = kWindowSize)
{
    WorkspacePreview::setEnabledForTest(workspaceLook);
    auto& app = *qobject_cast<QApplication*>(QCoreApplication::instance());
    applyDarkPalette(app);
    applyAppBaselineQss(app);
    if (workspaceLook) {
        applyWorkspacePalette(app);
    }
    auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                               MainWindow::ConnectionStartup::Deferred);
    window->radioModel()->setBoardForTest(HPSDRHW::Saturn);
    window->resize(size);
    window->show();
    window->resize(size);
    settle();
    // The fold ladder reads the bar's width in the window's resize, before the
    // layout has given the bar its new width: one more resize lets it see it.
    window->resize(size + QSize(1, 0));
    settle();
    return window;
}

ContainerDocument freeContainer(const QString& id, const QRect& geometry, AxisLock anchor)
{
    ContainerDocument d;
    d.id = id;
    d.name = id;
    d.layout = ContentLayout::VerticalStack;
    d.dockMode = DockMode::OverlayDocked;
    d.anchor = anchor;
    d.geometry = geometry;
    d.config = {{QStringLiteral("enabled"), true},
                {QStringLiteral("showOnRx"), true},
                {QStringLiteral("showOnTx"), true},
                {QStringLiteral("border"), true},
                {QStringLiteral("pinOnTop"), false}};
    return d;
}

QWidget* panColumn(MainWindow& window)
{
    return window.findChild<QWidget*>(QStringLiteral("panColumn"));
}

QLabel* dots(MainWindow& window)
{
    return window.findChild<QLabel*>(QStringLiteral("statusBandStackDots"));
}

QAction* menuEntry(MainWindow& window)
{
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("Band &Stacking...")) {
            return action;
        }
    }
    return nullptr;
}

int toastCount(MainWindow& window, const QString& message)
{
    int count = 0;
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->message() == message) {
            ++count;
        }
    }
    return count;
}

// What a save of the window's layout writes, one string per value.
QStringList savedLayout(MainWindow& window, ContainerManager& manager)
{
    QStringList saved;
    manager.saveState();
    saved << AppSettings::instance().value(QStringLiteral("MainSplitterSizes")).toString();
    if (auto* area = window.findChild<EdgeDockArea*>()) {
        for (DockEdge edge : {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom}) {
            saved << QString::number(area->thickness(edge));
        }
    }
    if (auto* splitter = window.findChild<QSplitter*>()) {
        saved << QString::number(splitter->count());
    }
    return saved;
}

void clearPanelSettings()
{
    auto& settings = AppSettings::instance();
    settings.remove(QString::fromLatin1(BandStackPanel::kOpenKey));
    settings.remove(QString::fromLatin1(BandStackPanel::kWidthKey));
    settings.remove(QString::fromLatin1(BandStackPanel::kGroupByBandKey));
    settings.remove(QString::fromLatin1(BandStackPanel::kShowOnSpectrumKey));
}

void capture(MainWindow& window, const QString& name)
{
    const QString directory = qEnvironmentVariable("NEREUS_BANDSTACK_CAPTURE_DIR");
    if (directory.isEmpty()) {
        return;
    }
    settle();
    QVERIFY(window.grab().save(QDir(directory).filePath(name + QStringLiteral(".png"))));
}

} // namespace

class TstBandStackPanel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        RadioDiscovery::clearHoldOffForTest();
        QVERIFY(qEnvironmentVariableIsSet("NEREUS_TEST_HOME"));
    }

    void init()
    {
        clearPanelSettings();
        BandStackPanel::setReducedMotionForTest(false);
    }

    void cleanup()
    {
        BandStackPanel::setReducedMotionForTest(std::nullopt);
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
        RadioDiscovery::clearHoldOffForTest();
    }

    void thePaneHoldsThePanelAndThePanColumn_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("workspace look") << true;
        QTest::newRow("classic look") << false;
    }

    void thePaneHoldsThePanelAndThePanColumn()
    {
        QFETCH(bool, workspaceLook);
        const std::unique_ptr<MainWindow> window = buildWindow(workspaceLook);
        auto* panel = window->findChild<BandStackPanel*>();
        QVERIFY(panel);
        QVERIFY(!panel->isOpen());
        QVERIFY(!panel->isVisible());
        QWidget* column = panColumn(*window);
        QVERIFY(column);
        QCOMPARE(column->minimumWidth(), 400);

        QWidget* pane = column->parentWidget();
        QVERIFY(pane);
        QCOMPARE(panel->parentWidget(), pane);
        auto* paneLayout = qobject_cast<QHBoxLayout*>(pane->layout());
        QVERIFY(paneLayout);
        QCOMPARE(paneLayout->itemAt(0)->widget(), panel);
        QCOMPARE(paneLayout->itemAt(1)->widget(), column);
        QCOMPARE(pane->minimumWidth(), 0);

        if (auto* area = window->findChild<EdgeDockArea*>()) {
            QCOMPARE(area->center(), pane);
        }

        // Opens left of the pans, inside the pane, and the pans give way.
        const int columnWidthClosed = column->width();
        panel->setOpen(true, false);
        settle();
        QCOMPARE(panel->x(), 0);
        QCOMPARE(panel->width(), BandStackPanel::kDefaultWidth);
        QCOMPARE(column->x(), BandStackPanel::kDefaultWidth);
        QCOMPARE(column->width(), columnWidthClosed - BandStackPanel::kDefaultWidth);
    }

    void freeContainersAndSavedSizesSurviveARoundTrip_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("workspace look") << true;
        QTest::newRow("classic look") << false;
    }

    void freeContainersAndSavedSizesSurviveARoundTrip()
    {
        QFETCH(bool, workspaceLook);
        const std::unique_ptr<MainWindow> window = buildWindow(workspaceLook);
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        WorkspaceDocument document = manager->workspaceStore()->snapshot();
        document.containers.append(
            freeContainer(QStringLiteral("free-left"), kLeftAnchored, AxisLock::Left));
        document.containers.append(
            freeContainer(QStringLiteral("free-right"), kRightAnchored, AxisLock::Right));
        QCOMPARE(manager->commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        settle();
        ContainerWidget* left = manager->container(QStringLiteral("free-left"));
        ContainerWidget* right = manager->container(QStringLiteral("free-right"));
        QVERIFY(left && right);
        QWidget* column = panColumn(*window);
        QCOMPARE(left->parentWidget(), column);
        QCOMPARE(right->parentWidget(), column);
        // Past ContainerManager's first-layout settling: until then a free
        // container's baseline follows the pane on purpose.
        QTest::qWait(kShownMs);
        QVERIFY(QTest::qWaitFor([manager] { return !manager->geometryCommitPendingForTest(); },
                                kSettleTimeoutMs));
        const QRect leftBefore = left->geometry();
        const QRect rightBefore = right->geometry();
        const QStringList savedBefore = savedLayout(*window, *manager);

        auto* panel = window->findChild<BandStackPanel*>();
        const int columnBefore = column->width();
        panel->setOpen(true);
        QTRY_COMPARE(panel->width(), BandStackPanel::kDefaultWidth);
        settle();
        QCOMPARE(column->width(), columnBefore - BandStackPanel::kDefaultWidth);
        // A left-anchored container stays; a right-anchored one follows the edge.
        QCOMPARE(left->geometry(), leftBefore);
        QCOMPARE(right->geometry(), rightBefore.translated(-BandStackPanel::kDefaultWidth, 0));

        panel->setOpen(false);
        QTRY_VERIFY(!panel->isVisible());
        settle();
        QCOMPARE(column->width(), columnBefore);
        QCOMPARE(left->geometry(), leftBefore);
        QCOMPARE(right->geometry(), rightBefore);
        QVERIFY(QTest::qWaitFor([manager] { return !manager->geometryCommitPendingForTest(); },
                                kSettleTimeoutMs));
        QCOMPARE(savedLayout(*window, *manager), savedBefore);
    }

    void slidesOverItsTimeAndAtOnceWithReducedMotion()
    {
        QWidget host;
        host.resize(600, 400);
        auto* layout = new QHBoxLayout(&host);
        BandStackPanel panel(nullptr, &host);
        layout->addWidget(&panel);
        layout->addStretch(1);
        host.show();
        settle();

        QElapsedTimer timer;
        timer.start();
        panel.setOpen(true);
        QVERIFY(panel.isOpen());
        QVERIFY(panel.width() < BandStackPanel::kDefaultWidth);
        QTRY_COMPARE(panel.width(), BandStackPanel::kDefaultWidth);
        QVERIFY2(timer.elapsed() >= BandStackPanel::kSlideMs - 20,
                 qPrintable(QString::number(timer.elapsed())));
        QCOMPARE(BandStackPanel::kSlideMs, 140);

        panel.setOpen(false);
        QVERIFY(panel.width() > 0);
        QTRY_VERIFY(!panel.isVisible());
        QCOMPARE(panel.width(), 0);

        BandStackPanel::setReducedMotionForTest(true);
        panel.setOpen(true);
        QCOMPARE(panel.width(), BandStackPanel::kDefaultWidth);
        panel.setOpen(false);
        QCOMPARE(panel.width(), 0);
        QVERIFY(!panel.isVisible());
    }

    void widthKeepsItsRangeAndFollowsTheGrip()
    {
        QWidget host;
        host.resize(600, 400);
        auto* layout = new QHBoxLayout(&host);
        BandStackPanel panel(nullptr, &host);
        layout->addWidget(&panel);
        layout->addStretch(1);
        host.show();
        panel.setOpen(true, false);
        settle();
        QCOMPARE(panel.panelWidth(), 158);

        panel.setPanelWidth(500);
        QCOMPARE(panel.panelWidth(), 240);
        QCOMPARE(panel.width(), 240);
        panel.setPanelWidth(10);
        QCOMPARE(panel.panelWidth(), 120);
        QCOMPARE(panel.width(), 120);
        panel.setPanelWidth(158);

        auto* grip = panel.findChild<QWidget*>(QStringLiteral("bandStackPanelGrip"));
        QVERIFY(grip);
        const auto send = [grip](QEvent::Type type, int globalX) {
            QMouseEvent event(type, QPointF(2, 10), QPointF(globalX, 10), Qt::LeftButton,
                              type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                              Qt::NoModifier);
            QCoreApplication::sendEvent(grip, &event);
        };
        send(QEvent::MouseButtonPress, 300);
        send(QEvent::MouseMove, 342);
        QCOMPARE(panel.width(), 200);
        send(QEvent::MouseMove, 900);
        QCOMPARE(panel.width(), 240);
        send(QEvent::MouseButtonRelease, 900);
        send(QEvent::MouseMove, 300);
        QCOMPARE(panel.width(), 240);
        QCOMPARE(AppSettings::instance().value(QString::fromLatin1(BandStackPanel::kWidthKey)).toInt(), 240);
    }

    void keysSurviveARelaunchAndStayOnThisDevice()
    {
        {
            const std::unique_ptr<MainWindow> window = buildWindow(true);
            auto* panel = window->findChild<BandStackPanel*>();
            QVERIFY(panel);
            QVERIFY(panel->groupByBand());
            QVERIFY(!panel->showOnSpectrum());
            panel->setPanelWidth(201);
            panel->setOpen(true, false);
            panel->setGroupByBand(false);
            panel->setShowOnSpectrum(true);
            QVERIFY(AppSettings::instance().save());
            window->close();
        }
        QCOMPARE(AppSettings::instance().value(QStringLiteral("BandStackPanel/Open")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("BandStackPanel/GroupByBand")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("BandStackPanel/ShowOnSpectrum")).toString(),
                 QStringLiteral("True"));
        AppSettings::instance().load();
        {
            const std::unique_ptr<MainWindow> window = buildWindow(true);
            auto* panel = window->findChild<BandStackPanel*>();
            QVERIFY(panel);
            QVERIFY(panel->isOpen());
            QCOMPARE(panel->width(), 201);
            QVERIFY(panel->isVisible());
            QVERIFY(!panel->groupByBand());
            QVERIFY(panel->showOnSpectrum());
            window->close();
        }
        for (const char* key : {BandStackPanel::kOpenKey, BandStackPanel::kWidthKey,
                                BandStackPanel::kGroupByBandKey, BandStackPanel::kShowOnSpectrumKey}) {
            QCOMPARE(classifySettingsKey(QString::fromLatin1(key)), SettingsScope::OperatorLocal);
        }
    }

    void dotsMenuAndCloseToggleIt()
    {
        BandStackPanel::setReducedMotionForTest(true);
        const std::unique_ptr<MainWindow> window = buildWindow(true, kWideWindowSize);
        auto* panel = window->findChild<BandStackPanel*>();
        QLabel* statusDots = dots(*window);
        QAction* entry = menuEntry(*window);
        QVERIFY(panel && statusDots && entry);
        QVERIFY(statusDots->isVisible());
        QVERIFY(entry->isVisible());
        QVERIFY(entry->isEnabled());
        for (const UnbuiltFeatures::Entry& item : UnbuiltFeatures::all()) {
            QVERIFY(item.key != QStringLiteral("band-stack"));
        }

        QTest::mouseClick(statusDots, Qt::LeftButton);
        QVERIFY(panel->isOpen());
        QTest::mouseClick(statusDots, Qt::LeftButton);
        QVERIFY(!panel->isOpen());

        entry->trigger();
        QVERIFY(panel->isOpen());

        auto* close = panel->findChild<QToolButton*>(QStringLiteral("bandStackPanelClose"));
        QVERIFY(close);
        close->click();
        QVERIFY(!panel->isOpen());

        // The hot keys find it by name: a public slot with no arguments.
        QVERIFY(QMetaObject::invokeMethod(window.get(), "bandStackPanel"));
        QVERIFY(panel->isOpen());
    }

    void greyedStatesCarryTheirReasons_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("workspace look") << true;
        QTest::newRow("classic look") << false;
    }

    void greyedStatesCarryTheirReasons()
    {
        QFETCH(bool, workspaceLook);
        const QString look = workspaceLook ? QStringLiteral("workspace") : QStringLiteral("classic");
        BandStackPanel::setReducedMotionForTest(true);
        const std::unique_ptr<MainWindow> window = buildWindow(workspaceLook, kWideWindowSize);
        auto* panel = window->findChild<BandStackPanel*>();
        QVERIFY(panel);
        QLabel* statusDots = dots(*window);
        QAction* entry = menuEntry(*window);
        auto* reasonLabel = panel->findChild<QLabel*>(QStringLiteral("bandStackPanelReason"));
        auto* rows = panel->findChild<QScrollArea*>(QStringLiteral("bandStackPanelRows"));
        auto* menu = panel->findChild<QToolButton*>(QStringLiteral("bandStackPanelMenu"));
        auto* close = panel->findChild<QToolButton*>(QStringLiteral("bandStackPanelClose"));
        QVERIFY(statusDots && entry && reasonLabel && rows && menu && close);

        // A window with no radio, on the real access.
        QCOMPARE(window->bandStackReason(), BandStackAccess::notConnectedReason());
        capture(*window, QStringLiteral("%1-closed").arg(look));
        panel->setOpen(true, false);
        settle();
        QCOMPARE(panel->unavailableReason(), QStringLiteral("Connect to a radio to use band stacks."));
        QCOMPARE(statusDots->toolTip(), QStringLiteral("Connect to a radio to use band stacks."));
        QCOMPARE(entry->toolTip(), QStringLiteral("Connect to a radio to use band stacks."));
        QVERIFY(reasonLabel->isVisible());
        QVERIFY(!rows->isEnabled());
        QVERIFY(!menu->isEnabled());
        QVERIFY(close->isEnabled());
        QVERIFY(statusDots->isVisible());
        capture(*window, QStringLiteral("%1-greyed-no-connection").arg(look));

        FakeBandStackAccess fake;
        window->setBandStackAccessForTest(&fake);
        fake.setState(BandStackAccess::State::Ready);
        settle();
        QCOMPARE(window->bandStackReason(), QString());
        QVERIFY(!reasonLabel->isVisible());
        QVERIFY(rows->isEnabled());
        QVERIFY(menu->isEnabled());
        QCOMPARE(statusDots->toolTip(), QStringLiteral("Band stack"));
        QVERIFY(!entry->toolTip().contains(QStringLiteral("Connect")));
        {
            // A named slice in the header for the capture; the window's own has none.
            SliceModel slice(0);
            slice.setFrequency(7185000.0);
            panel->setSlice(&slice);
            capture(*window, QStringLiteral("%1-open").arg(look));
            panel->setSlice(nullptr);
        }

        fake.setState(BandStackAccess::State::NeedsNewerCore, BandStackAccess::needsNewerCoreReason());
        settle();
        const QString older = QStringLiteral("This Core does not have band stacks. Updating the Core may help.");
        QCOMPARE(panel->unavailableReason(), older);
        QCOMPARE(reasonLabel->text(), older);
        QCOMPARE(statusDots->toolTip(), older);
        QCOMPARE(panel->toolTip(), older);
        QVERIFY(!rows->isEnabled());
        QVERIFY(statusDots->isVisible());
        capture(*window, QStringLiteral("%1-greyed-older-core").arg(look));

        // A change the access did not signal arrives through the link's signal.
        fake.blockSignals(true);
        fake.setState(BandStackAccess::State::NotConnected, BandStackAccess::notConnectedReason());
        fake.blockSignals(false);
        emit window->radioModel()->stationLinkStateChanged();
        QCOMPARE(panel->unavailableReason(), BandStackAccess::notConnectedReason());
        // And through the connection's.
        fake.blockSignals(true);
        fake.setState(BandStackAccess::State::Ready);
        fake.blockSignals(false);
        emit window->radioModel()->connectionStateChanged(ConnectionState::Connected);
        QCOMPARE(panel->unavailableReason(), QString());
        fake.setState(BandStackAccess::State::NotConnected, BandStackAccess::notConnectedReason());

        // The panel can still be closed and reopened while greyed.
        close->click();
        QVERIFY(!panel->isOpen());
        QTest::mouseClick(statusDots, Qt::LeftButton);
        QVERIFY(panel->isOpen());
        window->setBandStackAccessForTest(window->radioModel()->bandStack());
    }

    void refusalsAndReportsToastOnce()
    {
        const std::unique_ptr<MainWindow> window = buildWindow(true);
        FakeBandStackAccess first;
        FakeBandStackAccess second;
        window->setBandStackAccessForTest(&first);
        const QString reason = QStringLiteral("Band 40m ignored: the slice is locked. Unlock it to change bands.");
        const QString report = QStringLiteral("40m: 7.185 LSB, 5 of 6");
        emit first.refused(reason);
        QCOMPARE(toastCount(*window, reason), 1);
        emit first.reported(report);
        QCOMPARE(toastCount(*window, report), 1);

        qDeleteAll(window->findChildren<StatusToast*>());
        window->setBandStackAccessForTest(&second);
        window->setBandStackAccessForTest(&second);
        emit first.refused(reason);
        QCOMPARE(toastCount(*window, reason), 0);
        emit second.refused(reason);
        QCOMPARE(toastCount(*window, reason), 1);
        window->setBandStackAccessForTest(window->radioModel()->bandStack());
    }

    void theHeaderNamesTheSliceAndItsBand()
    {
        BandStackPanel panel(nullptr);
        panel.setOpen(true, false);
        auto* flag = panel.findChild<QLabel*>(QStringLiteral("bandStackPanelSliceFlag"));
        auto* onBand = panel.findChild<QLabel*>(QStringLiteral("bandStackPanelOnBand"));
        QVERIFY(flag && onBand);
        QCOMPARE(onBand->text(), QStringLiteral("No slice"));

        auto slice = std::make_unique<SliceModel>(0);
        slice->setFrequency(7185000.0);
        panel.setSlice(slice.get());
        QCOMPARE(flag->text(), QStringLiteral("A"));
        QCOMPARE(flag->toolTip(), QStringLiteral("Slice A"));
        QCOMPARE(onBand->text(), QStringLiteral("on 40m"));
        slice->setFrequency(14200000.0);
        QCOMPARE(onBand->text(), QStringLiteral("on 20m"));

        slice.reset();
        panel.setSlice(nullptr);
        QCOMPARE(onBand->text(), QStringLiteral("No slice"));
    }

    void rowsHaveAPlaceAndTheViewOptionsDefault()
    {
        BandStackPanel panel(nullptr);
        QVERIFY(panel.rowsLayout());
        auto* row = new QLabel(QStringLiteral("row"));
        panel.rowsLayout()->addWidget(row);
        QVERIFY(row->parentWidget());
        QCOMPARE(row->parentWidget()->objectName(), QStringLiteral("bandStackPanelRowsHost"));
        QVERIFY(panel.groupByBand());
        QVERIFY(!panel.showOnSpectrum());
    }
};

QTEST_MAIN(TstBandStackPanel)
#include "tst_bandstack_panel.moc"
