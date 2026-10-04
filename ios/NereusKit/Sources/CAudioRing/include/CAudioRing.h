// NereusSDR for iOS: a single-producer, single-consumer ring of interleaved audio frames and a shared number, both lock-free, for the audio render thread
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#ifndef NEREUS_CAUDIORING_H
#define NEREUS_CAUDIORING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// A ring of interleaved float frames between one writer thread and one
/// reader thread. Reading and writing never lock and never allocate; only
/// create and destroy allocate. The writer calls only the write side, the
/// reader only the read side; either may ask how much is readable.
typedef struct nereus_audio_ring nereus_audio_ring;

/// A ring holding up to capacity_frames frames of channels floats each, or
/// NULL when either is zero or the memory cannot be had.
nereus_audio_ring *nereus_audio_ring_create(size_t capacity_frames, size_t channels);
void nereus_audio_ring_destroy(nereus_audio_ring *ring);

size_t nereus_audio_ring_capacity(const nereus_audio_ring *ring);
/// Frames written and not yet read.
size_t nereus_audio_ring_readable(const nereus_audio_ring *ring);
/// Frames that can be written now (writer side).
size_t nereus_audio_ring_writable(const nereus_audio_ring *ring);
/// Writes up to frames frames and returns how many were written (writer side).
size_t nereus_audio_ring_write(nereus_audio_ring *ring, const float *interleaved, size_t frames);
/// Reads up to frames frames and returns how many were read (reader side).
size_t nereus_audio_ring_read(nereus_audio_ring *ring, float *interleaved, size_t frames);
/// Monotonic frame positions, including frames already read into the matcher.
uint64_t nereus_audio_ring_written_frames(const nereus_audio_ring *ring);
uint64_t nereus_audio_ring_read_frames(const nereus_audio_ring *ring);

/// One render-owned, coherent observation. The callback writes a bounded SPSC
/// queue; the feed thread reads it. All fields describe the same callback end.
typedef struct nereus_render_observation {
    uint64_t stream_epoch;
    uint64_t output_epoch;
    uint64_t measured_ns;
    uint64_t read_window_ns;
    uint64_t ring_read_frames;
    uint64_t source_frames_requested;
    uint64_t source_frames_rendered;
    uint64_t stream_render_underruns;
    int64_t ring_queued_frames;
    double matcher_prefetched_frames;
    double matcher_ratio;
    uint32_t callback_frames;
    uint32_t chunk_start_frame;
    uint32_t chunk_end_frame;
    uint32_t source_rate_hz;
    uint32_t timestamp_flags;
    uint64_t source_host_ticks;
    double source_sample_time;
    uint8_t ratio_measured;
    uint8_t starved;
    uint8_t has_timestamp;
    uint8_t has_source_host_time;
    uint8_t has_source_sample_time;
} nereus_render_observation;

typedef struct nereus_render_observation_queue nereus_render_observation_queue;
nereus_render_observation_queue *nereus_render_observation_queue_create(void);
void nereus_render_observation_queue_destroy(nereus_render_observation_queue *queue);
/// False on a full queue; the producer never waits or allocates.
int nereus_render_observation_queue_push(nereus_render_observation_queue *queue,
                                         const nereus_render_observation *observation);
int nereus_render_observation_queue_pop(nereus_render_observation_queue *queue,
                                        nereus_render_observation *observation);
uint64_t nereus_render_observation_queue_dropped(const nereus_render_observation_queue *queue);

/// One 64-bit number one thread publishes and another reads, lock-free.
typedef struct nereus_shared_value nereus_shared_value;

nereus_shared_value *nereus_shared_value_create(int64_t initial);
void nereus_shared_value_destroy(nereus_shared_value *value);
void nereus_shared_value_store(nereus_shared_value *value, int64_t number);
int64_t nereus_shared_value_load(const nereus_shared_value *value);
int64_t nereus_shared_value_add(nereus_shared_value *value, int64_t amount);
int nereus_shared_value_claim(nereus_shared_value *value);

/// One render producer and any retiring control thread. Begin and retire
/// share one atomic modification order; end follows record publication.
typedef struct nereus_render_fence nereus_render_fence;
nereus_render_fence *nereus_render_fence_create(void);
void nereus_render_fence_destroy(nereus_render_fence *fence);
uint64_t nereus_render_fence_begin(nereus_render_fence *fence);
/// Returns whether a callback was held across this retirement.
int nereus_render_fence_retire(nereus_render_fence *fence);
/// Returns whether no retirement crossed this callback.
int nereus_render_fence_end(nereus_render_fence *fence, uint64_t ticket);

#ifdef __cplusplus
}
#endif

#endif
