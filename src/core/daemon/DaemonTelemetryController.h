#pragma once
// no-port-check: NereusSDR-original. Bounded observational Core telemetry
// collection for R-R3-32/33 (and each receiver's processing load for
// R-R3-40); no radio, media, retry or liveness policy.

#include "core/daemon/HostTelemetrySampler.h"
#include "core/session/StationTelemetry.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/ConnectionState.h"
#include "core/RadioConnection.h"
#include "models/ReceiverDspLoadSampler.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

class RadioModel;
class StationServer;

/// Publishes one bounded station snapshot per second while an authenticated,
/// snapshot-complete telemetry session exists. All observations are read-only.
class DaemonTelemetryController final : public QObject {
    Q_OBJECT
public:
    using MonotonicClock = std::function<qint64()>;
    using AudioDiagnosticsProvider = std::function<DaemonAudioDiagnostics()>;
    // Each receiver's latest processing load, or nullopt when nothing
    // measures receivers. The default reads RadioModel::receiverDspLoad for
    // every slice; it only reads the cached snapshot and never samples.
    using ReceiverLoadProvider =
        std::function<std::optional<QVector<StationReceiverTelemetry>>()>;

    static constexpr int kSamplePeriodMs = 1000;
    static constexpr int kObservationStalePeriods = 3;

    explicit DaemonTelemetryController(
        StationServer* server, RadioModel* radioModel,
        DaemonMediaController* mediaController, QObject* parent = nullptr,
        MonotonicClock clock = {},
        AudioDiagnosticsProvider audioDiagnosticsProvider = {},
        std::unique_ptr<HostTelemetrySampler> hostSampler = {},
        ReceiverLoadProvider receiverLoadProvider = {},
        std::shared_ptr<SharedHostSampler> sharedHostSampler = {});
    ~DaemonTelemetryController() override;

    bool isCollecting() const noexcept { return m_epoch != 0; }
    /// iPhone app Task 76 (ruling 9.1): collect for the one media session
    /// `epoch` names, starting now when its telemetry is available, and
    /// never for another. Without it a controller serves one session at a
    /// time, the first to start while it has none live.
    void bindToSession(quint64 epoch);
    quint64 sessionEpoch() const noexcept { return m_epoch; }

    /// One receiver's wire entry from its cached load snapshot (R-R3-40):
    /// load as a percentage of real time, absent when the receiver was idle.
    static StationReceiverTelemetry receiverTelemetry(int sliceId,
                                                      const ReceiverDspLoad& load);

    /// Runs the same bounded sample path as the 1 Hz timer. Public so tests
    /// and explicit host loops can sample without sleeping.
    void sampleNow();

#ifdef NEREUS_BUILD_TESTS
    void disableAutomaticSamplingForTest()
    {
        m_automaticSamplingEnabled = false;
        m_timer.stop();
    }
#endif

signals:
    void radioTelemetryRequested(quint64 requestId);

private slots:
    void onSessionStarted(quint64 epoch);
    void onSessionEnded(quint64 epoch);
    void onRadioConnectionStateChanged(ConnectionState state);
    void onRadioObservation(quint64 requestId, double rxMbps, double txMbps,
                            bool hasRtt, qint64 rttMs, qint64 rttAgeMs,
                            RadioDiagnosticsObservation diagnostics);

private:
    struct RadioObservation {
        double rxMbps = 0.0;
        double txMbps = 0.0;
        std::optional<qint64> rttMs;
        std::optional<qint64> rttAgeMs;
        RadioDiagnosticsObservation diagnostics;
        qint64 requestedElapsedMs = 0;
    };

    struct AudioBaseline {
        quint32 contextGeneration = 0;
        qint64 sampledElapsedMs = 0;
        std::uint64_t sourceFrames = 0;
        std::uint64_t sourceDrops = 0;
        std::uint64_t encodedPackets = 0;
        std::uint64_t encodeFailures = 0;
        std::uint64_t sendAccepted = 0;
        std::uint64_t sendRejected = 0;
    };

    qint64 clockNowMs() const;
    qint64 sessionElapsedMs() const;
    void synchronizeRadioConnection();
    void retireRadioConnection();
    void requestRadioObservation();
    void applyRadioObservation(StationTelemetrySnapshot& snapshot,
                               qint64 sampledElapsedMs) const;
    void applyAudioObservation(StationTelemetrySnapshot& snapshot,
                               qint64 sampledElapsedMs);
    // R-R3-32 (parity Task 6): the radio's PA readings and link quality.
    void applyRadioStatus(StationTelemetrySnapshot& snapshot) const;
    void stopCollecting();
    std::optional<QVector<StationReceiverTelemetry>> radioModelReceiverLoads() const;

    QPointer<StationServer> m_server;
    QPointer<RadioModel> m_radioModel;
    QPointer<DaemonMediaController> m_mediaController;
    QPointer<RadioConnection> m_radioConnection;
    QPointer<RadioConnection> m_requestedConnection;
    QMetaObject::Connection m_radioRequestConnection;
    QMetaObject::Connection m_radioReplyConnection;
    QTimer m_timer;
    QElapsedTimer m_processClock;
    MonotonicClock m_clock;
    AudioDiagnosticsProvider m_audioDiagnosticsProvider;
    // Reads this computer's procfs/sysfs (Linux only; disabled elsewhere),
    // shared with the display load governor when DaemonApp passes one
    // (R-R3-40); otherwise this controller's own. The first reading in each
    // telemetry session carries no CPU percentages, so no session reports
    // an interval that began before it.
    std::shared_ptr<SharedHostSampler> m_hostSampler;
    bool m_hostCpuBaselinePending{false};
    ReceiverLoadProvider m_receiverLoadProvider;
    std::optional<RadioObservation> m_radioObservation;
    std::optional<AudioBaseline> m_audioBaseline;
    quint64 m_epoch{0};
    quint64 m_boundEpoch{0};
    quint64 m_nextRadioRequestId{0};
    quint64 m_outstandingRadioRequestId{0};
    qint64 m_sessionStartedMs{0};
    qint64 m_radioRequestElapsedMs{0};
    quint32 m_sequence{0};
    bool m_automaticSamplingEnabled{true};
};

} // namespace NereusSDR
