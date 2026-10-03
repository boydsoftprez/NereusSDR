// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test.
//
// tst_rade_end_of_over_tail: the transmit safety boundary around the RADE
// end-of-over tail (FreeDV's end-of-over frame after an operator's
// release).
//
// MoxController:
//   tailRunsBeforeTeardown        a release asks for a tail once; while it
//                                 runs MOX is off, the hardware stays keyed
//                                 (no txAboutToEnd, no drain, no
//                                 hardwareFlipped(false)); once it is done
//                                 the walk ends in Rx.
//   tailIsBounded                 kEndOfOverTailMaxMs is 1 s, and a tail
//                                 that never reports releases the radio at
//                                 its bound.
//   tailNeverKeys                 a key never asks for a tail; during one
//                                 nothing keys, and a repeated release does
//                                 not ask again.
//   blockedUnkeysSkipTheTail      TX inhibit, the PA trip and receive-only
//                                 unkey at once without asking.
//   blockDuringTailEndsIt         any of the three asserted during a tail
//                                 ends it at once.
//   newKeyEndsTheTail             a key during the tail ends it and keys;
//                                 a late "done" changes nothing.
//   abortEndsTheTailAtOnce        abortEndOfOverTail goes straight on.
//   noTailWalksAsBefore           a function that starts no tail leaves the
//                                 walk as it was (txAboutToEnd at once).
// RadioModel (the real tail: RadioModel's tail function, the slice's
// RadeChannel, wireRadeChannel's lambda and a ticked TX worker):
//   realTailStaysWithinResamplerBlock  r8brain is never given more than
//                                 its block size (review Critical 1).
//   modeSwitchedToRadeWhileKeyedSendsNoTail  keyed in USB, switched to RADE,
//                                 released: no tail (the latched path).
//   modeChangeDuringTailEndsIt    a mode change mid-tail ends it.
//   permittedOnlyForRadeRelease   radeEndOfOverTailPermitted: keyed in RADE
//                                 yes; USB, TUNE and after a stop no.
//   tailKeepsRadioKeyedThenReleases  during the tail the radio's MOX stays
//                                 on, the Core reports keyed and txEnding;
//                                 when it is done MOX goes off.
//   stopAllTxSkipsTheTail         a stop during the tail stops at once
//                                 (MOX off before it returns) and ends the
//                                 tail; txEnding clears.
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns. AI tooling:
//                 Anthropic Claude Code.

#include <QtTest/QtTest>
#include <cmath>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/FreeDVReporterClient.h"
#include "core/MoxController.h"
#include "core/RadeChannel.h"
#include "core/RadeText.h"
#include "core/Resampler.h"
#include "core/TwoToneController.h"
#include "core/TxWorkerThread.h"
#include "core/audio/TxMicSource.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/session/TransmitStateFacade.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

void pump(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override
    {
        log.append(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off"));
    }
    void setTrxRelay(bool on) override
    {
        log.append(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
    }
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

// A bare controller on zero timers with a tail function that counts its
// asks and starts a tail when `start` is set.
struct Ctrl {
    MoxController mox;
    int asks{0};
    bool start{true};
    Ctrl()
    {
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setEndOfOverTail([this]() {
            ++asks;
            return start;
        });
    }
    void key()
    {
        mox.setMox(true);
        pump();
        QVERIFY(mox.isMox());
        QCOMPARE(mox.state(), MoxState::Tx);
    }
};

// A second of speech-like 16 kHz int16 audio (as tst_rade_channel's).
QByteArray speech16k(int nSamples)
{
    QByteArray buf(nSamples * static_cast<int>(sizeof(int16_t)), Qt::Uninitialized);
    auto* p = reinterpret_cast<int16_t*>(buf.data());
    for (int i = 0; i < nSamples; ++i) {
        p[i] = static_cast<int16_t>(12000.0 * std::sin(6.283185307179586 * 300.0 * i / 16000.0));
    }
    return buf;
}

// The whole RADE transmit path with no radio behind it: RadioModel's own
// tail function, the slice's RadeChannel (created, wired and started by
// setDspMode as in the app), wireRadeChannel's txModemReady lambda and its
// 24 -> 48 kHz resampler, and a TX worker the test ticks.
struct RealRig {
    MockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};
    AudioEngine engine;
    TxMicSource src;
    RadioModel model;
    TransmitState state;
    TxWorkerThread* worker{nullptr};

    explicit RealRig(DSPMode mode = DSPMode::RADE_U)
    {
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.injectTxChannelForTest(&tx);
        model.wireTxChannelKeyingForTest();
        auto w = std::make_unique<TxWorkerThread>();
        w->setTxChannel(&tx);
        w->setAudioEngine(&engine);
        w->setMicSource(&src);
        src.start();
        worker = w.get();
        model.installTxWorkerForTest(std::move(w));
        model.addSlice();
        if (SliceModel* slice = model.activeSlice()) {
            slice->setFrequency(14'236'000.0);
            slice->setDspMode(mode);
        }
        state.bind(&model);
    }
    ~RealRig()
    {
        src.stop();
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    RadeChannel* channel()
    {
        SliceModel* slice = model.activeSlice();
        return slice ? model.wdspEngine()->radeChannel(slice->sliceIndex()) : nullptr;
    }
    // One worker block per call, as the pump runs.
    void tick(int blocks = 1)
    {
        std::vector<float> mic(static_cast<size_t>(TxWorkerThread::kBlockFrames), 0.0f);
        for (int i = 0; i < blocks; ++i) {
            src.inbound(mic.data(), TxWorkerThread::kBlockFrames);
            worker->tickForTest();
        }
    }
    void key()
    {
        model.moxController()->setMox(true);
        pump();
    }
};

}  // namespace

class TestRadeEndOfOverTail : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    // Review Critical 1: the tail's modem block (about 15,000 frames at
    // 24 kHz) goes through wireRadeChannel's 24 -> 48 kHz resampler, built
    // for 4096; r8brain must never be handed more than that at once. Runs
    // RadioModel's own tail function end to end.
    void realTailStaysWithinResamplerBlock()
    {
        RealRig rig;
        QVERIFY(rig.channel() != nullptr);
        QVERIFY(rig.channel()->isActive());
        rig.key();
        QVERIFY(rig.model.mox());
        QCOMPARE(rig.worker->currentTxPathForTest(), TxWorkerThread::TxPath::Rade);

        QSignalSpy tail(&rig.model, &RadioModel::endOfOverTailChanged);
        rig.model.moxController()->setMox(false);
        QVERIFY(rig.model.endOfOverTailActive());
        QVERIFY(rig.channel()->endOfOverQueued());
        const Resampler* up = rig.model.radeTxResamplerForTest();
        QVERIFY(up != nullptr);
        QVERIFY2(up->largestInputBlock() <= up->maxBlockSamples(),
                 qPrintable(QStringLiteral("r8brain was given %1 samples at once; built for %2")
                                .arg(up->largestInputBlock())
                                .arg(up->maxBlockSamples())));

        // The queued audio reaches the worker and goes through the TX chain;
        // the drained notice ends the tail and the radio unkeys.
        pump();
        for (int i = 0; i < 2000 && rig.model.endOfOverTailActive(); ++i) {
            rig.tick();
            pump(2);
        }
        QVERIFY(!rig.model.endOfOverTailActive());
        QTRY_COMPARE_WITH_TIMEOUT(rig.model.moxController()->state(), MoxState::Rx, 5000);
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX off")));
    }

    void cleanup() { AppSettings::instance().clear(); }

    // Review Minor 1: the tail pushes both resamplers' latency out, so the
    // worker gets all of the EOO and the 200 ms of silence at 48 kHz:
    // (1152 + 1600) x 6 samples from fresh resamplers.
    void tailFlushesBothResamplers()
    {
        RealRig rig;
        rig.key();
        QCOMPARE(rig.worker->radeAudioQueuedSamplesForTest(), 0);
        rig.model.moxController()->setMox(false);
        QVERIFY(rig.model.endOfOverTailActive());
        pump();
        const int queued = rig.worker->radeAudioQueuedSamplesForTest();
        QVERIFY2(std::abs(queued - (1152 + 1600) * 6) <= 6,
                 qPrintable(QStringLiteral("%1 samples queued").arg(queued)));
    }

    // Review Minor 3: during the tail txState still says who keyed, though
    // keyedBy clears at the release; it empties once back in receive.
    void txStateShowsWhoKeyedDuringTail()
    {
        RealRig rig;
        RadioModel::KeyedBy who;
        who.deviceId = QByteArrayLiteral("device-1");
        who.deviceName = QStringLiteral("Test phone");
        who.deviceKind = QStringLiteral("phone");
        who.trigger = QByteArrayLiteral("screen");
        who.epoch = 1;
        rig.model.setKeyedBy(who);
        rig.key();
        QCOMPARE(rig.state.keyedByName(), QStringLiteral("Test phone"));

        rig.model.moxController()->setMox(false);
        rig.model.setKeyedBy({});   // as RemoteKeying does at the release
        pump();
        QVERIFY(rig.state.txEnding());
        QCOMPARE(rig.state.keyedByName(), QStringLiteral("Test phone"));
        QCOMPARE(rig.state.keyedByKind(), QStringLiteral("phone"));
        QCOMPARE(rig.state.keyedTrigger(), QStringLiteral("screen"));

        for (int i = 0; i < 2000 && rig.model.endOfOverTailActive(); ++i) {
            rig.tick();
            pump(2);
        }
        QTRY_COMPARE_WITH_TIMEOUT(rig.model.moxController()->state(), MoxState::Rx, 5000);
        pump();
        QVERIFY(!rig.state.keyed());
        QVERIFY(rig.state.keyedByName().isEmpty());
    }

    // Review Minor 4: two-tone keyed from a RADE key releases MOX first
    // (Thetis's TestIMD walk); that release is not an over's end, so it
    // sends no tail.
    void twoToneActivationSendsNoTail()
    {
        RealRig rig;
        TwoToneController* twoTone = rig.model.twoToneController();
        twoTone->setTxChannel(&rig.tx);
        twoTone->setPowerOn(true);
        twoTone->setSettleDelaysMs(0, 0);
        rig.key();
        QVERIFY(rig.model.mox());

        QSignalSpy tail(&rig.model, &RadioModel::endOfOverTailChanged);
        twoTone->setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(twoTone->isActive(), 5000);
        pump();
        QCOMPARE(tail.count(), 0);
        twoTone->setActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!twoTone->isActive(), 5000);
        pump();
        QCOMPARE(tail.count(), 0);
        twoTone->setTxChannel(nullptr);
    }

    // Review Minor 5: a new key during the tail ends the tail but not the
    // end-of-over frame already queued: it plays out whole ahead of the new
    // over's audio, as FreeDV's output FIFO does.
    void newKeyLetsTheQueuedFrameFinish()
    {
        RealRig rig;
        rig.key();
        rig.model.moxController()->setMox(false);
        QVERIFY(rig.model.endOfOverTailActive());
        pump();
        const int queued = rig.worker->radeAudioQueuedSamplesForTest();
        QVERIFY(queued > 0);

        rig.key();
        QVERIFY(rig.model.mox());
        QVERIFY(!rig.model.endOfOverTailActive());
        pump();
        QCOMPARE(rig.worker->radeAudioQueuedSamplesForTest(), queued);
        QVERIFY(!rig.channel()->endOfOverQueued());   // the new over encodes
        rig.tick(4);
        QCOMPARE(rig.worker->radeAudioQueuedSamplesForTest(),
                 queued - 4 * TxWorkerThread::kBlockFrames);
    }

    // Review Minor 6: as FreeDV, the end-of-over frame carries the callsign
    // only while the operator's FreeDV reporting is on; otherwise it goes
    // out with no callsign (zero data), as FreeDV's does with reporting off.
    void callsignOnlyWhileFreedvReporterRuns_data()
    {
        // How the Core's FreeDV Reporter client last reported itself, and
        // whether the operator's reporting is on. Reporting is on from the
        // start until the operator stops it, as FreeDV's reportingEnabled
        // setting is; a connection error or a lost connection keeps it on.
        QTest::addColumn<QString>("state");
        QTest::addColumn<bool>("reporting");
        QTest::newRow("never started") << QString() << false;
        QTest::newRow("connecting") << QStringLiteral("lost") << true;
        QTest::newRow("connected") << QStringLiteral("connected") << true;
        QTest::newRow("connection error") << QStringLiteral("error") << true;
        QTest::newRow("stopped") << QStringLiteral("stopped") << false;
    }

    void callsignOnlyWhileFreedvReporterRuns()
    {
        QFETCH(QString, state);
        QFETCH(bool, reporting);
        RealRig rig;
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("KG4VCF"));
        FreeDVReporterClient* client = rig.model.freeDvReporter();
        QVERIFY(client != nullptr);
        if (state == QStringLiteral("lost")) {
            emit client->connectionLost(1000);
        } else if (state == QStringLiteral("connected")) {
            emit client->connected();
        } else if (state == QStringLiteral("error")) {
            emit client->connected();
            emit client->connectionError(QStringLiteral("refused"));
        } else if (state == QStringLiteral("stopped")) {
            emit client->connected();
            emit client->disconnected();
        }
        rig.key();
        rig.model.moxController()->setMox(false);
        QVERIFY(rig.model.endOfOverTailActive());
        QCOMPARE(rig.channel()->textChannel()->ourCallsign(),
                 reporting ? QStringLiteral("KG4VCF") : QString());
    }

    // Found in review: when an over ends with no tail (Stop All TX, TX
    // inhibit), the audio still queued for the worker and held in the TX
    // resamplers came out at the start of the next over. Every unkey now
    // drops it: the next over starts exactly as on a fresh channel.
    void noOldAudioReachesTheNextOver()
    {
        // What a fresh channel emits for 1 s of speech (sizes only).
        int freshBytes = 0;
        {
            RadeChannel fresh;
            QVERIFY(fresh.start(QStringLiteral("dummy")));
            QObject::connect(&fresh, &RadeChannel::txModemReady,
                             [&freshBytes](const QByteArray& b) { freshBytes += b.size(); });
            fresh.txEncode(speech16k(16000));
            fresh.stop();
        }
        QVERIFY(freshBytes > 0);

        for (int path = 0; path < 2; ++path) {
            RealRig rig;
            rig.key();
            rig.channel()->txEncode(speech16k(16000 + 80));   // a partial frame too
            pump();
            QVERIFY(rig.worker->radeAudioQueuedSamplesForTest() > 0);
            QVERIFY(rig.model.radeTxResamplerForTest() != nullptr);

            if (path == 0) {
                rig.model.stopAllTx(QStringLiteral("Time Out Timer"));
            } else {
                rig.model.moxController()->setTxInhibited(true);
            }
            QTRY_COMPARE_WITH_TIMEOUT(rig.model.moxController()->state(), MoxState::Rx, 5000);
            pump();
            QVERIFY(!rig.model.endOfOverTailActive());
            QCOMPARE(rig.worker->radeAudioQueuedSamplesForTest(), 0);
            QVERIFY(rig.model.radeTxResamplerForTest() == nullptr);
            QCOMPARE(rig.channel()->txFeatureAccumSizeForTest(), 0);

            int nextBytes = 0;
            QObject::connect(rig.channel(), &RadeChannel::txModemReady,
                             [&nextBytes](const QByteArray& b) { nextBytes += b.size(); });
            rig.channel()->txEncode(speech16k(16000));
            QCOMPARE(nextBytes, freshBytes);
        }
    }

    // Review Important 1, case A: keyed in USB (the worker latched the WDSP
    // path), the slice switched to RADE while keyed, then released. The
    // live microphone path must not stay on the air for a tail.
    void modeSwitchedToRadeWhileKeyedSendsNoTail()
    {
        RealRig rig(DSPMode::USB);
        rig.key();
        QVERIFY(rig.model.mox());
        QCOMPARE(rig.worker->currentTxPathForTest(), TxWorkerThread::TxPath::Wdsp);
        rig.model.activeSlice()->setDspMode(DSPMode::RADE_U);
        QVERIFY(rig.channel() != nullptr);

        QSignalSpy tail(&rig.model, &RadioModel::endOfOverTailChanged);
        rig.model.moxController()->setMox(false);
        QVERIFY(!rig.model.endOfOverTailActive());
        QCOMPARE(tail.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(rig.model.moxController()->state(), MoxState::Rx, 5000);
    }

    // Review Important 1, case B: RADE switched to another mode during the
    // tail ends the tail at once; the rest of the EOO never goes out
    // through the new mode's modulator.
    void modeChangeDuringTailEndsIt()
    {
        RealRig rig;
        rig.key();
        QCOMPARE(rig.worker->currentTxPathForTest(), TxWorkerThread::TxPath::Rade);
        rig.model.moxController()->setMox(false);
        QVERIFY(rig.model.endOfOverTailActive());

        rig.model.activeSlice()->setDspMode(DSPMode::USB);
        QVERIFY(!rig.model.endOfOverTailActive());
        QTRY_COMPARE_WITH_TIMEOUT(rig.model.moxController()->state(), MoxState::Rx, 5000);
    }

    // Review Important 2: disconnecting while keyed in RADE sends no tail.
    // The unkey walks as before the tail existed: the amp's UNKEY
    // (txAboutToEnd) and the TX channel's drain happen inside the
    // teardown's setMox(false), while the TX channel is still wired, so
    // the RF gate is shut.
    void disconnectWhileKeyedSendsNoTail()
    {
        RealRig rig;
        rig.key();
        QVERIFY(rig.model.mox());
        QVERIFY(rig.tx.isRfGateOpen());

        MoxController* mox = rig.model.moxController();
        QSignalSpy tail(mox, &MoxController::endOfOverTailChanged);
        QSignalSpy aboutToEnd(mox, &MoxController::txAboutToEnd);
        QSignalSpy drain(mox, &MoxController::txDrainRequested);
        rig.model.disconnectFromRadio();

        QCOMPARE(tail.count(), 0);
        QCOMPARE(aboutToEnd.count(), 1);
        QCOMPARE(drain.count(), 1);
        QVERIFY(!rig.tx.isRfGateOpen());
        QVERIFY(!rig.model.endOfOverTailActive());
    }

    // Review Important 2, round 2: the operator releases, then disconnects
    // while the end-of-over tail is still running. MOX is already off, so
    // the teardown's setMox(false) changes nothing; the tail must still end
    // inside the teardown before the TX channel is unwired, so the amp's
    // UNKEY (txAboutToEnd) and the TX channel's drain see a wired channel.
    void disconnectDuringTailEndsItWhileWired()
    {
        RealRig rig;
        rig.key();
        QCOMPARE(rig.worker->currentTxPathForTest(), TxWorkerThread::TxPath::Rade);
        MoxController* mox = rig.model.moxController();
        mox->setMox(false);
        QVERIFY(rig.model.endOfOverTailActive());
        QVERIFY(rig.tx.isRfGateOpen());

        int aboutToEnd = 0;
        int drains = 0;
        bool wiredAtAboutToEnd = false;
        bool wiredAtDrain = false;
        connect(mox, &MoxController::txAboutToEnd, this, [&]() {
            ++aboutToEnd;
            wiredAtAboutToEnd = rig.model.txChannel() != nullptr;
        });
        connect(mox, &MoxController::txDrainRequested, this, [&]() {
            ++drains;
            wiredAtDrain = rig.model.txChannel() != nullptr;
        });
        rig.model.disconnectFromRadio();

        QCOMPARE(aboutToEnd, 1);
        QCOMPARE(drains, 1);
        QVERIFY(wiredAtAboutToEnd);
        QVERIFY(wiredAtDrain);
        QVERIFY(!rig.tx.isRfGateOpen());
        QVERIFY(!rig.model.endOfOverTailActive());
    }

    void tailRunsBeforeTeardown()
    {
        Ctrl c;
        c.key();
        QCOMPARE(c.asks, 0);

        QSignalSpy aboutToEnd(&c.mox, &MoxController::txAboutToEnd);
        QSignalSpy drain(&c.mox, &MoxController::txDrainRequested);
        QSignalSpy flipped(&c.mox, &MoxController::hardwareFlipped);
        QSignalSpy tail(&c.mox, &MoxController::endOfOverTailChanged);

        c.mox.setMox(false);
        QCOMPARE(c.asks, 1);
        QVERIFY(!c.mox.isMox());
        QVERIFY(c.mox.isEndOfOverTailActive());
        QCOMPARE(tail.count(), 1);
        QCOMPARE(tail.at(0).at(0).toBool(), true);

        pump();
        QCOMPARE(c.mox.state(), MoxState::TxToRxInFlight);
        QCOMPARE(aboutToEnd.count(), 0);
        QCOMPARE(drain.count(), 0);
        QCOMPARE(flipped.count(), 0);

        c.mox.onEndOfOverTailDone();
        QVERIFY(!c.mox.isEndOfOverTailActive());
        QCOMPARE(tail.count(), 2);
        QCOMPARE(tail.at(1).at(0).toBool(), false);
        QCOMPARE(aboutToEnd.count(), 1);
        QCOMPARE(drain.count(), 1);
        pump();
        QCOMPARE(c.mox.state(), MoxState::Rx);
        QCOMPARE(flipped.count(), 1);
        QCOMPARE(flipped.at(0).at(0).toBool(), false);
    }

    void tailIsBounded()
    {
        QCOMPARE(MoxController::kEndOfOverTailMaxMs, 1000);

        Ctrl c;
        c.mox.setEndOfOverTailMaxMsForTest(60);
        c.key();
        QSignalSpy flipped(&c.mox, &MoxController::hardwareFlipped);
        QElapsedTimer t;
        t.start();
        c.mox.setMox(false);
        QVERIFY(c.mox.isEndOfOverTailActive());
        QTRY_COMPARE_WITH_TIMEOUT(c.mox.state(), MoxState::Rx, 5000);
        QVERIFY2(t.elapsed() >= 50,
                 qPrintable(QStringLiteral("released after %1 ms").arg(t.elapsed())));
        QVERIFY(!c.mox.isEndOfOverTailActive());
        QCOMPARE(flipped.count(), 1);
        QCOMPARE(flipped.at(0).at(0).toBool(), false);
    }

    void tailNeverKeys()
    {
        Ctrl c;
        QSignalSpy aboutToBegin(&c.mox, &MoxController::txAboutToBegin);
        c.key();
        QCOMPARE(aboutToBegin.count(), 1);
        QCOMPARE(c.asks, 0);

        QSignalSpy flipped(&c.mox, &MoxController::hardwareFlipped);
        c.mox.setMox(false);
        c.mox.setMox(false);  // a repeated release is not a release
        pump();
        QCOMPARE(c.asks, 1);
        QCOMPARE(aboutToBegin.count(), 1);
        QCOMPARE(flipped.count(), 0);
        QVERIFY(!c.mox.isMox());
        c.mox.onEndOfOverTailDone();
        pump();
        QCOMPARE(aboutToBegin.count(), 1);
        QCOMPARE(c.mox.state(), MoxState::Rx);
    }

    void blockedUnkeysSkipTheTail()
    {
        {
            Ctrl c;
            c.key();
            c.mox.setTxInhibited(true);
            QCOMPARE(c.asks, 0);
            QVERIFY(!c.mox.isEndOfOverTailActive());
            pump();
            QCOMPARE(c.mox.state(), MoxState::Rx);
        }
        {
            Ctrl c;
            c.key();
            c.mox.setPaTripped(true);
            QCOMPARE(c.asks, 0);
            pump();
            QCOMPARE(c.mox.state(), MoxState::Rx);
        }
        {
            Ctrl c;
            c.key();
            c.mox.setRxOnly(true, QString());
            QCOMPARE(c.asks, 0);
            pump();
            QCOMPARE(c.mox.state(), MoxState::Rx);
        }
    }

    void blockDuringTailEndsIt()
    {
        for (int which = 0; which < 3; ++which) {
            Ctrl c;
            c.key();
            c.mox.setMox(false);
            QVERIFY(c.mox.isEndOfOverTailActive());
            QSignalSpy aboutToEnd(&c.mox, &MoxController::txAboutToEnd);
            if (which == 0) {
                c.mox.setTxInhibited(true);
            } else if (which == 1) {
                c.mox.setPaTripped(true);
            } else {
                c.mox.setRxOnly(true);
            }
            QVERIFY(!c.mox.isEndOfOverTailActive());
            QCOMPARE(aboutToEnd.count(), 1);
            pump();
            QCOMPARE(c.mox.state(), MoxState::Rx);
        }
    }

    void newKeyEndsTheTail()
    {
        Ctrl c;
        c.key();
        c.mox.setMox(false);
        QVERIFY(c.mox.isEndOfOverTailActive());

        QSignalSpy tail(&c.mox, &MoxController::endOfOverTailChanged);
        QSignalSpy aboutToEnd(&c.mox, &MoxController::txAboutToEnd);
        c.mox.setMox(true);
        QVERIFY(!c.mox.isEndOfOverTailActive());
        QCOMPARE(tail.count(), 1);
        QCOMPARE(tail.at(0).at(0).toBool(), false);
        pump();
        QVERIFY(c.mox.isMox());
        QCOMPARE(c.mox.state(), MoxState::Tx);

        c.mox.onEndOfOverTailDone();  // late: ignored
        pump();
        QCOMPARE(aboutToEnd.count(), 0);
        QVERIFY(c.mox.isMox());
        QCOMPARE(c.mox.state(), MoxState::Tx);
    }

    void abortEndsTheTailAtOnce()
    {
        Ctrl c;
        c.key();
        c.mox.setMox(false);
        QSignalSpy aboutToEnd(&c.mox, &MoxController::txAboutToEnd);
        c.mox.abortEndOfOverTail();
        QVERIFY(!c.mox.isEndOfOverTailActive());
        QCOMPARE(aboutToEnd.count(), 1);
        pump();
        QCOMPARE(c.mox.state(), MoxState::Rx);
        c.mox.abortEndOfOverTail();  // nothing to abort: no second walk
        QCOMPARE(aboutToEnd.count(), 1);
    }

    void noTailWalksAsBefore()
    {
        Ctrl c;
        c.start = false;
        c.key();
        QSignalSpy aboutToEnd(&c.mox, &MoxController::txAboutToEnd);
        QSignalSpy tail(&c.mox, &MoxController::endOfOverTailChanged);
        c.mox.setMox(false);
        QCOMPARE(c.asks, 1);
        QCOMPARE(aboutToEnd.count(), 1);
        QCOMPARE(tail.count(), 0);
        pump();
        QCOMPARE(c.mox.state(), MoxState::Rx);
    }

    void permittedOnlyForRadeRelease()
    {
        {
            RealRig rig(DSPMode::USB);
            rig.key();
            QVERIFY(rig.model.mox());
            QVERIFY(!rig.model.radeEndOfOverTailPermitted());
        }
        {
            RealRig rig(DSPMode::RADE_U);
            QVERIFY(!rig.model.radeEndOfOverTailPermitted());  // not keyed
            rig.key();
            QVERIFY(rig.model.mox());
            QVERIFY(rig.tx.isRfGateOpen());
            QVERIFY(rig.model.radeEndOfOverTailPermitted());
            rig.model.stopTransmitNow(QStringLiteral("test"));
            QVERIFY(!rig.model.radeEndOfOverTailPermitted());
        }
        {
            RealRig rig(DSPMode::RADE_L);
            rig.model.setTune(true);
            pump();
            QVERIFY(rig.model.mox());
            QVERIFY(!rig.model.radeEndOfOverTailPermitted());
        }
    }

    void tailKeepsRadioKeyedThenReleases()
    {
        RealRig rig;
        MoxController* mox = rig.model.moxController();
        QSignalSpy tailChanged(&rig.model, &RadioModel::endOfOverTailChanged);

        rig.key();
        QVERIFY(rig.state.keyed());
        rig.conn.log.clear();

        mox->setMox(false);
        pump();
        QVERIFY(rig.model.endOfOverTailActive());
        QCOMPARE(tailChanged.count(), 1);
        QVERIFY(rig.state.keyed());
        QVERIFY(rig.state.txEnding());
        QVERIFY2(!rig.conn.log.contains(QStringLiteral("MOX off")),
                 "the radio unkeyed before the end-of-over frame went out");
        QVERIFY(rig.tx.isRfGateOpen());

        for (int i = 0; i < 2000 && rig.model.endOfOverTailActive(); ++i) {
            rig.tick();
            pump(2);
        }
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 5000);
        pump();
        QVERIFY(!rig.model.endOfOverTailActive());
        QVERIFY(!rig.state.txEnding());
        QVERIFY(!rig.state.keyed());
        QVERIFY(rig.conn.log.contains(QStringLiteral("MOX off")));
        QVERIFY(!rig.conn.log.contains(QStringLiteral("MOX on")));
    }

    void stopAllTxSkipsTheTail()
    {
        RealRig rig;
        MoxController* mox = rig.model.moxController();
        rig.key();
        mox->setMox(false);
        pump();
        QVERIFY(rig.model.endOfOverTailActive());
        QVERIFY(rig.state.txEnding());
        rig.conn.log.clear();

        rig.model.stopAllTx(QStringLiteral("Time Out Timer"));
        // RF stops before stopAllTx returns, and the tail is over.
        QVERIFY(rig.conn.log.size() >= 2);
        QCOMPARE(rig.conn.log.at(0), QStringLiteral("MOX off"));
        QVERIFY(!rig.tx.isRfGateOpen());
        QVERIFY(!rig.model.endOfOverTailActive());
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 5000);
        pump();
        QVERIFY(!rig.state.txEnding());
        QVERIFY(!rig.conn.log.contains(QStringLiteral("MOX on")));
    }
};

QTEST_GUILESS_MAIN(TestRadeEndOfOverTail)
#include "tst_rade_end_of_over_tail.moc"
