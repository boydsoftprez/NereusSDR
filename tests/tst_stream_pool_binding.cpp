// =================================================================
// tests/tst_stream_pool_binding.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Tasks 5-6: stream pool + slice binding.
// Phase 3F Sub-Epic I Task 7b: per-stream DDC assignment + routing.
// Phase 3F Sub-Epic I closeout, defect F1: bindings reach a late worker.
// 2026-09-27: R-R3-49: a Protocol 2 slice comes back at the rate saved for
// its band after a restart, and a rate change saves it. J.J. Boyd (KG4VCF),
// with AI-assisted implementation via Anthropic Claude Code.
// 2026-09-28: a stream rate change never makes the event loop wait on a busy
// DSP worker, and the new drain size still lands before the next batch.
// J.J. Boyd (KG4VCF), with AI-assisted implementation via Anthropic Claude
// Code.
// =================================================================
#include <QtTest/QtTest>
#include <QRegularExpression>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>
#include "core/AppSettings.h"
#include "core/DdcAssignment.h"
#include "core/RadioDiscovery.h"
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/codec/P1CodecAnvelinaPro3.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecHermes.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/RadioModel.h"
#include "models/RxDspWorker.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// One in_size chunk of interleaved I/Q, enough to make RxDspWorker drain
// and fan the chunk out to whatever slices it believes are on the stream.
QVector<float> oneChunk(int inSize)
{
    QVector<float> v;
    v.reserve(inSize * 2);
    for (int i = 0; i < inSize; ++i) {
        v.append(0.1f);
        v.append(0.2f);
    }
    return v;
}

// Slice indices the worker actually fanned a stream-`st` chunk out to.
// This reads the worker's real binding map through its production drain
// path rather than a test-only accessor, so it cannot pass on a map the
// DSP thread would never consult.
QVector<int> workerBindingsFor(RxDspWorker& w, int st, int inSize)
{
    QSignalSpy spy(&w, &RxDspWorker::sliceProcessed);
    w.processIqBatch(st, oneChunk(inSize));
    QVector<int> out;
    for (int i = 0; i < spy.count(); ++i) {
        out.append(spy.at(i).at(0).toInt());
    }
    return out;
}

// Detaches a stack-injected RadioConnection on scope exit, however the scope
// is left. QCOMPARE / QVERIFY return from the enclosing slot on failure, so a
// trailing injectConnectionForTest(nullptr) is skipped on exactly the run
// where it matters most.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// The loudest output sample a running channel produces from a steady tone,
// fed straight to RxChannel::processIq (the call RxDspWorker makes) at
// `rateHz`: `settleBlocks` input blocks first, then the peak over the next
// `measureBlocks`. Only the settled blocks count, because a channel WDSP is
// about to silence still plays out its slew-down first. fexchange2 leaves the
// output untouched when the channel's exchange flag is clear, so each block
// starts from zeros: a channel WDSP has silenced reads 0.
double settledPeakFromATone(RxChannel* rx, int rateHz,
                            int settleBlocks = 100, int measureBlocks = 50)
{
    const int inSize = bufferSizeForRate(rateHz);
    const int outSize = inSize * 48000 / rateHz;
    std::vector<float> inI(inSize), inQ(inSize, 0.0f), outI(inSize), outQ(inSize);
    double peak = 0.0;
    long n = 0;
    for (int block = 0; block < settleBlocks + measureBlocks; ++block) {
        for (int i = 0; i < inSize; ++i, ++n) {
            // A real-valued 1 kHz tone lands in both sidebands, so the test
            // does not depend on the slice's mode.
            inI[i] = static_cast<float>(
                0.01 * std::cos(2.0 * std::numbers::pi * 1000.0
                                * static_cast<double>(n) / rateHz));
        }
        std::fill(outI.begin(), outI.end(), 0.0f);
        std::fill(outQ.begin(), outQ.end(), 0.0f);
        rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), inSize, outSize);
        if (block < settleBlocks) {
            continue;
        }
        for (int i = 0; i < outSize; ++i) {
            peak = std::max(peak, std::abs(static_cast<double>(outI[i])));
        }
    }
    return peak;
}

} // namespace

namespace {

QString savedRateKey(int sliceId)
{
    return QStringLiteral("Slice%1/Band20m/SampleRate").arg(sliceId);
}

// The codec is declared first so it outlives the model that points at it.
struct RestartedP2 {
    P2CodecSaturn codec;
    RadioModel model;
    WdspEngine* engine {nullptr};
    int id {-1};
};

} // namespace

class TestStreamPoolBinding : public QObject {
    Q_OBJECT
private:
    static void restartOnP2(RestartedP2& r, int savedRateHz)
    {
        r.model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        r.model.receiverManager()->setP2Codec(&r.codec);
        r.engine = r.model.wdspEngine();
        r.engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        r.model.configureStreamPool(4, 4, 192000);
        for (int st = 0; st < 4; ++st) {
            r.model.receiverManager()->createReceiver();
        }
        r.model.openRxChannelPool(4, bufferSizeForRate(192000), 192000);
        r.id = r.model.addSlice();
        r.model.sliceById(r.id)->setFrequency(14200000.0);
        QVERIFY(r.model.waitForReceiveLaneForTest());
        AppSettings::instance().remove(savedRateKey(r.id));
        if (savedRateHz > 0) {
            AppSettings::instance().setValue(savedRateKey(r.id), savedRateHz);
        }
    }

private slots:
    void pool_sizes_to_the_sku()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        QCOMPARE(model.streamPoolSize(), 5);
    }

    void first_slice_activates_stream_zero()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int idx = model.addSlice();
        SliceModel* s = model.slices().at(idx);

        QCOMPARE(s->streamIndex(), 0);
        QCOMPARE(s->shiftOffsetHz(), 0.0);
    }

    void same_band_slices_share_one_stream()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);

        QCOMPARE(model.slices().at(b)->streamIndex(), 0);
        QCOMPARE(model.slices().at(b)->shiftOffsetHz(), 25000.0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void cross_band_slices_take_separate_streams()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(model.slices().at(b)->streamIndex() != 0);
        QCOMPARE(model.activeStreamCount(), 2);
    }

    void four_slices_fit_one_ddc_on_one_band()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const double base = 14200000.0;
        for (double off : {0.0, -40000.0, 25000.0, 60000.0}) {
            const int i = model.addSlice();
            model.slices().at(i)->setFrequency(base + off);
        }

        QCOMPARE(model.activeStreamCount(), 1);
        for (SliceModel* s : model.slices()) {
            QCOMPARE(s->streamIndex(), 0);
        }
    }

    void exhausting_ddcs_rejects_with_a_reason()
    {
        RadioModel model;
        // One DDC, room for several slices: the second slice on a
        // different band has nowhere to go.
        model.configureStreamPool(/*userDdcCount*/ 1, /*maxSlices*/ 5, 192000);

        // Phase 3F Sub-Epic I closeout, defect F4: this spied
        // sliceAddRejected, but nothing here adds a slice that fails -- B is
        // seeded onto A's band and binds fine, then the setFrequency below
        // RETUNES it off the only DDC. sliceRetuneRejected is the signal for
        // that, and it is what the operator's status bar now shows.
        QSignalSpy spy(&model, &RadioModel::sliceRetuneRejected);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.at(0).at(0).toInt(), model.slices().at(b)->sliceIndex());
        QVERIFY(!spy.at(0).at(1).toString().isEmpty());
    }

    // ── Phase 3F Sub-Epic I Task 7b ─────────────────────────────────────
    //
    // Test seam: the codec is injected through ReceiverManager's existing
    // public setP2Codec(), which is exactly where connectToRadio puts it
    // (RadioModel.cpp p2CodecChanged wiring). No socket, no fake
    // connection, and the assertions still run the production path:
    // addSlice/setFrequency -> bindSliceToStream -> requestDdcAssignment
    // -> invokeCodecDdcAssignment -> publishDdcAssignment.

    void co_hosted_slices_share_one_ddc()
    {
        RadioModel model;
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);

        // Both slices are on stream 0, so both must report the SAME DDC.
        // Indexing the codec by slice would hand them DDC2 and DDC3.
        QCOMPARE(model.slices().at(a)->streamIndex(),
                 model.slices().at(b)->streamIndex());
        QCOMPARE(model.slices().at(a)->ddcIndex(),
                 model.slices().at(b)->ddcIndex());

        // ...and it has to be a real DDC. Both would trivially agree on the
        // -1 sentinel if the publish path never ran at all.
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void each_active_stream_routes_its_ddc_to_its_receiver()
    {
        RadioModel model;
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(5, 5, 192000);

        // One ReceiverManager receiver per stream, mirroring the loop
        // connectToRadio runs after configureStreamPool. Without them
        // setDdcMapping has no receiver to route onto.
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        // Two streams, two distinct DDCs, and each stream's logical receiver
        // must carry the DDC the codec chose or its packets get dropped by
        // ReceiverManager::feedIqData.
        const int ddcA = model.slices().at(a)->ddcIndex();
        const int ddcB = model.slices().at(b)->ddcIndex();
        QVERIFY(ddcA >= 0);
        QVERIFY(ddcB >= 0);
        QVERIFY(ddcA != ddcB);
        QCOMPARE(model.ddcForStream(0), ddcA);
        QCOMPARE(model.ddcForStream(1), ddcB);

        // Defect 1: ReceiverManager::ddcIndex() reports the resolved
        // hardwareRx, which is what feedIqData keys m_hwToLogical on. Before
        // Task 7b receiver 1 fell through rebuildHardwareMapping's
        // nextAutoHw fallback to DDC0 (reserved for the PureSignal /
        // diversity pair) while the codec enabled DDC3.
        QCOMPARE(model.receiverManager()->ddcIndex(0), ddcA);
        QCOMPARE(model.receiverManager()->ddcIndex(1), ddcB);

        // Streams 2..4 are idle: no DDC, and no stale routing left behind.
        QCOMPARE(model.ddcForStream(2), -1);
        QCOMPARE(model.receiverManager()->ddcIndex(2), -1);
    }

    void published_assignment_updates_slice_and_receiver_physical_routing()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(5, 5, 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);
        const int c = model.addSlice();
        model.slices().at(c)->setFrequency(7150000.0);

        SliceModel* sliceA = model.slices().at(a);
        SliceModel* sliceB = model.slices().at(b);
        SliceModel* sliceC = model.slices().at(c);
        const int streamAB = sliceA->streamIndex();
        const int streamC = sliceC->streamIndex();
        QCOMPARE(sliceB->streamIndex(), streamAB);
        QVERIFY(streamAB >= 0);
        QVERIFY(streamC >= 0);
        QVERIFY(streamAB != streamC);

        // First publication: co-hosted A/B use DDC2 on ADC/chain 0 while C
        // uses DDC4 on ADC/chain 1. DDC4 exercises adcCtrl2 as well as the
        // low-byte route used by the first stream.
        DdcAssignment first{};
        first.streamDdc[streamAB] = 2;
        first.streamDdc[streamC] = 4;
        first.ddcEnable = (1 << 2) | (1 << 4);
        first.adcCtrl2 = 0x01; // DDC4 -> ADC1
        model.publishDdcAssignmentForTest(first);

        QCOMPARE(sliceA->ddcIndex(), 2);
        QCOMPARE(sliceB->ddcIndex(), 2);
        QCOMPARE(sliceC->ddcIndex(), 4);
        QCOMPARE(sliceA->chainIndex(), 0);
        QCOMPARE(sliceB->chainIndex(), 0);
        QCOMPARE(sliceC->chainIndex(), 1);
        QCOMPARE(model.receiverManager()->receiverConfig(streamAB).ddcIndex, 2);
        QCOMPARE(model.receiverManager()->receiverConfig(streamAB).adcIndex, 0);
        QCOMPARE(model.receiverManager()->receiverConfig(streamC).ddcIndex, 4);
        QCOMPARE(model.receiverManager()->receiverConfig(streamC).adcIndex, 1);

        // Republish a different complete physical map. Both the receiver
        // routing mirror and every hosted slice must move together.
        DdcAssignment second{};
        second.streamDdc[streamAB] = 3;
        second.streamDdc[streamC] = 5;
        second.ddcEnable = (1 << 3) | (1 << 5);
        second.adcCtrl1 = (1 << (3 * 2)); // DDC3 -> ADC1
        model.publishDdcAssignmentForTest(second);

        QCOMPARE(sliceA->ddcIndex(), 3);
        QCOMPARE(sliceB->ddcIndex(), 3);
        QCOMPARE(sliceC->ddcIndex(), 5);
        QCOMPARE(sliceA->chainIndex(), 1);
        QCOMPARE(sliceB->chainIndex(), 1);
        QCOMPARE(sliceC->chainIndex(), 0);
        QCOMPARE(model.receiverManager()->receiverConfig(streamAB).ddcIndex, 3);
        QCOMPARE(model.receiverManager()->receiverConfig(streamAB).adcIndex, 1);
        QCOMPARE(model.receiverManager()->receiverConfig(streamC).ddcIndex, 5);
        QCOMPARE(model.receiverManager()->receiverConfig(streamC).adcIndex, 0);
    }

    // Plan Task 11: one stream count, from the board row and the protocol
    // in use (BoardCapsTable::userDdcCountFor). Four on Protocol 1, the row
    // on Protocol 2; before a radio is chosen, the row's own protocol.
    void user_stream_count_follows_the_protocol()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        QCOMPARE(model.userStreamCount(), 5);  // row protocol: Protocol 2

        RadioInfo info;
        info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
        info.boardType  = HPSDRHW::OrionMKII;
        info.protocol   = ProtocolVersion::Protocol1;
        model.setLastRadioInfoForTest(info);
        QCOMPARE(model.userStreamCount(), 4);

        info.protocol = ProtocolVersion::Protocol2;
        model.setLastRadioInfoForTest(info);
        QCOMPARE(model.userStreamCount(), 5);

        RadioModel hl2;
        hl2.setBoardForTest(HPSDRHW::HermesLite);
        info.boardType = HPSDRHW::HermesLite;
        info.protocol  = ProtocolVersion::Protocol1;
        hl2.setLastRadioInfoForTest(info);
        QCOMPARE(hl2.userStreamCount(), 2);
    }

    // Plan Task 11: Protocol 1 routes each stream by the codec's FRAME SLOT
    // (RadioModel::publishDdcAssignment no longer excludes Protocol 1).
    //
    // Issue #263 guard: AnvelinaPro3 and RedPitaya used to publish stream 0
    // on "DDC2", and routing by that dropped every EP6 packet. They now
    // publish slot 0 for slice A and slot 2 for slice B (Thetis GetDDC,
    // Protocol 1 OrionMKII: rx1 = 0; rx2 = 2), and each slice gets the
    // packets of its own slot. Slot 1 carries slice A's frequency on these
    // boards (bank 3, nddc 5) and must reach no receiver.
    void protocol1_routes_by_frame_slot_data()
    {
        QTest::addColumn<QString>("codecName");
        QTest::addColumn<int>("streams");
        QTest::addColumn<int>("slotA");
        QTest::addColumn<int>("slotB");
        QTest::addColumn<int>("deadSlot");
        QTest::newRow("AnvelinaPro3") << QStringLiteral("ap3")  << 4 << 0 << 2 << 1;
        QTest::newRow("RedPitaya")    << QStringLiteral("rp")   << 4 << 0 << 2 << 1;
        QTest::newRow("Hermes")       << QStringLiteral("std")  << 4 << 0 << 1 << 2;
        QTest::newRow("HL2")          << QStringLiteral("hl2")  << 2 << 0 << 1 << 2;
    }

    void protocol1_routes_by_frame_slot()
    {
        QFETCH(QString, codecName);
        QFETCH(int, streams);
        QFETCH(int, slotA);
        QFETCH(int, slotB);
        QFETCH(int, deadSlot);

        std::unique_ptr<IP1Codec> codec;
        if (codecName == QLatin1String("ap3"))      { codec = std::make_unique<P1CodecAnvelinaPro3>(); }
        else if (codecName == QLatin1String("rp"))  { codec = std::make_unique<P1CodecRedPitaya>(); }
        else if (codecName == QLatin1String("hl2")) { codec = std::make_unique<P1CodecHl2>(); }
        else                                        { codec = std::make_unique<P1CodecStandard>(); }

        RadioModel model;
        model.receiverManager()->setP1Codec(codec.get());
        model.configureStreamPool(streams, 5, 192000);
        for (int st = 0; st < streams; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7100000.0);  // outside A's window: own stream
        const int streamA = model.slices().at(a)->streamIndex();
        const int streamB = model.slices().at(b)->streamIndex();
        QCOMPARE(streamA, 0);
        QCOMPARE(streamB, 1);

        QCOMPARE(model.ddcForStream(streamA), slotA);
        QCOMPARE(model.ddcForStream(streamB), slotB);
        ReceiverManager* rm = model.receiverManager();
        QCOMPARE(rm->receiverConfig(streamA).hardwareRx, slotA);
        QCOMPARE(rm->receiverConfig(streamB).hardwareRx, slotB);

        QSignalSpy spy(rm, &ReceiverManager::iqDataForReceiver);
        const QVector<float> iq(64, 0.25f);
        rm->feedIqData(slotA, iq);
        rm->feedIqData(slotB, iq);
        rm->feedIqData(deadSlot, iq);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), streamA);
        QCOMPARE(spy.at(1).at(0).toInt(), streamB);
    }

    // The HL2 keeps today's routing in every state it supports (plan Task 11,
    // operator's condition): each active stream's slot equals what the old
    // sequential auto-assign gave (nth active receiver -> slot n), for
    // PureSignal off / armed / transmitting, with and without MOX, and with
    // one to five slices sharing its two streams. The one accepted
    // difference: slice B alone after slice A is removed keeps slot 1.
    void hl2_routing_matches_today_in_every_state()
    {
        P1CodecHl2 codec;
        RadioModel model;
        model.receiverManager()->setP1Codec(&codec);
        model.configureStreamPool(2, 5, 192000);
        model.receiverManager()->createReceiver();
        model.receiverManager()->createReceiver();

        // Five slices sharing the HL2's two streams: A, C, E on 20 m, B, D on 40 m.
        const double freqs[5] = {14200000.0, 7100000.0, 14210000.0, 7110000.0, 14220000.0};
        QVector<int> ids;
        for (double f : freqs) {
            const int id = model.addSlice();
            QVERIFY(id >= 0);
            model.slices().at(id)->setFrequency(f);
            ids.append(id);
        }
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(model.slices().at(ids[i])->streamIndex(), (i % 2 == 0) ? 0 : 1);
        }

        ReceiverManager* rm = model.receiverManager();
        struct State { bool mox; bool ps; };
        const State states[] = {{false, false}, {true, false}, {false, true}, {true, true}};
        for (const State& st : states) {
            CodecContext ctx{};
            ctx.model = HPSDRModel::HERMESLITE;
            ctx.mox = st.mox;
            ctx.puresignalRun = st.ps;
            std::array<SliceConfig, 5> streams{};
            streams[0].live = true;
            streams[0].sampleRateHz = 192000;
            streams[1].live = true;
            streams[1].sampleRateHz = 192000;
            const DdcAssignment asg = codec.applyDdcAssignment(ctx, streams);
            model.publishDdcAssignmentForTest(asg);

            // Today's sequential answer: slot = position among active streams.
            int nth = 0;
            for (int s = 0; s < 2; ++s) {
                if (asg.streamDdc[s] < 0) {
                    QCOMPARE(rm->receiverConfig(s).active, false);
                    continue;
                }
                QVERIFY2(rm->receiverConfig(s).active,
                         qPrintable(QStringLiteral("mox %1 ps %2 stream %3").arg(st.mox).arg(st.ps).arg(s)));
                QCOMPARE(rm->receiverConfig(s).hardwareRx, nth);
                ++nth;
            }
            // PureSignal transmitting: slice B's stream is suspended, A stays
            // on slot 0 (mi0bot GetDDC rx1 = 0; the pair rides slots 2 + 3).
            if (st.mox && st.ps) {
                QCOMPARE(asg.streamDdc[1], -1);
                QCOMPARE(asg.psFwdDdc, 2);
                QCOMPARE(asg.psRevDdc, 3);
            }
        }

        // The accepted difference: A's slices removed, B's stream alone keeps
        // slot 1 (sequential would have moved it to slot 0).
        model.removeSlice(ids[0]);
        model.removeSlice(ids[2]);
        model.removeSlice(ids[4]);
        CodecContext ctx{};
        ctx.model = HPSDRModel::HERMESLITE;
        std::array<SliceConfig, 5> streams{};
        streams[1].live = true;
        streams[1].sampleRateHz = 192000;
        model.publishDdcAssignmentForTest(codec.applyDdcAssignment(ctx, streams));
        QCOMPARE(rm->receiverConfig(0).active, false);
        QCOMPARE(rm->receiverConfig(1).active, true);
        QCOMPARE(rm->receiverConfig(1).hardwareRx, 1);
    }

    void widening_a_stream_rate_admits_a_previously_excluded_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        // 14.400 sits outside +-96 kHz but inside +-384 kHz, so it only
        // fits once the window is widened.
        model.setStreamSampleRate(0, 768000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14400000.0);

        QCOMPARE(model.slices().at(b)->streamIndex(), 0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void successful_widening_commits_one_coherent_batch()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);

        QSignalSpy centres(&model, &RadioModel::streamCentreChanged);
        QSignalSpy bindings(&model, &RadioModel::streamBindingsChanged);
        QSignalSpy assignments(&model, &RadioModel::ddcAssignmentRequested);

        QVERIFY(model.setStreamSampleRate(0, 768000));
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(model.streamSampleRateHzForTest(0), 768000);
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 768000);
        QCOMPARE(centres.count(), 1);
        QCOMPARE(bindings.count(), 1);
        QCOMPARE(assignments.count(), 1);
    }

    void invalid_rate_is_rejected_without_signals()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);

        QSignalSpy centres(&model, &RadioModel::streamCentreChanged);
        QSignalSpy bindings(&model, &RadioModel::streamBindingsChanged);
        QSignalSpy assignments(&model, &RadioModel::ddcAssignmentRequested);

        QVERIFY(!model.setStreamSampleRate(0, 0));
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(model.streamSampleRateHzForTest(0), 192000);
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 192000);
        QCOMPARE(centres.count(), 0);
        QCOMPARE(bindings.count(), 0);
        QCOMPARE(assignments.count(), 0);
    }

    void narrowing_a_stream_rate_evicts_an_out_of_window_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        model.setStreamSampleRate(0, 768000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14400000.0);
        QCOMPARE(model.slices().at(b)->streamIndex(), 0);

        // Narrowing to +-96 kHz puts B out of window. It must migrate to
        // its own DDC, not be left silently aliased on a window that no
        // longer contains it.
        QVERIFY(model.setStreamSampleRate(0, 192000));
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        QVERIFY(model.slices().at(b)->streamIndex() != 0);
        QCOMPARE(model.activeStreamCount(), 2);
    }

    void impossible_narrowing_is_rejected_without_partial_mutation()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 3, 192000);
        model.receiverManager()->createReceiver();
        model.receiverManager()->createReceiver();
        model.openRxChannelPool(3, bufferSizeForRate(192000), 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        model.setStreamSampleRate(0, 768000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14400000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), 0);

        const int c = model.addSlice();
        model.sliceById(c)->setFrequency(7100000.0);
        QCOMPARE(model.sliceById(c)->streamIndex(), 1);
        QCOMPARE(model.activeStreamCount(), 2);

        struct StreamSnapshot {
            bool active;
            double centreHz;
            int rateHz;
        };
        const std::array<StreamSnapshot, 2> streamsBefore{{
            {model.streamActiveForTest(0),
             model.streamCentreHzForTest(0),
             model.streamSampleRateHzForTest(0)},
            {model.streamActiveForTest(1),
             model.streamCentreHzForTest(1),
             model.streamSampleRateHzForTest(1)}
        }};

        struct SliceSnapshot {
            int stream;
            double shiftHz;
            int rateHz;
        };
        const std::array<SliceSnapshot, 3> slicesBefore{{
            {model.sliceById(a)->streamIndex(),
             model.sliceById(a)->shiftOffsetHz(),
             model.sliceById(a)->sampleRateHz()},
            {model.sliceById(b)->streamIndex(),
             model.sliceById(b)->shiftOffsetHz(),
             model.sliceById(b)->sampleRateHz()},
            {model.sliceById(c)->streamIndex(),
             model.sliceById(c)->shiftOffsetHz(),
             model.sliceById(c)->sampleRateHz()}
        }};

        const std::array<ReceiverConfig, 2> receiversBefore{{
            model.receiverManager()->receiverConfig(0),
            model.receiverManager()->receiverConfig(1)
        }};
        const std::array<std::pair<int, int>, 3> wdspBefore{{
            {engine->rxChannel(a)->sampleRate(), engine->rxChannel(a)->bufferSize()},
            {engine->rxChannel(b)->sampleRate(), engine->rxChannel(b)->bufferSize()},
            {engine->rxChannel(c)->sampleRate(), engine->rxChannel(c)->bufferSize()}
        }};

        QSignalSpy centres(&model, &RadioModel::streamCentreChanged);
        QSignalSpy bindings(&model, &RadioModel::streamBindingsChanged);
        QSignalSpy assignments(&model, &RadioModel::ddcAssignmentRequested);

        QVERIFY(!model.setStreamSampleRate(0, 192000));
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        for (int st = 0; st < 2; ++st) {
            QCOMPARE(model.streamActiveForTest(st), streamsBefore[st].active);
            QCOMPARE(model.streamCentreHzForTest(st), streamsBefore[st].centreHz);
            QCOMPARE(model.streamSampleRateHzForTest(st), streamsBefore[st].rateHz);
        }

        const std::array<int, 3> ids{{a, b, c}};
        for (int i = 0; i < 3; ++i) {
            const SliceModel* slice = model.sliceById(ids[i]);
            QCOMPARE(slice->streamIndex(), slicesBefore[i].stream);
            QCOMPARE(slice->shiftOffsetHz(), slicesBefore[i].shiftHz);
            QCOMPARE(slice->sampleRateHz(), slicesBefore[i].rateHz);
        }

        for (int st = 0; st < 2; ++st) {
            const ReceiverConfig after =
                model.receiverManager()->receiverConfig(st);
            QCOMPARE(after.frequencyHz, receiversBefore[st].frequencyHz);
            QCOMPARE(after.sampleRate, receiversBefore[st].sampleRate);
            QCOMPARE(after.active, receiversBefore[st].active);
        }

        for (int i = 0; i < 3; ++i) {
            const RxChannel* channel = engine->rxChannel(ids[i]);
            QCOMPARE(channel->sampleRate(), wdspBefore[i].first);
            QCOMPARE(channel->bufferSize(), wdspBefore[i].second);
        }

        QCOMPARE(centres.count(), 0);
        QCOMPARE(bindings.count(), 0);
        QCOMPARE(assignments.count(), 0);
    }

    // ── Phase 3F Sub-Epic I closeout, defect F1 ─────────────────────────
    //
    // connectToRadio sizes the pool and binds every slice BEFORE
    // wireConnectionSignals constructs the RxDspWorker, so every bind-time
    // republish hit the `if (m_dspWorker)` guard with a null pointer. The
    // worker then started life knowing only its constructor seed
    // ({stream 0: [slice 0]}) and anything on a non-zero stream demodulated
    // nothing.

    void bindings_reach_a_worker_constructed_after_the_binds()
    {
        constexpr int kInSize = 4;

        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        const int streamB = model.slices().at(b)->streamIndex();
        const int idB     = model.slices().at(b)->sliceIndex();
        QVERIFY(streamB > 0);

        // Production ordering: the worker only exists now, well after every
        // bind above already ran.
        RxDspWorker worker;
        worker.setBufferSizes(kInSize, 64);
        model.attachDspWorkerForTest(&worker);
        model.republishAllStreamBindings();
        // republishStreamBindings posts a queued call; drain it.
        QCoreApplication::processEvents();

        QCOMPARE(workerBindingsFor(worker, streamB, kInSize), QVector<int>{idB});
    }

    // Teardown left every slice's streamIndex set, so connectToRadio's
    // `streamIndex() < 0` bind loop skipped all of them, nothing
    // republished, and the fresh worker was left with the constructor seed
    // alone. A Slice B on stream 1 went silent until it was retuned.

    void reconnect_rebinds_a_slice_that_was_on_a_non_zero_stream()
    {
        constexpr int kInSize = 4;

        RadioModel model;

        // ── connect #1 ──────────────────────────────────────────────────
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        const int idB = model.slices().at(b)->sliceIndex();
        QVERIFY(model.slices().at(b)->streamIndex() > 0);
        QCOMPARE(model.activeStreamCount(), 2);

        RxDspWorker firstWorker;
        firstWorker.setBufferSizes(kInSize, 64);
        model.attachDspWorkerForTest(&firstWorker);
        model.republishAllStreamBindings();
        QCoreApplication::processEvents();

        // ── teardown ────────────────────────────────────────────────────
        // teardownConnection destroys the worker, then releases the
        // bindings so the next connect has something to re-bind.
        model.attachDspWorkerForTest(nullptr);
        model.releaseStreamBindings();

        QCOMPARE(model.slices().at(b)->streamIndex(), -1);
        QCOMPARE(model.slices().at(b)->ddcIndex(), -1);
        QCOMPARE(model.activeStreamCount(), 0);

        // ── connect #2 ──────────────────────────────────────────────────
        model.configureStreamPool(5, 5, 192000);
        model.bindUnboundSlices();

        const int streamB = model.slices().at(b)->streamIndex();
        QVERIFY(streamB > 0);
        QCOMPARE(model.activeStreamCount(), 2);

        RxDspWorker secondWorker;
        secondWorker.setBufferSizes(kInSize, 64);
        model.attachDspWorkerForTest(&secondWorker);
        model.republishAllStreamBindings();
        QCoreApplication::processEvents();

        QCOMPARE(workerBindingsFor(secondWorker, streamB, kInSize),
                 QVector<int>{idB});

        // ...and the constructor seed must not have survived onto a stream
        // that Slice A no longer occupies alone.
        QCOMPARE(workerBindingsFor(secondWorker,
                                   model.slices().at(a)->streamIndex(),
                                   kInSize),
                 QVector<int>{model.slices().at(a)->sliceIndex()});

        model.attachDspWorkerForTest(nullptr);
    }

    // ── Phase 3F Sub-Epic I closeout, defect F3 ─────────────────────────
    //
    // On the 1-ADC HERMES class Thetis collapses to a single synced pair the
    // moment PureSignal transmits, dropping every user receiver:
    //
    //   From Thetis console.cs:8448-8456 [v2.10.3.15]:
    //     else // transmitting and PS is ON
    //     { P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
    //       Rate[0] = ps_rate; Rate[1] = ps_rate; cntrl1 = 4; cntrl2 = 0; }
    //
    // with no trailing rx2_enabled clause, unlike the ORION class at
    // console.cs:8299-8303 which keeps RX2 on DDC3 throughout. Thetis's own
    // GetDDC confirms it: the P2 Hermes-class MOX+PS cases are empty, so rx1
    // and rx2 both come back -1 (console.cs:8635-8636 [v2.10.3.15]).
    //
    // So the drop is correct and stays. What must not stay is the silence,
    // and a slice reporting a DDC the radio has stopped streaming.

    void puresignal_on_tx_suspends_the_extra_streams_visibly()
    {
        RadioModel model;
        P2CodecHermes codec;   // ANAN-10 / ANAN-100 / ANAN-G2E on P2
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
        for (int st = 0; st < 4; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        const int streamA = model.slices().at(a)->streamIndex();
        const int streamB = model.slices().at(b)->streamIndex();
        QVERIFY(streamB > 0);

        // Plain RX: both slices have a real DDC and nothing is suspended.
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QVERIFY(model.slices().at(b)->ddcIndex() >= 0);
        QCOMPARE(model.slices().at(b)->chainIndex(), 0);
        QVERIFY(model.suspendedStreams().isEmpty());

        QSignalSpy spy(&model, &RadioModel::streamsSuspended);

        // Key MOX with PureSignal running.
        model.setDdcContextForTest(/*mox*/ true, /*puresignalRun*/ true,
                                   /*diversity*/ false);
        model.refreshDdcAssignmentForRadioState();

        // The drop itself. Faithful to Thetis, and it takes the WHOLE radio:
        // GetDDC's Hermes-class case 5 ("on off on", meaning mox with
        // PureSignal) has an empty body (console.cs:8635-8636 [v2.10.3.15]),
        // so rx1 and rx2 both come back -1. RX1 is dropped too, not just the
        // extra receivers.
        //
        // This expectation used to read {streamB} alone. That was wrong, and
        // it was wrong in a way that would have blocked the correct fix:
        // P2CodecHermes set streamDdc[0] before branching, so stream 0 kept a
        // stale DDC0 mapping through the PureSignal branch and never looked
        // suspended. The assertion pinned the bug. (Codex review, PR #293.)
        QCOMPARE(model.suspendedStreams(), (QVector<int>{streamA, streamB}));

        // The model must say so rather than leaving the operator to notice
        // the audio stopped. Slice B, not stream 1: the operator sees letters.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<QVector<int>>(),
                 (QVector<int>{streamA, streamB}));
        const QString reason = spy.at(0).at(1).toString();
        QVERIFY(!reason.isEmpty());
        QVERIFY(reason.contains(QLatin1String("A")));
        QVERIFY(reason.contains(QLatin1String("B")));
        QVERIFY(reason.contains(QLatin1String("PureSignal")));

        // ...and slice B must not still be advertising the DDC it had before
        // the key. That stale number is what made this invisible.
        QCOMPARE(model.slices().at(b)->ddcIndex(), -1);
        QCOMPARE(model.slices().at(b)->streamIndex(), streamB);
        QCOMPARE(model.slices().at(b)->chainIndex(), 0);

        // Codex review, PR #293: SliceModel::psPaused is what greys the pan
        // and raises the PS HOLD pill (PanadapterApplet.cpp reads it), but no
        // production code ever wrote it, so the overlay went on presenting a
        // slice the radio had stopped streaming as live. Only the slice that
        // actually lost its DDC is paused.
        QVERIFY2(model.slices().at(a)->psPaused(),
                 "slice A lost its DDC to PureSignal and must report psPaused");
        QVERIFY2(model.slices().at(b)->psPaused(),
                 "slice B lost its DDC to PureSignal and must report psPaused");
    }

    void unkeying_restores_the_suspended_streams()
    {
        RadioModel model;
        P2CodecHermes codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(4, 4, 192000);
        for (int st = 0; st < 4; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);
        const int streamA = model.slices().at(a)->streamIndex();
        const int streamB = model.slices().at(b)->streamIndex();

        model.setDdcContextForTest(true, true, false);
        model.refreshDdcAssignmentForRadioState();
        // Both, not just B: Hermes-class PureSignal drops RX1 as well. See the
        // note on the same expectation in the suspend test above.
        QCOMPARE(model.suspendedStreams(), (QVector<int>{streamA, streamB}));

        QSignalSpy spy(&model, &RadioModel::streamsSuspended);

        // Unkey.
        model.setDdcContextForTest(false, true, false);
        model.refreshDdcAssignmentForRadioState();

        QVERIFY(model.suspendedStreams().isEmpty());
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QVERIFY(model.slices().at(b)->ddcIndex() >= 0);
        // One transition out, carrying an empty list so the UI can clear the
        // warning instead of leaving it up for its full timeout.
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).value<QVector<int>>().isEmpty());

        // Codex review, PR #293: and the pause has to lift, or the pan stays
        // greyed for the rest of the session once PureSignal has keyed once.
        QVERIFY2(!model.slices().at(a)->psPaused(),
                 "slice A got its DDC back and must no longer report psPaused");
        QVERIFY2(!model.slices().at(b)->psPaused(),
                 "slice B got its DDC back and must no longer report psPaused");
    }

    // The ORION class keeps its extra receivers through PureSignal
    // (console.cs:8299-8303 [v2.10.3.15]: the `if (rx2_enabled) DDCEnable +=
    // DDC3;` sits OUTSIDE the mox/PS/diversity chain and applies in every
    // case). Nothing may be reported suspended there.
    void orion_class_keeps_its_streams_through_puresignal()
    {
        RadioModel model;
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(5, 5, 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        model.setDdcContextForTest(true, true, false);
        model.refreshDdcAssignmentForRadioState();

        QVERIFY(model.suspendedStreams().isEmpty());
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QVERIFY(model.slices().at(b)->ddcIndex() >= 0);

        // Codex review, PR #293: psPaused has to discriminate, or it is just
        // "is PureSignal transmitting" wearing a per-slice name. The ORION
        // class keeps every receiver through PS, so no slice is paused even
        // though MOX and PureSignal are both on.
        QVERIFY2(!model.slices().at(a)->psPaused(),
                 "ORION keeps its receivers through PS; slice A is not paused");
        QVERIFY2(!model.slices().at(b)->psPaused(),
                 "ORION keeps its receivers through PS; slice B is not paused");
    }

    // ── Phase 3F Sub-Epic I closeout, defect F4 ─────────────────────────
    //
    // SliceModel::setFrequency commits and emits before the bind handler
    // runs, so a rejected retune left the VFO reading the new frequency
    // while the stream binding and the WDSP shift offset still described the
    // old one. The flag said 7.150, the panadapter said 20 m, and WDSP kept
    // demodulating 14.200.

    void a_rejected_retune_leaves_vfo_and_dsp_agreeing()
    {
        RadioModel model;
        // One DDC. A slice that leaves its window has nowhere to go.
        model.configureStreamPool(/*userDdcCount*/ 1, /*maxSlices*/ 5, 192000);

        const int a = model.addSlice();
        SliceModel* slice = model.slices().at(a);
        slice->setFrequency(14200000.0);

        const int    boundStream = slice->streamIndex();
        const double boundHz     = slice->frequency();
        QVERIFY(boundStream >= 0);

        // A second slice pins the DDC so the first cannot simply drag it.
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);
        QCOMPARE(model.slices().at(b)->streamIndex(), boundStream);

        QSignalSpy spy(&model, &RadioModel::sliceRetuneRejected);

        // Off the band entirely: no window covers it, no DDC is free.
        slice->setFrequency(7150000.0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), slice->sliceIndex());
        // The message must talk about staying put, not about adding a slice.
        const QString reason = spy.at(0).at(1).toString();
        QVERIFY(reason.contains(QLatin1String("stayed on")));
        // R-R3-34: the frequency and its unit stay on one line.
        QVERIFY2(reason.contains(QRegularExpression(
                     QStringLiteral("stayed on [0-9.]+\u00A0MHz\\."))),
                 qPrintable(reason));

        // The invariant that actually matters: whatever the VFO reads, the
        // DSP must be demodulating it. Reconstruct the demodulated frequency
        // from the binding the way WDSP does -- stream centre plus shift --
        // and require it to equal the frequency on the flag.
        QCOMPARE(slice->streamIndex(), boundStream);
        const double demodulatedHz =
            model.streamCentreHzForTest(slice->streamIndex())
            + slice->shiftOffsetHz();
        QCOMPARE(slice->frequency(), demodulatedHz);
        QCOMPARE(slice->frequency(), boundHz);
    }

    // An accepted retune must not be disturbed by the rollback machinery.
    void an_accepted_retune_still_moves_the_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        SliceModel* slice = model.slices().at(a);
        slice->setFrequency(14200000.0);

        QSignalSpy spy(&model, &RadioModel::sliceRetuneRejected);

        slice->setFrequency(7150000.0);

        QCOMPARE(spy.count(), 0);
        QCOMPARE(slice->frequency(), 7150000.0);
        QVERIFY(slice->streamIndex() >= 0);
        const double demodulatedHz =
            model.streamCentreHzForTest(slice->streamIndex())
            + slice->shiftOffsetHz();
        QCOMPARE(slice->frequency(), demodulatedHz);
    }

    // Slice letters must track the slice id, so the RX applet's buttons read
    // A, B, C rather than A, A, A.
    //
    // sliceLetter() used to return a stored member that defaulted to 'A' and
    // whose setter had no production caller. Readers guarded with
    //     sliceLetter().isNull() ? QChar('A' + sliceIndex()) : sliceLetter()
    // never took the fallback, because a defaulted QChar is 'A' and not null.
    // Bench-caught on a two-pan G2 layout, 2026-07-26.
    void slice_letters_follow_the_slice_id()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        const int b = model.addSlice();
        const int c = model.addSlice();
        QCOMPARE(model.sliceById(a)->sliceLetter(), QChar('A'));
        QCOMPARE(model.sliceById(b)->sliceLetter(), QChar('B'));
        QCOMPARE(model.sliceById(c)->sliceLetter(), QChar('C'));

        // Ids are not renumbered on removal, so the survivors keep their
        // letters and a recycled id gets the letter that goes with it.
        model.removeSlice(b);
        QCOMPARE(model.sliceById(a)->sliceLetter(), QChar('A'));
        QCOMPARE(model.sliceById(c)->sliceLetter(), QChar('C'));

        const int d = model.addSlice();     // reuses the lowest free id, 1
        QCOMPARE(d, b);
        QCOMPARE(model.sliceById(d)->sliceLetter(), QChar('B'));
    }

    // ── Phase 3F Sub-Epic I closeout, defect G2 ─────────────────────────
    //
    // The VFO flag's "Sample rate >" submenu emits
    // VfoWidget::sampleRateRequested(sliceId, hz). MainWindow used to route
    // that to SliceModel::setSampleRateHz, and nothing downstream read the
    // property once buildStreamConfigsForCodec started sourcing the rate
    // from the allocator, so the control did nothing at all.
    //
    // requestSliceSampleRate is the handler body, factored out of the two
    // MainWindow lambdas so it is reachable from a test. Proof that it
    // reaches the allocator: widening the window admits a slice that the
    // narrower window would have pushed onto its own DDC.
    void the_slice_rate_request_reaches_the_stream_allocator()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int streamA = model.sliceById(a)->streamIndex();
        QVERIFY(streamA >= 0);

        // Slice IDs, not list positions: VfoWidget carries sliceIndex().
        model.requestSliceSampleRate(a, 768000);

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14400000.0);

        // 200 kHz away: outside +-96 kHz, inside +-384 kHz. Sharing the
        // stream is only possible if the rate change landed.
        QCOMPARE(model.sliceById(b)->streamIndex(), streamA);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    // The rate is a property of the DDC stream, so SliceModel::sampleRateHz
    // is the RESOLVED stream rate, not a private per-slice wish. Every slice
    // co-hosted on the stream must report the same number, otherwise the
    // checkmark on one flag's rate menu contradicts the other's.
    void co_hosted_slices_report_the_streams_resolved_rate()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14210000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(),
                 model.sliceById(a)->streamIndex());

        // Asked for on B's flag; both flags must follow.
        model.requestSliceSampleRate(b, 768000);

        QCOMPARE(model.sliceById(a)->sampleRateHz(), 768000);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 768000);
    }

    // A slice evicted by a narrowing lands on a different stream at that
    // stream's own rate, so its mirror must follow the migration rather than
    // keep reporting the rate of the window it just left.
    void an_evicted_slice_reports_its_new_streams_rate()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        model.requestSliceSampleRate(a, 768000);

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14400000.0);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 768000);

        // Narrowing to +-96 kHz evicts B onto its own DDC, which was never
        // widened and still carries the pool default.
        model.requestSliceSampleRate(a, 192000);

        QVERIFY(model.sliceById(b)->streamIndex()
                != model.sliceById(a)->streamIndex());
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 192000);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 192000);
    }

    void a_rate_request_for_an_unknown_slice_is_ignored()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);

        model.requestSliceSampleRate(99, 768000);

        QCOMPARE(model.sliceById(a)->sampleRateHz(), 192000);
    }

    // ── Phase 3F Sub-Epic I closeout, defect H1 ─────────────────────────
    //
    // A stream's rate IS its drain geometry, on both sides of the chunk:
    //
    //   From Thetis cmaster.c:461,473-475 [v2.10.3.15] (SetXcmInrate):
    //     pcm->xcm_insize[in_id] = getbuffsize (rate);
    //     for (i = 0; i < pcm->cmSubRCVR; i++) {
    //         SetInputSamplerate (chid (in_id, i), rate);
    //         SetInputBuffsize (chid (in_id, i), pcm->xcm_insize[in_id]);
    //     }
    //
    // One size per input stream, pushed to EVERY sub-receiver channel bound to
    // it. Changing a stream's rate without following it into that stream's
    // WDSP channels leaves fexchange2 reading ch[].in_size samples
    // (iobuffs.c:532-536) out of a chunk the drain loop sized differently.

    void a_stream_rate_change_reaches_only_its_own_slices_wdsp_channels()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        const int streamA = model.sliceById(a)->streamIndex();
        const int streamB = model.sliceById(b)->streamIndex();
        QVERIFY(streamA >= 0);
        QVERIFY(streamB != streamA);

        // Sub-Epic I invariant: WDSP RX channel id == slice index.
        engine->createRxChannel(a, bufferSizeForRate(192000), 4096,
                                192000, 48000, 48000);
        engine->createRxChannel(b, bufferSizeForRate(192000), 4096,
                                192000, 48000, 48000);
        QVERIFY(engine->rxChannel(a) != nullptr);
        QVERIFY(engine->rxChannel(b) != nullptr);

        model.setStreamSampleRate(streamA, 768000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        // A's channel follows its stream, input rate AND input buffsize.
        QCOMPARE(engine->rxChannel(a)->sampleRate(), 768000);
        QCOMPARE(engine->rxChannel(a)->bufferSize(), bufferSizeForRate(768000));

        // ...and only those. B lives on another DDC that nobody re-rated;
        // dragging it along would make its chunk geometry disagree with the
        // stream actually feeding it.
        QCOMPARE(engine->rxChannel(b)->sampleRate(), 192000);
        QCOMPARE(engine->rxChannel(b)->bufferSize(), bufferSizeForRate(192000));
    }

    // The other half of the same geometry: the DSP worker's drain threshold
    // for that stream, computed with the one bufferSizeForRate() in the tree.
    void a_stream_rate_change_publishes_that_streams_drain_chunk()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        const int streamA = model.sliceById(a)->streamIndex();
        const int streamB = model.sliceById(b)->streamIndex();
        QVERIFY(streamB != streamA);

        RxDspWorker worker;
        worker.setBufferSizes(bufferSizeForRate(192000), 64);
        model.attachDspWorkerForTest(&worker);
        model.republishAllStreamBindings();
        QCoreApplication::processEvents();

        model.setStreamSampleRate(streamA, 768000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());
        QCoreApplication::processEvents();

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        // Exactly one chunk each, at each stream's OWN size. With a single
        // shared threshold, stream A's 1024 samples drained four 256-sample
        // chunks and every one of them was the wrong count for fexchange2.
        worker.processIqBatch(streamA, oneChunk(bufferSizeForRate(768000)));
        worker.processIqBatch(streamB, oneChunk(bufferSizeForRate(192000)));

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), streamA);
        QCOMPARE(spy.at(0).at(1).toInt(), bufferSizeForRate(768000));
        QCOMPARE(spy.at(1).at(0).toInt(), streamB);
        QCOMPARE(spy.at(1).at(1).toInt(), bufferSizeForRate(192000));

        model.attachDspWorkerForTest(nullptr);
    }

    // The stream geometry change quiesces a running DSP worker (disconnect
    // the feed, reset its accumulator on its own thread, publish the drain
    // size, re-rate the channels, reconnect). That wait belongs to the
    // receive lane, never the event loop: with the worker's thread busy in a
    // long block, setStreamSampleRate still returns at once, and the first
    // batch after the worker frees up drains at the new stream's size.
    void a_stream_rate_change_never_waits_on_a_busy_dsp_worker()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);
        const int streamA = model.sliceById(a)->streamIndex();
        QVERIFY(streamA >= 0);
        QVERIFY(model.sliceById(b)->streamIndex() != streamA);

        QThread dspThread;
        dspThread.setObjectName(QStringLiteral("TestDspThread"));
        auto worker = std::make_unique<RxDspWorker>();
        worker->setBufferSizes(bufferSizeForRate(192000), 64);
        worker->moveToThread(&dspThread);
        dspThread.start();
        struct Teardown {
            RadioModel* model;
            QThread* thread;
            ~Teardown()
            {
                model->attachDspWorkerOnThreadForTest(nullptr, nullptr);
                thread->quit();
                thread->wait();
            }
        } teardown{&model, &dspThread};

        model.attachDspWorkerOnThreadForTest(worker.get(), &dspThread);
        ReceiverManager* receivers = model.receiverManager();
        QVERIFY(receivers != nullptr);
        connect(receivers, &ReceiverManager::iqDataForReceiverStamped,
                worker.get(), &RxDspWorker::processStampedIqBatch,
                Qt::QueuedConnection);
        model.republishAllStreamBindings();

        // Drains observed on this thread, in the order the worker made them.
        QObject sink;
        QVector<QPair<int, int>> drained;
        connect(worker.get(), &RxDspWorker::chunkDrainedForStream, &sink,
                [&drained](int stream, int samples) {
            drained.append({stream, samples});
        }, Qt::QueuedConnection);

        // Hold the worker's thread in one long block. It lets go when the
        // test releases it, or after a bound if the event loop were ever made
        // to wait on it (then workerBusy reads false below and the test fails
        // instead of hanging).
        QSemaphore entered;
        QSemaphore release;
        std::atomic<bool> workerBusy{false};
        QMetaObject::invokeMethod(worker.get(), [&entered, &release, &workerBusy]() {
            workerBusy.store(true);
            entered.release();
            release.tryAcquire(1, 10000);
            workerBusy.store(false);
        }, Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1, 10000));

        model.setStreamSampleRate(streamA, 768000);
        QVERIFY2(workerBusy.load(),
                 "setStreamSampleRate waited on the busy DSP worker");

        release.release();
        QVERIFY(model.waitForReceiveLaneForTest(10000));

        // The next batch on stream A drains once, at the new 768 kHz size. At
        // the old 192 kHz threshold the same samples drain as four chunks.
        emit receivers->iqDataForReceiverStamped(
            streamA, oneChunk(bufferSizeForRate(768000)),
            ReceiverManager::enqueueClockNs());
        QTRY_COMPARE_WITH_TIMEOUT(drained.size(), 1, 10000);
        QCOMPARE(drained.at(0).first, streamA);
        QCOMPARE(drained.at(0).second, bufferSizeForRate(768000));
    }

    // Protocol 1 carries one rate for the whole radio (composeCcBank0 takes a
    // single sampleRate), so there the change has to reach every active
    // stream's slices, not just the named stream's.
    void on_protocol1_the_rate_change_reaches_every_active_streams_channels()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        // The injected connection lives on the stack and dies before the
        // model does. A failing QCOMPARE returns from the slot immediately,
        // so the detach has to be unconditional or the model is left holding
        // a dangling pointer through its own destructor.
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        const int streamA = model.sliceById(a)->streamIndex();
        QVERIFY(model.sliceById(b)->streamIndex() != streamA);
        QVERIFY(model.sampleRateIsRadioWide());

        engine->createRxChannel(a, bufferSizeForRate(192000), 4096,
                                192000, 48000, 48000);
        engine->createRxChannel(b, bufferSizeForRate(192000), 4096,
                                192000, 48000, 48000);

        model.setStreamSampleRate(streamA, 384000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(engine->rxChannel(a)->sampleRate(), 384000);
        QCOMPARE(engine->rxChannel(b)->sampleRate(), 384000);
    }

    // ...and it has to reach the wire, not only the client-side geometry.
    //
    // P1 carries the rate in C&C bank 0 byte C1 as a 2-bit code
    // (P1RadioConnection::composeCcBank0Full: 0/1/2/3 for 48/96/192/384 kHz).
    // setStreamSampleRate moved the allocator window, the WDSP channel rates,
    // the drain geometry and the FFT bin math, but nothing in that path
    // touches P1RadioConnection::m_sampleRate. The radio therefore kept
    // sending 192 kHz into a client that had reconfigured itself for 384 kHz.
    //
    // The wire push for a radio-wide rate is the 12-step setSampleRateLive
    // coordinator (stop, set, start, requiesce, restore), so this delegates to
    // it rather than growing a second copy of the same dance.
    void on_protocol1_the_rate_change_reaches_the_wire()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        // Seed the connection at 192 kHz. Not running, so restartStreamWithRate
        // records the rate without a stop/start burst.
        conn.restartStreamWithRate(192000);
        QCOMPARE(static_cast<quint8>(conn.captureBank0ForTest().at(1)),
                 quint8(2));   // srBits 2 == 192 kHz

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int streamA = model.sliceById(a)->streamIndex();
        QVERIFY(streamA >= 0);
        engine->createRxChannel(a, bufferSizeForRate(192000), 4096,
                                192000, 48000, 48000);

        QVERIFY(model.sampleRateIsRadioWide());
        model.setStreamSampleRate(streamA, 384000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        // setSampleRateLive marshals the wire write to the connection with a
        // queued invocation; in production the connection thread's event loop
        // delivers it. conn lives on this thread here, so drain it by hand.
        QCoreApplication::processEvents();

        QCOMPARE(static_cast<quint8>(conn.captureBank0ForTest().at(1)),
                 quint8(3));   // srBits 3 == 384 kHz
    }

    // The client-side half of a radio-wide rate change is only safe once the
    // wire half is known to have landed. If setSampleRateLive refuses (WDSP
    // down, engine torn down mid-change) and we widened the allocator window
    // anyway, we would manufacture exactly the desync the test above closes:
    // a client configured for 384 kHz reading a 192 kHz stream. Refusing
    // leaves both halves consistent at the old rate.
    void a_refused_radio_wide_rate_change_leaves_the_geometry_alone()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        // WDSP deliberately left uninitialised: setSampleRateLive's guard
        // fires and it returns -1 without touching the wire.
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int streamA = model.sliceById(a)->streamIndex();
        QVERIFY(streamA >= 0);
        QVERIFY(model.sampleRateIsRadioWide());

        QSignalSpy spy(&model, &RadioModel::streamCentreChanged);
        model.setStreamSampleRate(streamA, 384000);
        // R-R3-39: the WDSP and worker sides of the change run on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(spy.count(), 0);
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 192000);
    }

    // ── Phase 3F: pooled channels have to be switched on ────────────────
    //
    // A pooled channel is opened stopped. WDSP's create_rcvr passes
    // `0, // initial state` (Thetis ChannelMaster/cmaster.c:80
    // [v2.10.3.15]) and RxChannel::m_active defaults false, and
    // RxChannel::processIq short-circuits on exactly that flag:
    //
    //     if (!m_active.load()) {
    //         std::memset(outI, 0, sampleCount * sizeof(float));
    //         std::memset(outQ, 0, sampleCount * sizeof(float));
    //         return;
    //     }
    //
    // so an unactivated channel returns a zeroed buffer instead of
    // reaching fexchange2, and its slice is silent while its flag,
    // S-meter and mixer slot all look live.  isActive() IS that branch
    // condition, which is why these assert on it.

    // The short-circuit itself, on a bare channel that was never opened.
    // Safe in any build: the inactive path never touches WDSP.
    void an_inactive_channel_returns_silence_without_reaching_wdsp()
    {
        RxChannel ch(/*channelId=*/0, /*bufferSize=*/256,
                     /*sampleRate=*/192000);
        QVERIFY(!ch.isActive());

        constexpr int n = 16;
        float inI[n]{}, inQ[n]{};
        float outI[n], outQ[n];
        for (int i = 0; i < n; ++i) {
            inI[i] = 0.5f;
            inQ[i] = -0.5f;
            outI[i] = 12345.0f;   // sentinel
            outQ[i] = 12345.0f;
        }

        ch.processIq(inI, inQ, outI, outQ, n, n);

        for (int i = 0; i < n; ++i) {
            QCOMPARE(outI[i], 0.0f);
            QCOMPARE(outQ[i], 0.0f);
        }
    }

    // Slice A alone: its channel is live and no other pooled channel is,
    // because nothing is bound to them.  This is the single-slice
    // regression guard.
    void single_slice_leaves_only_slice_as_channel_live()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        QVERIFY(model.sliceById(a)->streamIndex() >= 0);

        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        QVERIFY(engine->rxChannel(a) != nullptr);
        QVERIFY(engine->rxChannel(a)->isActive());
        for (int ch = 1; ch < 5; ++ch) {
            QVERIFY2(!engine->rxChannel(ch)->isActive(),
                     qPrintable(QStringLiteral("unbound pool channel %1 is running")
                                    .arg(ch)));
        }
    }

    // Slice B on the same band as A: it shares A's DDC, so RxDspWorker
    // hands it the same chunk.  Before the fix its channel stayed
    // inactive and processIq zeroed the buffer, so B was permanently
    // silent.
    void a_second_slice_sharing_a_ddc_gets_a_live_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14210000.0);

        // Same DDC — this is the co-hosted case, not a second stream.
        QCOMPARE(model.sliceById(b)->streamIndex(),
                 model.sliceById(a)->streamIndex());

        QVERIFY(engine->rxChannel(b) != nullptr);
        QVERIFY(engine->rxChannel(b)->isActive());
    }

    // Slice B on its own DDC, bound after the pool is already open: the
    // bind is what switches its channel on.
    void a_slice_on_a_second_ddc_gets_a_live_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        QVERIFY(model.sliceById(b)->streamIndex()
                != model.sliceById(a)->streamIndex());
        QVERIFY(engine->rxChannel(b)->isActive());
    }

    // Reconnect shape: slices are bound BEFORE the engine has any
    // channels (connectToRadio calls bindUnboundSlices ahead of the
    // WDSP-init lambda), so the pool open has to reconcile.  Without the
    // reconcile every slice would come back silent.
    void slices_bound_before_the_pool_exists_are_activated_by_the_pool()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        // Nothing is open yet — the binds above had no channel to touch.
        QVERIFY(engine->rxChannel(a) == nullptr);
        QVERIFY(engine->rxChannel(b) == nullptr);

        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        QVERIFY(engine->rxChannel(a)->isActive());
        QVERIFY(engine->rxChannel(b)->isActive());
    }

    // Removing Slice A used to silence the radio outright: channel 0 was
    // the only activated channel, and after the removal no slice was
    // bound to it.  B must keep working, and A's channel must stop.
    void removing_slice_a_leaves_slice_b_audible()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);
        QVERIFY(engine->rxChannel(a)->isActive());
        QVERIFY(engine->rxChannel(b)->isActive());

        model.removeSlice(a);

        QVERIFY2(engine->rxChannel(b)->isActive(),
                 "removing slice A silenced slice B");
        // A's channel stops running but stays open for reuse.
        QVERIFY(engine->rxChannel(a) != nullptr);
        QVERIFY(!engine->rxChannel(a)->isActive());
    }

    // A re-added slice takes the lowest free id, i.e. the channel the
    // removed slice just vacated.  It has to come back live.
    void a_reused_channel_id_comes_back_live()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        model.removeSlice(a);
        QVERIFY(!engine->rxChannel(a)->isActive());

        const int c = model.addSlice();
        QCOMPARE(c, a);   // lowest free id
        QVERIFY(engine->rxChannel(c)->isActive());
    }

    // ── A radio-wide live rate change quiesces every slice's channel ─────
    //
    // setSampleRateLive used to switch off channel 0 alone before moving the
    // rate on every slice's channel, and switched on channel 0 alone after.
    // Every other slice's channel was re-rated while still running.  Thetis
    // switches off every receiver channel first (subs, then the main channel
    // last with a drain), changes the rate, then switches the main channel
    // back on first and the others only if they had been running:
    //   From Thetis setup.cs:7112-7115 and 7175-7179 [v2.10.3.15]
    //
    // Each activeChanged is recorded with the channel's input rate at that
    // moment. A channel whose off event carries the old rate and whose on
    // event carries the new one was re-rated only while it was stopped.
    // This records the order only; whether each restarted channel actually
    // plays is a_live_rate_change_leaves_every_running_channel_audible's
    // job (fix wave 1, C1).
    void a_live_rate_change_stops_and_restarts_every_slices_channel()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);
        const int c = model.addSlice();
        model.sliceById(c)->setFrequency(3700000.0);
        const int d = model.addSlice();
        QVERIFY(d >= 0);
        model.sliceById(d)->setFrequency(21200000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        const int chA = model.sliceById(a)->sliceIndex();
        const int chB = model.sliceById(b)->sliceIndex();
        const int chC = model.sliceById(c)->sliceIndex();
        const int chD = model.sliceById(d)->sliceIndex();
        QCOMPARE(chA, 0);
        QCOMPARE(chB, 1);
        QCOMPARE(chC, 2);
        QCOMPARE(chD, 3);

        QVERIFY(engine->rxChannel(chA)->isActive());
        QVERIFY(engine->rxChannel(chB)->isActive());
        QVERIFY(engine->rxChannel(chC)->isActive());
        // Slice D's channel is stopped before the change and must stay so.
        engine->rxChannel(chD)->setActive(false);

        struct Event { int ch; bool active; int rate; };
        QVector<Event> events;
        // Declared after `events` so it dies first and takes the recording
        // connections with it before the model's teardown can emit.
        QObject recorder;
        for (int ch : {chA, chB, chC, chD}) {
            RxChannel* rx = engine->rxChannel(ch);
            // R-R3-39: the change stops and restarts the channels on the
            // receive lane; record there, as it happens (the model's wait
            // for the change keeps the event loop off `events`).
            connect(rx, &RxChannel::activeChanged, &recorder,
                    [&events, rx, ch](bool on) {
                        events.append({ch, on, rx->sampleRate()});
                    }, Qt::DirectConnection);
        }

        QVERIFY(model.setSampleRateLive(384000, false) >= 0);

        const QVector<Event> expected{
            // Off: highest channel first, channel 0 last and drained (the
            // others stop without a drain, Task 8).
            {chC, false, 192000},
            {chB, false, 192000},
            {chA, false, 192000},
            // On: channel 0 first, then the others that were running.
            {chA, true, 384000},
            {chB, true, 384000},
            {chC, true, 384000},
        };
        QCOMPARE(events.size(), expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            QVERIFY2(events[i].ch == expected[i].ch
                         && events[i].active == expected[i].active
                         && events[i].rate == expected[i].rate,
                     qPrintable(QStringLiteral("event %1: got ch%2 %3 @%4, "
                                               "want ch%5 %6 @%7")
                                    .arg(i)
                                    .arg(events[i].ch)
                                    .arg(events[i].active ? "on" : "off")
                                    .arg(events[i].rate)
                                    .arg(expected[i].ch)
                                    .arg(expected[i].active ? "on" : "off")
                                    .arg(expected[i].rate)));
        }

        QVERIFY(engine->rxChannel(chA)->isActive());
        QVERIFY(engine->rxChannel(chB)->isActive());
        QVERIFY(engine->rxChannel(chC)->isActive());
        QVERIFY(!engine->rxChannel(chD)->isActive());
        QCOMPARE(engine->rxChannel(chD)->sampleRate(), 384000);
    }

    // One slice: channel 0 goes off at the old rate and back on at the new
    // one, exactly as before the change above.
    void a_live_rate_change_with_one_slice_cycles_channel_zero_only()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);
        QCOMPARE(model.sliceById(a)->sliceIndex(), 0);

        QVector<QPair<bool, int>> events;
        QObject recorder;   // dies before `events`; see the test above
        RxChannel* rx0 = engine->rxChannel(0);
        QVERIFY(rx0->isActive());
        // R-R3-39: recorded on the receive lane, as it happens.
        connect(rx0, &RxChannel::activeChanged, &recorder,
                [&events, rx0](bool on) { events.append({on, rx0->sampleRate()}); },
                Qt::DirectConnection);
        for (int ch = 1; ch < 5; ++ch) {
            connect(engine->rxChannel(ch), &RxChannel::activeChanged, &recorder,
                    [ch](bool) { QFAIL(qPrintable(
                        QStringLiteral("unbound channel %1 toggled").arg(ch))); });
        }

        QVERIFY(model.setSampleRateLive(384000, false) >= 0);

        QCOMPARE(events.size(), 2);
        QCOMPARE(events[0], qMakePair(false, 192000));
        QCOMPARE(events[1], qMakePair(true, 384000));
        QVERIFY(rx0->isActive());
    }

    // ── A live rate change leaves every running channel audible ─────────
    //
    // Fix wave 1, C1. A channel stopped without a drain keeps WDSP's
    // slew-down and flush flags set (channel.c:288-290) until the channel's
    // next exchange clears them, and NereusSDR never exchanges on a stopped
    // channel (RxChannel::processIq returns early on !m_active). A channel
    // already at the new rate is skipped by setRxChannelRate, so nothing
    // rebuilds it; on restart its first block slews down and clears
    // exchange (iobuffs.c:553-560) and it is silent while isActive() says
    // true. On Protocol 2 that is a slice whose own rate (the per-slice rate
    // menu, setStreamSampleRate) already equals the new radio-wide rate.
    // Both channels here must produce audio after the change: slice B's,
    // already at the target rate, and slice A's, which is re-rated.
    void a_live_rate_change_leaves_every_running_channel_audible()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        RxChannel* rxA = engine->rxChannel(model.sliceById(a)->sliceIndex());
        RxChannel* rxB = engine->rxChannel(model.sliceById(b)->sliceIndex());
        QVERIFY(rxA && rxA->isActive());
        QVERIFY(rxB && rxB->isActive());

        // Slice B's channel already runs at the rate the change moves to,
        // as the per-slice rate menu leaves it on Protocol 2.
        QVERIFY(engine->setRxChannelRate(rxB->channelId(), 384000));
        // R-R3-39: the channels open, start and re-rate on the receive lane.
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY2(settledPeakFromATone(rxA, 192000) > 0.0, "slice A silent before the change");
        QVERIFY2(settledPeakFromATone(rxB, 384000) > 0.0, "slice B silent before the change");

        QVERIFY(model.setSampleRateLive(384000, false) >= 0);

        QCOMPARE(rxA->sampleRate(), 384000);
        QCOMPARE(rxB->sampleRate(), 384000);
        QVERIFY(rxA->isActive());
        QVERIFY(rxB->isActive());
        QVERIFY2(settledPeakFromATone(rxB, 384000) > 0.0,
                 "slice B, already at the new rate, is silent after the change");
        QVERIFY2(settledPeakFromATone(rxA, 384000) > 0.0,
                 "slice A, re-rated, is silent after the change");
    }

    // ── R-R3-49: a Protocol 2 slice's saved rate survives a restart ─────
    //
    // A window's rate change on Protocol 2 is per DDC and is recorded only
    // as the slice's per-band SampleRate. connectToRadio opens every stream
    // at the radio-wide rate and binds the slices there; it then reads each
    // slice's saved rate (savedSliceSampleRates, before the bind) and puts
    // it back (applySavedSliceSampleRates). This stands up the same state a
    // restart leaves: a P2 codec, WDSP channels, a slice bound at 192 kHz
    // with 768 kHz saved for its band.
    void a_p2_slice_comes_back_at_its_saved_rate()
    {
        RestartedP2 r;
        restartOnP2(r, 768000);
        const auto forget = qScopeGuard([&r] { AppSettings::instance().remove(savedRateKey(r.id)); });
        SliceModel* slice = r.model.sliceById(r.id);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        QCOMPARE(r.model.streamSampleRateHzForTest(stream), 192000);

        const QHash<int, int> saved = r.model.savedSliceSampleRates();
        QCOMPARE(saved.value(r.id), 768000);
        r.model.applySavedSliceSampleRates(saved, ProtocolVersion::Protocol2);
        QVERIFY(r.model.waitForReceiveLaneForTest());

        QCOMPARE(r.model.streamSampleRateHzForTest(slice->streamIndex()), 768000);
        QCOMPARE(slice->sampleRateHz(), 768000);
        // The WDSP channel, through the live-apply path.
        RxChannel* channel = r.engine->rxChannel(r.id);
        QVERIFY(channel);
        QCOMPARE(channel->sampleRate(), 768000);
        QCOMPARE(channel->bufferSize(), bufferSizeForRate(768000));
        // The wire: the DDC assignment carries it for the slice's DDC.
        const std::optional<DdcAssignment> assignment = r.model.computeDdcAssignment();
        QVERIFY(assignment.has_value());
        QVERIFY(slice->ddcIndex() >= 0);
        QCOMPARE(assignment->rate[static_cast<size_t>(slice->ddcIndex())], 768000);
    }

    void a_band_with_no_saved_rate_stays_at_the_connect_rate()
    {
        RestartedP2 r;
        restartOnP2(r, 0);
        SliceModel* slice = r.model.sliceById(r.id);
        const QHash<int, int> saved = r.model.savedSliceSampleRates();
        QVERIFY(!saved.contains(r.id));
        r.model.applySavedSliceSampleRates(saved, ProtocolVersion::Protocol2);
        QVERIFY(r.model.waitForReceiveLaneForTest());
        QCOMPARE(r.model.streamSampleRateHzForTest(slice->streamIndex()), 192000);
        QCOMPARE(r.engine->rxChannel(r.id)->sampleRate(), 192000);
    }

    void an_unsupported_saved_rate_stays_at_the_connect_rate()
    {
        RestartedP2 r;
        restartOnP2(r, 200000);   // not a rate any board runs
        const auto forget = qScopeGuard([&r] { AppSettings::instance().remove(savedRateKey(r.id)); });
        SliceModel* slice = r.model.sliceById(r.id);
        QSignalSpy rejected(&r.model, &RadioModel::sliceRetuneRejected);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
            QStringLiteral("saved sample rate 200000 is not supported by this radio")));
        r.model.applySavedSliceSampleRates(r.model.savedSliceSampleRates(),
                                          ProtocolVersion::Protocol2);
        QVERIFY(r.model.waitForReceiveLaneForTest());
        QCOMPARE(r.model.streamSampleRateHzForTest(slice->streamIndex()), 192000);
        QCOMPARE(r.engine->rxChannel(r.id)->sampleRate(), 192000);
        QCOMPARE(rejected.count(), 0);
    }

    // Protocol 1 has one rate for the whole radio, from the radio-wide key;
    // a slice's per-band rate does not move it at connect.
    void protocol_1_keeps_the_radio_wide_rate()
    {
        RestartedP2 r;
        restartOnP2(r, 768000);
        const auto forget = qScopeGuard([&r] { AppSettings::instance().remove(savedRateKey(r.id)); });
        SliceModel* slice = r.model.sliceById(r.id);
        r.model.applySavedSliceSampleRates(r.model.savedSliceSampleRates(),
                                          ProtocolVersion::Protocol1);
        QVERIFY(r.model.waitForReceiveLaneForTest());
        QCOMPARE(r.model.streamSampleRateHzForTest(slice->streamIndex()), 192000);
    }

    // A rate change is saved for the slice's band, so there is something to
    // come back to: before this nothing saved it unless some other slice
    // setting changed too.
    void a_rate_change_saves_the_slices_band_rate()
    {
        RestartedP2 r;
        restartOnP2(r, 0);
        const auto forget = qScopeGuard([&r] { AppSettings::instance().remove(savedRateKey(r.id)); });
        r.model.requestSliceSampleRate(r.id, 768000);
        QVERIFY(r.model.waitForReceiveLaneForTest());
        QCOMPARE(r.model.sliceById(r.id)->sampleRateHz(), 768000);
        r.model.flushPendingSettingsSave();
        QCOMPARE(r.model.sliceById(r.id)->savedSampleRateHz(Band::Band20m), 768000);
    }
};

QTEST_MAIN(TestStreamPoolBinding)
#include "tst_stream_pool_binding.moc"
