#pragma once
// no-port-check: NereusSDR-original. Task 24 bounded Settings Hygiene reply.
// J.J. Boyd / KG4VCF, 2026-09-27. AI-assisted implementation via Codex.

#include "core/SettingsHygiene.h"
#include "core/session/MirrorSchema.h"

#include <QList>
#include <QString>
#include <QVector>
#include <optional>

namespace NereusSDR {

struct SettingsHygieneReply {
    QString mac;
    QVector<SettingsHygiene::Issue> issues;
};

class SettingsHygieneWire {
public:
    static std::optional<QList<MirrorUpdate>> encode(const SettingsHygieneReply& reply);
    static std::optional<SettingsHygieneReply> decode(const QList<MirrorUpdate>& values);
};

} // namespace NereusSDR
