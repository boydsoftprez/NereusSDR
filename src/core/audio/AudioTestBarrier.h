// =================================================================
// src/core/audio/AudioTestBarrier.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The no-device rule for test runs
// (R-AUD-32).
//
// True in a test build whose process runs in Qt's test mode (every test
// binary does, through TestSandboxInit.cpp).  Every audio engine refuses
// to open a real device while it is true, so no test reaches a sound
// device on any engine; tests install fakes instead.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-32). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

namespace NereusSDR {

bool audioDevicesBarredForTestRun();

} // namespace NereusSDR
