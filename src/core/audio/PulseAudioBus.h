// =================================================================
// src/core/audio/PulseAudioBus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The PulseAudio engine's streams
// (R-AUD-07, R-AUD-15, R-AUD-32); no upstream logic.
//
// An output is a playback pa_stream on the system's threaded mainloop.
// It asks for tlength and minreq of the request's buffer frames
// (kPulseDefaultBufferFrames when it names none) with
// PA_STREAM_ADJUST_LATENCY.  Its write callback runs on the mainloop's
// thread under libpulse's own mainloop lock: it reads the stream's
// DeviceRateMatcher (MatcherReader::read) into the buffer
// pa_stream_begin_write hands it, the pair's channels carrying the stereo
// mix and every other channel zero; our code there takes no lock and
// allocates nothing.  The device latency is pa_stream_get_latency's.  An
// input is a record pa_stream whose read callback hands the request's
// pair, as MicChannelPick picks it, to an IAudioInputSink.
//
// A device whose channel map lists more than two channels opens with all
// of them, in its own map, with PA_STREAM_NO_REMIX_CHANNELS, so a pair
// lands on its own channels (R-AUD-07, where the interface's profile
// exposes its channels).  A named device opens with PA_STREAM_DONT_MOVE,
// so its stream fails (and posts DeviceLost) when the device goes away
// rather than moving elsewhere; an empty name follows the server default.
//
// The config and the fill hold no libpulse types and are tested without a
// server.  While audioDevicesBarredForTestRun() is true every open fails.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-07, R-AUD-15, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/PulseAudioSystem.h"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

struct pa_threaded_mainloop;
struct pa_context;

namespace NereusSDR {

class MatcherReader;

struct PulseStreamConfig {
    AudioDeviceDirection direction = AudioDeviceDirection::Output;
    QString deviceName;            // empty: the server default
    int rate = 48000;
    int channels = 2;
    QStringList channelMap;        // the device's own map when it has more than two channels; empty: the default map
    bool noRemix = false;          // PA_STREAM_NO_REMIX_CHANNELS
    bool dontMove = false;         // PA_STREAM_DONT_MOVE
    int bufferFrames = kPulseDefaultBufferFrames;
    std::uint32_t tlengthBytes = 0;    // an output's tlength
    std::uint32_t minreqBytes = 0;     // an output's minreq
    std::uint32_t fragsizeBytes = 0;   // an input's fragsize
    AudioChannelPair pair;         // the device pair
};

// The stream a device opens: rate and buffer from the request, the
// device's own map with no remix when it lists more than two channels,
// mono for a one-channel device, stereo otherwise.  A pair past the
// stream's channels plays on the first pair.
PulseStreamConfig pulseStreamConfig(const PulseDeviceRecord& device, const AudioStreamRequest& request);

// Frames an output reads from its matcher, or an input hands its sink, in
// one piece (the scratch each stream allocates before it opens).
inline constexpr int kPulseChunkFrames = 512;

// An output's fill: `frames` frames of `channels` interleaved float into
// dst, read from the matcher `scratchFrames` at a time through scratch
// (2 * scratchFrames floats), the pair's channels carrying the stereo and
// every other channel zero.  Silence when the reader is not valid.  No
// lock, no allocation, no system call.
void pulseFillFromMatcher(MatcherReader& reader, float* scratch, int scratchFrames,
                          float* dst, int frames, int channels, AudioChannelPair pair);

// A stream's way to post a stream event (DeviceLost) from the mainloop's
// thread.  libpulse fails every stream of a context that fails, so a
// server that goes away reaches each open stream through its own state.
struct PulseLossWatch {
    std::mutex mutex;
    std::function<void(const AudioStreamEvent&)> sink;

    void post(AudioStreamEvent::Kind kind, const QString& detail);
};

// What a stream needs of its connection.  The stream holds it, so the
// mainloop outlives every stream on it.
class PulseStreamHost {
public:
    virtual ~PulseStreamHost() = default;
    virtual pa_threaded_mainloop* mainloop() const = 0;
    virtual pa_context* context() const = 0;
    virtual bool running() const = 0;
};

// Waits on the mainloop (its lock held by the caller) until `done` is true
// or `timeoutMs` passes; true when `done`.  Every callback that changes
// what `done` reads signals the mainloop.
bool pulseWait(pa_threaded_mainloop* mainloop, pa_context* context, int timeoutMs,
               const std::function<bool()>& done);

std::unique_ptr<IAudioBus> makePulseOutputBus(std::shared_ptr<PulseStreamHost> host,
                                              PulseStreamConfig config, int delayMs);
std::unique_ptr<IAudioInputStream> makePulseInputStream(std::shared_ptr<PulseStreamHost> host,
                                                        PulseStreamConfig config,
                                                        MicChannelPick pick,
                                                        IAudioInputSink* sink);

} // namespace NereusSDR
