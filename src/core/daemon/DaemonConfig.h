#pragma once
// =================================================================
// src/core/daemon/DaemonConfig.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R1 Task 9
// (docs/architecture/2026-08-02-remote-daemon-r1-plan.md, Task 9): the
// headless nereusd daemon's own configuration, read from a plain
// "key = value" text file (default /etc/nereusd.conf, overridable with
// nereusd --config <path>) instead of AppSettings' XML store. A Pi-hosted
// systemd service wants a single flat file an operator can hand-edit and a
// package can drop a default copy of, not the GUI client's per-user
// ~/.config/NereusSDR/NereusSDR.settings.
//
// On-disk format: "key = value" lines. '#' starts a comment, whether it is
// the whole line or trails a value; blank lines are ignored; leading and
// trailing whitespace around both key and value is trimmed. Unknown keys
// log a warning (via LogCategories' lcApp) and are otherwise ignored --
// never a hard failure -- so a config file written for a newer nereusd
// still starts an older one instead of refusing to boot. A malformed value
// for a known numeric key (sample_rate_hz, slice_count) is likewise logged
// and the field is left at whatever it already was, rather than being
// clobbered with 0.
//
// sliceCount's further clamp to the connected board's
// BoardCapabilities::maxSlices happens once a radio is actually discovered
// (R1 Task 10, DaemonApp): this struct is parsed before any radio is
// contacted, radioMac may be empty (meaning "first responder", so the
// board is not even known yet), and BoardCapabilities' maxSlices is a
// per-SKU field (2 to 5 across the current board table) with no
// board-independent ceiling to check here. validate() below therefore only
// enforces the generic floor of 1.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QString>

namespace NereusSDR {

// Parsed, validated configuration for one nereusd process. See the file
// header above for the on-disk format and the design rationale.
struct DaemonConfig {
    QString radioMac;                          // empty = first discovered
    int     sampleRateHz {192000};
    int     sliceCount   {1};                  // see header comment: the
                                                // board-specific ceiling is
                                                // applied later, by R1 Task 10
    QString audioDevice;                       // empty = no local audio
    QString logLevel     {QStringLiteral("info")};

    // Reads and parses `path`. If the file cannot be opened, returns
    // defaults() with *errorOut set to a human-readable message describing
    // why; the caller decides whether that is fatal (src/server_main.cpp
    // logs it as a warning and continues with defaults -- a missing config
    // is not by itself a startup error, since a bare `nereusd` invocation
    // for local testing should still come up with sane values). On a
    // successful open, *errorOut is cleared, even if individual lines
    // inside the file were skipped with a logged warning.
    static DaemonConfig fromFile(const QString& path, QString* errorOut);

    // The struct's own default member initializers, as a value. Always
    // passes validate().
    static DaemonConfig defaults();

    // Generic sanity checks only; see the sliceCount comment above for why
    // the board-specific ceiling lives elsewhere. Returns false and fills
    // *errorOut with a human-readable reason on the first check that
    // fails; *errorOut is cleared on success.
    bool validate(QString* errorOut) const;
};

} // namespace NereusSDR
