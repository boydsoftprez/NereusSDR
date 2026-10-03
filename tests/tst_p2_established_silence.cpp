// tests/tst_p2_established_silence.cpp
//
// Real-loopback verification for the Protocol 2 established-stream silence
// boundary. The fake sends one genuine 1444-byte DDC2 datagram through the
// production QUdpSocket path, then controls the other accepted role traffic.

#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <functional>

#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P2FakeRadio;

namespace {

constexpr int kConnectTimeoutMs = 180;
constexpr int kEstablishedTimeoutMs = 220;

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000)
{
    return QTest::qWaitFor(predicate, timeoutMs);
}

void configureConnection(P2RadioConnection& connection,
                         const P2FakeRadio& fake)
{
    connection.setPortBasesForTest(fake.outboundPortBase(),
                                   fake.inputRolePortBase());
    connection.setSilenceTimeoutsForTest(kConnectTimeoutMs,
                                         kEstablishedTimeoutMs);
    connection.init();
}

// Connects and returns once the connection reports Connected. Event
// driven: the first DDC goes out on the first event-loop pass that sees
// the client, and the wait ends on the state change itself, so none of the
// connect watchdog's kConnectTimeoutMs is spent in qWaitFor's sleeps (the
// cause of a failure under load). 3 s bounds a connect that never comes.
bool establish(P2RadioConnection& connection, P2FakeRadio& fake)
{
    QEventLoop loop;
    bool connected = false;
    const QMetaObject::Connection onState = QObject::connect(
        &connection, &RadioConnection::connectionStateChanged, &loop,
        [&loop, &connected](ConnectionState state) {
            if (state == ConnectionState::Connected) {
                connected = true;
                loop.quit();
            }
        });
    const QMetaObject::Connection onFailed = QObject::connect(
        &connection, &RadioConnection::connectFailed, &loop, &QEventLoop::quit);
    bool sent = false;
    QTimer firstDdc;
    firstDdc.setInterval(0);
    QObject::connect(&firstDdc, &QTimer::timeout, &loop, [&fake, &sent, &firstDdc]() {
        if (!sent && fake.hasClient()) {
            sent = true;
            firstDdc.stop();
            fake.sendDdc(2);
        }
    });
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);

    connection.connectToRadio(fake.radioInfo());
    if (connection.state() != ConnectionState::Connected) {
        firstDdc.start();
        limit.start(3000);
        loop.exec();
    }
    QObject::disconnect(onState);
    QObject::disconnect(onFailed);
    return connected || connection.state() == ConnectionState::Connected;
}

} // namespace

class TestP2EstablishedSilence final : public QObject {
    Q_OBJECT

private slots:
    void allIngressSilenceQuiescesThenReportsOnce()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());

        P2RadioConnection connection;
        configureConnection(connection, fake);

        QStringList eventOrder;
        QObject::connect(&connection, &RadioConnection::errorOccurred,
                         &connection,
                         [&eventOrder](RadioConnectionError error,
                                       const QString&) {
            if (error == RadioConnectionError::NoDataTimeout) {
                eventOrder.append(QStringLiteral("error"));
            }
        });
        QObject::connect(&connection, &RadioConnection::connectionStateChanged,
                         &connection, [&eventOrder](ConnectionState state) {
            if (state == ConnectionState::LinkLost) {
                eventOrder.append(QStringLiteral("link-lost"));
            }
        });

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        QSignalSpy stateSpy(&connection,
                            &RadioConnection::connectionStateChanged);
        QSignalSpy iqSpy(&connection, &RadioConnection::iqDataReceived);

        QVERIFY(establish(connection, fake));
        QVERIFY(iqSpy.count() >= 1);
        errorSpy.clear();
        stateSpy.clear();

        // The TX-IQ timer keeps producing outbound frames while ingress is
        // silent. Loss must be based only on accepted inbound traffic.
        const int egressAtEstablished = fake.totalEgressDatagrams();
        QTRY_VERIFY_WITH_TIMEOUT(fake.totalEgressDatagrams()
                                     > egressAtEstablished,
                                 1000);
        fake.stopIngress();

        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::LinkLost, 3000);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).value<RadioConnectionError>(),
                 RadioConnectionError::NoDataTimeout);
        QCOMPARE(eventOrder,
                 QStringList({QStringLiteral("error"),
                              QStringLiteral("link-lost")}));
        QTRY_COMPARE_WITH_TIMEOUT(fake.stopCount(), 1, 1000);
        QCOMPARE(fake.lastHighPriorityFlags() & 0x03u, 0u);
        QCOMPARE(fake.moxAssertedCount(), 0);

        // All producers are terminal before LinkLost is visible. A late MOX
        // setter and late valid DDC/status packets cannot relaunch egress,
        // re-key, emit I/Q, refresh a timer, or resurrect Connected.
        // Let the fake drain datagrams produced before the terminal close;
        // subsequent equality then measures new production, not delivery lag.
        QTest::qWait(50);
        const int egressAfterStop = fake.totalEgressDatagrams();
        const int iqAfterStop = iqSpy.count();
        connection.setMox(true);
        fake.resumeIngress();
        fake.sendStatus();
        fake.sendDdc(2);
        QTest::qWait(kEstablishedTimeoutMs + 50);
        QCOMPARE(fake.totalEgressDatagrams(), egressAfterStop);
        QCOMPARE(fake.moxAssertedCount(), 0);
        QCOMPARE(iqSpy.count(), iqAfterStop);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(connection.state(), ConnectionState::LinkLost);
    }

    void datagramWaitingAtExpiryIsProcessedNotDeclaredLost()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);

        // A valid status datagram reaches the socket, then the event loop
        // stalls past the deadline so the wakeup runs ahead of readyRead.
        // Thetis only declares loss when its wait times out with nothing
        // received; a datagram that is waiting must be read first.
        fake.sendStatus();
        QThread::msleep(static_cast<unsigned long>(kEstablishedTimeoutMs + 80));
        connection.runEstablishedSilenceWakeupForTest();

        QCOMPARE(errorSpy.count(), 0);
        QCOMPARE(connection.state(), ConnectionState::Connected);
        QTest::qWait(50);
        QCOMPARE(fake.stopCount(), 0);

        // With nothing waiting, the same wakeup past the deadline is loss.
        QThread::msleep(static_cast<unsigned long>(kEstablishedTimeoutMs + 80));
        connection.runEstablishedSilenceWakeupForTest();
        QCOMPARE(connection.state(), ConnectionState::LinkLost);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).value<RadioConnectionError>(),
                 RadioConnectionError::NoDataTimeout);
        QTRY_COMPARE_WITH_TIMEOUT(fake.stopCount(), 1, 1000);
    }

    // Fix wave M2: the wakeup reads what is waiting before judging silence,
    // but only accepted traffic refreshes the deadline. A waiting datagram
    // that is rejected (wrong size, wrong role, wrong sender, or a disabled
    // wideband role) must still end in NoDataTimeout on that same wakeup.
    void rejectedDatagramWaitingAtExpiryIsStillLoss_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("wrong size") << QStringLiteral("malformed");
        QTest::newRow("wrong role") << QStringLiteral("role");
        QTest::newRow("wrong sender") << QStringLiteral("address");
        QTest::newRow("disabled wideband") << QStringLiteral("wideband");
    }

    void rejectedDatagramWaitingAtExpiryIsStillLoss()
    {
        QFETCH(QString, kind);
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        if (kind == QLatin1String("malformed")) {
            fake.sendMalformedStatus();
        } else if (kind == QLatin1String("role")) {
            fake.sendWrongRoleStatus();
        } else if (kind == QLatin1String("address")) {
            QVERIFY2(fake.sendWrongAddressStatus(),
                     "IPv6 loopback ::1 must be available for wrong-address coverage");
        } else {
            fake.sendWideband(0);
        }
        // The event loop stalls past the deadline, so the wakeup finds the
        // rejected datagram still waiting on the socket.
        QThread::msleep(static_cast<unsigned long>(kEstablishedTimeoutMs + 80));
        connection.runEstablishedSilenceWakeupForTest();

        QCOMPARE(connection.state(), ConnectionState::LinkLost);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).value<RadioConnectionError>(),
                 RadioConnectionError::NoDataTimeout);
        QTRY_COMPARE_WITH_TIMEOUT(fake.stopCount(), 1, 1000);
    }

    void statusOnlyTrafficKeepsEstablishedLinkAlive()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        QTimer statusTimer;
        statusTimer.setInterval(30);
        QObject::connect(&statusTimer, &QTimer::timeout,
                         &fake, &P2FakeRadio::sendStatus);
        statusTimer.start();

        // No further DDC arrives, but valid selected-radio status is accepted
        // all-inbound traffic and therefore refreshes the sourced watchdog.
        QTest::qWait(kEstablishedTimeoutMs * 3);
        QCOMPARE(connection.state(), ConnectionState::Connected);
        QCOMPARE(errorSpy.count(), 0);

        statusTimer.stop();
        fake.stopIngress();
        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::LinkLost, 3000);
        connection.disconnect();
    }

    void malformedAndWrongSourceTrafficDoNotRefresh()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        // Exercise the production sender-address filter with real UDP. The
        // connection socket is Any-bound, but the selected radio is IPv4
        // 127.0.0.1; valid status from IPv6 ::1 must not refresh it.
        QVERIFY2(fake.sendWrongAddressStatus(),
                 "IPv6 loopback ::1 must be available for wrong-address coverage");

        QTimer invalidTraffic;
        invalidTraffic.setInterval(25);
        QObject::connect(&invalidTraffic, &QTimer::timeout, &fake, [&fake]() {
            fake.sendMalformedStatus();
            fake.sendWrongRoleStatus();
            (void)fake.sendWrongAddressStatus();
        });
        invalidTraffic.start();

        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::LinkLost, 3000);
        invalidTraffic.stop();
        QTRY_COMPARE_WITH_TIMEOUT(fake.stopCount(), 1, 1000);
        connection.disconnect();
    }

    void disabledWidebandTrafficDoesNotRefresh()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        QTimer widebandTimer;
        widebandTimer.setInterval(25);
        QObject::connect(&widebandTimer, &QTimer::timeout,
                         &fake, [&fake]() { fake.sendWideband(0); });
        widebandTimer.start();

        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::LinkLost, 3000);
        widebandTimer.stop();
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).value<RadioConnectionError>(),
                 RadioConnectionError::NoDataTimeout);
        QTRY_COMPARE_WITH_TIMEOUT(fake.stopCount(), 1, 1000);
        connection.disconnect();
    }

    void enabledWidebandTrafficKeepsEstablishedLinkAlive()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        connection.setWidebandEnabled(0, true);

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        QTimer widebandTimer;
        widebandTimer.setInterval(25);
        QObject::connect(&widebandTimer, &QTimer::timeout,
                         &fake, [&fake]() { fake.sendWideband(0); });
        widebandTimer.start();
        QTest::qWait(kEstablishedTimeoutMs * 3);

        QCOMPARE(connection.state(), ConnectionState::Connected);
        QCOMPARE(errorSpy.count(), 0);
        widebandTimer.stop();
        fake.stopIngress();
        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::LinkLost, 3000);
        connection.disconnect();
    }

    void initialStatusDoesNotReplaceFirstIqWatchdog()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);

        QSignalSpy connectFailedSpy(&connection,
                                    &RadioConnection::connectFailed);
        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        QSignalSpy stateSpy(&connection,
                            &RadioConnection::connectionStateChanged);

        connection.connectToRadio(fake.radioInfo());
        QVERIFY(waitUntil([&fake]() { return fake.hasClient(); }));

        QTimer statusTimer;
        statusTimer.setInterval(25);
        QObject::connect(&statusTimer, &QTimer::timeout,
                         &fake, &P2FakeRadio::sendStatus);
        statusTimer.start();

        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::Disconnected, 3000);
        statusTimer.stop();
        QCOMPARE(connectFailedSpy.count(), 1);
        QCOMPARE(connectFailedSpy.at(0).at(0).value<ConnectFailure>(),
                 ConnectFailure::Timeout);
        QCOMPARE(errorSpy.count(), 0);
        for (const QList<QVariant>& transition : stateSpy) {
            QVERIFY(transition.at(0).value<ConnectionState>()
                    != ConnectionState::LinkLost);
        }
    }

    void explicitDisconnectCancelsEstablishedDeadline()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        QSignalSpy errorSpy(&connection, &RadioConnection::errorOccurred);
        QSignalSpy stateSpy(&connection,
                            &RadioConnection::connectionStateChanged);
        stateSpy.clear();

        connection.disconnect();
        QCOMPARE(connection.state(), ConnectionState::Disconnected);
        QTest::qWait(kEstablishedTimeoutMs * 2);
        QCOMPARE(errorSpy.count(), 0);
        for (const QList<QVariant>& transition : stateSpy) {
            QVERIFY(transition.at(0).value<ConnectionState>()
                    != ConnectionState::LinkLost);
        }
        QTRY_COMPARE_WITH_TIMEOUT(fake.stopCount(), 1, 1000);
    }

    void endpointReplacementRejectsOldTrafficAndOldDeadline()
    {
        P2FakeRadio oldRadio;
        QVERIFY(oldRadio.start());
        P2FakeRadio newRadio;
        QVERIFY(newRadio.start());
        QVERIFY(oldRadio.outboundPortBase() != newRadio.outboundPortBase());

        P2RadioConnection connection;
        configureConnection(connection, oldRadio);
        QVERIFY(establish(connection, oldRadio));

        // Replace the endpoint before the old generation's deadline. The
        // explicit stop closes its socket queue and advances the generation.
        QTest::qWait(kEstablishedTimeoutMs / 3);
        connection.disconnect();
        QCOMPARE(connection.state(), ConnectionState::Disconnected);
        connection.setPortBasesForTest(newRadio.outboundPortBase(),
                                       newRadio.inputRolePortBase());
        QVERIFY(establish(connection, newRadio));

        // Keep B alive beyond A's old deadline to prove the queued old wakeup
        // cannot terminate the replacement generation.
        QTimer newStatus;
        newStatus.setInterval(30);
        QObject::connect(&newStatus, &QTimer::timeout,
                         &newRadio, &P2FakeRadio::sendStatus);
        newStatus.start();
        QTest::qWait(kEstablishedTimeoutMs * 2);
        QCOMPARE(connection.state(), ConnectionState::Connected);

        // Now stop B and continuously target B's current client socket with
        // valid status from old endpoint A. Its old negotiated source-role
        // port must not map into B's replacement layout or keep B alive.
        newStatus.stop();
        newRadio.stopIngress();
        QTimer oldStatus;
        oldStatus.setInterval(25);
        QObject::connect(&oldStatus, &QTimer::timeout, &oldRadio,
                         [&oldRadio, &newRadio]() {
            oldRadio.sendStatusTo(newRadio.clientAddress(),
                                  newRadio.clientPort());
        });
        oldStatus.start();
        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::LinkLost, 3000);
        oldStatus.stop();
        QTRY_COMPARE_WITH_TIMEOUT(newRadio.stopCount(), 1, 1000);
        connection.disconnect();
    }

    void directErrorObserverCannotPoisonReplacementState()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));

        QSignalSpy stateSpy(&connection,
                            &RadioConnection::connectionStateChanged);
        stateSpy.clear();
        QObject::connect(&connection, &RadioConnection::errorOccurred,
                         &connection,
                         [&connection](RadioConnectionError error,
                                       const QString&) {
            if (error == RadioConnectionError::NoDataTimeout) {
                connection.disconnect();
            }
        }, Qt::DirectConnection);

        fake.stopIngress();
        QTRY_COMPARE_WITH_TIMEOUT(connection.state(),
                                  ConnectionState::Disconnected, 3000);
        for (const QList<QVariant>& transition : stateSpy) {
            QVERIFY(transition.at(0).value<ConnectionState>()
                    != ConnectionState::LinkLost);
        }
    }
};

QTEST_MAIN(TestP2EstablishedSilence)
#include "tst_p2_established_silence.moc"
