// no-port-check: NereusSDR-original file; Thetis console.cs references are
//   doc comments only — actual default values are delegated to
//   SliceModel::presetsForMode which already carries full attribution.
// =================================================================
// src/models/FilterPresetStore.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. Filter preset defaults are delegated to
// SliceModel::presetsForMode (ported from Thetis console.cs:5180-5575
// [v2.10.3.13] — InitFilterPresets). This class adds a user-override
// layer + AppSettings persistence on top.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
// =================================================================

#pragma once

#include "core/WdspTypes.h"

#include <QList>
#include <QObject>
#include <QString>

namespace NereusSDR {

/// A single filter preset slot.
/// name   — display label (e.g., "F1", "DX-2.4k"); user-renameable.
/// low    — low edge in Hz, signed (negative for LSB family).
/// high   — high edge in Hz, signed.
struct FilterPreset {
    QString name;
    int     low{0};
    int     high{0};
};

/// Wraps the Thetis-verbatim defaults from SliceModel::presetsForMode
/// with a user-override layer persisted in AppSettings.
///
/// AppSettings key layout (per slot):
///   filters/<mode>/<slot>/name   — e.g. "F1" / "DX-2.4k"
///   filters/<mode>/<slot>/low    — integer Hz (string-encoded)
///   filters/<mode>/<slot>/high   — integer Hz
///
/// <mode> is the DSPMode enum identifier as returned by
/// SliceModel::modeName(): "LSB", "USB", "DSB", "CWL", "CWU",
/// "FM", "AM", "DIGU", "SPEC", "DIGL", "SAM", "DRM".
///
/// A slot is only present in AppSettings when it has been explicitly
/// customised.  Missing keys fall through to defaultPreset().
///
/// Callers subscribe to presetsChanged(DSPMode) and call
/// rebuildFilterButtons on emission.
class FilterPresetStore : public QObject {
    Q_OBJECT
public:
    explicit FilterPresetStore(QObject* parent = nullptr);

    // ── Read ──────────────────────────────────────────────────────────────

    /// Returns the 10 presets for the mode in the user's chosen order.
    /// Any slot that has no user override returns the Thetis default for
    /// that slot (via defaultPreset).
    QList<FilterPreset> presetsForMode(DSPMode mode) const;

    /// Returns the Thetis-verbatim default preset for a given mode + slot.
    /// Defaults come from SliceModel::presetsForMode (InitFilterPresets,
    /// console.cs:5180-5575 [v2.10.3.13]).  Slot is 0-based (0..9).
    static FilterPreset defaultPreset(DSPMode mode, int slot);

    // ── Write ─────────────────────────────────────────────────────────────

    /// Update a single preset slot.  Persists immediately.
    /// Slot is 0-based.  Emits presetsChanged(mode).
    void setPreset(DSPMode mode, int slot, const FilterPreset& p);

    /// Replace the whole list for a mode (used by reorder + reset-all for
    /// a single mode).  Persists immediately.  Emits presetsChanged(mode).
    void setPresetsForMode(DSPMode mode, const QList<FilterPreset>& presets);

    /// Reset a single slot to its Thetis default.
    /// Removes the per-slot AppSettings keys.  Emits presetsChanged(mode).
    void resetPreset(DSPMode mode, int slot);

    /// Reset all slots in a mode to Thetis defaults.
    /// Removes all per-mode AppSettings keys.  Emits presetsChanged(mode).
    void resetMode(DSPMode mode);

    /// Reset every mode to Thetis defaults.
    /// Clears all filter override AppSettings keys.
    /// Emits presetsChanged for every mode.
    void resetAll();

signals:
    /// Emitted on any mutation for the affected mode.
    /// RxApplet and VfoWidget subscribe and call rebuildFilterButtons.
    void presetsChanged(NereusSDR::DSPMode mode);

private:
    // Returns the AppSettings key prefix for a mode+slot, e.g.
    // "filters/USB/3"
    static QString slotKey(DSPMode mode, int slot);

    // Returns the AppSettings key prefix for all slots in a mode, e.g.
    // "filters/USB"
    static QString modePrefix(DSPMode mode);

    // Persist a single preset at the given slot.
    static void persistPreset(DSPMode mode, int slot, const FilterPreset& p);

    // Remove the AppSettings keys for a single slot.
    static void clearSlot(DSPMode mode, int slot);

    // Remove all AppSettings keys for an entire mode.
    static void clearMode(DSPMode mode);

    // Load a single preset from AppSettings (returns std::nullopt when no
    // override exists for that slot).
    static std::optional<FilterPreset> loadSlot(DSPMode mode, int slot);
};

} // namespace NereusSDR
