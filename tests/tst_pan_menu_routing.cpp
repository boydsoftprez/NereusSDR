// no-port-check: AetherSDR-derived behaviour; see PanLayoutDialog.h.

// =================================================================
// tests/tst_pan_menu_routing.cpp  (NereusSDR)
// =================================================================
//
// Task B4 (bottom-banner + pan-menu epic): the status-bar "+PAN" text pill
// becomes a drawn icon -- AetherSDR MainWindow.cpp:4368-4396 [@c6481cb], a
// jagged spectrum polyline with a plus in the upper right -- and its click
// opens PanLayoutDialog instead of the retired showPanMenu() context menu.
//
// MainWindow is not constructible in this harness: a literal
// `MainWindow w;` starts real RadioDiscovery UDP broadcasts on the LAN,
// takes roughly 9 s to build the auto-opened ConnectionPanel, and SIGABRTs
// the test binary on teardown ("QThread: Destroyed while thread
// 'SpectrumThread' is still running"). tst_mainwindow_status_bar_safety.cpp,
// tst_pan_active_slice_sync.cpp and tst_pan_badge_click_wiring.cpp
// independently document the same conclusion (see the former's file-header
// note for the evidence). These tests therefore mirror
// MainWindow.cpp buildStatusBar()'s panBtn construction block and
// MainWindow::updateAddPanButtonState() verbatim against a standalone
// QLabel, the pattern tst_mainwindow_status_bar_safety.cpp already uses
// for the reserved safety-slot badges.
//
// Covered:
//   1. panButtonIsAnIconNotText          -- no text, a real drawn pixmap
//   2. panButtonIsDimWhenDisconnected    -- disabled + explains why, before
//                                           any click, not a silent no-op
//                                           after one
//   3. panButtonTooltipCarriesNoSourceCite -- neither state's tooltip leaks
//                                             the AetherSDR cite/stamp into
//                                             user-visible text
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QGraphicsOpacityEffect>

namespace {

// Mirrors MainWindow.cpp buildStatusBar()'s panBtn block verbatim (minus
// the barWidget parent, hbox insertion and installEventFilter(this), which
// need a real MainWindow and are not part of what these tests cover).
void buildPanIcon(QLabel& panBtn)
{
    panBtn.setObjectName(QStringLiteral("addPanButton"));
    panBtn.setAccessibleName(QStringLiteral("Add panadapter"));
    QPixmap pm(36, 28);
    {
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        pp.setRenderHint(QPainter::Antialiasing);
        const QColor stroke(255, 255, 255, 210);
        pp.setPen(QPen(stroke, 1.6));
        const QPointF pts[] = {
            { 0, 22}, { 1, 21}, { 2, 22}, { 3, 19}, { 4, 22},
            { 5, 21}, { 6, 18}, { 7, 12}, { 8, 17}, { 9, 22},
            {10, 21}, {11, 22}, {12, 16}, {13, 22},
            {14, 21}, {15, 19}, {16, 22},
            {17, 20}, {18, 12}, {19,  4}, {20, 11}, {21, 21},
            {22, 22}, {23, 21}, {24, 17}, {25, 22},
            {26, 21}, {27, 22}, {28, 18}, {29, 22}, {30, 22}
        };
        pp.drawPolyline(pts, sizeof(pts) / sizeof(pts[0]));
        pp.setPen(QPen(stroke, 2.2));
        pp.drawLine(30, 4, 30, 14);
        pp.drawLine(25, 9, 35, 9);
        pp.end();
    }
    panBtn.setPixmap(pm);
    panBtn.setCursor(Qt::PointingHandCursor);
    panBtn.setProperty("isAddPanButton", true);
}

// Mirrors MainWindow::updateAddPanButtonState() verbatim, minus the
// m_addPanButton null guard: callers here always pass a live reference.
void applyConnectionState(QLabel& panBtn, bool connected)
{
    panBtn.setEnabled(connected);
    auto* fx = qobject_cast<QGraphicsOpacityEffect*>(panBtn.graphicsEffect());
    if (!fx) {
        fx = new QGraphicsOpacityEffect(&panBtn);
        panBtn.setGraphicsEffect(fx);
    }
    fx->setOpacity(connected ? 1.0 : 0.35);
    panBtn.setToolTip(connected
        ? QStringLiteral("Change panadapter layout")
        : QStringLiteral("Connect a radio to change pan layout"));
}

} // namespace

class TstPanMenuRouting : public QObject {
    Q_OBJECT
private slots:
    void panButtonIsAnIconNotText()
    {
        QLabel btn;
        buildPanIcon(btn);
        QVERIFY2(btn.text().isEmpty(), qPrintable(btn.text()));
        QVERIFY(!btn.pixmap().isNull());
    }

    void panButtonIsDimWhenDisconnected()
    {
        QLabel btn;
        buildPanIcon(btn);
        // Disconnected at construction; the affordance must read
        // unavailable before the click, not silently no-op after it.
        applyConnectionState(btn, false);
        QVERIFY(!btn.isEnabled());
        QVERIFY(btn.toolTip().contains(QStringLiteral("Connect a radio")));
    }

    void panButtonTooltipCarriesNoSourceCite()
    {
        QLabel btn;
        buildPanIcon(btn);

        applyConnectionState(btn, false);
        QVERIFY(!btn.toolTip().contains(QStringLiteral("AetherSDR")));
        QVERIFY(!btn.toolTip().contains(QLatin1Char('@')));

        applyConnectionState(btn, true);
        QVERIFY(!btn.toolTip().contains(QStringLiteral("AetherSDR")));
        QVERIFY(!btn.toolTip().contains(QLatin1Char('@')));
    }
};
QTEST_MAIN(TstPanMenuRouting)
#include "tst_pan_menu_routing.moc"
