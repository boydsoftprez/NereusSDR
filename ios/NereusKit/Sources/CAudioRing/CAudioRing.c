// NereusSDR for iOS: the lock-free audio ring and shared number of CAudioRing.h, on C11 atomics
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include "CAudioRing.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct nereus_audio_ring {
    size_t capacity;
    size_t channels;
    float *samples;
    // Frames ever written and ever read. Each is stored by one thread only;
    // their difference is the fill, and unsigned wrap keeps it right.
    _Atomic size_t written;
    _Atomic size_t read;
};

struct nereus_shared_value {
    _Atomic int64_t number;
};

enum { NEREUS_RENDER_OBSERVATION_CAPACITY = 64 };
struct nereus_render_observation_queue {
    nereus_render_observation entries[NEREUS_RENDER_OBSERVATION_CAPACITY];
    _Atomic uint64_t written;
    _Atomic uint64_t read;
    _Atomic uint64_t dropped;
};

nereus_audio_ring *nereus_audio_ring_create(size_t capacity_frames, size_t channels)
{
    if (capacity_frames == 0 || channels == 0 || capacity_frames > SIZE_MAX / channels / sizeof(float)) {
        return NULL;
    }
    nereus_audio_ring *ring = calloc(1, sizeof *ring);
    if (ring == NULL) {
        return NULL;
    }
    ring->samples = calloc(capacity_frames * channels, sizeof(float));
    if (ring->samples == NULL) {
        free(ring);
        return NULL;
    }
    ring->capacity = capacity_frames;
    ring->channels = channels;
    atomic_init(&ring->written, 0);
    atomic_init(&ring->read, 0);
    return ring;
}

void nereus_audio_ring_destroy(nereus_audio_ring *ring)
{
    if (ring == NULL) {
        return;
    }
    free(ring->samples);
    free(ring);
}

size_t nereus_audio_ring_capacity(const nereus_audio_ring *ring)
{
    return ring->capacity;
}

size_t nereus_audio_ring_readable(const nereus_audio_ring *ring)
{
    size_t written = atomic_load_explicit(&((nereus_audio_ring *)ring)->written, memory_order_acquire);
    size_t read = atomic_load_explicit(&((nereus_audio_ring *)ring)->read, memory_order_acquire);
    return written - read;
}

size_t nereus_audio_ring_writable(const nereus_audio_ring *ring)
{
    return ring->capacity - nereus_audio_ring_readable(ring);
}

// Copies frames between the ring and a flat buffer, in at most two runs.
static void copy_frames(const nereus_audio_ring *ring, size_t position, float *flat, size_t frames,
                        int into_ring)
{
    size_t start = position % ring->capacity;
    size_t first = frames < ring->capacity - start ? frames : ring->capacity - start;
    size_t width = ring->channels * sizeof(float);
    float *at = ring->samples + start * ring->channels;
    if (into_ring) {
        memcpy(at, flat, first * width);
        memcpy(ring->samples, flat + first * ring->channels, (frames - first) * width);
    } else {
        memcpy(flat, at, first * width);
        memcpy(flat + first * ring->channels, ring->samples, (frames - first) * width);
    }
}

size_t nereus_audio_ring_write(nereus_audio_ring *ring, const float *interleaved, size_t frames)
{
    size_t written = atomic_load_explicit(&ring->written, memory_order_relaxed);
    size_t read = atomic_load_explicit(&ring->read, memory_order_acquire);
    size_t room = ring->capacity - (written - read);
    size_t count = frames < room ? frames : room;
    if (count > 0) {
        copy_frames(ring, written, (float *)interleaved, count, 1);
        atomic_store_explicit(&ring->written, written + count, memory_order_release);
    }
    return count;
}

size_t nereus_audio_ring_read(nereus_audio_ring *ring, float *interleaved, size_t frames)
{
    size_t read = atomic_load_explicit(&ring->read, memory_order_relaxed);
    size_t written = atomic_load_explicit(&ring->written, memory_order_acquire);
    size_t available = written - read;
    size_t count = frames < available ? frames : available;
    if (count > 0) {
        copy_frames(ring, read, interleaved, count, 0);
        atomic_store_explicit(&ring->read, read + count, memory_order_release);
    }
    return count;
}

uint64_t nereus_audio_ring_written_frames(const nereus_audio_ring *ring)
{
    return atomic_load_explicit(&((nereus_audio_ring *)ring)->written, memory_order_acquire);
}

uint64_t nereus_audio_ring_read_frames(const nereus_audio_ring *ring)
{
    return atomic_load_explicit(&((nereus_audio_ring *)ring)->read, memory_order_acquire);
}

nereus_render_observation_queue *nereus_render_observation_queue_create(void)
{
    return calloc(1, sizeof(nereus_render_observation_queue));
}

void nereus_render_observation_queue_destroy(nereus_render_observation_queue *queue)
{
    free(queue);
}

int nereus_render_observation_queue_push(nereus_render_observation_queue *queue,
                                         const nereus_render_observation *observation)
{
    const uint64_t written = atomic_load_explicit(&queue->written, memory_order_relaxed);
    const uint64_t read = atomic_load_explicit(&queue->read, memory_order_acquire);
    if (written - read >= NEREUS_RENDER_OBSERVATION_CAPACITY) {
        atomic_fetch_add_explicit(&queue->dropped, 1, memory_order_relaxed);
        return 0;
    }
    queue->entries[written % NEREUS_RENDER_OBSERVATION_CAPACITY] = *observation;
    atomic_store_explicit(&queue->written, written + 1, memory_order_release);
    return 1;
}

int nereus_render_observation_queue_pop(nereus_render_observation_queue *queue,
                                        nereus_render_observation *observation)
{
    const uint64_t read = atomic_load_explicit(&queue->read, memory_order_relaxed);
    const uint64_t written = atomic_load_explicit(&queue->written, memory_order_acquire);
    if (read == written) { return 0; }
    *observation = queue->entries[read % NEREUS_RENDER_OBSERVATION_CAPACITY];
    atomic_store_explicit(&queue->read, read + 1, memory_order_release);
    return 1;
}

uint64_t nereus_render_observation_queue_dropped(const nereus_render_observation_queue *queue)
{
    return atomic_load_explicit(&((nereus_render_observation_queue *)queue)->dropped, memory_order_acquire);
}

nereus_shared_value *nereus_shared_value_create(int64_t initial)
{
    nereus_shared_value *value = calloc(1, sizeof *value);
    if (value != NULL) {
        atomic_init(&value->number, initial);
    }
    return value;
}

void nereus_shared_value_destroy(nereus_shared_value *value)
{
    free(value);
}

void nereus_shared_value_store(nereus_shared_value *value, int64_t number)
{
    atomic_store_explicit(&value->number, number, memory_order_release);
}

int64_t nereus_shared_value_load(const nereus_shared_value *value)
{
    return atomic_load_explicit(&((nereus_shared_value *)value)->number, memory_order_acquire);
}

int64_t nereus_shared_value_add(nereus_shared_value *value, int64_t amount)
{
    return atomic_fetch_add_explicit(&value->number, amount, memory_order_acq_rel) + amount;
}

int nereus_shared_value_claim(nereus_shared_value *value)
{
    int64_t expected = 0;
    return atomic_compare_exchange_strong_explicit(&value->number, &expected, 1,
                                                    memory_order_acq_rel, memory_order_acquire);
}

struct nereus_render_fence { _Atomic uint64_t state; };

nereus_render_fence *nereus_render_fence_create(void)
{
    return calloc(1, sizeof(nereus_render_fence));
}

void nereus_render_fence_destroy(nereus_render_fence *fence)
{
    free(fence);
}

uint64_t nereus_render_fence_begin(nereus_render_fence *fence)
{
    // Low bit is in-flight; upper bits count retirements. Only one render
    // producer enters at a time. Acquire observes preceding epoch stores.
    return atomic_fetch_or_explicit(&fence->state, 1, memory_order_acq_rel) >> 1;
}

int nereus_render_fence_retire(nereus_render_fence *fence)
{
    // The RMW orders retirement with callback entry and completion. Epoch
    // stores precede this publication; later begins acquire them.
    return (atomic_fetch_add_explicit(&fence->state, 2, memory_order_acq_rel) & 1) != 0;
}

int nereus_render_fence_end(nereus_render_fence *fence, uint64_t ticket)
{
    const uint64_t prior = atomic_fetch_and_explicit(&fence->state, ~UINT64_C(1), memory_order_acq_rel);
    return (prior >> 1) == ticket;
}
