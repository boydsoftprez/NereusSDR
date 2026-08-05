// no-port-check: NereusSDR-original test file. Exercises new ConnectFailure
// enum and connectFailed() signal added in Phase 3Q Task 3. No Thetis logic
// is ported here; the tested infrastructure is NereusSDR-original.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QHostAddress>
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestRadioConnectionFailure : public QObject {
    Q_OBJECT

private:
    // Build a RadioInfo pointing at an RFC 5737 unreachable address so
    // connectToRadio() can never hear a reply.
    RadioInfo unreachableInfo() const {
        RadioInfo info;
        info.address         = QHostAddress(QStringLiteral("192.0.2.1"));
        info.port            = 1024;
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("00:00:00:00:00:00");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("Unreachable");
        return info;
    }

private slots:
    // After connectToRadio() to an unreachable host, connectFailed(Timeout, ...)
    // must be emitted within the 2-second connect-watchdog budget.
    // Budget for spy.wait(): 2000 ms connect watchdog + 1000 ms slack = 3000 ms.
    void emitsTypedFailureOnUnreachable() {
        P1RadioConnection conn;
        conn.init();

        QSignalSpy spy(&conn, &RadioConnection::connectFailed);

        conn.connectToRadio(unreachableInfo());

        QVERIFY(spy.wait(3000));
        QCOMPARE(spy.count(), 1);

        const auto reason = spy.takeFirst().at(0).value<ConnectFailure>();
        QCOMPARE(reason, ConnectFailure::Timeout);
    }

    // Issue #239 regression: while waiting for the first ep6 frame, state must
    // be Connecting (not Connected). The UI uses state() == Connected to drive
    // the green "Connected" pill, and the bug was that connectToRadio() would
    // set Connected immediately after sending metis-start, leaving the UI
    // claiming success even when the radio was powered off.
    void stateStaysConnectingUntilFirstEp6() {
        P1RadioConnection conn;
        conn.init();

        QSignalSpy stateSpy(&conn, &RadioConnection::connectionStateChanged);

        conn.connectToRadio(unreachableInfo());

        // Wait long enough for the worker to enter Connecting (a few event-loop
        // ticks) but well before the 2 s connect watchdog fires.
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connecting, 500);

        // State must NOT have transitioned to Connected before any data
        // arrived. Re-check after a short additional wait to be sure no
        // delayed setState() snuck through.
        QTest::qWait(200);
        QCOMPARE(conn.state(), ConnectionState::Connecting);

        // None of the emitted states should equal Connected.
        for (int i = 0; i < stateSpy.count(); ++i) {
            const auto s = stateSpy.at(i).at(0).value<ConnectionState>();
            QVERIFY2(s != ConnectionState::Connected,
                "Connected emitted before any ep6 frame arrived (issue #239)");
        }
    }

    // Issue #239 regression: after the connect watchdog fires for an
    // unreachable radio, state must end at Disconnected. Previously the
    // watchdog only emitted connectFailed and left state at Connected,
    // so the UI continued to show the green "Connected" pill forever.
    void stateBecomesDisconnectedOnConnectTimeout() {
        P1RadioConnection conn;
        conn.init();

        QSignalSpy failSpy(&conn, &RadioConnection::connectFailed);

        conn.connectToRadio(unreachableInfo());

        QVERIFY(failSpy.wait(3000));
        QCOMPARE(failSpy.count(), 1);

        // Watchdog teardown is queued behind the connectFailed emission;
        // pump the event loop briefly to let the state-change land.
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Disconnected, 500);
    }

    // connectFailed must NOT fire when an intentional disconnect() is called —
    // that is a user-initiated state change, not a failure.
    void noFailureOnIntentionalDisconnect() {
        P1RadioConnection conn;
        conn.init();

        QSignalSpy spy(&conn, &RadioConnection::connectFailed);

        conn.connectToRadio(unreachableInfo());
        // Immediately disconnect before the connect watchdog can fire.
        conn.disconnect();

        // Give the event loop a tick — if connectFailed was wrongly queued it
        // would arrive here.
        QTest::qWait(100);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestRadioConnectionFailure)
#include "tst_radio_connection_failure.moc"
