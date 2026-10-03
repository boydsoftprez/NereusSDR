// =================================================================
// src/core/audio/SpeakerFormatConverter.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-23; see SpeakerFormatConverter.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/audio/SpeakerFormatConverter.h"

#include "core/Resampler.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

SpeakerFormatConverter::SpeakerFormatConverter() = default;
SpeakerFormatConverter::~SpeakerFormatConverter() = default;

void SpeakerFormatConverter::configure(int deviceRateHz, int deviceChannels)
{
    m_rateHz = deviceRateHz > 0 ? deviceRateHz : kSourceRateHz;
    m_channels = deviceChannels > 0 ? deviceChannels : 2;
    m_passthrough = m_rateHz == kSourceRateHz && m_channels == 2;
    m_left.reset();
    m_right.reset();
    if (m_passthrough) {
        m_inLeft.clear();
        m_inRight.clear();
        m_outLeft.clear();
        m_outRight.clear();
        m_out.clear();
        m_outCapacityFrames = 0;
        return;
    }
    const bool resample = m_rateHz != kSourceRateHz;
    // Worst case out per piece: the ratio, rounded up, twice over for
    // r8brain's uneven per-call output, plus slack.
    const int ratio = static_cast<int>(
        std::ceil(static_cast<double>(m_rateHz) / static_cast<double>(kSourceRateHz)));
    m_outCapacityFrames = resample ? kMaxBlockFrames * std::max(1, ratio) * 2 + 256
                                   : kMaxBlockFrames;
    m_inLeft.assign(kMaxBlockFrames, 0.0f);
    m_inRight.assign(kMaxBlockFrames, 0.0f);
    m_outLeft.assign(static_cast<std::size_t>(m_outCapacityFrames), 0.0f);
    m_outRight.assign(static_cast<std::size_t>(m_outCapacityFrames), 0.0f);
    // Room for the output of a whole block convert() may be handed, in
    // pieces, at the device's channel count.
    m_out.assign(static_cast<std::size_t>(m_outCapacityFrames) * 2
                     * static_cast<std::size_t>(m_channels),
                 0.0f);
    if (resample) {
        m_left = std::make_unique<Resampler>(kSourceRateHz, m_rateHz, kMaxBlockFrames);
        if (m_channels >= 2) {
            m_right = std::make_unique<Resampler>(kSourceRateHz, m_rateHz, kMaxBlockFrames);
        }
    }
}

int SpeakerFormatConverter::convert(const float* stereo48k, int frames)
{
    if (stereo48k == nullptr || frames <= 0 || m_passthrough) {
        return 0;
    }
    int written = 0;
    for (int done = 0; done < frames; done += kMaxBlockFrames) {
        const int piece = std::min(kMaxBlockFrames, frames - done);
        written = convertPiece(stereo48k + static_cast<std::size_t>(done) * 2, piece, written);
    }
    return written;
}

int SpeakerFormatConverter::convertPiece(const float* stereo48k, int frames, int written)
{
    const bool mono = m_channels == 1;
    for (int i = 0; i < frames; ++i) {
        const float left = stereo48k[static_cast<std::size_t>(i) * 2];
        const float right = stereo48k[static_cast<std::size_t>(i) * 2 + 1];
        if (mono) {
            m_inLeft[static_cast<std::size_t>(i)] = 0.5f * (left + right);
        } else {
            m_inLeft[static_cast<std::size_t>(i)] = left;
            m_inRight[static_cast<std::size_t>(i)] = right;
        }
    }

    const float* outLeft = m_inLeft.data();
    const float* outRight = m_inRight.data();
    int outFrames = frames;
    if (m_left) {
        outFrames = m_left->processInto(m_inLeft.data(), frames, m_outLeft.data(),
                                        m_outCapacityFrames);
        outLeft = m_outLeft.data();
        if (!mono && m_right) {
            const int rightFrames = m_right->processInto(m_inRight.data(), frames,
                                                         m_outRight.data(),
                                                         m_outCapacityFrames);
            outFrames = std::min(outFrames, rightFrames);
            outRight = m_outRight.data();
        }
    }

    const int capacity = static_cast<int>(m_out.size()) - written;
    outFrames = std::max(0, std::min(outFrames, capacity / m_channels));
    float* out = m_out.data() + written;
    for (int i = 0; i < outFrames; ++i) {
        float* frame = out + static_cast<std::size_t>(i) * static_cast<std::size_t>(m_channels);
        frame[0] = outLeft[static_cast<std::size_t>(i)];
        if (!mono) {
            frame[1] = outRight[static_cast<std::size_t>(i)];
            for (int c = 2; c < m_channels; ++c) {
                frame[c] = 0.0f;
            }
        }
    }
    return written + outFrames * m_channels;
}

} // namespace NereusSDR
