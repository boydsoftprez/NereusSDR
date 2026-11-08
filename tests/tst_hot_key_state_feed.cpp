// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-07 - J.J. Boyd (KG4VCF), hot keys: VOX on or off (vox.toggle).
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// What lights and displays read: on and off, and the window's slice. First against
// models alone, then wired to a real offscreen window.
#include <QtTest>

#include <QApplication>
#include <QPushButton>
#include <QPointer>
#include <QSignalSpy>
#include <QTimer>

#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/WdspTypes.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/MainWindow.h"
#include "gui/applets/TxApplet.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyStateFeed.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class FakeSource : public HotKeyStateSource {
public:
    SliceModel* slice{nullptr};
    QHash<QString, bool> transmit;
    bool listenMuted{false};
    bool listened{false};
    bool radio{false};
    bool onAir{false};

    SliceModel* windowSlice() const override { return slice; }
    bool transmitOn(const QString& id) const override { return transmit.value(id, false); }
    bool muted(const SliceModel* s) const override { return listened ? listenMuted : s->muted(); }
    bool radioConnected() const override { return radio; }
    bool radioTransmitting() const override { return onAir; }
};

// One window, connected, with two slices. The cases share it: building one takes seconds.
struct Bench {
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    HotKeyStateFeed* feed{window.hotKeyStateFeed()};
    SliceModel* a{nullptr};
    SliceModel* b{nullptr};

    Bench()
    {
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->transmitModel().setMicSourceLocked(false);
        model->transmitModel().setMicSource(MicSource::Radio);
        a = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
        b = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
    }
};

} // namespace

class TestHotKeyStateFeed : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<FakeSource> m_source;
    std::unique_ptr<SliceModel> m_a;
    std::unique_ptr<SliceModel> m_b;
    std::unique_ptr<HotKeyStateFeed> m_feed;
    std::unique_ptr<Bench> m_bench;

    FakeSource& source() { return *m_source; }
    HotKeyStateFeed& feed() { return *m_feed; }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_source = std::make_unique<FakeSource>();
        m_a = std::make_unique<SliceModel>(0);
        m_b = std::make_unique<SliceModel>(1);
        m_a->setFrequency(14'250'000.0);
        m_a->setDspMode(DSPMode::USB);
        m_a->setFilter(100, 2800);
        m_b->setFrequency(7'074'001.0);
        m_b->setDspMode(DSPMode::LSB);
        m_b->setFilter(-500, -100);
        source().slice = m_a.get();
        m_feed = std::make_unique<HotKeyStateFeed>(m_source.get());
    }

    void cleanup()
    {
        m_feed.reset();
        m_a.reset();
        m_b.reset();
        m_source.reset();
    }

    void cleanupTestCase()
    {
        m_bench.reset();
    }

    void whetherTheRadioIsTransmittingIsPartOfTheDisplay()
    {
        QSignalSpy changed(&feed(), &HotKeyStateFeed::displayChanged);
        QCOMPARE(feed().display().transmitting, false);
        source().onAir = true;
        feed().refresh();
        QCOMPARE(feed().display().transmitting, true);
        QCOMPARE(changed.size(), 1);
        source().onAir = false;
        feed().refresh();
        QCOMPARE(feed().display().transmitting, false);
        QCOMPARE(changed.size(), 2);
    }

    void theRadioConnectionIsPartOfTheDisplay()
    {
        QSignalSpy changed(&feed(), &HotKeyStateFeed::displayChanged);
        QCOMPARE(feed().display().connected, false);
        source().radio = true;
        feed().refresh();
        QCOMPARE(feed().display().connected, true);
        QCOMPARE(changed.size(), 1);
    }

    void everyActionFromTheBrief()
    {
        const QStringList ids = HotKeyStateFeed::actionIds();
        for (const char* id : {"tx.mox", "tx.tune", "tx.twotone", "tx.hold", "vox.toggle", "mute", "slice.lock",
                               "rit.toggle", "xit.toggle", "nr.toggle", "nb.toggle", "anf.toggle"}) {
            QVERIFY2(ids.contains(QLatin1String(id)), id);
        }
    }

    void sliceStatesFollowTheModel()
    {
        QSignalSpy changed(&feed(), &HotKeyStateFeed::stateChanged);
        const auto expectOnly = [&](const char* id, bool on) {
            QCOMPARE(changed.size(), 1);
            QCOMPARE(changed.at(0).at(0).toString(), QString::fromLatin1(id));
            QCOMPARE(changed.at(0).at(1).toBool(), on);
            QCOMPARE(feed().isOn(QString::fromLatin1(id)), on);
            changed.clear();
        };
        m_a->setMuted(true);
        expectOnly("mute", true);
        m_a->setLocked(true);
        expectOnly("slice.lock", true);
        m_a->setRitEnabled(true);
        expectOnly("rit.toggle", true);
        m_a->setXitEnabled(true);
        expectOnly("xit.toggle", true);
        m_a->setAnfEnabled(true);
        expectOnly("anf.toggle", true);
        m_a->setActiveNr(NrSlot::NR2);
        expectOnly("nr.toggle", true);
        m_a->setNbMode(NbMode::NB2);
        expectOnly("nb.toggle", true);
        m_a->setActiveNr(NrSlot::Off);
        expectOnly("nr.toggle", false);
        m_a->setMuted(false);
        expectOnly("mute", false);
    }

    void aChangeOnAnotherSliceChangesNothing()
    {
        QSignalSpy changed(&feed(), &HotKeyStateFeed::stateChanged);
        QSignalSpy display(&feed(), &HotKeyStateFeed::displayChanged);
        m_b->setMuted(true);
        m_b->setFrequency(7'000'000.0);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(display.size(), 0);
    }

    void changingSliceUpdatesEverythingThatDiffers()
    {
        m_a->setMuted(true);
        m_b->setLocked(true);
        QSignalSpy changed(&feed(), &HotKeyStateFeed::stateChanged);
        QSignalSpy display(&feed(), &HotKeyStateFeed::displayChanged);
        source().slice = m_b.get();
        feed().refresh();
        QVERIFY(!feed().isOn(QStringLiteral("mute")));
        QVERIFY(feed().isOn(QStringLiteral("slice.lock")));
        QCOMPARE(changed.size(), 2);
        QCOMPARE(display.size(), 1);
        QCOMPARE(feed().display().slice, QLatin1Char('B'));
        // The old slice no longer reaches the feed, and the new one does.
        changed.clear();
        m_a->setRitEnabled(true);
        QCOMPARE(changed.size(), 0);
        m_b->setRitEnabled(true);
        QCOMPARE(changed.size(), 1);
    }

    void transmitStatesComeFromTheSource()
    {
        QSignalSpy changed(&feed(), &HotKeyStateFeed::stateChanged);
        for (const char* id : {"tx.mox", "tx.tune", "tx.twotone", "tx.hold", "vox.toggle"}) {
            changed.clear();
            source().transmit.insert(QString::fromLatin1(id), true);
            feed().refresh();
            QCOMPARE(changed.size(), 1);
            QCOMPARE(changed.at(0).at(0).toString(), QString::fromLatin1(id));
            QVERIFY(feed().isOn(QString::fromLatin1(id)));
            feed().refresh();
            QCOMPARE(changed.size(), 1);    // nothing new, nothing said
        }
    }

    void aListenedSliceHasThisDevicesOwnMute()
    {
        source().listened = true;
        source().listenMuted = true;
        m_a->setMuted(false);
        feed().refresh();
        QVERIFY(feed().isOn(QStringLiteral("mute")));
    }

    void displayHasHzModeWidthAndLetter()
    {
        const HotKeyStateFeed::Display first = feed().display();
        QCOMPARE(first.frequency, QStringLiteral("14.250.000"));
        QCOMPARE(first.mode, QStringLiteral("USB"));
        QCOMPARE(first.filter, QStringLiteral("2.7K"));
        QCOMPARE(first.slice, QLatin1Char('A'));

        QSignalSpy display(&feed(), &HotKeyStateFeed::displayChanged);
        m_a->setFrequency(14'074'321.0);
        QCOMPARE(display.size(), 1);
        QCOMPARE(feed().display().frequency, QStringLiteral("14.074.321"));
        m_a->setDspMode(DSPMode::CWU);
        QCOMPARE(feed().display().mode, QStringLiteral("CWU"));
        m_a->setFilter(100, 600);
        QCOMPARE(feed().display().filter, QStringLiteral("500"));
    }

    void aDigitWithLeadingZerosKeepsThem()
    {
        m_a->setFrequency(7'004'001.0);
        QCOMPARE(feed().display().frequency, QStringLiteral("7.004.001"));
    }

    void anUnrelatedStateChangeSaysNothingAboutTheDisplay()
    {
        QSignalSpy display(&feed(), &HotKeyStateFeed::displayChanged);
        m_a->setMuted(true);
        QCOMPARE(display.size(), 0);
    }

    void noSliceIsOffAndEmpty()
    {
        m_a->setMuted(true);
        source().slice = nullptr;
        feed().refresh();
        QVERIFY(!feed().isOn(QStringLiteral("mute")));
        QVERIFY(feed().display().frequency.isEmpty());
        QVERIFY(feed().display().slice.isNull());
    }

    void aSliceGoingAwayIsOff()
    {
        m_a->setMuted(true);
        QVERIFY(feed().isOn(QStringLiteral("mute")));
        source().slice = nullptr;
        m_a.reset();
        QCoreApplication::sendPostedEvents(&feed(), QEvent::MetaCall);
        QVERIFY(!feed().isOn(QStringLiteral("mute")));
        QVERIFY(feed().display().frequency.isEmpty());
    }

    void itNeverPolls()
    {
        QCOMPARE(feed().findChildren<QTimer*>().size(), 0);
    }

    // ---- wired to a real window ----

    void windowHasAFeed()
    {
        m_bench = std::make_unique<Bench>();
        QVERIFY(m_bench->feed != nullptr);
        QVERIFY(m_bench->a != nullptr);
        QVERIFY(m_bench->b != nullptr);
    }

    void windowFeedsTheDisplaysAndKnowsTheRadioIsConnected()
    {
        Bench& w = *m_bench;
        QCOMPARE(w.window.controllerHost()->stateFeed(), w.feed);
        w.feed->refresh();
        QCOMPARE(w.feed->display().connected, true);
    }

    void windowFollowsTheSelectedSlice()
    {
        Bench& w = *m_bench;
        QVERIFY(w.model->setActiveSliceById(w.a->sliceIndex()));
        QCOMPARE(w.feed->display().slice, w.a->sliceLetter());
        w.a->setMuted(false);
        w.b->setMuted(true);
        QVERIFY(!w.feed->isOn(QStringLiteral("mute")));

        QVERIFY(w.model->setActiveSliceById(w.b->sliceIndex()));
        QCOMPARE(w.feed->display().slice, w.b->sliceLetter());
        QVERIFY(w.feed->isOn(QStringLiteral("mute")));

        // The slice the window isn't on no longer moves it.
        QSignalSpy changed(w.feed, &HotKeyStateFeed::stateChanged);
        w.a->setLocked(true);
        QCOMPARE(changed.size(), 0);
        w.a->setLocked(false);
        w.b->setMuted(false);
        QVERIFY(w.model->setActiveSliceById(w.a->sliceIndex()));
    }

    void windowTransmitStatesFollowTheButtons()
    {
        Bench& w = *m_bench;
        QVERIFY(!w.feed->isOn(QStringLiteral("tx.mox")));
        w.model->moxController()->setMox(true);
        QTRY_VERIFY(w.feed->isOn(QStringLiteral("tx.mox")));
        w.model->moxController()->setMox(false);
        QTRY_VERIFY(!w.feed->isOn(QStringLiteral("tx.mox")));

        w.model->transmitModel().setTune(true);
        QTRY_VERIFY(w.feed->isOn(QStringLiteral("tx.tune")));
        w.model->transmitModel().setTune(false);
        QTRY_VERIFY(!w.feed->isOn(QStringLiteral("tx.tune")));
    }

    // The TX applet's VOX button, clicked, is what a deck key lit for VOX shows.
    void windowVoxFollowsTheTxAppletsButton()
    {
        Bench& w = *m_bench;
        const QString vox = QStringLiteral("vox.toggle");
        TxApplet* const applet = w.window.findChild<TxApplet*>();
        QVERIFY(applet != nullptr && applet->voxButton() != nullptr);
        QPushButton* const button = applet->voxButton();
        QVERIFY(button->isEnabled());
        QVERIFY(!w.model->transmitModel().voxEnabled());
        QVERIFY(!w.feed->isOn(vox));

        QSignalSpy changed(w.feed, &HotKeyStateFeed::stateChanged);
        button->click();
        QVERIFY(w.model->transmitModel().voxEnabled());
        QVERIFY(w.feed->isOn(vox));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), vox);
        QCOMPARE(changed.at(0).at(1).toBool(), true);

        button->click();
        QVERIFY(!w.model->transmitModel().voxEnabled());
        QVERIFY(!w.feed->isOn(vox));
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.at(1).at(1).toBool(), false);
    }

    void windowReportsAHoldThatKeys()
    {
        Bench& w = *m_bench;
        const HotKeySource source{HotKeySourceKind::Knob, QStringLiteral("RC28")};
        QVERIFY(!w.feed->isOn(QStringLiteral("tx.hold")));
        w.window.hotKeyDispatcher()->press(QStringLiteral("tx.hold"), source);
        QVERIFY(w.window.hotKeyDispatcher()->holdsTransmit(source));
        QVERIFY(w.feed->isOn(QStringLiteral("tx.hold")));
        w.window.hotKeyDispatcher()->release(QStringLiteral("tx.hold"), source);
        QVERIFY(!w.feed->isOn(QStringLiteral("tx.hold")));
    }
};

QTEST_MAIN(TestHotKeyStateFeed)
#include "tst_hot_key_state_feed.moc"
