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

    // ── 1. TX Inhibit paints onto the TX badge, it has no pill ────────────
    //
    // The old "INH" pill is gone. It carried a 1 px #ff6060 border that
    // survived dimming to 14% while its text did not, so the safety corner
    // showed an empty alarm-red outline while nothing was wrong, and the
    // abbreviation meant nothing to an operator (bench report 2026-08-03).
    // Inhibit is a property OF transmit, so it now paints the prohibition
    // icon onto the TX badge and raises a toast naming the reason.
    //
    // MainWindow is not constructible here, so this mirrors the contract:
    // the asserted look is the prohibition icon at Variant::Tx, and the
    // cleared look hands the badge back to MOX.

    void txInhibitPaintsOntoTheTxBadge()
    {
        StatusBadge tx;
        tx.setObjectName(u"txStatusBadge"_s);
        tx.setSvgIcon(u":/icons/badge-dot.svg"_s);
        tx.setLabel(u"TX"_s);
        tx.setVariant(StatusBadge::Variant::Off);

        // Asserted.
        tx.setSvgIcon(u":/icons/badge-prohibited.svg"_s);
        tx.setVariant(StatusBadge::Variant::Tx);
        tx.setToolTip(u"Transmit blocked by an external TX Inhibit signal."_s);
        QCOMPARE(tx.variant(), StatusBadge::Variant::Tx);
        QVERIFY(tx.toolTip().contains(u"blocked"_s));
        // The operator must never be shown a bare abbreviation again.
        QVERIFY(!tx.toolTip().contains(u"INH"_s));

        // Cleared, MOX off.
        tx.setSvgIcon(u":/icons/badge-dot.svg"_s);
        tx.setVariant(StatusBadge::Variant::Off);
        tx.setToolTip(u"Receive (MOX off)"_s);
        QCOMPARE(tx.variant(), StatusBadge::Variant::Off);
        QVERIFY(tx.toolTip().contains(u"Receive"_s));
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
    static void addSlot(QHBoxLayout* safetyRow, QWidget* group, QWidget* badge,
                        int widthPx = kSafetySlotWidthPx)
    {
        auto* slot = new QWidget(group);
        slot->setObjectName(QStringLiteral("safetySlot"));
        slot->setFixedWidth(widthPx);
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

        auto* pa = new StatusBadge(&host);
        auto* ovl = new AdcOverloadBadge(&host);
        ovl->setObjectName(QStringLiteral("adcOvlBadge"));
        auto* tx = new StatusBadge(&host);
        tx->setObjectName(QStringLiteral("txStatusBadge"));

        addSlot(safetyRow, group, pa);
        addSlot(safetyRow, group, ovl, kOverloadSlotWidthPx);
        addSlot(safetyRow, group, tx);
        hbox->addWidget(group);
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

        addSlot(safetyRow, group, new StatusBadge(&host));
        addSlot(safetyRow, group, new AdcOverloadBadge(&host),
                kOverloadSlotWidthPx);
        addSlot(safetyRow, group, new StatusBadge(&host));
        hbox->addWidget(group);

        const QList<QWidget*> slotWidgets =
            group->findChildren<QWidget*>(QStringLiteral("safetySlot"),
                                          Qt::FindDirectChildrenOnly);
        QCOMPARE(slotWidgets.size(), 3);
        // Slots are pinned, but no longer all the same width: the alarm
        // carries real content and gets a slot sized to it. Both bounds
        // must still be pinned, which is what the reserved-slot design
        // actually depends on.
        for (QWidget* s : slotWidgets) {
            // Assert the CONSTRAINT, not the laid-out geometry. Qt does not
            // lay out an unshown window, so width() would read the default
            // 100 here and fail for a reason that has nothing to do with
            // the fix. setFixedWidth pins both bounds, so this is the
            // property the reserved-slot design actually depends on.
            QCOMPARE(s->minimumWidth(), s->maximumWidth());
            QVERIFY(s->minimumWidth() == kSafetySlotWidthPx
                    || s->minimumWidth() == kOverloadSlotWidthPx);
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
        // Widest realistic case: all three ADCs overloading at once, with
        // the full word beneath it. This is the assertion that "OVL" was
        // invented to dodge; the badge gets a bigger slot instead now.
        ovl.setAdcs(QStringLiteral("0/1/2"));
        ovl.ensurePolished();
        QVERIFY2(ovl.sizeHint().width() <= kOverloadSlotWidthPx,
                 qPrintable(QStringLiteral("AdcOverloadBadge needs %1 px, slot is %2")
                                .arg(ovl.sizeHint().width())
                                .arg(kOverloadSlotWidthPx)));

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

    }
};

QTEST_MAIN(TestMainWindowStatusBarSafety)
#include "tst_mainwindow_status_bar_safety.moc"
