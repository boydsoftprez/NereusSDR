#pragma once
// =================================================================
// src/core/security/TokenStore.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Remote Daemon R2, Task 18.
//
// Design source: docs/architecture/2026-07-28-remote-daemon-architecture-
// design.md section 7.1 "Session model":
//
//   "Authentication is a generated pre-shared token, never user-chosen,
//   rate-limited on failure. The token distribution mechanism must be
//   specified before R2: daemon console output on first run, plus the
//   desktop Setup toggle, alongside the TLS fingerprint (section 10.5)."
//
// This class owns the first half of that sentence: generate, persist,
// and verify. The console output on first run belongs to StationServer
// (src/core/session/StationServer.cpp), which prints this store's token
// beside CertificateStore::fingerprintSha256() the one time the token is
// freshly generated.
//
// ---- Why a file beside the daemon profile, not an AppSettings key ----
//
// A token in AppSettings would be a secret sitting in the same XML the
// operator backs up, mails to a maintainer with a bug report, and syncs
// between machines. It would also land inside the reach of
// AppSettings::allKeys(), which SettingsProxyServer::buildSnapshot()
// scans, so keeping it out of the store is one fewer way for it to
// escape even by accident. CertificateStore already writes the TLS key
// beside the daemon profile rather than into settings for the same
// reason; this class mirrors that shape deliberately, down to the
// constructor doing all the work synchronously and the caller checking
// isValid() right afterwards.
//
// ---- Never user-chosen ----
//
// There is no setToken(). An earlier Core generated its token as 256 bits
// from QRandomGenerator::system() (the OS CSPRNG), base64url without
// padding. Deleting the file no longer brings a fresh one: since iPhone
// app Task 12 a Core without a token uses paired devices only.
//
// ---- Rate limiting ----
//
// verify() counts CONSECUTIVE failures, per source address (LINK minor 5:
// one guesser's lockout must not refuse the operator from elsewhere). On reaching maxFailuresPerLockout()
// it refuses every further attempt -- including one carrying the correct
// token -- until lockoutMs() has elapsed since the most recent failure,
// then starts a fresh count. A success at any point resets the count to
// zero. RateLimited is a DISTINCT result from Rejected because the two
// mean different things to the caller: Rejected is "that token is wrong",
// RateLimited is "I am not answering that question right now", and a
// station that collapsed them would leak, through timing and through its
// own logs, exactly which guesses were close.
//
// The comparison itself is constant-time over the SHA-256 of both sides
// rather than over the raw strings: hashing first makes the loop length
// independent of the candidate's length, so a caller cannot learn the
// stored token's length by timing candidates of different sizes.
//
// ---- iPhone app Task 12: the token is what an upgraded Core has ----
//
// Paired devices, each with its own key (DeviceStore, DeviceAuthenticator),
// replace the token. A new Core creates NO token: this class only loads a
// token an earlier Core generated, so an upgraded Core stays claimed
// through it (DeviceStore::isClaimed()) and every window that still signs
// in with it keeps working, enrolling its own device key as it does, until
// the token is retired (retire(): the Remote Access page or the console).
// After that, and on a Core that never had one, there is no token to
// accept. Nothing in the Core generates one any more; the generation
// described above is how the token on an upgraded Core came to be.
//
// SCOPE BOUNDARY: this class loads, retires and checks one shared
// secret. It knows nothing about sockets, sessions, TLS, or who is
// asking. Pairing, rotation, and per-client tokens are later phases (see
// docs/architecture/2026-08-02-remote-station-identity-and-pairing-
// design.md); nothing here should grow toward them without that design
// being read first.
//
// AI tooling: Anthropic Claude Code.
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 12 (R-IOS-08): no token is generated any
//               more; isActive() and retire(). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: iPhone app Task 17: moveDamagedAside() for the console's
//               reset. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: LINK minor 5: the failure limiter is kept per source
//               address. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include <QElapsedTimer>
#include <QHash>
#include <QString>

namespace NereusSDR {

class TokenStore {
public:
    enum class VerifyResult {
        Accepted,     // matches the stored token
        Rejected,     // does not match
        RateLimited,  // too many consecutive failures; not even checked
    };

    // Consecutive failures tolerated before verify() starts returning
    // RateLimited, and how long the refusal lasts. Chosen so a human
    // fat-fingering a paste gets several tries, while an automated
    // guesser is held to roughly five attempts per minute against a
    // 256-bit secret. Both are overridable via setRateLimit() -- the
    // tests drive them down to single-digit milliseconds so a rate-limit
    // case does not cost a minute of wall clock.
    static constexpr int kDefaultMaxFailuresPerLockout = 5;
    static constexpr int kDefaultLockoutMs = 60000;

    // directory is where the token file lives; it is created (mkpath,
    // recursively) if it does not exist. The token is LOADED from there
    // when an earlier Core left one; none is ever generated (see the header
    // comment). Defaults to the daemon
    // profile's own config directory, the same place CertificateStore
    // keeps tls-cert.pem / tls-key.pem. Tests pass an explicit scratch
    // directory so a run never reads back, or overwrites, a real
    // station's token.
    explicit TokenStore(const QString& directory = defaultDirectory());

    // Where the production token lives: the config directory of the
    // profile this process is actually running under, resolved through
    // AppSettings::resolveConfigDir() rather than rebuilt by hand. Follows
    // AppSettings::profileOverride() for the same reason
    // CertificateStore::defaultDirectory() does, and that header carries
    // the full rationale: hardcoding the daemon profile made two --profile
    // instances share one token, and a simultaneous first run let the
    // loser of a rename race accept a token that was not the one on disk.
    static QString defaultDirectory();

    // False means lastError() names why: the directory could not be
    // created or a token file exists but could not be read. A Core with no
    // token file at all is valid and simply not active.
    bool isValid() const { return m_valid; }

    // Empty when isValid() is true.
    QString lastError() const { return m_lastError; }

    // Empty unless isActive().
    QString token() const { return m_token; }

    // iPhone app Task 12: true while a token is loaded and not retired, so
    // a window can still sign in with it (DeviceStore::isClaimed()).
    bool isActive() const { return m_valid && !m_token.isEmpty(); }

    // iPhone app Task 12: retires the token for good. The file is deleted
    // and verify() refuses every candidate from then on. False (and the
    // token stays active) when the file could not be deleted. Retiring a
    // Core with no token is a success that changes nothing.
    bool retire();

    // iPhone app Task 17 (R-IOS-08): the console's `reset --unclaimed`. A
    // token file that exists but could not be read keeps the Core claimed
    // (DeviceStore::isClaimed()); this renames it to
    // `station-token.damaged-<UTC time>` (never deleted) and leaves the
    // store valid with no token. True when the store is valid afterwards;
    // nothing to do on a valid store.
    bool moveDamagedAside();

    // Absolute path this instance loads from / writes to. Always
    // populated, derived from the constructor's directory argument,
    // regardless of isValid().
    QString tokenPath() const { return m_tokenPath; }

    // See the class comment for the three results and the constant-time
    // comparison. Not const: it maintains the failure counter.
    // LINK minor 5: failures are counted per `source` (the peer's address;
    // empty over the relay), so one guesser's lockout does not refuse the
    // operator's token from another address.
    VerifyResult verify(const QString& candidate, const QString& source = QString());

    // Overrides the two constants above. A maxFailures below 1 is
    // clamped to 1 (zero would mean "rate-limited before the first
    // attempt", which would lock the station out of itself); a negative
    // lockout is clamped to 0.
    void setRateLimit(int maxFailures, int lockoutMs);

    int maxFailuresPerLockout() const { return m_maxFailures; }
    int lockoutMs() const { return m_lockoutMs; }

    // Consecutive failures since the last success (or since the last
    // lockout expired). Diagnostics and tests.
    int consecutiveFailures(const QString& source = QString()) const;

    // True while verify() would return RateLimited for `source` without
    // checking.
    bool isRateLimited(const QString& source = QString()) const;

    // How many sources the limiter remembers. Past it, sources that are
    // not refused are forgotten; a refusal in force never is.
    static constexpr int kMaxTrackedSources = 4096;

private:
    bool loadExisting();

    QString m_directory;
    QString m_tokenPath;
    bool    m_valid{false};
    QString m_lastError;
    QString m_token;

    int m_maxFailures{kDefaultMaxFailuresPerLockout};
    int m_lockoutMs{kDefaultLockoutMs};
    struct SourceFailures {
        int consecutive{0};
        // Restarted on every failure. Invalid (never started) until the
        // first one, which is what isRateLimited() checks before reading
        // it.
        QElapsedTimer sinceLastFailure;
    };
    void prune();

    QHash<QString, SourceFailures> m_failures;
};

} // namespace NereusSDR
