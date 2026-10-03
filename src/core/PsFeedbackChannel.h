#pragma once

// no-port-check: NereusSDR-original wrapper class.  Thetis manages WDSP
// channels via ChannelMaster.dll (Project Files/Source/ChannelMaster/), so
// there is no Thetis equivalent of this Qt6/C++ host-side wrapper.  See
// third_party/wdsp/src/ps_sync_stub.c for the routing-symbol stub that
// satisfies the SetPSRxIdx / SetPSTxIdx ABI surface.
//
// =================================================================
// src/core/PsFeedbackChannel.h  (NereusSDR)
// =================================================================
//
// PureSignal feedback RX channel wrapper.  The WDSP calcc autonomous state
// machine (pscc() at calcc.c:617 [v2.10.3.13]) reads samples from this
// channel during MOX+PS-on without further host orchestration.  The host's
// role is:
//
//   1. Configure sample rate per board (192000 for G2-class boards from
//      cmaster.cs:424 [v2.10.3.13] ps_rate=192000; rx1_rate for HL2 mi0bot,
//      indicated by BoardCapabilities::psSampleRate=0 sentinel per
//      mi0bot console.cs:8472-8488 [v2.10.3.13-beta2]).
//   2. Pump feedback samples in via feedSamples() (sample-pump wiring
//      deferred to Phase 3M-4 Task 7 PureSignal coordinator integration —
//      Task 4 ships counter-only stub).
//   3. Route the channel via SetPSRxIdx(0, channelId()) at PureSignal init
//      per cmaster.cs:533 [v2.10.3.13] comment "all current models use
//      Stream0 for RX feedback" (comment unchanged in mi0bot
//      cmaster.cs:533 [v2.10.3.13-beta2]).
//
// NOTE on the "channel id" concept:
//   In Thetis cmaster.cs:533, `SetPSRxIdx(0, 0)` passes Stream-0 as the
//   logical PS-stream id, NOT the WDSP channel id.  In NereusSDR, the
//   PureSignal coordinator (Task 7) calls TxChannel::setPSRxIdx(0, idx)
//   where idx is THIS WRAPPER's NereusSDR-assigned WDSP channel id.  The
//   mapping is what PsFeedbackChannel::channelId() exposes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Created by J.J. Boyd (KG4VCF) for Phase 3M-4 PureSignal
//                 (Task 4), with AI-assisted source-first protocol via
//                 Anthropic Claude Code.  NereusSDR-original wrapper class.
//   2026-09-24 - R-R3-39: setSampleRate's WDSP call runs on the receive
//                 lane when one is set. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <atomic>

namespace NereusSDR {

class DspControlThread;

// Wraps the WDSP feedback RX channel that calcc reads autonomously.
// One per WdspEngine.  Created when the engine initializes, destroyed at
// engine shutdown.  The class doesn't implement DSP — it's a thin holder.
//
// Thread safety: setSampleRate and feedSamples may be called from the
//                audio / connection thread; sampleRate(), totalSamplesIn(),
//                and channelId() may be called from any thread.  All
//                cross-thread state is std::atomic.
class PsFeedbackChannel : public QObject {
    Q_OBJECT

public:
    // The channelId is assigned by WdspEngine and matches the WDSP-side
    // OpenChannel slot.  Construction does NOT open the WDSP channel; the
    // caller (WdspEngine) must call OpenChannel before this constructor
    // and CloseChannel after the destructor.  (Mirrors the existing
    // RxChannel / TxChannel ownership pattern.)
    explicit PsFeedbackChannel(int channelId, QObject* parent = nullptr);
    ~PsFeedbackChannel() override;

    // Channel id (immutable for the lifetime of the wrapper).
    int channelId() const noexcept { return m_channelId; }

    // Current configured sample rate.  Defaults to 192000 (G2-class
    // ps_rate per cmaster.cs:424 [v2.10.3.13]); HL2 sets this to rx1_rate
    // via the PureSignal coordinator (Task 7).
    int sampleRate() const noexcept { return m_sampleRate.load(); }

    // Configure the WDSP-side input sample rate via SetInputSamplerate().
    // The PureSignal coordinator drives this on board-connect (per
    // BoardCapabilities::psSampleRate) and on rx1_rate change for HL2.
    //
    // R-R3-39: with a receive lane set the rate is carried at once and the
    // WDSP call runs on the lane (keyed, so the newest rate wins).
    void setSampleRate(int rate);
    void setControlLane(DspControlThread* lane) { m_lane.store(lane); }

    // Push interleaved I/Q samples into the channel.  size = number of
    // complex pairs (so iqInterleaved must point to 2 * size floats).
    //
    // Counter-only stub for Phase 3M-4 Task 4; actual fexchange0 sample-
    // pump wiring is deferred to Task 7 PureSignal coordinator
    // integration.  See PsFeedbackChannel.cpp for the TBD comment block.
    void feedSamples(const float* iqInterleaved, int size);

    // Activity counter for the future PsaIndicatorWidget "Fbk LED" — total
    // sample pairs pushed via feedSamples().  Monotonically increasing.
    qint64 totalSamplesIn() const noexcept { return m_totalSamples.load(); }

private:
    const int m_channelId;
    std::atomic<int>    m_sampleRate{192000};
    std::atomic<qint64> m_totalSamples{0};
    std::atomic<DspControlThread*> m_lane{nullptr};
};

} // namespace NereusSDR
