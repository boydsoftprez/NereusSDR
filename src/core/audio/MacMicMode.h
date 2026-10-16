// =================================================================
// src/core/audio/MacMicMode.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The Mac's side of MicModeKeeper:
// the system's Mic Mode calls, and the window's launch step that has the
// mic helper keep the window's stored Mic Mode at Standard before the
// window's first audio call.  Nothing off the Mac.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"
#include "core/audio/MicModeKeeper.h"

#include <QtGlobal>

namespace NereusSDR {

#ifdef Q_OS_MAC
// The system's calls; empty when this macOS has none.  Called only in the
// mic helper, which has no bundle id.
NEREUS_CORE_EXPORT MicModeCalls systemMicModeCalls();
// The window, before its first audio call: runs the mic helper once to
// keep this app's stored Mic Mode at Standard, waiting at most
// kMicModeWaitMs, and logs what it found.
NEREUS_CORE_EXPORT void keepWindowMicModeStandard();
#else
inline MicModeCalls systemMicModeCalls() { return {}; }
inline void keepWindowMicModeStandard() {}
#endif

} // namespace NereusSDR
