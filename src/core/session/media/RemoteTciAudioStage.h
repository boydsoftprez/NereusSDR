#pragma once
// no-port-check: NereusSDR-original bounded TCI stage for a remote PCM worker.
// Modification history (NereusSDR):
//   2026-10-27: a run that ends leaves the next run what its clients had
//               not converted and each client's unfinished block
//               (park/takeCarry), so a receiver restart or the
//               lossless-to-Opus fallback drops no audio an app was owed;
//               a client whose mailbox is full waits in the history
//               (hasRoom) instead of evicting a block, for at most
//               kMaxBehindNs; skipped audio is also
//               counted in blocks (skippedBlocks). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.

#include "core/session/media/IRemotePcmWorkerStage.h"

#include <QByteArray>
#include <QtGlobal>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace NereusSDR {

class RemoteTciAudioStage final : public IRemotePcmWorkerStage {
public:
    static constexpr int kHistoryFrames = 16384;
    static constexpr int kMaxClients = 8;
    static constexpr int kMaxFramesPerClient = 2;
    static constexpr int kMaxPayloadBytes = 131136;
    // How long an app may stay behind (a whole block or more still to
    // convert, normally waiting for room in its mailbox) before it moves
    // on to the newest audio. A burst arrives faster than real time and
    // the drain catches up with it in tens of milliseconds, so it loses
    // nothing; an app the drain cannot keep up with falls at most this
    // far behind, then hears current audio again.
    static constexpr qint64 kMaxBehindNs = 250'000'000;
    // Monotonic nanoseconds; steady_clock unless a test sets one.
    using Clock = std::function<qint64()>;

    struct ClientConfig {
        quint64 token = 0;
        quint64 revision = 0;
        int rate = 48000;
        int channels = 2;
        int type = 3;
        int blockFrames = 2048;
        float gain = 1.0f;
    };
    struct ConfigSnapshot {
        quint64 receiverGeneration = 0;
        int receiver = 0;
        std::vector<ClientConfig> clients;
    };
    struct Result {
        QByteArray bytes;
        quint64 receiverGeneration = 0;
        quint64 token = 0;
        quint64 revision = 0;
        quint64 sequence = 0;
    };
    struct Diagnostics {
        quint64 historySkippedFrames = 0;
        // The same skipped audio in blocks of each app's size: what the
        // server's "cannot keep up" notice counts, as it counted the
        // blocks the mailbox used to evict.
        quint64 skippedBlocks = 0;
        quint64 socketBackpressureDrops = 0;
        quint64 serviceWallNs = 0;
        quint64 serviceCpuNs = 0;
        quint64 maxServiceWallNs = 0;
        quint64 maxQuantumWallNs = 0;
        quint64 serviceQuanta = 0;
        quint64 resamplerRecreates = 0;
        quint64 resamplerRecreateWallNs = 0;
        quint64 resamplerRecreateCpuNs = 0;
        quint64 maxResamplerRecreateWallNs = 0;
        int liveWdspPairs = 0;
        // Blocks in the mailboxes now, waiting for the server's drain.
        int queuedResults = 0;
    };

    explicit RemoteTciAudioStage(int receiver);
    ~RemoteTciAudioStage() override;
    // A run's clients carry on into the next run of the same generation:
    // a run that ends leaves here the audio its clients had not converted
    // and each client's unfinished block, and the next run goes on from
    // there. A receiver restart or a new context (the lossless link
    // trial's fallback to Opus among them) therefore drops nothing an app
    // was owed, and every app on the receiver keeps one stream.
    std::unique_ptr<IRemotePcmWorkerStage::Run> createRun() override;
    quint64 generation() const;
    // Test-only: the clock later runs measure kMaxBehindNs on. No
    // production caller.
    void setClockForTest(Clock clock);
    // Ends every client's stream: what is queued, in flight or carried is
    // dropped, and the next run starts each client afresh.
    void invalidate();
    void publish(std::vector<ClientConfig> clients);
    bool popNext(Result* out);
    void noteSocketBackpressureDrop();
    Diagnostics diagnostics() const;

private:
    friend class RemoteTciAudioRun;
    // What a run that ended leaves the next: the stereo history its
    // clients had not converted yet, and each client's place in it and
    // unfinished block. Resamplers are not carried: WDSP handles live
    // and end on the run's own thread.
    struct ClientCarry {
        quint64 revision = 0;
        quint64 sequence = 0;
        int blockInput = 0;
        int pendingFrames = 0;
        std::vector<float> output;
    };
    struct Carry {
        quint64 receiverGeneration = 0;
        std::vector<float> history;
        std::map<quint64, ClientCarry> clients;
    };
    void push(Result result);
    // Whether this client's mailbox has room for another block. A run
    // converts nothing more for a client with a full mailbox: its audio
    // waits in the run's history, so a burst the worker converts faster
    // than the drain takes it loses nothing.
    bool hasRoom(quint64 token) const;
    void park(Carry carry);
    Carry takeCarry();
    int m_receiver;
    std::atomic<quint64> m_generation{1};
    mutable std::mutex m_mutex;
    std::shared_ptr<const ConfigSnapshot> m_config;
    std::map<quint64, std::deque<Result>> m_results;
    Carry m_carry;
    Clock m_clock;
    quint64 m_nextDrainToken = 0;
    std::atomic<quint64> m_historySkippedFrames{0};
    std::atomic<quint64> m_skippedBlocks{0};
    std::atomic<quint64> m_socketBackpressureDrops{0};
    std::atomic<quint64> m_serviceWallNs{0};
    std::atomic<quint64> m_serviceCpuNs{0};
    std::atomic<quint64> m_maxServiceWallNs{0};
    std::atomic<quint64> m_maxQuantumWallNs{0};
    std::atomic<quint64> m_serviceQuanta{0};
    std::atomic<quint64> m_resamplerRecreates{0};
    std::atomic<quint64> m_resamplerRecreateWallNs{0};
    std::atomic<quint64> m_resamplerRecreateCpuNs{0};
    std::atomic<quint64> m_maxResamplerRecreateWallNs{0};
    std::atomic<int> m_liveWdspPairs{0};
};

} // namespace NereusSDR
