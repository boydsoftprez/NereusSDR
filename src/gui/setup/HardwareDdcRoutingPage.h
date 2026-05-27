#pragma once

// =================================================================
// src/gui/setup/HardwareDdcRoutingPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Setup -> Hardware -> DDC
// Routing page (power-user override of automatic codec-driven DDC
// assignment). Phase 3F Sub-Epic E Tasks 8-10.
//
// Initial skeleton ships with a one-line explanation + a disabled
// "Reset to automatic" button. The per-DDC override table (slice +
// ADC combos per DDC row) plus the per-MAC AppSettings override
// schema land in a polish iteration once the override semantics are
// finalized against the codec layer from Sub-Epic B.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#include "gui/SetupPage.h"

namespace NereusSDR {

class RadioModel;

// HardwareDdcRoutingPage — Setup -> Hardware -> DDC Routing.
//
// Power-user surface for overriding the automatic DDC-to-slice
// assignment that the active codec emits (Phase 3F Sub-Epic B). The
// skeleton lands first so the menu entry exists; the per-DDC table
// follows once the override schema is committed.
class HardwareDdcRoutingPage : public SetupPage {
    Q_OBJECT

public:
    explicit HardwareDdcRoutingPage(RadioModel* model, QWidget* parent = nullptr);
    ~HardwareDdcRoutingPage() override = default;
};

} // namespace NereusSDR
