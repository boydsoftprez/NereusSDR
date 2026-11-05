// =================================================================
// tests/tst_window_layout_restart.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// What the main window brings back after a restart: a real MainWindow is
// built, changed the way an operator changes it, destroyed, and built again
// on the same settings.
//
// Modification history (NereusSDR):
//   2026-11-05 - Created for the persistence audit. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================
#include <QtTest/QtTest>

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>
#include <QSplitter>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/settings/PanDisplayKeys.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpotHubDialog.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

constexpr auto kPanelHiddenKey = "ContainerPanelHidden";

void settle()
{
    QTest::qWait(20);
}

void place(MainWindow& window)
{
    window.resize(1280, 800);
    window.show();
    settle();
}

QLabel* panelToggle(MainWindow& window)
{
    for (QLabel* label : window.findChildren<QLabel*>()) {
        if (label->property("isPanelToggle").toBool()) {
            return label;
        }
    }
    return nullptr;
}

bool panelHidden(MainWindow& window)
{
    if (auto* area = window.findChild<EdgeDockArea*>()) {
        return area->userCollapsed(DockEdge::Right) && !area->strip(DockEdge::Right)->isVisible();
    }
    auto* splitter = qobject_cast<QSplitter*>(window.centralWidget());
    return splitter && splitter->count() >= 2 && splitter->widget(1)->isHidden();
}

QSlider* zoomBar(MainWindow& window)
{
    for (QSlider* slider : window.findChildren<QSlider*>()) {
        if (slider->toolTip().startsWith(QStringLiteral("Zoom: drag"))) {
            return slider;
        }
    }
    return nullptr;
}

} // namespace

class TstWindowLayoutRestart : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        AppSettings::setProfileOverride(
            QStringLiteral("window-layout-restart-%1").arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        WorkspacePreview::setEnabledForTest(false);
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
        AppSettings::instance().clear();
    }

    // Hiding the panel with the bottom bar's menu icon survives a restart, in
    // both looks, and showing it again survives too. In the classic look the
    // panel comes back at the width it was hidden at.
    void panelHideComesBackAfterARestart_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("workspace look") << true;
        QTest::newRow("classic look") << false;
    }

    void panelHideComesBackAfterARestart()
    {
        QFETCH(bool, workspaceLook);
        WorkspacePreview::setEnabledForTest(workspaceLook);
        auto& s = AppSettings::instance();
        const QString key = QString::fromLatin1(kPanelHiddenKey);

        int rightWidth = 0;
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            place(window);
            QVERIFY(!panelHidden(window));
            if (!workspaceLook) {
                auto* splitter = qobject_cast<QSplitter*>(window.centralWidget());
                QVERIFY(splitter);
                splitter->setSizes({900, 300});
                rightWidth = splitter->sizes().at(1);
                QVERIFY(rightWidth > 0);
            }
            QLabel* toggle = panelToggle(window);
            QVERIFY(toggle);
            QTest::mouseClick(toggle, Qt::LeftButton);
            QVERIFY(panelHidden(window));
            QCOMPARE(s.value(key).toString(), QStringLiteral("True"));
        }
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            place(window);
            QVERIFY2(panelHidden(window), "the hidden panel came back after a restart");
            QTest::mouseClick(panelToggle(window), Qt::LeftButton);
            QVERIFY(!panelHidden(window));
            QCOMPARE(s.value(key).toString(), QStringLiteral("False"));
            if (!workspaceLook) {
                auto* splitter = qobject_cast<QSplitter*>(window.centralWidget());
                QVERIFY(qAbs(splitter->sizes().at(1) - rightWidth) <= 2);
            }
        }
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            place(window);
            QVERIFY(!panelHidden(window));
        }
    }

    // The window floats a pan again by itself when it opens. Docked again
    // before the window goes: the offscreen platform's backing store crashes
    // tearing down a main window with a visible floating child
    // (QOffscreenBackingStore::clearHash), which is the test platform, not this.
    void aPanLeftFloatingFloatsAgainWhenTheWindowOpens()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PanLayoutId"), QStringLiteral("2v"));
        s.setValue(QStringLiteral("FloatingPan_pan-1_Floating"), QStringLiteral("True"));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        place(window);
        auto* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        QVERIFY(stack->isFloating(QStringLiteral("pan-1")));
        QVERIFY(!stack->isFloating(QStringLiteral("pan-0")));
        QVERIFY(stack->floatingWindowForTest(QStringLiteral("pan-1"))->isVisible());
        stack->dockPanadapter(QStringLiteral("pan-1"));
        settle();
        QCOMPARE(s.value(QStringLiteral("FloatingPan_pan-1_Floating")).toString(),
                 QStringLiteral("False"));
    }

    // The slider under the pans shows the active pan's span, and following
    // the pan never changes a span.
    void zoomSliderFollowsTheActivePan()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        place(window);
        auto* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        stack->applyLayout(QStringLiteral("2v"), MainWindow::panIdsForLayout(QStringLiteral("2v")));
        settle();
        SpectrumWidget* first = stack->spectrum(QStringLiteral("pan-0"));
        SpectrumWidget* second = stack->spectrum(QStringLiteral("pan-1"));
        QVERIFY(first);
        QVERIFY(second);
        QSlider* bar = zoomBar(window);
        QVERIFY(bar);
        QCOMPARE(stack->activePanId(), QStringLiteral("pan-0"));

        QSignalSpy firstAsked(first, &SpectrumWidget::bandwidthChangeRequested);
        QSignalSpy secondAsked(second, &SpectrumWidget::bandwidthChangeRequested);

        first->setFrequencyRange(14'100'000.0, 48'000.0);
        QCOMPARE(bar->value(), 48);
        // A pan that is not the active one moves nothing.
        second->setFrequencyRange(7'100'000.0, 96'000.0);
        QCOMPARE(bar->value(), 48);

        stack->setActivePan(QStringLiteral("pan-1"));
        QCOMPARE(bar->value(), 96);
        QCOMPARE(first->bandwidth(), 48'000.0);
        QCOMPARE(second->bandwidth(), 96'000.0);

        // Wider than the slider's range: the thumb sits at the end and the
        // span stays what it is.
        second->setFrequencyRange(7'100'000.0, 2'000'000.0);
        QCOMPARE(bar->value(), 768);
        QCOMPARE(second->bandwidth(), 2'000'000.0);

        stack->setActivePan(QStringLiteral("pan-0"));
        QCOMPARE(bar->value(), 48);
        QCOMPARE(first->bandwidth(), 48'000.0);
        QCOMPARE(second->bandwidth(), 2'000'000.0);
        QVERIFY(firstAsked.isEmpty());
        QVERIFY(secondAsked.isEmpty());

        // The slider still sets the active pan's span.
        bar->setValue(100);
        QCOMPARE(first->bandwidth(), 100'000.0);
        QCOMPARE(second->bandwidth(), 2'000'000.0);
        QCOMPARE(firstAsked.size(), 1);
    }

    // The wheel and the scale drag both end in setFrequencyRange; a transmit
    // view swap writes the span directly and must be followed as well.
    void zoomSliderFollowsATransmitViewSwap()
    {
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        place(window);
        auto* stack = window.findChild<PanadapterStack*>();
        SpectrumWidget* pan = stack->spectrum(QStringLiteral("pan-0"));
        QSlider* bar = zoomBar(window);
        QVERIFY(bar);
        pan->setFrequencyRange(14'100'000.0, 48'000.0);
        QCOMPARE(bar->value(), 48);
        pan->setFrequencyRange(14'100'000.0, 200'000.0);
        pan->setMoxOverlay(true);
        pan->setFrequencyRange(14'100'000.0, 24'000.0);
        QCOMPARE(bar->value(), 24);
        pan->setMoxOverlay(false);
        QCOMPARE(pan->bandwidth(), 200'000.0);
        QCOMPARE(bar->value(), 200);
    }

    // A restart: the saved span and the slider agree before anything is touched.
    void zoomSliderShowsTheSavedSpanAfterARestart()
    {
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            place(window);
            auto* stack = window.findChild<PanadapterStack*>();
            stack->spectrum(QStringLiteral("pan-0"))->setFrequencyRange(14'100'000.0, 48'000.0);
            stack->spectrum(QStringLiteral("pan-0"))->saveSettings();
        }
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        place(window);
        auto* stack = window.findChild<PanadapterStack*>();
        QCOMPARE(stack->spectrum(QStringLiteral("pan-0"))->bandwidth(), 48'000.0);
        QCOMPARE(zoomBar(window)->value(), 48);
        QCOMPARE(stack->spectrum(QStringLiteral("pan-0"))->bandwidth(), 48'000.0);
    }
};

QTEST_MAIN(TstWindowLayoutRestart)
#include "tst_window_layout_restart.moc"
