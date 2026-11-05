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
};

QTEST_MAIN(TstWindowLayoutRestart)
#include "tst_window_layout_restart.moc"
