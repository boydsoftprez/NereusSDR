// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR — bandplan value types
//
// Ported from AetherSDR src/models/BandPlanManager.h [@0cd4559].
// AetherSDR is © its contributors and is licensed GPL-3.0-or-later.
//
// Modification history (NereusSDR):
//   2026-04-25  J.J. Boyd <jj@skyrunner.net>  Initial port for Phase 3G RX Epic sub-epic D.
//                                              Extracted Segment/Spot value types out of
//                                              BandPlanManager so consumers can include
//                                              them without QObject. AI assistance:
//                                              Anthropic Claude (claude-opus-4-7).
//   2026-09-25  J.J. Boyd <jj@skyrunner.net>  lowestLicenceClass() moved here from the
//                                              band-plan strip in SpectrumWidget, so the
//                                              strip and the station catalogue share one
//                                              rule. AI assistance: Anthropic Claude Code.

#pragma once

#include <QColor>
#include <QString>

namespace NereusSDR {

struct BandSegment {
    double  lowMhz{0.0};
    double  highMhz{0.0};
    QString label;
    QString license;  // "E", "E,G", "E,G,T", "T", "" = beacon/no TX
    QColor  color;
};

// The lowest licence class a segment's licence codes allow, as the band-plan
// strip names it beside the segment's label ("PHONE General"): "Tech" when the
// codes hold T, "General" when they hold G, "Extra" for exactly "E", and ""
// for anything else (a beacon, no transmit, or a code this rule does not know).
// From AetherSDR SpectrumWidget.cpp:4266-4269 [@0cd4559].
inline QString lowestLicenceClass(const QString& licence)
{
    if (licence.contains(QLatin1Char('T'))) {
        return QStringLiteral("Tech");
    }
    if (licence.contains(QLatin1Char('G'))) {
        return QStringLiteral("General");
    }
    if (licence == QLatin1String("E")) {
        return QStringLiteral("Extra");
    }
    return QString();
}

struct BandSpot {
    double  freqMhz{0.0};
    QString label;
};

}  // namespace NereusSDR
