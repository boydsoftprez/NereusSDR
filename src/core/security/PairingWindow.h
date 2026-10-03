#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/PairingWindow.h  (NereusSDR)
// =================================================================
//
// When the Core accepts a new device, and the code it accepts it with
// (iPhone app plan Task 14, R-IOS-08; the pairing design,
// docs/architecture/2026-08-02-remote-station-identity-and-pairing-design.md
// section 4.5).
//
// Four states:
//
//   OpenUnclaimed  the Core has no paired device and no pairing token
//                  (DeviceStore::isClaimed() false). Open with NO timer
//                  (but with the attempt ceiling below):
//                  an unclaimed Core holds nothing worth taking, and a
//                  timer would expire while the operator fetches a phone.
//                  One tap on the Core's own network pairs, and so does
//                  the code from anywhere.
//   ClosedClaimed  the Core is claimed and the window is shut: no pairing
//                  of any kind.
//   OpenReopened   the operator reopened it, from the Core's console or
//                  from a paired device (`pairing.open`): the code pairs
//                  (not one tap); it closes after one successful pairing,
//                  close() or `pairing.close`, or kReopenedLifetimeMs
//                  (10 minutes) after it opened.
//   ClosedUnclaimed an unclaimed window the attempt ceiling closed. Only
//                  the console's reopen() opens it again.
//
// The attempt ceiling (fix wave R1-I2): kMaxConsecutiveFailures (5)
// burned codes in a row close any open window, reopened or unclaimed, and
// reopening starts afresh (no failures, no wait). It counts only codes
// burned on a direct connection (Route::Direct).
//
// Codes burned through the remote access service (Route::Service, a
// pairing mailbox; the operator's ruling of 2026-09-26 on iPhone plan Task
// 27's review item I5) never close the window: an unclaimed Core claims the
// lowest free nameplate there, so anyone could otherwise shut a fresh
// Core's pairing from the internet. Instead, kMaxConsecutiveFailures of
// them in a row pause pairing through the service: 1 minute the first
// time, doubling each time it is hit again with no pairing in between, at
// most 60 minutes (kFirstServicePauseMs, kMaxServicePauseMs). While paused,
// a pairing through the service is refused before it takes a code, so it
// burns nothing; pairing on a direct connection (the home network) stays
// open throughout. A pairing, or reopening the window, ends the pause and
// starts the ladder over. A burned code rotates either way, after the same
// wait, except the burn that starts a pause: its next code follows after
// the first wait (kFirstRetryMs), since only the home network can take it
// (the follow-up to Task 27's re-review, new Minor 4).
//
// LINK-I4 (JJ's ruling, 2026-09-30): the pauses slow guessing through
// the service but never end it, so the codes burned through it are also
// counted over the window's whole life. At kMaxServiceFailuresTotal (20)
// pairing through the service shuts: every attempt through it is refused
// with no retry time, until the Core's own console or window reopens
// pairing (reopenAtCore()) or a device pairs. A pause, a paired device's
// `pairing.open` and the window closing by itself leave the count alone.
// Pairing on a direct connection is untouched while the window is open.
//
// The state follows the device store: the first pairing claims the Core
// and closes an unclaimed window; a Core reset to unclaimed from its
// console opens again (devices.revoke never removes the last device while
// no token is active, so physical access is the only way back). The
// window's state is the Core's, independent of any connection.
//
// The code is single use. The exchange takes it (takeCode()) at the step
// where the Core first commits to it, so exactly one guess is made per
// code, even with several connections trying at once. A success claims
// (or closes); anything else burns it: the next code appears after 5 s,
// the wait doubling after each consecutive failure (up to 300 s, though
// the ceiling closes the window at the fifth), and a success resets it.
// retryAfterMs() says when.
//
// Time comes from an injected clock (milliseconds, monotonic); a
// single-shot timer calls poll() when a wait ends, and a test advances its
// clock and calls poll() itself.
//
// The code is a secret while it is live. Nothing logs or prints it; the
// Core's console gives it on request (`nereusd pairing show`), and the
// link carries it only to a connection signed in with a paired device's
// key.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I1): the last device is not
//               revoked while no token is active. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the pairing code is never printed
//               to standard output (the journal on a packaged Core). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08): the nameplate comes
//               from the remote access service while the Core is
//               registered. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Task 27 fix wave (I5, the operator's ruling): codes burned
//               through the service pause pairing through it (1 to 60
//               minutes) instead of closing the window. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Task 27 follow-up (new Minor 4): the burn that starts a
//               pause shows the next code after the first wait. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: Fix wave LINK-I4 (JJ's ruling): 20 codes burned through
//               the service in total shut pairing through it until the
//               Core reopens pairing or a device pairs. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix round 1 for LINK-I4: a constructor that keeps the
//               total of wrong codes through the service in AppSettings,
//               and its two keys. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QString>

#include <functional>

class QTimer;

namespace NereusSDR {

class AppSettings;
class DeviceStore;

class PairingWindow : public QObject {
    Q_OBJECT

public:
    enum class State { ClosedClaimed, OpenUnclaimed, OpenReopened, ClosedUnclaimed };
    Q_ENUM(State)

    using Clock = std::function<qint64()>;

    /// The wait before the next code after one failure, and its ceiling;
    /// it doubles after each consecutive failure between the two.
    static constexpr qint64 kFirstRetryMs = 5000;
    static constexpr qint64 kMaxRetryMs = 300000;
    /// The nameplate when the remote access service has supplied none: 1
    /// to this.
    static constexpr int kLocalNameplateMax = 99;
    /// Fix wave R1-I2 (the controller's ruling, 2026-09-24): consecutive
    /// burned codes that close any open window, and how long a reopened
    /// window stays open.
    static constexpr int kMaxConsecutiveFailures = 5;
    static constexpr qint64 kReopenedLifetimeMs = 10 * 60 * 1000;
    /// The operator's ruling on Task 27 item I5 (2026-09-26): how long
    /// pairing through the remote access service pauses after
    /// kMaxConsecutiveFailures codes burned through it in a row, doubling
    /// each time up to the ceiling.
    static constexpr qint64 kFirstServicePauseMs = 60 * 1000;
    static constexpr qint64 kMaxServicePauseMs = 60 * 60 * 1000;
    /// LINK-I4 (JJ's ruling, 2026-09-30; NereusSDR-original): codes burned
    /// through the service, in total since the last pairing or reopening
    /// at the Core, that shut pairing through the service.
    static constexpr int kMaxServiceFailuresTotal = 20;

    /// How a pairing reached the Core: on a direct connection, or through
    /// the remote access service's mailbox.
    enum class Route { Direct, Service };
    Q_ENUM(Route)

    /// LINK-I4 fix round 1 (NereusSDR-original): the AppSettings keys that
    /// keep the total count of wrong codes through the service, and whether
    /// it shut pairing through the service, across a restart of the run
    /// (a radio change) and of the Core. Cleared only by reopenAtCore() or
    /// a pairing.
    static constexpr const char* kServiceFailuresTotalKey = "PairingServiceFailuresTotal";
    static constexpr const char* kServiceShutKey = "PairingServiceShut";

    /// `devices` is not owned and must outlive this object.
    explicit PairingWindow(DeviceStore& devices, QObject* parent = nullptr);
    /// As above, and the total count of wrong codes through the service is
    /// read from `settings` and kept there. `settings` is not owned and
    /// must outlive this object.
    PairingWindow(DeviceStore& devices, AppSettings& settings, QObject* parent = nullptr);
    ~PairingWindow() override;

    State state() const { return m_state; }
    bool isOpen() const { return isOpenState(m_state); }

    /// Opens a closed window afresh (no failures, no wait) with a code: on
    /// a claimed Core OpenReopened, for kReopenedLifetimeMs, from the
    /// console or a paired device; on an unclaimed Core the attempt ceiling
    /// closed (ClosedUnclaimed) OpenUnclaimed again, from the console.
    /// Nothing to do when it is open.
    void reopen();
    /// LINK-I4: the Core's own console or window reopens pairing. As
    /// reopen(), and pairing through the service turns back on (its total
    /// count of wrong codes starts over), also while the window is open.
    void reopenAtCore();
    /// Closes a reopened window. An unclaimed Core's window stays open.
    void close();

    /// The code to pair with; empty while closed, while the last one is
    /// being tried, and while the wait after a failure runs.
    QString currentCode() const { return m_code; }

    /// The number in the code. While the Core is registered with the
    /// remote access service it is the nameplate the service gave it
    /// (iPhone app plan Task 27: DaemonApp::startRendezvous() claims one
    /// while the window is open and passes it here). A Core that has never
    /// held one shows a random number from 1 to kLocalNameplateMax, for
    /// pairing on this network or at a typed address; one that lost the
    /// service keeps the number it last held (the link document, section
    /// 3.6). A change makes a new code, unless the code is in use, when the
    /// next one carries it.
    void setNameplate(int nameplate);
    int nameplate() const { return m_nameplate; }

    // ── The exchange's accounting (StationServer) ─────────────────────

    /// Changes with every new code, so an exchange that started on one
    /// code cannot spend the next.
    quint64 codeSerial() const { return m_serial; }
    /// Spends the code of `serial`: true once, while it is still the
    /// current code and no other exchange holds it. From here it is
    /// either paired with or burned.
    bool takeCode(quint64 serial);
    /// A device was paired (by the code or by one tap): resets the wait
    /// and closes a reopened window. The device store's change closes an
    /// unclaimed one.
    void pairingSucceeded();
    /// The code taken was wrong, or its exchange ended without pairing:
    /// burned, and the next code waits. `route` decides what it counts
    /// toward: the ceiling that closes the window (Direct), or the pause of
    /// pairing through the service (Service).
    void pairingFailed(Route route = Route::Direct);
    /// True between takeCode() and the exchange's outcome.
    bool codeInUse() const { return m_codeInUse; }
    /// Part C fix wave (R1-M1): the exchange that took the code of `serial`
    /// may still pair with it: the window is open, the code is taken, and
    /// the window has not closed since (closing moves the serial). The
    /// exchange asks again at its confirm step.
    bool holdsCode(quint64 serial) const
    {
        return isOpen() && m_codeInUse && serial == m_serial;
    }
    /// Milliseconds until the next code appears: 0 when one is shown or
    /// the window is closed; kFirstRetryMs while another exchange holds
    /// the code. For Route::Service, at least until the pause ends.
    qint64 retryAfterMs(Route route = Route::Direct) const;
    /// Codes burned on a direct connection in a row (the ceiling's count).
    int consecutiveFailures() const { return m_failures; }
    /// Codes burned through the service in a row since the last pause.
    int consecutiveServiceFailures() const { return m_serviceFailures; }
    /// Whether a pairing by `route` is paused now: only Route::Service
    /// ever is, and only while the window is open.
    bool isPaused(Route route) const;
    /// Milliseconds until pairing through the service resumes; 0 when it
    /// is not paused.
    qint64 servicePauseRemainingMs() const;
    /// LINK-I4: codes burned through the service since the last pairing or
    /// reopening at the Core.
    int serviceFailuresTotal() const { return m_serviceFailuresTotal; }
    /// LINK-I4: pairing through the service is shut (the total reached
    /// kMaxServiceFailuresTotal) until reopenAtCore() or a pairing.
    bool isServiceShut() const { return m_serviceFailuresTotal >= kMaxServiceFailuresTotal; }

    void setClock(Clock clock);
    /// Re-checks the wait against the clock: a code whose wait has ended
    /// appears. The wait's timer calls it.
    void poll();

signals:
    void stateChanged(NereusSDR::PairingWindow::State state);
    /// The code changed; empty when there is none to show.
    void codeChanged(const QString& code);
    /// LINK-I4: pairing through the service shut, or turned back on.
    void serviceShutChanged(bool shut);

private:
    static bool isOpenState(State state);
    void followDevices();
    /// No failures counted and no wait (reopen() and a reset).
    void startAfresh();
    /// Pairing through the service unpaused, its ladder back to the start.
    void endServicePause();
    /// LINK-I4: the total count of codes burned through the service starts
    /// over (a pairing, or reopenAtCore()).
    void clearServiceTotal();
    /// LINK-I4 fix round 1: the total and the shut state, read from and
    /// written to m_settings (nothing without it).
    void loadServiceTotal();
    void storeServiceTotal();
    /// The attempt ceiling: closes the open window.
    void closeForCeiling();
    /// Sets the state and the code, then signals each that moved.
    void commit(State state, const QString& code);
    /// The code to show in `state`: the current one, a new one, or none
    /// (closed, in use, or waiting; the wait's timer is then armed).
    QString codeFor(State state);
    qint64 now() const;

    DeviceStore& m_devices;
    Clock m_clock;
    QTimer* m_wait = nullptr;
    QTimer* m_expiry = nullptr;
    State m_state = State::ClosedClaimed;
    QString m_code;
    quint64 m_serial = 0;
    int m_nameplate = 1;
    bool m_codeInUse = false;
    int m_failures = 0;
    qint64 m_nextCodeAt = 0;
    /// Pairing through the service (the ruling on Task 27 item I5): codes
    /// burned through it in a row since the last pause, how many pauses
    /// since the last pairing or reopening, and when the current one ends.
    int m_serviceFailures = 0;
    int m_servicePauses = 0;
    qint64 m_servicePausedUntil = 0;
    /// LINK-I4: codes burned through the service since the last pairing or
    /// reopening at the Core; never reset by a pause.
    int m_serviceFailuresTotal = 0;
    /// Where m_serviceFailuresTotal is kept; not owned, may be null.
    AppSettings* m_settings = nullptr;
    /// When a reopened window closes by itself; 0 for none.
    qint64 m_openUntil = 0;
};

} // namespace NereusSDR
