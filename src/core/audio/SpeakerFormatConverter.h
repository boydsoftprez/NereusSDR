// =================================================================
// src/core/audio/SpeakerFormatConverter.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-23 (R3 receiver audio fix wave):
// local speakers at any format. The resampling is the vendored r8brain
// wrapper (src/core/Resampler.h) the capture path already uses; no
// upstream logic is translated here.
//
// The local master mix is 48 kHz interleaved stereo. A speaker device can
// be opened at another rate or as mono (the Devices page offers 44.1 to
// 384 kHz, one or two channels), and the mix used to be pushed to it
// unchanged: a 96 kHz device played it at twice the speed, and a mono
// device read interleaved stereo as mono. This converts each block to the
// device's own format before the push:
//   - mono: (left + right) / 2, as a remote window's mono speaker hears it
//     (RemoteAudioReceiver);
//   - another rate: one r8brain resampler per channel, built when the
//     device opens (configure()), so converting a block allocates nothing.
// 48 kHz stereo passes through untouched.
//
// configure() runs on the control thread while the DSP thread cannot push
// (AudioEngine holds the speakers mutex around both); convert() runs on
// the DSP thread.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <memory>
#include <vector>

namespace NereusSDR {

class Resampler;

class SpeakerFormatConverter {
public:
    static constexpr int kSourceRateHz = 48000;
    // The largest block convert() takes at once; larger ones are converted
    // in pieces of this size.
    static constexpr int kMaxBlockFrames = 4096;

    SpeakerFormatConverter();
    ~SpeakerFormatConverter();

    SpeakerFormatConverter(const SpeakerFormatConverter&) = delete;
    SpeakerFormatConverter& operator=(const SpeakerFormatConverter&) = delete;

    /// Control thread: the speaker device's format. A rate or channel count
    /// of 0 or less means 48 kHz stereo (pass through).
    void configure(int deviceRateHz, int deviceChannels);

    /// True when blocks go out unchanged (a 48 kHz stereo device).
    bool passthrough() const { return m_passthrough; }
    int deviceRateHz() const { return m_rateHz; }
    int deviceChannels() const { return m_channels; }

    /// DSP thread: converts `frames` frames of 48 kHz interleaved stereo.
    /// Returns the interleaved device-format samples (not frames) written
    /// to the converter's own buffer, available at output() until the next
    /// call. Allocates nothing.
    int convert(const float* stereo48k, int frames);
    const float* output() const { return m_out.data(); }

private:
    int convertPiece(const float* stereo48k, int frames, int written);

    bool m_passthrough = true;
    int m_rateHz = kSourceRateHz;
    int m_channels = 2;
    std::unique_ptr<Resampler> m_left;   // or the mono downmix
    std::unique_ptr<Resampler> m_right;
    std::vector<float> m_inLeft;
    std::vector<float> m_inRight;
    std::vector<float> m_outLeft;
    std::vector<float> m_outRight;
    std::vector<float> m_out;
    int m_outCapacityFrames = 0;
};

} // namespace NereusSDR
