// =================================================================
// src/gui/RemoteDisplayAllocator.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original GUI display-quality allocation.
//
// =================================================================

#pragma once

#include "core/session/media/DisplayBudget.h"

#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

struct RemoteDisplayIntent {
    enum class Kind { Pan, Mini };
    QString panId;
    int pixels = 0;
    int fps = 0;
    bool includeWidePlane = false;
    int waterfallPeriodMs = 0;
    bool active = false;
    Kind kind = Kind::Pan;
    /// The display extras sections the request asks for (kDisplayExtras*
    /// bits; a remote window asks only for the waterfall levels), which the
    /// Core charges on top of the frames (displayCostWithExtras).
    quint8 extrasSections = 0;
};

struct RemoteDisplayQuality {
    QString panId;
    int pixels = 0;
    int fps = 0;
    int framesPerLine = 0;
    bool suspended = false;
    DisplayBudgetCharge charge;
};

struct RemoteDisplayAllocation {
    QList<RemoteDisplayQuality> pans;
    DisplayBudgetCharge total;
};

std::optional<RemoteDisplayAllocation> allocateRemoteDisplay(
    const DisplayBudgetLimits& limits, const QList<RemoteDisplayIntent>& intents,
    bool ps3Enabled, QString* error = nullptr, bool iqActive = false);

} // namespace NereusSDR
