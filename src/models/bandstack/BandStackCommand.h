#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/BandStackCommand.h  (NereusSDR)
// =================================================================
//
// The bandStack.* verbs as data (design section 6.3), one builder per verb, so a
// remote window and the hosting desktop send exactly the same arguments.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

#include "core/session/MirrorSchema.h"
#include "models/Band.h"
#include "models/bandstack/BandStackController.h"
#include "models/bandstack/BandStackStore.h"

namespace NereusSDR {

struct BandStackCommand {
    QString verb;
    QJsonObject arguments;

    /// `arguments` as the link carries them. Every value is text, a whole
    /// number or a flag.
    QList<MirrorUpdate> wireArguments() const;

    static BandStackCommand press(int sliceId, Band band, BandStackDirection direction,
                                  const BandStackView& view);
    static BandStackCommand step(int sliceId, BandStackDirection direction,
                                 const BandStackView& view);
    static BandStackCommand goTo(int sliceId, const QString& entryId, const BandStackView& view);
    static BandStackCommand add(int sliceId, const BandStackView& view);
    static BandStackCommand saveHere(int sliceId, const QString& entryId,
                                     const BandStackView& view);
    static BandStackCommand edit(const QString& entryId, const std::optional<QString>& name,
                                 std::optional<bool> locked);
    static BandStackCommand remove(const QString& entryId);
    static BandStackCommand reorder(Band band, const QStringList& ids);
    static BandStackCommand setReturn(Band band, BandStackReturnMode mode, const QString& entryId);
    static BandStackCommand clear(std::optional<Band> band);
    static BandStackCommand reseed();
    static BandStackCommand options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                                    std::optional<int> dwellSeconds);
    static BandStackCommand restoreExpired(const QString& batchId);
};

} // namespace NereusSDR
