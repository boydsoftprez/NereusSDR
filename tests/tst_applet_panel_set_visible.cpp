// Verify AppletPanelWidget::setAppletVisible toggles wrapper visibility
// while keeping the applet's position in the stack layout intact.

#include <QApplication>
#include <QtTest/QtTest>
#include <QVBoxLayout>
#include <QScrollArea>
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletWidget.h"

using namespace NereusSDR;

namespace {
class FakeApplet : public AppletWidget {
public:
    explicit FakeApplet(const QString& title)
        : AppletWidget(nullptr), m_title(title) {}
    QString appletId() const override { return m_title; }
    QString appletTitle() const override { return m_title; }
    void syncFromModel() override {}
private:
    QString m_title;
};
} // anon

class TstAppletPanelSetVisible : public QObject {
    Q_OBJECT
private slots:
    void hides_wrapper_without_changing_layout_index();
    void shows_wrapper_again_at_same_index();
    void null_applet_is_noop();
    void unknown_applet_is_noop();
};

static QWidget* wrapperFor(AppletPanelWidget& panel, AppletWidget* applet)
{
    auto* scroll = panel.findChild<QScrollArea*>();
    if (!scroll) { return nullptr; }
    auto* stackWidget = scroll->widget();
    if (!stackWidget) { return nullptr; }
    auto* layout = qobject_cast<QVBoxLayout*>(stackWidget->layout());
    if (!layout) { return nullptr; }
    for (int i = 0; i < layout->count(); ++i) {
        auto* w = layout->itemAt(i)->widget();
        if (w && w->isAncestorOf(applet)) { return w; }
    }
    return nullptr;
}

static int indexOf(AppletPanelWidget& panel, QWidget* wrapper)
{
    auto* scroll = panel.findChild<QScrollArea*>();
    auto* stackWidget = scroll->widget();
    auto* layout = qobject_cast<QVBoxLayout*>(stackWidget->layout());
    return layout->indexOf(wrapper);
}

void TstAppletPanelSetVisible::hides_wrapper_without_changing_layout_index()
{
    AppletPanelWidget panel;
    auto* a = new FakeApplet(QStringLiteral("A"));
    auto* b = new FakeApplet(QStringLiteral("B"));
    panel.addApplet(a);
    panel.addApplet(b);

    QWidget* wrapperB = wrapperFor(panel, b);
    QVERIFY(wrapperB);
    int idxBefore = indexOf(panel, wrapperB);

    panel.setAppletVisible(b, false);

    QVERIFY(!wrapperB->isVisible());
    QCOMPARE(indexOf(panel, wrapperB), idxBefore);
}

void TstAppletPanelSetVisible::shows_wrapper_again_at_same_index()
{
    AppletPanelWidget panel;
    panel.show();
    auto* a = new FakeApplet(QStringLiteral("A"));
    auto* b = new FakeApplet(QStringLiteral("B"));
    panel.addApplet(a);
    panel.addApplet(b);

    QWidget* wrapperB = wrapperFor(panel, b);
    int idxBefore = indexOf(panel, wrapperB);

    panel.setAppletVisible(b, false);
    panel.setAppletVisible(b, true);

    QVERIFY(wrapperB->isVisible());
    QCOMPARE(indexOf(panel, wrapperB), idxBefore);
}

void TstAppletPanelSetVisible::null_applet_is_noop()
{
    AppletPanelWidget panel;
    panel.setAppletVisible(nullptr, false);
    panel.setAppletVisible(nullptr, true);
}

void TstAppletPanelSetVisible::unknown_applet_is_noop()
{
    AppletPanelWidget panel;
    auto* orphan = new FakeApplet(QStringLiteral("Orphan"));
    panel.setAppletVisible(orphan, false);
    delete orphan;
}

QTEST_MAIN(TstAppletPanelSetVisible)
#include "tst_applet_panel_set_visible.moc"
