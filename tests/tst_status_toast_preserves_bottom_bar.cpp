// =================================================================
// tests/tst_status_toast_preserves_bottom_bar.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. StatusToast has no upstream; the
// behaviour under test is a Qt contract plus a NereusSDR decision.
//
// Bench report 2026-07-30 (JJ, KG4VCF): pressing TUNE with PureSignal
// active replaced the entire bottom bar with a single line of text for
// six seconds, taking the CH pill, the PureSignal indicator, the radio
// name, CAT and TCI state, the PA and TX badges and the clock with it.
//
// The message was correct. The surface was not. MainWindow::buildStatusBar
// adds the whole bar as ONE widget via QStatusBar::addWidget, and Qt hides
// every non-permanent widget for the full duration of a showMessage call.
// So every notice in the tree cost the operator the bar, and the two that
// fire during transmit cost it at the worst possible moment.
//
// Case 1 is the root cause, asserted rather than described: it fails if
// Qt ever stops hiding normal widgets, which would mean this whole change
// is unnecessary. It is the reason the other cases exist.
//
// MainWindow itself is not constructed here. It needs a full RadioModel
// (WDSP, audio, network), which tst_mainwindow_status_bar_safety.cpp
// already documents as too heavyweight for a unit test. These cases work
// on a QStatusBar assembled the same way buildStatusBar assembles it.
// =================================================================

#include <QtTest/QtTest>

#include <QLabel>
#include <QMouseEvent>
#include <QStatusBar>

#include "gui/widgets/StatusToast.h"

using namespace NereusSDR;
using namespace Qt::StringLiterals;

class TestStatusToastPreservesBottomBar : public QObject
{
    Q_OBJECT

private:
    /// A status bar carrying one full-width child, exactly as
    /// MainWindow::buildStatusBar builds it:
    ///
    ///     sb->addWidget(barWidget, 1);
    ///
    /// The stretch matters to the real window's layout but not to
    /// visibility, which is what these cases are about.
    static QStatusBar* barWithFullWidthChild(QLabel** childOut)
    {
        auto* sb = new QStatusBar();
        auto* child = new QLabel(u"CH 0 | PS-A | PA | TX | 22:46 UTC"_s, sb);
        sb->addWidget(child, 1);
        sb->show();
        *childOut = child;
        return sb;
    }

private slots:

    // ── 1. The root cause, pinned ────────────────────────────────────────
    //
    // This is why StatusToast exists. If this case ever fails, Qt has
    // changed its showMessage semantics and the rest of this file can be
    // reconsidered. Until then, no notice may go through showMessage,
    // because this is what it does to the bar.
    void show_message_hides_a_normal_bar_widget()
    {
        QLabel* child = nullptr;
        std::unique_ptr<QStatusBar> sb(barWithFullWidthChild(&child));
        QVERIFY(QTest::qWaitForWindowExposed(sb.get()));
        QVERIFY2(child->isVisible(),
                 "precondition: the bar contents start visible");

        sb->showMessage(u"Slice A has no receiver while PureSignal is "
                        "transmitting on this radio. Unkey to restore."_s,
                        60000);

        QVERIFY2(!child->isVisible(),
                 "QStatusBar::showMessage hides non-permanent widgets. This "
                 "is the whole bottom bar. Use MainWindow::showToast.");

        sb->clearMessage();
        QVERIFY2(child->isVisible(),
                 "the bar comes back when the message is cleared");
    }

    // ── 2. A toast leaves the bar alone ──────────────────────────────────
    //
    // The fix, stated as the property that was violated: showing a notice
    // must not change what the bottom bar is showing.
    void a_toast_leaves_the_bottom_bar_visible()
    {
        QLabel* child = nullptr;
        std::unique_ptr<QStatusBar> sb(barWithFullWidthChild(&child));
        QVERIFY(QTest::qWaitForWindowExposed(sb.get()));

        auto* toast = new StatusToast(
            u"Slice A has no receiver while PureSignal is transmitting on "
            "this radio. Unkey to restore."_s,
            ToastSeverity::Warning, 60000);
        toast->show();

        QVERIFY2(child->isVisible(),
                 "the bottom bar must survive a notice; that is the point");
        QVERIFY2(sb->currentMessage().isEmpty(),
                 "a toast must not route through the status bar at all");

        toast->close();
    }

    // ── 3. The message is readable back ──────────────────────────────────
    //
    // MainWindow::showToast collapses a repeat of a message already on
    // screen into the existing toast rather than stacking a duplicate.
    // That comparison reads message(), so it has to survive construction.
    void a_toast_reports_the_message_it_was_given()
    {
        const QString msg = u"Hermes Lite 2 supports a maximum of 1 slices"_s;
        std::unique_ptr<StatusToast> toast(
            new StatusToast(msg, ToastSeverity::Warning, 60000));

        QCOMPARE(toast->message(), msg);
    }

    // ── 4. It goes away on its own ───────────────────────────────────────
    void a_toast_dismisses_itself_when_its_timer_expires()
    {
        QPointer<StatusToast> toast(
            new StatusToast(u"TX > Slice B"_s, ToastSeverity::Info, 50));
        toast->show();
        QVERIFY(!toast.isNull());

        // WA_DeleteOnClose, so expiry destroys it rather than hiding it.
        QTRY_VERIFY_WITH_TIMEOUT(toast.isNull(), 5000);
    }

    // ── 5. It goes away when clicked ─────────────────────────────────────
    //
    // A notice the operator has already read should not sit on the
    // panadapter for its full timeout.
    void a_toast_dismisses_on_click()
    {
        QPointer<StatusToast> toast(
            new StatusToast(u"TX interlock blocked: SWR 3.1"_s,
                            ToastSeverity::Error, 60000));
        toast->show();
        QVERIFY(QTest::qWaitForWindowExposed(toast));

        QMouseEvent press(QEvent::MouseButtonPress,
                          QPointF(10, 10), QPointF(10, 10),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(toast, &press);

        QTRY_VERIFY_WITH_TIMEOUT(toast.isNull(), 5000);
    }

    // ── 6. Severity does not change the contract ─────────────────────────
    //
    // Severity drives the accent colour only. Every severity must still
    // stay off the status bar, including Error, which is the one that
    // fires during transmit.
    void every_severity_stays_off_the_status_bar()
    {
        QLabel* child = nullptr;
        std::unique_ptr<QStatusBar> sb(barWithFullWidthChild(&child));
        QVERIFY(QTest::qWaitForWindowExposed(sb.get()));

        for (const ToastSeverity sev : {ToastSeverity::Info,
                                        ToastSeverity::Warning,
                                        ToastSeverity::Error}) {
            auto* toast = new StatusToast(u"notice"_s, sev, 60000);
            toast->show();
            QVERIFY(child->isVisible());
            QVERIFY(sb->currentMessage().isEmpty());
            toast->close();
        }
    }
};

QTEST_MAIN(TestStatusToastPreservesBottomBar)
#include "tst_status_toast_preserves_bottom_bar.moc"
