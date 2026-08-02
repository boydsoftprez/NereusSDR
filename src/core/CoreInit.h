// =================================================================
// src/core/CoreInit.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. R1 Task 8 (docs/architecture/2026-07-28-remote-daemon-
// architecture-design.md section 4.2, item 7): the settings-migration entry
// point and the file-logging setup both used to live inline in main(), each
// with exactly one call site. A daemon-first install (src/server_main.cpp,
// R1 Task 9) needs the identical sequence before it touches AppSettings, so
// both binaries now share it here instead of one binary running against an
// unmigrated settings store.
//
// No Thetis port at this layer: this only orchestrates AppSettings and
// LogManager calls that are already implemented (and, where ported,
// already attributed) elsewhere.
// no-port-check: NereusSDR-original orchestration.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-08-02: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code. Extracted from src/main.cpp (settings load,
//               one-shot migrations, LogManager restore, file-logging
//               install) so nereusd can run the identical startup sequence.
// =================================================================

#pragma once

#include <QString>

namespace NereusSDR {

// Shared startup sequence for every NereusSDR binary: the GUI's
// src/main.cpp today, and the headless nereusd daemon's src/server_main.cpp
// from R1 Task 9 onward. Loads AppSettings from disk, applies every
// one-shot settings-schema migration in order, restores LogManager's
// per-category enable/disable state, and installs file-backed logging
// (PII-redacted, rotated to the newest 5 files, symlinked as
// nereussdr.log) via qInstallMessageHandler.
//
// `profile` must be the SAME already-resolved --profile name the caller
// passed to AppSettings::setProfileOverride() before constructing its
// QApplication/QCoreApplication. initialize() does not call
// setProfileOverride() itself: that has to happen before the first
// AppSettings::instance() access anywhere in the process, which is a
// main()-entry concern, not this one. The only thing `profile` drives here
// is the log directory, via AppSettings::resolveConfigDir(profile), which
// is exactly what main.cpp computed inline before this task existed.
//
// Idempotent: a file-static flag guards the body, so every call after the
// first is a genuine no-op that returns true without touching AppSettings,
// the log file, or the message handler again.
namespace CoreInit {

bool initialize(const QString& profile = {});

// Tears down what initialize() installed: uninstalls the custom Qt message
// handler and closes the log file. Call once, near process exit, after the
// event loop returns; mirrors the two-line teardown that used to sit at
// the bottom of main(). No-op if initialize() was never called.
void shutdown();

#ifdef NEREUS_BUILD_TESTS
// Test-only hook, only compiled when NEREUS_BUILD_TESTS is defined. Number
// of times initialize()'s body has actually run, as opposed to been
// short-circuited by the idempotency guard. 0 before the first call, 1
// after, and pinned at 1 no matter how many more times initialize() is
// called. Exists so a test can prove the guard produced a genuine no-op
// rather than a harmless replay of migrations that each happen to be
// idempotent on their own. Not part of the public API.
int initializeRunCount();
#endif

} // namespace CoreInit
} // namespace NereusSDR
