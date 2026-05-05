// =================================================================
// src/core/audio/TxMicSource.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/cmbuffs.c — original licence
//   from Thetis source is included below
//
// =================================================================

/*  cmbuffs.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — Initial port for NereusSDR by J.J. Boyd (KG4VCF),
//                 Phase 3M-1c TX pump architecture redesign v3, with
//                 AI-assisted implementation via Anthropic Claude
//                 Code.  Reimplements the Thetis Inbound() / cm_main
//                 ring + semaphore primitives in C++/Qt as the
//                 cadence-source for the TX worker thread.
// =================================================================

#pragma once

#include <QObject>
#include <QMutex>
#include <QSemaphore>

#include <atomic>
#include <vector>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// TxMicSource — ring + semaphore handoff from the network thread to
// the TX DSP worker.  Mirrors Thetis cmbuffs.c [v2.10.3.13] one-to-one:
//
//   inbound()       <==>  Inbound (cmbuffs.c:88-121)
//   waitForBlock()  <==>  WaitForSingleObject(Sem_BuffReady, ...) inside
//                          cm_main (cmbuffs.c:163)
//   drainBlock()    <==>  cmdata (cmbuffs.c:123-149)
//   start()/stop()  <==>  create_cmbuffs / destroy_cmbuffs
//                          (cmbuffs.c:35-58 / 60-76)
//
// The semantics of Thetis's CMB struct map directly:
//   r1_baseptr     -> m_ring (interleaved I/Q doubles)
//   r1_active_buffsize -> m_ring.size() / 2  (in complex pairs)
//   r1_inidx       -> m_writeIdx
//   r1_outidx      -> m_readIdx
//   r1_unqueuedsamps -> m_unqueued
//   r1_outsize     -> kBlockFrames (constant 64 here, equals
//                                   getbuffsize(48000))
//   csIN / csOUT   -> m_csIn / m_csOut
//   Sem_BuffReady  -> m_blockReadySem
//   accept         -> m_accept (atomic bool)
//   run            -> m_running (atomic bool)
//
// Single-producer / single-consumer:
//   - Producer = network thread (P1 EP6 parser, P2 port 1026 reader,
//     LOS injector).  Calls inbound().
//   - Consumer = TxWorkerThread.  Calls waitForBlock() and drainBlock().
//   - start()/stop() called from the main thread.
//
// Block size is fixed at 64 frames (kBlockFrames) to mirror the Thetis
// formula getbuffsize(48000) = base_size(64) * rate(48000) / base_rate(48000)
// — see cmsetup.c:106-110 [v2.10.3.13].  The mic feed is always 48 kHz on
// OpenHPSDR P1/P2, so the block size is constant.
// ---------------------------------------------------------------------------
class TxMicSource : public QObject {
    Q_OBJECT

public:
    /// Block size in mono frames.  Matches Thetis getbuffsize(48000) =
    /// base_size(64) * rate(48000) / base_rate(48000).
    /// From Thetis cmsetup.c:106-110 [v2.10.3.13].
    static constexpr int kBlockFrames = 64;

    /// Ring capacity expressed in blocks of kBlockFrames.  Thetis uses
    /// CMB_MULT (defined in cmcomm.h) — the practical effect is
    /// "headroom for jitter".  8 blocks gives ~10.6 ms of buffering at
    /// 48 kHz which is more than enough for OS network jitter and is
    /// well under the 3000 ms LOS timeout.
    static constexpr int kRingBlockMultiple = 8;

    explicit TxMicSource(QObject* parent = nullptr);
    ~TxMicSource() override;

    /// Producer-side: write `nsamples` mono float samples into the ring.
    /// Thread-safe (acquires m_csIn).  Called from the network thread.
    /// If `nsamples` is larger than the ring, only the most recent
    /// kRingBlockMultiple * kBlockFrames samples survive (overwrite —
    /// matches Thetis Inbound() which has no overwrite check either,
    /// see cmbuffs.c:108-109 noting "add check with *error += -1; for
    /// case when r1 is full and an overwrite occurs").
    /// No-op when m_accept is false (post-stop()).
    void inbound(const float* samples, int nsamples);

    /// Consumer-side: wait for at least one full block to be ready.
    /// `timeoutMs == -1` blocks indefinitely (mirrors INFINITE in
    /// cm_main).  Returns true if a block was acquired; false on
    /// timeout or shutdown.
    bool waitForBlock(int timeoutMs = -1);

    /// Consumer-side: drain one block of kBlockFrames into `dst`.
    /// `dst` must point to 2 * kBlockFrames doubles (interleaved I/Q
    /// pairs).  The Q channel is always zeroed (mic input is mono;
    /// matches Thetis network.c:769 `prn->TxReadBufp[2 * i + 1] = 0.0`).
    /// Thread-safe (acquires m_csOut).  Must follow a successful
    /// waitForBlock().
    void drainBlock(double* dst);

    /// Alternate consumer-side primitive: drain up to `n` mono float
    /// samples from the same ring as drainBlock.  Reads only the
    /// I-channel (mic input is mono).  Returns the number of samples
    /// written; on underrun, zero-fills the remainder and still returns
    /// `n`.  Returns 0 only when `dst` is null or `n <= 0`.
    ///
    /// Used by CompositeTxMicRouter::pullSamples for the Radio source
    /// path (TxMicRouter abstract API works in mono floats, not
    /// interleaved I/Q doubles).  Acquires the same m_csOut lock as
    /// drainBlock; only ONE of {pullSamples, drainBlock} should be the
    /// active consumer at any time — the production path uses drainBlock
    /// via TxWorkerThread; pullSamples is reserved for the future
    /// TxChannel::m_micRouter wiring (see TxChannel.h:2128-2133) and
    /// for unit tests.
    ///
    /// Lock-free constraint: this method holds m_csOut briefly. It is
    /// safe to call from the WDSP audio thread because m_csOut is only
    /// otherwise contended by drainBlock (single-consumer in production
    /// — never concurrent with itself).
    int pullSamples(float* dst, int n);

    /// Lifecycle.  start() opens the inbound gate and resets the ring;
    /// stop() closes the gate, releases any waiter via a poison
    /// semaphore release, and prevents further block consumption.
    /// Mirrors Thetis create_cmbuffs / destroy_cmbuffs in cmbuffs.c.
    void start();
    void stop();

    /// True once start() has been called and stop() has not yet been
    /// invoked.  Read by the worker loop's run-condition.
    bool isRunning() const noexcept;

private:
    void resetRingLocked();   // m_csIn AND m_csOut must be held
    int  ringFrameCapacity() const noexcept;

    // Ring: interleaved I/Q doubles.  Capacity =
    // kRingBlockMultiple * kBlockFrames (in frames) * 2 (doubles per
    // frame).  Allocated in ctor; never resized.
    std::vector<double> m_ring;

    // Indices in mono-frame units (0 .. ringFrameCapacity()-1).
    // m_writeIdx is mutated by inbound() under m_csIn.
    // m_readIdx  is mutated by drainBlock() under m_csOut.
    int m_writeIdx{0};
    int m_readIdx{0};

    // Unqueued sample counter — Thetis r1_unqueuedsamps.  Mutated by
    // inbound() only, under m_csIn.  Drives semaphore release count.
    int m_unqueued{0};

    QMutex     m_csIn;
    QMutex     m_csOut;
    QSemaphore m_blockReadySem;

    // Lifecycle flags.  std::atomic so the producer can read m_accept
    // without holding m_csIn (matches Thetis's _InterlockedAnd on
    // a->accept inside Inbound at cmbuffs.c:95).  m_running is read by
    // both the worker (top of loop) and stop() — atomic bool covers
    // the simple read/write pattern.
    std::atomic<bool> m_accept{false};
    std::atomic<bool> m_running{false};
};

} // namespace NereusSDR
