// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_conformance_session.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 3 (R-IOS-01): the station runs the link's session
// fixtures (tests/data/link/v1/sessions/*.json). Each describes a station
// (stationSetup) and a script of client and station messages, virtual time
// and closes; LinkFixtures::runSession plays the client's half against a
// real StationServer over the in-process loopback and checks the station's
// half. The app runs the same scripts the other way round.
//
// stationSetup, as this runner builds it:
//   radio            "static": a RadioModel reporting an HL2 connected
//                    (MAC AA:BB:CC:DD:EE:01), with no radio behind it and its
//                    slice meter pump stopped, so nothing changes on its own;
//                    its TX profile bank is that radio's, as a connect makes
//                    it (the factory profiles, "Default" active);
//                    "connectable": ConnectableRadioModel, a RadioModel
//                    connected to the P1 fake radio, WDSP channels and all
//   board            the static radio's model: "hermesLite2" (the HL2 on
//                    Protocol 1, "Bench HL2") or "ananG2" (an ANAN-G2 on
//                    Protocol 2, "Bench G2", same MAC) ("hermesLite2")
//   slices           slices the model holds before the client connects (1)
//   panadapters      panadapters it holds (0)
//   coreAccessories  the Core owns its accessories (the amplifier, RF-Kit
//                    and tuner objects and their verbs) (false)
//   stationTci       the Core runs a station TCI server; it stays off (false)
//   stepAttenuator   a step attenuator controller is bound (false)
//   media            the station's media is enabled (false)
//   mediaController  with media, the Core's media controllers run, as
//                    nereusd runs them (a DaemonMediaHub making one per
//                    session as its media starts), so a client's
//                    media.control reaches them and they answer (false:
//                    media.control reaches nothing). Each media
//                    connection's transport is a stand-in that offers one
//                    description ("v=0" and nothing else) and never
//                    connects, gathers a candidate or becomes ready, so
//                    what the Core sends does not depend on this
//                    computer's network
//   priorFailedAuthentications
//                    other clients that each sent a wrong token before this
//                    one connects (0); the lockout is the station's, not a
//                    peer's
//   otherConnections other clients connected and still connecting, which
//                    send nothing (0); with 24 the station is at its limit
//   lanScanWindowMs  how long the Core's Tuner Genius and Power Genius
//                    scans listen (the local dialog's 3000 ms); the scan
//                    fixtures shorten it, since the answer is matched as
//                    any text
//   token            "active": a Core upgraded from before paired devices,
//                    with its pairing token; "none": a new Core, without one
//                    ("active")
//   clientAnswersPings, pairedDevice, otherClients   read by the player
//                    itself (pairedDevice: the runner's own device, made at
//                    run time, is paired before the client connects;
//                    otherClients: iPhone app Task 71, other clients the
//                    player signs in as paired devices, LinkFixtures.h)
//   coreListener, coreInterfaces   read by the player too: the phone's
//                    direct addresses, the Core's listener and interfaces
//                    in place of this computer's (LinkFixtures.cpp)
//   remoteTransmit   the Core's remote_transmit: "allow" or "deny"
//                    ("deny", as every Core was before iPhone app plan
//                    Task 34)
//   transmitReady    iPhone app plan Task 35: the static radio's MOX walk
//                    runs with no delays and the radio's own microphone
//                    carries the audio, so a key can key it (false)
//   unkeyWalkMs      iPhone app plan Task 77: with transmitReady, the MOX
//                    walk back to receive takes this many real milliseconds
//                    (0 to 5000), so a transfer is seen running (absent: 0)
//   microphoneLine   fix wave C1: with transmitReady, every device's media
//                    carries a microphone line whose buffer is full at
//                    once, so a voice key keys (false: no device has the
//                    line, and a voice key is refused micNotReady)
//   receivers        iPhone app Task 74: the static radio's receivers,
//                    192 kHz each, sized before its slices are made, so
//                    slices bind to them (absent: none, as before)
//   maxSlices        with receivers, the slice cap (5)
//   alexRxAntennas   iPhone app Task 75: the static radio's receive antenna
//                    per band, 14 numbers 1 to 3 in Band order (160 m ..
//                    XVTR), and band tracking on (the per-band antenna
//                    switch, ruling 5.11a) as though a radio were connected
//                    (absent: no band tracking, as before)
//   otherPairedDevices
//                    devices besides the runner's own paired before the
//                    client connects (0); their keys are made at run time
//                    and their ids recorded for "$ref:device:<n>" (the
//                    runner's own is "$ref:device:self")
//
//   stationRadios    parity Task 21: the Core chooses its own radio (as
//                    nereusd does): the static radio is its radio and "Bench
//                    G2" (AA:BB:CC:DD:EE:02, Protocol 2) is in sight; a
//                    choice or a scan reaches nothing (false)
//
// NEREUS_LINK_TRACE_DIR, when set, receives every message the station sent
// in each fixture (<id>.jsonl), for writing a fixture to what the code does.
//
// NEREUS_LINK_CONNECTABLE picks the fixtures whose station is
// "connectable" (a model connected to the fake radio, WDSP and all):
// "only" runs just those fixtures and nothing else, "skip" runs everything
// but them, and unset runs all. ctest registers the binary twice, once each
// way (tst_link_conformance_session and
// tst_link_conformance_session_connectable), so neither entry carries the
// other's time against its limit.
//
// NEREUS_LINK_REALTIME picks the fixtures that run a real-time MOX walk
// (stationSetup.unkeyWalkMs above 0) the same way: "only" runs just those,
// "skip" runs everything but them. Their ctest entry,
// tst_link_conformance_session_realtime, carries the realtime label, so a
// busy machine's `-LE realtime` run leaves them out (Task 77 fix wave, M4);
// the other two entries skip them.
//
//   cmake --build build --target tst_link_conformance_session
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build \
//       -R '^tst_link_conformance_session(_connectable)?$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): session
//                                    conformance runner. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): runs once per link major in
//                                    the manifest, against a station that
//                                    offers that major.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4b (R-IOS-01): the
//                                    connectable station waits for
//                                    PureSignal's readiness to settle.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    PureSignal's readiness follows the
//                                    receive-only station at once; only
//                                    the slices' readings are waited for.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    fixtures say which ends run them and
//                                    each client step's role; placeholders
//                                    in client messages.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Lane B takes integration (R-IOS-01,
//                                    R-R3-47): a verb's optional
//                                    arguments may be left out of either
//                                    leg's check (setPgxlHardware).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49: the DFNR model is hidden
//                                    too, so dfnrRunnable is false on
//                                    every machine. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (tx-followup-3): the Core
//                                    plays one that cannot run MNR, so
//                                    mnrRunnable is false on every
//                                    machine. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08):
//                                    stationSetup "token" and
//                                    "pairedDevice" for the device sign-in
//                                    fixtures. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A (R-IOS-01,
//                                    R-IOS-08): the app-fixture guard
//                                    admits the Core hello's own
//                                    challenge capture and a device
//                                    sign-in as "$device:signed".
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08):
//                                    stationSetup "otherPairedDevices".
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06):
//                                    stationSetup "board", so a catalogue
//                                    fixture can stand up an ANAN-G2 as
//                                    well as the HL2; the verbs-ps3 steps
//                                    the guard test plants into move by
//                                    two (the catalogue's schema and
//                                    object). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 34 (R-IOS-02):
//                                    stationSetup "remoteTransmit".
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 35 (R-IOS-13):
//                                    stationSetup "transmitReady".
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the static
//                                    station's TX profile bank is its
//                                    radio's, as a connect makes it.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 71 (R-IOS-02):
//                                    stationSetup "otherClients" in place
//                                    of "preemptingClient"; an app's
//                                    fixture check skips other clients'
//                                    steps. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 74 (R-IOS-30):
//                                    stationSetup "receivers" and
//                                    "maxSlices". AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 75 (R-IOS-30):
//                                    stationSetup "alexRxAntennas".
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app (R-IOS-01, R-IOS-02): the
//                                    {"$json": ...} cases; a failure
//                                    names its fixture first.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7): a receive-only
//                                    Core permits PureSignal, so the
//                                    connectable station waits for its
//                                    readiness to come on.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (group B fix wave, I2): the
//                                    planted PureSignal steps follow
//                                    verbs-ps3 without its arming verbs.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (group B fix wave, I3):
//                                    lanScanWindowMs, and
//                                    NEREUS_LINK_CONNECTABLE for the
//                                    connectable fixture's own ctest entry.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: stationSetup microphoneLine.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               stationSetup unkeyWalkMs; a verb with only optional arguments
//               may be sent with none. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Task 77 fix wave, M4: NEREUS_LINK_REALTIME for the real-time
//               fixtures' own ctest entry (labelled realtime). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 21 (R-IOS-18): stationSetup
//                                    stationRadios. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Tasks 27-29 fix wave (R-R3-49): the
//                                    connectable station's readings wait
//                                    for the meter pump's own poll, not
//                                    the slice's construction default; run
//                                    after another fixture the wait passed
//                                    before the first poll. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the data-channel mode
//               (sessionFixturesOverADataChannel): every session fixture
//               again, each client joined to the station over a control
//               data channel (DataChannelTransport, DTLS and SCTP on this
//               computer) instead of an in-process pipe. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: merge of Task 28 with the trunk (R-IOS-16): the real-time
//               entry (NEREUS_LINK_REALTIME=only) runs its fixtures over
//               both transports too, and the data-channel connectable
//               entry skips them. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 28 tail (R-IOS-16): the virtual
//               clock's own tests (a long timer's due time as real time
//               passes; the station's timers fire on the clock alone).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: Load findings 2: a fixture's data channel that fails
//               ends its open wait at once with the failure's reason, and
//               one that stalls names the stage it stalled at; the 15 s
//               bound is unchanged. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: stationSetup.deferOwnConnection: the own client connects
//               at the fixture's openOwnConnection step (addendum G-53,
//               the place-freed fixture). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: The phone's direct addresses: coreListener and
//               coreInterfaces are known stationSetup keys. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: The data-channel mode's connect hook reports a channel for
//               another client that did not open, with its stage. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: The phone's monitor-audio fixtures:
//               stationSetup.mediaController (a DaemonMediaHub on a
//               stand-in transport that offers one description), and a
//               behaviour media.control's connectionId held to the app's
//               own. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include <functional>
#include <memory>
#include <vector>

#include "core/ModelPaths.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/MoxController.h"
#include "core/StepAttenuatorController.h"
#include "core/accessories/AlexController.h"
#include "core/dsp/DspAssetService.h"
#include "core/meters/SliceMeterPump.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/LinkVersion.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/RemoteKeying.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IMediaTransport.h"
#include "core/station/StationRadios.h"
#include "core/settings/SettingsScope.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "LinkFixtures.h"
#include "LinkVirtualClock.h"
#include "OperatorWording.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/DataChannelPair.h"
#include "fakes/DataChannelStartupEvidence.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LinkFixtures;
using NereusSDR::Test::LinkVirtualClock;
using NereusSDR::Test::LoopbackTransport;

namespace {

// How long a fixture's data channel has to open (the DTLS and SCTP
// handshakes on this computer, in real time).
constexpr int kOpenBoundMs = 15000;

const QStringList kSetupKeys{
    QStringLiteral("radio"),           QStringLiteral("slices"),
    QStringLiteral("panadapters"),     QStringLiteral("coreAccessories"),
    QStringLiteral("stationTci"),      QStringLiteral("stepAttenuator"),
    QStringLiteral("media"),           QStringLiteral("priorFailedAuthentications"),
    QStringLiteral("clientAnswersPings"), QStringLiteral("otherClients"),
    QStringLiteral("otherConnections"), QStringLiteral("token"),
    QStringLiteral("pairedDevice"),    QStringLiteral("otherPairedDevices"),
    QStringLiteral("board"),           QStringLiteral("receivers"),
    QStringLiteral("maxSlices"),       QStringLiteral("alexRxAntennas"),
    QStringLiteral("lanScanWindowMs"), QStringLiteral("remoteTransmit"),
    QStringLiteral("transmitReady"),   QStringLiteral("microphoneLine"),
    QStringLiteral("unkeyWalkMs"),
    QStringLiteral("stationRadios"),   QStringLiteral("deferOwnConnection"),
    QStringLiteral("coreListener"),    QStringLiteral("coreInterfaces"),
    QStringLiteral("mediaController"),
};

// NEREUS_LINK_CONNECTABLE (see the file comment).
enum class ConnectableSelection { All, Only, Skip };

ConnectableSelection connectableSelection()
{
    const QByteArray value = qgetenv("NEREUS_LINK_CONNECTABLE");
    if (value == "only") {
        return ConnectableSelection::Only;
    }
    if (value == "skip") {
        return ConnectableSelection::Skip;
    }
    return ConnectableSelection::All;
}

// NEREUS_LINK_REALTIME (see the file comment).
ConnectableSelection realtimeSelection()
{
    const QByteArray value = qgetenv("NEREUS_LINK_REALTIME");
    if (value == "only") {
        return ConnectableSelection::Only;
    }
    if (value == "skip") {
        return ConnectableSelection::Skip;
    }
    return ConnectableSelection::All;
}

// Task 28: which transports the fixture rows run over. NEREUS_LINK_MODE
// "loopback" or "datachannel" runs one; unset, both. The connectable
// fixtures run one mode per process (their own ctest entries): a second
// connectable station in one process can read its slice meter before the
// receiver has measured again, so its snapshot carries the no-reading value.
enum class ModeSelection { Both, Loopback, DataChannel };

ModeSelection modeSelection()
{
    const QByteArray value = qgetenv("NEREUS_LINK_MODE");
    if (value == "loopback") {
        return ModeSelection::Loopback;
    }
    if (value == "datachannel") {
        return ModeSelection::DataChannel;
    }
    return ModeSelection::Both;
}

// stationSetup.mediaController: the transport each media connection gets.
// As the Core's offerer it offers one description, after the start returns
// (as the real transport offers once it has made one), and nothing else: no
// candidate, never ready, so the Core's media control traffic in a fixture
// is the same on every computer. Its description is "v=0" alone; fixtures
// match it as any string.
class ConformanceMediaTransport final : public IMediaTransport {
public:
    explicit ConformanceMediaTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions& options) override
    {
        if (options.role == Role::Offerer) {
            QTimer::singleShot(0, this, [this] {
                emit localDescription(QStringLiteral("v=0\r\n"), QStringLiteral("offer"));
            });
        }
        return true;
    }
    void stop() override {}
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return false; }
    bool sendRtp(const QByteArray&) override { return false; }
    bool isReady() const override { return false; }
};

// The station a fixture's stationSetup describes. Members are declared in
// the order that makes destruction safe: the server goes first, then the
// model, then the controller the model points at.
struct Station {
    QTemporaryDir dir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<StepAttenuatorController> stepAtt;
    std::unique_ptr<ConnectableRadioModel> harness;
    std::unique_ptr<RadioModel> ownModel;
    RadioModel* model = nullptr;
    // otherConnections: the far ends of connections still connecting,
    // kept open until the fixture ends (the server goes first).
    std::vector<std::unique_ptr<LoopbackTransport>> others;
    // Parity Task 21: the Core's radios, as nereusd attaches them.
    std::unique_ptr<StationRadios> radios;
    std::unique_ptr<StationServer> server;
    // stationSetup.mediaController: the Core's media controllers. They
    // hold the server and the model, so they go before either.
    std::unique_ptr<DaemonMediaHub> mediaHub;

    ~Station()
    {
        mediaHub.reset();
        server.reset();
        radios.reset();
        if (model != nullptr && stepAtt) {
            model->setStepAttController(nullptr);
        }
    }
};

QString buildStation(const QJsonObject& setup, Station* station, quint16 major)
{
    for (auto it = setup.constBegin(); it != setup.constEnd(); ++it) {
        if (!kSetupKeys.contains(it.key())) {
            return QStringLiteral("stationSetup: unknown field \"%1\"").arg(it.key());
        }
    }
    if (!station->dir.isValid()) {
        return QStringLiteral("no scratch directory");
    }
    // RadioModel keeps state in AppSettings::instance() (notches, accessory
    // choices, DSP asset selections). Each fixture starts from an empty
    // profile, so one fixture's writes never reach the next.
    AppSettings::instance().clear();
    station->settings = std::make_unique<AppSettings>(
        station->dir.filePath(QStringLiteral("NereusSDR.settings")));

    const QString radio = setup.value(QStringLiteral("radio")).toString(QStringLiteral("static"));
    if (radio == QStringLiteral("connectable")) {
        // Load findings 4: the fake radio does not stream on its own; the
        // runner feeds the receiver's first block below (buildStation).
        station->harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, {}, HPSDRHW::HermesLite, /*stream=*/false);
        if (!station->harness) {
            return QStringLiteral("the connectable radio model did not connect");
        }
        station->model = &station->harness->model();
    } else if (radio == QStringLiteral("static")) {
        // iPhone app Task 19: "board" picks the static radio's model.
        const QString board =
            setup.value(QStringLiteral("board")).toString(QStringLiteral("hermesLite2"));
        if (board != QStringLiteral("hermesLite2") && board != QStringLiteral("ananG2")) {
            return QStringLiteral("stationSetup.board must be \"hermesLite2\" or \"ananG2\"");
        }
        station->ownModel = std::make_unique<RadioModel>();
        station->model = station->ownModel.get();
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
        if (board == QStringLiteral("ananG2")) {
            station->model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
            info.name = QStringLiteral("Bench G2");
            info.boardType = HPSDRHW::Saturn;
            info.protocol = ProtocolVersion::Protocol2;
        } else {
            station->model->setBoardForTest(HPSDRHW::HermesLite);
            info.name = QStringLiteral("Bench HL2");
            info.boardType = HPSDRHW::HermesLite;
        }
        station->model->setLastRadioInfoForTest(info);
        station->model->setConnectionStateForTest(ConnectionState::Connected);
        // R-R3-49 (parity Task 3): the radio's TX profile bank, as a connect
        // scopes and loads it.
        station->model->scopeTxProfiles(info.macAddress);
        // No WDSP channels, so the pump would write its no-reading value to
        // every slice on each poll, on real time. Stopped, nothing changes
        // on its own and every delta in a fixture is one the script caused.
        if (SliceMeterPump* pump = station->model->sliceMeterPump()) {
            pump->stop();
        }
    } else {
        return QStringLiteral("stationSetup.radio must be \"static\" or \"connectable\"");
    }
    // R-R3-49 (tx-followup-3): MNR runs only on a Mac Core. Every fixture
    // plays a Core that cannot run it, so mnrRunnable and mnrStatus are the
    // same on every machine the runner runs on.
    if (DspAssetService* assets = station->model->dspAssets()) {
        assets->setMnrAvailability(false, RadioModel::mnrCannotRunReason());
    }
    if (radio != QStringLiteral("static") && setup.contains(QStringLiteral("board"))) {
        return QStringLiteral("stationSetup.board applies only to the static radio");
    }

    RadioModel& model = *station->model;
    if (setup.value(QStringLiteral("coreAccessories")).toBool(false)) {
        model.enableStationAccessoryIdentity();
    }
    if (setup.value(QStringLiteral("stationTci")).toBool(false)) {
        model.enableStationTci(QStringLiteral("127.0.0.1"));
    }
    if (setup.contains(QStringLiteral("lanScanWindowMs"))) {
        const int windowMs = setup.value(QStringLiteral("lanScanWindowMs")).toInt(-1);
        if (windowMs < 1) {
            return QStringLiteral("stationSetup.lanScanWindowMs must be a whole number of "
                                  "milliseconds from 1");
        }
        model.setTgxlLanScanWindowMsForTest(windowMs);
        model.setPgxlLanScanWindowMsForTest(windowMs);
    }
    if (setup.value(QStringLiteral("stepAttenuator")).toBool(false)) {
        station->stepAtt = std::make_unique<StepAttenuatorController>();
        station->stepAtt->setTickTimerEnabled(false);
        model.setStepAttController(station->stepAtt.get());
    }
    // iPhone app Task 74: "receivers" sizes the static radio's receiver
    // pool (192 kHz each), so slices bind to receivers and the anchor, pan
    // move and take rules apply; "maxSlices" is its slice cap (5).
    if (setup.contains(QStringLiteral("receivers"))) {
        if (radio != QStringLiteral("static")) {
            return QStringLiteral("stationSetup.receivers applies only to the static radio");
        }
        const int receivers = setup.value(QStringLiteral("receivers")).toInt(0);
        const int maxSlices = setup.value(QStringLiteral("maxSlices")).toInt(5);
        if (receivers < 1 || receivers > 5 || maxSlices < 1 || maxSlices > 5) {
            return QStringLiteral("stationSetup.receivers and maxSlices must be 1 to 5");
        }
        model.configureStreamPool(receivers, maxSlices, 192000);
    } else if (setup.contains(QStringLiteral("maxSlices"))) {
        return QStringLiteral("stationSetup.maxSlices needs receivers");
    }
    // iPhone app Task 75: the receive antennas per band, and band tracking.
    if (setup.contains(QStringLiteral("alexRxAntennas"))) {
        if (radio != QStringLiteral("static")) {
            return QStringLiteral("stationSetup.alexRxAntennas applies only to the static radio");
        }
        const QStringList antennas =
            setup.value(QStringLiteral("alexRxAntennas")).toString().split(QLatin1Char(','));
        if (antennas.size() != 14) {
            return QStringLiteral("stationSetup.alexRxAntennas must list 14 antennas");
        }
        for (int band = 0; band < antennas.size(); ++band) {
            const int antenna = antennas.at(band).trimmed().toInt();
            if (antenna < 1 || antenna > 3) {
                return QStringLiteral("stationSetup.alexRxAntennas: each antenna is 1 to 3");
            }
            model.alexControllerMutable().setRxAnt(static_cast<Band>(band), antenna);
        }
        model.enableBandTrackingForTest();
    }
    const int slices = setup.value(QStringLiteral("slices")).toInt(1);
    while (model.slices().size() < slices) {
        model.addSlice(QStringLiteral("pan-0"));
    }
    const int pans = setup.value(QStringLiteral("panadapters")).toInt(0);
    for (int i = 0; i < pans; ++i) {
        model.addPanadapter();
    }
    if (radio == QStringLiteral("static")) {
        if (SliceMeterPump* pump = model.sliceMeterPump()) {
            pump->stop();
        }
    }

    // A station that offers exactly the link major this pass covers.
    // iPhone app Task 12: "token": "active" (the default) stands up a Core
    // upgraded from before paired devices, which still has its token;
    // "none" a new Core, which has none.
    const QString token = setup.value(QStringLiteral("token")).toString(QStringLiteral("active"));
    if (token != QStringLiteral("active") && token != QStringLiteral("none")) {
        return QStringLiteral("stationSetup.token must be \"active\" or \"none\"");
    }
    const QString securityDir = token == QStringLiteral("active")
                                    ? NereusSDR::Test::seedUpgradedCoreToken(station->dir.path())
                                    : NereusSDR::Test::seedCoreIdentity(station->dir.path());
    station->server = std::make_unique<StationServer>(station->model, *station->settings,
                                                      securityDir, nullptr,
                                                      QList<quint16>{major});
    if (setup.value(QStringLiteral("media")).toBool(false)) {
        station->server->setMediaEnabled(true);
    }
    // The phone's transmit monitor fixtures: the Core's media controllers,
    // made as nereusd makes them (StationHost: a DaemonMediaHub over the
    // server and the model), each connection on the stand-in transport.
    const QJsonValue mediaController = setup.value(QStringLiteral("mediaController"));
    if (!mediaController.isUndefined() && !mediaController.isBool()) {
        return QStringLiteral("stationSetup.mediaController must be true or false");
    }
    if (mediaController.toBool(false)) {
        if (!setup.value(QStringLiteral("media")).toBool(false)) {
            return QStringLiteral("stationSetup.mediaController needs media");
        }
        station->mediaHub = std::make_unique<DaemonMediaHub>(
            station->server.get(), station->model, nullptr,
            [](QObject* parent) -> IMediaTransport* {
                return new ConformanceMediaTransport(parent);
            });
    }
    // Parity Task 21 (R-IOS-18): "stationRadios": the Core chooses its own
    // radio, as nereusd does. The static radio is its radio, and a second
    // radio, "Bench G2" at AA:BB:CC:DD:EE:02, is in sight. Choosing it or
    // scanning reaches nothing (no DaemonApp is behind it).
    if (setup.value(QStringLiteral("stationRadios")).toBool(false)) {
        if (radio != QStringLiteral("static")) {
            return QStringLiteral("stationSetup.stationRadios applies only to the static radio");
        }
        station->radios = std::make_unique<StationRadios>(*station->settings);
        RadioInfo other;
        other.macAddress = QStringLiteral("AA:BB:CC:DD:EE:02");
        other.name = QStringLiteral("Bench G2");
        other.boardType = HPSDRHW::Saturn;
        other.protocol = ProtocolVersion::Protocol2;
        other.address = QHostAddress(QStringLiteral("192.168.1.22"));
        station->radios->setVisible({other});
        station->radios->setCurrent(station->model->currentRadioInfo());
        station->server->setStationRadios(station->radios.get());
    }
    // iPhone app plan Task 34: the station transmit gate's setting.
    const QString remoteTransmit =
        setup.value(QStringLiteral("remoteTransmit")).toString(QStringLiteral("deny"));
    if (remoteTransmit != QStringLiteral("allow") && remoteTransmit != QStringLiteral("deny")) {
        return QStringLiteral("stationSetup.remoteTransmit must be \"allow\" or \"deny\"");
    }
    station->server->setRemoteTransmitAllowed(remoteTransmit == QStringLiteral("allow"));
    // iPhone app plan Task 35: a key can key the static radio. Its MOX walk
    // runs with no delays (so a key's messages arrive in a fixed order),
    // and the radio's own microphone carries the audio (no PC microphone
    // to wait for). No radio is behind it; nothing reaches a transmitter.
    const QJsonValue transmitReady = setup.value(QStringLiteral("transmitReady"));
    if (!transmitReady.isUndefined() && !transmitReady.isBool()) {
        return QStringLiteral("stationSetup.transmitReady must be true or false");
    }
    if (transmitReady.toBool(false)) {
        if (setup.value(QStringLiteral("radio")).toString() != QStringLiteral("static")
            || station->model->moxController() == nullptr) {
            return QStringLiteral("stationSetup.transmitReady applies only to the static radio");
        }
        station->model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        station->model->transmitModel().setMicSourceLocked(false);
        station->model->transmitModel().setMicSource(MicSource::Radio);
        // iPhone app plan Task 39: the time-out's clock is the Core's device
        // clock, which the runner moves virtually, so `txState`'s time left
        // reads the same on every machine.
        StationServer* server = station->server.get();
        station->model->txTimeOutTimer()->setClock(
            [server]() { return server->deviceSessions()->now(); });
    }
    // iPhone app plan Task 77: with transmitReady, the MOX walk from
    // transmit back to receive takes this long in real time, so a transfer
    // that unkeys a holder is still running when the next message arrives.
    const QJsonValue unkeyWalk = setup.value(QStringLiteral("unkeyWalkMs"));
    if (!unkeyWalk.isUndefined()) {
        const double ms = unkeyWalk.toDouble(-1.0);
        if (!unkeyWalk.isDouble() || ms < 0.0 || ms > 5000.0
            || ms != static_cast<double>(static_cast<int>(ms)) || !transmitReady.toBool(false)) {
            return QStringLiteral("stationSetup.unkeyWalkMs must be a whole number from 0 to "
                                  "5000, with transmitReady");
        }
        const int walk = static_cast<int>(ms);
        station->model->moxController()->setTimerIntervals(0, walk, 0, walk, walk, 0);
    }
    // Fix wave C1: a voice key needs the device's microphone line, which
    // travels on media, not on this session. microphoneLine stands for a
    // media connection that carries it, its buffer full at once.
    const QJsonValue microphoneLine = setup.value(QStringLiteral("microphoneLine"));
    if (!microphoneLine.isUndefined() && !microphoneLine.isBool()) {
        return QStringLiteral("stationSetup.microphoneLine must be true or false");
    }
    if (microphoneLine.toBool(false)) {
        if (!transmitReady.toBool(false) || station->server->remoteKeying() == nullptr) {
            return QStringLiteral("stationSetup.microphoneLine needs transmitReady");
        }
        RemoteKeying::MicUplink uplink;
        uplink.carriesMic = [](const QByteArray&) { return true; };
        uplink.prime = [](const QByteArray&, std::function<void(bool)> done) { done(true); };
        uplink.endPriming = [](const QByteArray&) {};
        station->server->remoteKeying()->setMicUplink(uplink);
    }
    if (station->harness) {
        // A StationServer makes its radio receive-only
        // (setReceiveOnlyStationPolicy). Since parity Task 7 (R-R3-49) a
        // receive-only Core permits PureSignal (a window arms it off the
        // air), so PureSignal's readiness (canActuate) is on once the radio
        // is connected; the pureSignal object follows on that call
        // (RadioModel::receiveOnlyStationPolicyChanged).
        PureSignalSessionFacade* const facade = station->model->pureSignalFacade();
        if (facade == nullptr
            || !QTest::qWaitFor([facade]() { return facade->canActuate(); }, 5000)) {
            return QStringLiteral("PureSignal's readiness did not settle on the receive-only "
                                  "station");
        }
        // Each slice's signal readings: the meter pump writes the
        // no-reading value until the receiver's meter has one. Only a
        // reading the pump wrote counts: before its first poll a slice
        // holds its construction default (-140 dBm), above the no-reading
        // value. The first station in a process is slow enough to set up
        // that a poll always landed first; a later one is not, and the
        // wait passed on the default, so the pump's first real poll (the
        // no-reading value while the channel's meter was not yet ready)
        // landed inside the script instead.
        SliceMeterPump* const pump = station->model->sliceMeterPump();
        if (pump == nullptr) {
            return QStringLiteral("the connectable station has no meter pump");
        }
        QSignalSpy polls(pump, &SliceMeterPump::polled);
        // Load findings 4 (lead's ruling, 2026-09-29): the snapshot carries
        // the slice's live meter readings, so the receiver must have
        // measured before the client attaches, and the readings must hold
        // still until the snapshot is sent. A receiver block is about 228
        // of the fake's ep6 frames, and each block moves the readings (the
        // tone's leak through the filter decays about 3.7 dB a block), so
        // the fake does not stream on its own here (the harness keeps the
        // link alive with a frame every 500 ms, far short of a block) and
        // the runner feeds the first block itself: 16 frames at a time,
        // each time waiting three of the pump's polls for the reading (the
        // pump reads the receive lane's cached meters, a poll behind),
        // until the ADC peak leaves the no-reading value.
        NereusSDR::Test::P1FakeRadio& fake = station->harness->fake();
        const auto firstBlockMeasured = [station]() {
            for (SliceModel* slice : station->model->slices()) {
                if (slice->adcPeakDbfs() <= SliceMeterPump::kNoReadingDbm) {
                    return false;
                }
            }
            return true;
        };
        QSignalSpy feedPolls(pump, &SliceMeterPump::polled);
        QElapsedTimer feeding;
        feeding.start();
        bool measured = firstBlockMeasured();
        while (!measured && feeding.elapsed() < 30000) {
            fake.sendEp6Frames(16);
            const qsizetype pollsBefore = feedPolls.size();
            measured = QTest::qWaitFor([&]() {
                return firstBlockMeasured() || feedPolls.size() >= pollsBefore + 3;
            }, 5000) && firstBlockMeasured();
        }
        if (!measured) {
            return QStringLiteral("the connectable station's receiver did not measure a block");
        }
        const bool readings = QTest::qWaitFor([station, &polls]() {
            if (polls.isEmpty()) {
                return false;
            }
            for (SliceModel* slice : station->model->slices()) {
                if (slice->signalStrengthDbm() <= SliceMeterPump::kNoReadingDbm
                    || slice->signalPeakDbm() <= SliceMeterPump::kNoReadingDbm
                    || slice->signalAverageDbm() <= SliceMeterPump::kNoReadingDbm) {
                    return false;
                }
            }
            return true;
        }, 5000);
        if (!readings) {
            return QStringLiteral("the slices' signal readings did not settle");
        }
    }

    // Other clients connected and still connecting (they send nothing), so
    // the fixture's client is the next one to arrive.
    const int others = setup.value(QStringLiteral("otherConnections")).toInt(0);
    for (int i = 0; i < others; ++i) {
        auto other = std::make_unique<LoopbackTransport>(QStringLiteral("conformance-waiting-client"));
        auto* otherEnd =
            new LoopbackTransport(QStringLiteral("conformance-waiting"), station->server.get());
        otherEnd->linkTo(other.get());
        station->server->acceptTransport(otherEnd);
        station->others.push_back(std::move(other));
    }
    QCoreApplication::processEvents();

    const int failures = setup.value(QStringLiteral("priorFailedAuthentications")).toInt(0);
    for (int i = 0; i < failures; ++i) {
        LoopbackTransport other(QStringLiteral("conformance-other-client"));
        auto* otherEnd =
            new LoopbackTransport(QStringLiteral("conformance-other"), station->server.get());
        otherEnd->linkTo(&other);
        station->server->acceptTransport(otherEnd);
        other.sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("conformance-other"))));
        other.sendText(SessionMessages::encode(
            SessionMessages::authRequest(QStringLiteral("not-the-station-token"))));
        // Event processing only: the refusal and the close are queued work.
        const QDeadlineTimer deadline(5000);
        while (other.isOpen() && !deadline.hasExpired()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (other.isOpen()) {
            return QStringLiteral("a prior client's wrong token was not refused");
        }
    }
    return QString();
}

// A placeholder text of the link document's section 16.3.
bool isPlaceholderText(const QJsonValue& value)
{
    return value.isString() && value.toString().startsWith(QLatin1Char('$'));
}

// Every string value at any depth under `value`, with its path.
void collectStrings(const QJsonValue& value, const QString& path,
                    QList<QPair<QString, QString>>* out)
{
    if (value.isString()) {
        out->append({path, value.toString()});
    } else if (value.isObject()) {
        const QJsonObject o = value.toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            collectStrings(it.value(), path + QLatin1Char('.') + it.key(), out);
        }
    } else if (value.isArray()) {
        const QJsonArray a = value.toArray();
        for (int i = 0; i < a.size(); ++i) {
            collectStrings(a.at(i), QStringLiteral("%1[%2]").arg(path).arg(i), out);
        }
    }
}

// Why a fixture marked for the app holds something a conformant client
// could not send, or an app runner could not play (section 16.3); empty
// when it holds none. `surface` is surface.json.
QStringList appConformanceProblems(const QString& id, const QJsonObject& fixture,
                                   const QJsonObject& surface)
{
    QStringList problems;
    const auto fail = [&problems, &id](int step, const QString& why) {
        problems.append(QStringLiteral("%1 step %2: %3").arg(id).arg(step).arg(why));
    };
    QHash<QString, QJsonObject> verbs;
    for (const QJsonValue& v : surface.value(QStringLiteral("commands")).toArray()) {
        verbs.insert(v.toObject().value(QStringLiteral("verb")).toString(), v.toObject());
    }
    const QJsonObject classes = surface.value(QStringLiteral("mirrorClasses")).toObject();
    const QSet<QString> clientKinds{
        QStringLiteral("hello"),          QStringLiteral("auth.request"),
        QStringLiteral("command.invoke"), QStringLiteral("media.control"),
        QStringLiteral("property.write"), QStringLiteral("settings.write"),
        QStringLiteral("settings.remove")};
    QHash<QString, QString> classOfKey;
    QHash<QString, double> capabilities;
    int agreedMinor = -1;
    // Whether a station hello's challenge was "$capture:challenge", which
    // an app's runner fills with a challenge of its own and records, so a
    // later "$device:signed" has a transcript to be checked against.
    bool challengeRecorded = false;
    const QJsonArray steps = fixture.value(QStringLiteral("steps")).toArray();
    for (int index = 0; index < steps.size(); ++index) {
        const QJsonObject step = steps.at(index).toObject();
        // iPhone app Task 71: an app's runner plays only its own client and
        // skips other clients' steps and the messages sent to them.
        if (step.contains(QStringLiteral("client")) || step.contains(QStringLiteral("to"))) {
            continue;
        }
        const QJsonObject message = step.value(QStringLiteral("message")).toObject();
        const QString type = message.value(QStringLiteral("type")).toString();
        const QString from = step.value(QStringLiteral("from")).toString();
        if (from == QStringLiteral("station")) {
            // What an app runner sends its client: nothing it cannot fill.
            QList<QPair<QString, QString>> strings;
            collectStrings(message, QStringLiteral("$"), &strings);
            for (const auto& [path, text] : strings) {
                const bool summarised =
                    text == QStringLiteral("$any")
                    && ((type == QStringLiteral("schema") && path == QStringLiteral("$.fields"))
                        || (type == QStringLiteral("object.create")
                            && path == QStringLiteral("$.properties")));
                // The one capture an app's runner makes: the challenge it
                // puts in the Core's hello (section 16.3).
                const bool ownChallenge = type == QStringLiteral("hello")
                    && path == QStringLiteral("$.challenge")
                    && text == QStringLiteral("$capture:challenge");
                if (ownChallenge) {
                    challengeRecorded = true;
                    continue;
                }
                if (text.startsWith(QStringLiteral("$capture:"))
                    || text.startsWith(QStringLiteral("$string:"))
                    || text.startsWith(QStringLiteral("$uuid:"))
                    || text.startsWith(QStringLiteral("$int:"))
                    || (text == QStringLiteral("$any") && !summarised)
                    || text == QStringLiteral("$object") || text == QStringLiteral("$majors")) {
                    fail(index, QStringLiteral("%1 is %2, which an app runner cannot send")
                                    .arg(path, text));
                }
            }
            if (type == QStringLiteral("object.create")) {
                classOfKey.insert(message.value(QStringLiteral("key")).toString(),
                                  message.value(QStringLiteral("class")).toString());
            }
            if ((type == QStringLiteral("schema") || type == QStringLiteral("object.create"))
                && !classes.contains(message.value(QStringLiteral("class")).toString())) {
                fail(index, QStringLiteral("class not in mirrorClasses, so no stand-in"));
            }
            if (type == QStringLiteral("capabilities")) {
                capabilities.clear();
                for (const QJsonValue& p : message.value(QStringLiteral("properties")).toArray()) {
                    capabilities.insert(p.toObject().value(QStringLiteral("name")).toString(),
                                        p.toObject().value(QStringLiteral("value")).toDouble());
                }
            }
            continue;
        }
        if (from != QStringLiteral("client")) {
            continue;
        }
        const QString role = step.value(QStringLiteral("role")).toString();
        if ((type == QStringLiteral("hello") || type == QStringLiteral("auth.request"))
            && role != QStringLiteral("behaviour")) {
            fail(index, QStringLiteral("the client makes its own %1; it cannot be scripted")
                            .arg(type));
        }
        if (role != QStringLiteral("behaviour")) {
            // A scripted message is filled alike by both runners only when
            // it names nothing: no $string:<name> or $int:<name> (so only
            // the station's runner ever advances the counter), and its own
            // ids are literals from 1000 up, clear of the client's.
            QList<QPair<QString, QString>> strings;
            collectStrings(message, QStringLiteral("$"), &strings);
            for (const auto& [path, text] : strings) {
                if (text.startsWith(QLatin1Char('$')) && text != QStringLiteral("$ref:token")) {
                    fail(index, QStringLiteral("a scripted message holds %1 at %2").arg(text, path));
                }
            }
            for (const QString& key : {QStringLiteral("id"), QStringLiteral("writeId")}) {
                if ((type == QStringLiteral("command.invoke") || type == QStringLiteral("property.write"))
                    && message.contains(key) && message.value(key).toDouble() < 1000.0) {
                    fail(index, QStringLiteral("a scripted %1 is a literal from 1000 up").arg(key));
                }
            }
            continue;
        }
        if (!clientKinds.contains(type)) {
            fail(index, QStringLiteral("a client never sends %1").arg(type));
            continue;
        }
        if (type == QStringLiteral("hello")) {
            // An app lists every major it supports (its own and the one
            // before, section 6.1), so a fixture cannot pin the list.
            if (message.value(QStringLiteral("majors")) != QJsonValue(QStringLiteral("$majors"))) {
                fail(index, QStringLiteral("hello must carry majors as \"$majors\""));
            }
            if (!message.contains(QStringLiteral("features"))) {
                fail(index, QStringLiteral("hello must carry features"));
            }
            if (message.value(QStringLiteral("peer")) != QJsonValue(QStringLiteral("$string"))
                || message.value(QStringLiteral("settingsSchema"))
                       != QJsonValue(QStringLiteral("$int"))) {
                fail(index, QStringLiteral("peer and settingsSchema are the app's own: "
                                           "$string and $int"));
            }
            agreedMinor = message.value(QStringLiteral("minor")).toInt();
        } else if (type == QStringLiteral("auth.request")) {
            const QString token = message.value(QStringLiteral("token")).toString();
            if (message.contains(QStringLiteral("device"))) {
                // A device sign-in (section 3.5): token "" and the app's own
                // device block, which an app's runner checks against this
                // connection's transcript, so the challenge must be its own.
                if (message.value(QStringLiteral("device"))
                        != QJsonValue(QStringLiteral("$device:signed"))
                    || !message.value(QStringLiteral("token")).isString() || !token.isEmpty()) {
                    fail(index, QStringLiteral("a device sign-in is token \"\" and device "
                                               "\"$device:signed\""));
                }
                if (!challengeRecorded) {
                    fail(index, QStringLiteral("$device:signed needs the Core's hello to carry "
                                               "challenge \"$capture:challenge\""));
                }
            } else if (token != QStringLiteral("$ref:token") && token != QStringLiteral("$string")) {
                fail(index, QStringLiteral("token must be $ref:token or $string"));
            }
        } else if (type == QStringLiteral("command.invoke")) {
            const QString verb = message.value(QStringLiteral("verb")).toString();
            // Section 9.1: an id is a whole number the client chooses, from
            // 0 to 4294967295, and from 1 for the nnr, ps3 and dspAssets
            // families. The placeholder holds the client to that range.
            const bool fromOne = verb.startsWith(QStringLiteral("nnr."))
                || verb.startsWith(QStringLiteral("ps3."))
                || verb.startsWith(QStringLiteral("dspAssets."));
            const QString id = message.value(QStringLiteral("id")).toString();
            const QString range = fromOne ? QStringLiteral(":1:4294967295")
                                          : QStringLiteral(":0:4294967295");
            if (!id.startsWith(QStringLiteral("$int:")) || !id.endsWith(range)
                || id.count(QLatin1Char(':')) != 3) {
                fail(index, QStringLiteral("id is the app's own: $int:<name>%1").arg(range));
            }
            QList<QPair<QString, QString>> argStrings;
            collectStrings(message.value(QStringLiteral("args")), QStringLiteral("$.args"),
                           &argStrings);
            for (const auto& [path, text] : argStrings) {
                if (text.startsWith(QLatin1Char('$'))) {
                    fail(index, QStringLiteral("%1 holds %2: arguments are what the app is told "
                                               "to send, never placeholders")
                                    .arg(path, text));
                }
            }
            if (!verbs.contains(verb)) {
                fail(index, QStringLiteral("%1 is not a verb").arg(verb));
                continue;
            }
            const QJsonObject spec = verbs.value(verb);
            const QJsonArray declared = spec.value(QStringLiteral("arguments")).toArray();
            const QJsonArray args = message.value(QStringLiteral("args")).toArray();
            // The declared arguments in order, each with its kind; an
            // optional one may be left out (setPgxlHardware takes one of
            // its three), a required one may not.
            bool fits = true;
            int next = 0;
            for (int d = 0; fits && d < declared.size(); ++d) {
                const QJsonObject want = declared.at(d).toObject();
                const bool present = next < args.size()
                    && args.at(next).toObject().value(QStringLiteral("name"))
                           == want.value(QStringLiteral("name"));
                if (present) {
                    fits = args.at(next).toObject().value(QStringLiteral("kind"))
                        == want.value(QStringLiteral("kind"));
                    ++next;
                } else {
                    fits = want.value(QStringLiteral("optional")).toBool();
                }
            }
            // iPhone app plan Task 77: a verb whose arguments are all
            // optional (tx.take) may be sent with none.
            bool allOptional = !declared.isEmpty();
            for (const QJsonValue& d : declared) {
                allOptional = allOptional && d.toObject().value(QStringLiteral("optional")).toBool();
            }
            fits = fits && next == args.size()
                && (args.isEmpty() == declared.isEmpty() || (args.isEmpty() && allOptional));
            if (!fits) {
                fail(index, QStringLiteral("%1's arguments are not the ones it takes").arg(verb));
            }
            const QString capability = spec.value(QStringLiteral("capability")).toString();
            const double version = spec.value(QStringLiteral("capabilityVersion")).toDouble();
            // The PureSignal action verbs need psAlgorithmVersion equal to
            // their version, not at least (section 6.2).
            const bool exact = capability == QStringLiteral("psAlgorithmVersion");
            const double advertised = capabilities.value(capability, 0.0);
            if (!capability.isEmpty() && (exact ? advertised != version : advertised < version)) {
                fail(index, QStringLiteral("%1 was not advertised (%2 %3 needed)")
                                .arg(verb, capability).arg(version));
            }
            if (agreedMinor < spec.value(QStringLiteral("minMinor")).toInt()) {
                fail(index, QStringLiteral("%1 needs a newer minor").arg(verb));
            }
        } else if (type == QStringLiteral("media.control")) {
            // A media connection id is the app's own: made by it for each
            // connection ("$uuid:<name>") or one it made earlier
            // ("$ref:<name>").
            const QString connection = message.value(QStringLiteral("payload"))
                                           .toObject()
                                           .value(QStringLiteral("connectionId"))
                                           .toString();
            if (!connection.startsWith(QStringLiteral("$uuid:"))
                && !connection.startsWith(QStringLiteral("$ref:"))) {
                fail(index, QStringLiteral("a media connectionId is the app's own: "
                                           "$uuid:<name> or $ref:<name>"));
            }
        } else if (type == QStringLiteral("property.write")) {
            const QString writeId = message.value(QStringLiteral("writeId")).toString();
            if (!writeId.startsWith(QStringLiteral("$int:"))
                || !writeId.endsWith(QStringLiteral(":1:4294967295"))
                || writeId.count(QLatin1Char(':')) != 3) {
                fail(index, QStringLiteral("writeId is the app's own: $int:<name>:1:4294967295"));
            }
            const QString cls = classOfKey.value(message.value(QStringLiteral("key")).toString());
            QHash<QString, QJsonObject> fields;
            for (const QJsonValue& f : classes.value(cls).toObject()
                                            .value(QStringLiteral("properties")).toArray()) {
                fields.insert(f.toObject().value(QStringLiteral("name")).toString(), f.toObject());
            }
            for (const QJsonValue& p : message.value(QStringLiteral("properties")).toArray()) {
                const QJsonObject entry = p.toObject();
                const QJsonObject field =
                    fields.value(entry.value(QStringLiteral("name")).toString());
                if (field.isEmpty()
                    || field.value(QStringLiteral("direction")) == QJsonValue(QStringLiteral("outbound"))
                    || field.value(QStringLiteral("kind")) != entry.value(QStringLiteral("kind"))
                    || field.value(QStringLiteral("ordinal")) != entry.value(QStringLiteral("ordinal"))) {
                    fail(index, QStringLiteral("%1 is not a property a client may write")
                                    .arg(entry.value(QStringLiteral("name")).toString()));
                }
            }
        } else if (type == QStringLiteral("settings.write")
                   || type == QStringLiteral("settings.remove")) {
            const QString key = message.value(QStringLiteral("key")).toString();
            if (classifySettingsKey(key) != SettingsScope::Station) {
                fail(index, QStringLiteral("%1 is not a Core setting").arg(key));
            }
            if (type == QStringLiteral("settings.write")
                && !message.value(QStringLiteral("origin")).toString().startsWith(
                    QStringLiteral("$string:"))) {
                fail(index, QStringLiteral("origin is the app's own: $string:<name>"));
            }
        }
    }
    return problems;
}

void writeTrace(const QString& id, const LoopbackTransport& client)
{
    const QByteArray directory = qgetenv("NEREUS_LINK_TRACE_DIR");
    if (directory.isEmpty()) {
        return;
    }
    QDir().mkpath(QString::fromLocal8Bit(directory));
    QFile file(QDir(QString::fromLocal8Bit(directory)).filePath(id + QStringLiteral(".jsonl")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    for (const QByteArray& wire : client.received()) {
        file.write(wire);
        file.write("\n");
    }
}

} // namespace

class TstLinkConformanceSession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void sessionFixtures_data();
    void sessionFixtures();
    void sessionFixturesOverADataChannel_data();
    void sessionFixturesOverADataChannel();
    void aRejectedDataChannelStartReportsItsStage();
    void aFailedDataChannelEndsTheOpenWaitWithItsReason();
    void everyVerbIsInvokedRightAndWrong();
    void rightAndWrongLegsGetDifferentAnswers();
    void everyFixtureRunsOnTheStation();
    void appFixturesHoldOnlyWhatAConformantClientSends();
    void theConformanceCheckCatchesWhatAnAppCannotSend();
    void mediaConnectionIdsAreTheAppsOwn();
    void refusalsOfOutboundWritesArePlain();
    void alteredFixturesFailReadably();
    void aDeferredOwnConnectionOpensAtItsStep();
    void jsonStringsMatchTheirShape();
    void currentCoreHelloOfferRemainsStrict();
    void theVirtualClockKeepsALongTimersDueAsRealTimePasses();
    void theVirtualClockAloneFiresTheStationsTimers();
    void theVirtualClockTimesAStartByVirtualTimeOnly();

private:
    QString run(const QString& id, const QJsonObject& fixture,
                quint16 major = kSessionProtocolMajor, bool overDataChannel = false);
    void runFixtureRow(bool overDataChannel);
    QJsonObject fixture(const QString& id);
    QJsonObject m_manifest;
};

void TstLinkConformanceSession::initTestCase()
{
    // RadioModel reads AppSettings::instance(); keep this process's copy
    // private, as tst_station_session does.
    const QString profile =
        QStringLiteral("link-conformance-session-%1").arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
    AppSettings::instance().clear();
    // Whether this machine's build carries the bundled NR3 model files
    // decides what the dspAssets object says about them. Fixtures are the
    // same on every machine, so none is found here.
    DspAssetService::setBundledNr3ModelPathsForTest([](const QString&) { return QString(); });
    // R-R3-49: likewise the DFNR model, so dfnrRunnable is false on every
    // machine. Its reason names what is missing (the model, or DFNR in this
    // build), so the fixtures match it as any string.
    ModelPaths::setDfnrModelTarballForTest(QString());

    QString error;
    m_manifest = LinkFixtures::readObject(
        QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("manifest.json")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

// With NEREUS_LINK_CONNECTABLE=only the binary runs the connectable
// fixtures and nothing else; the checks over every fixture run in the
// other entry.
void TstLinkConformanceSession::init()
{
    if (connectableSelection() == ConnectableSelection::Only
        && qstrcmp(QTest::currentTestFunction(), "sessionFixtures") != 0
        && qstrcmp(QTest::currentTestFunction(), "sessionFixturesOverADataChannel") != 0) {
        QSKIP("NEREUS_LINK_CONNECTABLE=only runs the connectable fixtures alone");
    }
    // Merge of Task 28 (R-IOS-16) with Task 77's real-time entry: the
    // real-time fixtures run over both transports, like every other one.
    if (realtimeSelection() == ConnectableSelection::Only
        && qstrcmp(QTest::currentTestFunction(), "sessionFixtures") != 0
        && qstrcmp(QTest::currentTestFunction(), "sessionFixturesOverADataChannel") != 0) {
        QSKIP("NEREUS_LINK_REALTIME=only runs the real-time fixtures alone");
    }
    // Load findings 3: the data-channel fixtures run as their own ctest
    // entry (NEREUS_LINK_MODE=datachannel), the in-process ones and every
    // other check in the first (NEREUS_LINK_MODE=loopback), so neither
    // carries the other's time.
    if (modeSelection() == ModeSelection::DataChannel
        && qstrcmp(QTest::currentTestFunction(), "sessionFixturesOverADataChannel") != 0) {
        QSKIP("NEREUS_LINK_MODE=datachannel runs the data-channel fixtures alone");
    }
}

void TstLinkConformanceSession::cleanupTestCase()
{
    DspAssetService::setBundledNr3ModelPathsForTest({});
    ModelPaths::clearDfnrModelTarballForTest();
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

QJsonObject TstLinkConformanceSession::fixture(const QString& id)
{
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("session"))) {
        if (entry.id == id) {
            QString error;
            const QJsonObject o = LinkFixtures::readObject(
                QDir(LinkFixtures::dataDirectory()).filePath(entry.file), &error);
            return error.isEmpty() ? o : QJsonObject{};
        }
    }
    return {};
}

QString TstLinkConformanceSession::run(const QString& id, const QJsonObject& fixture,
                                       quint16 major, bool overDataChannel)
{
    Station station;
    const QString problem =
        buildStation(fixture.value(QStringLiteral("stationSetup")).toObject(), &station, major);
    if (!problem.isEmpty()) {
        return problem;
    }
    // Declared after the station, so it goes first; the server owns the
    // station end once it accepts it.
    LoopbackTransport client(QStringLiteral("conformance-client"));
    // stationSetup.deferOwnConnection: the fixture's openOwnConnection step
    // connects the runner's own client instead (LinkFixtures::runSession).
    const bool deferOwn = fixture.value(QStringLiteral("stationSetup"))
                              .toObject()
                              .value(QStringLiteral("deferOwnConnection"))
                              .toBool(false);
    if (!overDataChannel) {
        if (!deferOwn) {
            auto* stationEnd =
                new LoopbackTransport(QStringLiteral("conformance"), station.server.get());
            stationEnd->linkTo(&client);
            station.server->acceptTransport(stationEnd);
        }
        const QString failure = LinkFixtures::runSession(fixture, *station.server, client);
        writeTrace(id, client);
        return failure.isEmpty() ? failure : QStringLiteral("%1: %2").arg(id, failure);
    }

    // Task 28, the data-channel mode: each client the player plays reaches
    // the station over a control data channel of its own, the station
    // presenting its own certificate. Declared after `client`, so the
    // bridges (and the channels) go before the clients they carry.
    StationServer* server = station.server.get();
    const QString certificate = server->certificatePemPath();
    const QString key = server->privateKeyPemPath();
    if (certificate.isEmpty() || key.isEmpty()) {
        return QStringLiteral("%1: the station has no certificate for a data channel").arg(id);
    }
    std::vector<std::unique_ptr<NereusSDR::Test::DataChannelBridge>> bridges;
    const auto join = [&bridges, server, certificate, key](LoopbackTransport* joined,
                                                           QString* diagnostic = nullptr) {
        QElapsedTimer elapsed;
        elapsed.start();
        NereusSDR::Test::DataChannelStartupEvidence evidence;
        auto bridge = std::make_unique<NereusSDR::Test::DataChannelBridge>(
            joined, StationServer::kMaxIncomingMessageBytes,
            StationClient::kMaxIncomingMessageBytes, certificate, key,
            [server](DataChannelTransport* end) { server->acceptTransport(end); },
            nullptr, evidence.observer());
        NereusSDR::Test::DataChannelBridge* opened = bridge.get();
        bridges.push_back(std::move(bridge));
        // Real time: the DTLS and SCTP handshakes on this computer. Load
        // findings 2: a failure at either end ends the wait at once and is
        // reported with its reason; a stall is reported with the stage it
        // stalled at.
        const bool didOpen = opened->started() && opened->waitForOpen(kOpenBoundMs);
        if (!didOpen && diagnostic) {
            const QString what = !opened->started()
                ? QStringLiteral("did not start")
                : opened->failed()
                    ? QStringLiteral("failed before it opened (%1)").arg(opened->failureReasons())
                    : QStringLiteral("did not open within %1 ms, stalled at %2")
                          .arg(kOpenBoundMs).arg(evidence.lastLibraryStage());
            *diagnostic = QStringLiteral("%1 (%2 elapsed=%3ms")
                              .arg(what, opened->openDiagnostic())
                              .arg(elapsed.elapsed())
                + QLatin1Char('\n') + evidence.diagnostic() + QLatin1Char(')');
        }
        return didOpen;
    };
    QString openDiagnostic;
    if (!deferOwn && !join(&client, &openDiagnostic)) {
        return QStringLiteral("%1: the data channel %2").arg(id, openDiagnostic);
    }
    LinkFixtures::setSettleCheck([&bridges] {
        for (const auto& bridge : bridges) {
            if (!bridge->settled()) {
                return false;
            }
        }
        return true;
    });
    LinkFixtures::setConnectClient([&join](LoopbackTransport* joined, StationServer&) {
        QString diagnostic;
        if (join(joined, &diagnostic)) {
            return QString();
        }
        return QStringLiteral("the data channel for that client %1").arg(diagnostic);
    });
    const QString failure = LinkFixtures::runSession(fixture, *station.server, client);
    LinkFixtures::setSettleCheck({});
    LinkFixtures::setConnectClient({});
    // A failure names its fixture first, then the step and the path.
    return failure.isEmpty() ? failure : QStringLiteral("%1: %2").arg(id, failure);
}

void TstLinkConformanceSession::sessionFixtures_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("major");
    const QList<LinkFixtures::Entry> entries =
        LinkFixtures::entries(m_manifest, QStringLiteral("session"));
    QVERIFY(!entries.isEmpty());
    const ConnectableSelection selection = connectableSelection();
    const ConnectableSelection realtime = realtimeSelection();
    // Once per link major the suite covers (manifest linkMajors).
    for (const quint16 major : LinkFixtures::linkMajors(m_manifest)) {
        for (const LinkFixtures::Entry& entry : entries) {
            const QJsonObject setup =
                fixture(entry.id).value(QStringLiteral("stationSetup")).toObject();
            const bool connectable =
                setup.value(QStringLiteral("radio")).toString() == QStringLiteral("connectable");
            if ((selection == ConnectableSelection::Only && !connectable)
                || (selection == ConnectableSelection::Skip && connectable)) {
                continue;
            }
            const bool walksInRealTime = setup.value(QStringLiteral("unkeyWalkMs")).toDouble(0.0) > 0.0;
            if ((realtime == ConnectableSelection::Only && !walksInRealTime)
                || (realtime == ConnectableSelection::Skip && walksInRealTime)) {
                continue;
            }
            QTest::newRow(qPrintable(QStringLiteral("%1 link %2").arg(entry.id).arg(major)))
                << entry.id << entry.file << int(major);
        }
    }
}

void TstLinkConformanceSession::sessionFixtures()
{
    if (modeSelection() == ModeSelection::DataChannel) {
        QSKIP("NEREUS_LINK_MODE=datachannel runs the data-channel mode alone");
    }
    runFixtureRow(false);
}

// Task 28 (R-IOS-16): the whole suite again over the control data channel.
void TstLinkConformanceSession::sessionFixturesOverADataChannel_data()
{
    sessionFixtures_data();
}

void TstLinkConformanceSession::sessionFixturesOverADataChannel()
{
    if (modeSelection() == ModeSelection::Loopback) {
        QSKIP("NEREUS_LINK_MODE=loopback runs the in-process mode alone");
    }
    runFixtureRow(true);
}

void TstLinkConformanceSession::aRejectedDataChannelStartReportsItsStage()
{
    LoopbackTransport client(QStringLiteral("diagnostic-client"));
    {
        NereusSDR::Test::DataChannelBridge bridge(
            &client, 0, StationClient::kMaxIncomingMessageBytes, {}, {},
            [](DataChannelTransport*) {});
        QVERIFY(!bridge.started());
        QCOMPARE(bridge.openDiagnostic(),
                 QStringLiteral("start(answerer=0,offerer=0) opened(answerer=0,offerer=0) "
                                "failed(answerer=0,offerer=0)"));
    }
    {
        NereusSDR::Test::DataChannelBridge bridge(
            &client, StationServer::kMaxIncomingMessageBytes, 0, {}, {},
            [](DataChannelTransport*) {});
        QVERIFY(!bridge.started());
        QCOMPARE(bridge.openDiagnostic(),
                 QStringLiteral("start(answerer=1,offerer=0) opened(answerer=0,offerer=0) "
                                "failed(answerer=0,offerer=0)"));
        bridge.answerer()->failed(QStringLiteral("synthetic failure"));
        bridge.offerer()->failed(QStringLiteral("synthetic failure"));
        QCOMPARE(bridge.openDiagnostic(),
                 QStringLiteral("start(answerer=1,offerer=0) opened(answerer=0,offerer=0) "
                                "failed(answerer=1,offerer=1)"));
    }
}

// Load findings 2: a channel that fails before it opens ends the open wait
// at once and names its reason; a library's own message is not repeated.
void TstLinkConformanceSession::aFailedDataChannelEndsTheOpenWaitWithItsReason()
{
    LoopbackTransport client(QStringLiteral("diagnostic-client"));
    NereusSDR::Test::DataChannelBridge bridge(
        &client, StationServer::kMaxIncomingMessageBytes, StationClient::kMaxIncomingMessageBytes,
        {}, {}, [](DataChannelTransport*) {});
    QVERIFY(bridge.started());
    emit bridge.offerer()->failed(QStringLiteral("the control connection could not be made"));
    emit bridge.answerer()->failed(QStringLiteral("a message from the library, 10.0.0.1"));
    QElapsedTimer waited;
    waited.start();
    QVERIFY(!bridge.waitForOpen(kOpenBoundMs));
    QVERIFY2(waited.elapsed() < 1000, qPrintable(QString::number(waited.elapsed())));
    QVERIFY(bridge.failed());
    QCOMPARE(bridge.failureReasons(),
             QStringLiteral("answerer: library error; "
                            "offerer: the control connection could not be made"));
}

void TstLinkConformanceSession::runFixtureRow(bool overDataChannel)
{
    QFETCH(QString, id);
    QFETCH(QString, file);
    QFETCH(int, major);
    QVERIFY2(LinkVersion::supportedMajors().contains(quint16(major)),
             qPrintable(QStringLiteral("this station does not offer link major %1").arg(major)));
    QString error;
    const QJsonObject o =
        LinkFixtures::readObject(QDir(LinkFixtures::dataDirectory()).filePath(file), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    if (o.value(QStringLiteral("stationSetup")).toObject()
            .value(QStringLiteral("otherConnections")).toInt(0) > 0) {
        // The station logs the connection it turns away; that is the case.
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^Refusing connection from")));
    }
    const QString failure = run(id, o, quint16(major), overDataChannel);
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void TstLinkConformanceSession::everyVerbIsInvokedRightAndWrong()
{
    // Each verb in verbSpecs() is invoked by some session fixture with its
    // own argument names, and again with an argument name it does not take.
    QSet<QString> right;
    QSet<QString> wrong;
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("session"))) {
        const QJsonObject o = fixture(entry.id);
        for (const QJsonValue& step : o.value(QStringLiteral("steps")).toArray()) {
            const QJsonObject s = step.toObject();
            const QJsonObject message = s.value(QStringLiteral("message")).toObject();
            if (s.value(QStringLiteral("from")).toString() != QStringLiteral("client")
                || message.value(QStringLiteral("type")).toString()
                       != QStringLiteral("command.invoke")) {
                continue;
            }
            const QString verb = message.value(QStringLiteral("verb")).toString();
            QStringList names;
            for (const QJsonValue& arg : message.value(QStringLiteral("args")).toArray()) {
                names.append(arg.toObject().value(QStringLiteral("name")).toString());
            }
            for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
                if (QString::fromUtf8(spec.verb) != verb) {
                    continue;
                }
                QStringList declared;
                QStringList required;
                for (const CommandArgumentSpec& argument : spec.arguments) {
                    declared.append(QString::fromUtf8(argument.name));
                    if (!argument.optional) {
                        required.append(QString::fromUtf8(argument.name));
                    }
                }
                bool unknownName = false;
                for (const QString& name : names) {
                    unknownName = unknownName || !declared.contains(name);
                }
                // Its own arguments: every required one, optional ones as
                // the verb takes them, in the declared order without
                // repeats (setPgxlHardware takes one of its three). With
                // no optional argument this is names == declared.
                bool own = !unknownName && !names.isEmpty() == !declared.isEmpty();
                qsizetype at = -1;
                for (const QString& name : names) {
                    const qsizetype found = declared.indexOf(name);
                    own = own && found > at;
                    at = found;
                }
                for (const QString& name : required) {
                    own = own && names.contains(name);
                }
                if (unknownName) {
                    wrong.insert(verb);
                } else if (own) {
                    right.insert(verb);
                }
            }
        }
    }
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        const QString verb = QString::fromUtf8(spec.verb);
        QVERIFY2(right.contains(verb),
                 qPrintable(verb + QStringLiteral(" is never invoked with its own arguments")));
        if (!spec.arguments.isEmpty()) {
            QVERIFY2(wrong.contains(verb),
                     qPrintable(verb + QStringLiteral(" is never invoked with a wrong name")));
        }
    }
}

void TstLinkConformanceSession::rightAndWrongLegsGetDifferentAnswers()
{
    // A verb invoked with its own arguments and again with one renamed
    // must get two different answers, or the fixture does not show the
    // station read the arguments at all (a refusal made before reading
    // them answers both alike). The answer compared is every
    // command.result for the invoke's id (PureSignal answers twice), each
    // as accepted and reason.
    QStringList same;
    int compared = 0;
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("session"))) {
        const QJsonArray steps = fixture(entry.id).value(QStringLiteral("steps")).toArray();
        QHash<QString, QString> right;
        QHash<QString, QString> wrong;
        QHash<QString, QJsonArray> rightArgs;
        QHash<QString, QJsonArray> wrongArgs;
        for (int i = 0; i < steps.size(); ++i) {
            const QJsonObject step = steps.at(i).toObject();
            const QJsonObject message = step.value(QStringLiteral("message")).toObject();
            if (step.value(QStringLiteral("from")).toString() != QStringLiteral("client")
                || message.value(QStringLiteral("type")).toString()
                       != QStringLiteral("command.invoke")
                || message.value(QStringLiteral("args")).toArray().isEmpty()) {
                continue;
            }
            const QJsonValue id = message.value(QStringLiteral("id"));
            // Ranged app-owned IDs capture only the name, not the range.
            const QJsonValue refersTo = id.isString()
                    && id.toString().startsWith(QStringLiteral("$int:"))
                ? QJsonValue(QStringLiteral("$ref:%1").arg(id.toString().section(QLatin1Char(':'), 1, 1)))
                : id;
            QStringList answers;
            for (int j = i + 1; j < steps.size(); ++j) {
                const QJsonObject reply = steps.at(j).toObject().value(QStringLiteral("message")).toObject();
                if (reply.value(QStringLiteral("type")).toString() == QStringLiteral("command.result")
                    && reply.value(QStringLiteral("id")) == refersTo) {
                    answers.append(QStringLiteral("%1 %2")
                                       .arg(reply.value(QStringLiteral("accepted")).toBool())
                                       .arg(reply.value(QStringLiteral("reason")).toString()));
                }
            }
            const QString answer = answers.join(QStringLiteral(" / "));
            bool renamed = false;
            for (const QJsonValue& arg : message.value(QStringLiteral("args")).toArray()) {
                renamed = renamed
                    || arg.toObject().value(QStringLiteral("name")).toString()
                           == QStringLiteral("conformanceWrongName");
            }
            const QString verb = message.value(QStringLiteral("verb")).toString();
            (renamed ? wrong : right).insert(verb, answer);
            (renamed ? wrongArgs : rightArgs).insert(verb, message.value(QStringLiteral("args")).toArray());
        }
        for (auto it = wrong.constBegin(); it != wrong.constEnd(); ++it) {
            if (!right.contains(it.key())) {
                continue;
            }
            ++compared;
            // The legs differ by the one renamed argument name and nothing
            // else: the same arguments in order, kinds and values alike.
            const QJsonArray a = rightArgs.value(it.key());
            const QJsonArray b = wrongArgs.value(it.key());
            int renamedCount = 0;
            bool otherwiseAlike = a.size() == b.size();
            for (int k = 0; otherwiseAlike && k < a.size(); ++k) {
                QJsonObject x = a.at(k).toObject();
                QJsonObject y = b.at(k).toObject();
                if (x.value(QStringLiteral("name")) != y.value(QStringLiteral("name"))) {
                    ++renamedCount;
                    otherwiseAlike = y.value(QStringLiteral("name")).toString()
                        == QStringLiteral("conformanceWrongName");
                    x.remove(QStringLiteral("name"));
                    y.remove(QStringLiteral("name"));
                }
                otherwiseAlike = otherwiseAlike && x == y;
            }
            if (!otherwiseAlike || renamedCount != 1) {
                same.append(QStringLiteral("%1 %2: the legs differ by more than one renamed "
                                           "argument name")
                                .arg(entry.id, it.key()));
            }
            if (right.value(it.key()) == it.value()) {
                same.append(QStringLiteral("%1 %2: both legs answered \"%3\"")
                                .arg(entry.id, it.key(), it.value()));
            }
        }
    }
    QVERIFY2(compared >= 25, qPrintable(QString::number(compared)));
    QVERIFY2(same.isEmpty(), qPrintable(same.join(QLatin1Char('\n'))));

    // nnr.applyModelSelection names the selection it applies by the
    // revision the snapshot gave (dspAssets' selectionRevision), as an app
    // does; any other revision is refused before the arguments matter.
    const QJsonArray steps =
        fixture(QStringLiteral("session-verbs-nnr")).value(QStringLiteral("steps")).toArray();
    double snapshotRevision = -1.0;
    double invokedRevision = -2.0;
    for (const QJsonValue& value : steps) {
        const QJsonObject step = value.toObject();
        const QJsonObject message = step.value(QStringLiteral("message")).toObject();
        if (message.value(QStringLiteral("type")).toString() == QStringLiteral("object.create")
            && message.value(QStringLiteral("key")).toString() == QStringLiteral("dspAssets")) {
            for (const QJsonValue& p : message.value(QStringLiteral("properties")).toArray()) {
                if (p.toObject().value(QStringLiteral("name")).toString()
                    == QStringLiteral("selectionRevision")) {
                    snapshotRevision = p.toObject().value(QStringLiteral("value")).toDouble();
                }
            }
        }
        if (step.value(QStringLiteral("role")).toString() == QStringLiteral("behaviour")
            && message.value(QStringLiteral("verb")).toString()
                   == QStringLiteral("nnr.applyModelSelection")) {
            invokedRevision = message.value(QStringLiteral("args")).toArray().at(0).toObject()
                                  .value(QStringLiteral("value")).toDouble();
        }
    }
    QCOMPARE(invokedRevision, snapshotRevision);
}

void TstLinkConformanceSession::everyFixtureRunsOnTheStation()
{
    // The station's runner plays every fixture (section 16.3), and each
    // fixture's shape is the format's.
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("session"))) {
        const QJsonObject o = fixture(entry.id);
        const QString problem = LinkFixtures::checkSessionFormat(o);
        QVERIFY2(problem.isEmpty(), qPrintable(entry.id + QStringLiteral(": ") + problem));
        QVERIFY2(LinkFixtures::runsOn(o, QStringLiteral("station")), qPrintable(entry.id));
    }
}

void TstLinkConformanceSession::appFixturesHoldOnlyWhatAConformantClientSends()
{
    QString error;
    const QJsonObject surface = LinkFixtures::readObject(
        QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("surface.json")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QStringList problems;
    int forTheApp = 0;
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("session"))) {
        const QJsonObject o = fixture(entry.id);
        if (!LinkFixtures::runsOn(o, QStringLiteral("app"))) {
            continue;
        }
        ++forTheApp;
        problems.append(appConformanceProblems(entry.id, o, surface));
    }
    QVERIFY2(forTheApp >= 20, qPrintable(QString::number(forTheApp)));
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

// The phone's monitor-audio fixtures: a media connection id is the app's
// own. The runner fills "$uuid:<name>" with a new canonical UUID each time
// and records it; a station message cannot hold one; a fixture for the app
// whose media.control pins a connection id is named.
void TstLinkConformanceSession::mediaConnectionIdsAreTheAppsOwn()
{
    LinkFixtures::Captures captures;
    int counter = 0;
    QString error;
    const QJsonValue first = LinkFixtures::substitute(
        QJsonValue(QStringLiteral("$uuid:media1")), &captures, &counter, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QJsonValue second = LinkFixtures::substitute(
        QJsonValue(QStringLiteral("$uuid:media2")), &captures, &counter, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    for (const QJsonValue& filled : {first, second}) {
        const QUuid uuid = QUuid::fromString(filled.toString());
        QVERIFY(!uuid.isNull());
        QCOMPARE(uuid.toString(QUuid::WithoutBraces), filled.toString());
    }
    QVERIFY(first != second);
    QCOMPARE(captures.value(QStringLiteral("media1")), first);
    QCOMPARE(captures.value(QStringLiteral("media2")), second);
    QCOMPARE(counter, 0);
    QCOMPARE(LinkFixtures::match(QJsonValue(QStringLiteral("$uuid:media1")), first, &captures),
             QStringLiteral("$: $uuid:media1 stands only in a client message"));

    const QJsonObject surface = LinkFixtures::readObject(
        QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("surface.json")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QJsonObject base = fixture(QStringLiteral("session-monitor-audio"));
    QVERIFY(!base.isEmpty());
    QVERIFY(appConformanceProblems(QStringLiteral("base"), base, surface).isEmpty());
    const auto firstPayload = [](const QJsonObject& step) {
        return step.value(QStringLiteral("message"))
            .toObject()
            .value(QStringLiteral("payload"))
            .toObject();
    };
    // A start whose connection id is pinned, and a station message
    // holding "$uuid:<name>".
    QJsonObject pinned = base;
    QJsonObject stationUuid = base;
    QJsonArray pinnedSteps = pinned.value(QStringLiteral("steps")).toArray();
    QJsonArray stationSteps = stationUuid.value(QStringLiteral("steps")).toArray();
    bool pinnedOne = false;
    bool stationOne = false;
    for (int i = 0; i < pinnedSteps.size(); ++i) {
        QJsonObject step = pinnedSteps.at(i).toObject();
        QJsonObject payload = firstPayload(step);
        if (payload.isEmpty()) {
            continue;
        }
        payload.insert(QStringLiteral("connectionId"),
                       QStringLiteral("11111111-2222-4333-8444-555555555555"));
        QJsonObject message = step.value(QStringLiteral("message")).toObject();
        message.insert(QStringLiteral("payload"), payload);
        step.insert(QStringLiteral("message"), message);
        if (!pinnedOne && step.value(QStringLiteral("from")) == QJsonValue(QStringLiteral("client"))) {
            pinnedSteps.replace(i, step);
            pinnedOne = true;
        }
        if (!stationOne && step.value(QStringLiteral("from")) == QJsonValue(QStringLiteral("station"))) {
            payload.insert(QStringLiteral("connectionId"), QStringLiteral("$uuid:media1"));
            message.insert(QStringLiteral("payload"), payload);
            step.insert(QStringLiteral("message"), message);
            stationSteps.replace(i, step);
            stationOne = true;
        }
    }
    QVERIFY(pinnedOne && stationOne);
    pinned.insert(QStringLiteral("steps"), pinnedSteps);
    stationUuid.insert(QStringLiteral("steps"), stationSteps);
    QStringList p = appConformanceProblems(QStringLiteral("x"), pinned, surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("a media connectionId is the app's own")),
             qPrintable(p.join('|')));
    p = appConformanceProblems(QStringLiteral("x"), stationUuid, surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(
                 QStringLiteral("$.payload.connectionId is $uuid:media1, which an app runner "
                                "cannot send")),
             qPrintable(p.join('|')));
}

void TstLinkConformanceSession::theConformanceCheckCatchesWhatAnAppCannotSend()
{
    QString error;
    const QJsonObject surface = LinkFixtures::readObject(
        QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("surface.json")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QJsonObject base = fixture(QStringLiteral("session-property-write"));
    QVERIFY(!base.isEmpty());
    QVERIFY(appConformanceProblems(QStringLiteral("base"), base, surface).isEmpty());

    // Each alteration an app could not produce is named.
    const auto altered = [&base](const std::function<void(QJsonObject&)>& change, int* at) {
        QJsonObject o = base;
        QJsonArray steps = o.value(QStringLiteral("steps")).toArray();
        for (int i = 0; i < steps.size(); ++i) {
            QJsonObject step = steps.at(i).toObject();
            if (step.value(QStringLiteral("role")).toString() != QStringLiteral("behaviour")) {
                continue;
            }
            QJsonObject message = step.value(QStringLiteral("message")).toObject();
            const QJsonObject before = message;
            change(message);
            if (message != before) {
                step.insert(QStringLiteral("message"), message);
                steps.replace(i, step);
                *at = i;
                break;
            }
        }
        o.insert(QStringLiteral("steps"), steps);
        return o;
    };
    int at = -1;
    // An older app's hello, without majors or features.
    QStringList p = appConformanceProblems(QStringLiteral("x"), altered([](QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() == QStringLiteral("hello")) {
            m.remove(QStringLiteral("majors"));
            m.remove(QStringLiteral("features"));
        }
    }, &at), surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("majors")), qPrintable(p.join('|')));
    // A pinned list of majors.
    p = appConformanceProblems(QStringLiteral("x"), altered([](QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() == QStringLiteral("hello")) {
            m.insert(QStringLiteral("majors"), QJsonArray{1});
        }
    }, &at), surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("$majors")), qPrintable(p.join('|')));
    // A pinned peer name.
    p = appConformanceProblems(QStringLiteral("x"), altered([](QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() == QStringLiteral("hello")) {
            m.insert(QStringLiteral("peer"), QStringLiteral("NereusSDR iPhone"));
        }
    }, &at), surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("peer")), qPrintable(p.join('|')));
    // A literal write id.
    p = appConformanceProblems(QStringLiteral("x"), altered([](QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() == QStringLiteral("property.write")) {
            m.insert(QStringLiteral("writeId"), 2);
        }
    }, &at), surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("writeId")), qPrintable(p.join('|')));
    // A write to a property the Core sets itself.
    p = appConformanceProblems(QStringLiteral("x"), altered([](QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() == QStringLiteral("property.write")) {
            m.insert(QStringLiteral("properties"),
                     QJsonArray{QJsonObject{{QStringLiteral("ordinal"), 15},
                                            {QStringLiteral("name"), QStringLiteral("signalStrengthDbm")},
                                            {QStringLiteral("kind"), QStringLiteral("f64")},
                                            {QStringLiteral("value"), -50.0}}});
        }
    }, &at), surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("signalStrengthDbm")),
             qPrintable(p.join('|')));
    // A message kind a client never sends, as behaviour.
    p = appConformanceProblems(QStringLiteral("x"), altered([](QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() == QStringLiteral("auth.request")) {
            m = QJsonObject{{QStringLiteral("type"), QStringLiteral("conformance.unknown")}};
        }
    }, &at), surface);
    QVERIFY2(p.join(QLatin1Char('|')).contains(QStringLiteral("never sends")),
             qPrintable(p.join('|')));
    // The rest of the rules, each planted once on a copy of a fixture:
    // step `index` of `id`, changed by `change`, must be named by `expect`.
    const auto planted = [&surface, this](const QString& id, int index,
                                          const std::function<void(QJsonObject&)>& change) {
        QJsonObject o = fixture(id);
        QJsonArray steps = o.value(QStringLiteral("steps")).toArray();
        QJsonObject step = steps.at(index).toObject();
        change(step);
        steps.replace(index, step);
        o.insert(QStringLiteral("steps"), steps);
        return appConformanceProblems(id, o, surface).join(QLatin1Char('|'));
    };
    const auto setIn = [](QJsonObject& step, const QString& key, const QJsonValue& value) {
        QJsonObject message = step.value(QStringLiteral("message")).toObject();
        message.insert(key, value);
        step.insert(QStringLiteral("message"), message);
    };
    const QString write = QStringLiteral("session-property-write");
    const QString ps3 = QStringLiteral("session-verbs-ps3");
    // A scripted hello or token in a fixture for the app.
    QString found = planted(write, 1, [](QJsonObject& step) {
        step.insert(QStringLiteral("role"), QStringLiteral("scripted"));
    });
    QVERIFY2(found.contains(QStringLiteral("cannot be scripted")), qPrintable(found));
    found = planted(write, 2, [](QJsonObject& step) {
        step.insert(QStringLiteral("role"), QStringLiteral("scripted"));
    });
    QVERIFY2(found.contains(QStringLiteral("own auth.request")), qPrintable(found));
    // Station messages an app's runner cannot fill: $capture, a stray
    // $any, a named $int.
    found = planted(write, 3, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("reason"), QStringLiteral("$capture:why"));
    });
    QVERIFY2(found.contains(QStringLiteral("$capture:why")), qPrintable(found));
    found = planted(write, 3, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("accepted"), QStringLiteral("$any"));
    });
    QVERIFY2(found.contains(QStringLiteral("$.accepted is $any")), qPrintable(found));
    found = planted(write, 3, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("retryable"), QStringLiteral("$int:n"));
    });
    QVERIFY2(found.contains(QStringLiteral("$int:n")), qPrintable(found));
    // The Core's hello may capture its challenge as "challenge", and
    // nothing else; a device sign-in is "$device:signed" with token "",
    // after that capture.
    const QString signIn = QStringLiteral("session-device-sign-in");
    QCOMPARE(planted(signIn, 0, [](QJsonObject&) {}), QString());
    found = planted(signIn, 0, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("challenge"), QStringLiteral("$capture:other"));
    });
    QVERIFY2(found.contains(QStringLiteral("$capture:other")), qPrintable(found));
    QVERIFY2(found.contains(QStringLiteral("needs the Core's hello")), qPrintable(found));
    found = planted(signIn, 2, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("device"), QStringLiteral("$device:otherChallenge"));
    });
    QVERIFY2(found.contains(QStringLiteral("a device sign-in is")), qPrintable(found));
    found = planted(signIn, 2, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("token"), QStringLiteral("$ref:token"));
    });
    QVERIFY2(found.contains(QStringLiteral("a device sign-in is")), qPrintable(found));
    // A verb with arguments it does not take, and one not advertised
    // (PureSignal's gate is psAlgorithmVersion equal to 3; 4 fails it).
    found = planted(ps3, 31, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("args"),
              QJsonArray{QJsonObject{{QStringLiteral("ordinal"), 0},
                                     {QStringLiteral("name"), QStringLiteral("enabled")},
                                     {QStringLiteral("kind"), QStringLiteral("i64")},
                                     {QStringLiteral("value"), 0}}});
    });
    QVERIFY2(found.contains(QStringLiteral("ps3.twoTone's arguments")), qPrintable(found));
    found = planted(ps3, 4, [](QJsonObject& step) {
        QJsonObject message = step.value(QStringLiteral("message")).toObject();
        QJsonArray properties = message.value(QStringLiteral("properties")).toArray();
        for (int i = 0; i < properties.size(); ++i) {
            QJsonObject entry = properties.at(i).toObject();
            if (entry.value(QStringLiteral("name")).toString() == QStringLiteral("psAlgorithmVersion")) {
                entry.insert(QStringLiteral("value"), 4);
                properties.replace(i, entry);
            }
        }
        message.insert(QStringLiteral("properties"), properties);
        step.insert(QStringLiteral("message"), message);
    });
    QVERIFY2(found.contains(QStringLiteral("ps3.off was not advertised")), qPrintable(found));
    // A placeholder among a behaviour step's arguments.
    found = planted(ps3, 38, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("args"),
              QJsonArray{QJsonObject{{QStringLiteral("ordinal"), 0},
                                     {QStringLiteral("name"), QStringLiteral("label")},
                                     {QStringLiteral("kind"), QStringLiteral("utf8")},
                                     {QStringLiteral("value"), QStringLiteral("$string")}}});
    });
    QVERIFY2(found.contains(QStringLiteral("never placeholders")), qPrintable(found));
    // An id without the link's range, or from 0 where 1 is the least.
    found = planted(ps3, 27, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("id"), QStringLiteral("$int:invoke23"));
    });
    QVERIFY2(found.contains(QStringLiteral(":1:4294967295")), qPrintable(found));
    found = planted(ps3, 27, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("id"), QStringLiteral("$int:invoke23:0:4294967295"));
    });
    QVERIFY2(found.contains(QStringLiteral(":1:4294967295")), qPrintable(found));
    // A scripted id below 1000, and a scripted message naming a value.
    found = planted(ps3, 35, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("id"), 167);
    });
    QVERIFY2(found.contains(QStringLiteral("from 1000 up")), qPrintable(found));
    found = planted(ps3, 35, [&setIn](QJsonObject& step) {
        setIn(step, QStringLiteral("id"), QStringLiteral("$int:scripted"));
    });
    QVERIFY2(found.contains(QStringLiteral("a scripted message holds $int:scripted")),
             qPrintable(found));
}

void TstLinkConformanceSession::refusalsOfOutboundWritesArePlain()
{
    // The property-write fixture pins the station's two refusals of an
    // outbound write, and an operator may read both.
    const QJsonObject o = fixture(QStringLiteral("session-property-write"));
    QVERIFY(!o.isEmpty());
    QSet<QString> reasons;
    for (const QJsonValue& step : o.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject message = step.toObject().value(QStringLiteral("message")).toObject();
        if (message.value(QStringLiteral("type")).toString() != QStringLiteral("property.result")) {
            continue;
        }
        for (const QJsonValue& result : message.value(QStringLiteral("results")).toArray()) {
            const QJsonObject r = result.toObject();
            const QString property = r.value(QStringLiteral("property")).toString();
            if (property == QStringLiteral("active")
                || property == QStringLiteral("signalStrengthDbm")) {
                reasons.insert(property + QLatin1Char('=')
                               + r.value(QStringLiteral("reason")).toString());
            }
        }
    }
    const QString active = SliceModel::activeWriteReason();
    QVERIFY2(reasons.contains(QStringLiteral("active=") + active), qPrintable(active));
    QVERIFY2(OperatorWording::isPlain(active), qPrintable(active));
    bool signal = false;
    for (const QString& entry : reasons) {
        if (entry.startsWith(QStringLiteral("signalStrengthDbm="))) {
            signal = true;
            const QString reason = entry.section(QLatin1Char('='), 1);
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }
    QVERIFY(signal);
}

void TstLinkConformanceSession::alteredFixturesFailReadably()
{
    // A number the DSP measures matches within its stated tolerance only.
    LinkFixtures::Captures none;
    QVERIFY(LinkFixtures::match(QStringLiteral("$within:1:-399.02"), -399.5, &none).isEmpty());
    const QString outside = LinkFixtures::match(QStringLiteral("$within:1:-399.02"), -401.0, &none);
    QVERIFY2(outside.contains(QStringLiteral("within 1 of -399.02")), qPrintable(outside));
    QVERIFY(!LinkFixtures::match(QStringLiteral("$within:1:-399.02"), QStringLiteral("x"), &none)
                 .isEmpty());
    // Both numbers are JSON numbers: forms a number parser might take but
    // JSON does not ("+1", "inf", ".5", "1.", "0x10", a space) are refused.
    for (const char* bad : {"$within:+1:5", "$within:1:inf", "$within:.5:5", "$within:1.:5",
                            "$within:1:0x10", "$within: 1:5", "$within:1:-nan"}) {
        const QString refused = LinkFixtures::match(QString::fromLatin1(bad), 5.0, &none);
        QVERIFY2(refused.contains(QStringLiteral("is not $within")), bad);
    }
    QVERIFY(LinkFixtures::match(QStringLiteral("$within:1e-1:5.05"), 5.0, &none).isEmpty());
    // "$majors": an app supporting [1, 2] and choosing 1 passes; a list out
    // of order, or one without the chosen major, does not.
    const QJsonObject hello{{QStringLiteral("major"), 1},
                            {QStringLiteral("majors"), QStringLiteral("$majors")}};
    const auto withMajors = [](const QJsonArray& majors) {
        return QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("majors"), majors}};
    };
    QVERIFY(LinkFixtures::match(hello, withMajors({1, 2}), &none).isEmpty());
    QVERIFY(LinkFixtures::match(hello, withMajors({1}), &none).isEmpty());
    QVERIFY(!LinkFixtures::match(hello, withMajors({2, 1}), &none).isEmpty());
    QVERIFY(!LinkFixtures::match(hello, withMajors({2, 3}), &none).isEmpty());
    QVERIFY(!LinkFixtures::match(hello, withMajors({}), &none).isEmpty());
    // connect-connectable's meter readings never admit the meter pump's
    // no-reading value: a reading of -400 fails the fixture. Load findings
    // 4: nor the floor an unmeasured receiver reads (-400 plus the meter
    // offset, -399.02, which the fixture used to pin): the eight readings
    // are the receiver's first measured block.
    int readings = 0;
    for (const QJsonValue& step :
         fixture(QStringLiteral("session-connect-connectable")).value(QStringLiteral("steps")).toArray()) {
        const QJsonObject message = step.toObject().value(QStringLiteral("message")).toObject();
        if (!message.value(QStringLiteral("properties")).isArray()) {
            continue;
        }
        for (const QJsonValue& p : message.value(QStringLiteral("properties")).toArray()) {
            const QJsonValue value = p.toObject().value(QStringLiteral("value"));
            if (value.toString().startsWith(QStringLiteral("$within:"))) {
                ++readings;
                QVERIFY2(!LinkFixtures::match(value, SliceMeterPump::kNoReadingDbm, &none).isEmpty(),
                         qPrintable(value.toString()));
                QVERIFY2(!LinkFixtures::match(value, -399.02, &none).isEmpty(),
                         qPrintable(value.toString()));
            }
        }
    }
    QCOMPARE(readings, 8);

    // A value the station sends, changed: the failure names the step, the
    // path inside the message and what the station really sent.
    QJsonObject changed = fixture(QStringLiteral("session-wrong-token"));
    QVERIFY(!changed.isEmpty());
    QJsonArray steps = changed.value(QStringLiteral("steps")).toArray();
    int index = -1;
    for (int i = 0; i < steps.size(); ++i) {
        const QJsonObject message = steps.at(i).toObject().value(QStringLiteral("message")).toObject();
        if (message.value(QStringLiteral("type")).toString() == QStringLiteral("auth.result")) {
            index = i;
        }
    }
    QVERIFY(index >= 0);
    QJsonObject step = steps.at(index).toObject();
    QJsonObject message = step.value(QStringLiteral("message")).toObject();
    message.insert(QStringLiteral("reason"), QStringLiteral("Something else"));
    step.insert(QStringLiteral("message"), message);
    steps.replace(index, step);
    changed.insert(QStringLiteral("steps"), steps);
    QString failure = run(QStringLiteral("altered-wrong-token"), changed);
    QVERIFY2(failure.contains(QStringLiteral("step %1").arg(index))
                 && failure.contains(QStringLiteral("$.reason"))
                 && failure.contains(QStringLiteral("the station sent")),
             qPrintable(failure));

    // Too little virtual time: the connect deadline has not come, so the
    // station is still waiting and never sends what the fixture expects.
    QJsonObject early = fixture(QStringLiteral("session-connect-deadline"));
    QVERIFY(!early.isEmpty());
    steps = early.value(QStringLiteral("steps")).toArray();
    for (int i = 0; i < steps.size(); ++i) {
        if (steps.at(i).toObject().contains(QStringLiteral("advanceMs"))) {
            steps.replace(i, QJsonObject{{QStringLiteral("advanceMs"), 29000}});
        }
    }
    early.insert(QStringLiteral("steps"), steps);
    failure = run(QStringLiteral("altered-connect-deadline"), early);
    QVERIFY2(failure.contains(QStringLiteral("the station sent nothing more")),
             qPrintable(failure));
}

// Addendum G-53: the place-freed fixture passes 200 s of virtual time
// before its own client signs in, past the station's sign-in deadline, so
// its own connection opens at an openOwnConnection step instead of before
// the first step.
void TstLinkConformanceSession::aDeferredOwnConnectionOpensAtItsStep()
{
    const QJsonObject placeFreed = fixture(QStringLiteral("session-place-freed"));
    QVERIFY(!placeFreed.isEmpty());
    QVERIFY2(LinkFixtures::checkSessionFormat(placeFreed).isEmpty(),
             qPrintable(LinkFixtures::checkSessionFormat(placeFreed)));
    QJsonArray steps = placeFreed.value(QStringLiteral("steps")).toArray();
    int open = -1;
    for (int i = 0; i < steps.size(); ++i) {
        if (steps.at(i).toObject().contains(QStringLiteral("openOwnConnection"))) {
            open = i;
        }
    }
    QVERIFY(open > 0 && open + 1 < steps.size());

    // The same fixture with its own connection opened first, as before: the
    // sign-in deadline ends it during the 200 s, before the client's hello.
    QJsonObject eager = placeFreed;
    QJsonObject setup = eager.value(QStringLiteral("stationSetup")).toObject();
    setup.remove(QStringLiteral("deferOwnConnection"));
    eager.insert(QStringLiteral("stationSetup"), setup);
    QJsonArray eagerSteps = steps;
    eagerSteps.removeAt(open);
    eager.insert(QStringLiteral("steps"), eagerSteps);
    QVERIFY2(LinkFixtures::checkSessionFormat(eager).isEmpty(),
             qPrintable(LinkFixtures::checkSessionFormat(eager)));
    QString failure = run(QStringLiteral("eager-place-freed"), eager);
    QVERIFY2(failure.contains(QStringLiteral("(client hello): the link is already closed")),
             qPrintable(failure));

    // The format check: an own-client step before the connection opens, a
    // deferred fixture that never opens it, and the step without the setup.
    QJsonObject early = placeFreed;
    QJsonArray earlySteps = steps;
    const QJsonValue opening = earlySteps.at(open);
    earlySteps.replace(open, earlySteps.at(open + 1));
    earlySteps.replace(open + 1, opening);
    early.insert(QStringLiteral("steps"), earlySteps);
    QVERIFY2(LinkFixtures::checkSessionFormat(early).contains(
                 QStringLiteral("has no connection before the openOwnConnection step")),
             qPrintable(LinkFixtures::checkSessionFormat(early)));

    QJsonObject neverOpened = placeFreed;
    QJsonArray closedSteps;
    for (int i = 0; i < open; ++i) {
        closedSteps.append(steps.at(i));
    }
    neverOpened.insert(QStringLiteral("steps"), closedSteps);
    QVERIFY2(LinkFixtures::checkSessionFormat(neverOpened).contains(
                 QStringLiteral("no step opens the runner's own connection")),
             qPrintable(LinkFixtures::checkSessionFormat(neverOpened)));

    QJsonObject stray = eager;
    QJsonArray straySteps = eagerSteps;
    straySteps.insert(0, QJsonObject{{QStringLiteral("openOwnConnection"), true}});
    stray.insert(QStringLiteral("steps"), straySteps);
    QVERIFY2(LinkFixtures::checkSessionFormat(stray).contains(
                 QStringLiteral("openOwnConnection must be true, once")),
             qPrintable(LinkFixtures::checkSessionFormat(stray)));
}

void TstLinkConformanceSession::currentCoreHelloOfferRemainsStrict()
{
    const QJsonObject legacy{{QStringLiteral("type"), QStringLiteral("hello")},
        {QStringLiteral("features"), QJsonObject{{QStringLiteral("deviceAuth"), 1},
            {QStringLiteral("pairing"), 1}, {QStringLiteral("sessionHolder"), 1}}}};
    const QJsonValue expected = LinkFixtures::currentCoreStationExpectation(legacy);
    QJsonObject actual = expected.toObject();
    QCOMPARE(actual.value(QStringLiteral("features")).toObject()
                 .value(QStringLiteral("radioMic")).toInt(), 2);
    QVERIFY(LinkFixtures::match(expected, actual, nullptr).isEmpty());
    for (int version : {0, 1, 3}) {
        QJsonObject wrong = actual;
        QJsonObject features = wrong.value(QStringLiteral("features")).toObject();
        if (version == 0) { features.remove(QStringLiteral("radioMic")); }
        else { features.insert(QStringLiteral("radioMic"), version); }
        wrong.insert(QStringLiteral("features"), features);
        QVERIFY(LinkFixtures::match(expected, wrong, nullptr)
                    .contains(QStringLiteral("radioMic")));
    }
    QJsonObject extra = actual;
    QJsonObject features = extra.value(QStringLiteral("features")).toObject();
    features.insert(QStringLiteral("unexpected"), 1);
    extra.insert(QStringLiteral("features"), features);
    QVERIFY(LinkFixtures::match(expected, extra, nullptr)
                .contains(QStringLiteral("unexpected")));

    QJsonObject explicitOlder = actual;
    features.remove(QStringLiteral("unexpected"));
    features.insert(QStringLiteral("radioMic"), 1);
    explicitOlder.insert(QStringLiteral("features"), features);
    QCOMPARE(LinkFixtures::currentCoreStationExpectation(explicitOlder), QJsonValue(explicitOlder));
    QVERIFY(!LinkFixtures::match(explicitOlder, actual, nullptr).isEmpty());
    QJsonObject other = legacy;
    other.insert(QStringLiteral("type"), QStringLiteral("capabilities"));
    QCOMPARE(LinkFixtures::currentCoreStationExpectation(other), QJsonValue(other));
    const QJsonObject bare{{QStringLiteral("type"), QStringLiteral("hello")}};
    QCOMPARE(LinkFixtures::currentCoreStationExpectation(bare), QJsonValue(bare));
    const QJsonObject noIdentity{{QStringLiteral("type"), QStringLiteral("hello")},
        {QStringLiteral("features"), QJsonObject{{QStringLiteral("pairing"), 1}}}};
    QCOMPARE(LinkFixtures::currentCoreStationExpectation(noIdentity), QJsonValue(noIdentity));
}

void TstLinkConformanceSession::jsonStringsMatchTheirShape()
{
    // {"$json": <expectation>}: a string the station sends, parsed and
    // matched against the expectation with the same placeholders.
    const auto form = [](const QJsonValue& expectation) {
        return QJsonObject{{QStringLiteral("$json"), expectation}};
    };
    const QJsonObject entry{{QStringLiteral("deviceId"), QStringLiteral("$string")},
                            {QStringLiteral("state"), QStringLiteral("listening")},
                            {QStringLiteral("listeningOn"),
                             QJsonArray{QJsonObject{{QStringLiteral("sliceId"), 0},
                                                    {QStringLiteral("letter"), QStringLiteral("A")}}}}};
    LinkFixtures::Captures none;

    // A match, and the same text with its keys in another order.
    const QString text = QStringLiteral(
        R"([{"deviceId":"abc","state":"listening","listeningOn":[{"sliceId":0,"letter":"A"}]}])");
    const QString reordered = QStringLiteral(
        R"( [ {"listeningOn":[{"letter":"A","sliceId":0}], "state":"listening", "deviceId":"x"} ] )");
    QString result = LinkFixtures::match(form(QJsonArray{entry}), text, &none);
    QVERIFY2(result.isEmpty(), qPrintable(result));
    result = LinkFixtures::match(form(QJsonArray{entry}), reordered, &none);
    QVERIFY2(result.isEmpty(), qPrintable(result));

    // Array order matters: two entries the other way round fail, at the
    // first element inside the string.
    const QJsonObject other{{QStringLiteral("deviceId"), QStringLiteral("$string")},
                            {QStringLiteral("state"), QStringLiteral("away")},
                            {QStringLiteral("listeningOn"), QJsonArray{}}};
    const QString two = QStringLiteral(
        R"([{"deviceId":"b","state":"away","listeningOn":[]},)"
        R"({"deviceId":"a","state":"listening","listeningOn":[{"sliceId":0,"letter":"A"}]}])");
    result = LinkFixtures::match(form(QJsonArray{entry, other}), two, &none);
    QVERIFY2(result.startsWith(QStringLiteral("$($json)[0].")), qPrintable(result));
    result = LinkFixtures::match(form(QJsonArray{other, entry}), two, &none);
    QVERIFY2(result.isEmpty(), qPrintable(result));

    // Placeholders inside: a capture made inside one string is a $ref
    // inside the next; a $json nests inside a $json; a scalar is JSON too.
    LinkFixtures::Captures captures;
    const QJsonObject captured{{QStringLiteral("id"), QStringLiteral("$capture:first")},
                               {QStringLiteral("count"), QStringLiteral("$int")},
                               {QStringLiteral("inner"), form(QJsonObject{
                                    {QStringLiteral("ok"), true}})}};
    result = LinkFixtures::match(
        form(captured), QStringLiteral(R"({"id":"k1","count":3,"inner":"{\"ok\":true}"})"),
        &captures);
    QVERIFY2(result.isEmpty(), qPrintable(result));
    QCOMPARE(captures.value(QStringLiteral("first")).toString(), QStringLiteral("k1"));
    const QJsonObject referred{{QStringLiteral("id"), QStringLiteral("$ref:first")}};
    QVERIFY(LinkFixtures::match(form(referred), QStringLiteral(R"({"id":"k1"})"), &captures)
                .isEmpty());
    result = LinkFixtures::match(form(referred), QStringLiteral(R"({"id":"k2"})"), &captures);
    QVERIFY2(result.contains(QStringLiteral("$($json).id")), qPrintable(result));
    result = LinkFixtures::match(form(QJsonObject{{QStringLiteral("inner"), form(true)}}),
                                 QStringLiteral(R"({"inner":"false"})"), &none);
    QVERIFY2(result.startsWith(QStringLiteral("$($json).inner($json)")), qPrintable(result));
    QVERIFY(LinkFixtures::match(form(QStringLiteral("$int")), QStringLiteral("7"), &none).isEmpty());

    // Not a string, not one JSON value, or a second key beside "$json".
    QVERIFY(LinkFixtures::match(form(QJsonArray{}), QJsonArray{}, &none)
                .contains(QStringLiteral("expected a string holding JSON")));
    for (const QString& bad : {QStringLiteral("[1,"), QStringLiteral(""), QStringLiteral("1,2"),
                               QStringLiteral("/tmp/key.pem")}) {
        result = LinkFixtures::match(form(QStringLiteral("$any")), bad, &none);
        QVERIFY2(result.startsWith(QStringLiteral("$: not JSON")), qPrintable(result));
    }
    QJsonObject twoKeys = form(QJsonArray{});
    twoKeys.insert(QStringLiteral("other"), 1);
    QVERIFY(LinkFixtures::match(twoKeys, QStringLiteral("[]"), &none)
                .contains(QStringLiteral("stands alone")));

    // Only the station sends one: a client message holding it is refused.
    int counter = 0;
    QString error;
    LinkFixtures::substitute(QJsonObject{{QStringLiteral("value"), form(QJsonArray{})}}, &none,
                             &counter, &error);
    QVERIFY2(error.contains(QStringLiteral("cannot stand in a client message")), qPrintable(error));

    // In a fixture: a string that does not parse fails with the fixture,
    // the step, the path and "not JSON". keyPath is a file path, not JSON.
    QJsonObject altered = fixture(QStringLiteral("session-connected-devices"));
    QVERIFY(!altered.isEmpty());
    QJsonArray steps = altered.value(QStringLiteral("steps")).toArray();
    int index = -1;
    for (int i = 0; i < steps.size() && index < 0; ++i) {
        const QJsonObject message = steps.at(i).toObject().value(QStringLiteral("message")).toObject();
        if (message.value(QStringLiteral("key")).toString() == QStringLiteral("devices")
            && message.value(QStringLiteral("type")).toString() == QStringLiteral("delta")) {
            index = i;
        }
    }
    QVERIFY(index >= 0);
    QJsonObject step = steps.at(index).toObject();
    QJsonObject message = step.value(QStringLiteral("message")).toObject();
    QJsonArray properties = message.value(QStringLiteral("properties")).toArray();
    QJsonObject keyPath = properties.at(6).toObject();
    QCOMPARE(keyPath.value(QStringLiteral("name")).toString(), QStringLiteral("keyPath"));
    keyPath.insert(QStringLiteral("value"), form(QStringLiteral("$string")));
    properties.replace(6, keyPath);
    message.insert(QStringLiteral("properties"), properties);
    step.insert(QStringLiteral("message"), message);
    steps.replace(index, step);
    altered.insert(QStringLiteral("steps"), steps);
    const QString failure = run(QStringLiteral("altered-connected-devices"), altered);
    QVERIFY2(failure.startsWith(QStringLiteral("altered-connected-devices: step %1").arg(index))
                 && failure.contains(QStringLiteral("$.properties[6].value: not JSON")),
             qPrintable(failure));
}

// Task 28 tail (R-IOS-16): session-grace-expired failed over a data
// channel because the clock read a long coarse timer as started again each
// time it looked (its remaining time, kept to the interval, stayed at the
// interval while real time passed), and pushed the grace timer's due time
// past the fixture's advanceMs. A timer reporting more than its interval
// is forced here, then real time passes between two looks; the 180 s must
// still end at 180 s of virtual time.
void TstLinkConformanceSession::theVirtualClockKeepsALongTimersDueAsRealTimePasses()
{
    constexpr int kIntervalMs = 180000;
    // How far above its interval the timer must report, so the real time
    // below passes while the report is still above it.
    constexpr int kAboveMs = 100;
    QObject root;
    auto* timer = new QTimer(&root);
    timer->setSingleShot(true);
    QSignalSpy fired(timer, &QTimer::timeout);
    // A coarse timer (Qt's default) is rounded, later or earlier by the
    // time it starts; start it until it reports well above its interval.
    const QDeadlineTimer searching(5000);
    timer->start(kIntervalMs);
    while (timer->remainingTime() <= kIntervalMs + kAboveMs && !searching.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        timer->start(kIntervalMs);
    }
    QVERIFY2(timer->remainingTime() > kIntervalMs + kAboveMs,
             "no start of a coarse timer reported more than its interval");

    LinkVirtualClock clock(&root, [] { QCoreApplication::processEvents(); });
    clock.scan();
    QVERIFY(clock.advance(1000).isEmpty());
    // Real time, well inside the time the report stays above the interval.
    QElapsedTimer passing;
    passing.start();
    while (passing.elapsed() < 60) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    QVERIFY2(timer->remainingTime() > kIntervalMs,
             "the report fell to the interval before the clock looked again");
    QVERIFY(clock.advance(kIntervalMs - 1000 - 1).isEmpty());
    QCOMPARE(fired.count(), 0);
    QVERIFY(clock.advance(1).isEmpty());
    QCOMPARE(fired.count(), 1);
    QCOMPARE(clock.now(), qint64(kIntervalMs));
}

// Task 28 tail (R-IOS-16): a station timer firing in real time made a
// fixture depend on how long its run took. Over a data channel the second
// client's connect took long enough for the 50 ms delta flush to fire in
// the middle of the step, and one coalesced delta went out as two. While
// the player holds the clock (a drain, a connect, an advance), the timers
// under its root fire on virtual time only; outside a hold they still run
// in real time, which the fixtures count on for a delta the flush sends.
void TstLinkConformanceSession::theVirtualClockAloneFiresTheStationsTimers()
{
    constexpr int kFlushMs = 50;
    QObject root;
    auto* flush = new QTimer(&root);
    flush->setSingleShot(true);
    flush->setTimerType(Qt::PreciseTimer);
    QSignalSpy fired(flush, &QTimer::timeout);
    // A timer outside the root keeps real time, held or not.
    QTimer outside;
    outside.setSingleShot(true);
    outside.setTimerType(Qt::PreciseTimer);
    QSignalSpy outsideFired(&outside, &QTimer::timeout);

    LinkVirtualClock clock(&root, [] { QCoreApplication::processEvents(); });
    {
        const LinkVirtualClock::Hold hold(clock);
        flush->start(kFlushMs);
        outside.start(kFlushMs);
        clock.scan();
        // Real time: until the flush timer's real expiry has come and gone.
        QVERIFY(QTest::qWaitFor([&outsideFired] { return outsideFired.count() == 1; }, 5000));
        QTRY_VERIFY_WITH_TIMEOUT(clock.swallowedForTest() >= 1, 5000);
        QCOMPARE(fired.count(), 0);
        QVERIFY(flush->isActive());
        // Virtual time: it fires at its interval (as the clock read it
        // when it first looked, a millisecond or so after the start), once.
        QVERIFY(clock.advance(kFlushMs - 5).isEmpty());
        QCOMPARE(fired.count(), 0);
        QVERIFY(clock.advance(5).isEmpty());
        QCOMPARE(fired.count(), 1);
        QVERIFY(!flush->isActive());
    }

    // Outside a hold its real expiry still fires it.
    flush->start(kFlushMs);
    QVERIFY(QTest::qWaitFor([&fired] { return fired.count() == 2; }, 5000));
}

// R-R3-49: session-tx-keepalive over a data channel failed under load
// (step 55: the watchdog's linkLost came where the time-out's 179 was
// due). The clock took a timer's due time from its real remaining time, so
// the meter pump, first seen a millisecond after its start, polled at
// 999 ms instead of 1000 ms, before the time-out read 179, and the
// watchdog's stop at 1051 ms came first. A start is timed by virtual time
// only: real time before the clock looks, a restart before the real
// remaining time has grown, and a repeating timer's real expiries outside
// a hold all leave its due time where virtual time puts it.
void TstLinkConformanceSession::theVirtualClockTimesAStartByVirtualTimeOnly()
{
    constexpr int kPumpMs = 100;
    constexpr int kWatchMs = 401;
    QObject root;
    auto* pump = new QTimer(&root);
    pump->setTimerType(Qt::PreciseTimer);
    pump->setInterval(kPumpMs);
    auto* watch = new QTimer(&root);
    watch->setSingleShot(true);
    watch->setTimerType(Qt::PreciseTimer);
    LinkVirtualClock clock(&root, [] { QCoreApplication::processEvents(); });

    // Started, then real time passes (as a busy machine takes it) before
    // the clock first looks.
    pump->start();
    QTest::qWait(30);
    clock.scan();
    QCOMPARE(clock.dueForTest(pump), qint64(kPumpMs));

    // Restarted at once, its real remaining time no higher than before.
    watch->start(kWatchMs);
    clock.scan();
    QVERIFY(clock.advance(50).isEmpty());
    watch->start(kWatchMs);
    clock.scan();
    QCOMPARE(clock.dueForTest(watch), qint64(50 + kWatchMs));

    // The repeating timer's real expiries outside a hold fire it in real
    // time, and leave its virtual phase alone.
    QSignalSpy polled(pump, &QTimer::timeout);
    QVERIFY(QTest::qWaitFor([&polled] { return polled.count() >= 2; }, 5000));
    clock.scan();
    QCOMPARE(clock.dueForTest(pump), qint64(kPumpMs));
    QVERIFY(clock.advance(kPumpMs * 10 - 50).isEmpty());
    QCOMPARE(clock.dueForTest(pump), qint64(kPumpMs * 11));
    QCOMPARE(clock.dueForTest(watch), -1);
}

QTEST_MAIN(TstLinkConformanceSession)
#include "tst_link_conformance_session.moc"
