// no-port-check: pure math helpers extracted for unit-testability; adaptive algorithm reference only
#pragma once

#include <QRect>
#include <QVector>

namespace NereusSDR::DbmStrip {

// Strip occupies the rightmost `stripW` pixels of the given rect.
//
// IMPORTANT: pass the FULL-WIDTH widget rect (width == widget.width()),
// NOT a rect that has been clipped to leave room for the strip. The strip
// is DERIVED from the full rect — it doesn't sit BESIDE a clipped rect.
// Passing a `w - stripW`-wide rect would put the strip inside the spectrum
// area instead of the reserved right-edge zone.
QRect stripRect(const QRect& specRect, int stripW);

// Arrow row (up/down triangles side-by-side) sits at the top of the strip.
// Height is kDbmArrowH; width matches strip. Returns the arrow row rect.
QRect arrowRowRect(const QRect& strip, int arrowH);

// Hit-test an x coordinate against the left/right halves of the arrow row.
// Returns:
//   -1 = not in arrow row
//    0 = left half (Up)
//    1 = right half (Down)
int arrowHit(int x, const QRect& arrowRow);

// Adaptive dB step sizing matching AetherSDR's 4-6-label target.
// rawStep = dynamicRange / 5.0f
// stepDb  = 20 if rawStep >= 20
//           10 if rawStep >= 10
//            5 if rawStep >=  5
//            2 otherwise
// From AetherSDR SpectrumWidget.cpp:4901-4906 [@0cd4559]
float adaptiveStepDb(float dynamicRange);

// The rounded (lround'd) dBm values SpectrumWidget::drawDbmScaleLabels()
// would draw for a given topDbm/rangeDb: the adaptive-step tick run plus
// the always-drawn bottom label. Empty when rangeDb <= 0 (mirrors that
// function's guard).
//
// Two (topDbm, rangeDb) pairs that return equal sets paint identical
// visible label TEXT for a fixed specRect: drawDbmScaleLabels' own
// on-screen Y-position culling only ever trims entries from this same
// set, deterministically for a fixed layout, so an unchanged set can
// never diverge after culling.
//
// NereusSDR-original (no upstream equivalent -- AetherSDR has no overlay
// cache to keep fresh): pulled out of SpectrumWidget so the GPU-path 3D
// dBm-scale overlay-cache staleness check (SpectrumWidget's
// updateDssScaleOverlayFreshness(), final-review fix I2) can ask "would
// the operator see different text" without a QPainter or a live
// SpectrumWidget.
QVector<int> dssRoundedLabelSet(float topDbm, float rangeDb);

}  // namespace NereusSDR::DbmStrip
