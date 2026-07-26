// =================================================================
// tests/tst_p2_wideband_marshalling.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis file
//   names appear only inside source-cite comments documenting which
//   upstream line each constant comes from. No Thetis logic is ported.
//
// Phase 3F Sub-Epic I closeout. The widebandExtensionRequestedChanged
// handler that addSlice() wires called P2RadioConnection::
// setWidebandEnabled as a direct C++ call from the main / GUI thread.
// setWidebandEnabled writes m_wbEnableMask and, when connected, calls
// sendCmdGeneral(), which writes the UDP socket. RadioConnection lives
// on the connection thread that RadioModel::connectToRadio moves it to.
// So:
//
//   * m_wbEnableMask was torn — the connection thread could be
//     mid-compose of a CmdGeneral frame (byte 23 is the mask, Thetis
//     ChannelMaster/network.c:879 [v2.10.3.15]) while the GUI thread
//     rewrote it, putting a half-updated wideband enable on the wire;
//   * QUdpSocket::writeDatagram ran on a thread that owns neither the
//     socket nor its notifier.
//
// This is the third instance of the same pattern. The DDC-assignment
// instance is covered by tests/tst_p2_ddc_assignment_marshalling.cpp
// and republishAlexAdcSlices already marshalled correctly.
//
// The fix marshals through the functor overload of
// QMetaObject::invokeMethod, matching the hardwareReceiverCountChanged /
// hardwareFrequencyChanged / setAlexRxBpf / applyDdcAssignment pushes
// already in RadioModel. No qRegisterMetaType is involved: the functor
// overload packages the lambda itself, so the arguments travel as
// ordinary by-value captures rather than through the metatype system.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QThread>

#include "core/P2RadioConnection.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// CmdGeneral byte 23 is the per-ADC wideband enable bitmask; bit N is ADCN.
// From Thetis ChannelMaster/network.c:879 [v2.10.3.15]:
//   packetbuf[23] = (char)_InterlockedAnd(&prn->wb_enable, 0xff);
constexpr int kCmdGeneralWbEnableByte = 23;

// A fresh SliceModel carries chainIndex 0, so the wideband request lands
// on ADC0 and sets bit 0.
constexpr quint8 kMaskAdc0 = 0x01;

quint8 cmdGeneralWbMask(const P2RadioConnection& conn)
{
    quint8 buf[60] = {};
    conn.composeCmdGeneralForTest(buf);
    return buf[kCmdGeneralWbEnableByte];
}

} // namespace

class TestP2WidebandMarshalling : public QObject {
    Q_OBJECT

private slots:

    // The connection is moved onto a worker thread that is NOT started, so
    // its event queue cannot drain. A direct call would land immediately;
    // a queued QMetaCallEvent cannot land until the loop runs. Starting the
    // thread afterwards then proves the event was really posted, and the
    // final read is taken FROM the connection thread through a blocking
    // invoke, so it cannot race the delivery it is checking and it reports
    // the thread the mask was actually observed on.
    void the_wideband_enable_reaches_the_connection_through_its_own_thread()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2ConnWorker"));

        auto* conn = new P2RadioConnection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        const quint8 maskBefore = cmdGeneralWbMask(*conn);

        conn->moveToThread(&worker);   // worker deliberately not started yet

        RadioModel model;
        model.injectConnectionForTest(conn);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice != nullptr);
        QCOMPARE(slice->chainIndex(), 0);

        slice->setWidebandExtensionRequested(true);

        // Drain THIS thread's queue. The worker's stays untouched, so a
        // properly marshalled enable is still sitting in it.
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        // Safe to read: the object's thread is not running, so nothing else
        // can be touching it.
        const quint8 maskWhileWorkerIdle = cmdGeneralWbMask(*conn);

        worker.start();

        // Runs on the worker, after the queued setWidebandEnabled ahead of
        // it in the same FIFO. Both the value and the thread identity come
        // back from inside the connection's own thread.
        quint8   maskAfterDrain = 0;
        QThread* observedOn     = nullptr;
        QMetaObject::invokeMethod(conn, [&]() {
            observedOn     = QThread::currentThread();
            maskAfterDrain = cmdGeneralWbMask(*conn);
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
                 "setWidebandEnabled ran while the connection thread was "
                 "stopped, so RadioModel called it directly across threads: "
                 "that mutates m_wbEnableMask and (when connected) writes "
                 "the UDP socket from the GUI thread");
        QVERIFY2(observedOn == &worker,
                 "the wideband enable was not observed on the connection "
                 "thread");
        QCOMPARE(maskAfterDrain, kMaskAdc0);
    }

    // Turning the request back off has to travel the same way. An off-flip
    // that stayed direct would still tear the mask, and it is the flip that
    // stops the radio streaming wideband packets.
    void the_wideband_disable_travels_the_same_route()
    {
        QThread worker;
        worker.setObjectName(QStringLiteral("P2ConnWorker"));

        auto* conn = new P2RadioConnection();
        conn->setBoardForTest(HPSDRHW::Saturn);

        RadioModel model;
        model.injectConnectionForTest(conn);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice != nullptr);

        // Raise it while the connection is still on this thread, so the
        // starting state is unambiguous: mask == bit 0 set.
        slice->setWidebandExtensionRequested(true);
        QCOMPARE(cmdGeneralWbMask(*conn), kMaskAdc0);

        conn->moveToThread(&worker);   // worker deliberately not started yet

        slice->setWidebandExtensionRequested(false);

        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        const quint8 maskWhileWorkerIdle = cmdGeneralWbMask(*conn);

        worker.start();

        quint8 maskAfterDrain = 0xff;
        QMetaObject::invokeMethod(conn, [&]() {
            maskAfterDrain = cmdGeneralWbMask(*conn);
        }, Qt::BlockingQueuedConnection);

        model.injectConnectionForTest(nullptr);
        worker.quit();
        worker.wait();
        delete conn;

        QVERIFY2(maskWhileWorkerIdle == kMaskAdc0,
                 "the off-flip ran while the connection thread was stopped, "
                 "so it was a direct cross-thread call");
        QCOMPARE(maskAfterDrain, quint8(0));
    }
};

QTEST_MAIN(TestP2WidebandMarshalling)
#include "tst_p2_wideband_marshalling.moc"
