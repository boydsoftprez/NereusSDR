// Modification history (NereusSDR):
// 2026-09-27: Cover queued and synchronous final connection closure.
// J.J. Boyd (KG4VCF), AI-assisted implementation via OpenAI Codex.
// 2026-09-29: Pin that a remote window logs no schema skew from the
// current Core and that every feature-gate row names a real property.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.
// 2026-09-29: load finding: positive waits follow the store's lockout and the
// heartbeat's own pings, nothing-happens checks the link's flush bound
// (SessionWait.h), and the suite runs as more than one ctest entry.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-30: RADE reason: the remote window declares radeReason.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
// tests/tst_station_session.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 18: the wss session.
//
// ── HOW THIS FILE IS SPLIT, AND WHY ──────────────────────────────────────
//
// The protocol assertions (the section 7.0 connect sequence and its exact
// message ORDER, the version policy in both directions, token rejection
// and rate limiting, admission, schema skew, the heartbeat's detection of
// a silently dead peer) all run over an IN-PROCESS, NON-TLS link
// (fakes/LoopbackTransport.h) and therefore run UNCONDITIONALLY, on every
// build. Only the genuinely TLS-specific slots -- that a real wss listener
// comes up, that a real client completes the handshake through it, and
// that a mismatched certificate fingerprint is refused -- QSKIP when
// QSslSocket::supportsSsl() is false, naming the Qt TLS backend in the
// skip message.
//
// That split is deliberate and is the difference between a suite that
// proves something and one that reports green because it skipped. It is
// possible only because StationServer and StationClient hold a
// SessionTransport rather than a QWebSocket; there is no test-only branch
// inside either class.
//
// ── THE ONE THAT MATTERS MOST ────────────────────────────────────────────
//
// heartbeatDetectsAPeerThatWentSilentWithoutClosing. The maintainer pulled
// the control-channel heartbeat forward from R4 for exactly one reason: a
// TCP connection that dies silently (laptop lid, cell handoff, NAT
// timeout) NEVER produces a close, so a daemon relying on a close sits
// believing a dead client is alive. The in-tree precedent this task was
// pointed at (TciServer.cpp's 20 s ping timer) explicitly does not track
// pongs and relies on a write eventually erroring, which does not detect
// that case. This slot is what proves this implementation does: the fake
// transport keeps the link nominally OPEN and simply stops answering
// pings, and the station is required to notice anyway. A test that killed
// the peer instead would prove nothing, because the close would do the
// work.
//
// =================================================================

#include <QtTest/QtTest>

#include <algorithm>

#include <QApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QGroupBox>
#include <QHostAddress>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUrl>
#include <QWebSocket>
#include <QWebSocketProtocol>

#include <memory>
#include <optional>

#include "core/ModelPaths.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/CoreInit.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/P1RadioConnection.h"
#include "core/security/CertificateStore.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/TokenStore.h"
#include "core/session/LinkVersion.h"
#include "core/session/SessionEndReasons.h"
#include "core/session/ObjectRegistry.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/SessionMessages.h"
#include "core/session/SessionTransport.h"
#include "core/session/StateMirror.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/security/StationIdentity.h"
#include "core/settings/SettingsProxy.h"
#include "core/meters/SliceMeterPump.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/CalibrationController.h"
#include "core/OcMatrix.h"
#include "core/IoBoardHl2.h"
#include "core/IoBoardHl2Facade.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/StationDevicesFacade.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/accessories/AlexController.h"
#include "gui/setup/HardwarePage.h"
#include "gui/widgets/FilterPolicyDialog.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/TunerModel.h"
#include "core/TgxlConnection.h"
#include "core/SmartSdrApiListener.h"

#include "fakes/LoopbackTransport.h"
#include "OperatorWording.h"
#include "SessionWait.h"
#include "TestFunctionGroups.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

constexpr const char* kTlsSkipPrefix =
    "Qt reports no working TLS backend, so a wss listener cannot be created. "
    "Active Qt TLS backend: ";

QString tlsSkipMessage()
{
    QString backend = QSslSocket::activeBackend();
    if (backend.isEmpty()) {
        backend = QStringLiteral("<none>");
    }
    QString message = QString::fromLatin1(kTlsSkipPrefix) + backend;
    const QString diagnostic = CertificateStore::tlsBackendDiagnostic();
    if (!diagnostic.isEmpty()) {
        message += QStringLiteral(". ") + diagnostic;
    }
    return message;
}

/// A daemon-side RadioModel that reports Connected against a real board,
/// without a socket. Same seams every other RadioModel-level test in this
/// suite uses; connectToRadio() is unusable from a test (it blocks on cold
/// FFTW wisdom generation for minutes -- see DaemonApp.h's own note).
std::unique_ptr<RadioModel> makeStationRadioModel(int extraSlices)
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    for (int i = 0; i < extraSlices; ++i) {
        model->addSlice(QStringLiteral("pan-0"));
    }
    return model;
}

/// makeStationRadioModel() reports Connected but builds no WDSP channels, so
/// the model's own SliceMeterPump writes the no-reading value to every slice
/// on each poll (R-R3-13, SliceMeterPump::poll's no-channel branch). A test
/// that feeds slice readings by hand stops it first so the value it sets is
/// the value the mirror carries.
void stopSliceMeterPump(RadioModel* model)
{
    SliceMeterPump* pump = model->sliceMeterPump();
    QVERIFY(pump != nullptr);
    pump->stop();
}

SessionMessage decodeOrFail(const QByteArray& wire)
{
    SessionMessage message;
    if (!SessionMessages::decode(wire, &message)) {
        return SessionMessage{};
    }
    return message;
}

/// Index of the first message of `kind` in a recorded stream, or -1.
int indexOfKind(const QList<QByteArray>& kinds, const char* name)
{
    return static_cast<int>(kinds.indexOf(QByteArray(name)));
}

/// Delivers synchronously only where this regression needs to preempt a
/// session before StationServer's already-queued radio callback runs. The
/// ordinary handshake tests keep using LoopbackTransport's realistic queued
/// delivery; this narrow fixture exercises the server's reentrancy guard.
class ImmediateTransport final : public SessionTransport {
public:
    explicit ImmediateTransport(const QString& description, QObject* parent = nullptr)
        : SessionTransport(parent)
        , m_description(description)
    {
    }

    void linkTo(ImmediateTransport* peer)
    {
        m_peer = peer;
        if (peer != nullptr) {
            peer->m_peer = this;
        }
    }

    void sendText(const QByteArray& wire) override
    {
        if (!m_open || m_peer == nullptr || !m_peer->m_open) {
            return;
        }
        m_peer->m_received.append(wire);
        emit m_peer->textReceived(wire);
    }

    void ping() override
    {
        if (m_open && m_peer != nullptr && m_peer->m_open) {
            emit pongReceived();
        }
    }

    void closeLink(const QString& reason) override
    {
        if (!m_open) {
            return;
        }
        m_open = false;
        emit closed();
        if (m_peer != nullptr) {
            m_peer->closeLink(reason);
        }
    }

    bool isOpen() const override { return m_open; }
    QString peerDescription() const override { return m_description; }

    QList<QByteArray> receivedKinds() const
    {
        QList<QByteArray> kinds;
        kinds.reserve(m_received.size());
        for (const QByteArray& wire : m_received) {
            const QJsonDocument document = QJsonDocument::fromJson(wire);
            kinds.append(document.object().value(QStringLiteral("type")).toString().toUtf8());
        }
        return kinds;
    }

private:
    QString m_description;
    ImmediateTransport* m_peer = nullptr;
    bool m_open = true;
    QList<QByteArray> m_received;
};

// Holds queued output until the test explicitly completes the close. This
// models a busy socket without relying on scheduler timing or a real network.
class DrainingTransport final : public SessionTransport {
public:
    bool immediate = false;
    bool closing = false;
    QList<QByteArray> pending;
    void sendText(const QByteArray& wire) override { pending.append(wire); }
    void ping() override {}
    void closeLink(const QString&) override
    {
        closing = true;
        if (immediate) { emit closed(); }
    }
    bool isOpen() const override { return !closing; }
    QString peerDescription() const override { return QStringLiteral("draining-test"); }
    void finish() { emit closed(); }
};

// ── Capturing whatever reaches the Qt logging handler ────────────────────
//
// Production installs CoreInit's handler, which redacts and then writes to
// BOTH stderr and a log file kept indefinitely. A test cannot assert on
// that file without running CoreInit::initialize(), so it asserts one step
// earlier instead, on what is handed to the handler at all: anything that
// arrives here in production reaches the log file.

QStringList* g_capturedLines = nullptr;

void capturingMessageHandler(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    if (g_capturedLines != nullptr) {
        g_capturedLines->append(msg);
    }
}

/// RAII, because every assertion in a QtTest slot is a bare `return`: a
/// hand-rolled install/restore pair would leave this handler installed for
/// the rest of the binary on the first failure.
class LogCapture {
public:
    explicit LogCapture(QStringList* sink)
    {
        g_capturedLines = sink;
        m_previous = qInstallMessageHandler(&capturingMessageHandler);
    }
    ~LogCapture()
    {
        qInstallMessageHandler(m_previous);
        g_capturedLines = nullptr;
    }
    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

private:
    QtMessageHandler m_previous = nullptr;
};

} // namespace

class TstStationSession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void finalMessagesSurviveUntilClose_data();
    void finalMessagesSurviveUntilClose();

    // ---- TokenStore (task 18 step 3) ----
    void tokenIsGeneratedNotChosenAndPersists();
    void tokenVerifyIsRateLimitedAfterRepeatedFailures();
    void tokenFailuresFromOneSourceDoNotLimitAnother();

    // ---- The connect sequence, over a non-TLS in-process link ----
    void handshakeCompletesInSectionSevenZeroOrder();
    void capabilitiesAdvertiseEffectiveNotBoardLimits();
    void clientAppliesCapabilitiesAndDrivesConnected();
    void lateRadioRefreshUpdatesAuthenticatedClientWithoutReplayingMirror();
    void queuedLateRadioRefreshReachesOnlyTheSessionsAdmittedBeforeIt();
    void mediaEnvelopeIsBoundedAndTyped();
    void mediaRejectsPreAuthenticationAndOldProtocol();
    void mediaRequiresReadySessionAndRejectsPriorEpoch();
    void telemetryRequiresReadySessionAndRejectsPriorEpoch();
    void telemetryDoesNotRequireMediaAndRejectsOldProtocol();
    void telemetryClientWaitsForCapabilityAndSnapshot();
    void hostTelemetryReachesVersionTwoPeer();
    void hostTelemetryIsOmittedForMinorNinePeer();
    void clientKeepsHostTelemetryOnlyWhenNegotiated_data();
    void clientKeepsHostTelemetryOnlyWhenNegotiated();
    void receiverLoadReachesVersionThreePeer();
    void receiverLoadIsOmittedForMinorTenPeer();
    void clientKeepsReceiverLoadOnlyWhenNegotiated_data();
    void clientKeepsReceiverLoadOnlyWhenNegotiated();
    void radioStatusIsOmittedForMinorTenPeer();
    void clientKeepsRadioStatusOnlyWhenNegotiated_data();
    void clientKeepsRadioStatusOnlyWhenNegotiated();
    void clientKeepsRadioDiagnosticsOnlyWhenNegotiated_data();
    void clientKeepsRadioDiagnosticsOnlyWhenNegotiated();
    void nnrLimitReachesMinorElevenPeerAndTryAgainClearsIt();
    void featureGatesNameRealProperties();
    void remoteWindowSeesNoSchemaSkewFromTheCurrentCore();
    void nnrLimitIsOmittedForMinorTenPeer();
    void minorTenWriteThatClearsTheLimitCarriesNoNnrLimit();
    void minorTenPeerReadsWhyInNnrStatus();
    void clientSendsTryAgainOnlyAtMinorEleven();
    void remoteTgxlClientRequiresHandshakeMinorAndCapability();
    void remoteFourO3AClientRequiresHandshakeMinorAndCapability();
    void remoteFourO3AServerRejectsPreAuthAndOldMinor();
    void remoteFourO3AAuthenticatedRoundTripMirrorsActualListenerState();
    void remoteFourO3AUnansweredCommandDoesNotSurviveSession_data();
    void remoteFourO3AUnansweredCommandDoesNotSurviveSession();
    void remoteTgxlCommandIsGatedAtAuthenticatedServerBoundary();
    void remoteTgxlConfigureAcceptanceStartsIdentityOnly();

    // ---- Version policy (task 18 step 1) ----
    void majorVersionMismatchRefusesNamingBothVersions();
    void minorVersionMismatchNegotiatesDown();

    // ---- Authentication (task 18 steps 1 and 3) ----
    void badTokenIsRefusedAndThenRateLimited();

    // ---- Session model (task 18 step 1) ----
    void secondAuthenticatedConnectionIsAdmittedBesideTheFirst();

    // ---- Heartbeat (task 18 step 2a) ----
    void heartbeatDetectsAPeerThatWentSilentWithoutClosing();
    void heartbeatLeavesAnAnsweringPeerAlone();

    // ---- Mirror and settings wiring (task 18 steps 7 and 8) ----
    void mirrorRoundTripsSliceStateAndDoesNotEcho();
    void filterTelemetryFollowsCoreAcrossReconnect();
    void autoAgcTelemetryFollowsCoreAcrossReconnect();
    void settingsProxyIsNotReadyBeforeTheSnapshot();
    void aRemovedStationSettingReachesTheClientAsAbsenceNotAnEmptyString();
    void schemaSkewIsCaughtByNameComparison();
    void receiveOnlyStationBlocksRemoteBandRecall();
    void receiveOnlyStationRefusesTransmitKeyingWrites();
    void receiveOnlyStationTakesTransmitDspOptionsSettingsWrites();
    void receiveOnlyStationTakesTransmitDspOptionsSettingsRemoves();
    void acceptedReceiveDspOptionsWriteAppliesToMatchingSlices();
    void receiveOnlyPolicySurvivesRadioTeardown();
    void nr3ModelChoiceLoadsOnceOnTheCoreAndMirrors();
    void nr3CannotRunIsRefusedOnTheCoreAndInTheWindow();
    void dfnrCannotRunIsRefusedOnTheCoreAndInTheWindow();
    void mnrCannotRunIsRefusedOnTheCoreAndInTheWindow();
    void bnrIsRefusedOnTheCoreAndInTheWindow();
#ifdef HAVE_DFNR
    void dfnrFailingAtFirstSelectionTurnsTheWindowOff();
#endif
    void savedNr3OnACoreWithNoModelShowsOffInTheWindow();
    void nr3CannotRunEndsWithTheSession();
    void olderCoreLeavesTheNr3ModelUnchangeable();
    void olderAppNr3ModelPathWriteIsRefused();
    void remoteNotchEditKeepsTheCoresWholeList();
    void remoteNotchMoveToggleAndDeleteReachTheCore();
    void remoteNotchRefusalsAreInPlainWords();
    void coreNotchChangesReachTheWindow();
    void appNotchSettingsWritesAreRefused();
    void olderCoreKeepsTodaysNotchBehaviour();
    void olderAppIgnoresTheNotchesObjectGolden();

    // ---- R-R3-46: the window knows the Core's radio ----
    void radioIdentityEntriesRoundTrip();
    void coreSendsRadioIdentityOnlyFromMinorEleven();
    void remoteModelResolvesTheCoresRadio();
    void remoteModelSignalsOncePerIdentityChange();

    // ---- R-R3-46 / R-R3-11: the Core's attenuator and preamp (stepAtt) ----
    void coreOffersTheAttenuatorOnlyFromMinorEleven();
    void appStepAttenuatorSettingsWritesAreRefused();
    void windowAttenuatorEditsWaitForACoreThatOffersThem();

    // ---- R-R3-46: Hardware Config through the Core (radioHardwareVersion 2) ----
    void windowAntennaEditsReachTheCoresController();
    void windowAntennaEditsWaitForACoreThatOffersThem();
    void appRawAntennaSettingsWritesAreRefused();
    void hardwareWritesForAnotherRadioAreRefused();
    void coreAppliesHardwareConfigWritesLive();
    void receiveOnlyCoreRefusesTransmitHardwareKeys();
    void coreAppliesHl2ClockWritesLive();
    void ioBoardProbeIsAskedOfTheCore();
    void windowBandAntennaEditKeepsTheCoresNewerBands();
    void windowTxBandAntennaEditKeepsTheCoresNewerBands();
    void windowUsesBoundAntennaVerbAndCurrentMacWhenOffered();
    void windowShowsTheCoresIoBoard();
    void windowForgetsTheIoBoardOfACoreThatDoesNotOfferIt();
    void windowOcMatrixFollowsTheCore();
    void hardwareConfigRateGoesToEveryReceiver();
    // R-R3-46 / R-R3-21 (radioHardwareVersion 4): the filter policy dialog.
    void windowFilterPolicyReachesTheCore();
    void windowFilterPolicyWaitsForACoreThatOffersIt();
    void windowFilterPolicyApplySendsWhatIsShown();
    void coreTakesTheFilterPolicyOnlyFromMinorElevenAtVersionFour();

    // ---- Fix round 1 ----
    void reconnectSurvivesTheOldTransportClosing();
    void heartbeatTimeoutReportsTheSessionAsEnded();
    void tunerPropertiesHydrateWithoutClientCommands();
    void remoteTgxlStateClearsOnSessionLossRetainingConfiguredEndpoint();
    void handshakeDeadlineDropsASilentPeer();
    void peerLimitRefusesFurtherConnections();
    void oneAddressHoldsAtMostTwoConnectingSlots();
    void ipv6PeersAreCountedPerSlash64();
    void listenIsIdempotent();

    // ---- Security fix round ----
    void firstRunPairingBannerNeverReachesTheLoggingHandler();
    void oversizedMessageIsRefusedBeforeAnyAuthentication();
    void clientCapsWhatAStationCanMakeItAllocate();
    void wsSchemeIsRefusedWhenAFingerprintIsPinned();
    void tokenIsNeverSentOnALinkWhosePinWasNeverChecked();
    void transientRefusalsStayRetryableAndABadTokenDoesNot();
    void lockedOutOperatorRetriesButABadTokenDoesNot();

    // ---- TLS-specific (QSKIP when the backend is unusable) ----
    void wssListenerComesUpAndCompletesAHandshake();
    void wssRefusesAMismatchedCertificateFingerprint();
    void failedInitialConnectReportsPromptly();

    // ---- iPhone app Task 18 (R-IOS-08, R-IOS-17): the desktop's own key,
    // identity trust and the end codes. Refusals first. ----
    void pairedCoreShowingAnotherIdentityIsRefused();
    void pairedCoreShowingNoIdentityIsRefused();
    void certificateWithoutAValidBindingIsRefused();
    void changedCertificateWhoseBindingVerifiesIsAccepted();
    void helloDeclaresDeviceAuthOnlyWithAKey();
    void coreWithNoIdentityGetsTheTokenAlone();
    void tokenSignInEnrolsTheKeyThenSignsInByKey();
    void endCodesChooseTheReport();
    void revokedDeviceIsEndedWithDeviceRemoved();
    void retiredTokenIsRefusedWithPairingRequired();

private:
    /// One temp dir for the whole class so the RSA-3072 key pair is
    /// generated once and every later StationServer loads it back, rather
    /// than paying key generation per slot.
    QTemporaryDir m_securityDir;
};

void TstStationSession::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    // TGXL admission persists through the process settings singleton. The
    // generic Qt test sandbox is shared by parallel test executables, so a
    // different GUI test can replace that file between our save and reload.
    // Follow the receive-layout session fixture's process-specific profile.
    const QString profile = QStringLiteral("station-session-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
    AppSettings::instance().clear();
}

void TstStationSession::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstStationSession::finalMessagesSurviveUntilClose_data()
{
    QTest::addColumn<bool>("immediate");
    QTest::addColumn<bool>("destroyServer");
    QTest::newRow("immediate") << true << false;
    QTest::newRow("delayed") << false << false;
    QTest::newRow("server-destroyed") << false << true;
}

void TstStationSession::finalMessagesSurviveUntilClose()
{
    QFETCH(bool, immediate);
    QFETCH(bool, destroyServer);
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("drain.settings")));
    auto model = makeStationRadioModel(0);
    auto server = std::make_unique<StationServer>(model.get(), settings,
        NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QPointer<DrainingTransport> transport = new DrainingTransport;
    transport->immediate = immediate;
    server->acceptTransport(transport);
    server->close();
    QVERIFY(transport && transport->closing);
    if (destroyServer) { server.reset(); }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (!immediate) {
        QVERIFY2(transport, "Connection deleted before queued final output drained");
        QVERIFY(!transport->pending.isEmpty());
        const QJsonObject last = QJsonDocument::fromJson(transport->pending.last()).object();
        QCOMPARE(last.value(QStringLiteral("type")).toString(), QStringLiteral("session.end"));
        transport->finish();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QVERIFY2(transport.isNull(), "Completed close must release connection promptly");
}

void TstStationSession::mediaEnvelopeIsBoundedAndTyped()
{
    SessionMessage message;
    message.kind = SessionMessageKind::MediaControl;
    message.mediaPayload = {{QStringLiteral("op"), QStringLiteral("start")}};
    SessionMessage decoded;
    QVERIFY(SessionMessages::decode(SessionMessages::encode(message), &decoded));
    QCOMPARE(decoded.kind, SessionMessageKind::MediaControl);
    QCOMPARE(decoded.mediaPayload, message.mediaPayload);
    QVERIFY(!SessionMessages::decode(
        QByteArrayLiteral("{\"type\":\"media.control\",\"payload\":[]}"), &decoded));
    QCOMPARE(decoded.mediaPayload, message.mediaPayload);
    message.mediaPayload.insert(QStringLiteral("sdp"),
                                QString(kMaxMediaControlBytes, QLatin1Char('x')));
    QVERIFY(SessionMessages::encode(message).isEmpty());
    const QByteArray oversized = QJsonDocument(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("media.control")},
        {QStringLiteral("payload"), message.mediaPayload}}).toJson(QJsonDocument::Compact);
    QVERIFY(!SessionMessages::decode(oversized, &decoded));
}

void TstStationSession::mediaRejectsPreAuthenticationAndOldProtocol()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("media.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setMediaEnabled(true);
    QSignalSpy inbound(&server, &StationServer::mediaControlReceived);
    SessionMessage media;
    media.kind = SessionMessageKind::MediaControl;
    media.mediaPayload = {{QStringLiteral("op"), QStringLiteral("start")}};

    auto* unauthStation = new LoopbackTransport(QStringLiteral("unauth-station"), this);
    auto* unauthPeer = new LoopbackTransport(QStringLiteral("unauth-peer"), this);
    unauthStation->linkTo(unauthPeer);
    server.acceptTransport(unauthStation);
    unauthPeer->sendText(SessionMessages::encode(media));
    NEREUS_TRY_VERIFY(!unauthPeer->isOpen());
    QCOMPARE(inbound.count(), 0);

    auto* oldStation = new LoopbackTransport(QStringLiteral("old-station"), this);
    auto* oldPeer = new LoopbackTransport(QStringLiteral("old-peer"), this);
    oldStation->linkTo(oldPeer);
    server.acceptTransport(oldStation);
    oldPeer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, 0, 6, QStringLiteral("old-client"))));
    oldPeer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    QVERIFY(!server.mediaAvailable());
    QVERIFY(!server.sendMediaControl(media.mediaPayload, server.mediaSessionEpoch()));
    oldPeer->sendText(SessionMessages::encode(media));
    QCoreApplication::processEvents();
    QCOMPARE(inbound.count(), 0);
    QVERIFY(oldPeer->isOpen());
}

void TstStationSession::mediaRequiresReadySessionAndRejectsPriorEpoch()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("media.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setMediaEnabled(true);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    const QJsonObject payload{{QStringLiteral("op"), QStringLiteral("start")}};
    QSignalSpy serverInbound(&server, &StationServer::mediaControlReceived);
    QSignalSpy clientInbound(&client, &StationClient::mediaControlReceived);
    QSignalSpy ended(&server, &StationServer::mediaSessionEnded);
    QVERIFY(!client.sendMediaControl(payload, client.sessionEpoch()));
    QVERIFY(!server.sendMediaControl(payload, server.mediaSessionEpoch()));

    auto connectPair = [&] {
        auto* station = new LoopbackTransport(QStringLiteral("media-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("media-client"), this);
        station->linkTo(peer);
        client.startSession(peer, server.token());
        server.acceptTransport(station);
    };
    connectPair();
    NEREUS_TRY_VERIFY(client.mediaAvailable());
    QVERIFY(server.mediaAvailable());
    const quint32 firstClientEpoch = client.sessionEpoch();
    const quint64 firstServerEpoch = server.mediaSessionEpoch();
    QVERIFY(client.sendMediaControl(payload, firstClientEpoch));
    QVERIFY(server.sendMediaControl(payload, firstServerEpoch));
    NEREUS_TRY_COMPARE(serverInbound.count(), 1);
    NEREUS_TRY_COMPARE(clientInbound.count(), 1);
    QCOMPARE(serverInbound.first().at(1).toULongLong(), firstServerEpoch);
    QCOMPARE(clientInbound.first().at(1).toUInt(), firstClientEpoch);

    connectPair();
    NEREUS_TRY_VERIFY(client.mediaAvailable());
    QVERIFY(client.sessionEpoch() != firstClientEpoch);
    QVERIFY(server.mediaSessionEpoch() != firstServerEpoch);
    QVERIFY(!ended.isEmpty());
    QVERIFY(!client.sendMediaControl(payload, firstClientEpoch));
    QVERIFY(!server.sendMediaControl(payload, firstServerEpoch));
    QVERIFY(client.sendMediaControl(payload, client.sessionEpoch()));
    NEREUS_TRY_COMPARE(serverInbound.count(), 2);
    QCOMPARE(clientInbound.count(), 1);
    client.disconnectFromStation(QStringLiteral("media test complete"));
    NEREUS_TRY_VERIFY(!server.mediaAvailable());
    QVERIFY(!client.sendMediaControl(payload, client.sessionEpoch()));
}

void TstStationSession::telemetryRequiresReadySessionAndRejectsPriorEpoch()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("telemetry.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    QSignalSpy ended(&server, &StationServer::telemetrySessionEnded);
    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 1;
    QVERIFY(!server.sendTelemetry(snapshot, server.sessionEpoch()));
    QVERIFY(!client.telemetryAvailable());
    auto connectPair = [&] {
        auto* station = new LoopbackTransport(QStringLiteral("metrics-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("metrics-client"), this);
        station->linkTo(peer);
        client.startSession(peer, server.token());
        server.acceptTransport(station);
    };
    connectPair();
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    const quint64 oldServerEpoch = server.sessionEpoch();
    const quint32 oldClientEpoch = client.sessionEpoch();
    QVERIFY(server.sendTelemetry(snapshot, oldServerEpoch));
    NEREUS_TRY_COMPARE(samples.count(), 1);
    QCOMPARE(samples.first().at(1).toUInt(), oldClientEpoch);
    QVERIFY(server.sendTelemetry(snapshot, oldServerEpoch)); // duplicate ignored
    snapshot.sequence = 2;
    snapshot.sampledElapsedMs = 1000;
    QVERIFY(server.sendTelemetry(snapshot, oldServerEpoch));
    NEREUS_TRY_COMPARE(samples.count(), 2);
    QCOMPARE(qvariant_cast<StationTelemetrySnapshot>(samples.last().at(0)).sequence, 2u);
    snapshot.sequence = 3;
    snapshot.sampledElapsedMs = 500; // a regressing producer sample is ignored
    QVERIFY(server.sendTelemetry(snapshot, oldServerEpoch));
    connectPair();
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    QVERIFY(server.sessionEpoch() != oldServerEpoch);
    QVERIFY(client.sessionEpoch() != oldClientEpoch);
    QVERIFY(!ended.isEmpty());
    QVERIFY(!server.sendTelemetry(snapshot, oldServerEpoch));
    snapshot.sequence = 1; // new epoch establishes a new sequence baseline
    snapshot.sampledElapsedMs = 0;
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    NEREUS_TRY_COMPARE(samples.count(), 3);
    QCOMPARE(samples.last().at(1).toUInt(), client.sessionEpoch());
    client.disconnectFromStation(QStringLiteral("telemetry complete"));
    NEREUS_TRY_VERIFY(!server.telemetryAvailable());
    QVERIFY(!client.telemetryAvailable());
    QVERIFY(!server.sendTelemetry(snapshot, server.sessionEpoch()));
}

void TstStationSession::telemetryDoesNotRequireMediaAndRejectsOldProtocol()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("telemetry-old.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    QVERIFY(!server.mediaAvailable());
    auto* station = new LoopbackTransport(QStringLiteral("old-metrics-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("old-metrics-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 1;
    QVERIFY(!server.sendTelemetry(snapshot, server.sessionEpoch()));
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kStationTelemetrySessionProtocolMinor - 1, 6,
        QStringLiteral("older-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    QVERIFY(!server.telemetryAvailable());
    QVERIFY(!server.sendTelemetry(snapshot, server.sessionEpoch()));
    QVERIFY(peer->isOpen());
    // iPhone app Task 71: telemetry goes to one session until each device
    // has its own (Task 76), the first admitted while none holds it; the
    // older client leaves first so the newer one is that session.
    peer->closeLink(QStringLiteral("older client done"));
    NEREUS_TRY_VERIFY(!server.hasAuthenticatedSession());

    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    auto* newerStation = new LoopbackTransport(QStringLiteral("new-metrics-station"), this);
    auto* newerPeer = new LoopbackTransport(QStringLiteral("new-metrics-client"), this);
    newerStation->linkTo(newerPeer);
    client.startSession(newerPeer, server.token());
    server.acceptTransport(newerStation);
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    QVERIFY(server.telemetryAvailable());
    QVERIFY(!server.mediaAvailable());
    QVERIFY(!client.mediaAvailable());
}

void TstStationSession::telemetryClientWaitsForCapabilityAndSnapshot()
{
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("raw-metrics-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-metrics-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    SessionMessage sample;
    sample.kind = SessionMessageKind::StationTelemetry;
    sample.telemetry.sequence = 1;
    const auto send = [&](const SessionMessage& message) {
        station->sendText(SessionMessages::encode(message));
    };
    send(sample); // before hello/authentication
    send(SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 6,
                                QStringLiteral("station")));
    send(SessionMessages::authResult(true, {}, false));
    StationCapabilities caps;
    caps.stationTelemetryVersion = 1;
    send(SessionMessages::capabilities(caps.toUpdates()));
    send(sample); // capability present, but snapshot not ready
    NEREUS_TRY_VERIFY(!peer->receivedKinds().isEmpty());
    QCoreApplication::processEvents();
    QCOMPARE(samples.count(), 0);
    QVERIFY(!client.telemetryAvailable());
    send(SessionMessages::snapshotComplete());
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    send(sample);
    NEREUS_TRY_COMPARE(samples.count(), 1);
}

namespace {
StationHostTelemetry hostSample()
{
    StationHostTelemetry host;
    host.systemCpuPercent = 23.5;
    host.processCpuPercent = 4.25;
    host.memoryAvailableKiB = 6500000;
    host.memoryTotalKiB = 8000000;
    host.processResidentKiB = 51234;
    host.hottestZoneCelsius = 52.5;
    host.hottestZoneName = QStringLiteral("bigcore0-thermal");
    return host;
}

QVector<StationReceiverTelemetry> receiverSample()
{
    StationReceiverTelemetry a;
    a.sliceId = 0;
    a.loadPercent = 37.5;
    a.inputDelayMs = 4;
    a.skippedInputMs = 0;
    StationReceiverTelemetry b;
    b.sliceId = 1;
    b.inputDelayMs = 0; // processed nothing this interval: load absent
    b.skippedInputMs = 250;
    return {a, b};
}
} // namespace

// R-R3-32/33: a current GUI and Core negotiate the current minor and
// telemetry version 3, and the host section arrives intact.
void TstStationSession::hostTelemetryReachesVersionTwoPeer()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("host-telemetry.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    QCOMPARE(server.buildCapabilities().stationTelemetryVersion, 6);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("host-metrics-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("host-metrics-client"), this);
    station->linkTo(peer);
    client.startSession(peer, server.token());
    server.acceptTransport(station);
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    QCOMPARE(client.agreedMinor(), kSessionProtocolMinor);

    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 1;
    snapshot.host = hostSample();
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    NEREUS_TRY_COMPARE(samples.count(), 1);
    const auto received = qvariant_cast<StationTelemetrySnapshot>(samples.first().at(0));
    QCOMPARE(received.host.systemCpuPercent, std::optional<double>(23.5));
    QCOMPARE(received.host.processCpuPercent, std::optional<double>(4.25));
    QCOMPARE(received.host.memoryAvailableKiB, std::optional<qint64>(6500000));
    QCOMPARE(received.host.memoryTotalKiB, std::optional<qint64>(8000000));
    QCOMPARE(received.host.processResidentKiB, std::optional<qint64>(51234));
    QCOMPARE(received.host.hottestZoneCelsius, std::optional<double>(52.5));
    QCOMPARE(received.host.hottestZoneName, QStringLiteral("bigcore0-thermal"));
    client.disconnectFromStation(QStringLiteral("host telemetry complete"));
}

// A minor-9 GUI receives exactly today's telemetry: the same bytes a Core
// without host telemetry would have sent.
void TstStationSession::hostTelemetryIsOmittedForMinorNinePeer()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("host-telemetry-old.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    auto* station = new LoopbackTransport(QStringLiteral("minor9-metrics-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("minor9-metrics-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kCoreHostTelemetrySessionProtocolMinor - 1, 6,
        QStringLiteral("minor-9-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.telemetryAvailable());
    peer->clearReceived();

    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 2;
    snapshot.sampledElapsedMs = 1000;
    snapshot.host = hostSample();
    snapshot.receivers = receiverSample();
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    QByteArray wire;
    NEREUS_TRY_VERIFY([&] {
        for (const QByteArray& message : peer->received()) {
            if (QJsonDocument::fromJson(message).object().value(QStringLiteral("type"))
                    == QStringLiteral("station.metrics.v1")) {
                wire = message;
                return true;
            }
        }
        return false;
    }());
    const QByteArray golden =
        R"({"payload":{"audio":{"active":false,"contextGeneration":0},)"
        R"("radio":{"connected":false},"sampledElapsedMs":1000,"sequence":2},)"
        R"("type":"station.metrics.v1"})";
    QCOMPARE(wire, golden);
    SessionMessage withoutHost;
    withoutHost.kind = SessionMessageKind::StationTelemetry;
    withoutHost.telemetry = snapshot;
    withoutHost.telemetry.host = {};
    withoutHost.telemetry.receivers.reset();
    QCOMPARE(wire, SessionMessages::encode(withoutHost));
}

// The GUI accepts a host section only from a Core that negotiated both the
// minor and telemetry version 2; anything else is delivered without it.
void TstStationSession::clientKeepsHostTelemetryOnlyWhenNegotiated_data()
{
    QTest::addColumn<int>("minor");
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("kept");
    QTest::newRow("minor 10, version 2")
        << int(kCoreHostTelemetrySessionProtocolMinor) << 2 << true;
    QTest::newRow("minor 10, version 1")
        << int(kCoreHostTelemetrySessionProtocolMinor) << 1 << false;
    QTest::newRow("minor 9, version 2")
        << int(kCoreHostTelemetrySessionProtocolMinor - 1) << 2 << false;
}

void TstStationSession::clientKeepsHostTelemetryOnlyWhenNegotiated()
{
    QFETCH(int, minor);
    QFETCH(int, version);
    QFETCH(bool, kept);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("raw-host-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-host-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    const auto send = [&](const SessionMessage& message) {
        station->sendText(SessionMessages::encode(message));
    };
    send(SessionMessages::hello(kSessionProtocolMajor, static_cast<quint16>(minor), 6,
                                QStringLiteral("station")));
    send(SessionMessages::authResult(true, {}, false));
    StationCapabilities caps;
    caps.stationTelemetryVersion = version;
    send(SessionMessages::capabilities(caps.toUpdates()));
    send(SessionMessages::snapshotComplete());
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    SessionMessage sample;
    sample.kind = SessionMessageKind::StationTelemetry;
    sample.telemetry.sequence = 1;
    sample.telemetry.host = hostSample();
    send(sample);
    NEREUS_TRY_COMPARE(samples.count(), 1);
    const auto received = qvariant_cast<StationTelemetrySnapshot>(samples.first().at(0));
    QCOMPARE(!received.host.isEmpty(), kept);
    QCOMPARE(received.host.hottestZoneName.isEmpty(), !kept);
}

// R-R3-40: a current GUI and Core negotiate minor 11 and telemetry version
// 3, and each receiver's load arrives intact next to the host section.
void TstStationSession::receiverLoadReachesVersionThreePeer()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("receiver-load.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    QCOMPARE(server.buildCapabilities().stationTelemetryVersion, 6);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("receiver-load-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("receiver-load-client"), this);
    station->linkTo(peer);
    client.startSession(peer, server.token());
    server.acceptTransport(station);
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    QCOMPARE(kSessionProtocolMinor, kReceiverLoadSessionProtocolMinor);
    QCOMPARE(client.agreedMinor(), kReceiverLoadSessionProtocolMinor);

    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 1;
    snapshot.host = hostSample();
    snapshot.receivers = receiverSample();
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    NEREUS_TRY_COMPARE(samples.count(), 1);
    const auto received = qvariant_cast<StationTelemetrySnapshot>(samples.first().at(0));
    QVERIFY(received.receivers);
    QCOMPARE(received.receivers->size(), 2);
    QCOMPARE(received.receivers->at(0).sliceId, 0);
    QCOMPARE(received.receivers->at(0).loadPercent, std::optional<double>(37.5));
    QCOMPARE(received.receivers->at(0).inputDelayMs, 4LL);
    QCOMPARE(received.receivers->at(1).sliceId, 1);
    QVERIFY(!received.receivers->at(1).loadPercent);
    QCOMPARE(received.receivers->at(1).skippedInputMs, 250LL);
    QCOMPARE(received.host.hottestZoneName, QStringLiteral("bigcore0-thermal"));

    // Measured with no receiver reading yet stays distinct from absent.
    snapshot.sequence = 2;
    snapshot.receivers = QVector<StationReceiverTelemetry>{};
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    NEREUS_TRY_COMPARE(samples.count(), 2);
    const auto empty = qvariant_cast<StationTelemetrySnapshot>(samples.last().at(0));
    QVERIFY(empty.receivers);
    QVERIFY(empty.receivers->isEmpty());
    client.disconnectFromStation(QStringLiteral("receiver load complete"));
}

// A minor-10 GUI receives exactly the telemetry a minor-10 Core sends:
// radio, audio and host, and no receivers section.
void TstStationSession::receiverLoadIsOmittedForMinorTenPeer()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("receiver-load-old.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    auto* station = new LoopbackTransport(QStringLiteral("minor10-metrics-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("minor10-metrics-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kReceiverLoadSessionProtocolMinor - 1, 6,
        QStringLiteral("minor-10-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.telemetryAvailable());
    peer->clearReceived();

    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 3;
    snapshot.sampledElapsedMs = 2000;
    snapshot.host.systemCpuPercent = 23.5;
    snapshot.host.memoryTotalKiB = 8000000;
    snapshot.receivers = receiverSample();
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    QByteArray wire;
    NEREUS_TRY_VERIFY([&] {
        for (const QByteArray& message : peer->received()) {
            if (QJsonDocument::fromJson(message).object().value(QStringLiteral("type"))
                    == QStringLiteral("station.metrics.v1")) {
                wire = message;
                return true;
            }
        }
        return false;
    }());
    // Today's minor-10 wire, written out: the host section and nothing more.
    const QByteArray golden =
        R"({"payload":{"audio":{"active":false,"contextGeneration":0},)"
        R"("host":{"memoryTotalKiB":8000000,"systemCpuPercent":23.5},)"
        R"("radio":{"connected":false},"sampledElapsedMs":2000,"sequence":3},)"
        R"("type":"station.metrics.v1"})";
    QCOMPARE(wire, golden);
    SessionMessage withoutReceivers;
    withoutReceivers.kind = SessionMessageKind::StationTelemetry;
    withoutReceivers.telemetry = snapshot;
    withoutReceivers.telemetry.receivers.reset();
    QCOMPARE(wire, SessionMessages::encode(withoutReceivers));
}

// The GUI accepts receiver load only from a Core that negotiated both minor
// 11 and telemetry version 3; anything else is delivered without it.
void TstStationSession::clientKeepsReceiverLoadOnlyWhenNegotiated_data()
{
    QTest::addColumn<int>("minor");
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("kept");
    QTest::newRow("minor 11, version 3")
        << int(kReceiverLoadSessionProtocolMinor) << 3 << true;
    QTest::newRow("minor 11, version 2")
        << int(kReceiverLoadSessionProtocolMinor) << 2 << false;
    QTest::newRow("minor 10, version 3")
        << int(kReceiverLoadSessionProtocolMinor - 1) << 3 << false;
}

void TstStationSession::clientKeepsReceiverLoadOnlyWhenNegotiated()
{
    QFETCH(int, minor);
    QFETCH(int, version);
    QFETCH(bool, kept);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("raw-receivers-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-receivers-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    const auto send = [&](const SessionMessage& message) {
        station->sendText(SessionMessages::encode(message));
    };
    send(SessionMessages::hello(kSessionProtocolMajor, static_cast<quint16>(minor), 6,
                                QStringLiteral("station")));
    send(SessionMessages::authResult(true, {}, false));
    StationCapabilities caps;
    caps.stationTelemetryVersion = version;
    send(SessionMessages::capabilities(caps.toUpdates()));
    send(SessionMessages::snapshotComplete());
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    SessionMessage sample;
    sample.kind = SessionMessageKind::StationTelemetry;
    sample.telemetry.sequence = 1;
    sample.telemetry.host = hostSample();
    sample.telemetry.receivers = receiverSample();
    send(sample);
    NEREUS_TRY_COMPARE(samples.count(), 1);
    const auto received = qvariant_cast<StationTelemetrySnapshot>(samples.first().at(0));
    QCOMPARE(received.receivers.has_value(), kept);
    // The host section follows its own negotiation, untouched by this one.
    QCOMPARE(received.host.isEmpty(), version < 2);
}

// R-R3-32 (remote-window parity Task 6): a minor-10 GUI receives no PA
// readings or link quality in the radio section.
void TstStationSession::radioStatusIsOmittedForMinorTenPeer()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("radio-status-old.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setTelemetryEnabled(true);
    auto* station = new LoopbackTransport(QStringLiteral("minor10-radio-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("minor10-radio-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kReceiverLoadSessionProtocolMinor - 1, 6,
        QStringLiteral("minor-10-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.telemetryAvailable());
    peer->clearReceived();

    StationTelemetrySnapshot snapshot;
    snapshot.sequence = 3;
    snapshot.sampledElapsedMs = 2000;
    snapshot.radio.connected = true;
    snapshot.radio.rxMbps = 12.0;
    snapshot.radio.paVolts = 13.8;
    snapshot.radio.paTemperatureCelsius = 40.0;
    snapshot.radio.packetLossPercent = 1.0;
    snapshot.radio.udpPacketsSeen = 10;
    snapshot.radio.connectionAgeMs = 12345;
    snapshot.radio.radioUdpBasePort = 41024;
    snapshot.radio.adcOverloads = QVector<StationAdcOverloadTelemetry>{
        {0, 1, 100, true, 100}};
    QVERIFY(server.sendTelemetry(snapshot, server.sessionEpoch()));
    QByteArray wire;
    NEREUS_TRY_VERIFY([&] {
        for (const QByteArray& message : peer->received()) {
            if (QJsonDocument::fromJson(message).object().value(QStringLiteral("type"))
                    == QStringLiteral("station.metrics.v1")) {
                wire = message;
                return true;
            }
        }
        return false;
    }());
    const QByteArray golden =
        R"({"payload":{"audio":{"active":false,"contextGeneration":0},)"
        R"("radio":{"connected":true,"rxMbps":12},"sampledElapsedMs":2000,"sequence":3},)"
        R"("type":"station.metrics.v1"})";
    QCOMPARE(wire, golden);
}

// The GUI accepts the radio's PA readings and link quality only from a Core
// that negotiated both minor 11 and telemetry version 4.
void TstStationSession::clientKeepsRadioStatusOnlyWhenNegotiated_data()
{
    QTest::addColumn<int>("minor");
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("kept");
    QTest::newRow("minor 11, version 4")
        << int(kReceiverLoadSessionProtocolMinor) << 4 << true;
    QTest::newRow("minor 11, version 3")
        << int(kReceiverLoadSessionProtocolMinor) << 3 << false;
    QTest::newRow("minor 10, version 4")
        << int(kReceiverLoadSessionProtocolMinor - 1) << 4 << false;
}

void TstStationSession::clientKeepsRadioStatusOnlyWhenNegotiated()
{
    QFETCH(int, minor);
    QFETCH(int, version);
    QFETCH(bool, kept);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("raw-radio-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-radio-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    const auto send = [&](const SessionMessage& message) {
        station->sendText(SessionMessages::encode(message));
    };
    send(SessionMessages::hello(kSessionProtocolMajor, static_cast<quint16>(minor), 6,
                                QStringLiteral("station")));
    send(SessionMessages::authResult(true, {}, false));
    StationCapabilities caps;
    caps.stationTelemetryVersion = version;
    send(SessionMessages::capabilities(caps.toUpdates()));
    send(SessionMessages::snapshotComplete());
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    SessionMessage sample;
    sample.kind = SessionMessageKind::StationTelemetry;
    sample.telemetry.sequence = 1;
    sample.telemetry.radio.connected = true;
    sample.telemetry.radio.rxMbps = 3.0;
    sample.telemetry.radio.paVolts = 13.8;
    sample.telemetry.radio.jitterMs = 0.5;
    send(sample);
    NEREUS_TRY_COMPARE(samples.count(), 1);
    const auto received = qvariant_cast<StationTelemetrySnapshot>(samples.first().at(0));
    QCOMPARE(received.radio.paVolts.has_value(), kept);
    QCOMPARE(received.radio.jitterMs.has_value(), kept);
    QCOMPARE(received.radio.rxMbps, std::optional<double>(3.0));
}

void TstStationSession::clientKeepsRadioDiagnosticsOnlyWhenNegotiated_data()
{
    QTest::addColumn<int>("minor");
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("kept");
    QTest::newRow("minor 11, version 6")
        << int(kReceiverLoadSessionProtocolMinor) << 6 << true;
    QTest::newRow("minor 11, version 5")
        << int(kReceiverLoadSessionProtocolMinor) << 5 << false;
    QTest::newRow("minor 10, version 6")
        << int(kReceiverLoadSessionProtocolMinor - 1) << 6 << false;
}

void TstStationSession::clientKeepsRadioDiagnosticsOnlyWhenNegotiated()
{
    QFETCH(int, minor);
    QFETCH(int, version);
    QFETCH(bool, kept);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy samples(&client, &StationClient::telemetryReceived);
    auto* station = new LoopbackTransport(QStringLiteral("raw-diagnostics-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-diagnostics-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    const auto send = [&](const SessionMessage& message) {
        station->sendText(SessionMessages::encode(message));
    };
    send(SessionMessages::hello(kSessionProtocolMajor, static_cast<quint16>(minor), 6,
                                QStringLiteral("station")));
    send(SessionMessages::authResult(true, {}, false));
    StationCapabilities caps;
    caps.stationTelemetryVersion = version;
    send(SessionMessages::capabilities(caps.toUpdates()));
    send(SessionMessages::snapshotComplete());
    NEREUS_TRY_VERIFY(client.telemetryAvailable());
    SessionMessage sample;
    sample.kind = SessionMessageKind::StationTelemetry;
    sample.telemetry.sequence = 1;
    sample.telemetry.radio.connected = true;
    sample.telemetry.radio.connectionAgeMs = 12345;
    sample.telemetry.radio.radioUdpBasePort = 41024;
    sample.telemetry.radio.adcOverloads = QVector<StationAdcOverloadTelemetry>{
        {0, 1, 100, true, 100}};
    send(sample);
    NEREUS_TRY_COMPARE(samples.count(), 1);
    const StationRadioTelemetry& radio =
        qvariant_cast<StationTelemetrySnapshot>(samples.first().at(0)).radio;
    QCOMPARE(radio.connectionAgeMs.has_value(), kept);
    QCOMPARE(radio.radioUdpBasePort.has_value(), kept);
    QCOMPARE(radio.adcOverloads.has_value(), kept);
    if (kept) {
        QCOMPARE(radio.connectionAgeMs, std::optional<qint64>(12345));
        QCOMPARE(radio.radioUdpBasePort, std::optional<qint64>(41024));
        QCOMPARE(radio.adcOverloads->at(0).overloaded, std::optional<bool>(true));
    }
}

// R-R3-40: a current GUI sees the Core's runtime NNR step-back and asks for
// the saved choice back with nnr.tryAgain; a station echo of the model is
// never taken as the operator asking. No schema-skew or apply warnings.
void TstStationSession::nnrLimitReachesMinorElevenPeerAndTryAgainClearsIt()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("nnr-limit.settings")));
    auto model = makeStationRadioModel(0);
    QVERIFY(!model->slices().isEmpty());
    SliceModel* coreSlice = model->slices().first();
    const int sliceId = coreSlice->sliceIndex();
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QStringList lines;
    LogCapture capture(&lines);
    auto* station = new LoopbackTransport(QStringLiteral("nnr-limit-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("nnr-limit-client"), this);
    station->linkTo(peer);
    client.startSession(peer, server.token());
    server.acceptTransport(station);
    NEREUS_TRY_VERIFY(client.nnrRetryAvailable());
    QCOMPARE(client.agreedMinor(), kNnrLimitSessionProtocolMinor);
    NEREUS_TRY_VERIFY(remote.sliceById(sliceId) != nullptr);
    SliceModel* guiSlice = remote.sliceById(sliceId);
    QCOMPARE(guiSlice->nnrLimit(), 0);

    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));   // the Core stepped back
    NEREUS_TRY_COMPARE(guiSlice->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));
    // A remote window names the Core computer, never "this computer".
    const QString coreText = QStringLiteral(
        "Noise reduction is using the Standard model. The Core computer could not keep up with Premium.");
    QCOMPARE(guiSlice->nnrLimitText(), coreText);
    NEREUS_TRY_COMPARE(guiSlice->nnrStatus(), coreText);

    // The station changing the model is an echo, not the operator asking.
    NnrSettings standard = coreSlice->nnrSettings();
    standard.modelSlot = coreSlice->nnrModelSlot() == 1 ? 0 : 1;
    QVERIFY(coreSlice->applyNnrSettings(standard));
    NEREUS_TRY_COMPARE(guiSlice->nnrModelSlot(), standard.modelSlot);
    NereusSDR::Test::settleSession();
    QCOMPARE(coreSlice->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));

    guiSlice->requestNnrRetry();   // "Try again" on the remote GUI
    NEREUS_TRY_COMPARE(coreSlice->nnrLimit(), 0);
    NEREUS_TRY_COMPARE(guiSlice->nnrLimit(), 0);
    QVERIFY(guiSlice->nnrLastError().isEmpty());
    QCOMPARE(coreSlice->nnrModelSlot(), standard.modelSlot);   // saved choice unchanged
    for (const QString& line : std::as_const(lines)) {
        QVERIFY2(!line.contains(QStringLiteral("schema skew")), qPrintable(line));
        QVERIFY2(!(line.contains(QStringLiteral("No way to apply"))
                   && line.contains(QStringLiteral("nnr"))), qPrintable(line));
    }
    client.disconnectFromStation(QStringLiteral("nnr limit complete"));
}

// Every row of MirrorPolicy::featureGates names a property its class still
// declares, so the remote window's schema comparison skips a real name and a
// renamed property cannot leave a stale row behind.
void TstStationSession::featureGatesNameRealProperties()
{
    const QHash<QByteArray, const QMetaObject*> classes{
        { QByteArrayLiteral("TransmitModel"), &TransmitModel::staticMetaObject },
        { QByteArrayLiteral("SliceModel"), &SliceModel::staticMetaObject },
        { QByteArrayLiteral("RadioModel"), &RadioModel::staticMetaObject },
        { QByteArrayLiteral("StationDevicesFacade"), &StationDevicesFacade::staticMetaObject },
    };
    QVERIFY(!MirrorPolicy::featureGates().isEmpty());
    for (const MirrorPolicy::FeatureGate& gate : MirrorPolicy::featureGates()) {
        const QByteArray className(gate.className);
        QVERIFY2(classes.contains(className), gate.className);
        bool declared = false;
        for (const MirrorProperty& prop :
             MirrorSchema::forMetaObject(classes.value(className)).properties()) {
            declared = declared || prop.name == gate.property;
        }
        QVERIFY2(declared, qPrintable(className + '.' + gate.property));
        QVERIFY(MirrorPolicy::featureGateFor(className, QByteArray(gate.property)) == &gate);
        QVERIFY(gate.minVersion >= 1);
    }
}

// The Core leaves a feature-gated property (MirrorPolicy::featureGates) out
// for a window whose hello did not declare its feature. The desktop remote
// window declares only radeStatus, txInhibitReason, alexLpf,
// paTransmitBand and levelCalibration of them, so its schema comparison must not count the other
// absences as skew: after the whole snapshot it logs no schema skew and both
// skew sets are empty.
void TstStationSession::remoteWindowSeesNoSchemaSkewFromTheCurrentCore()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("no-skew.settings")));
    auto model = makeStationRadioModel(0);
    QVERIFY(!model->slices().isEmpty());
    const int sliceId = model->slices().first()->sliceIndex();
    StationServer server(model.get(), settings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QStringList lines;
    LogCapture capture(&lines);
    auto* station = new LoopbackTransport(QStringLiteral("no-skew-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("no-skew-client"), this);
    station->linkTo(peer);
    client.startSession(peer, server.token());
    server.acceptTransport(station);
    QTRY_VERIFY(client.stationLinkReady());
    QTRY_VERIFY(remote.sliceById(sliceId) != nullptr);
    QTRY_VERIFY(client.mirroredObject(QByteArrayLiteral("transmit")) != nullptr);
    QTest::qWait(200);

    // The gated properties of features this window did not declare really
    // were left out of what it received, so the comparison below exercised
    // the gate rather than passing vacuously. The gated features it
    // declares (radeStatus: its VFO flag's RADE row; txInhibitReason: why
    // the Core's transmit is held; alexLpf: the Alex-1 tab's low-pass
    // lamps; paTransmitBand: the PA row the Core holds on the air;
    // levelCalibration: Setup's calibration run; rxFilterLowPass: why the
    // receive low-pass is set for another slice) arrive.
    const QSet<QByteArray> declaredGatedFeatures{QByteArrayLiteral("radeStatus"),
                                                 QByteArrayLiteral("txInhibitReason"),
                                                 QByteArrayLiteral("alexLpf"),
                                                 QByteArrayLiteral("paTransmitBand"),
                                                 QByteArrayLiteral("levelCalibration"),
                                                 QByteArrayLiteral("rxFilterLowPass"),
                                                 QByteArrayLiteral("radeReason")};
    QSet<QByteArray> arrivedGatedFeatures;
    bool sawTransmitSchema = false;
    for (const QByteArray& wire : peer->received()) {
        const SessionMessage message = decodeOrFail(wire);
        if (message.kind != SessionMessageKind::Schema) {
            continue;
        }
        for (const MirrorPolicy::FeatureGate& gate : MirrorPolicy::featureGates()) {
            if (MirrorSchema::shortClassName(message.className) != gate.className) {
                continue;
            }
            for (const SessionSchemaField& field : message.fields) {
                if (field.name != gate.property) {
                    continue;
                }
                QVERIFY2(declaredGatedFeatures.contains(QByteArray(gate.feature)),
                         qPrintable(QByteArray(gate.className) + '.' + gate.property));
                arrivedGatedFeatures.insert(QByteArray(gate.feature));
            }
        }
        sawTransmitSchema = sawTransmitSchema
            || MirrorSchema::shortClassName(message.className) == "TransmitModel";
    }
    QVERIFY(sawTransmitSchema);
    QCOMPARE(arrivedGatedFeatures, declaredGatedFeatures);

    QVERIFY2(client.schemaNamesOnlyLocal().isEmpty(),
             qPrintable(QStringList(client.schemaNamesOnlyLocal().cbegin(),
                                    client.schemaNamesOnlyLocal().cend()).join(u' ')));
    QVERIFY2(client.schemaNamesOnlyOnStation().isEmpty(),
             qPrintable(QStringList(client.schemaNamesOnlyOnStation().cbegin(),
                                    client.schemaNamesOnlyOnStation().cend()).join(u' ')));
    for (const QString& line : std::as_const(lines)) {
        QVERIFY2(!line.contains(QStringLiteral("schema skew")), qPrintable(line));
    }
    client.disconnectFromStation(QStringLiteral("no skew complete"));
}

// A minor-10 GUI never sees nnrLimit (schema, object, delta), so it has
// nothing to log as schema skew, and its nnr.tryAgain is refused.
void TstStationSession::nnrLimitIsOmittedForMinorTenPeer()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("nnr-limit-old.settings")));
    auto model = makeStationRadioModel(0);
    SliceModel* coreSlice = model->slices().first();
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    auto* station = new LoopbackTransport(QStringLiteral("minor10-nnr-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("minor10-nnr-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kNnrLimitSessionProtocolMinor - 1, 6,
        QStringLiteral("minor-10-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));

    // Changes after the snapshot: the limit itself never reaches this peer
    // (a change sends only nnrStatus, its plain reason); a limit change
    // batched with another property sends only the others.
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::Off));
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::None));
    coreSlice->setNnrAlpha(2.5);
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    NEREUS_TRY_VERIFY([&] {
        for (const QByteArray& wire : peer->received()) {
            if (wire.contains("\"nnrAlpha\"") && wire.contains("\"delta\"")) {
                return true;
            }
        }
        return false;
    }());
    NereusSDR::Test::settleSession();
    bool sawSliceSchema = false;
    for (const QByteArray& wire : peer->received()) {
        QVERIFY2(!wire.contains("nnrLimit"), wire.constData());
        const SessionMessage message = decodeOrFail(wire);
        if (message.kind == SessionMessageKind::Schema && message.className == "SliceModel") {
            sawSliceSchema = true;
        }
        if (message.kind == SessionMessageKind::Delta) {
            QVERIFY(!message.updates.isEmpty());
        }
    }
    QVERIFY(sawSliceSchema);

    peer->clearReceived();
    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "nnr.tryAgain", 31,
        { MirrorUpdate{ 0, "sliceId", MirrorWireKind::Int64, qint64(coreSlice->sliceIndex()) } })));
    SessionMessage result;
    NEREUS_TRY_VERIFY([&] {
        for (const QByteArray& wire : peer->received()) {
            const SessionMessage candidate = decodeOrFail(wire);
            if (candidate.kind == SessionMessageKind::CommandResult
                && candidate.commandId == quint32(31)) {
                result = candidate;
                return true;
            }
        }
        return false;
    }());
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason, QStringLiteral(
        "Update this app to try noise reduction again on this Core."));
    QCOMPARE(coreSlice->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));
}

// R-R3-40: a minor-10 GUI turning NNR off while the Core holds a limit
// clears it as a side effect; the write's corrections never carry nnrLimit
// to that GUI.
void TstStationSession::minorTenWriteThatClearsTheLimitCarriesNoNnrLimit()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("nnr-limit-write.settings")));
    auto model = makeStationRadioModel(0);
    SliceModel* coreSlice = model->slices().first();
    coreSlice->setActiveNr(NrSlot::NNR);
    QCOMPARE(coreSlice->activeNr(), NrSlot::NNR);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    auto* station = new LoopbackTransport(QStringLiteral("minor10-nnr-write-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("minor10-nnr-write-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kNnrLimitSessionProtocolMinor - 1, 6,
        QStringLiteral("minor-10-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));

    const QByteArray key = ObjectRegistry::keyForSlice(coreSlice->sliceIndex());
    MirrorUpdate activeNr;
    for (const QByteArray& wire : peer->received()) {
        const SessionMessage message = decodeOrFail(wire);
        if (message.kind == SessionMessageKind::ObjectCreate && message.objectKey == key) {
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == "activeNr") {
                    activeNr = update;
                }
            }
        }
    }
    QCOMPARE(activeNr.name, QByteArray("activeNr"));

    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    NereusSDR::Test::settleSession();
    peer->clearReceived();
    activeNr.value = qint64(static_cast<int>(NrSlot::NR2));
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(key, {activeNr})));
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::NR2);
    QCOMPARE(coreSlice->nnrLimit(), 0);   // cleared as a side effect
    NEREUS_TRY_VERIFY([&] {
        for (const QByteArray& wire : peer->received()) {
            if (wire.contains("\"activeNr\"") && wire.contains("\"delta\"")) {
                return true;
            }
        }
        return false;
    }());
    NereusSDR::Test::settleSession();
    for (const QByteArray& wire : peer->received()) {
        QVERIFY2(!wire.contains("nnrLimit"), wire.constData());
        const SessionMessage message = decodeOrFail(wire);
        if (message.kind == SessionMessageKind::Delta) {
            QVERIFY(!message.updates.isEmpty());
        }
    }
}

// R-R3-40: a minor-10 GUI never sees nnrLimit, but its existing nnrStatus
// text says why its model changed, naming the Core computer; the text goes
// back to the receiver's own status when the limit clears.
void TstStationSession::minorTenPeerReadsWhyInNnrStatus()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("nnr-limit-status.settings")));
    auto model = makeStationRadioModel(0);
    SliceModel* coreSlice = model->slices().first();
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    auto* station = new LoopbackTransport(QStringLiteral("minor10-nnr-status-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("minor10-nnr-status-client"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kNnrLimitSessionProtocolMinor - 1, 6,
        QStringLiteral("minor-10-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
    const QString normal = coreSlice->nnrStatus();

    const auto lastStatus = [&]() -> std::optional<QString> {
        std::optional<QString> status;
        for (const QByteArray& wire : peer->received()) {
            const SessionMessage message = decodeOrFail(wire);
            if (message.kind != SessionMessageKind::Delta) {
                continue;
            }
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == "nnrStatus") {
                    status = update.value.toString();
                }
            }
        }
        return status;
    };
    peer->clearReceived();
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
    NEREUS_TRY_COMPARE(lastStatus(), std::optional<QString>(QStringLiteral(
        "Noise reduction is using the Standard model. The Core computer could not keep up with Premium.")));
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::Off));
    NEREUS_TRY_COMPARE(lastStatus(), std::optional<QString>(QStringLiteral(
        "Noise reduction was turned off. The Core computer could not keep up.")));
    coreSlice->setNnrLimit(static_cast<int>(NnrLimit::None));
    NEREUS_TRY_COMPARE(lastStatus(), std::optional<QString>(normal));
    for (const QByteArray& wire : peer->received()) {
        QVERIFY2(!wire.contains("nnrLimit"), wire.constData());
    }
}

// The GUI sends nnr.tryAgain only to a Core that negotiated minor 11.
void TstStationSession::clientSendsTryAgainOnlyAtMinorEleven()
{
    for (const quint16 minor : {quint16(kNnrLimitSessionProtocolMinor - 1),
                                kNnrLimitSessionProtocolMinor}) {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("raw-nnr-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("raw-nnr-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        const auto send = [&](const SessionMessage& message) {
            station->sendText(SessionMessages::encode(message));
        };
        send(SessionMessages::hello(kSessionProtocolMajor, minor, 6, QStringLiteral("station")));
        send(SessionMessages::authResult(true, {}, false));
        StationCapabilities caps;
        caps.propertyResultVersion = 1;
        caps.nnrVersion = 1;
        send(SessionMessages::capabilities(caps.toUpdates()));
        send(SessionMessages::snapshotComplete());
        NEREUS_TRY_VERIFY(client.nnrControlAvailable());
        station->clearReceived();
        const auto outcome = client.requestNnrRetry(0);
        QCOMPARE(outcome.sent, minor >= kNnrLimitSessionProtocolMinor);
        QCOMPARE(client.nnrRetryAvailable(), minor >= kNnrLimitSessionProtocolMinor);
        const auto sawCommand = [&] {
            for (const QByteArray& wire : station->received()) {
                if (wire.contains("nnr.tryAgain")) {
                    return true;
                }
            }
            return false;
        };
        if (outcome.sent) {
            NEREUS_TRY_VERIFY(sawCommand());
        } else {
            NereusSDR::Test::settleSession();
            QVERIFY(!sawCommand());
        }
        if (!outcome.sent) {
            QCOMPARE(outcome.reason, QStringLiteral(
                "This station cannot try noise reduction again. Update the station software."));
        }
    }
}

void TstStationSession::remoteTgxlClientRequiresHandshakeMinorAndCapability()
{
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy availabilityChanged(&remote, &RadioModel::stationLinkStateChanged);

    // A client with no negotiated station is inert; neither typed request
    // may fall through to a local accessory connection.
    QVERIFY(!client.remoteTgxlConfigAvailable());
    QVERIFY(!client.requestConfigureTgxl(QStringLiteral("192.0.2.10"), 9010).sent);
    QVERIFY(!client.requestDisconnectTgxl().sent);

    auto* station = new LoopbackTransport(QStringLiteral("tgxl-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("tgxl-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));

    station->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
    station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
    StationCapabilities caps;
    caps.remoteTgxlConfigVersion = 1;
    station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
    station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
    NEREUS_TRY_VERIFY(client.remoteTgxlConfigAvailable());
    NEREUS_TRY_VERIFY(availabilityChanged.count() >= 1);

    const IStationLink::CommandOutcome configured =
        client.requestConfigureTgxl(QStringLiteral("192.0.2.10"), 9010);
    QVERIFY2(configured.sent, qPrintable(configured.reason));
    // The client sends on peer; LoopbackTransport delivers that wire to the
    // linked station endpoint.  Assert the actual receiving route rather
    // than the sender's inbound capture.
    NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
    const QList<QByteArray> sent = station->received();
    const SessionMessage command = decodeOrFail(sent.last());
    QCOMPARE(command.kind, SessionMessageKind::CommandInvoke);
    QCOMPARE(command.commandVerb, QByteArrayLiteral("configureTgxl"));
    QCOMPARE(command.arguments.size(), 2);
    QCOMPARE(command.arguments.at(0).name, QByteArrayLiteral("host"));
    QCOMPARE(command.arguments.at(0).kind, MirrorWireKind::Utf8);
    QCOMPARE(command.arguments.at(0).value.toString(), QStringLiteral("192.0.2.10"));
    QCOMPARE(command.arguments.at(1).name, QByteArrayLiteral("port"));
    QCOMPARE(command.arguments.at(1).kind, MirrorWireKind::Int64);
    QCOMPARE(command.arguments.at(1).value.toLongLong(), qint64(9010));

    const IStationLink::CommandOutcome disconnected = client.requestDisconnectTgxl();
    QVERIFY2(disconnected.sent, qPrintable(disconnected.reason));
    NEREUS_TRY_VERIFY(station->receivedKinds().count(QByteArrayLiteral("command.invoke")) == 2);
    const QList<QByteArray> afterDisconnect = station->received();
    const SessionMessage disconnect = decodeOrFail(afterDisconnect.last());
    QCOMPARE(disconnect.kind, SessionMessageKind::CommandInvoke);
    QCOMPARE(disconnect.commandVerb, QByteArrayLiteral("disconnectTgxl"));
    QVERIFY(disconnect.arguments.isEmpty());

    const int availableSignalCount = availabilityChanged.count();
    station->closeLink(QStringLiteral("test teardown"));
    NEREUS_TRY_VERIFY(!client.remoteTgxlConfigAvailable());
    NEREUS_TRY_VERIFY(availabilityChanged.count() > availableSignalCount);

    RadioModel olderRemote(RadioModel::Role::Remote);
    SettingsProxy olderProxy;
    StationClient olderClient(&olderRemote, &olderProxy);
    auto* olderStation = new LoopbackTransport(QStringLiteral("tgxl-older-station"), this);
    auto* olderPeer = new LoopbackTransport(QStringLiteral("tgxl-older-client"), this);
    olderStation->linkTo(olderPeer);
    olderClient.startSession(olderPeer, QStringLiteral("test-token"));
    olderStation->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor,
        static_cast<quint16>(kRemoteTgxlConfigSessionProtocolMinor - 1),
        6, QStringLiteral("older-station"))));
    olderStation->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
    olderStation->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
    olderStation->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
    NEREUS_TRY_VERIFY(olderClient.isHandshakeComplete());
    QVERIFY(!olderClient.remoteTgxlConfigAvailable());
}

void TstStationSession::remoteFourO3AClientRequiresHandshakeMinorAndCapability()
{
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);

    QVERIFY(!client.remoteFourO3AControlAvailable());
    QVERIFY(!client.requestFourO3AEnabled(true).sent);

    auto* station = new LoopbackTransport(QStringLiteral("four-o3a-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("four-o3a-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    station->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
    station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
    StationCapabilities caps;
    caps.remoteFourO3AControlVersion = 1;
    station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
    station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
    NEREUS_TRY_VERIFY(client.remoteFourO3AControlAvailable());

    const IStationLink::CommandOutcome requested = client.requestFourO3AEnabled(true);
    QVERIFY2(requested.sent, qPrintable(requested.reason));
    NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
    const SessionMessage command = decodeOrFail(station->received().last());
    QCOMPARE(command.commandVerb, QByteArrayLiteral("setFourO3AEnabled"));
    QCOMPARE(command.arguments.size(), 1);
    QCOMPARE(command.arguments.first().name, QByteArrayLiteral("enabled"));
    QCOMPARE(command.arguments.first().kind, MirrorWireKind::Bool);
    QVERIFY(command.arguments.first().value.toBool());
    // No command can start the Remote model's local listener.
    QVERIFY(!remote.smartSdrListener()->isListening());

    RadioModel olderRemote(RadioModel::Role::Remote);
    SettingsProxy olderProxy;
    StationClient olderClient(&olderRemote, &olderProxy);
    auto* olderStation = new LoopbackTransport(QStringLiteral("four-o3a-older-station"), this);
    auto* olderPeer = new LoopbackTransport(QStringLiteral("four-o3a-older-client"), this);
    olderStation->linkTo(olderPeer);
    olderClient.startSession(olderPeer, QStringLiteral("test-token"));
    olderStation->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor,
        static_cast<quint16>(kRemoteFourO3AControlSessionProtocolMinor - 1), 6,
        QStringLiteral("older-station"))));
    olderStation->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
    olderStation->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
    olderStation->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
    NEREUS_TRY_VERIFY(olderClient.isHandshakeComplete());
    QVERIFY(!olderClient.remoteFourO3AControlAvailable());
}

void TstStationSession::remoteFourO3AServerRejectsPreAuthAndOldMinor()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("four-o3a.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    const SessionMessage request = SessionMessages::commandInvoke(
        "setFourO3AEnabled", 41,
        { MirrorUpdate{0, "enabled", MirrorWireKind::Bool, true} });

    auto* unauthStation = new LoopbackTransport(QStringLiteral("four-o3a-unauth-station"), this);
    auto* unauthPeer = new LoopbackTransport(QStringLiteral("four-o3a-unauth-peer"), this);
    unauthStation->linkTo(unauthPeer);
    server.acceptTransport(unauthStation);
    unauthPeer->sendText(SessionMessages::encode(request));
    NEREUS_TRY_VERIFY(!unauthPeer->isOpen());

    auto* oldStation = new LoopbackTransport(QStringLiteral("four-o3a-old-station"), this);
    auto* oldPeer = new LoopbackTransport(QStringLiteral("four-o3a-old-peer"), this);
    oldStation->linkTo(oldPeer);
    server.acceptTransport(oldStation);
    oldPeer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor,
        static_cast<quint16>(kRemoteFourO3AControlSessionProtocolMinor - 1), 6,
        QStringLiteral("older-client"))));
    oldPeer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    oldPeer->clearReceived();
    oldPeer->sendText(SessionMessages::encode(request));
    NEREUS_TRY_VERIFY(oldPeer->receivedKinds().contains(QByteArrayLiteral("command.result")));
    SessionMessage result;
    for (const QByteArray& wire : oldPeer->received()) {
        const SessionMessage candidate = decodeOrFail(wire);
        if (candidate.kind == SessionMessageKind::CommandResult
            && candidate.commandId == request.commandId) {
            result = candidate;
            break;
        }
    }
    QCOMPARE(result.kind, SessionMessageKind::CommandResult);
    QVERIFY(!result.accepted);
    QVERIFY2(result.reason.startsWith(QStringLiteral("Update this app to ")),
             qPrintable(result.reason));
}

void TstStationSession::remoteFourO3AAuthenticatedRoundTripMirrorsActualListenerState()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("four-o3a-roundtrip.settings")));
    auto stationModel = makeStationRadioModel(0);
    stationModel->enableStationAccessoryIdentity();
    stationModel->smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy finished(&remote, &RadioModel::stationFourO3ACommandFinished);
    auto* station = new LoopbackTransport(QStringLiteral("four-o3a-roundtrip-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("four-o3a-roundtrip-client"), this);
    station->linkTo(peer);
    client.startSession(peer, server.token());
    server.acceptTransport(station);

    NEREUS_TRY_VERIFY(client.remoteFourO3AControlAvailable());
    QVERIFY(!remote.currentRadioMac().isEmpty());
    QVERIFY(!remote.fourO3AEnabled());
    QVERIFY(!remote.fourO3AListening());
    QVERIFY(remote.fourO3AListenerError().isEmpty());

    const IStationLink::CommandOutcome enabled = client.requestFourO3AEnabled(true);
    QVERIFY2(enabled.sent, qPrintable(enabled.reason));
    // The remote model owns no listener. It remains false until the Core
    // snapshot/delta arrives after the accepted CommandResult.
    QVERIFY(!remote.smartSdrListener()->isListening());
    NEREUS_TRY_VERIFY(!finished.isEmpty());
    QCOMPARE(finished.last().at(0).toBool(), true);
    NEREUS_TRY_VERIFY(stationModel->fourO3AEnabled());
    NEREUS_TRY_VERIFY(stationModel->fourO3AListening());
    NEREUS_TRY_VERIFY(remote.fourO3AEnabled());
    NEREUS_TRY_VERIFY(remote.fourO3AListening());
    QVERIFY(remote.fourO3AListenerError().isEmpty());
    QVERIFY(!remote.smartSdrListener()->isListening());
    QCOMPARE(stationModel->peripheralValue(QStringLiteral("FourO3A_Enabled")),
             QStringLiteral("True"));

    // A real occupied endpoint is accepted as intent but mirrored as the
    // listener failure it is; no invented listening success is possible.
    const IStationLink::CommandOutcome disabled = client.requestFourO3AEnabled(false);
    QVERIFY2(disabled.sent, qPrintable(disabled.reason));
    NEREUS_TRY_VERIFY(!remote.fourO3AEnabled());
    NEREUS_TRY_VERIFY(!stationModel->fourO3AListening());
    QTcpServer blocker;
    QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));
    stationModel->smartSdrListener()->setListenEndpointForTesting(
        QHostAddress::LocalHost, blocker.serverPort());
    const IStationLink::CommandOutcome bindFailure = client.requestFourO3AEnabled(true);
    QVERIFY2(bindFailure.sent, qPrintable(bindFailure.reason));
    NEREUS_TRY_VERIFY(remote.fourO3AEnabled());
    NEREUS_TRY_VERIFY(!remote.fourO3AListening());
    NEREUS_TRY_VERIFY(!remote.fourO3AListenerError().isEmpty());
    QVERIFY(!remote.smartSdrListener()->isListening());

    // Link loss clears the remote-only snapshot cache, including a real
    // listener error, without starting a local listener or writing a local
    // setting on the GUI model.
    station->closeLink(QStringLiteral("four-o3a roundtrip teardown"));
    NEREUS_TRY_VERIFY(!remote.fourO3AEnabled());
    NEREUS_TRY_VERIFY(!remote.fourO3AListening());
    NEREUS_TRY_VERIFY(remote.fourO3AListenerError().isEmpty());
    QVERIFY(!remote.smartSdrListener()->isListening());
}

void TstStationSession::remoteFourO3AUnansweredCommandDoesNotSurviveSession_data()
{
    QTest::addColumn<bool>("closeBeforeReplacement");
    QTest::newRow("link-loss") << true;
    QTest::newRow("direct-replacement") << false;
}

void TstStationSession::remoteFourO3AUnansweredCommandDoesNotSurviveSession()
{
    QFETCH(bool, closeBeforeReplacement);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy finished(&remote, &RadioModel::stationFourO3ACommandFinished);
    const auto attach = [&]() {
        auto* station = new LoopbackTransport(QStringLiteral("four-o3a-command-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("four-o3a-command-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
        station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        StationCapabilities caps;
        caps.remoteFourO3AControlVersion = 1;
        station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        return station;
    };

    auto* firstStation = attach();
    NEREUS_TRY_VERIFY(client.remoteFourO3AControlAvailable());
    QVERIFY(client.requestFourO3AEnabled(true).sent);
    NEREUS_TRY_VERIFY(firstStation->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
    const SessionMessage abandoned = decodeOrFail(firstStation->received().last());
    QCOMPARE(abandoned.commandVerb, QByteArrayLiteral("setFourO3AEnabled"));
    QVERIFY(finished.isEmpty());
    // The old request never receives a result. Both a link loss and a
    // directly adopted replacement must retire its pending completion.
    if (closeBeforeReplacement) {
        firstStation->closeLink(QStringLiteral("lost before command result"));
        NEREUS_TRY_VERIFY(!client.remoteFourO3AControlAvailable());
    }

    auto* currentStation = attach();
    NEREUS_TRY_VERIFY(client.remoteFourO3AControlAvailable());
    QVERIFY(client.requestFourO3AEnabled(false).sent);
    NEREUS_TRY_VERIFY(currentStation->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
    const SessionMessage current = decodeOrFail(currentStation->received().last());
    QCOMPARE(current.commandVerb, QByteArrayLiteral("setFourO3AEnabled"));
    QVERIFY(current.commandId != abandoned.commandId);
    currentStation->sendText(SessionMessages::encode(SessionMessages::commandResult(
        current.commandVerb, current.commandId, true, {}, {})));
    NEREUS_TRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first().at(0).toBool());
    QVERIFY(!remote.smartSdrListener()->isListening());
}

void TstStationSession::remoteTgxlCommandIsGatedAtAuthenticatedServerBoundary()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* station = new LoopbackTransport(QStringLiteral("tgxl-server"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("tgxl-peer"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, static_cast<quint16>(kRemoteTgxlConfigSessionProtocolMinor - 1),
        6, QStringLiteral("older-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    peer->clearReceived();

    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "configureTgxl", 17,
        { MirrorUpdate{ 0, "host", MirrorWireKind::Utf8, QStringLiteral("192.0.2.10") },
          MirrorUpdate{ 0, "port", MirrorWireKind::Int64, qint64(9010) } })));
    NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("command.result")));
    SessionMessage result;
    for (const QByteArray& wire : peer->received()) {
        const SessionMessage candidate = decodeOrFail(wire);
        if (candidate.kind == SessionMessageKind::CommandResult
            && candidate.commandId == quint32(17)) {
            result = candidate;
            break;
        }
    }
    QCOMPARE(result.kind, SessionMessageKind::CommandResult);
    QVERIFY(!result.accepted);
    QVERIFY2(result.reason.startsWith(QStringLiteral("Update this app to ")),
             qPrintable(result.reason));

    // With the negotiated minor this reaches the dispatcher and model
    // policy. The fixture intentionally lacks station accessory identity,
    // so it must refuse without mutating an accessory.
    auto* currentStation = new LoopbackTransport(QStringLiteral("tgxl-current-server"), this);
    auto* currentPeer = new LoopbackTransport(QStringLiteral("tgxl-current-peer"), this);
    currentStation->linkTo(currentPeer);
    server.acceptTransport(currentStation);
    currentPeer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("current-client"))));
    currentPeer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(currentPeer->receivedKinds().contains(QByteArrayLiteral("auth.result")));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    currentPeer->clearReceived();
    currentPeer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "configureTgxl", 18,
        { MirrorUpdate{ 0, "host", MirrorWireKind::Utf8, QStringLiteral("192.0.2.10") },
          MirrorUpdate{ 0, "port", MirrorWireKind::Int64, qint64(9010) } })));
    NEREUS_TRY_VERIFY(currentPeer->receivedKinds().contains(QByteArrayLiteral("command.result")));
    SessionMessage currentResult;
    for (const QByteArray& wire : currentPeer->received()) {
        const SessionMessage candidate = decodeOrFail(wire);
        if (candidate.kind == SessionMessageKind::CommandResult
            && candidate.commandId == quint32(18)) {
            currentResult = candidate;
            break;
        }
    }
    QCOMPARE(currentResult.kind, SessionMessageKind::CommandResult);
    QVERIFY(!currentResult.accepted);
    QVERIFY(currentResult.reason != QStringLiteral("The Core does not know this request. Updating the Core may help."));
    QVERIFY(!currentResult.reason.contains(QStringLiteral("newer station protocol")));
}

void TstStationSession::remoteTgxlConfigureAcceptanceStartsIdentityOnly()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    // This is a genuine station-side admission path.  A listening test peer
    // lets us prove acceptance starts native identification, while withholding
    // the TGXL version/identity exchange proves accepted does not mean the
    // device state has been hydrated or declared connected.
    QTcpServer tgxl;
    QVERIFY(tgxl.listen(QHostAddress::LocalHost, 0));
    auto stationModel = makeStationRadioModel(0);
    stationModel->setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    stationModel->enableStationAccessoryIdentity();
    TunerModel* const tuner = stationModel->tunerModel();
    QVERIFY(tuner != nullptr);
    AppSettings::instance().save(); // Establish the pre-command on-disk state.
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* station = new LoopbackTransport(QStringLiteral("tgxl-accepted-server"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("tgxl-accepted-peer"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("current-client"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    peer->clearReceived();

    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "configureTgxl", 19,
        { MirrorUpdate{ 0, "host", MirrorWireKind::Utf8, QStringLiteral("127.0.0.1") },
          MirrorUpdate{ 0, "port", MirrorWireKind::Int64, qint64(tgxl.serverPort()) } })));
    NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("command.result")));
    SessionMessage result;
    for (const QByteArray& wire : peer->received()) {
        const SessionMessage candidate = decodeOrFail(wire);
        if (candidate.kind == SessionMessageKind::CommandResult
            && candidate.commandId == quint32(19)) {
            result = candidate;
            break;
        }
    }
    QCOMPARE(result.kind, SessionMessageKind::CommandResult);
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QCOMPARE(stationModel->peripheralValue(QStringLiteral("TGXL_ManualIp")),
             QStringLiteral("127.0.0.1"));
    QCOMPARE(stationModel->peripheralValue(QStringLiteral("TGXL_ManualPort")),
             QString::number(tgxl.serverPort()));
    AppSettings persisted(AppSettings::instance().filePath());
    persisted.load();
    QCOMPARE(persisted.hardwareValue(stationModel->currentRadioMac(),
                  QStringLiteral("peripherals/TGXL_ManualIp")).toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(persisted.hardwareValue(stationModel->currentRadioMac(),
                  QStringLiteral("peripherals/TGXL_ManualPort")).toString(), QString::number(tgxl.serverPort()));
    NEREUS_TRY_VERIFY(tgxl.hasPendingConnections());
    QVERIFY(stationModel->tgxlConnection()->identityInfo().serial.isEmpty());
    QVERIFY(!tuner->hasDirectConnection());
    QVERIFY(!tuner->isPresent());

    QString reason;
    QVERIFY2(stationModel->disconnectTgxlForStation(&reason), qPrintable(reason));
}

// ── TokenStore ───────────────────────────────────────────────────────────

void TstStationSession::tokenIsGeneratedNotChosenAndPersists()
{
    // iPhone app Task 12: a new Core creates NO token (its devices pair
    // with it instead); a Core upgraded from before paired devices keeps
    // the token it has, unchanged, across starts, and can retire it.
    QTemporaryDir fresh;
    QVERIFY(fresh.isValid());
    TokenStore newCore(fresh.path());
    QVERIFY2(newCore.isValid(), qPrintable(newCore.lastError()));
    QVERIFY(!newCore.isActive());
    QVERIFY(newCore.token().isEmpty());
    QVERIFY(!QFile::exists(fresh.filePath(QStringLiteral("station-token"))));
    QCOMPARE(newCore.verify(QString()), TokenStore::VerifyResult::Rejected);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    NereusSDR::Test::seedUpgradedCoreToken(dir.path());
    TokenStore first(dir.path());
    QVERIFY2(first.isValid(), qPrintable(first.lastError()));
    QVERIFY(first.isActive());

    // 256 bits, base64url, no padding.
    QCOMPARE(first.token().size(), 43);
    QVERIFY(!first.token().contains(QLatin1Char('=')));
    QVERIFY(!first.token().contains(QLatin1Char('+')));
    QVERIFY(!first.token().contains(QLatin1Char('/')));

    // There is no setter, by design: a second construction against the
    // same directory returns the SAME token.
    TokenStore second(dir.path());
    QVERIFY(second.isActive());
    QCOMPARE(second.token(), first.token());

    // Retired for good: the file goes and nothing is accepted after.
    const QString token = second.token();
    QVERIFY(second.retire());
    QVERIFY(!second.isActive());
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("station-token"))));
    QCOMPARE(second.verify(token), TokenStore::VerifyResult::Rejected);
    TokenStore afterRetire(dir.path());
    QVERIFY(!afterRetire.isActive());
}

void TstStationSession::tokenVerifyIsRateLimitedAfterRepeatedFailures()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TokenStore store(NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    QVERIFY(store.isActive());

    // Short lockout so the slot does not cost a minute.
    store.setRateLimit(3, 200);

    QCOMPARE(store.verify(store.token()), TokenStore::VerifyResult::Accepted);

    for (int i = 0; i < 3; ++i) {
        QCOMPARE(store.verify(QStringLiteral("nope")), TokenStore::VerifyResult::Rejected);
    }
    QCOMPARE(store.consecutiveFailures(), 3);

    // Rate limited now -- and the CORRECT token is refused too, which is
    // the point: a rate limiter that let the right answer through would
    // not slow an attacker down at all.
    QCOMPARE(store.verify(QStringLiteral("nope")), TokenStore::VerifyResult::RateLimited);
    QCOMPARE(store.verify(store.token()), TokenStore::VerifyResult::RateLimited);

    // The lockout ends on the store's own clock.
    NEREUS_TRY_VERIFY(!store.isRateLimited());
    QCOMPARE(store.verify(store.token()), TokenStore::VerifyResult::Accepted);
    QCOMPARE(store.consecutiveFailures(), 0);
}

// LINK minor 5: the token's limiter is kept per source address. A guesser's
// lockout refuses that guesser, not the operator's right token from another
// address.
void TstStationSession::tokenFailuresFromOneSourceDoNotLimitAnother()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TokenStore store(NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    QVERIFY(store.isActive());
    store.setRateLimit(3, 60000);
    const QString guesser = QStringLiteral("198.51.100.9");
    const QString operatorAddress = QStringLiteral("192.0.2.7");

    for (int i = 0; i < 3; ++i) {
        QCOMPARE(store.verify(QStringLiteral("nope"), guesser),
                 TokenStore::VerifyResult::Rejected);
    }
    QVERIFY(store.isRateLimited(guesser));
    QCOMPARE(store.verify(store.token(), guesser), TokenStore::VerifyResult::RateLimited);
    QVERIFY(!store.isRateLimited(operatorAddress));
    QCOMPARE(store.consecutiveFailures(operatorAddress), 0);
    QCOMPARE(store.verify(store.token(), operatorAddress), TokenStore::VerifyResult::Accepted);
    // The guesser stays refused.
    QCOMPARE(store.verify(store.token(), guesser), TokenStore::VerifyResult::RateLimited);
}

// ── The connect sequence ─────────────────────────────────────────────────

void TstStationSession::handshakeCompletesInSectionSevenZeroOrder()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50001"));

    auto stationModel = makeStationRadioModel(1);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(!server.token().isEmpty(), "TokenStore did not provision");

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    // The client end is driven BY HAND here, so this slot asserts the
    // station's real output rather than what a client happened to make of
    // it.
    server.acceptTransport(stationEnd);

    NEREUS_TRY_VERIFY(!clientEnd->received().isEmpty());
    const SessionMessage stationHello = decodeOrFail(clientEnd->received().first());
    QCOMPARE(stationHello.kind, SessionMessageKind::Hello);
    QCOMPARE(stationHello.protocolMajor, kSessionProtocolMajor);

    clientEnd->sendText(SessionMessages::encode(
        SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 6,
                               QStringLiteral("test-client"))));
    clientEnd->sendText(
        SessionMessages::encode(SessionMessages::authRequest(server.token())));

    NEREUS_TRY_VERIFY(clientEnd->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));

    const QList<QByteArray> kinds = clientEnd->receivedKinds();

    // Parent design section 7.0: "TLS establish -> protocol hello carrying
    // a semantic version from both ends -> authentication -> capability
    // exchange -> state snapshot -> snapshot-complete marker."
    const int hello = indexOfKind(kinds, "hello");
    const int auth = indexOfKind(kinds, "auth.result");
    const int caps = indexOfKind(kinds, "capabilities");
    const int settings = indexOfKind(kinds, "settings.snapshot");
    const int schema = indexOfKind(kinds, "schema");
    const int create = indexOfKind(kinds, "object.create");
    const int done = indexOfKind(kinds, "snapshot.complete");

    QVERIFY2(hello == 0, qPrintable(QStringLiteral("kinds: %1")
                                        .arg(QString::fromUtf8(kinds.join(',')))));
    QVERIFY(auth > hello);
    QVERIFY(caps > auth);
    QVERIFY(settings > caps);
    QVERIFY(schema > settings);
    QVERIFY(create > schema);
    QVERIFY(done > create);
    // The marker closes the burst: no schema and no object.create may
    // follow it. Asserted this way rather than "the marker is the last
    // message", because an ordinary Delta legitimately can follow (the
    // station's flush timer runs from the moment the session is
    // established), and pinning the absolute last index would make this
    // slot fail on timing rather than on ordering.
    QVERIFY(kinds.lastIndexOf(QByteArrayLiteral("schema")) < done);
    QVERIFY(kinds.lastIndexOf(QByteArrayLiteral("object.create")) < done);
    QVERIFY(kinds.lastIndexOf(QByteArrayLiteral("capabilities")) < done);
    QVERIFY(kinds.lastIndexOf(QByteArrayLiteral("settings.snapshot")) < done);

    QVERIFY(server.hasAuthenticatedSession());
}

void TstStationSession::capabilitiesAdvertiseEffectiveNotBoardLimits()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    const StationCapabilities wide = server.buildCapabilities();
    QVERIFY(wide.boardMaxSlices > 1);
    // With no narrowing configured the effective value equals the board's.
    QCOMPARE(wide.effectiveMaxSlices, wide.boardMaxSlices);
    QCOMPARE(wide.board, HPSDRHW::HermesLite);
    QVERIFY(wide.radioConnected);
    // TX is R4 in its entirety; R2 must never advertise otherwise.
    QVERIFY(!wide.txPermitted);

    // Parent section 4.5: the descriptor advertises what the DAEMON can
    // sustain, and the client gates on that, never on the board value.
    server.setSustainableSliceLimit(2);
    const StationCapabilities narrowed = server.buildCapabilities();
    QCOMPARE(narrowed.effectiveMaxSlices, 2);
    QCOMPARE(narrowed.boardMaxSlices, wide.boardMaxSlices);
    QVERIFY(narrowed.effectiveMaxSlices < narrowed.boardMaxSlices);

    // A configured limit ABOVE what the radio has is clamped: advertising
    // more slices than exist is a worse failure than advertising fewer.
    server.setSustainableSliceLimit(wide.boardMaxSlices + 5);
    QCOMPARE(server.buildCapabilities().effectiveMaxSlices, wide.boardMaxSlices);

    // Round trip through the wire form.
    const QList<MirrorUpdate> encoded = narrowed.toUpdates();
    const StationCapabilities decoded = StationCapabilities::fromUpdates(encoded);
    QCOMPARE(decoded.effectiveMaxSlices, narrowed.effectiveMaxSlices);
    QCOMPARE(decoded.boardMaxSlices, narrowed.boardMaxSlices);
    QCOMPARE(decoded.board, narrowed.board);
    QCOMPARE(decoded.macAddress, narrowed.macAddress);
    QCOMPARE(server.buildCapabilities().remoteWidebandDisplayVersion, 0);
    QCOMPARE(server.buildCapabilities().remoteAudioStatusVersion, 0);
    QCOMPARE(StationCapabilities::fromUpdates(narrowed.toUpdates()).remoteAudioStatusVersion, 0);
    QCOMPARE(server.buildCapabilities().spectrumGrantVersion, 0);
    QCOMPARE(StationCapabilities::fromUpdates(narrowed.toUpdates()).spectrumGrantVersion, 0);
    server.setMediaEnabled(true);
    const auto mediaCaps = server.buildCapabilities();
    QCOMPARE(mediaCaps.remoteWidebandDisplayVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(mediaCaps.toUpdates()).remoteWidebandDisplayVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates({}).remoteWidebandDisplayVersion, 0);
    QCOMPARE(mediaCaps.remoteAudioStatusVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates(mediaCaps.toUpdates()).remoteAudioStatusVersion, 1);
    QCOMPARE(StationCapabilities::fromUpdates({}).remoteAudioStatusVersion, 0);
    QCOMPARE(mediaCaps.spectrumGrantVersion, 2); // parity Task 17: decimation
    QCOMPARE(StationCapabilities::fromUpdates(mediaCaps.toUpdates()).spectrumGrantVersion, 2);
    QCOMPARE(StationCapabilities::fromUpdates({}).spectrumGrantVersion, 0);
}

void TstStationSession::clientAppliesCapabilitiesAndDrivesConnected()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(2);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setSustainableSliceLimit(3);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    // Before the handshake: task 3's storage-backed isConnected() is
    // false, and maxSlices() is pinned at its disconnected default.
    QVERIFY(!clientModel.isConnected());
    QCOMPARE(clientModel.maxSlices(), 1);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy stateChanges(&clientModel, &RadioModel::connectionStateChanged);

    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);

    NEREUS_TRY_COMPARE(completed.count(), 1);

    // Step 6, the step that makes three earlier tasks mean anything.
    QVERIFY(clientModel.isConnected());
    QVERIFY(!stateChanges.isEmpty());
    QCOMPARE(clientModel.maxSlices(), 3);
    QCOMPARE(clientModel.stationUserDdcCount(),
             stationModel->boardCapabilities().userDdcCount);
    QCOMPARE(clientModel.boardCapabilities().board, HPSDRHW::HermesLite);
    QCOMPARE(clientModel.currentRadioMac(), QStringLiteral("AA:BB:CC:DD:EE:01"));

    // The mirror carried every slice the station already held, under the
    // STATION's ids.
    QCOMPARE(clientModel.slices().size(), stationModel->slices().size());
    for (SliceModel* stationSlice : stationModel->slices()) {
        QVERIFY2(clientModel.sliceById(stationSlice->sliceIndex()) != nullptr,
                 qPrintable(QStringLiteral("client is missing slice id %1")
                                .arg(stationSlice->sliceIndex())));
    }
}

void TstStationSession::lateRadioRefreshUpdatesAuthenticatedClientWithoutReplayingMirror()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    // Start as the R-R3-27 daemon does when discovery has not yet found its
    // configured radio.  Keep a real slice in the mirror so the assertion
    // below proves the late identity update does not replace GUI objects.
    auto stationModel = makeStationRadioModel(0);
    stationModel->setConnectionStateForTest(ConnectionState::Disconnected);
    QCOMPARE(stationModel->addPanadapter(), 0);
    const QString mac = QStringLiteral("AA:BB:CC:DD:EE:01");
    stationSettings.setHardwareValue(
        mac, QStringLiteral("radioInfo/sampleRate"), QStringLiteral("192000"));

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setSustainableSliceLimit(2);
    server.setMediaEnabled(true);

    RadioModel clientModel(RadioModel::Role::Remote);
    QCOMPARE(clientModel.addPanadapter(), 0);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy authenticated(&server, &StationServer::clientAuthenticated);
    QSignalSpy mediaStarted(&server, &StationServer::mediaSessionStarted);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    QVERIFY(!clientModel.isConnected());
    QVERIFY(clientModel.currentRadioMac().isEmpty());
    const int sliceId = stationModel->slices().first()->sliceIndex();
    SliceModel* const existingSlice = clientModel.sliceById(sliceId);
    const QList<PanadapterModel*> initialPans = clientModel.panadapters();
    QVERIFY(!initialPans.isEmpty());
    PanadapterModel* const existingPan = initialPans.first();
    QVERIFY(existingSlice != nullptr);
    QVERIFY(existingPan != nullptr);
    QCOMPARE(authenticated.count(), 1);
    QCOMPARE(mediaStarted.count(), 1);
    QVERIFY(!proxy.contains(QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac)));

    clientEnd->clearReceived();
    stationModel->setConnectionStateForTest(ConnectionState::Connected);
    stationModel->emitCurrentRadioChangedForTest();

    NEREUS_TRY_VERIFY(clientModel.isConnected());
    QCOMPARE(clientModel.currentRadioMac(), mac);
    QCOMPARE(clientModel.boardCapabilities().board, HPSDRHW::HermesLite);
    QCOMPARE(clientModel.maxSlices(), 2);
    NEREUS_TRY_COMPARE(proxy.value(QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac), QVariant{})
                     .toString(),
                 QStringLiteral("192000"));

    const QList<QByteArray> lateKinds = clientEnd->receivedKinds();
    QVERIFY(lateKinds.contains(QByteArrayLiteral("capabilities")));
    QVERIFY(lateKinds.contains(QByteArrayLiteral("settings.snapshot")));
    QVERIFY(!lateKinds.contains(QByteArrayLiteral("schema")));
    QVERIFY(!lateKinds.contains(QByteArrayLiteral("object.create")));
    QVERIFY(!lateKinds.contains(QByteArrayLiteral("snapshot.complete")));
    QCOMPARE(clientModel.sliceById(sliceId), existingSlice);
    const QList<PanadapterModel*> refreshedPans = clientModel.panadapters();
    QVERIFY(!refreshedPans.isEmpty());
    QCOMPARE(refreshedPans.first(), existingPan);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(authenticated.count(), 1);
    QCOMPARE(mediaStarted.count(), 1);
}

// ── Version policy ───────────────────────────────────────────────────────

// The replacement race uses the same real session boundary as the ordinary
// loopback coverage above.
void TstStationSession::queuedLateRadioRefreshReachesOnlyTheSessionsAdmittedBeforeIt()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    stationModel->setConnectionStateForTest(ConnectionState::Disconnected);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel oldClientModel(RadioModel::Role::Remote);
    SettingsProxy oldProxy;
    StationClient oldClient(&oldClientModel, &oldProxy);
    auto* oldStationEnd = new LoopbackTransport(QStringLiteral("old-station-end"), this);
    auto* oldClientEnd = new LoopbackTransport(QStringLiteral("old-client-end"), this);
    oldStationEnd->linkTo(oldClientEnd);
    QSignalSpy oldCompleted(&oldClient, &StationClient::handshakeComplete);
    oldClient.startSession(oldClientEnd, server.token());
    server.acceptTransport(oldStationEnd);
    NEREUS_TRY_COMPARE(oldCompleted.count(), 1);
    oldClientEnd->clearReceived();

    // The deferred callback captures the sessions admitted now (the old
    // one). Before its timer may run, synchronously authenticate a second
    // device (iPhone app Task 71: admitted beside the first, not in its
    // place). This is the reentrant boundary a normal queued socket cannot
    // reach in one test turn: the second device already received its own
    // capabilities and settings snapshot, and must not get the refresh a
    // second time.
    stationModel->setConnectionStateForTest(ConnectionState::Connected);
    stationModel->emitCurrentRadioChangedForTest();

    auto* replacementStation =
        new ImmediateTransport(QStringLiteral("replacement-station"), this);
    auto* replacementClient =
        new ImmediateTransport(QStringLiteral("replacement-client"), this);
    replacementStation->linkTo(replacementClient);
    server.acceptTransport(replacementStation);
    replacementClient->sendText(SessionMessages::encode(
        SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 6,
                               QStringLiteral("replacement-client"))));
    replacementClient->sendText(
        SessionMessages::encode(SessionMessages::authRequest(server.token())));

    NEREUS_TRY_VERIFY(replacementClient->receivedKinds().contains(
        QByteArrayLiteral("snapshot.complete")));
    // The old session gets the refresh it was owed, and stays up.
    NEREUS_TRY_VERIFY(oldClientEnd->receivedKinds().contains(QByteArrayLiteral("settings.snapshot")));

    const QList<QByteArray> replacementKinds = replacementClient->receivedKinds();
    QCOMPARE(replacementKinds.count(QByteArrayLiteral("capabilities")), 1);
    QCOMPARE(replacementKinds.count(QByteArrayLiteral("settings.snapshot")), 1);
    const QList<QByteArray> oldKinds = oldClientEnd->receivedKinds();
    QCOMPARE(oldKinds.count(QByteArrayLiteral("capabilities")), 1);
    QCOMPARE(oldKinds.count(QByteArrayLiteral("settings.snapshot")), 1);
    QVERIFY(!oldKinds.contains(QByteArrayLiteral("session.end")));
    QCOMPARE(oldCompleted.count(), 1);
}

// Version policy (task 18 step 1)
void TstStationSession::majorVersionMismatchRefusesNamingBothVersions()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    server.acceptTransport(stationEnd);
    NEREUS_TRY_VERIFY(!clientEnd->received().isEmpty());

    const quint16 wrongMajor = kSessionProtocolMajor + 1;
    clientEnd->sendText(SessionMessages::encode(SessionMessages::hello(
        wrongMajor, 4, 6, QStringLiteral("from-the-future"))));

    NEREUS_TRY_VERIFY(clientEnd->receivedKinds().contains(QByteArrayLiteral("session.end")));

    QString reason;
    for (const QByteArray& wire : clientEnd->received()) {
        const SessionMessage message = decodeOrFail(wire);
        if (message.kind == SessionMessageKind::SessionEnd) {
            reason = message.reason;
        }
    }
    // Section 7.0: "refused with a message naming both versions rather
    // than failing obscurely". BOTH, not just the offending one; since the
    // iPhone app's Task 4 (R-IOS-01) in plain words, naming each side's
    // link version (SessionEndReasons::versionRefused).
    QCOMPARE(reason, SessionEndReasons::versionRefused({kSessionProtocolMajor}, {wrongMajor}));
    QVERIFY2(reason.contains(QStringLiteral("version %1").arg(kSessionProtocolMajor)),
             qPrintable(reason));
    QVERIFY2(reason.contains(QStringLiteral("version %1").arg(wrongMajor)),
             qPrintable(reason));

    QVERIFY(!server.hasAuthenticatedSession());

    // And the mirror-image case: a CLIENT pointed at a station whose major
    // it does not know refuses from its own side, also naming both.
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    auto* fakeStationEnd = new LoopbackTransport(QStringLiteral("fake-station"), this);
    auto* realClientEnd = new LoopbackTransport(QStringLiteral("client"), this);
    fakeStationEnd->linkTo(realClientEnd);
    client.startSession(realClientEnd, QStringLiteral("irrelevant"));
    fakeStationEnd->sendText(SessionMessages::encode(
        SessionMessages::hello(wrongMajor, 9, 6, QStringLiteral("future-station"))));

    NEREUS_TRY_COMPARE(ended.count(), 1);
    const QString clientReason = ended.first().first().toString();
    QCOMPARE(clientReason,
             SessionEndReasons::versionRefused({wrongMajor}, {kSessionProtocolMajor}));
    QVERIFY2(clientReason.contains(QStringLiteral("version %1").arg(kSessionProtocolMajor)),
             qPrintable(clientReason));
    QVERIFY2(clientReason.contains(QStringLiteral("version %1").arg(wrongMajor)),
             qPrintable(clientReason));
    QVERIFY(!clientModel.isConnected());
}

void TstStationSession::minorVersionMismatchNegotiatesDown()
{
    // Section 7.0: "Equal major with differing minor negotiates down to
    // the lower, and each side gates optional behaviour on the agreed
    // value. A desktop GUI at R4 pointed at a Pi still running R2 is the
    // expected case, not an error case."
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* fakeStationEnd = new LoopbackTransport(QStringLiteral("fake-station"), this);
    auto* realClientEnd = new LoopbackTransport(QStringLiteral("client"), this);
    fakeStationEnd->linkTo(realClientEnd);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    client.startSession(realClientEnd, QStringLiteral("token"));
    const quint16 higherMinor = static_cast<quint16>(kSessionProtocolMinor + 7);
    fakeStationEnd->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, higherMinor, 6, QStringLiteral("newer-station"))));

    // The client answers rather than refusing, and settles on the LOWER of
    // the two minors.
    NEREUS_TRY_VERIFY(fakeStationEnd->receivedKinds().contains(QByteArrayLiteral("hello")));
    QCOMPARE(client.agreedMinor(), kSessionProtocolMinor);
    QCOMPARE(ended.count(), 0);

    // The station side does not refuse a differing minor either: a full
    // session establishes against a client advertising one.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* rawClient = new LoopbackTransport(QStringLiteral("raw-client"), this);
    stationEnd->linkTo(rawClient);
    server.acceptTransport(stationEnd);
    NEREUS_TRY_VERIFY(!rawClient->received().isEmpty());

    rawClient->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, higherMinor, 6, QStringLiteral("newer-client"))));
    rawClient->sendText(
        SessionMessages::encode(SessionMessages::authRequest(server.token())));

    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    QVERIFY(!rawClient->receivedKinds().contains(QByteArrayLiteral("session.end")));
}

// ── Authentication ───────────────────────────────────────────────────────

void TstStationSession::badTokenIsRefusedAndThenRateLimited()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setAuthRateLimit(2, 60000);

    QStringList reasons;
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* rawClient = new LoopbackTransport(QStringLiteral("raw-client"), this);
        stationEnd->linkTo(rawClient);
        server.acceptTransport(stationEnd);
        NEREUS_TRY_VERIFY(!rawClient->received().isEmpty());

        rawClient->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6,
            QStringLiteral("wrong-token-client"))));
        rawClient->sendText(SessionMessages::encode(
            SessionMessages::authRequest(QStringLiteral("definitely-not-the-token"))));

        NEREUS_TRY_VERIFY(rawClient->receivedKinds().contains(QByteArrayLiteral("auth.result")));
        for (const QByteArray& wire : rawClient->received()) {
            const SessionMessage message = decodeOrFail(wire);
            if (message.kind == SessionMessageKind::AuthResult) {
                QVERIFY(!message.accepted);
                reasons.append(message.reason);
            }
        }
        QVERIFY(!server.hasAuthenticatedSession());
    }

    QCOMPARE(reasons.size(), 3);
    // The first two are ordinary refusals; the third is the rate limiter,
    // which says something DIFFERENT on purpose. Collapsing the two would
    // leak, through the daemon's own answer, which guesses were close.
    QVERIFY2(reasons.at(0).contains(QStringLiteral("did not accept this app's pairing token")),
             qPrintable(reasons.at(0)));
    QVERIFY2(reasons.at(1).contains(QStringLiteral("did not accept this app's pairing token")),
             qPrintable(reasons.at(1)));
    QVERIFY2(reasons.at(2).contains(QStringLiteral("too many wrong ones")),
             qPrintable(reasons.at(2)));

    // Even the RIGHT token is refused while the lockout stands.
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* rawClient = new LoopbackTransport(QStringLiteral("raw-client"), this);
    stationEnd->linkTo(rawClient);
    server.acceptTransport(stationEnd);
    NEREUS_TRY_VERIFY(!rawClient->received().isEmpty());
    rawClient->sendText(SessionMessages::encode(
        SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 6,
                               QStringLiteral("right-token-client"))));
    rawClient->sendText(
        SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(rawClient->receivedKinds().contains(QByteArrayLiteral("auth.result")));
    QVERIFY(!server.hasAuthenticatedSession());
}

// ── Session model ────────────────────────────────────────────────────────

void TstStationSession::secondAuthenticatedConnectionIsAdmittedBesideTheFirst()
{
    // iPhone app Task 71 (R-IOS-02): the remote design's section 7.1
    // preemption is gone. A second window signing in with the token is a
    // second device (a token window) and is admitted beside the first; no
    // sign-in by another device ends a session.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto authenticate = [&](const QString& name, LoopbackTransport** clientEndOut) {
        auto* stationEnd = new LoopbackTransport(name, this);
        auto* clientEnd = new LoopbackTransport(name + QStringLiteral("-client"), this);
        stationEnd->linkTo(clientEnd);
        server.acceptTransport(stationEnd);
        NEREUS_TRY_VERIFY(!clientEnd->received().isEmpty());
        clientEnd->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, name)));
        clientEnd->sendText(
            SessionMessages::encode(SessionMessages::authRequest(server.token())));
        NEREUS_TRY_VERIFY(
            clientEnd->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
        *clientEndOut = clientEnd;
    };

    LoopbackTransport* first = nullptr;
    authenticate(QStringLiteral("first"), &first);
    QVERIFY(server.hasAuthenticatedSession());
    QCOMPARE(server.peerCount(), 1);

    LoopbackTransport* second = nullptr;
    authenticate(QStringLiteral("second"), &second);

    // Both admitted, neither told to go.
    QCOMPARE(server.peerCount(), 2);
    QCOMPARE(server.authenticatedSessionCount(), 2);
    QVERIFY(first->isOpen());
    QVERIFY(second->isOpen());
    for (const QByteArray& wire : first->received()) {
        QVERIFY2(decodeOrFail(wire).kind != SessionMessageKind::SessionEnd,
                 "a second device's sign-in ended the first session");
    }
}

// ── Heartbeat ────────────────────────────────────────────────────────────

void TstStationSession::heartbeatDetectsAPeerThatWentSilentWithoutClosing()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    // The production defaults are 20 s and 2 misses (StationServer.h has
    // the reasoning for both numbers). Driven down here so the slot costs
    // milliseconds; the MECHANISM under test is identical, and the numbers
    // are asserted against the configuration rather than hardcoded, so
    // this stays honest if the defaults ever move.
    server.setHeartbeatIntervalMs(20);
    server.setMaxMissedPongs(2);
    QCOMPARE(server.heartbeatIntervalMs(), 20);
    QCOMPARE(server.maxMissedPongs(), 2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("silent-peer"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy timedOut(&server, &StationServer::peerHeartbeatTimeout);
    server.acceptTransport(stationEnd);
    NEREUS_TRY_VERIFY(!clientEnd->received().isEmpty());

    // THE CASE THIS WHOLE STEP EXISTS FOR. The link stays nominally OPEN;
    // the peer simply stops answering. No close, no write error, nothing
    // for a close-driven implementation to notice. This is what a laptop
    // lid, a cell handoff and a NAT timeout all look like from here, and
    // it is why copying TciServer's ping-without-pong-tracking would not
    // have been enough.
    QVERIFY(clientEnd->isOpen());
    clientEnd->setAnswersPings(false);

    NEREUS_TRY_COMPARE_WITH_TIMEOUT(timedOut.count(), 1, 5000);
    QCOMPARE(timedOut.first().first().toString(), QStringLiteral("silent-peer"));

    // The peer really was dropped, not merely reported.
    NEREUS_TRY_COMPARE(server.peerCount(), 0);
    QVERIFY(!server.hasAuthenticatedSession());

    // The pings really did go out: without them the miss counter could
    // only ever have been driven by something other than the heartbeat.
    QVERIFY2(clientEnd->pingsSeen() >= server.maxMissedPongs(),
             qPrintable(QStringLiteral("only %1 pings reached the peer")
                            .arg(clientEnd->pingsSeen())));
}

void TstStationSession::heartbeatLeavesAnAnsweringPeerAlone()
{
    // The control for the slot above. A detector that fired on every peer
    // would pass that test and be worse than useless.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setHeartbeatIntervalMs(20);
    server.setMaxMissedPongs(2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("healthy-client"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy timedOut(&server, &StationServer::peerHeartbeatTimeout);
    server.acceptTransport(stationEnd);

    // Many more than maxMissedPongs intervals, counted by the pings the
    // heartbeat itself sends rather than by a wait picked by hand. A
    // timeout ends the wait too, so the claim below names it.
    NEREUS_TRY_VERIFY(timedOut.count() > 0
                || clientEnd->pingsSeen() > 4 * server.maxMissedPongs());

    // The claim FIRST, the non-vacuity guard second. Ordered this way
    // deliberately: an implementation that stopped tracking pongs kills
    // this peer, which also stops the pings, so a pingsSeen() check placed
    // first would fire instead and the failure would name the wrong thing.
    QCOMPARE(timedOut.count(), 0);
    QCOMPARE(server.peerCount(), 1);
    QVERIFY2(clientEnd->pingsSeen() > server.maxMissedPongs(),
             "the heartbeat did not actually run during the wait, so the "
             "absence of a timeout above proves nothing");
}

// ── Mirror and settings ──────────────────────────────────────────────────

void TstStationSession::mirrorRoundTripsSliceStateAndDoesNotEcho()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    SliceModel* stationSlice = stationModel->slices().first();
    SliceModel* clientSlice = clientModel.sliceById(stationSlice->sliceIndex());
    QVERIFY(clientSlice != nullptr);

    // Station to client: an ordinary property delta.
    stationEnd->clearReceived();
    stationSlice->setFrequency(7123456.0);
    NEREUS_TRY_COMPARE(clientSlice->frequency(), 7123456.0);

    // THE ECHO GUARD, asserted directly rather than inferred from message
    // volume. Applying an inbound value calls a real setter, which emits a
    // real NOTIFY, which the client's own outbound watcher would forward
    // straight back as a property.write. m_applyingInbound is what stops
    // it. Counting total traffic instead would NOT catch the guard being
    // removed: SliceModel's setters are emit-on-change, so the echo the
    // station receives carries the value it already holds and dies there
    // without generating a reply -- one wasted round trip, invisible in a
    // volume count, and a genuine correctness hole for any property whose
    // setter is not emit-on-change.
    NereusSDR::Test::settleSession();
    QVERIFY2(!stationEnd->receivedKinds().contains(QByteArrayLiteral("property.write")),
             "the client echoed the station's own delta straight back to it");

    // Client to station: the inbound half of the mirror.
    const int before = clientEnd->received().size();
    clientSlice->setFrequency(14074000.0);
    NEREUS_TRY_COMPARE(stationSlice->frequency(), 14074000.0);

    // And no echo storm: applying the station's answer must not produce
    // another outbound write, which would ping-pong forever. Give the
    // event loop several flush intervals to misbehave in.
    NereusSDR::Test::settleSession();
    const int after = clientEnd->received().size();
    QVERIFY2(after - before < 5,
             qPrintable(QStringLiteral("station sent %1 messages after one client write")
                            .arg(after - before)));
    QCOMPARE(stationSlice->frequency(), 14074000.0);
    QCOMPARE(clientSlice->frequency(), 14074000.0);

    // The per-slice S-meter reaches the client. SliceModel::
    // signalStrengthDbm has no Q_PROPERTY WRITE, so this only works
    // through the applyMirroredValue hook task 12 built for exactly this.
    // This station has no WDSP channel, so its SliceMeterPump would
    // overwrite every reading with the no-reading value (R-R3-13) before
    // the mirror carries it. Stop it: these setters stand in for the pump.
    stopSliceMeterPump(stationModel.get());
    stationSlice->setSignalStrengthDbm(-73.0);
    NEREUS_TRY_COMPARE(clientSlice->signalStrengthDbm(), -73.0);
    stationSlice->setSignalPeakDbm(-61.0);
    stationSlice->setSignalAverageDbm(-79.0);
    NEREUS_TRY_COMPARE(clientSlice->signalPeakDbm(), -61.0);
    NEREUS_TRY_COMPARE(clientSlice->signalAverageDbm(), -79.0);

    // Read-only telemetry must never turn into a client command, even if
    // code changes the client's local copy. Only Core is authoritative.
    stationEnd->clearReceived();
    clientSlice->setSignalPeakDbm(-20.0);
    clientSlice->setSignalAverageDbm(-30.0);
    // A real writable property is a flush barrier, avoiding a timed sleep.
    clientSlice->setFrequency(14075100.0);
    NEREUS_TRY_COMPARE(stationSlice->frequency(), 14075100.0);
    for (const QByteArray& wire : stationEnd->received()) {
        const SessionMessage message = decodeOrFail(wire);
        if (message.kind != SessionMessageKind::PropertyWrite) { continue; }
        for (const MirrorUpdate& update : message.updates) {
            QVERIFY(update.name != "signalPeakDbm");
            QVERIFY(update.name != "signalAverageDbm");
        }
    }
    QCOMPARE(stationSlice->signalPeakDbm(), -61.0);
    QCOMPARE(stationSlice->signalAverageDbm(), -79.0);
}

void TstStationSession::autoAgcTelemetryFollowsCoreAcrossReconnect()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
    auto station = makeStationRadioModel(1);
    auto* first = station->slices().at(0);
    auto* second = station->slices().at(1);
    StationServer server(station.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    for (int session = 1; session <= 2; ++session) {
        first->setStationAutoAgcNoiseFloor(-113.0, true, session * 10);
        second->setStationAutoAgcNoiseFloor(-91.0, false, session * 10 + 1);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("agc-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("agc-client"), this);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        NEREUS_TRY_COMPARE(completed.count(), session);
        auto* remoteFirst = remote.sliceById(first->sliceIndex());
        auto* remoteSecond = remote.sliceById(second->sliceIndex());
        QVERIFY(remoteFirst && remoteSecond);
        QCOMPARE(remoteFirst->stationAutoAgcNoiseFloorDbm(), -113.0);
        QVERIFY(remoteFirst->stationAutoAgcNoiseFloorValid());
        QCOMPARE(remoteFirst->stationAutoAgcNoiseFloorGeneration(), quint64(session * 10));
        QCOMPARE(remoteSecond->stationAutoAgcNoiseFloorDbm(), -91.0);
        QVERIFY(!remoteSecond->stationAutoAgcNoiseFloorValid());

        first->setStationAutoAgcNoiseFloor(-108.5, false, session * 10 + 2);
        NEREUS_TRY_COMPARE(remoteFirst->stationAutoAgcNoiseFloorGeneration(), quint64(session * 10 + 2));
        QCOMPARE(remoteFirst->stationAutoAgcNoiseFloorDbm(), -108.5);
        QVERIFY(!remoteFirst->stationAutoAgcNoiseFloorValid());
        first->setStationAutoAgcNoiseFloor(-107.0, true, session * 10 + 2);
        NEREUS_TRY_VERIFY(remoteFirst->stationAutoAgcNoiseFloorValid());
        QCOMPARE(remoteFirst->stationAutoAgcNoiseFloorDbm(), -107.0);
        QCOMPARE(remoteSecond->stationAutoAgcNoiseFloorDbm(), -91.0);

        stationEnd->clearReceived();
        remoteFirst->setStationAutoAgcNoiseFloor(-55.0, true, 999);
        // An actual operator write is the barrier for the client's write
        // flush. None of the telemetry notifies may join that outbound batch.
        remoteFirst->setFrequency(14080000.0 + session * 100.0);
        NEREUS_TRY_COMPARE(first->frequency(), remoteFirst->frequency());
        for (const QByteArray& wire : stationEnd->received()) {
            const auto message = decodeOrFail(wire);
            if (message.kind != SessionMessageKind::PropertyWrite) { continue; }
            for (const auto& update : message.updates) {
                QVERIFY(!update.name.startsWith("stationAutoAgc"));
            }
        }
        QCOMPARE(first->stationAutoAgcNoiseFloorDbm(), -107.0);
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QVERIFY(!remoteFirst->stationAutoAgcNoiseFloorValid());
        QVERIFY(!remoteSecond->stationAutoAgcNoiseFloorValid());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

void TstStationSession::filterTelemetryFollowsCoreAcrossReconnect()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto station = makeStationRadioModel(1);
    auto* slice0 = station->slices().at(0);
    auto* slice1 = station->slices().at(1);
    // The allocator/codec normally publishes these coordinates. This test
    // isolates their already-existing mirror from the physical routing tests.
    slice0->setStreamIndex(0);
    slice0->setChainIndex(0);
    slice1->setStreamIndex(1);
    slice1->setChainIndex(1);
    station->alexControllerMutable().setBpfMode(1, AlexController::BpfMode::ForceBypass);
    StationServer server(station.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QVERIFY(!remote.filterChainStateAvailable(1));
    QVERIFY(!station->applyStationFilterValue("rxFilter1Effective", 0));

    for (int session = 1; session <= 2; ++session) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("filter-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("filter-client"), this);
        stationEnd->linkTo(clientEnd);
        auto snapshotSeen = std::make_shared<bool>(false);
        auto presentedEarly = std::make_shared<bool>(false);
        connect(clientEnd, &SessionTransport::textReceived, clientEnd,
                [snapshotSeen](const QByteArray& wire) {
            if (decodeOrFail(wire).kind == SessionMessageKind::SnapshotComplete) {
                *snapshotSeen = true;
            }
        });
        connect(&remote, &RadioModel::filterStateChanged, clientEnd,
                [&remote, snapshotSeen, presentedEarly] {
            if (!*snapshotSeen && remote.filterChainStateAvailable(1)) {
                *presentedEarly = true;
            }
        });
        client.startSession(clientEnd, server.token());
        QVERIFY(!remote.filterChainStateAvailable(1));
        server.acceptTransport(stationEnd);
        NEREUS_TRY_COMPARE(completed.count(), session);
        QVERIFY(*snapshotSeen);
        QVERIFY(!*presentedEarly);
        QVERIFY(remote.filterChainStateAvailable(0));
        QVERIFY(remote.filterChainStateAvailable(1));
        QCOMPARE(remote.rxFilter1Reason(), station->rxFilter1Reason());
        QCOMPARE(remote.rxFilter1Effective(), int(AlexController::BpfEffective::Bypass));
        QCOMPARE(remote.sliceChainIndex(slice1->sliceIndex()), 1);
        QVERIFY(!remote.panBypassState({slice0->sliceIndex()}).bypassed);
        QVERIFY(remote.panBypassState({slice1->sliceIndex()}).bypassed);
        QVERIFY(remote.panBypassState({slice1->sliceIndex()}).reason.contains("Filter Policy"));

        // A client-local Alex change must not replace the station's answer.
        remote.alexControllerMutable().setWidebandActive(0, true);
        QVERIFY(!remote.panBypassState({slice0->sliceIndex()}).bypassed);
        station->alexControllerMutable().setWidebandActive(1, true);
        NEREUS_TRY_COMPARE(remote.rxFilter1Effective(), int(AlexController::BpfEffective::WidebandLocked));
        QVERIFY(remote.panBypassState({slice1->sliceIndex()}).reason.contains("more spectrum"));
        station->alexControllerMutable().setWidebandActive(1, false);
        NEREUS_TRY_COMPARE(remote.rxFilter1Effective(), int(AlexController::BpfEffective::Bypass));

        stationEnd->clearReceived();
        QVERIFY(remote.applyStationFilterValue("rxFilter1Reason", QStringLiteral("client-only")));
        QVERIFY(!remote.applyStationFilterValue("rxFilter1Effective", 99));
        auto* clientSlice = remote.sliceById(slice0->sliceIndex());
        QVERIFY(clientSlice);
        clientSlice->setFrequency(14075000.0 + session * 100.0);
        NEREUS_TRY_COMPARE(slice0->frequency(), clientSlice->frequency());
        for (const QByteArray& wire : stationEnd->received()) {
            const auto message = decodeOrFail(wire);
            if (message.kind != SessionMessageKind::PropertyWrite) { continue; }
            for (const auto& update : message.updates) {
                QVERIFY(!update.name.startsWith("rxFilter"));
            }
        }
        QVERIFY(station->rxFilter1Reason() != QStringLiteral("client-only"));
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QVERIFY(!remote.filterChainStateAvailable(1));
        QVERIFY(!remote.panBypassState({slice1->sliceIndex()}).bypassed);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

void TstStationSession::settingsProxyIsNotReadyBeforeTheSnapshot()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    // A Station-classified key with a distinctive value.
    stationSettings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50123"));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    // Task 15's handoff, restated as an assertion: several model
    // constructors do contains()-then-seed against Station prefixes, and
    // the ONLY thing stopping them writing ship defaults into the
    // station's store is that writes are dropped while not ready.
    QVERIFY(!proxy.ready());
    QVERIFY(!proxy.hasReceivedSnapshot());

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    QVERIFY(proxy.ready());
    QVERIFY(proxy.hasReceivedSnapshot());
    QVERIFY(proxy.setupDialogAllowed() || !proxy.hasNonEmptySnapshot());
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("50001")).toString(),
             QStringLiteral("50123"));

    // ── Fix round 1, Important 2 ─────────────────────────────────────────
    //
    // A client write must reach the station's own store THROUGH THE REAL
    // CLIENT. An earlier version of this slot hand-relayed the frame and
    // attributed the wiring to Task 20, which contradicted both records
    // that assign it here (SettingsProxy.h's own "for a live session (Task
    // 18) to relay over", and the plan's self-review). Nothing in src/
    // consumed outboundWriteRequested or outboundRemoveRequested, so the
    // operator-visible shape was: a remote GUI's Setup change updates the
    // optimistic cache, appears to take, never reaches the station, and is
    // silently reverted by the next snapshot.
    //
    // Hand-relaying here would pass against exactly that broken build,
    // which is why this now goes through proxy.setValue() and nothing else.
    QSignalSpy outbound(&proxy, &SettingsProxy::outboundWriteRequested);
    proxy.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50999"));
    QCOMPARE(outbound.count(), 1);
    NEREUS_TRY_COMPARE(stationSettings.value(QStringLiteral("StationCallsign")).toString(),
                 QStringLiteral("50999"));

    // The removal half of the same seam.
    QVERIFY(stationSettings.contains(QStringLiteral("StationCallsign")));
    proxy.remove(QStringLiteral("StationCallsign"));
    NEREUS_TRY_VERIFY(!stationSettings.contains(QStringLiteral("StationCallsign")));
}

// Whole-branch review, Important 4. A settings remove on the station used
// to arrive at the client as "set to empty string", because the daemon's
// change hook reports every mutation the same way: it emits
// outboundValueChanged(key, m_appSettings.value(key), ...) and value() on
// an absent key returns an INVALID QVariant, which StationServer then
// flattened with value.toString() into "". The client cached that, so
// contains() stayed true and value(key, someDefault) returned "" rather
// than the caller's default, while the station said absent.
//
// This is the same invariant an earlier fix round removed inside one
// process (AppSettings::remove's own "actually gone, not just hidden"),
// reintroduced at the wire seam. Reachable operator actions on a remote
// GUI: deleting a mic profile (roughly 91 keys), shrinking the notch list
// (whose own prune exists so a later grow cannot read stale values back,
// which the ghost defeats exactly), and Diagnostics cleanup on a
// non-BPF1 board.
//
// Driven end to end through the real StationServer, the real wire codec
// and the real StationClient, because the flattening happened in the
// relay and no unit-level test of either half could see it.
void TstStationSession::aRemovedStationSettingReachesTheClientAsAbsenceNotAnEmptyString()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50123"));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("fallback")).toString(),
             QStringLiteral("50123"));
    QVERIFY(proxy.contains(QStringLiteral("StationCallsign")));

    // ── Station to client ────────────────────────────────────────────────
    // A removal on the daemon, for any reason of its own.
    stationSettings.remove(QStringLiteral("StationCallsign"));

    NEREUS_TRY_VERIFY2(!proxy.contains(QStringLiteral("StationCallsign")),
                 "the client still holds a key the station removed");
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("fallback")).toString(),
             QStringLiteral("fallback"));
    QVERIFY2(!proxy.handledKeys().contains(QStringLiteral("StationCallsign")),
             "a removed key must not still be listed");

    // ── Client to station, and the echo back ─────────────────────────────
    // The direction an operator actually triggers. The client removes it
    // locally and immediately, the station catches up, and then the
    // station's own broadcast for that removal comes BACK to this client.
    // That echo is what used to resurrect the key as an empty string,
    // undoing a removal the client had already performed correctly.
    stationSettings.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));
    NEREUS_TRY_COMPARE(proxy.value(QStringLiteral("Slice0/Locked"), QString()).toString(),
                 QStringLiteral("True"));

    proxy.remove(QStringLiteral("Slice0/Locked"));
    QVERIFY(!proxy.contains(QStringLiteral("Slice0/Locked")));
    NEREUS_TRY_VERIFY(!stationSettings.contains(QStringLiteral("Slice0/Locked")));

    // Several flush intervals for the echo to land and misbehave in.
    NereusSDR::Test::settleSession();
    QVERIFY2(!proxy.contains(QStringLiteral("Slice0/Locked")),
             "the station's echo resurrected the key the client just removed");
    QCOMPARE(proxy.value(QStringLiteral("Slice0/Locked"), QStringLiteral("fallback")).toString(),
             QStringLiteral("fallback"));
}

void TstStationSession::schemaSkewIsCaughtByNameComparison()
{
    // Task 18 step 8. MirrorSchema's ordinals are dense and per-class, so
    // two builds declaring different property sets assign DIFFERENT
    // ordinals to the SAME names. Comparing ordinals would therefore
    // report noise; comparing NAMES reports the actual difference, which
    // is why every MirrorUpdate carries its name as well as its ordinal.
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* fakeStationEnd = new LoopbackTransport(QStringLiteral("fake-station"), this);
    auto* realClientEnd = new LoopbackTransport(QStringLiteral("client"), this);
    fakeStationEnd->linkTo(realClientEnd);
    client.startSession(realClientEnd, QStringLiteral("token"));

    fakeStationEnd->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
    fakeStationEnd->sendText(
        SessionMessages::encode(
            SessionMessages::authResult(true, QString(), /*retryable=*/false)));

    StationCapabilities caps;
    caps.stationName = QStringLiteral("Skewed");
    caps.board = HPSDRHW::HermesLite;
    caps.radioConnected = true;
    caps.effectiveMaxSlices = 2;
    caps.boardMaxSlices = 5;
    caps.settingsSchemaVersion = 6;
    fakeStationEnd->sendText(
        SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));

    NEREUS_TRY_VERIFY(client.mirroredObject(QByteArrayLiteral("radio")) != nullptr);

    // A schema for RadioModel carrying a property this build has never
    // heard of, and omitting one it does have.
    const MirrorSchema& local = MirrorSchema::forObject(&clientModel);
    QVERIFY(local.size() > 1);
    QList<SessionSchemaField> fields;
    quint16 ordinal = 0;
    for (const MirrorProperty& prop : local.properties()) {
        if (prop.name == local.properties().first().name) {
            continue; // omit exactly one real property
        }
        fields.append(SessionSchemaField{ ordinal++, prop.name, prop.kind });
    }
    fields.append(SessionSchemaField{ ordinal, QByteArrayLiteral("aPropertyFromTheFuture"),
                                      MirrorWireKind::Int64 });

    fakeStationEnd->sendText(SessionMessages::encode(
        SessionMessages::schema(QByteArrayLiteral("RadioModel"), fields)));

    NEREUS_TRY_VERIFY(!client.schemaNamesOnlyOnStation().isEmpty());
    QVERIFY(client.schemaNamesOnlyOnStation().contains(
        QByteArrayLiteral("RadioModel.aPropertyFromTheFuture")));
    QVERIFY(client.schemaNamesOnlyLocal().contains(
        QByteArrayLiteral("RadioModel.") + local.properties().first().name));

    // Skew of the AppSettings schema version is caught at handshake too,
    // by the same kind of name-keyed comparison (both ends read the value
    // stored under the literal key "SettingsSchemaVersion" in their own
    // store), and is reported rather than refused.
    QVERIFY(client.stationSettingsSchemaVersion() == 6);
    QCOMPARE(client.hasSettingsSchemaSkew(),
             client.localSettingsSchemaVersion() != 6);
}

// ── Fix round 1 ──────────────────────────────────────────────────────────

void TstStationSession::reconnectSurvivesTheOldTransportClosing()
{
    // Important 3. attachTransport() used to overwrite m_transport with no
    // disconnect and no deleteLater, and onTransportClosed() took no
    // sender argument, so it could not tell WHICH link had closed.
    // WebSocketTransport::closeLink is asynchronous, so the real sequence
    // -- heartbeat timeout, reconnect from the slot, the old socket's
    // disconnected arrives a moment later -- drove the BRAND NEW session
    // to Disconnected, stopped both timers and called setReady(false).
    // Task 19 is reconnect and walks straight into it.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    // First session, established normally. Held through QPointers because
    // the whole point of the fix is that the client DESTROYS the stale
    // transport, so raw pointers here would dangle.
    auto* firstStation = new LoopbackTransport(QStringLiteral("station-1"), this);
    auto* firstClient = new LoopbackTransport(QStringLiteral("client-1"), this);
    QPointer<LoopbackTransport> staleStation(firstStation);
    QPointer<LoopbackTransport> staleClient(firstClient);
    firstStation->linkTo(firstClient);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(firstClient, server.token());
    server.acceptTransport(firstStation);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(clientModel.isConnected());

    // Reconnect on a fresh pair WITHOUT closing the old one first, which
    // is exactly what a reconnect-from-the-timeout-slot looks like.
    auto* secondStation = new LoopbackTransport(QStringLiteral("station-2"), this);
    auto* secondClient = new LoopbackTransport(QStringLiteral("client-2"), this);
    secondStation->linkTo(secondClient);
    client.startSession(secondClient, server.token());
    server.acceptTransport(secondStation);
    NEREUS_TRY_COMPARE(completed.count(), 2);
    QVERIFY(clientModel.isConnected());
    QVERIFY(proxy.ready());

    // The stale transport must have been RELEASED, not merely orphaned.
    // attachTransport() disconnects it from this client, closes it and
    // deleteLater()s it; the old code overwrote m_transport and did none
    // of the three, which leaked a transport (and, over a real socket, a
    // QWebSocket still connected to onTransportText) on every reconnect.
    NEREUS_TRY_VERIFY2(staleClient.isNull(),
                 "the stale transport was orphaned rather than released");

    // And if anything of the old link is still around to make noise, it
    // must not reach the live session. Under the fix there is nothing left
    // to poke, which is itself the assertion above; this covers the case
    // where a late close still arrives from the far end.
    if (!staleStation.isNull()) {
        staleStation->closeLink(QStringLiteral("stale link finally closing"));
    }
    NereusSDR::Test::settleSession();

    QVERIFY2(client.isHandshakeComplete(),
             "a stale transport's close tore down the fresh session");
    QVERIFY2(clientModel.isConnected(),
             "a stale transport's close drove the fresh session to Disconnected");
    QVERIFY2(proxy.ready(),
             "a stale transport's close called setReady(false) on the fresh session");
}

void TstStationSession::heartbeatTimeoutReportsTheSessionAsEnded()
{
    // Important 4, the half that needs no socket. onTransportClosed() used
    // to emit sessionEnded only `if (m_handshakeComplete)`, and
    // disconnectFromStation() cleared that flag before the close handler
    // read it, so the client's own heartbeat timeout reported
    // stationHeartbeatTimeout and then went silent.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    // Keep the station's own heartbeat out of the way; this slot is about
    // the CLIENT's.
    server.setHeartbeatIntervalMs(0);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHeartbeatIntervalMs(20);
    client.setMaxMissedPongs(2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy timedOut(&client, &StationClient::stationHeartbeatTimeout);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    // The station goes silent without closing.
    stationEnd->setAnswersPings(false);

    NEREUS_TRY_COMPARE_WITH_TIMEOUT(timedOut.count(), 1, 5000);
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 2000);
    QCOMPARE(ended.first().first().toString(), QStringLiteral("heartbeat timeout"));
    QVERIFY(!clientModel.isConnected());
    QVERIFY(!proxy.ready());

    // Exactly once, no matter how many close paths unwind afterwards.
    NereusSDR::Test::settleSession();
    QCOMPARE(ended.count(), 1);
}

void TstStationSession::tunerPropertiesHydrateWithoutClientCommands()
{
    // R-R3-22/25: the station's whole tuner property bag is telemetry on the
    // client. In particular, isOperate/isBypass/antennaA must not be routed
    // through TunerModel::applyMirroredValue(), because that is the daemon's
    // command hook and calls the native TGXL command slots. isTuning=true is
    // also an ordinary snapshot value here, not permission to start a local
    // tune-carrier cycle.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    TunerModel* const stationTuner = stationModel->tunerModel();
    QVERIFY(stationTuner != nullptr);
    stationModel->tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));
    stationTuner->applyStatus({
        {QStringLiteral("relayC1"), QStringLiteral("42")},
        {QStringLiteral("relayL"), QStringLiteral("199")},
        {QStringLiteral("relayC2"), QStringLiteral("88")},
        {QStringLiteral("operate"), QStringLiteral("1")},
        {QStringLiteral("bypass"), QStringLiteral("1")},
        {QStringLiteral("tuning"), QStringLiteral("1")},
        {QStringLiteral("antA"), QStringLiteral("2")},
        {QStringLiteral("3way"), QStringLiteral("1")},
        {QStringLiteral("model"), QStringLiteral("TunerGenius")},
        {QStringLiteral("ip"), QStringLiteral("192.0.2.34")},
        {QStringLiteral("fwd"), QStringLiteral("12.5")},
        {QStringLiteral("swr"), QStringLiteral("1.4")},
    });
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy clientTgxlFrames(clientModel.tgxlConnection(),
                                &TgxlConnection::testFrameWrittenForTesting);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    TunerModel* const clientTuner = clientModel.tunerModel();
    QVERIFY(clientTuner != nullptr);
    QCOMPARE(clientTuner->relayC1(), 42);
    QCOMPARE(clientTuner->relayL(), 199);
    QCOMPARE(clientTuner->relayC2(), 88);
    QVERIFY(clientTuner->isOperate());
    QVERIFY(clientTuner->isBypass());
    QVERIFY(clientTuner->isTuning());
    QCOMPARE(clientTuner->antennaA(), 2);
    QVERIFY(clientTuner->hasAntennaSwitch());
    QVERIFY(clientTuner->isPresent());
    QVERIFY(clientTuner->hasDirectConnection());
    QCOMPARE(clientTuner->tgxlIp(), QStringLiteral("192.0.2.34"));
    QCOMPARE(clientTuner->fwdPower(), 12.5f);
    QCOMPARE(clientTuner->swr(), 1.4f);
    QCOMPARE(clientTgxlFrames.count(), 0);

    const QSet<QByteArray> unapplied = client.unappliedProperties();
    for (const QByteArray& property : {
             QByteArrayLiteral("relayC1"), QByteArrayLiteral("relayL"),
             QByteArrayLiteral("relayC2"), QByteArrayLiteral("isOperate"),
             QByteArrayLiteral("isBypass"), QByteArrayLiteral("isTuning"),
             QByteArrayLiteral("antennaA"), QByteArrayLiteral("hasAntennaSwitch"),
             QByteArrayLiteral("isPresent"), QByteArrayLiteral("hasDirectConnection"),
             QByteArrayLiteral("tgxlIp"), QByteArrayLiteral("fwdPower"),
             QByteArrayLiteral("swr")}) {
        QVERIFY2(!unapplied.contains(QByteArrayLiteral("TunerModel.") + property),
                 property.constData());
    }

    // False and zero are state, not "missing" values. Exercise a live delta
    // after the non-default snapshot so the client must actively clear them.
    stationTuner->applyStatus({
        {QStringLiteral("relayC1"), QStringLiteral("0")},
        {QStringLiteral("relayL"), QStringLiteral("0")},
        {QStringLiteral("relayC2"), QStringLiteral("0")},
        {QStringLiteral("operate"), QStringLiteral("0")},
        {QStringLiteral("bypass"), QStringLiteral("0")},
        {QStringLiteral("tuning"), QStringLiteral("0")},
        {QStringLiteral("antA"), QStringLiteral("0")},
        {QStringLiteral("3way"), QStringLiteral("0")},
        {QStringLiteral("fwd"), QStringLiteral("0")},
        {QStringLiteral("swr"), QStringLiteral("0")},
    });
    NEREUS_TRY_COMPARE(clientTuner->relayC1(), 0);
    NEREUS_TRY_COMPARE(clientTuner->relayL(), 0);
    NEREUS_TRY_COMPARE(clientTuner->relayC2(), 0);
    NEREUS_TRY_VERIFY(!clientTuner->isOperate());
    NEREUS_TRY_VERIFY(!clientTuner->isBypass());
    NEREUS_TRY_VERIFY(!clientTuner->isTuning());
    NEREUS_TRY_COMPARE(clientTuner->antennaA(), 0);
    NEREUS_TRY_VERIFY(!clientTuner->hasAntennaSwitch());
    NEREUS_TRY_COMPARE(clientTuner->fwdPower(), 0.0f);
    NEREUS_TRY_COMPARE(clientTuner->swr(), 0.0f);
    QCOMPARE(clientTgxlFrames.count(), 0);

    // And the property that genuinely DOES land is not swept into the set
    // along with them: SliceModel::signalStrengthDbm reaches its own plain
    // setter through the hook, which is the one pair the client allowlists.
    SliceModel* stationSlice = stationModel->slices().first();
    SliceModel* clientSlice = clientModel.sliceById(stationSlice->sliceIndex());
    QVERIFY(clientSlice != nullptr);
    // No WDSP channel on this station: stop the pump that would write the
    // no-reading value over the reading this setter stands in for (R-R3-13).
    stopSliceMeterPump(stationModel.get());
    stationSlice->setSignalStrengthDbm(-91.0);
    NEREUS_TRY_COMPARE(clientSlice->signalStrengthDbm(), -91.0);
    QVERIFY(!client.unappliedProperties().contains(
        QByteArrayLiteral("SliceModel.signalStrengthDbm")));
}

void TstStationSession::remoteTgxlStateClearsOnSessionLossRetainingConfiguredEndpoint()
{
    // The state arrives through the actual authenticated snapshot path.  A
    // remote GUI must not retain an admitted device or its live telemetry
    // after that session ends, but its configured endpoint remains a useful
    // draft for the next station connection.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    TunerModel* const stationTuner = stationModel->tunerModel();
    QVERIFY(stationTuner != nullptr);
    TunerModel::StationConnectionState state;
    state.configuredHost = QStringLiteral("tgxl.example.test");
    state.configuredPort = 9010;
    state.phase = TunerModel::ConnectionPhase::Connected;
    state.peerAddress = QStringLiteral("192.0.2.34");
    state.deviceModel = QStringLiteral("TunerGeniusXL");
    state.deviceSerial = QStringLiteral("241288-1");
    state.deviceVersion = QStringLiteral("1.2.17");
    state.deviceNickname = QStringLiteral("Station TGXL");
    stationTuner->setStationConnectionState(state);
    stationTuner->applyStatus({
        {QStringLiteral("relayC1"), QStringLiteral("42")},
        {QStringLiteral("relayL"), QStringLiteral("199")},
        {QStringLiteral("relayC2"), QStringLiteral("88")},
        {QStringLiteral("operate"), QStringLiteral("1")},
        {QStringLiteral("bypass"), QStringLiteral("1")},
        {QStringLiteral("tuning"), QStringLiteral("1")},
        {QStringLiteral("antA"), QStringLiteral("2")},
        {QStringLiteral("3way"), QStringLiteral("1")},
        {QStringLiteral("fwd"), QStringLiteral("12.5")},
        {QStringLiteral("swr"), QStringLiteral("1.4")},
    });
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("tgxl-state-station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("tgxl-state-client"), this);
    stationEnd->linkTo(clientEnd);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_VERIFY(client.isHandshakeComplete());

    TunerModel* const clientTuner = clientModel.tunerModel();
    QVERIFY(clientTuner != nullptr);
    NEREUS_TRY_VERIFY(clientTuner->hasDirectConnection());
    QVERIFY(clientTuner->isPresent());
    QCOMPARE(clientTuner->configuredHost(), state.configuredHost);
    QCOMPARE(clientTuner->configuredPort(), int(state.configuredPort));
    QCOMPARE(clientTuner->deviceSerial(), state.deviceSerial);
    QCOMPARE(clientTuner->fwdPower(), 12.5f);
    QCOMPARE(clientTuner->swr(), 1.4f);

    clientEnd->closeLink(QStringLiteral("station link lost"));
    NEREUS_TRY_VERIFY(!client.isHandshakeComplete());
    QVERIFY(!clientTuner->hasDirectConnection());
    QVERIFY(!clientTuner->isPresent());
    QCOMPARE(clientTuner->connectionPhase(), TunerModel::ConnectionPhase::Disconnected);
    QCOMPARE(clientTuner->configuredHost(), state.configuredHost);
    QCOMPARE(clientTuner->configuredPort(), int(state.configuredPort));
    QVERIFY(clientTuner->connectionError().isEmpty());
    QVERIFY(clientTuner->deviceModel().isEmpty());
    QVERIFY(clientTuner->deviceSerial().isEmpty());
    QVERIFY(clientTuner->deviceVersion().isEmpty());
    QVERIFY(clientTuner->deviceNickname().isEmpty());
    QVERIFY(clientTuner->tgxlIp().isEmpty());
    QCOMPARE(clientTuner->relayC1(), 0);
    QCOMPARE(clientTuner->relayL(), 0);
    QCOMPARE(clientTuner->relayC2(), 0);
    QVERIFY(!clientTuner->isOperate());
    QVERIFY(!clientTuner->isBypass());
    QVERIFY(!clientTuner->isTuning());
    QCOMPARE(clientTuner->antennaA(), 0);
    QVERIFY(!clientTuner->hasAntennaSwitch());
    QCOMPARE(clientTuner->fwdPower(), 0.0f);
    QCOMPARE(clientTuner->swr(), 1.0f);
}

void TstStationSession::receiveOnlyStationBlocksRemoteBandRecall()
{
    // R-R3-25 must hold through the real authenticated property-write path,
    // not just for a direct unit-test call on the station slice. The daemon's
    // RadioModel is Role::Local because it owns the hardware, so the durable
    // receive-only station policy is what distinguishes it from desktop-local
    // direct mode.
    AppSettings& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("TGXL_AutoTuneMemoryRecall"),
                      QStringLiteral("True"));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    SliceModel* const stationSlice = stationModel->slices().first();
    stationSlice->setFrequency(14200000.0);
    stationModel->tuneMemoryStore()->store(
        TuneMemory{1, Band::Band40m, 4, 5, 6, 1});
    stationModel->tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));
    QSignalSpy tgxlFrames(stationModel->tgxlConnection(),
                         &TgxlConnection::testFrameWrittenForTesting);

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(stationModel->receiveOnlyStationPolicy(),
             "a receive-only StationServer must protect a standalone local-role model");

    MoxController* const stationMox = stationModel->moxController();
    QVERIFY(stationMox != nullptr);
    QSignalSpy moxRefused(stationMox, &MoxController::moxRejected);
    QSignalSpy tuneRefused(stationModel.get(), &RadioModel::tuneRefused);

    stationMox->setMox(true);
    QCOMPARE(moxRefused.count(), 1);
    QVERIFY(!stationMox->isMox());
    QVERIFY(stationMox->state() == MoxState::Rx);

    stationModel->setTune(true);
    QCOMPARE(tuneRefused.count(), 1);
    QVERIFY(!stationModel->isTune());
    QVERIFY(!stationMox->isManualMox());

    stationModel->startTgxlAutotune(/*fromHardware=*/false);
    QCOMPARE(tuneRefused.count(), 2);
    QVERIFY(!stationModel->isTune());
    QVERIFY(!stationMox->isMox());
    QCOMPARE(tgxlFrames.count(), 0);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    SliceModel* const clientSlice = clientModel.sliceById(stationSlice->sliceIndex());
    QVERIFY(clientSlice != nullptr);
    clientSlice->setFrequency(7100000.0);

    NEREUS_TRY_COMPARE(stationSlice->frequency(), 7100000.0);
    QCOMPARE(tgxlFrames.count(), 0);

    // Session teardown must never lift the daemon's persistent policy.
    clientEnd->closeLink(QStringLiteral("test session complete"));
    NEREUS_TRY_VERIFY(!clientModel.isConnected());
    QVERIFY(stationModel->receiveOnlyStationPolicy());
    stationMox->setMox(true);
    QCOMPARE(moxRefused.count(), 2);
    QVERIFY(!stationMox->isMox());

    // Restore the singleton keys this accessory fixture owns. Each test
    // process has an isolated profile, but leaving state behind inside the
    // same binary would make later slots order-dependent.
    stationModel->tuneMemoryStore()->clear(1, Band::Band40m);
    settings.remove(QStringLiteral("TGXL_AutoTuneMemoryRecall"));
}

void TstStationSession::receiveOnlyStationRefusesTransmitKeyingWrites()
{
    // R-R3-25 applies at the authenticated StationServer boundary too.
    // TransmitModel is mirrored bidirectionally for later phases (mox and
    // tune from the Core only, iPhone app plan Task 35), but an R3
    // receive-only server must reject the keying writes (mox, tune) and
    // return authoritative accepted-state results rather than adopting the
    // client's optimistic state. R-R3-49 (parity Task 1): a transmit
    // setting such as power is taken while the radio is off the air
    // (transmitSettingsVersion 1; tst_transmit_settings_gate has the rest).
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY(stationModel->receiveOnlyStationPolicy());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    TransmitModel& stationTx = stationModel->transmitModel();
    TransmitModel& clientTx = clientModel.transmitModel();
    const int settledPower = stationTx.power();
    const int requestedPower = settledPower == 17 ? 18 : 17;
    QVERIFY(!stationTx.isMox());
    QVERIFY(!stationTx.isTune());
    QCOMPARE(clientTx.power(), settledPower);

    stationEnd->clearReceived();
    clientEnd->clearReceived();
    clientTx.setMox(true);
    clientTx.setTune(true);
    clientTx.setPower(requestedPower);

    NEREUS_TRY_VERIFY(stationEnd->receivedKinds().contains(
        QByteArrayLiteral("property.write")));

    // These are model-state assertions. This fixture does not claim that a
    // radio socket emitted RF in the uncorrected implementation.
    QVERIFY(!stationTx.isMox());
    QVERIFY(!stationTx.isTune());
    NEREUS_TRY_COMPARE(stationTx.power(), requestedPower);

    NEREUS_TRY_VERIFY(clientEnd->receivedKinds().contains(QByteArrayLiteral("property.result")));
    NEREUS_TRY_COMPARE(clientTx.power(), requestedPower);
    // iPhone app plan Task 35: mox and tune travel from the Core only
    // (MirrorPolicy Outbound), so the window never sends them: a device
    // keys with the transmit verbs. Only the power reached the Core.
    for (const QByteArray& wire : stationEnd->received()) {
        if (!wire.contains("\"property.write\"")) {
            continue;
        }
        QVERIFY2(!wire.contains("\"mox\"") && !wire.contains("\"tune\""), wire.constData());
    }
}

void TstStationSession::nr3CannotRunIsRefusedOnTheCoreAndInTheWindow()
{
    // Fix wave I3 (R-R3-21). A Core with no usable NR3 model file cannot run
    // NR3 (WDSP would pass the audio through unchanged). It says so: turning
    // NR3 on is refused with the plain sentence on the Core, and in a remote
    // window, which learns it through the mirrored nr3Runnable flag and
    // refuses without asking the Core. Other reducers still turn on. Once
    // the Core has a model again, the window turns NR3 on.
    DspAssetService::setBundledNr3ModelPathsForTest([](const QString&) { return QString(); });
    const auto restorePaths = qScopeGuard([] {
        DspAssetService::setBundledNr3ModelPathsForTest({});
    });
    const QString none =
        QStringLiteral("No NR3 model file was found on this Core, so NR3 cannot run.");
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    DspAssetService* core = stationModel->dspAssets();
    QVERIFY(!core->nr3Runnable());
    QCOMPARE(core->nr3ModelStatus(), none);

    SliceModel* coreSlice = stationModel->slices().constFirst();
    coreSlice->setActiveNr(NrSlot::Off);
    QSignalSpy coreRefused(coreSlice, &SliceModel::nrSelectionRefused);
    coreSlice->setActiveNr(NrSlot::NR3);
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    QCOMPARE(coreRefused.count(), 1);
    QCOMPARE(coreRefused.constFirst().at(0).toString(), none);
    QCOMPARE(coreSlice->nnrLastError(), none);

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    DspAssetService* window = clientModel.dspAssets();
    NEREUS_TRY_VERIFY(!window->nr3Runnable());
    NEREUS_TRY_COMPARE(window->nr3ModelStatus(), none);
    NEREUS_TRY_VERIFY(!clientModel.slices().isEmpty());
    SliceModel* windowSlice = clientModel.slices().constFirst();
    QSignalSpy windowRefused(windowSlice, &SliceModel::nrSelectionRefused);
    windowSlice->setActiveNr(NrSlot::NR3);
    QCOMPARE(windowSlice->activeNr(), NrSlot::Off);
    QCOMPARE(windowRefused.count(), 1);
    QCOMPARE(windowRefused.constFirst().at(0).toString(), none);
    NereusSDR::Test::settleSession();
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);

    // Another reducer still turns on from the window.
    windowSlice->setActiveNr(NrSlot::NR2);
    QCOMPARE(windowSlice->activeNr(), NrSlot::NR2);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::NR2);

    // The model comes back: the Core says so and the window turns NR3 on.
    DspAssetService::setBundledNr3ModelPathsForTest({});
    QVERIFY(core->applyNr3Model());
    QVERIFY(core->nr3Runnable());
    NEREUS_TRY_VERIFY(window->nr3Runnable());
    windowSlice->setActiveNr(NrSlot::NR3);
    QCOMPARE(windowSlice->activeNr(), NrSlot::NR3);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::NR3);
    QCOMPARE(windowRefused.count(), 1);

    windowSlice->setActiveNr(NrSlot::Off);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::Off);
    AppSettings::instance().remove(QStringLiteral("DspAssets/Nr3Model"));
}

void TstStationSession::dfnrCannotRunIsRefusedOnTheCoreAndInTheWindow()
{
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 3). A Core that cannot run
    // DFNR (no DeepFilterNet model file, or a build without DFNR) says so
    // through the mirrored dfnrRunnable and dfnrModelStatus: turning DFNR
    // on is refused on the Core and in a remote window with the Core's
    // sentence. A Core that finds its model failing at a channel's first
    // selection says so mid-session, and a window's DFNR turns off.
    ModelPaths::setDfnrModelTarballForTest(QString());
    const auto restorePath = qScopeGuard([] { ModelPaths::clearDfnrModelTarballForTest(); });
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    DspAssetService* core = stationModel->dspAssets();
    QVERIFY(!core->dfnrRunnable());
    const QString reason = core->dfnrModelStatus();
    QVERIFY(!reason.isEmpty());

    SliceModel* coreSlice = stationModel->slices().constFirst();
    coreSlice->setActiveNr(NrSlot::Off);
    coreSlice->setActiveNr(NrSlot::DFNR);
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    QCOMPARE(coreSlice->nnrLastError(), reason);

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QCOMPARE(server.buildCapabilities().dspAssetVersion, 4);
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    DspAssetService* window = clientModel.dspAssets();
    NEREUS_TRY_VERIFY(!window->dfnrRunnable());
    NEREUS_TRY_COMPARE(window->dfnrModelStatus(), reason);
    NEREUS_TRY_VERIFY(!clientModel.slices().isEmpty());
    SliceModel* windowSlice = clientModel.slices().constFirst();
    QSignalSpy windowRefused(windowSlice, &SliceModel::nrSelectionRefused);
    windowSlice->setActiveNr(NrSlot::DFNR);
    QCOMPARE(windowSlice->activeNr(), NrSlot::Off);
    QCOMPARE(windowRefused.count(), 1);
    QCOMPARE(windowRefused.constFirst().at(0).toString(), reason);
    NereusSDR::Test::settleSession();
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    windowSlice->setActiveNr(NrSlot::NR2);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::NR2);
    windowSlice->setActiveNr(NrSlot::Off);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::Off);
    client.disconnectFromStation(QStringLiteral("test complete"));
}

void TstStationSession::mnrCannotRunIsRefusedOnTheCoreAndInTheWindow()
{
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 4). MNR runs only on a Mac. A
    // Core that cannot run it says so through the mirrored mnrRunnable and
    // mnrStatus: turning MNR on is refused on the Core and in a remote
    // window with the Core's sentence, whatever computer the window runs
    // on. On a Mac Core the test sets the Core's availability as a Linux
    // Core's start does.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    DspAssetService* core = stationModel->dspAssets();
    const QString reason = RadioModel::mnrCannotRunReason();
#ifdef HAVE_MNR
    QVERIFY(core->mnrRunnable());
    core->setMnrAvailability(false, reason);
#endif
    QVERIFY(!core->mnrRunnable());
    QCOMPARE(core->mnrStatus(), reason);

    SliceModel* coreSlice = stationModel->slices().constFirst();
    coreSlice->setActiveNr(NrSlot::Off);
    coreSlice->setActiveNr(NrSlot::MNR);
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    QCOMPARE(coreSlice->nnrLastError(), reason);

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QCOMPARE(server.buildCapabilities().dspAssetVersion, 4);
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    DspAssetService* window = clientModel.dspAssets();
    NEREUS_TRY_VERIFY(!window->mnrRunnable());
    NEREUS_TRY_COMPARE(window->mnrStatus(), reason);
    QCOMPARE(clientModel.nrCannotRunReason(NrSlot::MNR), reason);
    NEREUS_TRY_VERIFY(!clientModel.slices().isEmpty());
    SliceModel* windowSlice = clientModel.slices().constFirst();
    QSignalSpy windowRefused(windowSlice, &SliceModel::nrSelectionRefused);
    windowSlice->setActiveNr(NrSlot::MNR);
    QCOMPARE(windowSlice->activeNr(), NrSlot::Off);
    QCOMPARE(windowRefused.count(), 1);
    QCOMPARE(windowRefused.constFirst().at(0).toString(), reason);
    NereusSDR::Test::settleSession();
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    windowSlice->setActiveNr(NrSlot::NR2);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::NR2);
    windowSlice->setActiveNr(NrSlot::Off);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::Off);
    client.disconnectFromStation(QStringLiteral("test complete"));
}

void TstStationSession::bnrIsRefusedOnTheCoreAndInTheWindow()
{
    // R-R3-49, Sub-epic C-1: BNR is in no build, so it is refused on the
    // Core and in a window with the same plain sentence, and no mirrored
    // value is needed for it.
    QVERIFY(!RadioModel::bnrBuilt());
    const QString reason = RadioModel::bnrCannotRunReason();
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    SliceModel* coreSlice = stationModel->slices().constFirst();
    coreSlice->setActiveNr(NrSlot::Off);
    coreSlice->setActiveNr(NrSlot::BNR);
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    QCOMPARE(coreSlice->nnrLastError(), reason);

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    NEREUS_TRY_VERIFY(!clientModel.slices().isEmpty());
    SliceModel* windowSlice = clientModel.slices().constFirst();
    QSignalSpy windowRefused(windowSlice, &SliceModel::nrSelectionRefused);
    windowSlice->setActiveNr(NrSlot::BNR);
    QCOMPARE(windowSlice->activeNr(), NrSlot::Off);
    QCOMPARE(windowRefused.count(), 1);
    QCOMPARE(windowRefused.constFirst().at(0).toString(), reason);
    NereusSDR::Test::settleSession();
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    client.disconnectFromStation(QStringLiteral("test complete"));
}

#ifdef HAVE_DFNR
void TstStationSession::dfnrFailingAtFirstSelectionTurnsTheWindowOff()
{
    // R-R3-49, Sub-epic C-1: the model is there at start, so a window turns
    // DFNR on; the Core's channel then fails to load it at its first
    // selection. The Core turns DFNR off with the reason, and the window
    // follows: its slice shows off and it can no longer choose DFNR.
    QTemporaryDir modelDir;
    ModelPaths::setDfnrModelTarballForTest(
        modelDir.filePath(QStringLiteral("DeepFilterNet3_onnx.tar.gz")));
    const auto restorePath = qScopeGuard([] { ModelPaths::clearDfnrModelTarballForTest(); });
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    DspAssetService* core = stationModel->dspAssets();
    QVERIFY(core->dfnrRunnable());
    SliceModel* coreSlice = stationModel->slices().constFirst();

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    NEREUS_TRY_VERIFY(!clientModel.slices().isEmpty());
    SliceModel* windowSlice = clientModel.slices().constFirst();
    DspAssetService* window = clientModel.dspAssets();
    QVERIFY(window->dfnrRunnable());
    windowSlice->setActiveNr(NrSlot::DFNR);
    NEREUS_TRY_COMPARE(coreSlice->activeNr(), NrSlot::DFNR);

    stationModel->reportDfnrUnavailableForTest(/*modelMissing=*/false);
    const QString reason = QStringLiteral(
        "The DFNR model file on this Core could not be loaded, so DFNR cannot run.");
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    NEREUS_TRY_VERIFY(!window->dfnrRunnable());
    NEREUS_TRY_COMPARE(window->dfnrModelStatus(), reason);
    NEREUS_TRY_COMPARE(windowSlice->activeNr(), NrSlot::Off);
    windowSlice->setActiveNr(NrSlot::DFNR);
    QCOMPARE(windowSlice->activeNr(), NrSlot::Off);
    client.disconnectFromStation(QStringLiteral("test complete"));
}
#endif

void TstStationSession::savedNr3OnACoreWithNoModelShowsOffInTheWindow()
{
    // Follow-up item 1 (R-R3-21). NR3 was saved on for the radio, then the
    // Core lost its NR3 model files. The Core's slice comes up with NR off
    // and the reason set, and a window attaching to it shows NR off.
    DspAssetService::setBundledNr3ModelPathsForTest([](const QString&) { return QString(); });
    const QString prefix = QStringLiteral("hardware/AA:BB:CC:DD:EE:01/slices/0/nnr/");
    const auto cleanup = qScopeGuard([prefix] {
        DspAssetService::setBundledNr3ModelPathsForTest({});
        auto& settings = AppSettings::instance();
        for (const QString& key : settings.allKeys()) {
            if (key.startsWith(prefix)) { settings.remove(key); }
        }
    });
    AppSettings::instance().setValue(prefix + QStringLiteral("NrActive"),
                                     static_cast<int>(NrSlot::NR3));
    const QString none =
        QStringLiteral("No NR3 model file was found on this Core, so NR3 cannot run.");
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    SliceModel* coreSlice = stationModel->slices().constFirst();
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    QCOMPARE(coreSlice->nnrLastError(), none);

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    NEREUS_TRY_VERIFY(!clientModel.slices().isEmpty());
    SliceModel* windowSlice = clientModel.slices().constFirst();
    QCOMPARE(windowSlice->activeNr(), NrSlot::Off);
    NEREUS_TRY_VERIFY(!clientModel.dspAssets()->nr3Runnable());
    QCOMPARE(coreSlice->activeNr(), NrSlot::Off);
    client.disconnectFromStation(QStringLiteral("test complete"));
}

void TstStationSession::nr3CannotRunEndsWithTheSession()
{
    // Follow-up item 2 (R-R3-21). A window learned from one Core that NR3
    // cannot run there. When that session ends and the window attaches to
    // an older Core, which never reports whether NR3 can run, the window
    // does not carry the first Core's refusal over.
    DspAssetService::setBundledNr3ModelPathsForTest([](const QString&) { return QString(); });
    const auto restorePaths = qScopeGuard([] {
        DspAssetService::setBundledNr3ModelPathsForTest({});
    });
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    DspAssetService* window = clientModel.dspAssets();
    NEREUS_TRY_VERIFY(!window->nr3Runnable());
    QVERIFY(!window->nr3ModelStatus().isEmpty());
    client.disconnectFromStation(QStringLiteral("first Core done"));
    NEREUS_TRY_VERIFY(!client.isHandshakeComplete());

    // A second connection, to an older Core that never sends nr3Runnable.
    auto* olderStation = new LoopbackTransport(QStringLiteral("older-station"), this);
    auto* olderPeer = new LoopbackTransport(QStringLiteral("older-client"), this);
    olderStation->linkTo(olderPeer);
    client.startSession(olderPeer, QStringLiteral("test-token"));
    olderStation->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
    olderStation->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
    StationCapabilities caps;
    caps.propertyResultVersion = 1;
    caps.dspAssetVersion = 2;
    olderStation->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
    olderStation->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
    NEREUS_TRY_VERIFY(client.isHandshakeComplete());
    QVERIFY(window->nr3Runnable());
    QVERIFY(window->nr3ModelStatus().isEmpty());
    client.disconnectFromStation(QStringLiteral("test complete"));
}

void TstStationSession::nr3ModelChoiceLoadsOnceOnTheCoreAndMirrors()
{
    // R-R3-21. A remote window chooses the Core's NR3 model with the
    // dspAssets.selectNr3Model command; the Core loads that file once, live,
    // and the choice and its plain-language status reach the window.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    QStringList loaded;
    stationModel->dspAssets()->setNr3ModelLoader(
        [&loaded](const QString& path) { loaded.append(path); });
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QCOMPARE(server.buildCapabilities().dspAssetVersion, 4);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);

    DspAssetService* remote = clientModel.dspAssets();
    NEREUS_TRY_VERIFY(remote->nr3ModelsSupported());
    NEREUS_TRY_COMPARE(remote->nr3ModelStatus(), stationModel->dspAssets()->nr3ModelStatus());

    const QString small = QString::fromLatin1(DspAssetService::kNr3BundledSmallId);
    QSignalSpy answered(remote, &DspAssetService::requestCompleted);
    const quint32 request = remote->request("dspAssets.selectNr3Model",
                                            {{QStringLiteral("id"), small}});
    QVERIFY(request != 0);
    NEREUS_TRY_COMPARE(answered.count(), 1);
    QCOMPARE(answered.first().at(0).toUInt(), request);
    QVERIFY2(answered.first().at(1).toBool(), qPrintable(answered.first().at(2).toString()));

    QCOMPARE(loaded, QStringList{DspAssetService::bundledNr3ModelPath(small)});
    QCOMPARE(stationModel->dspAssets()->nr3ModelAsset(), small);
    NEREUS_TRY_COMPARE(remote->nr3ModelAsset(), small);
    NEREUS_TRY_COMPARE(remote->nr3ModelStatus(), QStringLiteral("Using the bundled small model."));
    // The window never loads a model itself.
    QVERIFY(!remote->applyNr3Model());
    QCOMPARE(loaded.size(), 1);

    AppSettings::instance().remove(QStringLiteral("DspAssets/Nr3Model"));
}

void TstStationSession::olderCoreLeavesTheNr3ModelUnchangeable()
{
    // R-R3-21. A Core advertising dspAssetVersion 1 has no NR3 models: the
    // window reports it cannot change the model and sends no NR3 request.
    // A version 2 Core on the same protocol minor enables it.
    for (const int version : {1, 2}) {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("nr3-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("nr3-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
        station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        StationCapabilities caps;
        caps.propertyResultVersion = 1;
        caps.dspAssetVersion = version;
        station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        NEREUS_TRY_VERIFY(client.isHandshakeComplete());

        DspAssetService* assets = remote.dspAssets();
        QCOMPARE(assets->nr3ModelsSupported(), version >= 2);
        QCOMPARE(client.remoteNr3ModelsAvailable(), version >= 2);
        const quint32 select = assets->request(
            "dspAssets.selectNr3Model",
            {{QStringLiteral("id"), QString::fromLatin1(DspAssetService::kNr3BundledLargeId)}});
        const quint32 upload = assets->request("dspAssets.beginImport", {
            {QStringLiteral("kind"), 2}, {QStringLiteral("label"), QStringLiteral("x")},
            {QStringLiteral("size"), qint64(1)}, {QStringLiteral("hash"), QString(64, QLatin1Char('a'))},
            {QStringLiteral("radioIdentity"), QString()}});
        QCOMPARE(select != 0, version >= 2);
        QCOMPARE(upload != 0, version >= 2);
        // The older Core's NNR requests are unaffected.
        QVERIFY(assets->request("dspAssets.list", {}) != 0);
    }
}

void TstStationSession::olderAppNr3ModelPathWriteIsRefused()
{
    // R-R3-21. An older app still writes Nr3ModelPath (a file on the app's
    // own computer). The Core owns its NR3 models now, so the write is
    // refused with a plain reason and the Core's value stays put.
    const QString key = QStringLiteral("Nr3ModelPath");
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(proxy.ready());

    QSignalSpy rejected(&proxy, &SettingsProxy::valueRejected);
    QSignalSpy toast(&clientModel, &RadioModel::sliceAddRejected);
    proxy.setValue(key, QStringLiteral("C:/Users/op/model.bin"));
    NEREUS_TRY_COMPARE(rejected.count(), 1);
    QCOMPARE(rejected.first().at(0).toString(), key);
    QVERIFY(!stationSettings.contains(key));
    QCOMPARE(toast.count(), 1);
    QCOMPARE(toast.first().at(0).toString(),
             QStringLiteral("This Core keeps its own NR3 models. Update this app to choose one."));

    proxy.remove(key);
    NEREUS_TRY_COMPARE(rejected.count(), 2);
    QCOMPARE(toast.count(), 2);
    QCOMPARE(toast.last().at(0).toString(),
             QStringLiteral("This Core keeps its own NR3 models. Update this app to choose one."));
}

namespace {

// Install a client SettingsProxy as AppSettings' remote backend for one
// scope, the way a remote window runs. Only around the window's own edit:
// the Core model shares this process's AppSettings in these tests.
struct RemoteSettingsScope {
    explicit RemoteSettingsScope(SettingsProxy* proxy)
    {
        AppSettings::instance().setRemoteBackend(proxy);
    }
    ~RemoteSettingsScope() { AppSettings::instance().setRemoteBackend(nullptr); }
};

void removeLocalNotchKeys()
{
    auto& s = AppSettings::instance();
    const QStringList keys = s.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(QStringLiteral("Notch"))) {
            s.remove(key);
        }
    }
}

void writeCoreNotchList(AppSettings& settings, const QList<Notch>& notches)
{
    settings.setValue(QStringLiteral("NotchCount"), QString::number(notches.size()));
    for (int i = 0; i < notches.size(); ++i) {
        settings.setValue(QStringLiteral("Notch%1Center").arg(i),
                          QString::number(notches.at(i).centerHz, 'f', 6));
        settings.setValue(QStringLiteral("Notch%1Width").arg(i),
                          QString::number(notches.at(i).widthHz, 'f', 6));
        settings.setValue(QStringLiteral("Notch%1Active").arg(i), QStringLiteral("True"));
    }
}


// A Core and a remote window joined over the loopback, for the notch tests.
// Members are destroyed in reverse order: the window side first.
struct NotchSession {
    QTemporaryDir dir;
    std::unique_ptr<AppSettings> stationSettings;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    std::unique_ptr<RadioModel> window;
    std::unique_ptr<SettingsProxy> proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* stationEnd = nullptr;
    LoopbackTransport* clientEnd = nullptr;
};

// Build the Core first (so a test can seed its list), then join a window.
void joinNotchWindow(NotchSession& s, QObject* owner, const QString& securityDir)
{
    s.server = std::make_unique<StationServer>(s.core.get(), *s.stationSettings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
    s.window = std::make_unique<RadioModel>(RadioModel::Role::Remote);
    s.proxy = std::make_unique<SettingsProxy>();
    s.client = std::make_unique<StationClient>(s.window.get(), s.proxy.get());
    s.stationEnd = new LoopbackTransport(QStringLiteral("station-end"), owner);
    s.clientEnd = new LoopbackTransport(QStringLiteral("client-end"), owner);
    s.stationEnd->linkTo(s.clientEnd);
    QSignalSpy completed(s.client.get(), &StationClient::handshakeComplete);
    s.client->startSession(s.clientEnd, s.server->token());
    s.server->acceptTransport(s.stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(s.client->remoteNotchControlAvailable());
    QVERIFY(s.window->notchModel()->mirrorMode());
}

void prepareNotchCore(NotchSession& s)
{
    QVERIFY(s.dir.isValid());
    s.stationSettings = std::make_unique<AppSettings>(
        s.dir.filePath(QStringLiteral("NereusSDR.settings")));
    s.core = makeStationRadioModel(0);
}

bool sameNotchList(const NotchModel* a, const NotchModel* b)
{
    if (a->notches().size() != b->notches().size()) {
        return false;
    }
    for (int i = 0; i < a->notches().size(); ++i) {
        const Notch& x = a->notches().at(i);
        const Notch& y = b->notches().at(i);
        if (x.id != y.id || x.centerHz != y.centerHz || x.widthHz != y.widthHz
            || x.active != y.active) {
            return false;
        }
    }
    return true;
}

bool hasNotchSettings(const AppSettings& settings)
{
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(QStringLiteral("Notch"))) {
            return true;
        }
    }
    return false;
}

} // namespace

void TstStationSession::remoteNotchEditKeepsTheCoresWholeList()
{
    // R-R3-21 / R-R3-09, red first. The Core holds two notches. A remote
    // window starts with an empty notch list (it read its settings before
    // the Core's arrived), and its first notch add used to write the whole
    // Notch* set from that empty list: NotchCount 1 replaced the Core's two
    // saved notches, and the Core's live list never saw the add at all.
    // The Core now owns the list: the window's add reaches the Core's list
    // and every receiver, the Core's saved list is not rewritten by the
    // window, and the window shows the Core's list with the Core's ids.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    removeLocalNotchKeys();

    auto stationModel = makeStationRadioModel(0);
    NotchModel* core = stationModel->notchModel();
    QVERIFY(core->addNotch(7040000.0, 200.0) > 0);
    QVERIFY(core->addNotch(7050000.0, 300.0) > 0);
    QCOMPARE(core->notches().size(), 2);
    writeCoreNotchList(stationSettings, core->notches());
    removeLocalNotchKeys();

    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(proxy.ready());

    SliceModel* remoteSlice = clientModel.sliceById(0);
    QVERIFY(remoteSlice != nullptr);
    {
        RemoteSettingsScope scope(&proxy);
        clientModel.addNotchForSlice(remoteSlice, 7060000.0, 250.0);
    }

    // The Core's live list gains the notch; nothing replaced it. The
    // window's settings, when it wrote any, have landed by the time the
    // window has heard back from the Core.
    NEREUS_TRY_VERIFY(stationEnd->receivedKinds().contains(QByteArrayLiteral("command.invoke"))
                || stationSettings.value(QStringLiteral("NotchCount")).toString()
                       != QStringLiteral("2"));
    QCOMPARE(stationSettings.value(QStringLiteral("NotchCount")).toString(),
             QStringLiteral("2"));
    NEREUS_TRY_COMPARE(core->notches().size(), 3);
    QCOMPARE(core->notches().at(0).centerHz, 7040000.0);
    QCOMPARE(core->notches().at(1).centerHz, 7050000.0);
    QCOMPARE(core->notches().at(2).centerHz, 7060000.0);
    // The window wrote no notch settings over the Core's saved list.
    QCOMPARE(stationSettings.value(QStringLiteral("NotchCount")).toString(),
             QStringLiteral("2"));
    QCOMPARE(stationSettings.value(QStringLiteral("Notch1Center")).toDouble(), 7050000.0);
    // The window shows the Core's whole list, under the Core's ids.
    NotchModel* remote = clientModel.notchModel();
    NEREUS_TRY_COMPARE(remote->notches().size(), 3);
    for (int i = 0; i < 3; ++i) {
        QCOMPARE(remote->notches().at(i).id, core->notches().at(i).id);
        QCOMPARE(remote->notches().at(i).centerHz, core->notches().at(i).centerHz);
        QCOMPARE(remote->notches().at(i).widthHz, core->notches().at(i).widthHz);
    }
    removeLocalNotchKeys();
}

void TstStationSession::remoteNotchMoveToggleAndDeleteReachTheCore()
{
    // R-R3-21 / R-R3-09. Every window edit is one request on one notch,
    // applied by the Core's own NotchModel; the window shows the result
    // under the Core's ids and writes no notch settings of its own.
    removeLocalNotchKeys();
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    NotchModel* core = s.core->notchModel();
    const int first = core->addNotch(7040000.0, 200.0);
    QVERIFY(first > 0);
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }
    NotchModel* remote = s.window->notchModel();
    NEREUS_TRY_COMPARE(remote->notches().size(), 1);
    QCOMPARE(remote->notches().first().id, first);

    QVERIFY(remote->setCenter(first, 7041000.0));
    NEREUS_TRY_COMPARE(core->notchById(first)->centerHz, 7041000.0);
    QVERIFY(remote->setWidth(first, 400.0));
    NEREUS_TRY_COMPARE(core->notchById(first)->widthHz, 400.0);
    QVERIFY(remote->setActive(first, false));
    NEREUS_TRY_VERIFY(!core->notchById(first)->active);
    NEREUS_TRY_VERIFY(sameNotchList(core, remote));

    s.window->addNotchForSlice(s.window->sliceById(0), 7060000.0, 250.0);
    NEREUS_TRY_COMPARE(core->notches().size(), 2);
    const int second = core->notches().at(1).id;
    NEREUS_TRY_COMPARE(remote->notches().size(), 2);
    QCOMPARE(remote->notches().at(1).id, second);

    QVERIFY(remote->removeNotch(first));
    NEREUS_TRY_COMPARE(core->notches().size(), 1);
    QCOMPARE(core->notches().first().id, second);
    NEREUS_TRY_VERIFY(sameNotchList(core, remote));
    NEREUS_TRY_COMPARE(remote->revision(), core->revision());

    // Only the Core wrote notch settings, and not through the window.
    QVERIFY(!hasNotchSettings(*s.stationSettings));
    removeLocalNotchKeys();
}

void TstStationSession::remoteNotchRefusalsAreInPlainWords()
{
    // R-R3-21 / R-R3-09. A full list and a notch another window already
    // removed are refused with plain reasons, and the window goes back to
    // the Core's list.
    removeLocalNotchKeys();
    auto& local = AppSettings::instance();
    local.setValue(QStringLiteral("NotchCount"), QString::number(NotchModel::kMaxNotches));
    for (int i = 0; i < NotchModel::kMaxNotches; ++i) {
        local.setValue(QStringLiteral("Notch%1Center").arg(i),
                       QString::number(7000000.0 + i * 100.0, 'f', 6));
        local.setValue(QStringLiteral("Notch%1Width").arg(i), QStringLiteral("50"));
        local.setValue(QStringLiteral("Notch%1Active").arg(i), QStringLiteral("True"));
    }
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    removeLocalNotchKeys();
    NotchModel* core = s.core->notchModel();
    QCOMPARE(core->notches().size(), NotchModel::kMaxNotches);
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }
    NotchModel* remote = s.window->notchModel();
    // The whole list travels, ids and all.
    NEREUS_TRY_COMPARE(remote->notches().size(), NotchModel::kMaxNotches);
    QVERIFY(sameNotchList(core, remote));

    QSignalSpy addRefused(remote, &NotchModel::notchAddRejected);
    s.window->addNotchForSlice(s.window->sliceById(0), 14200000.0, 200.0);
    NEREUS_TRY_COMPARE(addRefused.count(), 1);
    QCOMPARE(addRefused.first().at(0).toString(),
             QStringLiteral("Maximum of 1024 notches reached"));
    QCOMPARE(core->notches().size(), NotchModel::kMaxNotches);

    // Another window (here the Core itself) removes a notch this window
    // still shows; this window's toggle of it is refused and undone.
    const int gone = core->notches().at(5).id;
    QVERIFY(core->removeNotch(gone));
    QSignalSpy refused(remote, &NotchModel::notchRequestRefused);
    QVERIFY(remote->setActive(gone, false));
    NEREUS_TRY_COMPARE(refused.count(), 1);
    QCOMPARE(refused.first().at(0).toString(),
             QStringLiteral("That notch is no longer on this Core."));
    NEREUS_TRY_VERIFY(remote->notchById(gone) == nullptr);
    NEREUS_TRY_VERIFY(sameNotchList(core, remote));

    // A malformed or stale request sent straight to the Core.
    QSignalSpy results(s.client.get(), &StationClient::commandResult);
    const quint32 stale = s.client->invokeCommand("notch.delete",
        {{0, "id", MirrorWireKind::Int64, qlonglong(gone)}});
    QVERIFY(stale != 0);
    const quint32 malformed = s.client->invokeCommand("notch.delete",
        {{0, "id", MirrorWireKind::Float64, double(gone)}});
    QVERIFY(malformed != 0);
    NEREUS_TRY_COMPARE(results.count(), 2);
    for (const QList<QVariant>& args : std::as_const(results)) {
        QVERIFY(!args.at(1).toBool());
        QCOMPARE(args.at(2).toString(), args.at(0).toUInt() == stale
            ? QStringLiteral("That notch is no longer on this Core.")
            : QStringLiteral("This notch change is not one this Core understands."));
    }
    removeLocalNotchKeys();
}

void TstStationSession::coreNotchChangesReachTheWindow()
{
    // R-R3-21 / R-R3-09. A change made on the Core (a TCI rx_nf_enable, a
    // notch placed at the Core) reaches the window; the window's two
    // switches reach the Core.
    removeLocalNotchKeys();
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }
    NotchModel* core = s.core->notchModel();
    NotchModel* remote = s.window->notchModel();
    QVERIFY(!core->globalEnabled());

    s.core->setRxNf(0, true);   // the TCI rx_nf_enable path
    NEREUS_TRY_VERIFY(remote->globalEnabled());

    const int placed = core->addNotch(14074000.0, 300.0);
    QVERIFY(placed > 0);
    NEREUS_TRY_COMPARE(remote->notches().size(), 1);
    QCOMPARE(remote->notches().first().id, placed);
    QCOMPARE(remote->notches().first().widthHz, 300.0);

    remote->setGlobalEnabled(false);
    NEREUS_TRY_VERIFY(!core->globalEnabled());
    QVERIFY(core->autoIncrease());
    remote->setAutoIncrease(false);
    NEREUS_TRY_VERIFY(!core->autoIncrease());
    QVERIFY(!hasNotchSettings(*s.stationSettings));
    removeLocalNotchKeys();
}

void TstStationSession::appNotchSettingsWritesAreRefused()
{
    // R-R3-21 / R-R3-09. An app's raw Notch* write or remove (what an older
    // app sends on every notch edit) is refused with a plain reason, so it
    // can no longer replace the Core's list. NotchVisualEnabled is a
    // display preference and still lands.
    removeLocalNotchKeys();
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    s.stationSettings->setValue(QStringLiteral("NotchCount"), QStringLiteral("2"));
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(s.proxy->ready());

    const QString reason =
        QStringLiteral("This Core keeps its own notch list. Update this app to change notches.");
    QSignalSpy rejected(s.proxy.get(), &SettingsProxy::valueRejected);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    s.proxy->setValue(QStringLiteral("NotchCount"), QStringLiteral("0"));
    NEREUS_TRY_COMPARE(rejected.count(), 1);
    QCOMPARE(toast.last().at(0).toString(), reason);
    QCOMPARE(s.stationSettings->value(QStringLiteral("NotchCount")).toString(),
             QStringLiteral("2"));
    s.proxy->setValue(QStringLiteral("NotchGlobalEnabled"), QStringLiteral("True"));
    NEREUS_TRY_COMPARE(rejected.count(), 2);
    QVERIFY(!s.stationSettings->contains(QStringLiteral("NotchGlobalEnabled")));
    s.proxy->remove(QStringLiteral("NotchCount"));
    NEREUS_TRY_COMPARE(rejected.count(), 3);
    QCOMPARE(toast.last().at(0).toString(), reason);
    QCOMPARE(s.stationSettings->value(QStringLiteral("NotchCount")).toString(),
             QStringLiteral("2"));

    s.proxy->setValue(QStringLiteral("NotchVisualEnabled"), QStringLiteral("True"));
    NEREUS_TRY_COMPARE(s.stationSettings->value(QStringLiteral("NotchVisualEnabled")).toString(),
                 QStringLiteral("True"));
    QCOMPARE(rejected.count(), 3);
    removeLocalNotchKeys();
}

void TstStationSession::olderCoreKeepsTodaysNotchBehaviour()
{
    // R-R3-21 / R-R3-09. Against a Core without notchControlVersion the
    // window keeps today's notches exactly: no mirror mode, a local add, no
    // notch request. With the version, the add becomes a request.
    for (const int version : {0, 1}) {
        removeLocalNotchKeys();
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("notch-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("notch-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
        station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        StationCapabilities caps;
        caps.propertyResultVersion = 1;
        caps.notchControlVersion = version;
        station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        NEREUS_TRY_VERIFY(client.isHandshakeComplete());

        NotchModel* notches = remote.notchModel();
        QCOMPARE(notches->mirrorMode(), version >= 1);
        QCOMPARE(client.remoteNotchControlAvailable(), version >= 1);
        station->clearReceived();
        const int added = remote.addNotchForSlice(nullptr, 7040000.0, 200.0);
        if (version == 0) {
            QVERIFY(added > 0);
            QCOMPARE(notches->notches().size(), 1);
            QCOMPARE(AppSettings::instance().value(QStringLiteral("NotchCount")).toString(),
                     QStringLiteral("1"));
            NereusSDR::Test::settleSession();
            QVERIFY(!station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
        } else {
            QCOMPARE(added, -1);
            QCOMPARE(notches->notches().size(), 0);
            NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
            QVERIFY(!AppSettings::instance().contains(QStringLiteral("NotchCount")));
        }
    }
    removeLocalNotchKeys();
}

void TstStationSession::olderAppIgnoresTheNotchesObjectGolden()
{
    // R-R3-21 / R-R3-09, golden. An app that does not know
    // notchControlVersion (modelled by removing that entry: an older app
    // ignores it) is handed this Core's real burst plus a later notch
    // change. It ends in exactly the state it reaches from the same burst
    // with every `notches` message removed, which is what an older Core
    // sends: the new object changes nothing for it.
    removeLocalNotchKeys();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings stationSettings(dir.filePath(QStringLiteral("NereusSDR.settings")));
    auto core = makeStationRadioModel(0);
    QVERIFY(core->notchModel()->addNotch(7040000.0, 200.0) > 0);
    removeLocalNotchKeys();
    StationServer server(core.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    auto* station = new LoopbackTransport(QStringLiteral("golden-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("golden-peer"), this);
    station->linkTo(peer);
    server.acceptTransport(station);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("older-app"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
    QVERIFY(core->notchModel()->addNotch(7050000.0, 200.0) > 0);
    const auto hasNotchDelta = [peer]() {
        for (const QByteArray& wire : peer->received()) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::Delta && m.objectKey == "notches") {
                return true;
            }
        }
        return false;
    };
    NEREUS_TRY_VERIFY(hasNotchDelta());
    const QList<QByteArray> burst = peer->received();

    int notchMessages = 0;
    const auto replay = [&](bool keepNotches) {
        QList<QByteArray> out;
        for (const QByteArray& wire : burst) {
            SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::Capabilities) {
                QList<MirrorUpdate> kept;
                for (const MirrorUpdate& u : std::as_const(m.updates)) {
                    if (u.name != "notchControlVersion") { kept.append(u); }
                }
                m.updates = kept;
                out.append(SessionMessages::encode(m));
                continue;
            }
            const bool aboutNotches = m.objectKey == "notches"
                || (m.kind == SessionMessageKind::Schema && m.className == "NotchModel");
            if (aboutNotches) {
                if (keepNotches) {
                    ++notchMessages;
                    out.append(wire);
                    // Fix wave minor 6: more notch changes cost an older app
                    // no more lines. Each delta is sent three times.
                    if (m.kind == SessionMessageKind::Delta) {
                        out.append(wire);
                        out.append(wire);
                    }
                }
                continue;
            }
            out.append(wire);
        }
        return out;
    };

    struct Window {
        RadioModel model{RadioModel::Role::Remote};
        SettingsProxy proxy;
        std::unique_ptr<StationClient> client;
        LoopbackTransport* station = nullptr;
    };
    const auto run = [&](Window& w, const QList<QByteArray>& messages) {
        w.client = std::make_unique<StationClient>(&w.model, &w.proxy);
        w.station = new LoopbackTransport(QStringLiteral("replay-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("replay-client"), this);
        w.station->linkTo(clientEnd);
        w.client->startSession(clientEnd, server.token());
        for (const QByteArray& wire : messages) {
            w.station->sendText(wire);
        }
    };
    // The Core's own notch settings share this process's store; a window
    // must start from the empty store a fresh remote window has.
    removeLocalNotchKeys();
    Window withNotches;
    Window without;
    // The only two lines the new object costs an older app: the tolerance
    // StationClient already has for any object it does not hold.
    QTest::ignoreMessage(QtWarningMsg,
        "Station named an object this client cannot construct: \"notches\" \"NotchModel\"");
    QTest::ignoreMessage(QtWarningMsg,
        "Delta for an object this client does not hold: \"notches\"");
    // Once per object: any further line for the same object fails the test.
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("^Delta for an object this client does not hold")));
    run(withNotches, replay(true));
    run(without, replay(false));
    QVERIFY(notchMessages >= 3);   // schema, object.create, delta
    NEREUS_TRY_VERIFY(withNotches.client->isHandshakeComplete());
    NEREUS_TRY_VERIFY(without.client->isHandshakeComplete());
    NereusSDR::Test::settleSession();

    NotchModel* a = withNotches.model.notchModel();
    NotchModel* b = without.model.notchModel();
    QVERIFY(!a->mirrorMode());
    QVERIFY(!b->mirrorMode());
    QVERIFY(sameNotchList(a, b));
    QCOMPARE(a->notches().size(), 0);
    QCOMPARE(a->globalEnabled(), b->globalEnabled());
    QCOMPARE(a->autoIncrease(), b->autoIncrease());
    QCOMPARE(withNotches.model.slices().size(), without.model.slices().size());
    QCOMPARE(withNotches.station->receivedKinds(), without.station->receivedKinds());
    withNotches.client.reset();
    without.client.reset();
    removeLocalNotchKeys();
}

void TstStationSession::receiveOnlyStationTakesTransmitDspOptionsSettingsWrites()
{
    // R-R3-21. The DSP > Options TX combos write station transmit settings:
    // DspOptions keys are Station-scoped (SettingsScope.cpp). R-R3-49
    // (parity Task 1): a receive-only Core takes them while its radio is
    // off the air (transmitSettingsVersion 1) and refuses them while it is
    // on the air (tst_transmit_settings_gate). Receive DSP options are the
    // operator's to change as before.
    const QString txKey = QStringLiteral("DspOptionsBufferSizePhoneTx");
    const QString rxKey = QStringLiteral("DspOptionsBufferSizePhoneRx");

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(txKey, QStringLiteral("1024"));
    stationSettings.setValue(rxKey, QStringLiteral("1024"));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY(stationModel->receiveOnlyStationPolicy());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(proxy.ready());
    QCOMPARE(proxy.value(txKey, QString()).toString(), QStringLiteral("1024"));

    QSignalSpy rejected(&proxy, &SettingsProxy::valueRejected);
    QSignalSpy toast(&clientModel, &RadioModel::sliceAddRejected);
    proxy.setValue(txKey, QStringLiteral("2048"));
    NEREUS_TRY_COMPARE(stationSettings.value(txKey).toString(), QStringLiteral("2048"));
    QCOMPARE(proxy.value(txKey, QString()).toString(), QStringLiteral("2048"));
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(toast.count(), 0);

    proxy.setValue(rxKey, QStringLiteral("2048"));
    NEREUS_TRY_COMPARE(stationSettings.value(rxKey).toString(), QStringLiteral("2048"));
    QCOMPARE(rejected.count(), 0);
}

void TstStationSession::receiveOnlyStationTakesTransmitDspOptionsSettingsRemoves()
{
    // R-R3-21. A remove resets a DSP > Options TX setting to its default.
    // R-R3-49 (parity Task 1): a receive-only Core takes it as it takes a
    // write to the same key, while the radio is off the air. Removing a
    // receive DSP option is allowed as before.
    const QString txKey = QStringLiteral("DspOptionsBufferSizePhoneTx");
    const QString rxKey = QStringLiteral("DspOptionsBufferSizePhoneRx");

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(txKey, QStringLiteral("1024"));
    stationSettings.setValue(rxKey, QStringLiteral("1024"));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY(stationModel->receiveOnlyStationPolicy());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(proxy.ready());
    QCOMPARE(proxy.value(txKey, QString()).toString(), QStringLiteral("1024"));

    QSignalSpy rejected(&proxy, &SettingsProxy::valueRejected);
    QSignalSpy toast(&clientModel, &RadioModel::sliceAddRejected);
    proxy.remove(txKey);
    QVERIFY(!proxy.contains(txKey));
    NEREUS_TRY_VERIFY(!stationSettings.contains(txKey));
    NereusSDR::Test::settleSession();
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(toast.count(), 0);
    QVERIFY(!proxy.contains(txKey));

    proxy.remove(rxKey);
    NEREUS_TRY_VERIFY(!stationSettings.contains(rxKey));
    QCOMPARE(rejected.count(), 0);
}

void TstStationSession::acceptedReceiveDspOptionsWriteAppliesToMatchingSlices()
{
    // R-R3-21. A DSP > Options RX write or remove from a remote window takes
    // effect on the Core at once: the Core re-runs the mode-change apply for
    // each slice in the key's mode group instead of waiting for the next
    // mode change. A TX key (R-R3-49, parity Task 1: taken off the air; its
    // TX apply is tst_transmit_settings_gate's), an unrelated key and a
    // local write to the Core's own store apply nothing to a receive slice.
    const QString phoneRx = QStringLiteral("DspOptionsBufferSizePhoneRx");
    const QString cwRx = QStringLiteral("DspOptionsFilterSizeCwRx");
    const QString phoneTx = QStringLiteral("DspOptionsBufferSizePhoneTx");
    const QString unrelated = QStringLiteral("DspOptionsCacheImpulse");

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(phoneRx, QStringLiteral("1024"));
    stationSettings.setValue(cwRx, QStringLiteral("4096"));
    stationSettings.setValue(phoneTx, QStringLiteral("1024"));

    auto stationModel = makeStationRadioModel(1);
    QCOMPARE(stationModel->slices().size(), 2);
    SliceModel* phoneSlice = stationModel->slices().at(0);
    SliceModel* cwSlice = stationModel->slices().at(1);
    phoneSlice->setDspMode(DSPMode::USB);
    cwSlice->setDspMode(DSPMode::CWU);
    QList<QPair<int, DSPMode>> applied;
    stationModel->setDspOptionsApplyObserverForTest([&applied](int index, DSPMode mode) {
        applied.append(qMakePair(index, mode));
    });
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY(stationModel->receiveOnlyStationPolicy());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    QVERIFY(proxy.ready());

    // Accepted RX write: one apply, to the Phone slice only.
    proxy.setValue(phoneRx, QStringLiteral("2048"));
    NEREUS_TRY_COMPARE(stationSettings.value(phoneRx).toString(), QStringLiteral("2048"));
    NEREUS_TRY_COMPARE(applied.size(), 1);
    QCOMPARE(applied.first(), qMakePair(phoneSlice->sliceIndex(), DSPMode::USB));

    // A TX write and an unrelated accepted key: no receive slice applies.
    QSignalSpy rejected(&proxy, &SettingsProxy::valueRejected);
    proxy.setValue(phoneTx, QStringLiteral("2048"));
    NEREUS_TRY_COMPARE(stationSettings.value(phoneTx).toString(), QStringLiteral("2048"));
    QCOMPARE(rejected.count(), 0);
    proxy.setValue(unrelated, QStringLiteral("True"));
    NEREUS_TRY_COMPARE(stationSettings.value(unrelated).toString(), QStringLiteral("True"));
    NereusSDR::Test::settleSession();
    QCOMPARE(applied.size(), 1);

    // Accepted RX remove: the CW slice applies its default.
    proxy.remove(cwRx);
    NEREUS_TRY_VERIFY(!stationSettings.contains(cwRx));
    NEREUS_TRY_COMPARE(applied.size(), 2);
    QCOMPARE(applied.at(1), qMakePair(cwSlice->sliceIndex(), DSPMode::CWU));

    // Local half: the Core's own store changing is not a remote write.
    stationSettings.setValue(phoneRx, QStringLiteral("512"));
    NereusSDR::Test::settleSession();
    QCOMPARE(applied.size(), 2);
}

void TstStationSession::receiveOnlyPolicySurvivesRadioTeardown()
{
    // Use the real non-null connection teardown path, without opening a
    // socket or starting DSP. A null connection returns before clearing the
    // ordinary local MOX check and would miss this lifecycle regression.
    P1RadioConnection connection;
    RadioModel station;
    station.injectConnectionForTest(&connection);
    station.setReceiveOnlyStationPolicy(true);
    station.disconnectFromRadio();
    QVERIFY(station.connection() == nullptr);
    QVERIFY(station.receiveOnlyStationPolicy());
    QSignalSpy rejected(station.moxController(), &MoxController::moxRejected);
    station.moxController()->setMox(true);
    QCOMPARE(rejected.count(), 1);
    QVERIFY(!station.moxController()->isMox());
}

void TstStationSession::handshakeDeadlineDropsASilentPeer()
{
    // Minor: a peer that opens a socket and answers pings but never
    // authenticates used to live forever, holding a slot.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setAuthDeadlineMs(60);
    QCOMPARE(server.authDeadlineMs(), 60);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("lurker"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("lurker-client"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy dropped(&server, &StationServer::peerDisconnected);
    server.acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(server.peerCount(), 1);

    // It answers pings (the default) but never says hello or authenticates.
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(server.peerCount(), 0, 3000);
    QCOMPARE(dropped.count(), 1);
    QVERIFY(!server.hasAuthenticatedSession());

    // A peer that DOES authenticate is not dropped by the same deadline.
    auto* goodStation = new LoopbackTransport(QStringLiteral("good"), this);
    auto* goodClient = new LoopbackTransport(QStringLiteral("good-client"), this);
    goodStation->linkTo(goodClient);
    server.acceptTransport(goodStation);
    NEREUS_TRY_VERIFY(!goodClient->received().isEmpty());
    goodClient->sendText(SessionMessages::encode(
        SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 6,
                               QStringLiteral("good"))));
    goodClient->sendText(
        SessionMessages::encode(SessionMessages::authRequest(server.token())));
    NEREUS_TRY_VERIFY(server.hasAuthenticatedSession());
    // Nothing to wait for but the deadline itself: twice the server's own.
    QTest::qWait(2 * server.authDeadlineMs());
    NereusSDR::Test::drainQueuedDeliveries();
    QVERIFY2(server.hasAuthenticatedSession(),
             "the handshake deadline fired on a peer that had authenticated");
}

void TstStationSession::peerLimitRefusesFurtherConnections()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    // Out of the way: this slot is about the cap, not the deadline.
    server.setAuthDeadlineMs(0);

    QList<LoopbackTransport*> clientEnds;
    for (int i = 0; i < StationServer::kMaxConcurrentPeers; ++i) {
        auto* stationEnd =
            new LoopbackTransport(QStringLiteral("peer-%1").arg(i), this);
        auto* clientEnd =
            new LoopbackTransport(QStringLiteral("peer-%1-client").arg(i), this);
        stationEnd->linkTo(clientEnd);
        server.acceptTransport(stationEnd);
        clientEnds.append(clientEnd);
    }
    QCOMPARE(server.peerCount(), StationServer::kMaxConcurrentPeers);

    auto* overflowStation = new LoopbackTransport(QStringLiteral("overflow"), this);
    auto* overflowClient = new LoopbackTransport(QStringLiteral("overflow-client"), this);
    overflowStation->linkTo(overflowClient);
    server.acceptTransport(overflowStation);

    QCOMPARE(server.peerCount(), StationServer::kMaxConcurrentPeers);
    // Refused with a reason on the wire, not an unexplained close.
    NEREUS_TRY_VERIFY(overflowClient->receivedKinds().contains(QByteArrayLiteral("session.end")));
}

// Part C fix wave (R1-M4): one host cannot hold every one of the
// kMaxConcurrentPeers slots by redialling within the handshake deadline.
void TstStationSession::oneAddressHoldsAtMostTwoConnectingSlots()
{
    QCOMPARE(StationServer::kMaxHandshakesPerAddress, 2);
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setAuthDeadlineMs(0);

    const auto dial = [this, &server](const QString& address) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client"), this);
        stationEnd->setPeerAddress(address);
        stationEnd->linkTo(clientEnd);
        server.acceptTransport(stationEnd);
        return std::make_pair(stationEnd, clientEnd);
    };
    const QString attacker = QStringLiteral("203.0.113.9");
    auto first = dial(attacker);
    auto second = dial(attacker);
    QCOMPARE(server.peerCount(), 2);
    // The third from that address, however it is written, is refused,
    // retryable, with the cap's own words.
    for (const QString& same : {attacker, QStringLiteral("::ffff:203.0.113.9")}) {
        auto third = dial(same);
        QCOMPARE(server.peerCount(), 2);
        NEREUS_TRY_VERIFY(third.second->receivedKinds().contains(QByteArrayLiteral("session.end")));
        SessionMessage end;
        for (const QByteArray& wire : third.second->received()) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::SessionEnd) {
                end = m;
            }
        }
        QVERIFY(end.retryable);
        QCOMPARE(end.reason, QStringLiteral("The Core already has as many connections as it "
                                            "allows. Try again shortly."));
    }
    // Another address still gets in, and so does a connection with no
    // address of its own (the relay's, later), which is not counted here.
    dial(QStringLiteral("198.51.100.4"));
    dial(QString());
    dial(QString());
    dial(QString());
    QCOMPARE(server.peerCount(), 6);
    // Once one of the two ends, the address may connect again.
    first.second->closeLink(QStringLiteral("gone"));
    NEREUS_TRY_COMPARE(server.peerCount(), 5);
    dial(attacker);
    QCOMPARE(server.peerCount(), 6);
    Q_UNUSED(second);
}

// Part C follow-up (R-IOS-08): an IPv6 host has a whole /64 to dial from,
// so the per-address count keys IPv6 by its /64 prefix. IPv4, and IPv4
// written as IPv4-mapped IPv6, stays keyed by the full address.
void TstStationSession::ipv6PeersAreCountedPerSlash64()
{
    QCOMPARE(StationServer::kMaxHandshakesPerAddress, 2);
    QCOMPARE(StationServer::kMaxConcurrentPeers, 24);
    const QString capReason = QStringLiteral(
        "The Core already has as many connections as it allows. Try again shortly.");

    const auto makeServer = [this](RadioModel* model, AppSettings& settings) {
        auto server = std::make_unique<StationServer>(
            model, settings,
            NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        server->setAuthDeadlineMs(0);
        return server;
    };
    const auto dial = [this](StationServer& server, const QString& address) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client"), this);
        stationEnd->setPeerAddress(address);
        stationEnd->linkTo(clientEnd);
        server.acceptTransport(stationEnd);
        return clientEnd;
    };
    const auto refusedWithCap = [&capReason](LoopbackTransport* client) {
        if (!client->receivedKinds().contains(QByteArrayLiteral("session.end"))) {
            return false;
        }
        for (const QByteArray& wire : client->received()) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::SessionEnd) {
                return m.retryable && m.reason == capReason;
            }
        }
        return false;
    };

    // Four addresses in one /64, two handshakes each: two slots, the rest
    // refused with the cap's reason.
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        auto model = makeStationRadioModel(0);
        auto server = makeServer(model.get(), settings);
        const QStringList oneSlash64 = {
            QStringLiteral("2001:db8:1:2::1"), QStringLiteral("2001:db8:1:2::2"),
            QStringLiteral("2001:db8:1:2:aaaa:bbbb:cccc:dddd"),
            QStringLiteral("2001:db8:1:2:ffff:ffff:ffff:fffe")};
        QList<LoopbackTransport*> refused;
        int dialled = 0;
        for (const QString& address : oneSlash64) {
            for (int i = 0; i < 2; ++i) {
                LoopbackTransport* client = dial(*server, address);
                if (++dialled > 2) {
                    refused.append(client);
                }
            }
        }
        QCOMPARE(server->peerCount(), 2);
        QCOMPARE(refused.size(), 6);
        for (LoopbackTransport* client : std::as_const(refused)) {
            NEREUS_TRY_VERIFY(refusedWithCap(client));
        }
    }

    // Two different /64s get two each.
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        auto model = makeStationRadioModel(0);
        auto server = makeServer(model.get(), settings);
        dial(*server, QStringLiteral("2001:db8:1:2::1"));
        dial(*server, QStringLiteral("2001:db8:1:2::2"));
        dial(*server, QStringLiteral("2001:db8:1:3::1"));
        dial(*server, QStringLiteral("2001:db8:1:3::2"));
        QCOMPARE(server->peerCount(), 4);
        LoopbackTransport* third = dial(*server, QStringLiteral("2001:db8:1:3::3"));
        QCOMPARE(server->peerCount(), 4);
        NEREUS_TRY_VERIFY(refusedWithCap(third));
    }

    // An IPv4-mapped peer is counted as its IPv4 address: it shares a count
    // with the plain form, and not with other mapped addresses (which all
    // sit in one /64, ::ffff:0:0/96).
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        auto model = makeStationRadioModel(0);
        auto server = makeServer(model.get(), settings);
        dial(*server, QStringLiteral("::ffff:192.0.2.7"));
        dial(*server, QStringLiteral("::ffff:192.0.2.7"));
        LoopbackTransport* plain = dial(*server, QStringLiteral("192.0.2.7"));
        QCOMPARE(server->peerCount(), 2);
        NEREUS_TRY_VERIFY(refusedWithCap(plain));
        dial(*server, QStringLiteral("::ffff:192.0.2.8"));
        dial(*server, QStringLiteral("192.0.2.8"));
        dial(*server, QStringLiteral("::ffff:192.0.2.9"));
        QCOMPARE(server->peerCount(), 5);
    }

    // A signed-in session still does not count against its /64.
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        auto model = makeStationRadioModel(0);
        auto server = makeServer(model.get(), settings);
        LoopbackTransport* signedIn = dial(*server, QStringLiteral("2001:db8:5:6::10"));
        NEREUS_TRY_VERIFY(!signedIn->received().isEmpty());
        signedIn->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("phone"))));
        signedIn->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
        NEREUS_TRY_VERIFY(signedIn->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
        QVERIFY(server->hasAuthenticatedSession());
        dial(*server, QStringLiteral("2001:db8:5:6::11"));
        dial(*server, QStringLiteral("2001:db8:5:6::12"));
        QCOMPARE(server->peerCount(), 3);
        LoopbackTransport* third = dial(*server, QStringLiteral("2001:db8:5:6::13"));
        QCOMPARE(server->peerCount(), 3);
        NEREUS_TRY_VERIFY(refusedWithCap(third));
    }

    // The key itself.
    QCOMPARE(StationServer::addressKey(QStringLiteral("2001:db8:1:2:aaaa::1")),
             StationServer::addressKey(QStringLiteral("2001:db8:1:2::9")));
    QVERIFY(StationServer::addressKey(QStringLiteral("2001:db8:1:2::1"))
            != StationServer::addressKey(QStringLiteral("2001:db8:1:3::1")));
    QCOMPARE(StationServer::addressKey(QStringLiteral("::ffff:192.0.2.7")),
             QStringLiteral("192.0.2.7"));
    QCOMPARE(StationServer::addressKey(QStringLiteral("192.0.2.7")),
             QStringLiteral("192.0.2.7"));
    QCOMPARE(StationServer::addressKey(QString()), QString());
}

void TstStationSession::listenIsIdempotent()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));
    const quint16 port = server.serverPort();

    // A second call used to re-apply the SSL config and fail the bind into
    // lastError(), leaving a working listener described as broken.
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));
    QVERIFY(server.lastError().isEmpty());
    QCOMPARE(server.serverPort(), port);
    QVERIFY(server.isListening());

    server.close();
}

// ── Security fix round ───────────────────────────────────────────────────

void TstStationSession::firstRunPairingBannerNeverReachesTheLoggingHandler()
{
    // The banner used to go out through qCInfo(lcStation), which put it
    // into the hands of CoreInit's process-wide message handler. That
    // handler does two things to it: redactPii()'s MAC rule shredded the
    // 32-pair TLS fingerprint down to 7 surviving bytes, and the message
    // was written verbatim into the daemon's persistent log file -- the
    // file CONTRIBUTING.md tells operators to attach to a bug report, and
    // a worse medium for a shared secret than the AppSettings XML
    // TokenStore.h refuses to use for exactly that reason.
    //
    // A FRESH security directory, not the class-wide m_securityDir: this
    // slot needs the Core's identity key to be created on this start
    // (iPhone app Task 12: that is the first run now, and a new Core has
    // no token to print). The price is one extra RSA-3072 key generation.
    QTemporaryDir freshSecurity;
    QVERIFY(freshSecurity.isValid());
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);

    QStringList captured;
    std::unique_ptr<StationServer> server;
    {
        LogCapture capture(&captured);
        server = std::make_unique<StationServer>(stationModel.get(), stationSettings,
                                                 freshSecurity.path());
    }

    const QString fingerprint = server->certificateFingerprint();
    const QString keyPath = server->stationIdentity().keyPath();
    QVERIFY2(server->stationIdentity().wasCreatedThisRun(),
             "no identity key was created, so this slot proves nothing");
    QVERIFY2(server->token().isEmpty(), "a new Core created a pairing token");
    QVERIFY2(!fingerprint.isEmpty(),
             "no certificate was provisioned, so this slot proves nothing");

    for (const QString& line : captured) {
        QVERIFY2(!line.contains(fingerprint),
                 qPrintable(QStringLiteral(
                                "the TLS fingerprint reached the Qt logging handler, "
                                "whose redactPii() destroys 25 of its 32 bytes. "
                                "Line: %1").arg(line)));
    }

    // A first run must still leave a trace an operator can find, or the
    // fix trades one support problem for another.
    bool mentionsFirstRun = false;
    for (const QString& line : captured) {
        if (line.contains(QStringLiteral("First run"))) {
            mentionsFirstRun = true;
            break;
        }
    }
    QVERIFY2(mentionsFirstRun,
             "nothing in the log says a first run provisioned anything");

    // And what the operator IS shown carries the pin intact and where the
    // identity key is, with the prompt to back it up. Asserted against the
    // formatter rather than by capturing stdout, so the check is the same
    // on every platform; writePairingBanner() is a single fwrite of exactly
    // this string.
    const QString banner = StationServer::formatFirstRunBanner(fingerprint, keyPath);
    QVERIFY(banner.contains(fingerprint));
    QVERIFY(banner.contains(keyPath));
    QVERIFY(banner.contains(QStringLiteral("Back up")));

    // The other half of the same defect, pinned here because this is where
    // the two meet: even a fingerprint logged from somewhere else now
    // survives redaction untouched.
    QCOMPARE(CoreInit::redactPiiForTest(fingerprint), fingerprint);
}

void TstStationSession::oversizedMessageIsRefusedBeforeAnyAuthentication()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // Critical 2. StationServer never capped the sockets it accepted, and
    // Qt's defaults are roughly INT_MAX, about 2 GiB per message, buffered
    // in full before textMessageReceived fires. The 30 s auth deadline
    // bounds TIME, not BYTES, so this was entirely pre-authentication.
    //
    // The assertion is on the CLOSE CODE, deliberately, not on the peer
    // going away. An uncapped station also drops this peer -- it buffers
    // the whole message, fails to decode it, and closes with
    // CloseCodeNormal and the reason "undecodable message". Only a capped
    // socket refuses the BYTES, which Qt reports as CloseCodeTooMuchData
    // (1009). Asserting on peerCount alone would pass either way.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    // A RAW QWebSocket, not a StationClient: this peer is deliberately
    // hostile and must not be constrained by the client's own protocol.
    QWebSocket raw;
    connect(&raw, &QWebSocket::sslErrors, &raw,
            [&raw](const QList<QSslError>& errors) { raw.ignoreSslErrors(errors); });
    QSignalSpy rawConnected(&raw, &QWebSocket::connected);
    raw.open(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())));
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(rawConnected.count(), 1, 15000);
    NEREUS_TRY_COMPARE(server.peerCount(), 1);

    // Comfortably past the cap and nowhere near Qt's default, so an
    // uncapped station accepts every byte of it.
    const qsizetype oversize =
        static_cast<qsizetype>(StationServer::kMaxIncomingMessageBytes) + 4096;
    raw.sendTextMessage(QString(oversize, QLatin1Char('x')));

    NEREUS_TRY_COMPARE_WITH_TIMEOUT(server.peerCount(), 0, 15000);
    QVERIFY2(!server.hasAuthenticatedSession(),
             "an oversized message reached a peer that had authenticated");
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(raw.closeCode(),
                              QWebSocketProtocol::CloseCodeTooMuchData, 5000);

    server.close();
}

void TstStationSession::clientCapsWhatAStationCanMakeItAllocate()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // The other direction. A pinned certificate proves WHO the station is;
    // it promises nothing about how much the station will ask this GUI to
    // allocate. Asserted on the socket the production dial path actually
    // creates, through StationClient::transport(), rather than on a
    // hand-built WebSocketTransport -- the defect was that dialStation()
    // passed no cap, so constructing a transport by hand in the test would
    // have proved nothing about the call site.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);

    client.connectToStation(
        QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
        server.token(), server.certificateFingerprint());
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);

    auto* transport = qobject_cast<WebSocketTransport*>(client.transport());
    QVERIFY2(transport != nullptr, "the dial did not produce a WebSocketTransport");
    QVERIFY(transport->socket() != nullptr);
    QCOMPARE(transport->socket()->maxAllowedIncomingMessageSize(),
             StationClient::kMaxIncomingMessageBytes);
    QCOMPARE(transport->socket()->maxAllowedIncomingFrameSize(),
             StationClient::kMaxIncomingMessageBytes);

    // And the daemon's own accepted socket carries the smaller cap, so the
    // two constants are not accidentally the same number.
    QVERIFY(StationServer::kMaxIncomingMessageBytes
            < StationClient::kMaxIncomingMessageBytes);

    client.disconnectFromStation(QStringLiteral("test complete"));
    server.close();
}

void TstStationSession::wsSchemeIsRefusedWhenAFingerprintIsPinned()
{
    // Critical 3, instance 1. No TLS needed to prove it, which is the
    // point: RemoteStationOptions::isValidStationUrl accepts ws:// and its
    // rejection message advertises it, connectToStation() refused only an
    // EMPTY fingerprint, and QWebSocket::sslErrors cannot fire on a link
    // with no TLS under it. An operator who had pinned a fingerprint
    // correctly and typed ws:// therefore got no pin comparison anywhere,
    // and handleHello() then put the pre-shared token on the wire in
    // cleartext.
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    const QString fingerprint =
        QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                       "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    client.connectToStation(QUrl(QStringLiteral("ws://127.0.0.1:50100")),
                            QStringLiteral("the-shared-secret"), fingerprint);

    // Refused SYNCHRONOUSLY, before anything was dialed at all. The old
    // code reached dialStation(), built a QWebSocket and opened it, so
    // this count was 0 on return.
    QCOMPARE(ended.count(), 1);
    QVERIFY2(client.transport() == nullptr,
             "a transport was created for a scheme that cannot carry a pin");
    QVERIFY2(client.lastError().contains(QStringLiteral("wss://")),
             qPrintable(client.lastError()));
    // Deliberately NOT asserting isPinSatisfied() here: the refusal
    // returns before any transport is attached, so that flag describes no
    // attach at all and reads as its neutral default. What is being pinned
    // is that nothing was dialed, which the two checks above cover.

    // And nothing was latched for the automatic reconnect to redial: a
    // scheme refusal cannot converge by being retried.
    QVERIFY(!client.isReconnectPending());
}

void TstStationSession::tokenIsNeverSentOnALinkWhosePinWasNeverChecked()
{
    // Critical 3, instance 2. QWebSocket::sslErrors fires ONLY when the
    // handshake produced errors, and the pin comparison lived exclusively
    // inside that handler. A handshake the client's own trust store
    // already accepts -- a corporate or antivirus MITM root, a real DV
    // certificate for a dynamic-DNS station name -- therefore reached
    // handleHello() with no comparison having happened anywhere, and
    // handleHello() sent the pre-shared token.
    //
    // WHAT THIS SLOT DOES NOT DO, stated plainly. It does not stand up a
    // genuinely error-free TLS handshake. That is not reachable in-process
    // against this station: CertificateStore issues CN "nereusd" with no
    // subjectAltName, so any connection to 127.0.0.1 raises
    // HostNameMismatch, and QWebSocket exposes no per-socket
    // setPeerVerifyName() to redirect the check (that is a QSslSocket
    // member, not a QSslConfiguration one, on Qt 6.11). Turning peer
    // verification off in the default configuration was tried and does not
    // help: Qt still emits sslErrors and merely continues afterwards, so
    // the old code's handler still ran and the case stayed invisible.
    //
    // So the property is pinned where it is actually specified instead:
    // the token must not leave this process while the pin is unchecked,
    // whatever produced that state. A LoopbackTransport carries no TLS at
    // all, which is the strongest possible form of "no certificate was
    // ever compared" -- strictly worse than the trusted-MITM case, and
    // driven through the same handleHello() gate that case now goes
    // through. Before the gate existed, the client answered the station's
    // Hello with an auth.request carrying the token.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client"), this);
    stationEnd->linkTo(clientEnd);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy authenticated(&server, &StationServer::clientAuthenticated);

    server.acceptTransport(stationEnd);

    // The CORRECT token, so nothing but the pin can refuse this, and a
    // fingerprint the link has no way to satisfy.
    const QString pin =
        QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                       "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    client.startSession(clientEnd, server.token(), pin);

    NEREUS_TRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
    QVERIFY(!client.isPinSatisfied());
    QVERIFY2(client.lastError().contains(QStringLiteral("no certificate")),
             qPrintable(client.lastError()));

    // The leak, asserted from both ends. On the wire: no auth.request ever
    // left the client. On the station: nobody proved they held the secret.
    NereusSDR::Test::settleSession();
    QVERIFY2(!stationEnd->receivedKinds().contains(QByteArrayLiteral("auth.request")),
             "the client sent its pre-shared token over a link whose certificate "
             "fingerprint had never been compared");
    QCOMPARE(authenticated.count(), 0);

    // And a seam session with NO pin stated is unaffected, which is every
    // other slot in this file: the default argument means those keep
    // authenticating exactly as before.
    QVERIFY(StationClient(&clientModel, &proxy).isPinSatisfied());
}

void TstStationSession::transientRefusalsStayRetryableAndABadTokenDoesNot()
{
    // Important 4. Two station-side refusals an operator actually hits are
    // transient by nature, and both used to take
    // disconnectFromStation()'s default of attemptReconnect = false, which
    // PERMANENTLY disarms automatic reconnect:
    //
    //   - "Station is at its concurrent-connection limit", a cap
    //     StationServer.h sizes for "one client, and a couple of stale
    //     sockets from a reconnecting client" -- so it is expected to be
    //     hit BY a reconnecting client, which then gave up forever.
    //   - "Too many failed authentication attempts", where the rate
    //     limiter is GLOBAL rather than per-peer (TokenStore.h:44-48), so
    //     a stranger's five bad guesses inside 60 s refuse the operator's
    //     correct token too.
    //
    // A wrong token must stay permanent, because retrying a wrong secret
    // forever is how the rate limiter above gets fed.
    //
    // Asserted on the WIRE FLAG rather than on client timer state, because
    // that flag is the whole mechanism: the client must not be classifying
    // refusals by matching the station's English prose.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setAuthDeadlineMs(0);  // out of the way; this slot is about refusals

    // ---- Peer-limit refusal ----
    QList<LoopbackTransport*> held;
    for (int i = 0; i < StationServer::kMaxConcurrentPeers; ++i) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("hold-%1").arg(i), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("hold-%1-c").arg(i), this);
        stationEnd->linkTo(clientEnd);
        server.acceptTransport(stationEnd);
        held.append(clientEnd);
    }
    auto* overflowStation = new LoopbackTransport(QStringLiteral("overflow"), this);
    auto* overflowClient = new LoopbackTransport(QStringLiteral("overflow-c"), this);
    overflowStation->linkTo(overflowClient);
    server.acceptTransport(overflowStation);

    NEREUS_TRY_VERIFY(overflowClient->receivedKinds().contains(QByteArrayLiteral("session.end")));
    SessionMessage limitEnd;
    for (const QByteArray& wire : overflowClient->received()) {
        const SessionMessage m = decodeOrFail(wire);
        if (m.kind == SessionMessageKind::SessionEnd) {
            limitEnd = m;
            break;
        }
    }
    QCOMPARE(limitEnd.kind, SessionMessageKind::SessionEnd);
    QVERIFY(limitEnd.reason.contains(QStringLiteral("as many connections as it allows")));
    QVERIFY2(limitEnd.retryable,
             "the concurrent-connection cap was sent as permanent, so a "
             "reconnecting client that hits it gives up forever");

    // ---- Bad token, then the rate limit it produces ----
    StationServer authServer(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    authServer.setAuthDeadlineMs(0);
    authServer.setAuthRateLimit(2, 60000);

    // Returns the CLIENT end so the caller can QTRY on it: LoopbackTransport
    // delivers through the event loop, so reading received() straight after
    // sendText() sees nothing.
    auto refuse = [&](const QString& candidate) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("guess"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("guess-c"), this);
        stationEnd->linkTo(clientEnd);
        authServer.acceptTransport(stationEnd);
        clientEnd->sendText(SessionMessages::encode(
            SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 6,
                                   QStringLiteral("guess"))));
        clientEnd->sendText(
            SessionMessages::encode(SessionMessages::authRequest(candidate)));
        return clientEnd;
    };

    auto refusalOn = [](LoopbackTransport* end) {
        SessionMessage result;
        for (const QByteArray& wire : end->received()) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::AuthResult && !m.accepted) {
                result = m;
            }
        }
        return result;
    };

    LoopbackTransport* firstEnd = refuse(QStringLiteral("not-the-token"));
    NEREUS_TRY_VERIFY(firstEnd->receivedKinds().contains(QByteArrayLiteral("auth.result")));
    const SessionMessage firstBad = refusalOn(firstEnd);
    QCOMPARE(firstBad.kind, SessionMessageKind::AuthResult);
    QCOMPARE(firstBad.reason, QStringLiteral("The Core did not accept this app's pairing token. Check the token saved for this Core."));
    QVERIFY2(!firstBad.retryable,
             "a wrong token was marked retryable, so a client would redial it "
             "forever and feed the station's own rate limiter");

    LoopbackTransport* secondEnd = refuse(QStringLiteral("still-not-the-token"));
    NEREUS_TRY_VERIFY(secondEnd->receivedKinds().contains(QByteArrayLiteral("auth.result")));
    const SessionMessage secondBad = refusalOn(secondEnd);
    QCOMPARE(secondBad.reason, QStringLiteral("The Core did not accept this app's pairing token. Check the token saved for this Core."));
    QVERIFY(!secondBad.retryable);

    // Two failures at a limit of two: the next attempt is rate limited,
    // and it would be even with the CORRECT token, which is exactly the
    // lockout this flag has to let the operator recover from.
    LoopbackTransport* lockedEnd = refuse(authServer.token());
    NEREUS_TRY_VERIFY(lockedEnd->receivedKinds().contains(QByteArrayLiteral("auth.result")));
    const SessionMessage locked = refusalOn(lockedEnd);
    QCOMPARE(locked.kind, SessionMessageKind::AuthResult);
    QVERIFY(locked.reason.contains(QStringLiteral("too many wrong ones")));
    QVERIFY2(locked.retryable,
             "a rate-limit lockout was sent as permanent, so a stranger's bad "
             "guesses lock the operator out with no automatic recovery");
}

void TstStationSession::lockedOutOperatorRetriesButABadTokenDoesNot()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // The client half of Important 4, end to end and over a real dial,
    // because only a dial latches a redial target for scheduleReconnect().
    //
    // The narrative, exactly as an operator meets it: a stranger who can
    // reach the port guesses wrong, the GLOBAL rate limiter trips
    // (TokenStore.h:44-48: a lockout refuses a connection "including one
    // carrying the correct token"), and the operator's own GUI is then
    // refused. Before this fix that refusal permanently disarmed automatic
    // reconnect, so the operator stayed locked out until they noticed and
    // reconnected by hand. Now the GUI backs off and comes back on its own
    // once the lockout expires.
    //
    // Both halves are discriminating. The stranger's wrong token must NOT
    // re-arm; the operator's rate-limited refusal MUST.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    // One failure trips the lockout, so the sequence below is two dials
    // rather than six.
    server.setAuthRateLimit(1, 60000);
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    const QUrl url(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()));

    // ---- The stranger ----
    RadioModel strangerModel(RadioModel::Role::Remote);
    SettingsProxy strangerProxy;
    StationClient stranger(&strangerModel, &strangerProxy);
    QSignalSpy strangerEnded(&stranger, &StationClient::sessionEnded);
    stranger.connectToStation(url, QStringLiteral("not-the-token"),
                              server.certificateFingerprint());
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(strangerEnded.count(), 1, 15000);
    QVERIFY(stranger.lastError().contains(QStringLiteral("did not accept this app's pairing token")));
    QVERIFY2(!stranger.isReconnectPending(),
             "a wrong token re-armed automatic reconnect, which would hammer the "
             "station's rate limiter and keep the operator locked out");

    // ---- The operator, refused by the stranger's lockout ----
    RadioModel operatorModel(RadioModel::Role::Remote);
    SettingsProxy operatorProxy;
    StationClient op(&operatorModel, &operatorProxy);
    QSignalSpy opEnded(&op, &StationClient::sessionEnded);
    op.connectToStation(url, server.token(), server.certificateFingerprint());
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(opEnded.count(), 1, 15000);
    QVERIFY2(op.lastError().contains(QStringLiteral("too many wrong ones")),
             qPrintable(op.lastError()));
    QVERIFY2(op.isReconnectPending(),
             "the operator's own client gave up permanently on a lockout that "
             "expires on its own, so a stranger's failed guesses locked them out "
             "of their own station until they reconnected by hand");

    // Cancel the pending retry before teardown so it cannot fire into a
    // station this slot is about to close.
    op.disconnectFromStation(QStringLiteral("test complete"));
    QVERIFY(!op.isReconnectPending());

    server.close();
}

// ── TLS ──────────────────────────────────────────────────────────────────

void TstStationSession::wssListenerComesUpAndCompletesAHandshake()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(1);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));
    QVERIFY(server.isListening());
    QVERIFY(server.serverPort() != 0);
    QVERIFY(!server.certificateFingerprint().isEmpty());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    const QUrl url(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()));
    client.connectToStation(url, server.token(), server.certificateFingerprint());

    NEREUS_TRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);
    QCOMPARE(ended.count(), 0);
    QVERIFY(clientModel.isConnected());
    QCOMPARE(clientModel.slices().size(), stationModel->slices().size());

    server.close();
}

void TstStationSession::wssRefusesAMismatchedCertificateFingerprint()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(
        settingsDir.filePath(QStringLiteral("NereusSDR.settings")));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    // Pinning is the entire identity check for a self-signed certificate
    // (parent design section 10.5). A client that connected anyway when
    // the fingerprint did not match would make the certificate model
    // decorative.
    const QString wrongFingerprint =
        QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                       "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    const QUrl url(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()));
    client.connectToStation(url, server.token(), wrongFingerprint);

    NEREUS_TRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 15000);
    QCOMPARE(completed.count(), 0);
    QVERIFY(!clientModel.isConnected());
    QVERIFY2(client.lastError().contains(QStringLiteral("fingerprint")),
             qPrintable(client.lastError()));

    // And an EMPTY pin is refused outright rather than silently accepting
    // anything, which is the failure mode that looks like it works.
    RadioModel unpinnedModel(RadioModel::Role::Remote);
    SettingsProxy unpinnedProxy;
    StationClient unpinned(&unpinnedModel, &unpinnedProxy);
    QSignalSpy unpinnedEnded(&unpinned, &StationClient::sessionEnded);
    unpinned.connectToStation(url, server.token(), QString());
    QCOMPARE(unpinnedEnded.count(), 1);
    QVERIFY(!unpinnedModel.isConnected());

    server.close();
}

void TstStationSession::failedInitialConnectReportsPromptly()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // Important 4's headline case. errorOccurred set m_lastError and
    // logged but emitted NOTHING, and onTransportClosed emitted
    // sessionEnded only `if (m_handshakeComplete)`, so a station that was
    // down, a wrong port or a refused TLS handshake produced no signal at
    // all for a full 40 to 60 second heartbeat window. Only the
    // fingerprint-mismatch path emitted, which is why the existing TLS
    // slot passed while the ORDINARY failure was uncovered.
    //
    // v0.5.1 shipped "connection state stuck Connected on failed initial
    // connect". Same bug class, so it gets a test rather than a comment.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 deadPort = probe.serverPort();
    probe.close();  // nothing is listening there now

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);

    const QString anyFingerprint =
        QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                       "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(deadPort)),
                            QStringLiteral("token"), anyFingerprint);

    // WELL inside one heartbeat interval, which is the entire point: the
    // old behaviour would have taken 40 to 60 seconds, and only then via
    // a mechanism that had nothing to do with the connect failing.
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
    QCOMPARE(completed.count(), 0);
    QVERIFY(!clientModel.isConnected());
    QVERIFY(!ended.first().first().toString().isEmpty());

    // And exactly once, however many socket errors and closes unwind.
    NereusSDR::Test::settleSession();
    QCOMPARE(ended.count(), 1);
}

// ---- R-R3-46: the window knows the Core's radio ---------------------------

namespace {

int updateIndexOf(const QList<MirrorUpdate>& updates, const QByteArray& name)
{
    for (qsizetype i = 0; i < updates.size(); ++i) {
        if (updates.at(i).name == name) { return int(i); }
    }
    return -1;
}

StationCapabilities g21kCaps()
{
    StationCapabilities caps;
    caps.stationName = QStringLiteral("Bench G2 1K");
    caps.radioModelName = QStringLiteral("ANAN-G2");
    caps.firmwareVersion = QStringLiteral("27");
    caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:46");
    caps.board = HPSDRHW::Saturn;
    caps.radioConnected = true;
    caps.radioIdentityEntries = true;
    caps.hpsdrModel = HPSDRModel::ANAN_G2_1K;
    caps.radioProtocol = 2;
    caps.radioAddress = QStringLiteral("192.168.1.50");
    return caps;
}

} // namespace

void TstStationSession::radioIdentityEntriesRoundTrip()
{
    // The three entries travel together and come back as sent;
    // radioHardwareVersion (R-R3-46, Task 2) follows in the same block, then
    // remotePgxlControlVersion and remoteRfKitControlVersion (R-R3-47), then
    // stationTciVersion (R-R3-48), then accessoryDataVersion (R-R3-47), then
    // remoteTgxlControlVersion (R-R3-47, the Tuner Genius's own settings),
    // then stationIdentityVersion (iPhone app Task 12), then
    // deviceAdminVersion (iPhone app Task 13), then pairingVersion (iPhone
    // app Task 14), then stationCatalogVersion (iPhone app Task 19), then
    // displayExtrasVersion (iPhone app Task 20), then
    // transmitSettingsVersion (R-R3-49, parity Task 1), then
    // bandSelectVersion (R-IOS-27, R-IOS-06), then meterReadingsVersion
    // (R-R3-13, parity Task 15), then dspInfoVersion (R-R3-49, parity Task
    // 16), then recordStreamVersion (R-IOS-25, parity Task 19), then
    // stationRadiosVersion (R-IOS-18, parity Task 21), then txDisplayVersion
    // (R-R3-49, parity Task 28), then displayClockVersion (R-R3-21, R-R3-08), then
    // controlChannelVersion (R-IOS-16, the Task 28 fix wave), then
    // txMonitorAudioVersion (R-IOS-13, parity Task 32), then
    // stationFreedvVersion (R-IOS-26, iPhone plan Task 22), then
    // mediaReplaceVersion, controlSwitchVersion and relayAllowed (R-IOS-16,
    // iPhone app plan Task 29), then supportBundleVersion (R-R3-49, parity
    // Task 22), then the unpublished media tunnel and relay-routing versions,
    // then remoteIqVersion (R-IOS-16).
    StationCapabilities sent = g21kCaps();
    sent.radioHardwareVersion = 1;
    sent.remotePgxlControlVersion = 1;
    sent.remoteRfKitControlVersion = 1;
    sent.stationTciVersion = 1;
    sent.accessoryDataVersion = 1;
    sent.remoteTgxlControlVersion = 1;
    sent.stationIdentityVersion = 1;
    sent.deviceAdminVersion = 1;
    sent.pairingVersion = 1;
    sent.stationCatalogVersion = 1;
    sent.displayExtrasVersion = 1;
    sent.transmitSettingsVersion = 1;
    sent.bandSelectVersion = 1;
    sent.meterReadingsVersion = 1;
    sent.dspInfoVersion = 1;
    sent.recordStreamVersion = 1;
    sent.stationRadiosVersion = 1;
    sent.txDisplayVersion = 1;
    sent.displayClockVersion = 1; // R-R3-21 / R-R3-08, after it
    sent.controlChannelVersion = 1; // R-IOS-16, after it
    sent.txMonitorAudioVersion = 1; // R-IOS-13 (parity Task 32), after it
    sent.stationFreedvVersion = 1; // R-IOS-26 (iPhone plan Task 22), after it
    sent.mediaReplaceVersion = 1;   // iPhone app plan Task 29, after it
    sent.controlSwitchVersion = 1;
    sent.relayAllowed = true;
    sent.supportBundleVersion = 1; // R-R3-49 (parity Task 22), after it
    sent.mediaTunnelVersion = 1;
    sent.mediaRelayRoutingVersion = 1;
    sent.remoteIqVersion = 1;
    sent.txModMonitorVersion = 1;
    const QList<MirrorUpdate> updates = sent.toUpdates();
    const int model = updateIndexOf(updates, "hpsdrModel");
    QCOMPARE(model, int(updates.size()) - 34);
    QCOMPARE(updateIndexOf(updates, "radioProtocol"), model + 1);
    QCOMPARE(updateIndexOf(updates, "radioAddress"), model + 2);
    QCOMPARE(updateIndexOf(updates, "radioHardwareVersion"), model + 3);
    QCOMPARE(updateIndexOf(updates, "remotePgxlControlVersion"), model + 4);
    QCOMPARE(updateIndexOf(updates, "remoteRfKitControlVersion"), model + 5);
    QCOMPARE(updateIndexOf(updates, "stationTciVersion"), model + 6);
    QCOMPARE(updateIndexOf(updates, "accessoryDataVersion"), model + 7);
    QCOMPARE(updateIndexOf(updates, "remoteTgxlControlVersion"), model + 8);
    QCOMPARE(updateIndexOf(updates, "stationIdentityVersion"), model + 9);
    QCOMPARE(updateIndexOf(updates, "deviceAdminVersion"), model + 10);
    QCOMPARE(updateIndexOf(updates, "pairingVersion"), model + 11);
    QCOMPARE(updateIndexOf(updates, "stationCatalogVersion"), model + 12);
    QCOMPARE(updateIndexOf(updates, "displayExtrasVersion"), model + 13);
    QCOMPARE(updateIndexOf(updates, "transmitSettingsVersion"), model + 14);
    QCOMPARE(updateIndexOf(updates, "bandSelectVersion"), model + 15);
    QCOMPARE(updateIndexOf(updates, "meterReadingsVersion"), model + 16);
    QCOMPARE(updateIndexOf(updates, "dspInfoVersion"), model + 17);
    QCOMPARE(updateIndexOf(updates, "recordStreamVersion"), model + 18);
    QCOMPARE(updateIndexOf(updates, "stationRadiosVersion"), model + 19);
    QCOMPARE(updateIndexOf(updates, "txDisplayVersion"), model + 20);
    QCOMPARE(updateIndexOf(updates, "displayClockVersion"), model + 21);
    QCOMPARE(updateIndexOf(updates, "controlChannelVersion"), model + 22);
    QCOMPARE(updateIndexOf(updates, "txMonitorAudioVersion"), model + 23);
    QCOMPARE(updateIndexOf(updates, "stationFreedvVersion"), model + 24);
    QCOMPARE(updateIndexOf(updates, "mediaReplaceVersion"), model + 25);
    QCOMPARE(updateIndexOf(updates, "controlSwitchVersion"), model + 26);
    QCOMPARE(updateIndexOf(updates, "relayAllowed"), model + 27);
    QCOMPARE(updateIndexOf(updates, "supportBundleVersion"), model + 28);
    QCOMPARE(updateIndexOf(updates, "mediaTunnelVersion"), model + 29);
    QCOMPARE(updateIndexOf(updates, "mediaRelayRoutingVersion"), model + 30);
    QCOMPARE(updateIndexOf(updates, "remoteIqVersion"), model + 31);
    QCOMPARE(updateIndexOf(updates, "txModMonitorVersion"), model + 32);
    QCOMPARE(updateIndexOf(updates, "accessoryTxVersion"), model + 33);
    const StationCapabilities received = StationCapabilities::fromUpdates(updates);
    QCOMPARE(received.mediaTunnelVersion, 1);
    QCOMPARE(received.mediaRelayRoutingVersion, 1);
    QCOMPARE(received.remoteIqVersion, 1);
    QCOMPARE(received.txModMonitorVersion, 1);
    QCOMPARE(received.txMonitorAudioVersion, 1);
    QCOMPARE(received.stationFreedvVersion, 1);
    QCOMPARE(received.supportBundleVersion, 1);
    QVERIFY(received.radioIdentityEntries);
    QCOMPARE(received.radioHardwareVersion, 1);
    QCOMPARE(received.remotePgxlControlVersion, 1);
    QCOMPARE(received.remoteRfKitControlVersion, 1);
    QCOMPARE(received.stationTciVersion, 1);
    QCOMPARE(received.accessoryDataVersion, 1);
    QCOMPARE(received.remoteTgxlControlVersion, 1);
    QCOMPARE(received.stationIdentityVersion, 1);
    QCOMPARE(received.deviceAdminVersion, 1);
    QCOMPARE(received.pairingVersion, 1);
    QCOMPARE(received.stationCatalogVersion, 1);
    QCOMPARE(received.displayExtrasVersion, 1);
    QCOMPARE(received.transmitSettingsVersion, 1);
    QCOMPARE(received.bandSelectVersion, 1);
    QCOMPARE(received.hpsdrModel, HPSDRModel::ANAN_G2_1K);
    QCOMPARE(received.radioProtocol, 2);
    QCOMPARE(received.radioAddress, QStringLiteral("192.168.1.50"));

    // Not negotiated: none of the three is on the wire, and an absent entry
    // reads as not reported.
    StationCapabilities older = sent;
    older.radioIdentityEntries = false;
    const QList<MirrorUpdate> olderUpdates = older.toUpdates();
    for (const char* name : {"hpsdrModel", "radioProtocol", "radioAddress",
                             "radioHardwareVersion", "remotePgxlControlVersion",
                             "remoteRfKitControlVersion"}) {
        QCOMPARE(updateIndexOf(olderUpdates, name), -1);
    }
    const StationCapabilities fromOlder = StationCapabilities::fromUpdates(olderUpdates);
    QVERIFY(!fromOlder.radioIdentityEntries);
    QCOMPARE(fromOlder.hpsdrModel, HPSDRModel::FIRST);
    QCOMPARE(fromOlder.radioProtocol, 0);
    QVERIFY(fromOlder.radioAddress.isEmpty());
    QCOMPARE(fromOlder.radioHardwareVersion, 0);
    QCOMPARE(fromOlder.remotePgxlControlVersion, 0);
    QCOMPARE(fromOlder.remoteRfKitControlVersion, 0);

    // Values this build cannot use read as not reported, never as a guess.
    QList<MirrorUpdate> odd = olderUpdates;
    odd.append(MirrorUpdate{0, "hpsdrModel", MirrorWireKind::Int64, QVariant(qlonglong(99))});
    odd.append(MirrorUpdate{0, "radioProtocol", MirrorWireKind::Int64, QVariant(qlonglong(7))});
    odd.append(MirrorUpdate{0, "radioAddress", MirrorWireKind::Utf8,
                            QVariant(QStringLiteral("not an address"))});
    const StationCapabilities fromOdd = StationCapabilities::fromUpdates(odd);
    QVERIFY(fromOdd.radioIdentityEntries);
    QCOMPARE(fromOdd.hpsdrModel, HPSDRModel::FIRST);
    QCOMPARE(fromOdd.radioProtocol, 0);
    QVERIFY(fromOdd.radioAddress.isEmpty());
}

void TstStationSession::coreSendsRadioIdentityOnlyFromMinorEleven()
{
    // The Core describes its own radio (its model choice included) to an app
    // at minor 11; a minor-10 app gets exactly the descriptor it had.
    const auto capture = [this](quint16 minor) {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("identity.settings")));
        auto model = std::make_unique<RadioModel>();
        model->setHpsdrModelForTest(HPSDRModel::ANAN_G2_1K);
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:46");
        info.name = QStringLiteral("Bench G2 1K");
        info.boardType = HPSDRHW::Saturn;
        info.protocol = ProtocolVersion::Protocol2;
        info.address = QHostAddress(QStringLiteral("192.168.1.50"));
        model->setLastRadioInfoForTest(info);
        model->setConnectionStateForTest(ConnectionState::Connected);
        StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        auto* station = new LoopbackTransport(QStringLiteral("identity-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("identity-peer"), this);
        station->linkTo(peer);
        server.acceptTransport(station);
        peer->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 6, QStringLiteral("identity-app"))));
        peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
        QList<MirrorUpdate> updates;
        [&] {
            NEREUS_TRY_VERIFY([&] {
                for (const QByteArray& wire : peer->received()) {
                    const SessionMessage m = decodeOrFail(wire);
                    if (m.kind == SessionMessageKind::Capabilities) {
                        updates = m.updates;
                        return true;
                    }
                }
                return false;
            }());
        }();
        return updates;
    };

    const QList<MirrorUpdate> current = capture(kRadioIdentitySessionProtocolMinor);
    const StationCapabilities caps = StationCapabilities::fromUpdates(current);
    QVERIFY(caps.radioIdentityEntries);
    QCOMPARE(caps.board, HPSDRHW::Saturn);
    QCOMPARE(caps.hpsdrModel, HPSDRModel::ANAN_G2_1K);
    QCOMPARE(caps.radioProtocol, 2);
    QCOMPARE(caps.radioAddress, QStringLiteral("192.168.1.50"));

    const QList<MirrorUpdate> older = capture(quint16(kRadioIdentitySessionProtocolMinor - 1));
    QVERIFY(!older.isEmpty());
    for (const char* name : {"hpsdrModel", "radioProtocol", "radioAddress",
                             "radioHardwareVersion", "remotePgxlControlVersion",
                             "remoteRfKitControlVersion", "stationTciVersion",
                             "accessoryDataVersion", "remoteTgxlControlVersion",
                             "stationIdentityVersion", "deviceAdminVersion",
                             "pairingVersion", "stationCatalogVersion",
                             "displayExtrasVersion", "transmitSettingsVersion",
                             "bandSelectVersion", "meterReadingsVersion",
                             "dspInfoVersion", "recordStreamVersion",
                             "stationRadiosVersion", "txDisplayVersion",
                             "displayClockVersion", "controlChannelVersion",
                             "txMonitorAudioVersion", "stationFreedvVersion",
                             "mediaReplaceVersion", "controlSwitchVersion", "relayAllowed",
                             "supportBundleVersion", "mediaTunnelVersion",
                             "mediaRelayRoutingVersion", "remoteIqVersion", "txModMonitorVersion",
                             "accessoryTxVersion"}) {
        QCOMPARE(updateIndexOf(older, name), -1);
    }
    // Byte for byte: the minor-11 descriptor without the thirty-three (and the
    // display budget reason, which is not sent here) is the minor-10 one.
    QList<MirrorUpdate> stripped = current;
    for (const char* name : {"hpsdrModel", "radioProtocol", "radioAddress",
                             "radioHardwareVersion", "remotePgxlControlVersion",
                             "remoteRfKitControlVersion", "stationTciVersion",
                             "accessoryDataVersion", "remoteTgxlControlVersion",
                             "stationIdentityVersion", "deviceAdminVersion",
                             "pairingVersion", "stationCatalogVersion",
                             "displayExtrasVersion", "transmitSettingsVersion",
                             "bandSelectVersion", "meterReadingsVersion",
                             "dspInfoVersion", "recordStreamVersion",
                             "stationRadiosVersion", "txDisplayVersion",
                             "displayClockVersion", "controlChannelVersion",
                             "txMonitorAudioVersion", "stationFreedvVersion",
                             "mediaReplaceVersion", "controlSwitchVersion", "relayAllowed",
                             "supportBundleVersion", "mediaTunnelVersion",
                             "mediaRelayRoutingVersion", "remoteIqVersion", "txModMonitorVersion",
                             "accessoryTxVersion"}) {
        stripped.removeAt(updateIndexOf(stripped, name));
    }
    QCOMPARE(SessionMessages::encode(SessionMessages::capabilities(stripped)),
             SessionMessages::encode(SessionMessages::capabilities(older)));

    // A Core that has never had a radio reports no model and nothing else.
    QTemporaryDir dir;
    AppSettings settings(dir.filePath(QStringLiteral("no-radio.settings")));
    RadioModel noRadio;
    StationServer server(&noRadio, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    const StationCapabilities none = server.buildCapabilities();
    QCOMPARE(none.board, HPSDRHW::Unknown);
    QCOMPARE(none.hpsdrModel, HPSDRModel::FIRST);
    QCOMPARE(none.radioProtocol, 0);
    QVERIFY(none.radioAddress.isEmpty());
}

void TstStationSession::remoteModelResolvesTheCoresRadio()
{
    RadioModel remote(RadioModel::Role::Remote);

    // The Core's model wins when it matches the board.
    remote.applyStationCapabilities(g21kCaps());
    QCOMPARE(remote.hardwareProfile().model, HPSDRModel::ANAN_G2_1K);
    QCOMPARE(remote.boardCapabilities().board, HPSDRHW::Saturn);
    QCOMPARE(remote.currentRadioInfo().protocol, ProtocolVersion::Protocol2);
    QCOMPARE(remote.currentRadioInfo().address, QHostAddress(QStringLiteral("192.168.1.50")));
    QCOMPARE(remote.currentRadioInfo().firmwareVersion, 27);
    QCOMPARE(remote.currentRadioInfo().macAddress, QStringLiteral("AA:BB:CC:DD:EE:46"));
    QCOMPARE(remote.currentRadioInfo().boardType, HPSDRHW::Saturn);
    QCOMPARE(remote.transmitModel().hpsdrModel(), HPSDRModel::ANAN_G2_1K);

    // ANAN-8000DLE keeps its own row on an OrionMKII board.
    StationCapabilities dle = g21kCaps();
    dle.board = HPSDRHW::OrionMKII;
    dle.hpsdrModel = HPSDRModel::ANAN8000D;
    remote.applyStationCapabilities(dle);
    QCOMPARE(remote.hardwareProfile().model, HPSDRModel::ANAN8000D);

    // A model that does not match the board is not trusted: the board picks.
    StationCapabilities mismatch = dle;
    mismatch.hpsdrModel = HPSDRModel::ANAN_G2_1K;
    remote.applyStationCapabilities(mismatch);
    QCOMPARE(remote.hardwareProfile().model, defaultModelForBoard(HPSDRHW::OrionMKII));

    // An older Core (no entries): the board picks, as before.
    StationCapabilities older = g21kCaps();
    older.radioIdentityEntries = false;
    older.hpsdrModel = HPSDRModel::FIRST;
    older.radioProtocol = 0;
    older.radioAddress.clear();
    remote.applyStationCapabilities(
        StationCapabilities::fromUpdates(older.toUpdates()));
    QCOMPARE(remote.hardwareProfile().model, HPSDRModel::ANAN_G2);
    QVERIFY(remote.currentRadioInfo().address.isNull());

    // A Core whose radio is offline and unknown gives Unknown, not Hermes,
    // whatever model it sends.
    StationCapabilities offline;
    offline.stationName = QStringLiteral("Core");
    offline.radioIdentityEntries = true;
    offline.hpsdrModel = HPSDRModel::HERMES;
    remote.applyStationCapabilities(offline);
    QCOMPARE(remote.hardwareProfile().model, HPSDRModel::FIRST);
    QCOMPARE(remote.hardwareProfile().effectiveBoard, HPSDRHW::Unknown);
    QCOMPARE(remote.boardCapabilities().board, HPSDRHW::Unknown);
    QVERIFY(!remote.boardCapabilities().hasPaProfile);
    QVERIFY(!remote.isConnected());

    // Local mode unchanged: an unknown board still resolves as it always
    // has, and a local model ignores a descriptor.
    QCOMPARE(defaultModelForBoard(HPSDRHW::Unknown), HPSDRModel::HERMES);
    RadioModel local;
    local.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
    local.applyStationCapabilities(g21kCaps());
    QCOMPARE(local.hardwareProfile().model, HPSDRModel::ANAN7000D);
}

void TstStationSession::remoteModelSignalsOncePerIdentityChange()
{
    RadioModel remote(RadioModel::Role::Remote);
    QSignalSpy radio(&remote, &RadioModel::currentRadioChanged);
    HPSDRModel modelAtSignal = HPSDRModel::LAST;
    bool connectedAtSignal = false;
    connect(&remote, &RadioModel::currentRadioChanged, this,
            [&](const RadioInfo& info) {
        // After the profile and the state, like a local connect.
        modelAtSignal = remote.hardwareProfile().model;
        connectedAtSignal = remote.isConnected();
        QCOMPARE(info.macAddress, remote.currentRadioInfo().macAddress);
    });

    remote.applyStationCapabilities(g21kCaps());
    QCOMPARE(radio.count(), 1);
    QCOMPARE(modelAtSignal, HPSDRModel::ANAN_G2_1K);
    QVERIFY(connectedAtSignal);
    QCOMPARE(radio.first().first().value<RadioInfo>().protocol, ProtocolVersion::Protocol2);

    // The same radio again, or a change that is not the radio's: nothing.
    remote.applyStationCapabilities(g21kCaps());
    StationCapabilities granted = g21kCaps();
    granted.txPermitted = true;
    granted.effectiveMaxSlices = 2;
    remote.applyStationCapabilities(granted);
    QCOMPARE(radio.count(), 1);

    // A different radio: once.
    StationCapabilities hl2 = g21kCaps();
    hl2.board = HPSDRHW::HermesLite;
    hl2.hpsdrModel = HPSDRModel::HERMESLITE;
    hl2.radioProtocol = 1;
    hl2.macAddress = QStringLiteral("AA:BB:CC:DD:EE:02");
    remote.applyStationCapabilities(hl2);
    QCOMPARE(radio.count(), 2);
    QCOMPARE(modelAtSignal, HPSDRModel::HERMESLITE);

    // A new address for the same radio is an identity change too.
    hl2.radioAddress = QStringLiteral("192.168.1.51");
    remote.applyStationCapabilities(hl2);
    QCOMPARE(radio.count(), 3);
}

void TstStationSession::coreOffersTheAttenuatorOnlyFromMinorEleven()
{
    // R-R3-46 / R-R3-11. An app at minor 11 is told radioHardwareVersion 1
    // and gets the `stepAtt` object and its changes. An app at minor 10 gets
    // neither: no entry, no schema, no object, no delta, so its burst is the
    // one it was built for; a write it sends anyway is refused in plain words.
    const auto aboutStepAtt = [](const QList<QByteArray>& wires) {
        int count = 0;
        for (const QByteArray& wire : wires) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::PropertyResult) {
                continue;
            }
            if (m.objectKey == "stepAtt"
                || (m.kind == SessionMessageKind::Schema && m.className == "StepAttenuatorFacade")) {
                ++count;
            }
        }
        return count;
    };
    // R-R3-46: the `alexAntennas` object follows the same rule.
    const auto aboutAlex = [](const QList<QByteArray>& wires) {
        int count = 0;
        for (const QByteArray& wire : wires) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.objectKey == "alexAntennas"
                || (m.kind == SessionMessageKind::Schema && m.className == "AlexAntennaFacade")) {
                ++count;
            }
        }
        return count;
    };
    // R-R3-46 fix wave: so does the read-only `ioBoard` object.
    const auto aboutIoBoard = [](const QList<QByteArray>& wires) {
        int count = 0;
        for (const QByteArray& wire : wires) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.objectKey == "ioBoard"
                || (m.kind == SessionMessageKind::Schema && m.className == "IoBoardHl2Facade")) {
                ++count;
            }
        }
        return count;
    };
    const auto run = [this, aboutStepAtt](quint16 minor, QList<QByteArray>* wires,
                            QList<SessionPropertyResult>* results) {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("step-att.settings")));
        auto core = makeStationRadioModel(0);
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        core->setStepAttController(&controller);
        StationServer server(core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        auto* station = new LoopbackTransport(QStringLiteral("step-att-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("step-att-peer"), this);
        station->linkTo(peer);
        server.acceptTransport(station);
        peer->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 6, QStringLiteral("step-att-app"))));
        peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
        [&] { NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))); }();
        controller.setAttenuation(7);
        peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "stepAtt",
            {MirrorUpdate{0, "attenuationDb", MirrorWireKind::Int64, QVariant(qlonglong(9))}},
            41)));
        [&] {
            NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("property.result")));
        }();
        // The Core's deltas are flushed on their own timer and can land
        // after the write's result; wait for them before reading the wire.
        if (minor >= kRadioIdentitySessionProtocolMinor) {
            [&] { NEREUS_TRY_VERIFY(aboutStepAtt(peer->received()) >= 3); }();
        }
        // Only the Core's own change happened; a refused write changed nothing.
        [&] { QCOMPARE(controller.attenuatorDb(), minor >= 11 ? 9 : 7); }();
        *wires = peer->received();
        for (const QByteArray& wire : std::as_const(*wires)) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::PropertyResult) {
                *results = m.propertyResults;
            }
        }
        core->setStepAttController(nullptr);
    };
    const auto capabilitiesIn = [](const QList<QByteArray>& wires) {
        for (const QByteArray& wire : wires) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind == SessionMessageKind::Capabilities) {
                return m.updates;
            }
        }
        return QList<MirrorUpdate>{};
    };

    QList<QByteArray> current;
    QList<SessionPropertyResult> currentResults;
    run(kRadioIdentitySessionProtocolMinor, &current, &currentResults);
    // R-R3-46: 3, since the Core's Alex antennas and the hardware apply
    // step (2), and its I/O board and the per-band antenna verb (3, fix
    // wave) are behind it too; 4 with the filter policy verb (R-R3-46 /
    // R-R3-21); 7 since parity Task 14; 8 with the Alex Filters tabs'
    // receive filter rows; 9 with setRadioSampleRate (parity ruling C4);
    // 10 with the Alex-1 Filters tab's low-pass rows; 11 with HL2 Options'
    // clock rows; 12 with resetLevelCalibration, startLevelCalibration and
    // cancelLevelCalibration (Level Cal); 13 with the receive audio to the
    // radio and HL2 Swap audio channels (radio codec lane).
    QCOMPARE(StationCapabilities::fromUpdates(capabilitiesIn(current)).radioHardwareVersion, 13);
    // Schema, object, the Core's change and the accepted write's echo.
    QVERIFY(aboutStepAtt(current) >= 3);
    QCOMPARE(currentResults.size(), 1);
    QVERIFY(currentResults.first().accepted);
    QVERIFY(aboutAlex(current) >= 2);  // its schema and object
    QVERIFY(aboutIoBoard(current) >= 2);  // its schema and object

    QList<QByteArray> older;
    QList<SessionPropertyResult> olderResults;
    run(quint16(kRadioIdentitySessionProtocolMinor - 1), &older, &olderResults);
    QCOMPARE(updateIndexOf(capabilitiesIn(older), "radioHardwareVersion"), -1);
    QCOMPARE(aboutStepAtt(older), 0);
    QCOMPARE(aboutAlex(older), 0);
    QCOMPARE(aboutIoBoard(older), 0);
    QCOMPARE(olderResults.size(), 1);
    QVERIFY(!olderResults.first().accepted);
    QCOMPARE(olderResults.first().reason,
             QStringLiteral("Update this app to change the radio's attenuator on this Core."));
    QVERIFY(OperatorWording::isPlain(olderResults.first().reason));
}

void TstStationSession::appStepAttenuatorSettingsWritesAreRefused()
{
    // R-R3-46 / R-R3-11. The Core's attenuator and preamp settings are its
    // controller's: a raw write or remove from an app (what an older app's
    // Setup sends) is refused with the plain "update this app" reason, and
    // the Core's saved value stays. Other hardware keys still land.
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    const QString mac = QStringLiteral("AA:BB:CC:DD:EE:01");
    const QString value = QStringLiteral("hardware/%1/options/stepAtt/rx1Value").arg(mac);
    const QString band = QStringLiteral("hardware/%1/options/stepAtt/rx1Band/40m").arg(mac);
    const QString mode = QStringLiteral("hardware/%1/options/autoAtt/rx1Mode").arg(mac);
    const QString preamp = QStringLiteral("hardware/%1/options/preamp/rx1Band/40m").arg(mac);
    s.stationSettings->setValue(value, QStringLiteral("10"));
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(s.proxy->ready());

    const QString reason = QStringLiteral(
        "This Core keeps its own attenuator and preamp settings. Update this app to change them.");
    QVERIFY(OperatorWording::isPlain(reason));
    QSignalSpy rejected(s.proxy.get(), &SettingsProxy::valueRejected);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    int expected = 0;
    for (const QString& key : {value, band, mode, preamp}) {
        s.proxy->setValue(key, QStringLiteral("20"));
        ++expected;
        NEREUS_TRY_COMPARE(rejected.count(), expected);
        QCOMPARE(toast.last().at(0).toString(), reason);
    }
    QCOMPARE(s.stationSettings->value(value).toString(), QStringLiteral("10"));
    QVERIFY(!s.stationSettings->contains(band));
    QVERIFY(!s.stationSettings->contains(mode));
    QVERIFY(!s.stationSettings->contains(preamp));

    s.proxy->remove(value);
    ++expected;
    NEREUS_TRY_COMPARE(rejected.count(), expected);
    QCOMPARE(toast.last().at(0).toString(), reason);
    QCOMPARE(s.stationSettings->value(value).toString(), QStringLiteral("10"));

    const QString rate = QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac);
    s.proxy->setValue(rate, QStringLiteral("192000"));
    NEREUS_TRY_COMPARE(s.stationSettings->value(rate).toString(), QStringLiteral("192000"));
    QCOMPARE(rejected.count(), expected);
}

void TstStationSession::windowAttenuatorEditsWaitForACoreThatOffersThem()
{
    // R-R3-46 / R-R3-11. Against a Core without radioHardwareVersion the
    // window's attenuator object refuses an edit in plain words and sends
    // nothing; with it, the edit goes to the Core as a property write.
    for (const int version : {0, 1}) {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("step-att-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("step-att-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
        station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        StationCapabilities caps;
        caps.propertyResultVersion = 1;
        caps.radioIdentityEntries = true;
        caps.radioHardwareVersion = version;
        station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        NEREUS_TRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(client.remoteRadioHardwareAvailable(), version >= 1);

        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        QVERIFY(!stepAtt->isBound());
        QSignalSpy refused(stepAtt, &StepAttenuatorFacade::editRejected);
        station->clearReceived();
        stepAtt->setAttenuationDb(15);
        if (version == 0) {
            QCOMPARE(refused.count(), 1);
            const QString reason = refused.last().at(0).toString();
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
            QCOMPARE(stepAtt->attenuationDb(), 0);
            NereusSDR::Test::settleSession();
            QVERIFY(!station->receivedKinds().contains(QByteArrayLiteral("property.write")));
        } else {
            QCOMPARE(refused.count(), 0);
            QCOMPARE(stepAtt->attenuationDb(), 15);
            NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("property.write")));
        }
    }
}

namespace {

// R-R3-46: a Core with its step attenuator bound (radioHardwareVersion 2),
// joined by a remote window. The server writes to `serverSettings`.
struct HardwareSession {
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StepAttenuatorController> stepAtt;
    std::unique_ptr<StationServer> server;
    std::unique_ptr<RadioModel> window;
    std::unique_ptr<SettingsProxy> proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* stationEnd = nullptr;
};

const QString kHardwareMac = QStringLiteral("AA:BB:CC:DD:EE:01");  // makeStationRadioModel's

void joinHardwareWindow(HardwareSession& s, AppSettings& serverSettings, QObject* owner,
                        const QString& securityDir, bool alexBoard = false,
                        int extraSlices = 0)
{
    s.core = makeStationRadioModel(extraSlices);
    if (alexBoard) {
        s.core->setBoardForTest(HPSDRHW::Hermes);
    }
    s.stepAtt = std::make_unique<StepAttenuatorController>();
    s.stepAtt->setTickTimerEnabled(false);
    s.core->setStepAttController(s.stepAtt.get());
    s.server = std::make_unique<StationServer>(s.core.get(), serverSettings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
    s.window = std::make_unique<RadioModel>(RadioModel::Role::Remote);
    s.proxy = std::make_unique<SettingsProxy>();
    s.client = std::make_unique<StationClient>(s.window.get(), s.proxy.get());
    auto* stationEnd = new LoopbackTransport(QStringLiteral("hw-station"), owner);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("hw-client"), owner);
    stationEnd->linkTo(clientEnd);
    s.stationEnd = stationEnd;
    QSignalSpy completed(s.client.get(), &StationClient::handshakeComplete);
    s.client->startSession(clientEnd, s.server->token());
    s.server->acceptTransport(stationEnd);
    NEREUS_TRY_COMPARE(completed.count(), 1);
    NEREUS_TRY_VERIFY(s.proxy->ready());
    QVERIFY(s.client->remoteHardwareConfigAvailable());
}

void leaveHardwareSession(HardwareSession& s)
{
    if (s.core) {
        s.core->setStepAttController(nullptr);
    }
}

} // namespace

void TstStationSession::windowAntennaEditsReachTheCoresController()
{
    // R-R3-46. The window's receive antenna edits reach the Core's own
    // AlexController (the radio's relays and the Core's saved per-band
    // choice follow it); the Core's transmit antennas and relay switches
    // arrive in the window as the Core reports them.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("hw.settings")));
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }

    AlexAntennaFacade* window = s.window->alexAntennaFacade();
    QVERIFY(!window->isBound());
    QVERIFY(s.core->alexAntennaFacade()->isBound());
    QSignalSpy refused(window, &AlexAntennaFacade::editRejected);
    // The Core's controller knows its radio, as after the Core's connect;
    // it saves to the store the Core's controllers use.
    AppSettings& coreStore = AppSettings::instance();
    coreStore.clearHardwareValues(kHardwareMac);
    const auto cleanStore = qScopeGuard([&coreStore] { coreStore.clearHardwareValues(kHardwareMac); });
    s.core->alexControllerMutable().setMacAddress(kHardwareMac);

    window->setRxAnt(Band::Band40m, 2);
    NEREUS_TRY_COMPARE(s.core->alexController().rxAnt(Band::Band40m), 2);
    // Saved on the Core by its own controller (the save its teardown
    // repeats), so its later saves keep the change.
    NEREUS_TRY_COMPARE(coreStore.value(QStringLiteral("hardware/%1/alex/antenna/40m/rx")
                                     .arg(kHardwareMac)).toString(),
                 QStringLiteral("2"));
    window->setRxOnlyAnt(Band::Band20m, 3);
    NEREUS_TRY_COMPARE(s.core->alexController().rxOnlyAnt(Band::Band20m), 3);
    window->setUseTxAntennaForRx(true);
    NEREUS_TRY_VERIFY(s.core->alexController().useTxAntForRx());
    // Group B fix wave (radioHardwareVersion 5): RX bypass on TX, the VFO
    // flag's BYPS, reaches the Core's controller too, which clears Ext1
    // and Ext2 out on TX as Thetis's chkRxOutOnTx does.
    QVERIFY(s.client->remoteRxBypassOnTxAvailable());
    QVERIFY(s.client->rxBypassOnTxUnavailableReason().isEmpty());
    s.core->alexControllerMutable().setExt1OutOnTx(true);
    NEREUS_TRY_VERIFY(window->ext1OutOnTx());
    window->setRxOutOnTx(true);
    NEREUS_TRY_VERIFY(s.core->alexController().rxOutOnTx());
    QVERIFY(!s.core->alexController().ext1OutOnTx());
    NEREUS_TRY_VERIFY(!window->ext1OutOnTx());
    QVERIFY(window->rxOutOnTx());
    window->setRxOutOnTx(false);
    NEREUS_TRY_VERIFY(!s.core->alexController().rxOutOnTx());
    // The Core's own change reaches the window.
    s.core->alexControllerMutable().setRxOutOnTx(true);
    NEREUS_TRY_VERIFY(window->rxOutOnTx());
    QCOMPARE(refused.count(), 0);

    // The Core's transmit settings, changed on the Core, reach the window.
    s.core->alexControllerMutable().setTxAnt(Band::Band20m, 3);
    s.core->alexControllerMutable().setBlockTxAnt2(true);
    NEREUS_TRY_COMPARE(window->txAnt(Band::Band20m), 3);
    NEREUS_TRY_VERIFY(window->blockTxAnt2());
    // Parity Task 12 (radioHardwareVersion 6): and the window's reach the
    // Core's controller.
    QVERIFY(s.client->remoteTransmitAntennasAvailable());
    window->setTxAnt(Band::Band40m, 3);
    NEREUS_TRY_COMPARE(s.core->alexController().txAnt(Band::Band40m), 3);
    window->setBlockTxAnt2(false);
    NEREUS_TRY_VERIFY(!s.core->alexController().blockTxAnt2());
    window->setExt2OutOnTx(true);
    NEREUS_TRY_VERIFY(s.core->alexController().ext2OutOnTx());
    NEREUS_TRY_VERIFY(!window->rxOutOnTx());  // cleared by the Core, as Thetis does
    window->setRxOutOverride(true);
    NEREUS_TRY_VERIFY(s.core->alexController().rxOutOverride());
    // And a receive change made on the Core.
    s.core->alexControllerMutable().setRxAnt(Band::Band80m, 3);
    NEREUS_TRY_COMPARE(window->rxAnt(Band::Band80m), 3);
}

void TstStationSession::windowAntennaEditsWaitForACoreThatOffersThem()
{
    // R-R3-46. Against a Core below radioHardwareVersion 2 (an attenuator
    // only Core, or none) the window's antenna object refuses an edit in
    // plain words and sends nothing; with it, the edit is a property write.
    // R-R3-46 fix wave: from radioHardwareVersion 3 a band's edit is the
    // setAlexRxAntenna command, not a whole-list property write, and the
    // window's value follows the Core's answer.
    for (const int version : {0, 1, 2, 3, 4, 5, 6}) {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("alex-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("alex-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
        station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        StationCapabilities caps;
        caps.propertyResultVersion = 1;
        caps.radioIdentityEntries = true;
        caps.radioHardwareVersion = version;
        station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        NEREUS_TRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(client.remoteHardwareConfigAvailable(), version >= 2);
        // Group B fix wave: RX bypass on TX from radioHardwareVersion 5.
        QCOMPARE(client.remoteRxBypassOnTxAvailable(), version >= 5);
        QCOMPARE(client.rxBypassOnTxUnavailableReason().isEmpty(), version >= 5);
        // Parity Task 12: the rest of the transmit antennas and relays from 6.
        QCOMPARE(client.remoteTransmitAntennasAvailable(), version >= 6);
        // Parity mini-round: and one band's TX antenna at a time
        // (setAlexTxAntenna) from 6.
        QCOMPARE(remote.alexAntennaFacade()->hasTxBandEditSender(), version >= 6);
        const QString txAntennasReason = client.transmitAntennasUnavailableReason();
        QCOMPARE(txAntennasReason.isEmpty(), version >= 6);
        if (version >= 2 && version < 6) {
            QVERIFY2(OperatorWording::isPlain(txAntennasReason), qPrintable(txAntennasReason));
        }
        const QString reason = client.hardwareConfigUnavailableReason();
        QCOMPARE(reason.isEmpty(), version >= 2);
        if (version < 2) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
            // The I/O board probe waits for the same Core.
            const IStationLink::CommandOutcome probe = client.requestIoBoardProbe();
            QVERIFY(!probe.sent);
            QCOMPARE(probe.reason, reason);
        }

        AlexAntennaFacade* alex = remote.alexAntennaFacade();
        QSignalSpy refused(alex, &AlexAntennaFacade::editRejected);
        station->clearReceived();
        alex->setRxAnt(Band::Band40m, 2);
        if (version < 2) {
            QCOMPARE(refused.count(), 1);
            QCOMPARE(refused.last().at(0).toString(), reason);
            QCOMPARE(alex->rxAnt(Band::Band40m), 1);
            NereusSDR::Test::settleSession();
            QVERIFY(!station->receivedKinds().contains(QByteArrayLiteral("property.write")));
        } else if (version == 2) {
            QCOMPARE(refused.count(), 0);
            QCOMPARE(alex->rxAnt(Band::Band40m), 2);
            NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("property.write")));
        } else {
            QCOMPARE(refused.count(), 0);
            NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
            NereusSDR::Test::settleSession();
            QVERIFY(!station->receivedKinds().contains(QByteArrayLiteral("property.write")));
            QCOMPARE(alex->rxAnt(Band::Band40m), 1);  // until the Core's delta
        }
    }
}

void TstStationSession::appRawAntennaSettingsWritesAreRefused()
{
    // R-R3-46. The Core's Alex antenna settings are its AlexController's
    // (saved by it, again at teardown): a raw write or remove from an app
    // is refused with the plain "update this app" reason, and the Core's
    // saved value stays.
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    const QString rx = QStringLiteral("hardware/%1/alex/antenna/40m/rx").arg(kHardwareMac);
    const QString block = QStringLiteral("hardware/%1/alex/antenna/blockTxAnt2").arg(kHardwareMac);
    s.stationSettings->setValue(rx, QStringLiteral("1"));
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }

    const QString reason = QStringLiteral(
        "This Core keeps its own antenna settings. Update this app to change them.");
    QVERIFY(OperatorWording::isPlain(reason));
    QSignalSpy rejected(s.proxy.get(), &SettingsProxy::valueRejected);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    s.proxy->setValue(rx, QStringLiteral("3"));
    NEREUS_TRY_COMPARE(rejected.count(), 1);
    QCOMPARE(toast.last().at(0).toString(), reason);
    s.proxy->setValue(block, QStringLiteral("True"));
    NEREUS_TRY_COMPARE(rejected.count(), 2);
    s.proxy->remove(rx);
    NEREUS_TRY_COMPARE(rejected.count(), 3);
    QCOMPARE(toast.last().at(0).toString(), reason);
    QCOMPARE(s.stationSettings->value(rx).toString(), QStringLiteral("1"));
    QVERIFY(!s.stationSettings->contains(block));
}

void TstStationSession::hardwareWritesForAnotherRadioAreRefused()
{
    // R-R3-46. The Core applies hardware settings for the radio it is
    // connected to; a write or remove naming another radio's MAC is refused
    // in plain words and lands nowhere. The connected radio's keys, and
    // hardware/oc/ (not a MAC), still land.
    NotchSession s;
    prepareNotchCore(s);
    if (QTest::currentTestFailed()) { return; }
    const QString other = QStringLiteral("hardware/11:22:33:44:55:66/radioInfo/sampleRate");
    s.stationSettings->setValue(other, QStringLiteral("48000"));
    joinNotchWindow(s, this, m_securityDir.path());
    if (QTest::currentTestFailed()) { return; }

    const QString reason =
        QStringLiteral("These settings are for a radio this Core is not connected to.");
    QVERIFY(OperatorWording::isPlain(reason));
    QSignalSpy rejected(s.proxy.get(), &SettingsProxy::valueRejected);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    s.proxy->setValue(other, QStringLiteral("192000"));
    NEREUS_TRY_COMPARE(rejected.count(), 1);
    QCOMPARE(toast.last().at(0).toString(), reason);
    QCOMPARE(s.stationSettings->value(other).toString(), QStringLiteral("48000"));
    s.proxy->remove(other);
    NEREUS_TRY_COMPARE(rejected.count(), 2);
    QCOMPARE(toast.last().at(0).toString(), reason);
    QCOMPARE(s.stationSettings->value(other).toString(), QStringLiteral("48000"));

    // Its own radio, in any letter case, and the literal hardware/oc/.
    const QString own = QStringLiteral("hardware/%1/xvtr/autoSelectBand")
                            .arg(kHardwareMac.toLower());
    s.proxy->setValue(own, QStringLiteral("True"));
    NEREUS_TRY_COMPARE(s.stationSettings->value(own).toString(), QStringLiteral("True"));
    const QString oc = QStringLiteral("hardware/oc/usbBcd/enabled");
    s.proxy->setValue(oc, QStringLiteral("True"));
    NEREUS_TRY_COMPARE(s.stationSettings->value(oc).toString(), QStringLiteral("True"));
    QCOMPARE(rejected.count(), 2);

    // With no radio connected the Core takes no radio's hardware settings.
    s.core->setConnectionStateForTest(ConnectionState::Disconnected);
    const QString rate = QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(kHardwareMac);
    s.proxy->setValue(rate, QStringLiteral("96000"));
    NEREUS_TRY_COMPARE(rejected.count(), 3);
    QVERIFY(!s.stationSettings->contains(rate));
}

void TstStationSession::coreAppliesHardwareConfigWritesLive()
{
    // R-R3-46. A Hardware Config write from a window reaches the Core's
    // own controllers now, not at its next connect: the OC receive pins
    // (which the codec reads every frame), the HL2 N2ADR filter board and
    // the frequency calibration (which the P2 codec reads every command).
    // A burst of keys costs one reload per controller. The Core's settings
    // store here is the one its controllers read, as on a real Core.
    AppSettings& settings = AppSettings::instance();
    settings.clearHardwareValues(kHardwareMac);
    const auto cleanSettings = qScopeGuard([&settings] {
        settings.clearHardwareValues(kHardwareMac);
    });
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(s.core->boardCapabilities().hasIoBoardHl2);
    QStringList reloads;
    s.core->setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

    // An OC receive pin, as a window's OcMatrix saves one pin click (only
    // the key that changed).
    const QString pin = QStringLiteral("hardware/%1/oc/rx/40m/pin3").arg(kHardwareMac);
    QVERIFY(!s.core->ocMatrix().pinEnabled(Band::Band40m, 2, /*tx=*/false));
    s.proxy->setValue(pin, QStringLiteral("True"));
    s.proxy->setValue(QStringLiteral("hardware/%1/oc/rx/20m/pin4").arg(kHardwareMac),
                      QStringLiteral("True"));
    NEREUS_TRY_VERIFY(s.core->ocMatrix().pinEnabled(Band::Band40m, 2, /*tx=*/false));
    QVERIFY(s.core->ocMatrix().pinEnabled(Band::Band20m, 3, /*tx=*/false));
    QCOMPARE(reloads, QStringList{QStringLiteral("oc")});

    // The frequency calibration factor and the 10 MHz reference.
    reloads.clear();
    s.proxy->setValue(QStringLiteral("hardware/%1/cal/freqFactor").arg(kHardwareMac),
                      QStringLiteral("1.000001"));
    s.proxy->setValue(QStringLiteral("hardware/%1/cal/using10M").arg(kHardwareMac),
                      QStringLiteral("True"));
    NEREUS_TRY_VERIFY(s.core->calibrationController().using10MHzRef());
    QCOMPARE(s.core->calibrationController().freqCorrectionFactor(), 1.000001);
    QCOMPARE(reloads, QStringList{QStringLiteral("cal")});
    // The reload leaves the Core a PA forward-power table, as a connect does.
    QVERIFY(s.core->calibrationController().paCalProfile().boardClass
            != PaCalBoardClass::None);

    // The N2ADR switch: off clears the filter-board pins, on fills them
    // (mi0bot's preset), each saved on the Core.
    reloads.clear();
    const QString n2adr = QStringLiteral("hardware/%1/hl2IoBoard/n2adrFilter").arg(kHardwareMac);
    s.proxy->setValue(n2adr, QStringLiteral("False"));
    NEREUS_TRY_COMPARE(reloads, QStringList{QStringLiteral("n2adr")});
    QVERIFY(!s.core->ocMatrix().pinEnabled(Band::Band40m, 2, /*tx=*/false));
    reloads.clear();
    s.proxy->setValue(n2adr, QStringLiteral("True"));
    NEREUS_TRY_COMPARE(reloads, QStringList{QStringLiteral("n2adr")});
    QVERIFY(s.core->ocMatrix().pinEnabled(Band::Band80m, 6, /*tx=*/false));
    QCOMPARE(settings.value(QStringLiteral("hardware/%1/oc/rx/80m/pin7").arg(kHardwareMac))
                 .toString(),
             QStringLiteral("True"));

    // A key no controller holds live changes nothing on the Core.
    reloads.clear();
    s.proxy->setValue(QStringLiteral("hardware/%1/xvtr/autoSelectBand").arg(kHardwareMac),
                      QStringLiteral("True"));
    NereusSDR::Test::settleSession();
    QVERIFY(reloads.isEmpty());
}

void TstStationSession::coreAppliesHl2ClockWritesLive()
{
    // HL2 clock options (radioHardwareVersion 10). Enable CL2, the CL2
    // frequency and External 10 MHz from a window reach the Core's HL2
    // options through the "hl2" reload, which sends them to its radio
    // (RadioModel::applyHl2Options -> P1RadioConnection::setHl2Clock, whose
    // bytes tst_p1_hl2_clock checks). They are not transmit keys, so a
    // receive-only Core takes them, as mi0bot's handlers carry no MOX check
    // (setup.cs:21732-21756 [@c26a8a4]). A frequency that is not a number
    // or is outside 1..200 MHz (udCl2Freq, setup.designer.cs:11133-11163
    // [@c26a8a4]) is refused with a plain reason, and the Core keeps its
    // own. Three decimal places carry through.
    AppSettings& settings = AppSettings::instance();
    settings.clearHardwareValues(kHardwareMac);
    const auto cleanSettings = qScopeGuard([&settings] {
        settings.clearHardwareValues(kHardwareMac);
    });
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(s.core->receiveOnlyStationPolicy());
    QStringList reloads;
    s.core->setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });
    QSignalSpy rejected(s.proxy.get(), &SettingsProxy::valueRejected);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    const Hl2OptionsModel& hl2 = s.core->hl2Options();
    QVERIFY(!hl2.cl2Enabled());
    QCOMPARE(hl2.cl2FreqKHz(), 116000);
    QVERIFY(!hl2.ext10MHz());
    const auto hw = [](const char* rest) {
        return QStringLiteral("hardware/%1/%2").arg(kHardwareMac, QLatin1String(rest));
    };

    s.proxy->setValue(hw("hl2/cl2Enable"), QStringLiteral("True"));
    s.proxy->setValue(hw("hl2/cl2FreqMHz"), QStringLiteral("24.576"));
    s.proxy->setValue(hw("hl2/ext10MHz"), QStringLiteral("True"));
    QTRY_VERIFY(hl2.ext10MHz());
    QTRY_COMPARE(hl2.cl2FreqKHz(), 24576);
    QVERIFY(hl2.cl2Enabled());
    QVERIFY(!reloads.isEmpty());
    for (const QString& name : reloads) {
        QCOMPARE(name, QStringLiteral("hl2"));
    }
    QCOMPARE(settings.value(hw("hl2/cl2FreqMHz")).toString(), QStringLiteral("24.576"));

    const QString key = hw("hl2/cl2FreqMHz");
    int expected = 0;
    for (const char* bad : {"500", "0", "999", "nan", "inf", "abc"}) {
        s.proxy->setValue(key, QString::fromLatin1(bad));
        ++expected;
        QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), expected, 2000);
        QCOMPARE(rejected.last().at(0).toString(), key);
        QCOMPARE(rejected.last().at(1).toString(), QStringLiteral("24.576"));
        QCOMPARE(toast.last().at(0).toString(),
                 QStringLiteral("Choose a CL2 frequency from 1 to 200 MHz."));
        QCOMPARE(settings.value(key).toString(), QStringLiteral("24.576"));
        QCOMPARE(hl2.cl2FreqKHz(), 24576);
    }

    s.proxy->setValue(hw("hl2/cl2Enable"), QStringLiteral("False"));
    s.proxy->setValue(hw("hl2/ext10MHz"), QStringLiteral("False"));
    QTRY_VERIFY(!hl2.ext10MHz());
    QVERIFY(!hl2.cl2Enabled());
    QCOMPARE(rejected.count(), expected);
}

void TstStationSession::receiveOnlyCoreRefusesTransmitHardwareKeys()
{
    // R-R3-46 / R-R3-21 (transmit safety). The Core's hardware apply step
    // reloads oc/, cal/ and hl2/ into its live controllers, so a raw write
    // of a transmit-side hardware key would reach the radio's transmit
    // path. A receive-only Core refuses every such key the parity tasks
    // have not moved onto its off-air list, write and remove, with the
    // transmit reason: its saved value and its live controller stay as they
    // were, and nothing is reloaded.
    AppSettings& settings = AppSettings::instance();
    settings.clearHardwareValues(kHardwareMac);
    const QStringList globalKeys{QStringLiteral("hardware/oc/extPa/model"),
                                 QStringLiteral("hardware/oc/extPa/biasDelayMs")};
    for (const QString& key : globalKeys) {
        settings.remove(key);
    }
    const auto cleanSettings = qScopeGuard([&settings, globalKeys] {
        settings.clearHardwareValues(kHardwareMac);
        for (const QString& key : globalKeys) {
            settings.remove(key);
        }
    });
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(s.core->receiveOnlyStationPolicy());
    QStringList reloads;
    s.core->setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });
    QSignalSpy rejected(s.proxy.get(), &SettingsProxy::valueRejected);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    const QString reason =
        QStringLiteral("Transmit configuration is unavailable on this receive-only Core.");

    const QString mac = kHardwareMac;
    const auto hw = [&mac](const QString& rest) {
        return QStringLiteral("hardware/%1/%2").arg(mac, rest);
    };
    const OcMatrix& oc = s.core->ocMatrix();
    const CalibrationController& cal = s.core->calibrationController();
    const Hl2OptionsModel& hl2 = s.core->hl2Options();
    const OcMatrix::TXPinAction action1 = oc.pinAction(0);
    const double txDisplay = cal.txDisplayOffsetDb();
    const double paSens = cal.paCurrentSensitivity();
    const double paOffset = cal.paCurrentOffset();
    const PaCalProfile paTable = cal.paCalProfile();
    const int pttHang = hl2.pttHangMs();
    const int txLatency = hl2.txLatencyMs();
    QVERIFY(!oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));

    // One key of each class, with a value its controller would take.
    // R-R3-49 (parity Task 6): the PA forward-power table
    // (paCalibration/boardClass, calPoint1..10) and the PA profiles (pa/...)
    // are taken off the air at transmitSettingsVersion 6
    // (tst_remote_pa_pages). R-R3-46 / R-R3-49 (parity Task 13): the OC
    // transmit pins (oc/tx/...), the OC pin actions (oc/actions/...) and
    // TX Display Cal and Volts/Amps Calibration (cal/txDisplayOffset,
    // paSens, paOffset, and the Calibration tab's paCalibration/cal/
    // copies) at version 8 (tst_remote_oc_cal).
    const QList<QPair<QString, QString>> writes{
        {hw(QStringLiteral("hl2/pttHangMs")), QStringLiteral("30")},
        {hw(QStringLiteral("hl2/txLatencyMs")), QStringLiteral("40")},
        {hw(QStringLiteral("tx/UserDigOut")), QStringLiteral("15")},
        {hw(QStringLiteral("powerByBand/40m")), QStringLiteral("100")},
        {hw(QStringLiteral("tunePowerByBand/40m")), QStringLiteral("100")},
        {hw(QStringLiteral("ocOutputs/hardware/oc/extPa/model")), QStringLiteral("2")},
        {QStringLiteral("hardware/oc/extPa/model"), QStringLiteral("2")},
        {QStringLiteral("hardware/oc/extPa/biasDelayMs"), QStringLiteral("50")},
        // Follow-up item 4. The three transmit high-pass switches are taken
        // from a window at radioHardwareVersion 7 since parity Task 14, and
        // the Alex-1 low-pass rows at version 10 (tst_remote_hl2_io,
        // tst_alex_lpf_rows), so neither is listed here.
        {QStringLiteral("hardware/oc/allowHotSwitching"), QStringLiteral("True")},
    };
    int expected = 0;
    for (const auto& [key, value] : writes) {
        s.proxy->setValue(key, value);
        ++expected;
        NEREUS_TRY_COMPARE_WITH_TIMEOUT(rejected.count(), expected, 2000);
        QCOMPARE(rejected.last().at(0).toString(), key);
        QVERIFY2(!settings.contains(key), qPrintable(key));
        QCOMPARE(toast.last().at(0).toString(), reason);
    }

    // A remove of a transmit key the Core already holds keeps it.
    const QString held = hw(QStringLiteral("hl2/txLatencyMs"));
    settings.setValue(held, QStringLiteral("25"));
    s.proxy->remove(held);
    ++expected;
    NEREUS_TRY_COMPARE(rejected.count(), expected);
    QCOMPARE(rejected.last().at(0).toString(), held);
    QCOMPARE(rejected.last().at(1).toString(), QStringLiteral("25"));
    QCOMPARE(settings.value(held).toString(), QStringLiteral("25"));

    // Nothing reached the Core's controllers.
    NereusSDR::Test::settleSession();
    QVERIFY2(reloads.isEmpty(), qPrintable(reloads.join(QLatin1Char(','))));
    QVERIFY(!oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));
    QCOMPARE(oc.pinAction(0), action1);
    QCOMPARE(cal.txDisplayOffsetDb(), txDisplay);
    QCOMPARE(cal.paCurrentSensitivity(), paSens);
    QCOMPARE(cal.paCurrentOffset(), paOffset);
    QCOMPARE(cal.paCalProfile().boardClass, paTable.boardClass);
    QVERIFY(cal.paCalProfile().watts == paTable.watts);
    QCOMPARE(hl2.pttHangMs(), pttHang);
    QCOMPARE(hl2.txLatencyMs(), txLatency);

    // Follow-up item 1: the N2ADR switch. R-R3-46 / R-R3-49 (parity Task
    // 13): a receive-only Core now takes the OC transmit pins off the air
    // (transmitSettingsVersion 8), so off the air the whole preset applies,
    // transmit pins included, in memory and saved, as the local switch does
    // (tst_remote_oc_cal covers the wait on the air).
    reloads.clear();
    s.core->ocMatrixMutable().setPin(Band::Band20m, 0, /*tx=*/true, true);
    const QString n2adr = hw(QStringLiteral("hl2IoBoard/n2adrFilter"));
    s.proxy->setValue(n2adr, QStringLiteral("True"));
    NEREUS_TRY_COMPARE(reloads, QStringList{QStringLiteral("n2adr")});
    QVERIFY(oc.pinEnabled(Band::Band40m, 2, /*tx=*/false));   // receive half applied
    QVERIFY(!oc.pinEnabled(Band::Band20m, 0, /*tx=*/true));   // cleared by the preset
    QVERIFY(oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));    // set by the preset
    QCOMPARE(settings.value(hw(QStringLiteral("oc/tx/40m/pin3")), QStringLiteral("False"))
                 .toString(), QStringLiteral("True"));
    s.proxy->setValue(n2adr, QStringLiteral("False"));
    NEREUS_TRY_VERIFY(!oc.pinEnabled(Band::Band40m, 2, /*tx=*/false));
    QVERIFY(!oc.pinEnabled(Band::Band40m, 2, /*tx=*/true));

    // The receive side is still the window's to change.
    s.proxy->setValue(hw(QStringLiteral("oc/rx/40m/pin3")), QStringLiteral("True"));
    NEREUS_TRY_VERIFY(oc.pinEnabled(Band::Band40m, 2, /*tx=*/false));
    s.proxy->setValue(hw(QStringLiteral("cal/freqFactor")), QStringLiteral("1.000002"));
    NEREUS_TRY_COMPARE(cal.freqCorrectionFactor(), 1.000002);
    QCOMPARE(rejected.count(), expected);
}

void TstStationSession::ioBoardProbeIsAskedOfTheCore()
{
    // R-R3-46. The window's Probe asks the Core (the radio is the Core's);
    // a Core whose radio has no I/O board to reach says so in plain words.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("probe.settings")));
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }

    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    const RadioModel::IoBoardProbeOutcome outcome = s.window->requestIoBoardProbe();
    QVERIFY(outcome.sent);
    NEREUS_TRY_COMPARE(toast.count(), 1);
    const QString reason = toast.last().at(0).toString();
    QCOMPARE(reason,
             QStringLiteral("The radio is not connected, so there is no I/O board to probe."));
    QVERIFY(OperatorWording::isPlain(reason));
}

void TstStationSession::windowBandAntennaEditKeepsTheCoresNewerBands()
{
    // R-R3-46 fix wave (radioHardwareVersion 3). A window's antenna click
    // used to send all 14 bands as the window last saw them, so a change
    // the Core made to another band in between (the VFO flag, another
    // window) was put back. The window now sends only the band clicked.
    AppSettings& coreStore = AppSettings::instance();
    coreStore.clearHardwareValues(kHardwareMac);
    const auto cleanStore = qScopeGuard([&coreStore] { coreStore.clearHardwareValues(kHardwareMac); });
    HardwareSession s;
    joinHardwareWindow(s, coreStore, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QCOMPARE(s.client->capabilities().radioHardwareVersion, 13);
    s.core->alexControllerMutable().setMacAddress(kHardwareMac);
    AlexAntennaFacade* window = s.window->alexAntennaFacade();
    QVERIFY(window->hasBandEditSender());
    QCOMPARE(window->rxAnt(Band::Band20m), 1);

    // The Core moves 20 m to Ant 3; before that reaches the window, the
    // window's operator picks Ant 2 on 40 m.
    s.core->alexControllerMutable().setRxAnt(Band::Band20m, 3);
    QCOMPARE(window->rxAnt(Band::Band20m), 1);
    window->setRxAnt(Band::Band40m, 2);
    NEREUS_TRY_COMPARE(s.core->alexController().rxAnt(Band::Band40m), 2);
    NereusSDR::Test::settleSession();
    QCOMPARE(s.core->alexController().rxAnt(Band::Band20m), 3);
    NEREUS_TRY_COMPARE(window->rxAnt(Band::Band20m), 3);
    NEREUS_TRY_COMPARE(window->rxAnt(Band::Band40m), 2);

    // The receive-only input the same way.
    s.core->alexControllerMutable().setRxOnlyAnt(Band::Band15m, 2);
    window->setRxOnlyAnt(Band::Band10m, 3);
    NEREUS_TRY_COMPARE(s.core->alexController().rxOnlyAnt(Band::Band10m), 3);
    NereusSDR::Test::settleSession();
    QCOMPARE(s.core->alexController().rxOnlyAnt(Band::Band15m), 2);

    // A value the Core cannot take is refused in plain words and the
    // window's view re-reads the Core's value.
    QSignalSpy resync(window, &AlexAntennaFacade::bandEditRefused);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    window->setRxAnt(Band::Band17m, 7);
    NEREUS_TRY_COMPARE(resync.count(), 1);
    NEREUS_TRY_COMPARE(toast.count(), 1);
    QVERIFY(OperatorWording::isPlain(toast.last().at(0).toString()));
    QCOMPARE(s.core->alexController().rxAnt(Band::Band17m), 1);
    QCOMPARE(window->rxAnt(Band::Band17m), 1);
}

void TstStationSession::windowTxBandAntennaEditKeepsTheCoresNewerBands()
{
    // Parity mini-round (radioHardwareVersion 6, setAlexTxAntenna). Task 12
    // sent a window's TX antenna click as all 14 bands as the window last
    // saw them, so a TX antenna the Core changed on another band in between
    // (another window, the Core's own Setup) was put back. The window now
    // sends only the band clicked, as setAlexRxAntenna does for receive.
    AppSettings& coreStore = AppSettings::instance();
    coreStore.clearHardwareValues(kHardwareMac);
    const auto cleanStore = qScopeGuard([&coreStore] { coreStore.clearHardwareValues(kHardwareMac); });
    HardwareSession s;
    joinHardwareWindow(s, coreStore, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QCOMPARE(s.client->capabilities().radioHardwareVersion, 13);
    QVERIFY(s.client->remoteTransmitAntennasAvailable());
    s.core->alexControllerMutable().setMacAddress(kHardwareMac);
    AlexAntennaFacade* window = s.window->alexAntennaFacade();
    QCOMPARE(window->txAnt(Band::Band20m), 1);

    // The Core moves 20 m's TX antenna to Ant 3; before that reaches the
    // window, the window's operator picks Ant 2 for 40 m.
    QVERIFY(window->hasTxBandEditSender());
    s.core->alexControllerMutable().setTxAnt(Band::Band20m, 3);
    QCOMPARE(window->txAnt(Band::Band20m), 1);
    window->setTxAnt(Band::Band40m, 2);
    NEREUS_TRY_COMPARE(s.core->alexController().txAnt(Band::Band40m), 2);
    NereusSDR::Test::settleSession();
    QCOMPARE(s.core->alexController().txAnt(Band::Band20m), 3);
    NEREUS_TRY_COMPARE(window->txAnt(Band::Band20m), 3);
    NEREUS_TRY_COMPARE(window->txAnt(Band::Band40m), 2);

    // A port blocked for transmit, and a value outside 1 to 3, are refused
    // in plain words; the window's view re-reads the Core's value.
    // (Block TX on Ant 2 moves 40 m back to Ant 1, as the local tab does.)
    s.core->alexControllerMutable().setBlockTxAnt2(true);
    NEREUS_TRY_VERIFY(window->blockTxAnt2());
    NEREUS_TRY_COMPARE(window->txAnt(Band::Band40m), 1);
    QSignalSpy resync(window, &AlexAntennaFacade::bandEditRefused);
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    window->setTxAnt(Band::Band17m, 2);
    NEREUS_TRY_COMPARE(resync.count(), 1);
    NEREUS_TRY_COMPARE(toast.count(), 1);
    QCOMPARE(toast.last().at(0).toString(),
             QStringLiteral("An antenna blocked for transmit cannot be a band's TX antenna."));
    QCOMPARE(s.core->alexController().txAnt(Band::Band17m), 1);
    QCOMPARE(window->txAnt(Band::Band17m), 1);
    window->setTxAnt(Band::Band15m, 7);
    NEREUS_TRY_COMPARE(resync.count(), 2);
    NEREUS_TRY_COMPARE(toast.count(), 2);
    QVERIFY(OperatorWording::isPlain(toast.last().at(0).toString()));
    QCOMPARE(s.core->alexController().txAnt(Band::Band15m), 1);
    QCOMPARE(s.core->alexController().txAnt(Band::Band20m), 3);
}

void TstStationSession::windowUsesBoundAntennaVerbAndCurrentMacWhenOffered()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("bound-antenna.settings")));
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path(), true);
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) {
        return;
    }
    QCOMPARE(s.client->capabilities().radioAntennaRowsVersion, 1);
    QCOMPARE(s.window->currentRadioMac(), kHardwareMac);

    s.stationEnd->clearReceived();
    s.window->alexAntennaFacade()->setRxAnt(Band::Band40m, 2);
    NEREUS_TRY_COMPARE(s.core->alexController().rxAnt(Band::Band40m), 2);
    // The antenna edit's invoke, either verb: the window's own connect
    // sequence (records.subscribe) can still be on the wire after the clear.
    QJsonObject invoke;
    for (const QByteArray& wire : s.stationEnd->received()) {
        const QJsonObject message = QJsonDocument::fromJson(wire).object();
        if (message.value(QStringLiteral("type")) == QStringLiteral("command.invoke")
            && message.value(QStringLiteral("verb")).toString().startsWith(
                QStringLiteral("setAlexRxAntenna"))) {
            invoke = message;
            break;
        }
    }
    QCOMPARE(invoke.value(QStringLiteral("verb")).toString(),
             QStringLiteral("setAlexRxAntennaForRadio"));
    bool foundMac = false;
    for (const QJsonValue& value : invoke.value(QStringLiteral("args")).toArray()) {
        const QJsonObject arg = value.toObject();
        if (arg.value(QStringLiteral("name")) == QStringLiteral("mac")) {
            QCOMPARE(arg.value(QStringLiteral("kind")).toString(), QStringLiteral("utf8"));
            QCOMPARE(arg.value(QStringLiteral("value")).toString(), kHardwareMac);
            foundMac = true;
        }
    }
    QVERIFY(foundMac);
    s.stationEnd->closeLink(QStringLiteral("test session ended"));
    NEREUS_TRY_COMPARE(s.client->capabilities().radioAntennaRowsVersion, 0);
}

void TstStationSession::windowFilterPolicyReachesTheCore()
{
    // R-R3-46 / R-R3-21 (radioHardwareVersion 4). The filter policy dialog
    // in a remote window shows the Core's policy; a change reaches the
    // Core, is applied there by its own controller as a local Apply is,
    // is saved for the Core's radio, and every window shows it.
    AppSettings& coreStore = AppSettings::instance();
    coreStore.clearHardwareValues(kHardwareMac);
    const auto cleanStore = qScopeGuard([&coreStore] { coreStore.clearHardwareValues(kHardwareMac); });
    HardwareSession s;
    joinHardwareWindow(s, coreStore, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QCOMPARE(s.client->capabilities().radioHardwareVersion, 13);
    QVERIFY(s.client->filterPolicyEditAvailable());
    QVERIFY(s.client->filterPolicyUnavailableReason().isEmpty());
    s.core->alexControllerMutable().setMacAddress(kHardwareMac);
    // A slice on 40 m on the Core's ADC0 sets the chain's band, so a forced
    // filter is that band's. (This Core's slice has no receiver behind it,
    // so the Core's own republish then reports the chain idle; its band
    // stays 40 m.)
    s.core->alexControllerMutable().notifySlicesOnAdc(
        0, {Band::Band40m, Band::Count, Band::Count, Band::Count, Band::Count});
    QCOMPARE(s.core->alexController().adcState(0).currentBpfBand, Band::Band40m);
    NEREUS_TRY_VERIFY(s.window->filterChainStateAvailable(0));
    NEREUS_TRY_COMPARE(s.window->filterChainState(0).currentBpfBand, Band::Band40m);
    QCOMPARE(s.window->filterChainState(0).mode, AlexController::BpfMode::Auto);

    const QString savedKey =
        QStringLiteral("hardware/%1/alex/antenna/Alex0_BpfMode").arg(kHardwareMac);
    {
        FilterPolicyDialog dialog(0, s.window.get());
        auto* group = dialog.findChild<QGroupBox*>(QStringLiteral("filterPolicyModeGroup"));
        auto* autoBtn = dialog.findChild<QRadioButton*>(QStringLiteral("filterPolicyAuto"));
        auto* force = dialog.findChild<QRadioButton*>(QStringLiteral("filterPolicyForceFilter"));
        auto* apply = dialog.findChild<QPushButton*>(QStringLiteral("filterPolicyApply"));
        auto* note = dialog.findChild<QLabel*>(QStringLiteral("filterPolicyNote"));
        QVERIFY(group && autoBtn && force && apply && note);
        QVERIFY(group->isEnabled());
        QVERIFY(autoBtn->isChecked());  // the Core's policy
        QVERIFY(OperatorWording::isPlain(note->text()));
        QVERIFY2(!note->text().contains(QStringLiteral("not available")), qPrintable(note->text()));
        force->setChecked(true);
        apply->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }
    // Applied by the Core's controller: the 40 m filter, forced.
    NEREUS_TRY_COMPARE(s.core->alexController().bpfMode(0), AlexController::BpfMode::ForceBand);
    QCOMPARE(s.core->alexController().adcState(0).effective,
             AlexController::BpfEffective::Filtered);
    QCOMPARE(s.core->alexController().adcState(0).currentBpfBand, Band::Band40m);
    QCOMPARE(s.core->alexController().adcState(0).reasonText, QStringLiteral("40m (forced)"));
    QCOMPARE(s.core->alexController().bpfMode(1), AlexController::BpfMode::Auto);
    // Saved on the Core for its radio, under the key the Core's controller
    // loads.
    NEREUS_TRY_COMPARE(coreStore.value(savedKey).toString(), QStringLiteral("1"));
    // Every window shows it.
    NEREUS_TRY_COMPARE(s.window->filterChainState(0).mode, AlexController::BpfMode::ForceBand);
    NEREUS_TRY_COMPARE(s.window->filterChainState(0).effective, AlexController::BpfEffective::Filtered);
    NEREUS_TRY_COMPARE(s.window->filterChainState(0).reasonText, QStringLiteral("40m (forced)"));
    {
        FilterPolicyDialog again(0, s.window.get());
        auto* force = again.findChild<QRadioButton*>(QStringLiteral("filterPolicyForceFilter"));
        QVERIFY(force && force->isChecked());
    }

    // A wideband chain stays bypassed, but its new policy still reaches the
    // Core, is saved and is shown. The window has seen the chain go wide
    // before the change, so the change's own publish is the only thing
    // that can bring the new policy (the chain's effective filter and
    // reason do not move, so AlexController's bpfStateChanged is quiet).
    s.core->alexControllerMutable().setWidebandActive(1, true);
    NEREUS_TRY_VERIFY(s.window->filterChainStateAvailable(1));
    NEREUS_TRY_COMPARE(s.window->filterChainState(1).effective,
                 AlexController::BpfEffective::WidebandLocked);
    QCOMPARE(s.window->filterChainState(1).mode, AlexController::BpfMode::Auto);
    const IStationLink::CommandOutcome sent =
        s.client->requestFilterPolicy(1, int(AlexController::BpfMode::ForceBand));
    QVERIFY(sent.sent);
    NEREUS_TRY_COMPARE(s.core->alexController().bpfMode(1), AlexController::BpfMode::ForceBand);
    NEREUS_TRY_COMPARE(coreStore.value(QStringLiteral("hardware/%1/alex/antenna/Alex1_BpfMode")
                                     .arg(kHardwareMac)).toString(),
                 QStringLiteral("1"));
    NEREUS_TRY_COMPARE(s.window->filterChainState(1).mode, AlexController::BpfMode::ForceBand);

    // A policy the Core does not have is refused in plain words and changes
    // nothing.
    QSignalSpy toast(s.window.get(), &RadioModel::sliceAddRejected);
    QVERIFY(s.client->requestFilterPolicy(0, 7).sent);
    NEREUS_TRY_COMPARE(toast.count(), 1);
    QVERIFY2(OperatorWording::isPlain(toast.last().at(0).toString()),
             qPrintable(toast.last().at(0).toString()));
    QCOMPARE(s.core->alexController().bpfMode(0), AlexController::BpfMode::ForceBand);
}

void TstStationSession::windowFilterPolicyApplySendsWhatIsShown()
{
    // R-R3-46 / R-R3-21. The dialog opens on the Core's Auto; before Apply,
    // the Core's policy is changed elsewhere (its own window, another
    // remote window). Apply with Auto still shown puts the Core back on
    // Auto: the dialog sends what it shows, not only what differs from
    // what it saw when it opened.
    AppSettings& coreStore = AppSettings::instance();
    coreStore.clearHardwareValues(kHardwareMac);
    const auto cleanStore = qScopeGuard([&coreStore] { coreStore.clearHardwareValues(kHardwareMac); });
    HardwareSession s;
    joinHardwareWindow(s, coreStore, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    s.core->alexControllerMutable().setMacAddress(kHardwareMac);
    NEREUS_TRY_VERIFY(s.window->filterChainStateAvailable(0));
    QCOMPARE(s.window->filterChainState(0).mode, AlexController::BpfMode::Auto);

    FilterPolicyDialog dialog(0, s.window.get());
    auto* autoBtn = dialog.findChild<QRadioButton*>(QStringLiteral("filterPolicyAuto"));
    auto* apply = dialog.findChild<QPushButton*>(QStringLiteral("filterPolicyApply"));
    QVERIFY(autoBtn && apply);
    QVERIFY(autoBtn->isChecked());

    s.core->alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);
    NEREUS_TRY_COMPARE(s.window->filterChainState(0).mode, AlexController::BpfMode::ForceBypass);

    apply->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    NEREUS_TRY_COMPARE(s.core->alexController().bpfMode(0), AlexController::BpfMode::Auto);
    NEREUS_TRY_COMPARE(s.window->filterChainState(0).mode, AlexController::BpfMode::Auto);
}

void TstStationSession::coreTakesTheFilterPolicyOnlyFromMinorElevenAtVersionFour()
{
    // R-R3-46 / R-R3-21. A raw setAlexBpfMode against a real Core: taken
    // from an app at minor 11 on a Core at radioHardwareVersion 4; refused
    // in plain words from an app at minor 10, and on a Core below version 4
    // (no attenuator controller behind it: version 0). A refusal changes
    // nothing on the Core.
    struct Outcome {
        bool accepted = false;
        QString reason;
        AlexController::BpfMode mode = AlexController::BpfMode::Auto;
    };
    const auto run = [this](quint16 minor, bool versionFour) {
        Outcome out;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("bpf.settings")));
        auto core = makeStationRadioModel(0);
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        if (versionFour) {
            core->setStepAttController(&controller);
        }
        const auto unbind = qScopeGuard([&core] { core->setStepAttController(nullptr); });
        StationServer server(core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        auto* station = new LoopbackTransport(QStringLiteral("bpf-core"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("bpf-peer"), this);
        station->linkTo(peer);
        server.acceptTransport(station);
        peer->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 6, QStringLiteral("bpf-app"))));
        peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
        [&] { NEREUS_TRY_VERIFY(peer->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))); }();
        peer->clearReceived();
        peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "setAlexBpfMode", 57,
            { MirrorUpdate{ 0, "chain", MirrorWireKind::Int64, QVariant(qlonglong(0)) },
              MirrorUpdate{ 0, "mode", MirrorWireKind::Int64, QVariant(qlonglong(2)) } })));
        SessionMessage result;
        [&] {
            NEREUS_TRY_VERIFY([&] {
                for (const QByteArray& wire : peer->received()) {
                    const SessionMessage candidate = decodeOrFail(wire);
                    if (candidate.kind == SessionMessageKind::CommandResult
                        && candidate.commandId == quint32(57)) {
                        result = candidate;
                        return true;
                    }
                }
                return false;
            }());
        }();
        out.accepted = result.accepted;
        out.reason = result.reason;
        out.mode = core->alexController().bpfMode(0);
        return out;
    };

    const Outcome taken = run(kRadioIdentitySessionProtocolMinor, true);
    QVERIFY2(taken.accepted, qPrintable(taken.reason));
    QCOMPARE(taken.mode, AlexController::BpfMode::ForceBypass);

    const Outcome olderApp = run(quint16(kRadioIdentitySessionProtocolMinor - 1), true);
    QVERIFY(!olderApp.accepted);
    QCOMPARE(olderApp.reason,
             QStringLiteral("Update this app to change the filter policy on this Core."));
    QVERIFY(OperatorWording::isPlain(olderApp.reason));
    QCOMPARE(olderApp.mode, AlexController::BpfMode::Auto);

    const Outcome olderCore = run(kRadioIdentitySessionProtocolMinor, false);
    QVERIFY(!olderCore.accepted);
    QCOMPARE(olderCore.reason, QStringLiteral("The Core has no filter settings ready."));
    QVERIFY(OperatorWording::isPlain(olderCore.reason));
    QCOMPARE(olderCore.mode, AlexController::BpfMode::Auto);
}

void TstStationSession::windowFilterPolicyWaitsForACoreThatOffersIt()
{
    // R-R3-46 / R-R3-21. Against a Core below radioHardwareVersion 4 the
    // remote dialog says in plain words that the Core needs updating and
    // sends nothing; from 4 its Apply is the setAlexBpfMode command.
    for (const int version : {2, 3, 4}) {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("bpf-station"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("bpf-client"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("test-token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
        station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        StationCapabilities caps;
        caps.propertyResultVersion = 1;
        caps.radioIdentityEntries = true;
        caps.radioHardwareVersion = version;
        station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        NEREUS_TRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(remote.stationLink(), static_cast<IStationLink*>(&client));
        QCOMPARE(client.filterPolicyEditAvailable(), version >= 4);
        const QString reason = client.filterPolicyUnavailableReason();
        QCOMPARE(reason.isEmpty(), version >= 4);

        // The Core's chain state as it reports it.
        AlexController::AlexAdcState coreState;
        coreState.mode = AlexController::BpfMode::Auto;
        coreState.reasonText = QStringLiteral("20m");
        FilterPolicyDialog dialog(0, &remote.alexControllerMutable(), nullptr, &coreState,
                                  true, remote.stationLink());
        auto* group = dialog.findChild<QGroupBox*>(QStringLiteral("filterPolicyModeGroup"));
        auto* bypass = dialog.findChild<QRadioButton*>(QStringLiteral("filterPolicyForceBypass"));
        auto* apply = dialog.findChild<QPushButton*>(QStringLiteral("filterPolicyApply"));
        auto* note = dialog.findChild<QLabel*>(QStringLiteral("filterPolicyNote"));
        QVERIFY(group && bypass && apply && note);
        QVERIFY(OperatorWording::isPlain(note->text()));
        station->clearReceived();
        if (version < 4) {
            QVERIFY(!group->isEnabled());
            QCOMPARE(note->text(), reason);
            QVERIFY2(reason.contains(QStringLiteral("Updating the Core")), qPrintable(reason));
            QVERIFY(OperatorWording::isPlain(reason));
            const IStationLink::CommandOutcome refused = client.requestFilterPolicy(0, 2);
            QVERIFY(!refused.sent);
            QCOMPARE(refused.reason, reason);
            apply->click();
            NereusSDR::Test::settleSession();
            QVERIFY(!station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
        } else {
            QVERIFY(group->isEnabled());
            bypass->setChecked(true);
            apply->click();
            NEREUS_TRY_VERIFY(station->receivedKinds().contains(QByteArrayLiteral("command.invoke")));
            QVERIFY(!station->receivedKinds().contains(QByteArrayLiteral("property.write")));
        }
        // Nothing changes in the window on the way out: the Core's answer
        // comes back as its published chain state.
        QCOMPARE(remote.alexController().bpfMode(0), AlexController::BpfMode::Auto);
    }
}

void TstStationSession::windowShowsTheCoresIoBoard()
{
    // R-R3-46 fix wave (radioHardwareVersion 3). The HL2 I/O board's
    // readings come back from the radio after a probe, over later frames,
    // on the Core. The Core mirrors its board (detected, hardware version,
    // registers) as `ioBoard`, and the window writes them into its own
    // board, which Setup's HL2 I/O board tab shows. Read only.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("ioboard.settings")));
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    QCOMPARE(s.client->capabilities().radioHardwareVersion, 13);
    const IoBoardHl2& windowBoard = s.window->ioBoard();
    QVERIFY(!windowBoard.isDetected());

    // The radio answers the probe on the Core.
    IoBoardHl2& coreBoard = s.core->ioBoardMutable();
    coreBoard.setRegisterValue(IoBoardHl2::Register::REG_FIRMWARE_MAJOR, 0x02);
    coreBoard.setRegisterValue(IoBoardHl2::Register::REG_FIRMWARE_MINOR, 0x07);
    coreBoard.setRegisterValue(IoBoardHl2::Register::REG_ANTENNA, 0x03);
    coreBoard.setHardwareVersion(IoBoardHl2::kHardwareVersion1);
    coreBoard.setDetected(true);

    NEREUS_TRY_VERIFY(windowBoard.isDetected());
    QCOMPARE(windowBoard.hardwareVersion(), IoBoardHl2::kHardwareVersion1);
    QCOMPARE(windowBoard.registerValue(IoBoardHl2::Register::REG_FIRMWARE_MAJOR), quint8(0x02));
    QCOMPARE(windowBoard.registerValue(IoBoardHl2::Register::REG_FIRMWARE_MINOR), quint8(0x07));
    QCOMPARE(windowBoard.registerValue(IoBoardHl2::Register::REG_ANTENNA), quint8(0x03));

    // A later register reading follows.
    coreBoard.setRegisterValue(IoBoardHl2::Register::REG_FAULT, 0x01);
    NEREUS_TRY_COMPARE(windowBoard.registerValue(IoBoardHl2::Register::REG_FAULT), quint8(0x01));

    // The window never writes the Core's board: every property is the
    // Core's to report, and a write is refused in plain words.
    for (const char* name : {"detected", "hardwareVersion", "registers"}) {
        QVERIFY2(!MirrorPolicy::inboundAllowed(QByteArrayLiteral("IoBoardHl2Facade"),
                                               QByteArray(name)), name);
    }
    QVERIFY(OperatorWording::isPlain(IoBoardHl2Facade::readOnlyReason()));
}

void TstStationSession::windowForgetsTheIoBoardOfACoreThatDoesNotOfferIt()
{
    // R-R3-46 follow-up item 5. A window that showed one Core's I/O board
    // and then joins a Core that does not offer `ioBoard` (below
    // radioHardwareVersion 3) no longer shows the first Core's board.
    RadioModel remote(RadioModel::Role::Remote);
    IoBoardHl2Facade* facade = remote.ioBoardFacade();
    QVERIFY(facade->applyRemoteProperty("hardwareVersion", int(IoBoardHl2::kHardwareVersion1)));
    QString registers(IoBoardHl2Facade::kRegisterCount * 2, QLatin1Char('0'));
    registers.replace(int(IoBoardHl2::Register::REG_FIRMWARE_MAJOR) * 2, 2, QStringLiteral("02"));
    QVERIFY(facade->applyRemoteProperty("registers", registers));
    QVERIFY(facade->applyRemoteProperty("detected", true));
    QVERIFY(remote.ioBoard().isDetected());
    QCOMPARE(remote.ioBoard().registerValue(IoBoardHl2::Register::REG_FIRMWARE_MAJOR), quint8(0x02));

    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    auto* station = new LoopbackTransport(QStringLiteral("io-station"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("io-client"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("test-token"));
    station->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("station"))));
    station->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
    StationCapabilities caps;
    caps.propertyResultVersion = 1;
    caps.radioIdentityEntries = true;
    caps.radioHardwareVersion = 2;
    station->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
    station->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
    NEREUS_TRY_VERIFY(client.isHandshakeComplete());

    QVERIFY(!remote.ioBoard().isDetected());
    QCOMPARE(remote.ioBoard().hardwareVersion(), quint8(0));
    QCOMPARE(remote.ioBoard().registerValue(IoBoardHl2::Register::REG_FIRMWARE_MAJOR), quint8(0));
    QVERIFY(!facade->detected());
}

void TstStationSession::windowOcMatrixFollowsTheCore()
{
    // R-R3-46 fix wave. The window keeps a copy of the Core's OC pin
    // matrix; OcMatrix::save writes every cell that differs from the store.
    // When the Core changed a pin (another window, the N2ADR preset) the
    // window's copy stayed old, and its next pin click sent the old cell
    // back. The window now reloads its copy when the Core's OC keys arrive.
    AppSettings& coreStore = AppSettings::instance();
    coreStore.clearHardwareValues(kHardwareMac);
    const auto cleanStore = qScopeGuard([&coreStore] { coreStore.clearHardwareValues(kHardwareMac); });
    HardwareSession s;
    joinHardwareWindow(s, coreStore, this, m_securityDir.path());
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    OcMatrix& windowOc = s.window->ocMatrixMutable();
    QVERIFY(!windowOc.pinEnabled(Band::Band40m, 2, /*tx=*/false));

    // The Core sets 40 m pin 3 (as another window's click would).
    const QString pin = QStringLiteral("hardware/%1/oc/rx/40m/pin3").arg(kHardwareMac);
    coreStore.setValue(pin, QStringLiteral("True"));
    NEREUS_TRY_VERIFY(windowOc.pinEnabled(Band::Band40m, 2, /*tx=*/false));
    QCOMPARE(s.proxy->value(pin, QString()).toString(), QStringLiteral("True"));
}

void TstStationSession::hardwareConfigRateGoesToEveryReceiver()
{
    // R-R3-46 and parity ruling C4. In a remote window, Hardware Config >
    // Radio Info's sample rate is the local window's change: one request
    // for the whole radio (setRadioSampleRate, radioHardwareVersion 9),
    // every receiver and the radio's own rate, without a reconnect; and it
    // is saved on the Core as that radio's default for its next connect.
    // (An older Core gets each receiver's own request instead:
    // tst_remote_slice_commands.)
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("rate.settings")));
    HardwareSession s;
    joinHardwareWindow(s, settings, this, m_securityDir.path(), /*alexBoard=*/false,
                       /*extraSlices=*/2);
    const auto cleanup = qScopeGuard([&s] { leaveHardwareSession(s); });
    if (QTest::currentTestFailed()) { return; }
    NEREUS_TRY_COMPARE(s.window->slices().size(), 3);
    NEREUS_TRY_COMPARE(s.window->currentRadioInfo().macAddress, kHardwareMac);
    QCOMPARE(s.client->capabilities().radioHardwareVersion, 13);
    QVERIFY(s.window->radioSampleRateReachesEveryReceiver());
    s.window->alexAntennaFacade()->setWindowAvailability(true, {});

    RemoteSettingsScope scope(s.proxy.get());
    HardwarePage page(s.window.get());
    QComboBox* rate = nullptr;
    for (QComboBox* combo : page.tabWidgetForTest(HardwarePage::Tab::RadioInfo)
                                ->findChildren<QComboBox*>()) {
        if (combo->isEnabled() && combo->count() > 1 && combo->itemData(0).toInt() > 0) {
            rate = combo;
            break;
        }
    }
    QVERIFY(rate != nullptr);
    const int target = rate->currentIndex() == 0 ? 1 : 0;
    const int hz = rate->itemData(target).toInt();
    s.stationEnd->clearReceived();
    rate->setCurrentIndex(target);

    const QString key = QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(kHardwareMac);
    NEREUS_TRY_COMPARE(settings.value(key).toInt(), hz);
    QList<int> radioWide;
    int perReceiver = 0;
    const auto collect = [&] {
        radioWide.clear();
        perReceiver = 0;
        for (const QByteArray& wire : s.stationEnd->received()) {
            const SessionMessage m = decodeOrFail(wire);
            if (m.kind != SessionMessageKind::CommandInvoke) {
                continue;
            }
            if (m.commandVerb == "requestSliceSampleRate") {
                ++perReceiver;
            } else if (m.commandVerb == "setRadioSampleRate") {
                for (const MirrorUpdate& argument : m.arguments) {
                    if (argument.name == "rateHz") { radioWide.append(argument.value.toInt()); }
                }
            }
        }
        return !radioWide.isEmpty();
    };
    NEREUS_TRY_VERIFY(collect());
    NereusSDR::Test::settleSession();
    collect();
    QCOMPARE(radioWide, QList<int>{hz});
    QCOMPARE(perReceiver, 0);
}

// ── iPhone app Task 18 (R-IOS-08, R-IOS-17) ─────────────────────────────

namespace {

// A Core's hello as the Core sends it (the link document, section 3.4):
// `coreKey`'s identity, bound by `bindingKey` to `certSha256`, and a
// challenge. `withIdentity` false is a Core from before identities.
SessionMessage scriptedCoreHello(const StationIdentity& coreKey, const StationIdentity& bindingKey,
                                 const QByteArray& certSha256, bool withIdentity = true)
{
    QHash<QByteArray, int> features;
    if (withIdentity) {
        features.insert("deviceAuth", 1);
        features.insert("pairing", 1);
    }
    SessionMessage hello = SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
                                                  QStringLiteral("scripted core"),
                                                  {kSessionProtocolMajor}, features);
    if (withIdentity) {
        hello.stationIdentity = SessionStationIdentity{
            StationIdentity::toBase64Url(coreKey.publicKeySpki()),
            StationIdentity::toBase64Url(bindingKey.certBinding(certSha256))};
        hello.challenge = StationIdentity::toBase64Url(QByteArray(32, '\x07'));
    }
    return hello;
}

QByteArray randomSha()
{
    QByteArray bytes(32, '\0');
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return bytes;
}

QString pinOf(const QByteArray& sha)
{
    QStringList pairs;
    for (const char byte : sha) {
        pairs.append(QString::number(static_cast<quint8>(byte), 16).rightJustified(2, QLatin1Char('0'))
                         .toUpper());
    }
    return pairs.join(QLatin1Char(':'));
}

QList<QJsonObject> sentOfType(const LoopbackTransport* station, const QString& type)
{
    QList<QJsonObject> out;
    for (const QByteArray& wire : station->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == type) {
            out.append(o);
        }
    }
    return out;
}

// A window, its key, and a scripted Core end it can be linked to.
struct KeyedWindow {
    QTemporaryDir keyDir;
    std::shared_ptr<const ClientDeviceIdentity> key = std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(keyDir.path()));
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&remote, &proxy};

    KeyedWindow() { client.setDeviceIdentity(key, QStringLiteral("Shack MacBook")); }

    // Links a scripted Core end; `certificate` is what this window's side
    // reports the Core presented.
    LoopbackTransport* link(QObject* owner, const QByteArray& certificate, const QString& token,
                            const QString& pin, const QByteArray& identity)
    {
        auto* station = new LoopbackTransport(QStringLiteral("scripted core"), owner);
        auto* peer = new LoopbackTransport(QStringLiteral("window"), owner);
        peer->setPeerCertificateSha256(certificate);
        station->linkTo(peer);
        client.startSession(peer, token, pin, identity);
        return station;
    }
};

void verifyIdentityRefusal(KeyedWindow& window, LoopbackTransport* station)
{
    NEREUS_TRY_VERIFY(!window.client.isConnectionActive());
    // Nothing went to that Core: not this window's hello, not a sign-in.
    QVERIFY(sentOfType(station, QStringLiteral("hello")).isEmpty());
    QVERIFY(sentOfType(station, QStringLiteral("auth.request")).isEmpty());
    const StationEndReport report = window.client.lastEndReport();
    QCOMPARE(report.kind, StationEndReport::Kind::IdentityChanged);
    QCOMPARE(report.code, QString::fromLatin1(SessionEndCode::kIdentityChanged));
    QVERIFY2(OperatorWording::isPlain(report.reason), qPrintable(report.reason));
    QVERIFY2(OperatorWording::coreCalledStationIn(report.reason).isEmpty(),
             qPrintable(report.reason));
    QVERIFY(report.reason.contains(QLatin1String("Core")));
    QVERIFY(!window.client.isReconnectPending());
}

} // namespace

// A saved Core whose hello shows another identity key: refused with this
// app's own identityChanged, before this window says anything, and never
// trusted silently.
void TstStationSession::pairedCoreShowingAnotherIdentityIsRefused()
{
    QTemporaryDir paired;
    QTemporaryDir impostor;
    const StationIdentity pairedKey = StationIdentity::loadOrCreate(paired.path());
    const StationIdentity otherKey = StationIdentity::loadOrCreate(impostor.path());
    const QByteArray certificate = randomSha();
    KeyedWindow window;
    LoopbackTransport* station =
        window.link(this, certificate, QString(), QString(), pairedKey.fingerprint());
    // The impostor's own identity, validly bound to the certificate.
    station->sendText(SessionMessages::encode(scriptedCoreHello(otherKey, otherKey, certificate)));
    verifyIdentityRefusal(window, station);
    QVERIFY(window.client.lastEndReport().reason.startsWith(
        QLatin1String("The Core at this address is not the Core this computer paired with")));
}

void TstStationSession::pairedCoreShowingNoIdentityIsRefused()
{
    QTemporaryDir paired;
    const StationIdentity pairedKey = StationIdentity::loadOrCreate(paired.path());
    const QByteArray certificate = randomSha();
    KeyedWindow window;
    LoopbackTransport* station =
        window.link(this, certificate, QStringLiteral("a token"), QString(), pairedKey.fingerprint());
    station->sendText(SessionMessages::encode(
        scriptedCoreHello(pairedKey, pairedKey, certificate, /*withIdentity=*/false)));
    verifyIdentityRefusal(window, station);
}

// The right key, but its binding is for another certificate than the one
// this connection presented: refused with plain words.
void TstStationSession::certificateWithoutAValidBindingIsRefused()
{
    QTemporaryDir paired;
    const StationIdentity pairedKey = StationIdentity::loadOrCreate(paired.path());
    const QByteArray presented = randomSha();
    KeyedWindow window;
    LoopbackTransport* station =
        window.link(this, presented, QString(), QString(), pairedKey.fingerprint());
    station->sendText(
        SessionMessages::encode(scriptedCoreHello(pairedKey, pairedKey, randomSha())));
    verifyIdentityRefusal(window, station);
    QCOMPARE(window.client.lastEndReport().reason,
             QStringLiteral("The Core's certificate is not signed by the Core this computer "
                            "paired with, so this computer did not connect."));
}

// After pairing, a new certificate the Core's key binds is accepted with no
// question asked, whatever pin was saved, and this window signs in by key.
void TstStationSession::changedCertificateWhoseBindingVerifiesIsAccepted()
{
    QTemporaryDir paired;
    const StationIdentity pairedKey = StationIdentity::loadOrCreate(paired.path());
    const QByteArray newCertificate = randomSha();
    KeyedWindow window;
    LoopbackTransport* station = window.link(this, newCertificate, QStringLiteral("old token"),
                                             pinOf(randomSha()), pairedKey.fingerprint());
    const SessionMessage hello = scriptedCoreHello(pairedKey, pairedKey, newCertificate);
    station->sendText(SessionMessages::encode(hello));
    NEREUS_TRY_COMPARE(sentOfType(station, QStringLiteral("auth.request")).size(), 1);
    QCOMPARE(window.client.lastEndReport().kind, StationEndReport::Kind::None);

    const QJsonObject windowHello = sentOfType(station, QStringLiteral("hello")).first();
    QCOMPARE(windowHello.value(QStringLiteral("features")).toObject()
                 .value(QStringLiteral("deviceAuth")).toInt(), 1);
    const QJsonObject auth = sentOfType(station, QStringLiteral("auth.request")).first();
    // The token is never sent to a paired Core.
    QCOMPARE(auth.value(QStringLiteral("token")).toString(), QString());
    const QJsonObject device = auth.value(QStringLiteral("device")).toObject();
    QCOMPARE(device.value(QStringLiteral("kind")).toString(), QStringLiteral("computer"));
    QCOMPARE(device.value(QStringLiteral("name")).toString(), QStringLiteral("Shack MacBook"));
    QCOMPARE(device.value(QStringLiteral("publicKey")).toString(),
             StationIdentity::toBase64Url(window.key->publicKeySpki()));
    QCOMPARE(device.value(QStringLiteral("id")).toString(),
             StationIdentity::toBase64Url(window.key->fingerprint()));
    // Signed over this connection's challenge, certificate and Core key.
    QVERIFY(StationIdentity::verify(
        window.key->publicKeySpki(),
        DeviceAuthenticator::transcript(StationIdentity::fromBase64Url(hello.challenge),
                                        newCertificate, pairedKey.publicKeySpki(),
                                        window.key->publicKeySpki()),
        StationIdentity::fromBase64Url(device.value(QStringLiteral("signature")).toString())));
    window.client.disconnectFromStation(QStringLiteral("test done"));
}

// With no key this window says nothing it cannot do; with one it declares
// deviceAuth 1.
void TstStationSession::helloDeclaresDeviceAuthOnlyWithAKey()
{
    QTemporaryDir coreDir;
    const StationIdentity coreKey = StationIdentity::loadOrCreate(coreDir.path());
    {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("scripted core"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("window"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("token"));
        station->sendText(SessionMessages::encode(scriptedCoreHello(coreKey, coreKey, randomSha())));
        NEREUS_TRY_COMPARE(sentOfType(station, QStringLiteral("hello")).size(), 1);
        QVERIFY(!sentOfType(station, QStringLiteral("hello")).first()
                     .value(QStringLiteral("features")).toObject()
                     .contains(QStringLiteral("deviceAuth")));
        client.disconnectFromStation(QStringLiteral("test done"));
    }
    KeyedWindow window;
    LoopbackTransport* station =
        window.link(this, QByteArray(), QStringLiteral("token"), QString(), QByteArray());
    station->sendText(SessionMessages::encode(scriptedCoreHello(coreKey, coreKey, randomSha())));
    NEREUS_TRY_COMPARE(sentOfType(station, QStringLiteral("hello")).size(), 1);
    QCOMPARE(sentOfType(station, QStringLiteral("hello")).first()
                 .value(QStringLiteral("features")).toObject()
                 .value(QStringLiteral("deviceAuth")).toInt(), 1);
    window.client.disconnectFromStation(QStringLiteral("test done"));
}

// A Core with no identity (an older one), or a bench link whose pin was
// never checked, gets the token alone: nothing is enrolled.
void TstStationSession::coreWithNoIdentityGetsTheTokenAlone()
{
    QTemporaryDir coreDir;
    const StationIdentity coreKey = StationIdentity::loadOrCreate(coreDir.path());
    const QByteArray certificate = randomSha();
    {
        KeyedWindow window;
        LoopbackTransport* station =
            window.link(this, certificate, QStringLiteral("token"), pinOf(certificate), {});
        station->sendText(SessionMessages::encode(
            scriptedCoreHello(coreKey, coreKey, certificate, /*withIdentity=*/false)));
        NEREUS_TRY_COMPARE(sentOfType(station, QStringLiteral("auth.request")).size(), 1);
        const QJsonObject auth = sentOfType(station, QStringLiteral("auth.request")).first();
        QCOMPARE(auth.value(QStringLiteral("token")).toString(), QStringLiteral("token"));
        QVERIFY(!auth.contains(QStringLiteral("device")));
        window.client.disconnectFromStation(QStringLiteral("test done"));
    }
    {
        // No pin (a bench link): the identity it shows is not learned.
        KeyedWindow window;
        LoopbackTransport* station =
            window.link(this, certificate, QStringLiteral("token"), QString(), {});
        station->sendText(SessionMessages::encode(scriptedCoreHello(coreKey, coreKey, certificate)));
        NEREUS_TRY_COMPARE(sentOfType(station, QStringLiteral("auth.request")).size(), 1);
        QVERIFY(!sentOfType(station, QStringLiteral("auth.request")).first()
                     .contains(QStringLiteral("device")));
        window.client.disconnectFromStation(QStringLiteral("test done"));
    }
}

// An existing saved Core (token and pin) enrols this computer's key on its
// next token sign-in, nothing typed; afterwards it signs in by key alone.
void TstStationSession::tokenSignInEnrolsTheKeyThenSignsInByKey()
{
    QTemporaryDir dir;
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("enrol.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    server.setHeartbeatIntervalMs(0);
    QString pin = server.certificateFingerprint();
    const QByteArray certificate = QByteArray::fromHex(pin.remove(QLatin1Char(':')).toLatin1());

    KeyedWindow window;
    QSignalSpy learned(&window.client, &StationClient::stationIdentityLearned);
    auto* station = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("window"), this);
    station->setPeerAddress(QStringLiteral("127.0.0.1"));
    peer->setPeerCertificateSha256(certificate);
    station->linkTo(peer);
    window.client.startSession(peer, server.token(), server.certificateFingerprint());
    server.acceptTransport(station);
    NEREUS_TRY_VERIFY(window.client.isHandshakeComplete());
    QCOMPARE(learned.size(), 1);
    QCOMPARE(learned.first().first().toByteArray(), server.stationIdentity().fingerprint());
    QCOMPARE(window.client.stationIdentityFingerprint(), server.stationIdentity().fingerprint());
    const auto enrolled = server.deviceStore()->find(window.key->fingerprint());
    QVERIFY(enrolled.has_value());
    QCOMPARE(enrolled->kind, QStringLiteral("computer"));
    QCOMPARE(enrolled->name, QStringLiteral("Shack MacBook"));
    QVERIFY(enrolled->enrolledThroughToken);
    const QJsonObject firstAuth = sentOfType(station, QStringLiteral("auth.request")).first();
    QCOMPARE(firstAuth.value(QStringLiteral("token")).toString(), server.token());
    QVERIFY(firstAuth.contains(QStringLiteral("device")));
    window.client.disconnectFromStation(QStringLiteral("test done"));
    NEREUS_TRY_VERIFY(!server.hasAuthenticatedSession());

    // The next connection, by key alone: no token leaves this window.
    auto* station2 = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer2 = new LoopbackTransport(QStringLiteral("window"), this);
    station2->setPeerAddress(QStringLiteral("127.0.0.1"));
    peer2->setPeerCertificateSha256(certificate);
    station2->linkTo(peer2);
    window.client.startSession(peer2, server.token(), QString(),
                               window.client.stationIdentityFingerprint());
    server.acceptTransport(station2);
    NEREUS_TRY_VERIFY(window.client.isHandshakeComplete());
    const QJsonObject secondAuth = sentOfType(station2, QStringLiteral("auth.request")).first();
    QCOMPARE(secondAuth.value(QStringLiteral("token")).toString(), QString());
    QVERIFY(secondAuth.contains(QStringLiteral("device")));
    QCOMPARE(learned.size(), 1);
    window.client.disconnectFromStation(QStringLiteral("test done"));
}

// The kind comes from the code where the Core sends one, and from the
// words only where it sends none (an older Core).
void TstStationSession::endCodesChooseTheReport()
{
    const struct {
        const char* reason;
        const char* code;
        StationEndReport::Kind kind;
    } cases[] = {
        {"This device was removed from the Core.", "deviceRemoved",
         StationEndReport::Kind::DeviceRemoved},
        {"Anything at all.", "deviceRemoved", StationEndReport::Kind::DeviceRemoved},
        {"This device is not paired with this Core. Pair it first.", "deviceNotPaired",
         StationEndReport::Kind::DeviceRemoved},
        {"This Core uses paired devices. Pair this device first.", "pairingRequired",
         StationEndReport::Kind::PairingRequired},
        {"Another app at 192.0.2.9:5000 connected to the Core and took over. "
         "Connect again to take it back.", "takenOver", StationEndReport::Kind::TakenOver},
        {"Some other words.", "takenOver", StationEndReport::Kind::TakenOver},
        {"The Core could not read a message from this app.", "protocolError",
         StationEndReport::Kind::Refused},
        // No code: an older Core, read by its words.
        {"Another app at 192.0.2.9:5000 connected to the Core and took over. "
         "Connect again to take it back.", "", StationEndReport::Kind::TakenOver},
        {"Displaced by a newer authenticated connection from 192.0.2.9:5000", "",
         StationEndReport::Kind::TakenOver},
        {"This device was removed from the Core.", "", StationEndReport::Kind::Refused},
    };
    for (const auto& entry : cases) {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* station = new LoopbackTransport(QStringLiteral("scripted core"), this);
        auto* peer = new LoopbackTransport(QStringLiteral("window"), this);
        station->linkTo(peer);
        client.startSession(peer, QStringLiteral("token"));
        station->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("scripted core"))));
        NEREUS_TRY_COMPARE(sentOfType(station, QStringLiteral("auth.request")).size(), 1);
        station->sendText(SessionMessages::encode(SessionMessages::sessionEnd(
            QString::fromLatin1(entry.reason), false, QString::fromLatin1(entry.code))));
        NEREUS_TRY_VERIFY(!client.isConnectionActive());
        const StationEndReport report = client.lastEndReport();
        QVERIFY2(report.kind == entry.kind, entry.reason);
        QCOMPARE(report.code, QString::fromLatin1(entry.code));
        if (report.kind == StationEndReport::Kind::TakenOver
            && QString::fromLatin1(entry.reason).contains(QLatin1String("192.0.2.9"))) {
            QCOMPARE(report.takenOverBy, QStringLiteral("192.0.2.9"));
        }
    }
}

// A device revoked while it is connected is ended with deviceRemoved, and
// the report says so by the code.
void TstStationSession::revokedDeviceIsEndedWithDeviceRemoved()
{
    QTemporaryDir dir;
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("revoke.settings")));
    auto model = makeStationRadioModel(0);
    StationServer server(model.get(), settings, NereusSDR::Test::seedCoreIdentity(dir.path()));
    server.setHeartbeatIntervalMs(0);
    QString pin = server.certificateFingerprint();
    const QByteArray certificate = QByteArray::fromHex(pin.remove(QLatin1Char(':')).toLatin1());

    KeyedWindow window;
    PairedDevice device;
    device.id = window.key->fingerprint();
    device.publicKeySpki = window.key->publicKeySpki();
    device.name = QStringLiteral("Shack MacBook");
    device.kind = QStringLiteral("computer");
    QVERIFY(server.deviceStore()->add(device));

    auto* station = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("window"), this);
    station->setPeerAddress(QStringLiteral("127.0.0.1"));
    peer->setPeerCertificateSha256(certificate);
    station->linkTo(peer);
    window.client.startSession(peer, QString(), QString(), server.stationIdentity().fingerprint());
    server.acceptTransport(station);
    NEREUS_TRY_VERIFY(window.client.isHandshakeComplete());

    QVERIFY(server.deviceStore()->remove(device.id));
    NEREUS_TRY_VERIFY(!window.client.isConnectionActive());
    QCOMPARE(window.client.lastEndReport().kind, StationEndReport::Kind::DeviceRemoved);
    QCOMPARE(window.client.lastEndReport().code,
             QString::fromLatin1(SessionEndCode::kDeviceRemoved));
    QVERIFY(!window.client.isReconnectPending());

    // Signing in again is refused as not paired, the same notice.
    auto* station2 = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer2 = new LoopbackTransport(QStringLiteral("window"), this);
    station2->setPeerAddress(QStringLiteral("127.0.0.1"));
    peer2->setPeerCertificateSha256(certificate);
    station2->linkTo(peer2);
    window.client.startSession(peer2, QString(), QString(), server.stationIdentity().fingerprint());
    server.acceptTransport(station2);
    NEREUS_TRY_VERIFY(!window.client.isConnectionActive());
    QCOMPARE(window.client.lastEndReport().kind, StationEndReport::Kind::DeviceRemoved);
    QCOMPARE(window.client.lastEndReport().code,
             QString::fromLatin1(SessionEndCode::kDeviceNotPaired));
}

// A saved Core from before paired devices whose token has been retired:
// the sign-in is refused with pairingRequired and the report says so.
void TstStationSession::retiredTokenIsRefusedWithPairingRequired()
{
    QTemporaryDir dir;
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("retired.settings")));
    auto model = makeStationRadioModel(0);
    // A Core with an identity and no token (new, or its token retired).
    StationServer server(model.get(), settings, NereusSDR::Test::seedCoreIdentity(dir.path()));
    server.setHeartbeatIntervalMs(0);
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    auto* station = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("window"), this);
    station->linkTo(peer);
    client.startSession(peer, QStringLiteral("the saved token"));
    server.acceptTransport(station);
    NEREUS_TRY_VERIFY(!client.isConnectionActive());
    QCOMPARE(client.lastEndReport().kind, StationEndReport::Kind::PairingRequired);
    QCOMPARE(client.lastEndReport().code, QString::fromLatin1(SessionEndCode::kPairingRequired));
    QVERIFY(!client.isReconnectPending());
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstStationSession test;
    QTEST_SET_MAIN_SOURCE_PATH
    // Load finding: about 19 s on a quiet machine, past ctest's 120 s under
    // a loaded full run (the limit is not raised). The cases for the Core's
    // noise reduction, notches, attenuator, antennas and hardware settings
    // run as their own ctest entry, tst_station_session_controls
    // (tests/CMakeLists.txt); the rest as tst_station_session.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_STATION_SESSION_GROUP",
        {{QStringLiteral("controls"),
          {QStringLiteral("nr3ModelChoiceLoadsOnceOnTheCoreAndMirrors"),
           QStringLiteral("nr3CannotRunIsRefusedOnTheCoreAndInTheWindow"),
           QStringLiteral("dfnrCannotRunIsRefusedOnTheCoreAndInTheWindow"),
           QStringLiteral("mnrCannotRunIsRefusedOnTheCoreAndInTheWindow"),
           QStringLiteral("bnrIsRefusedOnTheCoreAndInTheWindow"),
           QStringLiteral("savedNr3OnACoreWithNoModelShowsOffInTheWindow"),
           QStringLiteral("nr3CannotRunEndsWithTheSession"),
           QStringLiteral("olderCoreLeavesTheNr3ModelUnchangeable"),
           QStringLiteral("olderAppNr3ModelPathWriteIsRefused"),
           QStringLiteral("remoteNotchEditKeepsTheCoresWholeList"),
           QStringLiteral("remoteNotchMoveToggleAndDeleteReachTheCore"),
           QStringLiteral("remoteNotchRefusalsAreInPlainWords"),
           QStringLiteral("coreNotchChangesReachTheWindow"),
           QStringLiteral("appNotchSettingsWritesAreRefused"),
           QStringLiteral("olderCoreKeepsTodaysNotchBehaviour"),
           QStringLiteral("olderAppIgnoresTheNotchesObjectGolden"),
           QStringLiteral("radioIdentityEntriesRoundTrip"),
           QStringLiteral("coreSendsRadioIdentityOnlyFromMinorEleven"),
           QStringLiteral("remoteModelResolvesTheCoresRadio"),
           QStringLiteral("remoteModelSignalsOncePerIdentityChange"),
           QStringLiteral("coreOffersTheAttenuatorOnlyFromMinorEleven"),
           QStringLiteral("appStepAttenuatorSettingsWritesAreRefused"),
           QStringLiteral("windowAttenuatorEditsWaitForACoreThatOffersThem"),
           QStringLiteral("windowAntennaEditsReachTheCoresController"),
           QStringLiteral("windowAntennaEditsWaitForACoreThatOffersThem"),
           QStringLiteral("appRawAntennaSettingsWritesAreRefused"),
           QStringLiteral("hardwareWritesForAnotherRadioAreRefused"),
           QStringLiteral("coreAppliesHardwareConfigWritesLive"),
           QStringLiteral("receiveOnlyCoreRefusesTransmitHardwareKeys"),
           QStringLiteral("ioBoardProbeIsAskedOfTheCore"),
           QStringLiteral("windowBandAntennaEditKeepsTheCoresNewerBands"),
           QStringLiteral("windowTxBandAntennaEditKeepsTheCoresNewerBands"),
           QStringLiteral("windowUsesBoundAntennaVerbAndCurrentMacWhenOffered"),
           QStringLiteral("windowShowsTheCoresIoBoard"),
           QStringLiteral("windowForgetsTheIoBoardOfACoreThatDoesNotOfferIt"),
           QStringLiteral("windowOcMatrixFollowsTheCore"),
           QStringLiteral("hardwareConfigRateGoesToEveryReceiver"),
           QStringLiteral("windowFilterPolicyReachesTheCore"),
           QStringLiteral("windowFilterPolicyWaitsForACoreThatOffersIt"),
           QStringLiteral("windowFilterPolicyApplySendsWhatIsShown"),
           QStringLiteral("coreTakesTheFilterPolicyOnlyFromMinorElevenAtVersionFour")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_station_session.moc"
