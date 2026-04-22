// src/core/AntennaLabels.h (NereusSDR)
//
// Single source for the "ANT1/ANT2/ANT3" label list. Replaces 10+
// hardcoded QStringList{"ANT1","ANT2","ANT3"} sites across the UI.
// Returns an empty list when the connected board has no Alex (HL2,
// Atlas), so UI callers can `setVisible(!labels.isEmpty())`.
//
// Phase 3P-I-a. Design: docs/architecture/antenna-routing-design.md §6.1 Rule 1.

#pragma once
#include <QStringList>

namespace NereusSDR {
struct BoardCapabilities;

QStringList antennaLabels(const BoardCapabilities& caps);
}
