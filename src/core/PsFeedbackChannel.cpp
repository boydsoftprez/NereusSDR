// no-port-check: NereusSDR-original wrapper class.  Thetis manages WDSP
// channels via ChannelMaster.dll — there is no Thetis equivalent of this
// Qt6/C++ host-side wrapper.  See third_party/wdsp/src/ps_sync_stub.c.
//
// =================================================================
// src/core/PsFeedbackChannel.cpp  (NereusSDR)
// =================================================================
//
// Implementation of the PureSignal feedback RX channel wrapper.  See
// PsFeedbackChannel.h for the full header narrative on calcc autonomous
// reads via pscc() (calcc.c:617 [v2.10.3.13]) and the SetPSRxIdx routing
// per cmaster.cs:533 [v2.10.3.13].
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Created by J.J. Boyd (KG4VCF) for Phase 3M-4 PureSignal
//                 (Task 4), with AI-assisted source-first protocol via
//                 Anthropic Claude Code.  NereusSDR-original wrapper.
//   2026-09-24 - R-R3-39: setSampleRate on the receive lane. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "PsFeedbackChannel.h"

#include "DspControlThread.h"
#include "wdsp_api.h"   // ::SetInputSamplerate (HAVE_WDSP-gated)

namespace NereusSDR {

namespace {
// R-R3-39: the receive-lane key of this channel's input rate ("PSFBRATE").
constexpr quint64 kSampleRateLaneKey = 0x5053464252415445ull;
} // namespace

PsFeedbackChannel::PsFeedbackChannel(int channelId, QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
{
    // No WDSP-side allocation here — WdspEngine opens the channel via
    // OpenChannel(channelId, ..., type=0) BEFORE constructing this wrapper.
    // Mirrors the existing RxChannel / TxChannel ownership pattern.
}

PsFeedbackChannel::~PsFeedbackChannel() = default;

void PsFeedbackChannel::setSampleRate(int rate)
{
    m_sampleRate.store(rate);
#ifdef HAVE_WDSP
    // WDSP-side rate change.  Per wdsp_api.h:255 — declared in channel.h /
    // implemented in channel.c.  Idempotent on the WDSP side; no need to
    // guard against rate==current.
    DspControlThread* lane = m_lane.load();
    if (lane != nullptr && !lane->isCurrentThread()) {
        // R-R3-39: keyed on the channel, so a burst leaves the newest rate.
        const int channelId = m_channelId;
        lane->postKeyed(kSampleRateLaneKey ^ static_cast<quint64>(channelId),
                        [channelId, rate]() { ::SetInputSamplerate(channelId, rate); });
        return;
    }
    ::SetInputSamplerate(m_channelId, rate);
#endif
}

void PsFeedbackChannel::feedSamples(const float* iqInterleaved, int size)
{
    // Activity counter for the future PsaIndicatorWidget "Fbk LED".  This
    // alone is sufficient to verify call-site activity in Task 4 unit
    // tests.
    m_totalSamples.fetch_add(size);

    // TBD (Phase 3M-4 Task 7 — PureSignal coordinator integration):
    // Wire the actual WDSP feed call.  The choice between fexchange2
    // (separate float I/Q, used by RxChannel::processIq) and fexchange0
    // (interleaved double I/Q, used by TxChannel post-3M-1c) depends on
    // what calcc's pscc() autonomous reader expects.  See
    // RxChannel::processIq at src/core/RxChannel.cpp:1318 [v2.10.3.13]
    // for the precedent feed pattern; mirror it once Task 7 wires the
    // coordinator + decides whether the PS feedback path is host-pumped
    // or fully autonomous.
    //
    // For Task 4, the counter increment alone is sufficient — the test
    // exercises wrapper plumbing, not WDSP DSP behavior.  Production
    // code must NOT call feedSamples() yet (no callers exist before
    // Task 7 lands).
    (void)iqInterleaved;
}

} // namespace NereusSDR
