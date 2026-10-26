// no-port-check: NereusSDR-original migration of saved container entries.
// Modification history (NereusSDR):
//   2026-10-25 - Written: a Longpath needle or bar's "direction" becomes
//                 "binding" on load. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include "ContentEntryMigration.h"

#include "gui/instruments/LongpathContent.h"

namespace NereusSDR {

namespace {

void migrateLongpathDirection(ContentEntry& entry)
{
    if (entry.typeId != QLatin1String(LongpathContent::kNeedleType)
        && entry.typeId != QLatin1String(LongpathContent::kBarType)) {
        return;
    }
    const QString directionKey = QString::fromLatin1(LongpathContent::kDirectionKey);
    if (!entry.config.contains(directionKey)) {
        return;
    }
    const QString bindingKey = QString::fromLatin1(LongpathContent::kBindingKey);
    if (!entry.config.contains(bindingKey)) {
        entry.config.insert(bindingKey, LongpathContent::primaryBinding(entry.typeId, entry.config));
    }
    entry.config.remove(directionKey);
}

} // namespace

void migrateEntryOnLoad(ContentEntry& entry)
{
    migrateLongpathDirection(entry);
}

} // namespace NereusSDR
