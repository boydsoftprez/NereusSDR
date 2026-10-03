// =================================================================
// tests/tst_remote_window_harness.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Drives one real remote MainWindow
// against an in-process Core; no upstream logic is ported.
//
// R3 remote window harness plan, Task 2 (R-R3-16, R-R3-17, R-R3-21,
// R-R3-24). Every case starts from the window's own controls: a menu
// QAction, a mouse click on a chrome widget or a pan, or a button in a
// dialog the window opened. Nothing here calls a MainWindow slot or
// StationClient directly to make something happen; the harness only
// reads state back and plays the Core's side of the wire.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote window harness plan, Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote window Setup plan, Task 3
//                                    (R-R3-16, R-R3-17, R-R3-38):
//                                    Connections opens only after the
//                                    operator's Disconnect. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote window Setup plan, Task 2
//                                    (R-R3-21, R-R3-10, R-R3-17): Setup
//                                    opens on a disconnected window.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote radio hardware plan, Task 3
//                                    (R-R3-46, R-R3-21): the window's
//                                    attenuator, preamp, auto-attenuate
//                                    and overload controls use the Core's
//                                    `stepAtt` object; an older Core
//                                    leaves them disabled with its
//                                    reason. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 4
//                                    (R-R3-42): a Core's stored TCI values
//                                    are ignored. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49: the Core page floor drops by
//                                    the three leaves not registered while
//                                    their features are not built.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the RADE applet's profile combo and
//                                    Reset vocoder follow the transmit
//                                    settings gate.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Transmit group fix wave 2 (M8): the
//                                    permission push is watched on MOX;
//                                    VOX waits for the microphone line.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  D79 (R-IOS-11, R-R3-49): the window
//                                    follows the Core's band plan, strip
//                                    and menu check.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control and shared listening
//                                    plan Task 5: a window whose Core sends
//                                    no sliceAccessVersion sends no slice
//                                    access request and tunes as before.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 fix: the
//                                    flag's TX button moves transmit on the
//                                    Core (tx.setTxSlice) with the letter
//                                    row's reasons.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 17: a window
//                                    sharing the Core's slices. Container
//                                    slice buttons follow the change of
//                                    control; a layout change stops
//                                    listening; an accepted listen or take
//                                    shows the slice here; placements follow
//                                    the Core's access updates, the take's
//                                    answer arriving before its update.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Load findings 4: run as six ctest
//                                    entries (TestFunctionGroups), past
//                                    the 120 s limit as one under load.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Load findings 4: the take answered
//                                    before its access update waits for
//                                    the Core's update to be held (it
//                                    comes on the Core's next delta
//                                    flush). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I1: the badge's take
//                                    question closes with the link and
//                                    when its take is abandoned. GUI-I5:
//                                    a container's MON and PS-A follow
//                                    the transmit holder.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave round 1: the header's fault
//                                    width check lays the segment out
//                                    with the fault text in place instead
//                                    of measuring it against the width
//                                    chosen for the muted text.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave round 2: the fault case also
//                                    checks the drawn row keeps all four
//                                    groups. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave round 3: the width check
//                                    runs every audio state's wording, the
//                                    longest being "Audio radio offline".
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  The radio-offline audio group now
//                                    reads "Radio offline"; the width check
//                                    and its 34 px margin are unchanged.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QRadioButton>
#include <QScopeGuard>
#include <QComboBox>
#include <QGraphicsOpacityEffect>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QLayout>
#include <QStackedWidget>
#include <QTabWidget>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include "TestFunctionGroups.h"
#include "core/safety/TxRefusal.h"
#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/TxAnalyzer.h"
#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/session/StationCapabilities.h"
#include "core/session/IStationLink.h"
#include "core/session/StationClient.h"
#include "core/session/SliceAccessMirror.h"
#include "gui/meters/MeterPoller.h"
#include "gui/MainWindow.h"
#include "gui/MoxDisplayController.h"
#include "gui/RemoteMediaController.h"
#include "gui/OperatorReasonText.h"
#include "gui/RemoteConnectionController.h"
#include "gui/SetupDialog.h"
#include "gui/multidevice/MultiDeviceController.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "gui/SpectrumWidget.h"
#include "gui/TitleBar.h"
#include "gui/applets/PureSignalApplet.h"
#include "gui/applets/RadeApplet.h"
#include "gui/applets/RxApplet.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/TransmitHolder.h"
#include "gui/setup/DeviceCard.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "core/IoBoardHl2.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "gui/setup/hardware/AntennaAlexAntennaControlTab.h"
#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/accessories/AlexController.h"
#include "core/RadioDiscovery.h"
#include "gui/widgets/VfoWidget.h"
#include "gui/widgets/StationBlock.h"
#include "models/BandPlanManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/RemoteWindowHarness.h"
#include "core/SliceOwnership.h"
#include "core/session/DeviceSessionRegistry.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SliceChooser.h"
#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StatusToast.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

// Long enough for several queued turns and any backoff step a case uses
// to fire if something were wrongly scheduled.
constexpr int kSettleMs = 300;

// Entry points (entryPointsStartAnExplicitConnect).
constexpr int kTitleBar = 0;
constexpr int kStationBlock = 1;
constexpr int kDisconnectedPan = 2;
constexpr int kSetupConnections = 3;

// Disconnect surfaces (cancelDuringBackoffStopsTheRetry,
// operatorDisconnectOpensConnectionsOnce).
constexpr int kRadioMenuDisconnect = 0;
constexpr int kCorePanelDisconnect = 1;

void clickLeft(QWidget* widget)
{
    QTest::mouseClick(widget, Qt::LeftButton, Qt::NoModifier,
                      widget->rect().center());
}

// Slice control plan Task 17: a window that shares slices with the Core's
// other devices, on the Core's slices and a saved layout.
RemoteWindowHarness::Options sharingOptions(int stationSlices, const QString& layout)
{
    RemoteWindowHarness::Options options;
    options.stationSlices = stationSlices;
    options.panLayout = layout;
    options.sliceAccess = true;
    return options;
}

// Starts the window's own connection and waits until it shares slices
// and its mirror names it the controller of each slice it adopted.
bool connectSharing(RemoteWindowHarness& h)
{
    h.startStartupConnection();
    StationClient* client = h.client();
    if (!client || !QTest::qWaitFor([client] { return client->stationLinkReady(); }, 10000)) {
        return false;
    }
    if (!client->remoteSliceAccessAvailable()) { return false; }
    return QTest::qWaitFor([&h, client] {
        const QList<int> live = h.station().sliceOwnership()->liveSlices();
        for (int id : live) {
            const auto entry = client->sliceAccess()->entry(id);
            if (!entry || entry->controllerDeviceId != QStringLiteral("token:1")) { return false; }
        }
        return !live.isEmpty();
    }, 10000);
}

// A named phone on the Core, as tst_desktop_station_window admits one.
QByteArray admitPhone(RemoteWindowHarness& h, QObject& session)
{
    DeviceSessionRegistry::Entry phone;
    phone.deviceId = QByteArrayLiteral("phone-device-id-for-remote-window1");
    phone.kind = DeviceSessionRegistry::Kind::Paired;
    phone.name = QStringLiteral("Living room iPhone");
    phone.shortName = QStringLiteral("iPhone");
    phone.deviceKind = QStringLiteral("phone");
    const bool admitted = h.server().deviceSessions()->admit(phone, &session).admission
        == DeviceSessionRegistry::Admission::Admitted;
    return admitted ? phone.deviceId : QByteArray();
}

// A slice the phone controls on a pan this window does not have, that this
// window neither controls nor listens to. -1 when the Core did not add it.
int phonesSliceOnAnotherPan(RemoteWindowHarness& h, const QByteArray& phone)
{
    const int id = h.station().addSlice(QStringLiteral("pan-3"));
    if (id < 0) { return -1; }
    SliceOwnership* ownership = h.station().sliceOwnership();
    ownership->setOwner(id, phone);
    ownership->leave(QByteArrayLiteral("token:1"), id);
    StationClient* client = h.client();
    const bool known = QTest::qWaitFor([client, id, phone] {
        const auto entry = client->sliceAccess()->entry(id);
        return entry && !entry->listeners.contains(QStringLiteral("token:1"))
            && entry->controllerDeviceId != QStringLiteral("token:1");
    }, 10000);
    return known && !ownership->isListening(QByteArrayLiteral("token:1"), id) ? id : -1;
}

// The bottom RX area's chooser, opened as the operator opens it.
SliceChooser* openChooser(RemoteWindowHarness& h)
{
    auto* dashboard = h.window()->findChild<RxDashboard*>();
    if (!dashboard || !dashboard->chooserButton()) { return nullptr; }
    dashboard->chooserButton()->click();
    return h.window()->findChild<SliceChooser*>();
}

VfoWidget* flagFor(RemoteWindowHarness& h, int sliceId)
{
    for (VfoWidget* flag : h.window()->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) { return flag; }
    }
    return nullptr;
}

int toastsSaying(RemoteWindowHarness& h, const QString& words)
{
    int count = 0;
    for (StatusToast* toast : h.window()->findChildren<StatusToast*>()) {
        if (toast->message() == words) { ++count; }
    }
    return count;
}

// Radio > Connect, then wait for the Core's snapshot to be applied.
bool connectFromRadioMenu(RemoteWindowHarness& h)
{
    QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
    if (!connect || !connect->isEnabled()) { return false; }
    connect->trigger();
    return QTest::qWaitFor([&h] { return h.client()->isHandshakeComplete(); }, 10000);
}

bool disconnectFromRadioMenu(RemoteWindowHarness& h)
{
    QAction* disconnect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Disconnect"));
    if (!disconnect || !disconnect->isEnabled()) { return false; }
    disconnect->trigger();
    return true;
}

bool corePanelVisible(RemoteWindowHarness& h)
{
    auto* panel = h.window()->findChild<RemoteConnectionPanel*>();
    return panel && panel->isVisible();
}

// The Core panel, reached the way an operator reaches it on a connected
// window: right-click the title bar's connection segment and choose
// "Core connection details..." from the menu it opens.
bool openCorePanelFromTitleMenu(RemoteWindowHarness& h)
{
    ConnectionSegment* segment = h.titleSegment();
    if (!segment) { return false; }
    bool chosen = false;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &poll, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) { return; }
        poll.stop();
        for (QAction* action : menu->actions()) {
            if (action->text() == QStringLiteral("Core connection details...")) {
                menu->setActiveAction(action);
                QTest::keyClick(menu, Qt::Key_Return);
                chosen = true;
                return;
            }
        }
        menu->close();
    });
    poll.start();
    QTest::mouseClick(segment, Qt::RightButton, Qt::NoModifier, segment->rect().center());
    poll.stop();
    return chosen && corePanelVisible(h);
}

// File > Settings..., as the operator opens it.
SetupDialog* openSettings(RemoteWindowHarness& h)
{
    QAction* settings = h.menuAction(QStringLiteral("&File"), QStringLiteral("&Settings..."));
    if (!settings) { return nullptr; }
    settings->trigger();
    return h.window()->findChild<SetupDialog*>();
}

// Selects a Setup leaf in the page tree and returns the page on screen.
QWidget* showSetupLeaf(SetupDialog* dialog, const QString& label)
{
    auto* tree = dialog ? dialog->findChild<QTreeWidget*>() : nullptr;
    auto* stack = dialog ? dialog->findChild<QStackedWidget*>() : nullptr;
    if (!tree || !stack) { return nullptr; }
    const auto found = tree->findItems(label, Qt::MatchExactly | Qt::MatchRecursive);
    if (found.isEmpty()) { return nullptr; }
    tree->setCurrentItem(found.first());
    return stack->currentWidget();
}

// The Setup dialog's Remote Access page, reached the way an operator
// reaches it: File > Settings..., then its entry in the page tree.
QPushButton* openSetupConnectionsButton(RemoteWindowHarness& h)
{
    SetupDialog* dialog = openSettings(h);
    if (!showSetupLeaf(dialog, QStringLiteral("Remote Access"))) { return nullptr; }
    return dialog->findChild<QPushButton*>(QStringLiteral("remoteStationConnections"));
}

DeviceCard* deviceCardOf(QWidget* page, const QString& title)
{
    for (DeviceCard* card : page->findChildren<DeviceCard*>()) {
        if (card->title() == title) { return card; }
    }
    return nullptr;
}

// A card's buffer-size combo: the one whose first entry is 64 samples.
QComboBox* deviceCardBufferCombo(DeviceCard* card)
{
    for (QComboBox* combo : card->findChildren<QComboBox*>()) {
        if (combo->count() > 1 && combo->itemData(0).toInt() == 64) { return combo; }
    }
    return nullptr;
}

const QString kStationReason = QStringLiteral("Connect to the Core to change these.");

// R-R3-46: triggers Radio > Protocol Info and returns the text of the
// dialog it opens (closing it), or an empty string when none opened.
QString protocolInfoText(RemoteWindowHarness& h)
{
    QAction* info = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Protocol Info"));
    if (!info || !info->isEnabled()) { return {}; }
    QString text;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &poll, [&] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) { return; }
        poll.stop();
        text = box->text();
        box->accept();
    });
    poll.start();
    info->trigger();
    poll.stop();
    return text;
}

QStringList preampLabelsFor(HPSDRHW board)
{
    const BoardCapabilities& caps = BoardCapsTable::forBoard(board);
    QStringList labels;
    for (const auto& item : BoardCapsTable::preampItemsForBoard(board, caps.hasAlexFilters)) {
        labels.append(QString::fromLatin1(item.label));
    }
    return labels;
}

StationCapabilities coreRadio(RemoteWindowHarness& h, HPSDRHW board, HPSDRModel model,
                              const QString& mac)
{
    StationCapabilities caps = h.server().buildCapabilities();
    caps.board = board;
    caps.macAddress = mac;
    caps.radioConnected = true;
    caps.firmwareVersion = QStringLiteral("27");
    caps.radioIdentityEntries = true;
    caps.hpsdrModel = model;
    caps.radioProtocol = 2;
    caps.radioAddress = QStringLiteral("192.168.1.50");
    return caps;
}

QStringList sliceIds(const RadioModel& model)
{
    QStringList ids;
    for (SliceModel* slice : model.slices()) {
        ids << QString::number(slice->sliceIndex());
    }
    ids.sort();
    return ids;
}

} // namespace

class TestRemoteWindowHarness final : public QObject {
    Q_OBJECT

private slots:
    // Opt-in full-window companion to the remote pan capture. The Core and
    // window are real, but the trace rows are test-synthesised at the widget.
    void remoteTransmitDisplayWindowEvidence()
    {
        const QString output = qEnvironmentVariable("NEREUS_TX_DISPLAY_EVIDENCE");
        if (output.isEmpty()) { QSKIP("Set NEREUS_TX_DISPLAY_EVIDENCE to capture PNGs"); }
        auto analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId);
        RemoteWindowHarness h;
        h.station().setTxAnalyzer(analyzer.get());
        auto* coreSlice = h.station().activeSlice();
        QVERIFY(coreSlice);
        coreSlice->setFrequency(7'236'400.0);
        coreSlice->setDspMode(DSPMode::LSB);
        coreSlice->setFilterLow(-3000);
        coreSlice->setFilterHigh(-100);
        coreSlice->setTxSlice(true);
        QVERIFY(h.start());
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        auto* pan = h.panSpectrum(QStringLiteral("pan-0"));
        auto* display = h.window()->findChild<MoxDisplayController*>();
        auto* media = h.window()->findChild<RemoteMediaController*>();
        QVERIFY(pan && display && media);
        pan->setTxMode(DSPMode::LSB);
        pan->setVfoFrequency(7'236'400.0);
        pan->setTxFilterRange(100, 2900);
        pan->setTxFilterVisible(true);
        h.window()->resize(1280, 800);
        auto* mox = h.station().moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(display->isKeyed(), 5000);
        // MainWindow has no media transport seam. Supply this synthetic
        // post-reduction display directly after the Core's key reaches it.
        media->setPanTransmitting(QStringLiteral("pan-0"), true, false);
        pan->setTxCenterFrequency(7'236'400.0);
        pan->setTxSampleRate(8'000.0);
        pan->setDisplayWindowPreservingHistory(7'236'400.0, 8'000.0);
        for (int row = 0; row < 600; ++row) {
            QVector<float> trace(1280);
            for (int index = 0; index < trace.size(); ++index) {
                const double hz = 7'232'400.0 + (index + 0.5) * (8'000.0 / trace.size());
                const float wobble = float(std::sin(index * 0.37 + row * 0.21)) * 3.0f;
                if (hz >= 7'233'500.0 && hz <= 7'236'300.0) {
                    trace[index] = -18.0f + float(std::sin(hz / 420.0 + row * 0.09)) * 9.0f + wobble;
                } else {
                    const double skirt = hz > 7'236'300.0 ? hz - 7'236'300.0 : 7'233'500.0 - hz;
                    trace[index] = std::max(-66.0f + wobble, -30.0f - float(skirt / 60.0));
                }
            }
            pan->updateSpectrumFromTxPixels(-1, trace);
            pan->pushTxWaterfallRow(-1, trace);
            QCoreApplication::processEvents();
        }
        QVERIFY(pan->drawsSpectrumTrace());
        QVERIFY(h.window()->grab().save(output + QStringLiteral("/desktop-keyed-full-window.png")));
        h.station().swrProt().setEnabled(true);
        h.station().swrProt().setWindBackEnabled(true);
        for (int sample = 0; sample < 50 && !h.station().swrProt().highSwr(); ++sample) {
            h.station().swrProt().ingest(50.0f, 15.0f, false);
        }
        QTRY_VERIFY_WITH_TIMEOUT(pan->isHighSwrOverlayActive(), 5000);
        QVERIFY(pan->isHighSwrFoldback());
        QVERIFY(h.window()->grab().save(output + QStringLiteral("/desktop-high-swr-full-window.png")));
        mox->setMox(false);
        h.station().setTxAnalyzer(nullptr);
    }

    void initTestCase()
    {
        // A whole window logs every settings read at debug level; keep the
        // info and warning lines that explain a failure.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-window-harness")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // R-R3-16: each disconnected entry point dials the configured Core once.
    void entryPointsStartAnExplicitConnect_data()
    {
        QTest::addColumn<int>("entry");
        QTest::newRow("title bar") << kTitleBar;
        QTest::newRow("station block") << kStationBlock;
        QTest::newRow("disconnected pan") << kDisconnectedPan;
        QTest::newRow("Setup Connections") << kSetupConnections;
    }

    void entryPointsStartAnExplicitConnect()
    {
        QFETCH(int, entry);
        RemoteWindowHarness h;
        QVERIFY(h.start());
        StationClient* const client = h.client();
        QVERIFY(client);

        // Nothing dials by itself: the window was built with its startup
        // connection deferred, and showing it runs no automatic dial.
        QTest::qWait(kSettleMs);
        QCOMPARE(h.acceptedConnections(), 0);
        QVERIFY(!client->isConnectionActive());

        int expectedDials = 1;
        QPointer<QPushButton> setupConnections;
        if (entry == kSetupConnections) {
            // R-R3-21 / R-R3-10: Setup opens on the disconnected window
            // itself (it used to be refused until the Core's settings
            // arrived), and Remote Station is this computer's page.
            setupConnections = openSetupConnectionsButton(h);
            QVERIFY(setupConnections);
            QVERIFY(setupConnections->isEnabled());
            QTest::qWait(kSettleMs);
            QCOMPARE(h.acceptedConnections(), 0);
        }

        switch (entry) {
        case kTitleBar: {
            ConnectionSegment* segment = h.titleSegment();
            QVERIFY(segment);
            QCOMPARE(segment->state(), ConnectionState::Disconnected);
            clickLeft(segment);
            break;
        }
        case kStationBlock: {
            StationBlock* block = h.stationBlock();
            QVERIFY(block);
            clickLeft(block);
            break;
        }
        case kDisconnectedPan: {
            SpectrumWidget* pan = h.panSpectrum(QStringLiteral("pan-0"));
            QVERIFY(pan);
            clickLeft(pan);
            break;
        }
        case kSetupConnections: {
            QVERIFY(setupConnections);
            QVERIFY(setupConnections->isVisible());
            setupConnections->click();
            break;
        }
        }

        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 10000);
        QCOMPARE(h.acceptedConnections(), expectedDials);
        QCOMPARE(h.controls()->state(), ConnectionState::Connected);
        QCOMPARE(h.titleSegment()->state(), ConnectionState::Connected);
        QCOMPARE(h.titleSegment()->remoteStatusText(), QStringLiteral("Core connected"));

        // The same gesture on a live session never opens a second one.
        const quint32 epoch = client->sessionEpoch();
        if (entry == kTitleBar) {
            clickLeft(h.titleSegment());
        } else if (entry == kStationBlock) {
            clickLeft(h.stationBlock());
        }
        QTest::qWait(kSettleMs);
        QCOMPARE(h.acceptedConnections(), expectedDials);
        QCOMPARE(client->sessionEpoch(), epoch);
    }

    void connectedHeaderShowsCurrentSocketAndClearsOnDisconnect()
    {
        RemoteWindowHarness h;
        h.server().setMediaEnabled(true);
        QVERIFY(h.start());
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        h.window()->setConnectionPickerManaged(true);
        ConnectionSegment* segment = h.titleSegment();
        QVERIFY(segment);
        h.window()->resize(1440, 900);
        QCoreApplication::processEvents();
        QTRY_VERIFY(segment->remotePresentationText().contains(QStringLiteral("Traffic")));
        QVERIFY(segment->remotePresentationText().contains(QStringLiteral("Audio")));
        QVERIFY(segment->remotePresentationText().contains(QStringLiteral("Radio")));
        QVERIFY(segment->remotePresentationText().contains(QStringLiteral("Core RTT")));
        const QFontMetrics headerMetrics(QFont(QStringLiteral("SF Mono"), 10, QFont::DemiBold));
        // Load findings 3: the segment takes its width from the layout pass
        // after the text changes (and the readings keep changing it), so
        // the width is checked once the layout has settled on the text.
        QTRY_VERIFY2(segment->width() >= headerMetrics.horizontalAdvance(segment->remotePresentationText()) + 34,
                 qPrintable(QStringLiteral("segment %1, text %2")
                                .arg(segment->width())
                                .arg(headerMetrics.horizontalAdvance(segment->remotePresentationText()))));
        clickLeft(segment);
        QTRY_VERIFY(segment->routePopup()->isVisible());
        auto* controls = segment->routePopup()->findChild<QLabel*>(QStringLiteral("controlsRoute"));
        QVERIFY(controls);
        QTRY_VERIFY(controls->text().contains(QStringLiteral("socket endpoints")));
        QVERIFY(controls->text().contains(QStringLiteral("Direct")));
        QVERIFY(controls->text().contains(QStringLiteral("Local")));
        QVERIFY(controls->text().contains(QStringLiteral("Remote")));
        h.remoteModel()->audioEngine()->setMasterMuted(true);
        QTRY_VERIFY(segment->remotePresentationText().contains(QStringLiteral("Audio muted")));
        QTRY_VERIFY2(segment->width() >= headerMetrics.horizontalAdvance(segment->remotePresentationText()) + 34,
                 qPrintable(QStringLiteral("muted segment %1, text %2")
                                .arg(segment->width())
                                .arg(headerMetrics.horizontalAdvance(segment->remotePresentationText()))));
        // Every audio group the header can show must fit beside the other
        // three at 1440 px too, the longest ("Audio unavailable") above
        // all. The segment is laid out with each state's text in place (the
        // layout runs here, before the next telemetry refresh can put the
        // muted text back), so the check measures the width the title bar
        // gives that text in this platform's fonts, not whether the width
        // chosen for "Audio muted" happens to cover it.
        const QStringList liveGroups = segment->remotePresentationText().split(QStringLiteral(" · "));
        QCOMPARE(liveGroups.size(), 4);
        QList<QLayout*> layouts;
        for (QWidget* widget = segment->parentWidget(); widget; widget = widget->parentWidget()) {
            if (widget->layout()) {
                layouts.prepend(widget->layout());
            }
        }
        // The row the segment draws starts 25 px in (8 px margin, 10 px dot,
        // 8 px gap) and stops 6 px short of the right edge
        // (ConnectionSegment::paintEvent).
        constexpr int kSegmentTextInset = 25 + 6;
        using AudioState = RemoteAudioStatus::State;
        for (AudioState state : {AudioState::NotConnected, AudioState::WaitingForAudio,
                                 AudioState::MutedHere, AudioState::RadioOffline,
                                 AudioState::CoreCouldNotStart, AudioState::Starting,
                                 AudioState::Playing, AudioState::Reconnecting,
                                 AudioState::PlaybackProblem}) {
            QStringList groups = liveGroups;
            groups[1] = ConnectionSegment::audioMetricText(std::nullopt, state);
            const QString text = groups.join(QStringLiteral(" · "));
            segment->setRemoteMetrics(groups);
            for (QLayout* layout : std::as_const(layouts)) {
                layout->activate();
            }
            QCOMPARE(segment->remotePresentationText(), text);
            QVERIFY2(segment->width() >= headerMetrics.horizontalAdvance(text) + 34,
                     qPrintable(QStringLiteral("audio state %1: segment %2, text %3: %4")
                                    .arg(static_cast<int>(state))
                                    .arg(segment->width())
                                    .arg(headerMetrics.horizontalAdvance(text))
                                    .arg(text)));
            // All four groups are drawn, Radio included.
            QCOMPARE(segment->remoteTextForWidth(segment->width() - kSegmentTextInset), text);
        }
        h.remoteModel()->audioEngine()->setMasterMuted(false);
        QVERIFY(disconnectFromRadioMenu(h));
        QTRY_VERIFY(!h.client()->isConnectionActive());
        QVERIFY(!segment->routePopup()->isVisible());
        QVERIFY(controls->text().contains(QStringLiteral("Path unavailable")));
    }

    void setupConnectionsAsksManagedPickerWithoutDialing()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        h.window()->setConnectionPickerManaged(true);
        QSignalSpy requests(h.window(), &MainWindow::connectionsRequested);
        QPushButton* connections = openSetupConnectionsButton(h);
        QVERIFY(connections);
        QVERIFY(connections->isEnabled());
        connections->click();
        QCOMPARE(requests.size(), 1);
        QCOMPARE(h.acceptedConnections(), 0);
        QVERIFY(!h.client()->isConnectionActive());
    }

    // R-R3-16 / R-R3-17: cancelling while a retry waits stops it for good.
    void cancelDuringBackoffStopsTheRetry_data()
    {
        QTest::addColumn<int>("surface");
        QTest::newRow("Radio > Disconnect") << kRadioMenuDisconnect;
        QTest::newRow("Core panel Disconnect") << kCorePanelDisconnect;
    }

    void cancelDuringBackoffStopsTheRetry()
    {
        QFETCH(int, surface);
        RemoteWindowHarness::Options options;
        options.backoffUnitMs = 1000;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        StationClient* const client = h.client();
        QSignalSpy retries(client, &StationClient::reconnectScheduled);

        // The title bar dial also opens the Core panel the second surface uses.
        clickLeft(h.titleSegment());
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 10000);

        h.dropLink();
        QTRY_VERIFY_WITH_TIMEOUT(client->isReconnectPending(), 5000);
        QCOMPARE(retries.size(), 1);
        QCOMPARE(h.controls()->state(), ConnectionState::LinkLost);
        QVERIFY(h.titleSegment()->remoteStatusText().startsWith(QStringLiteral("Retrying Core")));

        if (surface == kRadioMenuDisconnect) {
            QVERIFY(disconnectFromRadioMenu(h));
        } else {
            auto* panel = h.window()->findChild<RemoteConnectionPanel*>();
            QVERIFY(panel);
            auto* stop = panel->findChild<QPushButton*>(QStringLiteral("disconnectCore"));
            QVERIFY(stop);
            QVERIFY(stop->isEnabled());
            stop->click();
        }

        // Past the first retry's deadline: nothing may dial.
        QTest::qWait(2 * options.backoffUnitMs);
        QCOMPARE(h.acceptedConnections(), 1);
        QCOMPARE(retries.size(), 1);
        QVERIFY(!client->isConnectionActive());
        QVERIFY(!client->isReconnectPending());

        // The window stays disconnected and says so where it persists.
        QCOMPARE(h.controls()->state(), ConnectionState::Disconnected);
        QCOMPARE(h.controls()->statusText(), QStringLiteral("Core disconnected"));
        QCOMPARE(h.titleSegment()->state(), ConnectionState::Disconnected);
        QCOMPARE(h.titleSegment()->remoteStatusText(), QStringLiteral("Core disconnected"));
        QVERIFY(h.stationBlock()->radioName().contains(h.controls()->endpointText()));
        QCOMPARE(h.stationBlock()->hardwareLine(), QStringLiteral("Core disconnected"));
        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QAction* disconnect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Disconnect"));
        QVERIFY(connect && disconnect);
        QVERIFY(connect->isEnabled());
        QVERIFY(!disconnect->isEnabled());
    }

    // R-R3-16 / R-R3-38: the operator's own Disconnect opens Connections
    // exactly once with the picker managing the window, and shows the Core
    // panel in direct mode (a --station window has no picker). Neither
    // dials: the Core still accepted exactly one connection. The window's
    // automatic open on a Disconnected state is for local models only, so
    // it adds no second open here.
    void operatorDisconnectOpensConnectionsOnce_data()
    {
        QTest::addColumn<bool>("picker");
        QTest::addColumn<int>("surface");
        QTest::newRow("direct, Radio > Disconnect") << false << kRadioMenuDisconnect;
        QTest::newRow("direct, Core panel Disconnect") << false << kCorePanelDisconnect;
        QTest::newRow("picker, Radio > Disconnect") << true << kRadioMenuDisconnect;
        QTest::newRow("picker, Core panel Disconnect") << true << kCorePanelDisconnect;
    }

    void operatorDisconnectOpensConnectionsOnce()
    {
        QFETCH(bool, picker);
        QFETCH(int, surface);
        RemoteWindowHarness h;
        QVERIFY(h.start());
        // GuiConnectionController makes this call on every window it
        // attaches; a --station window never gets it.
        h.window()->setConnectionPickerManaged(picker);
        StationClient* const client = h.client();

        // The window's own startup connection, as the application starts it.
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 10000);
        QVERIFY(h.remoteModel()->isConnected());
        QVERIFY(!h.remoteModel()->name().isEmpty());

        QPointer<RemoteConnectionPanel> panel;
        if (surface == kCorePanelDisconnect) {
            QVERIFY(openCorePanelFromTitleMenu(h));
            panel = h.window()->findChild<RemoteConnectionPanel*>();
            QVERIFY(panel);
            QVERIFY(panel->isVisible());
        } else {
            QVERIFY(!corePanelVisible(h));
        }

        QSignalSpy connectionsRequested(h.window(), &MainWindow::connectionsRequested);
        QSignalSpy operatorDisconnects(h.controls(),
                                       &RemoteConnectionController::operatorDisconnected);
        if (surface == kRadioMenuDisconnect) {
            QVERIFY(disconnectFromRadioMenu(h));
        } else {
            auto* stop = panel->findChild<QPushButton*>(QStringLiteral("disconnectCore"));
            QVERIFY(stop);
            QVERIFY(stop->isEnabled());
            stop->click();
        }
        QTest::qWait(kSettleMs);

        QCOMPARE(operatorDisconnects.size(), 1);
        QCOMPARE(connectionsRequested.size(), picker ? 1 : 0);
        if (!picker || surface == kCorePanelDisconnect) {
            // Direct mode shows the Core panel; a panel the operator
            // already had open stays open.
            QVERIFY(corePanelVisible(h));
        } else {
            QVERIFY(!corePanelVisible(h));
        }
        QCOMPARE(h.acceptedConnections(), 1);
        QVERIFY(!client->isConnectionActive());
        QVERIFY(!client->isReconnectPending());
        QCOMPARE(h.controls()->statusText(), QStringLiteral("Core disconnected"));
    }

    // R-R3-16 / R-R3-17: link loss is not the operator's Disconnect.
    // Nothing opens; the title bar says the window is retrying; the retry
    // reaches the Core.
    void linkLossOpensNothingAndRetries_data()
    {
        QTest::addColumn<bool>("picker");
        QTest::newRow("direct mode") << false;
        QTest::newRow("picker mode") << true;
    }

    void linkLossOpensNothingAndRetries()
    {
        QFETCH(bool, picker);
        RemoteWindowHarness::Options options;
        // Long enough to read the retrying state before the redial.
        options.backoffUnitMs = 1000;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.window()->setConnectionPickerManaged(picker);
        StationClient* const client = h.client();
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 10000);
        QVERIFY(!corePanelVisible(h));

        QSignalSpy connectionsRequested(h.window(), &MainWindow::connectionsRequested);
        QSignalSpy operatorDisconnects(h.controls(),
                                       &RemoteConnectionController::operatorDisconnected);
        QSignalSpy retries(client, &StationClient::reconnectScheduled);
        h.dropLink();
        QTRY_VERIFY_WITH_TIMEOUT(client->isReconnectPending(), 5000);
        QCOMPARE(retries.size(), 1);
        QCOMPARE(h.controls()->state(), ConnectionState::LinkLost);
        QCOMPARE(h.titleSegment()->remoteStatusText(),
                 QStringLiteral("Retrying Core (attempt 1)"));

        // The redial happens, and still nothing opened.
        QTRY_VERIFY_WITH_TIMEOUT(h.acceptedConnections() == 2
                                 && client->isHandshakeComplete(), 10000);
        QTest::qWait(kSettleMs);
        QCOMPARE(connectionsRequested.size(), 0);
        QCOMPARE(operatorDisconnects.size(), 0);
        QVERIFY(!corePanelVisible(h));
        QCOMPARE(h.titleSegment()->remoteStatusText(), QStringLiteral("Core connected"));
    }

    // R-R3-17: a Core session whose radio is offline is not a disconnected
    // window. Nothing opens; the station block says the radio is offline;
    // the session stays up.
    void radioOfflineOpensNothing_data()
    {
        QTest::addColumn<bool>("picker");
        QTest::newRow("direct mode") << false;
        QTest::newRow("picker mode") << true;
    }

    void radioOfflineOpensNothing()
    {
        QFETCH(bool, picker);
        RemoteWindowHarness h;
        QVERIFY(h.start());
        h.window()->setConnectionPickerManaged(picker);
        StationClient* const client = h.client();
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 10000);
        QVERIFY(h.remoteModel()->isConnected());
        QVERIFY(!h.remoteModel()->name().isEmpty());
        QVERIFY(!corePanelVisible(h));

        QSignalSpy connectionsRequested(h.window(), &MainWindow::connectionsRequested);
        QSignalSpy operatorDisconnects(h.controls(),
                                       &RemoteConnectionController::operatorDisconnected);
        h.reportRadioOffline();
        QTRY_VERIFY_WITH_TIMEOUT(!h.remoteModel()->isConnected(), 5000);
        QCOMPARE(h.remoteModel()->connectionState(), ConnectionState::Disconnected);
        QTest::qWait(kSettleMs);

        QCOMPARE(h.stationBlock()->hardwareLine(), QStringLiteral("Radio offline"));
        QCOMPARE(h.controls()->state(), ConnectionState::Connected);
        QCOMPARE(h.titleSegment()->remoteStatusText(), QStringLiteral("Core connected"));
        QCOMPARE(connectionsRequested.size(), 0);
        QCOMPARE(operatorDisconnects.size(), 0);
        QVERIFY(!corePanelVisible(h));
        QVERIFY(client->isHandshakeComplete());
        QCOMPARE(h.acceptedConnections(), 1);
    }

    // R-R3-24: the extra-slice startup reproduction through the real
    // connection, snapshot and populateEmptyPans path.
    void heldSnapshotCreatesNoSliceOnConnectOrReconnect_data()
    {
        QTest::addColumn<int>("stationSlices");
        QTest::addColumn<QString>("layout");
        QTest::newRow("one slice, one saved pan") << 1 << QStringLiteral("1");
        QTest::newRow("one slice, two saved pans") << 1 << QStringLiteral("2v");
        QTest::newRow("two slices, two saved pans") << 2 << QStringLiteral("2v");
    }

    void heldSnapshotCreatesNoSliceOnConnectOrReconnect()
    {
        QFETCH(int, stationSlices);
        QFETCH(QString, layout);
        RemoteWindowHarness::Options options;
        options.stationSlices = stationSlices;
        options.panLayout = layout;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        StationClient* const client = h.client();
        RadioModel* const remote = h.remoteModel();
        QSignalSpy stationAdds(&h.station(), &RadioModel::sliceAdded);
        QSignalSpy connectedStates(remote, &RadioModel::connectionStateChanged);
        QCOMPARE(sliceIds(h.station()).size(), stationSlices);
        const QStringList panIds = MainWindow::panIdsForLayout(layout);
        // The saved layout was restored: its last pan exists.
        QVERIFY(h.panSpectrum(panIds.constLast()));

        const auto connectedCount = [&connectedStates] {
            int n = 0;
            for (const QList<QVariant>& args : connectedStates) {
                if (args.constFirst().value<ConnectionState>() == ConnectionState::Connected) {
                    ++n;
                }
            }
            return n;
        };

        quint32 previousEpoch = client->sessionEpoch();
        for (int attachment = 0; attachment < 2; ++attachment) {
            if (attachment == 1 && panIds.size() > 1) {
                // Before the link drops, the Core closes whatever sits on the
                // saved layout's last pan, the way an operator at the station
                // would. The window keeps its retained slices across a drop,
                // so without an empty pan the reconnect could not create
                // anything whether or not the guard held; this makes the
                // reconnect half test the guard on its own. The single-pan
                // row cannot test the populatePanSlices guard on reconnect:
                // its only pan always holds the Core's only slice.
                for (SliceModel* slice : h.station().slicesOnPan(panIds.constLast())) {
                    h.station().removeSlice(slice->sliceIndex());
                }
                QTRY_VERIFY(remote->pansWithoutSlices(panIds).contains(panIds.constLast()));
                QCOMPARE(sliceIds(*remote), sliceIds(h.station()));
            }

            // This attachment's baseline: nothing below may add to it.
            const QStringList stationIds = sliceIds(h.station());
            const int addsBefore = static_cast<int>(stationAdds.size());
            const int connectedBefore = connectedCount();

            h.holdNextSnapshot();
            if (attachment == 0) {
                h.startStartupConnection();
            } else {
                h.dropLink();
            }
            QTRY_VERIFY_WITH_TIMEOUT(h.acceptedConnections() == attachment + 1
                                     && h.snapshotHeld(), 10000);

            // Evidence of this attachment's own session before the negative
            // check: a new epoch, and the window's model reporting the radio
            // connected again from this session's capabilities. That report
            // is what queues the window's real populateEmptyPans().
            QTRY_VERIFY_WITH_TIMEOUT(client->sessionEpoch() != previousEpoch
                                     && connectedCount() == connectedBefore + 1, 10000);
            previousEpoch = client->sessionEpoch();
            QVERIFY(remote->isConnected());
            // The slices have not arrived; give the queued call every chance
            // to run.
            QTest::qWait(kSettleMs);
            QVERIFY(!client->isHandshakeComplete());
            QVERIFY2(h.addSliceCommands().isEmpty(),
                     qPrintable(h.addSliceCommands().join(QLatin1Char(','))));
            QCOMPARE(static_cast<int>(stationAdds.size()), addsBefore);
            QCOMPARE(sliceIds(h.station()), stationIds);

            h.releaseSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 10000);
            QTest::qWait(kSettleMs);
            QVERIFY2(h.addSliceCommands().isEmpty(),
                     qPrintable(h.addSliceCommands().join(QLatin1Char(','))));
            QCOMPARE(static_cast<int>(stationAdds.size()), addsBefore);
            QCOMPARE(sliceIds(h.station()), stationIds);
            QCOMPARE(sliceIds(*remote), stationIds);
        }

        // Hydration and layout restore did not create; an explicit operator
        // create still does, once, within the station's capacity.
        const int slicesBeforeAdd = static_cast<int>(h.station().slices().size());
        QAction* add = h.menuAction(QStringLiteral("&View"),
                                    QStringLiteral("&Add slice on active pan"));
        QVERIFY(add);
        add->trigger();
        QTRY_COMPARE(h.addSliceCommands().size(), 1);
        QVERIFY(h.addSliceCommands().first().startsWith(QStringLiteral("addSliceOnPan:pan-")));
        QTRY_COMPARE(static_cast<int>(h.station().slices().size()), slicesBeforeAdd + 1);
        QTRY_COMPARE(static_cast<int>(remote->slices().size()), slicesBeforeAdd + 1);
        QTest::qWait(kSettleMs);
        QCOMPARE(h.addSliceCommands().size(), 1);
        QCOMPARE(h.acceptedConnections(), 2);
    }

    // R-R3-21 / R-R3-10 / R-R3-17: a remote window that has never
    // connected opens Setup. This computer's settings work (a Devices
    // change sticks and is the one used once connected); the Core's
    // settings wait, with the reason, and nothing reaches the Core; once
    // connected the Core's pages are built from the Core's values; after
    // Disconnect they are disabled again.
    void disconnectedWindowSetupKeepsThisComputersSettings()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        StationClient* const client = h.client();
        QTest::qWait(kSettleMs);
        QVERIFY(!client->isConnectionActive());
        QSignalSpy writes(&h.proxy(), &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&h.proxy(), &SettingsProxy::outboundRemoveRequested);
        // Seeds the window's own models made at startup, before Setup.
        const QSet<QString> seededAtStartup = h.proxy().droppedWhileOffline();

        SetupDialog* const dialog = openSettings(h);
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        auto* const notice = dialog->findChild<QLabel*>(QStringLiteral("setupStationUnavailable"));
        QVERIFY(notice);

        // A Core page: a stand-in with the reason, not ship defaults.
        QWidget* nb = showSetupLeaf(dialog, QStringLiteral("NB/SNB"));
        QVERIFY(nb);
        QCOMPARE(nb->objectName(), QStringLiteral("setupStationPlaceholder"));
        QVERIFY(!nb->isEnabled());
        QVERIFY(notice->isVisible());
        QCOMPARE(notice->text(), kStationReason);

        // This computer's microphone buffer, on Devices.
        QWidget* const devices = showSetupLeaf(dialog, QStringLiteral("Devices"));
        QVERIFY(devices);
        QVERIFY(devices->isEnabled());
        QVERIFY(!notice->isVisible());
        DeviceCard* const mic = deviceCardOf(devices, QStringLiteral("TX Input (Microphone)"));
        QVERIFY(mic);
        QVERIFY(mic->isEnabled());
        QComboBox* const buffer = deviceCardBufferCombo(mic);
        QVERIFY(buffer);
        const int next = (buffer->currentIndex() + 1) % buffer->count();
        const int samples = buffer->itemData(next).toInt();
        buffer->setCurrentIndex(next);  // the card saves after its 200 ms debounce
        const QString bufferKey = QStringLiteral("audio/TxInput/BufferSamples");
        QTRY_COMPARE(AppSettings::instance().value(bufferKey).toString(), QString::number(samples));
        QTRY_COMPARE(h.remoteModel()->localAudioDevices()->txInputConfig().bufferSamples, samples);

        // Nothing towards the Core, and nothing held to be sent later.
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);
        QVERIFY((h.proxy().droppedWhileOffline() - seededAtStartup).isEmpty());
        QCOMPARE(h.acceptedConnections(), 0);

        // Connect from the window. The Core's page is built from the
        // Core's settings and is live.
        QVERIFY(connectFromRadioMenu(h));
        QTRY_VERIFY_WITH_TIMEOUT(
            (nb = showSetupLeaf(dialog, QStringLiteral("NB/SNB")))
                && nb->objectName() != QStringLiteral("setupStationPlaceholder")
                && nb->isEnabled(),
            5000);
        QVERIFY(!notice->isVisible());

        // The Devices change stuck and is the one the window uses; it never
        // went to the Core.
        QCOMPARE(AppSettings::instance().value(bufferKey).toString(), QString::number(samples));
        QCOMPARE(h.remoteModel()->localAudioDevices()->txInputConfig().bufferSamples, samples);
        QVERIFY(!h.stationSettings().contains(bufferKey));
        QVERIFY(showSetupLeaf(dialog, QStringLiteral("Devices"))->isEnabled());

        // The operator's Disconnect: the Core's page is disabled again,
        // with the reason, and still shows the Core's last values.
        QVERIFY(disconnectFromRadioMenu(h));
        QTRY_VERIFY(!client->isConnectionActive());
        nb = showSetupLeaf(dialog, QStringLiteral("NB/SNB"));
        QTRY_VERIFY(!nb->isEnabled());
        QCOMPARE(nb->objectName() == QStringLiteral("setupStationPlaceholder"), false);
        QVERIFY(notice->isVisible());
        QCOMPARE(notice->text(), kStationReason);
        QVERIFY(showSetupLeaf(dialog, QStringLiteral("Devices"))->isEnabled());
        QCOMPARE(h.acceptedConnections(), 1);
    }

    // R-R3-21 / R-R3-10 (R3 Setup fix wave, final review I2): connected to
    // a Core whose settings never arrive (its profile is empty and was never
    // marked), Setup's Core pages wait as stand-ins, say what is true (not
    // "Connect to the Core"), and nothing is sent.
    void connectedWithoutTheCoresSettingsCorePagesWait()
    {
        RemoteWindowHarness h;
        h.stationSettings().remove(QLatin1String(AppSettings::kDaemonProfileSeededKey));
        QVERIFY(h.start());
        QVERIFY(connectFromRadioMenu(h));
        // R-R3-49: past the Core's 500 ms coalesced save, which the window's
        // admission schedules. It failed this case whenever it landed before
        // the look (on a busy computer); the harness now makes it at the
        // admission, into the Core's own store, and this wait keeps a
        // regression of that from hiding behind a quick run.
        QTest::qWait(kSettleMs + 600);
        QVERIFY(h.proxy().ready());
        QVERIFY(h.proxy().hasReceivedSnapshot());
        QVERIFY(!h.proxy().setupDialogAllowed());
        QSignalSpy writes(&h.proxy(), &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&h.proxy(), &SettingsProxy::outboundRemoveRequested);

        SetupDialog* const dialog = openSettings(h);
        QVERIFY(dialog);
        auto* const notice = dialog->findChild<QLabel*>(QStringLiteral("setupStationUnavailable"));
        QVERIFY(notice);
        QWidget* const nb = showSetupLeaf(dialog, QStringLiteral("NB/SNB"));
        QVERIFY(nb);
        QCOMPARE(nb->objectName(), QStringLiteral("setupStationPlaceholder"));
        QVERIFY(!nb->isEnabled());
        QVERIFY(notice->isVisible());
        const QString reason = QStringLiteral("The Core has not sent its settings.");
        QCOMPARE(notice->text(), reason);
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(showSetupLeaf(dialog, QStringLiteral("Devices"))->isEnabled());
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);

        // Disconnected, the reason asks to connect again.
        QVERIFY(disconnectFromRadioMenu(h));
        QTRY_VERIFY(!h.client()->isConnectionActive());
        showSetupLeaf(dialog, QStringLiteral("NB/SNB"));
        QTRY_COMPARE(notice->text(), kStationReason);
    }

    // R-R3-17 / R-R3-21 (R3 Setup fix wave, final review M1): connected
    // once, then disconnected, the operator opens Setup and visits every
    // Core page (built from the Core's last values, disabled). None of
    // them records an edit, so the reconnect warns about nothing.
    void setupOpenedWhileDisconnectedRecordsNoEdit()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        StationClient* const client = h.client();
        QVERIFY(connectFromRadioMenu(h));
        QVERIFY(disconnectFromRadioMenu(h));
        QTRY_VERIFY(!client->isConnectionActive());
        QTest::qWait(kSettleMs);
        const QSet<QString> heldBefore = h.proxy().droppedWhileOffline();
        QSignalSpy writes(&h.proxy(), &SettingsProxy::outboundWriteRequested);
        QSignalSpy removes(&h.proxy(), &SettingsProxy::outboundRemoveRequested);

        SetupDialog* const dialog = openSettings(h);
        QVERIFY(dialog);
        auto* const tree = dialog->findChild<QTreeWidget*>();
        auto* const stack = dialog->findChild<QStackedWidget*>();
        QVERIFY(tree && stack);
        int corePages = 0;
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            const int index = (*it)->data(0, Qt::UserRole).toInt();
            if (index < 0 || dialog->pageScopeAtForTest(index) != SetupScope::Core) {
                continue;
            }
            tree->setCurrentItem(*it);
            QWidget* const page = stack->currentWidget();
            QVERIFY(page);
            // Built from the Core's last values, not a stand-in, and disabled.
            QVERIFY2(page->objectName() != QStringLiteral("setupStationPlaceholder"),
                     qPrintable((*it)->text(0)));
            QVERIFY2(!page->isEnabled(), qPrintable((*it)->text(0)));
            ++corePages;
        }
        // R-R3-49: three Core leaves are not registered while their
        // features are not built (TX Profiles, Signal Generator, Hardware
        // Tests).
        QVERIFY(corePages >= 22);
        QCOMPARE(writes.size(), 0);
        QCOMPARE(removes.size(), 0);
        const QSet<QString> held = h.proxy().droppedWhileOffline() - heldBefore;
        QVERIFY2(held.isEmpty(),
                 qPrintable(QStringList(held.cbegin(), held.cend()).join(QStringLiteral(", "))));
        QCOMPARE(h.proxy().droppedWhileOffline(), heldBefore);

        QSignalSpy superseded(&h.proxy(), &SettingsProxy::offlineEditsSuperseded);
        // R-R3-46 (carried): the gap between Connect and the snapshot. An
        // edit dropped after the check above (while the link came back)
        // would show as a key the Core's snapshot contradicts.
        QVERIFY(connectFromRadioMenu(h));
        QTest::qWait(kSettleMs);
        QCOMPARE(superseded.size(), 0);
        const QSet<QString> contradicted = h.proxy().keysContradictedByLastSnapshot();
        QVERIFY2(contradicted.isEmpty(),
                 qPrintable(QStringList(contradicted.cbegin(), contradicted.cend())
                                .join(QStringLiteral(", "))));
        QCOMPARE(h.acceptedConnections(), 2);
    }

    // R-R3-17 / R-R3-21: a fresh remote window's first connect tells the
    // operator nothing about edits that did not stick, because it made
    // none. Building the window's own models (the band plan in particular)
    // must not count as a change made while the link was down.
    void freshWindowFirstConnectRaisesNoOfflineEditWarning()
    {
        RemoteWindowHarness h;
        // A Core that has run before holds its band plan choice, and TCI
        // settings stored while they were still the Core's (before R-R3-42
        // made them each computer's own; the window ignores them).
        AppSettings& core = h.stationSettings();
        core.setValue(QStringLiteral("BandPlanName"), QStringLiteral("ARRL (US)"));
        core.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"), QStringLiteral("True"));
        core.setValue(QStringLiteral("TciEmulateSunSDR2Pro"), QStringLiteral("True"));
        core.setValue(QStringLiteral("TciSliceAGain"), QStringLiteral("-6"));
        core.setValue(QStringLiteral("TciTxGain"), QStringLiteral("-3"));
        QVERIFY(h.start());
        QTest::qWait(kSettleMs);
        QVERIFY2(h.proxy().droppedWhileOffline().isEmpty(),
                 qPrintable(QStringList(h.proxy().droppedWhileOffline().cbegin(),
                                        h.proxy().droppedWhileOffline().cend())
                                .join(QStringLiteral(", "))));
        QSignalSpy superseded(&h.proxy(), &SettingsProxy::offlineEditsSuperseded);

        QVERIFY(connectFromRadioMenu(h));
        QTest::qWait(kSettleMs);
        QCOMPARE(superseded.size(), 0);
        QCOMPARE(h.acceptedConnections(), 1);
        // R-R3-42: the window's TCI settings are its own; the Core's stored
        // values are ignored, not copied here.
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("TciSliceAGain")));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("TciTxGain")));
        QVERIFY(!h.proxy().handlesKey(QStringLiteral("TciSliceAGain")));
    }

    // R-R3-21: the meter update interval (MultimeterDelayMs) is the Core's
    // setting. The window's meter poller read it at startup, before the
    // Core's settings arrived; it takes the Core's value once they do.
    void meterIntervalFollowsTheCoresSetting()
    {
        RemoteWindowHarness h;
        h.stationSettings().setValue(QStringLiteral("MultimeterDelayMs"), 250);
        QVERIFY(h.start());
        MeterPoller* poller = h.window()->radioModel()->meterPoller();
        QVERIFY(poller != nullptr);
        QCOMPARE(poller->intervalMs(), 100);  // no Core settings yet
        QVERIFY(connectFromRadioMenu(h));
        QTRY_COMPARE(poller->intervalMs(), 250);
    }

    // D79 (R-IOS-11, R-R3-49): the band plan is the Core's. The window
    // read its plan at startup, before the Core's settings arrived; it
    // takes the Core's plan once they do and follows every later change,
    // and so do its strip and its View > Band Plan check.
    void windowFollowsTheCoresBandPlan()
    {
        const QString key = QStringLiteral("BandPlanName");
        RemoteWindowHarness h;
        h.stationSettings().setValue(key, QStringLiteral("IARU Region 2"));
        QVERIFY(h.start());
        RadioModel* model = h.window()->radioModel();
        QVERIFY(model != nullptr);
        const BandPlanManager& plans = model->bandPlanManager();
        QCOMPARE(plans.activePlanName(), QStringLiteral("ARRL (US)"));  // no Core settings yet

        // The strip on every pan draws this manager's plan; the menu's
        // check is the one plan named.
        auto stripDraws = [&](const QString& name) {
            const QList<SpectrumWidget*> strips = h.window()->findChildren<SpectrumWidget*>();
            if (strips.isEmpty() || plans.activePlanName() != name) {
                return false;
            }
            for (SpectrumWidget* strip : strips) {
                if (strip->bandPlanManager() != &plans) {
                    return false;
                }
            }
            for (const BandPlanManager::PlanData& plan : plans.plans()) {
                if (plan.name == name) {
                    return plans.segments().size() == plan.segments.size()
                        && plans.spots().size() == plan.spots.size();
                }
            }
            return false;
        };
        auto checkedPlans = [&]() {
            QStringList checked;
            for (const QString& name : plans.availablePlans()) {
                QAction* action = h.menuAction(QStringLiteral("&Band Plan"), name);
                if (action == nullptr) {
                    checked << QStringLiteral("<no action for %1>").arg(name);
                } else if (action->isChecked()) {
                    checked << name;
                }
            }
            return checked;
        };
        QCOMPARE(checkedPlans(), QStringList{QStringLiteral("ARRL (US)")});

        QVERIFY(connectFromRadioMenu(h));
        QTRY_VERIFY(stripDraws(QStringLiteral("IARU Region 2")));
        QCOMPARE(checkedPlans(), QStringList{QStringLiteral("IARU Region 2")});

        // A later change on the Core (another device's pick) reaches it.
        h.stationSettings().setValue(key, QStringLiteral("RAC (Canada)"));
        QTRY_VERIFY(stripDraws(QStringLiteral("RAC (Canada)")));
        QCOMPARE(checkedPlans(), QStringList{QStringLiteral("RAC (Canada)")});

        // A removal on the Core: ARRL (US), the Core's default.
        h.stationSettings().remove(key);
        QTRY_VERIFY(stripDraws(QStringLiteral("ARRL (US)")));
        QCOMPARE(checkedPlans(), QStringList{QStringLiteral("ARRL (US)")});
        // Following wrote nothing back to the Core.
        QTest::qWait(kSettleMs);
        QVERIFY(!h.stationSettings().contains(key));
    }

    // R-R3-46 / R-R3-21: a Core whose controller stands behind its
    // `stepAtt` object (radioHardwareVersion 1). The window's RX applet row
    // and Setup's Step Attenuator and Auto Attenuate groups are enabled and
    // show the Core's settled values; a change made in either round-trips
    // through the Core's controller; a change on the Core reaches both; the
    // overload alarm lights on the Core's overload report.
    void attenuatorControlsUseTheCoresObject()
    {
        StepAttenuatorController coreAtt;
        coreAtt.setTickTimerEnabled(false);
        coreAtt.setStepAttEnabled(true);
        coreAtt.setAttenuation(12);
        RemoteWindowHarness h;
        h.station().setStepAttController(&coreAtt);
        QVERIFY(h.start());
        QVERIFY(connectFromRadioMenu(h));
        QVERIFY(h.client()->remoteRadioHardwareAvailable());

        auto* rx = h.window()->findChild<RxApplet*>();
        QVERIFY(rx);
        auto* att = rx->findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(att);
        auto* spin = att->findChild<QSpinBox*>();
        QVERIFY(spin);
        QTRY_VERIFY(att->isEnabled());
        QVERIFY(att->toolTip().isEmpty());
        QTRY_COMPARE(spin->value(), 12);
        QCOMPARE(spin->maximum(), coreAtt.maxAttenuation());
        QCOMPARE(rx->attLabelTextForTest(), QStringLiteral("S-ATT"));

        // The window's change reaches the Core's controller.
        spin->setValue(18);
        QTRY_COMPARE(coreAtt.attenuatorDb(), 18);

        // Setup > General > Options shows the Core's values too.
        SetupDialog* dialog = openSettings(h);
        QVERIFY(dialog);
        dialog->selectPage(QStringLiteral("Options"));
        GeneralOptionsPage* page = nullptr;
        QTRY_VERIFY((page = dialog->findChild<GeneralOptionsPage*>()) != nullptr);
        auto* stepGroup = page->findChild<QGroupBox*>(QStringLiteral("grpStepAttenuator"));
        auto* autoGroup = page->findChild<QGroupBox*>(QStringLiteral("grpAutoAttRx1"));
        QVERIFY(stepGroup && autoGroup);
        QVERIFY(stepGroup->isEnabled());
        QVERIFY(autoGroup->isEnabled());
        auto* pageSpin = stepGroup->findChild<QSpinBox*>();
        QVERIFY(pageSpin);
        QCOMPARE(pageSpin->value(), 18);

        // A change on the Core reaches both.
        coreAtt.setAttenuation(7);
        QTRY_COMPARE(spin->value(), 7);
        QTRY_COMPARE(pageSpin->value(), 7);

        // Auto-attenuate, from Setup, round-trips through the Core.
        QCheckBox* autoEnable = nullptr;
        for (QCheckBox* box : autoGroup->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Enable")) { autoEnable = box; }
        }
        QVERIFY(autoEnable);
        QVERIFY(!coreAtt.autoAttEnabled());
        autoEnable->click();
        QTRY_VERIFY(coreAtt.autoAttEnabled());
        QTRY_COMPARE(rx->attLabelTextForTest(), QStringLiteral("A-ATT"));

        // The preamp: turning the step attenuator off on the Core shows the
        // combo, and the window's choice reaches the Core.
        coreAtt.setStepAttEnabled(false);
        QTRY_COMPARE(rx->attLabelTextForTest(), QStringLiteral("ATT"));
        auto* combo = att->findChild<QComboBox*>();
        QVERIFY(combo);
        if (combo->count() > 1) {
            const int other = combo->currentIndex() == 0 ? 1 : 0;
            combo->setCurrentIndex(other);
            QTRY_COMPARE(static_cast<int>(coreAtt.preampMode()), combo->itemData(other).toInt());
        }

        // The overload alarm lights on the Core's report.
        auto* badge = h.window()->findChild<QWidget*>(QStringLiteral("adcOvlBadge"));
        QVERIFY(badge);
        const auto opacity = [badge] {
            auto* fx = qobject_cast<QGraphicsOpacityEffect*>(badge->graphicsEffect());
            return fx ? fx->opacity() : 1.0;
        };
        QVERIFY(opacity() < 0.5);
        coreAtt.onAdcOverflow(0);
        coreAtt.tick();
        QTRY_COMPARE(opacity(), 1.0);
        QVERIFY(badge->toolTip().contains(QStringLiteral("ADC0: overload")));

        h.station().setStepAttController(nullptr);
    }

    // R-R3-46: Hardware Config in a window connected to a Core that offers
    // it (radioHardwareVersion 2). The tabs are live and show the Core's
    // radio; an RX antenna change reaches the Core's own AlexController;
    // an OC receive pin reaches the Core's settings for that radio and the
    // Core reloads its matrix; the transmit fields that key nothing follow
    // their own versions (parity tasks).
    void hardwareConfigReceiveSettingsReachTheCore()
    {
        StepAttenuatorController coreAtt;
        coreAtt.setTickTimerEnabled(false);
        RemoteWindowHarness h;
        h.station().setStepAttController(&coreAtt);
        const auto unbind = qScopeGuard([&h] { h.station().setStepAttController(nullptr); });
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:53");
        RadioInfo radio;
        radio.macAddress = mac;
        radio.boardType = HPSDRHW::Saturn;
        h.station().setLastRadioInfoForTest(radio);
        QStringList reloads;
        h.station().setHardwareApplyObserverForTest(
            [&reloads](const QString& name) { reloads << name; });
        QVERIFY(h.start());
        QVERIFY(connectFromRadioMenu(h));
        QVERIFY(h.client()->remoteHardwareConfigAvailable());
        QTRY_COMPARE(h.remoteModel()->currentRadioInfo().macAddress, mac);

        SetupDialog* dialog = openSettings(h);
        QVERIFY(dialog);
        QWidget* const page = showSetupLeaf(dialog, QStringLiteral("Hardware Config"));
        auto* hardware = qobject_cast<HardwarePage*>(page);
        QVERIFY(hardware);
        QVERIFY(hardware->isEnabled());
        QTRY_VERIFY(hardware->remoteEditsAvailableForTest());
        QVERIFY(hardware->findChild<QTabWidget*>()->isEnabled());

        // RX antenna: the Core's controller changes.
        auto* antennas = hardware->findChild<AntennaAlexAntennaControlTab*>();
        QVERIFY(antennas);
        // Parity Task 12: the TX antennas go to the Core too (version 6);
        // tst_remote_tx_antennas covers them.
        QTRY_VERIFY(antennas->txGridForTest()->isEnabled());
        QRadioButton* const ant2 = antennas->rxButtonForTest(Band::Band40m, 2);
        QVERIFY(ant2 && ant2->isEnabled());
        ant2->click();
        QTRY_COMPARE(h.station().alexController().rxAnt(Band::Band40m), 2);
        QTRY_VERIFY(ant2->isChecked());

        // OC receive pin: the Core's settings for its radio, and a reload.
        auto* hf = hardware->findChild<OcOutputsHfTab*>();
        QVERIFY(hf);
        QCheckBox* pin = nullptr;
        for (QCheckBox* box : hf->findChildren<QCheckBox*>()) {
            if (box->toolTip() == QStringLiteral("RX OC pin 4, band 20m")) { pin = box; }
        }
        QVERIFY(pin && pin->isEnabled());
        pin->click();
        const QString key = QStringLiteral("hardware/%1/oc/rx/20m/pin4").arg(mac);
        QTRY_COMPARE(h.stationSettings().value(key).toString(), QStringLiteral("True"));
        QTRY_VERIFY(reloads.contains(QStringLiteral("oc")));

        // R-R3-46 fix wave (radioHardwareVersion 3): the HL2 I/O board tab
        // shows the Core's board, whose readings arrive on the Core after a
        // probe. (4 since the filter policy verb, R-R3-46 / R-R3-21.)
        QCOMPARE(h.client()->capabilities().radioHardwareVersion, 13);
        auto* ioTab = hardware->findChild<Hl2IoBoardTab*>();
        QVERIFY(ioTab);
        const auto statusText = [ioTab]() {
            for (QLabel* label : ioTab->findChildren<QLabel*>()) {
                if (label->text().startsWith(QStringLiteral("mi0bot custom I/O board"))) {
                    return label->text();
                }
            }
            return QString();
        };
        QCOMPARE(statusText(), QStringLiteral("mi0bot custom I/O board (0x41): Not detected"));
        IoBoardHl2& coreBoard = h.station().ioBoardMutable();
        coreBoard.setRegisterValue(IoBoardHl2::Register::REG_FIRMWARE_MAJOR, 0x02);
        coreBoard.setHardwareVersion(IoBoardHl2::kHardwareVersion1);
        coreBoard.setDetected(true);
        QTRY_COMPARE(statusText(), QStringLiteral("mi0bot custom I/O board (0x41): Active"));
    }

    // R-R3-46 / R-R3-21: a Core that does not offer its attenuator
    // (radioHardwareVersion 0) leaves the window's rows disabled, with the
    // plain reason through OperatorReasonText, and a click changes nothing.
    void olderCoreLeavesAttenuatorControlsDisabledWithAReason()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        QVERIFY(connectFromRadioMenu(h));
        QVERIFY(!h.client()->remoteRadioHardwareAvailable());
        const QString reason =
            OperatorReasonText::forDisplay(h.client()->radioHardwareUnavailableReason());
        QVERIFY(!reason.isEmpty());
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));

        auto* rx = h.window()->findChild<RxApplet*>();
        QVERIFY(rx);
        auto* att = rx->findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(att);
        QTRY_COMPARE(att->toolTip(), reason);
        QVERIFY(!att->isEnabled());

        SetupDialog* dialog = openSettings(h);
        QVERIFY(dialog);
        dialog->selectPage(QStringLiteral("Options"));
        GeneralOptionsPage* page = nullptr;
        QTRY_VERIFY((page = dialog->findChild<GeneralOptionsPage*>()) != nullptr);
        for (const char* name : {"grpStepAttenuator", "grpAutoAttRx1"}) {
            auto* group = page->findChild<QGroupBox*>(QLatin1String(name));
            QVERIFY2(group, name);
            QVERIFY2(!group->isEnabled(), name);
            QCOMPARE(group->toolTip(), reason);
        }
    }

    // GUI-I3 (fix wave): the PureSignal applet follows the Core's radio.
    // A radio with PureSignal hardware on a Core that has not advertised
    // PureSignal 3 shows the applet disabled with the reason (before, it was
    // hidden); PureSignal 3 enables it; a radio without the hardware (Atlas)
    // hides it, as a local window does.
    void pureSignalAppletIsDisabledNotHiddenBelowPureSignal3()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        QVERIFY(connectFromRadioMenu(h));
        auto* applet = h.window()->findChild<PureSignalApplet*>();
        QVERIFY(applet);
        const QString reason = QStringLiteral("The connected Core has not advertised PureSignal 3.");

        StationCapabilities older = coreRadio(h, HPSDRHW::Saturn, HPSDRModel::ANAN_G2_1K,
                                              QStringLiteral("AA:BB:CC:DD:EE:46"));
        older.psAlgorithmVersion = 0;
        h.pushCapabilities(older);
        QTRY_COMPARE(h.client()->capabilities().psAlgorithmVersion, 0);
        QTRY_VERIFY(!applet->isHidden());
        QVERIFY(!applet->isEnabled());
        QCOMPARE(applet->toolTip(), reason);
        QVERIFY(OperatorWording::isPlain(reason));

        StationCapabilities ps3 = older;
        ps3.psAlgorithmVersion = 3;
        h.pushCapabilities(ps3);
        QTRY_VERIFY(applet->isEnabled());
        QVERIFY(!applet->isHidden());
        QVERIFY(applet->toolTip().isEmpty());

        h.pushCapabilities(coreRadio(h, HPSDRHW::Atlas, HPSDRModel::HPSDR,
                                     QStringLiteral("AA:BB:CC:DD:EE:01")));
        QTRY_VERIFY(applet->isHidden());
    }

    // R-R3-46: the window follows the Core's radio. A Saturn ANAN-G2 1K
    // brings its preamp items and attenuator range to the RX applet, its
    // antenna labels to the VFO flag and its tabs to Hardware Config;
    // Protocol Info shows P2 and the radio's address; a Core whose radio is
    // offline gives Unknown, not Hermes; an older Core still shows what it
    // has.
    void windowFollowsTheCoresRadio()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        QVERIFY(connectFromRadioMenu(h));
        RadioModel* const windowModel = h.remoteModel();
        QVERIFY(windowModel);
        auto* rx = h.window()->findChild<RxApplet*>();
        QVERIFY(rx);

        // First a radio whose items and range differ from the G2 1K's.
        h.pushCapabilities(coreRadio(h, HPSDRHW::Angelia, HPSDRModel::ANAN100D,
                                     QStringLiteral("AA:BB:CC:DD:EE:10")));
        QTRY_COMPARE(rx->preampComboLabelsForTest(), preampLabelsFor(HPSDRHW::Angelia));
        const int angeliaMax = BoardCapsTable::stepAttMaxDb(
            HPSDRHW::Angelia, BoardCapsTable::forBoard(HPSDRHW::Angelia).hasAlexFilters);
        QCOMPARE(rx->stepAttMaxForTest(), angeliaMax);
        QVERIFY(preampLabelsFor(HPSDRHW::Angelia) != preampLabelsFor(HPSDRHW::Saturn));

        h.pushCapabilities(coreRadio(h, HPSDRHW::Saturn, HPSDRModel::ANAN_G2_1K,
                                     QStringLiteral("AA:BB:CC:DD:EE:46")));
        QTRY_COMPARE(rx->preampComboLabelsForTest(), preampLabelsFor(HPSDRHW::Saturn));
        const int saturnMax = BoardCapsTable::stepAttMaxDb(
            HPSDRHW::Saturn, BoardCapsTable::forBoard(HPSDRHW::Saturn).hasAlexFilters);
        QVERIFY(saturnMax != angeliaMax);
        QCOMPARE(rx->stepAttMaxForTest(), saturnMax);
        QCOMPARE(rx->stepAttMinForTest(),
                 BoardCapsTable::forBoard(HPSDRHW::Saturn).attenuator.minDb);
        QCOMPARE(windowModel->hardwareProfile().model, HPSDRModel::ANAN_G2_1K);

        const auto expectedLabels = [](HPSDRModel sku) {
            const auto labels = skuUiProfileFor(sku).rxOnlyLabels;
            return QStringList(labels.cbegin(), labels.cend());
        };
        const QList<VfoWidget*> flags = h.window()->findChildren<VfoWidget*>();
        QVERIFY(!flags.isEmpty());
        for (VfoWidget* flag : flags) {
            QCOMPARE(flag->rxOnlyAntennaLabelsForTest(), expectedLabels(HPSDRModel::ANAN_G2_1K));
        }
        QVERIFY(expectedLabels(HPSDRModel::ANAN_G2_1K) != expectedLabels(HPSDRModel::HERMES));

        // Hardware Config shows the Saturn's tabs, then follows a change.
        SetupDialog* dialog = openSettings(h);
        QVERIFY(dialog);
        QVERIFY(showSetupLeaf(dialog, QStringLiteral("Hardware Config")));
        auto* hardware = dialog->findChild<HardwarePage*>();
        QVERIFY(hardware);
        const BoardCapabilities& saturn = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QCOMPARE(hardware->isTabVisibleForTest(HardwarePage::Tab::AntennaAlex),
                 saturn.hasAlexFilters);
        // R-R3-49: the Diversity tab was removed, remote as well as local.
        for (const QTabWidget* tabs : hardware->findChildren<QTabWidget*>()) {
            for (int i = 0; i < tabs->count(); ++i) {
                QVERIFY(tabs->tabText(i) != QStringLiteral("Diversity"));
            }
        }
        QVERIFY(!hardware->isTabVisibleForTest(HardwarePage::Tab::Hl2Options));
        QCOMPARE(hardware->tabTextForTest(HardwarePage::Tab::OcOutputs),
                 QStringLiteral("OC Outputs"));
        StationCapabilities hl2 = coreRadio(h, HPSDRHW::HermesLite, HPSDRModel::HERMESLITE,
                                            QStringLiteral("AA:BB:CC:DD:EE:02"));
        hl2.radioProtocol = 1;
        h.pushCapabilities(hl2);
        QTRY_VERIFY(hardware->isTabVisibleForTest(HardwarePage::Tab::Hl2Options));
        QCOMPARE(hardware->tabTextForTest(HardwarePage::Tab::OcOutputs),
                 QStringLiteral("Hermes Lite Control"));
        dialog->close();

        // Protocol Info shows the Core's radio.
        h.pushCapabilities(coreRadio(h, HPSDRHW::Saturn, HPSDRModel::ANAN_G2_1K,
                                     QStringLiteral("AA:BB:CC:DD:EE:46")));
        QTRY_COMPARE(windowModel->currentRadioInfo().macAddress,
                     QStringLiteral("AA:BB:CC:DD:EE:46"));
        QAction* info = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Protocol Info"));
        QVERIFY(info);
        QVERIFY(info->isEnabled());
        QVERIFY(OperatorWording::isPlain(info->toolTip()));
        const QString shown = protocolInfoText(h);
        QVERIFY2(shown.contains(QStringLiteral("Protocol: P2")), qPrintable(shown));
        QVERIFY2(shown.contains(QStringLiteral("192.168.1.50")), qPrintable(shown));
        QVERIFY2(shown.contains(QStringLiteral("AA:BB:CC:DD:EE:46")), qPrintable(shown));
        QVERIFY2(shown.contains(QStringLiteral("Firmware: 27")), qPrintable(shown));

        // A Core whose radio is offline: Unknown, never Hermes.
        StationCapabilities offline = h.server().buildCapabilities();
        offline.board = HPSDRHW::Unknown;
        offline.macAddress.clear();
        offline.radioConnected = false;
        offline.radioIdentityEntries = true;
        offline.hpsdrModel = HPSDRModel::FIRST;
        offline.radioProtocol = 0;
        offline.radioAddress.clear();
        h.pushCapabilities(offline);
        QTRY_COMPARE(h.client()->capabilities().board, HPSDRHW::Unknown);
        QCOMPARE(windowModel->hardwareProfile().model, HPSDRModel::FIRST);
        QCOMPARE(windowModel->boardCapabilities().board, HPSDRHW::Unknown);

        // An older Core (none of the three entries): Protocol Info shows
        // what it has and says what it does not.
        StationCapabilities older = coreRadio(h, HPSDRHW::Saturn, HPSDRModel::FIRST,
                                              QStringLiteral("AA:BB:CC:DD:EE:46"));
        older.radioIdentityEntries = false;
        older.radioProtocol = 0;
        older.radioAddress.clear();
        h.pushCapabilities(older);
        QTRY_COMPARE(windowModel->hardwareProfile().model, HPSDRModel::ANAN_G2);
        QVERIFY(info->isEnabled());
        const QString partial = protocolInfoText(h);
        QVERIFY2(partial.contains(QStringLiteral("AA:BB:CC:DD:EE:46")), qPrintable(partial));
        QVERIFY2(partial.contains(QStringLiteral("not reported by the Core")),
                 qPrintable(partial));
        QVERIFY(OperatorWording::isPlain(QStringLiteral("not reported by the Core")));
        QVERIFY(h.client()->isHandshakeComplete());
    }

    // R-R3-21: a capability change from the Core re-gates the window on the
    // live session.
    void capabilityChangeRegatesWithoutReconnect()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        StationClient* const client = h.client();
        QVERIFY(connectFromRadioMenu(h));
        const quint32 epoch = client->sessionEpoch();

        // R-R3-49 (parity Task 4): Tools > TX Equalizer opens in a remote
        // window whatever the Core permits (the dialog shows why it is
        // greyed), so the remote transmit push is watched on the TX
        // applet's MOX button, which keeps it. (Transmit group fix wave 2:
        // VOX also waits for this computer's microphone line, which this
        // harness never opens, so it stays disabled with that reason.)
        QPushButton* txEq = nullptr;
        for (QPushButton* b : h.window()->findChildren<QPushButton*>()) {
            if (b->accessibleName() == QStringLiteral("MOX transmit")) {
                txEq = b;
            }
        }
        QVERIFY(txEq);
        auto* vox = h.window()->findChild<QPushButton*>(QStringLiteral("TxVoxButton"));
        QVERIFY(vox);
        auto* txEqualizer = h.window()->findChild<QAction*>(QStringLiteral("toolsTxEqualizer"));
        QVERIFY(txEqualizer && txEqualizer->isEnabled());
        QVERIFY(!client->capabilities().txPermitted);
        QVERIFY(!txEq->isEnabled());
        const QString reason = txEq->toolTip();
        QVERIFY(!reason.isEmpty());
        // R-R3-49 (parity Task 3): the RADE applet's profile combo and Reset
        // vocoder follow the transmit settings gate, not the remote transmit
        // push: the Core takes them while its radio is off the air.
        auto* rade = h.window()->findChild<RadeApplet*>();
        QVERIFY(rade);
        QComboBox* const radeProfile = rade->profileComboForTest();
        QPushButton* const radeReset = rade->resetVocoderButtonForTest();
        QTRY_VERIFY(radeProfile->isEnabled());
        QVERIFY(radeReset->isEnabled());

        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(txEq->isEnabled());
        QVERIFY(client->capabilities().txPermitted);
        QVERIFY(txEq->toolTip() != reason);
        QVERIFY(!vox->isEnabled());
        QCOMPARE(vox->toolTip(), TxRefusals::micNotConnected().text);
        QVERIFY(radeProfile->isEnabled());
        QVERIFY(radeReset->isEnabled());

        StationCapabilities withdrawn = h.server().buildCapabilities();
        withdrawn.txPermitted = false;
        h.pushCapabilities(withdrawn);
        QTRY_VERIFY(!txEq->isEnabled());
        QCOMPARE(txEq->toolTip(), reason);
        QVERIFY(radeProfile->isEnabled());
        QVERIFY(radeReset->isEnabled());

        // A Core without the TX profile commands (transmitSettingsVersion
        // below 3) greys both with the plain reason, on the live session.
        StationCapabilities older = h.server().buildCapabilities();
        older.transmitSettingsVersion = 2;
        h.pushCapabilities(older);
        QTRY_VERIFY(!radeProfile->isEnabled());
        QCOMPARE(radeProfile->toolTip(), IStationLink::transmitSettingsUnavailableReason());
        QVERIFY(!radeReset->isEnabled());
        QCOMPARE(radeReset->toolTip(), IStationLink::transmitSettingsUnavailableReason());

        QCOMPARE(client->sessionEpoch(), epoch);
        QCOMPARE(h.acceptedConnections(), 1);
        QVERIFY(client->isHandshakeComplete());
    }

    // Slice control plan Task 5: a window whose Core sends no
    // sliceAccessVersion (this bench window signs in with the token and
    // never declares sliceAccess) holds no access object, marks none of its
    // slices read-only, refuses the four slice access requests here without
    // sending them, and its flag still tunes the Core as before.
    void aWindowWithoutSliceAccessTunesAsBefore()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        h.startStartupConnection();
        StationClient* client = h.client();
        QVERIFY(client);
        QTRY_VERIFY_WITH_TIMEOUT(client->stationLinkReady(), 10000);
        QVERIFY(!client->remoteSliceAccessAvailable());
        QVERIFY(!client->capabilities().sliceAccessEntry);
        QCOMPARE(client->capabilities().sliceAccessVersion, 0);
        QVERIFY(client->sliceAccess()->entries().isEmpty());
        RadioModel* windowModel = h.remoteModel();
        QTRY_VERIFY(windowModel->sliceById(0) != nullptr);
        for (SliceModel* slice : windowModel->slices()) {
            QVERIFY(!slice->isReadOnlyListener());
        }

        IStationLink* link = client;
        const QList<IStationLink::CommandOutcome> outcomes{
            link->requestListen(0, 1), link->requestStopListening(0, 1),
            link->requestTakeControl(0, 1, 1), link->requestRelease(0, 1, 1)};
        for (const IStationLink::CommandOutcome& outcome : outcomes) {
            QVERIFY(!outcome.sent);
            QCOMPARE(outcome.commandId, quint32(0));
            QCOMPARE(outcome.reason, IStationLink::sliceAccessUnavailableReason());
            QVERIFY(OperatorWording::isPlain(outcome.reason));
        }

        // The flag's wheel reaches the Core.
        VfoWidget* flag = nullptr;
        QTRY_VERIFY([&]() {
            for (VfoWidget* candidate : h.window()->findChildren<VfoWidget*>()) {
                if (candidate->sliceIndex() == 0) {
                    flag = candidate;
                    return true;
                }
            }
            return false;
        }());
        // Slice control plan Task 14a: a Core that does not share slices
        // gives the flag no access line and holds nothing.
        QCOMPARE(flag->sliceAccess().state, VfoWidget::SliceAccess::State::Unshared);
        QVERIFY(flag->accessLineText().isEmpty());
        QVERIFY(!flag->isListening());
        SliceModel* coreSlice = h.station().sliceById(0);
        QVERIFY(coreSlice);
        const double before = coreSlice->frequency();
        const QPointF at(flag->width() / 2.0, flag->height() / 2.0);
        QWheelEvent wheel(at, flag->mapToGlobal(at), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(flag, &wheel);
        QTRY_VERIFY(coreSlice->frequency() != before);
        QVERIFY(!windowModel->sliceById(0)->isReadOnlyListener());
    }

    // Slice control plan Task 11 fix: a remote window's flag TX button asks
    // the Core with tx.setTxSlice exactly as the TX applet's letter row
    // does. TX badge take (JJ's ruling): while this window does not hold
    // transmit the badge takes it first (at once when nobody holds it,
    // through the take question when another device does) and then makes
    // the slice the TX slice; the letter row stays held. Nothing keys: the
    // Core has no radio.
    void theFlagsTxButtonMovesTransmitLikeTheLetterRow()
    {
        RemoteWindowHarness::Options options;
        options.stationSlices = 2;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        StationClient* client = h.client();
        QVERIFY(client);
        client->setTokenSessionHolderForTest(QStringLiteral("token:1"));
        h.startStartupConnection();
        QTRY_VERIFY_WITH_TIMEOUT(client->stationLinkReady(), 10000);
        QVERIFY(client->sessionHolderAvailable());
        QVERIFY(client->remoteTransmitAvailable());
        TxSliceArbiter* arbiter = h.station().txSliceArbiter();
        QVERIFY(arbiter);

        VfoWidget* flag = nullptr;
        QTRY_VERIFY([&]() {
            for (VfoWidget* candidate : h.window()->findChildren<VfoWidget*>()) {
                if (candidate->sliceIndex() == 1) {
                    flag = candidate;
                    return true;
                }
            }
            return false;
        }());
        auto* badge = flag->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        // The TX applet rebuilds its letters as the holder changes: find
        // slice B's afresh each time.
        const auto letterB = [&h]() {
            return h.window()->findChild<QPushButton*>(QStringLiteral("TxSliceButtonB"));
        };
        QTRY_VERIFY(letterB() != nullptr);

        // Refusals stay the Core's (fix round 1): this bench window is not
        // paired, so the Core refuses its transmit in its own words and the
        // badge is held with them, whoever holds transmit. A click asks
        // nothing and sends nothing.
        const QString notPaired = TxRefusals::deviceNotPaired().text;
        QSignalSpy finished(client, &StationClient::deviceCommandFinished);
        QTRY_COMPARE(client->capabilities().txRefusalReason, notPaired);
        QTRY_VERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), notPaired);
        flag->simulateTxBadgeClick();
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        TransmitHolder::Holder phoneHolder;
        phoneHolder.deviceId = phone;
        phoneHolder.name = QStringLiteral("Living room iPhone");
        phoneHolder.shortName = QStringLiteral("iPhone");
        phoneHolder.kind = QStringLiteral("phone");
        TransmitHolder* holder = h.server().transmitHolder();
        holder->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        QVERIFY(!client->holdsTransmitHere());
        QTest::qWait(kSettleMs);
        QVERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), notPaired);
        flag->simulateTxBadgeClick();
        QTest::qWait(kSettleMs);
        QVERIFY(h.window()->findChild<TakeTransmitDialog*>() == nullptr);
        QVERIFY(finished.isEmpty());
        QVERIFY(holder->isHeldBy(phone));

        // A Core that permits this window's transmit (its radio up; the
        // bench Core has none, so the capability says so for it): another
        // device holding transmit is what a take answers. The letter row is
        // held with the Core's reason; the badge offers the take.
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        QTRY_COMPARE(letterB()->toolTip(), TxRefusals::notHolder().text);
        QVERIFY(!letterB()->isEnabled());
        QTRY_VERIFY(badge->isEnabled());
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take transmit from iPhone and make this the TX slice"));

        // Cancelled question: nothing moves.
        badge->click();
        QPointer<TakeTransmitDialog> question;
        QTRY_VERIFY((question = h.window()->findChild<TakeTransmitDialog*>()) != nullptr);
        question->cancelButton()->click();
        QTRY_VERIFY(question.isNull());
        QTest::qWait(kSettleMs);
        QVERIFY(holder->isHeldBy(phone));
        QVERIFY(h.txSliceCommands().isEmpty());

        // Taken: the Core answers. This bench window is a token session,
        // which the Core never lets transmit, so it refuses in its own
        // words; nothing moves and the badge still offers the take.
        badge->click();
        QTRY_VERIFY((question = h.window()->findChild<TakeTransmitDialog*>()) != nullptr);
        question->takeButton()->click();
        QTRY_COMPARE(toastsSaying(h, notPaired), 1);
        QVERIFY(holder->isHeldBy(phone));
        QVERIFY(h.txSliceCommands().isEmpty());
        QVERIFY(badge->isEnabled());

        // Nobody holds transmit: the take is sent at once, no question.
        holder->release(phone, QStringLiteral("test"));
        QTRY_VERIFY(!client->transmitHeldElsewhere());
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take transmit and make this the TX slice"));
        QVERIFY(badge->isEnabled());
        finished.clear();
        badge->click();
        QTRY_VERIFY(!finished.isEmpty());
        QCOMPARE(finished.first().at(0).toByteArray(), QByteArrayLiteral("tx.take"));
        QVERIFY(!finished.first().at(2).toBool());
        QCOMPARE(finished.first().at(3).toString(), notPaired);
        QVERIFY(h.window()->findChild<TakeTransmitDialog*>() == nullptr);
        QVERIFY(h.txSliceCommands().isEmpty());

        // Holding transmit (handed over on the Core; nothing keys): the
        // flag's press sends the letter row's tx.setTxSlice, never a move
        // on the window's own model.
        TransmitHolder::Holder self;
        self.deviceId = QByteArrayLiteral("token:1");
        self.name = QStringLiteral("Bench window");
        holder->transferTo(self, QStringLiteral("test"));
        QTRY_VERIFY(client->holdsTransmitHere());
        QTRY_VERIFY(badge->isEnabled());
        QTRY_VERIFY(letterB() && letterB()->isEnabled());
        QCOMPARE(letterB()->toolTip(), QStringLiteral("Transmit on slice B"));
        QVERIFY(arbiter->txBoundSliceId() != 1);
        badge->click();
        QTRY_COMPARE(h.txSliceCommands(), QList<int>({1}));
        letterB()->click();
        QTRY_COMPARE(h.txSliceCommands(), QList<int>({1, 1}));
        QVERIFY(!h.station().mox());
    }

    // TX badge take fix round 1: a window the Core lets transmit (the
    // bench window counted as paired). Case 2: the take question's take
    // brings transmit here and then tx.setTxSlice. Case 3 on the phone's
    // only slice: the slice take frees transmit (ruling Q8), and the Core
    // sends the change of holder before its answer to slice.takeControl,
    // so no question is asked and tx.take goes out at once. Nothing keys.
    void remoteTxBadgeTakesTransmitOnASessionTheCoreLetsTransmit()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        StationClient* client = h.client();
        QVERIFY(client);
        QVERIFY(connectSharing(h));
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        TransmitHolder::Holder phoneHolder;
        phoneHolder.deviceId = phone;
        phoneHolder.name = QStringLiteral("Living room iPhone");
        phoneHolder.shortName = QStringLiteral("iPhone");
        phoneHolder.kind = QStringLiteral("phone");
        TransmitHolder* holder = h.server().transmitHolder();
        TxSliceArbiter* arbiter = h.station().txSliceArbiter();
        QVERIFY(arbiter);
        const QByteArray self = QByteArrayLiteral("token:1");

        // Case 2.
        holder->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        QTRY_VERIFY(flagFor(h, 1) != nullptr);
        auto* badge = flagFor(h, 1)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        QTRY_VERIFY(badge->isEnabled());
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take transmit from iPhone and make this the TX slice"));
        badge->click();
        QPointer<TakeTransmitDialog> question;
        QTRY_VERIFY((question = h.window()->findChild<TakeTransmitDialog*>()) != nullptr);
        question->takeButton()->click();
        QTRY_VERIFY(client->holdsTransmitHere());
        QVERIFY(holder->isHeldBy(self));
        QTRY_COMPARE(h.txSliceCommands(), QList<int>({1}));
        QTRY_COMPARE(arbiter->txBoundSliceId(), 1);
        QVERIFY(!h.station().mox());

        // Case 3 on the phone's only slice.
        SliceOwnership* ownership = h.station().sliceOwnership();
        holder->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        ownership->setOwner(1, phone);
        // The phone's transmit is on B, the one slice it has.
        if (arbiter->txBoundSliceId() != 1) {
            QVERIFY(arbiter->requestHandoff(1, phone));
        }
        QTRY_COMPARE(arbiter->txBoundSliceId(), 1);
        QTRY_VERIFY(flagFor(h, 1) && flagFor(h, 1)->isListening());
        badge = flagFor(h, 1)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QTRY_VERIFY(badge->isEnabled());
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take control of this slice, then take transmit from iPhone"));
        QSignalSpy finished(client, &StationClient::deviceCommandFinished);
        int dialogs = 0;
        QTimer watch;
        QObject::connect(&watch, &QTimer::timeout, &watch, [&h, &dialogs]() {
            if (h.window()->findChild<TakeTransmitDialog*>()) { ++dialogs; }
        });
        watch.start(5);
        badge->click();
        QTRY_COMPARE(ownership->mark(1).owner, self);
        QTRY_VERIFY(client->holdsTransmitHere());
        QTRY_COMPARE(h.txSliceCommands(), QList<int>({1, 1}));
        QTest::qWait(kSettleMs);
        watch.stop();
        QCOMPARE(dialogs, 0);
        QVERIFY(h.window()->findChild<TakeTransmitDialog*>() == nullptr);
        QStringList verbs;
        for (const QList<QVariant>& answer : finished) {
            verbs.append(QString::fromLatin1(answer.at(0).toByteArray()));
        }
        QCOMPARE(verbs, QStringList({QStringLiteral("slice.takeControl"),
                                     QStringLiteral("tx.take"),
                                     QStringLiteral("tx.setTxSlice")}));
        QVERIFY(!h.station().mox());
    }

    // TX badge take fix round 1: a badge take does not outlive the link. A
    // take question open when the link drops, and a slice take sent just
    // before it drops, are over: after the redial a take of transmit this
    // window makes by other means selects no slice, and the operator's own
    // Take control of a slice goes on to nothing. Nothing keys.
    void remoteTxBadgeTakeDoesNotOutliveTheLink()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        StationClient* client = h.client();
        QVERIFY(client);
        QVERIFY(connectSharing(h));
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        TransmitHolder::Holder phoneHolder;
        phoneHolder.deviceId = phone;
        phoneHolder.name = QStringLiteral("Living room iPhone");
        phoneHolder.shortName = QStringLiteral("iPhone");
        phoneHolder.kind = QStringLiteral("phone");
        TransmitHolder* holder = h.server().transmitHolder();

        // The take question open when the link drops.
        holder->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        QTRY_VERIFY(flagFor(h, 1) != nullptr);
        auto* badge = flagFor(h, 1)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QTRY_VERIFY(badge->isEnabled());
        badge->click();
        QTRY_VERIFY(h.window()->findChild<TakeTransmitDialog*>() != nullptr);
        const QPointer<TakeTransmitDialog> openAsk = h.window()->findChild<TakeTransmitDialog*>();
        h.dropLink();
        QTRY_VERIFY_WITH_TIMEOUT(h.acceptedConnections() == 2
                                 && client->isHandshakeComplete(), 10000);
        // Fix wave GUI-I1: the question went with the link.
        QTRY_VERIFY(!openAsk || !openAsk->isVisible());
        // The bench Core numbers each token session it accepts.
        client->setTokenSessionHolderForTest(QStringLiteral("token:2"));
        // The client names itself per session from the handshake and each
        // capabilities message; one more applies the new token id.
        h.pushCapabilities(granted);
        QTRY_VERIFY_WITH_TIMEOUT(client->transmitTakeAvailable(), 10000);
        holder->release(phone, QStringLiteral("test"));
        QTRY_VERIFY(!client->transmitHeldElsewhere());
        QVERIFY(client->requestTakeTransmit(false, 0, false) != 0);
        QTRY_VERIFY(client->holdsTransmitHere());
        QTest::qWait(kSettleMs);
        QVERIFY(h.txSliceCommands().isEmpty());

        // A slice take sent as the link drops.
        holder->release(QByteArrayLiteral("token:2"), QStringLiteral("test"));
        QTRY_VERIFY(!client->holdsTransmitHere());
        // The redialled window is a new token session to the bench Core,
        // on the slices the Core gave it; the phone takes one of them and
        // this window listens.
        SliceOwnership* ownership = h.station().sliceOwnership();
        QTRY_VERIFY(!ownership->joinedBy(QByteArrayLiteral("token:2")).isEmpty());
        const int sid = ownership->joinedBy(QByteArrayLiteral("token:2")).constFirst();
        ownership->setOwner(sid, phone);
        QTRY_VERIFY(flagFor(h, sid) && flagFor(h, sid)->isListening());
        badge = flagFor(h, sid)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QTRY_VERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(),
                 QStringLiteral("Take control of this slice and make it the TX slice"));
        badge->click();
        h.dropLink();
        QTRY_VERIFY_WITH_TIMEOUT(h.acceptedConnections() == 3
                                 && client->isHandshakeComplete(), 10000);
        client->setTokenSessionHolderForTest(QStringLiteral("token:3"));
        h.pushCapabilities(granted);
        QTRY_VERIFY_WITH_TIMEOUT(client->remoteSliceAccessAvailable(), 10000);
        // Again on a slice the Core gave the redialled window, which the
        // phone takes: the operator's own Take control from the menu.
        QTRY_VERIFY(!ownership->joinedBy(QByteArrayLiteral("token:3")).isEmpty());
        const int later = ownership->joinedBy(QByteArrayLiteral("token:3")).constFirst();
        ownership->setOwner(later, phone);
        QTRY_VERIFY(flagFor(h, later) && flagFor(h, later)->isListening());
        auto* chooser = h.window()->findChild<SliceChooser*>();
        QVERIFY(chooser);
        QSignalSpy finished(client, &StationClient::deviceCommandFinished);
        emit chooser->takeControlRequested(later);
        QTRY_COMPARE(ownership->mark(later).owner, QByteArrayLiteral("token:3"));
        QTest::qWait(kSettleMs);
        for (const QList<QVariant>& answer : finished) {
            QVERIFY(answer.at(0).toByteArray() != QByteArrayLiteral("tx.take"));
        }
        QVERIFY(!client->holdsTransmitHere());
        QVERIFY(h.txSliceCommands().isEmpty());
        QVERIFY(!h.station().mox());
    }

    // Fix wave GUI-I1: a badge take abandoned while its question is open
    // (here a refusal arrives for it) closes the question, so its Take can
    // no longer send tx.take. Nothing keys.
    void remoteTxBadgeAskClosesWhenTheTakeIsAbandoned()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        StationClient* client = h.client();
        QVERIFY(client);
        QVERIFY(connectSharing(h));
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        TransmitHolder::Holder phoneHolder;
        phoneHolder.deviceId = phone;
        phoneHolder.name = QStringLiteral("Living room iPhone");
        phoneHolder.shortName = QStringLiteral("iPhone");
        phoneHolder.kind = QStringLiteral("phone");
        TransmitHolder* holder = h.server().transmitHolder();
        holder->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        QTRY_VERIFY(flagFor(h, 1) != nullptr);
        auto* badge = flagFor(h, 1)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QTRY_VERIFY(badge->isEnabled());
        badge->click();
        QTRY_VERIFY(h.window()->findChild<TakeTransmitDialog*>() != nullptr);
        const QPointer<TakeTransmitDialog> ask = h.window()->findChild<TakeTransmitDialog*>();
        auto* controller = h.window()->findChild<MultiDeviceController*>();
        QVERIFY(controller);

        emit controller->refusal(QStringLiteral("Transmit is changing hands. Try again in a moment."));
        QTRY_VERIFY(!ask || !ask->isVisible());
        QVERIFY(controller->openDialog() == nullptr);
        QVERIFY(holder->isHeldBy(phone));
        QVERIFY(!h.station().mox());
    }

    // Fix wave GUI-I5: a container's MON and PS-A follow the transmit holder
    // as the TX applet's do: while the phone holds transmit, MON is shown
    // disabled with the Core's holder reason, and PS-A says the same
    // reason as the TX applet's PS-A. Nothing keys.
    void remoteContainerMonAndPsaFollowTheTransmitHolder()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        StationClient* client = h.client();
        QVERIFY(client);
        QVERIFY(connectSharing(h));
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        auto* manager = h.window()->findChild<ContainerManager*>();
        QVERIFY(manager);
        ContainerWidget* container = manager->createContainer(1, DockMode::Floating);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* buttons = new OtherButtonItem();
        meter->addItem(buttons);
        container->wireInteractiveItem(buttons);
        const auto destroy = qScopeGuard([manager, container] {
            manager->destroyContainer(container->id());
        });
        using Id = OtherButtonItem::ButtonId;
        QTRY_VERIFY(buttons->isButtonAvailable(Id::Mon));

        TransmitHolder::Holder phoneHolder;
        phoneHolder.deviceId = phone;
        phoneHolder.name = QStringLiteral("Living room iPhone");
        phoneHolder.shortName = QStringLiteral("iPhone");
        phoneHolder.kind = QStringLiteral("phone");
        h.server().transmitHolder()->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        const QString holderReason = client->otherHolderReason();
        QVERIFY(!holderReason.isEmpty());
        QTRY_VERIFY(!buttons->isButtonAvailable(Id::Mon));
        QCOMPARE(buttons->buttonUnavailableReason(buttons->indexOf(Id::Mon)), holderReason);
        QVERIFY(!buttons->isButtonAvailable(Id::PsA));
        auto* applet = h.window()->findChild<TxApplet*>();
        QVERIFY(applet);
        QPushButton* psa = nullptr;
        for (QPushButton* b : applet->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("PS-A")) { psa = b; }
        }
        QVERIFY(psa);
        QVERIFY(!psa->isEnabled());
        QCOMPARE(buttons->buttonUnavailableReason(buttons->indexOf(Id::PsA)), psa->toolTip());
        QVERIFY(!h.station().mox());
    }

    // TX badge take, case 3 in a remote window: the badge of a slice the
    // phone controls (this window listens) sends slice.takeControl and,
    // with transmit already here, tx.setTxSlice. With the phone holding
    // transmit it offers both takes; the transmit take after the slice
    // take is the Core's to answer (this bench window is a token session,
    // so it refuses in its own words) and the slice stays taken. Nothing
    // keys.
    void remoteTxBadgeOnAListenedSliceTakesTheSliceThenTransmit()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        StationClient* client = h.client();
        QVERIFY(client);
        QVERIFY(connectSharing(h));
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        SliceOwnership* ownership = h.station().sliceOwnership();
        const QByteArray self = QByteArrayLiteral("token:1");
        TransmitHolder* holder = h.server().transmitHolder();
        TransmitHolder::Holder selfHolder;
        selfHolder.deviceId = self;
        selfHolder.name = QStringLiteral("Bench window");
        holder->transferTo(selfHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->holdsTransmitHere());
        ownership->setOwner(1, phone);
        QTRY_VERIFY(flagFor(h, 1) && flagFor(h, 1)->isListening());
        auto* badge = flagFor(h, 1)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        QTRY_VERIFY(badge->isEnabled());
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take control of this slice and make it the TX slice"));
        badge->click();
        QTRY_COMPARE(ownership->mark(1).owner, self);
        QTRY_COMPARE(h.txSliceCommands(), QList<int>({1}));
        QTRY_VERIFY(!flagFor(h, 1)->isListening());
        QVERIFY(h.window()->findChild<TakeTransmitDialog*>() == nullptr);

        // The phone controls the slice and holds transmit.
        ownership->setOwner(1, phone);
        TransmitHolder::Holder phoneHolder;
        phoneHolder.deviceId = phone;
        phoneHolder.name = QStringLiteral("Living room iPhone");
        phoneHolder.shortName = QStringLiteral("iPhone");
        phoneHolder.kind = QStringLiteral("phone");
        holder->transferTo(phoneHolder, QStringLiteral("test"));
        QTRY_VERIFY(client->transmitHeldElsewhere());
        QTRY_VERIFY(flagFor(h, 1)->isListening());
        badge = flagFor(h, 1)->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QTRY_VERIFY(badge->isEnabled());
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take control of this slice, then take transmit from iPhone"));
        badge->click();
        QTRY_COMPARE(ownership->mark(1).owner, self);
        QTRY_COMPARE(toastsSaying(h, TxRefusals::deviceNotPaired().text), 1);
        QVERIFY(!client->holdsTransmitHere());
        QCOMPARE(h.txSliceCommands(), QList<int>({1}));
        QTRY_VERIFY(!flagFor(h, 1)->isListening());
        QVERIFY(!h.station().mox());
    }
    // Slice control plan Task 17 (carried from Task 15): a remote window's
    // container slice buttons follow the Core's change of control through
    // the window's slice-access mirror, with no other refresh. A press on
    // a slice another device controls changes nothing on either side.
    void remoteContainerSliceButtonsFollowTheCoresChangeOfControl()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        auto* manager = h.window()->findChild<ContainerManager*>();
        QVERIFY(manager);
        ContainerWidget* container = manager->createContainer(1 + 1, DockMode::Floating);  // B
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* buttons = new OtherButtonItem();
        meter->addItem(buttons);
        container->wireInteractiveItem(buttons);
        const auto destroy = qScopeGuard([manager, container] {
            manager->destroyContainer(container->id());
        });
        using Id = OtherButtonItem::ButtonId;
        QTRY_VERIFY(buttons->isButtonAvailable(Id::Mute));

        // The buttons read at the moment the window's mirror hears of the
        // change (this connection runs after the window's own): a timer
        // also refreshes containers, and must not be what passes this.
        QList<bool> availableOnChange;
        QObject watch;
        connect(h.client()->sliceAccess(), &SliceAccessMirror::changed, &watch,
                [&availableOnChange, buttons](int sliceId) {
                    if (sliceId == 1) { availableOnChange << buttons->isButtonAvailable(Id::Mute); }
                });
        SliceOwnership* ownership = h.station().sliceOwnership();
        ownership->setOwner(1, phone);
        QTRY_VERIFY(h.client()->sliceAccess()->entry(1)->controllerDeviceId
                    != QStringLiteral("token:1"));
        QVERIFY(!availableOnChange.isEmpty());
        QVERIFY(!availableOnChange.last());
        QVERIFY(!buttons->isButtonAvailable(Id::Mute));
        QCOMPARE(buttons->buttonUnavailableReason(buttons->indexOf(Id::Mute)),
                 QStringLiteral("Living room iPhone controls this slice"));
        SliceModel* windowB = h.remoteModel()->sliceById(1);
        SliceModel* coreB = h.station().sliceById(1);
        QVERIFY(windowB && coreB);
        const bool mutedBefore = coreB->muted();
        QSignalSpy windowMuted(windowB, &SliceModel::mutedChanged);
        emit container->otherButtonClicked(int(Id::Mute));
        QTest::qWait(kSettleMs);
        QCOMPARE(windowB->muted(), mutedBefore);
        QCOMPARE(coreB->muted(), mutedBefore);
        QCOMPARE(windowMuted.count(), 0);

        availableOnChange.clear();
        ownership->setOwner(1, QByteArrayLiteral("token:1"));
        QTRY_COMPARE(h.client()->sliceAccess()->entry(1)->controllerDeviceId,
                     QStringLiteral("token:1"));
        QVERIFY(!availableOnChange.isEmpty());
        QVERIFY(availableOnChange.last());
        QVERIFY(buttons->isButtonAvailable(Id::Mute));
    }

    // Desktop listening lane (JJ, 2026-09-30; it replaces Task 17's
    // auto-stop): listening ends only when the operator ends it. A layout
    // change that retires the pan showing a listened slice places it on a
    // pan that remains and keeps listening, with no slice.stopListening
    // and no notice. The slice stays where its controller put it, and
    // nothing is added.
    void remoteLayoutChangeKeepsListeningToASliceOnARemainingPan()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        // B sits well away from A, so the pan that remains cannot show it.
        const double bHz = h.station().sliceById(0)->frequency() + 1'000'000.0;
        h.station().sliceById(1)->setFrequency(bHz);
        QTRY_COMPARE(h.remoteModel()->sliceById(1)->frequency(), bHz);
        SliceOwnership* ownership = h.station().sliceOwnership();
        ownership->setOwner(1, phone);
        QVERIFY(ownership->isListening(QByteArrayLiteral("token:1"), 1));
        QTRY_VERIFY(flagFor(h, 1) && flagFor(h, 1)->isListening());
        auto* stack = h.window()->findChild<PanadapterStack*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("2v"));
        PanadapterApplet* remaining = stack->panadapter(QStringLiteral("pan-0"));
        QVERIFY(remaining);
        QTest::qWait(kSettleMs);
        const double viewCentre = remaining->spectrumWidget()->centerFrequency();
        const double viewSpan = remaining->spectrumWidget()->bandwidth();

        QVERIFY(QMetaObject::invokeMethod(h.window(), "applyPanLayout",
                                          Q_ARG(QString, QStringLiteral("1"))));
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("1"));
        // Placed on the pan that remains, as its flag, still listening.
        QCOMPARE(stack->panadapter(QStringLiteral("pan-0")), remaining);
        QTRY_VERIFY(remaining->associatedSlices().contains(1));
        QTRY_VERIFY(flagFor(h, 1) && flagFor(h, 1)->parentWidget() == remaining->spectrumWidget());
        QVERIFY(flagFor(h, 1)->isListening());
        QTest::qWait(kSettleMs);
        // The operator's view of that pan stays where it was, so B, off
        // its span, is the pan's edge marker rather than a moved view.
        QCOMPARE(remaining->spectrumWidget()->centerFrequency(), viewCentre);
        QCOMPARE(remaining->spectrumWidget()->bandwidth(), viewSpan);
        QVERIFY(bHz > viewCentre + viewSpan / 2.0);
        QVERIFY(flagFor(h, 1)->isHidden());
        // A marker only there: its own edge marker, never the pan's VFO.
        QVERIFY(remaining->spectrumWidget()->isEdgeMarkedSlice(1));
        // Its controller tunes it: that pan's view still does not move.
        h.station().sliceById(1)->setFrequency(bHz + 400'000.0);
        QTRY_COMPARE(h.remoteModel()->sliceById(1)->frequency(), bHz + 400'000.0);
        QTest::qWait(kSettleMs);
        QCOMPARE(remaining->spectrumWidget()->centerFrequency(), viewCentre);
        QCOMPARE(remaining->spectrumWidget()->bandwidth(), viewSpan);
        QVERIFY(remaining->spectrumWidget()->isEdgeMarkedSlice(1));
        QVERIFY(ownership->isListening(QByteArrayLiteral("token:1"), 1));
        QVERIFY(h.sliceAccessCommands().isEmpty());
        QCOMPARE(ownership->mark(1).subject(), phone);
        QCOMPARE(h.station().sliceById(1)->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(h.remoteModel()->sliceById(1)->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(h.station().sliceById(0)->panKey(), QStringLiteral("pan-0"));
        QVERIFY(h.addSliceCommands().isEmpty());
        QCOMPARE(toastsSaying(h, QStringLiteral(
                     "Stopped listening to Slice B: it is no longer shown in this window.")), 0);
        // Its row still offers Stop listening.
        SliceChooser* chooser = openChooser(h);
        QVERIFY(chooser);
        chooser->selectSlice(1);
        bool stopOffered = false;
        for (QPushButton* action :
             chooser->findChildren<QPushButton*>(QStringLiteral("sliceChooserAction"))) {
            if (action->text() == QStringLiteral("Stop listening") && action->isEnabled()) {
                stopOffered = true;
            }
        }
        QVERIFY(stopOffered);
    }

    // Slice control plan Task 17 (carried from Task 16, ruling U1): a
    // listen the Core accepts shows the slice in this window. With one pan
    // and no empty one, the window grows to the next layout and places the
    // slice on the new pan; the slice's pan for its controller stays put.
    void remoteListenTheCoreAcceptsShowsTheSliceHere()
    {
        RemoteWindowHarness h(sharingOptions(1, QStringLiteral("1")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        const int bId = phonesSliceOnAnotherPan(h, phone);
        QVERIFY(bId > 0);
        auto* stack = h.window()->findChild<PanadapterStack*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("1"));

        SliceChooser* chooser = openChooser(h);
        QVERIFY(chooser);
        // The window is sent the slice only once it listens, and the slice
        // arrives before its pan key does.
        QVERIFY(!h.remoteModel()->sliceById(bId));
        emit chooser->listenRequested(bId);
        SliceOwnership* ownership = h.station().sliceOwnership();
        QTRY_VERIFY(ownership->isListening(QByteArrayLiteral("token:1"), bId));
        QTRY_COMPARE(stack->currentLayoutId(), QStringLiteral("2v"));
        PanadapterApplet* grown = stack->panadapter(QStringLiteral("pan-1"));
        QVERIFY(grown);
        QTRY_VERIFY(grown->associatedSlices().contains(bId));
        QTRY_VERIFY(flagFor(h, bId) && flagFor(h, bId)->parentWidget() == grown->spectrumWidget());
        QCOMPARE(h.remoteModel()->sliceById(bId)->panKey(), QStringLiteral("pan-3"));
        QCOMPARE(h.station().sliceById(bId)->panKey(), QStringLiteral("pan-3"));
        QCOMPARE(ownership->mark(bId).subject(), phone);
        QVERIFY(h.addSliceCommands().isEmpty());
    }

    // Slice control plan Task 17 (carried from Task 16): the window's
    // placement of a listened slice follows the Core's slice-access
    // updates. Given control, the placement becomes the slice's pan on the
    // Core; no longer listening, the slice leaves the pan it was placed on.
    void remotePlacementsFollowTheCoresAccessUpdates()
    {
        RemoteWindowHarness h(sharingOptions(1, QStringLiteral("1")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        SliceOwnership* ownership = h.station().sliceOwnership();
        const QByteArray self = QByteArrayLiteral("token:1");
        auto* stack = h.window()->findChild<PanadapterStack*>();
        QVERIFY(stack);

        // Listened and placed on the grown pan, then given to this window.
        const int bId = phonesSliceOnAnotherPan(h, phone);
        QVERIFY(bId > 0);
        SliceChooser* chooser = openChooser(h);
        QVERIFY(chooser);
        emit chooser->listenRequested(bId);
        QTRY_VERIFY(ownership->isListening(self, bId));
        QTRY_VERIFY(stack->panadapter(QStringLiteral("pan-1"))
                    && stack->panadapter(QStringLiteral("pan-1"))->associatedSlices().contains(bId));
        QTRY_VERIFY(!chooser->requestInFlight().size());
        ownership->setOwner(bId, self);
        QTRY_COMPARE(h.station().sliceById(bId)->panKey(), QStringLiteral("pan-1"));
        QTRY_COMPARE(h.remoteModel()->sliceById(bId)->panKey(), QStringLiteral("pan-1"));

        // Listened and placed, then no longer listened: it leaves the pan.
        const int cId = phonesSliceOnAnotherPan(h, phone);
        QVERIFY(cId > 0);
        chooser = openChooser(h);
        QVERIFY(chooser);
        emit chooser->listenRequested(cId);
        QTRY_VERIFY(ownership->isListening(self, cId));
        QTRY_COMPARE(stack->currentLayoutId(), QStringLiteral("3v"));
        PanadapterApplet* placed = stack->panadapter(QStringLiteral("pan-2"));
        QVERIFY(placed);
        QTRY_VERIFY(placed->associatedSlices().contains(cId));
        QVERIFY(ownership->leave(self, cId));
        QTRY_VERIFY(!placed->associatedSlices().contains(cId));
        QCOMPARE(h.station().sliceById(cId)->panKey(), QStringLiteral("pan-3"));
        QCOMPARE(ownership->mark(cId).subject(), phone);
    }

    // Slice control plan Task 17 (carried from Task 16): a take the Core
    // accepts whose answer arrives before the Core's slice-access update.
    // The window first places the slice as a listened one, then, when the
    // update says it controls the slice, makes that pan the slice's pan on
    // the Core.
    void remoteTakeAnsweredBeforeItsAccessUpdateEndsOnTheWindowsPan()
    {
        RemoteWindowHarness h(sharingOptions(1, QStringLiteral("1")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        const int bId = phonesSliceOnAnotherPan(h, phone);
        QVERIFY(bId > 0);
        SliceOwnership* ownership = h.station().sliceOwnership();
        const QByteArray self = QByteArrayLiteral("token:1");
        auto* stack = h.window()->findChild<PanadapterStack*>();
        QVERIFY(stack);
        StationClient* client = h.client();
        QTRY_VERIFY(client->sliceAccess()->entry(bId).has_value());

        // The phone drops off and is away: another device may take its
        // slice. The window holds the Core's latest control revision.
        h.server().deviceSessions()->sessionEnded(phone, &phoneSession,
                                                  DeviceSessionRegistry::EndKind::Dropped);
        QTRY_COMPARE(client->sliceAccess()->entry(bId)->controlRevision,
                     ownership->controlRevision(bId));
        QTest::qWait(kSettleMs);
        QCOMPARE(client->sliceAccess()->entry(bId)->controlRevision,
                 ownership->controlRevision(bId));
        h.coreLink()->holdSliceAccessUpdates();
        SliceChooser* chooser = openChooser(h);
        QVERIFY(chooser);
        QSignalSpy answered(client, &StationClient::deviceCommandFinished);
        emit chooser->takeControlRequested(bId);
        QTRY_VERIFY(!answered.isEmpty());
        QCOMPARE(answered.first().at(0).toByteArray(), QByteArrayLiteral("slice.takeControl"));
        QVERIFY(answered.first().at(2).toBool());
        QCOMPARE(ownership->mark(bId).owner, self);
        // The Core answers the take at once, but sends the slice's access
        // update on its next delta flush (StationServer::kDefaultDeltaFlushMs,
        // a rate limit), so on a busy computer the window can read the answer
        // before the update is made and held. Wait for it to be held.
        QTRY_VERIFY(h.coreLink()->heldSliceAccessCount() > 0);
        // Step one: the mirror still names the phone, so the window places
        // the slice on a pan of its own without moving it for anyone.
        QVERIFY(client->sliceAccess()->entry(bId)->controllerDeviceId != QStringLiteral("token:1"));
        QTRY_COMPARE(stack->currentLayoutId(), QStringLiteral("2v"));
        QTRY_VERIFY(stack->panadapter(QStringLiteral("pan-1"))->associatedSlices().contains(bId));
        QCOMPARE(h.station().sliceById(bId)->panKey(), QStringLiteral("pan-3"));

        // Step two: the update arrives and the placement becomes the pan.
        h.coreLink()->releaseSliceAccessUpdates();
        QTRY_COMPARE(client->sliceAccess()->entry(bId)->controllerDeviceId,
                     QStringLiteral("token:1"));
        QTRY_COMPARE(h.station().sliceById(bId)->panKey(), QStringLiteral("pan-1"));
        QTRY_COMPARE(h.remoteModel()->sliceById(bId)->panKey(), QStringLiteral("pan-1"));
        QVERIFY(flagFor(h, bId));
        QTRY_VERIFY(!flagFor(h, bId)->isListening());
        QVERIFY(h.addSliceCommands().isEmpty());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TestRemoteWindowHarness test;
    QTEST_SET_MAIN_SOURCE_PATH
    // Load findings 4: about 37 s on a quiet computer, past ctest's 120 s
    // under 150 busy loops (200 to 207 s at load 133 to 187), every case
    // still passing one after another. The time is the window's own work,
    // spread over all the cases (about 3 to 9 s each at that load, the
    // Setup page sweep 18 s); its fixed waits are "nothing happens" windows
    // that do not grow with load. Five groups run as their own ctest
    // entries, tst_remote_window_harness_connect, _disconnect, _links,
    // _setup and _radio (tests/CMakeLists.txt), each 27 to 41 s at that
    // load; the slice access cases run as tst_remote_window_harness. The
    // limit is not raised.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_REMOTE_WINDOW_HARNESS_GROUP",
        {{QStringLiteral("connect"),
          {QStringLiteral("entryPointsStartAnExplicitConnect"),
           QStringLiteral("connectedHeaderShowsCurrentSocketAndClearsOnDisconnect"),
           QStringLiteral("setupConnectionsAsksManagedPickerWithoutDialing")}},
         {QStringLiteral("disconnect"),
          {QStringLiteral("cancelDuringBackoffStopsTheRetry"),
           QStringLiteral("operatorDisconnectOpensConnectionsOnce")}},
         {QStringLiteral("links"),
          {QStringLiteral("linkLossOpensNothingAndRetries"),
           QStringLiteral("radioOfflineOpensNothing"),
           QStringLiteral("heldSnapshotCreatesNoSliceOnConnectOrReconnect")}},
         {QStringLiteral("setup"),
          {QStringLiteral("disconnectedWindowSetupKeepsThisComputersSettings"),
           QStringLiteral("connectedWithoutTheCoresSettingsCorePagesWait"),
           QStringLiteral("setupOpenedWhileDisconnectedRecordsNoEdit"),
           QStringLiteral("freshWindowFirstConnectRaisesNoOfflineEditWarning"),
           QStringLiteral("meterIntervalFollowsTheCoresSetting")}},
         {QStringLiteral("radio"),
          {QStringLiteral("windowFollowsTheCoresBandPlan"),
           QStringLiteral("attenuatorControlsUseTheCoresObject"),
           QStringLiteral("hardwareConfigReceiveSettingsReachTheCore"),
           QStringLiteral("olderCoreLeavesAttenuatorControlsDisabledWithAReason"),
           QStringLiteral("windowFollowsTheCoresRadio"),
           QStringLiteral("pureSignalAppletIsDisabledNotHiddenBelowPureSignal3"),
           QStringLiteral("capabilityChangeRegatesWithoutReconnect")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_remote_window_harness.moc"
