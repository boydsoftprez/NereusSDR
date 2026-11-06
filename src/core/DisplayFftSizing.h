#pragma once

// =================================================================
// src/core/DisplayFftSizing.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The stored side of the FFT sizing mode
// (design D38 to D41; R-DSP-62 to R-DSP-65): the one function that reads
// the mode and the values it plans from, the one that writes a mode, and
// the rule a Core applies when a client that does not know the mode writes
// the Hz/bin target. The planner itself is ControlRanges::planDisplayFft.
//
// The keys are station settings (SettingsScope.cpp), so in a remote window
// they hold the Core's values and a write here goes to the Core.
//
// Modification history (NereusSDR):
//   2026-10-11  J.J. Boyd (KG4VCF). Created for the FFT sizing mode.
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/AppSettings.h"
#include "core/ControlRanges.h"

#include <QByteArray>
#include <QLatin1String>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace NereusSDR {

/// The station's FFT sizing as stored, with the on-read rule applied.
struct DisplayFftSizing {
    /// The mode in force (ControlRanges::displayFftSizingModeOnRead).
    ControlRanges::DisplayFftSizingMode mode{ControlRanges::kDisplayFftSizingModeDefault};
    /// The stored DisplayFftSize, the FFT Size slider's own value: one of
    /// the seven sizes, the default when the key is unset or holds anything
    /// else. It sets the running size in Fixed only.
    int storedSize{ControlRanges::kDisplayFftSizeDefault};
    /// The Hz/bin target in force: above 0 in HzPerBin, 0 in the other modes.
    double hzPerBinTarget{0.0};
    /// The last Hz/bin target chosen (DisplayHzPerBinLastTarget; the target
    /// in force when none is stored), 0 when there has never been one. What
    /// a grayed Hz/bin control shows and what entering Hz/bin restores.
    double lastHzPerBinTarget{0.0};

    /// The size the mode's own control chose
    /// (ControlRanges::displayFftSizingBaseline).
    int baseline() const noexcept
    {
        return ControlRanges::displayFftSizingBaseline(mode, storedSize);
    }

    /// The size a pan at `sampleRateHz` showing `spanHz` plans
    /// (ControlRanges::planDisplayFft).
    ControlRanges::AutoZoomPlan plan(double sampleRateHz, double spanHz) const noexcept
    {
        return ControlRanges::planDisplayFft(mode, storedSize, sampleRateHz, spanHz,
                                             hzPerBinTarget);
    }
};

namespace DisplayFftSizingDetail {

// A stored Hz/bin value as a target: above 0 and finite, at most the
// control's maximum; anything else is 0.
inline double hzPerBinFrom(const QVariant& stored)
{
    const double hz = stored.toString().toDouble();
    if (!std::isfinite(hz) || !(hz > 0.0)) {
        return 0.0;
    }
    return std::min(hz, ControlRanges::kDisplayHzPerBinTargetMax);
}

inline void setIfDifferent(AppSettings& settings, const char* key, const QString& value)
{
    const QString name = QLatin1String(key);
    if (!settings.contains(name) || settings.value(name).toString() != value) {
        settings.setValue(name, value);
    }
}

}  // namespace DisplayFftSizingDetail

/// Reads the station's FFT sizing. The one reader: the local window's
/// planner, a remote window's planner, Setup and the pill all take the mode
/// and its values from here, so the upgrade rule (design D40) is applied in
/// one place. Reads only; a stored size that is not an FFT size is not
/// repaired here (MainWindow::refreshFftPoolConfig does that).
inline DisplayFftSizing readDisplayFftSizing(const AppSettings& settings)
{
    using namespace ControlRanges;
    DisplayFftSizing sizing;
    const int size = settings.value(QLatin1String(kDisplayFftSizeKey),
                                    QString::number(kDisplayFftSizeDefault)).toString().toInt();
    sizing.storedSize = isDisplayFftSize(size) ? size : kDisplayFftSizeDefault;
    const double target = DisplayFftSizingDetail::hzPerBinFrom(
        settings.value(QLatin1String(kDisplayHzPerBinTargetKey)));
    const QByteArray token =
        settings.value(QLatin1String(kDisplayFftSizingModeKey)).toString().toLatin1();
    sizing.mode = displayFftSizingModeOnRead(
        std::string_view(token.constData(), std::size_t(token.size())), target);
    sizing.hzPerBinTarget = sizing.mode == DisplayFftSizingMode::HzPerBin ? target : 0.0;
    const double last = DisplayFftSizingDetail::hzPerBinFrom(
        settings.value(QLatin1String(kDisplayHzPerBinLastTargetKey)));
    sizing.lastHzPerBinTarget = last > 0.0 ? last : sizing.hzPerBinTarget;
    return sizing;
}

/// The rate a window on a Core paints at and asks the Core to send frames at:
/// its own DisplayPaintFps once the Display pill has set one, otherwise the
/// Core's DisplaySpectrumFps, so a window's first run changes nothing on sight
/// and a window with no rate of its own keeps following the Core's. Not read
/// direct to a radio, where DisplaySpectrumFps is the one rate.
inline int readDisplayPaintFps(const AppSettings& settings)
{
    using namespace ControlRanges;
    const int own = settings.value(QLatin1String(kDisplayPaintFpsKey)).toString().toInt();
    const int fps = own > 0 ? own
                            : settings.value(QLatin1String(kDisplaySpectrumFpsKey),
                                             QString::number(kDisplaySpectrumFpsDefault))
                                  .toString().toInt();
    return qBound(1, fps, 60);
}

/// The mode alone (readDisplayFftSizing(settings).mode).
inline ControlRanges::DisplayFftSizingMode readDisplayFftSizingMode(const AppSettings& settings)
{
    return readDisplayFftSizing(settings).mode;
}

/// Stores `mode`, and the Hz/bin keys with it: DisplayFftSizingMode first,
/// then DisplayHzPerBinTarget (the target in HzPerBin, 0 in the other
/// modes, which is what the key means to a client that does not know the
/// mode), then, in HzPerBin, DisplayHzPerBinLastTarget. The mode goes first
/// so a Core's rule for the target key (followHzPerBinTargetWrite) finds the
/// mode already in place and leaves it. A key already holding its value is
/// not written again. DisplayFftSize is never touched.
///
/// `hzPerBinTarget` is read in HzPerBin only: the target to run, clamped to
/// the control's range. When it is not above 0 the remembered target is
/// used, and with none remembered the control's maximum (the coarsest,
/// which changes the least); a caller that wants "the bin width running
/// now" passes it.
inline void writeDisplayFftSizingMode(AppSettings& settings,
                                      ControlRanges::DisplayFftSizingMode mode,
                                      double hzPerBinTarget = 0.0)
{
    using namespace ControlRanges;
    double target = 0.0;
    if (mode == DisplayFftSizingMode::HzPerBin) {
        target = DisplayFftSizingDetail::hzPerBinFrom(QString::number(hzPerBinTarget));
        if (!(target > 0.0)) {
            target = readDisplayFftSizing(settings).lastHzPerBinTarget;
        }
        if (!(target > 0.0)) {
            target = kDisplayHzPerBinTargetMax;
        }
    }
    DisplayFftSizingDetail::setIfDifferent(
        settings, kDisplayFftSizingModeKey, QLatin1String(displayFftSizingModeToken(mode)));
    DisplayFftSizingDetail::setIfDifferent(
        settings, kDisplayHzPerBinTargetKey, QString::number(target));
    if (mode == DisplayFftSizingMode::HzPerBin) {
        DisplayFftSizingDetail::setIfDifferent(
            settings, kDisplayHzPerBinLastTargetKey, QString::number(target));
    }
}

/// The Core's rule for a client that does not know the mode (design D41;
/// R-DSP-65). Call it after a station write of DisplayHzPerBinTarget has
/// been stored: a target above 0 sets the mode HzPerBin (and remembers the
/// target); 0 sets Auto if the stored mode was HzPerBin; Fixed and Auto are
/// left alone on 0. A client that knows the mode writes the mode first, so
/// this finds it in place and changes nothing. Returns whether the mode key
/// was written.
inline bool followHzPerBinTargetWrite(AppSettings& settings)
{
    using namespace ControlRanges;
    const QString modeKey = QLatin1String(kDisplayFftSizingModeKey);
    const QString stored = settings.value(modeKey).toString();
    const double target = DisplayFftSizingDetail::hzPerBinFrom(
        settings.value(QLatin1String(kDisplayHzPerBinTargetKey)));
    if (target > 0.0) {
        DisplayFftSizingDetail::setIfDifferent(
            settings, kDisplayHzPerBinLastTargetKey, QString::number(target));
        if (stored == QLatin1String(kDisplayFftSizingModeHzPerBinToken)) {
            return false;
        }
        settings.setValue(modeKey, QString::fromLatin1(kDisplayFftSizingModeHzPerBinToken));
        return true;
    }
    if (stored == QLatin1String(kDisplayFftSizingModeHzPerBinToken)) {
        settings.setValue(modeKey, QString::fromLatin1(kDisplayFftSizingModeAutoToken));
        return true;
    }
    return false;
}

}  // namespace NereusSDR
