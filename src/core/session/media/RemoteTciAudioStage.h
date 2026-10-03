#pragma once
// no-port-check: NereusSDR-original bounded TCI stage for a remote PCM worker.

#include "core/session/media/IRemotePcmWorkerStage.h"

#include <QByteArray>
#include <QtGlobal>
#include <atomic>
#include <cstdint>
#include <deque>
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
        quint64 mailboxEvictions = 0;
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
    };

    explicit RemoteTciAudioStage(int receiver);
    ~RemoteTciAudioStage() override;
    std::unique_ptr<IRemotePcmWorkerStage::Run> createRun() override;
    quint64 generation() const;
    void invalidate();
    void publish(std::vector<ClientConfig> clients);
    bool popNext(Result* out);
    void noteSocketBackpressureDrop();
    Diagnostics diagnostics() const;

private:
    friend class RemoteTciAudioRun;
    void push(Result result);
    int m_receiver;
    std::atomic<quint64> m_generation{1};
    mutable std::mutex m_mutex;
    std::shared_ptr<const ConfigSnapshot> m_config;
    std::map<quint64, std::deque<Result>> m_results;
    quint64 m_nextDrainToken = 0;
    std::atomic<quint64> m_historySkippedFrames{0};
    std::atomic<quint64> m_mailboxEvictions{0};
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
