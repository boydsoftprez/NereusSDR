// no-port-check: NereusSDR-original unit-test file.  RADE audio
// routing is NereusSDR-native; no Thetis equivalent.
// =================================================================
// tests/tst_audio_engine_rade.cpp  (NereusSDR)
// =================================================================
//
// Phase 3R Task J4 unit tests: RadeChannel::rxSpeechReady is routed
// into AudioEngine via the same speakers bus path as the WDSP
// RxChannel.  The connection is set up by
// RadioModel::wireRadeChannel(sliceId, channel, slice) (which now
// adds the audio connection on top of the I5 signal graph) and
// returns float32 stereo PCM to the generation-checked DSP worker, which
// is the single producer calling AudioEngine::rxBlockReady.
//
// The regression drives a real WDSP receiver, worker and RADE wrapper so it
// covers the production 48 -> 24 -> codec -> 48 kHz route and its ownership
// generation rather than bypassing it with a signal-only test seam.
//
// Test case (1):
//   1. switchToRadeRoutesAudioToAudioEngine - real worker input after
//      wireRadeChannel must push to the FakeAudioBus speakers stand-in.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 - New test file for Phase 3R Task J4.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-21 - Updated for generation-checked worker return by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-09-30 - RADE threads: the codec decodes on its own thread and the
//                 worker plays its speech; the test waits for that thread
//                 instead of spying the retired radeIqReady hop. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 - RADE threads review: the decoder's own blocks reach the
//                 speakers (playedSlots), past the late bound. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/RadeChannel.h"
#include "core/RadeRxWorker.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/RxDspWorker.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>
#include <QStandardPaths>

using namespace NereusSDR;

namespace {

struct DspWorkerDetach {
    RadioModel* radio{nullptr};
    ~DspWorkerDetach()
    {
        if (radio) {
            radio->attachDspWorkerForTest(nullptr);
        }
    }
};

} // namespace


class TstAudioEngineRade : public QObject {
    Q_OBJECT

private slots:

    // ── switchToRadeRoutesAudioToAudioEngine ───────────────────────────
    //
    // Wiring contract for J4: after RadioModel::wireRadeChannel(0,
    // channel, slice), emitting RadeChannel::rxSpeechReady with a
    // valid float32 stereo block must reach the speakers bus through
    // AudioEngine::rxBlockReady.  Verified by FakeAudioBus push count.
    void switchToRadeRoutesAudioToAudioEngine() {
        // Standard harness mirrors tst_audio_engine_rx_leak_during_mox:
        // RadioModel + audioEngine + FakeSpeakers injected via
        // setSpeakersBusForTest.
        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();

        auto speakers = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        speakers->open(fmt);
        FakeAudioBus* speakersRaw = speakers.get();
        engine->setSpeakersBusForTest(std::move(speakers));

        // Register the mixer slots, as connecting to a radio does
        // (RadioModel::configureStreamPool -> AudioEngine::preregisterSlices).
        // MasterMixer::accumulate() drops any slice id it has no entry for,
        // so without this the RADE speech block never reaches the mix. It
        // used to look like it did, because the speakers push was
        // unconditional and an empty push still incremented pushCount().
        radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/48000);

        const int sliceId = radio->addSlice();
        SliceModel* slice = radio->sliceById(sliceId);
        QVERIFY(slice != nullptr);

        // Wire the RADE channel.  J4's audio connection is added inside
        // RadioModel::wireRadeChannel alongside the I5 signal graph.
        WdspEngine* const wdsp = radio->wdspEngine();
        wdsp->setSynchronousInitForTest(true);
        QVERIFY(wdsp->initialize(QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation)));
        RxChannel* const rx = wdsp->createRxChannel(
            sliceId, 64, 4096, 48000, 48000, 48000);
        QVERIFY(rx);
        rx->setActive(true);

        RadeChannel channel;
        QVERIFY(channel.start(QStringLiteral("dummy")));
        RxDspWorker worker;
        worker.setEngines(wdsp, engine);
        worker.setBufferSizes(64, 64);
        worker.setStreamSlices(0, QVector<int>{sliceId});
        radio->attachDspWorkerForTest(&worker);
        DspWorkerDetach detach{radio.get()};
        radio->wireRadeChannel(sliceId, &channel, slice);
        QSignalSpy speech(&channel, &RadeChannel::rxSpeechReady);
        QVERIFY(speech.isValid());
        QVERIFY(channel.rxWorkerRunning());
        QCoreApplication::processEvents();

        // Baseline: speakers bus has not been pushed yet.
        const int baselinePushes = speakersRaw->pushCount();
        QCOMPARE(baselinePushes, 0);

        const std::shared_ptr<RadeRxBridge> bridge = channel.rxBridge();
        QVERIFY(bridge);

        QVector<float> iq(128, 0.05f);
        // Two resamplers warm here: worker 48 -> 24 kHz and the returned
        // speech 24 -> 48 kHz adapter. The latter's high-attenuation filter
        // can retain more than the 3,072 24-kHz frames produced by the old
        // 96-block bound on some platforms. 256 blocks remain a bounded
        // 341-ms input while clearing that documented filter history.
        // RADE threads review: and past the late bound (105 blocks of 64),
        // so the decoder's own output fills a slot rather than silence.
        constexpr int kMaxInputBlocks = 256;
        QVERIFY(kMaxInputBlocks > radeLateBoundBlocks(64));
        // RADE threads: the codec runs on the channel's decoder thread; wait
        // for it to take each block so the loop's count is deterministic.
        for (int rep = 0;
             rep < kMaxInputBlocks
             && (speakersRaw->pushCount() == baselinePushes
                 || speech.count() == 0
                 || bridge->playedSlots() == 0);
             ++rep) {
            worker.processIqBatch(0, iq);
            QVERIFY(channel.waitRxIdleForTest(5000));
            QCoreApplication::processEvents();
        }

        const QString evidence = QStringLiteral(
            "speech=%1 speakers=%2 played=%3 silent=%4 after at most %5 blocks")
            .arg(speech.count())
            .arg(speakersRaw->pushCount() - baselinePushes)
            .arg(bridge->playedSlots())
            .arg(bridge->silentSlots())
            .arg(kMaxInputBlocks);
        QVERIFY2(speakersRaw->pushCount() > baselinePushes
                     && speech.count() > 0,
                 qPrintable(evidence));
        // The decoded blocks themselves reached the speakers, not only the
        // silence of slots with nothing due.
        QVERIFY2(bridge->playedSlots() > 0, qPrintable(evidence));
    }
};

QTEST_GUILESS_MAIN(TstAudioEngineRade)
#include "tst_audio_engine_rade.moc"
