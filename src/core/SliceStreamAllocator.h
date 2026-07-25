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
    Placement placeSlice(double frequencyHz) const;

    /// Where should an existing slice go after retuning to `frequencyHz`?
    ///
    /// `mayRetuneStream` is a PERMISSION to move the stream's centre, not
    /// merely an observation about occupancy. Granted, the DDC follows the
    /// slice (Outcome::RetunedStream) whether or not the new frequency
    /// still falls inside the old window, because a lone slice belongs on
    /// its DDC centre. Withheld, the stream's centre is fixed: the slice
    /// carries a shift offset while it stays in the window and migrates to
    /// another DDC when it leaves.
    ///
    /// Callers withhold it in two cases: other slices depend on this
    /// window, or the DDC is pinned for CTUN (RadioModel reads
    /// ReceiverManager::ddcFrequencyLocked for exactly this).
    Placement retuneSlice(int currentStream,
                          bool mayRetuneStream,
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
