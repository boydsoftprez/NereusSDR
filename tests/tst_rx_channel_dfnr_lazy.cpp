// =================================================================
// tests/tst_rx_channel_dfnr_lazy.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. DeepFilterNet3 (DFNR) is an
// AetherSDR-native filter with no Thetis equivalent.
//
// R-R3-39, Sub-epic C-1: an RxChannel loads the DeepFilterNet3 model only
// when that channel first picks DFNR, on its receive lane, and never again.
// A connect opens five receive channels; loading the 8 MB model in each
// constructor cost about 250 ms per channel on every connect in any build
// that ships the model.
//
// Model loads are counted from DeepFilterFilter's own "loading model" debug
// line (lcDsp), so the count needs no production hook. The channels here
// have no WDSP channel open: the NR selection's WDSP calls check the channel
// first (nnr_channel_valid) and do nothing, which leaves the DFNR path alone
// under test.
// =================================================================
#include <QtTest/QtTest>

#include <QLoggingCategory>
#include <QSignalSpy>
#include <QMutex>
#include <QMutexLocker>

#include <atomic>
#include <memory>
#include <vector>

#include "core/DspControlThread.h"
#include "core/ModelPaths.h"
#include "core/RxChannel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

namespace {

constexpr int kLaneIdleTimeoutMs = 10000;
constexpr int kInSize = 64;
constexpr int kRateHz = 48000;
constexpr int kConnectChannels = 5;

// Counts DeepFilterFilter model loads and records whether each ran on the
// receive lane.
struct LoadLog {
    QMutex mutex;
    int loads{0};
    int loadsOffLane{0};
    DspControlThread* lane{nullptr};
    QtMessageHandler previous{nullptr};
};

LoadLog* g_log = nullptr;

void countLoads(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (g_log != nullptr && message.contains(QLatin1String("DeepFilterFilter: loading model"))) {
        QMutexLocker lock(&g_log->mutex);
        ++g_log->loads;
        if (g_log->lane == nullptr || !g_log->lane->isCurrentThread()) {
            ++g_log->loadsOffLane;
        }
        return;
    }
    if (g_log != nullptr && g_log->previous != nullptr) {
        g_log->previous(type, context, message);
    }
}

} // namespace

class TestRxChannelDfnrLazy : public QObject {
    Q_OBJECT

private:
    LoadLog m_log;

    int loads()
    {
        QMutexLocker lock(&m_log.mutex);
        return m_log.loads;
    }
    int loadsOffLane()
    {
        QMutexLocker lock(&m_log.mutex);
        return m_log.loadsOffLane;
    }
    void resetCounts(DspControlThread* lane)
    {
        QMutexLocker lock(&m_log.mutex);
        m_log.loads = 0;
        m_log.loadsOffLane = 0;
        m_log.lane = lane;
    }

private slots:
    void initTestCase()
    {
#ifndef HAVE_DFNR
        QSKIP("DFNR is not built in this configuration.");
#endif
        QVERIFY2(!ModelPaths::dfnrModelTarball().isEmpty(),
                 "the DeepFilterNet3 model must be reachable for this test");
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp.debug=true"));
        g_log = &m_log;
        m_log.previous = qInstallMessageHandler(countLoads);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(m_log.previous);
        g_log = nullptr;
    }

    // A connect's five channels load no model.
    void fiveChannelsLoadNoModel()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        resetCounts(&lane);
        std::vector<std::unique_ptr<RxChannel>> channels;
        for (int id = 0; id < kConnectChannels; ++id) {
            channels.push_back(std::make_unique<RxChannel>(id, kInSize, kRateHz, &lane, nullptr));
        }
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QCOMPARE(loads(), 0);
        for (const auto& ch : channels) {
            QVERIFY(!ch->dfnrActive());
            QVERIFY(!ch->dfnrLoaded());
        }
        channels.clear();
        lane.stop();
    }

    // The first DFNR selection loads exactly one model, on the receive lane,
    // for that channel alone; reselecting and switching filters loads none.
    void firstSelectionLoadsOnceOnTheLane()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        resetCounts(&lane);
        std::vector<std::unique_ptr<RxChannel>> channels;
        for (int id = 0; id < kConnectChannels; ++id) {
            channels.push_back(std::make_unique<RxChannel>(id, kInSize, kRateHz, &lane, nullptr));
        }
        RxChannel* picked = channels[2].get();

        QVERIFY(picked->setActiveNr(NrSlot::DFNR));
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QCOMPARE(loads(), 1);
        QCOMPARE(loadsOffLane(), 0);
        QCOMPARE(picked->activeNr(), NrSlot::DFNR);
        QVERIFY(picked->dfnrActive());
        QVERIFY(picked->dfnrLoaded());

        const NrSlot cycle[] = {NrSlot::NR1, NrSlot::DFNR, NrSlot::NR2, NrSlot::Off,
                                NrSlot::DFNR, NrSlot::MNR, NrSlot::DFNR};
        for (NrSlot slot : cycle) {
            QVERIFY(picked->setActiveNr(slot));
        }
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QCOMPARE(loads(), 1);
        QVERIFY(picked->dfnrActive());

        QVERIFY(picked->setActiveNr(NrSlot::Off));
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QVERIFY(!picked->dfnrActive());

        for (int id = 0; id < kConnectChannels; ++id) {
            if (id != 2) {
                QVERIFY(!channels[static_cast<std::size_t>(id)]->dfnrActive());
                QVERIFY(!channels[static_cast<std::size_t>(id)]->dfnrLoaded());
            }
        }
        QCOMPARE(loads(), 1);
        channels.clear();
        lane.stop();
    }

    // With no lane (the pre-lane path tests and tools use) the selection
    // builds the instance at once, still only once.
    void noLaneBuildsAtOnce()
    {
        resetCounts(nullptr);
        RxChannel ch(0, kInSize, kRateHz);
        QCOMPARE(loads(), 0);
        QVERIFY(ch.setActiveNr(NrSlot::DFNR));
        QCOMPARE(loads(), 1);
        QVERIFY(ch.dfnrActive());
        QVERIFY(ch.setActiveNr(NrSlot::NR2));
        QVERIFY(!ch.dfnrActive());
        QVERIFY(ch.setActiveNr(NrSlot::DFNR));
        QCOMPARE(loads(), 1);
        QVERIFY(ch.dfnrActive());
        QVERIFY(ch.dfnrLoaded());
    }

    // Availability comes from the build and ModelPaths, with no load.
    void availabilityLoadsNothing()
    {
        resetCounts(nullptr);
        QVERIFY(RxChannel::dfnrAvailable());
        QCOMPARE(loads(), 0);
    }

    // A missing model leaves DFNR unavailable: selecting it loads nothing,
    // DFNR stays off on the channel, the selection itself stands (as a
    // missing model showed before), and tuning still takes values.
    void missingModelLoadsNothing()
    {
        ModelPaths::setDfnrModelTarballForTest(QString());
        DspControlThread lane(DspLane::Receive);
        lane.start();
        resetCounts(&lane);
        QVERIFY(!RxChannel::dfnrAvailable());
        {
            RxChannel ch(0, kInSize, kRateHz, &lane, nullptr);
            // R-R3-49: the first selection reports that DFNR cannot run
            // (the model is missing), once.
            QSignalSpy unavailable(&ch, &RxChannel::dfnrUnavailable);
#ifdef HAVE_DFNR
            // Trunk merge: the tuning setters exist only in a DFNR build
            // (RxChannel.h); the whole test skips without it.
            ch.setDfnrAttenLimit(40.0f);
#endif
            QVERIFY(ch.setActiveNr(NrSlot::DFNR));
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QCOMPARE(ch.activeNr(), NrSlot::DFNR);
            QVERIFY(!ch.dfnrActive());
            QVERIFY(!ch.dfnrLoaded());
            // Not retried on a later selection.
            QVERIFY(ch.setActiveNr(NrSlot::NR1));
            QVERIFY(ch.setActiveNr(NrSlot::DFNR));
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QVERIFY(!ch.dfnrActive());
#ifdef HAVE_DFNR
            QCOMPARE(unavailable.count(), 1);
            QCOMPARE(unavailable.at(0).at(0).toBool(), true);
#else
            QCOMPARE(unavailable.count(), 0);
#endif
        }
        QCOMPARE(loads(), 0);
        ModelPaths::clearDfnrModelTarballForTest();
        lane.stop();
    }
};

QTEST_GUILESS_MAIN(TestRxChannelDfnrLazy)
#include "tst_rx_channel_dfnr_lazy.moc"
