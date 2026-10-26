// no-port-check: NereusSDR-original migration of saved container entries.
// Modification history (NereusSDR):
//   2026-10-25 - Written: a Longpath needle or bar's "direction" becomes
//                 "binding" on load. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#pragma once

#include "ContainerDocument.h"

namespace NereusSDR {

// Brings one decoded entry's config up to the keys this build reads.
// ContainerDocumentCodec's decodeEntry calls it on every entry it decodes,
// so the workspace store, profiles, imports and duplicates all load
// through it. Idempotent; an entry of a type it does not know is left as
// it is.
//
// longpath.needle and longpath.bar: "direction" (no editor offers it) is
// replaced by the "binding" it meant (LongpathContent::primaryBinding)
// when the entry has no "binding", and dropped when it has one.
void migrateEntryOnLoad(ContentEntry& entry);

} // namespace NereusSDR
