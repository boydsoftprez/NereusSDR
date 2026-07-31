// =================================================================
// src/core/SliceStreamAllocator.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original policy class. The window-fit rule
// ports Thetis console.cs:31913-31925 [v2.10.3.15]; the stream topology
// mirrors ChannelMaster cmaster.h:75-82 (one _rcvr drives cmMAXSubRcvr
// channels off one I/Q input). Thetis has no equivalent allocator
// because it hard-codes RX1 -> DDC2 and RX2 -> DDC3; NereusSDR
// allocates across every user DDC the SKU has.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-24  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic I Task 2.
//                                    Slice-to-DDC-stream placement
//                                    policy. AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QString>
#include <QVector>

namespace NereusSDR {

/// Decides which DDC stream should host a slice at a given frequency.
///
/// A stream is one hardware DDC plus its receiver, FFT engine, panadapter
/// window, and noise blanker. Slices bind to a stream many-to-one: any
/// number whose frequencies fall inside the stream's window share its I/Q
/// (ChannelMaster cmaster.h:75-82 [v2.10.3.15]: one `_rcvr`, one noise
/// blanker, one panadapter, but `audio[cmMAXSubRcvr]`).
///
/// Pure policy: no Qt signals, no hardware, no WDSP. Everything it needs
/// is passed in, so the whole decision surface is unit-testable.
class SliceStreamAllocator {
public:
    enum class Outcome {
        JoinedExisting,   ///< Fits an active stream's window; set shift only.
        NewStream,        ///< Claimed a free DDC, centred on the slice.
        RetunedStream,    ///< Sole occupant; moved its stream's centre instead.
        Rejected          ///< No stream fits and none free. `reason` explains.
    };

    struct Placement {
        Outcome outcome{Outcome::Rejected};
        int     streamIndex{-1};
        double  shiftOffsetHz{0.0};     ///< slice freq minus stream centre
        double  newStreamCentreHz{0.0}; ///< set for NewStream / RetunedStream
        QString reason;                 ///< human-readable, for Rejected
    };

    /// Size the allocator to the connected SKU. Clears all stream state.
    void configure(int userDdcCount, int maxSlices);

    /// Mark a stream active at a centre frequency and sample rate.
    void activateStream(int streamIndex, double centreHz, int sampleRateHz);

    /// Mark a stream idle (its last slice went away).
    void deactivateStream(int streamIndex);

    /// Where should a brand-new slice at `frequencyHz` go?
    ///
    /// `preferDedicated` asks for a DDC of this slice's own even when an
    /// active window already covers the frequency. A new PAN wants that: it
    /// is a new receiver, and sharing a DDC would make it a second view of
    /// an existing one, tuning and panning in lockstep because one DDC has
    /// one centre. A slice added to a pan that already has slices does NOT
    /// want it; sharing that pan's receiver is the point and costs no
    /// hardware.
    ///
    /// A preference, not a demand. With every DDC spoken for it falls
    /// through to the sharing path, because a coupled pan beats no pan.
    Placement placeSlice(double frequencyHz,
                         bool preferDedicated = false) const;

    /// Where should an existing slice go after retuning to `frequencyHz`?
    ///
    /// Two separate facts decide whether the stream's centre may move, and
    /// they are NOT interchangeable -- they were a single `mayRetuneStream`
    /// flag until the 2026-07-26 G2E bench, and conflating them cost a DDC.
    ///
    /// `soleOccupant`: no other slice depends on this window. A lone slice
    /// belongs on its DDC centre, so the DDC follows it
    /// (Outcome::RetunedStream). A shared window cannot move, so a slice
    /// leaving it must migrate to another DDC.
    ///
    /// `ddcPinned`: CTUN is holding the window still deliberately. This
    /// applies only while the slice stays INSIDE the window -- that is the
    /// whole point of CTUN, the panadapter must not slide under the
    /// operator as the VFO moves. Once the slice leaves the window the pan
    /// has to jump regardless, so the pin is moot: honouring it there would
    /// migrate a lone slice off a stream that is about to go idle anyway,
    /// leaving a hole in the DDC enable mask.
    ///
    /// Callers withhold it in two cases: other slices depend on this
    /// window, or the DDC is pinned for CTUN (RadioModel reads
    /// ReceiverManager::ddcFrequencyLocked for exactly this).
    Placement retuneSlice(int currentStream,
                          bool soleOccupant,
                          bool ddcPinned,
                          double frequencyHz) const;

    int  streamCount() const { return m_streams.size(); }
    int  activeStreamCount() const;
    bool isStreamActive(int streamIndex) const;
    double streamCentreHz(int streamIndex) const;
    int  streamSampleRateHz(int streamIndex) const;

    /// Default rate for a newly claimed stream. Callers override per SKU.
    void setDefaultSampleRateHz(int rateHz) { m_defaultRateHz = rateHz; }

private:
    struct Stream {
        bool   active{false};
        double centreHz{0.0};
        int    sampleRateHz{0};
    };

    /// True when `frequencyHz` is strictly inside the stream's window.
    /// Strict on both sides, matching Thetis console.cs:31920 [v2.10.3.15].
    /// A slice exactly at the Nyquist edge is aliased, so it does not fit.
    bool windowContains(const Stream& s, double frequencyHz) const;

    int firstFreeStream() const;

    QVector<Stream> m_streams;
    int m_maxSlices{0};
    int m_defaultRateHz{192000};
};

} // namespace NereusSDR
