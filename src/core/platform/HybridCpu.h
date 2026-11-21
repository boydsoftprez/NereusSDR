// src/core/platform/HybridCpu.h  (NereusSDR)
// =================================================================
// Ported from Thetis: Console/HybridCpu.cs (v1.3.2), the "P-cores only"
// process-affinity option on Setup -> General -> Options.
//
// Windows exposes the P-core / E-core split through CPU-set information:
// every logical CPU has an EfficiencyClass, and the highest class is the
// performance-core set. On a CPU without a hybrid topology every set has
// the same class, so the option correctly has no effect.
//
// Thetis restricted the whole process, which also covers the audio callback
// threads a driver creates inside it. Nereus does the same here.
// =================================================================
// NereusSDR is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
// =================================================================
#pragma once

#include <QString>
#include <QtGlobal>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

// Result of reading the Windows CPU-set table.
struct HybridCpuTopology {
    bool isHybrid{false};
    // False when the mask cannot be expressed as one process affinity mask
    // (more than 64 logical CPUs, or more than one processor group).
    bool supported{false};
    quint64 allMask{0};
    quint64 performanceMask{0};
    int performanceCount{0};
    int efficiencyCount{0};
    int totalCount{0};
    QString description{QStringLiteral("CPU topology not queried.")};
};

// Query the current CPU topology. Safe to call repeatedly.
NEREUS_CORE_EXPORT HybridCpuTopology detectHybridCpu();

// Restrict this process to the performance cores. Returns false and leaves
// affinity untouched when the CPU is not hybrid or the topology cannot be
// expressed as one process mask; `message` receives the reason either way.
NEREUS_CORE_EXPORT bool applyPerformanceCoresOnly(QString* message = nullptr);

// Allow every logical CPU again.
NEREUS_CORE_EXPORT void restoreAllCores();

} // namespace NereusSDR
