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
//
// Remote transmit hold then cut (Task 7, decision 7) added the TX badge's
// "Held" cases. They drive the real TxHeldIndicator, the object MainWindow
// paints the TX badge through, against a real TransmitState and a real
// StatusBadge, so no MainWindow is needed for them.
//
// Modification history (NereusSDR):
//   2026-10-27: Remote transmit hold then cut: the "Held" badge cases (a
//               hold the window sees, one it learns of only after it
//               ended, a whole delta read at once, a serial that goes
//               down, MOX dropping, TX Inhibit over every state) and
//               "Held" fitting the TX slot and drawn in amber in the
//               classic and workspace looks. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QCoreApplication>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>

#include "gui/widgets/StatusBadge.h"
#include "gui/widgets/AdcOverloadBadge.h"
#include "gui/chrome/ChromeBarItems.h"
#include "gui/TxHeldIndicator.h"
#include "core/session/TransmitStateFacade.h"

#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"

#include <QElapsedTimer>
#include <cstdlib>
#include <QImage>
#include <QScopeGuard>
#include <QStatusBar>

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
        badge->setToolTip(u"PA status: FAULT (the PA tripped and MOX dropped)"_s);
    }

    static void applyPaOk(QLabel* badge)
    {
        badge->setText(u"PA OK"_s);
        badge->setStyleSheet(u"QLabel { color: #60ff60; font-weight: bold;"
                             " font-size: 11px; padding: 2px 6px; }"_s);
        badge->setToolTip(u"PA status: OK"_s);
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
        badge.setToolTip(u"PA status: OK"_s);

        QCOMPARE(badge.text(), u"PA OK"_s);
        QVERIFY(badge.toolTip().contains(u"OK"_s));
    }

    // ── 3. setPaTripped(true) changes badge text to "PA FAULT" ───────────
    //
    // Mirrors MainWindow::setPaTripped(true):
    //   m_paStatusBadge->setText("PA FAULT");
    //   m_paStatusBadge->setStyleSheet("... color: #ff6060 ...");
    //   m_paStatusBadge->setToolTip("PA status: FAULT ...");

    void setPaTripped_true_changesBadgeText()
    {
        QLabel badge(u"PA OK"_s);
        badge.setObjectName(u"paStatusBadge"_s);
        badge.setStyleSheet(
            u"QLabel { color: #60ff60; font-weight: bold; font-size: 11px; padding: 2px 6px; }"_s);
        badge.setToolTip(u"PA status: OK"_s);

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
    //   m_paStatusBadge->setToolTip("PA status: OK");

    void setPaTripped_false_changesBadgeText()
    {
        QLabel badge(u"PA FAULT"_s);
        badge.setObjectName(u"paStatusBadge"_s);
        badge.setStyleSheet(
            u"QLabel { color: #ff6060; font-weight: bold; font-size: 11px; padding: 2px 6px; }"_s);
        badge.setToolTip(u"PA status: FAULT (the PA tripped and MOX dropped)"_s);

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

        // Remote transmit hold then cut: the TX badge reads "Held" while
        // the Core holds a remote key's RF, in the same slot.
        StatusBadge held;
        held.setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
        held.setLabel(QStringLiteral("Held"));
        held.setVariant(StatusBadge::Variant::Warn);
        held.ensurePolished();
        QVERIFY2(held.sizeHint().width() <= kSlotWidth,
                 qPrintable(QStringLiteral("Held badge needs %1 px, slot is %2")
                                .arg(held.sizeHint().width()).arg(kSlotWidth)));
    }

private:
    // ── Remote transmit hold then cut: the "Held" TX badge ───────────────

    // A txState delta as StationClient applies one: every value in order,
    // synchronously, before control returns to the event loop.
    static void applyDelta(TransmitState& state,
                           const QList<QPair<QByteArray, QVariant>>& values)
    {
        for (const auto& [name, value] : values) {
            QVERIFY(state.applyStationValue(name, value));
        }
    }

    static void expectBadge(const TxHeldIndicator& held, const QString& label,
                            StatusBadge::Variant variant)
    {
        StatusBadge badge;
        held.applyTo(&badge, false);
        QCOMPARE(badge.label(), label);
        QCOMPARE(badge.variant(), variant);
        QCOMPARE(badge.svgIcon(), u":/icons/badge-dot.svg"_s);
    }

private slots:

    // Keyed and rfHeld: "Held" in amber; rfHeld false while keyed: "TX" in
    // red; unkeyed: "TX" dim, as before. The Core window's own state.
    void aKeyedHoldReadsHeldInAmber()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Off);

        held.setKeyed(true);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Tx);

        state.setRfHold(true, QString::fromLatin1(TransmitState::kHoldLink));
        QTRY_VERIFY(held.showsHeld());
        expectBadge(held, u"Held"_s, StatusBadge::Variant::Warn);

        // The reason changing keeps "Held".
        state.setRfHold(true, QString::fromLatin1(TransmitState::kHoldMic));
        QCoreApplication::processEvents();
        expectBadge(held, u"Held"_s, StatusBadge::Variant::Warn);

        // A hold this window saw ends: back to TX, nothing shown after it.
        state.setRfHold(false, QString());
        QTRY_VERIFY(!held.showsHeld());
        QCOMPARE(state.rfHoldSerial(), quint64(1));
        QCOMPARE(held.afterHoldMs(), 0);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Tx);

        held.setKeyed(false);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Off);
    }

    // A hold while this window is not keyed shows nothing: unkeyed is "TX"
    // dim whatever the Core holds.
    void aHoldWhileUnkeyedIsTxDim()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        state.setRfHold(true, QString::fromLatin1(TransmitState::kHoldLink));
        QCoreApplication::processEvents();
        QVERIFY(!held.showsHeld());
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Off);

        // Keying while the Core still reads held shows it.
        held.setKeyed(true);
        expectBadge(held, u"Held"_s, StatusBadge::Variant::Warn);
        held.setKeyed(false);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Off);
    }

    // A remote window whose link was the one that went quiet: rfHoldSerial
    // advances without rfHeld ever true for it. "Held" for rfLastHoldMs,
    // then "TX".
    void aHoldTheWindowNeverSawShowsHeldForItsLength()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);

        QElapsedTimer since;
        since.start();
        applyDelta(state, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 400}});
        QTRY_VERIFY(held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 400);
        expectBadge(held, u"Held"_s, StatusBadge::Variant::Warn);

        QTRY_VERIFY_WITH_TIMEOUT(!held.showsHeld(), 2000);
        QVERIFY2(since.elapsed() >= 380,
                 qPrintable(QStringLiteral("Held went after %1 ms").arg(since.elapsed())));
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Tx);
    }

    // The shown length is min(rfLastHoldMs, 3000).
    void theLengthShownIsCappedAt3000Ms()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        applyDelta(state, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 5200}});
        QTRY_VERIFY(held.showsHeld());
        QCOMPARE(held.afterHoldMs(), TxHeldIndicator::kMaxAfterHoldMs);
        QCOMPARE(TxHeldIndicator::kMaxAfterHoldMs, 3000);
    }

    // holdChanged comes once per property, rfHoldSerial before
    // rfLastHoldMs. The indicator reads the hold only once the whole
    // delta is in, so the length shown is this hold's, not the last one's.
    void aDeltaIsReadWholeSerialBeforeLength()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);

        // A first hold the window saw, 2500 ms long.
        applyDelta(state, {{"rfHeld", true}, {"rfHeldReason", u"link"_s}});
        QTRY_VERIFY(held.showsHeld());
        applyDelta(state, {{"rfHeld", false}, {"rfHeldReason", QString()},
                           {"rfHoldSerial", 1}, {"rfLastHoldMs", 2500}});
        QTRY_VERIFY(!held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 0);

        // A second the window never saw, 150 ms. Reacting to rfHoldSerial
        // as it lands would read 2500.
        int changes = 0;
        connect(&held, &TxHeldIndicator::changed, this, [&changes] { ++changes; });
        applyDelta(state, {{"rfHoldSerial", 2}, {"rfLastHoldMs", 150}});
        QCOMPARE(changes, 0);
        QVERIFY(!held.showsHeld());
        QTRY_VERIFY(held.showsHeld());
        QCOMPARE(changes, 1);
        QCOMPARE(held.afterHoldMs(), 150);
        QTRY_VERIFY_WITH_TIMEOUT(!held.showsHeld(), 1000);
    }

    // The first hold a fresh window learns of after it ended reads its
    // length too (the length before it is 0, which would show nothing).
    void aFirstDeltaIsReadWholeToo()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        applyDelta(state, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 600}});
        QTRY_VERIFY(held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 600);
    }

    // MOX dropping ends a "Held" shown after the hold at once, and keying
    // again does not bring it back.
    void moxDroppingEndsAnAfterHoldAtOnce()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        applyDelta(state, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 2000}});
        QTRY_VERIFY(held.showsHeld());

        held.setKeyed(false);
        QVERIFY(!held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 0);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Off);

        held.setKeyed(true);
        QVERIFY(!held.showsHeld());
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Tx);
    }

    // A hold that ended while this window was not keyed is not shown.
    void aHoldEndedWhileUnkeyedIsNotShown()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        applyDelta(state, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 2000}});
        QCoreApplication::processEvents();
        held.setKeyed(true);
        QVERIFY(!held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 0);
    }

    // A remote window that saw rfHeld true shows "Held" while it is, and
    // nothing after it ends.
    void aHoldTheWindowSawShowsNothingAfter()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        applyDelta(state, {{"rfHeld", true}, {"rfHeldReason", u"mic"_s}});
        QTRY_VERIFY(held.showsHeld());
        applyDelta(state, {{"rfHeld", false}, {"rfHeldReason", QString()},
                           {"rfHoldSerial", 1}, {"rfLastHoldMs", 900}});
        QTRY_VERIFY(!held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 0);
        expectBadge(held, u"TX"_s, StatusBadge::Variant::Tx);
    }

    // rfHoldSerial goes down when the client moves to another Core
    // (clearStationValues, then the new Core's count). That is no hold
    // ended, never one to show; the new count is followed from there.
    void aSerialThatGoesDownIsNoHoldEnded()
    {
        TransmitState state;
        applyDelta(state, {{"rfHoldSerial", 5}, {"rfLastHoldMs", 800}});
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        QCoreApplication::processEvents();
        QVERIFY(!held.showsHeld());

        state.clearStationValues();
        applyDelta(state, {{"rfHoldSerial", 2}, {"rfLastHoldMs", 1200}});
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(!held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 0);

        // The new Core's next hold, unseen, is shown.
        applyDelta(state, {{"rfHoldSerial", 3}, {"rfLastHoldMs", 300}});
        QTRY_VERIFY(held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 300);
    }

    // A serial that goes down also ends a "Held" being shown for the old
    // Core's hold.
    void aSerialThatGoesDownEndsAnAfterHold()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        applyDelta(state, {{"rfHoldSerial", 4}, {"rfLastHoldMs", 2500}});
        QTRY_VERIFY(held.showsHeld());
        applyDelta(state, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 2500}});
        QTRY_VERIFY(!held.showsHeld());
        QCOMPARE(held.afterHoldMs(), 0);
    }

    // Following another TransmitState takes what it reads as seen: its
    // serial is no hold that just ended.
    void aNewStateIsTakenAsSeen()
    {
        TransmitState first;
        TransmitState second;
        applyDelta(second, {{"rfHoldSerial", 7}, {"rfLastHoldMs", 1500}});
        TxHeldIndicator held;
        held.setTransmitState(&first);
        held.setKeyed(true);
        held.setTransmitState(&second);
        QCoreApplication::processEvents();
        QVERIFY(!held.showsHeld());

        // The one it no longer follows is not heard.
        applyDelta(first, {{"rfHeld", true}});
        QCoreApplication::processEvents();
        QVERIFY(!held.showsHeld());

        held.setTransmitState(nullptr);
        applyDelta(second, {{"rfHeld", true}});
        QCoreApplication::processEvents();
        QVERIFY(!held.showsHeld());
    }

    // TX Inhibit's prohibition icon paints over the badge in every state:
    // unkeyed, keyed, held, and held after the hold.
    void txInhibitPaintsOverEveryHeldState()
    {
        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);

        const auto expectInhibited = [&held]() {
            StatusBadge badge;
            badge.setToolTip(u"Transmit blocked by an external TX Inhibit signal."_s);
            held.applyTo(&badge, true);
            QCOMPARE(badge.svgIcon(), u":/icons/badge-prohibited.svg"_s);
            QCOMPARE(badge.variant(), StatusBadge::Variant::Tx);
            QCOMPARE(badge.label(), u"TX"_s);
            // The inhibit reason stays the caller's.
            QVERIFY(badge.toolTip().contains(u"blocked"_s));
        };

        expectInhibited();
        held.setKeyed(true);
        expectInhibited();
        state.setRfHold(true, QString::fromLatin1(TransmitState::kHoldLink));
        QTRY_VERIFY(held.showsHeld());
        expectInhibited();
        state.setRfHold(false, QString());
        QTRY_VERIFY(!held.showsHeld());

        TransmitState remote;
        held.setTransmitState(&remote);
        applyDelta(remote, {{"rfHoldSerial", 1}, {"rfLastHoldMs", 1000}});
        QTRY_VERIFY(held.showsHeld());
        expectInhibited();
        // Cleared: the badge goes back to what the hold says.
        expectBadge(held, u"Held"_s, StatusBadge::Variant::Warn);
    }

    // The status bar's safety group with the TX badge reading "Held", in
    // the classic look and the workspace look, built as buildStatusBar()
    // builds it. The badge's amber is checked in the picture; with
    // NEREUS_CAPTURE_DIR set the pictures are saved there for review.
    void theHeldBadgeIsAmberInBothLooks_data()
    {
        QTest::addColumn<bool>("workspace");
        QTest::newRow("classic") << false;
        QTest::newRow("workspace") << true;
    }

    void theHeldBadgeIsAmberInBothLooks()
    {
        QFETCH(bool, workspace);
        WorkspacePreview::setEnabledForTest(workspace);
        const auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });

        QStatusBar bar;
        bar.setSizeGripEnabled(false);
        if (workspace) {
            const auto& wp = Workspace::WorkspacePalette::glassAndDepth();
            bar.setStyleSheet(QStringLiteral(
                "QStatusBar { background: %1; border-top: 1px solid %2; }"
                "QStatusBar::item { border: none; }")
                .arg(wp.qss(QStringLiteral("statusbar-bg")),
                     wp.qss(QStringLiteral("statusbar-border"))));
        } else {
            bar.setStyleSheet(QStringLiteral(
                "QStatusBar { background: #0a0a14; border-top: 1px solid #203040; }"
                "QStatusBar::item { border: none; }"));
        }
        auto* barWidget = new QWidget(&bar);
        auto* hbox = new QHBoxLayout(barWidget);
        hbox->setContentsMargins(6, 0, 6, 0);
        auto* group = new QWidget(barWidget);
        group->setObjectName(QStringLiteral("safetyGroup"));
        group->setStyleSheet(QStringLiteral(
            "QWidget#safetyGroup { border-left: 1px solid #203040; }"));
        auto* safetyRow = new QHBoxLayout(group);
        safetyRow->setContentsMargins(8, 0, 0, 0);
        safetyRow->setSpacing(6);
        auto* ovl = new AdcOverloadBadge(barWidget);
        ovl->setObjectName(QStringLiteral("adcOvlBadge"));
        auto* tx = new StatusBadge(barWidget);
        tx->setObjectName(QStringLiteral("txStatusBadge"));
        addSlot(safetyRow, group, ovl, kOverloadSlotWidthPx);
        addSlot(safetyRow, group, tx);
        hbox->addWidget(group);
        hbox->addStretch(1);
        bar.addWidget(barWidget, 1);
        dimBadge(ovl, false);

        TransmitState state;
        TxHeldIndicator held;
        held.setTransmitState(&state);
        held.setKeyed(true);
        state.setRfHold(true, QString::fromLatin1(TransmitState::kHoldLink));
        QTRY_VERIFY(held.showsHeld());
        held.applyTo(tx, false);
        if (workspace) {
            WorkspacePreview::themeSubtree(&bar);
        }

        bar.resize(260, 28);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        QCoreApplication::processEvents();
        const QImage shot = bar.grab().toImage();

        // The amber the badge draws in this look, somewhere on the badge.
        const QColor amber = WorkspacePreview::statusColour(QColor(QStringLiteral("#ffd700")),
                                                            "measured");
        const QRect badgeRect(tx->mapTo(&bar, QPoint(0, 0)) * shot.devicePixelRatio(),
                              tx->size() * shot.devicePixelRatio());
        int amberPixels = 0;
        for (int y = badgeRect.top(); y <= badgeRect.bottom() && y < shot.height(); ++y) {
            for (int x = badgeRect.left(); x <= badgeRect.right() && x < shot.width(); ++x) {
                const QColor c = shot.pixelColor(x, y);
                if (std::abs(c.red() - amber.red()) < 40 && std::abs(c.green() - amber.green()) < 40
                    && std::abs(c.blue() - amber.blue()) < 40) {
                    ++amberPixels;
                }
            }
        }
        QVERIFY2(amberPixels > 20,
                 qPrintable(QStringLiteral("%1 amber pixels on the Held badge").arg(amberPixels)));

        const QString dir = qEnvironmentVariable("NEREUS_CAPTURE_DIR");
        if (!dir.isEmpty()) {
            const QString name = QStringLiteral("%1/tx-badge-held-%2.png")
                                     .arg(dir, workspace ? u"workspace"_s : u"classic"_s);
            QVERIFY2(shot.save(name), qPrintable(name));
        }
    }
};

QTEST_MAIN(TestMainWindowStatusBarSafety)
#include "tst_mainwindow_status_bar_safety.moc"
