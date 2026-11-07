// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 3.
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
#include "core/AppSettings.h"
#include "core/NbFamily.h"
#include "core/SliceOwnership.h"
#include "core/UnbuiltFeatureList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SliceAccessController.h"
#include "core/session/StationServer.h"
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

using namespace NereusSDR;

namespace {

const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};

const QString kVoiceReason = QStringLiteral("Voice messages aren't built.");
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
        QVERIFY(!UnbuiltFeatures::isBuilt(UnbuiltFeature::Voice));
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
        QCOMPARE(targets.unavailableReason(QStringLiteral("voice.msg1")), kVoiceReason);
        QCOMPARE(targets.unavailableReason(QStringLiteral("voice.stop")), kVoiceReason);
        QCOMPARE(targets.unavailableReason(QStringLiteral("bandstack.panel")), kBandStackReason);
        QCOMPARE(targets.unavailableReason(QStringLiteral("tune.up")), QString());

        w.press("voice.msg1");
        QVERIFY(toastShown(w.window, kVoiceReason));
        w.press("bandstack.next");
        QVERIFY(toastShown(w.window, kBandStackReason));

        QCOMPARE(frequency.count(), 0);
        QCOMPARE(mode.count(), 0);
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
