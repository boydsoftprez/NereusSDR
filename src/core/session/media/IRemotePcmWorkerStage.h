#pragma once
// no-port-check: NereusSDR-original worker extension point for remote PCM.

#include <chrono>
#include <memory>

namespace NereusSDR {

class IRemotePcmWorkerStage {
public:
    class Run {
    public:
        virtual ~Run() = default;
        virtual void appendPcm(const float* stereo, int frames) = 0;
        virtual void reconcile() = 0;
        virtual void serviceUntil(std::chrono::steady_clock::time_point deadline,
                                  int maxQuanta) = 0;
        virtual bool hasRunnableWork() const = 0;
    };

    virtual ~IRemotePcmWorkerStage() = default;
    virtual std::unique_ptr<Run> createRun() = 0;
};

} // namespace NereusSDR
