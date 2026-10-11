// =================================================================
// src/core/audio/PipeWireStream.h  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
//   2026-09-23: R-R3-44: output playback counters (outputCounters) and
//                 isStreaming() for a remote window's VAX feeder. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 10 (R-AUD-07, R-AUD-15): device
//                 stream modes. StreamConfig gains audioPosition, dontRemix
//                 and pair; setMatcherReader() makes an output read a
//                 DeviceRateMatcher from the process callback, and
//                 setInputSink() hands an input's pair to an
//                 IAudioInputSink. The graph's quantum and the device delay
//                 are published for the delay readout. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <pipewire/pipewire.h>
#include <spa/node/io.h>
#include <spa/param/audio/format-utils.h>

#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/AudioRingSpsc.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/IAudioEngineBackend.h"

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
    // Native audio plan Task 10 (R-AUD-07): a device stream on a node of
    // more than two channels takes the node's own positions and asks the
    // server not to remix them, so a pair lands on its own channels.
    QStringList audioPosition;   // empty: the server's (the FL,FR / MONO hint below)
    bool dontRemix = false;      // stream.dont-remix
    AudioChannelPair pair;       // matcher output and input sink modes: the device pair
};

// Frames a device stream reads from its matcher, or hands its input sink,
// in one piece (the scratch the modes allocate before open()).
inline constexpr int kPipeWireDeviceChunkFrames = 512;

// The matcher output mode's fill: `frames` frames of `channels` interleaved
// float into dst, read from the matcher `scratchFrames` at a time through
// scratch (2 * scratchFrames floats), the pair's channels carrying the
// stereo and every other channel zero (writeStereoToDevice).  Silence when
// the reader is not valid.  No lock, no allocation, no system call.
void pipeWireFillFromMatcher(MatcherReader& reader, float* scratch, int scratchFrames,
                             float* dst, int frames, int channels, AudioChannelPair pair);

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

    // Native audio plan Task 10.  Both before open(); with neither set the
    // stream behaves as it always has (Linux VAX).
    //
    // Output: the process callback reads the matcher (no lock of ours, no
    // allocation, no ring) and writes the pair into the stream's channels.
    void setMatcherReader(MatcherReader reader);
    // Input: the process callback converts the pair to stereo by the mic
    // pick and calls sink->onInput (the ring is not used).
    void setInputSink(IAudioInputSink* sink, MicChannelPick pick);

    // Device modes: the graph's cycle in frames (the position's duration,
    // else the frames the last cycle asked for), 0 before the first cycle.
    // Any thread.
    int graphQuantumFrames() const;
    // Device modes: pw_stream_get_time_n's delay in ns, -1 before the first
    // cycle.  Any thread.
    std::int64_t deviceDelayNs() const;
    // Matcher output mode: the reader's fade (R-AUD-06).  Any thread.
    void requestFadeOut();
    bool fadedOut() const;

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

    // R-R3-44: OUTPUT streams. Frames the graph has taken (every process
    // callback takes one buffer, audio or the silence filling it), frames
    // still queued in the ring, the ring's capacity and the last buffer's
    // size in frames. Any thread.
    struct OutputCounters {
        quint64 consumedFrames = 0;
        int queuedFrames = 0;
        int capacityFrames = 0;
        int callbackFrames = 0;
    };
    OutputCounters outputCounters() const;
    // R-R3-44: true while PipeWire drives the stream. A VAX source node is
    // paused while no app is linked to it. Any thread.
    bool isStreaming() const;

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
    static void onIoChangedCb(void* userData, uint32_t id, void* area,
                              uint32_t size);

    void onProcessOutput();
    void onProcessInput();
    void onProcessMatcherOutput();
    void onProcessSinkInput();
    void publishGraphTiming(const pw_buffer* b);
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
    // R-R3-44: see outputCounters().
    std::atomic<quint64>  m_outputConsumedFrames{0};
    std::atomic<int>      m_outputCallbackFrames{0};
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

    // Native audio plan Task 10: the device modes.  Set before open(), read
    // by the process callback only.
    MatcherReader      m_matcherReader;
    bool               m_matcherMode = false;
    IAudioInputSink*   m_inputSink = nullptr;
    MicChannelPick     m_micPick = MicChannelPick::Left;
    std::vector<float> m_stereoScratch;   // 2 * kPipeWireDeviceChunkFrames
    std::atomic<spa_io_position*> m_position{nullptr};
    std::atomic<int>          m_graphQuantum{0};
    std::atomic<std::int64_t> m_deviceDelayNs{-1};
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
