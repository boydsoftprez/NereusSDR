// =================================================================
// tests/tst_vfo_widget_slice_access.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Slice control and shared listening plan, Task 14a: a slice flag says
// who controls the slice. A flag this window listens to (another device
// controls the slice) keeps its letter and color, says who controls it,
// disables the shared tuning controls with that reason, never writes the
// slice, and offers Take control and Stop listening. A flag this window
// controls says "You control" and offers Release.
//
// Task 14b (ruling U5): a listened flag's AF slider and Mute return as
// "Your volume", bound to this device's own listening level. They never
// write the slice, and the slice's AF does not move them.
//
// Core-slice take-over (2026-09-30, J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code): Take control stays, disabled with the Core's
// words, when the Core refuses the take of its own slice.
//
// TX badge take (2026-09-30, J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code): a TX badge that offers a take is enabled, says what a
// click will do, and a click asks for the take (txTakeRequested), never
// the move; the radio's own transmission and a held reason still hold it.
// =================================================================
#include <QtTest/QtTest>
#include <QAction>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QWheelEvent>
#include "core/AppSettings.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

VfoWidget::SliceAccess listened()
{
    VfoWidget::SliceAccess access;
    access.state = VfoWidget::SliceAccess::State::Listening;
    access.line = QStringLiteral("Listening · controlled by Shack iPad");
    access.heldReason = QStringLiteral("Shack iPad controls this slice");
    return access;
}

VfoWidget::SliceAccess controlled()
{
    VfoWidget::SliceAccess access;
    access.state = VfoWidget::SliceAccess::State::Controlled;
    access.line = QStringLiteral("You control");
    return access;
}

QAction* findAction(QMenu& menu, const QString& text)
{
    for (QAction* action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

// The same three writes MainWindow wires from a flag to its slice.
void wireLikeMainWindow(VfoWidget& flag, SliceModel& slice)
{
    QObject::connect(&flag, &VfoWidget::frequencyChanged,
                     &slice, &SliceModel::setFrequency);
    QObject::connect(&flag, &VfoWidget::afGainChanged,
                     &slice, &SliceModel::setAfGain);
    QObject::connect(&flag, &VfoWidget::muteChanged,
                     &slice, &SliceModel::setMuted);
}

void wheel(QWidget& target)
{
    const QPointF pos(target.width() / 2.0, target.height() / 2.0);
    QWheelEvent event(pos, target.mapToGlobal(pos), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&target, &event);
}

} // namespace

class TestVfoWidgetSliceAccess : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    void unshared_flag_shows_no_access_line()
    {
        VfoWidget flag;
        QCOMPARE(flag.sliceAccess().state, VfoWidget::SliceAccess::State::Unshared);
        QVERIFY(flag.accessLineText().isEmpty());
        QVERIFY(!flag.isListening());
    }

    void listened_flag_names_the_controller_and_disables_tuning()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setSliceAccess(listened());

        QCOMPARE(flag.accessLineText(),
                 QStringLiteral("Listening · controlled by Shack iPad"));
        QVERIFY(flag.isListening());

        const QString reason = QStringLiteral("Shack iPad controls this slice");
        const QStringList held = {QStringLiteral("m_rxAntBtn")};
        for (const QString& name : held) {
            auto* control = flag.findChild<QWidget*>(name);
            QVERIFY2(control, qPrintable(name));
            QVERIFY2(!control->isEnabled(), qPrintable(name));
            QCOMPARE(control->toolTip(), reason);
        }
        const QList<QWidget*> tuning = flag.heldControlsForTest();
        QVERIFY(!tuning.isEmpty());
        for (QWidget* control : tuning) {
            QVERIFY(!control->isEnabled());
            QCOMPARE(control->toolTip(), reason);
            QCOMPARE(control->accessibleDescription(), reason);
        }
    }

    void access_change_restores_the_controls()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        auto* ant = flag.findChild<QWidget*>(QStringLiteral("m_rxAntBtn"));
        QVERIFY(ant);
        const QString ownTip = ant->toolTip();

        flag.setSliceAccess(listened());
        QVERIFY(!ant->isEnabled());
        flag.setSliceAccess(controlled());
        QVERIFY(ant->isEnabled());
        QCOMPARE(ant->toolTip(), ownTip);
        QCOMPARE(flag.accessLineText(), QStringLiteral("You control"));
        for (QWidget* control : flag.heldControlsForTest()) {
            QVERIFY(control->isEnabled());
        }
    }

    void listened_flag_never_writes_the_slice()
    {
        SliceModel slice(1);
        slice.setFrequency(14'200'000.0);
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setFrequency(14'200'000.0);
        wireLikeMainWindow(flag, slice);
        flag.setSliceAccess(listened());

        QSignalSpy freq(&slice, &SliceModel::frequencyChanged);
        QSignalSpy af(&slice, &SliceModel::afGainChanged);
        QSignalSpy mute(&slice, &SliceModel::mutedChanged);
        QSignalSpy flagFreq(&flag, &VfoWidget::frequencyChanged);

        wheel(flag);
        QTest::mouseDClick(&flag, Qt::LeftButton, Qt::NoModifier,
                           flag.frequencyAreaForTest().center());

        QCOMPARE(flagFreq.count(), 0);
        QCOMPARE(freq.count(), 0);
        QCOMPARE(af.count(), 0);
        QCOMPARE(mute.count(), 0);
        QVERIFY(!flag.frequencyEditOpen());
        QCOMPARE(slice.frequency(), 14'200'000.0);
    }

    void controlled_flag_still_tunes()
    {
        SliceModel slice(1);
        slice.setFrequency(14'200'000.0);
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setFrequency(14'200'000.0);
        wireLikeMainWindow(flag, slice);
        flag.setSliceAccess(controlled());

        QSignalSpy freq(&slice, &SliceModel::frequencyChanged);
        wheel(flag);
        QCOMPARE(freq.count(), 1);
    }

    // Core-slice take-over: a Core below sliceAccessVersion 3 refuses the
    // take of its own slice, so Take control stays, disabled with its words.
    void listened_menu_take_control_is_disabled_with_the_cores_words()
    {
        VfoWidget flag;
        flag.setSliceIndex(0);
        VfoWidget::SliceAccess access = listened();
        access.takeHeldReason = QStringLiteral("Slice A is run by the Core itself, so control of it cannot pass to this device.");
        flag.setSliceAccess(access);

        QMenu menu;
        flag.populateContextMenu(menu);
        QAction* take = findAction(menu, QStringLiteral("Take control"));
        QVERIFY(take);
        QVERIFY(!take->isEnabled());
        QCOMPARE(take->toolTip(), access.takeHeldReason);
        QAction* stop = findAction(menu, QStringLiteral("Stop listening"));
        QVERIFY(stop && stop->isEnabled());
        QSignalSpy taken(&flag, &VfoWidget::takeControlRequested);
        take->trigger();
        QCOMPARE(taken.count(), 0);
    }

    void listened_menu_offers_take_control_and_stop_listening()
    {
        VfoWidget flag;
        flag.setSliceIndex(2);
        flag.setSliceAccess(listened());

        QMenu menu;
        flag.populateContextMenu(menu);
        const QList<QAction*> actions = menu.actions();
        QVERIFY(actions.size() >= 2);
        QCOMPARE(actions.at(0)->text(), QStringLiteral("Take control"));
        QCOMPARE(actions.at(1)->text(), QStringLiteral("Stop listening"));
        QVERIFY(actions.at(0)->isEnabled());
        QVERIFY(actions.at(1)->isEnabled());
        QVERIFY(!findAction(menu, QStringLiteral("Release")));

        // Everything else on a listened flag changes the shared slice, so
        // it is shown disabled with the reason.
        for (QAction* action : actions.mid(2)) {
            if (action->isSeparator()) {
                continue;
            }
            QVERIFY2(!action->isEnabled(), qPrintable(action->text()));
            QCOMPARE(action->toolTip(), QStringLiteral("Shack iPad controls this slice"));
        }

        QSignalSpy take(&flag, &VfoWidget::takeControlRequested);
        QSignalSpy stop(&flag, &VfoWidget::stopListeningRequested);
        actions.at(0)->trigger();
        actions.at(1)->trigger();
        QCOMPARE(take.count(), 1);
        QCOMPARE(take.first().first().toInt(), 2);
        QCOMPARE(stop.count(), 1);
        QCOMPARE(stop.first().first().toInt(), 2);
    }

    void controlled_menu_offers_release()
    {
        VfoWidget flag;
        flag.setSliceIndex(3);
        flag.setSliceAccess(controlled());

        QMenu menu;
        flag.populateContextMenu(menu);
        QAction* release = menu.actions().value(0);
        QVERIFY(release);
        QCOMPARE(release->text(), QStringLiteral("Release"));
        QVERIFY(!findAction(menu, QStringLiteral("Take control")));

        QSignalSpy spy(&flag, &VfoWidget::releaseRequested);
        release->trigger();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 3);
    }

    void unshared_menu_has_no_access_actions()
    {
        VfoWidget flag;
        QMenu menu;
        flag.populateContextMenu(menu);
        QVERIFY(!findAction(menu, QStringLiteral("Take control")));
        QVERIFY(!findAction(menu, QStringLiteral("Release")));
        QVERIFY(!findAction(menu, QStringLiteral("Stop listening")));
        QCOMPARE(menu.actions().value(0)->text(),
                 QStringLiteral("Make this the TX slice"));
    }

    void pending_request_shows_and_disables_access_actions()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setSliceAccess(listened());
        flag.setSliceAccessPending(QStringLiteral("Asking the Core..."));
        QCOMPARE(flag.accessLineText(), QStringLiteral("Asking the Core..."));

        QMenu menu;
        flag.populateContextMenu(menu);
        QVERIFY(!menu.actions().at(0)->isEnabled());
        QVERIFY(!menu.actions().at(1)->isEnabled());

        flag.setSliceAccessPending(QString());
        QCOMPARE(flag.accessLineText(),
                 QStringLiteral("Listening · controlled by Shack iPad"));
    }

    void close_on_a_listened_flag_stops_listening()
    {
        QWidget pan;
        pan.resize(800, 400);
        auto* flag = new VfoWidget(&pan);
        flag->setSliceIndex(0);  // even Slice A: stopping listening is fine
        flag->setSliceAccess(listened());
        flag->updatePosition(400, 20);
        QPushButton* close = flag->closeButtonForTest();
        QVERIFY(close);
        QCOMPARE(close->toolTip(), QStringLiteral("Stop listening"));

        QSignalSpy stop(flag, &VfoWidget::stopListeningRequested);
        QSignalSpy closeReq(flag, &VfoWidget::closeRequested);
        close->click();
        QCOMPARE(stop.count(), 1);
        QCOMPARE(stop.first().first().toInt(), 0);
        QCOMPARE(closeReq.count(), 0);
    }

    void tx_badge_stays_red_on_a_listened_flag_on_the_air()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setSliceAccess(listened());
        flag.setTxSlice(true);
        QVERIFY(flag.txSliceShown());

        QSignalSpy spy(&flag, &VfoWidget::txHandoffRequested);
        flag.simulateTxBadgeClick();
        QCOMPARE(spy.count(), 0);
    }
    void listened_flag_offers_your_volume_and_mute()
    {
        SliceModel slice(1);
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setAfGain(80);
        wireLikeMainWindow(flag, slice);
        flag.setSliceAccess(listened());
        flag.setListenVolume(30, false);

        QSlider* volume = flag.afSliderForTest();
        QPushButton* mute = flag.muteButtonForTest();
        QVERIFY(volume && mute);
        QVERIFY(volume->isEnabled());
        QVERIFY(mute->isEnabled());
        QVERIFY(!flag.heldControlsForTest().contains(volume));
        QVERIFY(!flag.heldControlsForTest().contains(mute));
        QCOMPARE(flag.afNameForTest(), QStringLiteral("Your volume"));
        QCOMPARE(volume->value(), 30);

        QSignalSpy sliceAf(&slice, &SliceModel::afGainChanged);
        QSignalSpy sliceMute(&slice, &SliceModel::mutedChanged);
        QSignalSpy flagAf(&flag, &VfoWidget::afGainChanged);
        QSignalSpy flagMute(&flag, &VfoWidget::muteChanged);
        QSignalSpy mine(&flag, &VfoWidget::listenVolumeRequested);

        volume->setValue(55);
        mute->setChecked(true);

        QCOMPARE(mine.count(), 2);
        QCOMPARE(mine.at(0).at(0).toInt(), 1);
        QCOMPARE(mine.at(0).at(1).toInt(), 55);
        QCOMPARE(mine.at(0).at(2).toBool(), false);
        QCOMPARE(mine.at(1).at(1).toInt(), 55);
        QCOMPARE(mine.at(1).at(2).toBool(), true);
        QCOMPARE(flagAf.count(), 0);
        QCOMPARE(flagMute.count(), 0);
        QCOMPARE(sliceAf.count(), 0);
        QCOMPARE(sliceMute.count(), 0);
        QCOMPARE(flag.listenVolume(), 55);
        QVERIFY(flag.listenMuted());
    }

    void slice_af_does_not_move_your_volume()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setSliceAccess(listened());
        flag.setListenVolume(40, false);

        // The controller turns the slice's AF to zero and mutes it.
        flag.setAfGain(0);
        flag.setMuted(true);
        QCOMPARE(flag.afSliderForTest()->value(), 40);
        QVERIFY(!flag.muteButtonForTest()->isChecked());

        // Back to controlling: the slice's own AF and mute return.
        flag.setSliceAccess(controlled());
        QCOMPARE(flag.afNameForTest(), QStringLiteral("AF"));
        QCOMPARE(flag.afSliderForTest()->value(), 0);
        QVERIFY(flag.muteButtonForTest()->isChecked());
    }

    void listened_flag_holds_the_rest_of_the_audio_page()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setSliceAccess(listened());
        const QList<QWidget*> held = flag.heldControlsForTest();
        // Pan, AGC, squelch and the other tabs stay held.
        int heldCount = 0;
        for (QWidget* control : held) {
            if (!control->isEnabled()) { ++heldCount; }
        }
        QCOMPARE(heldCount, held.size());
        QVERIFY(held.size() > 6);
    }

    void controlled_flag_af_still_writes_the_slice()
    {
        SliceModel slice(1);
        VfoWidget flag;
        flag.setSliceIndex(1);
        wireLikeMainWindow(flag, slice);
        flag.setSliceAccess(controlled());
        QSignalSpy mine(&flag, &VfoWidget::listenVolumeRequested);
        flag.afSliderForTest()->setValue(70);
        QCOMPARE(slice.afGain(), 70);
        QCOMPARE(mine.count(), 0);
        QCOMPARE(flag.afNameForTest(), QStringLiteral("AF"));
    }

    // Case 2: this window's slice while another device holds transmit.
    void tx_badge_offering_a_take_is_enabled_and_asks_for_the_take()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        flag.setSliceAccess(controlled());
        const QString notHolder = QStringLiteral("Another device holds transmit.");
        flag.setTransmitPermitted(false, notHolder);
        auto* badge = flag.findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        QVERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), notHolder);

        VfoWidget::TxBadgeOffer offer;
        offer.offered = true;
        offer.toolTip = QStringLiteral("Take transmit from iPhone and make this the TX slice");
        flag.setTxBadgeOffer(offer);
        QVERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(), offer.toolTip);
        QCOMPARE(badge->accessibleDescription(), offer.toolTip);

        QSignalSpy take(&flag, &VfoWidget::txTakeRequested);
        QSignalSpy move(&flag, &VfoWidget::txHandoffRequested);
        badge->click();
        QCOMPARE(take.count(), 1);
        QCOMPARE(take.first().at(0).toInt(), 1);
        QCOMPARE(move.count(), 0);
        // The TX mark follows the Core, not the click.
        QVERIFY(!badge->isChecked());

        // The radio's own transmission on this frequency: held, its words.
        flag.setInUseByRadio(true);
        QVERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), VfoWidget::inUseByRadioText());
        badge->click();
        QCOMPARE(take.count(), 1);
        flag.setInUseByRadio(false);
        QVERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(), offer.toolTip);

        // Transmit is here again: the offer is moot, the badge moves
        // transmit as before and says what it always said.
        flag.setTransmitPermitted(true, QString());
        QVERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(), QStringLiteral("Indicates this slice is the TX slice"));
        flag.simulateTxBadgeClick();
        QCOMPARE(move.count(), 1);
        QCOMPARE(take.count(), 1);

        // The offer withdrawn: held with today's reason.
        flag.setTransmitPermitted(false, notHolder);
        flag.setTxBadgeOffer({});
        QVERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), notHolder);
    }

    // Case 3: a listened slice. The offer replaces the hold; a slice
    // request waiting, or the slice on the air, holds it with those words.
    void tx_badge_on_a_listened_flag_offers_the_slice_take()
    {
        VfoWidget flag;
        flag.setSliceIndex(2);
        flag.setSliceAccess(listened());
        auto* badge = flag.findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        QVERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), QStringLiteral("Shack iPad controls this slice"));

        VfoWidget::TxBadgeOffer offer;
        offer.offered = true;
        offer.toolTip = QStringLiteral("Take control of this slice and make it the TX slice");
        flag.setTxBadgeOffer(offer);
        QVERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(), offer.toolTip);
        QSignalSpy take(&flag, &VfoWidget::txTakeRequested);
        flag.simulateTxBadgeClick();
        QCOMPARE(take.count(), 1);
        QCOMPARE(take.first().at(0).toInt(), 2);

        // A slice request waiting: held with what it waits for.
        flag.setSliceAccessPending(QStringLiteral("Asking the Core…"));
        QVERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), QStringLiteral("Asking the Core…"));
        flag.simulateTxBadgeClick();
        QCOMPARE(take.count(), 1);
        flag.setSliceAccessPending(QString());
        QVERIFY(badge->isEnabled());

        // On the air: held with the Core's on-air words, the red mark kept.
        VfoWidget::TxBadgeOffer onAir;
        onAir.heldReason = QStringLiteral("Slice C is transmitting. Take control once it stops.");
        flag.setTxBadgeOffer(onAir);
        flag.setTxSlice(true);
        QVERIFY(!badge->isEnabled());
        QVERIFY(badge->isChecked());
        QCOMPARE(badge->toolTip(), onAir.heldReason);

        // Control came here: the badge is this window's own again.
        flag.setTxSlice(false);
        flag.setTxBadgeOffer({});
        flag.setSliceAccess(controlled());
        QVERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(), QStringLiteral("Indicates this slice is the TX slice"));
    }
};

QTEST_MAIN(TestVfoWidgetSliceAccess)
#include "tst_vfo_widget_slice_access.moc"
