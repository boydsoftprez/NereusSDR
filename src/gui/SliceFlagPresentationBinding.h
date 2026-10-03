#pragma once

// =================================================================
// src/gui/SliceFlagPresentationBinding.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native lifetime boundary for a secondary slice's model-to-flag
// presentation wiring. There is no upstream AetherSDR equivalent for the
// multi-pan flag rehoming lifecycle exercised here (no port check applies).
//
// Modification history (NereusSDR):
//   2026-09-21 -- Extracted by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via OpenAI Codex.
//   2026-09-23 -- Frequency, mode, filter, AGC, gain, step and
//                 antenna handlers moved here with the flag as context
//                 (R-R3-30), by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "core/WdspTypes.h"

#include <QList>
#include <QMetaObject>

#include <functional>

namespace NereusSDR {

class SliceModel;
class VfoWidget;

/// Wire model state that is rendered directly by a secondary slice flag.
///
/// The returned handles make the connection lifetime contract independently
/// testable. MainWindow does not otherwise need to retain them.
QList<QMetaObject::Connection> wireSliceFlagPresentation(
    SliceModel* slice, VfoWidget* flag);

/// Host-side follow-up work for the frequency, mode and filter presentation
/// handlers: the hosting pan's VFO marker, band jump, TX mode and passband.
/// Each hook runs after the flag has been updated. Empty hooks are skipped.
struct SliceFlagHostHooks {
    std::function<void(double hz)> frequencyChanged;
    std::function<void(DSPMode mode)> modeChanged;
    std::function<void(int low, int high)> filterChanged;
};

/// Wire the flag's frequency, mode, filter, AGC mode, AF/RF gain, step and
/// antenna presentation. Every connection uses the flag as its context, so
/// removing or rehoming the flag retires all of them and a rehome never
/// leaves an extra copy of a handler behind (R-R3-30). The hooks are owned
/// by the connections and die with the flag too.
QList<QMetaObject::Connection> wireSliceFlagStatePresentation(
    SliceModel* slice, VfoWidget* flag, SliceFlagHostHooks hooks);

}  // namespace NereusSDR
