// =================================================================
// src/core/audio/PipeWireStream.h  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
// =================================================================
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include "core/audio/AudioRingSpsc.h"

namespace NereusSDR {

class PipeWireThreadLoop;

struct StreamConfig {
    QString nodeName;
    QString nodeDescription;
    enum Direction : int { Output = PW_DIRECTION_OUTPUT,
                           Input  = PW_DIRECTION_INPUT } direction = Output;
    QString mediaClass;      // "Audio/Source", "Stream/Output/Audio", …
    QString mediaRole;       // "Music", "Communication", "Phone", …
    uint32_t rate     = 48000;
    uint32_t channels = 2;
    uint32_t quantum  = 512;
    QString targetNodeName;  // empty = follow default
};

// Pure: translates StreamConfig → pw_properties* owned by caller.
// Unit-testable without a running daemon.
pw_properties* configToProperties(const StreamConfig& cfg);

class PipeWireStream : public QObject {
    Q_OBJECT

public:
    explicit PipeWireStream(PipeWireThreadLoop* loop,
                            StreamConfig cfg,
                            QObject* parent = nullptr);
    ~PipeWireStream() override;

    bool open();
    void close();

    qint64 push(const char* data, qint64 bytes);
    qint64 pull(char* data, qint64 maxBytes);

    struct Telemetry {
        double   measuredLatencyMs    = 0.0;
        double   ringDepthMs          = 0.0;
        double   pwQuantumMs          = 0.0;
        double   deviceLatencyMs      = 0.0;
        uint64_t xrunCount            = 0;
        double   processCbCpuPct      = 0.0;
        QString  streamStateName      = QStringLiteral("closed");
        int      consumerCount        = 0;
        int      schedPolicy          = -1;   // SCHED_OTHER / SCHED_FIFO / SCHED_RR / -1=unprobed
        int      schedPriority        = 0;
    };
    Telemetry telemetry() const;

signals:
    void streamStateChanged(QString state);
    void telemetryUpdated();
    void errorOccurred(QString reason);

private:
    // libpipewire callbacks (static → instance dispatch).
    static void onProcessCb(void* userData);
    static void onStateChangedCb(void* userData,
                                 pw_stream_state old_,
                                 pw_stream_state new_,
                                 const char* error);
    static void onParamChangedCb(void* userData, uint32_t id,
                                 const spa_pod* param);

    void onProcessOutput();
    void onProcessInput();
    void probeSchedOnce();
    void maybeEmitTelemetry();

    // Translates a pw_stream_state enum value to its display string.
    // Safe to call from any thread; no shared state.
    static QString streamStateName(int s);

    PipeWireThreadLoop* m_loop;
    StreamConfig        m_cfg;
    pw_stream*          m_stream = nullptr;
    spa_hook            m_listener{};

    // Sized for ~170 ms at 8 B/frame (F32 stereo); push must be sizeof(float)*channels-aligned.
    AudioRingSpsc<65536> m_ring;

    std::atomic<uint64_t> m_xruns{0};
    std::atomic<double>   m_cpuPct{0.0};
    std::atomic<double>   m_latencyMs{0.0};
    std::atomic<double>   m_deviceLatencyMs{0.0};
    std::atomic<qint64>   m_lastTelemetryNs{0};
    std::atomic<int>      m_schedPolicy{-1};
    std::atomic<int>      m_schedPriority{0};
    std::atomic<bool>     m_schedProbed{false};

    // RMS of last block pushed (0..1, F32 absolute) — read by the IAudioBus
    // wrapper's rxLevel() for meter UI. Computed on the producer thread
    // inside push() (DSP thread for OUTPUT direction streams).
    std::atomic<float>    m_rxLevel{0.0f};
public:
    float rxLevel() const { return m_rxLevel.load(std::memory_order_relaxed); }
private:

    // Stream state as an atomic int holding pw_stream_state enum values.
    // Written by the PipeWire event thread (onStateChangedCb) and read by
    // the GUI thread (telemetry()) — must be atomic to avoid UB on
    // concurrent QString access (the original m_stateName was not safe).
    std::atomic<int> m_streamState{int(PW_STREAM_STATE_UNCONNECTED)};
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
