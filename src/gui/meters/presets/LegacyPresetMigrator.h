// =================================================================
// src/gui/meters/presets/LegacyPresetMigrator.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original — Task 18 of the Edit Container refactor.
// Detects legacy ItemGroup-expanded primitive signatures emitted by
// pre-refactor NereusSDR saved containers and collapses them into
// the new first-class MeterItem subclasses (AnanMultiMeterItem,
// CrossNeedleItem, SMeterPresetItem, PowerSwrPresetItem,
// MagicEyePresetItem, SignalTextPresetItem, HistoryGraphPresetItem,
// VfoDisplayPresetItem, ClockPresetItem, ContestPresetItem,
// BarPresetItem). No Thetis port here: the migrator only interprets
// the legacy wire format that NereusSDR's own `MeterItem::serialize()`
// emits, and synthesises the new first-class classes from the
// primitive state. Tolerant by design — anything that doesn't match
// a known signature passes through unchanged.
//
// Modification history (NereusSDR):
//   2026-04-19 — Authored for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude
//                 Opus 4.7.
// =================================================================

#pragma once

#include <QVector>

namespace NereusSDR {

class MeterItem;

// Tolerant legacy-preset migrator.
//
// Consumes a vector of already-deserialised legacy primitive
// MeterItems (the pipe-delimited format) and emits a new vector
// where any region matching a known preset signature is collapsed
// into the corresponding first-class preset class.
//
// Ownership: migrate() TAKES OWNERSHIP of every MeterItem pointer
// in the input vector. On return, any collapsed (consumed) legacy
// items have been deleted; the items in the returned Result vector
// are either freshly-constructed preset instances or pass-through
// legacy items. The caller then owns everything in Result.
class LegacyPresetMigrator
{
public:
    struct Result {
        // Final item set after migration — mixture of freshly-
        // constructed first-class preset instances and pass-through
        // primitives. Caller takes ownership of every pointer here.
        QVector<MeterItem*> upgradedItems;

        // Count of full-signature collapses (one preset emitted per
        // count). Used by the migration test and by a startup log
        // line so users see what got upgraded.
        int presetsMigrated{0};

        // Count of signatures that matched the shape but bailed out
        // because one or more constituent items had customisation
        // outside what the new first-class class can represent.
        // Those regions pass through as loose legacy items (no data
        // loss). Used by the migration test.
        int fallbackCases{0};
    };

    // Walk legacyItems left-to-right, detect preset signatures, and
    // collapse matched regions. See header comment for ownership.
    static Result migrate(QVector<MeterItem*> legacyItems);
};

} // namespace NereusSDR
