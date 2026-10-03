// =================================================================
// tests/tst_p2_ddc_assignment_marshalling.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis file
//   names appear only inside source-cite comments documenting which
//   upstream line each constant comes from. No Thetis logic is ported.
//
// Phase 3F Sub-Epic I closeout. RadioModel::invokeCodecDdcAssignment
// called P2RadioConnection::applyDdcAssignment as a direct C++ call from
// the main / GUI thread. applyDdcAssignment rewrites m_rx[] and calls
// sendCmdRx(), which writes the UDP socket, and RadioConnection lives on
// the connection thread that RadioModel::connectToRadio moves it to. So:
//
//   * m_rx[] was torn — the connection thread could be mid-compose of a
//     CmdRx or CmdHighPriority frame while the GUI thread rewrote enable
//     bits and rates, producing a frame with a half-updated mask;
//   * QUdpSocket::writeDatagram ran on a thread that owns neither the
//     socket nor its notifier.
//
// requestDdcAssignment is wired to every slice's frequencyChanged, so
// this fired on every VFO tick of every connected P2 radio.
//
// The fix marshals through the functor overload of
// QMetaObject::invokeMethod, matching the hardwareReceiverCountChanged /
// hardwareFrequencyChanged / setAlexRxBpf pushes already in RadioModel.
// No qRegisterMetaType is involved: the functor overload packages the
// lambda itself, so DdcAssignment travels as an ordinary by-value
// capture rather than through the metatype system.
//
// The mask-ownership defect in the same publish path is covered by
// tests/tst_p2_ddc_mask_ownership.cpp.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QThread>

#include "core/P2RadioConnection.h"
#include "core/ReceiverManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Saturn (ANAN-G2) reserves DDC0 / DDC1 for the PureSignal or diversity
// sync pair and puts slice A on DDC2. From Thetis console.cs:8244-8245
// [v2.10.3.15] (DDCEnable = DDC2) with the bit values at console.cs:8199
// [v2.10.3.15]: int DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
constexpr quint8 kMaskDdc2 = 0x04;

// CmdRx byte 7 is the enable bitmask.
// From Thetis ChannelMaster/network.c:1097-1103 [v2.10.3.15]:
//   packetbuf[7] = (prn->rx[6].enable << 6 | ... | prn->rx[0].enable) & 0xff;
constexpr int kCmdRxEnableByte = 7;
constexpr int kCmdRxFirstDdcByte = 17;
constexpr int kCmdRxDdcStride = 6;

quint8 cmdRxEnableMask(const P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdRxForTest(buf);
    return buf[kCmdRxEnableByte];
}

int cmdRxRateKhz(const P2RadioConnection& conn, int ddc)
{
    quint8 buf[1444] = {};
    conn.composeCmdRxForTest(buf);
    const int base = kCmdRxFirstDdcByte + (ddc * kCmdRxDdcStride);
    return (static_cast<int>(buf[base + 1]) << 8)
        | static_cast<int>(buf[base + 2]);
}

// P2RadioConnection::setState is protected. Exposing it from a test-local
// subclass lets the regression keep the model and the real connection in the
// same lifecycle state without adding a production seam; the qobject_cast in
// invokeCodecDdcAssignment still matches, because this IS a P2RadioConnection.
class TestableP2Connection : public P2RadioConnection {
    Q_OBJECT
public:
    using P2RadioConnection::P2RadioConnection;
    void markConnectingForTest() { setState(ConnectionState::Connecting); }
    void markConnectedForTest() { setState(ConnectionState::Connected); }
    void markDisconnectedForTest() { setState(ConnectionState::Disconnected); }
};

} // namespace

class TestP2DdcAssignmentMarshalling : public QObject {
    Q_OBJECT

private slots:

    // The connection is moved onto a worker thread that is NOT started, so
    // its event queue cannot drain. A direct call would land immediately;
    // a queued QMetaCallEvent cannot land until the loop runs. Starting the
    // thread afterwards then proves the event was really posted, and the
    // final read is taken FROM the connection thread through a blocking
    // invoke, so it cannot race the delivery it is checking and it reports
    // the thread the assignment was actually observed on.
    void the_assignment_reaches_the_connection_through_its_own_thread()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2ConnWorker"));

        auto* conn = new TestableP2Connection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        conn->markConnectedForTest();
        const quint8 maskBefore = cmdRxEnableMask(*conn);

        conn->moveToThread(&worker);   // worker deliberately not started yet

        RadioModel model;
        model.injectConnectionForTest(conn);

        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        // connectToRadio creates one ReceiverManager receiver per stream
        // after sizing the pool; publishDdcAssignment needs them to route
        // onto.
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);

        // Drain THIS thread's queue. The worker's stays untouched, so a
        // properly marshalled assignment is still sitting in it.
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        // Safe to read: the object's thread is not running, so nothing else
        // can be touching it.
        const quint8 maskWhileWorkerIdle = cmdRxEnableMask(*conn);

        worker.start();

        // Runs on the worker, after the queued applyDdcAssignment ahead of
        // it in the same FIFO. Both the value and the thread identity come
        // back from inside the connection's own thread.
        quint8   maskAfterDrain = 0;
        QThread* observedOn     = nullptr;
        QMetaObject::invokeMethod(conn, [&]() {
            observedOn     = QThread::currentThread();
            maskAfterDrain = cmdRxEnableMask(*conn);
        }, Qt::BlockingQueuedConnection);

        // Tear the worker down before asserting, so a failure cannot leave
        // a running thread for QThread's destructor to trip over, and the
        // model cannot outlive its injected connection.
        model.injectConnectionForTest(nullptr);
        worker.quit();
        worker.wait();
        delete conn;

        QCOMPARE(maskBefore, quint8(0));
        QVERIFY2(maskWhileWorkerIdle == quint8(0),
                 "applyDdcAssignment ran while the connection thread was "
                 "stopped, so RadioModel called it directly across threads: "
                 "that mutates m_rx[] and writes the UDP socket from the GUI "
                 "thread on every VFO tick");
        QVERIFY2(observedOn == &worker,
                 "the assignment was not observed on the connection thread");
        QCOMPARE(maskAfterDrain, kMaskDdc2);
    }

    // A P2 codec becomes usable from inside the connection-thread
    // connectToRadio() call, before the first I/Q packet can promote the model
    // to Connected. The complete codec assignment must be allowed to replace
    // the bootstrap DDC geometry during that Connecting window. Otherwise the
    // sole Saturn slice can leave DDC2 at its 48 kHz constructor rate while
    // the allocator and WDSP wait for 192 kHz input. A later slice add or
    // retune used to hide that gap by issuing another assignment request.
    void a_connecting_single_slice_publishes_its_rate_to_the_primary_ddc()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2ConnectingWorker"));

        auto* conn = new TestableP2Connection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        conn->markConnectingForTest();
        conn->moveToThread(&worker);   // hold the queued assignment for inspection

        RadioModel model;
        model.injectConnectionForTest(conn);
        // Reproduce signal order: P2 has already selected the codec and moved
        // its own atomic state to Connecting, while RadioModel still carries
        // the prior cached state until the queued lifecycle signal is handled.
        model.setConnectionStateForTest(ConnectionState::Probing);
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int sliceId = model.addSlice();
        QCOMPARE(sliceId, 0);
        QCOMPARE(model.slices().size(), 1);

        // The connection thread is stopped, so the composed packet must still
        // show constructor state until its own queue drains.
        QCOMPARE(cmdRxEnableMask(*conn), quint8(0));
        QCOMPARE(cmdRxRateKhz(*conn, /*DDC2*/ 2), 48);

        model.onConnectionStateChangedForTest(ConnectionState::Connecting);
        worker.start();

        quint8 maskAfterDrain = 0;
        int ddc2RateAfterDrainKhz = 0;
        QMetaObject::invokeMethod(conn, [&]() {
            maskAfterDrain = cmdRxEnableMask(*conn);
            ddc2RateAfterDrainKhz = cmdRxRateKhz(*conn, /*DDC2*/ 2);
        }, Qt::BlockingQueuedConnection);

        model.injectConnectionForTest(nullptr);
        worker.quit();
        worker.wait();
        delete conn;

        QCOMPARE(maskAfterDrain, kMaskDdc2);
        QCOMPARE(ddc2RateAfterDrainKhz, 192);
    }

    void a_disconnected_model_does_not_queue_a_ddc_wire_write()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2DisconnectedWorker"));

        auto* conn = new TestableP2Connection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        conn->moveToThread(&worker);

        RadioModel model;
        model.injectConnectionForTest(conn);
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }
        QCOMPARE(model.addSlice(), 0);

        worker.start();

        quint8 maskAfterDrain = 0xff;
        int ddc2RateAfterDrainKhz = 0;
        QMetaObject::invokeMethod(conn, [&]() {
            maskAfterDrain = cmdRxEnableMask(*conn);
            ddc2RateAfterDrainKhz = cmdRxRateKhz(*conn, /*DDC2*/ 2);
        }, Qt::BlockingQueuedConnection);

        model.injectConnectionForTest(nullptr);
        worker.quit();
        worker.wait();
        delete conn;

        QCOMPARE(maskAfterDrain, quint8(0));
        QCOMPARE(ddc2RateAfterDrainKhz, 48);
    }

    void a_connected_transition_retries_an_assignment_suppressed_before_readiness()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2ConnectedConvergenceWorker"));

        auto* conn = new TestableP2Connection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        conn->markConnectedForTest();
        conn->moveToThread(&worker);

        RadioModel model;
        model.injectConnectionForTest(conn);
        model.setConnectionStateForTest(ConnectionState::Probing);
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(cmdRxEnableMask(*conn), quint8(0));

        // This production transition owns the idempotent convergence retry;
        // no second slice, frequency write or explicit assignment request is
        // needed to repair the bootstrap packet geometry.
        model.onConnectionStateChangedForTest(ConnectionState::Connected);
        worker.start();

        quint8 maskAfterDrain = 0;
        int ddc2RateAfterDrainKhz = 0;
        QMetaObject::invokeMethod(conn, [&]() {
            maskAfterDrain = cmdRxEnableMask(*conn);
            ddc2RateAfterDrainKhz = cmdRxRateKhz(*conn, /*DDC2*/ 2);
        }, Qt::BlockingQueuedConnection);

        model.injectConnectionForTest(nullptr);
        worker.quit();
        worker.wait();
        delete conn;

        QCOMPARE(maskAfterDrain, kMaskDdc2);
        QCOMPARE(ddc2RateAfterDrainKhz, 192);
    }

    void a_queued_disconnect_supersedes_an_admitted_connecting_assignment()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2QueuedDisconnectWorker"));

        auto* conn = new TestableP2Connection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        conn->markConnectingForTest();
        conn->moveToThread(&worker);

        RadioModel model;
        model.injectConnectionForTest(conn);
        model.setConnectionStateForTest(ConnectionState::Connecting);
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        // The connection owns the final admission decision. Queue its
        // disconnect first, then let the cached Connecting model admit an
        // assignment behind it. The worker must discard that stale delivery.
        QMetaObject::invokeMethod(conn, [conn]() {
            conn->markDisconnectedForTest();
        });
        QCOMPARE(model.addSlice(), 0);

        worker.start();

        quint8 maskAfterDrain = 0xff;
        int ddc2RateAfterDrainKhz = 0;
        QMetaObject::invokeMethod(conn, [&]() {
            maskAfterDrain = cmdRxEnableMask(*conn);
            ddc2RateAfterDrainKhz = cmdRxRateKhz(*conn, /*DDC2*/ 2);
        }, Qt::BlockingQueuedConnection);

        model.injectConnectionForTest(nullptr);
        worker.quit();
        worker.wait();
        delete conn;

        QCOMPARE(maskAfterDrain, quint8(0));
        QCOMPARE(ddc2RateAfterDrainKhz, 48);
    }
};

QTEST_MAIN(TestP2DdcAssignmentMarshalling)
#include "tst_p2_ddc_assignment_marshalling.moc"
