#pragma once

// =================================================================
// src/gui/setup/HardwareDdcRoutingPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Setup -> Hardware -> DDC
// Routing page (power-user override of automatic codec-driven DDC
// assignment). Phase 3F Sub-Epic E Tasks 8-10.
//
// Ships a 7-row override table (DDC0..DDC6 max, matching the P2
// Orion-class DDC bank).  Each row offers a Slice combo (auto / A..E)
// and an ADC combo (auto / ADC0 / ADC1).  Defaults are (auto) for both;
// explicit picks persist per-MAC via AppSettings under
// hardware/<mac>/DdcOverride<N>/{Slice,Adc}.  Codec-side consumption
// (read these overrides during the codec's auto-assignment pass) wires
// as a follow-up; today the page is round-trip-correct.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
//   2026-05-27 Phase 3F closeout (Sub-Epic E Tasks 8-10): replaced
//              stub with per-DDC override table backed by AppSettings.
// =================================================================

#include "gui/SetupPage.h"

namespace NereusSDR {

class RadioModel;

// HardwareDdcRoutingPage — Setup -> Hardware -> DDC Routing.
//
// Power-user surface for overriding the automatic DDC-to-slice
// assignment that the active codec emits (Phase 3F Sub-Epic B).
class HardwareDdcRoutingPage : public SetupPage {
    Q_OBJECT

public:
    explicit HardwareDdcRoutingPage(RadioModel* model, QWidget* parent = nullptr);
    ~HardwareDdcRoutingPage() override = default;
};

} // namespace NereusSDR
