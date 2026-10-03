// =================================================================
// tests/tst_rade_rx_multislice_routing.cpp  (NereusSDR)
// =================================================================
//
// R-R3-31, then RADE threads: NereusSDR-native regression for RADE receive
// on several slices at once. There is no upstream multi-slice orchestration
// equivalent to port (no port check applies).
//
// Fixtures: the real WDSP receive channels (synchronous test init), the real
// RADE codec with its built-in weights (the "dummy" model sentinel,
// rade_api_nopy.c), and a synthetic 64-frame I/Q block. No radio, nothing
// keys a transmitter: MOX edges are the model's signal only.
//
// Modification history (NereusSDR):
//   2026-09-21 -- Added by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via OpenAI Codex.
//   2026-09-30 -- RADE threads: every RADE slice decodes at once, each on
//                 its own decoder thread; a stalled decoder plays silence
//                 and never holds the mixer; a removed slice takes its
//                 decoder with it. The single-owner checks this file held
//                 are retired with the single owner. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 -- A RADE slice restored from its saved band (a device's
//                 slice made again, a band button) decodes; a slice
//                 restored in another mode has no decoder. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 -- RADE threads review: decoded speech (not only silence
//                 slots) reaches the mixer; txEncode never waits for a
//                 decode on the TX slice's channel; a TX worker made again
//                 after a reconnect reaches the slice the transmitter moves
//                 to. J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-09-30 -- RADE gaps: a RADE slice with no sync plays silence, never
//                 its sideband; a slice created locally or by a phone
//                 (addSlice, addSliceOnPan) never disturbs a decoding RADE
//                 slice; two RADE slices on one pan both decode. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 -- RADE gaps: a RADE slice with no route yet (the blocks
//                 before the queued route lands, a new DSP worker before the
//                 replay) is muted. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30 -- RADE gaps: a create refused after the seed put the new
//                 slice in RADE leaves no decoder behind. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 -- RADE gaps fix round 1: the mode mask reaches the worker
//                 before the route; a refused create gives the restored
//                 RADE owner back; item C checks A's block count. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 -- RADE reason: a RADE slice whose decoder create or start
//                 fails stays muted and says why on its radeReason, and the
//                 reason clears when it leaves RADE. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 -- Fix wave CI: feed() waits for the receive lane before each
//                 block, so a new slice's settings are applied by block count,
//                 not by how soon the lane runs (B in USB was silent through
//                 the 512 blocks on the Linux runner). J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QThread>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/RadeChannel.h"
#include "core/RadeRxWorker.h"
#include "core/ReceiveLayoutStore.h"
#include "core/RxChannel.h"
#include "core/TxSliceArbiter.h"
#include "core/TxWorkerThread.h"
#include "core/WdspEngine.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "fakes/FakeAudioBus.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/RxDspWorker.h"
#include "models/SliceModel.h"

#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 64;

struct DspWorkerDetach {
    RadioModel* radio{nullptr};
    ~DspWorkerDetach()
    {
        if (radio) {
            radio->attachDspWorkerForTest(nullptr);
        }
    }
};

struct BusView {
    std::unique_ptr<FakeAudioBus> owned;
    FakeAudioBus* view{nullptr};
};

BusView makeOpenBus(const QString& name)
{
    BusView result;
    result.owned = std::make_unique<FakeAudioBus>(name);
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    if (!result.owned->open(format)) {
        return result;
    }
    result.view = result.owned.get();
    return result;
}

QVector<float> makeIqBlock(int frames)
{
    QVector<float> iq(frames * 2);
    for (int i = 0; i < frames; ++i) {
        const float sample = 0.08f * static_cast<float>((i % 11) - 5);
        iq[2 * i] = sample;
        iq[2 * i + 1] = -sample;
    }
    return iq;
}

// A decoder stall a test holds and lets go of.
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool open{false};
    std::atomic<int> entered{0};

    void hold()
    {
        entered.fetch_add(1);
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return open; });
    }
    void release()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            open = true;
        }
        cv.notify_all();
    }
};

// Two receivers A (stream 0, VAX 1) and B (stream 1, VAX 2) on a real WDSP
// engine and a worker driven on this thread.
struct TwoSliceRig {
    RadioModel radio;
    int a{-1};
    int b{-1};
    int c{-1};
    SliceModel* sliceA{nullptr};
    SliceModel* sliceB{nullptr};
    SliceModel* sliceC{nullptr};  // setUp(3) only
    WdspEngine* wdsp{nullptr};
    FakeAudioBus* speakers{nullptr};
    FakeAudioBus* vaxA{nullptr};
    FakeAudioBus* vaxB{nullptr};
    RxDspWorker worker;
    std::unique_ptr<DspWorkerDetach> detach;
    QVector<float> iq{makeIqBlock(kFrames)};

    // `slices` 3 adds a third receiver C (stream 2, no VAX).
    bool setUp(int slices = 2)
    {
        radio.configureStreamPool(/*userDdcCount=*/slices, /*maxSlices=*/slices,
                                  /*defaultRateHz=*/48000);
        a = radio.addSlice();
        b = radio.addSlice();
        sliceA = radio.sliceById(a);
        sliceB = radio.sliceById(b);
        if (!sliceA || !sliceB) {
            return false;
        }
        if (slices == 3) {
            c = radio.addSlice();
            sliceC = radio.sliceById(c);
            if (!sliceC) {
                return false;
            }
        }
        sliceA->setVaxChannel(1);
        sliceB->setVaxChannel(2);
        AudioEngine* const audio = radio.audioEngine();
        wdsp = radio.wdspEngine();
        wdsp->setSynchronousInitForTest(true);
        if (!wdsp->initialize(QStandardPaths::writableLocation(
                QStandardPaths::AppConfigLocation))) {
            return false;
        }
        BusView spk = makeOpenBus(QStringLiteral("speakers"));
        BusView va = makeOpenBus(QStringLiteral("vax-a"));
        BusView vb = makeOpenBus(QStringLiteral("vax-b"));
        if (!spk.view || !va.view || !vb.view) {
            return false;
        }
        speakers = spk.view;
        vaxA = va.view;
        vaxB = vb.view;
        audio->setSpeakersBusForTest(std::move(spk.owned));
        audio->setVaxBusForTest(1, std::move(va.owned));
        audio->setVaxBusForTest(2, std::move(vb.owned));
        RxChannel* const rxA = wdsp->createRxChannel(a, kFrames, 4096, 48000, 48000, 48000);
        RxChannel* const rxB = wdsp->createRxChannel(b, kFrames, 4096, 48000, 48000, 48000);
        if (!rxA || !rxB) {
            return false;
        }
        rxA->setActive(true);
        rxB->setActive(true);
        worker.setEngines(wdsp, audio);
        worker.setBufferSizes(kFrames, kFrames);
        worker.setStreamSlices(0, QVector<int>{a});
        worker.setStreamSlices(1, QVector<int>{b});
        if (sliceC) {
            RxChannel* const rxC =
                wdsp->createRxChannel(c, kFrames, 4096, 48000, 48000, 48000);
            if (!rxC) {
                return false;
            }
            rxC->setActive(true);
            worker.setStreamSlices(2, QVector<int>{c});
        }
        radio.attachDspWorkerForTest(&worker);
        detach = std::make_unique<DspWorkerDetach>();
        detach->radio = &radio;
        return true;
    }

    // The app's path into RADE: the slice's own mode change creates, wires
    // and starts its channel through the engine.
    RadeChannel* toRade(SliceModel* slice)
    {
        slice->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();  // the route reaches the worker
        return wdsp->radeChannel(slice->sliceIndex());
    }

    // Runs blocks through both streams until `channel` has decoded, and
    // says on which thread it did. False (with the counts in `evidence`)
    // when it never ran the codec or ran it on the main thread.
    bool decodesOnItsOwnThread(RadeChannel* channel, QString* evidence)
    {
        std::atomic<Qt::HANDLE> decodedOn{nullptr};
        const QMetaObject::Connection probe = QObject::connect(
            channel, &RadeChannel::rxSpeechReady, channel,
            [&decodedOn](const QByteArray&) {
                decodedOn.store(QThread::currentThreadId());
            }, Qt::DirectConnection);
        const int callsBefore = channel->radeRxCallCountForTest();
        constexpr int kBlocks = 512;  // past the resamplers and one rade_rx
        bool idle = true;
        for (int i = 0; i < kBlocks && idle; ++i) {
            worker.processIqBatch(0, iq);
            worker.processIqBatch(1, iq);
            idle = channel->waitRxIdleForTest(5000);
        }
        QObject::disconnect(probe);
        const Qt::HANDLE on = decodedOn.load();
        *evidence = QStringLiteral("idle=%1 radeRx=%2 decodedOnDecoderThread=%3 onMain=%4")
            .arg(idle)
            .arg(channel->radeRxCallCountForTest() - callsBefore)
            .arg(on != nullptr && on == channel->rxThreadIdForTest())
            .arg(on == QThread::currentThreadId());
        return idle && channel->radeRxCallCountForTest() > callsBefore
            && on != nullptr && on != QThread::currentThreadId()
            && on == channel->rxThreadIdForTest();
    }
};

// RADE gaps (2026-09-30). A loud tone 1 kHz above the dial: the USB
// sideband plays it loudly, and RADE's decoder never locks to it.
struct Tone {
    double phase{0.0};
    QVector<float> next(int frames)
    {
        constexpr double kTwoPi = 6.283185307179586;
        constexpr double kStep = kTwoPi * 1000.0 / 48000.0;
        QVector<float> iq(frames * 2);
        for (int i = 0; i < frames; ++i) {
            iq[2 * i] = 0.5f * static_cast<float>(std::cos(phase));
            iq[2 * i + 1] = 0.5f * static_cast<float>(std::sin(phase));
            phase += kStep;
            if (phase > kTwoPi) {
                phase -= kTwoPi;
            }
        }
        return iq;
    }
};

// The largest magnitude a bus was given from byte `from` on.
float peakSince(const FakeAudioBus* bus, qsizetype from)
{
    const QByteArray& buffer = bus->buffer();
    const auto* samples = reinterpret_cast<const float*>(buffer.constData());
    const qsizetype count = buffer.size() / qsizetype(sizeof(float));
    float peak = 0.0f;
    for (qsizetype i = from / qsizetype(sizeof(float)); i < count; ++i) {
        peak = std::max(peak, std::fabs(samples[i]));
    }
    return peak;
}

constexpr float kAudible = 1.0e-3f;

// RADE gaps: receiver A alone on a pool of `streams` DDCs, with WDSP
// channels open for every id a new slice can take, so a slice made later
// (locally, or by a phone through the session verbs) takes the model's own
// path: the seed, the stream bind and the binding republished to the
// worker. Each slice's VAX channel is its own, so a VAX bus holds exactly
// that slice's blocks.
struct GrowRig {
    RadioModel radio;
    int a{-1};
    SliceModel* sliceA{nullptr};
    WdspEngine* wdsp{nullptr};
    std::array<FakeAudioBus*, 4> vax{};
    RxDspWorker worker;
    std::unique_ptr<DspWorkerDetach> detach;
    Tone tone;

    bool setUp(int streams, int maxSlices = 3)
    {
        radio.configureStreamPool(/*userDdcCount=*/streams, /*maxSlices=*/maxSlices,
                                  /*defaultRateHz=*/48000);
        a = radio.addSlice();
        sliceA = radio.sliceById(a);
        if (!sliceA || sliceA->streamIndex() < 0) {
            return false;
        }
        sliceA->setDspMode(DSPMode::USB);
        sliceA->setVaxChannel(1);
        AudioEngine* const audio = radio.audioEngine();
        wdsp = radio.wdspEngine();
        wdsp->setSynchronousInitForTest(true);
        if (!wdsp->initialize(QStandardPaths::writableLocation(
                QStandardPaths::AppConfigLocation))) {
            return false;
        }
        BusView spk = makeOpenBus(QStringLiteral("speakers"));
        if (!spk.view) {
            return false;
        }
        audio->setSpeakersBusForTest(std::move(spk.owned));
        for (int ch = 1; ch <= 4; ++ch) {
            BusView bus = makeOpenBus(QStringLiteral("vax-%1").arg(ch));
            if (!bus.view) {
                return false;
            }
            vax[size_t(ch - 1)] = bus.view;
            audio->setVaxBusForTest(ch, std::move(bus.owned));
        }
        for (int id = 0; id < maxSlices; ++id) {
            RxChannel* const rx =
                wdsp->createRxChannel(id, kFrames, 4096, 48000, 48000, 48000);
            if (!rx) {
                return false;
            }
            rx->setMode(DSPMode::USB);
            rx->setActive(true);
        }
        worker.setEngines(wdsp, audio);
        worker.setBufferSizes(kFrames, kFrames);
        worker.setStreamSlices(sliceA->streamIndex(), QVector<int>{a});
        radio.attachDspWorkerForTest(&worker);
        detach = std::make_unique<DspWorkerDetach>();
        detach->radio = &radio;
        return true;
    }

    FakeAudioBus* vaxOf(const SliceModel* slice) const
    {
        const int ch = slice->vaxChannel();
        return ch >= 1 && ch <= 4 ? vax[size_t(ch - 1)] : nullptr;
    }

    // One tone block into every stream a slice is on, through `w`.
    void feedOnce(RxDspWorker& w)
    {
        const QVector<float> iq = tone.next(kFrames);
        QVector<int> fed;
        for (const SliceModel* s : radio.slices()) {
            const int stream = s ? s->streamIndex() : -1;
            if (stream >= 0 && !fed.contains(stream)) {
                fed.append(stream);
                w.processIqBatch(stream, iq);
            }
        }
    }

    // `blocks` tone blocks; each waits for every RADE decoder and for the
    // receive lane to go idle so the run is paced by blocks, not by this
    // machine's load. The lane applies a slice's mode, filter, AGC and
    // run state (RxChannel::runKeyed / runOrdered); until it has, the
    // channel's blocks are silent, so without the wait how many blocks a
    // new slice stays silent for depends on how soon the lane thread runs.
    bool feed(int blocks)
    {
        for (int i = 0; i < blocks; ++i) {
            if (!radio.waitForReceiveLaneForTest(5000)) {
                return false;
            }
            feedOnce(worker);
            for (const SliceModel* s : radio.slices()) {
                RadeChannel* const ch = s ? wdsp->radeChannel(s->sliceIndex()) : nullptr;
                if (ch && !ch->waitRxIdleForTest(5000)) {
                    return false;
                }
            }
        }
        return true;
    }

    // Feeds until `bus` has had audible audio since byte `from` (WDSP's
    // buffering and AGC first). The blocks it took, or -1 if none in
    // `maxBlocks`.
    int feedUntilAudible(const FakeAudioBus* bus, qsizetype from, int maxBlocks = 2048)
    {
        for (int i = 1; i <= maxBlocks; ++i) {
            if (!feed(1)) {
                return -1;
            }
            if (peakSince(bus, from) > kAudible) {
                return i;
            }
        }
        return -1;
    }

    bool decodesOnItsOwnThread(RadeChannel* channel, QString* evidence)
    {
        std::atomic<Qt::HANDLE> decodedOn{nullptr};
        const QMetaObject::Connection probe = QObject::connect(
            channel, &RadeChannel::rxSpeechReady, channel,
            [&decodedOn](const QByteArray&) {
                decodedOn.store(QThread::currentThreadId());
            }, Qt::DirectConnection);
        const int callsBefore = channel->radeRxCallCountForTest();
        const bool idle = feed(512);  // past both resamplers and one rade_rx
        QObject::disconnect(probe);
        const Qt::HANDLE on = decodedOn.load();
        *evidence = QStringLiteral("idle=%1 radeRx=%2 decodedOnDecoderThread=%3 onMain=%4")
            .arg(idle)
            .arg(channel->radeRxCallCountForTest() - callsBefore)
            .arg(on != nullptr && on == channel->rxThreadIdForTest())
            .arg(on == QThread::currentThreadId());
        return idle && channel->radeRxCallCountForTest() > callsBefore
            && on != nullptr && on != QThread::currentThreadId()
            && on == channel->rxThreadIdForTest();
    }
};

MirrorUpdate utf8Arg(const QByteArray& name, const QString& value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
}

// The way a slice is made: this computer's own +RX (addSliceOnPan), or a
// phone's session verb through the Core's dispatcher.
enum class CreatePath { LocalOnPan, PhoneAddSlice, PhoneAddSliceOnPan };

int createSlice(RadioModel& radio, CreatePath path, const QString& panId)
{
    if (path == CreatePath::LocalOnPan) {
        QSignalSpy added(&radio, &RadioModel::sliceAdded);
        radio.addSliceOnPan(panId);
        return added.isEmpty() ? -1 : added.last().at(0).toInt();
    }
    SessionCommandDispatcher dispatcher(&radio);
    dispatcher.setRequester(QByteArrayLiteral("phone"));
    bool succeeded = false;
    QObject::connect(&dispatcher, &SessionCommandDispatcher::commandResultReady,
                     &dispatcher, [&succeeded](const SessionMessage& result) {
                         succeeded = result.accepted;
                     });
    QSignalSpy added(&radio, &RadioModel::sliceAdded);
    if (path == CreatePath::PhoneAddSlice) {
        dispatcher.dispatch(SessionMessages::commandInvoke(
            "addSlice", 1, {utf8Arg("initialPanId", panId)}));
    } else {
        dispatcher.dispatch(SessionMessages::commandInvoke(
            "addSliceOnPan", 1, {utf8Arg("panId", panId)}));
    }
    if (!succeeded || added.isEmpty()) {
        return -1;
    }
    return added.last().at(0).toInt();
}

}  // namespace

class TestRadeRxMultisliceRouting : public QObject {
    Q_OBJECT

private slots:
    // The late bound is one rade_rx input (rade_nin_max, 1120 samples at
    // 8 kHz = 140 ms) in the mixer's own blocks.
    void lateBoundIsOneRadeFrameOfBlocks()
    {
        QCOMPARE(kRadeNinMax8k, 1120);
        QCOMPARE(radeLateBoundBlocks(64), 105);
        QCOMPARE(radeLateBoundBlocks(1024), 7);
    }

    // The due record plays; one older than its slot, or from a replaced
    // route, is dropped rather than played late; a later one waits.
    void bridgePlaysDueBlockAndDropsLateOnes()
    {
        RadeRxBridge bridge;
        const quint32 stale = bridge.nextEpoch();
        const quint32 epoch = bridge.nextEpoch();
        const float speech[8] = {1, 1, 2, 2, 3, 3, 4, 4};
        std::vector<float> out;

        QVERIFY(bridge.pushOutput({stale, 1, 4}, speech, 4));
        QVERIFY(bridge.pushOutput({epoch, 0, 4}, speech, 4));
        QVERIFY(bridge.pushOutput({epoch, 1, 4}, speech, 4));
        QVERIFY(bridge.pushOutput({epoch, 5, 4}, speech, 4));

        QCOMPARE(bridge.takeDue(epoch, 1, out), 4);
        QCOMPARE(bridge.lateDrops(), quint64(2));  // other epoch, and seq 0
        QCOMPARE(out.size(), size_t(8));
        QCOMPARE(out[6], 4.0f);

        QCOMPARE(bridge.takeDue(epoch, 3, out), 0);  // seq 5 is early: kept
        QCOMPARE(bridge.lateDrops(), quint64(2));
        QCOMPARE(bridge.takeDue(epoch, 5, out), 4);
        QCOMPARE(bridge.takeDue(epoch, 6, out), 0);  // nothing: silence
    }

    // Ruling 1: both RADE slices decode at once, each on its own thread,
    // which is not the main thread, and both reach the speakers.
    void twoRadeSlicesDecodeAtOnceOnTheirOwnThreads()
    {
        TwoSliceRig rig;
        QVERIFY(rig.setUp());
        RadeChannel* const radeA = rig.toRade(rig.sliceA);
        RadeChannel* const radeB = rig.toRade(rig.sliceB);
        QVERIFY(radeA && radeB && radeA != radeB);
        QVERIFY(radeA->isActive() && radeB->isActive());
        QCOMPARE(rig.worker.radeRxRouteCount(), 2);
        const std::shared_ptr<RadeRxBridge> bridgeA = radeA->rxBridge();
        const std::shared_ptr<RadeRxBridge> bridgeB = radeB->rxBridge();
        QVERIFY(bridgeA && bridgeB);

        // Which thread runs processIq's body, seen from inside it.
        std::atomic<Qt::HANDLE> decodedOnA{nullptr};
        std::atomic<Qt::HANDLE> decodedOnB{nullptr};
        connect(radeA, &RadeChannel::rxSpeechReady, radeA,
                [&decodedOnA](const QByteArray&) {
                    decodedOnA.store(QThread::currentThreadId());
                }, Qt::DirectConnection);
        connect(radeB, &RadeChannel::rxSpeechReady, radeB,
                [&decodedOnB](const QByteArray&) {
                    decodedOnB.store(QThread::currentThreadId());
                }, Qt::DirectConnection);
        QSignalSpy speechA(radeA, &RadeChannel::rxSpeechReady);
        QSignalSpy speechB(radeB, &RadeChannel::rxSpeechReady);

        const int vaxABefore = rig.vaxA->pushCount();
        const int vaxBBefore = rig.vaxB->pushCount();
        const int masterBefore = rig.speakers->pushCount();
        // Past the resamplers, one rade_rx, and well past the late bound
        // (105 blocks), so decoded blocks come back into their slots.
        constexpr int kBlocks = 512;
        QVERIFY(kBlocks > 2 * radeLateBoundBlocks(kFrames));
        for (int i = 0; i < kBlocks; ++i) {
            rig.worker.processIqBatch(0, rig.iq);
            rig.worker.processIqBatch(1, rig.iq);
            QVERIFY(radeA->waitRxIdleForTest(5000));
            QVERIFY(radeB->waitRxIdleForTest(5000));
        }

        const QString evidence = QStringLiteral(
            "speechA=%1 speechB=%2 radeRxA=%3 radeRxB=%4 vaxA=%5 vaxB=%6 master=%7 "
            "playedA=%8 playedB=%9")
            .arg(speechA.count()).arg(speechB.count())
            .arg(radeA->radeRxCallCountForTest()).arg(radeB->radeRxCallCountForTest())
            .arg(rig.vaxA->pushCount() - vaxABefore)
            .arg(rig.vaxB->pushCount() - vaxBBefore)
            .arg(rig.speakers->pushCount() - masterBefore)
            .arg(bridgeA->playedSlots())
            .arg(bridgeB->playedSlots());
        // What each decoder returned came back to the mixer in its own slot,
        // not only silence slots.
        QVERIFY2(bridgeA->playedSlots() > 0 && bridgeB->playedSlots() > 0,
                 qPrintable(evidence));
        // Both got input and ran the codec, both produced output.
        QVERIFY2(radeA->radeRxCallCountForTest() > 0
                     && radeB->radeRxCallCountForTest() > 0, qPrintable(evidence));
        QVERIFY2(speechA.count() > 0 && speechB.count() > 0, qPrintable(evidence));
        QVERIFY2(rig.vaxA->pushCount() > vaxABefore
                     && rig.vaxB->pushCount() > vaxBBefore
                     && rig.speakers->pushCount() > masterBefore,
                 qPrintable(evidence));

        // Each decoded on its own thread, named for its slice.
        const Qt::HANDLE mainThread = QThread::currentThreadId();
        QVERIFY(decodedOnA.load() != nullptr && decodedOnB.load() != nullptr);
        QVERIFY(decodedOnA.load() != mainThread);
        QVERIFY(decodedOnB.load() != mainThread);
        QVERIFY(decodedOnA.load() != decodedOnB.load());
        QCOMPARE(decodedOnA.load(), radeA->rxThreadIdForTest());
        QCOMPARE(decodedOnB.load(), radeB->rxThreadIdForTest());
        QCOMPARE(radeA->rxThreadNameForTest(), QStringLiteral("RadeRx%1").arg(rig.a));
        QCOMPARE(radeB->rxThreadNameForTest(), QStringLiteral("RadeRx%1").arg(rig.b));
        // The tick log counts per channel.
        QCOMPARE(radeA->rxTickCountForTest(), speechA.count());
        QCOMPARE(radeB->rxTickCountForTest(), speechB.count());
    }

    // Ruling 2: a decoder that never returns costs its own slice silence,
    // never the other slices' time, and its late speech is dropped.
    void aStalledDecoderNeverHoldsTheOtherSlices()
    {
        // The gate outlives the rig (whose teardown joins the decoder), and
        // is opened on every exit path so a failed check cannot hang it.
        Gate gate;
        TwoSliceRig rig;
        struct OpenOnExit {
            Gate& gate;
            ~OpenOnExit() { gate.release(); }
        } openOnExit{gate};
        QVERIFY(rig.setUp());
        RadeChannel* const radeB = rig.toRade(rig.sliceB);
        QVERIFY(radeB && radeB->isActive());
        std::shared_ptr<RadeRxBridge> bridge = radeB->rxBridge();
        QVERIFY(bridge);

        // Both slices in the mixer first.
        for (int i = 0; i < 8; ++i) {
            rig.worker.processIqBatch(0, rig.iq);
            rig.worker.processIqBatch(1, rig.iq);
        }
        QVERIFY(radeB->waitRxIdleForTest(5000));

        radeB->setRxStallHookForTest([&gate] { gate.hold(); });
        // Hold the decoder first: feed until it is inside the hook (the
        // decoder thread may start late on a loaded machine), then count.
        for (int i = 0; i < 512 && gate.entered.load() == 0; ++i) {
            rig.worker.processIqBatch(0, rig.iq);
            rig.worker.processIqBatch(1, rig.iq);
            QTest::qWait(1);
        }
        QTRY_VERIFY_WITH_TIMEOUT(gate.entered.load() == 1, 5000);
        const int masterBefore = rig.speakers->pushCount();
        const int vaxABefore = rig.vaxA->pushCount();
        const int vaxBBefore = rig.vaxB->pushCount();
        const quint64 silentBefore = bridge->silentSlots();
        const int lateBound = radeLateBoundBlocks(kFrames);
        const int blocks = 2 * lateBound;
        for (int i = 0; i < blocks; ++i) {
            rig.worker.processIqBatch(0, rig.iq);
            rig.worker.processIqBatch(1, rig.iq);
        }
        const int masterDelta = rig.speakers->pushCount() - masterBefore;
        const int vaxADelta = rig.vaxA->pushCount() - vaxABefore;
        const int vaxBDelta = rig.vaxB->pushCount() - vaxBBefore;
        const quint64 silentDelta = bridge->silentSlots() - silentBefore;
        const QString evidence = QStringLiteral(
            "blocks=%1 entered=%2 master=%3 vaxA=%4 vaxB=%5 silentSlots=%6 inputDrops=%7")
            .arg(blocks).arg(gate.entered.load()).arg(masterDelta)
            .arg(vaxADelta).arg(vaxBDelta).arg(silentDelta).arg(bridge->inputDrops());

        // The decoder is held for the whole run...
        QVERIFY2(gate.entered.load() == 1, qPrintable(evidence));
        // ...and the mix and both slices kept the worker's pace: one mixed
        // block per block fed, B contributing silence.
        QVERIFY2(masterDelta == blocks && vaxADelta == blocks && vaxBDelta == blocks,
                 qPrintable(evidence));
        // Every slot after the late bound found nothing due.
        QVERIFY2(silentDelta >= quint64(blocks - lateBound), qPrintable(evidence));

        // Let go: what it decodes now is past its slot and dropped.
        const quint64 lateBefore = bridge->lateDrops();
        gate.release();
        QVERIFY(radeB->waitRxIdleForTest(5000));
        rig.worker.processIqBatch(0, rig.iq);
        rig.worker.processIqBatch(1, rig.iq);
        QVERIFY2(bridge->lateDrops() > lateBefore,
                 qPrintable(QStringLiteral("lateDrops=%1").arg(bridge->lateDrops())));
        radeB->setRxStallHookForTest({});
    }

    // A closed slice takes its decoder; the id's next slice gets a fresh one.
    void removingARadeSliceDestroysItsDecoder()
    {
        TwoSliceRig rig;
        QVERIFY(rig.setUp());
        QVERIFY(rig.toRade(rig.sliceB));
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);

        rig.radio.removeSlice(rig.b);
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(rig.b) == nullptr);
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);

        const int reused = rig.radio.addSlice();
        QCOMPARE(reused, rig.b);
        SliceModel* const fresh = rig.radio.sliceById(reused);
        QVERIFY(fresh && fresh != rig.sliceB);
        QTest::failOnWarning(QRegularExpression(QStringLiteral("already exists")));
        RadeChannel* const again = rig.toRade(fresh);
        QVERIFY(again && again->isActive());
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
    }

    // RADE TX stays on one channel: the TX slice's. The other RADE slice
    // hears the microphone blocks but encodes nothing, and only the keyed
    // slice's decoder stops.
    void onlyTheTxSliceChannelEncodes()
    {
        TwoSliceRig rig;
        QVERIFY(rig.setUp());
        RadeChannel* const radeA = rig.toRade(rig.sliceA);
        RadeChannel* const radeB = rig.toRade(rig.sliceB);
        QVERIFY(radeA && radeB);

        QVERIFY(rig.radio.txSliceArbiter()->requestHandoff(rig.b));
        QVERIFY(!radeA->txSelected());
        QVERIFY(radeB->txSelected());
        QVERIFY(!radeA->rxGated() && !radeB->rxGated());

        // Model signal only; no radio is attached.
        QVERIFY(QMetaObject::invokeMethod(
            rig.radio.moxController(), "moxStateChanged", Qt::DirectConnection,
            Q_ARG(bool, true)));
        QVERIFY(radeB->rxGated());
        QVERIFY(!radeA->rxGated());
        QVERIFY(QMetaObject::invokeMethod(
            rig.radio.moxController(), "moxStateChanged", Qt::DirectConnection,
            Q_ARG(bool, false)));
        QVERIFY(!radeB->rxGated());

        // An unselected channel takes the block and encodes nothing.
        QByteArray speech16k(16000 * int(sizeof(int16_t)), '\0');
        radeA->txEncode(speech16k);
        QCOMPARE(radeA->radeTxCallCountForTest(), 0);
        radeB->txEncode(speech16k);
        QVERIFY(radeB->radeTxCallCountForTest() > 0);

        QVERIFY(rig.radio.txSliceArbiter()->requestHandoff(rig.a));
        QVERIFY(radeA->txSelected());
        QVERIFY(!radeB->txSelected());
    }

    // Review Critical: the TX slice's own decoder holds the codec for a
    // whole decode. Unkeyed, with that slice also decoding, a microphone
    // block that reaches txEncode then must not wait for the decode: it is
    // held and encoded with the next call once the codec is free.
    void txEncodeNeverWaitsForADecode()
    {
        Gate gate;
        TwoSliceRig rig;
        struct OpenOnExit {
            Gate& gate;
            ~OpenOnExit() { gate.release(); }
        } openOnExit{gate};
        QVERIFY(rig.setUp());
        RadeChannel* const radeB = rig.toRade(rig.sliceB);
        QVERIFY(radeB && radeB->isActive());
        QVERIFY(rig.radio.txSliceArbiter()->requestHandoff(rig.b));
        QVERIFY(radeB->txSelected());
        QVERIFY(!rig.radio.mox());
        QVERIFY(!radeB->rxGated());

        // Hold the decoder inside a decode, the codec taken. Blocks reach
        // the decoder once the DSP worker's 48 -> 24 kHz resampler has
        // filled, so feed until it is in (bounded).
        radeB->setRxDecodeLockedHookForTest([&gate] { gate.hold(); });
        for (int i = 0; i < 512 && gate.entered.load() == 0; ++i) {
            rig.worker.processIqBatch(1, rig.iq);
            QTest::qWait(1);
        }
        QTRY_VERIFY_WITH_TIMEOUT(gate.entered.load() == 1, 5000);

        // A watchdog frees the decoder after 5 s, so a txEncode that waited
        // would return only then; one that never waits returns long before.
        std::atomic<bool> watchdogFired{false};
        std::atomic<bool> done{false};
        std::thread watchdog([&] {
            for (int i = 0; i < 500 && !done.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!done.load()) {
                watchdogFired.store(true);
                gate.release();
            }
        });
        struct JoinOnExit {
            std::thread& thread;
            std::atomic<bool>& done;
            ~JoinOnExit()
            {
                done.store(true);
                if (thread.joinable()) {
                    thread.join();
                }
            }
        } joinOnExit{watchdog, done};

        constexpr int kCalls = 10;
        constexpr int kSamples = 1600;  // 100 ms at 16 kHz each, 1 s in all
        const QByteArray block(kSamples * int(sizeof(int16_t)), '\0');
        qint64 slowestMs = 0;
        for (int i = 0; i < kCalls; ++i) {
            QElapsedTimer timer;
            timer.start();
            radeB->txEncode(block);
            slowestMs = std::max(slowestMs, timer.elapsed());
        }
        done.store(true);
        const QString evidence = QStringLiteral(
            "watchdog=%1 slowestMs=%2 busy=%3 held=%4 radeTx=%5 heldDrops=%6")
            .arg(watchdogFired.load()).arg(slowestMs)
            .arg(radeB->txCodecBusyCountForTest()).arg(radeB->txHeldBytesForTest())
            .arg(radeB->radeTxCallCountForTest()).arg(radeB->txHeldDrops());
        // No call waited for the decode, which was still running.
        QVERIFY2(!watchdogFired.load(), qPrintable(evidence));
        QVERIFY2(gate.entered.load() == 1, qPrintable(evidence));
        QVERIFY2(slowestMs < 5000, qPrintable(evidence));
        QVERIFY2(radeB->txCodecBusyCountForTest() == kCalls, qPrintable(evidence));
        QVERIFY2(radeB->txHeldBytesForTest() == kCalls * block.size(), qPrintable(evidence));
        QVERIFY2(radeB->radeTxCallCountForTest() == 0, qPrintable(evidence));
        QVERIFY2(radeB->txHeldDrops() == 0, qPrintable(evidence));

        // Once the decode ends, the next block encodes, held blocks first.
        radeB->setRxDecodeLockedHookForTest({});
        gate.release();
        QVERIFY(radeB->waitRxIdleForTest(5000));
        radeB->txEncode(block);
        QCOMPARE(radeB->txHeldBytesForTest(), 0);
        QVERIFY2(radeB->radeTxCallCountForTest() > 0,
                 qPrintable(QStringLiteral("radeTx=%1").arg(radeB->radeTxCallCountForTest())));
    }

    // Review Important 1: after a reconnect the TX worker is a new one, and
    // only the channels re-wired at that point know it. Moving the
    // transmitter to another RADE slice connects that slice's channel to
    // the current worker, so its microphone blocks reach txEncode. Nothing
    // is keyed: the worker's signal is emitted by the test.
    void aNewTxWorkerReachesTheSliceTheTransmitterMovesTo()
    {
        TwoSliceRig rig;
        QVERIFY(rig.setUp(3));
        RadeChannel* const radeA = rig.toRade(rig.sliceA);
        RadeChannel* const radeB = rig.toRade(rig.sliceB);
        RadeChannel* const radeC = rig.toRade(rig.sliceC);
        QVERIFY(radeA && radeB && radeC);
        QCOMPARE(rig.worker.radeRxRouteCount(), 3);

        // The first connection's worker, with A transmitting.
        rig.radio.installTxWorkerForTest(std::make_unique<TxWorkerThread>());
        QVERIFY(rig.radio.txSliceArbiter()->requestHandoff(rig.a));
        // A reconnect: that worker goes and a new one takes its place.
        auto fresh = std::make_unique<TxWorkerThread>();
        TxWorkerThread* const worker = fresh.get();
        rig.radio.installTxWorkerForTest(std::move(fresh));

        // The transmitter moves to C, a third RADE slice.
        QVERIFY(rig.radio.txSliceArbiter()->requestHandoff(rig.c));
        QVERIFY(radeC->txSelected());
        QVERIFY(!radeA->txSelected() && !radeB->txSelected());
        QCOMPARE(worker->radeChannelForTest(), radeC);

        QByteArray speech16k(16000 * int(sizeof(int16_t)), '\0');
        emit worker->radeMicBlockReady(speech16k);
        QCoreApplication::processEvents();
        QVERIFY2(radeC->radeTxCallCountForTest() > 0,
                 qPrintable(QStringLiteral("radeTxC=%1").arg(radeC->radeTxCallCountForTest())));
        QCOMPARE(radeA->radeTxCallCountForTest(), 0);
        QCOMPARE(radeB->radeTxCallCountForTest(), 0);
    }

    // JJ's bench, 2026-09-30: a device's RADE slice closed after the grace
    // (saved, then removed) and made again on reconnect read RADE but
    // played its sideband audio. restoreFromSettings set the mode without
    // the RADE start, and the setDspMode after it saw no change. It now
    // decodes again, on its own decoder thread.
    void aRestoredRadeSliceDecodesAgain()
    {
        AppSettings::instance().clear();
        TwoSliceRig rig;
        QVERIFY(rig.setUp());
        rig.sliceB->setFrequency(14200000.0);
        QVERIFY(rig.toRade(rig.sliceB));

        // What the grace end does (StationServer releaseDeviceClaims):
        // save the slice, then close it.
        const Band band = bandFromFrequency(rig.sliceB->frequency());
        rig.sliceB->saveToSettings(band);
        ReceiveSliceState saved;
        saved.id = rig.b;
        saved.panKey = rig.sliceB->panKey();
        saved.frequencyHz = rig.sliceB->frequency();
        saved.dspMode = rig.sliceB->dspMode();
        rig.radio.removeSlice(rig.b);
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(rig.b) == nullptr);

        // Readmission (StationServer placeSlicesForAdmission).
        QTest::failOnWarning(QRegularExpression(QStringLiteral("already exists")));
        QString reason;
        const int restored = rig.radio.restoreSliceFor(QByteArrayLiteral("phone"), rig.b,
                                                       saved, &reason);
        QVERIFY2(restored == rig.b, qPrintable(reason));
        QCoreApplication::processEvents();  // the route reaches the worker
        SliceModel* const slice = rig.radio.sliceById(restored);
        QVERIFY(slice);
        QCOMPARE(slice->dspMode(), DSPMode::RADE_U);

        RadeChannel* const channel = rig.wdsp->radeChannel(restored);
        QVERIFY(channel);
        QVERIFY(channel->isActive());
        QVERIFY(channel->sidebandUpper());
        QVERIFY(channel->rxWorkerRunning());
        QCOMPARE(channel->rxThreadNameForTest(), QStringLiteral("RadeRx%1").arg(restored));
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
        QString evidence;
        QVERIFY2(rig.decodesOnItsOwnThread(channel, &evidence), qPrintable(evidence));
    }

    // The band buttons restore a band's saved mode the same way. A RADE
    // slice keeps decoding across them, a band saved in RADE starts its
    // decoder, and a band saved in another mode stops it.
    void bandButtonsKeepARadeSliceDecoding()
    {
        AppSettings::instance().clear();
        TwoSliceRig rig;
        QVERIFY(rig.setUp());
        rig.sliceA->setFrequency(14200000.0);
        QVERIFY(rig.toRade(rig.sliceA));  // 20 m in RADE-U

        // 40 m, first visit: its seed mode, not RADE. No decoder.
        rig.radio.onBandButtonClicked(rig.sliceA, Band::Band40m);
        QCoreApplication::processEvents();
        QVERIFY(rig.sliceA->dspMode() != DSPMode::RADE_U
                && rig.sliceA->dspMode() != DSPMode::RADE_L);
        QVERIFY(rig.wdsp->radeChannel(rig.a) == nullptr);
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        rig.sliceA->setDspMode(DSPMode::RADE_L);  // 40 m in RADE-L
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(rig.a));

        // Back to 20 m, saved in RADE-U: the decoder follows the band.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("already exists")));
        rig.radio.onBandButtonClicked(rig.sliceA, Band::Band20m);
        QCoreApplication::processEvents();
        QCOMPARE(rig.sliceA->dspMode(), DSPMode::RADE_U);
        RadeChannel* channel = rig.wdsp->radeChannel(rig.a);
        QVERIFY(channel && channel->isActive() && channel->sidebandUpper());
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
        QString evidence;
        QVERIFY2(rig.decodesOnItsOwnThread(channel, &evidence), qPrintable(evidence));

        // And to 40 m, saved in RADE-L: still decoding.
        rig.radio.onBandButtonClicked(rig.sliceA, Band::Band40m);
        QCoreApplication::processEvents();
        QCOMPARE(rig.sliceA->dspMode(), DSPMode::RADE_L);
        channel = rig.wdsp->radeChannel(rig.a);
        QVERIFY(channel && channel->isActive() && !channel->sidebandUpper());
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
        QVERIFY2(rig.decodesOnItsOwnThread(channel, &evidence), qPrintable(evidence));

        // 80 m, first visit, is saved in its seed mode; coming back to it
        // from a RADE band restores that mode and stops the decoder.
        rig.radio.onBandButtonClicked(rig.sliceA, Band::Band80m);
        QCoreApplication::processEvents();
        const DSPMode seed80 = rig.sliceA->dspMode();
        QVERIFY(rig.wdsp->radeChannel(rig.a) == nullptr);
        rig.radio.onBandButtonClicked(rig.sliceA, Band::Band20m);
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(rig.a));
        rig.radio.onBandButtonClicked(rig.sliceA, Band::Band80m);
        QCoreApplication::processEvents();
        QCOMPARE(rig.sliceA->dspMode(), seed80);
        QVERIFY(rig.wdsp->radeChannel(rig.a) == nullptr);
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
    }

    // A slice restored in another mode makes no decoder.
    void aSliceRestoredInAnotherModeHasNoDecoder()
    {
        AppSettings::instance().clear();
        TwoSliceRig rig;
        QVERIFY(rig.setUp());
        rig.sliceB->setFrequency(7150000.0);
        rig.sliceB->setDspMode(DSPMode::LSB);
        rig.sliceB->saveToSettings(bandFromFrequency(rig.sliceB->frequency()));
        ReceiveSliceState saved;
        saved.id = rig.b;
        saved.panKey = rig.sliceB->panKey();
        saved.frequencyHz = rig.sliceB->frequency();
        saved.dspMode = DSPMode::LSB;
        rig.radio.removeSlice(rig.b);
        QCoreApplication::processEvents();

        QString reason;
        const int restored = rig.radio.restoreSliceFor(QByteArrayLiteral("phone"), rig.b,
                                                       saved, &reason);
        QVERIFY2(restored == rig.b, qPrintable(reason));
        QCoreApplication::processEvents();
        QCOMPARE(rig.radio.sliceById(restored)->dspMode(), DSPMode::LSB);
        QVERIFY(rig.wdsp->radeChannel(restored) == nullptr);
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
    }

    // ── RADE gaps (2026-09-30) ──────────────────────────────────────────

    // Item A. A routed RADE slice whose decoder never syncs plays silence,
    // every block, and never its WDSP sideband; a USB slice beside it plays.
    // freedv-gui does the same: RADEReceiveStep::execute outputs only FARGAN
    // speech, none while rade_rx returns no features (RADEReceiveStep.cpp:
    // 196-270 [@77e793a]); the demodulated audio is heard only when the
    // operator picks Analog (TxRxThread.cpp:483-495 [@77e793a]).
    void aRadeSliceWithoutSyncPlaysSilenceNeverItsSideband()
    {
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/2));
        const int b = createSlice(rig.radio, CreatePath::LocalOnPan, QStringLiteral("pan-b"));
        QVERIFY(b >= 0);
        QCoreApplication::processEvents();  // B's stream binding reaches the worker
        SliceModel* const sliceB = rig.radio.sliceById(b);
        QVERIFY(sliceB && sliceB->streamIndex() != rig.sliceA->streamIndex());
        sliceB->setDspMode(DSPMode::USB);
        sliceB->setVaxChannel(2);
        FakeAudioBus* const vaxA = rig.vaxOf(rig.sliceA);
        FakeAudioBus* const vaxB = rig.vaxOf(sliceB);

        // The tone is loud on A's sideband while A is in USB.
        QVERIFY2(rig.feedUntilAudible(vaxA, 0) > 0,
                 qPrintable(QStringLiteral("usbPeakA=%1").arg(peakSince(vaxA, 0))));

        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();  // the route reaches the worker
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        QVERIFY(radeA && radeA->isActive());
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);

        const qsizetype fromA = vaxA->buffer().size();
        const qsizetype fromB = vaxB->buffer().size();
        const int blocks = 640;  // past both resamplers, several rade_rx calls and the late bound
        QVERIFY(blocks > 3 * radeLateBoundBlocks(kFrames));
        QVERIFY(rig.feed(blocks));
        const qsizetype bytesA = vaxA->buffer().size() - fromA;
        const QString evidence = QStringLiteral(
            "blocks=%1 bytesA=%2 peakA=%3 peakB=%4 synced=%5 radeRx=%6 played=%7 silent=%8")
            .arg(blocks).arg(bytesA).arg(peakSince(vaxA, fromA)).arg(peakSince(vaxB, fromB))
            .arg(radeA->isSynced()).arg(radeA->radeRxCallCountForTest())
            .arg(radeA->rxBridge()->playedSlots()).arg(radeA->rxBridge()->silentSlots());
        // The decoder ran and never locked to the tone.
        QVERIFY2(radeA->radeRxCallCountForTest() > 0 && !radeA->isSynced(),
                 qPrintable(evidence));
        // A block every tick, and every sample of it silent.
        QVERIFY2(bytesA == qsizetype(blocks) * kFrames * 2 * qsizetype(sizeof(float)),
                 qPrintable(evidence));
        QVERIFY2(peakSince(vaxA, fromA) == 0.0f, qPrintable(evidence));
        // The USB slice beside it still plays.
        QVERIFY2(peakSince(vaxB, fromB) > kAudible, qPrintable(evidence));
    }

    // Item B. A slice in RADE whose route has not reached the worker (the
    // blocks after the mode change, or a new DSP worker before the replay
    // lands) is muted. It does not play its sideband.
    void aRadeSliceWithNoRouteYetIsMuted()
    {
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        FakeAudioBus* const vaxA = rig.vaxOf(rig.sliceA);
        QVERIFY(rig.feedUntilAudible(vaxA, 0) > 0);

        // The mode changes; the route is queued, not yet on the worker.
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        QCOMPARE(rig.worker.radeModeSlices(), 1u << rig.a);
        qsizetype from = vaxA->buffer().size();
        constexpr int kTransient = 8;
        for (int i = 0; i < kTransient; ++i) {
            rig.feedOnce(rig.worker);
        }
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        QCOMPARE(vaxA->buffer().size() - from,
                 qsizetype(kTransient) * kFrames * 2 * qsizetype(sizeof(float)));
        QVERIFY2(peakSince(vaxA, from) == 0.0f,
                 qPrintable(QStringLiteral("peakBeforeRoute=%1").arg(peakSince(vaxA, from))));
        QCoreApplication::processEvents();
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);

        // A new DSP worker, as after a radio recovery: until the replayed
        // route lands it has no route for A, and A is still silent.
        RxDspWorker second;
        second.setEngines(rig.wdsp, rig.radio.audioEngine());
        second.setBufferSizes(kFrames, kFrames);
        second.setStreamSlices(rig.sliceA->streamIndex(), QVector<int>{rig.a});
        struct Reattach {
            RadioModel& radio;
            RxDspWorker& worker;
            ~Reattach()
            {
                radio.attachDspWorkerForTest(&worker);
                QCoreApplication::processEvents();
            }
        } reattach{rig.radio, rig.worker};
        rig.radio.attachDspWorkerForTest(&second);
        QCOMPARE(second.radeRxRouteCount(), 0);
        QCOMPARE(second.radeModeSlices(), 1u << rig.a);
        from = vaxA->buffer().size();
        for (int i = 0; i < kTransient; ++i) {
            rig.feedOnce(second);
        }
        QCOMPARE(second.radeRxRouteCount(), 0);
        QCOMPARE(vaxA->buffer().size() - from,
                 qsizetype(kTransient) * kFrames * 2 * qsizetype(sizeof(float)));
        QVERIFY2(peakSince(vaxA, from) == 0.0f,
                 qPrintable(QStringLiteral("peakNewWorker=%1").arg(peakSince(vaxA, from))));
        QCoreApplication::processEvents();
        QCOMPARE(second.radeRxRouteCount(), 1);

        // Out of RADE, the sideband plays again once the route has gone.
        rig.radio.attachDspWorkerForTest(&rig.worker);
        QCoreApplication::processEvents();
        rig.sliceA->setDspMode(DSPMode::USB);
        QCoreApplication::processEvents();
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        QCOMPARE(rig.worker.radeModeSlices(), 0u);
        from = vaxA->buffer().size();
        QVERIFY2(rig.feedUntilAudible(vaxA, from) > 0,
                 qPrintable(QStringLiteral("usbPeakAgain=%1").arg(peakSince(vaxA, from))));
    }

    // Item C. With A in RADE and decoding, a new slice B, made locally or
    // by a phone, in USB or in RADE, on A's pan or a new one, leaves A
    // routed, on the same decoder and channel, silent (no sideband), and
    // decoding on its own thread. B in RADE decodes on its own thread too.
    void addingASliceNeverDisturbsADecodingRadeSlice_data()
    {
        QTest::addColumn<int>("path");
        QTest::addColumn<bool>("newPan");
        QTest::addColumn<bool>("bInRade");
        const struct {
            const char* name;
            CreatePath path;
            bool newPan;
        } paths[] = {
            {"local +RX on A's pan", CreatePath::LocalOnPan, false},
            {"local new pan", CreatePath::LocalOnPan, true},
            {"phone addSlice on A's pan", CreatePath::PhoneAddSlice, false},
            {"phone addSliceOnPan on A's pan", CreatePath::PhoneAddSliceOnPan, false},
            {"phone addSliceOnPan new pan", CreatePath::PhoneAddSliceOnPan, true},
        };
        for (const auto& p : paths) {
            QTest::newRow(qPrintable(QStringLiteral("%1, B in USB").arg(p.name)))
                << int(p.path) << p.newPan << false;
            QTest::newRow(qPrintable(QStringLiteral("%1, B in RADE").arg(p.name)))
                << int(p.path) << p.newPan << true;
        }
    }

    void addingASliceNeverDisturbsADecodingRadeSlice()
    {
        QFETCH(int, path);
        QFETCH(bool, newPan);
        QFETCH(bool, bInRade);
        AppSettings::instance().clear();
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/2));
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        QVERIFY(radeA && radeA->isActive());
        const std::shared_ptr<RadeRxBridge> bridgeA = radeA->rxBridge();
        RxChannel* const rxA = rig.wdsp->rxChannel(rig.a);
        const int streamA = rig.sliceA->streamIndex();
        FakeAudioBus* const vaxA = rig.vaxOf(rig.sliceA);
        QString evidence;
        QVERIFY2(rig.decodesOnItsOwnThread(radeA, &evidence), qPrintable(evidence));
        const Qt::HANDLE threadA = radeA->rxThreadIdForTest();

        const QString panId = newPan ? QStringLiteral("pan-new") : rig.sliceA->panKey();
        const int b = createSlice(rig.radio, CreatePath(path), panId);
        QVERIFY2(b >= 0 && b != rig.a, qPrintable(QStringLiteral("b=%1").arg(b)));
        QCoreApplication::processEvents();
        SliceModel* const sliceB = rig.radio.sliceById(b);
        QVERIFY(sliceB && sliceB->streamIndex() >= 0);
        QCOMPARE(sliceB->streamIndex() != streamA, newPan);
        sliceB->setDspMode(bInRade ? DSPMode::RADE_U : DSPMode::USB);
        sliceB->setVaxChannel(2);
        // A phone's slice is left out of this computer's VAX (ruling 5.14);
        // the phone hears the same block through its receiver tap. VAX is
        // only this test's window onto each slice's block, so carry all.
        rig.radio.audioEngine()->setVaxSliceMask(0xFFFFFFFFu);
        QCoreApplication::processEvents();

        // A is untouched: its id, stream, mode, WDSP channel, decoder and
        // rings are the ones it had, and its route is still on the worker.
        QCOMPARE(rig.radio.sliceById(rig.a), rig.sliceA);
        QCOMPARE(rig.sliceA->streamIndex(), streamA);
        QCOMPARE(rig.sliceA->dspMode(), DSPMode::RADE_U);
        QCOMPARE(rig.wdsp->rxChannel(rig.a), rxA);
        QCOMPARE(rig.wdsp->radeChannel(rig.a), radeA);
        QCOMPARE(radeA->rxBridge(), bridgeA);
        QVERIFY(radeA->isActive() && radeA->rxWorkerRunning());
        QCOMPARE(rig.worker.radeRxRouteCount(), bInRade ? 2 : 1);

        const qsizetype fromA = vaxA->buffer().size();
        FakeAudioBus* const vaxB = rig.vaxOf(sliceB);
        const qsizetype fromB = vaxB->buffer().size();
        const quint64 playedA = bridgeA->playedSlots();
        // A still decodes, on the same thread, and its speech still plays.
        QVERIFY2(rig.decodesOnItsOwnThread(radeA, &evidence), qPrintable(evidence));
        QCOMPARE(radeA->rxThreadIdForTest(), threadA);
        evidence = QStringLiteral("playedA=%1->%2 peakA=%3 peakB=%4")
            .arg(playedA).arg(bridgeA->playedSlots())
            .arg(peakSince(vaxA, fromA)).arg(peakSince(vaxB, fromB));
        QVERIFY2(bridgeA->playedSlots() > playedA, qPrintable(evidence));
        // JJ's report 2: A never falls back to its sideband. A block a tick
        // reached A's bus (decodesOnItsOwnThread feeds 512), all silent.
        QCOMPARE(vaxA->buffer().size() - fromA,
                 qsizetype(512) * kFrames * 2 * qsizetype(sizeof(float)));
        QVERIFY2(peakSince(vaxA, fromA) == 0.0f, qPrintable(evidence));

        if (bInRade) {
            RadeChannel* const radeB = rig.wdsp->radeChannel(b);
            QVERIFY(radeB && radeB != radeA && radeB->isActive());
            QVERIFY2(rig.decodesOnItsOwnThread(radeB, &evidence), qPrintable(evidence));
            QVERIFY(radeB->rxThreadIdForTest() != threadA);
            QVERIFY2(peakSince(vaxB, fromB) == 0.0f, qPrintable(evidence));
        } else {
            QVERIFY(rig.wdsp->radeChannel(b) == nullptr);
            QVERIFY2(peakSince(vaxB, fromB) > kAudible, qPrintable(evidence));
        }
    }

    // Item C, the refused create: a phone asks for a new pan when no DDC is
    // left. The half-made slice (seeded into RADE from A) leaves nothing
    // behind, and A decodes on.
    void aRefusedNewSliceLeavesADecodingRadeSliceAlone()
    {
        AppSettings::instance().clear();
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        QVERIFY(radeA);
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);

        const int b = createSlice(rig.radio, CreatePath::PhoneAddSliceOnPan,
                                  QStringLiteral("pan-new"));
        QCOMPARE(b, -1);
        QCoreApplication::processEvents();
        QCOMPARE(rig.radio.slices().size(), 1);
        for (int id = 0; id < WdspEngine::kMaxSliceChannels; ++id) {
            if (id != rig.a) {
                QVERIFY2(rig.wdsp->radeChannel(id) == nullptr,
                         qPrintable(QStringLiteral("left a decoder on %1").arg(id)));
            }
        }
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
        QCOMPARE(rig.wdsp->radeChannel(rig.a), radeA);
        FakeAudioBus* const vaxA = rig.vaxOf(rig.sliceA);
        const qsizetype fromA = vaxA->buffer().size();
        QString evidence;
        QVERIFY2(rig.decodesOnItsOwnThread(radeA, &evidence), qPrintable(evidence));
        QCOMPARE(vaxA->buffer().size() - fromA,
                 qsizetype(512) * kFrames * 2 * qsizetype(sizeof(float)));
        QCOMPARE(peakSince(vaxA, fromA), 0.0f);

        // The id is free for the next slice, with no "already exists".
        QTest::failOnWarning(QRegularExpression(QStringLiteral("already exists")));
        const int next = createSlice(rig.radio, CreatePath::LocalOnPan, rig.sliceA->panKey());
        QVERIFY(next >= 0);
        QCoreApplication::processEvents();
        SliceModel* const sliceNext = rig.radio.sliceById(next);
        QVERIFY(sliceNext);
        sliceNext->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(next) && rig.wdsp->radeChannel(next)->isActive());
        QCOMPARE(rig.worker.radeRxRouteCount(), 2);
    }

    // Fix round 1: a refused create gives the restored RADE owner (the
    // saved layout's RADE receiver) back to the slice that held it, not
    // leave it empty.
    void aRefusedNewSliceGivesTheRadeOwnerBack()
    {
        AppSettings::instance().clear();
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        rig.radio.prepareReceiveLayout(QString());  // the layout is managed
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        QCOMPARE(rig.radio.restoredRadeReceiveOwner(), std::optional<int>(rig.a));

        const int b = createSlice(rig.radio, CreatePath::PhoneAddSliceOnPan,
                                  QStringLiteral("pan-new"));
        QCOMPARE(b, -1);
        QCoreApplication::processEvents();
        QCOMPARE(rig.radio.slices().size(), 1);
        QCOMPARE(rig.radio.restoredRadeReceiveOwner(), std::optional<int>(rig.a));
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
    }

    // Fix round 1: every route is queued after its mode is committed, and
    // queueRadeRxRoute hands the worker the RADE-mode slices first, so the
    // mute is on the worker before the route (and its first block). Seen
    // on a path with no mode change (a re-wire, as radio recovery does),
    // with the worker's mask cleared first so only the queue can set it.
    void theRadeModeMaskReachesTheWorkerBeforeTheRoute()
    {
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        QVERIFY(radeA);

        rig.worker.setRadeModeSlices(0);
        rig.worker.clearRadeRxRoutes();
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        rig.radio.wireRadeChannel(rig.a, radeA, rig.sliceA);
        // The route is queued, not delivered; the mask is already set.
        QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        QCOMPARE(rig.worker.radeModeSlices(), 1u << rig.a);
        QCoreApplication::processEvents();
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
    }

    // Item D, JJ's report 1: two RADE slices on one pan (one DDC) are both
    // routed and both decode; neither plays USB.
    void twoRadeSlicesOnOnePanBothDecodeAndNeitherPlaysUsb()
    {
        AppSettings::instance().clear();
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        const int b = createSlice(rig.radio, CreatePath::LocalOnPan, rig.sliceA->panKey());
        QVERIFY(b >= 0);
        QCoreApplication::processEvents();
        SliceModel* const sliceB = rig.radio.sliceById(b);
        QVERIFY(sliceB);
        QCOMPARE(sliceB->streamIndex(), rig.sliceA->streamIndex());
        sliceB->setDspMode(DSPMode::RADE_U);
        sliceB->setVaxChannel(2);
        QCoreApplication::processEvents();
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        RadeChannel* const radeB = rig.wdsp->radeChannel(b);
        QVERIFY(radeA && radeB && radeA != radeB);
        QCOMPARE(rig.worker.radeRxRouteCount(), 2);

        FakeAudioBus* const vaxA = rig.vaxOf(rig.sliceA);
        FakeAudioBus* const vaxB = rig.vaxOf(sliceB);
        const qsizetype fromA = vaxA->buffer().size();
        const qsizetype fromB = vaxB->buffer().size();
        QString evidence;
        QVERIFY2(rig.decodesOnItsOwnThread(radeA, &evidence), qPrintable(evidence));
        QVERIFY2(rig.decodesOnItsOwnThread(radeB, &evidence), qPrintable(evidence));
        QVERIFY(radeA->rxThreadIdForTest() != radeB->rxThreadIdForTest());
        evidence = QStringLiteral("bytesA=%1 bytesB=%2 peakA=%3 peakB=%4 playedA=%5 playedB=%6")
            .arg(vaxA->buffer().size() - fromA).arg(vaxB->buffer().size() - fromB)
            .arg(peakSince(vaxA, fromA)).arg(peakSince(vaxB, fromB))
            .arg(radeA->rxBridge()->playedSlots()).arg(radeB->rxBridge()->playedSlots());
        QVERIFY2(vaxA->buffer().size() - fromA == vaxB->buffer().size() - fromB
                     && vaxA->buffer().size() > fromA, qPrintable(evidence));
        QVERIFY2(radeA->rxBridge()->playedSlots() > 0 && radeB->rxBridge()->playedSlots() > 0,
                 qPrintable(evidence));
        QVERIFY2(peakSince(vaxA, fromA) == 0.0f && peakSince(vaxB, fromB) == 0.0f,
                 qPrintable(evidence));
    }

    // ── RADE reason (2026-09-30) ────────────────────────────────────────

    // A RADE slice whose decoder could not be created, or was created and
    // did not start, stays muted and says why on its radeReason. Leaving
    // RADE clears the reason; coming back to RADE with the cause gone
    // decodes again.
    void aRadeSliceWithNoWorkingDecoderSaysWhy_data()
    {
        QTest::addColumn<bool>("createFails");
        QTest::addColumn<QString>("expected");
        QTest::newRow("create fails")
            << true
            << QStringLiteral("RADE could not start on slice A: its RADE decoder could not be "
                              "created.");
        QTest::newRow("start fails")
            << false
            << QStringLiteral("RADE could not start on slice A: its RADE decoder did not "
                              "start.");
    }

    void aRadeSliceWithNoWorkingDecoderSaysWhy()
    {
        QFETCH(bool, createFails);
        QFETCH(QString, expected);
        AppSettings::instance().clear();
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        FakeAudioBus* const vaxA = rig.vaxOf(rig.sliceA);
        QVERIFY(rig.feedUntilAudible(vaxA, 0) > 0);
        QVERIFY(rig.sliceA->radeReason().isEmpty());
        QSignalSpy reasons(rig.sliceA, &SliceModel::radeReasonChanged);

        if (createFails) {
            rig.wdsp->setRadeCreateFailsForTest(true);
        } else {
            rig.wdsp->setRadeStartFailsForTest(true);
            QTest::ignoreMessage(QtWarningMsg,
                                 QRegularExpression(QStringLiteral("RADE will.*not decode")));
        }
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const failed = rig.wdsp->radeChannel(rig.a);
        if (createFails) {
            QVERIFY(failed == nullptr);
            QCOMPARE(rig.worker.radeRxRouteCount(), 0);
        } else {
            QVERIFY(failed && !failed->isActive());
        }
        QCOMPARE(rig.worker.radeModeSlices(), 1u << rig.a);
        QCOMPARE(rig.sliceA->radeReason(), expected);
        // One change, straight to the reason: no empty or interim value.
        QCOMPARE(reasons.size(), 1);
        QCOMPARE(reasons.first().first().toString(), expected);

        // Muted: a block every tick, every sample silent, never the sideband.
        qsizetype from = vaxA->buffer().size();
        constexpr int kBlocks = 256;
        for (int i = 0; i < kBlocks; ++i) {
            rig.feedOnce(rig.worker);
        }
        QCOMPARE(vaxA->buffer().size() - from,
                 qsizetype(kBlocks) * kFrames * 2 * qsizetype(sizeof(float)));
        QVERIFY2(peakSince(vaxA, from) == 0.0f,
                 qPrintable(QStringLiteral("peakFailed=%1").arg(peakSince(vaxA, from))));

        // The cause clears. Nothing restarts the decoder by itself: the
        // slice stays silent with its reason until the operator changes
        // its mode.
        rig.wdsp->setRadeCreateFailsForTest(false);
        rig.wdsp->setRadeStartFailsForTest(false);
        QCoreApplication::processEvents();
        QCOMPARE(rig.sliceA->radeReason(), expected);

        // Out of RADE: the reason clears and the sideband plays.
        rig.sliceA->setDspMode(DSPMode::USB);
        QCoreApplication::processEvents();
        QVERIFY(rig.sliceA->radeReason().isEmpty());
        QCOMPARE(rig.worker.radeModeSlices(), 0u);
        from = vaxA->buffer().size();
        QVERIFY(rig.feedUntilAudible(vaxA, from) > 0);

        // Back to RADE: it decodes, with no reason.
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        QVERIFY(radeA && radeA->isActive());
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
        QVERIFY(rig.sliceA->radeReason().isEmpty());
        QString evidence;
        QVERIFY2(rig.decodesOnItsOwnThread(radeA, &evidence), qPrintable(evidence));
    }

    // A RADE-U <-> RADE-L swap makes a new decoder. One that fails says so,
    // and the next swap that works clears it.
    void aFailedSidebandSwapSaysWhyAndTheNextSwapClearsIt()
    {
        AppSettings::instance().clear();
        GrowRig rig;
        QVERIFY(rig.setUp(/*streams=*/1));
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(rig.a));
        QVERIFY(rig.sliceA->radeReason().isEmpty());

        rig.wdsp->setRadeCreateFailsForTest(true);
        rig.sliceA->setDspMode(DSPMode::RADE_L);
        QCoreApplication::processEvents();
        QVERIFY(rig.wdsp->radeChannel(rig.a) == nullptr);
        QCOMPARE(rig.worker.radeModeSlices(), 1u << rig.a);
        QCOMPARE(rig.sliceA->radeReason(),
                 QStringLiteral("RADE could not start on slice A: its RADE decoder could not "
                                "be created."));

        rig.wdsp->setRadeCreateFailsForTest(false);
        rig.sliceA->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const radeA = rig.wdsp->radeChannel(rig.a);
        QVERIFY(radeA && radeA->isActive());
        QCOMPARE(rig.worker.radeRxRouteCount(), 1);
        QVERIFY(rig.sliceA->radeReason().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRadeRxMultisliceRouting)
#include "tst_rade_rx_multislice_routing.moc"
