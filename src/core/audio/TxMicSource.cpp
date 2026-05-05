// =================================================================
// src/core/audio/TxMicSource.cpp  (NereusSDR)
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
//                 Code.
// =================================================================

#include "TxMicSource.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cstring>

Q_LOGGING_CATEGORY(lcTxMicSrc, "nereus.tx.micsrc")

namespace NereusSDR {

TxMicSource::TxMicSource(QObject* parent)
    : QObject(parent)
{
    setObjectName(QStringLiteral("TxMicSource"));
    // Allocate ring once; capacity is fixed for the object's lifetime.
    // 2 doubles per frame (interleaved I/Q).
    // From Thetis cmbuffs.c:50 [v2.10.3.13]:
    //   a->r1_baseptr = (double*) malloc0 (a->r1_active_buffsize * sizeof (complex));
    m_ring.assign(static_cast<size_t>(kRingBlockMultiple) * kBlockFrames * 2, 0.0);
}

TxMicSource::~TxMicSource()
{
    // Defensive: stop the source if the owner forgot.  stop() is
    // idempotent.
    stop();
}

int TxMicSource::ringFrameCapacity() const noexcept
{
    return static_cast<int>(m_ring.size() / 2);
}

void TxMicSource::resetRingLocked()
{
    std::fill(m_ring.begin(), m_ring.end(), 0.0);
    m_writeIdx = 0;
    m_readIdx = 0;
    m_unqueued = 0;
    // Drain any pending semaphore releases — mirrors flush_cmbuffs at
    // cmbuffs.c:78-86 [v2.10.3.13]:
    //   while (!WaitForSingleObject (a->Sem_BuffReady, 1)) ;
    while (m_blockReadySem.tryAcquire(1, 0)) {
        // empty
    }
}

void TxMicSource::start()
{
    // From Thetis cmbuffs.c:35-58 [v2.10.3.13] — create_cmbuffs:
    //   a->accept = accept;
    //   a->run = 1;
    //   ... ring init ... CreateSemaphore ... start_cmthread ...
    QMutexLocker lockIn(&m_csIn);
    QMutexLocker lockOut(&m_csOut);
    resetRingLocked();
    m_running.store(true, std::memory_order_release);
    m_accept.store(true, std::memory_order_release);
    qCDebug(lcTxMicSrc) << "start: accept+run open; ring frame cap ="
                        << ringFrameCapacity()
                        << "; block frames =" << kBlockFrames;
}

void TxMicSource::stop()
{
    if (!m_running.load(std::memory_order_acquire)) {
        return;  // idempotent
    }

    // Mirror destroy_cmbuffs sequence at cmbuffs.c:60-76 [v2.10.3.13]:
    //   InterlockedBitTestAndReset(&a->accept, 0);  // close inbound
    //   EnterCriticalSection (&a->csIN);            // wait for in-flight
    //   EnterCriticalSection (&a->csOUT);           // block consumer
    //   Sleep (25);
    //   InterlockedBitTestAndReset(&a->run, 0);     // trap consumer
    //   ReleaseSemaphore(a->Sem_BuffReady, 1, 0);   // wake consumer
    //   LeaveCriticalSection(...); ...
    m_accept.store(false, std::memory_order_release);
    {
        // Drain in-flight inbound() — once we hold m_csIn, no producer
        // is mid-write.
        QMutexLocker lockIn(&m_csIn);
        Q_UNUSED(lockIn);
    }
    {
        // Block any concurrent consumer drainBlock() — once we hold
        // m_csOut, no consumer is mid-read.
        QMutexLocker lockOut(&m_csOut);
        Q_UNUSED(lockOut);
    }
    m_running.store(false, std::memory_order_release);
    // Wake the consumer one last time so it observes !m_running and
    // exits its loop.  Thetis releases 1; we do the same.
    m_blockReadySem.release(1);
    qCDebug(lcTxMicSrc) << "stop: accept+run closed; poison semaphore released";
}

bool TxMicSource::isRunning() const noexcept
{
    return m_running.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// inbound — port of Thetis Inbound() at cmbuffs.c:88-121 [v2.10.3.13].
//
// One-to-one mapping (NereusSDR <-> Thetis):
//   m_accept gate          <==> _InterlockedAnd(&a->accept, 1)
//   m_csIn lock            <==> EnterCriticalSection(&a->csIN)
//   wrap-aware ring write  <==> two-memcpy split (cmbuffs.c:108-109)
//   semaphore release math <==> a->r1_unqueuedsamps += nsamples; n =
//                                a->r1_unqueuedsamps / a->r1_outsize;
//                                ReleaseSemaphore(a->Sem_BuffReady, n, 0);
//
// NereusSDR-specific details:
//   - input is float (radio mic source produces float); Thetis uses
//     double throughout.  We promote float->double on copy and zero
//     the Q channel (mono mic) — matches Thetis network.c:769
//     `prn->TxReadBufp[2 * i + 1] = 0.0`.
//   - per-frame (not per-sample) addressing: m_writeIdx is in mono
//     frames; m_ring stride is 2 doubles.
// ---------------------------------------------------------------------------
void TxMicSource::inbound(const float* samples, int nsamples)
{
    if (samples == nullptr || nsamples <= 0) {
        return;
    }

    // Match Thetis cmbuffs.c:95 [v2.10.3.13] — drop the inbound when the
    // gate is closed.
    if (!m_accept.load(std::memory_order_acquire)) {
        return;
    }

    QMutexLocker lock(&m_csIn);

    const int cap = ringFrameCapacity();
    if (cap <= 0) {
        return;
    }

    // Clamp incoming length to ring capacity.  Thetis upstream notes
    // (cmbuffs.c:108-109): "add check with *error += -1; for case when
    // r1 is full and an overwrite occurs".  Thetis just lets the wrap
    // happen; we do the same but additionally prevent runaway writes
    // by clamping to one ring at most.
    int n = std::min(nsamples, cap);

    // Wrap-aware split write — mirrors the two-memcpy sequence at
    // cmbuffs.c:108-109 [v2.10.3.13]:
    //   memcpy (a->r1_baseptr + 2 * a->r1_inidx, in,           first  * sizeof (complex));
    //   memcpy (a->r1_baseptr,                  in + 2*first, second * sizeof (complex));
    int first  = std::min(n, cap - m_writeIdx);
    int second = n - first;

    for (int i = 0; i < first; ++i) {
        const int dstIdx = (m_writeIdx + i) * 2;
        m_ring[static_cast<size_t>(dstIdx + 0)] = static_cast<double>(samples[i]);
        m_ring[static_cast<size_t>(dstIdx + 1)] = 0.0;  // Q always zero
    }
    for (int i = 0; i < second; ++i) {
        const int dstIdx = i * 2;
        m_ring[static_cast<size_t>(dstIdx + 0)] = static_cast<double>(samples[first + i]);
        m_ring[static_cast<size_t>(dstIdx + 1)] = 0.0;
    }

    // Release-on-block-boundary — mirrors cmbuffs.c:111-116 [v2.10.3.13]:
    //   if ((a->r1_unqueuedsamps += nsamples) >= a->r1_outsize) {
    //       n = a->r1_unqueuedsamps / a->r1_outsize;
    //       ReleaseSemaphore(a->Sem_BuffReady, n, 0);
    //       a->r1_unqueuedsamps -= n * a->r1_outsize;
    //   }
    m_unqueued += n;
    if (m_unqueued >= kBlockFrames) {
        const int blocks = m_unqueued / kBlockFrames;
        m_unqueued -= blocks * kBlockFrames;
        m_blockReadySem.release(blocks);
    }

    // Advance write head — cmbuffs.c:117-118 [v2.10.3.13]:
    //   if ((a->r1_inidx += nsamples) >= a->r1_active_buffsize)
    //       a->r1_inidx -= a->r1_active_buffsize;
    m_writeIdx += n;
    if (m_writeIdx >= cap) {
        m_writeIdx -= cap;
    }
}

bool TxMicSource::waitForBlock(int timeoutMs)
{
    // Mirrors cm_main's WaitForSingleObject(Sem_BuffReady, INFINITE)
    // at cmbuffs.c:163 [v2.10.3.13].  Returns false on timeout or
    // when the source has been stopped (poison release in stop()).
    if (timeoutMs < 0) {
        m_blockReadySem.acquire(1);
        return m_running.load(std::memory_order_acquire);
    }
    return m_blockReadySem.tryAcquire(1, timeoutMs);
}

// ---------------------------------------------------------------------------
// drainBlock — port of Thetis cmdata() at cmbuffs.c:123-149 [v2.10.3.13].
//
// One block = kBlockFrames complex samples = 2 * kBlockFrames doubles.
// `dst` is interleaved I/Q matching fexchange0's `double* in` layout
// (iobuffs.c:478 [v2.10.3.13]).
//
// Wrap-aware split read — mirrors cmbuffs.c:144-145:
//   memcpy (out,             a->r1_baseptr + 2 * a->r1_outidx, first  * sizeof (complex));
//   memcpy (out + 2 * first, a->r1_baseptr,                    second * sizeof (complex));
// ---------------------------------------------------------------------------
void TxMicSource::drainBlock(double* dst)
{
    if (dst == nullptr) {
        return;
    }

    QMutexLocker lock(&m_csOut);

    const int cap = ringFrameCapacity();
    if (cap <= 0) {
        std::memset(dst, 0, sizeof(double) * 2 * kBlockFrames);
        return;
    }

    int first  = std::min(kBlockFrames, cap - m_readIdx);
    int second = kBlockFrames - first;

    // Copy the first run (m_readIdx .. m_readIdx+first-1).
    std::memcpy(dst,
                m_ring.data() + 2 * m_readIdx,
                static_cast<size_t>(first) * 2 * sizeof(double));
    if (second > 0) {
        std::memcpy(dst + 2 * first,
                    m_ring.data(),
                    static_cast<size_t>(second) * 2 * sizeof(double));
    }

    // Advance read head — cmbuffs.c:146-147 [v2.10.3.13]:
    //   if ((a->r1_outidx += a->r1_outsize) >= a->r1_active_buffsize)
    //       a->r1_outidx -= a->r1_active_buffsize;
    m_readIdx += kBlockFrames;
    if (m_readIdx >= cap) {
        m_readIdx -= cap;
    }
}

// ---------------------------------------------------------------------------
// pullSamples — alternate consumer primitive in mono float for the
// CompositeTxMicRouter::pullSamples path.  Drains the same ring as
// drainBlock but reads only the I-channel (mic input is mono) and
// demotes from double to float.
//
// Semantics (matches the TxMicRouter abstract contract):
//   - Always returns `n` when `dst` is non-null and `n > 0` (zero-fills
//     on underrun).
//   - Returns 0 only when `dst` is null or `n <= 0`.
//   - Thread-safe via m_csOut (same lock drainBlock uses).
//
// In production this method is unused: TxWorkerThread drives the WDSP
// TX path via drainBlock(double*).  pullSamples is wired through
// CompositeTxMicRouter for the future TxChannel::m_micRouter path
// (TxChannel.h:2128-2133) and for unit tests.  Only ONE of
// {pullSamples, drainBlock} is the active consumer at a time.
// ---------------------------------------------------------------------------
int TxMicSource::pullSamples(float* dst, int n)
{
    if (dst == nullptr || n <= 0) {
        return 0;
    }

    QMutexLocker lock(&m_csOut);

    const int cap = ringFrameCapacity();
    if (cap <= 0) {
        std::fill_n(dst, n, 0.0f);
        return n;
    }

    // Compute available frames in the ring under m_csOut.  m_writeIdx is
    // mutated under m_csIn (different lock) so we take a snapshot under
    // a momentary m_csIn acquisition.  The producer may advance further
    // during pullSamples, but the snapshot is conservative — we never
    // read past what's been committed at the moment of inspection.
    int writeSnap = 0;
    {
        QMutexLocker lockIn(&m_csIn);
        writeSnap = m_writeIdx;
    }
    int available = writeSnap - m_readIdx;
    if (available < 0) {
        available += cap;
    }

    const int toCopy = std::min(n, available);

    // Wrap-aware split read of the I channel (even-indexed doubles).
    int first  = std::min(toCopy, cap - m_readIdx);
    int second = toCopy - first;

    for (int i = 0; i < first; ++i) {
        const int srcIdx = (m_readIdx + i) * 2;  // I channel
        dst[i] = static_cast<float>(m_ring[static_cast<size_t>(srcIdx)]);
    }
    for (int i = 0; i < second; ++i) {
        const int srcIdx = i * 2;
        dst[first + i] = static_cast<float>(m_ring[static_cast<size_t>(srcIdx)]);
    }

    // Zero-fill the remainder on underrun.
    for (int i = toCopy; i < n; ++i) {
        dst[i] = 0.0f;
    }

    // Advance read head by ACTUAL frames drained (not n).  Underrun
    // zero-fill must not advance past committed producer data.
    if (toCopy > 0) {
        m_readIdx += toCopy;
        if (m_readIdx >= cap) {
            m_readIdx -= cap;
        }
    }

    return n;
}

} // namespace NereusSDR
