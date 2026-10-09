// =================================================================
// src/core/audio/AudioTestBarrier.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AudioTestBarrier.h (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-32). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioTestBarrier.h"

#include <QStandardPaths>

namespace NereusSDR {

bool audioDevicesBarredForTestRun()
{
#ifdef NEREUS_BUILD_TESTS
    return QStandardPaths::isTestModeEnabled();
#else
    return false;
#endif
}

} // namespace NereusSDR
