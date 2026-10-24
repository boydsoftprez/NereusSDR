// =================================================================
// tests/tst_vfo_flag_drag.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// A receiver flag can be dragged to tune: a left press on the flag's own
// surface (its background and labels, the frequency readout included) that
// moves past the drag distance tunes the flag's slice by the cursor's
// travel, converted with the pan's Hz per pixel and snapped to the step. A
// press that stays put is still a click (fold on the letter badge, open
// when folded). Buttons keep their own presses. A locked slice and one
// this window only listens to never move.
//
// Events are sent to the child under the point and reach the flag through
// QApplication's mouse propagation, as an ignored press on a label does in
// the app. Offscreen, so this proves the flag's handlers and the pan's
// conversion, not a real click through the macOS view tree.
// =================================================================
#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/ScrollableLabel.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

constexpr int    kW = 800;
constexpr int    kH = 400;
constexpr double kCenterHz = 14200000.0;
constexpr double kBandwidthHz = 80000.0;   // 100 Hz per pixel
constexpr double kHzPerPx = kBandwidthHz / kW;
constexpr int    kStepHz = 100;

// Sends a mouse event to whichever child of flag sits under flagPos.
void sendAt(VfoWidget* flag, QEvent::Type type, QPoint flagPos, int dxGlobal,
            Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QWidget* target = flag->childAt(flagPos);
    if (!target) {
        target = flag;
    }
    const QPoint local = target->mapFrom(flag, flagPos);
    const QPointF global = QPointF(flag->mapToGlobal(flagPos)) + QPointF(dxGlobal, 0);
    QMouseEvent me(type, QPointF(local) + QPointF(dxGlobal, 0), global, button, buttons,
                   Qt::NoModifier);
    QApplication::sendEvent(target, &me);
}

// Press at flagPos, move by each of dxs in turn, release at the last.
void drag(VfoWidget* flag, QPoint flagPos, std::initializer_list<int> dxs)
{
    sendAt(flag, QEvent::MouseButtonPress, flagPos, 0, Qt::LeftButton, Qt::LeftButton);
    int last = 0;
    for (int dx : dxs) {
        sendAt(flag, QEvent::MouseMove, flagPos, dx, Qt::NoButton, Qt::LeftButton);
        last = dx;
    }
    sendAt(flag, QEvent::MouseButtonRelease, flagPos, last, Qt::LeftButton, Qt::NoButton);
}

QLabel* letterBadge(VfoWidget& flag, const QString& letter)
{
    for (QLabel* label : flag.findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->text() == letter && label->size() == QSize(18, 18)) {
            return label;
        }
    }
    return nullptr;
}

void removeFoldKeys()
{
    for (int i = 0; i < 4; ++i) {
        AppSettings::instance().remove(VfoWidget::collapsedSettingsKey(i));
    }
}

struct Pan {
    SpectrumWidget sw;
    VfoWidget* flag{nullptr};
    explicit Pan(int slice = 1)
    {
        sw.resize(kW, kH);
        sw.setDbmScaleVisible(false);
        sw.setFrequencyRange(kCenterHz, kBandwidthHz);
        sw.setConnectionState(ConnectionState::Connected);
        sw.setStepSize(kStepHz);
        flag = sw.addVfoWidget(slice);
        flag->setFrequency(kCenterHz);
        flag->setFilter(-2850, -150);
        flag->setStepHz(kStepHz);
        sw.show();
        sw.updateVfoPositions();
        QCoreApplication::processEvents();
    }
    QPoint frequencyPoint() const { return flag->frequencyAreaForTest().center(); }
};

} // namespace

class TestVfoFlagDrag : public QObject {
    Q_OBJECT
private slots:
    void init() { removeFoldKeys(); }
    void cleanup() { removeFoldKeys(); }

    void drag_on_frequency_readout_tunes_by_travel()
    {
        Pan p;
        QVERIFY(p.flag->isVisible());
        const QPoint at = p.frequencyPoint();
        QVERIFY(!p.flag->frequencyAreaForTest().isEmpty());
        QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
        drag(p.flag, at, {30});
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 30 * kHzPerPx);
        QVERIFY(!p.flag->isFlagDragging());
        QVERIFY(!p.flag->frequencyEditOpen());
    }

    void drag_snaps_to_the_step()
    {
        Pan p;
        p.flag->setStepHz(1000);
        QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
        drag(p.flag, p.frequencyPoint(), {34});   // 3400 Hz of travel
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 3000.0);
    }

    void letter_badge_click_folds_and_drag_does_not()
    {
        {
            Pan p;
            QLabel* badge = letterBadge(*p.flag, QStringLiteral("B"));
            QVERIFY(badge);
            QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
            drag(p.flag, badge->geometry().center(), {2});
            QVERIFY(!p.flag->isCollapsed());   // deferred, as before
            QTRY_VERIFY(p.flag->isCollapsed());
            QCOMPARE(tuned.count(), 0);
        }
        {
            Pan p;
            QLabel* badge = letterBadge(*p.flag, QStringLiteral("B"));
            QVERIFY(badge);
            QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
            drag(p.flag, badge->geometry().center(), {15, 30});
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QVERIFY(!p.flag->isCollapsed());
            QVERIFY(tuned.count() >= 1);
            QCOMPARE(tuned.last().first().toDouble(), kCenterHz + 30 * kHzPerPx);
        }
    }

    void folded_flag_click_opens_and_drag_tunes()
    {
        {
            Pan p;
            p.flag->setCollapsed(true);
            drag(p.flag, QPoint(17, 10), {});
            QTRY_VERIFY(!p.flag->isCollapsed());
        }
        {
            Pan p;
            p.flag->setCollapsed(true);
            QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
            drag(p.flag, QPoint(17, 10), {30});
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QVERIFY(p.flag->isCollapsed());
            QCOMPARE(tuned.count(), 1);
            QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 30 * kHzPerPx);
        }
        {
            // The folded TX badge stays a click on press.
            Pan p;
            p.flag->setCollapsed(true);
            QSignalSpy handoff(p.flag, &VfoWidget::txHandoffRequested);
            QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
            drag(p.flag, p.flag->collapsedTxBadgeRect().center(), {30});
            QCOMPARE(handoff.count(), 1);
            QCOMPARE(tuned.count(), 0);
        }
    }

    void locked_or_listened_slice_does_not_move()
    {
        {
            Pan p;
            p.flag->setLocked(true);
            QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
            drag(p.flag, p.frequencyPoint(), {30});
            QCOMPARE(tuned.count(), 0);
        }
        {
            Pan p;
            VfoWidget::SliceAccess access;
            access.state = VfoWidget::SliceAccess::State::Listening;
            access.line = QStringLiteral("Listening");
            p.flag->setSliceAccess(access);
            QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
            drag(p.flag, p.frequencyPoint(), {30});
            QCOMPARE(tuned.count(), 0);
        }
    }

    void press_on_a_button_never_drags()
    {
        Pan p;
        QPushButton* button = nullptr;
        for (QPushButton* b : p.flag->findChildren<QPushButton*>()) {
            if (b->isVisible() && b->isEnabled() && b->window() == p.flag->window()
                && p.flag->isAncestorOf(b) && b->width() > 4) {
                button = b;
                break;
            }
        }
        QVERIFY(button);
        const QPoint at = button->mapTo(p.flag, button->rect().center());
        QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
        QSignalSpy started(p.flag, &VfoWidget::flagDragStarted);
        drag(p.flag, at, {15, 30});
        QCOMPARE(started.count(), 0);
        QCOMPARE(tuned.count(), 0);
    }

    // A press on the flag still selects its slice; a drag selects it once,
    // on the press, not again on each move or on the release.
    void press_selects_the_slice_once()
    {
        {
            Pan p;
            QSignalSpy select(p.flag, &VfoWidget::sliceActivationRequested);
            drag(p.flag, p.frequencyPoint(), {});
            QCOMPARE(select.count(), 1);
            QCOMPARE(select.first().first().toInt(), 1);
        }
        {
            Pan p;
            QSignalSpy select(p.flag, &VfoWidget::sliceActivationRequested);
            sendAt(p.flag, QEvent::MouseButtonPress, p.frequencyPoint(), 0, Qt::LeftButton,
                   Qt::LeftButton);
            QCOMPARE(select.count(), 1);
            sendAt(p.flag, QEvent::MouseMove, p.frequencyPoint(), 15, Qt::NoButton,
                   Qt::LeftButton);
            sendAt(p.flag, QEvent::MouseMove, p.frequencyPoint(), 30, Qt::NoButton,
                   Qt::LeftButton);
            sendAt(p.flag, QEvent::MouseButtonRelease, p.frequencyPoint(), 30, Qt::LeftButton,
                   Qt::NoButton);
            QCOMPARE(select.count(), 1);
        }
    }

    // The drag cursor is the flag's while it drags (the pointer is over the
    // flag for the whole grab) and is unset when the drag ends; the
    // spectrum's own cursor is left alone.
    void drag_cursor_is_on_the_flag()
    {
        Pan p;
        const Qt::CursorShape panCursor = p.sw.cursor().shape();
        QVERIFY(!p.flag->testAttribute(Qt::WA_SetCursor));
        sendAt(p.flag, QEvent::MouseButtonPress, p.frequencyPoint(), 0, Qt::LeftButton,
               Qt::LeftButton);
        sendAt(p.flag, QEvent::MouseMove, p.frequencyPoint(), 30, Qt::NoButton,
               Qt::LeftButton);
        QVERIFY(p.flag->isFlagDragging());
        QVERIFY(p.flag->testAttribute(Qt::WA_SetCursor));
        QCOMPARE(p.flag->cursor().shape(), Qt::SizeHorCursor);
        sendAt(p.flag, QEvent::MouseButtonRelease, p.frequencyPoint(), 30, Qt::LeftButton,
               Qt::NoButton);
        QVERIFY(!p.flag->testAttribute(Qt::WA_SetCursor));
        QCOMPARE(p.sw.cursor().shape(), panCursor);
    }

    // A flag hidden mid-drag (CTUN on, dragged past the span edge) never
    // sees its release; the drag ends when it hides, and a later move with
    // the button still down does not tune.
    void hiding_mid_drag_ends_the_drag()
    {
        Pan p;
        QSignalSpy finished(p.flag, &VfoWidget::flagDragFinished);
        QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
        sendAt(p.flag, QEvent::MouseButtonPress, p.frequencyPoint(), 0, Qt::LeftButton,
               Qt::LeftButton);
        sendAt(p.flag, QEvent::MouseMove, p.frequencyPoint(), 30, Qt::NoButton,
               Qt::LeftButton);
        QVERIFY(p.flag->isFlagDragging());
        QCOMPARE(tuned.count(), 1);
        p.flag->hide();
        QVERIFY(!p.flag->isFlagDragging());
        QCOMPARE(finished.count(), 1);
        QVERIFY(!p.flag->testAttribute(Qt::WA_SetCursor));
        // A stray move with the button still down tunes nothing.
        QMouseEvent stray(QEvent::MouseMove, QPointF(5, 5), QPointF(p.flag->mapToGlobal(QPoint(60, 5))),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(p.flag, &stray);
        QCOMPARE(tuned.count(), 1);
        QVERIFY(!p.flag->isFlagDragging());
    }

    // A press on an RIT/XIT or offset value selects the slice, as any press
    // on the flag does, but a drag from it never drags the flag.
    void press_on_a_scrollable_label_selects_but_never_drags()
    {
        Pan p;
        // The RIT/XIT tab, so its two values are on screen to be pressed.
        p.flag->showTab(VfoWidget::Tab::XRit);
        QCoreApplication::processEvents();
        QList<ScrollableLabel*> labels;
        for (ScrollableLabel* label : p.flag->findChildren<ScrollableLabel*>()) {
            if (label->isVisibleTo(p.flag)) {
                labels.append(label);
            }
        }
        QVERIFY(labels.size() >= 2);
        QSignalSpy started(p.flag, &VfoWidget::flagDragStarted);
        QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
        QSignalSpy select(p.flag, &VfoWidget::sliceActivationRequested);
        for (ScrollableLabel* label : labels) {
            // Sent to the value's own text, the child a click lands on.
            QWidget* target = label->currentWidget();
            QVERIFY(target);
            const QPoint at = target->rect().center();
            const auto send = [target, at](QEvent::Type type, int dx, Qt::MouseButton b,
                                           Qt::MouseButtons bs) {
                QMouseEvent me(type, QPointF(at + QPoint(dx, 0)),
                               QPointF(target->mapToGlobal(at + QPoint(dx, 0))), b, bs,
                               Qt::NoModifier);
                QApplication::sendEvent(target, &me);
            };
            select.clear();
            send(QEvent::MouseButtonPress, 0, Qt::LeftButton, Qt::LeftButton);
            QCOMPARE(select.count(), 1);
            send(QEvent::MouseMove, 15, Qt::NoButton, Qt::LeftButton);
            send(QEvent::MouseMove, 30, Qt::NoButton, Qt::LeftButton);
            send(QEvent::MouseButtonRelease, 30, Qt::LeftButton, Qt::NoButton);
            QCOMPARE(select.count(), 1);
        }
        QCOMPARE(started.count(), 0);
        QCOMPARE(tuned.count(), 0);
    }

    void transmit_overlay_withholds_a_flag_drag()
    {
        Pan p;
        p.sw.setMoxOverlay(true);
        QSignalSpy tuned(p.flag, &VfoWidget::frequencyChanged);
        drag(p.flag, p.frequencyPoint(), {30});
        QCOMPARE(tuned.count(), 0);
    }
};

QTEST_MAIN(TestVfoFlagDrag)
#include "tst_vfo_flag_drag.moc"
