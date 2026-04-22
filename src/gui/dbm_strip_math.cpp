// no-port-check: pure math helpers extracted for unit-testability; adaptive algorithm reference only
//
// Pure helpers for the right-edge dBm strip geometry and adaptive step.
// Extracted so SpectrumWidget's paint + hit-test logic is unit-testable
// without a display. No Qt widget dependency.

#include "dbm_strip_math.h"

namespace NereusSDR::DbmStrip {

QRect stripRect(const QRect& specRect, int stripW)
{
    const int stripX = specRect.right() - stripW + 1;
    return QRect(stripX, specRect.top(), stripW, specRect.height());
}

QRect arrowRowRect(const QRect& strip, int arrowH)
{
    return QRect(strip.left(), strip.top(), strip.width(), arrowH);
}

int arrowHit(int x, const QRect& arrowRow)
{
    if (x < arrowRow.left() || x > arrowRow.right()) {
        return -1;
    }
    const int half = arrowRow.left() + arrowRow.width() / 2;
    return (x < half) ? 0 : 1;
}

// From AetherSDR SpectrumWidget.cpp:4901-4906 [@0cd4559]
float adaptiveStepDb(float dynamicRange)
{
    const float rawStep = dynamicRange / 5.0f;
    if (rawStep >= 20.0f) return 20.0f;
    if (rawStep >= 10.0f) return 10.0f;
    if (rawStep >=  5.0f) return  5.0f;
    return 2.0f;
}

}  // namespace NereusSDR::DbmStrip
