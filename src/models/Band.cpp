// src/models/Band.cpp
#include "Band.h"

#include <QStringLiteral>

namespace NereusSDR {

namespace {

// Band edges in Hz. IARU Region 2 covers the widest allocations; we use
// the union so European/Australian frequencies also round-trip correctly.
// Source: ARRL band plan / IARU Region 2, cross-checked against Thetis
// BandStackManager HF definitions.
struct HamBandRange {
    Band band;
    double lowHz;
    double highHz;
};

constexpr HamBandRange kHamBandRanges[] = {
    { Band::Band160m,  1800000.0,   2000000.0 },
    { Band::Band80m,   3500000.0,   4000000.0 },
    { Band::Band60m,   5330000.0,   5410000.0 },  // US channelized block
    { Band::Band40m,   7000000.0,   7300000.0 },
    { Band::Band30m,  10100000.0,  10150000.0 },
    { Band::Band20m,  14000000.0,  14350000.0 },
    { Band::Band17m,  18068000.0,  18168000.0 },
    { Band::Band15m,  21000000.0,  21450000.0 },
    { Band::Band12m,  24890000.0,  24990000.0 },
    { Band::Band10m,  28000000.0,  29700000.0 },
    { Band::Band6m,   50000000.0,  54000000.0 },
};

// WWV time-signal transmitters (NIST Fort Collins, CO) and WWVH (Hawaii).
// Detect with a ±5 kHz window so a VFO parked near the carrier still
// auto-selects WWV.
constexpr double kWwvCenters[] = {
    2500000.0, 5000000.0, 10000000.0, 15000000.0, 20000000.0, 25000000.0
};
constexpr double kWwvHalfWindow = 5000.0;

} // namespace

QString bandLabel(Band b)
{
    switch (b) {
        case Band::Band160m: return QStringLiteral("160m");
        case Band::Band80m:  return QStringLiteral("80m");
        case Band::Band60m:  return QStringLiteral("60m");
        case Band::Band40m:  return QStringLiteral("40m");
        case Band::Band30m:  return QStringLiteral("30m");
        case Band::Band20m:  return QStringLiteral("20m");
        case Band::Band17m:  return QStringLiteral("17m");
        case Band::Band15m:  return QStringLiteral("15m");
        case Band::Band12m:  return QStringLiteral("12m");
        case Band::Band10m:  return QStringLiteral("10m");
        case Band::Band6m:   return QStringLiteral("6m");
        case Band::GEN:      return QStringLiteral("GEN");
        case Band::WWV:      return QStringLiteral("WWV");
        case Band::XVTR:     return QStringLiteral("XVTR");
        case Band::Count:    break;
    }
    return QStringLiteral("GEN");
}

QString bandKeyName(Band b)
{
    // Keep as a separate function from bandLabel so UI label text can
    // change without breaking persisted AppSettings keys.
    return bandLabel(b);
}

Band bandFromFrequency(double hz)
{
    for (const double center : kWwvCenters) {
        if (hz >= center - kWwvHalfWindow && hz <= center + kWwvHalfWindow) {
            return Band::WWV;
        }
    }
    for (const auto& range : kHamBandRanges) {
        if (hz >= range.lowHz && hz <= range.highHz) {
            return range.band;
        }
    }
    return Band::GEN;
}

Band bandFromUiIndex(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(Band::Count)) {
        return Band::GEN;
    }
    return static_cast<Band>(idx);
}

int uiIndexFromBand(Band b)
{
    return static_cast<int>(b);
}

} // namespace NereusSDR
