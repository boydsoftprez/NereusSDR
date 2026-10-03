// =================================================================
// tests/tst_tci_remote_window.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. TCI served by a remote window;
// no upstream logic is ported here.
//
// R3 receiver audio plan, Task 4 (R-R3-42, R-R3-21, R-R3-25).
//
// A remote window's TciServer serves apps on this computer while the
// receivers live on a Core:
//   - it hooks no RxChannel and no raw I/Q tap, and reaches no local
//     receiver or transmit channel;
//   - audio_start:N asks for the Core's slice N through the window's
//     receiver streams while at least one app listens, and releases it
//     when the last one stops; every app on a receiver gets all of it;
//   - an older Core (no receiver streams) leaves audio_start unanswered,
//     and the operator hears why, off the wire;
//   - vfo and modulation act on the Core's slice and answer with the
//     value the slice holds (a refused write re-broadcasts it);
//   - its init burst says receive_only:true and tx_enable false.
// The transmit and raw I/Q refusals have their own cases in
// tst_tci_tx_mutex and tst_tci_iq_roundtrip.
//
// Two kinds of case: unit cases drive the server through a fake receiver
// source (receiverAudioBlock called directly), and end-to-end cases use
// the real RemoteMediaController against an in-process Core
// (RemoteAudioSessionHarness), speakers muted, Opus and lossless.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 4.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio fix wave: stereo at
//                                    12 kHz keeps L and R apart; rx_sensors
//                                    read the Core's mirrored meter; a late
//                                    "cannot send" answer stops the app.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 35 (R-IOS-13;
//                                    ruling 8.14): an app's transmit is
//                                    forwarded to the Core as a program's
//                                    key; the TX audio lock only after the
//                                    Core admits it; a refused key takes
//                                    nothing and is answered trx:N,false;
//                                    trx:N,false releases only this
//                                    window's key. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 36 (R-IOS-13): the
//                                    lock holder's transmit audio goes to
//                                    the window's microphone line (the
//                                    forwarder's audio), and a real window
//                                    sends it to the Core. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-42 (parity Task 3):
//                                    tx_profile_ex selects the Core's
//                                    profile and is echoed only once the
//                                    Core has it, tx_profiles_ex answers
//                                    the Core's list, and mon_volume
//                                    changes the Core's monitor level.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include <QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/TciBinaryFrame.h"
#include "core/TciServer.h"
#include "core/session/media/RemoteMicReceiver.h"
#include "core/MicProfileManager.h"
#include "core/TciServer.h"
#include "core/TciVolume.h"
#include "core/WdspThreadCheck.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/session/media/RemoteTciAudioStage.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "core/session/media/OpusAudioCodec.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "gui/OperatorReasonText.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "gui/applets/TciApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "OperatorWording.h"

#include <QFile>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QWebSocket>

#include <cmath>
#include <array>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;

extern "C" {
void* create_resampleFV(int inRate, int outRate);
void xresampleFV(float* input, float* output, int frames, int* outputFrames, void* handle);
void destroy_resampleFV(void* handle);
}

namespace {

using Harness = Test::RemoteAudioSessionHarness;

bool connectClient(QWebSocket& client, quint16 port)
{
    QSignalSpy connected(&client, &QWebSocket::connected);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    return connected.wait(3000) && client.state() == QAbstractSocket::ConnectedState;
}

QStringList texts(const QSignalSpy& spy)
{
    QStringList lines;
    for (const QList<QVariant>& call : spy) {
        lines << call.at(0).toString();
    }
    return lines;
}

quint32 readLe32(const QByteArray& frame, int offset)
{
    const auto* p = reinterpret_cast<const quint8*>(frame.constData() + offset);
    return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
         | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
}

struct RxAudio {
    int blocks = 0;
    quint32 channels = 0;
    QVector<float> samples;   // concatenated, as sent (interleaved when stereo)
};

// The Float32 RX audio frames (stream type 1) one client received for one
// receiver.
RxAudio rxAudio(const QSignalSpy& binary, quint32 receiver)
{
    RxAudio audio;
    for (const QList<QVariant>& call : binary) {
        const QByteArray frame = call.at(0).toByteArray();
        if (frame.size() < 64 || readLe32(frame, 24) != 1u || readLe32(frame, 0) != receiver) {
            continue;
        }
        ++audio.blocks;
        audio.channels = readLe32(frame, 28);
        const qsizetype start = audio.samples.size();
        audio.samples.resize(start + (frame.size() - 64) / 4);
        std::memcpy(audio.samples.data() + start, frame.constData() + 64,
                    static_cast<size_t>(frame.size() - 64));
    }
    return audio;
}

int frameCount(const RxAudio& audio)
{
    return audio.channels == 0 ? 0 : int(audio.samples.size() / audio.channels);
}

// The Core's side of a remote window's receiver streams, faked: it records
// what the server asks for and lets the test deliver blocks as a receive
// worker would.
struct FakeReceiverSource {
    struct WorkerSink final : IReceiverPcmSink {
        IReceiverPcmSink* downstream;
        std::shared_ptr<RemoteTciAudioStage> stage;
        std::mutex mutex;
        std::condition_variable wake;
        std::deque<std::vector<float>> incoming;
        bool stop = false;
        std::thread worker;

        WorkerSink(IReceiverPcmSink* sink, int receiver)
            : downstream(sink), stage(std::make_shared<RemoteTciAudioStage>(receiver))
        {
            worker = std::thread([this] {
                auto run = stage->createRun();
                while (true) {
                    std::vector<float> pcm;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        if (!run->hasRunnableWork()) {
                            wake.wait_for(lock, std::chrono::milliseconds(2), [this] {
                                return stop || !incoming.empty();
                            });
                        }
                        if (stop) { break; }
                        if (!incoming.empty()) {
                            pcm = std::move(incoming.front());
                            incoming.pop_front();
                        }
                    }
                    if (!pcm.empty()) { run->appendPcm(pcm.data(), int(pcm.size() / 2)); }
                    run->reconcile();
                    run->serviceUntil(std::chrono::steady_clock::now()
                                      + std::chrono::microseconds(1800), 32);
                }
            });
        }
        ~WorkerSink() override
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                stop = true;
            }
            wake.notify_one();
            worker.join();
        }
        void receiverAudioBlock(int, const float* pcm, int frames) override
        {
            std::vector<float> copy(pcm, pcm + 2 * frames);
            {
                std::lock_guard<std::mutex> lock(mutex);
                incoming.push_back(std::move(copy));
            }
            wake.notify_one();
            // The real receiver calls its fanout before append; this fake
            // gives the server the same notice-clearing callback.
            downstream->receiverAudioBlock(stageReceiver, pcm, frames);
        }
        void receiverAudioStopped(int sliceId, const QString& reason) override
        {
            downstream->receiverAudioStopped(sliceId, reason);
        }
        int stageReceiver = -1;
    };
    QList<int> requests;
    QList<int> releases;
    QHash<int, IReceiverPcmSink*> sinks;
    std::vector<std::unique_ptr<WorkerSink>> workers;
    // When set, a request is answered at once with this stop reason, the
    // way RemoteMediaController answers for a Core without receiver streams.
    QString answerRequestsWith;

    TciServer::RemoteReceiverAudio source()
    {
        TciServer::RemoteReceiverAudio audio;
        audio.request = [this](int sliceId, IReceiverPcmSink* sink) {
            requests << sliceId;
            auto worker = std::make_unique<WorkerSink>(sink, sliceId);
            worker->stageReceiver = sliceId;
            const auto stage = worker->stage;
            sinks.insert(sliceId, worker.get());
            workers.push_back(std::move(worker));
            if (!answerRequestsWith.isEmpty()) {
                sink->receiverAudioStopped(sliceId, answerRequestsWith);
            }
            return stage;
        };
        audio.release = [this](int sliceId, IReceiverPcmSink* sink) {
            releases << sliceId;
            auto* worker = static_cast<WorkerSink*>(sinks.value(sliceId));
            if (worker && worker->downstream == sink) { sinks.remove(sliceId); }
        };
        audio.unavailableReason =
            QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason);
        return audio;
    }

    // Left channel of frame k is ramp(k), the right channel its negative.
    static float ramp(int k) { return 1.0e-4f * static_cast<float>(k % 9000 + 1); }

    void deliverRamp(int sliceId, int firstFrame, int frames)
    {
        IReceiverPcmSink* const sink = sinks.value(sliceId);
        QVERIFY(sink != nullptr);
        constexpr int kPacket = 1920;   // one 40 ms Opus packet
        std::vector<float> pcm(kPacket * 2);
        for (int sent = 0; sent < frames; sent += kPacket) {
            const int n = std::min(kPacket, frames - sent);
            for (int i = 0; i < n; ++i) {
                pcm[size_t(2 * i)] = ramp(firstFrame + sent + i);
                pcm[size_t(2 * i + 1)] = -ramp(firstFrame + sent + i);
            }
            sink->receiverAudioBlock(sliceId, pcm.data(), n);
            QTest::qWait(10);
        }
    }
};

// A tone's amplitude in one channel of interleaved audio at `rateHz`.
double amplitudeAt(const QVector<float>& samples, int channels, int channel, double hz,
                   int rateHz, int firstFrame)
{
    constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
    double cosine = 0.0;
    double sine = 0.0;
    int frames = 0;
    for (int f = firstFrame; f * channels + channel < samples.size(); ++f) {
        const double phase = kTwoPi * hz * double(f) / double(rateHz);
        const double v = samples.at(f * channels + channel);
        cosine += v * std::cos(phase);
        sine += v * std::sin(phase);
        ++frames;
    }
    return frames > 0 ? 2.0 * std::hypot(cosine, sine) / frames : 0.0;
}

// Receiver-audio requests the Core received for one slice, in order.
QList<QJsonObject> receiverRequests(const QSignalSpy& coreControls, int sliceId)
{
    QList<QJsonObject> found;
    for (const auto& call : coreControls) {
        const QJsonObject control = call.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")) == QLatin1String("receiver-audio")
            && control.value(QStringLiteral("sliceId")).toInt() == sliceId) {
            found << control;
        }
    }
    return found;
}

int enabledRequests(const QSignalSpy& coreControls, int sliceId)
{
    int count = 0;
    for (const QJsonObject& request : receiverRequests(coreControls, sliceId)) {
        if (request.value(QStringLiteral("enabled")).toBool()) { ++count; }
    }
    return count;
}

// Wires a TciServer to a real RemoteMediaController, as MainWindow does.
void wire(TciServer& tci, RemoteMediaController& media)
{
    const QPointer<RemoteMediaController> guarded(&media);
    TciServer::RemoteReceiverAudio source;
    source.request = [guarded](int sliceId, IReceiverPcmSink* sink) {
        return guarded ? guarded->requestReceiverAudio(sliceId, sink)
                       : std::shared_ptr<RemoteTciAudioStage>{};
    };
    source.release = [guarded](int sliceId, IReceiverPcmSink* sink) {
        if (guarded) { guarded->releaseReceiverAudio(sliceId, sink); }
    };
    source.unavailableReason =
        QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason);
    tci.setRemoteReceiverAudio(std::move(source));
}

// Where `later` starts inside `earlier` (stereo frames), or -1. Both apps
// read one history, so a later app's stream is the earlier app's stream
// from some frame on, sample for sample.
int alignedStart(const QVector<float>& earlier, const QVector<float>& later)
{
    constexpr int kProbeFrames = 2048;
    if (later.size() < kProbeFrames * 2) { return -1; }
    const int earlierFrames = int(earlier.size() / 2);
    for (int start = 0; start + kProbeFrames <= earlierFrames; ++start) {
        if (std::memcmp(earlier.constData() + 2 * start, later.constData(),
                        size_t(kProbeFrames) * 2 * sizeof(float)) == 0) {
            return start;
        }
    }
    return -1;
}

// iPhone app plan Task 35: the Core's side of a remote window's transmit,
// faked: it records every tx.key and tx.unkey the window sends and answers
// a key when the test says.
struct FakeCoreTransmit {
    int keys = 0;
    QList<quint32> unkeys;
    std::function<void(const TciServer::RemoteKeyAnswer&)> pending;

    TciServer::RemoteTransmit forwarder()
    {
        TciServer::RemoteTransmit forward;
        forward.key = [this](std::function<void(const TciServer::RemoteKeyAnswer&)> answer) {
            ++keys;
            pending = std::move(answer);
        };
        forward.unkey = [this](quint32 epoch) { unkeys << epoch; };
        return forward;
    }

    void accept(quint32 epoch)
    {
        TciServer::RemoteKeyAnswer answer;
        answer.accepted = true;
        answer.epoch = epoch;
        auto reply = std::move(pending);
        pending = {};
        reply(answer);
    }

    void refuse(const QString& reason)
    {
        TciServer::RemoteKeyAnswer answer;
        answer.reason = reason;
        auto reply = std::move(pending);
        pending = {};
        reply(answer);
    }
};

const QString kProgramNeedsTransmit = QStringLiteral(
    "A program can transmit only while this device has transmit. Take transmit here first.");

} // namespace

class TestTciRemoteWindow : public QObject {
    Q_OBJECT

private slots:
    void remoteHistoryOverflowSeparatesResamplerRecreationCost()
    {
        auto stage = std::make_shared<RemoteTciAudioStage>(0);
        std::vector<RemoteTciAudioStage::ClientConfig> clients;
        for (quint64 token = 1; token <= 8; ++token) {
            clients.push_back({token, 1, 384000, 2, 3, 2048, 1.0f});
        }
        stage->publish(std::move(clients));
        std::vector<float> pcm(std::size_t(RemoteTciAudioStage::kHistoryFrames + 192) * 2,
                               0.125f);
        std::thread worker([&] {
            auto run = stage->createRun();
            run->reconcile();
            run->appendPcm(pcm.data(), int(pcm.size() / 2));
            run->serviceUntil(std::chrono::steady_clock::now() + std::chrono::seconds(1),
                              32);
        });
        worker.join();
        const auto metrics = stage->diagnostics();
        QCOMPARE(metrics.historySkippedFrames, quint64{8 * 192});
        QCOMPARE(metrics.resamplerRecreates, quint64{8});
        QVERIFY(metrics.resamplerRecreateWallNs > 0);
        QVERIFY(metrics.resamplerRecreateCpuNs > 0);
        QVERIFY(metrics.maxQuantumWallNs >= metrics.maxResamplerRecreateWallNs);
        QVERIFY(metrics.serviceWallNs >= metrics.resamplerRecreateWallNs);
        QCOMPARE(metrics.liveWdspPairs, 0);
        qInfo().nospace() << "Overflow reset eight pairs: wall="
                          << metrics.resamplerRecreateWallNs << " ns CPU="
                          << metrics.resamplerRecreateCpuNs << " ns max reset="
                          << metrics.maxResamplerRecreateWallNs << " ns max quantum="
                          << metrics.maxQuantumWallNs << " ns";
    }

    void remoteDrainBoundsRejectedFramesAndSurvivesSynchronousRetirement()
    {
        RadioModel remote(RadioModel::Role::Remote);
        FakeReceiverSource core;
        auto tci = std::make_unique<TciServer>(&remote);
        tci->setRemoteReceiverAudio(core.source());
        QVERIFY(tci->start(0));
        QWebSocket first;
        QWebSocket second;
        QSignalSpy firstReplies(&first, &QWebSocket::textMessageReceived);
        QSignalSpy secondReplies(&second, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(first, tci->port()));
        QVERIFY(connectClient(second, tci->port()));
        for (QWebSocket* socket : {&first, &second}) {
            socket->sendTextMessage(QStringLiteral("audio_stream_samples:100;"));
            socket->sendTextMessage(QStringLiteral("audio_start:0;"));
        }
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(firstReplies).contains(QStringLiteral("audio_start:0;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(secondReplies).contains(QStringLiteral("audio_start:0;")), 3000);
        tci->m_drainTimer->stop();
        const auto stage = core.workers.front()->stage;
        core.deliverRamp(0, 0, 300);
        // Three blocks per client leave two queued and one evicted each.
        QTRY_COMPARE_WITH_TIMEOUT(stage->diagnostics().mailboxEvictions, quint64{2}, 3000);
        int sendCalls = 0;
        tci->drainRemoteAudio([&](QWebSocket*, const QByteArray&) -> qint64 {
            ++sendCalls;
            return -1;  // sustained send refusal must still consume the tick budget
        });
        QCOMPARE(sendCalls, 2);
        tci->drainRemoteAudio([&](QWebSocket*, const QByteArray&) -> qint64 {
            ++sendCalls;
            tci.reset();  // a synchronous send callback retires the server itself
            return -1;
        });
        QCOMPARE(sendCalls, 3);
        QVERIFY(!tci);
        QCOMPARE(stage->diagnostics().liveWdspPairs, 0);
    }

    void remoteTimerSurvivesNoticeRetirement()
    {
        RadioModel remote(RadioModel::Role::Remote);
        FakeReceiverSource core;
        auto tci = std::make_unique<TciServer>(&remote);
        tci->setRemoteReceiverAudio(core.source());
        QVERIFY(tci->start(0));
        QWebSocket socket;
        QSignalSpy replies(&socket, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(socket, tci->port()));
        socket.sendTextMessage(QStringLiteral("audio_stream_samples:100;"));
        socket.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(replies).contains(QStringLiteral("audio_start:0;")), 3000);
        tci->m_drainTimer->stop();
        const auto stage = core.workers.front()->stage;
        core.deliverRamp(0, 0, 300);
        QTRY_COMPARE_WITH_TIMEOUT(stage->diagnostics().mailboxEvictions, quint64{1}, 3000);
        tci->m_remoteSaturationDrops = 99;
        connect(tci.get(), &TciServer::operatorNotice, &remote,
                [&](const QString&, const QString&, bool) { tci.reset(); });
        // Exercise the real timer caller, not just its drain helper.
        QVERIFY(QMetaObject::invokeMethod(tci->m_drainTimer, "timeout", Qt::DirectConnection));
        QVERIFY(!tci);
        QCOMPARE(stage->diagnostics().liveWdspPairs, 0);

        tci = std::make_unique<TciServer>(&remote);
        tci->m_rxStoppedNotice[0] = true;
        tci->m_noticeFromReceiverStop = true;
        tci->m_remotePcmArrived[0].store(true);
        connect(tci.get(), &TciServer::operatorNoticeCleared, &remote,
                [&] { tci.reset(); });
        QVERIFY(QMetaObject::invokeMethod(tci->m_drainTimer, "timeout", Qt::DirectConnection));
        QVERIFY(!tci);
    }

    void remoteReceiverRevisionsAreIndependent()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        tci.setRemoteReceiverAudio(core.source());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy binary(&app, &QWebSocket::binaryMessageReceived);
        QSignalSpy replies(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_stream_samples:100;"));
        app.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(0), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(replies).contains(QStringLiteral("audio_start:0;")),
                                 3000);
        core.deliverRamp(0, 0, 100);
        QTRY_COMPARE_WITH_TIMEOUT(rxAudio(binary, 0).blocks, 1, 3000);

        app.sendTextMessage(QStringLiteral("audio_start:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(1), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(replies).contains(QStringLiteral("audio_start:1;")),
                                 3000);
        core.deliverRamp(0, 100, 100);
        QTRY_COMPARE_WITH_TIMEOUT(rxAudio(binary, 0).blocks, 2, 3000);

        tci.setSliceRxGainLinear(1, 0.5f);
        core.deliverRamp(0, 200, 100);
        QTRY_COMPARE_WITH_TIMEOUT(rxAudio(binary, 0).blocks, 3, 3000);

        app.sendTextMessage(QStringLiteral("audio_stop:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(replies).contains(QStringLiteral("audio_stop:1;")),
                                 3000);
        core.deliverRamp(0, 300, 100);
        QTRY_COMPARE_WITH_TIMEOUT(rxAudio(binary, 0).blocks, 4, 3000);

        tci.setSliceRxGainLinear(0, 0.5f);
        QTest::qWait(30);  // let the worker retire the old receiver-0 revision
        core.deliverRamp(0, 400, 100);
        QTRY_COMPARE_WITH_TIMEOUT(rxAudio(binary, 0).blocks, 5, 3000);
        QCOMPARE(rxAudio(binary, 0).samples.at(800), FakeReceiverSource::ramp(400) * 0.5f);
        app.close();
        tci.stop();
    }

    void remoteFormatAndBlockChangesRetirePreviousOutput()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        tci.setRemoteReceiverAudio(core.source());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy binary(&app, &QWebSocket::binaryMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        for (const QString& command : {
                 QStringLiteral("audio_samplerate:8000;"),
                 QStringLiteral("audio_stream_channels:1;"),
                 QStringLiteral("audio_stream_sample_type:int16;"),
                 QStringLiteral("audio_stream_samples:100;"),
                 QStringLiteral("audio_start:0;")}) { app.sendTextMessage(command); }
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(0), 3000);
        core.deliverRamp(0, 0, 100);
        QTRY_VERIFY_WITH_TIMEOUT(binary.count() >= 1, 3000);
        QByteArray frame = binary.constLast().at(0).toByteArray();
        QCOMPARE(readLe32(frame, 4), 8000u);
        QCOMPARE(readLe32(frame, 8), 0u);
        QCOMPARE(readLe32(frame, 28), 1u);
        binary.clear();

        for (const QString& command : {
                 QStringLiteral("audio_samplerate:384000;"),
                 QStringLiteral("audio_stream_channels:2;"),
                 QStringLiteral("audio_stream_sample_type:int24;"),
                 QStringLiteral("audio_stream_samples:2048;")}) { app.sendTextMessage(command); }
        QTest::qWait(30);
        core.deliverRamp(0, 100, 2048);
        QTRY_VERIFY_WITH_TIMEOUT(binary.count() >= 1, 3000);
        frame = binary.constLast().at(0).toByteArray();
        QCOMPARE(readLe32(frame, 4), 384000u);
        QCOMPARE(readLe32(frame, 8), 1u);
        QCOMPARE(readLe32(frame, 28), 2u);
        QCOMPARE(readLe32(frame, 20), 32768u);
        binary.clear();

        for (const QString& command : {
                 QStringLiteral("audio_samplerate:48000;"),
                 QStringLiteral("audio_stream_sample_type:float32;"),
                 QStringLiteral("audio_stream_samples:100;")}) { app.sendTextMessage(command); }
        QTest::qWait(30);
        core.deliverRamp(0, 2148, 100);
        QTRY_VERIFY_WITH_TIMEOUT(binary.count() >= 1, 3000);
        frame = binary.constLast().at(0).toByteArray();
        QCOMPARE(readLe32(frame, 4), 48000u);
        QCOMPARE(readLe32(frame, 8), 3u);
        QCOMPARE(readLe32(frame, 20), 200u);
        QCOMPARE(readLe32(frame, 28), 2u);
        app.close();
        tci.stop();
    }

    void remoteRetirementDropsQueuedAndInFlightOldRevisions()
    {
        auto stage = std::make_shared<RemoteTciAudioStage>(0);
        stage->publish({{1, 1, 8000, 2, 3, 100, 1.0f}});
        std::vector<float> pcm(200, 0.2f);
        std::promise<void> oldBlockReady;
        std::promise<void> resume;
        auto resumeFuture = resume.get_future();
        std::thread worker([&] {
            auto run = stage->createRun();
            run->reconcile();
            run->appendPcm(pcm.data(), 100);
            run->reconcile();
            run->serviceUntil(std::chrono::steady_clock::now()
                              + std::chrono::seconds(1), 32);
            run->appendPcm(pcm.data(), 100);
            oldBlockReady.set_value();
            resumeFuture.wait();
            // A result completed from the old snapshot after publication
            // is rejected by the mailbox's generation/revision check.
            run->serviceUntil(std::chrono::steady_clock::now()
                              + std::chrono::seconds(1), 32);
            run->reconcile();
            run->appendPcm(pcm.data(), 100);
            run->reconcile();
            run->serviceUntil(std::chrono::steady_clock::now()
                              + std::chrono::seconds(1), 32);
        });
        oldBlockReady.get_future().wait();
        stage->publish({{1, 2, 8000, 2, 3, 100, 1.0f}});
        RemoteTciAudioStage::Result result;
        QVERIFY(!stage->popNext(&result));
        resume.set_value();
        worker.join();
        QVERIFY(stage->popNext(&result));
        QCOMPARE(result.revision, quint64{2});
        QCOMPARE(result.sequence, quint64{1});
        QVERIFY(!stage->popNext(&result));
        QCOMPARE(stage->diagnostics().liveWdspPairs, 0);
        stage->invalidate();
        QVERIFY(!stage->popNext(&result));
    }

    void remoteWorkerRunFinalizesAfterDecoderFailure()
    {
        OpusAudioEncoder encoder;
        const QVector<float> pcm(OpusAudioCodecConfig::kFrameSamples * 2, 0.1f);
        const auto encoded = encoder.encode(pcm, 0, 0, 987);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        // This retains a valid RTP/Opus packet header and duration, but the
        // two-byte payload fails libopus decode after jitter release.
        const QByteArray malformed = encoded.packet.left(14);
        QCOMPARE(inspectOpusRtp(malformed, 987).status,
                 OpusAudioCodecStatus::Accepted);
        OpusAudioDecoder check;
        QCOMPARE(check.decodeRtp(malformed, 987).status,
                 OpusAudioCodecStatus::DecodeFailed);

        auto stage = std::make_shared<RemoteTciAudioStage>(0);
        stage->publish({{1, 1, 384000, 2, 3, 100, 1.0f}});
        RemoteAudioReceiver receiver(RemoteAudioReceiver::PcmSinkMode{
            [](const float*, int) {}, stage});
        QSignalSpy failed(&receiver, &RemoteAudioReceiver::restartRequested);
        WdspThreadCheck::install(QThread::currentThread());
        QVERIFY(receiver.start(987, 0, RemoteAudioProfile::Opus));
        QTRY_COMPARE_WITH_TIMEOUT(stage->diagnostics().liveWdspPairs, 1, 3000);
        receiver.submit(malformed);
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3000);
        QCOMPARE(failed.first().at(1).value<RemoteAudioReceiver::Fault>(),
                 RemoteAudioReceiver::Fault::DecodeFailed);
        QTRY_COMPARE_WITH_TIMEOUT(stage->diagnostics().liveWdspPairs, 0, 3000);
        receiver.stop();
        QCOMPARE(WdspThreadCheck::eventLoopEntries(), quint64{0});
        WdspThreadCheck::uninstall();
    }

    void remoteMailboxesStayWithinTwoFramesForEightClientsOnBothReceivers()
    {
        std::array<std::shared_ptr<RemoteTciAudioStage>, 2> stages{
            std::make_shared<RemoteTciAudioStage>(0),
            std::make_shared<RemoteTciAudioStage>(1)};
        std::vector<RemoteTciAudioStage::ClientConfig> clients;
        for (quint64 token = 1; token <= 8; ++token) {
            clients.push_back({token, 1, 384000, 2, 3, 2048, 1.0f});
        }
        for (const auto& stage : stages) { stage->publish(clients); }
        std::vector<float> pcm(4 * 2048 * 2, 0.125f);
        std::array<std::thread, 2> workers;
        for (int rx = 0; rx < 2; ++rx) {
            workers[rx] = std::thread([&, rx] {
                auto run = stages[rx]->createRun();
                run->reconcile();
                run->appendPcm(pcm.data(), 4 * 2048);
                while (run->hasRunnableWork()) {
                    run->serviceUntil(std::chrono::steady_clock::now()
                                      + std::chrono::microseconds(1800), 32);
                }
            });
        }
        for (auto& worker : workers) { worker.join(); }
        quint64 totalBytes = 0;
        for (const auto& stage : stages) {
            QCOMPARE(stage->diagnostics().mailboxEvictions, quint64{16});
            QCOMPARE(stage->diagnostics().liveWdspPairs, 0);
            QCOMPARE(stage->diagnostics().historySkippedFrames, quint64{0});
            RemoteTciAudioStage::Result result;
            int frames = 0;
            quint64 receiverBytes = 0;
            while (stage->popNext(&result)) {
                ++frames;
                receiverBytes += quint64(result.bytes.size());
            }
            QCOMPARE(frames, 16);
            QVERIFY(receiverBytes <= 2'098'176);
            totalBytes += receiverBytes;
        }
        QVERIFY(totalBytes <= 4'196'352);
    }

    void ninthRemoteAudioClientIsRefusedUntilAPlaceIsReleased()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        tci.setRemoteReceiverAudio(core.source());
        QSignalSpy notices(&tci, &TciServer::operatorNotice);
        QVERIFY(tci.start(0));
        std::vector<std::unique_ptr<QWebSocket>> sockets;
        std::vector<std::unique_ptr<QSignalSpy>> replies;
        for (int i = 0; i < 9; ++i) {
            auto socket = std::make_unique<QWebSocket>();
            auto reply = std::make_unique<QSignalSpy>(socket.get(),
                                                      &QWebSocket::textMessageReceived);
            QVERIFY(connectClient(*socket, tci.port()));
            socket->sendTextMessage(QStringLiteral("audio_start:0;"));
            sockets.push_back(std::move(socket));
            replies.push_back(std::move(reply));
        }
        for (int i = 0; i < 8; ++i) {
            QTRY_VERIFY_WITH_TIMEOUT(
                texts(*replies[std::size_t(i)]).contains(QStringLiteral("audio_start:0;")), 3000);
        }
        QTRY_VERIFY_WITH_TIMEOUT(notices.count() >= 1, 3000);
        QVERIFY(!texts(*replies[8]).contains(QStringLiteral("audio_start:0;")));
        QVERIFY(notices.constLast().at(1).toString().contains(QStringLiteral("eight clients")));
        sockets[0]->sendTextMessage(QStringLiteral("audio_stop:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(*replies[0]).contains(QStringLiteral("audio_stop:0;")), 3000);
        sockets[8]->sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(*replies[8]).contains(QStringLiteral("audio_start:0;")), 3000);
        tci.stop();
    }

    void remoteWorkerStageMatchesWholeBlockWdsp()
    {
        WdspThreadCheck::install(QThread::currentThread());
        for (int rate : {8000, 48000, 384000}) {
            for (int blockFrames : {100, 2048}) {
                for (int channels : {1, 2}) {
                    for (int type = 0; type <= 3; ++type) {
                        auto stage = std::make_shared<RemoteTciAudioStage>(0);
                        constexpr float kGain = 0.7f;
                        stage->publish({{1, 1, rate, channels, type, blockFrames, kGain}});
                        std::vector<float> pcm(std::size_t(blockFrames) * 2);
                        for (int i = 0; i < blockFrames; ++i) {
                            pcm[std::size_t(2 * i)] = float(0.25 * std::sin(i * 0.017));
                            pcm[std::size_t(2 * i + 1)] = float(0.18 * std::cos(i * 0.029));
                        }
                        QByteArray actual;
                        QByteArray expected;
                        std::thread worker([&] {
                            auto run = stage->createRun();
                            run->reconcile();
                            run->appendPcm(pcm.data(), blockFrames);
                            while (run->hasRunnableWork()) {
                                run->serviceUntil(std::chrono::steady_clock::now()
                                                  + std::chrono::seconds(1), 32);
                            }
                            RemoteTciAudioStage::Result result;
                            if (stage->popNext(&result)) { actual = std::move(result.bytes); }
                            run.reset();

                            std::vector<float> left(static_cast<std::size_t>(blockFrames), 0.0f);
                            std::vector<float> right(static_cast<std::size_t>(blockFrames), 0.0f);
                            for (int i = 0; i < blockFrames; ++i) {
                                left[std::size_t(i)] = pcm[std::size_t(2 * i)] * kGain;
                                right[std::size_t(i)] = pcm[std::size_t(2 * i + 1)] * kGain;
                            }
                            std::vector<float> output;
                            if (rate == 48000) {
                                for (int i = 0; i < blockFrames; ++i) {
                                    output.push_back(left[std::size_t(i)]);
                                    if (channels == 2) { output.push_back(right[std::size_t(i)]); }
                                }
                            } else {
                                void* l = create_resampleFV(48000, rate);
                                void* r = create_resampleFV(48000, rate);
                                std::vector<float> outL(std::size_t(blockFrames) * 8);
                                std::vector<float> outR(std::size_t(blockFrames) * 8);
                                int nl = 0, nr = 0;
                                xresampleFV(left.data(), outL.data(), blockFrames, &nl, l);
                                if (channels == 2) {
                                    xresampleFV(right.data(), outR.data(), blockFrames, &nr, r);
                                }
                                const int count = channels == 2 ? std::min(nl, nr) : nl;
                                for (int i = 0; i < count; ++i) {
                                    output.push_back(outL[std::size_t(i)]);
                                    if (channels == 2) { output.push_back(outR[std::size_t(i)]); }
                                }
                                destroy_resampleFV(l);
                                destroy_resampleFV(r);
                            }
                            expected = TciBinaryFrame::buildStreamPayload(
                                0, rate, type, int(output.size()),
                                int(TciStreamType::RxAudioStream), channels, output.data());
                        });
                        worker.join();
                        QVERIFY2(actual == expected,
                            qPrintable(QStringLiteral("rate=%1 frames=%2 channels=%3 type=%4")
                                .arg(rate).arg(blockFrames).arg(channels).arg(type)));
                        QCOMPARE(stage->diagnostics().liveWdspPairs, 0);
                    }
                }
            }
        }
        QCOMPARE(WdspThreadCheck::eventLoopEntries(), quint64{0});
        WdspThreadCheck::uninstall();
    }
    void initTestCase()
    {
        const QString profile = QStringLiteral("tci-remote-window-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ---- No local taps, no local DSP ------------------------------------
    void remoteWindowHooksNoLocalReceiverOrIq()
    {
        RadioModel remote(RadioModel::Role::Remote);
        const int handOutsBefore = remote.localDspHandOutCount();
        TciServer tci(&remote);
        QVERIFY(tci.isRemoteWindow());
        QVERIFY(tci.start(0));
        QWebSocket client;
        QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
        QVERIFY(connectClient(client, tci.port()));
        client.sendTextMessage(QStringLiteral("rx_sensors_enable:true,30;"));
        client.sendTextMessage(QStringLiteral("iq_start:0;"));
        QTest::qWait(150);
        tci.setTciTxGainLinear(0.5f);
        QCOMPARE(tci.tciTxPeakAbs(), 0.0f);
        // Radio I/Q never reaches a remote window's apps.
        tci.injectRawIqForTest(QVector<float>(2048, 0.25f));
        QTest::qWait(100);
        QCOMPARE(binary.count(), 0);
        QCOMPARE(tci.activeIqSubscriberCount(0), 0);
        // No receiver, I/Q or transmit channel of this computer was reached.
        // (The volume broadcast subscribes to this computer's playback
        // engine, which the master output drives in a remote window too.)
        QVERIFY2(!remote.localDspHandOutNames().contains(QByteArrayLiteral("wdspEngine"))
                     && !remote.localDspHandOutNames().contains(QByteArrayLiteral("receiverManager")),
                 qPrintable(QStringLiteral("%1 local DSP hand-outs, was %2")
                                .arg(remote.localDspHandOutCount()).arg(handOutsBefore)));
        client.close();
        tci.stop();
        // A local window keeps its taps: the flag follows the model's role.
        RadioModel local;
        TciServer localTci(&local);
        QVERIFY(!localTci.isRemoteWindow());
    }

    void remoteInitBurstSaysReceiveOnly()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        QVERIFY(tci.start(0));
        QWebSocket client;
        QSignalSpy text(&client, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(client, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
        const QStringList lines = texts(text);
        QVERIFY(lines.contains(QStringLiteral("receive_only:true;")));
        QVERIFY(!lines.contains(QStringLiteral("receive_only:false;")));
        QVERIFY(lines.contains(QStringLiteral("tx_enable:0,false;")));
        QVERIFY(lines.contains(QStringLiteral("tx_enable:1,false;")));
        client.close();
        tci.stop();
    }

    // ---- iPhone app plan Task 35: transmit forwarded to the Core ---------

    // A refused key (the holder rule: this window's device does not hold
    // transmit) takes no TX audio lock and is answered as a refused trx;
    // the Core's reason reaches the operator, never the wire.
    void aRefusedProgramKeyTakesNoAudioLockAndIsAnsweredFalse()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.forwardsRemoteTransmit());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
        // The Core decides each key, so the window is not receive-only.
        QVERIFY(texts(text).contains(QStringLiteral("receive_only:false;")));
        text.clear();

        app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
        // Nothing is taken while the Core has not answered.
        QCOMPARE(tci.activeTxClientCount(), 0);
        core.refuse(kProgramNeedsTransmit);
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("trx:0,false;")), 3000);
        QCOMPARE(tci.activeTxClientCount(), 0);
        QCOMPARE(tci.remoteKeyEpoch(), 0u);
        QCOMPARE(tci.operatorNoticeReason(), kProgramNeedsTransmit);
        QVERIFY(!texts(text).contains(QStringLiteral("trx:0,true;")));
        for (const QString& line : texts(text)) {
            QVERIFY2(!line.contains(QStringLiteral("program")), qPrintable(line));
        }
        QVERIFY(!remote.mox());
        app.close();
        tci.stop();
    }

    // An admitted key takes the TX audio lock only then; a second app's
    // trx while this window's key is on does nothing (the Thetis rule
    // among one server's apps); an app's trx:N,false releases this
    // window's key alone, by its epoch.
    void anAdmittedKeyTakesTheLockAndAReleaseUnkeysOnlyThisWindowsKey()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.start(0));
        QWebSocket wsjtx;
        QWebSocket logger;
        QSignalSpy wsjtxText(&wsjtx, &QWebSocket::textMessageReceived);
        QSignalSpy loggerText(&logger, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(wsjtx, tci.port()));
        QVERIFY(connectClient(logger, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(loggerText).contains(QStringLiteral("ready;")), 3000);
        wsjtxText.clear();
        loggerText.clear();

        wsjtx.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
        QCOMPARE(tci.activeTxClientCount(), 0);
        core.accept(42);
        QCOMPARE(tci.remoteKeyEpoch(), 42u);
        QCOMPARE(tci.activeTxClientCount(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(texts(wsjtxText).contains(QStringLiteral("trx:0,true;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(loggerText).contains(QStringLiteral("trx:0,true;")), 3000);

        // A second app's trx while the key is on: nothing is sent to the
        // Core and nothing changes.
        logger.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTest::qWait(150);
        QCOMPARE(core.keys, 1);
        QCOMPARE(tci.activeTxClientPeer().isEmpty(), false);

        // Its trx:0,false releases this window's key, by epoch.
        loggerText.clear();
        logger.sendTextMessage(QStringLiteral("trx:0,false;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.unkeys, QList<quint32>{42u}, 3000);
        QCOMPARE(tci.activeTxClientCount(), 0);
        QCOMPARE(tci.remoteKeyEpoch(), 0u);
        QTRY_VERIFY_WITH_TIMEOUT(texts(loggerText).contains(QStringLiteral("trx:0,false;")), 3000);
        // A second release has nothing of this window's to end.
        wsjtx.sendTextMessage(QStringLiteral("trx:0,false;"));
        QTest::qWait(150);
        QCOMPARE(core.unkeys.size(), 1);
        wsjtx.close();
        logger.close();
        tci.stop();
    }

    // The Core ending this window's key on its own (a safety stop or a
    // take) releases the lock here, and sends no unkey of its own.
    void theCoreEndingTheKeyReleasesTheLock()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
        app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
        core.accept(7);
        QCOMPARE(tci.activeTxClientCount(), 1);
        QVERIFY(remote.applyMirroredValue("transmitting", true).isEmpty());
        text.clear();
        QVERIFY(remote.applyMirroredValue("transmitting", false).isEmpty());
        QCOMPARE(tci.activeTxClientCount(), 0);
        QCOMPARE(tci.remoteKeyEpoch(), 0u);
        QVERIFY(core.unkeys.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("trx:0,false;")), 3000);
        app.close();
        tci.stop();
    }

    // An app that lets go before the Core answers: the admitted key is
    // released at once and nothing is taken.
    void aReleaseBeforeTheAnswerUnkeysTheAdmittedKeyAtOnce()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
        app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
        app.sendTextMessage(QStringLiteral("trx:0,false;"));
        QTest::qWait(150);
        core.accept(9);
        QCOMPARE(core.unkeys, QList<quint32>{9u});
        QCOMPARE(tci.activeTxClientCount(), 0);
        QCOMPARE(tci.remoteKeyEpoch(), 0u);
        app.close();
        tci.stop();
    }

    // The app whose audio the key carried disconnects: nothing would send
    // its trx:0,false, so the window's key is released on the Core.
    void theKeyingAppLeavingReleasesTheKey()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.start(0));
        {
            QWebSocket app;
            QSignalSpy text(&app, &QWebSocket::textMessageReceived);
            QVERIFY(connectClient(app, tci.port()));
            QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
            app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
            QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
            core.accept(11);
            QCOMPARE(tci.activeTxClientCount(), 1);
            app.close();
        }
        QTRY_COMPARE_WITH_TIMEOUT(core.unkeys, QList<quint32>{11u}, 3000);
        QCOMPARE(tci.activeTxClientCount(), 0);
        tci.stop();
    }

    // Fix round 2 (Critical 1, RD-C1): an app that keyed without TCI audio
    // (trx:0,true; with no ",tci") and then leaves releases the window's
    // key on the Core, as the TCI-audio app does above.
    void anAppKeyedWithoutTciAudioLeavingReleasesTheKey()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.start(0));
        {
            QWebSocket app;
            QSignalSpy text(&app, &QWebSocket::textMessageReceived);
            QVERIFY(connectClient(app, tci.port()));
            QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
            app.sendTextMessage(QStringLiteral("trx:0,true;"));
            QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
            core.accept(21);
            QCOMPARE(tci.remoteKeyEpoch(), 21u);
            QCOMPARE(tci.activeTxClientCount(), 0);
            app.close();
        }
        QTRY_COMPARE_WITH_TIMEOUT(core.unkeys, QList<quint32>{21u}, 3000);
        QCOMPARE(tci.remoteKeyEpoch(), 0u);
        tci.stop();
    }

    // Fix round 2 (Critical 1, RD-C1): the app leaves while its key is being
    // asked for; the Core's accepted answer is released at once and no key
    // is kept for an app that is gone.
    void anAppLeavingWhileItsKeyIsAskedForReleasesTheAnswer()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        tci.setRemoteTransmit(core.forwarder());
        QVERIFY(tci.start(0));
        {
            QWebSocket app;
            QSignalSpy text(&app, &QWebSocket::textMessageReceived);
            QVERIFY(connectClient(app, tci.port()));
            QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
            app.sendTextMessage(QStringLiteral("trx:0,true;"));
            QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
            app.close();
        }
        QTRY_COMPARE_WITH_TIMEOUT(tci.clientCount(), 0, 3000);
        QVERIFY(core.unkeys.isEmpty());
        core.accept(22);
        QCOMPARE(core.unkeys, QList<quint32>{22u});
        QCOMPARE(tci.remoteKeyEpoch(), 0u);
        tci.stop();
        QCOMPARE(core.unkeys, QList<quint32>{22u});
    }

    // Without a forwarder (an older Core) transmit stays refused as before.
    void withoutAForwarderTransmitStaysRefused()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        QVERIFY(!tci.forwardsRemoteTransmit());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
        QVERIFY(texts(text).contains(QStringLiteral("receive_only:true;")));
        text.clear();
        app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("trx:0,false;")), 3000);
        QCOMPARE(tci.operatorNoticeReason(),
                 QString::fromLatin1(TciServer::kRemoteTransmitRefusedReason));
        QCOMPARE(tci.activeTxClientCount(), 0);
        app.close();
        tci.stop();
    }

    // ---- Receive audio through a fake source ----------------------------

    // Two apps on receiver 0 each get every frame; the Core is asked once
    // and released when the last app stops.
    void twoRemoteAppsOnOneReceiverEachGetAllOfIt()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        tci.setRemoteReceiverAudio(core.source());
        QVERIFY(tci.start(0));
        QWebSocket appA;
        QWebSocket appB;
        QSignalSpy binaryA(&appA, &QWebSocket::binaryMessageReceived);
        QSignalSpy binaryB(&appB, &QWebSocket::binaryMessageReceived);
        QSignalSpy textA(&appA, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(appA, tci.port()));
        QVERIFY(connectClient(appB, tci.port()));

        // Nothing is asked for until an app listens.
        QTest::qWait(50);
        QVERIFY(core.requests.isEmpty());
        appA.sendTextMessage(QStringLiteral("audio_start:0;"));
        appB.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(0), 3000);
        QTest::qWait(50);
        QCOMPARE(core.requests, QList<int>{0});
        QTRY_VERIFY_WITH_TIMEOUT(texts(textA).contains(QStringLiteral("audio_start:0;")), 3000);

        constexpr int kBlockFrames = 2048;
        constexpr int kBlocks = 3;
        core.deliverRamp(0, 0, kBlockFrames * kBlocks);
        QTest::qWait(300);
        for (QSignalSpy* binary : {&binaryA, &binaryB}) {
            const RxAudio audio = rxAudio(*binary, 0);
            QCOMPARE(audio.blocks, kBlocks);
            QCOMPARE(audio.channels, 2u);
            for (int k = 0; k < kBlockFrames * kBlocks; ++k) {
                QVERIFY2(audio.samples.at(2 * k) == FakeReceiverSource::ramp(k)
                             && audio.samples.at(2 * k + 1) == -FakeReceiverSource::ramp(k),
                         qPrintable(QStringLiteral("frame %1 differs").arg(k)));
            }
        }

        // One app stops: the stream stays for the other.
        appA.sendTextMessage(QStringLiteral("audio_stop:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(textA).contains(QStringLiteral("audio_stop:0;")), 3000);
        QVERIFY(core.releases.isEmpty());
        QVERIFY(tci.remoteReceiverRequested(0));
        // The last one leaves (by disconnecting): released, once.
        appB.close();
        QTRY_COMPARE_WITH_TIMEOUT(core.releases, QList<int>{0}, 3000);
        QVERIFY(!tci.remoteReceiverRequested(0));
        appA.close();
        tci.stop();
        QCOMPARE(core.releases, QList<int>{0});
    }

    void remoteMonoIsTheLeftChannel()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        tci.setRemoteReceiverAudio(core.source());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy binary(&app, &QWebSocket::binaryMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_stream_channels:1;"));
        app.sendTextMessage(QStringLiteral("audio_start:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(1), 3000);
        core.deliverRamp(1, 0, 4096);
        QTest::qWait(300);
        const RxAudio audio = rxAudio(binary, 1);
        QCOMPARE(audio.blocks, 2);
        QCOMPARE(audio.channels, 1u);
        QCOMPARE(audio.samples.size(), 4096);
        for (int k = 0; k < 4096; ++k) {
            QVERIFY2(audio.samples.at(k) == FakeReceiverSource::ramp(k),
                     qPrintable(QStringLiteral("mono sample %1 differs").arg(k)));
        }
        app.close();
        tci.stop();
    }

    // Stereo at 12 kHz from a remote window: each channel is resampled on
    // its own, so a 440 Hz left and a 1000 Hz right arrive apart.
    void remoteStereoAt12kKeepsLeftAndRightApart()
    {
        RadioModel remote(RadioModel::Role::Remote);
        // Outlives the server, whose teardown releases through it.
        FakeReceiverSource core;
        TciServer tci(&remote);
        tci.setRemoteReceiverAudio(core.source());
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy binary(&app, &QWebSocket::binaryMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_samplerate:12000;"));
        app.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(0), 3000);
        IReceiverPcmSink* const sink = core.sinks.value(0);
        QVERIFY(sink != nullptr);
        constexpr int kPacket = 1920;
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        std::vector<float> pcm(kPacket * 2);
        for (int sent = 0; sent < 48000; sent += kPacket) {
            for (int i = 0; i < kPacket; ++i) {
                const double t = double(sent + i) / 48000.0;
                pcm[size_t(2 * i)] = float(0.3 * std::sin(kTwoPi * 440.0 * t));
                pcm[size_t(2 * i + 1)] = float(0.2 * std::sin(kTwoPi * 1000.0 * t));
            }
            sink->receiverAudioBlock(0, pcm.data(), kPacket);
            QTest::qWait(10);
        }
        QTest::qWait(300);
        const RxAudio audio = rxAudio(binary, 0);
        QCOMPARE(audio.channels, 2u);
        QVERIFY2(audio.samples.size() >= 2 * 11000, qPrintable(QString::number(audio.samples.size())));
        constexpr int kSkip = 600;
        const double leftLow = amplitudeAt(audio.samples, 2, 0, 440.0, 12000, kSkip);
        const double leftHigh = amplitudeAt(audio.samples, 2, 0, 1000.0, 12000, kSkip);
        const double rightLow = amplitudeAt(audio.samples, 2, 1, 440.0, 12000, kSkip);
        const double rightHigh = amplitudeAt(audio.samples, 2, 1, 1000.0, 12000, kSkip);
        QVERIFY2(std::abs(leftLow - 0.3) < 0.01 && leftHigh < 0.005,
                 qPrintable(QStringLiteral("left: 440 Hz %1, 1000 Hz %2").arg(leftLow).arg(leftHigh)));
        QVERIFY2(std::abs(rightHigh - 0.2) < 0.01 && rightLow < 0.005,
                 qPrintable(QStringLiteral("right: 440 Hz %1, 1000 Hz %2").arg(rightLow).arg(rightHigh)));
        app.close();
        tci.stop();
    }

    // rx_sensors in a remote window read the Core's slice meter the window
    // mirrors (the value its own S-meter draws, calibrated at the Core as
    // a local window calibrates its own), -140 dBm until a reading arrives.
    void remoteRxSensorsReadTheCoresMeter()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("rx_sensors_enable:true,30;"));
        // No slice mirrored yet: the floor.
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("rx_sensors:0,-140.0;")), 3000);

        QVERIFY(remote.addSliceWithStationId(0) >= 0);
        SliceModel* slice = remote.sliceById(0);
        QVERIFY(slice != nullptr);
        // The Core's no-reading sentinel is not a level.
        slice->setSignalAverageDbm(-400.0);
        text.clear();
        QTest::qWait(120);
        QVERIFY(texts(text).contains(QStringLiteral("rx_sensors:0,-140.0;")));
        slice->setSignalAverageDbm(-73.4);
        text.clear();
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("rx_sensors:0,-73.4;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(text).contains(QStringLiteral("rx_channel_sensors:0,0,-73.4;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(text).contains(QStringLiteral("rx_channel_sensors_ex:0,0,-73.4,-73.4,-73.4;")), 3000);
        // Still no receiver of this computer's reached.
        QVERIFY(!remote.localDspHandOutNames().contains(QByteArrayLiteral("wdspEngine")));
        app.close();
        tci.stop();
        remote.removeSliceWithStationId(0);
    }

    // The Core's "cannot send" answer can come after audio_start was
    // echoed (the media connection came up later): every app on that
    // receiver is then told the stream stopped, and it is released.
    void aLateUnavailableAnswerStopsTheApp()
    {
        RadioModel remote(RadioModel::Role::Remote);
        // Outlives the server, whose teardown releases through it.
        FakeReceiverSource core;
        TciServer tci(&remote);
        tci.setRemoteReceiverAudio(core.source());
        QVERIFY(tci.start(0));
        QWebSocket appA;
        QWebSocket appB;
        QSignalSpy textA(&appA, &QWebSocket::textMessageReceived);
        QSignalSpy textB(&appB, &QWebSocket::textMessageReceived);
        QSignalSpy binaryA(&appA, &QWebSocket::binaryMessageReceived);
        QVERIFY(connectClient(appA, tci.port()));
        QVERIFY(connectClient(appB, tci.port()));
        appA.sendTextMessage(QStringLiteral("audio_start:0;"));
        appB.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(textA).contains(QStringLiteral("audio_start:0;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(textB).contains(QStringLiteral("audio_start:0;")), 3000);
        QCOMPARE(core.requests, QList<int>{0});

        IReceiverPcmSink* const sink = core.sinks.value(0);
        QVERIFY(sink != nullptr);
        sink->receiverAudioStopped(
            0, QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason));
        QTRY_VERIFY_WITH_TIMEOUT(texts(textA).contains(QStringLiteral("audio_stop:0;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(textB).contains(QStringLiteral("audio_stop:0;")), 3000);
        QCOMPARE(core.releases, QList<int>{0});
        QVERIFY(!tci.remoteReceiverRequested(0));
        // Nothing more is sent for it.
        const int before = rxAudio(binaryA, 0).blocks;
        sink->receiverAudioBlock(0, std::vector<float>(4096 * 2, 0.1f).data(), 4096);
        QTest::qWait(200);
        QCOMPARE(rxAudio(binaryA, 0).blocks, before);
        appA.close();
        appB.close();
        tci.stop();
    }

    // Follow-up: a stop is one notice per receiver. The same reason for
    // the other receiver is its own notice; a repeat for the same receiver
    // is not toasted again.
    void aStopIsANoticePerReceiver()
    {
        RadioModel remote(RadioModel::Role::Remote);
        FakeReceiverSource core;
        TciServer tci(&remote);
        tci.setRemoteReceiverAudio(core.source());
        QSignalSpy stops(&tci, &TciServer::receiverStopNotice);
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_start:0;"));
        app.sendTextMessage(QStringLiteral("audio_start:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(tci.remoteReceiverRequested(0) && tci.remoteReceiverRequested(1), 3000);
        core.sinks.value(0)->receiverAudioStopped(0, QStringLiteral("slice-removed"));
        core.sinks.value(1)->receiverAudioStopped(1, QStringLiteral("slice-removed"));
        core.sinks.value(0)->receiverAudioStopped(0, QStringLiteral("slice-removed"));
        QCOMPARE(stops.count(), 3);
        QCOMPARE(stops.at(0).at(0).toInt(), 0);
        QVERIFY(stops.at(0).at(2).toBool());
        QCOMPARE(stops.at(1).at(0).toInt(), 1);
        QVERIFY(stops.at(1).at(2).toBool());
        QCOMPARE(stops.at(2).at(0).toInt(), 0);
        QVERIFY(!stops.at(2).at(2).toBool());
        app.close();
        tci.stop();
    }

    // An older Core cannot send a receiver's audio: audio_start is not
    // echoed, nothing stays requested, and the operator hears why (in the
    // notice, never on the TCI wire).
    void aCoreWithoutReceiverAudioLeavesAudioStartUnanswered()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        core.answerRequestsWith =
            QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason);
        tci.setRemoteReceiverAudio(core.source());
        QSignalSpy notices(&tci, &TciServer::operatorNotice);
        QVERIFY(tci.start(0));
        TciApplet applet(&tci);
        QVERIFY(applet.noticeText().isEmpty());
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.requests, QList<int>{0}, 3000);
        QTest::qWait(150);
        QVERIFY(!texts(text).contains(QStringLiteral("audio_start:0;")));
        QCOMPARE(core.releases, QList<int>{0});
        QVERIFY(!tci.remoteReceiverRequested(0));
        QCOMPARE(notices.count(), 1);
        const QString reason = notices.constFirst().at(1).toString();
        QCOMPARE(reason, QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason));
        QVERIFY(notices.constFirst().at(2).toBool());   // toasted
        QVERIFY(OperatorWording::isPlain(OperatorReasonText::forDisplay(reason)));
        // The TCI applet shows it on its notice line.
        QCOMPARE(applet.noticeText(), OperatorReasonText::forDisplay(reason));
        for (const QString& line : texts(text)) {
            QVERIFY2(!line.contains(reason), qPrintable(line));
        }
        app.close();
        tci.stop();
    }

    // A stop the Core reports reaches the operator in plain words, and the
    // notice clears once that receiver's audio flows again.
    void aStoppedReceiverIsExplainedAndClearsWhenAudioReturns()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeReceiverSource core;
        tci.setRemoteReceiverAudio(core.source());
        QSignalSpy notices(&tci, &TciServer::operatorNotice);
        QSignalSpy cleared(&tci, &TciServer::operatorNoticeCleared);
        QVERIFY(tci.start(0));
        TciApplet applet(&tci);
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("audio_start:0;")), 3000);

        core.sinks.value(0)->receiverAudioStopped(0, QStringLiteral("slice-removed"));
        QCOMPARE(notices.count(), 1);
        QCOMPARE(tci.operatorNoticeReason(), QStringLiteral("slice-removed"));
        const QString shown = OperatorReasonText::forDisplay(tci.operatorNoticeReason());
        QVERIFY2(OperatorWording::isPlain(shown), qPrintable(shown));
        QVERIFY(shown != QStringLiteral("slice-removed"));
        QCOMPARE(applet.noticeText(), shown);
        // The same stop again is not toasted twice.
        core.sinks.value(0)->receiverAudioStopped(0, QStringLiteral("slice-removed"));
        QCOMPARE(notices.count(), 2);
        QVERIFY(!notices.at(1).at(2).toBool());
        // The Core sends it again: audio flows and the notice goes.
        core.deliverRamp(0, 0, 1920);
        QTRY_COMPARE_WITH_TIMEOUT(cleared.count(), 1, 3000);
        QVERIFY(tci.operatorNoticeReason().isEmpty());
        QVERIFY(applet.noticeText().isEmpty());
        // The window's media not being ready (the link is down, or not up
        // yet) is shown but not toasted: the window says so already.
        core.sinks.value(0)->receiverAudioStopped(0, QStringLiteral("media-not-ready"));
        QCOMPARE(notices.count(), 3);
        QVERIFY(!notices.at(2).at(2).toBool());
        QVERIFY(OperatorWording::isPlain(OperatorReasonText::forDisplay(QStringLiteral("media-not-ready"))));
        // A refused transmit shown after it is not cleared by audio.
        QWebSocket other;
        QVERIFY(connectClient(other, tci.port()));
        other.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 4, 3000);
        core.deliverRamp(0, 1920, 1920);
        QTest::qWait(200);
        QCOMPARE(cleared.count(), 1);
        QCOMPARE(tci.operatorNoticeReason(),
                 QString::fromLatin1(TciServer::kRemoteTransmitRefusedReason));
        other.close();
        // Nothing of it was said to the app.
        for (const QString& line : texts(text)) {
            QVERIFY2(!line.contains(QStringLiteral("slice-removed")) && !line.contains(shown),
                     qPrintable(line));
        }
        app.close();
        tci.stop();
    }

    // ---- End to end: the real media controller against a Core ---------

    void appsHearTheCoresSlices_data()
    {
        QTest::addColumn<bool>("lossless");
        QTest::newRow("opus") << false;
        QTest::newRow("lossless") << true;
    }

    void appsHearTheCoresSlices()
    {
        QFETCH(bool, lossless);
        AppSettings::instance().setValue(
            QLatin1String(RemoteMediaController::kAudioProfileSettingKey),
            lossless ? QStringLiteral("Lossless") : QStringLiteral("Opus"));
        const auto restoreChoice = qScopeGuard([] {
            AppSettings::instance().remove(
                QLatin1String(RemoteMediaController::kAudioProfileSettingKey));
        });
        Harness h;
        QCOMPARE(h.sliceA, 0);
        QCOMPARE(h.sliceB, 1);
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        TciServer tci(&h.remote);
        wire(tci, remoteMedia);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(Harness::kFrames); });
        source.start();
        speaker.start();

        h.connectSession();
        QVERIFY(remoteMedia.receiverAudioNegotiated());
        // Nothing an app does needs this computer's speakers.
        h.remote.audioEngine()->setMasterMuted(true);

        QVERIFY(tci.start(0));
        QWebSocket appA;
        QWebSocket appB;
        QWebSocket secondAppB;
        QSignalSpy binaryA(&appA, &QWebSocket::binaryMessageReceived);
        QSignalSpy binaryB(&appB, &QWebSocket::binaryMessageReceived);
        QSignalSpy binarySecondB(&secondAppB, &QWebSocket::binaryMessageReceived);
        QSignalSpy textA(&appA, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(appA, tci.port()));
        QVERIFY(connectClient(appB, tci.port()));
        QVERIFY(connectClient(secondAppB, tci.port()));
        QTest::qWait(200);
        // The streams run only while an app listens.
        QCOMPARE(receiverRequests(coreControls, 0).size(), 0);
        QCOMPARE(receiverRequests(coreControls, 1).size(), 0);

        appA.sendTextMessage(QStringLiteral("audio_start:0;"));
        appB.sendTextMessage(QStringLiteral("audio_start:1;"));
        QTest::qWait(300);
        secondAppB.sendTextMessage(QStringLiteral("audio_start:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(textA).contains(QStringLiteral("audio_start:0;")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(frameCount(rxAudio(binaryA, 0)) >= 48000
                                     && frameCount(rxAudio(binaryB, 1)) >= 48000
                                     && frameCount(rxAudio(binarySecondB, 1)) >= 48000, 20000);

        // One request per slice, at the session's one quality choice.
        QCOMPARE(enabledRequests(coreControls, 0), 1);
        QCOMPARE(enabledRequests(coreControls, 1), 1);
        const QString wantedProfile = lossless ? QStringLiteral("lossless") : QStringLiteral("opus");
        QCOMPARE(receiverRequests(coreControls, 0).constFirst()
                     .value(QStringLiteral("profile")).toString(), wantedProfile);
        QCOMPARE(receiverRequests(coreControls, 1).constFirst()
                     .value(QStringLiteral("profile")).toString(), wantedProfile);
        const RemoteAudioProfile running =
            lossless ? RemoteAudioProfile::Lossless : RemoteAudioProfile::Opus;
        for (const RemoteReceiverAudioStatus& receiver : remoteMedia.audioStatus().receivers) {
            QCOMPARE(receiver.state, RemoteReceiverAudioStatus::State::Receiving);
            QCOMPARE(receiver.runningProfile, std::optional<RemoteAudioProfile>(running));
        }

        // TCI receiver N is the Core's slice N.
        const RxAudio a = rxAudio(binaryA, 0);
        const RxAudio b = rxAudio(binaryB, 1);
        const RxAudio secondB = rxAudio(binarySecondB, 1);
        QCOMPARE(a.channels, 2u);
        QCOMPARE(b.channels, 2u);
        constexpr int kSettle = 4800;
        for (int channel : {0, 1}) {
            const double a617 = Test::toneAmplitude(a.samples, channel, Harness::kSliceAToneHz, kSettle);
            const double a1579 = Test::toneAmplitude(a.samples, channel, Harness::kSliceBToneHz, kSettle);
            const double b1579 = Test::toneAmplitude(b.samples, channel, Harness::kSliceBToneHz, kSettle);
            const double b617 = Test::toneAmplitude(b.samples, channel, Harness::kSliceAToneHz, kSettle);
            qInfo() << (lossless ? "lossless" : "opus") << "channel" << channel
                    << "TCI rx0 617/1579" << a617 << a1579 << "TCI rx1 1579/617" << b1579 << b617;
            QVERIFY(a617 > 0.1);
            QVERIFY(a617 > 8.0 * a1579);
            QVERIFY(b1579 > 0.1);
            QVERIFY(b1579 > 8.0 * b617);
        }
        // Both apps on receiver 1 get all of it: the later app's stream is
        // the earlier app's from where it joined, sample for sample.
        const int joined = alignedStart(b.samples, secondB.samples);
        QVERIFY2(joined >= 0, "the second app's audio is not the first app's audio");
        const int overlapFrames = frameCount(b) - joined;
        QVERIFY2(std::abs(overlapFrames - frameCount(secondB)) <= 2 * 2048,
                 qPrintable(QStringLiteral("first app %1 frames from %2, second app %3")
                                .arg(frameCount(b)).arg(joined).arg(frameCount(secondB))));
        QCOMPARE(std::memcmp(b.samples.constData() + 2 * joined, secondB.samples.constData(),
                             size_t(std::min(overlapFrames, frameCount(secondB))) * 2 * sizeof(float)),
                 0);

        // Both apps on receiver 1 stop: the Core is asked to stop slice 1;
        // receiver 0 plays on.
        appB.sendTextMessage(QStringLiteral("audio_stop:1;"));
        QTest::qWait(200);
        QVERIFY(tci.remoteReceiverRequested(1));
        secondAppB.sendTextMessage(QStringLiteral("audio_stop:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(!receiverRequests(coreControls, 1).constLast()
                                      .value(QStringLiteral("enabled")).toBool(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.activeReceiverAudioStreamCount(), 1, 5000);
        const int aFrames = frameCount(rxAudio(binaryA, 0));
        QTRY_VERIFY_WITH_TIMEOUT(frameCount(rxAudio(binaryA, 0)) >= aFrames + 9600, 5000);
        // The last app leaves: nothing runs at the Core.
        appA.close();
        QTRY_VERIFY_WITH_TIMEOUT(!receiverRequests(coreControls, 0).constLast()
                                      .value(QStringLiteral("enabled")).toBool(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.activeReceiverAudioStreamCount(), 0, 5000);
        QCOMPARE(enabledRequests(coreControls, 0), 1);
        QCOMPARE(enabledRequests(coreControls, 1), 1);
        QCOMPARE(remoteErrors.count(), 0);
        secondAppB.close();
        appB.close();
        tci.stop();
    }

    // A Core without receiver streams: the wire stays exactly today's (no
    // receiver-audio op), audio_start is not echoed, and the operator is
    // told why in plain words.
    void anOlderCoreLeavesAudioStartUnanswered()
    {
        Harness h;
        h.hideReceiverAudio = true;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        TciServer tci(&h.remote);
        wire(tci, remoteMedia);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy notices(&tci, &TciServer::operatorNotice);
        h.connectSession();
        QVERIFY(!remoteMedia.receiverAudioNegotiated());
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedAudioContext().has_value(), 10000);

        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        app.sendTextMessage(QStringLiteral("audio_start:0;"));
        QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 1, 3000);
        QTest::qWait(300);
        QVERIFY(!texts(text).contains(QStringLiteral("audio_start:0;")));
        QVERIFY(!tci.remoteReceiverRequested(0));
        QCOMPARE(receiverRequests(coreControls, 0).size(), 0);
        const QString reason = notices.constFirst().at(1).toString();
        QCOMPARE(reason, QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason));
        QVERIFY(OperatorWording::isPlain(OperatorReasonText::forDisplay(reason)));
        app.close();
        tci.stop();
    }

    // vfo and modulation act on the Core's slice; each answer is what the
    // slice holds, so a refused write re-broadcasts the current value.
    void vfoAndModulationActOnTheCoresSlice()
    {
        Harness h;
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.client.isHandshakeComplete(), 10000);
        SliceModel* const stationSlice = h.station.sliceById(0);
        QVERIFY(stationSlice != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(h.remote.sliceById(0) != nullptr, 5000);
        SliceModel* const remoteSlice = h.remote.sliceById(0);

        TciServer tci(&h.remote);
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);

        app.sendTextMessage(QStringLiteral("vfo:0,0,14074000;"));
        QTRY_COMPARE_WITH_TIMEOUT(qint64(stationSlice->frequency()), qint64(14074000), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("vfo:0,0,14074000;")), 3000);

        app.sendTextMessage(QStringLiteral("modulation:0,digu;"));
        QTRY_COMPARE_WITH_TIMEOUT(SliceModel::modeName(stationSlice->dspMode()),
                                  QStringLiteral("DIGU"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("modulation:0,DIGU;")), 3000);

        // The Core's slice is locked: the write is refused and the app hears
        // the frequency the slice still has.
        stationSlice->setLocked(true);
        QTRY_VERIFY_WITH_TIMEOUT(remoteSlice->locked(), 5000);
        const int answersBefore = int(texts(text).count(QStringLiteral("vfo:0,0,14074000;")));
        app.sendTextMessage(QStringLiteral("vfo:0,0,7074000;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).count(QStringLiteral("vfo:0,0,14074000;"))
                                     > answersBefore, 3000);
        QTest::qWait(300);
        QVERIFY(!texts(text).contains(QStringLiteral("vfo:0,0,7074000;")));
        QCOMPARE(qint64(stationSlice->frequency()), qint64(14074000));
        QCOMPARE(qint64(remoteSlice->frequency()), qint64(14074000));
        app.close();
        tci.stop();
    }

    // iPhone app plan Task 36 (R-IOS-13): through a remote window, the app
    // holding the TX audio lock (its key accepted by the Core) has its
    // transmit audio handed to the window's microphone line, not to the
    // window's own TX ring; before the key is accepted nothing is handed.
    void aProgramsTransmitAudioGoesToTheMicrophoneLine()
    {
        RadioModel remote(RadioModel::Role::Remote);
        TciServer tci(&remote);
        FakeCoreTransmit core;
        int handedFrames = 0;
        int handedRate = 0;
        TciServer::RemoteTransmit forward = core.forwarder();
        forward.audio = [&handedFrames, &handedRate](const float*, int frames, int, int rate) {
            handedFrames += frames;
            handedRate = rate;
        };
        tci.setRemoteTransmit(forward);
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);

        const auto frame = [] {
            std::vector<float> samples(512, 0.1f);   // 256 stereo frames
            return TciBinaryFrame::buildStreamPayload(
                0, 12000, static_cast<int>(TciSampleType::Float32),
                static_cast<int>(samples.size()),
                static_cast<int>(TciStreamType::TxAudioStream), 2, samples.data());
        };
        // Not keyed: the audio is not the app's to send.
        app.sendBinaryMessage(frame());
        QTest::qWait(100);
        QCOMPARE(handedFrames, 0);

        app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_COMPARE_WITH_TIMEOUT(core.keys, 1, 3000);
        core.accept(7);
        QCOMPARE(tci.activeTxClientCount(), 1);
        app.sendBinaryMessage(frame());
        app.sendBinaryMessage(frame());
        QTRY_COMPARE_WITH_TIMEOUT(handedFrames, 512, 3000);
        QCOMPARE(handedRate, 12000);
        QCOMPARE(tci.peekTxRingSize(), 0);
        app.close();
        tci.stop();
    }

    // R-R3-49 / R-R3-42 (parity Task 3): an app's TX profile and MON volume
    // commands act on the Core. tx_profile_ex is heard once the Core has
    // applied it (a refused pick is answered with the Core's profile), and
    // tx_profiles_ex is the Core's list.
    void txProfilesAndMonVolumeActOnTheCore()
    {
        Harness h;
        h.station.scopeTxProfiles(QStringLiteral("AA:BB:CC:DD:EE:02"));
        MicProfileManager* const coreProfiles = h.station.micProfileManager();
        const QStringList coreNames = coreProfiles->profileNames();
        QVERIFY(coreNames.size() > 1);
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(h.client.isHandshakeComplete(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(h.remote.micProfileManager()->profileNames(), coreNames, 5000);

        TciServer tci(&h.remote);
        QVERIFY(tci.start(0));
        QWebSocket app;
        QSignalSpy text(&app, &QWebSocket::textMessageReceived);
        QVERIFY(connectClient(app, tci.port()));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("ready;")), 3000);
        const QString listLine =
            QStringLiteral("tx_profiles_ex:%1;").arg(coreNames.join(QLatin1Char(',')));
        // The init burst names the Core's profiles and its active one.
        QVERIFY(texts(text).contains(listLine));
        QVERIFY(texts(text).contains(QStringLiteral("tx_profile_ex:Default;")));

        // tx_profiles_ex answers the Core's list, never a stand-in.
        const int listsBefore = int(texts(text).count(listLine));
        app.sendTextMessage(QStringLiteral("tx_profiles_ex;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).count(listLine) > listsBefore, 3000);

        // A pick the Core takes: applied there, then heard.
        app.sendTextMessage(QStringLiteral("tx_profile_ex:AM;"));
        QTRY_COMPARE_WITH_TIMEOUT(coreProfiles->activeProfileName(), QStringLiteral("AM"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).contains(QStringLiteral("tx_profile_ex:AM;")), 3000);

        // A pick the Core refuses is never echoed; the app hears the Core's.
        const int amBefore = int(texts(text).count(QStringLiteral("tx_profile_ex:AM;")));
        app.sendTextMessage(QStringLiteral("tx_profile_ex:Nope;"));
        QTRY_VERIFY_WITH_TIMEOUT(texts(text).count(QStringLiteral("tx_profile_ex:AM;")) > amBefore,
                                 3000);
        QTest::qWait(200);
        QVERIFY(!texts(text).contains(QStringLiteral("tx_profile_ex:Nope;")));
        QCOMPARE(coreProfiles->activeProfileName(), QStringLiteral("AM"));

        // A profile added at the Core reaches the app's list.
        QVERIFY(coreProfiles->saveProfile(QStringLiteral("Late"), &h.station.transmitModel()));
        QTRY_VERIFY_WITH_TIMEOUT(
            texts(text).contains(QStringLiteral("tx_profiles_ex:%1;")
                                     .arg(coreProfiles->profileNames().join(QLatin1Char(',')))),
            3000);

        // mon_volume changes the Core's monitor level.
        const float before = h.station.transmitModel().monitorVolume();
        const auto levelFor = [](double dB) {
            return static_cast<float>(qBound(0, tciDbToLinearVolume(dB), 100)) / 100.0f;
        };
        const double db = qFuzzyCompare(levelFor(-3.0), before) ? -12.0 : -3.0;
        const float want = levelFor(db);
        QVERIFY(!qFuzzyCompare(want, before));
        app.sendTextMessage(QStringLiteral("mon_volume:%1;").arg(db, 0, 'f', 1));
        QTRY_VERIFY_WITH_TIMEOUT(qFuzzyCompare(h.station.transmitModel().monitorVolume(), want),
                                 5000);
        app.close();
        tci.stop();
    }
};

QTEST_MAIN(TestTciRemoteWindow)
#include "tst_tci_remote_window.moc"

#else  // !HAVE_WEBSOCKETS

int main() { return 0; }

#endif // HAVE_WEBSOCKETS
