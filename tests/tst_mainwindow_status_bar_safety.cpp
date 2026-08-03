// =================================================================
// tests/tst_mainwindow_status_bar_safety.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: widget-level construction / accessor test for the TX
// Inhibit indicator and PA Status badge added to the MainWindow status
// bar in Phase 3M-0 Task 14.
//
// MainWindow requires a full RadioModel (WDSP, audio, network) to
// construct, which is too heavyweight for a unit-test executable.
// These tests therefore QSKIP the MainWindow-instantiation path and
// instead verify the logic of the free setPaTripped() helper slot by
// exercising the label-update code through a standalone QLabel pair.
// Full widget-level verification that txInhibitLabel() and
// paStatusBadge() return non-null after construction happens during
// the Task 17 visual integration pass.
//
// Covered:
//   1. txInhibitLabel_hiddenByDefault      — label starts invisible
//   2. paStatusBadge_showsOkByDefault      — badge shows "PA OK"
//   3. setPaTripped_true_changesBadgeText  — text switches to "PA FAULT"
//   4. setPaTripped_false_changesBadgeText — text reverts to "PA OK"
//   5. safetySlotsHoldGeometryWhenAnAlarmFires — dimming, not hiding,
//      a slot's badge leaves a later sibling's position unchanged
//   6. everySafetySlotIsFixedWidth — each of the 4 safety slots pins
//      minimumWidth()==maximumWidth()==50
//
// Task A6 (design §4.5) added 5/6 and confirmed the note above still
// holds: a literal MainWindow w; in this harness was built and run
// (not just assumed) and it starts real RadioDiscovery broadcasts on
// the LAN, takes ~9 s to construct the auto-opened ConnectionPanel, and
// SIGABRTs the whole test binary on teardown ("QThread: Destroyed while
// thread 'SpectrumThread' is still running"). tst_pan_active_slice_sync.cpp
// and tst_pan_badge_click_wiring.cpp independently document the same
// "MainWindow is not constructible in this harness" conclusion. 5/6
// therefore mirror buildStatusBar()'s addSlot() lambda and
// MainWindow::dimSafetyBadge() logic verbatim against a standalone host
// widget, same as 1-4 above.
//
// Phase 3M-0 Task 14.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QCoreApplication>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>

#include "gui/widgets/StatusBadge.h"
#include "gui/widgets/AdcOverloadBadge.h"
#include "gui/chrome/ChromeBarItems.h"

using namespace Qt::StringLiterals;
using namespace NereusSDR;

class TestMainWindowStatusBarSafety : public QObject
{
    Q_OBJECT

private:
    // Standalone helpers that mirror the in-MainWindow state changes —
    // so we can exercise the logic without constructing MainWindow.
    static void applyPaFault(QLabel* badge)
    {
        badge->setText(u"PA FAULT"_s);
        badge->setStyleSheet(u"QLabel { color: #ff6060; font-weight: bold;"
                             " font-size: 11px; padding: 2px 6px; }"_s);
        badge->setToolTip(u"PA Status — FAULT (PA tripped, MOX dropped)"_s);
    }

    static void applyPaOk(QLabel* badge)
    {
        badge->setText(u"PA OK"_s);
        badge->setStyleSheet(u"QLabel { color: #60ff60; font-weight: bold;"
                             " font-size: 11px; padding: 2px 6px; }"_s);
        badge->setToolTip(u"PA Status — OK"_s);
    }

private slots:

    // ── 1. TX Inhibit label is dimmed by default ───────────────────────────
    //
    // Mirrors the CURRENT buildStatusBar() contract (bottom-banner cleanup
    // Task A6, design §4.5): the label lives permanently in its reserved
    // safety slot and is dimmed via dimSafetyBadge(), never hidden with
    // setVisible(false) -- a fixed-width slot showing at 14% opacity holds
    // its geometry; an outright-hidden widget would collapse it. This slot
    // used to assert the pre-A6 "TX INHIBIT" / setVisible(false) / em-dash
    // tooltip contract, none of which buildStatusBar() does any more, and
    // passed anyway because it is a self-contained mirror -- Task A8
    // Step 5b retires that drift.

    void txInhibitLabel_hiddenByDefault()
    {
        QLabel label(u"INH"_s);
        label.setObjectName(u"txInhibitLabel"_s);
        label.setStyleSheet(
            u"QLabel { color: #ff6060; font-weight: bold; font-size: 11px;"
            "         padding: 2px 6px; border: 1px solid #ff6060; border-radius: 3px; }"_s);
        label.setToolTip(u"External TX Inhibit asserted. TX is blocked."_s);
        dimBadge(&label, false);

        // Dimming (opacity), not setVisible(false), is the mechanism.
        // isVisible() itself is not asserted: an unshown, unparented
        // top-level QLabel like this one defaults to invisible regardless
        // of dimming state, so that read would be vacuous either way (see
        // the file-header note on asserting the constraint, not the
        // laid-out geometry).
        auto* fx = qobject_cast<QGraphicsOpacityEffect*>(label.graphicsEffect());
        QVERIFY(fx);
        QCOMPARE(fx->opacity(), 0.14);
        QCOMPARE(label.text(), u"INH"_s);
    }

    // ── 2. PA Status badge shows "PA OK" by default ───────────────────────
    //
    // Mirrors the buildStatusBar() contract:
    //   m_paStatusBadge->setText("PA OK");
    //   m_paStatusBadge->setStyleSheet("... color: #60ff60 ...");

    void paStatusBadge_showsOkByDefault()
    {
        QLabel badge(u"PA OK"_s);
        badge.setObjectName(u"paStatusBadge"_s);
        badge.setStyleSheet(
            u"QLabel { color: #60ff60; font-weight: bold; font-size: 11px; padding: 2px 6px; }"_s);
        badge.setToolTip(u"PA Status — OK"_s);

        QCOMPARE(badge.text(), u"PA OK"_s);
        QVERIFY(badge.toolTip().contains(u"OK"_s));
    }

    // ── 3. setPaTripped(true) changes badge text to "PA FAULT" ───────────
    //
    // Mirrors MainWindow::setPaTripped(true):
    //   m_paStatusBadge->setText("PA FAULT");
    //   m_paStatusBadge->setStyleSheet("... color: #ff6060 ...");
    //   m_paStatusBadge->setToolTip("PA Status — FAULT ...");

    void setPaTripped_true_changesBadgeText()
    {
        QLabel badge(u"PA OK"_s);
        badge.setObjectName(u"paStatusBadge"_s);
        badge.setStyleSheet(
            u"QLabel { color: #60ff60; font-weight: bold; font-size: 11px; padding: 2px 6px; }"_s);
        badge.setToolTip(u"PA Status — OK"_s);

        // simulate setPaTripped(true)
        applyPaFault(&badge);

        QCOMPARE(badge.text(), u"PA FAULT"_s);
        QVERIFY(badge.toolTip().contains(u"FAULT"_s));
    }

    // ── 4. setPaTripped(false) reverts badge text to "PA OK" ─────────────
    //
    // Mirrors MainWindow::setPaTripped(false):
    //   m_paStatusBadge->setText("PA OK");
    //   m_paStatusBadge->setStyleSheet("... color: #60ff60 ...");
    //   m_paStatusBadge->setToolTip("PA Status — OK");

    void setPaTripped_false_changesBadgeText()
    {
        QLabel badge(u"PA FAULT"_s);
        badge.setObjectName(u"paStatusBadge"_s);
        badge.setStyleSheet(
            u"QLabel { color: #ff6060; font-weight: bold; font-size: 11px; padding: 2px 6px; }"_s);
        badge.setToolTip(u"PA Status — FAULT (PA tripped, MOX dropped)"_s);

        // simulate setPaTripped(false)
        applyPaOk(&badge);

        QCOMPARE(badge.text(), u"PA OK"_s);
        QVERIFY(badge.toolTip().contains(u"OK"_s));
        QVERIFY(!badge.toolTip().contains(u"FAULT"_s));
    }

private:
    // Mirrors MainWindow::buildStatusBar()'s addSlot() lambda verbatim
    // (design §4.5): a fixed-50px "safetySlot" QWidget wrapping one badge,
    // added to a "safetyGroup" host's QHBoxLayout. See file header for why
    // this is built standalone instead of via a real MainWindow.
    static void addSlot(QHBoxLayout* safetyRow, QWidget* group, QWidget* badge)
    {
        auto* slot = new QWidget(group);
        slot->setObjectName(QStringLiteral("safetySlot"));
        slot->setFixedWidth(kSafetySlotWidthPx);
        auto* sl = new QHBoxLayout(slot);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->addWidget(badge);
        badge->setParent(slot);
        safetyRow->addWidget(slot);
    }

    // Mirrors MainWindow::dimSafetyBadge() verbatim (design §4.5):
    // opacity-only state change, never setVisible(), so a dimmed slot
    // never changes the layout's required width.
    static void dimBadge(QWidget* w, bool active)
    {
        auto* fx = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
        if (!fx) {
            fx = new QGraphicsOpacityEffect(w);
            w->setGraphicsEffect(fx);
        }
        fx->setOpacity(active ? 1.0 : 0.14);
    }

private slots:

    // ── 5. Reserved safety slots hold geometry when an alarm fires ────────
    //
    // Task A6 (design §4.5): the safety group gives TX INHIBIT / PA /
    // ADC overload / TX four permanently allocated 50 px slots so an
    // alarm dims the badge inside its slot instead of inserting/removing
    // a widget and sliding everything after it sideways. Proven here by
    // showing the host so layout actually runs (see class-level note: an
    // unshown window's geometry reads are vacuous), then comparing the TX
    // slot's position before/after the ADC badge is dimmed to "active".

    void safetySlotsHoldGeometryWhenAnAlarmFires()
    {
        QWidget host;
        auto* hbox = new QHBoxLayout(&host);
        auto* group = new QWidget(&host);
        group->setObjectName(QStringLiteral("safetyGroup"));
        auto* safetyRow = new QHBoxLayout(group);
        safetyRow->setContentsMargins(8, 0, 0, 0);
        safetyRow->setSpacing(6);

        auto* txInhibit = new QLabel(QStringLiteral("INH"), &host);
        auto* pa = new StatusBadge(&host);
        auto* ovl = new AdcOverloadBadge(&host);
        ovl->setObjectName(QStringLiteral("adcOvlBadge"));
        auto* tx = new StatusBadge(&host);
        tx->setObjectName(QStringLiteral("txStatusBadge"));

        addSlot(safetyRow, group, txInhibit);
        addSlot(safetyRow, group, pa);
        addSlot(safetyRow, group, ovl);
        addSlot(safetyRow, group, tx);
        hbox->addWidget(group);
        dimBadge(txInhibit, false);
        dimBadge(ovl, false);

        host.resize(400, 60);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        const QPoint before = tx->mapTo(&host, QPoint(0, 0));

        // Fire the alarm the same way overloadStatusChanged's handler does:
        // dim to full opacity, never setVisible().
        ovl->setAdcs(QStringLiteral("0"));
        ovl->setVariant(AdcOverloadBadge::Variant::Tx);
        dimBadge(ovl, true);
        QCoreApplication::processEvents();

        const QPoint after = tx->mapTo(&host, QPoint(0, 0));
        QCOMPARE(after, before);
    }

    void everySafetySlotIsFixedWidth()
    {
        QWidget host;
        auto* hbox = new QHBoxLayout(&host);
        auto* group = new QWidget(&host);
        group->setObjectName(QStringLiteral("safetyGroup"));
        auto* safetyRow = new QHBoxLayout(group);

        addSlot(safetyRow, group, new QLabel(QStringLiteral("INH"), &host));
        addSlot(safetyRow, group, new StatusBadge(&host));
        addSlot(safetyRow, group, new AdcOverloadBadge(&host));
        addSlot(safetyRow, group, new StatusBadge(&host));
        hbox->addWidget(group);

        const QList<QWidget*> slotWidgets =
            group->findChildren<QWidget*>(QStringLiteral("safetySlot"),
                                          Qt::FindDirectChildrenOnly);
        QCOMPARE(slotWidgets.size(), 4);
        for (QWidget* s : slotWidgets) {
            // Assert the CONSTRAINT, not the laid-out geometry. Qt does not
            // lay out an unshown window, so width() would read the default
            // 100 here and fail for a reason that has nothing to do with
            // the fix. setFixedWidth pins both bounds, so this is the
            // property the reserved-slot design actually depends on.
            QCOMPARE(s->minimumWidth(), kSafetySlotWidthPx);
            QCOMPARE(s->maximumWidth(), kSafetySlotWidthPx);
        }
    }

    // Smoke test on a real ANAN-G2E, 2026-08-03, caught what every test in
    // this file missed: AdcOverloadBadge rendered the literal word
    // "OVERLOAD", which needs about 78 px including its own 8 px side
    // margins, inside a 50 px reserved slot. On screen it read "/ERLO/".
    //
    // Nothing here asserted that a badge FITS the slot it was given. The
    // slot-width test above pins the slot; this one pins the contents, so
    // any future badge whose text outgrows its reservation fails here
    // rather than on someone's bench.
    void everySafetyBadgeFitsItsReservedSlot() {
        constexpr int kSlotWidth = kSafetySlotWidthPx;

        AdcOverloadBadge ovl;
        // Widest realistic case: all three ADCs overloading at once.
        ovl.setAdcs(QStringLiteral("0/1/2"));
        ovl.ensurePolished();
        QVERIFY2(ovl.sizeHint().width() <= kSlotWidth,
                 qPrintable(QStringLiteral("AdcOverloadBadge needs %1 px, slot is %2")
                                .arg(ovl.sizeHint().width()).arg(kSlotWidth)));

        StatusBadge pa;
        pa.setLabel(QStringLiteral("PA"));
        pa.ensurePolished();
        QVERIFY2(pa.sizeHint().width() <= kSlotWidth,
                 qPrintable(QStringLiteral("PA badge needs %1 px, slot is %2")
                                .arg(pa.sizeHint().width()).arg(kSlotWidth)));

        StatusBadge tx;
        tx.setLabel(QStringLiteral("TX"));
        tx.ensurePolished();
        QVERIFY2(tx.sizeHint().width() <= kSlotWidth,
                 qPrintable(QStringLiteral("TX badge needs %1 px, slot is %2")
                                .arg(tx.sizeHint().width()).arg(kSlotWidth)));

        // The TX-inhibit pill is a plain QLabel in buildStatusBar, styled
        // the same way and carrying the same shortened text.
        QLabel inh(QStringLiteral("INH"));
        inh.setStyleSheet(QStringLiteral(
            "QLabel { color: #ff6060; font-weight: bold; font-size: 11px;"
            "         padding: 2px 6px; border: 1px solid #ff6060;"
            "         border-radius: 3px; }"));
        inh.ensurePolished();
        QVERIFY2(inh.sizeHint().width() <= kSlotWidth,
                 qPrintable(QStringLiteral("INH pill needs %1 px, slot is %2")
                                .arg(inh.sizeHint().width()).arg(kSlotWidth)));
    }
};

QTEST_MAIN(TestMainWindowStatusBarSafety)
#include "tst_mainwindow_status_bar_safety.moc"
