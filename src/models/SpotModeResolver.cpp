// no-port-check: AetherSDR-derived NereusSDR file. The spot mode resolver is
// ported from AetherSDR src/core/SpotModeResolver.{h,cpp} [@1e0718ad].
// Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/SpotModeResolver.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR src/core/SpotModeResolver.cpp [@1e0718ad].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See SpotModeResolver.h for the Modification history (NereusSDR).
// =================================================================

#include "models/SpotModeResolver.h"

#include "models/Band.h"
#include "models/BandDefaults.h"
#include "models/SpotModel.h"

#include <QMap>
#include <QSet>
#include <QStringList>

namespace NereusSDR::SpotModeResolver {

namespace {

// From AetherSDR src/core/SpotModeResolver.cpp:11-19 [@1e0718ad].
const QSet<QString>& knownSpotModes()
{
    static const QSet<QString> kSet = {
        "CW", "SSB", "USB", "LSB", "AM", "FM", "FT8", "FT4",
        "JS8", "RTTY", "PSK31", "PSK63", "PSK", "OLIVIA",
        "JT65", "JT9", "SAM", "NFM", "DIGU", "DIGL"
    };
    return kSet;
}

// From AetherSDR src/core/SpotModeResolver.cpp:21-34 [@1e0718ad].
const QMap<QString, QString>& spotToRadioModeMap()
{
    static const QMap<QString, QString> kMap = {
        {"CW", "CW"}, {"CWL", "CW"}, {"CWU", "CW"},
        {"USB", "USB"}, {"LSB", "LSB"},
        {"FT8", "DIGU"}, {"FT4", "DIGU"}, {"JS8", "DIGU"},
        {"PSK31", "DIGU"}, {"PSK63", "DIGU"}, {"PSK", "DIGU"},
        {"OLIVIA", "DIGU"}, {"JT65", "DIGU"}, {"JT9", "DIGU"},
        {"RTTY", "DIGL"},
        {"AM", "AM"}, {"SAM", "SAM"},
        {"FM", "FM"}, {"NFM", "NFM"},
    };
    return kMap;
}

} // namespace

// From AetherSDR src/core/SpotModeResolver.cpp:38-47 [@1e0718ad].
QString extractSpotModeFromComment(const QString& comment)
{
    const auto& known = knownSpotModes();
    QStringList words = comment.split(' ', Qt::SkipEmptyParts);
    if (!words.isEmpty() && known.contains(words.first().toUpper())) {
        return words.first().toUpper();
    }
    if (!words.isEmpty() && known.contains(words.last().toUpper())) {
        return words.last().toUpper();
    }
    return {};
}

// From AetherSDR src/core/SpotModeResolver.cpp:49-68 [@1e0718ad].
QString inferSpotModeFromBand(double f)
{
    if ((f >= 1.800 && f < 1.850) || (f >= 3.500 && f < 3.600) ||
        (f >= 7.000 && f < 7.050) || (f >= 10.100 && f < 10.140) ||
        (f >= 14.000 && f < 14.070) || (f >= 18.068 && f < 18.095) ||
        (f >= 21.000 && f < 21.070) || (f >= 24.890 && f < 24.920) ||
        (f >= 28.000 && f < 28.070) || (f >= 50.000 && f < 50.100)) {
        return "CW";
    }
    if ((f >= 1.840 && f < 1.850) || (f >= 3.570 && f < 3.600) ||
        (f >= 7.040 && f < 7.050) || (f >= 10.130 && f < 10.150) ||
        (f >= 14.070 && f < 14.100) || (f >= 18.095 && f < 18.110) ||
        (f >= 21.070 && f < 21.100) || (f >= 24.915 && f < 24.930) ||
        (f >= 28.070 && f < 28.150)) {
        return "DIGU";
    }
    if (f >= 10.0) {
        return "USB";
    }
    if (f >= 1.8) {
        return "LSB";
    }
    return {};
}

// From AetherSDR src/core/SpotModeResolver.cpp:70-78 [@1e0718ad].
QString mapSpotModeToRadioMode(const QString& spotMode, double rxFreqMhz)
{
    const auto& map = spotToRadioModeMap();
    if (map.contains(spotMode)) {
        return map.value(spotMode);
    }
    if (spotMode == "SSB") {
        return (rxFreqMhz >= 10.0) ? "USB" : "LSB";
    }
    return {};
}

// From AetherSDR src/core/SpotModeResolver.cpp:80-92 [@1e0718ad].
QString resolveSpotRadioMode(const QString& explicitMode,
                             const QString& comment,
                             double rxFreqMhz)
{
    QString spotMode = explicitMode.toUpper().trimmed();
    if (spotMode.isEmpty()) {
        spotMode = extractSpotModeFromComment(comment);
    }
    if (spotMode.isEmpty()) {
        spotMode = inferSpotModeFromBand(rxFreqMhz);
    }
    if (spotMode.isEmpty()) {
        return {};
    }
    return mapSpotModeToRadioMode(spotMode, rxFreqMhz);
}

// NereusSDR: AetherSDR's spot click sets a radio mode name on a FlexRadio
// slice; a NereusSDR slice takes a DSPMode. See the header.
std::optional<DSPMode> dspModeForSpot(const SpotData& spot)
{
    // A FreeDV spot is RADE, on the sideband of the band's own default:
    // AetherSDR picks DIGL where the band's default mode is LSB and DIGU
    // everywhere else (MainWindow_DigitalModes.cpp:340-350 [@1e0718ad]).
    if (spot.source == QLatin1String("FreeDV")) {
        const BandSeed seed = BandDefaults::seedFor(bandFromFrequency(spot.rxFreqMhz * 1.0e6));
        return (seed.valid && seed.mode == DSPMode::LSB) ? DSPMode::RADE_L : DSPMode::RADE_U;
    }
    const QString radioMode = resolveSpotRadioMode(spot.mode, spot.comment, spot.rxFreqMhz);
    const bool upper = spot.rxFreqMhz >= 10.0;
    if (radioMode == QLatin1String("CW"))   { return upper ? DSPMode::CWU : DSPMode::CWL; }
    if (radioMode == QLatin1String("USB"))  { return DSPMode::USB; }
    if (radioMode == QLatin1String("LSB"))  { return DSPMode::LSB; }
    if (radioMode == QLatin1String("DIGU")) { return DSPMode::DIGU; }
    if (radioMode == QLatin1String("DIGL")) { return DSPMode::DIGL; }
    if (radioMode == QLatin1String("AM"))   { return DSPMode::AM; }
    if (radioMode == QLatin1String("SAM"))  { return DSPMode::SAM; }
    if (radioMode == QLatin1String("FM") || radioMode == QLatin1String("NFM")) {
        return DSPMode::FM;
    }
    return std::nullopt;
}

} // namespace NereusSDR::SpotModeResolver
