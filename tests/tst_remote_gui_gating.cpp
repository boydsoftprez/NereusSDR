// no-port-check: NereusSDR-original unit-test file. No Thetis logic is
// ported here; this exercises NereusSDR's own remote-station GUI gate
// (remote-daemon R2 Task 20).
// =================================================================
// tests/tst_remote_gui_gating.cpp  (NereusSDR)
// =================================================================
//
// Remote-daemon R2 Task 20 -- the remote-mode GUI gate.
//
// A Role::Remote RadioModel drives a radio that is in another process.
// Everything in src/gui that reaches for this process's RadioConnection,
// WdspEngine, AudioEngine or ReceiverManager is therefore reaching for
// something that either does not exist or exists and does nothing. There
// are roughly 77 such call sites across 16 files (the enumeration is in
// .superpowers/sdd/2026-08-03-remote-daemon-r2-plan/task-20-report.md),
// which is far too many to gate one at a time and keep gated.
//
// Two of the four accessors are why this file exists at all. connection()
// is nullptr on a remote model, so an unguarded caller crashes and any
// test that merely runs the path finds it. audioEngine(),
// receiverManager() and wdspEngine() are constructed unconditionally
// (RadioModel's constructor initializer list), so on a remote model they
// hand back a REAL BUT INERT object. The caller's writes land nowhere,
// the connects it makes never fire, and nothing is observably wrong until
// an operator notices a control that does not work. A null-dereference
// assertion cannot catch that shape. RadioModel's hand-out audit
// (localDspHandOutCount / localDspHandOutNames), armed for Role::Remote
// only, is what catches it, and this file is what asserts on the audit.
//
// MainWindow is NOT constructed here and cannot be: it boots WDSP, the
// audio engine and the discovery thread. Three existing test banners say
// so (tst_notch_hit_test.cpp, tst_mainwindow_status_bar_safety.cpp,
// tst_pan_active_slice_sync.cpp). Its gating entry points are pinned by
// name off MainWindow::staticMetaObject instead -- the same seam
// tst_notch_hit_test.cpp uses for the notch fan-out slots -- and the
// behaviour behind them is exercised through SetupDialog, which CAN be
// stood up against a Role::Remote model.
//
// That stopped being true with R-R3-38. GuiSessionCoordinator builds each
// MainWindow with ConnectionStartup::Deferred, so nothing is dialled or
// scanned until asked, and the Tools menu test entries case at the bottom
// of this file drives real windows through it.
//
//   2026-09-25: iPhone app plan Task 39 run: General > Options gates three
//               controls now, Task 38's Time Out Timers group the third.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 -- New test file for remote-daemon R2 Task 20. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-08-08 -- Fix round 2: the Setup gate's production entry point
//                 (Important 2) and the local-sweep sabotage detector
//                 (Minor 1). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-08-09 -- Fix round 4: the isConnected()-no-longer-implies-a-
//                 connection precondition (Critical), the TUNE refusal
//                 and its mirrored-state leak (Important), and the MOX
//                 button's missing follow of a refusal. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-22 -- The Tools menu's two developer test entries are
//                 disabled in a remote session (R-R3-21, R-R3-25),
//                 checked through real windows. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-22 -- R-R3-36 Task 7: the local keying case selects the radio
//                 mic, since PC-mic keying now waits for a ready
//                 microphone. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 -- R-R3-23 / R-R3-36: the Setup sweep reads each page's
//                 scope and is proved against a deliberately wrong
//                 ThisComputer page; Audio > Devices and TX Input work in a
//                 remote window (TX Input gates only the controls held for
//                 the radio); a remote window runs no VAX first-run check;
//                 and (R-R3-16) a local window still opens Connections when
//                 its radio drops. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 -- R-R3-21 control inventory: DSP > CFC and Test >
//                 Two-Tone IMD follow the transmit permission, a page the
//                 local-DSP gate disables now says why, and the controls
//                 the inventory found acting on this computer's own radio
//                 connection, amplifier socket or VAX buses are disabled
//                 with a plain reason in a remote session. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 -- R-R3-46 / R-R3-10: with the Core's radio known, the PA
//                 pages are shown and follow the transmit permission.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 -- R-R3-46 / R-R3-21: the attenuator rows show and write
//                 the Core's `stepAtt` object while the Core takes the
//                 window's edits, and give the object's plain reason
//                 otherwise. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 -- R3 receiver audio plan, Task 4 (R-R3-42): Audio > TCI
//                 and TCI Server work in a remote window. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 -- R3 receiver audio plan, Task 5 (R-R3-44): the VAX rows
//                 move to enabled. The VAX applet, the flag's VAX selector,
//                 the overlay's VAX combo, Audio > VAX (now ThisComputer)
//                 and Audio > Advanced work in a remote window; Send IQ to
//                 VAX and the VAX TX row keep a plain reason. No real page
//                 reaches local DSP any more, so the gate's cases use a
//                 probe page that does. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 -- R-R3-49: the page-sweep floors drop by the leaves not
//                 registered while their features are not built. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24 -- R-R3-49: the Options page gates the Network Watchdog with
//                 the Region (both the Core's). J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13): a
//                receive-only Core's transmit controls carry the Core's own
//                reason. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-24 -- R-R3-49 (parity Task 1): DSP > Options' TX combos, the TX
//                 applet's RF Power and TX filter and the RX applet's
//                 Shift-click follow the transmit settings gate, not the
//                 keying gate. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 -- R-R3-49 (parity Task 2): the TX applet's Tune Power, LEV
//                 and the Phone/CW applet's PROC follow the transmit
//                 settings gate in a real remote window; the container MON
//                 button toggles the Core's MON off the air and greys on it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): TX Input's
//                                    Mic Gain and radio microphone groups,
//                                    the RADE applet and the TX profile
//                                    combos follow the transmit settings
//                                    gate. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25 -- R-R3-49, R-R3-21, R-R3-44 (parity Task 11): the RX
//                 applet's XIT row writes without remote transmit, the
//                 container Antenna box's TX buttons write the Core's
//                 transmit slice with no toast, and the VAX first-run check
//                 runs in a remote window as in a local one. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 -- Receiver and transmit gaps plan, Task 16: Receive Only is
//                 the Core's too, so the Options page gates three controls.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 -- Remote-window parity Task 16: the high-resolution filter
//                 box is disabled only on a Core that does not send its
//                 curve, with that reason. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Remote parity on the air
//                                    (transmitSettingsVersion 13): the
//                                    transmit settings stay live keyed;
//                                    Region still waits. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The local RX buffer size lock follows
//                                    TUNE and the two-tone test too. AI-
//                                    assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "TestFunctionGroups.h"

#include "OperatorWording.h"
#include "gui/RemoteAudioStatus.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QHostAddress>
#include <QLabel>
#include <QGroupBox>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSlider>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QAbstractButton>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWebSocketServer>
#include <QWidget>

#include <chrono>
#include <functional>
#include <memory>
#include <type_traits>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/AudioEngine.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorFacade.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspTypes.h"
#include "core/session/RemoteStationOptions.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "gui/ConnectionPanel.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/SpectrumWidget.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "models/Band.h"
#include "gui/setup/PaSetupPages.h"
#include "core/session/IStationLink.h"
#include <QDoubleSpinBox>
#include <QCheckBox>
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/widgets/StatusToast.h"
#include "gui/MainWindow.h"
#include "gui/RemoteMediaController.h"
#include "gui/OperatorReasonText.h"
#include "gui/SetupDialog.h"
#include "gui/StationStartupSelection.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/applets/AmpApplet.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/RadeApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/VaxApplet.h"
#include "gui/setup/AudioAdvancedPage.h"
#include "gui/HGauge.h"
#include "gui/VaxFirstRunDialog.h"
#include "gui/setup/AudioDevicesPage.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/setup/DeviceCard.h"
#include "gui/setup/DspOptionsPage.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/Hl2OptionsTab.h"
#include "gui/setup/TransmitSetupPages.h"
#include "gui/setup/hardware/AntennaAlexAntennaControlTab.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "models/TransmitModel.h"
#include "gui/widgets/MeterSlider.h"
#include "gui/widgets/VaxChannelSelector.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {

// The AppSettings keys the Setup field group (Setup -> CAT & Network ->
// Remote Station) binds. Spelled here rather than #included from the page
// on purpose: this test is the thing that pins them as OperatorLocal, and
// a rename that silently moved them would otherwise rename the assertion
// with them and prove nothing.
const QString kStationUrlKey         = QStringLiteral("RemoteStationUrl");
const QString kStationTokenKey       = QStringLiteral("RemoteStationToken");
const QString kStationFingerprintKey = QStringLiteral("RemoteStationFingerprint");
const QString kStationAllowUnpinnedKey =
    QStringLiteral("RemoteStationAllowUnpinned");

// An ISettingsBackend that is NOT a SettingsProxy. Exists to prove the
// Setup gate's cross-cast has no opinion about backends it does not
// recognise, rather than refusing whenever any backend is installed.
class StubBackend : public ISettingsBackend {
public:
    bool handlesKey(const QString&) const override { return false; }
    QVariant value(const QString&, const QVariant& def) const override { return def; }
    void setValue(const QString&, const QVariant&) override {}
    bool contains(const QString&) const override { return false; }
    void remove(const QString&) override {}
    QStringList handledKeys() const override { return {}; }
};

// R-R3-21 / R-R3-25: the Tools menu's two developer test entries, and the
// TX Equalizer entry whose remote tooltip they are required to share.
const QString kTestToastActionName   = QStringLiteral("toolsTestAntennaSwitchToast");
const QString kTestReRouteActionName = QStringLiteral("toolsTestTxBoundReRoute");
const QString kTxEqualizerActionName = QStringLiteral("toolsTxEqualizer");

// MainWindow answers antennaAutoSwitched with an AntennaSwitchToast tool
// window and txBoundReRouteRequested with a modal TxBoundConfirmDialog,
// whose exec() would block the test. Neither is what the test entries
// case asserts: it asks whether an entry reaches RadioModel at all, and
// watches RadioModel's own signals for that. So the window's two
// consumers are detached first. False means one of them was not there to
// detach, i.e. the wiring moved and the case needs another look.
bool detachTestSurfaceConsumers(MainWindow* window)
{
    RadioModel* const model = window->radioModel();
    const bool toast = QObject::disconnect(
        model, &RadioModel::antennaAutoSwitched, window, nullptr);
    const bool reRoute = QObject::disconnect(
        model, &RadioModel::txBoundReRouteRequested, window, nullptr);
    return toast && reRoute;
}

// The Setup tree leaf registered under `label` (first match), for its
// tooltip. Setup's leaves are the second level of the tree.
QTreeWidgetItem* setupLeaf(SetupDialog& dialog, const QString& label)
{
    auto* tree = dialog.findChild<QTreeWidget*>();
    if (tree == nullptr) { return nullptr; }
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->parent() != nullptr && (*it)->text(0) == label) {
            return *it;
        }
    }
    return nullptr;
}

// R-R3-23: realizes every registered Setup leaf against `model` (a
// Role::Remote one) and returns the leaves that break the gate's rule:
//   - a Core or Mixed page that reached this process's DSP and was left
//     enabled (the rule since R2 Task 20);
//   - a ThisComputer page that reached this process's DSP at all, or came
//     up disabled. Such a page is supposed to work in a remote window, so
//     either is a bug in the page, even though the gate disables it.
// Iterates by INDEX: two leaves share the label "Options". `reachedCount`
// counts the pages that reached local DSP while being realized here.
QStringList remoteSetupSweepOffenders(RadioModel& model, SetupDialog& dialog,
                                      int* reachedCount = nullptr)
{
    QStringList offenders;
    int reached = 0;
    const QStringList labels = dialog.pageLabelsForTest();
    const int pageCount = dialog.registeredPageCountForTest();
    for (int i = 0; i < pageCount; ++i) {
        const int before = model.localDspHandOutCount();
        QWidget* page = dialog.realizePageAtForTest(i);
        if (page == nullptr) {
            continue;  // a factory that yields nothing has nothing to gate
        }
        const bool reachedLocalDsp = model.localDspHandOutCount() > before;
        if (reachedLocalDsp) {
            ++reached;
        }
        if (dialog.pageScopeAtForTest(i) == SetupScope::ThisComputer) {
            if (reachedLocalDsp || !page->isEnabled()) {
                offenders << labels.at(i);
            }
        } else if (reachedLocalDsp && page->isEnabled()) {
            offenders << labels.at(i);
        }
    }
    if (reachedCount != nullptr) {
        *reachedCount = reached;
    }
    return offenders;
}

// The Devices page's card titled `title`.
DeviceCard* deviceCardOf(QWidget* page, const QString& title)
{
    for (DeviceCard* card : page->findChildren<DeviceCard*>()) {
        if (card->title() == title) {
            return card;
        }
    }
    return nullptr;
}

// The microphone card's buffer-size combo (its items start at 64 samples;
// no other card combo's do).
QComboBox* deviceCardBufferCombo(DeviceCard* card)
{
    for (QComboBox* combo : card->findChildren<QComboBox*>()) {
        if (combo->count() > 1 && combo->itemData(0).toInt() == 64) {
            return combo;
        }
    }
    return nullptr;
}

// The value of every spin box, combo box and check box under `root`, in
// child order: a before/after fingerprint for "activation moved nothing".
QString widgetStateOf(QWidget* root)
{
    QStringList state;
    for (QWidget* w : root->findChildren<QWidget*>()) {
        if (auto* spin = qobject_cast<QSpinBox*>(w)) {
            state << QString::number(spin->value());
        } else if (auto* combo = qobject_cast<QComboBox*>(w)) {
            state << QString::number(combo->currentIndex());
        } else if (auto* button = qobject_cast<QAbstractButton*>(w); button && button->isCheckable()) {
            state << (button->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
        }
    }
    return state.join(QLatin1Char(','));
}

// R-R3-21: the Core settings reason MainWindow pushes to a disconnected
// remote window's Setup dialog.
const QString kStationReason = QStringLiteral("Connect to the Core to change these.");

// Shows the leaf at a registry index (labels are not unique: "Options"
// is both General and DSP) the way the operator does, by selecting it in
// the tree, and returns the page now on screen.
QWidget* showSetupLeafAt(SetupDialog& dialog, int entryIndex)
{
    auto* tree = dialog.findChild<QTreeWidget*>();
    auto* stack = dialog.findChild<QStackedWidget*>();
    if (tree == nullptr || stack == nullptr) { return nullptr; }
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toInt() == entryIndex) {
            tree->setCurrentItem(*it);
            return stack->currentWidget();
        }
    }
    return nullptr;
}

QTreeWidgetItem* setupLeafAt(SetupDialog& dialog, int entryIndex)
{
    auto* tree = dialog.findChild<QTreeWidget*>();
    if (tree == nullptr) { return nullptr; }
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toInt() == entryIndex) { return *it; }
    }
    return nullptr;
}

// The controls on `page` that carry `reason` as their accessible
// description: the ones a gate disabled with that reason.
QList<QWidget*> controlsGatedWith(QWidget* page, const QString& reason)
{
    QList<QWidget*> gated;
    for (QWidget* w : page->findChildren<QWidget*>()) {
        if (w->accessibleDescription() == reason) { gated << w; }
    }
    return gated;
}

// R3 Setup fix wave: the reason shown while connected to a Core that has
// not sent its settings (MainWindow::stationSettingsReason()).
const QString kCoreSettingsMissingReason = QStringLiteral("The Core has not sent its settings.");

QPushButton* buttonWithText(QWidget* page, const QString& text)
{
    for (QPushButton* button : page->findChildren<QPushButton*>()) {
        if (button->text() == text) { return button; }
    }
    return nullptr;
}

// A connected remote dialog: the Core's settings (seed marker plus
// `settings`) have arrived, the session is ready and MainWindow's push
// has made them available.
void connectDialog(SettingsProxy& proxy, SetupDialog& dialog,
                   const QMap<QString, QString>& settings = {})
{
    QMap<QString, QString> snapshot = settings;
    snapshot.insert(QLatin1String(AppSettings::kDaemonProfileSeededKey), QStringLiteral("True"));
    proxy.applySnapshot(snapshot);
    proxy.setReady(true);
    dialog.setStationSettingsAvailable(true, kStationReason);
    QCoreApplication::processEvents();
}

// Settings Validation is available only after the Core advertises the
// hygiene capability and this computer signs in with its paired key.
class PairedHygieneLink final : public IStationLink {
public:
    int requests{0};
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return true; }
    bool settingsHygieneAvailable() const override { return true; }
    bool signedInWithDeviceKey() const override { return true; }
    CommandOutcome requestSettingsHygiene(const QByteArray&, const QString&) override
    {
        ++requests;
        return {true, {}};
    }
};

void bindHygieneRadio(RadioModel& model)
{
    RadioInfo radio;
    radio.macAddress = QStringLiteral("AA:BB:CC:DD:EE:FF");
    model.setLastRadioInfoForTest(radio);
    model.setConnectionStateForTest(ConnectionState::Connected);
}

} // namespace

class TstRemoteGuiGating : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Before the first AppSettings::instance() call, so the singleton
        // resolves to this run's own file. The test entries case builds
        // MainWindows, which save; parallel ctest jobs must not share a
        // settings file (the same arrangement as tst_gui_session_coordinator).
        AppSettings::setProfileOverride(QStringLiteral("remote-gui-gating-%1")
                                            .arg(QCoreApplication::applicationPid()));
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ====================================================================
    // Step 3: one capability authority, and the audit that catches the
    // three accessors a crash cannot.
    // ====================================================================

    void localModelOwnsLocalDsp()
    {
        RadioModel model;
        QVERIFY(model.ownsLocalDsp());
    }

    void remoteModelDoesNotOwnLocalDsp()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(!model.ownsLocalDsp());
    }

    // The headline assertion of step 3. Each of the three accessors must
    // hand back a NON-NULL object (that is the whole problem) and must be
    // recorded doing it, by name.
    void remoteModelRecordsEveryLiveLocalDspHandOut()
    {
        RadioModel model(RadioModel::Role::Remote);
        QCOMPARE(model.localDspHandOutCount(), 0);

        QVERIFY2(model.audioEngine() != nullptr,
                 "audioEngine() is expected to be non-null even on a remote "
                 "model; if this ever becomes null the silent-failure premise "
                 "of this whole gate changed and the gate should be revisited");
        QVERIFY(model.receiverManager() != nullptr);
        QVERIFY(model.wdspEngine() != nullptr);

        QCOMPARE(model.localDspHandOutCount(), 3);
        const QSet<QByteArray> names = model.localDspHandOutNames();
        QVERIFY(names.contains(QByteArrayLiteral("audioEngine")));
        QVERIFY(names.contains(QByteArrayLiteral("receiverManager")));
        QVERIFY(names.contains(QByteArrayLiteral("wdspEngine")));
    }

    // connection() is the one that CAN be caught by a crash, and it must
    // not be counted: counting it would make every correctly-null-guarded
    // call site look like a leak and drown the real ones.
    void remoteModelConnectionIsNullAndUncounted()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(model.connection() == nullptr);
        QCOMPARE(model.localDspHandOutCount(), 0);
    }

    // The audit must be inert for local direct mode, which is the mode
    // every existing user is in. If it armed there, the SetupDialog gate
    // below would start disabling pages on a local radio.
    void localModelNeverArmsTheAudit()
    {
        RadioModel model;
        for (int i = 0; i < 5; ++i) {
            (void)model.audioEngine();
            (void)model.receiverManager();
            (void)model.wdspEngine();
        }
        QCOMPARE(model.localDspHandOutCount(), 0);
        QVERIFY(model.localDspHandOutNames().isEmpty());
    }

    // The audit's KNOWN BLIND SPOT, asserted rather than only described.
    //
    // rxChannelForSlice() forwards to WdspEngine::rxChannel(), which on a
    // channel-less remote engine returns nullptr. Every call site guards
    // with `if (RxChannel* ch = ...)`, so the null is swallowed and the
    // control silently does nothing -- the same shape as the three counted
    // accessors, reached through a null instead of an inert object.
    //
    // It is deliberately NOT counted: two of its seven src/gui call sites
    // run at page-construction time (DspOptionsPage::buildUI and
    // MnfSetupPage's constructor), so counting it would disable DSP >
    // Options and MNF, two pages that exist mainly to edit Station-scoped
    // settings. Pinning the decision here means a future change that routes
    // the wrapper has to come through this test and say so on purpose.
    // Fix round 1, Important 1.
    void rxChannelForSliceIsSilentOnRemoteAndDeliberatelyUncounted()
    {
        RadioModel model(RadioModel::Role::Remote);

        QVERIFY2(model.rxChannelForSlice(0) == nullptr,
                 "a remote model's WdspEngine has no channels, so this must "
                 "resolve to nullptr rather than a usable channel");
        QCOMPARE(model.localDspHandOutCount(), 0);
        QVERIFY(model.localDspHandOutNames().isEmpty());
    }

    void auditResetClearsCountAndNames()
    {
        RadioModel model(RadioModel::Role::Remote);
        (void)model.audioEngine();
        QCOMPARE(model.localDspHandOutCount(), 1);
        model.resetLocalDspHandOutAudit();
        QCOMPARE(model.localDspHandOutCount(), 0);
        QVERIFY(model.localDspHandOutNames().isEmpty());
    }

    // ====================================================================
    // Step 1a: every registered Setup page, realized against a
    // Role::Remote model.
    // ====================================================================

    // The crash sweep. Realizing all 55-odd leaves against a remote model
    // must not dereference the null connection(). This is the assertion
    // that catches the connection() third of the enumeration.
    void everySetupPageRealizesAgainstARemoteModel()
    {
        RadioModel model(RadioModel::Role::Remote);
        SetupDialog dialog(&model);

        QVERIFY2(dialog.registeredPageCountForTest() > 0,
                 "buildTree() must register at least one navigation leaf");
        dialog.realizeAllPagesForTest();
        QCOMPARE(dialog.realizedPageCountForTest(),
                 dialog.registeredPageCountForTest());
    }

    // The invariant that IS the gate: on a remote model, a page either
    // reached for no live local-DSP object, or it is disabled. Stated as
    // an invariant over the whole tree rather than as a list of page
    // names, so a leaf added tomorrow is covered on the day it lands.
    void everyRemoteSetupPageIsEitherLocalDspFreeOrDisabled()
    {
        RadioModel model(RadioModel::Role::Remote);
        SetupDialog dialog(&model);
        // R-R3-44: since Audio > VAX and Advanced reach this computer's
        // engine through localAudioDevices(), no real page reaches local
        // DSP. A Core page that does keeps the gate exercised: it must come
        // up disabled and is not an offender.
        const QString probeLabel = QStringLiteral("Probe Core page reaching local DSP");
        dialog.registerPageForTest(probeLabel, SetupScope::Core, [&model]() -> QWidget* {
            (void)model.wdspEngine();
            return new QWidget;
        });

        // Iterate by INDEX, not by label. Two leaves are registered as
        // "Options" (General and DSP), and pageEntryIndex() returns the
        // first match, so a label-driven loop realizes the General one
        // twice and never builds the DSP one -- while still reporting a
        // clean sweep, which is the worst possible failure for a test
        // whose whole job is coverage. Fix round 1, Minor 2.
        //
        // R-R3-23: scope-aware. A ThisComputer page (Audio > Devices, the
        // General placeholders, Remote Station, ...) must neither reach
        // local DSP nor come up disabled; see remoteSetupSweepOffenders.
        const int pageCount = dialog.registeredPageCountForTest();
        QVERIFY(pageCount > 0);
        QCOMPARE(dialog.pageLabelsForTest().size(), pageCount);

        int reachedCount = 0;
        const QStringList offenders = remoteSetupSweepOffenders(model, dialog, &reachedCount);

        // Minor 2's fix, pinned: the sweep must have BUILT every leaf. With
        // the previous label-driven loop this read pageCount - 1, because
        // the second "Options" resolved back to the first and its factory
        // never ran -- and the test still reported a clean sweep.
        QCOMPARE(dialog.realizedPageCountForTest(), pageCount);

        QVERIFY2(offenders.isEmpty(),
                 qPrintable(QStringLiteral(
                     "%1 Setup page(s) broke the remote gate: a Core or Mixed "
                     "page reached this process's DSP and stayed enabled, or a "
                     "ThisComputer page reached it or came up disabled: %2")
                                .arg(offenders.size())
                                .arg(offenders.join(QStringLiteral(", ")))));

        // Non-vacuity. If nothing in the tree reaches local DSP any more,
        // the loop above proved nothing and this test is a no-op that
        // would keep passing after the gate was deleted.
        QVERIFY2(reachedCount > 0,
                 "no Setup page reached local DSP at all, so the gate above "
                 "was never exercised -- either the enumeration changed or "
                 "the audit stopped arming");
        // Only the probe: every real page works in a remote window or is
        // declared unavailable by name.
        QCOMPARE(reachedCount, 1);
        QVERIFY(!dialog.realizedPageForTest(probeLabel)->isEnabled());

        // And the scopes the plan fixes (R-R3-23).
        const QStringList labels = dialog.pageLabelsForTest();
        const auto scopeOf = [&](const QString& label) {
            return dialog.pageScopeAtForTest(static_cast<int>(labels.indexOf(label)));
        };
        QCOMPARE(scopeOf(QStringLiteral("Devices")), SetupScope::ThisComputer);
        QCOMPARE(scopeOf(QStringLiteral("TX Input")), SetupScope::Mixed);
        QCOMPARE(scopeOf(QStringLiteral("Advanced")), SetupScope::Mixed);
        // R-R3-44: this computer's VAX channels.
        QCOMPARE(scopeOf(QStringLiteral("VAX")), SetupScope::ThisComputer);
        // R-R3-42: this computer's TCI server.
        QCOMPARE(scopeOf(QStringLiteral("TCI")), SetupScope::ThisComputer);
        QCOMPARE(scopeOf(QStringLiteral("TCI Server")), SetupScope::ThisComputer);
    }

    // R-R3-23: the sweep catches a ThisComputer page that reaches an
    // audited accessor. Proved with a page that does so on purpose: it is
    // disabled, the critical log names the accessor, and the sweep lists
    // it. A Core page doing the same is disabled with no critical log and
    // is not an offender (the gate working as designed).
    void aThisComputerPageThatReachesLocalDspFailsTheSweep()
    {
        RadioModel model(RadioModel::Role::Remote);
        SetupDialog dialog(&model);
        const QString wrongLabel = QStringLiteral("Deliberately wrong page");
        const QString coreLabel = QStringLiteral("Deliberately gated Core page");
        const int wrong = dialog.registerPageForTest(
            wrongLabel, SetupScope::ThisComputer, [&model]() -> QWidget* {
                (void)model.wdspEngine();
                return new QWidget;
            });
        const int core = dialog.registerPageForTest(
            coreLabel, SetupScope::Core, [&model]() -> QWidget* {
                (void)model.receiverManager();
                return new QWidget;
            });

        // Realized first, on its own, so the log can name exactly the one
        // accessor it reached.
        QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(
            QStringLiteral("^Setup page \"?%1\"? is declared ThisComputer but "
                           "reached wdspEngine on a remote-station model")
                .arg(QRegularExpression::escape(wrongLabel))));
        QWidget* const wrongPage = dialog.realizePageAtForTest(wrong);
        QVERIFY(wrongPage != nullptr);
        QVERIFY(!wrongPage->isEnabled());

        // No critical line for the Core page: captured around its
        // realization, then the previous handler is put back.
        static QStringList criticals;
        criticals.clear();
        static QtMessageHandler previous = nullptr;
        previous = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
                if (type == QtCriticalMsg) {
                    criticals << msg;
                    return;
                }
                if (previous != nullptr) {
                    previous(type, context, msg);
                }
            });
        QWidget* const corePage = dialog.realizePageAtForTest(core);
        qInstallMessageHandler(previous);
        QVERIFY(corePage != nullptr);
        QVERIFY(!corePage->isEnabled());
        QVERIFY2(criticals.isEmpty(), qPrintable(criticals.join(QLatin1Char('\n'))));

        const QStringList offenders = remoteSetupSweepOffenders(model, dialog);
        QCOMPARE(offenders, QStringList{wrongLabel});
    }

    // Local direct mode is the regression risk. The same sweep against a
    // Role::Local model must leave every page enabled.
    void theSameSweepAgainstALocalModelDisablesNothing()
    {
        RadioModel model;
        // A radio with power amplifier settings (an ANAN-G2): with no radio
        // the PA pages are disabled with the reason (Task 16 fix wave 2),
        // which is the radio's doing, not local mode's.
        model.setBoardForTest(HPSDRHW::Saturn);
        SetupDialog dialog(&model);

        // By index, for the same reason as the remote twin above: the
        // duplicate "Options" label would otherwise leave DSP > Options
        // unvisited in the local-mode regression guard too.
        QStringList disabled;
        const QStringList labels = dialog.pageLabelsForTest();
        const int pageCount = dialog.registeredPageCountForTest();
        for (int i = 0; i < pageCount; ++i) {
            QWidget* page = dialog.realizePageAtForTest(i);
            if (page != nullptr && !page->isEnabled()) {
                disabled << labels.at(i);
            }
        }

        // The same sabotage detector its remote-mode twin carries at the
        // top of this file. Without it, a revert to label-driven iteration
        // here leaves DSP > Options unvisited and this regression guard
        // reports local mode clean while never having built the one page
        // most likely to break it. Fix round 2, Minor 1.
        QCOMPARE(dialog.realizedPageCountForTest(), pageCount);

        QVERIFY2(disabled.isEmpty(),
                 qPrintable(QStringLiteral(
                     "local direct mode regressed: %1 Setup page(s) came up "
                     "disabled: %2")
                                .arg(disabled.size())
                                .arg(disabled.join(QStringLiteral(", ")))));
    }

    // ====================================================================
    // Step 1b: MainWindow's gating entry points, pinned by name.
    // ====================================================================

    // MainWindow cannot be constructed in a unit test (WDSP, audio engine,
    // discovery thread), so the gate's entry points are resolved off the
    // meta-object instead. A rename that stranded the gate -- leaving a
    // remote client with a live Connect action and a live Network
    // Diagnostics dialog reading a null connection -- would otherwise
    // reach a release silently.
    //
    // These must be SLOTS, not plain methods, because that is what makes
    // them resolvable here at all.
    void mainWindowExposesTheRemoteGatingSlots()
    {
        const QMetaObject& mo = MainWindow::staticMetaObject;
        for (const char* sig : {"applyRemoteRoleGating()",
                                "showConnectionPanel()",
                                "openNetworkDiagnostics()"}) {
            QVERIFY2(mo.indexOfSlot(sig) >= 0,
                     qPrintable(QStringLiteral("MainWindow::%1 is not an "
                                               "invokable slot; the remote-mode "
                                               "gate would be unreachable")
                                    .arg(QLatin1String(sig))));
        }
    }

    // ====================================================================
    // Fix round 2, Important 2: the Setup gate, and the fact that it is
    // now hung on something.
    // ====================================================================

    // MainWindow::createSetupDialog() is the only place in src/gui that
    // runs `new SetupDialog`; all twelve former call sites go through it.
    // MainWindow cannot be constructed here, so the name is pinned off the
    // meta-object, the same seam the gating slots above use. A rename or a
    // demotion to a plain method would strand the test that proves the gate
    // exists at all, and a thirteenth site constructing the dialog inline
    // would slip past a gate nobody was asserting on.
    void mainWindowRoutesSetupDialogThroughOneGatedFactory()
    {
        const QMetaObject& mo = MainWindow::staticMetaObject;
        QVERIFY2(mo.indexOfSlot("createSetupDialog()") >= 0,
                 "MainWindow::createSetupDialog() is not an invokable slot; "
                 "the Setup gate would be unreachable and unpinnable");
    }

    // Local direct mode, which is every existing user. AppSettings holds no
    // remote backend there, so the gate must not have an opinion.
    void setupGateIsOpenWhenNoRemoteBackendIsInstalled()
    {
        QVERIFY(AppSettings::instance().remoteBackend() == nullptr);
        QVERIFY2(setupDialogAllowedForCurrentBackend(),
                 "local direct mode has no SettingsProxy, so the gate must "
                 "open unconditionally; refusing here would take Setup away "
                 "from every non-remote user");
    }

    // A backend that is not a SettingsProxy is not this gate's business
    // either. The cross-cast yields nullptr and the gate opens.
    void setupGateIsOpenBehindAnUnrecognisedBackend()
    {
        StubBackend stub;
        AppSettings::instance().setRemoteBackend(&stub);
        QVERIFY(setupDialogAllowedForCurrentBackend());
        AppSettings::instance().setRemoteBackend(nullptr);
    }

    // The states that matter, asked through the production entry point
    // rather than through the method directly: this is what
    // MainWindow::createSetupDialog() actually calls, so a regression in
    // the cross-cast or the delegation shows up here and not only in
    // tst_settings_proxy's method-level coverage.
    void setupGateFollowsTheInstalledProxyThroughItsStates()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);

        // Pre-handshake. This is the window in which 187 widget
        // constructors would otherwise read their ship defaults and start
        // writing them into the STATION store on first touch.
        QVERIFY2(!setupDialogAllowedForCurrentBackend(),
                 "the gate must be shut before the handshake completes");

        // Ready, but nothing has arrived: a freshly reserved daemon
        // profile looks exactly like this, which is why ready() alone was
        // never sufficient.
        proxy.setReady(true);
        QVERIFY2(!setupDialogAllowedForCurrentBackend(),
                 "ready() alone must not open the gate");

        // Ready and empty is still shut.
        proxy.applySnapshot(QMap<QString, QString>{});
        QVERIFY(!setupDialogAllowedForCurrentBackend());

        // The seed marker alone opens it: that is what tells "empty
        // because the daemon profile is fresh" apart from "empty because
        // something is broken".
        QMap<QString, QString> seeded;
        seeded.insert(QLatin1String(AppSettings::kDaemonProfileSeededKey),
                      QStringLiteral("True"));
        proxy.applySnapshot(seeded);
        QVERIFY2(setupDialogAllowedForCurrentBackend(),
                 "a legitimately fresh daemon profile must not be locked out "
                 "of Setup, or a remote operator can never configure one");

        // And so does real station content.
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("Slice0/Locked"), QStringLiteral("True")}});
        QVERIFY(setupDialogAllowedForCurrentBackend());

        AppSettings::instance().setRemoteBackend(nullptr);
    }

    // ====================================================================
    // Step 4: MOX refuses on a remote model, through the EXISTING
    // pre-check, so the refusal reaches the status-bar toast.
    // ====================================================================

    // Setup description version 22's desktop half: DSP > Options' Buffer
    // Size (IQcomp) group locks while the radio is on the air, as Thetis
    // greys grpDSPBufferSize while MOX is on (setup.cs:5159 [v2.10.3.15]).
    // A remote window follows its Core's air state; the rows stay shown,
    // disabled with their reason, and the filter rows never lock.
    void remoteDspBufferSizesLockWhileTheCoreIsOnTheAir()
    {
        RadioModel model(RadioModel::Role::Remote);
        DspOptionsPage page(&model);
        const auto combo = [&page](const char* name) {
            auto* box = page.findChild<QComboBox*>(QLatin1String(name));
            return box;
        };
        const QList<QComboBox*> rx{combo("DspOptionsBufferSizePhoneRx"),
                                   combo("DspOptionsBufferSizeFmRx"),
                                   combo("DspOptionsBufferSizeCwRx"),
                                   combo("DspOptionsBufferSizeDigRx")};
        const QList<QComboBox*> txBuffer{combo("DspOptionsBufferSizePhoneTx"),
                                         combo("DspOptionsBufferSizeFmTx"),
                                         combo("DspOptionsBufferSizeDigTx")};
        QComboBox* txFilter = combo("DspOptionsFilterSizePhoneTx");
        QComboBox* rxFilter = combo("DspOptionsFilterSizePhoneRx");
        for (QComboBox* box : rx + txBuffer) { QVERIFY(box != nullptr); }
        QVERIFY(txFilter && rxFilter);
        const QString ownTip = rx.first()->toolTip();
        const QString locked = RadioModel::dspBufferOnAirLockedReason();
        QCOMPARE(locked, QStringLiteral("Can't change while transmitting."));

        // Off the air: the RX rows are live; the TX rows wait for transmit
        // settings, as before.
        for (QComboBox* box : rx) { QVERIFY(box->isEnabled()); }
        page.setTransmitSettingsPermitted(true, QString());
        for (QComboBox* box : txBuffer) { QVERIFY(box->isEnabled()); }
        QVERIFY(txFilter->isEnabled());

        QVERIFY(model.applyMirroredValue("transmitting", QVariant(true)).isEmpty());
        QVERIFY(model.isCoreOnAir());
        for (QComboBox* box : rx + txBuffer) {
            QVERIFY2(!box->isEnabled(), qPrintable(box->objectName()));
            QVERIFY2(!box->isHidden(), qPrintable(box->objectName())); // shown, never hidden
            QCOMPARE(box->toolTip(), locked);
            QCOMPARE(box->accessibleDescription(), locked);
        }
        QVERIFY(txFilter->isEnabled());
        QVERIFY(rxFilter->isEnabled());

        // Transmit settings withdrawn on the air: the TX buffer rows give
        // that reason instead, and keep it once the radio is back on receive.
        page.setTransmitSettingsPermitted(false, QStringLiteral("No transmit settings here."));
        for (QComboBox* box : txBuffer) {
            QCOMPARE(box->toolTip(), QStringLiteral("No transmit settings here."));
        }
        QVERIFY(model.applyMirroredValue("transmitting", QVariant(false)).isEmpty());
        QVERIFY(!model.isCoreOnAir());
        for (QComboBox* box : rx) {
            QVERIFY2(box->isEnabled(), qPrintable(box->objectName()));
            QCOMPARE(box->toolTip(), ownTip);
        }
        for (QComboBox* box : txBuffer) {
            QVERIFY(!box->isEnabled());
            QCOMPARE(box->toolTip(), QStringLiteral("No transmit settings here."));
        }
        page.setTransmitSettingsPermitted(true, QString());
        for (QComboBox* box : txBuffer) {
            QVERIFY(box->isEnabled());
            QCOMPARE(box->toolTip(), ownTip);
        }
    }

    // The same lock on a local window, following its own MOX.
    void localDspBufferSizesLockWhileKeyed()
    {
        RadioModel model;
        DspOptionsPage page(&model);
        auto* phoneRx = page.findChild<QComboBox*>(QStringLiteral("DspOptionsBufferSizePhoneRx"));
        auto* phoneTx = page.findChild<QComboBox*>(QStringLiteral("DspOptionsBufferSizePhoneTx"));
        QVERIFY(phoneRx && phoneTx);
        QVERIFY(phoneRx->isEnabled() && phoneTx->isEnabled());

        MoxController* mox = model.moxController();
        QVERIFY(mox != nullptr);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox->setMoxCheck({});
        mox->setMox(true); // logical state, no radio transport
        QTRY_VERIFY(model.isCoreOnAir());
        QVERIFY(!phoneRx->isEnabled());
        QVERIFY(!phoneTx->isEnabled());
        QCOMPARE(phoneRx->toolTip(), RadioModel::dspBufferOnAirLockedReason());
        mox->setMox(false);
        QTRY_VERIFY(!model.isCoreOnAir());
        QVERIFY(phoneRx->isEnabled());
        QVERIFY(phoneTx->isEnabled());

        // TUNE, both edges (the transmit model's, state only).
        model.transmitModel().setTune(true);
        QTRY_VERIFY(model.isCoreOnAir());
        QVERIFY(!phoneRx->isEnabled());
        QCOMPARE(phoneRx->toolTip(), RadioModel::dspBufferOnAirLockedReason());
        model.transmitModel().setTune(false);
        QTRY_VERIFY(!model.isCoreOnAir());
        QVERIFY(phoneRx->isEnabled());

        // The two-tone test, both edges (no radio, no RF).
        TxChannel tx(/*channelId=*/1);
        TwoToneController* const twoTone = model.twoToneController();
        QVERIFY(twoTone != nullptr);
        twoTone->setTxChannel(&tx);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setActive(true);
        QTRY_VERIFY(twoTone->isActive());
        QTRY_VERIFY(model.isCoreOnAir());
        QVERIFY(!phoneRx->isEnabled());
        QCOMPARE(phoneRx->toolTip(), RadioModel::dspBufferOnAirLockedReason());
        twoTone->setActive(false);
        QTRY_VERIFY(!twoTone->isActive());
        QTRY_VERIFY(!model.isCoreOnAir());
        QVERIFY(phoneRx->isEnabled());
        twoTone->setTxChannel(nullptr);
    }

    void remoteModelRefusesMoxWithAnOperatorReason()
    {
        RadioModel model(RadioModel::Role::Remote);
        MoxController* mox = model.moxController();
        QVERIFY(mox != nullptr);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy rejected(mox, &MoxController::moxRejected);
        QVERIFY(rejected.isValid());

        model.setMox(true);

        QCOMPARE(rejected.count(), 1);
        const QString reason = rejected.at(0).at(0).toString();
        QVERIFY2(reason.contains(QStringLiteral("transmit"))
                     && reason.contains(QStringLiteral("Core"))
                     && !reason.contains(QStringLiteral("R4")),
                 qPrintable(QStringLiteral("refusal must explain the unavailable "
                                           "Core operation without roadmap jargon: %1")
                                .arg(reason)));
        QVERIFY(!mox->isMox());
        QVERIFY(!model.mox());
    }

    // The refusal must survive a disconnect. disconnectFromRadio() is
    // reachable on a remote client (aboutToQuit calls it unconditionally),
    // and teardownConnection() contains a setMoxCheck({}) -- an EMPTY
    // MoxCheckFn being MoxController's BYPASS, not its deny.
    //
    // Honest about what this pins: today the clear is not reached at all,
    // because teardownConnection() returns early on a null m_connection and
    // a Role::Remote model always has one. Verified by sabotage -- removing
    // the role guard on that clear leaves this case green. So what this
    // asserts is the OBSERVABLE invariant (MOX stays refused across a
    // disconnect), which is currently made true by the constructor-time
    // install rather than by the guard. It is still the assertion worth
    // having: it is stated in terms of behaviour, so it keeps holding
    // whichever of the two mechanisms is the live one.
    void remoteMoxRefusalSurvivesDisconnectFromRadio()
    {
        RadioModel model(RadioModel::Role::Remote);
        MoxController* mox = model.moxController();
        QVERIFY(mox != nullptr);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);

        model.disconnectFromRadio();

        QSignalSpy rejected(mox, &MoxController::moxRejected);
        model.setMox(true);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(!mox->isMox());
    }

    // Non-vacuity for the two above: a Role::Local model with the same
    // pre-check installed and a legal mode/frequency must still key up.
    // Without this, a gate that refused MOX unconditionally would pass
    // both tests above and break every local user.
    void localModelStillKeysUpOnALegalFrequency()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        // Key from the radio mic: with the PC mic selected the R-R3-36
        // admission would refuse (no capture is Ready here), which is not
        // what this case is about.
        model.transmitModel().setMicSource(MicSource::Radio);

        const int aId = model.addSlice();
        SliceModel* const a = model.sliceById(aId);
        QVERIFY(a != nullptr);
        a->setDspMode(DSPMode::USB);
        a->setFrequency(14'200'000.0);

        QSignalSpy rejected(model.moxController(), &MoxController::moxRejected);
        model.setMox(true);
        QCOMPARE(rejected.count(), 0);
        QVERIFY(model.moxController()->isMox());
        model.setMox(false);
    }

    // ====================================================================
    // Step 2: --station / --token, and the field group's two keys.
    // ====================================================================

    void stationUrlAcceptsWsAndWss()
    {
        QString why;
        QVERIFY(RemoteStationOptions::isValidStationUrl(
            QStringLiteral("wss://station.example:4433"), &why));
        QVERIFY(RemoteStationOptions::isValidStationUrl(
            QStringLiteral("ws://127.0.0.1:50100"), &why));
    }

    void stationUrlRejectsHttpEmptyAndHostless()
    {
        QString why;

        QVERIFY(!RemoteStationOptions::isValidStationUrl(QString(), &why));
        QVERIFY(!why.isEmpty());

        why.clear();
        QVERIFY(!RemoteStationOptions::isValidStationUrl(
            QStringLiteral("https://station.example:4433"), &why));
        QVERIFY2(why.contains(QStringLiteral("wss://")),
                 qPrintable(QStringLiteral("the reason must name the scheme the "
                                           "field wants, got: %1").arg(why)));

        why.clear();
        QVERIFY(!RemoteStationOptions::isValidStationUrl(
            QStringLiteral("wss://"), &why));
        QVERIFY(!why.isEmpty());
    }

    void emptyStationUrlMeansLocalDirectMode()
    {
        RemoteStationOptions opts;
        QVERIFY(!opts.isRemote());
        opts.url = QStringLiteral("wss://127.0.0.1:50100");
        QVERIFY(opts.isRemote());
    }

    // The station address and its token are the client's own. If either
    // ever classified Station, every GUI connected to one daemon would
    // write its address book -- and its credentials -- into the shared
    // store, and read each other's back.
    void stationFieldGroupKeysAreOperatorLocal()
    {
        QCOMPARE(classifySettingsKey(kStationUrlKey), SettingsScope::OperatorLocal);
        QCOMPARE(classifySettingsKey(kStationTokenKey), SettingsScope::OperatorLocal);

        // All four keys the field group writes, not just the two the brief
        // named. The fingerprint is per-client TRUST state: a rule that
        // classified it Station would share one client's certificate pin
        // with every other client of the same daemon, which is a downgrade
        // no operator asked for and none would see. The allow-unpinned flag
        // is worse, because it would let one bench client turn pinning off
        // for everyone. Fix round 1, Minor 6.
        QCOMPARE(classifySettingsKey(kStationFingerprintKey),
                 SettingsScope::OperatorLocal);
        QCOMPARE(classifySettingsKey(kStationAllowUnpinnedKey),
                 SettingsScope::OperatorLocal);
    }

    // The one page a remote operator must be able to reach, since it is
    // where the station address lives. If the gate above ever disabled it,
    // a mistyped station would be unrecoverable from the GUI. This is also
    // the counter-example that keeps the gate honest: it proves the sweep
    // disables SOME pages and not simply all of them.
    void theRemoteStationPageStaysUsableOnARemoteModel()
    {
        RadioModel model(RadioModel::Role::Remote);
        SetupDialog dialog(&model);

        QVERIFY2(dialog.pageLabelsForTest().contains(
                     QStringLiteral("Remote Access")),
                 "the Setup field group carrying --station / --token is not "
                 "registered under any leaf");

        QWidget* page = dialog.realizePageForTest(QStringLiteral("Remote Access"));
        QVERIFY(page != nullptr);
        QSignalSpy requests(&dialog, &SetupDialog::connectionsRequested);
        auto* button = page->findChild<QPushButton*>(QStringLiteral("remoteStationConnections"));
        QVERIFY(button);
        button->click();
        QCOMPARE(requests.count(), 1);
        QVERIFY2(page->isEnabled(),
                 "the Remote Access page was disabled by the local-DSP gate; "
                 "it must not touch this process's DSP at all");
    }

    // ====================================================================
    // The ordering SettingsProxy.h explicitly asks Task 20 to confirm.
    // ====================================================================

    // SliceModel, NotchModel, FilterPresetStore and TciServer all do
    // contains()-then-seed against Station-classified prefixes in their
    // constructors. On a remote client those run before any snapshot can
    // land. What stops them baking this client's ship defaults into the
    // STATION store is entirely that SettingsProxy::ready() is still
    // false -- writes update the cache and are dropped rather than sent.
    // SettingsProxy.h:217-238 records that as load-bearing-but-accidental
    // and asks Task 20 to pin the ordering rather than inherit it.
    void constructingARemoteModelBehindTheProxyOffersNoOutboundWrite()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        QVERIFY(!proxy.ready());

        QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
        QVERIFY(outbound.isValid());

        {
            RadioModel model(RadioModel::Role::Remote);
            model.addSlice();
        }

        QVERIFY2(!proxy.ready(),
                 "nothing in RadioModel construction may flip the proxy ready; "
                 "if it does, every seed-if-absent constructor write starts "
                 "reaching the station store");
        QCOMPARE(outbound.count(), 0);

        AppSettings::instance().setRemoteBackend(nullptr);
    }

    // ====================================================================
    // Fix round 4, Critical: isConnected() stopped implying a connection.
    //
    // Before this branch, RadioModel::isConnected() was
    // `m_connection && m_connection->isConnected()`, so any caller that
    // tested it had ALSO tested connection() for null without meaning to.
    // Task 3 made it storage-backed (m_connectionState == Connected) so a
    // client that deliberately owns no RadioConnection can report
    // Connected. Every caller that leaned on the old implication became a
    // null dereference the moment a station handshake completed.
    //
    // The two tests below pin the two halves of what is now true, so the
    // next reader is not left inferring the implication from the name.
    // ====================================================================

    // Half one: on a Role::Remote model the implication is FALSE, and that
    // is the supported steady state, not a transient. This is the exact
    // precondition MainWindow::onConnectionStateChanged() crashed on.
    //
    // setStationConnectionState() is used rather than
    // applyStationCapabilities() on purpose: it is the narrowest public
    // writer of the same m_connectionState, so the assertion does not go
    // stale if the capabilities struct gains or loses a field.
    void remoteModelReportsConnectedWhileConnectionStaysNull()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(!model.isConnected());
        QVERIFY(model.connection() == nullptr);

        model.setStationConnectionState(ConnectionState::Connected);

        QCOMPARE(model.connectionState(), ConnectionState::Connected);
        QVERIFY2(model.isConnected(),
                 "a remote client whose station holds the radio must report "
                 "Connected; that is the premise of R2");
        QVERIFY2(model.connection() == nullptr,
                 "and it must still hold no RadioConnection. Any GUI branch "
                 "that reads isConnected() and then dereferences connection() "
                 "runs here, on a null pointer");
    }

    // Half two, and the non-vacuity for half one: a Role::Local model
    // cannot be talked into that state. Its connection state has exactly
    // one writer, its own RadioConnection, so the old implication still
    // holds for every existing local user. A change that let the storage
    // be forced on a local model would put local direct mode into the
    // same shape as the crash above.
    void localModelConnectionStateCannotBeForcedFromStorage()
    {
        RadioModel model;
        QVERIFY(model.connection() == nullptr);
        QVERIFY(!model.isConnected());

        model.setStationConnectionState(ConnectionState::Connected);

        QVERIFY2(!model.isConnected(),
                 "setStationConnectionState must be refused on a local model; "
                 "if it is not, isConnected() can go true with no connection "
                 "in local direct mode too");
        QVERIFY(model.connection() == nullptr);
    }

    // MainWindow cannot be constructed here (see the file banner), so the
    // slot that carries the crash is pinned by name only.
    //
    // Stated plainly, because it matters: this proves the slot still
    // exists and is still invokable, so the connect at MainWindow.cpp's
    // connectionStateChanged wiring cannot be silently unmade by a rename.
    // It proves NOTHING about the body -- it does not execute one line of
    // it, and it would pass just as happily with the null dereference
    // still in place. The guard itself is unreachable from a unit test in
    // this tree; the two model-side cases above are what state the
    // precondition, and the audit note in RadioModel.h is what tells the
    // next author the precondition is real.
    void mainWindowExposesTheConnectionStateSlot()
    {
        const QMetaObject& mo = MainWindow::staticMetaObject;
        QVERIFY2(mo.indexOfSlot("onConnectionStateChanged()") >= 0,
                 "MainWindow::onConnectionStateChanged() is not an invokable "
                 "slot; the connectionStateChanged wiring would be unmade");
    }

    // ====================================================================
    // Fix round 4, Important: TUNE is a second door into the transmitter
    // and it was not gated.
    //
    // RadioModel::setTune(true)'s power-on guard is `!isConnected() ||
    // !m_audioEngine`. Both halves pass on a connected remote model:
    // isConnected() is storage-backed (above), and m_audioEngine is
    // constructed unconditionally. MoxController::setTune(true) then sets
    // PttMode::Manual and m_manualMox and EMITS manualMoxChanged(true)
    // before it calls setMox(), and only setMox consults the R2 refusal --
    // so the refusal arrived after the state had already advanced.
    // ====================================================================

    void remoteModelRefusesTuneBeforeAnyStateAdvances()
    {
        RadioModel model(RadioModel::Role::Remote);
        MoxController* mox = model.moxController();
        QVERIFY(mox != nullptr);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);

        // The precondition that made the old guard pass.
        model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.isConnected());

        QSignalSpy refused(&model, &RadioModel::tuneRefused);
        QSignalSpy manual(mox, &MoxController::manualMoxChanged);
        QVERIFY(refused.isValid());
        QVERIFY(manual.isValid());

        model.setTune(true);

        QCOMPARE(refused.count(), 1);
        const QString reason = refused.at(0).at(0).toString();
        QVERIFY2(reason.contains(QStringLiteral("transmit"))
                     && reason.contains(QStringLiteral("Core"))
                     && !reason.contains(QStringLiteral("R4")),
                 qPrintable(QStringLiteral("refusal must explain the unavailable "
                                           "Core operation without roadmap jargon: %1")
                                .arg(reason)));

        // Nothing may have advanced. manualMoxChanged is the one that
        // reaches the UI: TxApplet paints the TUNE button "TUNING..." off
        // it, so an emission here leaves an operator looking at a button
        // that says the radio is transmitting.
        QCOMPARE(manual.count(), 0);
        QVERIFY(!mox->isManualMox());
        QVERIFY(!mox->isMox());
        QVERIFY2(!model.isTune(),
                 "m_isTuning must not latch: nothing clears it on a remote "
                 "model, because teardownConnection()'s clear sits behind "
                 "`if (!m_connection) return;`");
    }

    // The leak this closes does not stop at the client. TransmitModel is
    // watched for outbound mirroring and MirrorPolicy marks `tune`
    // Bidirectional, so a client-side TUNE press wrote tune=true on the
    // DAEMON -- which is what TransmitModel::setPowerUsingTargetDbm reads
    // to select txMode = 1, silently switching the station's drive-power
    // source out from under the operator sitting at it.
    void remoteTuneRefusalWritesNoMirroredTransmitState()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setStationConnectionState(ConnectionState::Connected);

        const int powerBefore = model.transmitModel().power();

        QSignalSpy tuneChanged(&model.transmitModel(),
                               &TransmitModel::tuneChanged);
        QSignalSpy powerChanged(&model.transmitModel(),
                                &TransmitModel::powerChanged);
        QVERIFY(tuneChanged.isValid());
        QVERIFY(powerChanged.isValid());

        model.setTune(true);

        QCOMPARE(tuneChanged.count(), 0);
        QVERIFY2(!model.transmitModel().isTune(),
                 "TransmitModel::tune is Bidirectional in MirrorPolicy; "
                 "setting it here writes it on the station");
        QCOMPARE(powerChanged.count(), 0);
        QCOMPARE(model.transmitModel().power(), powerBefore);
    }

    // Non-vacuity: a Role::Local model must keep refusing TUNE for the
    // ORIGINAL reason (power off), not the new one. A gate that refused
    // unconditionally, or that reported the Core refusal locally, would pass
    // the two cases above and mislead every local user.
    void localModelStillRefusesTuneForPowerNotForRole()
    {
        RadioModel model;
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        QVERIFY(!model.isConnected());

        QSignalSpy refused(&model, &RadioModel::tuneRefused);
        model.setTune(true);

        QCOMPARE(refused.count(), 1);
        const QString reason = refused.at(0).at(0).toString();
        QVERIFY2(reason.contains(QStringLiteral("Power")),
                 qPrintable(QStringLiteral("local direct mode must still get "
                                           "the power-on reason; got: %1")
                                .arg(reason)));
        QVERIFY2(!reason.contains(QStringLiteral("Core")),
                 "the remote reason must not leak into local direct mode");
        QVERIFY(!model.isTune());
    }

    // ====================================================================
    // Fix round 4, Important (sibling): the MOX button stayed checked
    // after a refusal.
    //
    // MoxController::setMox(true) returns on rejection without advancing
    // state, so moxStateChanged never fires -- and moxStateChanged was the
    // ONLY thing that unchecked the button. Locally that is occasional
    // (band-plan / interlock rejections). Remotely EVERY press is
    // rejected, so the button was permanently wrong.
    // ====================================================================

    void moxButtonUnchecksItselfWhenTheRequestIsRefused()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setStationConnectionState(ConnectionState::Connected);

        // TxApplet::wireControls() reads m_model->moxController() itself,
        // from the constructor, so there is nothing to inject.
        TxApplet applet(&model);
        applet.setTransmitPermitted(true);

        QPushButton* moxBtn = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->accessibleName() == QStringLiteral("MOX transmit")) {
                moxBtn = b;
                break;
            }
        }
        QVERIFY2(moxBtn != nullptr,
                 "MOX button not found by accessible name; the applet's "
                 "accessible names are the only stable handle a test has");

        QSignalSpy rejected(model.moxController(), &MoxController::moxRejected);
        QVERIFY(rejected.isValid());

        moxBtn->setChecked(true);

        QCOMPARE(rejected.count(), 1);
        QVERIFY(!model.moxController()->isMox());
        QVERIFY2(!moxBtn->isChecked(),
                 "the button must follow the refusal; leaving it checked "
                 "tells the operator the radio is transmitting when it is "
                 "not");
    }

    void remoteTransmitPermissionDisablesActivationWithoutWritingModelState()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        TxApplet applet(&model);
        applet.setTransmitPermitted(false,
                                    QStringLiteral("Remote transmit is unavailable"));

        const auto findButton = [&applet](const QString& accessibleName) {
            for (QPushButton* button : applet.findChildren<QPushButton*>()) {
                if (button->accessibleName() == accessibleName) {
                    return button;
                }
            }
            return static_cast<QPushButton*>(nullptr);
        };
        QPushButton* const tune = findButton(QStringLiteral("Tune carrier"));
        QPushButton* const mox = findButton(QStringLiteral("MOX transmit"));
        QPushButton* const vox = findButton(
            QStringLiteral("VOX voice-operated transmit"));
        QVERIFY(tune != nullptr);
        QVERIFY(mox != nullptr);
        QVERIFY(vox != nullptr);
        QVERIFY(!tune->isEnabled());
        QVERIFY(!mox->isEnabled());
        QVERIFY(!vox->isEnabled());
        QCOMPARE(mox->toolTip(), QStringLiteral("Remote transmit is unavailable"));

        QSignalSpy moxRejected(model.moxController(), &MoxController::moxRejected);
        QSignalSpy tuneRefused(&model, &RadioModel::tuneRefused);
        QSignalSpy voxChanged(&model.transmitModel(), &TransmitModel::voxEnabledChanged);

        // QAbstractButton::click() is the widget activation path and is a
        // no-op while disabled. It verifies the presentation gate prevents
        // reaching the existing MOX/TUNE/VOX model handlers.
        tune->click();
        mox->click();
        vox->click();

        QCOMPARE(moxRejected.count(), 0);
        QCOMPARE(tuneRefused.count(), 0);
        QCOMPARE(voxChanged.count(), 0);
        QVERIFY(!model.mox());
        QVERIFY(!model.isTune());
        QVERIFY(!model.transmitModel().voxEnabled());
    }

    void remoteTransmitControlsStartDeniedBeforeHandshakePermission()
    {
        RadioModel model(RadioModel::Role::Remote);
        TxApplet applet(&model);

        const auto findButton = [&applet](const QString& accessibleName) {
            for (QPushButton* button : applet.findChildren<QPushButton*>()) {
                if (button->accessibleName() == accessibleName) {
                    return button;
                }
            }
            return static_cast<QPushButton*>(nullptr);
        };
        QPushButton* const tune = findButton(QStringLiteral("Tune carrier"));
        QPushButton* const mox = findButton(QStringLiteral("MOX transmit"));
        QVERIFY(tune != nullptr);
        QVERIFY(mox != nullptr);

        QVERIFY(!tune->isEnabled());
        QVERIFY(!mox->isEnabled());
        QCOMPARE(mox->toolTip(),
                 QStringLiteral("Transmit controls are unavailable until the Core "
                                "confirms transmit permission."));
        QVERIFY2(OperatorWording::isPlain(mox->toolTip()), qPrintable(mox->toolTip()));

        QSignalSpy moxRejected(model.moxController(), &MoxController::moxRejected);
        QSignalSpy tuneRefused(&model, &RadioModel::tuneRefused);
        tune->click();
        mox->click();
        QCOMPARE(moxRejected.count(), 0);
        QCOMPARE(tuneRefused.count(), 0);
    }

    void localTransmitControlsRemainEnabledByDefault()
    {
        RadioModel model;
        TxApplet applet(&model);

        QVERIFY(applet.rfPowerSlider()->isEnabled());
        QVERIFY(applet.tunePowerSlider()->isEnabled());
        QVERIFY(applet.findChild<QPushButton*>(QStringLiteral("TxVoxButton"))->isEnabled());
    }

    void transmitPermissionRestorePreservesAnExistingFeatureGate()
    {
        RadioModel model;
        TxApplet applet(&model);
        QPushButton* const twoTone = applet.twoToneButton();
        QVERIFY(twoTone != nullptr);

        // Simulate an independent feature/dependency gate that was already
        // in effect before remote permission was denied.
        twoTone->setEnabled(false);
        applet.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        applet.setTransmitPermitted(true);

        QVERIFY(!twoTone->isEnabled());
    }

    // Parity Task 16 (R-R3-49): a remote window draws its Core's curve
    // (dsp.filterResponse); on a Core that does not send it (below
    // dspInfoVersion 1, as this bare remote model is) the box is disabled
    // with the reason.
    void remoteHighResolutionFilterGraphControlIsExplicitlyUnavailable()
    {
        RadioModel model(RadioModel::Role::Remote);
        DspOptionsPage page(&model);
        QCheckBox* const highRes = page.highResolutionFilterCharacteristicsCheckBox();
        QVERIFY(highRes != nullptr);
        QVERIFY(!highRes->isEnabled());
        QCOMPARE(highRes->toolTip(), IStationLink::filterResponseUnavailableReason());

        QSignalSpy toggled(highRes, &QCheckBox::toggled);
        highRes->click();
        QCOMPARE(toggled.count(), 0);
        QVERIFY(!highRes->isChecked());
    }

    void localHighResolutionFilterGraphControlRemainsAvailable()
    {
        RadioModel model;
        DspOptionsPage page(&model);
        QCheckBox* const highRes = page.highResolutionFilterCharacteristicsCheckBox();
        QVERIFY(highRes != nullptr);
        QVERIFY(highRes->isEnabled());
    }

    // ====================================================================
    // R-R3-21 control inventory: the two Setup leaves the audit found
    // transmit-only but not following the transmit permission.
    //
    // DSP > CFC holds the Phase Rotator, CFC and CESSB, all TX stages, and
    // its [Configure CFC bands] button opens the TX CFC editor. Test >
    // Two-Tone IMD writes the two-tone test settings of a keyed test
    // transmission. Neither writes anything the Core mirrors, so on a
    // receive-only session each was a live-looking page whose edits
    // landed in this window's own TransmitModel and nowhere else.
    // ====================================================================
    void remoteTransmitOnlySetupLeavesFollowThePermission_data()
    {
        QTest::addColumn<QString>("label");
        // R-R3-49 (parity Task 4): DSP > CFC left this list. The Core now
        // mirrors every setting on it and the page gates its own controls
        // on transmitSettingsVersion 4 (tst_remote_tx_eq_cfc).
        // R-R3-49 (parity Task 5): Test > Two-Tone IMD is no longer held for
        // remote transmit either. The Core mirrors its settings and the page
        // gates its own controls on transmitSettingsVersion 5; the case now
        // proves that gate, closed until the dialog pushes it
        // (tst_remote_transmit_setup_pages covers the open gate).
        QTest::newRow("Two-Tone IMD") << QStringLiteral("Two-Tone IMD");
    }

    void remoteTransmitOnlySetupLeavesFollowThePermission()
    {
        QFETCH(QString, label);
        const QString reason = QStringLiteral("Remote transmit is unavailable");

        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setTransmitPermitted(false, reason);
        QSignalSpy cfcEditor(&dialog, &SetupDialog::cfcDialogRequested);

        const int handOutsBefore = remote.localDspHandOutCount();
        dialog.selectPage(label);
        QWidget* const page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        // Neither page reaches this process's DSP, so the transmit gate is
        // the only thing disabling it; the case below proves the gate, not
        // the resource audit.
        QCOMPARE(remote.localDspHandOutCount(), handOutsBefore);
        // R-R3-49 (parity Task 5): the page opens without remote transmit;
        // its controls wait for the version 5 settings gate, with its reason.
        QVERIFY(page->isEnabled());
        QVERIFY(page->toolTip().isEmpty());
        QTreeWidgetItem* const leaf = setupLeaf(dialog, label);
        QVERIFY(leaf != nullptr);
        QVERIFY(leaf->toolTip(0).isEmpty());
        auto* const notice = dialog.findChild<QLabel*>(QStringLiteral("setupTransmitUnavailable"));
        QVERIFY(notice != nullptr);
        QVERIFY(notice->isHidden());

        // Activation reaches nothing: every button, check box and spin box
        // on the page is driven, and no transmit setting moves.
        const TransmitModel& tx = remote.transmitModel();
        const bool cfc = tx.cfcEnabled();
        const bool cfcPostEq = tx.cfcPostEqEnabled();
        const int precomp = tx.cfcPrecompDb();
        const bool phaseRotator = tx.phaseRotatorEnabled();
        const bool cessb = tx.cessbOn();
        const int freq1 = tx.twoToneFreq1();
        const int freq2 = tx.twoToneFreq2();
        const bool pulsed = tx.twoTonePulsed();
        const bool invert = tx.twoToneInvert();
        for (QAbstractButton* button : page->findChildren<QAbstractButton*>()) {
            QVERIFY2(!button->isEnabled(), qPrintable(button->text()));
            button->click();
        }
        for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
            QVERIFY(!spin->isEnabled());
            QTest::keyClick(spin, Qt::Key_Up);
        }
        QCOMPARE(tx.cfcEnabled(), cfc);
        QCOMPARE(tx.cfcPostEqEnabled(), cfcPostEq);
        QCOMPARE(tx.cfcPrecompDb(), precomp);
        QCOMPARE(tx.phaseRotatorEnabled(), phaseRotator);
        QCOMPARE(tx.cessbOn(), cessb);
        QCOMPARE(tx.twoToneFreq1(), freq1);
        QCOMPARE(tx.twoToneFreq2(), freq2);
        QCOMPARE(tx.twoTonePulsed(), pulsed);
        QCOMPARE(tx.twoToneInvert(), invert);
        QCOMPARE(cfcEditor.count(), 0);

        // R-R3-49 (parity Task 5): the version 5 gate opens the controls
        // whatever the transmit permission says, and closing it puts them
        // back with its reason, which is plain English.
        dialog.setTransmitSettingsPermitted(true, QString(), 5);
        for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
            QVERIFY(spin->isEnabled());
        }
        dialog.setTransmitSettingsPermitted(false, QString(), 5);
        for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
            QVERIFY(!spin->isEnabled());
            QVERIFY2(OperatorWording::isPlain(spin->toolTip()), qPrintable(spin->toolTip()));
        }
        QVERIFY(page->isEnabled());
        QVERIFY(notice->isHidden());

        // Local direct mode: unchanged, live, no reason shown.
        RadioModel local;
        SetupDialog localDialog(&local);
        localDialog.selectPage(label);
        QWidget* const localPage = localDialog.realizedPageForTest(label);
        QVERIFY(localPage != nullptr);
        QVERIFY(localPage->isEnabled());
        QVERIFY(localPage->toolTip().isEmpty());
        QVERIFY(localDialog.findChild<QLabel*>(
                    QStringLiteral("setupTransmitUnavailable"))->isHidden());
    }

    // ====================================================================
    // R-R3-21: a page the local-DSP gate disables says why.
    //
    // The gate (SetupDialog::realizePage) disabled the Audio leaves on a
    // remote model but gave no reason, so the operator saw a greyed page
    // and nothing else. R-R3-23 narrowed the set: Devices and TX Input
    // pick this computer's devices and now work (cases below). TCI reached
    // it only through the backend strip, which no longer counts; since
    // R-R3-42 it configures this computer's TCI server and works in a
    // remote window (remoteTciPagesWorkOnThisComputer). Since R-R3-44 VAX
    // and Advanced work too (remoteVaxAndAdvancedPagesWorkOnThisComputer),
    // so the reason is proved on a probe Mixed page that reaches this
    // process's engine on purpose.
    // ====================================================================
    void remoteLocalDspSetupPagesShowAPlainReason()
    {
        const QString label = QStringLiteral("Probe page reaching local DSP");
        const bool reachesLocalDsp = true;
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.registerPageForTest(label, SetupScope::Mixed, [&remote]() -> QWidget* {
            (void)remote.audioEngine();
            return new QWidget;
        });
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
            QStringLiteral("Setup page.*reached local DSP on a remote-station model")));

        const int handOutsBefore = remote.localDspHandOutCount();
        dialog.selectPage(label);
        QWidget* const page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        QVERIFY2((remote.localDspHandOutCount() > handOutsBefore) == reachesLocalDsp,
                 "whether the page reaches local DSP changed; re-audit its row "
                 "in remote-controls.md before changing this case");
        QVERIFY(!page->isEnabled());

        auto* const localNotice = dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"));
        auto* const txNotice = dialog.findChild<QLabel*>(QStringLiteral("setupTransmitUnavailable"));
        QVERIFY(localNotice != nullptr);
        QVERIFY(txNotice != nullptr);
        QVERIFY(!localNotice->isHidden());
        QVERIFY(txNotice->isHidden());
        const QString reason = localNotice->text();
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QCOMPARE(page->toolTip(), reason);
        QTreeWidgetItem* const leaf = setupLeaf(dialog, label);
        QVERIFY(leaf != nullptr);
        QCOMPARE(leaf->toolTip(0), reason);

        // A transmit permission does not make this computer's audio engine
        // the station's; the page and its reason stay.
        dialog.setTransmitPermitted(true);
        QVERIFY(!page->isEnabled());
        QVERIFY(!localNotice->isHidden());

        // Moving to a receive page that is available hides the notice.
        dialog.selectPage(QStringLiteral("NR/ANF"));
        QVERIFY(localNotice->isHidden());
        QVERIFY(txNotice->isHidden());
    }

    // R-R3-44: Audio > VAX and Audio > Advanced work in a remote window.
    // They reach this computer's engine (the VAX outputs a remote window
    // feeds from the Core) through localAudioDevices(), which the local-DSP
    // audit does not count, so neither page is disabled. The VAX page's
    // "Consumers:" row says what the platform reports in plain words.
    // Advanced refuses Send IQ to VAX with a plain reason, and its DSP group
    // (the Core's settings) follows the Core's availability.
    void remoteVaxAndAdvancedPagesWorkOnThisComputer()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        auto* const localNotice = dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"));
        QVERIFY(localNotice != nullptr);
        for (const QString& label : {QStringLiteral("VAX"), QStringLiteral("Advanced")}) {
            const int handOutsBefore = remote.localDspHandOutCount();
            dialog.selectPage(label);
            QWidget* const page = dialog.realizedPageForTest(label);
            QVERIFY2(page != nullptr, qPrintable(label));
            QCOMPARE(remote.localDspHandOutCount(), handOutsBefore);
            QVERIFY2(page->isEnabled(), qPrintable(label));
            QVERIFY2(page->toolTip().isEmpty(), qPrintable(label));
            QVERIFY2(localNotice->isHidden(), qPrintable(label));
        }

        QWidget* const vaxPage = dialog.realizedPageForTest(QStringLiteral("VAX"));
        const QList<QLabel*> consumers = vaxPage->findChildren<QLabel*>(QStringLiteral("vaxConsumerLabel"));
        QCOMPARE(consumers.size(), 4);
        for (QLabel* consumer : consumers) {
            QVERIFY2(OperatorWording::isPlain(consumer->text()), qPrintable(consumer->text()));
            QVERIFY(consumer->text() != QStringLiteral("\u2014"));
        }

        QWidget* const advanced = dialog.realizedPageForTest(QStringLiteral("Advanced"));
        QCheckBox* sendIq = nullptr;
        for (QCheckBox* box : advanced->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Send IQ to VAX")) { sendIq = box; }
        }
        QVERIFY(sendIq != nullptr);
        QVERIFY(sendIq->isHidden());
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("audio/SendIqToVax")));

        dialog.setStationSettingsAvailable(false, kStationReason);
        const QList<QWidget*> gated = controlsGatedWith(advanced, kStationReason);
        QCOMPARE(gated.size(), 2);
        for (QWidget* control : gated) {
            QVERIFY(qobject_cast<QComboBox*>(control) != nullptr);
            QVERIFY(!control->isEnabled());
        }
        dialog.setStationSettingsAvailable(true, QString());
        for (QWidget* control : gated) {
            QVERIFY(control->isEnabled());
        }

        // Locally Send IQ to VAX is stored as before.
        RadioModel local;
        AudioAdvancedPage localAdvanced(&local);
        for (QCheckBox* box : localAdvanced.findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Send IQ to VAX")) {
                QVERIFY(box->isEnabled());
            }
        }
    }

    // R-R3-43 / R-R3-44: SetupDialog carries MainWindow's live "receiver
    // streams are Opus" value to Audio > VAX, before the page is built and
    // after. A local window never gets it, so its note stays absent.
    void remoteVaxPageSaysWhenReceiverAudioIsCompressed()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setReceiverAudioNote(RemoteReceiverAudioNote::OpusChosen);  // before the page exists
        dialog.selectPage(QStringLiteral("VAX"));
        QWidget* const page = dialog.realizedPageForTest(QStringLiteral("VAX"));
        QVERIFY(page != nullptr);
        auto* const note = page->findChild<QLabel*>(QStringLiteral("vaxCompressedAudioNote"));
        QVERIFY(note != nullptr);
        QVERIFY(!note->isHidden());
        QVERIFY2(OperatorWording::isPlain(note->text()), qPrintable(note->text()));
        dialog.setReceiverAudioNote(RemoteReceiverAudioNote::None);  // Lossless now runs
        QVERIFY(note->isHidden());
        dialog.setReceiverAudioNote(RemoteReceiverAudioNote::LosslessUnavailable);  // it fell back to Opus
        QVERIFY(!note->isHidden());
        QVERIFY(!note->text().contains(QLatin1String("Set Audio quality")));

        RadioModel local;
        SetupDialog localDialog(&local);
        localDialog.selectPage(QStringLiteral("VAX"));
        QWidget* const localPage = localDialog.realizedPageForTest(QStringLiteral("VAX"));
        QVERIFY(localPage != nullptr);
        auto* const localNote = localPage->findChild<QLabel*>(QStringLiteral("vaxCompressedAudioNote"));
        QVERIFY(localNote != nullptr);
        QVERIFY(localNote->isHidden());
    }

    // R-R3-43 / R-R3-44 fix wave (M3): MainWindow's push of the VAX note,
    // through the same seams MainWindow's constructor and createSetupDialog
    // use (MainWindow itself cannot be built here). The note is pushed on
    // an audio status change, on a station link change (how a capability
    // change arrives, which need not change the audio status), and into a
    // Setup dialog when it opens.
    void mainWindowPushesTheReceiverAudioNote()
    {
        // As in a real window, CoreInit's migrations ran before a
        // StationClient exists (the file's other sessions use 6 too).
        AppSettings::instance().ensureSettingsAtVersion(6);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        RemoteMediaController media(&client, &remote, nullptr);
        QWidget root;
        RemoteReceiverAudioNote current = RemoteReceiverAudioNote::None;
        const auto source = [&current] { return current; };
        MainWindow::wireReceiverAudioNotePush(&root, &media, &remote, source);

        // Setup opens: seeded with the value at that moment.
        current = RemoteReceiverAudioNote::OpusChosen;
        auto* dialog = new SetupDialog(&remote, &root);
        MainWindow::seedReceiverAudioNote(dialog, source);
        dialog->selectPage(QStringLiteral("VAX"));
        QWidget* const page = dialog->realizedPageForTest(QStringLiteral("VAX"));
        QVERIFY(page != nullptr);
        auto* const note = page->findChild<QLabel*>(QStringLiteral("vaxCompressedAudioNote"));
        QVERIFY(note != nullptr);
        QVERIFY(!note->isHidden());
        QVERIFY(note->text().contains(QLatin1String("Set Audio quality")));

        // An audio status change.
        current = RemoteReceiverAudioNote::LosslessUnavailable;
        emit media.audioStatusChanged();
        QVERIFY(!note->isHidden());
        QVERIFY(!note->text().contains(QLatin1String("Set Audio quality")));

        // A capability change with no audio status change: the Core stops
        // sending receiver streams, so VAX is not fed from it.
        current = RemoteReceiverAudioNote::None;
        remote.reportStationLinkStateChanged();
        QVERIFY(note->isHidden());

        // And back.
        current = RemoteReceiverAudioNote::OpusChosen;
        remote.reportStationLinkStateChanged();
        QVERIFY(!note->isHidden());

        // Without media (a local window) the note is None.
        QCOMPARE(MainWindow::receiverAudioNoteFor(nullptr), RemoteReceiverAudioNote::None);
        // An unconnected Core sends no receiver streams.
        QCOMPARE(MainWindow::receiverAudioNoteFor(&media), RemoteReceiverAudioNote::None);
    }

    // R-R3-23: Audio > Devices in a remote window picks this computer's
    // speakers, headphones and microphone as it always has, whatever the
    // transmit permission. Nothing counted by the local-DSP audit is
    // reached, so the page is enabled with no reason shown, and a card
    // change is saved to this computer's audio/* keys and handed to the
    // engine that plays remote audio.
    void remoteDevicesPageWorksOnThisComputer()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));

        const int handOutsBefore = remote.localDspHandOutCount();
        dialog.selectPage(QStringLiteral("Devices"));
        QWidget* const page = dialog.realizedPageForTest(QStringLiteral("Devices"));
        QVERIFY(page != nullptr);
        QCOMPARE(remote.localDspHandOutCount(), handOutsBefore);
        QVERIFY(page->isEnabled());
        QVERIFY(page->toolTip().isEmpty());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"))->isHidden());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("setupTransmitUnavailable"))->isHidden());
        QTreeWidgetItem* const leaf = setupLeaf(dialog, QStringLiteral("Devices"));
        QVERIFY(leaf != nullptr);
        QVERIFY(leaf->toolTip(0).isEmpty());

        for (const char* title : {"Speakers", "Headphones", "TX Input (Microphone)"}) {
            DeviceCard* const card = deviceCardOf(page, QString::fromLatin1(title));
            QVERIFY2(card != nullptr, title);
            QVERIFY2(card->isEnabled(), title);
        }

        // The microphone choice: saved to audio/TxInput/* and handed to the
        // engine (the one Test Mic and a later remote microphone open).
        DeviceCard* const mic = deviceCardOf(page, QStringLiteral("TX Input (Microphone)"));
        QComboBox* const buffer = deviceCardBufferCombo(mic);
        QVERIFY(buffer != nullptr);
        const int next = (buffer->currentIndex() + 1) % buffer->count();
        const int samples = buffer->itemData(next).toInt();
        // The card debounces its buffer combo by 200 ms, then saves and
        // hands the config on.
        buffer->setCurrentIndex(next);
        QTRY_COMPARE(remote.localAudioDevices()->txInputConfig().bufferSamples, samples);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/TxInput/BufferSamples"))
                     .toString(),
                 QString::number(samples));
        QCOMPARE(remote.localDspHandOutCount(), handOutsBefore);
    }

    // R-R3-42: Audio > TCI and CAT & Network > TCI Server configure the TCI
    // server that runs on this computer and serves apps here, in a remote
    // window as in a local one. Both are usable, show no reason, reach no
    // local DSP, and save to this computer's own settings.
    void remoteTciPagesWorkOnThisComputer()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        const int handOutsBefore = remote.localDspHandOutCount();
        for (const char* label : {"TCI", "TCI Server"}) {
            const QString name = QString::fromLatin1(label);
            dialog.selectPage(name);
            QWidget* const page = dialog.realizedPageForTest(name);
            QVERIFY2(page != nullptr, label);
            QVERIFY2(page->isEnabled(), label);
            QVERIFY2(page->toolTip().isEmpty(), label);
            QVERIFY2(dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"))->isHidden(),
                     label);
            QTreeWidgetItem* const leaf = setupLeaf(dialog, name);
            QVERIFY2(leaf != nullptr, label);
            QVERIFY2(leaf->toolTip(0).isEmpty(), label);
        }
        QCOMPARE(remote.localDspHandOutCount(), handOutsBefore);
        QCOMPARE(classifySettingsKey(QStringLiteral("TciServerPort")), SettingsScope::OperatorLocal);
    }

    // R-R3-36: TX Input is Mixed. This computer's PC microphone (backend,
    // device, buffer, Test Mic) works in a remote window; the mic source,
    // Mic Gain and the radio's microphone hardware follow the transmit
    // permission with its reason, and move nothing while it is withheld.
    void remoteTxInputKeepsThisComputersMicrophoneUsable()
    {
        const QString txReason = QStringLiteral("Remote transmit is unavailable");
        RadioModel remote(RadioModel::Role::Remote);
        // An older client/Core path retains this computer's PC/VAX choices;
        // the radio input still requires the negotiated source command.
        StationClient client(&remote, nullptr);
        SetupDialog dialog(&remote);
        dialog.setTransmitPermitted(false, txReason);

        const int handOutsBefore = remote.localDspHandOutCount();
        dialog.selectPage(QStringLiteral("TX Input"));
        QWidget* const container = dialog.realizedPageForTest(QStringLiteral("TX Input"));
        QVERIFY(container != nullptr);
        QCOMPARE(remote.localDspHandOutCount(), handOutsBefore);
        QVERIFY(container->isEnabled());
        auto* const page = container->findChild<AudioTxInputPage*>();
        QVERIFY(page != nullptr);
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"))->isHidden());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("setupTransmitUnavailable"))->isHidden());

        // This computer's microphone.
        QVERIFY(page->backendCombo()->isEnabled());
        QVERIFY(page->deviceCombo()->isEnabled());
        QVERIFY(page->bufferSlider()->isEnabled());
        QVERIFY(page->testMicButton()->isEnabled());

        // The controls held for the radio: disabled, with the reason. The mic
        // source follows remote transmit; since parity Task 3 (R-R3-49) Mic
        // Gain and the radio microphone groups follow the transmit settings
        // gate (transmitSettingsVersion 3), closed until the Core offers it.
        QVERIFY(!page->micSourceGroup()->isEnabled());
        QCOMPARE(page->micSourceGroup()->toolTip(), txReason);
        QList<QWidget*> held{page->micGainSlider()};
        for (QGroupBox* group : {page->hermesRadioMicGroup(), page->orionRadioMicGroup(),
                                 page->saturnRadioMicGroup()}) {
            if (group != nullptr) { held << group; }
        }
        for (QWidget* control : held) {
            QVERIFY(control != nullptr);
            QVERIFY2(!control->isEnabled(), qPrintable(control->objectName()));
            QCOMPARE(control->toolTip(), IStationLink::transmitSettingsUnavailableReason());
        }

        // Activation moves nothing held for the radio.
        TransmitModel& tx = remote.transmitModel();
        const MicSource source = tx.micSource();
        const int micGain = tx.micGainDb();
        for (QRadioButton* button : page->micSourceGroup()->findChildren<QRadioButton*>()) {
            button->click();
        }
        QTest::keyClick(page->micGainSlider(), Qt::Key_Right);
        QCOMPARE(tx.micSource(), source);
        QCOMPARE(tx.micGainDb(), micGain);

        // The microphone choice is saved to this computer's audio/TxInput.
        QSlider* const buffer = page->bufferSlider();
        const int next = (buffer->value() + 1) % (buffer->maximum() + 1);
        buffer->setValue(next);
        const int samples = AudioTxInputPage::kBufferSizes.at(next);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/TxInput/BufferSamples"))
                     .toString(),
                 QString::number(samples));
        QCOMPARE(remote.localAudioDevices()->txInputConfig().bufferSamples, samples);

        // A Core that permits transmit lifts the mic source's gate; one that
        // takes transmit settings (version 3) lifts Mic Gain's and the radio
        // microphone groups'.
        dialog.setTransmitPermitted(true);
        QVERIFY(page->micSourceGroup()->isEnabled());
        QRadioButton* pc = nullptr;
        QRadioButton* vax = nullptr;
        QRadioButton* radio = nullptr;
        for (QRadioButton* button : page->micSourceGroup()->findChildren<QRadioButton*>()) {
            if (button->text() == QStringLiteral("PC Mic")) { pc = button; }
            if (button->text().startsWith(QStringLiteral("VAX TX"))) { vax = button; }
            if (button->text() == QStringLiteral("Radio Mic")) { radio = button; }
        }
        QVERIFY(pc && vax && radio);
        QVERIFY(pc->isEnabled());
        QVERIFY(vax->isEnabled());
        QVERIFY(!radio->isEnabled());
        vax->click();
        QCOMPARE(tx.micSource(), MicSource::Vax);
        pc->click();
        QCOMPARE(tx.micSource(), MicSource::Pc);
        QVERIFY(!page->micGainSlider()->isEnabled());
        dialog.setTransmitSettingsPermitted(true, QString(), 3);
        QVERIFY(page->micGainSlider()->isEnabled());
        QVERIFY(page->micGainSlider()->toolTip() != txReason);
        dialog.setTransmitPermitted(false, txReason);
        QVERIFY(!page->micSourceGroup()->isEnabled());
        QVERIFY(page->micGainSlider()->isEnabled());
        QVERIFY(page->bufferSlider()->isEnabled());

        // Local direct mode: every control live.
        RadioModel local;
        SetupDialog localDialog(&local);
        localDialog.selectPage(QStringLiteral("TX Input"));
        QWidget* const localContainer = localDialog.realizedPageForTest(QStringLiteral("TX Input"));
        QVERIFY(localContainer != nullptr && localContainer->isEnabled());
        auto* const localPage = localContainer->findChild<AudioTxInputPage*>();
        QVERIFY(localPage != nullptr);
        QVERIFY(localPage->micSourceGroup()->isEnabled());
        QVERIFY(localPage->micGainSlider()->isEnabled());
        QVERIFY(localPage->micGainSlider()->toolTip() != txReason);
    }

    // Local direct mode never runs the local-DSP gate: no Audio page is
    // disabled and the notice never shows.
    void localSetupNeverShowsTheLocalUnavailableNotice()
    {
        RadioModel local;
        SetupDialog dialog(&local);
        auto* const localNotice = dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"));
        QVERIFY(localNotice != nullptr);
        for (const char* label : {"Devices", "TX Input", "VAX", "TCI", "Advanced"}) {
            const QString name = QString::fromLatin1(label);
            dialog.selectPage(name);
            QWidget* const page = dialog.realizedPageForTest(name);
            QVERIFY(page != nullptr);
            QVERIFY2(page->isEnabled(), label);
            QVERIFY2(page->toolTip().isEmpty(), label);
            QVERIFY2(localNotice->isHidden(), label);
        }
    }

    // ====================================================================
    // R-R3-21 / R-R3-10 / R-R3-17: Setup in a disconnected remote window.
    //
    // The per-page table. MainWindow pushes "Connect to the Core to change
    // these." while the window has no live session with the Core's
    // settings. ThisComputer pages stay usable; Core pages are disabled
    // with that reason (and, before the Core's settings ever arrived, are
    // not built at all); Mixed pages disable exactly their Core controls.
    // Realizing every page writes nothing towards the Core, not even an
    // edit the proxy would hold for later.
    // ====================================================================
    void disconnectedRemoteSetupPerPageTable()
    {
        SettingsProxy proxy;  // never ready, no snapshot: never connected
        AppSettings::instance().setRemoteBackend(&proxy);
        QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&proxy, &SettingsProxy::outboundRemoveRequested);

        RadioModel remote(RadioModel::Role::Remote);
        // The model's own constructors seed a few station keys while the
        // proxy is not ready (BandPlanManager's "BandPlanName"; see
        // SettingsProxy.h, "ready()==false is load-bearing"). That is not
        // Setup's doing, so it is the baseline the pages must not add to.
        const QSet<QString> seededByTheModel = proxy.droppedWhileOffline();
        SetupDialog dialog(&remote);
        // The dialog reads the same predicate MainWindow pushes from.
        QVERIFY(!dialog.stationSettingsAvailableForTest());
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        dialog.setStationSettingsAvailable(false, kStationReason);
        QVERIFY(OperatorWording::isPlain(kStationReason));

        auto* const notice = dialog.findChild<QLabel*>(QStringLiteral("setupStationUnavailable"));
        QVERIFY(notice != nullptr);

        // Mixed pages: how many Core controls each disables. Filter
        // Presets, Spectrum Peaks, Waterfall Defaults and 3D View have no
        // Core controls and are ThisComputer (R3 Setup fix wave, final
        // review I4); so is VAX (R-R3-44). Export / Import remains a
        // ThisComputer page, with a Core-dependent combined export action.
        const QMap<QString, int> coreControls{
            {QStringLiteral("Startup & Preferences"), 2}, // callsign, grid (R-R3-21)
            // General: Region, Network Watchdog (R-R3-49), Receive Only
            // (Task 16), the Time Out Timers group (iPhone app plan
            // Task 38, gated as one), Extended (addendum G-42) and Prevent
            // TX'ing on a different band (the Core's setting since 2026-09-29).
            {QStringLiteral("Options"), 6},
            {QStringLiteral("Spectrum Defaults"), 5},   // FFT size, window, Hz/bin, fps x2
            {QStringLiteral("Grid & Scales"), 3},       // dB max, dB min, copy
            {QStringLiteral("Multimeter"), 1},          // sample interval
            {QStringLiteral("TX Display"), 9},          // TX analyzer
            // Without a negotiated hygiene capability, this page uses its
            // feature-specific reason rather than the generic Core reason.
            {QStringLiteral("Settings Validation"), 0},
            {QStringLiteral("Advanced"), 2},            // DSP rate, DSP block size (R-R3-44)
        };

        const QStringList labels = dialog.pageLabelsForTest();
        int thisComputer = 0;
        int core = 0;
        int mixed = 0;
        for (int i = 0; i < dialog.registeredPageCountForTest(); ++i) {
            const QString& label = labels.at(i);
            QWidget* const page = showSetupLeafAt(dialog, i);
            if (page == nullptr) { continue; }  // hidden PA leaves still realize; none yield null
            const QList<QWidget*> gated = controlsGatedWith(page, kStationReason);
            switch (dialog.pageScopeAtForTest(i)) {
            case SetupScope::ThisComputer:
                ++thisComputer;
                QVERIFY2(page->isEnabled(), qPrintable(label));
                QVERIFY2(notice->isHidden(), qPrintable(label));
                if (label == QStringLiteral("Export / Import")) {
                    auto* const exportAll = page->findChild<QPushButton*>(
                        QStringLiteral("exportAllSettingsButton"));
                    auto* const importAll = page->findChild<QPushButton*>(
                        QStringLiteral("importAllSettingsButton"));
                    QVERIFY(exportAll != nullptr);
                    QVERIFY(importAll != nullptr);
                    QVERIFY(!exportAll->isEnabled());
                    QCOMPARE(exportAll->accessibleDescription(), kStationReason);
                    QCOMPARE(exportAll->toolTip(), kStationReason);
                    QCOMPARE(gated, QList<QWidget*>{exportAll});
                    // Combined import is not available in a remote window;
                    // it has its own reason, independent of Core availability.
                    QVERIFY(!importAll->isEnabled());
                    QVERIFY(importAll->accessibleDescription().contains(
                        QStringLiteral("combined window and Core backup is not available")));
                } else {
                    QVERIFY2(gated.isEmpty(), qPrintable(label));
                }
                break;
            case SetupScope::Core:
                ++core;
                QVERIFY2(!page->isEnabled(), qPrintable(label));
                // Never connected: a stand-in, not a page built from this
                // computer's ship defaults.
                QCOMPARE(page->objectName(), QStringLiteral("setupStationPlaceholder"));
                QVERIFY2(!notice->isHidden(), qPrintable(label));
                QCOMPARE(notice->text(), kStationReason);
                QCOMPARE(page->toolTip(), kStationReason);
                QCOMPARE(setupLeafAt(dialog, i)->toolTip(0), kStationReason);
                break;
            case SetupScope::Mixed: {
                ++mixed;
                QVERIFY2(notice->isHidden(), qPrintable(label));
                QCOMPARE(page->objectName() == QStringLiteral("setupStationPlaceholder"), false);
                QVERIFY2(page->isEnabled(), qPrintable(label));
                if (label == QStringLiteral("TX Input")) {
                    // The controls held for the radio: the Core reason wins
                    // over the transmit reason while disconnected.
                    auto* const txInput = page->findChild<AudioTxInputPage*>();
                    QVERIFY(txInput != nullptr);
                    for (QWidget* held : {static_cast<QWidget*>(txInput->micSourceGroup()),
                                          static_cast<QWidget*>(txInput->micGainSlider())}) {
                        QVERIFY(!held->isEnabled());
                        QCOMPARE(held->toolTip(), kStationReason);
                    }
                    QVERIFY(txInput->deviceCombo()->isEnabled());
                    QVERIFY(txInput->testMicButton()->isEnabled());
                    QVERIFY(gated.size() >= 2);
                    break;
                }
                if (label == QStringLiteral("Settings Validation")) {
                    for (const QString& text : {QStringLiteral("Re-validate"),
                                                QStringLiteral("Forget This Radio"),
                                                QStringLiteral("Repair Invalid Settings")}) {
                        QPushButton* button = buttonWithText(page, text);
                        QVERIFY(button != nullptr);
                        QVERIFY(!button->isEnabled());
                        QVERIFY2(OperatorWording::isPlain(button->toolTip()),
                                 qPrintable(button->toolTip()));
                    }
                }
                QVERIFY2(gated.size() == coreControls.value(label, 0),
                         qPrintable(QStringLiteral("%1: %2 controls gated, expected %3")
                                        .arg(label).arg(gated.size())
                                        .arg(coreControls.value(label, 0))));
                for (QWidget* control : gated) {
                    QVERIFY2(!control->isEnabled(), qPrintable(label));
                    QCOMPARE(control->toolTip(), kStationReason);
                }
                break;
            }
            }
        }
        // R-R3-49: eight This Computer leaves (UI Scale & Theme, Navigation,
        // Skins, Collapsible Display, Serial Ports, TCP/IP CAT, MIDI Control,
        // Shortcuts) and three Core leaves (TX Profiles, Signal Generator,
        // Hardware Tests) are not registered while their features are not
        // built, so each floor drops by that many.
        QVERIFY(thisComputer >= 12);
        QVERIFY(core >= 22);
        QVERIFY(mixed >= 8);

        // Nothing towards the Core: nothing sent, and nothing held as an
        // edit to be reported as lost on the next connect.
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);
        const QSet<QString> heldByPages = proxy.droppedWhileOffline() - seededByTheModel;
        QStringList held(heldByPages.cbegin(), heldByPages.cend());
        held.sort();
        QVERIFY2(held.isEmpty(), qPrintable(held.join(QStringLiteral(", "))));
    }

    // R-R3-21: once the Core's settings arrive, a page realized before them
    // (or while disconnected) is rebuilt and shows the Core's values; a
    // later snapshot on the live session rebuilds it again; losing the Core
    // disables its pages again without rebuilding anything.
    void reconnectRebuildsSetupPagesFromTheCoresValues()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setStationSettingsAvailable(false, kStationReason);
        auto* const stack = dialog.findChild<QStackedWidget*>();

        // Before any snapshot: the Mixed pages show this computer's
        // defaults for the Core's settings (disabled), the Core page is a
        // stand-in.
        dialog.selectPage(QStringLiteral("Options"));   // General
        QPointer<QWidget> oldOptions = dialog.realizedPageForTest(QStringLiteral("Options"));
        QVERIFY(oldOptions);
        auto* region = oldOptions->findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
        QVERIFY(region != nullptr);
        QCOMPARE(region->currentText(), QStringLiteral("United States"));
        QVERIFY(!region->isEnabled());
        dialog.selectPage(QStringLiteral("NB/SNB"));
        QVERIFY(dialog.isPagePlaceholderForTest(QStringLiteral("NB/SNB")));
        QPointer<QWidget> oldNb = dialog.realizedPageForTest(QStringLiteral("NB/SNB"));
        dialog.selectPage(QStringLiteral("Multimeter"));
        QPointer<QWidget> oldMultimeter = dialog.realizedPageForTest(QStringLiteral("Multimeter"));
        QList<QWidget*> delay = controlsGatedWith(oldMultimeter, kStationReason);
        QCOMPARE(delay.size(), 1);
        QCOMPARE(qobject_cast<QSpinBox*>(delay.first())->value(), 100);

        // Connected: the snapshot, then ready, then (in a later event, when
        // the session reports itself established) MainWindow's push. The
        // snapshot's own queued rebuild runs first and finds the settings
        // still unavailable, so it is the push that rebuilds.
        proxy.applySnapshot({{QLatin1String(AppSettings::kDaemonProfileSeededKey), QStringLiteral("True")},
                             {QStringLiteral("Region"), QStringLiteral("Italy")},
                             {QStringLiteral("BandPlanRegion"), QStringLiteral("5")},
                             {QStringLiteral("MultimeterDelayMs"), QStringLiteral("250")}});
        proxy.setReady(true);
        QCoreApplication::processEvents();
        QVERIFY(oldOptions && oldNb && oldMultimeter);
        dialog.setStationSettingsAvailable(true, kStationReason);
        QTRY_VERIFY(!oldOptions && !oldNb && !oldMultimeter);

        QWidget* const options = dialog.realizedPageForTest(QStringLiteral("Options"));
        region = options->findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
        QCOMPARE(region->currentText(), QStringLiteral("Japan"));
        QVERIFY(!region->isEnabled());
        QCOMPARE(region->toolTip(),
                 QStringLiteral("Region selection is not available for transmit on this Core."));
        QVERIFY(!dialog.isPagePlaceholderForTest(QStringLiteral("NB/SNB")));
        QWidget* const nb = dialog.realizedPageForTest(QStringLiteral("NB/SNB"));
        QVERIFY(nb->isEnabled());
        QVERIFY(nb->toolTip().isEmpty());
        QVERIFY(setupLeaf(dialog, QStringLiteral("NB/SNB"))->toolTip(0).isEmpty());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("setupStationUnavailable"))->isHidden());
        QWidget* const multimeter = dialog.realizedPageForTest(QStringLiteral("Multimeter"));
        // The page on screen is still the one the operator was looking at.
        QCOMPARE(stack->currentWidget(), multimeter);
        QVERIFY(controlsGatedWith(multimeter, kStationReason).isEmpty());
        int delayMs = -1;
        for (QSpinBox* spin : multimeter->findChildren<QSpinBox*>()) {
            if (spin->value() == 250) { delayMs = spin->value(); }
        }
        QCOMPARE(delayMs, 250);

        // A later snapshot on the same live session (a Core whose radio
        // came online sends one): rebuilt again, queued.
        QPointer<QWidget> connectedOptions = options;
        proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Japan")},
                             {QStringLiteral("BandPlanRegion"), QStringLiteral("3")}});
        QTRY_VERIFY(!connectedOptions);
        region = dialog.realizedPageForTest(QStringLiteral("Options"))
                     ->findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
        QCOMPARE(region->currentText(), QStringLiteral("Italy"));

        // The Core goes away: nothing is rebuilt, the Core's pages and
        // controls are disabled with the reason, and they keep showing the
        // Core's last values.
        QPointer<QWidget> nbAfter = dialog.realizedPageForTest(QStringLiteral("NB/SNB"));
        QPointer<QWidget> optionsAfter = dialog.realizedPageForTest(QStringLiteral("Options"));
        proxy.setReady(false);
        dialog.setStationSettingsAvailable(false, kStationReason);
        QCoreApplication::processEvents();
        QVERIFY(nbAfter && optionsAfter);
        QVERIFY(!nbAfter->isEnabled());
        QCOMPARE(nbAfter->toolTip(), kStationReason);
        QVERIFY(!region->isEnabled());
        QCOMPARE(region->currentText(), QStringLiteral("Italy"));
        dialog.selectPage(QStringLiteral("NB/SNB"));
        QCOMPARE(dialog.findChild<QLabel*>(QStringLiteral("setupStationUnavailable"))->text(),
                 kStationReason);
        QVERIFY(!dialog.findChild<QLabel*>(QStringLiteral("setupStationUnavailable"))->isHidden());
    }

    // R-R3-21: a page whose construction pushes availability again (the
    // push can arrive from anywhere on the main thread) does not start a
    // second rebuild from inside its own construction: every page is built
    // once per snapshot, one at a time. A return of availability without a
    // new snapshot rebuilds nothing.
    void setupPageRebuildIsGuardedAgainstReentry()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setStationSettingsAvailable(false, kStationReason);

        int builds = 0;
        int building = 0;
        bool nested = false;
        const auto probe = [&] {
            ++builds;
            nested = nested || building > 0;
            ++building;
            dialog.setStationSettingsAvailable(false, kStationReason);
            dialog.setStationSettingsAvailable(true, kStationReason);
            --building;
            return new QWidget;
        };
        const int first = dialog.registerPageForTest(QStringLiteral("Probe one"),
                                                     SetupScope::Mixed, probe);
        const int second = dialog.registerPageForTest(QStringLiteral("Probe two"),
                                                      SetupScope::Mixed, probe);
        dialog.setStationSettingsAvailable(false, kStationReason);
        QVERIFY(dialog.realizePageAtForTest(first) != nullptr);
        dialog.setStationSettingsAvailable(false, kStationReason);
        QVERIFY(dialog.realizePageAtForTest(second) != nullptr);
        QCOMPARE(builds, 2);

        // A new snapshot while unavailable, then availability returns: both
        // are rebuilt, each once, neither inside the other.
        dialog.setStationSettingsAvailable(false, kStationReason);
        proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Italy")}});
        dialog.setStationSettingsAvailable(true, kStationReason);
        QCOMPARE(builds, 4);
        QVERIFY(!nested);
        QTest::qWait(50);  // the snapshot's queued rebuild finds nothing stale
        QCOMPARE(builds, 4);

        dialog.setStationSettingsAvailable(false, kStationReason);
        dialog.setStationSettingsAvailable(true, kStationReason);
        QCoreApplication::processEvents();
        QCOMPARE(builds, 4);  // no new snapshot since they were built
    }

    // R3 Setup fix wave (final review I2): connected to a Core whose
    // snapshot was empty and carried no seed marker, the session is ready
    // but the Core's settings have not arrived. No Core page is built (its
    // constructor would send this computer's defaults to the Core), nothing
    // is sent while every page is shown, and the reason says what is true.
    void coreSetupPagesWaitForTheCoresSettingsNotAnySnapshot()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        proxy.applySnapshot({});
        proxy.setReady(true);
        QVERIFY(proxy.hasReceivedSnapshot());
        QVERIFY(!proxy.setupDialogAllowed());
        QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&proxy, &SettingsProxy::outboundRemoveRequested);

        SetupDialog dialog(&remote);
        QVERIFY(!dialog.stationSettingsAvailableForTest());
        dialog.setStationSettingsAvailable(false, kCoreSettingsMissingReason);
        QVERIFY(OperatorWording::isPlain(kCoreSettingsMissingReason));
        int probeBuilds = 0;
        const int probe = dialog.registerPageForTest(
            QStringLiteral("Core probe"), SetupScope::Core,
            [&probeBuilds] { ++probeBuilds; return new QWidget; });

        auto* const notice = dialog.findChild<QLabel*>(QStringLiteral("setupStationUnavailable"));
        QVERIFY(notice != nullptr);
        const QStringList labels = dialog.pageLabelsForTest();
        int core = 0;
        for (int i = 0; i < dialog.registeredPageCountForTest(); ++i) {
            QWidget* const page = showSetupLeafAt(dialog, i);
            if (page == nullptr || dialog.pageScopeAtForTest(i) != SetupScope::Core) {
                continue;
            }
            ++core;
            QVERIFY2(page->objectName() == QStringLiteral("setupStationPlaceholder"),
                     qPrintable(labels.at(i)));
            QVERIFY2(!page->isEnabled(), qPrintable(labels.at(i)));
            QCOMPARE(notice->text(), kCoreSettingsMissingReason);
            QVERIFY2(!notice->isHidden(), qPrintable(labels.at(i)));
        }
        // R-R3-49: three Core leaves are not registered while their
        // features are not built (TX Profiles, Signal Generator, Hardware
        // Tests).
        QVERIFY(core >= 22);
        QCOMPARE(probeBuilds, 0);
        QVERIFY(dialog.realizePageAtForTest(probe) != nullptr);
        QCOMPARE(probeBuilds, 0);
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);

        // The Core's settings arrive: the stand-ins become the real pages.
        connectDialog(proxy, dialog);
        QCOMPARE(probeBuilds, 1);
        QVERIFY(!dialog.isPagePlaceholderForTest(QStringLiteral("NB/SNB")));
    }

    // R3 Setup fix wave (final review I1): a page is never rebuilt under
    // its own open dialog. A snapshot that arrives while Settings
    // Validation's question is open leaves the page (and the question)
    // alone; the page is rebuilt once the question has closed.
    void setupPageIsNotRebuiltUnderItsOwnOpenDialog()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        PairedHygieneLink link;
        remote.attachStation(&link);
        bindHygieneRadio(remote);
        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog, {{QStringLiteral("Region"), QStringLiteral("Japan")}});

        const QString label = QStringLiteral("Settings Validation");
        dialog.selectPage(label);
        QPointer<QWidget> page = dialog.realizedPageForTest(label);
        QVERIFY(page);
        QPushButton* const forget = buttonWithText(page, QStringLiteral("Forget This Radio"));
        QVERIFY(forget != nullptr && forget->isEnabled());

        bool boxOpen = false;
        bool pageKeptWhileOpen = false;
        QTimer::singleShot(0, this, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            boxOpen = box != nullptr;
            proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Italy")}});
            QTest::qWait(100);  // the snapshot's queued rebuild runs in the box's loop
            pageKeptWhileOpen = !page.isNull() && dialog.realizedPageForTest(label) == page;
            if (box != nullptr) {
                box->button(QMessageBox::No)->click();
            }
        });
        forget->click();  // returns when the question is answered
        QVERIFY(boxOpen);
        QVERIFY(pageKeptWhileOpen);
        // Rebuilt once the question has gone.
        QTRY_VERIFY(page.isNull());
        QWidget* const rebuilt = dialog.realizedPageForTest(label);
        QVERIFY(rebuilt != nullptr);
        QCOMPARE(dialog.findChild<QStackedWidget*>()->currentWidget(), rebuilt);
    }

    // R3 Setup fix wave (final review M2): the link drops while Settings
    // Validation's Forget question is open. Yes then writes nothing, not
    // even an edit held for the next connect.
    void settingsValidationYesAfterTheLinkDropsWritesNothing()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        PairedHygieneLink link;
        remote.attachStation(&link);
        bindHygieneRadio(remote);
        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog);
        const QString label = QStringLiteral("Settings Validation");
        dialog.selectPage(label);
        QWidget* const page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&proxy, &SettingsProxy::outboundRemoveRequested);
        const QSet<QString> heldBefore = proxy.droppedWhileOffline();

        // G-38: a Core without settingsHygieneVersion 2 (this link offers
        // validation and forget only) keeps Repair disabled with its reason.
        QPushButton* const repair = buttonWithText(page, QStringLiteral("Repair Invalid Settings"));
        QVERIFY(repair != nullptr);
        dialog.setStationSettingsAvailable(true, kStationReason);
        QVERIFY(!repair->isEnabled());
        QCOMPARE(repair->toolTip(), IStationLink::settingsRepairUnavailableReason());
        dialog.setStationSettingsAvailable(false, kStationReason);
        for (const QString& text : {QStringLiteral("Forget This Radio")}) {
            QPushButton* const button = buttonWithText(page, text);
            QVERIFY(button != nullptr);
            proxy.setReady(true);
            dialog.setStationSettingsAvailable(true, kStationReason);
            QVERIFY(button->isEnabled());
            bool answered = false;
            QTimer::singleShot(0, this, [&] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (box == nullptr) { return; }
                // The link drops: the session is no longer ready and
                // MainWindow pushes the change to the open dialog.
                proxy.setReady(false);
                dialog.setStationSettingsAvailable(false, kStationReason);
                remote.setConnectionStateForTest(ConnectionState::Disconnected);
                box->button(QMessageBox::Yes)->click();
                answered = true;
            });
            button->click();
            QVERIFY2(answered, qPrintable(text));
        }
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);
        QCOMPARE(link.requests, 0);
        const QSet<QString> held = proxy.droppedWhileOffline() - heldBefore;
        QVERIFY2(held.isEmpty(),
                 qPrintable(QStringList(held.cbegin(), held.cend()).join(QStringLiteral(", "))));
    }

    // R3 Setup fix wave (final review M3): a page whose factory yields
    // nothing on a rebuild keeps the page it had instead of losing its
    // entry until Setup is reopened.
    void setupPageRebuildThatYieldsNothingKeepsThePage()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog);

        int builds = 0;
        const int probe = dialog.registerPageForTest(
            QStringLiteral("Failing probe"), SetupScope::Mixed, [&builds]() -> QWidget* {
                ++builds;
                return builds == 1 ? new QWidget : nullptr;
            });
        QPointer<QWidget> first = dialog.realizePageAtForTest(probe);
        QVERIFY(first);

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("page factory yielded nothing")));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("rebuilding Setup page.*yielded nothing")));
        proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Italy")}});
        QTRY_COMPARE(builds, 2);
        QCoreApplication::processEvents();
        QVERIFY(first);
        QCOMPARE(dialog.realizePageAtForTest(probe), first.data());
        QCOMPARE(showSetupLeafAt(dialog, probe), first.data());
    }

    // R3 Setup fix wave (final review M5): a page the local-DSP gate
    // disables stays disabled for the whole remote session, so a new
    // snapshot does not rebuild it (or log the gate's warning again).
    // R-R3-44: no real page is gated that way any more (VAX and Advanced
    // work in a remote window), so two probe pages that reach this
    // process's DSP stand in for them.
    void pagesTheLocalDspGateDisablesAreNotRebuilt()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog);
        const QString mixedProbe = QStringLiteral("Probe Mixed page reaching local DSP");
        const QString coreProbe = QStringLiteral("Probe Core page reaching local DSP");
        int probeBuilds = 0;
        dialog.registerPageForTest(mixedProbe, SetupScope::Mixed, [&remote, &probeBuilds]() -> QWidget* {
            ++probeBuilds;
            (void)remote.audioEngine();
            return new QWidget;
        });
        dialog.registerPageForTest(coreProbe, SetupScope::Core, [&remote, &probeBuilds]() -> QWidget* {
            ++probeBuilds;
            (void)remote.wdspEngine();
            return new QWidget;
        });

        QMap<QString, QPointer<QWidget>> pages;
        for (const QString& label : {mixedProbe, coreProbe}) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
                QStringLiteral("Setup page.*reached local DSP on a remote-station model")));
            dialog.selectPage(label);
            pages.insert(label, dialog.realizedPageForTest(label));
            QVERIFY2(pages.value(label) && !pages.value(label)->isEnabled(), qPrintable(label));
        }
        // Another page is rebuilt by the same snapshot, so the rebuild ran.
        dialog.selectPage(QStringLiteral("Multimeter"));
        QPointer<QWidget> multimeter = dialog.realizedPageForTest(QStringLiteral("Multimeter"));
        proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Italy")}});
        QTRY_VERIFY(multimeter.isNull());
        for (auto it = pages.cbegin(); it != pages.cend(); ++it) {
            QVERIFY2(it.value(), qPrintable(it.key()));
            QCOMPARE(dialog.realizedPageForTest(it.key()), it.value().data());
        }
        // Each probe was built once and never again.
        QCOMPARE(probeBuilds, 2);
    }

    // Local direct mode: the push has no effect. Core pages are built and
    // live, Mixed pages gate nothing, no notice.
    void localSetupIgnoresStationSettingsAvailability()
    {
        RadioModel local;
        SetupDialog dialog(&local);
        QVERIFY(dialog.stationSettingsAvailableForTest());
        dialog.setStationSettingsAvailable(false, kStationReason);
        for (const char* label : {"NB/SNB", "Multimeter", "Options", "Spectrum Defaults"}) {
            const QString name = QString::fromLatin1(label);
            dialog.selectPage(name);
            QWidget* const page = dialog.realizedPageForTest(name);
            QVERIFY2(page != nullptr, label);
            QVERIFY2(page->isEnabled(), label);
            QVERIFY2(!dialog.isPagePlaceholderForTest(name), label);
            QVERIFY2(controlsGatedWith(page, kStationReason).isEmpty(), label);
            QVERIFY2(dialog.findChild<QLabel*>(QStringLiteral("setupStationUnavailable"))->isHidden(),
                     label);
        }
    }

    // ====================================================================
    // R-R3-21: Setup leaves declared unavailable in a remote session.
    //
    // DDC Routing changes the radio's hardware settings (a placeholder
    // locally too), which a remote window cannot do yet. Hardware Config
    // is no longer declared (R-R3-46: remoteHardwareConfigFollowsTheCore).
    // RF-Kit is no longer declared either (R-R3-47: the page asks the Core,
    // tst_rfkit_page_master_gate). None reaches local DSP, so the resource
    // audit does not catch them; they are declared.
    // ====================================================================
    void remoteDeclaredUnavailableSetupLeavesSayWhy_data()
    {
        QTest::addColumn<QString>("label");
        QTest::addColumn<QString>("reasonWord");
        QTest::newRow("DDC Routing") << QStringLiteral("DDC Routing")
                                     << QStringLiteral("hardware");
    }

    void remoteDeclaredUnavailableSetupLeavesSayWhy()
    {
        QFETCH(QString, label);
        QFETCH(QString, reasonWord);

        // R-R3-49: DDC Routing is hidden until multi-panadapter receiver
        // routing is built (ddc-routing); its remote reason is checked as
        // the page will be once it is.
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::DdcRouting, true);
        const auto unmark = qScopeGuard([] { UnbuiltFeatures::resetForTest(); });

        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);

        // The leaf says why before the page is ever opened.
        QTreeWidgetItem* const leaf = setupLeaf(dialog, label);
        QVERIFY(leaf != nullptr);
        const QString reason = leaf->toolTip(0);
        QVERIFY2(reason.contains(reasonWord), qPrintable(reason));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY(!dialog.isPageRealizedForTest(label));

        dialog.selectPage(label);
        QWidget* const page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        QVERIFY(!page->isEnabled());
        QCOMPARE(page->toolTip(), reason);
        auto* const notice = dialog.findChild<QLabel*>(QStringLiteral("setupLocalUnavailable"));
        QVERIFY(notice != nullptr);
        QVERIFY(!notice->isHidden());
        QCOMPARE(notice->text(), reason);

        // Not a transmit gate: a Core that permits transmit changes nothing.
        dialog.setTransmitPermitted(true);
        QVERIFY(!page->isEnabled());
        QVERIFY(!notice->isHidden());

        // Local direct mode: live, no reason.
        RadioModel local;
        SetupDialog localDialog(&local);
        QTreeWidgetItem* const localLeaf = setupLeaf(localDialog, label);
        QVERIFY(localLeaf != nullptr);
        QVERIFY(localLeaf->toolTip(0).isEmpty());
        localDialog.selectPage(label);
        QWidget* const localPage = localDialog.realizedPageForTest(label);
        QVERIFY(localPage != nullptr);
        QVERIFY(localPage->isEnabled());
        QVERIFY(localDialog.findChild<QLabel*>(
                    QStringLiteral("setupLocalUnavailable"))->isHidden());
    }

    // ====================================================================
    // R-R3-46: Hardware Config in a remote window.
    // ====================================================================

    // The page is not declared unavailable any more. It shows the Core's
    // radio; against a Core that does not offer Hardware Config its tabs
    // are disabled with the reason above them; with it they are live, and
    // the transmit fields follow the transmit permission with its reason.
    void remoteHardwareConfigFollowsTheCore()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        StationCapabilities caps;
        caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:47");
        caps.board = HPSDRHW::Angelia;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::ANAN100D;
        caps.radioProtocol = 1;
        remote.applyStationCapabilities(caps);
        AlexAntennaFacade* alex = remote.alexAntennaFacade();
        const QString olderCore = QStringLiteral(
            "This Core cannot change its radio's hardware settings for this app. "
            "Updating the Core may help.");
        QVERIFY(OperatorWording::isPlain(olderCore));
        alex->setWindowAvailability(false, olderCore);

        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog);
        QTreeWidgetItem* const leaf = setupLeaf(dialog, QStringLiteral("Hardware Config"));
        QVERIFY(leaf != nullptr);
        QVERIFY(leaf->toolTip(0).isEmpty());
        dialog.selectPage(QStringLiteral("Hardware Config"));
        QWidget* const page = dialog.realizedPageForTest(QStringLiteral("Hardware Config"));
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());  // not the local-DSP gate, not declared
        auto* hardware = qobject_cast<HardwarePage*>(page);
        QVERIFY(hardware != nullptr);
        auto* tabs = hardware->findChild<QTabWidget*>();
        auto* notice = hardware->findChild<QLabel*>(QStringLiteral("hardwareConfigUnavailable"));
        QVERIFY(tabs && notice);
        QVERIFY(!tabs->isEnabled());
        QCOMPARE(tabs->toolTip(), olderCore);
        QVERIFY(!notice->isHidden());
        QCOMPARE(notice->text(), olderCore);
        QVERIFY(!hardware->remoteEditsAvailableForTest());

        alex->setWindowAvailability(true, {});
        QVERIFY(tabs->isEnabled());
        QVERIFY(notice->isHidden());

        // Transmit fields: disabled with the transmit reason; the receive
        // fields beside them stay live.
        const QString transmitReason = QStringLiteral(
            "Remote transmit controls are not available from this Core.");
        auto* antennas = hardware->findChild<AntennaAlexAntennaControlTab*>();
        QVERIFY(antennas != nullptr);
        // Parity Task 12: Antenna Control's transmit half follows whether
        // the Core takes it (radioHardwareVersion 6, RX bypass on TX 5),
        // not the transmit permission; until told, "connect to the Core".
        const QString connectReason = QStringLiteral(
            "Connect to the Core to change the radio's hardware settings.");
        QVERIFY(!antennas->txGridForTest()->isEnabled());
        QCOMPARE(antennas->txGridForTest()->toolTip(), connectReason);
        QVERIFY(!antennas->blockTxAnt2ForTest()->isEnabled());
        QVERIFY(!antennas->rxOutOnTxForTest()->isEnabled());
        QVERIFY(antennas->useTxAntForRxForTest()->isEnabled());
        QVERIFY(antennas->rxButtonForTest(Band::Band40m, 2)->isEnabled());
        const QString olderTx = QStringLiteral(
            "This Core cannot change its radio's transmit antennas for this app. "
            "Updating the Core may help.");
        QVERIFY(OperatorWording::isPlain(olderTx));
        alex->setTransmitEditAvailability(false, olderTx, true, {});
        QVERIFY(!antennas->txGridForTest()->isEnabled());
        QCOMPARE(antennas->txGridForTest()->toolTip(), olderTx);
        QVERIFY(!antennas->ext1OutOnTxForTest()->isEnabled());
        QCOMPARE(antennas->rxOutOverrideForTest()->toolTip(), olderTx);
        QVERIFY(antennas->rxOutOnTxForTest()->isEnabled());
        alex->setTransmitEditAvailability(true, {}, true, {});
        QVERIFY(antennas->txGridForTest()->isEnabled());
        QVERIFY(antennas->blockTxAnt2ForTest()->isEnabled());
        QVERIFY(antennas->ext2OutOnTxForTest()->isEnabled());
        QVERIFY(antennas->txGridForTest()->toolTip().isEmpty());
        // The transmit permission does not take them away.
        dialog.setTransmitPermitted(false, transmitReason);
        QVERIFY(antennas->txGridForTest()->isEnabled());
        const auto group = [hardware](const QString& title) -> QGroupBox* {
            for (QGroupBox* box : hardware->findChildren<QGroupBox*>()) {
                if (box->title() == title) { return box; }
            }
            return nullptr;
        };
        for (const QString& title : {QStringLiteral("External PA control")}) {
            QGroupBox* box = group(title);
            QVERIFY2(box != nullptr, qPrintable(title));
            QVERIFY2(!box->isEnabled(), qPrintable(title));
            QCOMPARE(box->toolTip(), transmitReason);
        }
        // Parity Task 13: the OC transmit pins, pin actions and transmit
        // calibration follow whether the Core takes them
        // (transmitSettingsVersion 8), not the transmit permission; this
        // window has no Core that does (tst_remote_oc_cal covers one that
        // does). User Dig Out follows the transmit settings gate.
        const QString olderSettings = IStationLink::transmitSettingsUnavailableReason();
        const QStringList settingsGroups{QStringLiteral("TX OC Pins per Band"),
                                         QStringLiteral("TX Pin Action mapping"),
                                         QStringLiteral("TX Display Cal"),
                                         QStringLiteral("Volts/Amps Calibration")};
        for (const QString& title : settingsGroups) {
            QGroupBox* box = group(title);
            QVERIFY2(box != nullptr, qPrintable(title));
            QVERIFY2(!box->isEnabled(), qPrintable(title));
            QCOMPARE(box->toolTip(), olderSettings);
        }
        dialog.setTransmitSettingsPermitted(false, olderSettings);
        QVERIFY(!group(QStringLiteral("User Dig Out"))->isEnabled());
        QCOMPARE(group(QStringLiteral("User Dig Out"))->toolTip(), olderSettings);
        for (const QString& title : {QStringLiteral("RX OC Pins per Band"),
                                     QStringLiteral("HPSDR Freq Cal Diagnostic"),
                                     QStringLiteral("USB BCD output")}) {
            QGroupBox* box = group(title);
            QVERIFY2(box != nullptr, qPrintable(title));
            QVERIFY2(box->isEnabled(), qPrintable(title));
        }
        // The HL2's TX buffer latency and PTT hang are transmit settings
        // too (a receive-only Core refuses their keys).
        auto* hl2Options = hardware->findChild<Hl2OptionsTab*>();
        QVERIFY(hl2Options != nullptr);
        QVERIFY(!hl2Options->transmitTimingsEnabledForTest());
        // Follow-up item 4: OC hot switching and the Alex TX master switches.
        const auto check = [hardware](const QString& text) -> QCheckBox* {
            for (QCheckBox* box : hardware->findChildren<QCheckBox*>()) {
                if (box->text() == text) { return box; }
            }
            return nullptr;
        };
        const QStringList transmitChecks{QStringLiteral("Allow hot switching")};
        for (const QString& text : transmitChecks) {
            QCheckBox* box = check(text);
            QVERIFY2(box != nullptr, qPrintable(text));
            QVERIFY2(!box->isEnabled(), qPrintable(text));
            QCOMPARE(box->toolTip(), transmitReason);
        }
        // Parity Task 14: the Alex TX master switches follow whether the
        // Core takes them (radioHardwareVersion 7), not the transmit
        // permission; this window has no Core that does
        // (tst_remote_hl2_io covers one that does).
        const QStringList hpfChecks{QStringLiteral("HPF Bypass on TX"),
                                    QStringLiteral("HPF Bypass on PureSignal feedback"),
                                    QStringLiteral("Disable 6m LNA on TX")};
        for (const QString& text : hpfChecks) {
            QCheckBox* box = check(text);
            QVERIFY2(box != nullptr, qPrintable(text));
            QVERIFY2(!box->isEnabled(), qPrintable(text));
            QCOMPARE(box->toolTip(), IStationLink::alexHpfSwitchesUnavailableReason());
        }

        // A Core that permits transmit lifts it.
        dialog.setTransmitPermitted(true);
        QVERIFY(hl2Options->transmitTimingsEnabledForTest());
        for (const QString& text : transmitChecks) {
            QVERIFY2(check(text)->isEnabled(), qPrintable(text));
        }
        for (const QString& text : hpfChecks) {
            QVERIFY2(!check(text)->isEnabled(), qPrintable(text));
        }
        // It does not open what the Core's transmit settings gate holds.
        for (const QString& title : settingsGroups) {
            QVERIFY2(!group(title)->isEnabled(), qPrintable(title));
        }
        dialog.setTransmitSettingsPermitted(true, QString());
        QVERIFY(group(QStringLiteral("User Dig Out"))->isEnabled());
        QVERIFY(group(QStringLiteral("User Dig Out"))->toolTip() != olderSettings);

        // Local direct mode: everything live, no notice.
        AppSettings::instance().setRemoteBackend(nullptr);
        RadioModel local;
        local.setBoardForTest(HPSDRHW::Angelia);
        SetupDialog localDialog(&local);
        localDialog.selectPage(QStringLiteral("Hardware Config"));
        auto* localHardware = qobject_cast<HardwarePage*>(
            localDialog.realizedPageForTest(QStringLiteral("Hardware Config")));
        QVERIFY(localHardware != nullptr);
        QVERIFY(localHardware->findChild<QTabWidget*>()->isEnabled());
        QVERIFY(localHardware->findChild<QLabel*>(
                    QStringLiteral("hardwareConfigUnavailable"))->isHidden());
        auto* localAntennas = localHardware->findChild<AntennaAlexAntennaControlTab*>();
        QVERIFY(localAntennas->txGridForTest()->isEnabled());
        QVERIFY(localAntennas->blockTxAnt2ForTest()->isEnabled());
        QVERIFY(localHardware->findChild<Hl2OptionsTab*>()->transmitTimingsEnabledForTest());
    }

    // R3 unfinished controls fix wave (R-R3-49, R-R3-21): the groups and
    // rows hidden inside Setup pages the Core owns (DSP > CW, AM/SAM and
    // FM; Hardware Config's OC Outputs extras, antenna conflict policy,
    // XVTR and VHF tabs, HL2 bus 0 and Bandwidth Monitor) are hidden in a
    // connected remote window, where those pages are built for real rather
    // than as placeholders. Marking every feature built shows each of them,
    // so the check cannot pass on a page that lacks them.
    void coreScopedHiddenGroupsHideInAConnectedRemoteWindow_data()
    {
        QTest::addColumn<bool>("hl2");
        QTest::newRow("ANAN-100D") << false;
        QTest::newRow("Hermes Lite 2") << true;
    }

    void coreScopedHiddenGroupsHideInAConnectedRemoteWindow()
    {
        QFETCH(bool, hl2);
        const auto shown = [](QWidget* w, QWidget* page) {
            if (w == nullptr) { return false; }
            for (QWidget* p = w; p != nullptr && p != page; p = p->parentWidget()) {
                const bool stackPage = qobject_cast<QStackedWidget*>(p->parentWidget()) != nullptr;
                if (p->isHidden() && !stackPage) { return false; }
            }
            return true;
        };
        const auto named = [shown](const char* name) {
            return [shown, name](QWidget* page) {
                return shown(page->findChild<QWidget*>(QString::fromLatin1(name)), page);
            };
        };
        const auto text = [shown](const char* caption) {
            return [shown, caption](QWidget* page) {
                const QString want = QString::fromLatin1(caption);
                for (QLabel* l : page->findChildren<QLabel*>()) {
                    if (l->text() == want && shown(l, page)) { return true; }
                }
                for (QAbstractButton* b : page->findChildren<QAbstractButton*>()) {
                    if (b->text() == want && shown(b, page)) { return true; }
                }
                return false;
            };
        };
        const auto tab = [shown](const char* caption) {
            return [shown, caption](QWidget* page) {
                for (QTabWidget* tabs : page->findChildren<QTabWidget*>()) {
                    for (int i = 0; i < tabs->count(); ++i) {
                        if (tabs->tabText(i) == QLatin1String(caption) && tabs->isTabVisible(i)
                            && shown(tabs, page)) {
                            return true;
                        }
                    }
                }
                return false;
            };
        };
        struct Check {
            const char* page;
            const char* what;
            std::function<bool(QWidget*)> shown;
        };
        QList<Check> checks{
            {"CW", "keyer group", named("cwKeyerGroup")},
            {"CW", "timing group", named("cwTimingGroup")},
            {"CW", "peak filter bandwidth", text("Bandwidth")},
            {"CW", "peak filter gain", text("Gain")},
            {"AM/SAM", "synchronous AM group", named("samGroup")},
            {"AM/SAM", "maximum squelch tail", text("Max Tail")},
            {"FM", "deviation", named("fmRxDeviationCombo")},
            {"FM", "de-emphasis", named("fmDeEmphasisButton")},
            {"FM", "FM transmit group", named("fmTxGroup")},
            {"Hardware Config", "antenna conflict policy", named("antennaConflictPolicyGroup")},
            {"Hardware Config", "OC hot switching", named("ocAllowHotSwitching")},
            {"Hardware Config", "OC USB BCD", named("ocUsbBcdGroup")},
            {"Hardware Config", "OC external PA", named("ocExternalPaGroup")},
            {"Hardware Config", "OC VHF tab", tab("VHF")},
        };
        if (!hl2) {
            // The XVTR tab is not offered for the Hermes Lite 2 at all.
            checks << Check{"Hardware Config", "XVTR tab", tab("XVTR")};
        } else {
            checks << Check{"Hardware Config", "HL2 I2C bus 0", named("hl2I2cBus0")}
                   << Check{"Hardware Config", "Bandwidth Monitor tab", tab("Bandwidth Monitor")};
        }

        const auto unmark = qScopeGuard([] { UnbuiltFeatures::resetForTest(); });
        for (const bool allBuilt : {false, true}) {
            UnbuiltFeatures::resetForTest();
            if (allBuilt) {
                for (const UnbuiltFeatures::Entry& entry : UnbuiltFeatures::all()) {
                    UnbuiltFeatures::setBuiltForTest(entry.feature, true);
                }
            }
            SettingsProxy proxy;
            AppSettings::instance().setRemoteBackend(&proxy);
            const auto detach = qScopeGuard([] { AppSettings::instance().setRemoteBackend(nullptr); });
            RadioModel remote(RadioModel::Role::Remote);
            StationCapabilities caps;
            caps.macAddress = hl2 ? QStringLiteral("AA:BB:CC:DD:EE:49")
                                  : QStringLiteral("AA:BB:CC:DD:EE:50");
            caps.board = hl2 ? HPSDRHW::HermesLite : HPSDRHW::Angelia;
            caps.radioConnected = true;
            caps.radioIdentityEntries = true;
            caps.hpsdrModel = hl2 ? HPSDRModel::HERMESLITE : HPSDRModel::ANAN100D;
            caps.radioProtocol = 1;
            remote.applyStationCapabilities(caps);
            remote.alexAntennaFacade()->setWindowAvailability(true, {});
            remote.stepAttFacade()->setWindowAvailability(true, {});

            SetupDialog dialog(&remote);
            connectDialog(proxy, dialog);
            QStringList wrong;
            for (const Check& check : checks) {
                const QString label = QString::fromLatin1(check.page);
                dialog.selectPage(label);
                QWidget* const page = dialog.realizedPageForTest(label);
                QVERIFY2(page != nullptr, check.page);
                QVERIFY2(!dialog.isPagePlaceholderForTest(label), check.page);
                if (check.shown(page) != allBuilt) {
                    wrong << QStringLiteral("%1: %2").arg(label, QString::fromLatin1(check.what));
                }
            }
            QVERIFY2(wrong.isEmpty(),
                     qPrintable((allBuilt ? QStringLiteral("built, not shown: ")
                                          : QStringLiteral("not built, shown: "))
                                + wrong.join(QStringLiteral("; "))));
        }
    }

    // R-R3-46 (carried from the remote window Setup re-review): with the
    // Core's radio known and its settings ready, building every Core and
    // Mixed page outside the local-DSP gate sends the Core no write and no
    // remove. Hardware Config is built with a Core HL2 whose N2ADR switch is
    // saved (its restore used to rewrite the OC matrix).
    void buildingCoreAndMixedPagesSendsTheCoreNothing()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:48");
        StationCapabilities caps;
        caps.macAddress = mac;
        caps.board = HPSDRHW::HermesLite;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::HERMESLITE;
        caps.radioProtocol = 1;
        remote.applyStationCapabilities(caps);
        remote.alexAntennaFacade()->setWindowAvailability(true, {});
        remote.stepAttFacade()->setWindowAvailability(true, {});

        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog, {
            {QStringLiteral("hardware/%1/hl2IoBoard/n2adrFilter").arg(mac), QStringLiteral("True")},
            {QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac), QStringLiteral("96000")},
            {QStringLiteral("hardware/%1/oc/rx/40m/pin3").arg(mac), QStringLiteral("True")},
            {QStringLiteral("hardware/%1/cal/freqFactor").arg(mac), QStringLiteral("1.000001")},
        });
        QVERIFY(proxy.ready());
        QSignalSpy writes(&proxy, &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&proxy, &SettingsProxy::outboundRemoveRequested);

        QStringList offenders;
        int built = 0;
        const QStringList labels = dialog.pageLabelsForTest();
        for (int i = 0; i < dialog.registeredPageCountForTest(); ++i) {
            if (dialog.pageScopeAtForTest(i) == SetupScope::ThisComputer) {
                continue;
            }
            const int writesBefore = writes.size() + removes.size();
            const int handOutsBefore = remote.localDspHandOutCount();
            QWidget* const page = dialog.realizePageAtForTest(i);
            QCoreApplication::processEvents();
            if (page == nullptr || remote.localDspHandOutCount() > handOutsBefore) {
                continue;  // the local-DSP gate's pages are outside this sweep
            }
            ++built;
            if (writes.size() + removes.size() > writesBefore) {
                QStringList keys;
                for (int w = writesBefore; w < writes.size(); ++w) {
                    keys << writes.at(w).at(0).toString();
                }
                offenders << QStringLiteral("%1 (%2)").arg(labels.at(i), keys.join(QStringLiteral(", ")));
            }
        }
        QVERIFY(built >= 20);
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QStringLiteral("; "))));
        QVERIFY(dialog.isPageRealizedForTest(QStringLiteral("Hardware Config")));
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);
    }

    // R-R3-46 (carried): a page's modal window that is not a QDialog still
    // holds the page's rebuild back until it goes.
    void setupPageIsNotRebuiltUnderItsOwnModalWindow()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog, {{QStringLiteral("Region"), QStringLiteral("Japan")}});
        const int index = dialog.registerPageForTest(
            QStringLiteral("Modal owner"), SetupScope::Core, [] { return new QWidget; });
        dialog.show();
        QPointer<QWidget> page = dialog.realizePageAtForTest(index);
        QVERIFY(page);

        auto* modal = new QWidget(page, Qt::Window);
        modal->setWindowModality(Qt::ApplicationModal);
        modal->show();
        QTRY_COMPARE(QApplication::activeModalWidget(), modal);
        proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Italy")}});
        QTest::qWait(100);  // the snapshot's queued rebuild has had its turn
        QVERIFY(!page.isNull());
        QCOMPARE(dialog.realizedPageForTest(QStringLiteral("Modal owner")), page.data());

        delete modal;
        QTRY_VERIFY(page.isNull());
        QVERIFY(dialog.realizedPageForTest(QStringLiteral("Modal owner")) != nullptr);
    }

    // R-R3-46 (carried): a dialog the page keeps after it closes (hidden,
    // never destroyed) runs the postponed rebuild when it closes.
    void keptPageDialogRunsThePostponedRebuildWhenItCloses()
    {
        SettingsProxy proxy;
        AppSettings::instance().setRemoteBackend(&proxy);
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        connectDialog(proxy, dialog, {{QStringLiteral("Region"), QStringLiteral("Japan")}});
        const int index = dialog.registerPageForTest(
            QStringLiteral("Dialog owner"), SetupScope::Core, [] { return new QWidget; });
        dialog.show();
        QPointer<QWidget> page = dialog.realizePageAtForTest(index);
        QVERIFY(page);

        QPointer<QDialog> kept = new QDialog(page);
        kept->open();
        QTRY_VERIFY(kept->isVisible());
        proxy.applySnapshot({{QStringLiteral("Region"), QStringLiteral("Italy")}});
        QTest::qWait(100);
        QVERIFY(!page.isNull());

        kept->accept();  // hidden, not destroyed; the page is not shown again
        QTRY_VERIFY(page.isNull());
        QVERIFY(dialog.realizedPageForTest(QStringLiteral("Dialog owner")) != nullptr);
    }

    // R-R3-46 (carried): a radio-model change narrows the tune-power
    // spinbox's range. The clamp must not write tune power.
    void powerPageRadioChangeDoesNotWriteTunePower()
    {
        RadioModel local;
        TransmitModel& tx = local.transmitModel();
        tx.setTunePower(50);
        QCOMPARE(tx.tunePower(), 50);
        PowerPage page(&local);
        auto* spin = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 50.0);
        QSignalSpy written(&tx, &TransmitModel::tunePowerChanged);
        page.applyHpsdrModel(HPSDRModel::HERMESLITE);  // range becomes -16.5 .. 0 dB
        QCOMPARE(written.count(), 0);
        QCOMPARE(tx.tunePower(), 50);
    }

    // R-R3-46 (carried): the DSP > CW page's sidetone row, built after the
    // Core's radio is known, shows that radio's answer at once.
    void cwSidetoneRowReadsTheCoresRadioAtBuild()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(BoardCapsTable::forBoard(HPSDRHW::HermesLite).hasSidetoneGenerator);
        StationCapabilities caps;
        caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:49");
        caps.board = HPSDRHW::HermesLite;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::HERMESLITE;
        caps.radioProtocol = 1;
        remote.applyStationCapabilities(caps);
        CwSetupPage page(&remote);
        QVERIFY(page.sidetoneRowVisibleForTest());
    }

    // A remote window that does not know the Core's radio yet: its
    // capabilities are the Unknown board's and hasPaProfile is false. Task 16
    // fix wave 2 (the operator's rule, 2026-09-25: disabled with its reason,
    // never hidden): the PA category and its pages are shown, disabled, and
    // say why.
    void remotePaCategoryIsShownDisabledWithTheReason()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        auto* tree = dialog.findChild<QTreeWidget*>();
        QVERIFY(tree != nullptr);
        QTreeWidgetItem* pa = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            if (tree->topLevelItem(i)->text(0) == QStringLiteral("PA")) {
                pa = tree->topLevelItem(i);
            }
        }
        QVERIFY(pa != nullptr);
        QVERIFY(!pa->isHidden());
        QVERIFY(!pa->toolTip(0).isEmpty());
        QVERIFY(OperatorWording::isPlain(pa->toolTip(0)));
        for (const QString& label : {QStringLiteral("PA Gain"), QStringLiteral("Watt Meter"),
                                     QStringLiteral("PA Values")}) {
            QTreeWidgetItem* const leaf = setupLeaf(dialog, label);
            QVERIFY2(leaf != nullptr, qPrintable(label));
            QVERIFY2(!leaf->isHidden(), qPrintable(label));
            QVERIFY2(!leaf->toolTip(0).isEmpty(), qPrintable(label));
            dialog.selectPage(label);
            QWidget* const page = dialog.realizedPageForTest(label);
            QVERIFY2(page != nullptr, qPrintable(label));
            QVERIFY2(!page->isEnabled(), qPrintable(label));
            QVERIFY(OperatorWording::isPlain(page->toolTip()));
        }
    }

    // R-R3-46 / R-R3-10: with the Core's radio known (a Saturn ANAN-G2 1K,
    // which has PA settings) the PA pages are shown, and each follows the
    // transmit permission with its reason until remote transmit.
    void remotePaPagesFollowTheTransmitPermission()
    {
        RadioModel remote(RadioModel::Role::Remote);
        StationCapabilities caps;
        caps.stationName = QStringLiteral("Bench G2 1K");
        caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:46");
        caps.board = HPSDRHW::Saturn;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::ANAN_G2_1K;
        caps.radioProtocol = 2;
        remote.applyStationCapabilities(caps);
        QCOMPARE(remote.hardwareProfile().model, HPSDRModel::ANAN_G2_1K);
        QVERIFY(remote.boardCapabilities().hasPaProfile);

        SetupDialog dialog(&remote);
        auto* tree = dialog.findChild<QTreeWidget*>();
        QVERIFY(tree != nullptr);
        QTreeWidgetItem* pa = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            if (tree->topLevelItem(i)->text(0) == QStringLiteral("PA")) {
                pa = tree->topLevelItem(i);
            }
        }
        QVERIFY(pa != nullptr);
        QVERIFY(!pa->isHidden());

        // R-R3-49 (parity Task 6): the pages are no longer held whole for
        // remote transmit. Each opens and gates its own Core settings on
        // transmitSettingsVersion 6; PA Gain's auto-calibrate sweep, which
        // keys the radio, keeps the transmit permission.
        const QString transmitReason = QStringLiteral(
            "Remote transmit controls are not available from this Core.");
        auto* const notice = dialog.findChild<QLabel*>(
            QStringLiteral("setupTransmitUnavailable"));
        QVERIFY(notice != nullptr);
        for (const QString& label : {QStringLiteral("PA Gain"), QStringLiteral("Watt Meter"),
                                     QStringLiteral("PA Values")}) {
            QTreeWidgetItem* const leaf = setupLeaf(dialog, label);
            QVERIFY2(leaf != nullptr, qPrintable(label));
            QVERIFY2(!leaf->isHidden(), qPrintable(label));
            QVERIFY2(leaf->toolTip(0) != transmitReason, qPrintable(label));
            dialog.selectPage(label);
            QWidget* const page = dialog.realizedPageForTest(label);
            QVERIFY2(page != nullptr, qPrintable(label));
            QVERIFY2(page->isEnabled(), qPrintable(label));
            QVERIFY2(notice->isHidden(), qPrintable(label));
        }
        auto* const paGain = qobject_cast<PaGainByBandPage*>(
            dialog.realizedPageForTest(QStringLiteral("PA Gain")));
        QVERIFY(paGain != nullptr);
        QDoubleSpinBox* const gain20 = paGain->gainSpinForTest(Band::Band20m);
        QCheckBox* const autoCal = paGain->autoCalibrateCheckForTest();
        QVERIFY(gain20 && autoCal);
        // Closed until the version 6 gate is pushed, with the Core reason.
        QVERIFY(!gain20->isEnabled());
        QCOMPARE(gain20->toolTip(), IStationLink::transmitSettingsUnavailableReason());
        dialog.setTransmitSettingsPermitted(true, QString(), 6);
        QVERIFY(gain20->isEnabled());
        QVERIFY(!autoCal->isEnabled());
        QCOMPARE(autoCal->toolTip(), transmitReason);
        dialog.setTransmitSettingsPermitted(false, QStringLiteral("The radio is on the air. Try again when it stops."), 6);
        QVERIFY(!gain20->isEnabled());

        // A Core that permits transmit opens the sweep.
        dialog.setTransmitPermitted(true);
        QVERIFY(autoCal->isEnabled());

        // Local direct mode: the same radio's PA pages are live, no reason.
        RadioModel local;
        local.setHpsdrModelForTest(HPSDRModel::ANAN_G2_1K);
        SetupDialog localDialog(&local);
        for (const QString& label : {QStringLiteral("PA Gain"), QStringLiteral("Watt Meter"),
                                     QStringLiteral("PA Values")}) {
            QTreeWidgetItem* const leaf = setupLeaf(localDialog, label);
            QVERIFY2(leaf != nullptr, qPrintable(label));
            QVERIFY2(leaf->toolTip(0).isEmpty(), qPrintable(label));
            localDialog.selectPage(label);
            QWidget* const page = localDialog.realizedPageForTest(label);
            QVERIFY2(page != nullptr, qPrintable(label));
            QVERIFY2(page->isEnabled(), qPrintable(label));
        }
        QVERIFY(localDialog.findChild<QLabel*>(
                    QStringLiteral("setupTransmitUnavailable"))->isHidden());
    }

    // General > Options keeps its Region and Options groups; only the two
    // attenuator groups, which write the Core's attenuator, are unavailable
    // while no Core takes this window's edits.
    void remoteGeneralOptionsDisablesOnlyTheAttenuatorGroups()
    {
        RadioModel remote(RadioModel::Role::Remote);
        GeneralOptionsPage page(&remote);
        // A remote model builds no step attenuator controller, so what must
        // not move is the widgets' own state (and so anything they persist).
        QVERIFY(remote.stepAttController() == nullptr);
        for (const char* name : {"grpStepAttenuator", "grpAutoAttRx1", "grpAutoAttRx2"}) {
            auto* group = page.findChild<QGroupBox*>(QLatin1String(name));
            QVERIFY2(group != nullptr, name);
            QVERIFY2(!group->isEnabled(), name);
            QVERIFY2(OperatorWording::isPlain(group->toolTip()), qPrintable(group->toolTip()));
            // Activation reaches nothing.
            const QString before = widgetStateOf(group);
            QVERIFY2(!before.isEmpty(), name);
            for (QAbstractButton* button : group->findChildren<QAbstractButton*>()) {
                button->click();
            }
            for (QSpinBox* spin : group->findChildren<QSpinBox*>()) {
                QTest::keyClick(spin, Qt::Key_Up);
            }
            for (QComboBox* combo : group->findChildren<QComboBox*>()) {
                QTest::keyClick(combo, Qt::Key_Down);
            }
            QCOMPARE(widgetStateOf(group), before);
        }
        auto* hardware = page.findChild<QGroupBox*>(QStringLiteral("grpHardwareConfig"));
        QVERIFY(hardware != nullptr);
        QVERIFY(hardware->isEnabled());

        RadioModel local;
        GeneralOptionsPage localPage(&local);
        auto* localStepAtt = localPage.findChild<QGroupBox*>(QStringLiteral("grpStepAttenuator"));
        QVERIFY(localStepAtt != nullptr);
        QVERIFY(localStepAtt->isEnabled());
        QVERIFY(localStepAtt->toolTip().isEmpty());
    }

    // ====================================================================
    // R-R3-21: applet and flag controls the inventory found acting on this
    // computer's own radio connection, amplifier socket or VAX buses.
    // ====================================================================
    void remoteRxAppletAttenuatorRowIsUnavailable()
    {
        RadioModel remote(RadioModel::Role::Remote);
        RxApplet applet(nullptr, &remote);
        auto* att = applet.findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(att != nullptr);
        QVERIFY(!att->isEnabled());
        QVERIFY2(OperatorWording::isPlain(att->toolTip()), qPrintable(att->toolTip()));
        QVERIFY(att->toolTip().contains(QStringLiteral("attenuator")));

        // Activation reaches nothing: the step spin, the preamp combo and
        // (where the board shows it) the RX1 preamp toggle.
        QVERIFY(remote.stepAttController() == nullptr);
        const QString attBefore = widgetStateOf(&applet);
        auto* spin = att->findChild<QSpinBox*>();
        auto* combo = att->findChild<QComboBox*>();
        QVERIFY(spin != nullptr);
        QVERIFY(combo != nullptr);
        QVERIFY(!spin->isEnabled());
        QVERIFY(!combo->isEnabled());
        const int spinBefore = spin->value();
        const int comboBefore = combo->currentIndex();
        QTest::keyClick(spin, Qt::Key_Up);
        QTest::keyClick(combo, Qt::Key_Down);
        if (auto* preamp = applet.findChild<QCheckBox*>(QStringLiteral("RxRx1PreampToggle"))) {
            QVERIFY(!preamp->isEnabled());
            preamp->click();
        }
        QCOMPARE(spin->value(), spinBefore);
        QCOMPARE(combo->currentIndex(), comboBefore);
        QCOMPARE(widgetStateOf(&applet), attBefore);

        RadioModel local;
        RxApplet localApplet(nullptr, &local);
        auto* localAtt = localApplet.findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(localAtt != nullptr);
        QVERIFY(localAtt->isEnabled());
        QVERIFY(localAtt->toolTip().isEmpty());
    }

    // R-R3-46 / R-R3-21: with a Core that takes this window's attenuator
    // edits, the RX applet's row and Setup's two groups are enabled, show
    // the Core's values and write the Core's `stepAtt` object. An older
    // Core's plain reason (through OperatorReasonText) disables them again.
    void remoteAttenuatorRowsFollowTheCoresObject()
    {
        RadioModel remote(RadioModel::Role::Remote);
        RxApplet applet(nullptr, &remote);
        GeneralOptionsPage page(&remote);
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        QVERIFY(stepAtt != nullptr);
        QVERIFY(!stepAtt->isBound());
        auto* att = applet.findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
        auto* spin = att->findChild<QSpinBox*>();
        auto* combo = att->findChild<QComboBox*>();
        auto* stepGroup = page.findChild<QGroupBox*>(QStringLiteral("grpStepAttenuator"));
        auto* autoGroup = page.findChild<QGroupBox*>(QStringLiteral("grpAutoAttRx1"));
        QVERIFY(att && spin && combo && stepGroup && autoGroup);

        // An older Core: the rows stay disabled with its reason, in user words.
        const QString older = OperatorReasonText::forDisplay(QStringLiteral(
            "This Core cannot change its radio's attenuator for this app. Updating the Core may "
            "help."));
        QVERIFY(OperatorWording::isPlain(older));
        stepAtt->setWindowAvailability(false, older);
        for (QWidget* w : {static_cast<QWidget*>(att), static_cast<QWidget*>(stepGroup),
                           static_cast<QWidget*>(autoGroup)}) {
            QVERIFY(!w->isEnabled());
            QCOMPARE(w->toolTip(), older);
        }

        // A supporting Core: its values arrive, then the rows open.
        stepAtt->applyRemoteProperty("minDb", 0);
        stepAtt->applyRemoteProperty("maxDb", 61);
        stepAtt->setEnabled(true);
        stepAtt->setAttenuationDb(20);
        stepAtt->setAutoAttUndoDelayMs(7000);
        stepAtt->setWindowAvailability(true, QString());
        for (QWidget* w : {static_cast<QWidget*>(att), static_cast<QWidget*>(stepGroup),
                           static_cast<QWidget*>(autoGroup)}) {
            QVERIFY(w->isEnabled());
            QVERIFY(w->toolTip().isEmpty());
        }
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("S-ATT"));
        QCOMPARE(spin->maximum(), 61);
        QCOMPARE(spin->value(), 20);
        QCheckBox* pageStepEnable = nullptr;
        for (QCheckBox* box : stepGroup->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("RX1 Enable")) { pageStepEnable = box; }
        }
        QVERIFY(pageStepEnable != nullptr);
        QVERIFY(pageStepEnable->isChecked());
        const QList<QSpinBox*> pageSpins = stepGroup->findChildren<QSpinBox*>();
        QVERIFY(!pageSpins.isEmpty());
        QCOMPARE(pageSpins.first()->value(), 20);
        QCOMPARE(pageSpins.first()->maximum(), 61);

        // The Core moves: every row follows.
        stepAtt->setAttenuationDb(33);
        QCOMPARE(spin->value(), 33);
        QCOMPARE(pageSpins.first()->value(), 33);

        // The window's edits go to the Core's object.
        spin->setValue(12);
        QCOMPARE(stepAtt->attenuationDb(), 12);
        QCOMPARE(pageSpins.first()->value(), 12);
        pageSpins.first()->setValue(15);
        QCOMPARE(stepAtt->attenuationDb(), 15);
        QCOMPARE(spin->value(), 15);
        stepAtt->setEnabled(false);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("ATT"));
        if (combo->count() > 1) {
            combo->setCurrentIndex(1);
            QCOMPARE(stepAtt->preampMode(), combo->itemData(1).toInt());
        }
        QCheckBox* autoEnable = nullptr;
        for (QCheckBox* box : autoGroup->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Enable")) { autoEnable = box; }
        }
        QVERIFY(autoEnable != nullptr);
        autoEnable->click();
        QVERIFY(stepAtt->autoAttEnabled());
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("ATT"));
        auto* hold = autoGroup->findChild<QSpinBox*>();
        QVERIFY(hold != nullptr);
        QCOMPARE(hold->value(), 7);
        hold->setValue(9);
        QCOMPARE(stepAtt->autoAttUndoDelayMs(), 9000);

        // The link goes: the rows close again, with the reason they are given.
        stepAtt->setWindowAvailability(false, older);
        QVERIFY(!att->isEnabled());
        QVERIFY(!stepGroup->isEnabled());
        QCOMPARE(att->toolTip(), older);
    }

    // R-R3-49, R-R3-21 (parity Task 11): the RX applet's XIT row is a slice
    // setting, as the VFO flag's is. A remote model with no transmit
    // permission writes it, and so does a local one.
    void remoteRxAppletXitWritesWithoutTransmitPermission()
    {
        for (const bool remoteRole : {true, false}) {
            RadioModel model(remoteRole ? RadioModel::Role::Remote : RadioModel::Role::Local);
            SliceModel slice(0);
            RxApplet applet(nullptr, &model);
            applet.setSlice(&slice);  // as MainWindow wires it
            QPushButton* xit = nullptr;
            QList<QPushButton*> zeros;  // RIT's and XIT's
            for (QPushButton* b : applet.findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("XIT")) { xit = b; }
                if (b->text() == QStringLiteral("0")) { zeros.append(b); }
            }
            QVERIFY(xit != nullptr);
            QVERIFY(xit->isEnabled());
            QVERIFY(!xit->toolTip().contains(QStringLiteral("permission")));
            QVERIFY(!zeros.isEmpty());
            for (QPushButton* zero : zeros) { QVERIFY(zero->isEnabled()); }

            xit->click();
            QVERIFY(slice.xitEnabled());
            slice.setXitHz(300);
            for (QPushButton* zero : zeros) { zero->click(); }
            QCOMPARE(slice.xitHz(), 0);
            slice.setXitEnabled(false);
            QVERIFY(!xit->isChecked());
        }
    }

    // R-R3-44: VAX works in a remote window. The VAX channels are this
    // computer's, fed from the Core's receiver streams, so the applet, the
    // flag's VAX selector and the overlay's VAX combo are live. A pick lands
    // on the remote slice and is kept on this computer, never in the Core's
    // Slice<N>/VaxChannel. The applet's TX row (VAX as the microphone)
    // waits for remote transmit with a plain reason.
    void remoteVaxSurfacesWorkOnThisComputer()
    {
        RadioModel remote(RadioModel::Role::Remote);
        RadioModel local;
        QVERIFY(remote.addSliceWithStationId(0) >= 0);
        SliceModel* const slice = remote.sliceById(0);
        QVERIFY(slice != nullptr);
        QList<std::pair<int, int>> stored;
        remote.setRemoteVaxChannelStore([&stored](int sliceId, int channel) {
            stored.append({sliceId, channel});
        });

        // VAX applet: live, with this computer's engine.
        VaxApplet remoteApplet(&remote, remote.localAudioDevices());
        QVERIFY(remoteApplet.isEnabled());
        QVERIFY(remoteApplet.toolTip().isEmpty());
        // This computer's own rows, not the "Core computer" section's
        // (iPhone app plan Task 25), which holds the Core computer's VAX.
        const auto ownRows = [](const VaxApplet& applet, auto* tag) {
            using W = std::remove_pointer_t<decltype(tag)>;
            QWidget* const station = applet.stationSectionForTest();
            QList<W*> own;
            for (W* w : applet.findChildren<W*>()) {
                if (station == nullptr || !station->isAncestorOf(w)) { own.append(w); }
            }
            return own;
        };
        QPushButton* mute = nullptr;
        for (QPushButton* b : ownRows(remoteApplet, static_cast<QPushButton*>(nullptr))) {
            if (b->text() == QStringLiteral("Mute") && !mute) { mute = b; }
        }
        QVERIFY(mute != nullptr);
        QVERIFY(mute->isEnabled());
        {
            QSignalSpy muted(remote.localAudioDevices(), &AudioEngine::vaxMutedChanged);
            mute->click();
            QCOMPARE(muted.count(), 1);
            QVERIFY(remote.localAudioDevices()->vaxMuted(1));
            mute->click();
            QVERIFY(!remote.localAudioDevices()->vaxMuted(1));
        }
        // The TX row waits for remote transmit, and says so plainly.
        const QList<MeterSlider*> sliders =
            ownRows(remoteApplet, static_cast<MeterSlider*>(nullptr));
        QCOMPARE(sliders.size(), 5);
        MeterSlider* const txRow = sliders.constLast();
        QVERIFY(!txRow->isEnabled());
        QVERIFY2(OperatorWording::isPlain(txRow->toolTip()), qPrintable(txRow->toolTip()));
        for (int i = 0; i < 4; ++i) {
            QVERIFY(sliders.at(i)->isEnabled());
        }
        const QString reason = QStringLiteral("Remote transmit controls are not available from this Core.");
        remoteApplet.setTransmitPermitted(false, reason);
        QCOMPARE(txRow->toolTip(), reason);
        remoteApplet.setTransmitPermitted(true, QString());
        QVERIFY(txRow->isEnabled());
        QVERIFY(txRow->toolTip() != reason);

        // The "Core computer" section: built with its own four RX rows
        // and TX row, apart from this computer's, and usable only while a
        // Core sends its `vax` object. None has here, so it stays in place
        // disabled with the plain reason (disabled, never hidden). Its TX
        // row keeps following the transmit permission, but while the
        // section cannot be used the row shows the section's reason.
        const QString noCoreVax =
            QStringLiteral("The Core computer is not sharing its VAX channels.");
        QWidget* const station = remoteApplet.stationSectionForTest();
        QVERIFY(station != nullptr);
        QVERIFY(station->isVisibleTo(&remoteApplet));
        QVERIFY(!station->isEnabled());
        QCOMPARE(station->toolTip(), noCoreVax);
        const QList<MeterSlider*> stationSliders = station->findChildren<MeterSlider*>();
        QCOMPARE(stationSliders.size(), 5);
        for (int channel = 1; channel <= 4; ++channel) {
            MeterSlider* const rx = remoteApplet.stationRxMeterForTest(channel);
            QVERIFY(rx != nullptr);
            QVERIFY(stationSliders.contains(rx));
            QVERIFY(!sliders.contains(rx));
        }
        MeterSlider* const stationTx = remoteApplet.stationTxMeterForTest();
        QVERIFY(stationTx != nullptr);
        QVERIFY(stationSliders.contains(stationTx));
        QVERIFY(stationTx != txRow);
        remoteApplet.setStationTransmitPermitted(false, reason);
        QVERIFY(!stationTx->isEnabledTo(station));
        QVERIFY(!stationTx->isEnabled());
        QCOMPARE(stationTx->toolTip(), noCoreVax);
        remoteApplet.setStationTransmitPermitted(false, QString());
        QVERIFY(!stationTx->isEnabledTo(station));
        QVERIFY2(OperatorWording::isPlain(stationTx->toolTip()), qPrintable(stationTx->toolTip()));
        remoteApplet.setStationTransmitPermitted(true, QString());
        QVERIFY(stationTx->isEnabledTo(station));
        QVERIFY(!stationTx->isEnabled());
        QCOMPARE(stationTx->toolTip(), noCoreVax);
        // Its permission is its own: this computer's TX row is untouched.
        QVERIFY(txRow->isEnabled());

        VaxApplet localApplet(&local, local.localAudioDevices());
        QVERIFY(localApplet.isEnabled());
        const QList<MeterSlider*> localSliders =
            ownRows(localApplet, static_cast<MeterSlider*>(nullptr));
        QCOMPARE(localSliders.size(), 5);
        QVERIFY(localSliders.constLast()->isEnabled());
        // JJ's ruling (2026-09-30): a window that runs the radio directly
        // can never have the section, so it is hidden there.
        QVERIFY(localApplet.stationSectionForTest() != nullptr);
        QVERIFY(!localApplet.stationSectionForTest()->isVisibleTo(&localApplet));

        // VFO flag's VAX tab selector: a pick moves the remote slice.
        VfoWidget remoteFlag;
        remoteFlag.setRadioModel(&remote);
        remoteFlag.setSlice(slice);
        auto* selector = remoteFlag.findChild<VaxChannelSelector*>();
        QVERIFY(selector != nullptr);
        QVERIFY(selector->isEnabled());
        QVERIFY(!selector->toolTip().contains(QStringLiteral("not available")));
        selector->simulateClick(2);
        QCOMPARE(slice->vaxChannel(), 2);
        QCOMPARE(stored.constLast(), (std::pair<int, int>{0, 2}));

        // Spectrum overlay VAX flyout, bound to the remote slice.
        QWidget host;
        auto* panel = new SpectrumOverlayPanel(&host);
        panel->setSliceResolver([slice]() { return slice; });
        panel->setRadioModel(&remote);
        auto* combo = host.findChild<QComboBox*>(QStringLiteral("vaxCombo"));
        QVERIFY(combo != nullptr);
        QVERIFY(combo->isEnabled());
        QCOMPARE(combo->currentIndex(), 2);
        combo->setCurrentIndex(4);
        QCOMPARE(slice->vaxChannel(), 4);
        QCOMPARE(stored.constLast(), (std::pair<int, int>{0, 4}));
        // The IQ channel combo was removed (R-R3-49): not built remotely either.
        QVERIFY(host.findChild<QComboBox*>(QStringLiteral("vaxIqCombo")) == nullptr);

        // Never the Core's key.
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Slice0/VaxChannel")));
    }

    // R-R3-21 / R-R3-22: OPERATE waits for remote transmit. Disconnect
    // and Reconnect ask the Core (tst_remote_peripherals); with no Core
    // offering Power Genius control the item stays off and says why.
    void remoteAmplifierAppletControlsAreUnavailable()
    {
        RadioModel remote(RadioModel::Role::Remote);
        AmpApplet applet(&remote);
        QSignalSpy toggles(&applet, &AmpApplet::connectionToggleRequested);
        QSignalSpy operate(&applet, &AmpApplet::operateToggled);

        QPushButton* operateBtn = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("OPERATE")) { operateBtn = b; }
        }
        QVERIFY(operateBtn != nullptr);
        QVERIFY(!operateBtn->isEnabled());
        operateBtn->click();
        QCOMPARE(operate.count(), 0);

        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("Connect")
                || a->text() == QStringLiteral("Disconnect")) {
                toggle = a;
            }
        }
        QVERIFY(toggle != nullptr);
        QVERIFY(!toggle->isEnabled());
        QVERIFY2(OperatorWording::isPlain(toggle->toolTip()), qPrintable(toggle->toolTip()));
        QCOMPARE(toggle->toolTip(),
                 QStringLiteral("This Core does not offer Power Genius XL control to this app."));
        QCOMPARE(operateBtn->toolTip(), AmpApplet::remoteUnavailableReason());
        toggle->trigger();
        QCOMPARE(toggles.count(), 0);

        RadioModel local;
        AmpApplet localApplet(&local);
        std::unique_ptr<QMenu> localMenu(localApplet.buildContextMenuForTesting());
        QSignalSpy localToggles(&localApplet, &AmpApplet::connectionToggleRequested);
        for (QAction* a : localMenu->actions()) {
            if (a->text() == QStringLiteral("Connect")) {
                QVERIFY(a->isEnabled());
                a->trigger();
            }
        }
        QCOMPARE(localToggles.count(), 1);
    }

    // ====================================================================
    // R-R3-21 fix wave: transmit sections inside receive Setup pages.
    //
    // DSP > AGC/ALC carries the TX Leveler and TX ALC groups and DSP >
    // Options carries a TX combo per mode for buffer size, filter size and
    // filter type. The TX Leveler and TX ALC edits are not mirrored, so they
    // would land only in this window's TransmitModel; the DSP > Options TX
    // combos write station transmit settings (the DspOptions keys are
    // station-scoped). Each follows the transmit permission the rest of the
    // transmit surfaces follow. The pages stay live for their receive
    // halves.
    // ====================================================================
    void remoteAgcAlcTransmitGroupsFollowThePermission()
    {
        const QString reason = QStringLiteral("Remote transmit is unavailable");
        const auto findGroup = [](QWidget* page, const QString& title) {
            for (QGroupBox* group : page->findChildren<QGroupBox*>()) {
                if (group->title() == title) { return group; }
            }
            return static_cast<QGroupBox*>(nullptr);
        };

        RadioModel remote(RadioModel::Role::Remote);
        remote.addSliceWithStationId(0, QStringLiteral("pan-0"));
        QVERIFY(remote.activeSlice() != nullptr);
        SetupDialog dialog(&remote);
        // R-R3-49 (parity Task 4): the groups follow the transmit settings
        // gate at version 4 (the Core mirrors them), not the transmit
        // permission.
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is not here yet"));
        dialog.setTransmitSettingsPermitted(false, reason, 4);
        dialog.selectPage(QStringLiteral("AGC/ALC"));
        QWidget* const page = dialog.realizedPageForTest(QStringLiteral("AGC/ALC"));
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());
        QGroupBox* const leveler = findGroup(page, QStringLiteral("TX Leveler"));
        QGroupBox* const alc = findGroup(page, QStringLiteral("TX ALC"));
        QVERIFY(leveler != nullptr);
        QVERIFY(alc != nullptr);
        for (QGroupBox* group : {leveler, alc}) {
            QVERIFY2(!group->isEnabled(), qPrintable(group->title()));
            QCOMPARE(group->toolTip(), reason);
            QCOMPARE(group->accessibleDescription(), reason);
        }
        // The receive AGC controls on the same page stay live.
        for (QGroupBox* group : page->findChildren<QGroupBox*>()) {
            if (group != leveler && group != alc && !leveler->isAncestorOf(group)
                && !alc->isAncestorOf(group) && group->title().startsWith(QStringLiteral("AGC"))) {
                QVERIFY2(group->isEnabled(), qPrintable(group->title()));
            }
        }

        // Activation writes nothing.
        const TransmitModel& tx = remote.transmitModel();
        const bool levelerOn = tx.txLevelerOn();
        const int levelerMax = tx.txLevelerMaxGain();
        const int levelerDecay = tx.txLevelerDecay();
        const int alcMax = tx.txAlcMaxGain();
        const int alcDecay = tx.txAlcDecay();
        for (QGroupBox* group : {leveler, alc}) {
            for (QAbstractButton* button : group->findChildren<QAbstractButton*>()) {
                button->click();
            }
            for (QSpinBox* spin : group->findChildren<QSpinBox*>()) {
                QVERIFY(!spin->isEnabled());
                QTest::keyClick(spin, Qt::Key_Up);
            }
        }
        QCOMPARE(tx.txLevelerOn(), levelerOn);
        QCOMPARE(tx.txLevelerMaxGain(), levelerMax);
        QCOMPARE(tx.txLevelerDecay(), levelerDecay);
        QCOMPARE(tx.txAlcMaxGain(), alcMax);
        QCOMPARE(tx.txAlcDecay(), alcDecay);

        // The gate restores the groups and their own tooltips, with remote
        // transmit still denied; closing it gates them again with the reason
        // MainWindow passes.
        dialog.setTransmitSettingsPermitted(true, QString(), 4);
        for (QGroupBox* group : {leveler, alc}) {
            QVERIFY(group->isEnabled());
            QVERIFY(group->toolTip().isEmpty());
            QVERIFY(group->accessibleDescription().isEmpty());
        }
        dialog.setTransmitSettingsPermitted(false, QString(), 4);
        QVERIFY(!leveler->isEnabled());
        QVERIFY2(OperatorWording::isPlain(leveler->toolTip()), qPrintable(leveler->toolTip()));

        // A page built on its own for a remote model starts denied.
        AgcAlcSetupPage standalone(&remote);
        QGroupBox* const standaloneLeveler = findGroup(&standalone, QStringLiteral("TX Leveler"));
        QVERIFY(standaloneLeveler != nullptr);
        QVERIFY(!standaloneLeveler->isEnabled());

        // Local direct mode: live, and the controls still write.
        RadioModel local;
        local.addSlice();
        SetupDialog localDialog(&local);
        localDialog.selectPage(QStringLiteral("AGC/ALC"));
        QWidget* const localPage = localDialog.realizedPageForTest(QStringLiteral("AGC/ALC"));
        QVERIFY(localPage != nullptr);
        QGroupBox* const localLeveler = findGroup(localPage, QStringLiteral("TX Leveler"));
        QVERIFY(localLeveler != nullptr);
        QVERIFY(localLeveler->isEnabled());
        QVERIFY(localLeveler->toolTip().isEmpty());
        const bool localOn = local.transmitModel().txLevelerOn();
        localLeveler->findChild<QCheckBox*>()->click();
        QCOMPARE(local.transmitModel().txLevelerOn(), !localOn);
    }

    void remoteDspOptionsTransmitCombosFollowThePermission()
    {
        // R-R3-49 (parity Task 1): the nine TX combos follow the transmit
        // settings gate; the keying gate no longer holds them.
        const QString reason = QStringLiteral("The radio is on the air. Try again when it stops.");
        const QStringList txKeys = {
            QStringLiteral("DspOptionsBufferSizePhoneTx"),
            QStringLiteral("DspOptionsBufferSizeFmTx"),
            QStringLiteral("DspOptionsBufferSizeDigTx"),
            QStringLiteral("DspOptionsFilterSizePhoneTx"),
            QStringLiteral("DspOptionsFilterSizeFmTx"),
            QStringLiteral("DspOptionsFilterSizeDigTx"),
            QStringLiteral("DspOptionsFilterTypePhoneTx"),
            QStringLiteral("DspOptionsFilterTypeFmTx"),
            QStringLiteral("DspOptionsFilterTypeDigTx"),
        };

        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        dialog.setTransmitSettingsPermitted(false, reason);
        // "Options" is also a General leaf; select the one under DSP.
        auto* tree = dialog.findChild<QTreeWidget*>();
        QVERIFY(tree != nullptr);
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            if ((*it)->text(0) == QStringLiteral("Options") && (*it)->parent()
                && (*it)->parent()->text(0) == QStringLiteral("DSP")) {
                tree->setCurrentItem(*it);
            }
        }
        DspOptionsPage* page = dialog.findChild<DspOptionsPage*>();
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());

        QMap<QString, QVariant> before;
        for (const QString& key : txKeys) {
            auto* combo = page->findChild<QComboBox*>(key);
            QVERIFY2(combo != nullptr, qPrintable(key));
            QVERIFY2(!combo->isEnabled(), qPrintable(key));
            QCOMPARE(combo->toolTip(), reason);
            QCOMPARE(combo->accessibleDescription(), reason);
            before.insert(key, AppSettings::instance().value(key));
            QTest::keyClick(combo, Qt::Key_Down);
        }
        for (const QString& key : txKeys) {
            QCOMPARE(AppSettings::instance().value(key), before.value(key));
        }
        // The receive combos beside them stay live.
        auto* phoneRx = page->findChild<QComboBox*>(QStringLiteral("DspOptionsBufferSizePhoneRx"));
        QVERIFY(phoneRx != nullptr);
        QVERIFY(phoneRx->isEnabled());

        // The keying gate alone does not open them.
        dialog.setTransmitPermitted(true);
        for (const QString& key : txKeys) {
            QVERIFY2(!page->findChild<QComboBox*>(key)->isEnabled(), qPrintable(key));
        }
        // Keying still refused, settings taken: the combos are live.
        dialog.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        dialog.setTransmitSettingsPermitted(true);
        for (const QString& key : txKeys) {
            auto* combo = page->findChild<QComboBox*>(key);
            QVERIFY2(combo->isEnabled(), qPrintable(key));
            QVERIFY(combo->toolTip() != reason);
            QVERIFY(combo->accessibleDescription().isEmpty());
        }
        // A combo change writes the key the Core's settings proxy carries.
        auto* first = page->findChild<QComboBox*>(txKeys.first());
        const QString firstBefore = first->currentText();
        QTest::keyClick(first, Qt::Key_Down);
        QVERIFY(first->currentText() != firstBefore);
        QCOMPARE(AppSettings::instance().value(txKeys.first()).toString(), first->currentText());

        // A page built on its own for a remote model starts denied, with the
        // Core reason.
        DspOptionsPage standalone(&remote);
        auto* standaloneCombo = standalone.findChild<QComboBox*>(txKeys.first());
        QVERIFY(!standaloneCombo->isEnabled());
        QCOMPARE(standaloneCombo->toolTip(),
                 QStringLiteral("This Core does not let this app change transmit settings. "
                                "Updating the Core may help."));
        QVERIFY2(OperatorWording::isPlain(standaloneCombo->toolTip()),
                 qPrintable(standaloneCombo->toolTip()));

        RadioModel local;
        DspOptionsPage localPage(&local);
        for (const QString& key : txKeys) {
            auto* combo = localPage.findChild<QComboBox*>(key);
            QVERIFY2(combo != nullptr, qPrintable(key));
            QVERIFY2(combo->isEnabled(), qPrintable(key));
        }
        auto* localCombo = localPage.findChild<QComboBox*>(txKeys.first());
        const QString localBefore = localCombo->currentText();
        QTest::keyClick(localCombo, Qt::Key_Down);
        QVERIFY(localCombo->currentText() != localBefore);
        QCOMPARE(AppSettings::instance().value(txKeys.first()).toString(),
                 localCombo->currentText());
    }

    // ====================================================================
    // R-R3-21 fix wave: the RADE applet. Its profile combo writes the same
    // microphone profile Audio > TX Profile gates, and Reset vocoder acts on
    // a RADE channel only this computer's own DSP could hold.
    // ====================================================================
    void remoteRadeAppletFollowsTheTransmitPermission()
    {
        const QString reason = QStringLiteral("Remote transmit is unavailable");
        RadioModel remote(RadioModel::Role::Remote);
        remote.addSliceWithStationId(0, QStringLiteral("pan-0"));
        QVERIFY(remote.activeSlice() != nullptr);
        MicProfileManager* const mgr = remote.micProfileManager();
        QVERIFY(mgr != nullptr);
        // R-R3-49 (parity Task 3): a remote window's profiles are the Core's.
        QVERIFY(mgr->isStationMirror());
        mgr->applyStationProfiles({QStringLiteral("AM"), QStringLiteral("Default"),
                                   QStringLiteral("RADE")});
        mgr->applyStationActiveProfile(QStringLiteral("Default"));
        QVERIFY(mgr->profileNames().size() > 1);

        remote.resetLocalDspHandOutAudit();
        RadeApplet applet(&remote);
        QComboBox* const combo = applet.profileComboForTest();
        QPushButton* const reset = applet.resetVocoderButtonForTest();
        QVERIFY(combo != nullptr);
        QVERIFY(reset != nullptr);
        QVERIFY(!combo->isEnabled());
        QVERIFY(!reset->isEnabled());
        QVERIFY2(OperatorWording::isPlain(combo->toolTip()), qPrintable(combo->toolTip()));
        QVERIFY2(OperatorWording::isPlain(reset->toolTip()), qPrintable(reset->toolTip()));

        // R-R3-49 (parity Task 3): both follow the transmit settings gate
        // (setTxProfilePermitted), not remote transmit.
        applet.setTxProfilePermitted(false, reason);
        QCOMPARE(combo->toolTip(), reason);
        QCOMPARE(reset->toolTip(), reason);

        // While it is closed, nothing is asked of the Core.
        const QString activeBefore = mgr->activeProfileName();
        QTest::keyClick(combo, Qt::Key_Down);
        emit combo->textActivated(combo->itemText(combo->count() - 1));
        QCOMPARE(mgr->activeProfileName(), activeBefore);
        reset->click();
        emit mgr->profileListChanged();
        QVERIFY(!combo->isEnabled());
        // Nothing here looked up this window's own DSP.
        QCOMPARE(remote.localDspHandOutCount(), 0);

        // Remote transmit does not open them; the transmit settings gate
        // does, both of them (Reset vocoder resets the Core's vocoder).
        applet.setTransmitPermitted(true);
        QVERIFY(!combo->isEnabled());
        applet.setTxProfilePermitted(true);
        QVERIFY(combo->isEnabled());
        QVERIFY(combo->toolTip() != reason);
        QVERIFY(reset->isEnabled());
        applet.setTransmitPermitted(false, reason);
        QVERIFY(combo->isEnabled());
        applet.setTxProfilePermitted(false, reason);
        QVERIFY(!combo->isEnabled());
        QCOMPARE(reset->toolTip(), reason);
        QCOMPARE(reset->accessibleDescription(), reason);
        QCOMPARE(remote.localDspHandOutCount(), 0);

        RadioModel local;
        local.addSlice();
        MicProfileManager* const localMgr = local.micProfileManager();
        localMgr->setMacAddress(QStringLiteral("00:11:22:33:44:55"));
        localMgr->load();
        RadeApplet localApplet(&local);
        QComboBox* const localCombo = localApplet.profileComboForTest();
        QVERIFY(localCombo->isEnabled());
        QVERIFY(localCombo->toolTip().isEmpty());
        const QString target = localCombo->itemText(0) != localMgr->activeProfileName()
            ? localCombo->itemText(0) : localCombo->itemText(1);
        emit localCombo->textActivated(target);
        QCOMPARE(localMgr->activeProfileName(), target);
    }

    // ====================================================================
    // R-R3-21 fix wave: the RF-Kit RF2K-S applet. A Core with RF-Kit
    // enabled shows it in a remote window (rfKitEnabled is mirrored).
    // On a Core below remoteRfKitControlVersion 4, OPERATE and the antenna
    // buttons stay greyed with the older reason; from version 4 they ask
    // the Core (parity Task 10). Disconnect/Reconnect ask the Core, which
    // owns the amp (R-R3-22).
    // ====================================================================
    void remoteRfKitAppletControlsAreUnavailable()
    {
        RadioModel remote(RadioModel::Role::Remote);
        Rf2ksApplet applet(&remote);
        QSignalSpy operate(&applet, &Rf2ksApplet::operateToggled);
        QSignalSpy antenna(&applet, &Rf2ksApplet::antennaRequested);
        QSignalSpy toggles(&applet, &Rf2ksApplet::connectionToggleRequested);

        QPushButton* operateBtn = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("STANDBY")) { operateBtn = b; }
        }
        QVERIFY(operateBtn != nullptr);
        QVERIFY(!operateBtn->isEnabled());
        QCOMPARE(operateBtn->toolTip(), AmpApplet::remoteUnavailableReason());
        QVERIFY2(OperatorWording::isPlain(operateBtn->toolTip()), qPrintable(operateBtn->toolTip()));
        operateBtn->click();
        applet.clickOperateButtonForTesting();
        QCOMPARE(operate.count(), 0);

        // The amplifier reporting its antennas must not re-enable them.
        QList<RfKitAntenna> antennas;
        for (int i = 1; i <= 4; ++i) {
            RfKitAntenna a;
            a.type = RfKitAntenna::Type::Internal;
            a.number = i;
            a.state = RfKitAntenna::State::Available;
            antennas << a;
        }
        applet.setAntennas(antennas);
        for (int i = 1; i <= 4; ++i) {
            QVERIFY(!applet.antennaButtonIsEnabledForTesting(i));
            applet.clickAntennaButtonForTesting(i);
        }
        QCOMPARE(antenna.count(), 0);

        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("Connect")
                || a->text() == QStringLiteral("Disconnect")) {
                toggle = a;
            }
        }
        QVERIFY(toggle != nullptr);
        // R-R3-47: with no Core offering RF-Kit setup, the toggle stays off
        // and says why (with one, it asks the Core: tst_remote_peripherals).
        QVERIFY(!toggle->isEnabled());
        QCOMPARE(toggle->toolTip(),
                 QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app."));
        QVERIFY(OperatorWording::isPlain(toggle->toolTip()));
        toggle->trigger();
        QCOMPARE(toggles.count(), 0);

        RadioModel local;
        Rf2ksApplet localApplet(&local);
        QSignalSpy localOperate(&localApplet, &Rf2ksApplet::operateToggled);
        QSignalSpy localAntenna(&localApplet, &Rf2ksApplet::antennaRequested);
        QSignalSpy localToggles(&localApplet, &Rf2ksApplet::connectionToggleRequested);
        localApplet.clickOperateButtonForTesting();
        QCOMPARE(localOperate.count(), 1);
        localApplet.setAntennas(antennas);
        QVERIFY(localApplet.antennaButtonIsEnabledForTesting(1));
        localApplet.clickAntennaButtonForTesting(1);
        QCOMPARE(localAntenna.count(), 1);
        std::unique_ptr<QMenu> localMenu(localApplet.buildContextMenuForTesting());
        for (QAction* a : localMenu->actions()) {
            if (a->text() == QStringLiteral("Connect")) {
                QVERIFY(a->isEnabled());
                a->trigger();
            }
        }
        QCOMPARE(localToggles.count(), 1);
    }

    // ====================================================================
    // R-R3-21 fix wave: the RX applet's Shift-click on a filter preset also
    // matches the TX passband. R-R3-49 (parity Task 1): that half is a
    // transmit setting. It follows the transmit settings gate and, when the
    // gate is closed, says why instead of skipping silently.
    // ====================================================================
    void remoteRxAppletShiftClickFollowsTheTransmitSettingsGate()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SliceModel slice(0);
        slice.setDspMode(DSPMode::USB);
        RxApplet applet(&slice, &remote);
        applet.show();
        TransmitModel& tx = remote.transmitModel();
        const int lowBefore = tx.filterLow();
        const int highBefore = tx.filterHigh();
        QSignalSpy txFilterChanged(&tx, &TransmitModel::filterChanged);
        QSignalSpy refused(&applet, &RxApplet::transmitSettingRefused);
        const auto shiftClickEveryPreset = [&applet] {
            int presets = 0;
            for (QPushButton* b : applet.findChildren<QPushButton*>()) {
                if (!b->isCheckable() || !b->toolTip().contains(QStringLiteral(" Hz to "))) {
                    continue;
                }
                ++presets;
                QTest::mouseClick(b, Qt::LeftButton, Qt::ShiftModifier);
            }
            return presets;
        };
        // A remote window starts with the gate closed: the Core reason.
        const int presets = shiftClickEveryPreset();
        QVERIFY(presets > 1);
        QCOMPARE(txFilterChanged.count(), 0);
        QCOMPARE(tx.filterLow(), lowBefore);
        QCOMPARE(tx.filterHigh(), highBefore);
        QCOMPARE(refused.count(), presets);
        QCOMPARE(refused.last().at(0).toString(),
                 QStringLiteral("This Core does not let this app change transmit settings. "
                                "Updating the Core may help."));

        // On the air: the on-air reason.
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        applet.setTransmitSettingsPermitted(false, onAir);
        shiftClickEveryPreset();
        QCOMPARE(txFilterChanged.count(), 0);
        QCOMPARE(refused.last().at(0).toString(), onAir);

        // Open (the Core takes it, off the air): the TX passband moves, as
        // in a local window, with the keying gate still closed.
        refused.clear();
        applet.setTransmitSettingsPermitted(true);
        shiftClickEveryPreset();
        QVERIFY(txFilterChanged.count() > 0);
        QCOMPARE(refused.count(), 0);

        // Local direct mode: the same Shift-click moves the TX passband,
        // so the remote half above is not vacuous.
        RadioModel local;
        SliceModel localSlice(0);
        localSlice.setDspMode(DSPMode::USB);
        RxApplet localApplet(&localSlice, &local);
        localApplet.show();
        TransmitModel& localTx = local.transmitModel();
        QSignalSpy localTxFilterChanged(&localTx, &TransmitModel::filterChanged);
        for (QPushButton* b : localApplet.findChildren<QPushButton*>()) {
            if (!b->isCheckable() || !b->toolTip().contains(QStringLiteral(" Hz to "))) {
                continue;
            }
            QTest::mouseClick(b, Qt::LeftButton, Qt::ShiftModifier);
        }
        QVERIFY(localTxFilterChanged.count() > 0);
    }

    // ====================================================================
    // R-R3-49 (parity Task 1): the TX applet's RF Power and TX filter follow
    // the transmit settings gate; MOX, TUNE, VOX and 2-Tone keep the keying
    // gate.
    // ====================================================================
    void remoteTxAppletSettingsFollowTheTransmitSettingsGate()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TxApplet applet(&remote);
        const auto findButton = [&applet](const QString& accessibleName) {
            for (QPushButton* button : applet.findChildren<QPushButton*>()) {
                if (button->accessibleName() == accessibleName) {
                    return button;
                }
            }
            return static_cast<QPushButton*>(nullptr);
        };
        QSpinBox* low = nullptr;
        QSpinBox* high = nullptr;
        for (QSpinBox* spin : applet.findChildren<QSpinBox*>()) {
            if (spin->accessibleName() == QStringLiteral("TX filter low cutoff")) { low = spin; }
            if (spin->accessibleName() == QStringLiteral("TX filter high cutoff")) { high = spin; }
        }
        QVERIFY(low && high);
        QPushButton* const mox = findButton(QStringLiteral("MOX transmit"));
        QPushButton* const tune = findButton(QStringLiteral("Tune carrier"));
        QVERIFY(mox && tune);
        QSlider* const power = applet.rfPowerSlider();

        // A remote window starts with both closed; the settings say the Core
        // reason.
        const QString coreReason = QStringLiteral(
            "This Core does not let this app change transmit settings. Updating the Core may help.");
        for (QWidget* w : std::initializer_list<QWidget*>{power, low, high}) {
            QVERIFY(!w->isEnabled());
            QCOMPARE(w->toolTip(), coreReason);
        }

        // Settings open, keying closed.
        applet.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
        applet.setTransmitSettingsPermitted(true);
        for (QWidget* w : std::initializer_list<QWidget*>{power, low, high}) {
            QVERIFY(w->isEnabled());
            QVERIFY(w->toolTip() != coreReason);
        }
        QVERIFY(!mox->isEnabled());
        QVERIFY(!tune->isEnabled());
        QVERIFY(!applet.tunePowerSlider()->isEnabled());
        QVERIFY(!applet.twoToneButton()->isEnabled());

        // The window's own model takes the change; the link sends it on.
        const int target = power->value() == 30 ? 31 : 30;
        power->setValue(target);
        QCOMPARE(remote.transmitModel().power(), target);
        high->setValue(2600);
        QCOMPARE(remote.transmitModel().filterHigh(), 2600);

        // On the air: the settings grey with the on-air reason.
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        applet.setTransmitSettingsPermitted(false, onAir);
        for (QWidget* w : std::initializer_list<QWidget*>{power, low, high}) {
            QVERIFY(!w->isEnabled());
            QCOMPARE(w->toolTip(), onAir);
            QVERIFY2(OperatorWording::isPlain(w->toolTip()), qPrintable(w->toolTip()));
        }

        // Local: both open, nothing pushed.
        RadioModel local;
        TxApplet localApplet(&local);
        QVERIFY(localApplet.rfPowerSlider()->isEnabled());
    }

    // ====================================================================
    // R-R3-21 fix wave: the default reason every transmit surface shows
    // before MainWindow pushes its own is plain English too.
    // ====================================================================
    void defaultTransmitReasonsArePlainEnglish()
    {
        const QString expected = QStringLiteral(
            "Transmit controls are unavailable until the Core confirms "
            "transmit permission.");
        const auto reasonsOn = [](QWidget* root) {
            QStringList reasons;
            for (QWidget* w : root->findChildren<QWidget*>()) {
                if (w->toolTip().startsWith(QStringLiteral("Transmit controls are unavailable"))) {
                    reasons << w->toolTip();
                }
            }
            return reasons;
        };

        RadioModel remote(RadioModel::Role::Remote);
        TxApplet tx(&remote);
        PhoneCwApplet phone(&remote);
        VfoWidget flag;
        flag.setRadioModel(&remote);
        // R-R3-49 (parity Task 11): the RX applet is not here. Its only
        // transmit-gated controls were the XIT row, a slice setting now.
        for (QWidget* surface : std::initializer_list<QWidget*>{&tx, &phone, &flag}) {
            const QStringList reasons = reasonsOn(surface);
            QVERIFY2(!reasons.isEmpty(), surface->metaObject()->className());
            for (const QString& reason : reasons) {
                QCOMPARE(reason, expected);
                QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
            }
        }
    }

    // ====================================================================
    // R-R3-21 / R-R3-25: the Tools menu's two developer test entries.
    //
    // "Test antenna switch toast" and "Test TX-bound re-route dialog" fake
    // an antenna switch and a TX-bound antenna re-route on this window's
    // own RadioModel. Nothing on the Core stands behind either, so a
    // remote session gives them the transmit gate and the tooltip the
    // other unavailable transmit controls carry. Local direct mode keeps
    // them exactly as they were.
    //
    // Real windows, through GuiSessionCoordinator: the production path
    // from local mode to a Core session and back again.
    // ====================================================================
    void toolsMenuTestEntriesAreDisabledInARemoteSession()
    {
        // R-R3-21: the test entries exist only in developer builds, which
        // carry a smoke-build tag (tst_controls_that_work covers release).
        BuildIdentity::setBuildTag(QStringLiteral("test@0000000"));
        const auto clearBuildTag = qScopeGuard([] { BuildIdentity::setBuildTag(QString()); });
        // The arrangement tst_gui_session_coordinator makes first: no VAX
        // first-run dialog, and no discovery broadcast from the local
        // windows onto the LAN.
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] {
            RadioDiscovery::clearHoldOffForTest();
        });

        // A Core on loopback. Its model has no radio behind it, which is
        // all this needs: the handshake is what makes the session
        // Core-connected.
        QTemporaryDir stationDir;
        QVERIFY(stationDir.isValid());
        AppSettings stationSettings(stationDir.filePath(QStringLiteral("station.settings")));

        // Both ends of a real session have run CoreInit::initialize()'s
        // settings migrations before it opens. StationClient warns when
        // the client has not, and each end warns when their versions
        // differ. Which version does not matter here, only that both
        // ends have one and it is the same.
        constexpr int kMigratedSchema = 6;
        AppSettings::instance().ensureSettingsAtVersion(kMigratedSchema);
        stationSettings.ensureSettingsAtVersion(kMigratedSchema);

        RadioModel station;
        StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(stationDir.path()));
        QWebSocketServer listener(QStringLiteral("core"), QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&listener, &server] {
            server.acceptTransport(new WebSocketTransport(
                listener.nextPendingConnection(), StationServer::kMaxIncomingMessageBytes));
        });
        StationStartupSelection core;
        core.connection.url = QStringLiteral("ws://127.0.0.1:%1").arg(listener.serverPort());
        core.connection.token = server.token();
        core.connection.allowUnpinned = true;
        core.savedId = QStringLiteral("core");

        GuiSessionCoordinator sessions;
        QString localToastTip;
        QString localReRouteTip;
        QString remoteReason;

        // ---- Local direct mode: live, and as it has always been ----
        {
            QVERIFY(sessions.replace({}, false));
            MainWindow* const window = sessions.window();
            QVERIFY(window->radioModel()->ownsLocalDsp());
            QAction* const toast = window->findChild<QAction*>(kTestToastActionName);
            QAction* const reRoute = window->findChild<QAction*>(kTestReRouteActionName);
            QVERIFY(toast != nullptr);
            QVERIFY(reRoute != nullptr);
            QCOMPARE(toast->text(), QStringLiteral("Test antenna switch &toast"));
            QCOMPARE(reRoute->text(), QStringLiteral("Test TX-bound &re-route dialog"));
            QVERIFY(toast->isEnabled());
            QVERIFY(reRoute->isEnabled());
            localToastTip = toast->toolTip();
            localReRouteTip = reRoute->toolTip();
            QVERIFY(!localToastTip.isEmpty());
            QVERIFY(!localReRouteTip.isEmpty());

            QVERIFY(detachTestSurfaceConsumers(window));
            QSignalSpy switched(window->radioModel(), &RadioModel::antennaAutoSwitched);
            QSignalSpy reRouted(window->radioModel(), &RadioModel::txBoundReRouteRequested);
            toast->trigger();
            reRoute->trigger();
            // Non-vacuity for the remote half: the same trigger on a live
            // entry does reach RadioModel.
            QCOMPARE(switched.count(), 1);
            QCOMPARE(reRouted.count(), 1);
        }

        // ---- Connected to a Core: disabled, with the transmit reason ----
        {
            QVERIFY(sessions.replace(core, true));
            MainWindow* const window = sessions.window();
            QVERIFY(!window->radioModel()->ownsLocalDsp());
            QAction* const toast = window->findChild<QAction*>(kTestToastActionName);
            QAction* const reRoute = window->findChild<QAction*>(kTestReRouteActionName);
            QAction* const txEqualizer = window->findChild<QAction*>(kTxEqualizerActionName);
            QVERIFY(toast != nullptr);
            QVERIFY(reRoute != nullptr);
            QVERIFY(txEqualizer != nullptr);
            // Unavailable from the start, before the handshake lands.
            QVERIFY(!toast->isEnabled());
            QVERIFY(!reRoute->isEnabled());

            auto* const client = window->findChild<StationClient*>();
            QVERIFY(client != nullptr);
            QTRY_VERIFY(client->isHandshakeComplete());
            QVERIFY2(!client->capabilities().txPermitted,
                     "the Core advertises txPermitted=false in R3; this case is "
                     "the receive-only session an operator actually has");

            QVERIFY(!toast->isEnabled());
            QVERIFY(!reRoute->isEnabled());
            // R-R3-49 (parity Task 4): TX Equalizer opens in a remote window;
            // the dialog shows why it is greyed when it is.
            QVERIFY(txEqualizer->isEnabled());
            remoteReason = toast->toolTip();
            // Desktop remote transmit (R-IOS-13): the window declares
            // remoteTx, so the Core says why in its own words; this Core
            // is receive-only (a StationServer's default).
            QVERIFY2(remoteReason == QStringLiteral("This Core is set to receive only."),
                     qPrintable(QStringLiteral("the test entries no longer carry "
                                               "the Core's transmit reason: %1")
                                    .arg(remoteReason)));
            QCOMPARE(reRoute->toolTip(), remoteReason);

            // R-R3-49, R-R3-21 (parity Task 11): the RX applet's XIT row is
            // a slice setting and stays live without remote transmit.
            auto* const rxApplet = window->findChild<RxApplet*>();
            QVERIFY(rxApplet != nullptr);
            QPushButton* rxXit = nullptr;
            for (QPushButton* b : rxApplet->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("XIT")) { rxXit = b; }
            }
            QVERIFY(rxXit != nullptr);
            QVERIFY(rxXit->isEnabled());
            QVERIFY(rxXit->toolTip() != remoteReason);

            // R-R3-49 (parity Task 1): the TX applet's RF Power follows the
            // transmit settings gate. The Core offers transmitSettingsVersion
            // 1 and its radio is off the air, so it is live. Remote parity
            // on the air (transmitSettingsVersion 13): keying the Core's
            // radio leaves it live, as in a local window; General Region
            // still waits for the radio to stop.
            auto* const txApplet = window->findChild<TxApplet*>();
            QVERIFY(txApplet != nullptr);
            QVERIFY(client->transmitSettingsAvailable());
            QTRY_VERIFY(txApplet->rfPowerSlider()->isEnabled());
            // R-R3-49 (parity Task 2): Tune Power, LEV and the Phone/CW
            // applet's PROC follow the same gate at transmitSettingsVersion
            // 2; the VOX button keeps the remote transmit reason.
            QVERIFY(client->transmitSettingsAvailable(2));
            QVERIFY(client->transmitSettingsAvailable(9));
            GeneralOptionsPage regionPage(window->radioModel());
            regionPage.setStationSettingsAvailable(true, QString());
            auto* regionCombo = regionPage.findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
            QVERIFY(regionCombo);
            QVERIFY(regionCombo->isEnabled());
            regionCombo->setCurrentIndex(3);
            QTRY_COMPARE(stationSettings.value(QStringLiteral("BandPlanRegion")).toInt(), 3);
            // Addendum G-42: Extended is the Core's setting, changed only by
            // a device the Core permits to transmit. This receive-only
            // session is not, so the box is disabled with the Core's own
            // reason instead of being refused after a tick. A change on the
            // Core still reaches the window's box.
            QVERIFY(client->transmitSettingsAvailable(12));
            auto* extendedBox = regionPage.findChild<QCheckBox*>(QStringLiteral("chkExtended"));
            QVERIFY(extendedBox);
            QVERIFY(!extendedBox->isEnabled());
            QCOMPARE(extendedBox->toolTip(), QStringLiteral("This Core is set to receive only."));
            QVERIFY(!extendedBox->isChecked());
            extendedBox->setChecked(true);
            QVERIFY(!extendedBox->isChecked());
            QVERIFY(!stationSettings.contains(QStringLiteral("ExtendedTransmit")));
            stationSettings.setValue(QStringLiteral("ExtendedTransmit"), QStringLiteral("True"));
            QTRY_VERIFY(extendedBox->isChecked());
            stationSettings.remove(QStringLiteral("ExtendedTransmit"));
            QTRY_VERIFY(!extendedBox->isChecked());
            // Prevent TX'ing on a different band is the Core's setting the
            // same way, from transmitSettingsVersion 14.
            QVERIFY(client->transmitSettingsAvailable(14));
            auto* preventBox = regionPage.findChild<QCheckBox*>(
                QStringLiteral("chkPreventTXonDifferentBandToRX"));
            QVERIFY(preventBox);
            QVERIFY(!preventBox->isHidden());
            QVERIFY(!preventBox->isEnabled());
            QCOMPARE(preventBox->toolTip(), QStringLiteral("This Core is set to receive only."));
            QVERIFY(!preventBox->isChecked());
            preventBox->setChecked(true);
            QVERIFY(!preventBox->isChecked());
            QVERIFY(!stationSettings.contains(QStringLiteral("PreventTxOnDifferentBandToRx")));
            stationSettings.setValue(QStringLiteral("PreventTxOnDifferentBandToRx"),
                                     QStringLiteral("True"));
            QTRY_VERIFY(preventBox->isChecked());
            stationSettings.remove(QStringLiteral("PreventTxOnDifferentBandToRx"));
            QTRY_VERIFY(!preventBox->isChecked());
            QPushButton* const lev = txApplet->findChild<QPushButton*>(QStringLiteral("TxLevButton"));
            QPushButton* const vox = txApplet->findChild<QPushButton*>(QStringLiteral("TxVoxButton"));
            auto* const phone = window->findChild<PhoneCwApplet*>();
            QVERIFY(lev && vox && phone);
            QPushButton* const proc = phone->findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
            QVERIFY(proc);
            QTRY_VERIFY(txApplet->tunePowerSlider()->isEnabled());
            QTRY_VERIFY(lev->isEnabled());
            QTRY_VERIFY(proc->isEnabled());
            QVERIFY(!vox->isEnabled());
            QCOMPARE(vox->toolTip(), remoteReason);
            MoxController* const coreMox = station.moxController();
            QVERIFY(coreMox != nullptr);
            coreMox->setMoxCheck({});
            coreMox->setMox(true);
            QTRY_VERIFY(window->radioModel()->isCoreOnAir());
            QTRY_VERIFY(!regionCombo->isEnabled());
            QTRY_VERIFY(!extendedBox->isEnabled());
            QCOMPARE(extendedBox->toolTip(),
                     QStringLiteral("The radio is on the air. Try again when it stops."));
            QTRY_VERIFY(!preventBox->isEnabled());
            QCOMPARE(preventBox->toolTip(),
                     QStringLiteral("The radio is on the air. Try again when it stops."));
            QVERIFY(txApplet->rfPowerSlider()->isEnabled());
            for (QWidget* w : std::initializer_list<QWidget*>{txApplet->tunePowerSlider(), lev, proc}) {
                QVERIFY(w->isEnabled());
            }
            // R-R3-49 (parity Task 3): the TX profile combos and RADE's Reset
            // vocoder follow the transmit settings gate too.
            auto* const rade = window->findChild<RadeApplet*>();
            QVERIFY(rade != nullptr);
            for (QWidget* w : std::initializer_list<QWidget*>{
                     rade->profileComboForTest(), rade->resetVocoderButtonForTest(),
                     txApplet->profileCombo()}) {
                QVERIFY(w->isEnabled());
            }
            coreMox->setMox(false);
            QTRY_VERIFY(!window->radioModel()->isCoreOnAir());
            QTRY_VERIFY(regionCombo->isEnabled());
            QTRY_COMPARE(extendedBox->toolTip(),
                         QStringLiteral("This Core is set to receive only."));
            QVERIFY(!extendedBox->isEnabled());
            QTRY_COMPARE(preventBox->toolTip(),
                         QStringLiteral("This Core is set to receive only."));
            QVERIFY(!preventBox->isEnabled());
            QTRY_VERIFY(txApplet->rfPowerSlider()->isEnabled());
            QTRY_VERIFY(txApplet->tunePowerSlider()->isEnabled());
            QTRY_VERIFY(lev->isEnabled());
            QTRY_VERIFY(proc->isEnabled());

            // Off the air they are live again, whatever remote transmit says.
            QTRY_VERIFY(rade->profileComboForTest()->isEnabled());
            QVERIFY(rade->resetVocoderButtonForTest()->isEnabled());
            QVERIFY(txApplet->profileCombo()->isEnabled());
            QVERIFY(rade->profileComboForTest()->toolTip() != remoteReason);

            QVERIFY(detachTestSurfaceConsumers(window));
            QSignalSpy switched(window->radioModel(), &RadioModel::antennaAutoSwitched);
            QSignalSpy reRouted(window->radioModel(), &RadioModel::txBoundReRouteRequested);
            toast->trigger();
            reRoute->trigger();
            // Past the disabled action to the handlers themselves, which
            // refuse the way the TX Equalizer entry's handler does.
            emit toast->triggered(false);
            emit reRoute->triggered(false);
            QCOMPARE(switched.count(), 0);
            QCOMPARE(reRouted.count(), 0);
        }

        // ---- Back to local mode: live again, local tooltips back ----
        {
            QVERIFY(sessions.replace({}, false));
            MainWindow* const window = sessions.window();
            QVERIFY(window->radioModel()->ownsLocalDsp());
            QAction* const toast = window->findChild<QAction*>(kTestToastActionName);
            QAction* const reRoute = window->findChild<QAction*>(kTestReRouteActionName);
            QVERIFY(toast != nullptr);
            QVERIFY(reRoute != nullptr);
            QVERIFY(toast->isEnabled());
            QVERIFY(reRoute->isEnabled());
            QCOMPARE(toast->toolTip(), localToastTip);
            QCOMPARE(reRoute->toolTip(), localReRouteTip);
            auto* const rade = window->findChild<RadeApplet*>();
            QVERIFY(rade != nullptr);
            QVERIFY(rade->profileComboForTest()->isEnabled());
            QVERIFY(rade->profileComboForTest()->toolTip() != remoteReason);
            QVERIFY(localToastTip != remoteReason);
            QVERIFY(localReRouteTip != remoteReason);

            QVERIFY(detachTestSurfaceConsumers(window));
            QSignalSpy switched(window->radioModel(), &RadioModel::antennaAutoSwitched);
            QSignalSpy reRouted(window->radioModel(), &RadioModel::txBoundReRouteRequested);
            toast->trigger();
            reRoute->trigger();
            QCOMPARE(switched.count(), 1);
            QCOMPARE(reRouted.count(), 1);
        }

        sessions.shutdown();
    }

    // ====================================================================
    // R3 unfinished controls, Task 3 (R-R3-49, R-R3-21): a container's
    // function and band buttons act on the container's own slice, in a
    // local window and in a remote one. The band buttons light that
    // slice's band and follow it however it changes. In a remote window
    // the transmit buttons say the transmit reason and change nothing;
    // the others act on the Core's slice.
    // ====================================================================
    // Fix wave M2 (R-R3-49, R-R3-21): a container set to a slice that
    // closes shows none of that slice's state. Its VFO display says the
    // slice is not open instead of the slice's last frequency, its mode,
    // filter, step and antenna buttons light nothing, and a wheel on the
    // display says why it does nothing.
    void aContainerOnASliceThatClosesShowsNoneOfIt()
    {
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] {
            RadioDiscovery::clearHoldOffForTest();
        });

        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* const window = sessions.window();
        RadioModel* const model = window->radioModel();
        while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
        SliceModel* const b = model->sliceById(1);
        QVERIFY(b != nullptr);
        b->setFrequency(7100000.0);

        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager != nullptr);
        ContainerWidget* const container = manager->createContainer(2, DockMode::Floating);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* modes = new ModeButtonItem();
        auto* filters = new FilterButtonItem();
        auto* steps = new TuneStepButtonItem();
        auto* antennas = new AntennaButtonItem();
        auto* vfo = new VfoDisplayItem();
        for (MeterItem* item : std::initializer_list<MeterItem*>{modes, filters, steps, antennas, vfo}) {
            meter->addItem(item);
            container->wireInteractiveItem(item);
        }
        const auto destroy = qScopeGuard([manager, container] {
            manager->destroyContainer(container->id());
        });

        // On slice B: lit from slice B.
        QVERIFY(modes->activeMode() >= 0);
        QCOMPARE(vfo->frequency(), int64_t(7100000));
        QVERIFY(vfo->unavailableText().isEmpty());

        // Slice B closes.
        model->removeSlice(1);
        QVERIFY(model->sliceById(1) == nullptr);

        QCOMPARE(modes->activeMode(), -1);
        QCOMPARE(filters->activeFilter(), -1);
        for (int i = 0; i < 10; ++i) { QVERIFY(filters->filterLabel(i).isEmpty()); }
        QCOMPARE(steps->activeStep(), -1);
        for (int i = 0; i < antennas->buttonCount(); ++i) {
            QVERIFY2(!antennas->button(i).on, qPrintable(QString::number(i)));
        }
        QCOMPARE(vfo->unavailableText(), QStringLiteral("Slice B is not open"));
        QVERIFY(OperatorWording::isPlain(vfo->unavailableText()));

        // A wheel on the display says why it does nothing.
        emit container->frequencyChangeRequested(1000);
        bool said = false;
        for (StatusToast* toast : window->findChildren<StatusToast*>()) {
            if (toast->message() == ContainerButtonDispatcher::noSliceReason(2)) { said = true; }
        }
        QVERIFY(said);

        // Set to slice A, which is open: its state is back.
        container->setRxSource(1);
        QVERIFY(vfo->unavailableText().isEmpty());
        QCOMPARE(vfo->frequency(),
                 static_cast<int64_t>(std::llround(model->sliceById(0)->frequency())));
        QVERIFY(modes->activeMode() >= 0);
    }

    // Fix wave M3 (R-R3-49, R-R3-21): the Peak hold and VAX 1 buttons light
    // the moment their targets change elsewhere (the pan overlay, Setup >
    // Audio > VAX), not at the next unrelated refresh.
    void containerPeakAndVaxButtonsFollowChangesMadeElsewhere()
    {
        using Id = OtherButtonItem::ButtonId;
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] {
            RadioDiscovery::clearHoldOffForTest();
            AppSettings::instance().remove(QStringLiteral("audio/Vax1/Enabled"));
        });

        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* const window = sessions.window();
        RadioModel* const model = window->radioModel();
        while (model->slices().isEmpty()) { QVERIFY(model->addSlice() >= 0); }
        QVERIFY(model->sliceById(0) != nullptr);
        AudioEngine* const audio = model->localAudioDevices();
        QVERIFY(audio != nullptr);
        audio->setVaxBusFactoryForTest([](int channel) -> std::unique_ptr<IAudioBus> {
            auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax%1").arg(channel));
            AudioFormat fmt;
            fmt.sampleRate = 48000;
            fmt.channels = 2;
            fmt.sample = AudioFormat::Sample::Float32;
            bus->open(fmt);
            return bus;
        });
        audio->setVaxEnabled(1, false);

        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager != nullptr);
        ContainerWidget* const container = manager->createContainer(1, DockMode::Floating);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* buttons = new OtherButtonItem();
        meter->addItem(buttons);
        container->wireInteractiveItem(buttons);
        const auto destroy = qScopeGuard([manager, container] {
            manager->destroyContainer(container->id());
        });

        // Peak hold, changed on slice A's panadapter.
        SpectrumWidget* sw = nullptr;
        for (SpectrumWidget* candidate : window->findChildren<SpectrumWidget*>()) {
            candidate->setPeakHoldEnabled(false);
            if (sw == nullptr) { sw = candidate; }
        }
        QVERIFY(sw != nullptr);
        QVERIFY(buttons->isButtonAvailable(Id::PeakHold));
        const bool before = buttons->buttonState(Id::PeakHold);
        for (SpectrumWidget* candidate : window->findChildren<SpectrumWidget*>()) {
            candidate->setPeakHoldEnabled(true);
        }
        QVERIFY(!before);
        QVERIFY(buttons->buttonState(Id::PeakHold));

        // VAX 1, switched on as Setup > Audio > VAX does.
        QVERIFY(!buttons->buttonState(Id::Vac1));
        audio->setVaxEnabled(1, true);
        QVERIFY(audio->isVaxBusOpen(1));
        QVERIFY(buttons->buttonState(Id::Vac1));
        audio->setVaxEnabled(1, false);
        QVERIFY(!buttons->buttonState(Id::Vac1));
    }

    void containerButtonsActOnTheirOwnSliceLocallyAndRemotely()
    {
        using Id = OtherButtonItem::ButtonId;
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] {
            RadioDiscovery::clearHoldOffForTest();
        });

        struct Box {
            ContainerWidget* container{nullptr};
            OtherButtonItem* buttons{nullptr};
            BandButtonItem* bands{nullptr};
        };
        // A container set to `rxSource` holding function and band buttons,
        // wired as ContainerManager wires restored items.
        const auto addBox = [](MainWindow* window, int rxSource) {
            Box box;
            auto* manager = window->findChild<ContainerManager*>();
            if (!manager) { return box; }
            box.container = manager->createContainer(rxSource, DockMode::Floating);
            auto* meter = new MeterWidget();
            box.container->setContent(meter);
            box.buttons = new OtherButtonItem();
            box.bands = new BandButtonItem();
            meter->addItem(box.buttons);
            box.container->wireInteractiveItem(box.buttons);
            meter->addItem(box.bands);
            box.container->wireInteractiveItem(box.bands);
            return box;
        };
        const auto toastSaying = [](MainWindow* window, const QString& text) {
            for (StatusToast* toast : window->findChildren<StatusToast*>()) {
                if (toast->message() == text) { return true; }
            }
            return false;
        };

        GuiSessionCoordinator sessions;

        // ---- Local window, no radio ----
        {
            QVERIFY(sessions.replace({}, false));
            MainWindow* const window = sessions.window();
            RadioModel* const model = window->radioModel();
            QVERIFY(model->ownsLocalDsp());
            while (model->slices().size() < 2) { QVERIFY(model->addSlice() >= 0); }
            SliceModel* const a = model->sliceById(0);
            SliceModel* const b = model->sliceById(1);
            QVERIFY(a && b);
            a->setFrequency(14100000.0);
            b->setFrequency(7100000.0);
            QVERIFY(model->setActiveSliceById(1));
            QCOMPARE(model->activeSlice(), b);

            const Box box = addBox(window, 1);
            QVERIFY(box.container && box.buttons && box.bands);

            // The band buttons light slice A's band, not the active slice's.
            QCOMPARE(box.bands->activeBand(), uiIndexFromBand(Band::Band20m));
            // ...and follow it when it changes from elsewhere.
            a->setFrequency(21200000.0);
            QCOMPARE(box.bands->activeBand(), uiIndexFromBand(Band::Band15m));
            // A band button changes slice A, not the active slice B.
            emit box.container->bandClicked(uiIndexFromBand(Band::Band40m));
            QCOMPARE(bandFromFrequency(a->frequency()), Band::Band40m);
            QCOMPARE(bandFromFrequency(b->frequency()), Band::Band40m);  // B was on 40m
            b->setFrequency(3700000.0);
            QCOMPARE(box.bands->activeBand(), uiIndexFromBand(Band::Band40m));

            // ANF on slice A, lit from slice A.
            const bool anfB = b->anfEnabled();
            QVERIFY(!a->anfEnabled());
            emit box.container->otherButtonClicked(int(Id::Anf));
            QVERIFY(a->anfEnabled());
            QCOMPARE(b->anfEnabled(), anfB);
            QVERIFY(box.buttons->buttonState(Id::Anf));
            // Changed from elsewhere (the VFO flag), the button follows.
            a->setAnfEnabled(false);
            QVERIFY(!box.buttons->buttonState(Id::Anf));

            // No radio: TUN, MOX and 2TON are unavailable and do nothing.
            for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
                QVERIFY(!box.buttons->isButtonAvailable(id));
                emit box.container->otherButtonClicked(int(id));
            }
            QVERIFY(!model->isTune());
            QVERIFY(!model->moxController()->isMox());
            QVERIFY(toastSaying(window, ContainerButtonDispatcher::noRadioTransmitReason()));

            // Set to slice C, which is not open: unavailable, and says so.
            box.container->setRxSource(3);
            QVERIFY(!box.buttons->isButtonAvailable(Id::Anf));
            QCOMPARE(box.bands->activeBand(), -1);
            emit box.container->otherButtonClicked(int(Id::Anf));
            QVERIFY(!a->anfEnabled());
            QVERIFY(toastSaying(window, ContainerButtonDispatcher::noSliceReason(3)));

            window->findChild<ContainerManager*>()->destroyContainer(box.container->id());
        }

        // ---- Remote window on a Core with slices A and B ----
        QTemporaryDir stationDir;
        QVERIFY(stationDir.isValid());
        AppSettings stationSettings(stationDir.filePath(QStringLiteral("station.settings")));
        constexpr int kMigratedSchema = 6;
        AppSettings::instance().ensureSettingsAtVersion(kMigratedSchema);
        stationSettings.ensureSettingsAtVersion(kMigratedSchema);

        RadioModel station;
        while (station.slices().size() < 2) { QVERIFY(station.addSlice() >= 0); }
        station.sliceById(0)->setFrequency(14100000.0);
        station.sliceById(1)->setFrequency(7100000.0);
        station.setActiveSliceById(1);
        StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(stationDir.path()));
        QWebSocketServer listener(QStringLiteral("core"), QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&listener, &server] {
            server.acceptTransport(new WebSocketTransport(
                listener.nextPendingConnection(), StationServer::kMaxIncomingMessageBytes));
        });
        StationStartupSelection core;
        core.connection.url = QStringLiteral("ws://127.0.0.1:%1").arg(listener.serverPort());
        core.connection.token = server.token();
        core.connection.allowUnpinned = true;
        core.savedId = QStringLiteral("core");

        {
            QVERIFY(sessions.replace(core, true));
            MainWindow* const window = sessions.window();
            RadioModel* const model = window->radioModel();
            QVERIFY(!model->ownsLocalDsp());
            auto* const client = window->findChild<StationClient*>();
            QVERIFY(client != nullptr);
            QTRY_VERIFY(client->isHandshakeComplete());
            QTRY_VERIFY(model->sliceById(0) != nullptr && model->sliceById(1) != nullptr);
            SliceModel* const a = model->sliceById(0);
            QTRY_COMPARE(bandFromFrequency(a->frequency()), Band::Band20m);

            const Box box = addBox(window, 1);
            QVERIFY(box.container && box.buttons && box.bands);
            QCOMPARE(box.bands->activeBand(), uiIndexFromBand(Band::Band20m));

            // The Core's slice A retunes: the band buttons follow.
            station.sliceById(0)->setFrequency(21200000.0);
            QTRY_COMPARE(box.bands->activeBand(), uiIndexFromBand(Band::Band15m));

            // ANF acts on the Core's slice A, not its active slice B.
            const bool anfB = station.sliceById(1)->anfEnabled();
            QVERIFY(!station.sliceById(0)->anfEnabled());
            emit box.container->otherButtonClicked(int(Id::Anf));
            QTRY_VERIFY(station.sliceById(0)->anfEnabled());
            QCOMPARE(station.sliceById(1)->anfEnabled(), anfB);
            QTRY_VERIFY(box.buttons->buttonState(Id::Anf));

            // The transmit buttons say the Core's reason and change nothing
            // (desktop remote transmit: a receive-only Core says so).
            const QString reason = QStringLiteral("This Core is set to receive only.");
            for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
                QVERIFY(!box.buttons->isButtonAvailable(id));
                QCOMPARE(box.buttons->buttonUnavailableReason(box.buttons->indexOf(id)), reason);
            }
            // R-R3-49 (parity Task 7): PS-A arms PureSignal and keys
            // nothing, so the Core takes it off the air; this Core has no
            // PureSignal running, and the button says so.
            QVERIFY(!box.buttons->isButtonAvailable(Id::PsA));
            QCOMPARE(box.buttons->buttonUnavailableReason(box.buttons->indexOf(Id::PsA)),
                     QStringLiteral("PureSignal needs a connected radio that supports it."));
            emit box.container->otherButtonClicked(int(Id::Mox));
            QVERIFY(toastSaying(window, reason));
            QVERIFY(!station.moxController()->isMox());
            QVERIFY(!model->moxController()->isMox());

            // R-R3-49 (parity Task 2): MON is a transmit setting. It toggles
            // the Core's MON off the air and lights from the Core's value.
            const bool mon = station.transmitModel().monEnabled();
            QTRY_VERIFY(box.buttons->isButtonAvailable(Id::Mon));
            emit box.container->otherButtonClicked(int(Id::Mon));
            QTRY_COMPARE(station.transmitModel().monEnabled(), !mon);
            QTRY_COMPARE(box.buttons->buttonState(Id::Mon), !mon);
            station.transmitModel().setMonEnabled(mon);
            QTRY_COMPARE(box.buttons->buttonState(Id::Mon), mon);
            // Remote parity on the air (transmitSettingsVersion 13): on the
            // air it stays live and toggles the Core's MON, as locally.
            station.moxController()->setMoxCheck({});
            station.moxController()->setMox(true);
            QTRY_VERIFY(window->radioModel()->isCoreOnAir());
            QVERIFY(box.buttons->isButtonAvailable(Id::Mon));
            // R-R3-49 (parity Task 7): PS-A keeps its own reason, not the air.
            QCOMPARE(box.buttons->buttonUnavailableReason(box.buttons->indexOf(Id::PsA)),
                     QStringLiteral("PureSignal needs a connected radio that supports it."));
            emit box.container->otherButtonClicked(int(Id::Mon));
            QTRY_COMPARE(station.transmitModel().monEnabled(), !mon);
            station.transmitModel().setMonEnabled(mon);
            station.moxController()->setMox(false);
            QTRY_VERIFY(station.moxController()->state() == MoxState::Rx);
            QTRY_VERIFY(box.buttons->isButtonAvailable(Id::Mon));

            // R-R3-49, R-R3-21 (parity Task 11): the Antenna box's TX
            // buttons (6 to 8) write the transmit slice's txAntenna on the
            // Core as the VFO flag's TX antenna button does, with no toast.
            SliceModel* const txTarget = model->txBoundSlice() ? model->txBoundSlice() : a;
            SliceModel* const coreTx = station.sliceById(txTarget->sliceIndex());
            QVERIFY(coreTx != nullptr);
            const QString txAntBefore = coreTx->txAntenna();
            const QString txAntWanted =
                txAntBefore == QStringLiteral("ANT2") ? QStringLiteral("ANT3") : QStringLiteral("ANT2");
            const int txButton = txAntWanted == QStringLiteral("ANT2") ? 7 : 8;
            // The MOX click above left the transmit reason on screen.
            qDeleteAll(window->findChildren<StatusToast*>());
            QVERIFY(!toastSaying(window, reason));
            emit box.container->antennaSelected(txButton);
            QTRY_COMPARE(coreTx->txAntenna(), txAntWanted);
            QTRY_COMPARE(txTarget->txAntenna(), txAntWanted);
            QVERIFY(!toastSaying(window, reason));
            emit box.container->antennaSelected(6);
            QTRY_COMPARE(coreTx->txAntenna(), QStringLiteral("ANT1"));

            window->findChild<ContainerManager*>()->destroyContainer(box.container->id());
        }

        QVERIFY(sessions.replace({}, false));
        sessions.shutdown();
    }

    // ====================================================================
    // R-R3-16 (carried from the Connections task): a LOCAL window still
    // opens Connections on its own when its radio drops. Since R-R3-16 the
    // automatic open in MainWindow::onConnectionStateChanged runs only
    // for a model that owns its local DSP; this pins that the local half
    // still behaves exactly as before. The open keys on a radio name the
    // model learns on connect, so setNameForTest stands in for a connect.
    // ====================================================================
    // GUI-M2 (fix wave): a window running its own radio shows Radio >
    // Change radio, Edit radio and Forget radio disabled, with a reason that
    // points to the Connection panel (before, they were hidden).
    void localWindowShowsCoreRadioItemsDisabledWithTheReason_data()
    {
        QTest::addColumn<bool>("pickerManaged");
        QTest::addColumn<QString>("item");
        QTest::newRow("picker") << true << QStringLiteral("Radio > Connections");
        QTest::newRow("direct panel") << false << QStringLiteral("Radio > Manage Radios");
    }

    void localWindowShowsCoreRadioItemsDisabledWithTheReason()
    {
        QFETCH(bool, pickerManaged);
        QFETCH(QString, item);
        Test::markAudioFirstRunDone();
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(pickerManaged);
        QVERIFY(window.radioModel()->ownsLocalDsp());
        // The window's menus, wherever the title bar hosts them.
        QMenu* radioMenu = nullptr;
        for (QMenu* menu : window.findChildren<QMenu*>()) {
            if (menu->title() == QStringLiteral("&Radio")) {
                radioMenu = menu;
            }
        }
        QVERIFY(radioMenu);
        emit radioMenu->aboutToShow();
        int found = 0;
        for (QAction* action : radioMenu->actions()) {
            const QString text = action->text();
            if (text != QStringLiteral("Change radio\u2026") && text != QStringLiteral("Edit radio\u2026")
                && text != QStringLiteral("Forget radio")) {
                continue;
            }
            ++found;
            QVERIFY2(action->isVisible(), qPrintable(text));
            QVERIFY2(!action->isEnabled(), qPrintable(text));
            // Fix round 1 (minor 3): the item this window really has.
            QVERIFY2(action->toolTip().contains(item), qPrintable(action->toolTip()));
            QVERIFY(OperatorWording::isPlain(action->toolTip()));
        }
        QCOMPARE(found, 3);
    }

    void localWindowDisconnectStillOpensConnections_data()
    {
        QTest::addColumn<bool>("pickerManaged");
        QTest::newRow("picker") << true;
        // R3 Setup fix wave (final review M4): a window without the picker
        // shows its own connection panel.
        QTest::newRow("direct panel") << false;
    }

    void localWindowDisconnectStillOpensConnections()
    {
        QFETCH(bool, pickerManaged);
        Test::markAudioFirstRunDone();
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] {
            RadioDiscovery::clearHoldOffForTest();
        });

        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(pickerManaged);
        RadioModel* const model = window.radioModel();
        QVERIFY(model->ownsLocalDsp());
        QSignalSpy requests(&window, &MainWindow::connectionsRequested);
        const auto panelShown = [&window] {
            auto* panel = window.findChild<ConnectionPanel*>();
            return panel != nullptr && panel->isVisible();
        };

        // No radio name yet (the startup case): a disconnected state opens
        // nothing.
        model->setConnectionStateForTest(ConnectionState::Connecting);
        model->setConnectionStateForTest(ConnectionState::Disconnected);
        QCOMPARE(requests.count(), 0);
        QVERIFY(!panelShown());

        // After a connect has named the radio, losing it opens Connections,
        // once: the picker's request, or this window's own panel.
        model->setNameForTest(QStringLiteral("ANAN-G2"));
        model->setConnectionStateForTest(ConnectionState::Connecting);
        requests.clear();
        model->setConnectionStateForTest(ConnectionState::Disconnected);
        if (pickerManaged) {
            QCOMPARE(requests.count(), 1);
            QVERIFY(!panelShown());
        } else {
            QCOMPARE(requests.count(), 0);
            QVERIFY(panelShown());
            QCOMPARE(window.findChildren<ConnectionPanel*>().size(), 1);
        }
    }

    // ====================================================================
    // R-R3-44 (parity Task 11): the VAX first-run check for new virtual
    // cables runs in a remote window as in a local one. The VAX outputs,
    // audio/FirstRunComplete and the cable fingerprint are all this
    // computer's, so a remote window records them where a later local
    // window reads them. The dialog applies nothing until the operator
    // picks, so no device opens here.
    // ====================================================================
    void vaxFirstRunCheckRunsInRemoteAndLocalWindows()
    {
        RadioDiscovery::clearHoldOffForTest();
        {
            RadioDiscovery discovery;
            discovery.holdOffScans(std::chrono::minutes{5});
        }
        const auto releaseHoldOff = qScopeGuard([] {
            RadioDiscovery::clearHoldOffForTest();
        });

        // Migrated settings, so the remote window's StationClient does not
        // warn that CoreInit has not run (as the Tools menu case above).
        AppSettings::instance().ensureSettingsAtVersion(6);
        // The local window below spins the event loop; on Linux with no
        // audio backend the modal Linux audio first-run dialog would block
        // it (R-R3-21). audio/FirstRunComplete stays unset, so the VAX
        // first-run check this case is about still runs.
        Test::suppressLinuxAudioFirstRun();
        {
            SettingsProxy proxy;
            AppSettings::instance().setRemoteBackend(&proxy);
            const auto dropBackend = qScopeGuard([] {
                AppSettings::instance().setRemoteBackend(nullptr);
            });
            MainWindow remote({QStringLiteral("ws://127.0.0.1:1"), {}, {}, true}, nullptr,
                              MainWindow::ConnectionStartup::Deferred);
            QVERIFY(!remote.radioModel()->ownsLocalDsp());
            QVERIFY(!AppSettings::instance().contains(QStringLiteral("audio/LastDetectedCables")));
            QCoreApplication::processEvents();
            // It records this computer's cable fingerprint (never through
            // the Core's settings) and, first-run not done, asks.
            QVERIFY(AppSettings::instance().contains(QStringLiteral("audio/LastDetectedCables")));
            QVERIFY(!proxy.handlesKey(QStringLiteral("audio/LastDetectedCables")));
            QVERIFY(!proxy.handlesKey(QStringLiteral("audio/FirstRunComplete")));
            QVERIFY(remote.findChild<VaxFirstRunDialog*>() != nullptr);
            // Nothing is marked complete until the operator answers.
            QVERIFY(!AppSettings::instance().contains(QStringLiteral("audio/FirstRunComplete")));
        }

        // A local window with the same settings asks too (still not done).
        AppSettings::instance().remove(QStringLiteral("audio/LastDetectedCables"));
        MainWindow local({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        QVERIFY(local.radioModel()->ownsLocalDsp());
        QCoreApplication::processEvents();
        QVERIFY(AppSettings::instance().contains(QStringLiteral("audio/LastDetectedCables")));
        QVERIFY(local.findChild<VaxFirstRunDialog*>() != nullptr);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstRemoteGuiGating test;
    QTEST_SET_MAIN_SOURCE_PATH
    // Load findings 3: about 10 s on a quiet machine, past ctest's 120 s
    // under a loaded full run (the limit is not raised). The Setup sweeps
    // and the container and menu cases run as their own ctest entries,
    // tst_remote_gui_gating_setup and tst_remote_gui_gating_containers
    // (tests/CMakeLists.txt); the rest as tst_remote_gui_gating.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_REMOTE_GUI_GATING_GROUP",
        {{QStringLiteral("setup"),
          {QStringLiteral("everySetupPageRealizesAgainstARemoteModel"),
           QStringLiteral("everyRemoteSetupPageIsEitherLocalDspFreeOrDisabled"),
           QStringLiteral("aThisComputerPageThatReachesLocalDspFailsTheSweep"),
           QStringLiteral("theSameSweepAgainstALocalModelDisablesNothing"),
           QStringLiteral("coreSetupPagesWaitForTheCoresSettingsNotAnySnapshot"),
           QStringLiteral("coreScopedHiddenGroupsHideInAConnectedRemoteWindow"),
           QStringLiteral("buildingCoreAndMixedPagesSendsTheCoreNothing"),
           QStringLiteral("remoteHardwareConfigFollowsTheCore"),
           QStringLiteral("disconnectedRemoteSetupPerPageTable"),
           QStringLiteral("remoteDevicesPageWorksOnThisComputer")}},
         {QStringLiteral("containers"),
          {QStringLiteral("toolsMenuTestEntriesAreDisabledInARemoteSession"),
           QStringLiteral("aContainerOnASliceThatClosesShowsNoneOfIt"),
           QStringLiteral("containerPeakAndVaxButtonsFollowChangesMadeElsewhere"),
           QStringLiteral("containerButtonsActOnTheirOwnSliceLocallyAndRemotely"),
           QStringLiteral("localWindowDisconnectStillOpensConnections"),
           QStringLiteral("localWindowShowsCoreRadioItemsDisabledWithTheReason"),
           QStringLiteral("vaxFirstRunCheckRunsInRemoteAndLocalWindows")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}

#include "tst_remote_gui_gating.moc"
