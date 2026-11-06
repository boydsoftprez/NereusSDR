// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// MIDI profiles, routing into the dispatcher, the unplug failsafe and the worker
// (R-HK-15, spec 10). Simulated from documented layouts through a fake device layer;
// no device has been tried.
#include <QtTest>

#include <QPair>
#include <QPointer>
#include <QSignalSpy>
#include <QThread>

#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "fakes/FakeMidiBackend.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/midi/MidiInput.h"
#include "gui/hotkeys/midi/MidiProfile.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QString kNote60 = QStringLiteral("ch1.note60");
const QString kNote61 = QStringLiteral("ch1.note61");
const QString kFader = QStringLiteral("ch1.cc7");
const QString kKnob = QStringLiteral("ch1.cc16");
const QString kBend = QStringLiteral("ch1.bend");
const QString kPad = QStringLiteral("Pad");
// A local copy: QCOMPARE binds a reference, and MinGW can't import an exported
// class's static constexpr by address (CONTRIBUTING, Windows DLL Boundaries).
constexpr int kPortCheckMs = MidiInput::kPortCheckMs;

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    bool transmittingNow{false};
    qint64 clock{1000};

    struct SliceCall {
        QString id;
        int steps;
    };

    QList<double> tunes;
    QList<SliceCall> sliceActions;
    QStringList transmitActions;
    QList<bool> keyHolds;
    QList<QPair<QString, bool>> cwKeys;

    SliceModel* windowSlice() const override { return window; }
    SliceModel* sliceByLetter(QChar) const override { return window; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel* slice, double hz) override
    {
        tunes.append(hz);
        slice->setFrequency(hz);
        return true;
    }
    void runSliceAction(const QString& id, SliceModel*, int steps) override { sliceActions.append({id, steps}); }
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return transmittingNow; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool down) override
    {
        keyHolds.append(down);
        transmittingNow = down;
        return true;
    }
    void runTransmitAction(const QString& id) override { transmitActions.append(id); }
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString& id, bool down) override { cwKeys.append({id, down}); }
    void toast(const QString&) override {}
    qint64 nowMs() const override { return clock; }

    int sumOf(const QString& id) const
    {
        int sum = 0;
        for (const SliceCall& call : sliceActions) {
            if (call.id == id) {
                sum += call.steps;
            }
        }
        return sum;
    }
};

MidiProfile profileWith(const QList<MidiBinding>& bindings)
{
    MidiProfile profile(QStringLiteral("Test"));
    for (const MidiBinding& binding : bindings) {
        const QString reason = profile.bind(binding);
        Q_ASSERT(reason.isEmpty());
    }
    return profile;
}

MidiBinding button(const QString& control, const QString& id)
{
    return MidiBinding{control, MidiControlKind::Button, id, QString(), QChar()};
}

MidiBinding absoluteKnob(const QString& control, const QString& up, const QString& down)
{
    return MidiBinding{control, MidiControlKind::AbsoluteKnob, up, down, QChar()};
}

MidiBinding relativeKnob(const QString& control, const QString& up, const QString& down)
{
    return MidiBinding{control, MidiControlKind::RelativeKnob, up, down, QChar()};
}

// Hands every queued message on, on this thread. No clock, no load.
void deliverQueued(MidiInput& input)
{
    QCoreApplication::sendPostedEvents(&input, QEvent::MetaCall);
}

} // namespace

class TestMidiProfile : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<FakeTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<SliceModel> m_slice;
    std::unique_ptr<FakeMidiDeviceLayer> m_layer;
    std::unique_ptr<ControllerHost> m_host;

    FakeTargets& t() { return *m_targets; }
    ControllerHost& host() { return *m_host; }

    // A MidiInput on this thread, wired to the host as the worker's is, driven by hand.
    void wire(MidiInput& input)
    {
        connect(&input, &MidiInput::controlEvent, m_host.get(), &ControllerHost::onMidiControlEvent);
        connect(&input, &MidiInput::portOpened, m_host.get(), &ControllerHost::onMidiPortOpened);
        connect(&input, &MidiInput::connectionChanged, m_host.get(), &ControllerHost::onMidiConnectionChanged);
    }

    // Moves the clock past the burst limit, so every turn is seen at once.
    void sendAndFlush(const QString& control, int value, int delta)
    {
        t().clock += 1000;
        host().onMidiControlEvent(control, MidiControlKind::AbsoluteKnob, value, delta);
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<FakeTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_slice = std::make_unique<SliceModel>(0);
        m_slice->setFrequency(14200000.0);
        m_slice->setStepHz(100);
        t().window = m_slice.get();
        m_layer = std::make_unique<FakeMidiDeviceLayer>();
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
    }

    void cleanup()
    {
        m_host.reset();
        m_layer.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_slice.reset();
        m_bindings.reset();
        AppSettings::instance().clear();
    }

    // ---- Profile ----

    void profile_roundTrips()
    {
        MidiProfile saved(QStringLiteral("Desk"));
        QVERIFY(saved.bind(button(kNote60, QStringLiteral("tx.hold"))).isEmpty());
        MidiBinding knob = relativeKnob(kKnob, QStringLiteral("tune.up"), QStringLiteral("tune.down"));
        knob.pin = QLatin1Char('B');
        QVERIFY(saved.bind(knob).isEmpty());
        QVERIFY(saved.bind(absoluteKnob(kFader, QStringLiteral("volume.up"), QStringLiteral("volume.down"))).isEmpty());
        saved.save();

        MidiProfile loaded(QStringLiteral("Desk"));
        loaded.load();
        QCOMPARE(loaded.bindings().size(), 3);
        QCOMPARE(loaded.bindings().at(0).controlKey, kNote60);
        QCOMPARE(loaded.bindings().at(0).kind, MidiControlKind::Button);
        QCOMPARE(loaded.bindings().at(0).actionId, QStringLiteral("tx.hold"));
        QVERIFY(loaded.bindings().at(0).pin.isNull());
        QCOMPARE(loaded.bindings().at(1).kind, MidiControlKind::RelativeKnob);
        QCOMPARE(loaded.bindings().at(1).actionIdDown, QStringLiteral("tune.down"));
        QCOMPARE(loaded.bindings().at(1).pin, QLatin1Char('B'));
        QCOMPARE(loaded.bindings().at(2).kind, MidiControlKind::AbsoluteKnob);
    }

    void profile_savesUnderItsKey()
    {
        MidiProfile saved(QStringLiteral("Desk"));
        saved.bind(button(kNote60, QStringLiteral("mute")));
        saved.save();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Midi/Profiles/Desk/Count")).toInt(), 1);
    }

    void profile_twoKeepSeparateLists()
    {
        MidiProfile one(QStringLiteral("One"));
        one.bind(button(kNote60, QStringLiteral("mute")));
        one.save();
        MidiProfile two(QStringLiteral("Two"));
        two.bind(button(kNote61, QStringLiteral("nr.toggle")));
        two.bind(button(kNote60, QStringLiteral("nb.toggle")));
        two.save();

        MidiProfile loadedOne(QStringLiteral("One"));
        loadedOne.load();
        MidiProfile loadedTwo(QStringLiteral("Two"));
        loadedTwo.load();
        QCOMPARE(loadedOne.bindings().size(), 1);
        QCOMPARE(loadedOne.bindings().at(0).actionId, QStringLiteral("mute"));
        QCOMPARE(loadedTwo.bindings().size(), 2);
        QCOMPARE(MidiProfile::savedNames(), QStringList({QStringLiteral("One"), QStringLiteral("Two")}));
    }

    void profile_savingAgainLeavesNoOldRows()
    {
        MidiProfile profile(QStringLiteral("Desk"));
        profile.bind(button(kNote60, QStringLiteral("mute")));
        profile.bind(button(kNote61, QStringLiteral("nr.toggle")));
        profile.save();
        profile.clearControl(kNote61);
        profile.save();
        MidiProfile loaded(QStringLiteral("Desk"));
        loaded.load();
        QCOMPARE(loaded.bindings().size(), 1);
        const QStringList keys = AppSettings::instance().allKeys();
        QVERIFY(!keys.contains(QStringLiteral("HotKeys/Midi/Profiles/Desk/Binding1/ControlKey")));
    }

    void profile_aNameIsTheUsersText()
    {
        const QString name = QStringLiteral("Bob's desk/2 <main> & ü");
        MidiProfile saved(name);
        saved.bind(button(kNote60, QStringLiteral("mute")));
        saved.save();
        QCOMPARE(MidiProfile::savedNames(), QStringList{name});
        MidiProfile loaded(name);
        loaded.load();
        QCOMPARE(loaded.bindings().size(), 1);
        // The tag stays a name the settings file can hold.
        const QByteArray xml = AppSettings::instance().exportLocalXml();
        QVERIFY(AppSettings::validateLocalXml(xml));
        MidiProfile::removeSaved(name);
        QVERIFY(MidiProfile::savedNames().isEmpty());
    }

    void profile_emptyOneStillExists()
    {
        MidiProfile(QStringLiteral("New")).save();
        QCOMPARE(MidiProfile::savedNames(), QStringList{QStringLiteral("New")});
        MidiProfile loaded(QStringLiteral("New"));
        loaded.load();
        QVERIFY(loaded.bindings().isEmpty());
    }

    void profile_deletedActionIsDroppedWithAWarning()
    {
        MidiProfile saved(QStringLiteral("Desk"));
        saved.bind(button(kNote60, QStringLiteral("mute")));
        saved.save();
        AppSettings& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("HotKeys/Midi/Profiles/Desk/Count"), 2);
        const QString at = QStringLiteral("HotKeys/Midi/Profiles/Desk/Binding1/");
        settings.setValue(at + QStringLiteral("ControlKey"), kNote61);
        settings.setValue(at + QStringLiteral("Kind"), QStringLiteral("Button"));
        settings.setValue(at + QStringLiteral("ActionId"), QStringLiteral("gone.forever"));

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Dropped a saved MIDI binding.*gone\\.forever")));
        MidiProfile loaded(QStringLiteral("Desk"));
        loaded.load();
        QCOMPARE(loaded.bindings().size(), 1);
        QCOMPARE(loaded.bindings().at(0).actionId, QStringLiteral("mute"));
    }

    void profile_savedKnobOnATransmitActionIsDropped()
    {
        AppSettings& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("HotKeys/Midi/Profiles/Desk/Count"), 1);
        const QString at = QStringLiteral("HotKeys/Midi/Profiles/Desk/Binding0/");
        settings.setValue(at + QStringLiteral("ControlKey"), kKnob);
        settings.setValue(at + QStringLiteral("Kind"), QStringLiteral("RelativeKnob"));
        settings.setValue(at + QStringLiteral("ActionId"), QStringLiteral("tx.mox"));

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Dropped a saved MIDI binding.*tx\\.mox")));
        MidiProfile loaded(QStringLiteral("Desk"));
        loaded.load();
        QVERIFY(loaded.bindings().isEmpty());
    }

    void profile_oneBindingPerControl()
    {
        MidiProfile profile(QStringLiteral("Desk"));
        profile.bind(button(kNote60, QStringLiteral("mute")));
        profile.bind(button(kNote60, QStringLiteral("nr.toggle")));
        QCOMPARE(profile.bindings().size(), 1);
        QCOMPARE(profile.bindings().at(0).actionId, QStringLiteral("nr.toggle"));
    }

    void profile_knobRefusesWhatTurnWouldIgnore_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("up");
        QTest::addColumn<QString>("down");
        QTest::addColumn<QString>("reasonHas");
        const int relative = int(MidiControlKind::RelativeKnob);
        const int absolute = int(MidiControlKind::AbsoluteKnob);
        QTest::newRow("hold") << relative << "tx.hold" << "" << "Hold to talk";
        QTest::newRow("mox") << absolute << "tx.mox" << "" << "MOX";
        QTest::newRow("tune") << relative << "tx.tune" << "" << "TUNE";
        QTest::newRow("cw key") << relative << "cw.dot" << "cw.dash" << "CW dot";
        QTest::newRow("cw message") << absolute << "cw.msg1" << "" << "CW message 1";
        QTest::newRow("voice message") << relative << "voice.msg1" << "" << "Voice message 1";
        QTest::newRow("only the down way") << relative << "tune.up" << "tx.mox" << "MOX";
    }

    void profile_knobRefusesWhatTurnWouldIgnore()
    {
        QFETCH(int, kind);
        QFETCH(QString, up);
        QFETCH(QString, down);
        QFETCH(QString, reasonHas);
        MidiProfile profile(QStringLiteral("Desk"));
        const QString reason = profile.bind(MidiBinding{kKnob, MidiControlKind(kind), up, down, QChar()});
        QVERIFY2(reason.contains(reasonHas), qPrintable(reason));
        QVERIFY(reason.contains(QStringLiteral("button")));
        QVERIFY(profile.bindings().isEmpty());
    }

    void profile_aButtonMayRunThemAll_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"tx.hold", "tx.mox", "tx.tune", "cw.dot", "cw.msg1", "voice.msg1", "tune.up"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void profile_aButtonMayRunThemAll()
    {
        QFETCH(QString, id);
        MidiProfile profile(QStringLiteral("Desk"));
        QCOMPARE(profile.bind(button(kNote60, id)), QString());
    }

    void profile_stopActionsDoNotTransmit()
    {
        MidiProfile profile(QStringLiteral("Desk"));
        QCOMPARE(profile.bind(relativeKnob(kKnob, QStringLiteral("cw.stop"), QStringLiteral("voice.stop"))), QString());
    }

    void profile_refusalsAreInPlainWords()
    {
        MidiProfile profile(QStringLiteral("Desk"));
        QCOMPARE(profile.bind(MidiBinding{kKnob, MidiControlKind::RelativeKnob, QStringLiteral("tx.hold"), QString(), QChar()}),
                 QStringLiteral("A knob can't run Hold to talk. Bind it to a button."));
        QCOMPARE(profile.bind(button(kNote60, QStringLiteral("gone.forever"))),
                 QStringLiteral("That action no longer exists."));
        QCOMPARE(profile.bind(button(kNote60, QString())), QStringLiteral("Choose an action first."));
        QCOMPARE(profile.bind(button(QString(), QStringLiteral("mute"))), QStringLiteral("Move a control first."));
    }

    void profile_activeNameRoundTrips()
    {
        QCOMPARE(MidiProfile::activeName(), QString());
        MidiProfile::setActiveName(QStringLiteral("Desk"));
        QCOMPARE(MidiProfile::activeName(), QStringLiteral("Desk"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Midi/ActiveProfile")).toString(),
                 QStringLiteral("Desk"));
    }

    // ---- Routing ----

    void routing_buttonPressesAndReleases()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
    }

    void routing_aRepeatedNoteOnStartsNothingNew()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.mox"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 90, 0);
        QCOMPARE(t().transmitActions, QStringList{QStringLiteral("tx.mox")});
        // A press that isn't a hold has nothing to release, and MOX stays on.
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        QCOMPARE(t().transmitActions, QStringList{QStringLiteral("tx.mox")});
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QCOMPARE(t().transmitActions.size(), 2);
    }

    void routing_cwKeyHoldsThroughRelease()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("cw.dot"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        QCOMPARE(t().cwKeys.size(), 2);
        QCOMPARE(t().cwKeys.at(0), qMakePair(QStringLiteral("cw.dot"), true));
        QCOMPARE(t().cwKeys.at(1), qMakePair(QStringLiteral("cw.dot"), false));
    }

    void routing_twoButtonsOnOneHoldEndWithTheLast()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold")),
                                           button(kNote61, QStringLiteral("tx.hold"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        host().onMidiControlEvent(kNote61, MidiControlKind::Button, 64, 0);
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        host().onMidiControlEvent(kNote61, MidiControlKind::Button, 0, 0);
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
    }

    void routing_aChangedProfileStillReleasesWhatWasPressed()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        host().setMidiProfile(MidiProfile(QStringLiteral("Empty")));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
    }

    void routing_relativeKnobTurns()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({relativeKnob(kKnob, QStringLiteral("tune.up"), QStringLiteral("tune.down"))}));
        host().onMidiControlEvent(kKnob, MidiControlKind::RelativeKnob, 65, 3);
        QCOMPARE(t().tunes.size(), 1);
        QCOMPARE(t().tunes.at(0), 14200000.0 + 300.0);
        t().clock += 1000;
        host().onMidiControlEvent(kKnob, MidiControlKind::RelativeKnob, 63, -1);
        QCOMPARE(t().tunes.size(), 2);
        QCOMPARE(t().tunes.at(1), 14200000.0 + 200.0);
    }

    void routing_aKnobWithNoDownActionDoesNothingThatWay()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({relativeKnob(kKnob, QStringLiteral("filter.next"), QString())}));
        host().onMidiControlEvent(kKnob, MidiControlKind::RelativeKnob, 127, -1);
        QVERIFY(t().sliceActions.isEmpty());
        host().onMidiControlEvent(kKnob, MidiControlKind::RelativeKnob, 1, 1);
        QCOMPARE(t().sliceActions.size(), 1);
    }

    void routing_levelFaderMovesItsRangeOver20Steps()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({absoluteKnob(kFader, QStringLiteral("volume.up"), QStringLiteral("volume.down"))}));
        // One value at a time, up the whole range.
        sendAndFlush(kFader, 0, 0);
        for (int value = 1; value <= 127; ++value) {
            sendAndFlush(kFader, value, 1);
        }
        QCOMPARE(t().sumOf(QStringLiteral("volume.up")), 20);
        QCOMPARE(t().sumOf(QStringLiteral("volume.down")), 0);
        // And back down: none lost, none made up.
        for (int value = 126; value >= 0; --value) {
            sendAndFlush(kFader, value, -1);
        }
        QCOMPARE(t().sumOf(QStringLiteral("volume.up")), 20);
        QCOMPARE(t().sumOf(QStringLiteral("volume.down")), 20);
    }

    void routing_levelFaderKeepsWhatIsLeftOverAcrossJumps()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({absoluteKnob(kFader, QStringLiteral("volume.up"), QStringLiteral("volume.down"))}));
        int total = 0;
        int value = 0;
        for (const int jump : {3, 5, 2, 11, 7, 30, 1, 40, 28}) {
            value += jump;
            total += jump;
            sendAndFlush(kFader, value, jump);
        }
        QCOMPARE(total, 127);
        QCOMPARE(t().sumOf(QStringLiteral("volume.up")), 20);
    }

    void routing_faderOnAnythingElseTakesOneStepPerValue()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({absoluteKnob(kFader, QStringLiteral("filter.next"), QStringLiteral("filter.previous"))}));
        sendAndFlush(kFader, 40, 0);
        sendAndFlush(kFader, 45, 5);
        sendAndFlush(kFader, 43, -2);
        int next = 0;
        int previous = 0;
        for (const FakeTargets::SliceCall& call : std::as_const(t().sliceActions)) {
            next += call.id == QLatin1String("filter.next") ? 1 : 0;
            previous += call.id == QLatin1String("filter.previous") ? 1 : 0;
        }
        QCOMPARE(next, 5);
        QCOMPARE(previous, 2);
    }

    void routing_pitchBendCountsLikeACc()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({absoluteKnob(kBend, QStringLiteral("volume.up"), QStringLiteral("volume.down"))}));
        sendAndFlush(kBend, 0, 0);
        for (int value = 1024; value <= 16384; value += 1024) {
            sendAndFlush(kBend, qMin(value, 16383), qMin(value, 16383) - (value - 1024));
        }
        QCOMPARE(t().sumOf(QStringLiteral("volume.up")), 20);
    }

    void routing_refusesAKnobOnATransmitAction()
    {
        MidiProfile profile(QStringLiteral("Hand edited"));
        profile.appendUncheckedForTest(relativeKnob(kKnob, QStringLiteral("tx.mox"), QStringLiteral("tx.mox")));
        profile.appendUncheckedForTest(absoluteKnob(kFader, QStringLiteral("tx.hold"), QString()));
        QSignalSpy refused(m_host.get(), &ControllerHost::midiBindingRefused);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Refused a MIDI binding.*tx\\.mox")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Refused a MIDI binding.*tx\\.hold")));
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profile);
        QCOMPARE(refused.size(), 2);
        QCOMPARE(refused.at(0).at(0).toString(), kKnob);
        QCOMPARE(refused.at(0).at(1).toString(), QStringLiteral("A knob can't run MOX. Bind it to a button."));

        host().onMidiControlEvent(kKnob, MidiControlKind::RelativeKnob, 1, 1);
        host().onMidiControlEvent(kFader, MidiControlKind::AbsoluteKnob, 64, 64);
        QVERIFY(t().transmitActions.isEmpty());
        QVERIFY(t().keyHolds.isEmpty());
    }

    void routing_passesThePin()
    {
        SliceModel other(1);
        other.setFrequency(7100000.0);
        other.setStepHz(100);
        t().window = &other;    // not the slice the fake gives for the letter
        struct PinTargets : FakeTargets {
            SliceModel* pinned{nullptr};
            QChar asked;
            SliceModel* sliceByLetter(QChar letter) const override
            {
                const_cast<PinTargets*>(this)->asked = letter;
                return pinned;
            }
        } targets;
        targets.window = &other;
        targets.pinned = m_slice.get();
        HotKeyDispatcher dispatcher(m_bindings.get(), &targets);
        ControllerHost pinnedHost(&dispatcher, m_bindings.get());
        pinnedHost.onMidiPortOpened(kPad);
        MidiBinding binding = button(kNote60, QStringLiteral("mute"));
        binding.pin = QLatin1Char('B');
        pinnedHost.setMidiProfile(profileWith({binding}));
        pinnedHost.onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QCOMPARE(targets.asked, QLatin1Char('B'));
        t().window = m_slice.get();
    }

    void routing_withNoDevicePressesNothing()
    {
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QVERIFY(t().keyHolds.isEmpty());
    }

    void routing_offSwitchStopsItAndForgetsWhatIsDown()
    {
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        m_bindings->setEnabled(false);
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QCOMPARE(t().keyHolds.size(), 2);
        m_bindings->setEnabled(true);
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        // A press after the switch came back is a press, not a repeat.
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QCOMPARE(t().keyHolds.size(), 3);
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
    }

    void routing_sleepForgetsWhatIsDown()
    {
        qint64 wall = 1000000;
        qint64 awake = 5000;
        m_dispatcher->setSleepClocksForTest([&wall] { return wall; }, [&awake] { return awake; });
        host().onMidiPortOpened(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        wall += HotKeyDispatcher::kSleepGapMs + 1000;
        m_dispatcher->checkForSleep();
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
        // The release was lost in sleep. The next press still works.
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 64, 0);
        QCOMPARE(t().keyHolds, QList<bool>({true, false, true}));
        host().onMidiControlEvent(kNote60, MidiControlKind::Button, 0, 0);
        m_dispatcher->setSleepClocksForTest([] { return qint64{0}; }, [] { return qint64{0}; });
    }

    // ---- Unplug ----

    void unplug_dropsTheHoldAndComesBack()
    {
        m_layer->ports->setNames({kPad});
        MidiInput input;
        wire(input);
        QSignalSpy connection(&input, &MidiInput::connectionChanged);
        input.openPort(kPad);
        QCOMPARE(connection.size(), 1);
        QCOMPARE(connection.at(0).at(0).toBool(), true);
        QCOMPARE(m_layer->ports->opened(), kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));

        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        QCOMPARE(t().keyHolds, QList<bool>{true});

        // The port is still open as far as the device layer knows, until the list says.
        m_layer->ports->setNames({});
        input.checkPorts();
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
        QCOMPARE(connection.size(), 2);
        QCOMPARE(connection.at(1).at(0).toBool(), false);
        QCOMPARE(connection.at(1).at(1).toString(), QStringLiteral("Not plugged in."));
        QCOMPARE(m_layer->ports->opened(), QString());

        // Still gone: nothing more to say.
        input.checkPorts();
        QCOMPARE(connection.size(), 2);

        // It comes back. Connecting never keys.
        m_layer->ports->setNames({kPad});
        input.checkPorts();
        QCOMPARE(connection.size(), 3);
        QCOMPARE(connection.at(2).at(0).toBool(), true);
        QCOMPARE(m_layer->ports->opened(), kPad);
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));

        // And it works.
        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        QCOMPARE(t().keyHolds, QList<bool>({true, false, true}));
        m_layer->ports->send(0x80, 60, 0);
        deliverQueued(input);
        QCOMPARE(t().keyHolds, QList<bool>({true, false, true, false}));
    }

    void unplug_leavesALatchedMoxOn()
    {
        m_layer->ports->setNames({kPad});
        MidiInput input;
        wire(input);
        input.openPort(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));

        t().transmittingNow = true;     // MOX is on before the button goes down
        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        QVERIFY(t().keyHolds.isEmpty());

        m_layer->ports->setNames({});
        input.checkPorts();
        QVERIFY(t().keyHolds.isEmpty());
        QVERIFY(t().transmitActions.isEmpty());
        QVERIFY(t().transmittingNow);
    }

    void unplug_dropsOnlyThatDevicesHold()
    {
        const HotKeySource keyboard{HotKeySourceKind::Keyboard, {}};
        m_layer->ports->setNames({kPad});
        MidiInput input;
        wire(input);
        input.openPort(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));

        m_dispatcher->press(QStringLiteral("tx.hold"), keyboard);
        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        QCOMPARE(t().keyHolds, QList<bool>{true});

        m_layer->ports->setNames({});
        input.checkPorts();
        QCOMPARE(t().keyHolds, QList<bool>{true});      // the keyboard still holds it

        m_dispatcher->release(QStringLiteral("tx.hold"), keyboard);
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
    }

    void unplug_cwKeyLetsGo()
    {
        m_layer->ports->setNames({kPad});
        MidiInput input;
        wire(input);
        input.openPort(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("cw.straight"))}));
        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        m_layer->ports->setNames({});
        input.checkPorts();
        QCOMPARE(t().cwKeys.size(), 2);
        QCOMPARE(t().cwKeys.at(1).second, false);
    }

    void unplug_aMessageQueuedBeforeTheCloseIsDropped()
    {
        m_layer->ports->setNames({kPad});
        MidiInput input;
        wire(input);
        input.openPort(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));

        // A press on its way as the device goes: its release can never arrive.
        m_layer->ports->send(0x90, 60, 64);
        m_layer->ports->setNames({});
        input.checkPorts();
        deliverQueued(input);
        QVERIFY(t().keyHolds.isEmpty());
    }

    void port_closingAndSwitchingDropsTheHold()
    {
        m_layer->ports->setNames({kPad, QStringLiteral("Other")});
        MidiInput input;
        wire(input);
        input.openPort(kPad);
        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        input.openPort(QStringLiteral("Other"));
        QCOMPARE(t().keyHolds, QList<bool>({true, false}));
        QCOMPARE(m_layer->ports->opened(), QStringLiteral("Other"));

        m_layer->ports->send(0x90, 60, 64);
        deliverQueued(input);
        input.closePort();
        QCOMPARE(t().keyHolds, QList<bool>({true, false, true, false}));
        QCOMPARE(m_layer->ports->opened(), QString());
        // Nothing is wanted now, so it does not come back.
        input.checkPorts();
        QCOMPARE(m_layer->ports->opened(), QString());
    }

    void port_notThereYetOpensWhenItComes()
    {
        MidiInput input;
        QSignalSpy connection(&input, &MidiInput::connectionChanged);
        input.openPort(QStringLiteral("launchkey"));
        QCOMPARE(connection.size(), 1);
        QCOMPARE(connection.at(0).at(0).toBool(), false);
        QCOMPARE(connection.at(0).at(1).toString(), QStringLiteral("Not plugged in."));
        m_layer->ports->setNames({QStringLiteral("Launchkey Mini MIDI")});
        input.checkPorts();
        QCOMPARE(m_layer->ports->opened(), QStringLiteral("Launchkey Mini MIDI"));
        QCOMPARE(connection.last().at(0).toBool(), true);
    }

    void port_thatWontOpenSaysWhyOnce()
    {
        m_layer->ports->setNames({kPad});
        m_layer->ports->refuse = {kPad};
        MidiInput input;
        QSignalSpy connection(&input, &MidiInput::connectionChanged);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("MIDI: open failed")));
        input.openPort(kPad);
        QCOMPARE(connection.size(), 1);
        QCOMPARE(connection.at(0).at(1).toString(),
                 QStringLiteral("Pad could not be opened. Close any other app using it, then try again."));
        // The other app lets go.
        m_layer->ports->refuse.clear();
        input.checkPorts();
        QCOMPARE(m_layer->ports->opened(), kPad);
        QCOMPARE(connection.last().at(0).toBool(), true);
    }

    void port_listIsTheDeviceLayers()
    {
        m_layer->ports->setNames({kPad, QStringLiteral("Other")});
        QCOMPARE(MidiInput::portNames(), QStringList({kPad, QStringLiteral("Other")}));
        QVERIFY(MidiInput::supported());
    }

    void port_theTimerIsTwoSeconds()
    {
        QCOMPARE(kPortCheckMs, 2000);
    }

    // ---- The host and its worker ----

    void worker_messageCrossesToTheDispatcherAndDropsOnUnplug()
    {
        m_layer->ports->setNames({kPad});
        MidiInput* const midi = host().midi();
        QVERIFY(midi->thread() != QThread::currentThread());
        QVERIFY(midi->thread()->isRunning());
        QCOMPARE(midi->thread()->objectName(), QStringLiteral("NereusMidi"));

        host().setMidiProfile(profileWith({button(kNote60, QStringLiteral("tx.hold"))}));
        QSignalSpy connection(midi, &MidiInput::connectionChanged);
        host().setMidiDevice(kPad);
        QVERIFY(connection.wait(60000));
        QCOMPARE(connection.at(0).at(0).toBool(), true);

        m_layer->ports->send(0x90, 60, 64);
        QTRY_COMPARE_WITH_TIMEOUT(t().keyHolds, QList<bool>{true}, 60000);

        m_layer->ports->setNames({});
        QMetaObject::invokeMethod(midi, &MidiInput::checkPorts, Qt::QueuedConnection);
        QTRY_COMPARE_WITH_TIMEOUT(t().keyHolds, QList<bool>({true, false}), 60000);
    }

    void worker_stopsAndLeavesNothingBehind()
    {
        m_layer->ports->setNames({kPad});
        QPointer<MidiInput> midi(host().midi());
        QVERIFY(m_layer->ports->alive() >= 1);
        QSignalSpy connection(midi.data(), &MidiInput::connectionChanged);
        host().setMidiDevice(kPad);
        QVERIFY(connection.wait(60000));
        QCOMPARE(m_layer->ports->opened(), kPad);

        m_host.reset();
        QVERIFY(midi.isNull());
        QCOMPARE(m_layer->ports->alive(), 0);
        QCOMPARE(m_layer->ports->opened(), QString());
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
    }

    void host_startsFromWhatWasSaved()
    {
        MidiProfile saved(QStringLiteral("Desk"));
        saved.bind(button(kNote60, QStringLiteral("tx.hold")));
        saved.save();
        MidiProfile::setActiveName(QStringLiteral("Desk"));
        host().setMidiDevice(QString());
        AppSettings::instance().setValue(QStringLiteral("HotKeys/Midi/Device"), kPad);
        QCOMPARE(ControllerHost::savedMidiDevice(), kPad);
        m_layer->ports->setNames({kPad});

        m_host.reset();
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        QCOMPARE(host().midiProfile().name(), QStringLiteral("Desk"));
        QCOMPARE(host().midiProfile().bindings().size(), 1);
        // Opening is queued to the worker, and it never keys.
        QTRY_COMPARE_WITH_TIMEOUT(m_layer->ports->opened(), kPad, 60000);
        QVERIFY(t().keyHolds.isEmpty());
    }

    void host_startsQuietWhenNothingWasSaved()
    {
        QCOMPARE(host().midiProfile().bindings().size(), 0);
        QCOMPARE(m_layer->ports->opened(), QString());
    }
};

QTEST_MAIN(TestMidiProfile)
#include "tst_midi_profile.moc"
