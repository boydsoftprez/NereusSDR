// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original Digital Spectrum Recorder.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; the Digital Spectrum Recorder: each
//             receiver's I/Q copied by a lock-free tap and written by the
//             recordings worker into the ring file, one second at a time,
//             with your transmit marked and its audio beside it (spec 3,
//             6.1, 6.7, A10, A17). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; DsrStatus carries the rewound slices
//             (recording plan Task 22). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; DsrStatus carries the opened stretch and
//             the saves running (recording plan Task 25). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; DsrStatus carries the receivers it keeps
//             now and the bytes they take an hour, for the Setup page's
//             hours (recording plan Task 26). AI tooling: Anthropic Claude
//             Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/SpscFloatRing.h"
#include "core/recordings/dsr/DsrRingFile.h"

#include <QList>
#include <QString>
#include <QtGlobal>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace NereusSDR {

// One slice playing the past (rewind sound, recording plan Task 22): the
// moment it plays, the frequency it is on, and the frequency LIVE returns
// it to.
struct DsrRewoundSlice {
    int slice = -1;
    qint64 utcNs = 0;
    qint64 frequencyHz = 0;
    qint64 liveFrequencyHz = 0;
};

// One receiver the recorder keeps now, at its own rate (Task 26).
struct DsrReceiverRate {
    int receiver = -1;
    int rateHz = 0;
};

// What the recorder holds and why it is off, as the recordings document
// carries it (RecordingsFacade, the `dsr` part).
struct DsrStatus {
    bool enabled = false;
    bool recording = false;
    QString disk;              // as the settings hold it; "" is the Core's own disk
    int sizeLimitGb = 0;
    QString reason;            // why it is not recording although on; "" otherwise
    qint64 sizeLimitBytes = 0;
    qint64 usedBytes = 0;
    qint64 writePosition = 0;
    qint64 oldestUtcMs = 0;    // 0 when nothing is kept
    qint64 newestUtcMs = 0;
    quint64 overflows = 0;     // tap blocks dropped because the writer fell behind
    QList<DsrRewoundSlice> rewound;   // the slices playing the past, by slice
    // Task 25: a saved whole-pan stretch opened on slice A ("" for none)
    // and its span; the saves still running.
    QString stretchId;
    qint64 stretchFromUtcNs = 0;
    qint64 stretchToUtcNs = 0;
    int saving = 0;
    // Task 26: the receivers streaming now (what it keeps, or would keep
    // when on), by receiver, and the disk bytes they take an hour.
    QList<DsrReceiverRate> receivers;
    qint64 bytesPerHour = 0;
    double hoursHeld() const
    {
        return newestUtcMs > oldestUtcMs ? double(newestUtcMs - oldestUtcMs) / 3600000.0 : 0.0;
    }
};

// Owned by RecordingService. Three sides:
// - the taps (consumeIq on the connection thread, consumeTransmitAudio on
//   the transmit thread) copy into a lock-free ring and return; no lock,
//   no allocation, no disk;
// - the main thread says which receivers stream at what rate and centre,
//   whether you transmit, and whether the taps are taken;
// - the recordings worker drains the rings into one-second 16-bit blocks
//   in the ring file.
class NEREUS_CORE_EXPORT DsrRecorder {
public:
    static constexpr int kMaxReceivers = 8;
    // Per receiver, interleaved I and Q: 1,048,576 frames, about 5.4 s at
    // 192 kHz and 0.68 s at 1536 kHz. Made once, when the receiver first
    // streams.
    static constexpr int kIqRingFloats = 1 << 21;
    // Your transmit audio, mono: about 5.4 s at 48 kHz.
    static constexpr int kTxAudioRingFloats = 1 << 18;
    // The plan's format: I and Q scaled by 32767 and clipped.
    static constexpr float kIqScale = 32767.0f;
    // At most this many seconds of transmit audio wait for a block.
    static constexpr int kTxAudioHeldSeconds = 10;

    DsrRecorder();
    ~DsrRecorder();
    DsrRecorder(const DsrRecorder&) = delete;
    DsrRecorder& operator=(const DsrRecorder&) = delete;

    // ---- The taps. Copy and return; a block that does not fit is dropped
    // and counted.

    // Connection thread (RadioModel::forkIqToTaps): `frames` interleaved
    // float I/Q pairs of receiver (stream) `receiver`.
    void consumeIq(int receiver, const float* iq, int frames) noexcept
    {
        if (receiver < 0 || receiver >= kMaxReceivers || iq == nullptr || frames <= 0) {
            return;
        }
        m_tapsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (m_accepting.load(std::memory_order_seq_cst)) {
            Lane& lane = m_lanes[static_cast<size_t>(receiver)];
            if (SpscFloatRing* ring = lane.ring.load(std::memory_order_acquire)) {
                if (!ring->tryPush(iq, frames * 2)) {
                    lane.overflows.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        m_tapsInFlight.fetch_sub(1, std::memory_order_seq_cst);
    }

    // Transmit thread (the transmit monitor): your voice as sent, kept
    // beside the I/Q while you transmit.
    void consumeTransmitAudio(const float* mono, int frames, int rateHz) noexcept
    {
        if (mono == nullptr || frames <= 0 || rateHz <= 0) {
            return;
        }
        m_tapsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (m_accepting.load(std::memory_order_seq_cst) && m_transmitting.load(std::memory_order_acquire)) {
            m_txRateHz.store(rateHz, std::memory_order_release);
            if (!m_txRing.tryPush(mono, frames)) {
                m_txOverflows.fetch_add(1, std::memory_order_relaxed);
            }
        }
        m_tapsInFlight.fetch_sub(1, std::memory_order_seq_cst);
    }

    // ---- Main thread.

    // Receiver `receiver` streams at `sampleRateHz` around `centreHz`
    // (0 Hz rate: it does not). Makes its ring the first time.
    void setStream(int receiver, qint64 centreHz, int sampleRateHz);
    void setTransmitting(bool on);
    // Taps copy only while accepting. Turning it off waits for any tap
    // still inside.
    void setAccepting(bool on);
    bool accepting() const { return m_accepting.load(std::memory_order_acquire); }
    quint64 overflows() const;
    quint64 overflows(int receiver) const;

    // ---- Worker thread.

    // Opens (or takes up) the ring file.
    bool begin(const QString& path, qint64 sizeLimitBytes, QString* error);
    // Writes each whole second waiting in the rings. False when a write
    // failed (the file is then closed).
    bool drain(qint64 nowUtcNs);
    // Drains, writes what is held as a short block and closes the file.
    bool finish(qint64 nowUtcNs);
    // Closes the file, dropping what is held (after a failed write).
    void abandon();
    bool isOpen() const { return m_ring.isOpen(); }
    // The ring's figures: limit, used, write position, oldest, newest;
    // once closed, as they were when it closed.
    DsrStatus figures() const;
    int blocksWritten() const { return m_blocksWritten; }

    // Reading (Tasks 22 and 23) and tests.
    const DsrRingFile& ring() const { return m_ring; }

private:
    struct Lane {
        std::unique_ptr<SpscFloatRing> owned;            // main thread
        std::atomic<SpscFloatRing*> ring{nullptr};       // what the tap reads
        std::atomic<qint64> centreHz{0};
        std::atomic<int> rateHz{0};
        std::atomic<quint64> overflows{0};
        // Worker thread only.
        std::vector<qint16> block;
        int held = 0;
        qint64 startNs = 0;
        qint64 centre = 0;
        int rate = 0;
        bool transmit = false;
        bool anchored = false;
        quint64 seenOverflows = 0;
    };

    bool drainLane(int receiver, Lane& lane, qint64 nowUtcNs, bool transmitting);
    bool writeHeld(int receiver, Lane& lane);
    void drainTransmitAudio();
    void resetLanes();
    DsrStatus liveFigures() const;

    std::array<Lane, kMaxReceivers> m_lanes;
    SpscFloatRing m_txRing{kTxAudioRingFloats};
    std::atomic<int> m_txRateHz{0};
    std::atomic<quint64> m_txOverflows{0};
    std::atomic<bool> m_accepting{false};
    std::atomic<bool> m_transmitting{false};
    // Set when you start transmitting; the worker takes it, so a key
    // shorter than a drain still marks its block.
    std::atomic<bool> m_transmitSeen{false};
    std::atomic<int> m_tapsInFlight{0};

    // Worker thread only.
    DsrRingFile m_ring;
    std::vector<float> m_scratch;
    std::vector<qint16> m_txHeld;
    int m_txHeldRateHz = 0;
    int m_blocksWritten = 0;
    DsrStatus m_lastFigures;
};

}  // namespace NereusSDR
