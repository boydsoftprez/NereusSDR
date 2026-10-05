// no-port-check: NereusSDR-original test file. Exercises new ConnectFailure
// enum and connectFailed() signal added in Phase 3Q Task 3. No Thetis logic
// is ported here; the tested infrastructure is NereusSDR-original.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QHostAddress>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QThread>
#include <QUdpSocket>
#include <memory>
#include "core/P2RadioConnection.h"
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
    void failedPlaceholderBindCompletesWorkerInit_data()
    {
        QTest::addColumn<bool>("protocol2");
        QTest::newRow("p1") << false;
        QTest::newRow("p2") << true;
    }

    // Network denial forces the real Any:0 placeholder bind and subsequent
    // fallback to fail. The child uses the same started/init -> queued connect
    // ordering as RadioModel; a partial init crashes before its Timeout signal.
    void failedPlaceholderBindCompletesWorkerInit()
    {
#if defined(Q_OS_MAC)
        QFETCH(bool, protocol2);
        const QString sandbox = QStandardPaths::findExecutable(QStringLiteral("sandbox-exec"));
        if (sandbox.isEmpty()) {
            QSKIP("Failed-bind forcing requires macOS sandbox-exec");
        }
        QProcess child;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("NEREUS_FAILED_BIND_CHILD"), QStringLiteral("1"));
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::MergedChannels);
        QSignalSpy finished(&child, &QProcess::finished);
        child.start(sandbox, {
            QStringLiteral("-p"), QStringLiteral("(version 1)(allow default)(deny network*)"),
            QCoreApplication::applicationFilePath(),
            protocol2 ? QStringLiteral("failedPlaceholderBindChild:p2")
                      : QStringLiteral("failedPlaceholderBindChild:p1")});
        QVERIFY2(child.waitForStarted(), qPrintable(child.errorString()));
        const auto cleanup = qScopeGuard([&child]() {
            if (child.state() != QProcess::NotRunning) {
                child.kill();
                child.waitForFinished();
            }
        });
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 10000);
        const QByteArray output = child.readAll();
        qInfo().noquote() << output;
        const QByteArray warning = protocol2 ? "P2: Failed to bind UDP socket"
                                             : "P1: Failed to bind UDP socket";
        QVERIFY2(output.contains(warning), "Child did not reach the production failed placeholder bind");
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
#else
        QSKIP("Failed-bind forcing is currently implemented only with macOS sandbox-exec");
#endif
    }

    void failedPlaceholderBindChild_data()
    {
        failedPlaceholderBindCompletesWorkerInit_data();
    }

    // Invoked only by the externally restricted child process above. No
    // production injection seam is needed, and unsupported hosts explicitly skip.
    void failedPlaceholderBindChild()
    {
        if (!qEnvironmentVariableIsSet("NEREUS_FAILED_BIND_CHILD")) {
            QSKIP("Run through failedPlaceholderBindCompletesWorkerInit to force the bind failure");
        }
        QFETCH(bool, protocol2);
        QUdpSocket probe;
        QVERIFY2(!probe.bind(QHostAddress::Any, 0), "Network-denial profile did not force bind failure");

        std::unique_ptr<RadioConnection> ownedConnection;
        if (protocol2) {
            ownedConnection = std::make_unique<P2RadioConnection>();
        } else {
            ownedConnection = std::make_unique<P1RadioConnection>();
        }
        RadioConnection* connection = ownedConnection.get();
        QThread worker;
        connection->moveToThread(&worker);
        QObject::connect(&worker, &QThread::started, connection, &RadioConnection::init);
        QObject::connect(&worker, &QThread::finished, connection, &QObject::deleteLater);
        ownedConnection.release(); // finished/deleteLater owns the worker object.
        const auto cleanup = qScopeGuard([&worker]() {
            worker.quit();
            worker.wait();
        });
        // Collect worker signals on this thread before inspecting the lists.
        // QSignalSpy's direct cross-thread collection does not lock QList reads.
        QObject observations;
        QList<ConnectFailure> failures;
        QList<ConnectionState> states;
        QObject::connect(connection, &RadioConnection::connectFailed, &observations,
                         [&failures](ConnectFailure reason, const QString&) {
                             failures.append(reason);
                         }, Qt::QueuedConnection);
        QObject::connect(connection, &RadioConnection::connectionStateChanged, &observations,
                         [&states](ConnectionState state) {
                             states.append(state);
                         }, Qt::QueuedConnection);
        RadioInfo info = unreachableInfo();
        info.address = QHostAddress::LocalHost;
        info.protocol = protocol2 ? ProtocolVersion::Protocol2 : ProtocolVersion::Protocol1;
        worker.start();
        QVERIFY(QMetaObject::invokeMethod(connection, [connection, info]() {
            connection->connectToRadio(info);
        }, Qt::QueuedConnection));

        QTRY_COMPARE_WITH_TIMEOUT(failures.count(), 1, 3000);
        QCOMPARE(failures.first(), ConnectFailure::Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(!states.isEmpty(), 500);
        QCOMPARE(states.last(), ConnectionState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(connection, [connection]() {
            connection->disconnect();
        }, Qt::BlockingQueuedConnection));
        QCoreApplication::processEvents();
        QCOMPARE(failures.count(), 1);
    }

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
