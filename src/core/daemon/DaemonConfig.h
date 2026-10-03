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
// clobbered with 0. The optional display-limit pair is stricter: an explicit
// malformed or incomplete pair fails validate(), never disables enforcement.
//
// sliceCount's further clamp to the connected board's
// BoardCapabilities::maxSlices happens once a radio is actually discovered
// (R1 Task 10, DaemonApp): this struct is parsed before any radio is
// contacted, radioMac may be empty (parity Task 21: the Core then runs a
// radio chosen from an app, or the one radio it can see, or waits for a
// choice; so the board is not even known yet), and BoardCapabilities' maxSlices is a
// per-SKU field (2 to 5 across the current board table) with no
// board-independent ceiling to check here. validate() below therefore only
// enforces the generic floor of 1.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-23: thread_placement key (R-R3-41). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: display_adaptive key (R-R3-08, R-R3-37, R-R3-40). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 12 (R-IOS-08): the listener is on by
//               default (TCP 47910, every interface, IPv4 and IPv6) unless
//               the file sets remote_port or remote_bind; pairing_lan_click.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): remote_transmit. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24: iPhone app Task 17 (R-IOS-08): status_page, status_port
//               and state_directory. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08, R-IOS-16):
//               rendezvous_servers and relay. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Task 21 (R-IOS-18): radio_mac is second in the
//               Core's choice order (a radio chosen from an app first; the one
//               radio in sight only when exactly one is visible). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-27: R-R3-49: sample_rate_hz is a starting value only. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-27: R-R3-49 (remote-window parity Task 22): sourcePath, for the
//               Core's support bundle. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QHostAddress>
#include <QString>
#include <QStringList>
#include <optional>

#include "core/session/media/DisplayBudget.h"

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
    // Remote-window parity Task 22 (R-R3-49): the file these values came
    // from (fromFile's path), carried with secrets removed in the Core's
    // support bundle. Empty for defaults().
    QString sourcePath;
    QString radioMac;                          // empty = a radio chosen from
                                                // an app, else the one radio
                                                // in sight, else wait
                                                // (StationRadios::choose)
    int     sampleRateHz {192000};             // a starting value: seeded
                                                // into the per-MAC AppSettings
                                                // key the shared connect path
                                                // reads only when that radio
                                                // has no saved rate; see
                                                // DaemonApp::applyConfigToSettings
    // True only when sample_rate_hz was actually present in the config
    // file. sampleRateHz alone cannot express "unset", because validate()
    // rejects <= 0 and so the field must always hold a usable rate. Without
    // this flag, a bare `nereusd` with no config file (a non-fatal case:
    // server_main logs a warning and continues with defaults) would seed
    // the 192000 default into a radio the config file never mentioned.
    // Writing per-MAC settings is a side effect on shared user state, so it
    // happens only on an explicit request, and even then only to fill an
    // empty slot: a rate already saved for the radio always wins (R-R3-49).
    bool    sampleRateExplicit {false};
    int     sliceCount   {1};                  // see header comment: the
                                                // board-specific ceiling is
                                                // applied later, by R1 Task 10
    QString audioDevice;                       // empty = platform default;
                                               // seeds only when no speaker
                                               // choice is saved (G-16)

    // ── The wss control plane (R2 Task 18; defaults since iPhone app Task 12) ──
    //
    // The listener is ON by default: TCP 47910 on every interface, IPv4 and
    // IPv6 (remoteBind empty; DaemonApp binds QHostAddress::Any, which is
    // dual stack). Pairing is what makes that safe to expose: since iPhone
    // app Task 12 a Core admits only paired devices (and, on a Core
    // upgraded from before pairing, windows holding its token), and the
    // pairing design's first-run workflow (section 11) needs the Core
    // reachable with no configuration at all.
    //
    // A file that sets remote_port or remote_bind keeps exactly what it
    // meant before this default changed: the key it leaves out takes the
    // earlier default (port 0, off; bind 127.0.0.1, this machine only).
    // So a Core configured by hand, like the Rock with its explicit
    // remote_port, listens where it always did. remote_port = 0 still turns
    // the listener (and with it the LAN announcement) off.
    //
    // Both feed StationServer::listen() from DaemonApp::start(); see
    // packaging/nereusd.conf.sample, and note that a key reaching this
    // struct must reach behaviour AND the sample, which
    // tst_daemon_config's sampleFileKeysAndParserKeysAgree pins.
    static constexpr int kDefaultRemotePort = 47910;
    /// What a file that sets one of remote_port / remote_bind gets for the
    /// other: the defaults from before iPhone app Task 12.
    static constexpr int kExplicitConfigRemotePort = 0;
    static constexpr const char* kExplicitConfigRemoteBind = "127.0.0.1";
    QString coreName;                         // empty = machine hostname for LAN discovery
    int     remotePort {kDefaultRemotePort};
    QString remoteBind;                       // empty = every interface, IPv4 and IPv6

    // iPhone app Task 12 (R-IOS-08; pairing design section 4.2): whether a
    // device on this Core's own network may pair with one tap while the
    // Core is unclaimed. nereusd.conf `pairing_lan_click = allow|deny`,
    // default allow; deny makes every pairing use the code. Any other value
    // logs one warning and keeps allow. Feeds
    // StationServer::setPairingLanClickAllowed() from
    // DaemonApp::startStationServer(); the pairing window (Task 14) reads it.
    bool    pairingLanClickAllowed {true};

    // iPhone app plan Task 27 (R-IOS-08, R-IOS-16; the pairing design,
    // section 5.3): the remote access service (the rendezvous) this Core
    // registers with, so a paired device can reach it from anywhere and
    // pair by code through it. nereusd.conf `rendezvous_servers`: an ordered
    // list, separated by spaces or commas, the operator's own server first
    // and the default behind it; each a host name, host:port, or a wss://
    // address (RendezvousClient::serverUrls). Default rv.nereussdr.com. An
    // empty value names none, and the Core then registers nowhere. Entries
    // that cannot be read log one warning each and are skipped. Feeds
    // RendezvousClient from DaemonApp::startStationServer().
    QStringList rendezvousServers {QStringLiteral("rv.nereussdr.com")};

    // iPhone app plan Task 27 (the pairing design, section 5.4, "Switchable
    // off"): whether this Core uses the relay when a direct path fails.
    // nereusd.conf `relay = allow|deny`, default allow. deny asks the
    // service for no relay credentials and refuses the far end's relay
    // addresses, so every connection is direct or nothing. Any other value
    // logs one warning and keeps allow. Feeds RendezvousClient::
    // setRelayAllowed() from DaemonApp::startStationServer().
    bool    relayAllowed {true};

    // iPhone app plan Task 34 (R-IOS-02): whether devices may transmit
    // through this Core. nereusd.conf `remote_transmit = allow|deny`,
    // default allow: each session is then permitted by the station transmit
    // gate (a paired device whose app declares remote transmit, once
    // connected, while no other device holds transmit). deny keeps the Core
    // receive-only, as before. Any other value logs one warning and denies:
    // a transmit setting fails closed. Feeds StationServer::
    // setRemoteTransmitAllowed() and RadioModel::setReceiveOnlyStationPolicy()
    // from DaemonApp.
    bool    remoteTransmitAllowed {true};

    // iPhone app Task 17 (R-IOS-08; pairing design section 4.3): the Core's
    // small read-only status page. nereusd.conf `status_page = on|off`
    // (default on; anything else logs one warning and keeps on) and
    // `status_port` (default 47911). It runs beside the remote listener,
    // bound where the listener binds (DaemonApp::listenerAddressFor), and
    // answers only peers on this computer's directly connected networks
    // (StationServer::isOnDirectNetwork). Feeds StationStatusPage from
    // DaemonApp::startStationServer().
    static constexpr int kDefaultStatusPort = 47911;
    bool    statusPage {true};
    int     statusPort {kDefaultStatusPort};

    // iPhone app Task 17: where the console's control socket
    // (`nereusd-control`) lives. Empty (the default) is the profile's own
    // directory (AppSettings::resolveConfigDir). A packaged Core sets its
    // systemd StateDirectory here (/var/lib/nereusd in the shipped sample),
    // so `sudo nereusd status` finds the socket through the default
    // --config. Without it a console command also looks in a packaged
    // Core's HOME (StationControlSocket::candidatePathsFor), since under
    // sudo its own $HOME is root's. Only the socket moves; the identity
    // key and the device list stay in the profile's directory. Must be an
    // absolute path when set. Read by StationControlSocket::socketPathFor()
    // on both sides.
    QString stateDirectory;

    // R-R3-22 / R-R3-47 / R-R3-48: the station network, where every station
    // listener accepts connections: the SmartSDR API listener on TCP 4992,
    // the Power Genius and Tuner Genius discovery on UDP 9008 and 9010, and
    // the station TCI server (StationNetwork::StationBind). Empty: this
    // computer's address on the radio's subnet, found once the radio
    // connects (only this computer before then). Every listener also
    // accepts this computer. `station_bind` in the file; the older name
    // `station_tci_bind` (Task 3 of the core-owned accessories plan) is
    // still read when `station_bind` is empty or absent. Feeds
    // RadioModel::setStationBind() and enableStationTci() from DaemonApp.
    QString stationBind;

    // R-R3-23: the Opus encoder target for station audio, bit/s. Only the
    // two supported profiles are accepted (OpusAudioEncoder refuses any
    // other): 24000 codes wideband sound up to 8 kHz; 48000 codes fullband
    // sound up to 20 kHz (bandwidthForBitrate()). R-R3-21: 48000 is the
    // default for every mode (operator decision 2026-09-26); 24000 set
    // explicitly keeps working. Anything else in the file logs one warning
    // and keeps 48000. Feeds DaemonMediaController::setAudioTargetBitrate()
    // from DaemonApp::startStationServer(), which logs the one in use.
    static constexpr int kWidebandAudioBitrate = 24000;
    static constexpr int kFullbandAudioBitrate = 48000;
    static constexpr int kDefaultAudioBitrate = kFullbandAudioBitrate;
    int     audioBitrate {kDefaultAudioBitrate};

    // R-R3-41: thread_placement = auto (default) runs each busy signal
    // processing thread on a fast core of its own, chosen from the kernel's
    // CPU capacity data, and keeps every other thread off those cores
    // (src/core/platform/ThreadPlacement.h, Linux only). off leaves every
    // thread free to run on any core. Anything else logs one warning and
    // keeps auto. Feeds startDaemonThreadPlacement() in server_main.cpp.
    bool    threadPlacement {true};

    // R-R3-08/37/40: display_adaptive = on (default) lets the Core lower
    // spectrum quality, background pans first, while its computer is busy,
    // and restore it when there is room again (DisplayLoadGovernor, owned by
    // DaemonApp). With it on, the Core always advertises a display budget:
    // the configured pair below, or a ceiling computed for eight pans at
    // their largest plus PureSignal. off keeps today's behaviour exactly.
    // Anything else logs one warning and keeps on.
    bool    displayAdaptive {true};

    // R-R3-23: whether a GUI may switch this Core's audio to the lossless
    // profile (uncompressed 16-bit stereo, about 1.54 Mbit/s of audio, for
    // digital modes). nereusd.conf `audio_lossless = allow|deny`, default
    // allow. Any other value logs one warning and keeps allow. With deny a
    // request is refused and Opus keeps running. Feeds
    // DaemonMediaController::setAudioLosslessAllowed() from
    // DaemonApp::startStationServer().
    bool    audioLosslessAllowed {true};

    // Optional measured limits, supplied as a pair. A malformed explicit
    // value becomes zero so validate() fails instead of disabling the cap.
    std::optional<quint64> displayApplicationBytesPerSecond;
    std::optional<quint64> spectrumSampleUnitsPerSecond;
    std::optional<DisplayBudgetLimits> displayBudgetLimits() const;

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

    // The address DaemonApp gives StationServer::listen() for `bind`, the
    // remote_bind text. "::" means every address of both families: Qt binds
    // a parsed "::" IPv6-only (it sets IPV6_V6ONLY for an IPv6 address),
    // which refuses a LAN's IPv4 clients, so "::" maps to QHostAddress::Any,
    // Qt's dual-stack any-address. "0.0.0.0" stays IPv4-only and a specific
    // address stays that address. Null when `bind` is not an address.
    static QHostAddress listenAddressFor(const QString& bind);
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
// Remote Daemon R2, Task 1: the "share by default" behaviour this
// function used to implement was itself a trap one layer up. A daemon
// that silently reads and writes the GUI's own settings file lets R2's
// state-mirroring feature (SettingsProxy, R2 Task 15) pass its own
// verification while doing nothing at all -- a "remote" GUI would look
// correct because it was reading the SAME on-disk file the local daemon
// just wrote, not because anything was actually mirrored over the wire.
// See docs/architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md
// §2.1. The reserved profile below is the fix; sharing is now opt-in
// instead of the silent default.
//
// `requested` is the raw --profile value from QCommandLineParser (empty
// both when the option was not given and when it was given an empty
// value -- QCommandLineOption has no default here, so
// QCommandLineParser::value() cannot tell the two apart on its own).
// `wasSet` is parser.isSet(profileOpt):
//
//   wasSet == false             -> returns AppSettings::kDaemonProfileName
//     ("daemon"). No --profile on the command line at all now reserves
//     nereusd's own profile rather than silently sharing the GUI's.
//   wasSet == true, requested.isEmpty() -> returns an empty string,
//     meaning "share the GUI's own directory". Explicitly typing
//     --profile "" is the deliberate escape hatch for nereusd's primary
//     deployment (a single systemd-managed daemon on a Pi with no GUI to
//     collide with) -- opt-in now, not silent.
//   wasSet == true, requested non-empty -> validated against
//     AppSettings::isValidProfileName() as described below.
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
QString resolveDaemonProfileArgument(const QString& requested, bool wasSet,
                                     QString* errorOut);

} // namespace NereusSDR
