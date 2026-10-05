// =================================================================
// src/core/ReceiveLayoutStore.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original Core receive-layout persistence adapter.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-22: Original implementation for R-R3-34 by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Codex.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02, ruling 5.3): each entry
//               keeps its owner or the device it is held for. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/WdspTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

class AppSettings;

/// Stable, data-only receive state owned by the Core restart manifest.
struct ReceiveSliceState {
    int id {0};
    QString panKey;
    double frequencyHz {0.0};
    DSPMode dspMode {DSPMode::USB};
    /// iPhone app Task 73 (ruling 5.3): whose the slice is
    /// (SliceOwnership's owner: a paired device's raw id, or the station
    /// device), empty for none; and, when the station device runs it for
    /// an absent device, that device. Stored as the device's base64url id
    /// ("station" for the station device) and only when set, so a layout
    /// with no owners is written exactly as before. A window signed in
    /// with the older token is written as no owner: it cannot be
    /// recognised when it comes back. An entry without either restores
    /// with no owner.
    QByteArray owner;
    QByteArray heldFor;
};

/// Per-radio persistence adapter for the bounded receive-layout manifest.
///
/// This class only validates and stages AppSettings values. It deliberately
/// does not construct a SliceModel, allocate a DDC, start DSP, or write disk.
class ReceiveLayoutStore final {
public:
    enum class LoadState {
        Missing,
        Loaded,
        InvalidIdentity,
        InvalidData,
    };

    struct LoadResult {
        LoadState state {LoadState::Missing};
        QList<ReceiveSliceState> slices;
        QString error;
        // Decoded receive-audio owner only; never a TX or active-slice choice.
        std::optional<int> radeRxOwnerId{std::nullopt};
        std::optional<int> diversityOwnerId{std::nullopt};
    };

    /// Decode a per-MAC receive layout. Invalid stored bytes are retained.
    static LoadResult load(const AppSettings& settings, const QString& mac);

    /// Validate then replace the in-memory manifest. Does not call save().
    /// Empty pan keys are normalized to pan-0 before validation/serialization.
    /// radeRxOwnerId is the decoded receive-audio owner, never TX/active state.
    static bool stage(AppSettings& settings, const QString& mac,
                      const QList<ReceiveSliceState>& slices,
                      QString* error = nullptr,
                      std::optional<int> radeRxOwnerId = std::nullopt,
                      std::optional<int> diversityOwnerId = std::nullopt);

    /// Slice control plan Task 7 (ruling Q10): removes the stored layout for
    /// `mac`, so the next start has none (a Core left with no slice).
    /// Does not call save().
    static void forget(AppSettings& settings, const QString& mac);

    /// Validate already-decoded descriptor state and receive-audio ownership.
    static bool validate(const QList<ReceiveSliceState>& slices,
                         QString* error = nullptr,
                         std::optional<int> radeRxOwnerId = std::nullopt);
};

} // namespace NereusSDR
