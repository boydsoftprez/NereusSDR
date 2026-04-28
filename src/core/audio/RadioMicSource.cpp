// =================================================================
// src/core/audio/RadioMicSource.cpp  (NereusSDR)
// =================================================================
// See RadioMicSource.h for contract. NereusSDR-original.
// Plan: 3M-1b F.2. Master design §5.2.1.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "core/audio/RadioMicSource.h"
#include "core/RadioConnection.h"

#include <algorithm>  // std::min

namespace NereusSDR {

RadioMicSource::RadioMicSource(RadioConnection* connection, QObject* parent)
    : QObject(parent)
    , m_connection(connection)
{
    if (m_connection != nullptr) {
        // DirectConnection: onMicFrame runs synchronously on the connection
        // thread (the signal-emitting thread). This is the required contract
        // documented in RadioConnection.h — the const float* pointer is only
        // valid during the synchronous slot dispatch, not after.
        connect(m_connection, &RadioConnection::micFrameDecoded,
                this, &RadioMicSource::onMicFrame,
                Qt::DirectConnection);
    }
}

// ---------------------------------------------------------------------------
// Producer side: connection thread → onMicFrame → ring write
// ---------------------------------------------------------------------------

void RadioMicSource::onMicFrame(const float* samples, int frames)
{
    if (samples == nullptr || frames <= 0) {
        return;
    }

    // m_writeIdx is producer-owned: load relaxed (we are the only writer).
    // m_readIdx is consumer-owned:  load acquire (synchronize with the
    // release store in pullSamples so we see current read progress).
    const unsigned int w = m_writeIdx.load(std::memory_order_relaxed);
    const unsigned int r = m_readIdx.load(std::memory_order_acquire);

    // How many slots are free in the ring?
    // The difference (w - r) gives used slots; unsigned arithmetic handles
    // the wraparound case correctly (defined behaviour for unsigned).
    const unsigned int used = w - r;
    const unsigned int free = kRingCapacity - used;

    const int toWrite = std::min(static_cast<unsigned int>(frames), free);
    const int dropped = frames - toWrite;

    for (int i = 0; i < toWrite; ++i) {
        m_ring[(w + static_cast<unsigned int>(i)) & kMask] = samples[i];
    }

    // Release store: makes the written samples visible to the audio thread
    // before it sees the updated write index.
    m_writeIdx.store(w + static_cast<unsigned int>(toWrite),
                     std::memory_order_release);

    if (dropped > 0) {
        m_dropped.fetch_add(static_cast<unsigned int>(dropped),
                            std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// Consumer side: audio thread → pullSamples → ring read
// ---------------------------------------------------------------------------

int RadioMicSource::pullSamples(float* dst, int n)
{
    if (dst == nullptr || n <= 0) {
        return 0;
    }

    // m_writeIdx is producer-owned: load acquire (synchronize with the
    // release store in onMicFrame so we see current write progress).
    // m_readIdx is consumer-owned: load relaxed (we are the only reader).
    const unsigned int w = m_writeIdx.load(std::memory_order_acquire);
    const unsigned int r = m_readIdx.load(std::memory_order_relaxed);

    const unsigned int available = w - r;
    const int toRead = std::min(static_cast<unsigned int>(n), available);

    for (int i = 0; i < toRead; ++i) {
        dst[i] = m_ring[(r + static_cast<unsigned int>(i)) & kMask];
    }

    // Underrun: zero-fill the remainder so the audio pipeline always
    // receives a full buffer (silence is user-friendly; glitches are not).
    for (int i = toRead; i < n; ++i) {
        dst[i] = 0.0f;
    }

    // Release store: makes the read progress visible to the producer thread
    // before it sees the updated read index.
    m_readIdx.store(r + static_cast<unsigned int>(toRead),
                    std::memory_order_release);

    // Always return n: the audio pipeline expects a full buffer regardless
    // of how many samples were actually available in the ring.
    return n;
}

// ---------------------------------------------------------------------------
// Test seams (NEREUS_BUILD_TESTS only)
// ---------------------------------------------------------------------------

#ifdef NEREUS_BUILD_TESTS
int RadioMicSource::ringFillForTest() const
{
    const unsigned int w = m_writeIdx.load(std::memory_order_acquire);
    const unsigned int r = m_readIdx.load(std::memory_order_acquire);
    return static_cast<int>(w - r);
}

int RadioMicSource::ringDroppedForTest() const
{
    return static_cast<int>(m_dropped.load(std::memory_order_relaxed));
}
#endif

} // namespace NereusSDR
