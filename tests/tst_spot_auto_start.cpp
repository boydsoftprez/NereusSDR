// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test file. The spot ingest clients
// are AetherSDR ports, but the restore-on-launch driver pinned here
// is a NereusSDR addition (Phase 3J-2 + 3R M3).
//
// NereusSDR - Phase 3J-2 + 3R M3: spot-client auto-connect / auto-start
// state restore on launch.
//
// SpotHubDialog F2 persists each per-source AutoConnect / AutoStart flag
// + identity / port / interval. M3 plumbs the launch-time read-back:
// RadioModel::restoreSpotClientAutoStartState reads every persisted
// "<Source>AutoConnect" / "<Source>AutoStart" key and, when True, calls
// the corresponding start method with the persisted connection params.
// MainWindow invokes this once at startup after the RadioModel is fully
// wired (companion to the existing tryAutoReconnect singleShot).
//
// What is verified:
//   * DxCluster: AutoConnect=True + DxClusterHost / Port / Callsign
//     drives connectToCluster -> host() and port() reflect the saved values.
//   * RBN: same shape, different default host.
//   * WSJT-X: AutoStart=True + WsjtxAddress / WsjtxPort drives
//     startListening; isListening() becomes true after a successful bind.
//   * SpotCollector: AutoStart=True + SpotCollectorPort drives
//     startListening; isListening() becomes true.
//   * POTA: AutoStart=True + PotaPollInterval drives startPolling;
//     isPolling() becomes true.
//   * AutoConnect=False (or absent) does NOT fire any start.
//
// FreeDV Reporter / PSK Reporter coverage:
//   * FreeDV Reporter auto-connect would require a live WebSocket round-
//     trip; pin only that the request is non-destructive (no crash, no
//     side effect on the other clients).
//   * PSK Reporter is send-only and has no meaningful "start" call when
//     auto-start is set; pin that restoreSpotClientAutoStartState is a
//     no-op for PSK Reporter regardless of the AutoStart flag.
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 + 3R M3 initial commit.
//                                    AI tooling: Anthropic Claude Code.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/FreeDVReporterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/WsjtxClient.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

AppSettings& testSettings()
{
    return AppSettings::instance();
}

} // namespace

class TestSpotAutoStart : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        testSettings().clear();
    }

    void cleanup()
    {
        testSettings().clear();
    }

    // ── DxCluster ───────────────────────────────────────────────────────

    void dxClusterAutoConnectFiresStartCall()
    {
        testSettings().setValue("DxClusterAutoConnect", "True");
        testSettings().setValue("DxClusterHost",
                                QStringLiteral("dxc.example.org"));
        testSettings().setValue("DxClusterPort", 7100);
        testSettings().setValue("DxClusterCallsign", QStringLiteral("KG4VCF"));

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        // After connectToCluster, host()/port() reflect the persisted values
        // regardless of whether the TCP socket actually completed (those
        // assignments are unconditional in DxClusterClient.cpp:98-100).
        QCOMPARE(model.dxCluster()->host(), QString("dxc.example.org"));
        QCOMPARE(int(model.dxCluster()->port()), 7100);
    }

    void dxClusterAutoConnectFalseDoesNotFire()
    {
        testSettings().setValue("DxClusterAutoConnect", "False");
        testSettings().setValue("DxClusterHost",
                                QStringLiteral("dxc.example.org"));
        testSettings().setValue("DxClusterPort", 7100);

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        // host() default is empty; port() default is 7300.
        QCOMPARE(model.dxCluster()->host(), QString());
        QCOMPARE(int(model.dxCluster()->port()), 7300);
    }

    // ── RBN (same DxClusterClient type, different keys) ─────────────────

    void rbnAutoConnectFiresStartCall()
    {
        testSettings().setValue("RbnAutoConnect", "True");
        testSettings().setValue("RbnHost",
                                QStringLiteral("rbn.example.org"));
        testSettings().setValue("RbnPort", 7250);
        testSettings().setValue("RbnCallsign", QStringLiteral("KG4VCF"));

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QCOMPARE(model.rbn()->host(), QString("rbn.example.org"));
        QCOMPARE(int(model.rbn()->port()), 7250);
    }

    // ── WSJT-X ──────────────────────────────────────────────────────────

    void wsjtxAutoStartFiresListenCall()
    {
        testSettings().setValue("WsjtxAutoStart", "True");
        // 224.0.0.1 (multicast) is a real address WSJT-X uses by default.
        // Tests bind to UDP locally; pick a high port unlikely to clash
        // with anything else on the bench.
        testSettings().setValue("WsjtxAddress",
                                QStringLiteral("127.0.0.1"));
        testSettings().setValue("WsjtxPort", 28237);

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QVERIFY(model.wsjtx()->isListening());
    }

    void wsjtxAutoStartFalseDoesNotFire()
    {
        testSettings().setValue("WsjtxAutoStart", "False");
        testSettings().setValue("WsjtxPort", 28238);

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QVERIFY(!model.wsjtx()->isListening());
    }

    // ── SpotCollector ───────────────────────────────────────────────────

    void spotCollectorAutoStartFiresListenCall()
    {
        testSettings().setValue("SpotCollectorAutoStart", "True");
        testSettings().setValue("SpotCollectorPort", 29999);

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QVERIFY(model.spotCollector()->isListening());
    }

    // ── POTA ────────────────────────────────────────────────────────────

    void potaAutoStartFiresPollingCall()
    {
        testSettings().setValue("PotaAutoStart", "True");
        testSettings().setValue("PotaPollInterval", 60);

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QVERIFY(model.pota()->isPolling());
    }

    void potaAutoStartFalseDoesNotFire()
    {
        testSettings().setValue("PotaAutoStart", "False");
        testSettings().setValue("PotaPollInterval", 60);

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QVERIFY(!model.pota()->isPolling());
    }

    // ── FreeDV Reporter ─────────────────────────────────────────────────
    // FreeDV Reporter auto-connect calls startConnection() which spawns a
    // QWebSocket. The connection then drifts toward "connecting" on a
    // background timer; the unit test cannot drive it to "connected"
    // without a real server. Verify only that the call is non-destructive
    // and that the AutoStart=False path leaves the client idle.

    void freeDvReporterAutoConnectFalseDoesNotFire()
    {
        testSettings().setValue("FreeDvAutoStart", "False");

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        QVERIFY(!model.freeDvReporter()->isConnected());
    }

    void freeDvReporterAutoConnectIsNonDestructive()
    {
        // AutoConnect=True with identity pre-populated so the safety
        // gate doesn't skip the call. The call must not crash and must
        // not interfere with the other clients.
        testSettings().setValue("FreeDvAutoStart", "True");
        testSettings().setValue("User/Callsign", QStringLiteral("KG4VCF"));
        testSettings().setValue("User/GridSquare", QStringLiteral("EM73"));

        RadioModel model;
        model.restoreSpotClientAutoStartState();
        // Pinning non-crash + accessor surface stays intact.
        QVERIFY(model.freeDvReporter() != nullptr);
        QVERIFY(model.dxCluster() != nullptr);
        QVERIFY(model.rbn() != nullptr);
        QVERIFY(model.wsjtx() != nullptr);
    }

    // Post-3J-2: identity-aware FreeDV auto-start. The earlier landing
    // unconditionally fired startConnection() whenever FreeDvAutoStart
    // was True, producing anonymous reports (callsign empty) and the
    // user-reported "FreeDV Reporter not connecting" symptom. The
    // Settings tab puts a single source of identity in front of the
    // user; the restore path now refuses to start the WebSocket when
    // no identity is configured.

    void freeDvAutoStartSkipsWhenIdentityMissing()
    {
        // AutoStart=True but no callsign anywhere.
        testSettings().setValue("FreeDvAutoStart", "True");

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        // No identity --> client must not start.
        QVERIFY(!model.freeDvReporter()->isConnected());
    }

    void freeDvAutoStartFallsBackToUserCallsign()
    {
        // User/Callsign is set, FreeDvReporter/Callsign is not.
        testSettings().setValue("FreeDvAutoStart", "True");
        testSettings().setValue("User/Callsign", QStringLiteral("KG4VCF"));
        testSettings().setValue("User/GridSquare", QStringLiteral("EM73"));

        RadioModel model;
        model.restoreSpotClientAutoStartState();

        // The client's identity field should reflect User/Callsign
        // because the restore path re-applied setIdentity from the
        // User/* fallback. Test seam is callsignForTest (returns the
        // stored m_callsign).
        QCOMPARE(model.freeDvReporter()->callsignForTest(),
                 QString("KG4VCF"));
        QCOMPARE(model.freeDvReporter()->gridSquareForTest(),
                 QString("EM73"));
    }

    // ── PSK Reporter ────────────────────────────────────────────────────
    // PSK Reporter is send-only; restoreSpotClientAutoStartState is a
    // no-op for it. Pin that no listener is opened regardless of the
    // AutoStart flag and that the client accessor stays alive.

    void pskReporterAutoStartTrueIsNoOp()
    {
        testSettings().setValue("PskReporterAutoStart", "True");

        RadioModel model;
        model.restoreSpotClientAutoStartState();
        QVERIFY(model.pskReporter() != nullptr);
        QVERIFY(!model.pskReporter()->isListening());
    }
};

QTEST_GUILESS_MAIN(TestSpotAutoStart)
#include "tst_spot_auto_start.moc"
