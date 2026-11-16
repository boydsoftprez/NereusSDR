// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 3.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-08 - J.J. Boyd (KG4VCF), hot keys: with band stacking in main, a real
//                window's band stack keys find its functions.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 13: the voice keys,
//                F1 to F8 by the transmit slice's mode, and Escape.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// Each non-transmit hot key on a real offscreen local window: fire the action
// through the window's dispatcher, then read the model.
#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QLoggingCategory>
#include <QSslSocket>
#include <QTemporaryDir>

#include <memory>

#include "UnusedLoopbackPort.h"
#include "core/CwxSender.h"
#include "core/AppSettings.h"
#include "core/NbFamily.h"
#include "core/SliceOwnership.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SliceAccessController.h"
#include "core/session/StationServer.h"
#include "fakes/FakeRecordingsAccess.h"
#include "gui/MainWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "gui/widgets/StatusToast.h"
#include "gui/widgets/VfoWidget.h"
#include "models/BandDefaults.h"
#include "models/BandGrid.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/recordings/RecordingsAccess.h"

using namespace NereusSDR;

namespace {

const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};

const QString kOlderCore = QStringLiteral("This Core needs an update for recordings.");
const QString kHeld = QStringLiteral("iPhone has the transmitter. Take it to transmit from here.");
const QString kBandStackReason = QStringLiteral("Band stacking is not ready.");

bool applyLayout(MainWindow& window, const QString& layoutId)
{
    return QMetaObject::invokeMethod(&window, "applyPanLayout", Q_ARG(QString, layoutId));
}

VfoWidget* flagFor(MainWindow& window, int id)
{
    for (VfoWidget* flag : window.findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == id) {
            return flag;
        }
    }
    return nullptr;
}

bool toastShown(MainWindow& window, const QString& message)
{
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->message() == message) {
            return true;
        }
    }
    return false;
}

int toastCount(MainWindow& window)
{
    return int(window.findChildren<StatusToast*>().size());
}

// The recordings document with M1 to M8 (`filled` hold a message) and what
// goes out, as the Core publishes it.
QJsonObject voiceDocument(const QList<int>& filled, const QJsonValue& sending = QJsonValue::Null)
{
    QJsonArray slotList;
    for (int n = 1; n <= RecordingsAccess::kVoiceKeyerSlots; ++n) {
        const bool has = filled.contains(n);
        slotList.append(QJsonObject{{QStringLiteral("n"), n},
                                    {QStringLiteral("name"), QString()},
                                    {QStringLiteral("lengthMs"), has ? 6000 : 0},
                                    {QStringLiteral("empty"), !has}});
    }
    QJsonObject doc = FakeRecordingsAccess::document({}, 1);
    doc.insert(QStringLiteral("voiceKeyer"), QJsonObject{{QStringLiteral("slots"), slotList},
                                                          {QStringLiteral("repeat"), false},
                                                          {QStringLiteral("repeatDelaySec"), 5},
                                                          {QStringLiteral("sending"), sending}});
    return doc;
}

QJsonObject sendingFrom(int n, const QString& deviceId)
{
    return QJsonObject{{QStringLiteral("n"), n},
                       {QStringLiteral("positionMs"), 1000},
                       {QStringLiteral("lengthMs"), 6000},
                       {QStringLiteral("waitingMs"), 0},
                       {QStringLiteral("by"), QStringLiteral("iPhone")},
                       {QStringLiteral("deviceId"), deviceId}};
}

QStringList voiceCalls(const FakeRecordingsAccess& access)
{
    QStringList out;
    for (const FakeRecordingsAccess::Call& call : access.calls) {
        out.append(call.verb == QLatin1String("sendVoice") ? QStringLiteral("sendVoice %1").arg(call.sliceId)
                                                           : call.verb);
    }
    return out;
}

Band gridBand(const SliceModel* slice)
{
    return BandDefaults::bandGridBandFromFrequency(slice->frequency());
}

// One local window, connected, with Slice A on pan-0 and an optional Slice B on pan-1.
struct LocalWindow {
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    HotKeyDispatcher* keys{window.hotKeyDispatcher()};
    PanadapterStack* stack{window.findChild<PanadapterStack*>()};
    SliceModel* a{nullptr};
    SliceModel* b{nullptr};

    void openA()
    {
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        a = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
        QVERIFY(a != nullptr);
        QVERIFY(keys != nullptr);
    }

    void openAOnPan0BOnPan1()
    {
        openA();
        b = model->sliceById(model->addSlice(QStringLiteral("pan-1")));
        QVERIFY(b != nullptr);
        QVERIFY(applyLayout(window, QStringLiteral("2v")));
        QVERIFY(stack != nullptr);
        // Far enough apart that each pan shows its own slice's stream.
        b->setFrequency(a->frequency() + 1'000'000.0);
    }

    void press(const char* id, QChar pin = {})
    {
        keys->press(QString::fromLatin1(id), kKeyboard, pin);
    }
};

// A local window whose voice keys talk to a fake recordings access, run
// through a dispatcher of their own, as the window's runs them.
struct VoiceBench {
    LocalWindow w;
    FakeRecordingsAccess access;
    MainWindowHotKeyTargets targets{&w.window};
    HotKeyBindings bindings;
    HotKeyDispatcher keys{&bindings, &targets};

    VoiceBench()
    {
        w.openA();
        access.version = RecordingsAccess::kVoiceKeyerVersion;
        access.setDocument(voiceDocument({1, 2, 3, 4, 5, 6, 7, 8}));
        targets.setRecordingsForTest(&access);
    }

    void press(const char* id) { keys.press(QString::fromLatin1(id), kKeyboard); }
};

// The four slots band stack will add to MainWindow.
class WindowWithBandStack : public QObject {
    Q_OBJECT
public:
    int next{0};
    int previous{0};
    int add{0};
    int panel{0};
public slots:
    void bandStackNext() { ++next; }
    void bandStackPrevious() { ++previous; }
    void bandStackAdd() { ++add; }
    void bandStackPanel() { ++panel; }
};

} // namespace

class TestHotKeyWindowActions : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
    }

    void tuneMovesTheSliceAndItsFlagByTheSlicesStep()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.a->setStepHz(100);
        w.a->setFrequency(14'200'000.0);
        VfoWidget* const flag = flagFor(w.window, w.a->sliceIndex());
        QVERIFY(flag != nullptr);

        w.press("tune.up");

        QCOMPARE(w.a->frequency(), 14'200'100.0);
        QCOMPARE(flag->frequencyLabelForTest(), QStringLiteral("14.200.100"));
    }

    void stepKeysMoveTheStepList()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.a->setStepHz(100);
        SliceModel expected;
        expected.setStepHz(100);
        expected.changeTuneStepUp();

        w.press("step.larger");
        QCOMPARE(w.a->stepHz(), expected.stepHz());
        QVERIFY(w.a->stepHz() != 100);

        w.press("step.smaller");
        QCOMPARE(w.a->stepHz(), 100);
    }

    void lockAnfRitXitToggle()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        const bool anf = w.a->anfEnabled();
        w.press("anf.toggle");
        QCOMPARE(w.a->anfEnabled(), !anf);
        w.press("anf.toggle");
        QCOMPARE(w.a->anfEnabled(), anf);

        const bool rit = w.a->ritEnabled();
        w.press("rit.toggle");
        QCOMPARE(w.a->ritEnabled(), !rit);

        const bool xit = w.a->xitEnabled();
        w.press("xit.toggle");
        QCOMPARE(w.a->xitEnabled(), !xit);

        QVERIFY(!w.a->locked());
        w.press("slice.lock");
        QVERIFY(w.a->locked());
        w.press("slice.lock");
        QVERIFY(!w.a->locked());
    }

    void nrAndNbCycleAsTheContainerButtonsDo()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        QCOMPARE(w.a->activeNr(), NrSlot::Off);
        w.press("nr.toggle");
        QCOMPARE(w.a->activeNr(), NrSlot::NR1);

        const NbMode before = w.a->nbMode();
        w.press("nb.toggle");
        QCOMPARE(w.a->nbMode(), cycleNbMode(before));
    }

    void goToFrequencyUnfoldsTheFlagThenOpensTheEdit()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        VfoWidget* const flag = flagFor(w.window, w.a->sliceIndex());
        QVERIFY(flag != nullptr);
        flag->setCollapsed(true);
        QVERIFY(!flag->frequencyEditingForTest());

        w.press("tune.goto");

        QVERIFY(!flag->isCollapsed());
        QVERIFY(flag->frequencyEditingForTest());
    }

    void goToFrequencyOnALockedSliceRefusesWithNoEdit()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        VfoWidget* const flag = flagFor(w.window, w.a->sliceIndex());
        QVERIFY(flag != nullptr);
        w.a->setLocked(true);

        w.press("tune.goto");

        QVERIFY(!flag->frequencyEditingForTest());
        QVERIFY(toastShown(w.window, QStringLiteral("Slice A is locked.")));
    }

    void nextSliceWalksTheTabsAndWraps()
    {
        LocalWindow w;
        w.openAOnPan0BOnPan1();
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(w.model->setActiveSliceById(w.a->sliceIndex()));

        w.press("slice.next");
        QCOMPARE(w.model->activeSlice(), w.b);
        w.press("slice.next");
        QCOMPARE(w.model->activeSlice(), w.a);
    }

    void filterWalk()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.a->setDspMode(DSPMode::USB);
        const QList<FilterPreset> presets = w.model->filterPresetStore()->presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() >= 3);

        // From the first, next gives the second.
        w.a->setFilter(presets.first().low, presets.first().high);
        w.press("filter.next");
        QCOMPARE(w.a->filterLow(), presets[1].low);
        QCOMPARE(w.a->filterHigh(), presets[1].high);

        // From the first, previous gives the last.
        w.a->setFilter(presets.first().low, presets.first().high);
        w.press("filter.previous");
        QCOMPARE(w.a->filterLow(), presets.last().low);
        QCOMPARE(w.a->filterHigh(), presets.last().high);

        // Off every preset, next gives the first and previous the last.
        constexpr int kOffLow = 37;
        constexpr int kOffHigh = 211;
        w.a->setFilter(kOffLow, kOffHigh);
        w.press("filter.next");
        QCOMPARE(w.a->filterLow(), presets.first().low);
        QCOMPARE(w.a->filterHigh(), presets.first().high);
        w.a->setFilter(kOffLow, kOffHigh);
        w.press("filter.previous");
        QCOMPARE(w.a->filterLow(), presets.last().low);
        QCOMPARE(w.a->filterHigh(), presets.last().high);

        // Within 50 Hz per edge still counts as on the preset.
        w.a->setFilter(presets.first().low + 50, presets.first().high - 50);
        w.press("filter.next");
        QCOMPARE(w.a->filterLow(), presets[1].low);
        QCOMPARE(w.a->filterHigh(), presets[1].high);
    }

    void modeWalkGoesOnceAroundAndSkipsADisabledRade2()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY2(!w.model->radeV2Enabled(), "this case is the default, RADE v2 off");
        w.a->setDspMode(DSPMode::USB);

        // The RX applet's list, less RADE2-U and RADE2-L.
        const QList<DSPMode> expected = {
            DSPMode::LSB, DSPMode::CWU, DSPMode::CWL, DSPMode::AM, DSPMode::SAM, DSPMode::FM,
            DSPMode::DSB, DSPMode::DIGU, DSPMode::DIGL, DSPMode::DRM, DSPMode::RADE_U,
            DSPMode::RADE_L, DSPMode::USB,
        };
        for (const DSPMode mode : expected) {
            w.press("mode.next");
            QCOMPARE(w.a->dspMode(), mode);
        }

        w.press("mode.previous");
        QCOMPARE(w.a->dspMode(), DSPMode::RADE_L);
    }

    void bandWalkFollowsTheGridAndWraps()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.a->setFrequency(28'400'000.0);
        QCOMPARE(gridBand(w.a), Band::Band10m);
        w.press("band.next");
        QCOMPARE(gridBand(w.a), Band::Band6m);

        // A local window has no link saying 2 m is missing, so it isn't skipped.
        w.press("band.next");
        QCOMPARE(gridBand(w.a), Band::Band2m);

        w.a->setFrequency(10'000'000.0);
        QCOMPARE(gridBand(w.a), Band::WWV);
        w.press("band.next");
        QCOMPARE(gridBand(w.a), Band::Band160m);
        w.press("band.previous");
        QCOMPARE(gridBand(w.a), Band::WWV);
    }

    void bandWalkFromABandTheGridLacks()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        constexpr double kGeneralCoverageHz = 12'000'000.0;
        w.a->setFrequency(kGeneralCoverageHz);
        QVERIFY(bandGridEntry(gridBand(w.a)) == nullptr);
        w.press("band.next");
        QCOMPARE(gridBand(w.a), Band::Band160m);

        w.a->setFrequency(kGeneralCoverageHz);
        w.press("band.previous");
        QCOMPARE(gridBand(w.a), Band::WWV);
    }

    void volumeAndMuteChangeTheControlledSlice()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.a->setAfGain(50);
        w.press("volume.up");
        QCOMPARE(w.a->afGain(), 55);
        // The second turn inside the burst limit waits for the flush timer.
        w.press("volume.down");
        QTRY_COMPARE(w.a->afGain(), 50);

        // Clamped at the slider's top.
        w.a->setAfGain(99);
        w.press("volume.up");
        QTRY_COMPARE(w.a->afGain(), 100);

        const bool muted = w.a->muted();
        w.press("mute");
        QCOMPARE(w.a->muted(), !muted);
    }

    void zoomActsOnTheActivePanAndAFloatingPanWithFocus()
    {
        LocalWindow w;
        w.openAOnPan0BOnPan1();
        if (QTest::currentTestFailed()) {
            return;
        }
        SpectrumWidget* const active = w.window.activeSpectrumWidget();
        QVERIFY(active != nullptr);
        const double start = active->bandwidth();
        QVERIFY(start > 0.0);

        w.press("zoom.in");
        const double zoomedIn = active->bandwidth();
        QVERIFY2(zoomedIn < start, qPrintable(QString::number(zoomedIn)));
        w.press("zoom.out");
        QVERIFY(active->bandwidth() > zoomedIn);

        w.stack->floatPanadapter(QStringLiteral("pan-1"));
        PanFloatingWindow* const floating = w.stack->floatingWindowForTest(QStringLiteral("pan-1"));
        QVERIFY(floating != nullptr);
        MainWindowHotKeyTargets targets(&w.window);
        QCOMPARE(targets.zoomTarget(floating), w.stack->spectrum(QStringLiteral("pan-1")));
        QCOMPARE(targets.zoomTarget(&w.window), w.window.activeSpectrumWidget());
        QCOMPARE(targets.zoomTarget(nullptr), w.window.activeSpectrumWidget());
    }

    void bandStackIsFoundByItsSlotNames()
    {
        WindowWithBandStack ready;
        QObject bare;
        const QStringList ids = {QStringLiteral("bandstack.next"), QStringLiteral("bandstack.previous"),
                                 QStringLiteral("bandstack.add"), QStringLiteral("bandstack.panel")};
        for (const QString& id : ids) {
            QCOMPARE(MainWindowHotKeyTargets::bandStackReason(&bare, id), kBandStackReason);
            QVERIFY(!MainWindowHotKeyTargets::runBandStack(&bare, id));
            QCOMPARE(MainWindowHotKeyTargets::bandStackReason(&ready, id), QString());
            QVERIFY(MainWindowHotKeyTargets::runBandStack(&ready, id));
        }
        QCOMPARE(ready.next, 1);
        QCOMPARE(ready.previous, 1);
        QCOMPARE(ready.add, 1);
        QCOMPARE(ready.panel, 1);

        // Anything else is none of band stack's business.
        QCOMPARE(MainWindowHotKeyTargets::bandStackReason(&bare, QStringLiteral("zoom.in")), QString());
    }

    void disabledKeysShowTheirReasonAndChangeNothing()
    {
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.a->setFrequency(14'200'000.0);
        w.a->setDspMode(DSPMode::USB);
        QSignalSpy frequency(w.a, &SliceModel::frequencyChanged);
        QSignalSpy mode(w.a, &SliceModel::dspModeChanged);

        MainWindowHotKeyTargets targets(&w.window);
        // The voice keys go by the window's own recordings.
        const QString voiceReason = w.model->recordings()->unavailableReason(RecordingsAccess::kVoiceKeyerVersion);
        QCOMPARE(targets.unavailableReason(QStringLiteral("voice.msg1")), voiceReason);
        QCOMPARE(targets.setupUnavailableReason(QStringLiteral("voice.stop")), voiceReason);
        // The window has band stacking now, and its own functions say why when
        // the stack can't be used, so the hot key holds no reason of its own.
        QCOMPARE(targets.unavailableReason(QStringLiteral("bandstack.panel")), QString());
        QCOMPARE(targets.unavailableReason(QStringLiteral("tune.up")), QString());

        QCOMPARE(frequency.count(), 0);
        QCOMPARE(mode.count(), 0);
    }

    // ---- Voice messages and F1 to F8 (recording plan Task 13) ----

    void inAVoiceModeTheMessageKeysSendTheVoiceMessage()
    {
        VoiceBench b;
        if (QTest::currentTestFailed()) {
            return;
        }
        b.w.a->setDspMode(DSPMode::USB);
        QCOMPARE(b.targets.unavailableReason(QStringLiteral("msg.3")), QString());
        b.press("msg.3");
        b.press("voice.msg7");
        QCOMPARE(voiceCalls(b.access), (QStringList{QStringLiteral("sendVoice 3"), QStringLiteral("sendVoice 7")}));
    }

    void inCwTheMessageKeysSendTheCwMessage()
    {
        VoiceBench b;
        if (QTest::currentTestFailed()) {
            return;
        }
        b.w.a->setDspMode(DSPMode::CWU);
        CwxSender* const sender = b.w.model->cwxSender();
        QVERIFY(sender != nullptr);
        QSignalSpy cw(sender, &CwxSender::refused);
        // Nothing in CW message 3 here: the CW path answers, in its own words
        // (tst_hot_key_cw has the message going out).
        b.press("msg.3");
        QCOMPARE(cw.count(), 1);
        QVERIFY(toastShown(b.w.window, cw.first().first().toString()));
        QVERIFY(b.access.calls.isEmpty());
    }

    void anOlderCoreRefusesTheVoiceKeysAndLeavesCw()
    {
        VoiceBench b;
        if (QTest::currentTestFailed()) {
            return;
        }
        b.access.version = 2;
        b.w.a->setDspMode(DSPMode::USB);
        for (const char* id : {"voice.msg1", "voice.msg8", "msg.1"}) {
            QCOMPARE(b.targets.unavailableReason(QLatin1String(id)), kOlderCore);
            QCOMPARE(b.targets.setupUnavailableReason(QLatin1String(id)),
                     QLatin1String(id).startsWith(QLatin1String("msg.")) ? QString() : kOlderCore);
        }
        // The Keyboard page shows Stop disabled with the reason; the key itself
        // has nothing to stop, so it says nothing.
        QCOMPARE(b.targets.setupUnavailableReason(QStringLiteral("voice.stop")), kOlderCore);
        QCOMPARE(b.targets.unavailableReason(QStringLiteral("voice.stop")), QString());
        b.press("msg.1");
        QVERIFY(toastShown(b.w.window, kOlderCore));
        const int toasts = toastCount(b.w.window);
        b.press("voice.stop");
        QCOMPARE(toastCount(b.w.window), toasts);
        QVERIFY(b.access.calls.isEmpty());

        b.w.a->setDspMode(DSPMode::CWU);
        QCOMPARE(b.targets.unavailableReason(QStringLiteral("msg.1")), QString());
        QSignalSpy cw(b.w.model->cwxSender(), &CwxSender::refused);
        b.press("msg.1");
        QCOMPARE(cw.count(), 1);
        QVERIFY(b.access.calls.isEmpty());
    }

    void escapeStopsOnlyAMessageGoingOut()
    {
        VoiceBench b;
        if (QTest::currentTestFailed()) {
            return;
        }
        QCOMPARE(b.bindings.actionForChord(QStringLiteral("Escape")), QStringLiteral("voice.stop"));
        const int toasts = toastCount(b.w.window);
        // Nothing going out: Escape does nothing and says nothing.
        b.press("voice.stop");
        QVERIFY(b.access.calls.isEmpty());
        QCOMPARE(toastCount(b.w.window), toasts);

        // Another device's message: the Core refuses this window's stop with
        // the holder's words (ruling 8.5), once.
        b.access.setDocument(voiceDocument({1, 2, 3}, sendingFrom(2, QStringLiteral("token:phone"))));
        b.press("voice.stop");
        QCOMPARE(voiceCalls(b.access), QStringList{QStringLiteral("stopVoice")});
        emit b.access.refused(kHeld);
        QVERIFY(toastShown(b.w.window, kHeld));
        emit b.access.refused(QStringLiteral("The panel's answer."));
        QVERIFY(!toastShown(b.w.window, QStringLiteral("The panel's answer.")));
    }

    void onlyTheKeysOwnRefusalReachesTheToast()
    {
        VoiceBench b;
        if (QTest::currentTestFailed()) {
            return;
        }
        b.w.a->setDspMode(DSPMode::USB);
        const QString empty = QStringLiteral("M6 is empty.");
        b.press("voice.msg6");
        emit b.access.refused(empty);
        QVERIFY(toastShown(b.w.window, empty));

        // The message went out: a later refusal is another control's answer.
        b.press("voice.msg1");
        b.access.setDocument(voiceDocument({1}, sendingFrom(1, QStringLiteral("station"))));
        emit b.access.refused(QStringLiteral("Something else."));
        QVERIFY(!toastShown(b.w.window, QStringLiteral("Something else.")));
    }

    void aPinnedKeyActsOnItsSliceAndAClosedLetterSaysSo()
    {
        LocalWindow w;
        w.openAOnPan0BOnPan1();
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(w.model->setActiveSliceById(w.a->sliceIndex()));
        const bool anfA = w.a->anfEnabled();
        const bool anfB = w.b->anfEnabled();

        w.press("anf.toggle", QLatin1Char('B'));
        QCOMPARE(w.b->anfEnabled(), !anfB);
        QCOMPARE(w.a->anfEnabled(), anfA);

        w.press("anf.toggle", QLatin1Char('D'));
        QVERIFY(toastShown(w.window, QStringLiteral("Slice D isn't open.")));
    }

    // R-HK-07, the local half; tst_hot_key_remote_actions has the remote one.
    void theSelectedFlagChoosesBothSliceAndPan()
    {
        LocalWindow w;
        w.openAOnPan0BOnPan1();
        if (QTest::currentTestFailed()) {
            return;
        }
        MainWindowHotKeyTargets targets(&w.window);
        for (SliceModel* const slice : {w.b, w.a}) {
            VfoWidget* const flag = flagFor(w.window, slice->sliceIndex());
            QVERIFY(flag != nullptr);
            flag->requestActivation();
            QCOMPARE(targets.windowSlice(), slice);
            QCOMPARE(targets.zoomTarget(nullptr), w.window.spectrumForSlice(slice));
        }
    }

    // A hosting window: the phone took Slice B, so this window only listens.
    void hostingWindowListensToAnotherDevicesSlice()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        LocalWindow w;
        w.openA();
        if (QTest::currentTestFailed()) {
            return;
        }
        w.b = w.model->sliceById(w.model->addSlice(QStringLiteral("pan-0")));
        QVERIFY(w.b != nullptr);

        StationHostOptions options;
        options.settings = &settings;
        options.securityDirectory = directory.path();
        options.hostingDevice = StationHostOptions::HostingDevice{QStringLiteral("Shack Mac mini"),
                                                                  QStringLiteral("Shack")};
        options.remoteBind = QStringLiteral("127.0.0.1");
        options.statusPage = false;
        options.remotePort = unusedLoopbackPort();
        DesktopStationController controller(w.model, options);
        w.window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* const server = controller.server();
        QVERIFY(server != nullptr);

        QObject phoneSession;
        DeviceSessionRegistry::Entry phone;
        phone.deviceId = QByteArrayLiteral("phone-device-id-for-hot-keys-001");
        phone.kind = DeviceSessionRegistry::Kind::Paired;
        phone.name = QStringLiteral("Living room iPhone");
        phone.shortName = QStringLiteral("iPhone");
        phone.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(phone, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        w.model->sliceOwnership()->setOwner(w.b->sliceIndex(), phone.deviceId);
        VfoWidget* const flag = flagFor(w.window, w.b->sliceIndex());
        QVERIFY(flag != nullptr);
        QVERIFY(flag->isListening());

        MainWindowHotKeyTargets targets(&w.window);
        const QString refusal = targets.sliceRefusal(w.b);
        QVERIFY(!refusal.isEmpty());

        // Tune refuses with the window's reason and leaves the slice alone.
        const double before = w.b->frequency();
        w.press("tune.up", QLatin1Char('B'));
        QCOMPARE(w.b->frequency(), before);
        QVERIFY2(toastShown(w.window, refusal), qPrintable(refusal));

        // Volume and mute change only what this device hears.
        QSignalSpy af(w.b, &SliceModel::afGainChanged);
        QSignalSpy muted(w.b, &SliceModel::mutedChanged);
        const std::pair<int, bool> start = {flag->listenVolume(), flag->listenMuted()};
        w.press("volume.down", QLatin1Char('B'));
        QTRY_COMPARE(flag->listenVolume(), start.first - 5);
        w.press("mute", QLatin1Char('B'));
        QTRY_COMPARE(flag->listenMuted(), !start.second);
        QCOMPARE(af.count(), 0);
        QCOMPARE(muted.count(), 0);

        controller.stop();
    }
};

QTEST_MAIN(TestHotKeyWindowActions)
#include "tst_hot_key_window_actions.moc"
