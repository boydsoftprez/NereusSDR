// =================================================================
// src/core/SliceStreamAllocator.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See SliceStreamAllocator.h for
// the full rationale and Modification history block.
//
// =================================================================

#include "core/SliceStreamAllocator.h"

namespace NereusSDR {

void SliceStreamAllocator::configure(int userDdcCount, int maxSlices)
{
    m_streams.clear();
    m_streams.resize(qMax(0, userDdcCount));
    m_maxSlices = qMax(0, maxSlices);
}

void SliceStreamAllocator::activateStream(int streamIndex, double centreHz,
                                          int sampleRateHz)
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return; }
    Stream& s = m_streams[streamIndex];
    s.active       = true;
    s.centreHz     = centreHz;
    s.sampleRateHz = sampleRateHz;
}

void SliceStreamAllocator::deactivateStream(int streamIndex)
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return; }
    m_streams[streamIndex].active = false;
}

bool SliceStreamAllocator::windowContains(const Stream& s,
                                          double frequencyHz) const
{
    if (!s.active || s.sampleRateHz <= 0) { return false; }
    const double halfWindow = static_cast<double>(s.sampleRateHz) / 2.0;
    const double offset     = frequencyHz - s.centreHz;
    // Strict both sides. From Thetis console.cs:31920 [v2.10.3.15]:
    //   if (rx2_osc > -sample_rate_rx1 / 2 && rx2_osc < sample_rate_rx1 / 2)
    // MW0LGE [2.7.0.9] only when RX'ing. Fixes issue where multirx would be
    // outside sample area after a tx  [original inline comment from
    // console.cs:31913]
    return offset > -halfWindow && offset < halfWindow;
}

int SliceStreamAllocator::firstFreeStream() const
{
    for (int i = 0; i < m_streams.size(); ++i) {
        if (!m_streams.at(i).active) { return i; }
    }
    return -1;
}

SliceStreamAllocator::Placement
SliceStreamAllocator::placeSlice(double frequencyHz,
                                 bool preferOwnStream) const
{
    Placement p;

    // 1. Prefer sharing: an active stream whose window already covers this
    //    frequency costs no extra DDC and no extra bus bandwidth.
    //
    //    Skipped when the caller asked for its own window. See the header for
    //    why a pan and a slice want different answers here.
    if (!preferOwnStream) {
        for (int i = 0; i < m_streams.size(); ++i) {
            if (windowContains(m_streams.at(i), frequencyHz)) {
                p.outcome       = Outcome::JoinedExisting;
                p.streamIndex   = i;
                p.shiftOffsetHz = frequencyHz - m_streams.at(i).centreHz;
                return p;
            }
        }
    }

    // 2. Otherwise claim a free DDC and centre it on the slice.
    const int free = firstFreeStream();
    if (free >= 0) {
        p.outcome           = Outcome::NewStream;
        p.streamIndex       = free;
        p.shiftOffsetHz     = 0.0;
        p.newStreamCentreHz = frequencyHz;
        return p;
    }

    // 3. Every DDC is in use. Thetis would simply disable Multi-RX here
    //    (console.cs:31924); we surface the hardware limit instead so the UI
    //    can explain it.
    p.outcome = Outcome::Rejected;
    if (preferOwnStream) {
        // Distinct wording: nothing is wrong with the frequency here, the
        // radio simply has no spare receiver to give this pan. Reusing the
        // "none covers" phrasing would send the operator off retuning, which
        // cannot help.
        p.reason = QStringLiteral(
            "All %1 receiver DDCs are in use, so this radio cannot give a new "
            "panadapter its own receiver. Remove a panadapter, or add this "
            "slice to an existing one to share its receiver.")
            .arg(m_streams.size());
    } else {
        p.reason = QStringLiteral(
            "All %1 receiver DDCs are in use and none covers %2 MHz. "
            "Retune or remove a slice, or widen a DDC's sample rate.")
            .arg(m_streams.size())
            .arg(frequencyHz / 1.0e6, 0, 'f', 4);
    }
    return p;
}

SliceStreamAllocator::Placement
SliceStreamAllocator::retuneSlice(int currentStream,
                                  bool soleOccupant,
                                  bool ddcPinned,
                                  double frequencyHz) const
{
    Placement p;

    const bool haveStream =
        currentStream >= 0 && currentStream < m_streams.size();
    const bool inWindow =
        haveStream && windowContains(m_streams.at(currentStream), frequencyHz);

    // Sole occupant: no other slice depends on this window, so the DDC
    // follows the slice. Checked BEFORE the window test, because a lone
    // slice belongs on its DDC centre where it has the full half-rate of
    // headroom in both directions, not parked at an arbitrary offset left
    // over from wherever the stream happened to be claimed.
    //
    // The CTUN pin holds this back only while the slice is still inside the
    // window -- which is exactly what CTUN is for. Once the slice leaves,
    // the panadapter has to jump regardless (MainWindow's band-jump handler
    // drops the lock and re-centres for precisely this case), so keeping the
    // stream parked achieves nothing and costs a DDC: the slice would
    // migrate away and leave its old stream with no occupants at all.
    //
    // Bench defect, ANAN-G2E 2026-07-26: that migration left the enable mask
    // as DDC0 + DDC2 with a hole at DDC1, and the second pan went dead.
    if (haveStream && soleOccupant && (!ddcPinned || !inWindow)) {
        p.outcome           = Outcome::RetunedStream;
        p.streamIndex       = currentStream;
        p.shiftOffsetHz     = 0.0;
        p.newStreamCentreHz = frequencyHz;
        return p;
    }

    // Still inside its own window: nothing moves but the shift oscillator.
    if (inWindow) {
        p.outcome       = Outcome::JoinedExisting;
        p.streamIndex   = currentStream;
        p.shiftOffsetHz = frequencyHz - m_streams.at(currentStream).centreHz;
        return p;
    }

    // Outside it, and the window belongs to someone else too: this slice
    // must leave. Same policy as a fresh placement.
    return placeSlice(frequencyHz);
}

int SliceStreamAllocator::activeStreamCount() const
{
    int n = 0;
    for (const Stream& s : m_streams) {
        if (s.active) { ++n; }
    }
    return n;
}

bool SliceStreamAllocator::isStreamActive(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return false; }
    return m_streams.at(streamIndex).active;
}

double SliceStreamAllocator::streamCentreHz(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return 0.0; }
    return m_streams.at(streamIndex).centreHz;
}

int SliceStreamAllocator::streamSampleRateHz(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return 0; }
    return m_streams.at(streamIndex).sampleRateHz;
}

} // namespace NereusSDR
