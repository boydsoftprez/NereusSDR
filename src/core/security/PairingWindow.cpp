// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/PairingWindow.cpp  (NereusSDR)
// =================================================================
//
// See PairingWindow.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
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
//               Core reopens pairing (reopenAtCore) or a device pairs.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-30: Fix round 1 for LINK-I4: the total of wrong codes
//               through the service and the shut are kept in AppSettings
//               (PairingServiceFailuresTotal, PairingServiceShut), read
//               when the window is built and cleared only by
//               reopenAtCore() or a pairing. J.J. Boyd (KG4VCF), AI-
//               assisted via Anthropic Claude Code.
// =================================================================

#include "core/security/PairingWindow.h"

#include "core/AppSettings.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingCode.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcPairing, "nereussdr.pairing")

// A monotonic millisecond clock shared by every window that is not given
// one.
qint64 monotonicMs()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer.elapsed();
}
} // namespace

PairingWindow::PairingWindow(DeviceStore& devices, QObject* parent)
    : QObject(parent)
    , m_devices(devices)
    , m_clock(&monotonicMs)
    , m_nameplate(static_cast<int>(QRandomGenerator::system()->bounded(1, kLocalNameplateMax + 1)))
{
    m_wait = new QTimer(this);
    m_wait->setObjectName(QStringLiteral("pairingCodeWait"));
    m_wait->setSingleShot(true);
    connect(m_wait, &QTimer::timeout, this, &PairingWindow::poll);
    m_expiry = new QTimer(this);
    m_expiry->setObjectName(QStringLiteral("pairingWindowExpiry"));
    m_expiry->setSingleShot(true);
    connect(m_expiry, &QTimer::timeout, this, &PairingWindow::poll);
    connect(&m_devices, &DeviceStore::devicesChanged, this, &PairingWindow::followDevices);
    followDevices();
}

PairingWindow::PairingWindow(DeviceStore& devices, AppSettings& settings, QObject* parent)
    : PairingWindow(devices, parent)
{
    m_settings = &settings;
    loadServiceTotal();
}

PairingWindow::~PairingWindow() = default;

void PairingWindow::loadServiceTotal()
{
    if (m_settings == nullptr) {
        return;
    }
    bool ok = false;
    int total = m_settings->value(QLatin1String(kServiceFailuresTotalKey)).toString().toInt(&ok);
    if (!ok || total < 0) {
        total = 0;
    }
    total = std::min(total, kMaxServiceFailuresTotal);
    if (m_settings->value(QLatin1String(kServiceShutKey)).toString() == QLatin1String("True")) {
        total = kMaxServiceFailuresTotal;
    }
    m_serviceFailuresTotal = total;
    if (isServiceShut()) {
        qCInfo(lcPairing) << "Pairing from outside the Core's network stays off after"
                          << kMaxServiceFailuresTotal
                          << "wrong pairing codes; reopening pairing at the Core turns it "
                             "back on";
    }
}

void PairingWindow::storeServiceTotal()
{
    if (m_settings == nullptr) {
        return;
    }
    if (m_serviceFailuresTotal == 0) {
        m_settings->remove(QLatin1String(kServiceFailuresTotalKey));
        m_settings->remove(QLatin1String(kServiceShutKey));
    } else {
        m_settings->setValue(QLatin1String(kServiceFailuresTotalKey),
                             QString::number(m_serviceFailuresTotal));
        m_settings->setValue(QLatin1String(kServiceShutKey),
                             isServiceShut() ? QStringLiteral("True") : QStringLiteral("False"));
    }
    QString error;
    if (!m_settings->save(&error)) {
        qCWarning(lcPairing) << "Could not save the count of wrong pairing codes:" << error;
    }
}

qint64 PairingWindow::now() const
{
    return m_clock();
}

void PairingWindow::setClock(Clock clock)
{
    m_clock = clock ? std::move(clock) : Clock(&monotonicMs);
}

bool PairingWindow::isOpenState(State state)
{
    return state == State::OpenUnclaimed || state == State::OpenReopened;
}

void PairingWindow::startAfresh()
{
    m_failures = 0;
    m_nextCodeAt = 0;
    m_wait->stop();
    endServicePause();
}

void PairingWindow::endServicePause()
{
    m_serviceFailures = 0;
    m_servicePauses = 0;
    m_servicePausedUntil = 0;
}

void PairingWindow::followDevices()
{
    const bool claimed = m_devices.isClaimed();
    if (!claimed && (m_state == State::ClosedClaimed || m_state == State::OpenReopened)) {
        // A new Core, or one reset to unclaimed from its console (the only
        // way a claimed Core gets here): open, no timer, a fresh count. A
        // window the attempt ceiling closed (ClosedUnclaimed) stays shut
        // until the console reopens it.
        m_expiry->stop();
        startAfresh();
        commit(State::OpenUnclaimed, codeFor(State::OpenUnclaimed));
    } else if (claimed
               && (m_state == State::OpenUnclaimed || m_state == State::ClosedUnclaimed)) {
        // The first device claimed it.
        m_wait->stop();
        commit(State::ClosedClaimed, QString());
    }
}

void PairingWindow::commit(State state, const QString& code)
{
    // Both fields first, then the signals, so a listener reading the window
    // on the first signal sees the whole change: one change, not two.
    const bool stateMoved = m_state != state;
    const bool codeMoved = m_code != code;
    if (isOpenState(m_state) && !isOpenState(state)) {
        m_expiry->stop();
        m_openUntil = 0;
        // Part C fix wave (R1-M1): an exchange that took a code of this
        // window cannot pair with it once the window has closed, even if it
        // is reopened before that exchange's confirm step (holdsCode()).
        ++m_serial;
    }
    m_state = state;
    m_code = code;
    if (stateMoved) {
        qCInfo(lcPairing) << "Pairing window:" << state;
        emit stateChanged(state);
    }
    if (codeMoved) {
        // Never logged: the code is a secret while it is live.
        emit codeChanged(m_code);
    }
}

QString PairingWindow::codeFor(State state)
{
    if (!isOpenState(state) || m_codeInUse) {
        return {};
    }
    if (!m_code.isEmpty()) {
        return m_code;
    }
    const qint64 remaining = m_nextCodeAt - now();
    if (remaining > 0) {
        m_wait->start(static_cast<int>(std::min<qint64>(remaining, kMaxRetryMs)));
        return {};
    }
    m_wait->stop();
    const QString code = PairingCode::generate(m_nameplate);
    if (code.isEmpty()) {
        qCWarning(lcPairing) << "No pairing code could be made: the word list is missing";
        return {};
    }
    ++m_serial;
    return code;
}

void PairingWindow::reopen()
{
    if (isOpen()) {
        return;
    }
    // Whoever reopens it (the console, or a paired device on a claimed
    // Core) starts it afresh: no failures counted and no wait.
    startAfresh();
    if (m_state == State::ClosedUnclaimed) {
        // The attempt ceiling shut an unclaimed Core's window; only the
        // console reaches this (no device is paired). No timer, as before.
        commit(State::OpenUnclaimed, codeFor(State::OpenUnclaimed));
        return;
    }
    m_openUntil = now() + kReopenedLifetimeMs;
    m_expiry->start(static_cast<int>(kReopenedLifetimeMs));
    commit(State::OpenReopened, codeFor(State::OpenReopened));
}

void PairingWindow::reopenAtCore()
{
    // LINK-I4: only the Core's own console or window turns pairing through
    // the service back on; a paired device's `pairing.open` reaches
    // reopen() alone.
    clearServiceTotal();
    reopen();
}

void PairingWindow::clearServiceTotal()
{
    const bool wasShut = isServiceShut();
    const bool hadAny = m_serviceFailuresTotal != 0;
    m_serviceFailuresTotal = 0;
    if (hadAny) {
        storeServiceTotal();
    }
    if (wasShut) {
        qCInfo(lcPairing) << "Pairing from outside the Core's network is on again";
        emit serviceShutChanged(false);
    }
}

void PairingWindow::close()
{
    if (m_state != State::OpenReopened) {
        return;
    }
    m_wait->stop();
    commit(State::ClosedClaimed, QString());
}

void PairingWindow::closeForCeiling()
{
    m_wait->stop();
    const State closed =
        m_state == State::OpenUnclaimed ? State::ClosedUnclaimed : State::ClosedClaimed;
    qCInfo(lcPairing) << "Pairing closed after" << m_failures
                      << "wrong pairing codes in a row; it opens again only from the Core's "
                         "console or a paired device";
    commit(closed, QString());
}

void PairingWindow::setNameplate(int nameplate)
{
    if (nameplate < 1 || nameplate > PairingCode::kMaxNameplate || nameplate == m_nameplate) {
        return;
    }
    m_nameplate = nameplate;
    if (!m_code.isEmpty() && !m_codeInUse) {
        // The shown code carries the old number: make a new one.
        const QString old = m_code;
        m_code.clear();
        const QString next = codeFor(m_state);
        m_code = old;
        commit(m_state, next);
    }
}

bool PairingWindow::takeCode(quint64 serial)
{
    if (!isOpen() || m_codeInUse || m_code.isEmpty() || serial != m_serial) {
        return false;
    }
    m_codeInUse = true;
    commit(m_state, QString());
    return true;
}

void PairingWindow::pairingSucceeded()
{
    m_codeInUse = false;
    m_failures = 0;
    m_nextCodeAt = 0;
    m_wait->stop();
    // A pairing ends a pause of pairing through the service and starts its
    // ladder over (the ruling on Task 27 item I5).
    endServicePause();
    // LINK-I4: and starts the total count over.
    clearServiceTotal();
    if (m_state == State::OpenReopened) {
        // One device per reopening.
        commit(State::ClosedClaimed, QString());
        return;
    }
    followDevices();
    if (isOpen()) {
        commit(m_state, codeFor(m_state));
    }
}

void PairingWindow::pairingFailed(Route route)
{
    m_codeInUse = false;
    int streak = 0;
    bool pauseStarted = false;
    if (route == Route::Service) {
        // Through the service (the ruling on Task 27 item I5): never toward
        // the ceiling. The fifth in a row pauses pairing through the service
        // instead, for twice as long as the last pause, 1 to 60 minutes.
        streak = ++m_serviceFailures;
        // LINK-I4 (JJ's ruling, 2026-09-30): the total over the window's
        // life; at kMaxServiceFailuresTotal pairing through the service
        // shuts until the Core reopens it or a device pairs.
        // Fix round 1: kept in the Core's settings, so a restart of the run
        // or of the Core does not turn it back on.
        const bool wasShut = isServiceShut();
        if (!wasShut) {
            ++m_serviceFailuresTotal;
            storeServiceTotal();
        }
        if (!wasShut && isServiceShut()) {
            qCInfo(lcPairing) << "Pairing from outside the Core's network is off after"
                              << kMaxServiceFailuresTotal
                              << "wrong pairing codes; reopening pairing at the Core turns "
                                 "it back on";
            emit serviceShutChanged(true);
        }
        if (isOpen() && m_serviceFailures >= kMaxConsecutiveFailures) {
            const int doublings = std::min(m_servicePauses, 16);
            const qint64 pause =
                std::min<qint64>(kFirstServicePauseMs << doublings, kMaxServicePauseMs);
            ++m_servicePauses;
            m_serviceFailures = 0;
            m_servicePausedUntil = now() + pause;
            pauseStarted = true;
            qCInfo(lcPairing) << "Pairing from outside the Core's network paused for"
                              << pause / 1000 << "s after" << kMaxConsecutiveFailures
                              << "wrong pairing codes in a row";
        }
    } else {
        streak = ++m_failures;
        if (isOpen() && m_failures >= kMaxConsecutiveFailures) {
            // The attempt ceiling (fix wave R1-I2): the fifth burn in a row
            // on a direct connection closes the window, reopened or
            // unclaimed.
            closeForCeiling();
            return;
        }
    }
    // 5 s, 10 s, 20 s, 40 s (the ceiling closes the window before 80 s).
    // The burn through the service that starts a pause waits only the first
    // 5 s (the follow-up to the Task 27 re-review, new Minor 4): the service
    // is paused, so only the home network can take the next code, which the
    // pause never touches, and the first rung (1 minute) is not over before
    // a code is shown.
    const int doublings = std::min(streak - 1, 16);
    const qint64 wait = pauseStarted
        ? kFirstRetryMs
        : std::min<qint64>(kFirstRetryMs << doublings, kMaxRetryMs);
    m_nextCodeAt = now() + wait;
    qCInfo(lcPairing) << "Pairing failed; the next code follows in" << wait << "ms";
    commit(m_state, codeFor(m_state));
}

qint64 PairingWindow::retryAfterMs(Route route) const
{
    if (!isOpen()) {
        return 0;
    }
    if (route == Route::Service && isServiceShut()) {
        return 0;  // LINK-I4: shut, with no time to try again
    }
    const qint64 code = m_codeInUse ? kFirstRetryMs : std::max<qint64>(0, m_nextCodeAt - now());
    return route == Route::Service ? std::max(code, servicePauseRemainingMs()) : code;
}

bool PairingWindow::isPaused(Route route) const
{
    return route == Route::Service && servicePauseRemainingMs() > 0;
}

qint64 PairingWindow::servicePauseRemainingMs() const
{
    if (!isOpen()) {
        return 0;
    }
    return std::max<qint64>(0, m_servicePausedUntil - now());
}

void PairingWindow::poll()
{
    if (m_state == State::OpenReopened && m_openUntil > 0 && now() >= m_openUntil) {
        // A reopened window's lifetime is over (fix wave R1-I2).
        qCInfo(lcPairing) << "Pairing closed: it was open for"
                          << kReopenedLifetimeMs / 60000 << "minutes";
        m_wait->stop();
        commit(State::ClosedClaimed, QString());
        return;
    }
    if (m_state == State::OpenReopened && m_openUntil > 0 && !m_expiry->isActive()) {
        m_expiry->start(static_cast<int>(std::max<qint64>(0, m_openUntil - now())));
    }
    if (!isOpen() || m_codeInUse || !m_code.isEmpty()) {
        return;
    }
    commit(m_state, codeFor(m_state));
}

} // namespace NereusSDR
