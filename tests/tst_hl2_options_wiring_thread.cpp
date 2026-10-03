// no-port-check: test-only. NereusSDR-original; no Thetis or mi0bot logic.
//
// =================================================================
// The HL2 options reach a P1 connection that lives on its own thread.
//
// RadioModel watches Hl2OptionsModel::changed and pushes the options to the
// connection (RadioModel::connectHl2OptionsToConnection). The options and
// RadioModel::m_connection belong to the main thread, so the watch must run
// there and only the setters cross to the connection's thread. A watch run
// on the connection's thread reads both from the wrong thread, and reads
// them whenever that thread gets to it, not when the option changed.
//
// Nothing here keys a radio: the connection is an offline test object that
// never opens a socket.
// =================================================================

#include <QtTest/QtTest>

#include <QSemaphore>
#include <QThread>

#include "core/Hl2OptionsModel.h"
#include "core/P1RadioConnection.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstHl2OptionsWiringThread : public QObject {
    Q_OBJECT
private slots:
    void changeMadeWhileConnectionThreadIsBusyStillArrives();
};

// The connection's thread is busy when the option changes, and the model
// lets go of the connection before that thread is free again (a teardown
// racing the change). The change was made while the connection was up, so
// it reaches that connection: the model read the option on the main thread
// at the change, and only the setter waited for the connection's thread.
void TstHl2OptionsWiringThread::changeMadeWhileConnectionThreadIsBusyStillArrives()
{
    RadioModel model;
    QThread worker;
    worker.start();
    auto p1 = std::make_unique<P1RadioConnection>();
    p1->moveToThread(&worker);

    model.injectConnectionForTest(p1.get());
    model.wireHl2OptionsForTest();
    QVERIFY(!model.hl2Options().swapAudioChannels());

    // Hold the connection's thread.
    QSemaphore entered;
    QSemaphore release;
    QMetaObject::invokeMethod(p1.get(), [&entered, &release]() {
        entered.release();
        release.acquire();
    });
    entered.acquire();

    model.hl2OptionsMutable().setSwapAudioChannels(true);
    model.injectConnectionForTest(nullptr);

    release.release();
    // Flush the connection's thread: a blocking call queues behind every
    // call already posted to it.
    bool swapped = false;
    QMetaObject::invokeMethod(p1.get(), [&p1, &swapped]() {
        swapped = p1->hl2SwapAudioChannelsForTest();
    }, Qt::BlockingQueuedConnection);

    // The connection goes back to this thread before it is destroyed here,
    // and the thread stops, before anything is checked.
    QMetaObject::invokeMethod(p1.get(), [&p1]() {
        p1->moveToThread(QCoreApplication::instance()->thread());
    }, Qt::BlockingQueuedConnection);
    worker.quit();
    QVERIFY(worker.wait(5000));
    QVERIFY(swapped);
}

QTEST_MAIN(TstHl2OptionsWiringThread)
#include "tst_hl2_options_wiring_thread.moc"
