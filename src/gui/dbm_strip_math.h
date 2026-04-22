// no-port-check: pure math helpers extracted for unit-testability; adaptive algorithm reference only
#pragma once

#include <QRect>

namespace NereusSDR::DbmStrip {

// Strip occupies the rightmost kDbmStripW pixels of the spectrum area.
// Returns the strip rect given the full spectrum area rect and strip width.
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

}  // namespace NereusSDR::DbmStrip
