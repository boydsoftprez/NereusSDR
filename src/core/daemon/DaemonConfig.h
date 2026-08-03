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
//
// Every field here has a production consumer. An earlier revision shipped
// a `logLevel` field that nothing read, alongside a nereusd.conf.sample
// documenting it, which is why the rule is now written down: a key that
// reaches this struct must reach the daemon's behaviour too, or it does
// not belong in the sample file. Log verbosity is Qt's own
// QT_LOGGING_RULES environment variable instead (verified to take
// precedence over the QLoggingCategory::setFilterRules() call
// LogManager makes), set from the systemd unit, which needs no field
// here and no code at all.
struct DaemonConfig {
    QString radioMac;                          // empty = first discovered
    int     sampleRateHz {192000};             // seeded into the per-MAC
                                                // AppSettings key the shared
                                                // connect path reads; see
                                                // DaemonApp::applyConfigToSettings
    int     sliceCount   {1};                  // see header comment: the
                                                // board-specific ceiling is
                                                // applied later, by R1 Task 10
    QString audioDevice;                       // empty = platform default

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

// Resolves nereusd's --profile command-line argument (R1 Task 9 fix round
// 1: this gap was flagged in Task 8's review, before Task 9 existed, as
// "Note for Task 9's daemon caller", but never reached this task's brief).
// Without it, every invocation of nereusd on a developer workstation reads
// and writes the SAME ~/.config/NereusSDR (or ~/Library/Preferences/
// NereusSDR on macOS) the real GUI client uses -- there is no isolation
// analogous to main.cpp's --profile, which is exactly the trap that
// produced task-9-report.md section 5.
//
// `requested` is the raw --profile value from QCommandLineParser (empty if
// the option was not given). An empty value always succeeds and resolves
// to an empty string, meaning "no profile, use the shared default
// directory" -- this is a deliberate default, not an oversight; nereusd's
// primary deployment (a single systemd-managed daemon on a Pi) has no
// other process contending for that directory.
//
// A non-empty value must pass AppSettings::isValidProfileName(); on
// failure, returns an empty string and *errorOut is set to a human-
// readable reason. The caller (src/server_main.cpp) treats a non-empty
// *errorOut as fatal and refuses to start, rather than silently falling
// back to the shared directory the way a mistyped GUI --profile does
// (main.cpp only warns and continues) -- a daemon provisioning mistake in
// a systemd unit file should be loud, not silently ignored.
//
// Pure function: does not call AppSettings::setProfileOverride() itself.
// The caller is responsible for that (and must do so before AppSettings::
// instance() is first touched anywhere in the process -- see
// server_main.cpp's own comment on why the ordinary post-QCoreApplication
// QCommandLineParser path is sufficient here, unlike main.cpp's pre-
// QApplication argv scan).
QString resolveDaemonProfileArgument(const QString& requested, QString* errorOut);

} // namespace NereusSDR
