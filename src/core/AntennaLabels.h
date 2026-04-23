// src/core/AntennaLabels.h (NereusSDR)
//
// Single source for the "ANT1/ANT2/ANT3" label list. Replaces 10+
// hardcoded QStringList{"ANT1","ANT2","ANT3"} sites across the UI.
// Returns an empty list when the connected board has no Alex (HL2,
// Atlas), so UI callers can `setVisible(!labels.isEmpty())`.
//
// no-port-check: NereusSDR-original file. The reference to networkproto1.c
// below is a documentation reference to the wire-encoding spec, not ported
// logic. All logic in this file is independently implemented.
//
// Phase 3P-I-a. Design: docs/architecture/antenna-routing-design.md §6.1 Rule 1.

#pragma once
#include "SkuUiProfile.h"
#include <QStringList>
#include <array>

namespace NereusSDR {
struct BoardCapabilities;

QStringList antennaLabels(const BoardCapabilities& caps);

// RX-only antenna labels — SKU-specific (RX1/RX2/XVTR on Hermes;
// EXT2/EXT1/XVTR on ANAN100-class; BYPS/EXT1/XVTR on 7000D/G2/etc.).
// Returns the three strings in wire order (index 1, 2, 3 per the C3
// encoding at networkproto1.c:479-481).
//
// Phase 3P-I-b.
std::array<QString, 3> rxOnlyLabels(const SkuUiProfile& sku);
}
