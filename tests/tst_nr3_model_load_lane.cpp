// =================================================================
// tests/tst_nr3_model_load_lane.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. WDSP file names appear in
// comments to say what the load touches; no upstream logic is ported.
//
// R-R3-49: the Core's NR3 model load runs on the receive lane, like every
// other receive-side WDSP call (R-R3-39). RNNRloadModel walks rnnr.c's
// global list of NR3 instances, which create_rnnr grows and reallocates
// as the lane opens channels; called from the event loop at a connect it
// read that list while the lane replaced it (ThreadSanitizer, in
// tst_receive_layout_native).
// =================================================================
#include <QtTest/QtTest>

#include <QThread>

#include <condition_variable>
#include <mutex>

#include "core/DspControlThread.h"
#include "core/ModelPaths.h"
// The loader DspAssetService calls is private; this case calls it as the
// service does.
#define private public
#include "core/dsp/DspAssetService.h"
#undef private
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

QThread* g_loadThread = nullptr;
QtMessageHandler g_previousHandler = nullptr;

void recordLoadThread(QtMsgType type, const QMessageLogContext& context,
                      const QString& message)
{
    if (message.contains(QLatin1String("NR3: loading rnnoise model"))) {
        g_loadThread = QThread::currentThread();
    }
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

} // namespace

class TstNr3ModelLoadLane : public QObject {
    Q_OBJECT
private slots:
    void theNr3ModelLoadsOnTheReceiveLaneAfterItsQueuedWork()
    {
        // RNNRloadModel needs a real model file (rnnoise fopen()s it).
        const QString modelPath = ModelPaths::rnnoiseDefaultSmallBin();
        if (modelPath.isEmpty()) {
            QSKIP("no bundled NR3 model found next to this build");
        }
        RadioModel model;
        DspControlThread* lane = model.receiveLane();
        QVERIFY(lane);
        QVERIFY(model.dspAssets()->m_nr3Loader);

        // Hold the lane with a job, as a channel open queued at a connect
        // would be: a load that belongs on the lane cannot run before it.
        std::mutex mutex;
        std::condition_variable changed;
        bool held = false;
        bool released = false;
        lane->post([&]() {
            std::unique_lock<std::mutex> lock(mutex);
            held = true;
            changed.notify_all();
            changed.wait(lock, [&]() { return released; });
        });
        {
            std::unique_lock<std::mutex> lock(mutex);
            QVERIFY(changed.wait_for(lock, std::chrono::seconds(30),
                                     [&]() { return held; }));
        }

        g_loadThread = nullptr;
        g_previousHandler = qInstallMessageHandler(recordLoadThread);
        model.dspAssets()->m_nr3Loader(modelPath);
        QThread* const beforeRelease = g_loadThread;
        {
            std::lock_guard<std::mutex> lock(mutex);
            released = true;
        }
        changed.notify_all();
        const bool idle = lane->waitIdleForTest(30000);
        qInstallMessageHandler(g_previousHandler);

        QVERIFY2(beforeRelease == nullptr,
                 "the NR3 model loaded on the event loop, ahead of the lane's queued work");
        QVERIFY(idle);
        QVERIFY(g_loadThread != nullptr);
        QVERIFY(g_loadThread != QThread::currentThread());
    }
};

QTEST_MAIN(TstNr3ModelLoadLane)
#include "tst_nr3_model_load_lane.moc"
