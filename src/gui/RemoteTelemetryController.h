#pragma once
// no-port-check: NereusSDR-original. Observational Core/GUI telemetry adapter.
#include "gui/TelemetryHistory.h"
#include "gui/RemoteAudioStatus.h"
#include "core/session/StationTelemetry.h"
#include "core/session/SessionTransport.h"
#include "core/session/TxWatchClient.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "core/session/media/MediaPeer.h"
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>

namespace NereusSDR {
class StationClient;
class RemoteMediaController;
class RadioModel;

struct RemoteTelemetryView {
    enum class State { Disconnected, Unsupported, Waiting, Current, Stale };
    State state = State::Disconnected;
    std::optional<qint64> stationAgeMs;
    StationRadioTelemetry radio;
    StationAudioTelemetry coreAudio;
    std::optional<double> controlRxKbps, controlTxKbps;
    std::optional<double> coreGuiRxKbps, coreGuiTxKbps, coreGuiTotalKbps;
    std::optional<double> audioPayloadRxKbps, audioRtpRxKbps;
    std::optional<quint64> coreRttMs;
    std::optional<qint64> coreRttAgeMs;
    RemoteAudioReceiverTelemetry playback;
    bool playbackActive = false;
    // The Core computer's latest load while station telemetry is current,
    // and whether this session's Core has reported any of it (R-R3-32/33).
    StationHostTelemetry coreHost;
    bool coreHostReported = false;
    // Each Core receiver's latest processing load while station telemetry
    // is current (absent when the Core does not measure receivers), and
    // whether this session's Core has sent the receivers section (R-R3-40).
    std::optional<QVector<StationReceiverTelemetry>> coreReceivers;
    bool coreReceiversReported = false;
    // R-R3-35: the measured audio delay at the latest sample. measurable is
    // false for a Core that does not answer clock probes, and the text then
    // reads as before.
    RemoteAudioDelayReport audioDelay;
};

// All methods run on the GUI thread. Collection continues while the dialog is
// closed; no spectrum callbacks, settings writes or connection policy live here.
class RemoteTelemetryController final : public QObject {
    Q_OBJECT
public:
    using Clock = std::function<qint64()>;
    using PlaybackObserver = std::function<RemoteAudioReceiverTelemetry()>;
    using TrafficObserver = std::function<std::optional<MediaPeerTelemetry>()>;
    using DelayObserver = std::function<RemoteAudioDelayReport()>;
    RemoteTelemetryController(StationClient* client, RemoteMediaController* media,
                              QObject* parent = nullptr,
                              Clock clock = {}, PlaybackObserver playback = {},
                              TrafficObserver traffic = {}, DelayObserver delay = {});
    const RemoteTelemetryView& current() const { return m_view; }
    const TelemetryHistory& history() const { return m_history; }
    qint64 nowMs() const;
    QString bannerText() const;
    QString detailText() const;
    // Parity ruling C13: the Core's radio-link drops and audio drops for
    // View > Performance Overlay, headed as the Core's; one line saying so
    // when no current readings are in.
    static QStringList performanceOverlayLines(const RemoteTelemetryView& view);
    // Parity ruling C9: the Core's CPU for the System tile (its system
    // share when `system`, else its own process's), or nullopt with the
    // plain reason in `reason`.
    static std::optional<double> coreCpuPercent(const RemoteTelemetryView& view, bool system,
                                                QString* reason);
    // An injected clock makes sampling manually driven for deterministic
    // lifecycle tests; the production clock keeps its automatic timer.
    void sampleNow();
    // R-R3-32 / R-R3-46 (parity Task 6): the remote window's model takes
    // the Core's PA readings from each current sample, and all of them
    // absent while the measurements are out of date or the session ended
    // (RadioModel::applyCorePaReadings). R-R3-32 (parity Task 14): and the
    // Core's HL2 link the same way (RadioModel::applyCoreHl2LinkFigures).
    void setPaReadingsTarget(RadioModel* model);
signals:
    void changed();
private:
    void receiveStation(const StationTelemetrySnapshot& sample, quint32 epoch);
    void clearSession();
    void refreshCurrent(qint64 now);
    QPointer<StationClient> m_client;
    QPointer<RemoteMediaController> m_media;
    QPointer<RadioModel> m_paTarget;
    void pushPaReadings();
    QElapsedTimer m_clock;
    Clock m_now;
    PlaybackObserver m_playback;
    TrafficObserver m_traffic;
    DelayObserver m_delay;
    QTimer m_timer;
    TelemetryHistory m_history;
    RemoteTelemetryView m_view;
    std::optional<StationTelemetrySnapshot> m_station;
    qint64 m_stationReceivedMs = 0;
    quint32 m_epoch = 0;
    bool m_stationWasStale = false;
    std::optional<SessionTransportTelemetry> m_transportBaseline;
    std::optional<RemoteAudioReceiverTelemetry> m_playbackBaseline;
    std::optional<MediaPeerTelemetry> m_mediaBaseline;
    std::optional<AuxiliaryWatchTelemetry> m_watchBaseline;
    struct PlaybackEvents {
        quint64 underflows = 0, overflows = 0;
        qint64 sampledMs = 0;
    };
    std::optional<PlaybackEvents> m_playbackEventsBaseline;
    qint64 m_lastTickMs = -1;
    bool m_coreHostReported = false;
    bool m_coreReceiversReported = false;
    std::optional<qint64> m_diagnosticsLogBaselineMs;
    void logDiagnostics(qint64 now) const;
};
} // namespace NereusSDR
