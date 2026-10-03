// =================================================================
// tests/tst_session_link_loss.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 19: link loss, daemon restart, and reconnect.
//
// Task 18 built the session; nothing before this task defined what the
// client does with a mirror of objects that no longer exist. Parent design
// section 13 ("Error handling and reconnect"): "The client retains
// last-known state, indicates staleness rather than showing stale values
// as live, and reconnects with exponential backoff."
//
// ── HOW THIS FILE IS SPLIT, AND WHY ──────────────────────────────────────
//
// The link-loss CONTRACT (mirror teardown, the defined stale state, the
// settings-proxy cache-reads/drops-writes behaviour, the session epoch,
// and reconnect CONVERGENCE) is proven over an in-process, non-TLS
// SessionTransport (fakes/LoopbackTransport.h), matching
// tst_station_session.cpp's own split, and therefore runs on every build.
//
// The automatic-reconnect TIMER MECHANISM itself (owned, cancellable,
// exponential backoff, latched host/port) is new in this task and can only
// be exercised through connectToStation(), which always creates a real
// QWebSocket -- so those slots QSKIP when QSslSocket::supportsSsl() is
// false, naming the active Qt TLS backend, exactly like
// tst_station_session.cpp's TLS-specific slots.
//
// ── THE ONE THAT MATTERS MOST ────────────────────────────────────────────
//
// silentlyDeadPeerIsDetectedNotJustACleanClose. Killing the daemon (the
// first thing anyone does on a bench) produces a clean TCP close, and the
// close does all the work -- that is the easy case, covered by
// killedDaemonEntersDefinedStaleStateThenReconnectsToARestartedDaemonWith-
// DifferentState below. This slot covers the case that motivated pulling
// the heartbeat into R2 at all: a peer that stops responding WITHOUT
// closing, which is what a laptop lid, a cell handoff, or a NAT timeout
// actually produces. A test that only kills the process proves nothing
// about this path, because the close does the work.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 19: link loss,
//                                    daemon restart and reconnect. AI-
//                                    assisted transformation via Anthropic
//                                    Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 4
//                                    (R-R3-42): the sample Station key is
//                                    StationCallsign; TCI keys are this
//                                    computer's now. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-29  J.J. Boyd / KG4VCF  A slow snapshot is not a dead
//                                    station; a dead window-to-Core
//                                    direction is found while the Core
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/security/CertificateStore.h"
#include "core/session/SessionMessages.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/LoopbackTransport.h"
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
/// without a socket. Same seam tst_station_session.cpp's own
/// makeStationRadioModel() uses (file-local there, so duplicated here
/// rather than shared); connectToRadio() is unusable from a test.
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

/// An arbitrary fingerprint string, used only where the test never lets
/// the TLS handshake reach the point of comparing it (a dead-port dial
/// errors before any certificate is ever presented).
QString placeholderFingerprint()
{
    return QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                          "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
}

} // namespace

class TstSessionLinkLoss : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    // ---- Step 1 + step 4: the link-loss contract and reconnect ----
    void killedDaemonEntersDefinedStaleStateThenReconnectsToARestartedDaemonWithDifferentState();

    // ---- Step 3a ----
    void silentlyDeadPeerIsDetectedNotJustACleanClose();
    void aSlowSnapshotIsNotADeadStation();
    void aDeadWindowToCoreDirectionIsFoundWhileTheCoreStreams();

    // ---- Step 3's mechanism (TLS-specific: QSKIP when unusable) ----
    void autoReconnectUsesOwnedCancellableTimerWithExponentialBackoff();
    void staleTransportErrorDoesNotTearDownAFreshlyAttachedSession();
    void daemonRefusalDoesNotArmAutomaticReconnect();

    // ---- Fix round 1 ----
    void lateFrameOnADeadTransportCannotExitTheStaleState();
    void unpinnedRefusalDoesNotLeaveAPendingRetryArmed();
    void startSessionClearsAnyPreviouslyLatchedRedialTarget();
    void automaticRetryReconnectsToASuccessfulHandshake();
    void operatorConnectionActivityIsIndependentOfRadioState();

    // ---- R-R3-28: backoff across media failures after good handshakes ----
    void mediaSessionBackoffResetsOnlyOnceMediaIsEstablished();
    void sessionWithoutMediaResetsBackoffAtTheHandshake();

    // ---- R-R3-17: actionable reason for a blocked local network ----
    void hostUnreachableOnLocalNetworkNamesTheMacOsSetting();
    void otherFailuresKeepTheSocketText();
    void aProxyThatNeedsALoginIsSaidPlainly();

private:
    /// One temp dir for the whole class so the RSA-3072 key pair is
    /// generated once and every later StationServer loads it back, rather
    /// than paying key generation per slot. Same rationale as
    /// tst_station_session.cpp's own member of this name.
    QTemporaryDir m_securityDir;
};

void TstSessionLinkLoss::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
}

void TstSessionLinkLoss::operatorConnectionActivityIsIndependentOfRadioState()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    stationModel->setConnectionStateForTest(ConnectionState::Disconnected);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setHeartbeatIntervalMs(0);

    RadioModel model(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&model, &proxy);
    client.setHeartbeatIntervalMs(0);
    QSignalSpy activity(&client, &StationClient::connectionActivityChanged);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QVERIFY(!client.isConnectionActive());

    for (int session = 1; session <= 2; ++session) {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client"), this);
        stationEnd->linkTo(clientEnd);
        const int changesBeforeDial = activity.count();
        client.startSession(clientEnd, server.token());
        QVERIFY(client.isConnectionActive()); // Includes the incomplete handshake.
        QVERIFY(activity.count() > changesBeforeDial);
        server.acceptTransport(stationEnd);
        QTRY_COMPARE(completed.count(), session);
        QCOMPARE(client.sessionEpoch(), quint32(session));
        QVERIFY(client.isConnectionActive());
        QVERIFY(!model.isConnected()); // Core is live while its radio is offline.

        const int changesBeforeClose = activity.count();
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QVERIFY(!client.isConnectionActive());
        QVERIFY(!client.isReconnectPending());
        QVERIFY(!client.isHandshakeComplete());
        QVERIFY(!proxy.ready());
        QVERIFY(activity.count() > changesBeforeClose);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

// ── Step 1 + step 4 ──────────────────────────────────────────────────────

void TstSessionLinkLoss::killedDaemonEntersDefinedStaleStateThenReconnectsToARestartedDaemonWithDifferentState()
{
    // ---- First daemon ----
    QTemporaryDir settingsDir1;
    QVERIFY(settingsDir1.isValid());
    AppSettings stationSettings1(settingsDir1.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings1.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50123"));

    // TWO slices, and the restarted daemon below has ONE. Whole-branch
    // review, Important 1: this slot used to build both daemons with
    // makeStationRadioModel(0), so it exercised a changed VALUE and never
    // a changed SET, and a client-side slice the restarted station no
    // longer has survived forever -- unwatched, unmirrored, and silently
    // swallowing every edit the operator made to it. A daemon that comes
    // back with fewer slices is an ordinary restart, not a contrived one:
    // slice_count is a real nereusd.conf key and
    // DaemonApp::createConfiguredSlices clamps it to the board's cap and
    // stops early when the allocator refuses.
    auto stationModel1 = makeStationRadioModel(1);
    QCOMPARE(stationModel1->slices().size(), 2);
    stationModel1->slices().first()->setFrequency(7100000.0);
    stationModel1->slices().at(1)->setFrequency(21300000.0);
    StationServer server1(stationModel1.get(), stationSettings1, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);

    auto* stationEnd1 = new LoopbackTransport(QStringLiteral("station-1"), this);
    auto* clientEnd1 = new LoopbackTransport(QStringLiteral("client-1"), this);
    stationEnd1->linkTo(clientEnd1);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    client.startSession(clientEnd1, server1.token());
    server1.acceptTransport(stationEnd1);
    QTRY_COMPARE(completed.count(), 1);

    QVERIFY(clientModel.isConnected());
    QVERIFY2(!client.isStale(), "a freshly established session read as stale");
    QCOMPARE(client.sessionEpoch(), quint32(1));

    const int sliceId = stationModel1->slices().first()->sliceIndex();
    const int secondSliceId = stationModel1->slices().at(1)->sliceIndex();
    QVERIFY(secondSliceId != sliceId);
    SliceModel* clientSliceBeforeKill = clientModel.sliceById(sliceId);
    QVERIFY(clientSliceBeforeKill != nullptr);
    QCOMPARE(clientSliceBeforeKill->frequency(), 7100000.0);
    // Both of the station's slices arrived, so the reconnect below really
    // is a shrink and not merely a client that never saw the second one.
    QCOMPARE(clientModel.slices().size(), 2);
    SliceModel* clientSecondSlice = clientModel.sliceById(secondSliceId);
    QVERIFY(clientSecondSlice != nullptr);
    QCOMPARE(clientSecondSlice->frequency(), 21300000.0);
    QVERIFY(!client.mirroredObjectKeys().isEmpty());

    QVERIFY(proxy.ready());
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("0")).toString(),
             QStringLiteral("50123"));

    // ---- Kill the daemon: a clean TCP close, no SessionEnd message ----
    //
    // This is what Ctrl-C on nereusd produces: the process is gone before
    // it can send anything application-level, so the client only ever
    // sees the transport's closed() signal. Simulated by closing the
    // STATION-side transport, which is exactly what an OS-level socket
    // teardown looks like from the client's perspective.
    stationEnd1->closeLink(QStringLiteral("simulated: nereusd killed"));

    QTRY_COMPARE(ended.count(), 1);
    QCOMPARE(ended.first().first().toString(), QStringLiteral("link closed"));

    // ---- Step 1's contract ----

    QVERIFY2(client.mirroredObjectKeys().isEmpty(),
             "the client-side mirror registry was not torn down");
    QCOMPARE(clientModel.connectionState(), ConnectionState::Disconnected);
    QVERIFY(!clientModel.isConnected());
    QVERIFY2(client.isStale(),
             "isStale() did not enter the defined stale state on link loss");

    // RadioModel's own state is RETAINED, not reset (design doc section
    // 13): the client is showing last-known values, not zeros, and it is
    // the SAME SliceModel object -- nothing was destroyed. That retention
    // covers the whole SET too: link loss is not the moment to decide a
    // slice is gone, because the station may well come back with it.
    QCOMPARE(clientModel.sliceById(sliceId), clientSliceBeforeKill);
    QCOMPARE(clientSliceBeforeKill->frequency(), 7100000.0);
    QCOMPARE(clientModel.slices().size(), 2);
    QCOMPARE(clientModel.sliceById(secondSliceId), clientSecondSlice);

    // SettingsProxy: keeps serving the cache for reads, drops writes.
    QVERIFY(!proxy.ready());
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("0")).toString(),
             QStringLiteral("50123"));
    QSignalSpy outboundWhileOffline(&proxy, &SettingsProxy::outboundWriteRequested);
    proxy.setValue(QStringLiteral("StationCallsign"), QStringLiteral("60000"));
    QCOMPARE(outboundWhileOffline.count(), 0);
    // The LOCAL cache still updates optimistically (SettingsProxy.h's own
    // "offline behaviour": ready() gates the OUTBOUND side only) -- this
    // is what keeps a remote GUI's Setup page interactive while stale.
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("0")).toString(),
             QStringLiteral("60000"));
    QVERIFY(proxy.droppedWhileOffline().contains(QStringLiteral("StationCallsign")));

    // No automatic reconnect: this session was established via
    // startSession(), which never latches a URL.
    QVERIFY(!client.isReconnectPending());

    // ---- A restarted daemon, with DIFFERENT state ----
    QTemporaryDir settingsDir2;
    QVERIFY(settingsDir2.isValid());
    AppSettings stationSettings2(settingsDir2.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings2.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50999"));

    auto stationModel2 = makeStationRadioModel(0);
    stationModel2->slices().first()->setFrequency(14200000.0);
    StationServer server2(stationModel2.get(), stationSettings2, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* stationEnd2 = new LoopbackTransport(QStringLiteral("station-2"), this);
    auto* clientEnd2 = new LoopbackTransport(QStringLiteral("client-2"), this);
    stationEnd2->linkTo(clientEnd2);
    client.startSession(clientEnd2, server2.token());
    server2.acceptTransport(stationEnd2);

    QTRY_COMPARE(completed.count(), 2);

    // ---- Step 4: converges without a client relaunch ----

    QVERIFY(clientModel.isConnected());
    QVERIFY2(!client.isStale(), "a fresh reconnect still read as stale");
    QCOMPARE(client.sessionEpoch(), quint32(2));
    QVERIFY(!client.mirroredObjectKeys().isEmpty());

    // The SAME SliceModel object, adopted under the station's id -- proof
    // that no client relaunch and no rebinding was needed for a GUI
    // holding this pointer.
    SliceModel* clientSliceAfterReconnect = clientModel.sliceById(sliceId);
    QCOMPARE(clientSliceAfterReconnect, clientSliceBeforeKill);
    // ...but its value now reflects the fresh, DIFFERENT daemon's state.
    QCOMPARE(clientSliceAfterReconnect->frequency(), 14200000.0);

    // Whole-branch review, Important 1: a CHANGED SET, not just a changed
    // value. The restarted daemon has one slice; the client had two. The
    // second is REAPED at the snapshot-complete marker, which is the
    // first moment the station's full set is known -- a partial burst
    // must never be read as "the station no longer has this".
    QVERIFY2(clientModel.sliceById(secondSliceId) == nullptr,
             "a slice the restarted station no longer has survived the "
             "reconnect as an unwatched, unmirrored ghost");
    QCOMPARE(clientModel.slices().size(), 1);
    // ...and the ghost is gone from the wire registry too, so nothing can
    // route to it and no local edit on it is forwarded.
    QVERIFY(!client.mirroredObjectKeys().contains(
        QByteArray("slice:") + QByteArray::number(secondSliceId)));

    QVERIFY(proxy.ready());
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("0")).toString(),
             QStringLiteral("50999"));
}

// ── Step 3a ──────────────────────────────────────────────────────────────

void TstSessionLinkLoss::silentlyDeadPeerIsDetectedNotJustACleanClose()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("50123"));

    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    // Keep the STATION's own heartbeat out of the way; this slot is about
    // the CLIENT's detection of a silent STATION.
    server.setHeartbeatIntervalMs(0);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    // Task 18's own production defaults are 20000 ms / 2 misses
    // (StationClient::kDefaultHeartbeatIntervalMs /
    // kDefaultMaxMissedPongs). Driven down here so the slot costs
    // milliseconds; every assertion below reads the LIVE configured
    // values back through the class's own accessors rather than
    // hardcoding a duplicate number, so this stays honest if the
    // defaults ever move.
    client.setHeartbeatIntervalMs(20);
    client.setMaxMissedPongs(2);
    QCOMPARE(client.heartbeatIntervalMs(), 20);
    QCOMPARE(client.maxMissedPongs(), 2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("silent-station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy timedOut(&client, &StationClient::stationHeartbeatTimeout);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    // Captured SYNCHRONOUSLY inside the stationHeartbeatTimeout handler
    // (onHeartbeatTick() emits it BEFORE disconnectFromStation() touches
    // anything), rather than read from `stationEnd` after the fact: the
    // station's own peer cleanup (StationServer::dropPeer(), reacting to
    // the close disconnectFromStation() provokes) deleteLater()s
    // `stationEnd` once its forwarded close arrives, and the SECOND
    // QTRY_COMPARE_WITH_TIMEOUT below pumps enough event-loop turns for
    // that deferred delete to run before a later read would see it -- a
    // real use-after-free this test hit while being written, distinct
    // from anything in the implementation under test.
    int pingsSeenAtTimeout = -1;
    connect(&client, &StationClient::stationHeartbeatTimeout, this,
            [&]() { pingsSeenAtTimeout = stationEnd->pingsSeen(); });

    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QTRY_COMPARE(completed.count(), 1);

    // THE CASE THIS STEP EXISTS FOR. The link stays nominally OPEN; the
    // station simply stops answering. No close, no application-level
    // SessionEnd, nothing for a close-driven implementation to notice --
    // exactly what a laptop lid, a cell handoff, or a NAT timeout looks
    // like from here.
    QVERIFY(stationEnd->isOpen());
    stationEnd->setAnswersPings(false);

    // Detection happens WITHIN the configured interval/miss count, not
    // never. Fix round 1, Minor 5: the ceiling is DERIVED from the live
    // configuration (heartbeatIntervalMs() * maxMissedPongs(), theoretical
    // minimum 40 ms here) rather than a hardcoded 5000/2000 ms -- 125x and
    // 50x the theoretical minimum respectively, loose enough that a
    // regression making detection take three seconds would still have
    // passed. x10 plus a 500 ms floor for QTest's own polling granularity
    // stays comfortably clear of CI jitter while still catching a
    // regression an order of magnitude slower than expected.
    const int detectionDeadlineMs =
        std::max(500, client.heartbeatIntervalMs() * client.maxMissedPongs() * 10);
    QTRY_COMPARE_WITH_TIMEOUT(timedOut.count(), 1, detectionDeadlineMs);
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, detectionDeadlineMs);
    QCOMPARE(ended.first().first().toString(), QStringLiteral("heartbeat timeout"));

    // The pings really went out, against the LIVE configured value:
    // without them the miss counter could only have been driven by
    // something other than the heartbeat mechanism under test. Pings are
    // SENT by the client (client.m_transport is clientEnd) and RECEIVED
    // by the peer, so the count accumulates on stationEnd, not clientEnd.
    QVERIFY2(pingsSeenAtTimeout >= client.maxMissedPongs(),
             qPrintable(QStringLiteral("only %1 pings reached the station")
                            .arg(pingsSeenAtTimeout)));

    // Task 19's full link-loss contract holds via THIS detection path,
    // not only for a clean close (the previous slot).
    QVERIFY2(client.mirroredObjectKeys().isEmpty(),
             "the mirror registry survived a heartbeat-detected link loss");
    QVERIFY(!clientModel.isConnected());
    QVERIFY2(client.isStale(), "isStale() was not entered on heartbeat detection");
    QVERIFY(!proxy.ready());
    QCOMPARE(proxy.value(QStringLiteral("StationCallsign"), QStringLiteral("0")).toString(),
             QStringLiteral("50123"));

    // And it recovers: a reconnect (manual here -- this session has no
    // latched URL to auto-redial) converges.
    auto* stationEnd2 = new LoopbackTransport(QStringLiteral("station-2"), this);
    auto* clientEnd2 = new LoopbackTransport(QStringLiteral("client-2"), this);
    stationEnd2->linkTo(clientEnd2);
    client.startSession(clientEnd2, server.token());
    server.acceptTransport(stationEnd2);
    QTRY_COMPARE(completed.count(), 2);
    QVERIFY(!client.isStale());
    QVERIFY(clientModel.isConnected());
}

// A pong queued behind the snapshot is not a dead station
// (tst_relay_session: a session over the web relay under load was declared
// dead mid-snapshot and dialled again, joining the relay twice from each
// end). Everything the station sends after its Hello arrives late and in
// order, pongs included; the heartbeat runs from the Hello but counts no
// missed pong before the snapshot-complete marker (the handshake deadline
// bounds that window). Once the session is up, only a pong counts again.
void TstSessionLinkLoss::aSlowSnapshotIsNotADeadStation()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setHeartbeatIntervalMs(0);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHeartbeatIntervalMs(20);
    client.setMaxMissedPongs(2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("slow-station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    // The station's first frame (its Hello) goes through and starts the
    // client's heartbeat; from its second frame on the link is slow.
    int stationFrames = 0;
    connect(stationEnd, &LoopbackTransport::outboundText, this, [&](const QByteArray&) {
        if (++stationFrames == 2) {
            stationEnd->setHoldsOutgoing(true);
        }
    });

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy timedOut(&client, &StationClient::stationHeartbeatTimeout);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QTRY_VERIFY(stationFrames >= 2);
    QCOMPARE(clientEnd->received().size(), 1);

    // Ten times the interval and miss count the heartbeat allows once the
    // session is up: pings keep going out, their pongs wait behind the
    // snapshot, and nothing is counted against the link.
    const int window = client.heartbeatIntervalMs() * client.maxMissedPongs() * 10;
    QTest::qWait(window);
    QVERIFY2(stationEnd->pingsSeen() > client.maxMissedPongs(),
             qPrintable(QStringLiteral("only %1 pings").arg(stationEnd->pingsSeen())));
    QCOMPARE(timedOut.count(), 0);
    QCOMPARE(completed.count(), 0);

    // The snapshot arrives: the session is up, with no second dial.
    stationEnd->setHoldsOutgoing(false);
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(timedOut.count(), 0);

    // From here a missed pong counts: a silent station is found in time.
    stationEnd->setAnswersPings(false);
    const int detectionDeadlineMs =
        std::max(500, client.heartbeatIntervalMs() * client.maxMissedPongs() * 10);
    QTRY_COMPARE_WITH_TIMEOUT(timedOut.count(), 1, detectionDeadlineMs);
}

// Only a pong counts once the session is up (StationServer.h, heartbeat
// section): the window-to-Core direction dies while the Core keeps
// streaming to the window. The window's pings are lost, so no pong comes
// back, and the heartbeat finds the link dead within its interval and
// miss count although frames keep arriving.
void TstSessionLinkLoss::aDeadWindowToCoreDirectionIsFoundWhileTheCoreStreams()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setHeartbeatIntervalMs(0);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    // Two missed pongs span more than the station's delta flush
    // (StationServer::kDefaultDeltaFlushMs), so the Core's frames keep
    // arriving inside the window the heartbeat allows.
    client.setHeartbeatIntervalMs(StationServer::kDefaultDeltaFlushMs * 2);
    client.setMaxMissedPongs(2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("streaming-station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy timedOut(&client, &StationClient::stationHeartbeatTimeout);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QTRY_COMPARE(completed.count(), 1);

    // The Core keeps streaming: a slice's frequency moves every 5 ms, each
    // change a delta to this window.
    SliceModel* slice = stationModel->slices().first();
    QSignalSpy frames(clientEnd, &SessionTransport::textReceived);
    double frequency = 14000000.0;
    QTimer traffic;
    traffic.setInterval(5);
    connect(&traffic, &QTimer::timeout, this, [&] {
        frequency += 10.0;
        slice->setFrequency(frequency);
    });
    traffic.start();
    QTRY_VERIFY(frames.count() > 0);

    // Read inside the timeout handler: the close that follows lets the
    // station delete its end (see silentlyDeadPeerIsDetectedNotJustACleanClose).
    int pingsSeenAtTimeout = -1;
    int framesAtTimeout = -1;
    connect(&client, &StationClient::stationHeartbeatTimeout, this, [&]() {
        pingsSeenAtTimeout = stationEnd->pingsSeen();
        framesAtTimeout = frames.count();
    });

    // The window-to-Core direction dies without a close.
    clientEnd->setDropsOutgoing(true);
    const int pingsBefore = stationEnd->pingsSeen();
    const int framesBefore = frames.count();
    const int detectionDeadlineMs =
        std::max(500, client.heartbeatIntervalMs() * client.maxMissedPongs() * 10);
    QTRY_COMPARE_WITH_TIMEOUT(timedOut.count(), 1, detectionDeadlineMs);
    traffic.stop();
    QVERIFY2(framesAtTimeout > framesBefore,
             "the Core stopped streaming; the test proves nothing");
    QCOMPARE(pingsSeenAtTimeout, pingsBefore);
    QTRY_COMPARE(ended.count(), 1);
    QCOMPARE(ended.first().first().toString(), QStringLiteral("heartbeat timeout"));
}

// ── Step 3's mechanism ───────────────────────────────────────────────────

void TstSessionLinkLoss::autoReconnectUsesOwnedCancellableTimerWithExponentialBackoff()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // A guaranteed-refused port: bind then close, same trick
    // tst_station_session.cpp's failedInitialConnectReportsPromptly uses.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 deadPort = probe.serverPort();
    probe.close();

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    // Shrinks the whole schedule proportionally (1/2/5/10/30/60 s becomes
    // 200/400/1000/2000/6000/12000 ms) so the MECHANISM -- schedule shape,
    // growth, cancellability -- can be exercised without a multi-minute
    // wait. The mechanism itself is identical to production; only the
    // unit changes. Each step below is awaited on the retry signal itself,
    // and the unit is long enough that the "is it currently pending" checks
    // after the third attempt run well inside its 1000 ms wait rather than
    // racing the next scheduled attempt.
    client.setReconnectBackoffUnitMs(200);
    QCOMPARE(client.reconnectBackoffUnitMs(), 200);

    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);
    QSignalSpy activity(&client, &StationClient::connectionActivityChanged);
    const QUrl deadUrl(QStringLiteral("wss://127.0.0.1:%1").arg(deadPort));
    client.connectToStation(deadUrl, QStringLiteral("token"), placeholderFingerprint());
    QVERIFY(client.isConnectionActive());
    QVERIFY(!activity.isEmpty());

    // R-R3-17: wait on each retry signal itself, not a poll. The steps
    // below used QTRY_COMPARE_WITH_TIMEOUT, which polls in 50 ms qWait
    // slices; under load a slice could run past the next scheduled attempt
    // (200 ms, then 400 ms) so the count jumped past the value being waited
    // for and never read it again. QSignalSpy::wait() leaves its event loop
    // on the emission itself, before the just-armed retry timer can fire,
    // so every count below is read exactly once and the assertions stay as
    // strict as before (same fix as 9982cbed for the unpinned refusal
    // test). No isReconnectPending() check at the first step: its timer is
    // only 200 ms, while the load-bearing pending-ness checks below land in
    // the 1000 ms window after the third scheduled attempt.
    const auto waitForScheduled = [&scheduled](int count) {
        while (scheduled.count() < count) {
            if (!scheduled.wait(5000)) { return false; }
        }
        return scheduled.count() == count;
    };
    QVERIFY2(waitForScheduled(1), "the dial to the dead port never armed retry 1");
    QCOMPARE(scheduled.at(0).at(0).toInt(), 1);
    QCOMPARE(scheduled.at(0).at(1).toInt(), 200);   // backoff step 1 * unit 200

    QVERIFY2(waitForScheduled(2), "retry 1 never armed retry 2");
    QCOMPARE(scheduled.at(1).at(0).toInt(), 2);
    QCOMPARE(scheduled.at(1).at(1).toInt(), 400);   // backoff step 2 * unit 200

    QVERIFY2(waitForScheduled(3), "retry 2 never armed retry 3");
    QCOMPARE(scheduled.at(2).at(0).toInt(), 3);
    QCOMPARE(scheduled.at(2).at(1).toInt(), 1000);  // backoff step 5 * unit 200

    // Cancellable: an operator disconnect DURING the backoff wait must
    // win, even though no session is active right now (parent design
    // section 13: "ICE restart and operator-initiated disconnect both
    // need [cancellability]").
    QVERIFY(client.isReconnectPending());
    QVERIFY(client.isConnectionActive());
    const int changesBeforeCancel = activity.count();
    client.disconnectFromStation(QStringLiteral("operator disconnect"));
    QVERIFY(!client.isConnectionActive());
    QVERIFY(activity.count() > changesBeforeCancel);
    QVERIFY2(!client.isReconnectPending(),
             "disconnectFromStation() did not cancel the pending retry");

    const int scheduledAfterCancel = scheduled.count();
    // Comfortably shorter than the NEXT step (backoff step 10 * unit 200
    // = 2000 ms) this attempt count would have used had it fired, so this
    // proves the cancel held rather than merely landing in a lucky gap.
    QTest::qWait(800);
    QCOMPARE(scheduled.count(), scheduledAfterCancel);
    QVERIFY(!client.isReconnectPending());

    // An explicit Connect after cancellation starts a fresh retry sequence
    // on this same client, with no relaunch or direct-radio discovery.
    client.connectToStation(deadUrl, QStringLiteral("token"), placeholderFingerprint());
    QVERIFY(client.isConnectionActive());
    QVERIFY2(waitForScheduled(scheduledAfterCancel + 1),
             "the explicit Connect after cancel never armed a fresh retry");
    QCOMPARE(scheduled.last().at(0).toInt(), 1);
    QCOMPARE(scheduled.last().at(1).toInt(), 200);
    client.disconnectFromStation(QStringLiteral("operator disconnect"));
    QVERIFY(!client.isConnectionActive());
}

void TstSessionLinkLoss::staleTransportErrorDoesNotTearDownAFreshlyAttachedSession()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // Task 19 controller notes, "the defect you will hit inside the first
    // reconnect test you write." StationClient.cpp's sslErrors/
    // errorOccurred lambdas were connected with the QWebSocket as sender,
    // not the SessionTransport wrapping it, so attachTransport()'s
    // disconnect(stale, ...) release (Task 18 fix round 1) did not cover
    // them. An asynchronous error from a closing OLD socket, delivered
    // after a NEW transport has already been attached, used to reach
    // endSession() unconditionally and tear the fresh session down. Fixed
    // in dialStation() by capturing the transport in the lambda and
    // returning early when it is no longer m_transport -- the same guard
    // onTransportClosed() already uses, for the identical reason.
    //
    // NON-VACUITY, STATED HONESTLY: this slot exercises the exact code
    // path the fix lives in -- two real dials, the first torn down out
    // from under an open client socket while the second supersedes it --
    // and passes with the fix in place. It does NOT independently prove
    // the fix is necessary: sabotaging the guard back out (both the
    // errorOccurred and the sslErrors lambda, tried separately, each with
    // several shapes -- a doomed dial to a dead port; a brief wait then
    // supersede; this abrupt-daemon-destruction shape, both single-shot
    // and hammered across 8 rapid redials; a deliberately mismatched
    // fingerprint on the stale dial to force its OWN error path) did not
    // reproduce an observable failure here. A diagnostic pass confirmed
    // WebSocketTransport::closeLink()'s m_socket->close() does not
    // reliably produce an asynchronous errorOccurred on this platform
    // (Qt 6.11, macOS, loopback) for a locally-initiated close, and an
    // sslErrors mismatch needs enough wall-clock time to reach the
    // certificate-comparison point that it is no longer racing by the
    // time it fires. The fix itself is verified correct by construction
    // (it is the literally prescribed remedy, capturing the transport and
    // comparing against m_transport) and by mirroring a pattern already
    // proven necessary and already covered by sabotage:
    // onTransportClosed()'s identical guard, whose removal IS caught by
    // reconnectSurvivesTheOldTransportClosing in tst_station_session.cpp
    // (Task 18 fix round 1, finding F3). This is recorded here rather
    // than a false claim of a passing sabotage run.
    QTemporaryDir settingsDir1;
    QVERIFY(settingsDir1.isValid());
    AppSettings stationSettings1(settingsDir1.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel1 = makeStationRadioModel(0);
    auto server1 = std::make_unique<StationServer>(stationModel1.get(), stationSettings1,
                                                    NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server1->listen(QHostAddress::LocalHost, 0), qPrintable(server1->lastError()));

    QTemporaryDir settingsDir2;
    QVERIFY(settingsDir2.isValid());
    AppSettings stationSettings2(settingsDir2.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel2 = makeStationRadioModel(0);
    // A distinct MAC from server1's, so which session actually completed
    // can be told apart below rather than the two being indistinguishable.
    RadioInfo info2;
    info2.macAddress = QStringLiteral("AA:BB:CC:DD:EE:02");
    info2.name = QStringLiteral("Bench HL2 #2");
    info2.boardType = HPSDRHW::HermesLite;
    stationModel2->setLastRadioInfoForTest(info2);
    StationServer server2(stationModel2.get(), stationSettings2, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server2.listen(QHostAddress::LocalHost, 0), qPrintable(server2.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    // First dial: let it establish fully -- a real live socket, completed
    // TLS handshake, a genuine application session.
    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server1->serverPort())),
                            server1->token(), server1->certificateFingerprint());
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);
    QVERIFY(clientModel.isConnected());

    // Destroy the daemon's entire station-server side out from under the
    // still-open client socket -- its QWebSocketServer, and every
    // accepted peer connection including the one the client is still
    // holding, go away in the SAME call. Immediately afterward, and with
    // NO event-loop turn in between (so the client's socket cannot yet
    // know its peer is gone), supersede with the second real dial. Both
    // "the daemon vanished" and "we are closing this socket ourselves"
    // are now true at once, racing to be whichever asynchronous signal
    // Qt's socket layer reports first, exactly the ambiguity the fix
    // exists to make safe regardless of which one wins.
    server1.reset();
    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server2.serverPort())),
                            server2.token(), server2.certificateFingerprint());

    // Hammer it: several MORE back-to-back redials to the same real
    // server, each with no event-loop turn before the next, so several
    // stale sockets are in flight (mid-TCP-connect, mid-TLS-handshake, or
    // freshly established) at once when superseded. A single race attempt
    // narrows the window to one specific moment in the socket's
    // lifecycle; this widens it across many.
    const QUrl server2Url(QStringLiteral("wss://127.0.0.1:%1").arg(server2.serverPort()));
    for (int i = 0; i < 8; ++i) {
        client.connectToStation(server2Url, server2.token(), server2.certificateFingerprint());
    }

    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 2, 15000);
    QVERIFY(clientModel.isConnected());
    // The session that completed SECOND is server2's, not server1's
    // leftover: this pins WHICH session actually finished last.
    QCOMPARE(client.capabilities().macAddress, stationModel2->currentRadioMac());

    // Now give whatever asynchronous signal the first (stale, abruptly
    // closed) socket produces every chance to arrive, well after the
    // second session succeeded.
    QTest::qWait(2000);

    QVERIFY2(client.isHandshakeComplete(),
             "a stale transport's asynchronous error tore down the freshly attached session");
    QVERIFY2(clientModel.isConnected(),
             "a stale transport's asynchronous error drove the fresh session to Disconnected");
    QCOMPARE(completed.count(), 2);
    // The fix means the stale signal is silently swallowed, not merely
    // survived: the session that actually ended (if the guard were
    // missing) would be the CURRENT one (server2's), so sessionEnded
    // would fire for it. It never should here.
    QCOMPARE(ended.count(), 0);

    // server1 was already destroyed above, mid-test.
    server2.close();
}

void TstSessionLinkLoss::daemonRefusalDoesNotArmAutomaticReconnect()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // Task 18 review, carried item: StationServer::kMaxConcurrentPeers
    // counts the authenticated session, so a client that auto-retried
    // against every daemon-spoken refusal (a version mismatch, a bad
    // token, or -- sharpest -- being PREEMPTED by a newer session) could
    // fight the very session that just took over for the length of the
    // auth deadline. This slot proves the general mechanism that
    // prevents it: a closure carrying an explicit reason FROM THE DAEMON
    // is never retried. Authentication refusal is the cheapest one of
    // those to set up deterministically; version mismatch, preemption and
    // the peer-limit refusal all funnel through the identical
    // disconnectFromStation(reason) call with the same implicit
    // attemptReconnect=false default, so this one slot covers all four by
    // construction, not by re-running each scenario.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    // If the design were wrong, make the leak show up fast rather than
    // needing the full 1 s production first step.
    client.setReconnectBackoffUnitMs(5);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);

    const QUrl url(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()));
    client.connectToStation(url, QStringLiteral("definitely-not-the-token"),
                            server.certificateFingerprint());

    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 15000);
    QVERIFY2(!client.isReconnectPending(),
             "a daemon-refused authentication armed an automatic reconnect");
    QCOMPARE(scheduled.count(), 0);

    QTest::qWait(200);  // many multiples of the shrunk backoff unit
    QCOMPARE(ended.count(), 1);
    QCOMPARE(scheduled.count(), 0);
    QVERIFY(!client.isReconnectPending());

    server.close();
}

// ── Fix round 1 ──────────────────────────────────────────────────────────

void TstSessionLinkLoss::lateFrameOnADeadTransportCannotExitTheStaleState()
{
    // Review Important 1. endSession() cleared the mirror and drove
    // Disconnected but never disconnected m_transport's signals to this
    // object, and onTransportText() has no m_sessionActive gate of its
    // own -- only the heartbeat-start check does. So a frame arriving on
    // the SAME, now-dead transport after the session has ended used to be
    // dispatched in full, and a buffered SnapshotComplete would silently
    // re-set m_handshakeComplete / m_everConnected / m_forwardLocalChanges
    // with no sessionEnded ever firing for the attach that just ended.
    // Exactly the case this subsystem exists to prevent: a link that
    // resumes after a heartbeat timeout, delivering a frame that was
    // already in flight when the timeout was declared.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    // Isolate: this slot is about the CLIENT's own detection and its
    // aftermath, not the station's heartbeat.
    server.setHeartbeatIntervalMs(0);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHeartbeatIntervalMs(20);
    client.setMaxMissedPongs(2);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("silent-station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);

    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QTRY_COMPARE(completed.count(), 1);

    stationEnd->setAnswersPings(false);
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
    QVERIFY2(client.isStale(), "the session did not enter the stale state to test exiting from");
    QVERIFY(!client.isHandshakeComplete());

    // Simulate a frame that was ALREADY in flight when the timeout was
    // declared, landing now. This bypasses LoopbackTransport::deliver()'s
    // own m_open bookkeeping (already flipped false by disconnectFromStation()'s
    // closeLink() call) by invoking the textReceived SIGNAL directly --
    // exactly the shape an already-buffered byte stream delivers on a real
    // socket regardless of an application-level close() having run
    // meanwhile. Signals are invokable by name via QMetaObject::invokeMethod
    // (Qt's own documented behaviour: invoking a signal this way emits it,
    // reaching every connected slot exactly as a real emission would).
    QSignalSpy completedAfterStale(&client, &StationClient::handshakeComplete);
    const QByteArray lateFrame = SessionMessages::encode(SessionMessages::snapshotComplete());
    QVERIFY2(QMetaObject::invokeMethod(clientEnd, "textReceived", Qt::DirectConnection,
                                       Q_ARG(QByteArray, lateFrame)),
             "could not invoke textReceived by name -- check the signal name/signature");

    QVERIFY2(client.isStale(),
             "a frame on the dead transport silently exited the defined stale state");
    QVERIFY2(!client.isHandshakeComplete(),
             "a frame on the dead transport silently re-completed the handshake");
    QCOMPARE(completedAfterStale.count(), 0);
    QVERIFY(!clientModel.isConnected());
    QVERIFY2(client.mirroredObjectKeys().isEmpty(),
             "a frame on the dead transport repopulated the mirror registry");
}

void TstSessionLinkLoss::unpinnedRefusalDoesNotLeaveAPendingRetryArmed()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // Review Important 2, the carried item this task had judged
    // unreachable. connectToStation() resets m_reconnectAttempts, then
    // returns on the empty-fingerprint refusal without ever reaching
    // dialStation() -- and therefore without ever reaching
    // attachTransport()'s m_reconnectTimer->stop(). Every OTHER entry into
    // connectToStation() cancels a pending retry as a side effect of
    // attaching; this one did not, so a retry armed by an earlier failed
    // dial (station "A") survived an unrelated refused attempt at station
    // "B" and would go on to silently redial A.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 deadPort = probe.serverPort();
    probe.close();

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    // Real StationServer purely as a source of a well-formed token and
    // fingerprint to latch -- never listened on, so this dial is doomed.
    StationServer stationA(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setReconnectBackoffUnitMs(50);

    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);

    // Dial "A" against a dead port: fails, arms a retry latched to A's own
    // token and fingerprint.
    const QUrl urlA(QStringLiteral("wss://127.0.0.1:%1").arg(deadPort));
    client.connectToStation(urlA, stationA.token(), stationA.certificateFingerprint());
    // Wait on the exact event, not a poll. QTRY_COMPARE polls in 50 ms
    // qWait slices, the same length as this retry's first backoff step, so
    // when the dead-port error landed early in a slice the armed retry
    // fired, failed and scheduled attempt 2 before the poll ever saw a
    // count of 1, and the count then only grew. QSignalSpy::wait() leaves
    // its event loop on the emission itself, before the 50 ms retry timer
    // can fire, so exactly one retry is armed when the refusal below runs.
    if (scheduled.isEmpty()) {
        QVERIFY2(scheduled.wait(5000), "the dial to the dead port never armed a retry");
    }
    QCOMPARE(scheduled.count(), 1);
    QVERIFY2(client.isReconnectPending(), "no retry was armed to test cancellation against");

    // The operator tries "B" -- any URL, since the refusal fires on the
    // empty fingerprint before any socket is ever touched -- and is
    // refused for an unrelated reason (no pin, no allowUnpinned).
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:1")),
                            QStringLiteral("irrelevant-token"), QString());
    QCOMPARE(ended.count(), 1);

    QVERIFY2(!client.isReconnectPending(),
             "an unpinned refusal to a DIFFERENT station left a pending retry armed to the "
             "previous one");

    // No redial to A occurs: wait comfortably longer than the original
    // scheduled delay (50 ms) and confirm nothing further was scheduled
    // and no handshake ever completed out of nowhere.
    QTest::qWait(500);
    QCOMPARE(scheduled.count(), 1);
    QCOMPARE(completed.count(), 0);
    QVERIFY(!client.isReconnectPending());
}

void TstSessionLinkLoss::startSessionClearsAnyPreviouslyLatchedRedialTarget()
{
    // Minor 3. StationClient.h's own comment on m_lastUrl claimed a
    // startSession()-based (transport-seam) session can never auto-retry
    // because it never latches a URL -- true only for a client that has
    // never dialed via connectToStation() at all. A client that once
    // dialed and later runs a startSession() seam session (this suite's
    // own pattern for the manual-reconnect half of the link-loss
    // narrative) keeps the STALE m_lastUrl/m_token/m_lastFingerprint from
    // the earlier real dial, so a retry-eligible close of the
    // startSession()-based session would auto-dial that stale target.
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 deadPort = probe.serverPort();
    probe.close();

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);

    // A real, successful dial via connectToStation() -- this is what
    // latches m_lastUrl to something real and dialable.
    const QUrl realUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()));
    client.connectToStation(realUrl, server.token(), server.certificateFingerprint());
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);
    server.close();

    // Now a startSession()-based (transport-seam) session, exactly this
    // suite's own manual-reconnect pattern, and a retry-eligible closure
    // of IT.
    auto* stationEnd = new LoopbackTransport(QStringLiteral("seam-station"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("seam-client"), this);
    stationEnd->linkTo(clientEnd);
    QTemporaryDir settingsDir2;
    QVERIFY(settingsDir2.isValid());
    AppSettings stationSettings2(settingsDir2.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel2 = makeStationRadioModel(0);
    StationServer seamServer(stationModel2.get(), stationSettings2, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    client.startSession(clientEnd, seamServer.token());
    seamServer.acceptTransport(stationEnd);
    QTRY_COMPARE(completed.count(), 2);

    stationEnd->closeLink(QStringLiteral("simulated: seam session also dies"));
    QTest::qWait(300);

    QVERIFY2(!client.isReconnectPending(),
             "a startSession()-based session auto-retried against a stale latched URL from an "
             "earlier connectToStation() dial");
    QCOMPARE(scheduled.count(), 0);
}

void TstSessionLinkLoss::automaticRetryReconnectsToASuccessfulHandshake()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }

    // Minor 4, graded the most valuable of the six: no existing slot drove
    // an automatic reconnect all the way to a successful handshake.
    // autoReconnectUsesOwnedCancellableTimerWithExponentialBackoff only
    // ever dials a dead port and stops at cancellation; both convergence
    // slots (killedDaemon..., silentlyDeadPeer...) reconnect MANUALLY, via
    // a second startSession()/connectToStation() call the TEST itself
    // makes, never letting onReconnectTimeout()'s own timer fire the
    // successful redial. So onReconnectTimeout()'s relatch of token and
    // fingerprint into a WORKING session was untested, and so was outbound
    // forwarding after a reconnect via resolveOrCreate()'s re-watch when
    // reached by the automatic path. This is the headline bench row:
    // "kill nereusd, restart it, the GUI returns unaided" -- nothing in
    // this slot calls startSession() or connectToStation() a second time.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 port = probe.serverPort();
    probe.close();

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    // Constructed now, purely as a source of the token/fingerprint the
    // client latches on its first (doomed) dial, but not LISTENING yet --
    // this is "the daemon has not started back up" half of the scenario.
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setReconnectBackoffUnitMs(50);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);

    // Start listening in the first scheduling signal. Polling for count == 1
    // races the deliberately compressed 50 ms retry under full-suite load.
    // This observes the armed timer before allowing it to perform the redial.
    bool firstRetryWasPending = false;
    bool listening = false;
    connect(&client, &StationClient::reconnectScheduled, &server,
            [&](int attempt, int) {
        if (attempt == 1) {
            firstRetryWasPending = client.isReconnectPending()
                && !client.isHandshakeComplete();
            listening = server.listen(QHostAddress::LocalHost, port);
        }
    });
    const QUrl url(QStringLiteral("wss://127.0.0.1:%1").arg(port));
    client.connectToStation(url, server.token(), server.certificateFingerprint());
    QTRY_VERIFY_WITH_TIMEOUT(!scheduled.isEmpty(), 5000);
    QVERIFY(firstRetryWasPending);
    QVERIFY2(listening, qPrintable(server.lastError()));

    // The retry timer fires ON ITS OWN and completes the handshake.
    // Nothing here calls startSession() or connectToStation() again.
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    QVERIFY(client.isHandshakeComplete());
    QVERIFY(clientModel.isConnected());
    QVERIFY(!client.isStale());
    QVERIFY(!client.mirroredObjectKeys().isEmpty());

    // Outbound forwarding over the auto-reconnected session, exercising
    // resolveOrCreate()'s re-watch via the AUTOMATIC path rather than a
    // manually re-driven one.
    SliceModel* stationSlice = stationModel->slices().first();
    SliceModel* clientSlice = clientModel.sliceById(stationSlice->sliceIndex());
    QVERIFY(clientSlice != nullptr);
    clientSlice->setFrequency(21050000.0);
    QTRY_COMPARE(stationSlice->frequency(), 21050000.0);

    server.close();
}

namespace {
QString localNetworkReason(const QString& host)
{
    return QStringLiteral(
               "Can't reach the Core at %1. If this Mac is on the same network as "
               "the Core, macOS may be blocking NereusSDR from your local network: "
               "allow it in System Settings, Privacy & Security, Local Network, "
               "then press Connect.")
        .arg(host);
}
} // namespace

void TstSessionLinkLoss::hostUnreachableOnLocalNetworkNamesTheMacOsSetting()
{
    const QString hostUnreachable = QStringLiteral("Host unreachable");

    // Private IPv4, the operator's own station address.
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("192.168.109.106"),
                                                    /*macOs=*/true),
             localNetworkReason(QStringLiteral("192.168.109.106")));
    // The other private IPv4 ranges and IPv4 link-local.
    for (const QString& host : {QStringLiteral("10.1.2.3"), QStringLiteral("172.20.0.9"),
                                QStringLiteral("169.254.10.20")}) {
        QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                        hostUnreachable, host, true),
                 localNetworkReason(host));
    }
    // Link-local and unique-local IPv6, and the platform's own wording,
    // matched without regard to case.
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    QStringLiteral("no route to host"),
                                                    QStringLiteral("fe80::1c2:3ff:fe04:506"),
                                                    true),
             localNetworkReason(QStringLiteral("fe80::1c2:3ff:fe04:506")));
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("fd12:3456::7"), true),
             localNetworkReason(QStringLiteral("fd12:3456::7")));

    // Off macOS the same failure keeps today's text.
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("192.168.109.106"),
                                                    /*macOs=*/false),
             hostUnreachable);
}

// A network whose proxy demands a login: plain words, on every host and
// platform, in place of the socket's own text.
void TstSessionLinkLoss::aProxyThatNeedsALoginIsSaidPlainly()
{
    const QString words = QStringLiteral("This network's proxy needs a login, which NereusSDR can't provide.");
    for (const bool macOs : {true, false}) {
        for (const QString& host : {QStringLiteral("core.example.net"),
                                    QStringLiteral("192.168.109.106")}) {
            QCOMPARE(StationClient::connectionFailureReason(
                         QAbstractSocket::ProxyAuthenticationRequiredError,
                         QStringLiteral("Proxy requires authentication"), host, macOs),
                     words);
        }
    }
}

void TstSessionLinkLoss::otherFailuresKeepTheSocketText()
{
    const QString hostUnreachable = QStringLiteral("Host unreachable");

    // A public address: Local Network privacy does not apply.
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("8.8.8.8"), true),
             hostUnreachable);
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("2001:db8::1"), true),
             hostUnreachable);
    // Just outside 172.16/12, and a host name that is never resolved here.
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("172.32.0.1"), true),
             hostUnreachable);
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    hostUnreachable,
                                                    QStringLiteral("rock.local"), true),
             hostUnreachable);

    // Not host-unreachable, on a private address.
    const QString refused = QStringLiteral("Connection refused");
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::ConnectionRefusedError,
                                                    refused,
                                                    QStringLiteral("192.168.109.106"), true),
             refused);
    const QString networkUnreachable = QStringLiteral("Network unreachable");
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::NetworkError,
                                                    networkUnreachable,
                                                    QStringLiteral("192.168.109.106"), true),
             networkUnreachable);
    const QString timedOut = QStringLiteral("Socket operation timed out");
    QCOMPARE(StationClient::connectionFailureReason(QAbstractSocket::SocketTimeoutError,
                                                    timedOut,
                                                    QStringLiteral("10.0.0.5"), true),
             timedOut);
}

// ── R-R3-28 ──────────────────────────────────────────────────────────────

namespace {

/// What retryDelaysAcrossFailures() observed: the delay each retry was
/// scheduled with and, when a round did not complete, what it waited for.
struct RetryDelays {
    QList<int> delays;
    QString failure;
};

/// Drives `rounds` retry-eligible closures of a live wss session, each
/// after a good handshake, and returns the delay each retry was scheduled
/// with. `beforeFailure` runs on each established session first.
RetryDelays retryDelaysAcrossFailures(StationClient& client, QSignalSpy& completed,
                                      QSignalSpy& scheduled, int rounds,
                                      const std::function<void()>& beforeFailure = {})
{
    RetryDelays result;
    for (int round = 0; round < rounds; ++round) {
        const int handshakes = completed.count();
        const int retries = scheduled.count();
        if (beforeFailure) {
            beforeFailure();
        }
        client.disconnectFromStation(QStringLiteral("media peer connection failed"),
                                     /*attemptReconnect=*/true);
        if (scheduled.count() != retries + 1) {
            result.failure = QStringLiteral("round %1: the closure scheduled %2 retries, not 1")
                                 .arg(round + 1).arg(scheduled.count() - retries);
            return result;
        }
        result.delays.append(scheduled.constLast().at(1).toInt());
        if (!QTest::qWaitFor([&] { return completed.count() == handshakes + 1; }, 15000)) {
            result.failure = QStringLiteral("round %1: timed out after 15 s waiting for the "
                                            "retry's handshake (%2 handshakes, expected %3)")
                                 .arg(round + 1).arg(completed.count()).arg(handshakes + 1);
            return result;
        }
    }
    return result;
}

} // namespace

void TstSessionLinkLoss::mediaSessionBackoffResetsOnlyOnceMediaIsEstablished()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }
    // media-recovery.md "Explicit remaining boundary": each good control
    // handshake used to reset the backoff, so media that failed after
    // every handshake retried at the first step forever. With media
    // negotiated, only established media proves the session works.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setMediaEnabled(true);
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    constexpr int kUnitMs = 20;
    client.setReconnectBackoffUnitMs(kUnitMs);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);

    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
                            server.token(), server.certificateFingerprint());
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);
    QVERIFY(client.mediaAvailable());

    // Three media failures after good handshakes: the schedule grows.
    const RetryDelays grown = retryDelaysAcrossFailures(client, completed, scheduled, 3);
    QVERIFY2(grown.failure.isEmpty(), qPrintable(grown.failure));
    QCOMPARE(grown.delays, QList<int>({1 * kUnitMs, 2 * kUnitMs, 5 * kUnitMs}));

    // A late ready naming a retired session changes nothing.
    const quint32 retired = client.sessionEpoch() - 1;
    const RetryDelays afterLateReady = retryDelaysAcrossFailures(
        client, completed, scheduled, 1, [&] { client.noteMediaEstablished(retired); });
    QVERIFY2(afterLateReady.failure.isEmpty(), qPrintable(afterLateReady.failure));
    QCOMPARE(afterLateReady.delays, QList<int>({10 * kUnitMs}));

    // Established media on the current session starts the schedule over.
    const RetryDelays afterReady = retryDelaysAcrossFailures(
        client, completed, scheduled, 1,
        [&] { client.noteMediaEstablished(client.sessionEpoch()); });
    QVERIFY2(afterReady.failure.isEmpty(), qPrintable(afterReady.failure));
    QCOMPARE(afterReady.delays, QList<int>({1 * kUnitMs}));

    // Manual Disconnect during the wait cancels the retry.
    client.disconnectFromStation(QStringLiteral("media peer connection failed"), true);
    QVERIFY(client.isReconnectPending());
    const int retries = scheduled.count();
    client.disconnectFromStation(QStringLiteral("operator disconnect"));
    QVERIFY(!client.isReconnectPending());
    QTest::qWait(kUnitMs * 5);
    QCOMPARE(scheduled.count(), retries);
    QCOMPARE(completed.count(), 6);
    server.close();
}

void TstSessionLinkLoss::sessionWithoutMediaResetsBackoffAtTheHandshake()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }
    // No media negotiated: the handshake is the whole session, and it
    // resets the schedule exactly as before R-R3-28.
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    auto stationModel = makeStationRadioModel(0);
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    constexpr int kUnitMs = 20;
    client.setReconnectBackoffUnitMs(kUnitMs);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);

    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort())),
                            server.token(), server.certificateFingerprint());
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);
    QVERIFY(!client.mediaAvailable());

    const RetryDelays flat = retryDelaysAcrossFailures(client, completed, scheduled, 3);
    QVERIFY2(flat.failure.isEmpty(), qPrintable(flat.failure));
    QCOMPARE(flat.delays, QList<int>({kUnitMs, kUnitMs, kUnitMs}));
    client.disconnectFromStation(QStringLiteral("operator disconnect"));
    server.close();
}

QTEST_MAIN(TstSessionLinkLoss)
#include "tst_session_link_loss.moc"
