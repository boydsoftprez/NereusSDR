// no-port-check: NereusSDR-original.
// =================================================================
// src/core/WdspThreadCheck.h  (NereusSDR)
// =================================================================
// Counts WDSP calls made from the event loop (R-R3-39). Once the DSP control
// lanes (DspControlThread) carry every WDSP control call, the count stays
// at zero; a call still made from the event loop raises it, so a test can
// catch one.
//
// It installs WDSP's caller check (WDSPSetCallerCheckHook, dsplock.c), which
// every WDSP lock entry, channel teardown wait, OpenChannel and
// SetChannelState calls first, on the calling thread. The hook compares the
// calling thread with the event loop's and counts a match; it never blocks
// and never calls into WDSP.
//
// Compiled only into debug and test builds (kWdspThreadCheckCompiled). In a
// release build install() does nothing, no hook is ever installed, and WDSP
// pays one pointer test per checked call.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-39).
// =================================================================

#pragma once

#include <QtGlobal>

class QThread;

namespace NereusSDR {

#if defined(NEREUS_BUILD_TESTS) || !defined(NDEBUG)
inline constexpr bool kWdspThreadCheckCompiled = true;
#else
inline constexpr bool kWdspThreadCheckCompiled = false;
#endif

namespace WdspThreadCheck {

/// Starts counting WDSP calls made on `eventLoop`'s thread, from zero.
/// Called on that thread, counting starts at once; from another thread it
/// starts when `eventLoop` next runs its event loop. Does nothing in a
/// release build.
void install(const QThread* eventLoop);

/// Removes the check. The count keeps its value.
void uninstall();

/// WDSP calls counted on the event loop since install() (0 in a release
/// build).
quint64 eventLoopEntries() noexcept;

} // namespace WdspThreadCheck

} // namespace NereusSDR
