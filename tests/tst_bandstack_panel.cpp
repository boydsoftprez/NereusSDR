// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_panel.cpp  (NereusSDR)
// =================================================================
//
// The band stack panel's shell and its place beside the pans, on a real
// MainWindow offscreen, in both looks: the pane's shape, free containers and
// saved sizes across an open and close, the slide, width, the keys, the dots
// and menu, the greyed reasons, and one toast per refusal or report. Then the
// rows on a fake access: which verb each click, key, drag and menu item sends.
//
// NEREUS_BANDSTACK_CAPTURE_DIR, when set, saves the panel closed, open and
// greyed, and the open band, its menus, a rename, a locked row and a drag, in
// each look there, to read by eye.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-06 - The rows, menus, keys and drag (band stack plan Task 9).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-07 - Five ctest entries of this binary (TestFunctionGroups),
//                 near the 120 s limit as one on the Linux runner.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTimer>
#include <QSplitter>
#include <QToolButton>

#include <functional>
#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/UnbuiltFeatureList.h"
#include "core/settings/SettingsScope.h"
#include "TestFunctionGroups.h"
#include "fakes/FakeBandStackAccess.h"
#include "gui/MainWindow.h"
#include "gui/bandstack/BandStackPanel.h"
#include "gui/bandstack/BandStackRowList.h"
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

constexpr double kOn40mHz = 7185000.0;
constexpr double kOn20mHz = 14225000.0;
constexpr qint64 kViewCenterHz = 7200000;
constexpr qint64 kViewSpanHz = 192000;
constexpr int kDialogWaitMs = 10000;
constexpr int kUndoMs = 5000;
constexpr int kToastCloseWaitMs = 20000;

QJsonObject stackEntry(const QString& id, const QString& band, qint64 hz, const QString& mode,
                       int low, int high, bool locked = false, const QString& name = {})
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("band"), band},
                       {QStringLiteral("frequencyHz"), hz},
                       {QStringLiteral("mode"), mode},
                       {QStringLiteral("filterLowHz"), low},
                       {QStringLiteral("filterHighHz"), high},
                       {QStringLiteral("centerHz"), hz},
                       {QStringLiteral("spanHz"), kViewSpanHz},
                       {QStringLiteral("locked"), locked},
                       {QStringLiteral("seeded"), false},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("lastUsedMs"), 0}};
}

QJsonObject bandOrder(const QStringList& ids, const QString& mode = QStringLiteral("current"),
                      const QString& specific = {})
{
    return QJsonObject{{QStringLiteral("order"), QJsonArray::fromStringList(ids)},
                       {QStringLiteral("returnMode"), mode},
                       {QStringLiteral("specificId"), specific}};
}

// Four spots on 40m: the slice's place, a locked one, one Slice B is on and
// one that is named. Two on 20m. Everything else is empty.
QJsonObject stackDocument()
{
    return QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("region"), QStringLiteral("United States")},
        {QStringLiteral("options"),
         QJsonObject{{QStringLiteral("dupeProtection"), true},
                     {QStringLiteral("expiryMinutes"), 15},
                     {QStringLiteral("dwellSeconds"), 0}}},
        {QStringLiteral("bands"),
         QJsonObject{{QStringLiteral("40m"),
                      bandOrder({QStringLiteral("cw"), QStringLiteral("net"),
                                 QStringLiteral("ssb"), QStringLiteral("dx")},
                                QStringLiteral("specific"), QStringLiteral("dx"))},
                     {QStringLiteral("20m"),
                      bandOrder({QStringLiteral("ft8"), QStringLiteral("phone")})}}},
        {QStringLiteral("entries"),
         QJsonArray{stackEntry(QStringLiteral("cw"), QStringLiteral("40m"), 7030000,
                               QStringLiteral("CWL"), -600, -100),
                    stackEntry(QStringLiteral("net"), QStringLiteral("40m"), 7185000,
                               QStringLiteral("LSB"), -2800, -100),
                    stackEntry(QStringLiteral("ssb"), QStringLiteral("40m"), 7200000,
                               QStringLiteral("LSB"), -2800, -100, true),
                    stackEntry(QStringLiteral("dx"), QStringLiteral("40m"), 7255500,
                               QStringLiteral("LSB"), -2400, -100, false,
                               QStringLiteral("Evening net")),
                    stackEntry(QStringLiteral("ft8"), QStringLiteral("20m"), 14074000,
                               QStringLiteral("DIGU"), 200, 3000),
                    stackEntry(QStringLiteral("phone"), QStringLiteral("20m"), 14225000,
                               QStringLiteral("USB"), 100, 2800, true)}},
        {QStringLiteral("places"),
         QJsonObject{{QStringLiteral("A"), QJsonObject{{QStringLiteral("40m"), QStringLiteral("net")}}},
                     {QStringLiteral("B"), QJsonObject{{QStringLiteral("40m"), QStringLiteral("ssb")}}}}},
        {QStringLiteral("lastExpiry"),
         QJsonObject{{QStringLiteral("batchId"), QString()}, {QStringLiteral("count"), 0}}},
        {QStringLiteral("lastRecall"), QJsonObject{}}};
}

BandStackRowContext stackContext(const QString* blocked = nullptr)
{
    BandStackRowContext context;
    context.viewForSlice = [](int) {
        BandStackView view;
        view.centerHz = kViewCenterHz;
        view.spanHz = kViewSpanHz;
        return view;
    };
    context.slices = [] {
        return QList<BandStackSliceInfo>{{0, QLatin1Char('A'), kOn40mHz},
                                         {1, QLatin1Char('B'), 7200000.0}};
    };
    // The 40m phone segment's colour, for the tint.
    context.planColourAt = [](qint64 hz) {
        return hz >= 7125000 && hz <= 7300000 ? QColor(0x2f, 0x9f, 0x5f) : QColor();
    };
    if (blocked) {
        context.blockedReason = [blocked](int) { return *blocked; };
    }
    return context;
}

// A list on the fake, on 40m as Slice A, in a window's worth of width.
struct RowsFixture {
    FakeBandStackAccess fake;
    QWidget host;
    BandStackRowList* rows{nullptr};

    explicit RowsFixture(const QString* blocked = nullptr)
    {
        fake.setDocument(stackDocument());
        host.resize(158, 500);
        auto* layout = new QVBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        rows = new BandStackRowList(&fake, &host);
        layout->addWidget(rows);
        layout->addStretch(1);
        rows->setContext(stackContext(blocked));
        rows->setSliceId(0);
        rows->setSliceFrequency(kOn40mHz);
        host.show();
        settle();
    }
};

QPoint centerOf(const QRect& rect)
{
    return rect.center();
}

QStringList actionTexts(const QMenu& menu)
{
    QStringList texts;
    for (const QAction* action : menu.actions()) {
        texts << (action->isSeparator() ? QStringLiteral("-")
                                        : action->text().section(QLatin1Char('\t'), 0, 0));
    }
    return texts;
}

QAction* actionNamed(const QMenu& menu, const QString& text)
{
    for (QAction* action : menu.actions()) {
        if (action->text().section(QLatin1Char('\t'), 0, 0) == text) {
            return action;
        }
    }
    return nullptr;
}

// Waits for the confirmation dialog, hands its words back and presses `button`.
// The dialog runs its own event loop, so this has to be set up first.
void answerConfirm(const QString& button, QString* words)
{
    auto* poll = new QTimer(qApp);
    poll->setInterval(5);
    auto* giveUp = new QElapsedTimer;
    giveUp->start();
    QObject::connect(poll, &QTimer::timeout, poll, [=] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog && dialog->objectName() == QStringLiteral("bandStackConfirm")) {
            if (auto* label = dialog->findChild<QLabel*>(QStringLiteral("bandStackConfirmText"))) {
                *words = label->text();
            }
            dialog->findChild<QPushButton*>(button)->click();
        } else if (giveUp->elapsed() < kDialogWaitMs) {
            return;
        }
        delete giveUp;
        poll->deleteLater();
        poll->stop();
    });
    poll->start();
}

// Runs `open`, which shows a menu in its own event loop, and hands back what
// that menu held, closing it.
QStringList textsOfNextPopup(const std::function<void()>& open)
{
    QStringList texts;
    QTimer poll;
    poll.setInterval(5);
    QElapsedTimer giveUp;
    giveUp.start();
    QObject::connect(&poll, &QTimer::timeout, &poll, [&] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            texts = actionTexts(*menu);
            menu->close();
            poll.stop();
        } else if (giveUp.elapsed() >= kDialogWaitMs) {
            poll.stop();
        }
    });
    poll.start();
    open();
    return texts;
}

void captureWidget(QWidget& widget, const QString& name)
{
    const QString directory = qEnvironmentVariable("NEREUS_BANDSTACK_CAPTURE_DIR");
    if (directory.isEmpty()) {
        return;
    }
    settle();
    QVERIFY(widget.grab().save(QDir(directory).filePath(name + QStringLiteral(".png"))));
}

// A menu over the panel, as it looks when it is open. The window behind stays
// out of it: its pans have no QRhi offscreen and would show through.
void captureWithMenu(BandStackPanel& panel, QMenu& menu, const QPoint& at, const QString& name)
{
    const QString directory = qEnvironmentVariable("NEREUS_BANDSTACK_CAPTURE_DIR");
    if (directory.isEmpty()) {
        return;
    }
    settle();
    menu.ensurePolished();
    menu.adjustSize();
    const QPixmap under = panel.grab();
    const QPixmap over = menu.grab();
    QPixmap shot(qMax(under.width(), at.x() + over.width()), under.height());
    shot.fill(Qt::black);
    QPainter painter(&shot);
    painter.drawPixmap(0, 0, under);
    painter.drawPixmap(at, over);
    painter.end();
    QVERIFY(shot.save(QDir(directory).filePath(name + QStringLiteral(".png"))));
}

StatusToast* toastWith(MainWindow& window, const QString& message)
{
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->message() == message) {
            return toast;
        }
    }
    return nullptr;
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

    void theCurrentBandOpensAndACollapsedOneStaysCollapsed()
    {
        RowsFixture f;
        QCOMPARE(f.rows->visibleRowIds(),
                 QStringList({QStringLiteral("cw"), QStringLiteral("net"), QStringLiteral("ssb"),
                              QStringLiteral("dx")}));
        QVERIFY(f.rows->sectionRect(Band::Band160m).isValid());
        QVERIFY(f.rows->sectionRect(Band::GEN).isValid());
        QVERIFY(!f.rows->sectionRect(Band::XVTR).isValid());
        QCOMPARE(f.rows->currentBand(), std::optional<Band>(Band::Band40m));
        QCOMPARE(f.rows->placeRowId(), QStringLiteral("net"));
        QCOMPARE(f.rows->entryCount(Band::Band20m), 2);

        // Collapsing the current band holds while tuning stays on it.
        QTest::mouseClick(f.rows, Qt::LeftButton, Qt::NoModifier,
                          centerOf(f.rows->sectionRect(Band::Band40m)));
        QVERIFY(f.rows->visibleRowIds().isEmpty());
        f.rows->setSliceFrequency(7190000.0);
        QVERIFY(f.rows->visibleRowIds().isEmpty());

        // A band you open stays open; the band changing opens the new one.
        QTest::mouseClick(f.rows, Qt::LeftButton, Qt::NoModifier,
                          centerOf(f.rows->sectionRect(Band::Band20m)));
        QCOMPARE(f.rows->visibleRowIds(),
                 QStringList({QStringLiteral("ft8"), QStringLiteral("phone")}));
        f.rows->setSliceFrequency(kOn20mHz);
        QCOMPARE(f.rows->visibleRowIds(),
                 QStringList({QStringLiteral("ft8"), QStringLiteral("phone")}));
        // And back: the collapse has been forgotten, so 40m opens again.
        f.rows->setSliceFrequency(kOn40mHz);
        QVERIFY(f.rows->visibleRowIds().contains(QStringLiteral("net")));
        QVERIFY(f.rows->visibleRowIds().contains(QStringLiteral("phone")));
    }

    void theOtherSelectionsOfTheList()
    {
        RowsFixture f;
        QVERIFY(f.rows->addRect().isValid());
        f.rows->setGrouped(false);
        QVERIFY(!f.rows->sectionRect(Band::Band40m).isValid());
        QCOMPARE(f.rows->visibleRowIds(),
                 QStringList({QStringLiteral("cw"), QStringLiteral("net"), QStringLiteral("ssb"),
                              QStringLiteral("dx"), QStringLiteral("ft8"), QStringLiteral("phone")}));
        f.rows->setGrouped(true);

        f.rows->setBand(Band::Band20m);
        QCOMPARE(f.rows->visibleRowIds(),
                 QStringList({QStringLiteral("ft8"), QStringLiteral("phone")}));
        QVERIFY(!f.rows->sectionRect(Band::Band20m).isValid());
        QVERIFY(!f.rows->addRect().isValid());
        f.rows->setBand(Band::Band40m);
        QVERIFY(f.rows->addRect().isValid());
        f.rows->setBand(std::nullopt);
        QVERIFY(f.rows->sectionRect(Band::Band20m).isValid());

        // Another slice's selection reads its own place.
        f.rows->setSliceId(1);
        f.rows->setSliceFrequency(7200000.0);
        QCOMPARE(f.rows->placeRowId(), QStringLiteral("ssb"));
    }

    void aRowClickGoesThereAndAddAddsWithTheView()
    {
        RowsFixture f;
        QTest::mouseClick(f.rows, Qt::LeftButton, Qt::NoModifier,
                          centerOf(f.rows->rowRect(QStringLiteral("ssb"))));
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.at(0).verb, QStringLiteral("goTo"));
        QCOMPARE(f.fake.calls.at(0).sliceId, 0);
        QCOMPARE(f.fake.calls.at(0).entryId, QStringLiteral("ssb"));
        QCOMPARE(f.fake.calls.at(0).view.centerHz, std::optional<qint64>(kViewCenterHz));
        QCOMPARE(f.fake.calls.at(0).view.spanHz, std::optional<qint64>(kViewSpanHz));

        QTest::mouseClick(f.rows, Qt::LeftButton, Qt::NoModifier, centerOf(f.rows->addRect()));
        QCOMPARE(f.fake.calls.size(), 2);
        QCOMPARE(f.fake.calls.at(1).verb, QStringLiteral("add"));
        QCOMPARE(f.fake.calls.at(1).sliceId, 0);
        QCOMPARE(f.fake.calls.at(1).view.centerHz, std::optional<qint64>(kViewCenterHz));

        // A band open beside the current one offers no add at its foot.
        f.rows->setBandOpen(Band::Band40m, false);
        f.rows->setBandOpen(Band::Band20m, true);
        QVERIFY(!f.rows->addRect().isValid());
    }

    void aDocumentBetweenPressAndReleaseIsNotAClickOnAnotherRow()
    {
        RowsFixture f;
        const QPoint at = centerOf(f.rows->rowRect(QStringLiteral("cw")));
        QTest::mousePress(f.rows, Qt::LeftButton, Qt::NoModifier, at);
        QJsonObject document = stackDocument();
        QJsonObject bands = document.value(QStringLiteral("bands")).toObject();
        bands[QStringLiteral("40m")] = bandOrder({QStringLiteral("net"), QStringLiteral("ssb"),
                                                  QStringLiteral("dx")});
        document[QStringLiteral("bands")] = bands;
        QJsonArray entries = document.value(QStringLiteral("entries")).toArray();
        entries.removeAt(0);
        document[QStringLiteral("entries")] = entries;
        f.fake.setDocument(document);
        QVERIFY(!f.rows->rowRect(QStringLiteral("cw")).isValid());
        QTest::mouseRelease(f.rows, Qt::LeftButton, Qt::NoModifier, at);
        QVERIFY(f.fake.calls.isEmpty());
    }

    void aDoubleClickRenames()
    {
        RowsFixture f;
        const QRect plain = f.rows->rowRect(QStringLiteral("cw"));
        QTest::mouseDClick(f.rows, Qt::LeftButton, Qt::NoModifier, centerOf(plain));
        auto* editor = f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"));
        QVERIFY(editor);
        QCOMPARE(editor->placeholderText(), QStringLiteral("Name this spot"));
        QVERIFY(f.rows->rowRect(QStringLiteral("cw")).height() > plain.height());
        editor->setText(QStringLiteral("  CW practice  "));
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.at(0).verb, QStringLiteral("edit"));
        QCOMPARE(f.fake.calls.at(0).entryId, QStringLiteral("cw"));
        QCOMPARE(f.fake.calls.at(0).name, std::optional<QString>(QStringLiteral("CW practice")));
        QVERIFY(!f.fake.calls.at(0).locked.has_value());
        QCOMPARE(f.rows->rowRect(QStringLiteral("cw")).height(), plain.height());
        f.fake.calls.clear();

        // Esc leaves the name alone.
        f.rows->beginRename(QStringLiteral("dx"));
        editor = f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"));
        QVERIFY(editor);
        QCOMPARE(editor->text(), QStringLiteral("Evening net"));
        editor->setText(QStringLiteral("Nope"));
        QTest::keyClick(editor, Qt::Key_Escape);
        QVERIFY(f.fake.calls.isEmpty());
        f.rows->beginRename(QStringLiteral("dx"));
        editor = f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"));
        // No change sends nothing.
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(f.fake.calls.isEmpty());

        // An entry deleted elsewhere takes its open editor with it, unsent.
        f.rows->beginRename(QStringLiteral("cw"));
        editor = f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"));
        QVERIFY(editor);
        f.fake.setDocument(QJsonObject{});
        QVERIFY(f.rows->visibleRowIds().isEmpty());
        QTRY_VERIFY(!f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename")));
        QVERIFY(f.fake.calls.isEmpty());
    }

    void aDragReordersOnDrop()
    {
        RowsFixture f;
        const QPoint start = centerOf(f.rows->rowRect(QStringLiteral("cw")));
        QTest::mousePress(f.rows, Qt::LeftButton, Qt::NoModifier, start);
        const auto moveTo = [&f](const QPoint& to) {
            QMouseEvent move(QEvent::MouseMove, QPointF(to), f.rows->mapToGlobal(QPointF(to)),
                             Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(f.rows, &move);
        };
        moveTo(start + QPoint(0, 2));
        QVERIFY(!f.rows->isDragging());
        moveTo(centerOf(f.rows->rowRect(QStringLiteral("ssb"))) + QPoint(0, 3));
        QVERIFY(f.rows->isDragging());
        QCOMPARE(f.rows->visibleRowIds(),
                 QStringList({QStringLiteral("net"), QStringLiteral("ssb"), QStringLiteral("cw"),
                              QStringLiteral("dx")}));
        // A row on another band is not a target, and no call goes until the drop.
        moveTo(f.rows->sectionRect(Band::Band20m).center());
        QVERIFY(f.fake.calls.isEmpty());
        QTest::mouseRelease(f.rows, Qt::LeftButton, Qt::NoModifier,
                            f.rows->sectionRect(Band::Band20m).center());
        QVERIFY(!f.rows->isDragging());
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.at(0).verb, QStringLiteral("reorder"));
        QCOMPARE(f.fake.calls.at(0).band, std::optional<Band>(Band::Band40m));
        QCOMPARE(f.fake.calls.at(0).ids,
                 QStringList({QStringLiteral("net"), QStringLiteral("ssb"), QStringLiteral("cw"),
                              QStringLiteral("dx")}));
        f.fake.calls.clear();

        // A refusal puts the Core's order back.
        emit f.fake.refused(QStringLiteral("no"));
        QCOMPARE(f.rows->visibleRowIds().first(), QStringLiteral("cw"));

        // A drag back to where it started sends nothing, and is not a click.
        const QPoint again = centerOf(f.rows->rowRect(QStringLiteral("net")));
        QTest::mousePress(f.rows, Qt::LeftButton, Qt::NoModifier, again);
        moveTo(again + QPoint(0, 5));
        QVERIFY(f.rows->isDragging());
        QTest::mouseRelease(f.rows, Qt::LeftButton, Qt::NoModifier, again + QPoint(0, 5));
        QVERIFY(f.fake.calls.isEmpty());

        // Esc drops the drag and the order.
        QTest::mousePress(f.rows, Qt::LeftButton, Qt::NoModifier, again);
        moveTo(centerOf(f.rows->rowRect(QStringLiteral("dx"))) + QPoint(0, 3));
        QVERIFY(f.rows->visibleRowIds().last() == QStringLiteral("net"));
        QTest::keyClick(f.rows, Qt::Key_Escape);
        QVERIFY(!f.rows->isDragging());
        QCOMPARE(f.rows->visibleRowIds().at(1), QStringLiteral("net"));
        QTest::mouseRelease(f.rows, Qt::LeftButton, Qt::NoModifier, again);
        QVERIFY(f.fake.calls.isEmpty());

        // A flat list and a popup do not drag.
        f.rows->setGrouped(false);
        const QPoint flat = centerOf(f.rows->rowRect(QStringLiteral("cw")));
        QTest::mousePress(f.rows, Qt::LeftButton, Qt::NoModifier, flat);
        moveTo(flat + QPoint(0, 30));
        QVERIFY(!f.rows->isDragging());
        QTest::mouseRelease(f.rows, Qt::LeftButton, Qt::NoModifier, flat + QPoint(0, 30));
    }

    void theKeysActOnTheRows()
    {
        RowsFixture f;
        QSignalSpy closes(f.rows, &BandStackRowList::closeRequested);
        // The keys start from the slice's place.
        QTest::keyClick(f.rows, Qt::Key_Down);
        QCOMPARE(f.rows->keyboardRowId(), QStringLiteral("ssb"));
        QTest::keyClick(f.rows, Qt::Key_Up);
        QTest::keyClick(f.rows, Qt::Key_Up);
        QCOMPARE(f.rows->keyboardRowId(), QStringLiteral("cw"));
        QTest::keyClick(f.rows, Qt::Key_Up);
        QCOMPARE(f.rows->keyboardRowId(), QStringLiteral("cw"));
        QVERIFY(f.fake.calls.isEmpty());

        QTest::keyClick(f.rows, Qt::Key_Return);
        QCOMPARE(f.fake.calls.last().verb, QStringLiteral("goTo"));
        QCOMPARE(f.fake.calls.last().entryId, QStringLiteral("cw"));
        QCOMPARE(f.fake.calls.last().view.spanHz, std::optional<qint64>(kViewSpanHz));

        QTest::keyClick(f.rows, Qt::Key_U, Qt::ControlModifier);
        QCOMPARE(f.fake.calls.last().verb, QStringLiteral("edit"));
        QCOMPARE(f.fake.calls.last().locked, std::optional<bool>(true));
        QVERIFY(!f.fake.calls.last().name.has_value());

        QTest::keyClick(f.rows, Qt::Key_Delete);
        QCOMPARE(f.fake.calls.last().verb, QStringLiteral("remove"));
        QCOMPARE(f.fake.calls.last().entryId, QStringLiteral("cw"));
        QTest::keyClick(f.rows, Qt::Key_Backspace);
        QCOMPARE(f.fake.calls.size(), 4);
        QCOMPARE(f.fake.calls.last().verb, QStringLiteral("remove"));

        // A plain U is not the lock key.
        QTest::keyClick(f.rows, Qt::Key_U);
        QCOMPARE(f.fake.calls.size(), 4);

        QTest::keyClick(f.rows, Qt::Key_F2);
        auto* editor = f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"));
        QVERIFY(editor);
        QTest::keyClick(editor, Qt::Key_Escape);

        QCOMPARE(closes.count(), 0);
        QTest::keyClick(f.rows, Qt::Key_Escape);
        QCOMPARE(closes.count(), 1);

        // A window's shortcut does not take the panel's keys.
        QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_F2, Qt::NoModifier);
        override.ignore();
        QCoreApplication::sendEvent(f.rows, &override);
        QVERIFY(override.isAccepted());
        QKeyEvent other(QEvent::ShortcutOverride, Qt::Key_M, Qt::NoModifier);
        other.ignore();
        QCoreApplication::sendEvent(f.rows, &other);
        QVERIFY(!other.isAccepted());
    }

    void theRowMenuSendsEachVerb()
    {
        RowsFixture f;
        const QStringList expected{QStringLiteral("Go there"),
                                   QStringLiteral("Save current spot here"),
                                   QStringLiteral("-"),
                                   QStringLiteral("Rename"),
                                   QStringLiteral("Lock"),
                                   QStringLiteral("Always return here"),
                                   QStringLiteral("-"),
                                   QStringLiteral("Delete")};
        {
            QMenu menu;
            f.rows->fillRowMenu(menu, QStringLiteral("net"));
            QCOMPARE(actionTexts(menu), expected);
            QVERIFY(menu.toolTipsVisible());
            QVERIFY(!menu.styleSheet().isEmpty());
            QVERIFY(actionNamed(menu, QStringLiteral("Rename"))->text().contains(QLatin1Char('\t')));
            QVERIFY(actionNamed(menu, QStringLiteral("Save current spot here"))->isEnabled());
            QVERIFY(!actionNamed(menu, QStringLiteral("Always return here"))->isChecked());

            actionNamed(menu, QStringLiteral("Go there"))->trigger();
            QCOMPARE(f.fake.calls.last().verb, QStringLiteral("goTo"));
            QCOMPARE(f.fake.calls.last().entryId, QStringLiteral("net"));
            actionNamed(menu, QStringLiteral("Save current spot here"))->trigger();
            QCOMPARE(f.fake.calls.last().verb, QStringLiteral("saveHere"));
            QCOMPARE(f.fake.calls.last().sliceId, 0);
            QCOMPARE(f.fake.calls.last().entryId, QStringLiteral("net"));
            QCOMPARE(f.fake.calls.last().view.centerHz, std::optional<qint64>(kViewCenterHz));
            actionNamed(menu, QStringLiteral("Lock"))->trigger();
            QCOMPARE(f.fake.calls.last().verb, QStringLiteral("edit"));
            QCOMPARE(f.fake.calls.last().locked, std::optional<bool>(true));
            actionNamed(menu, QStringLiteral("Always return here"))->trigger();
            QCOMPARE(f.fake.calls.last().verb, QStringLiteral("setReturn"));
            QCOMPARE(f.fake.calls.last().mode, BandStackReturnMode::Specific);
            QCOMPARE(f.fake.calls.last().entryId, QStringLiteral("net"));
            actionNamed(menu, QStringLiteral("Delete"))->trigger();
            QCOMPARE(f.fake.calls.last().verb, QStringLiteral("remove"));
            QCOMPARE(f.fake.calls.last().entryId, QStringLiteral("net"));
            actionNamed(menu, QStringLiteral("Rename"))->trigger();
            QVERIFY(f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename")));
            f.rows->setFocus();
            QTRY_VERIFY(!f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"))
                        || !f.rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"))->isVisible());
        }
        {
            // Locked: Unlock, and Save is disabled with the reason.
            QMenu menu;
            f.rows->fillRowMenu(menu, QStringLiteral("ssb"));
            QAction* save = actionNamed(menu, QStringLiteral("Save current spot here"));
            QVERIFY(!save->isEnabled());
            QVERIFY(save->toolTip().contains(QStringLiteral("locked")));
            QAction* unlock = actionNamed(menu, QStringLiteral("Unlock"));
            QVERIFY(unlock);
            unlock->trigger();
            QCOMPARE(f.fake.calls.last().locked, std::optional<bool>(false));
        }
        {
            // Off the band: disabled with its own reason.
            QMenu menu;
            f.rows->fillRowMenu(menu, QStringLiteral("ft8"));
            QAction* save = actionNamed(menu, QStringLiteral("Save current spot here"));
            QVERIFY(!save->isEnabled());
            QCOMPARE(save->toolTip(),
                     QStringLiteral("Slice A is not on 20m. Tune to 20m to save here."));
        }
        {
            // "Always return here" on the band's specific entry goes back to Current.
            QMenu menu;
            f.rows->fillRowMenu(menu, QStringLiteral("dx"));
            QAction* always = actionNamed(menu, QStringLiteral("Always return here"));
            QVERIFY(always->isChecked());
            always->trigger();
            QCOMPARE(f.fake.calls.last().verb, QStringLiteral("setReturn"));
            QCOMPARE(f.fake.calls.last().mode, BandStackReturnMode::Current);
        }

        // A right-click really opens it, at the row.
        const QPoint at = centerOf(f.rows->rowRect(QStringLiteral("net")));
        const QStringList shown = textsOfNextPopup([&] {
            QContextMenuEvent event(QContextMenuEvent::Mouse, at, f.rows->mapToGlobal(at));
            QCoreApplication::sendEvent(f.rows, &event);
        });
        QCOMPARE(shown, expected);

        // A right-click on a header and on nothing say so.
        QSignalSpy band(f.rows, &BandStackRowList::bandContextRequested);
        QSignalSpy panel(f.rows, &BandStackRowList::panelContextRequested);
        const QPoint header = centerOf(f.rows->sectionRect(Band::Band20m));
        QContextMenuEvent onHeader(QContextMenuEvent::Mouse, header, f.rows->mapToGlobal(header));
        QCoreApplication::sendEvent(f.rows, &onHeader);
        QCOMPARE(band.count(), 1);
        QCOMPARE(band.at(0).at(0).value<Band>(), Band::Band20m);
        QCOMPARE(band.at(0).at(1).toPoint(), f.rows->mapToGlobal(header));
        const QPoint margin(1, f.rows->rowRect(QStringLiteral("net")).center().y());
        QContextMenuEvent onMargin(QContextMenuEvent::Mouse, margin, f.rows->mapToGlobal(margin));
        QCoreApplication::sendEvent(f.rows, &onMargin);
        QCOMPARE(panel.count(), 1);
    }

    void theBandMenuSendsEachVerbAndAsks()
    {
        FakeBandStackAccess fake;
        fake.setDocument(stackDocument());
        BandStackPanel panel(&fake);
        panel.rows()->setSliceId(0);
        panel.rows()->setSliceFrequency(kOn40mHz);

        QMenu menu;
        panel.fillBandMenu(menu, Band::Band40m);
        QCOMPARE(actionTexts(menu), QStringList({QStringLiteral("Return to"),
                                                 QStringLiteral("Collapse"),
                                                 QStringLiteral("-"),
                                                 QStringLiteral("Clear unlocked 40m entries")}));
        QVERIFY(menu.toolTipsVisible());
        QMenu* returnTo = actionNamed(menu, QStringLiteral("Return to"))->menu();
        QVERIFY(returnTo);
        QVERIFY(returnTo->toolTipsVisible());
        QCOMPARE(actionTexts(*returnTo), QStringList({QStringLiteral("Current entry"),
                                                      QStringLiteral("Last frequency used"),
                                                      QStringLiteral("Specific entry")}));
        // This document has 40m on a specific entry.
        QVERIFY(actionNamed(*returnTo, QStringLiteral("Specific entry"))->isChecked());
        QVERIFY(!actionNamed(*returnTo, QStringLiteral("Current entry"))->isChecked());

        actionNamed(*returnTo, QStringLiteral("Current entry"))->trigger();
        QCOMPARE(fake.calls.last().verb, QStringLiteral("setReturn"));
        QCOMPARE(fake.calls.last().band, std::optional<Band>(Band::Band40m));
        QCOMPARE(fake.calls.last().mode, BandStackReturnMode::Current);
        actionNamed(*returnTo, QStringLiteral("Last frequency used"))->trigger();
        QCOMPARE(fake.calls.last().mode, BandStackReturnMode::LastUsed);
        // Specific entry takes the slice's place.
        actionNamed(*returnTo, QStringLiteral("Specific entry"))->trigger();
        QCOMPARE(fake.calls.last().mode, BandStackReturnMode::Specific);
        QCOMPARE(fake.calls.last().entryId, QStringLiteral("net"));

        // With no place, the first voice entry.
        {
            QMenu other;
            panel.fillBandMenu(other, Band::Band20m);
            QMenu* sub = actionNamed(other, QStringLiteral("Return to"))->menu();
            QVERIFY(actionNamed(*sub, QStringLiteral("Current entry"))->isChecked());
            actionNamed(*sub, QStringLiteral("Specific entry"))->trigger();
            QCOMPARE(fake.calls.last().entryId, QStringLiteral("phone"));
            QVERIFY(actionNamed(other, QStringLiteral("Expand")));
        }
        // An empty band cannot pick one, and says why.
        {
            QMenu empty;
            panel.fillBandMenu(empty, Band::Band17m);
            QAction* specific = actionNamed(*actionNamed(empty, QStringLiteral("Return to"))->menu(),
                                            QStringLiteral("Specific entry"));
            QVERIFY(!specific->isEnabled());
            QCOMPARE(specific->toolTip(), QStringLiteral("This band has no entries."));
        }

        QVERIFY(panel.rows()->isBandOpen(Band::Band40m));
        actionNamed(menu, QStringLiteral("Collapse"))->trigger();
        QVERIFY(!panel.rows()->isBandOpen(Band::Band40m));

        const int before = fake.calls.size();
        QString words;
        answerConfirm(QStringLiteral("bandStackConfirmCancel"), &words);
        actionNamed(menu, QStringLiteral("Clear unlocked 40m entries"))->trigger();
        QCOMPARE(words, QStringLiteral("Clear the unlocked entries on 40m?"));
        QCOMPARE(fake.calls.size(), before);

        answerConfirm(QStringLiteral("bandStackConfirmAccept"), &words);
        actionNamed(menu, QStringLiteral("Clear unlocked 40m entries"))->trigger();
        QCOMPARE(fake.calls.size(), before + 1);
        QCOMPARE(fake.calls.last().verb, QStringLiteral("clear"));
        QCOMPARE(fake.calls.last().band, std::optional<Band>(Band::Band40m));
    }

    void thePanelMenuSendsEachVerbAndAsks()
    {
        FakeBandStackAccess fake;
        fake.setDocument(stackDocument());
        BandStackPanel panel(&fake);
        panel.setOpen(true, false);
        panel.rows()->setSliceId(0);
        panel.rows()->setSliceFrequency(kOn40mHz);

        const QStringList expected{QStringLiteral("Group by band"),
                                   QStringLiteral("-"),
                                   QStringLiteral("Auto-expiry"),
                                   QStringLiteral("Auto-save dwell"),
                                   QStringLiteral("-"),
                                   QStringLiteral("Don't save over a frequency already here"),
                                   QStringLiteral("Show on the spectrum"),
                                   QStringLiteral("-"),
                                   QStringLiteral("Reset to region defaults..."),
                                   QStringLiteral("Clear unlocked entries...")};
        QMenu menu;
        panel.fillPanelMenu(menu);
        QCOMPARE(actionTexts(menu), expected);

        QAction* group = actionNamed(menu, QStringLiteral("Group by band"));
        QVERIFY(group->isChecked());
        group->trigger();
        QVERIFY(!panel.groupByBand());
        QVERIFY(!panel.rows()->isGrouped());
        QVERIFY(fake.calls.isEmpty());
        group->trigger();
        QVERIFY(panel.rows()->isGrouped());

        QMenu* expiry = actionNamed(menu, QStringLiteral("Auto-expiry"))->menu();
        QCOMPARE(actionTexts(*expiry),
                 QStringList({QStringLiteral("Off"), QStringLiteral("5 min"), QStringLiteral("15 min"),
                              QStringLiteral("30 min"), QStringLiteral("60 min")}));
        QVERIFY(actionNamed(*expiry, QStringLiteral("15 min"))->isChecked());
        actionNamed(*expiry, QStringLiteral("30 min"))->trigger();
        QCOMPARE(fake.calls.last().verb, QStringLiteral("options"));
        QCOMPARE(fake.calls.last().expiryMinutes, std::optional<int>(30));
        QVERIFY(!fake.calls.last().dwellSeconds.has_value());
        QVERIFY(!fake.calls.last().dupes.has_value());

        QMenu* dwell = actionNamed(menu, QStringLiteral("Auto-save dwell"))->menu();
        QCOMPARE(actionTexts(*dwell),
                 QStringList({QStringLiteral("Off"), QStringLiteral("10 sec"), QStringLiteral("30 sec"),
                              QStringLiteral("60 sec")}));
        QVERIFY(actionNamed(*dwell, QStringLiteral("Off"))->isChecked());
        actionNamed(*dwell, QStringLiteral("60 sec"))->trigger();
        QCOMPARE(fake.calls.last().dwellSeconds, std::optional<int>(60));
        QVERIFY(!fake.calls.last().expiryMinutes.has_value());

        QAction* dupes = actionNamed(menu, QStringLiteral("Don't save over a frequency already here"));
        QVERIFY(dupes->isChecked());
        dupes->trigger();
        QCOMPARE(fake.calls.last().dupes, std::optional<bool>(false));

        QAction* spectrum = actionNamed(menu, QStringLiteral("Show on the spectrum"));
        QVERIFY(!spectrum->isChecked());
        const int beforeSpectrum = fake.calls.size();
        spectrum->trigger();
        QVERIFY(panel.showOnSpectrum());
        QCOMPARE(fake.calls.size(), beforeSpectrum);

        QString words;
        answerConfirm(QStringLiteral("bandStackConfirmCancel"), &words);
        actionNamed(menu, QStringLiteral("Reset to region defaults..."))->trigger();
        QCOMPARE(words, QStringLiteral("Replace the unlocked entries with the defaults for "
                                       "United States? Your locked entries stay."));
        QCOMPARE(fake.calls.size(), beforeSpectrum);
        answerConfirm(QStringLiteral("bandStackConfirmAccept"), &words);
        actionNamed(menu, QStringLiteral("Reset to region defaults..."))->trigger();
        QCOMPARE(fake.calls.last().verb, QStringLiteral("reseed"));

        const int beforeClear = fake.calls.size();
        answerConfirm(QStringLiteral("bandStackConfirmCancel"), &words);
        actionNamed(menu, QStringLiteral("Clear unlocked entries..."))->trigger();
        QCOMPARE(words, QStringLiteral("Clear every unlocked entry on every band? Locked entries stay."));
        QCOMPARE(fake.calls.size(), beforeClear);
        answerConfirm(QStringLiteral("bandStackConfirmAccept"), &words);
        actionNamed(menu, QStringLiteral("Clear unlocked entries..."))->trigger();
        QCOMPARE(fake.calls.last().verb, QStringLiteral("clear"));
        QVERIFY(!fake.calls.last().band.has_value());

        // The "..." button and a right-click on a header open the menus.
        auto* dots = panel.findChild<QToolButton*>(QStringLiteral("bandStackPanelMenu"));
        QVERIFY(dots);
        QCOMPARE(textsOfNextPopup([dots] { dots->click(); }), expected);
        const QStringList bandShown = textsOfNextPopup([&panel] {
            emit panel.rows()->bandContextRequested(Band::Band20m, QPoint(10, 10));
        });
        QCOMPARE(bandShown.first(), QStringLiteral("Return to"));
        QCOMPARE(bandShown.last(), QStringLiteral("Clear unlocked 20m entries"));
    }

    void escClosesThePanelAndOpeningFocusesTheRows()
    {
        FakeBandStackAccess fake;
        fake.setDocument(stackDocument());
        QWidget host;
        host.resize(600, 400);
        auto* layout = new QHBoxLayout(&host);
        BandStackPanel panel(&fake, &host);
        layout->addWidget(&panel);
        layout->addStretch(1);
        host.show();
        panel.setOpen(true, false);
        settle();
        QCOMPARE(host.focusWidget(), panel.rows());
        QSignalSpy closes(&panel, &BandStackPanel::closeRequested);
        QTest::keyClick(panel.rows(), Qt::Key_Escape);
        QCOMPARE(closes.count(), 1);
    }

    void aListenerReadsTheRowsAndTheRecallIsRefusedThroughTheAccess()
    {
        const QString reason = QStringLiteral(
            "You are only listening to Slice A. Take control of it to change its band.");
        RowsFixture f(&reason);
        QCOMPARE(f.rows->blockedReason(), reason);
        QCOMPARE(f.rows->visibleRowIds().size(), 4);
        // The row still sends the verb: the Core or the access refuses it, once,
        // through refused().
        QTest::mouseClick(f.rows, Qt::LeftButton, Qt::NoModifier,
                          centerOf(f.rows->rowRect(QStringLiteral("ssb"))));
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.at(0).verb, QStringLiteral("goTo"));
    }

    void anExpiryNoteOffersUndoForFiveSeconds()
    {
        const std::unique_ptr<MainWindow> window = buildWindow(true);
        FakeBandStackAccess fake;
        window->setBandStackAccessForTest(&fake);
        QJsonObject document = stackDocument();
        document[QStringLiteral("lastExpiry")] = QJsonObject{
            {QStringLiteral("batchId"), QStringLiteral("batch-1")}, {QStringLiteral("count"), 3}};
        QElapsedTimer shown;
        shown.start();
        fake.setDocument(document);
        const QString message = QStringLiteral("Removed 3 expired entries");
        QPointer<StatusToast> toast = toastWith(*window, message);
        QVERIFY(toast);
        auto* undo = toast->findChild<QPushButton*>(QStringLiteral("statusToastAction"));
        QVERIFY(undo);
        QCOMPARE(undo->text(), QStringLiteral("Undo"));
        // The same note again is not a new note.
        fake.setDocument(document);
        QCOMPARE(toastCount(*window, message), 1);
        QTRY_VERIFY_WITH_TIMEOUT(toast.isNull(), kToastCloseWaitMs);
        // The toast's QTimer is coarse, which Qt lets fire up to 5% early (seen
        // here at 4758 ms under load).
        QVERIFY2(shown.elapsed() >= kUndoMs * 95 / 100 - 50,
                 qPrintable(QString::number(shown.elapsed())));
        QVERIFY(fake.calls.isEmpty());

        // The next batch, one entry, and Undo.
        document[QStringLiteral("lastExpiry")] = QJsonObject{
            {QStringLiteral("batchId"), QStringLiteral("batch-2")}, {QStringLiteral("count"), 1}};
        fake.setDocument(document);
        toast = toastWith(*window, QStringLiteral("Removed 1 expired entry"));
        QVERIFY(toast);
        toast->findChild<QPushButton*>(QStringLiteral("statusToastAction"))->click();
        QCOMPARE(fake.calls.size(), 1);
        QCOMPARE(fake.calls.at(0).verb, QStringLiteral("restoreExpired"));
        QCOMPARE(fake.calls.at(0).entryId, QStringLiteral("batch-2"));
        QTRY_VERIFY(toast.isNull());
        window->setBandStackAccessForTest(window->radioModel()->bandStack());
    }

    void theRowsReachTheWindowsSliceAndFlags()
    {
        const std::unique_ptr<MainWindow> window = buildWindow(true);
        FakeBandStackAccess fake;
        fake.setDocument(stackDocument());
        window->setBandStackAccessForTest(&fake);
        auto* panel = window->findChild<BandStackPanel*>();
        QVERIFY(panel);
        QVERIFY(window->radioModel()->addSlice() >= 0);
        QVERIFY(window->radioModel()->addSlice() >= 0);
        const QList<SliceModel*> slices = window->radioModel()->slices();
        QVERIFY(slices.size() >= 2);
        slices.at(0)->setFrequency(kOn40mHz);
        slices.at(1)->setFrequency(7200000.0);
        panel->setOpen(true, false);
        settle();
        panel->setSlice(slices.at(0));
        BandStackRowList* rows = panel->rows();
        QCOMPARE(rows->currentBand(), std::optional<Band>(Band::Band40m));
        auto* onBand = panel->findChild<QLabel*>(QStringLiteral("bandStackPanelOnBand"));
        QCOMPARE(onBand->text(), QStringLiteral("on 40m"));
        QVERIFY(rows->blockedReason().isEmpty());

        // A locked slice dims the rows with the reason.
        slices.at(0)->setLocked(true);
        QVERIFY(rows->blockedReason().contains(QStringLiteral("locked")));
        slices.at(0)->setLocked(false);
        QVERIFY(rows->blockedReason().isEmpty());

        settle();
        QTest::mouseClick(rows, Qt::LeftButton, Qt::NoModifier,
                          rows->rowRect(QStringLiteral("cw")).center());
        QCOMPARE(fake.calls.last().verb, QStringLiteral("goTo"));
        QCOMPARE(fake.calls.last().sliceId, slices.at(0)->sliceIndex());
        panel->setSlice(nullptr);
        window->setBandStackAccessForTest(window->radioModel()->bandStack());
    }

    void capturesOfTheRowsAndMenus_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("workspace look") << true;
        QTest::newRow("classic look") << false;
    }

    void capturesOfTheRowsAndMenus()
    {
        QFETCH(bool, workspaceLook);
        const QString look = workspaceLook ? QStringLiteral("workspace") : QStringLiteral("classic");
        BandStackPanel::setReducedMotionForTest(true);
        const std::unique_ptr<MainWindow> window = buildWindow(workspaceLook);
        FakeBandStackAccess fake;
        fake.setDocument(stackDocument());
        window->setBandStackAccessForTest(&fake);
        fake.setState(BandStackAccess::State::Ready);
        auto* panel = window->findChild<BandStackPanel*>();
        QVERIFY(panel);
        SliceModel slice(0);
        slice.setFrequency(kOn40mHz);
        panel->rows()->setContext(stackContext());
        panel->setSlice(&slice);
        panel->setOpen(true, false);
        settle();
        BandStackRowList* rows = panel->rows();
        const auto prefix = [&look](const char* name) {
            return QStringLiteral("%1-%2").arg(look, QString::fromLatin1(name));
        };
        const QPoint atPanel(30, 120);
        captureWidget(*window, prefix("rows-open-band"));
        captureWidget(*panel, prefix("rows-panel-only"));

        {
            QMenu menu;
            rows->fillRowMenu(menu, QStringLiteral("net"));
            captureWithMenu(*panel, menu, atPanel, prefix("menu-row"));
        }
        {
            QMenu menu;
            rows->fillRowMenu(menu, QStringLiteral("ssb"));
            captureWithMenu(*panel, menu, atPanel, prefix("menu-row-locked"));
        }
        {
            QMenu menu;
            panel->fillBandMenu(menu, Band::Band40m);
            captureWithMenu(*panel, menu, atPanel, prefix("menu-band"));
            QMenu* returnTo = actionNamed(menu, QStringLiteral("Return to"))->menu();
            captureWithMenu(*panel, *returnTo, atPanel, prefix("menu-band-return"));
        }
        {
            QMenu menu;
            panel->fillPanelMenu(menu);
            captureWithMenu(*panel, menu, atPanel, prefix("menu-panel"));
            captureWithMenu(*panel, *actionNamed(menu, QStringLiteral("Auto-expiry"))->menu(), atPanel,
                            prefix("menu-panel-expiry"));
        }

        rows->beginRename(QStringLiteral("cw"));
        captureWidget(*panel, prefix("rows-rename"));
        QTest::keyClick(rows->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename")), Qt::Key_Escape);

        const QPoint start = rows->rowRect(QStringLiteral("cw")).center();
        QTest::mousePress(rows, Qt::LeftButton, Qt::NoModifier, start);
        const QPoint to = rows->rowRect(QStringLiteral("ssb")).center() + QPoint(0, 3);
        QMouseEvent move(QEvent::MouseMove, QPointF(to), rows->mapToGlobal(QPointF(to)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(rows, &move);
        QVERIFY(rows->isDragging());
        captureWidget(*panel, prefix("rows-drag"));
        QTest::mouseRelease(rows, Qt::LeftButton, Qt::NoModifier, to);

        rows->setFocus();
        QTest::keyClick(rows, Qt::Key_Down);
        captureWidget(*panel, prefix("rows-keyboard"));

        panel->setSlice(nullptr);
        window->setBandStackAccessForTest(window->radioModel()->bandStack());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstBandStackPanel test;
    QTEST_SET_MAIN_SOURCE_PATH
    // 2026-11-07: 109 to 113 s as one entry on the GitHub Linux runner,
    // against ctest's 120 s limit, and past it once. The time is the real
    // MainWindows (sixteen of them over the cases that build one). Those
    // cases run as four entries of the same binary, tst_bandstack_panel_pane,
    // _keys, _greyed and _window (tests/CMakeLists.txt), each building three
    // or four windows; tst_bandstack_panel keeps the cases on a bare panel or
    // row list and whatever is added next. The limit is not raised.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_BANDSTACK_PANEL_GROUP",
        {{QStringLiteral("pane"),
          {QStringLiteral("thePaneHoldsThePanelAndThePanColumn"),
           QStringLiteral("freeContainersAndSavedSizesSurviveARoundTrip")}},
         {QStringLiteral("keys"),
          {QStringLiteral("keysSurviveARelaunchAndStayOnThisDevice"),
           QStringLiteral("dotsMenuAndCloseToggleIt")}},
         {QStringLiteral("greyed"),
          {QStringLiteral("greyedStatesCarryTheirReasons"),
           QStringLiteral("refusalsAndReportsToastOnce")}},
         {QStringLiteral("window"),
          {QStringLiteral("anExpiryNoteOffersUndoForFiveSeconds"),
           QStringLiteral("theRowsReachTheWindowsSliceAndFlags"),
           QStringLiteral("capturesOfTheRowsAndMenus")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_bandstack_panel.moc"
