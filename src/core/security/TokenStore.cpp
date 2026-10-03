// =================================================================
// src/core/security/TokenStore.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Remote Daemon R2, Task 18.
// See TokenStore.h for the design rationale.
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 12: load only, never generate; retire().
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24: iPhone app Task 17: moveDamagedAside(). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: LINK minor 5: the failure limiter is kept per source
//               address. LINK minor 6: the token file is made owner-only
//               again on every load. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/security/TokenStore.h"

#include "core/AppSettings.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcTokenStore, "nereus.tokenstore")

// Longest token file this class will read. A token is 43 characters; the
// bound exists so a corrupt or hostile file cannot make the daemon
// allocate on a whim before it ever gets to validate anything.
constexpr qint64 kMaxTokenFileBytes = 4096;

// Byte-wise difference accumulator with no early exit. Both arguments are
// SHA-256 digests, so they are always the same length and the loop count
// carries no information about either input -- see TokenStore.h's note on
// why the raw strings are not compared directly.
bool constantTimeEqual(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    quint8 diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff = static_cast<quint8>(
            diff | (static_cast<quint8>(a[i]) ^ static_cast<quint8>(b[i])));
    }
    return diff == 0;
}

QByteArray digestOf(const QString& s)
{
    return QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256);
}
} // namespace

TokenStore::TokenStore(const QString& directory)
    : m_directory(directory)
    , m_tokenPath(directory + QStringLiteral("/station-token"))
{
    if (!QDir().mkpath(m_directory)) {
        m_lastError = QStringLiteral("Could not create %1").arg(m_directory);
        return;
    }

    // iPhone app Task 12: loaded when an earlier Core left one, never
    // generated. A file that exists but cannot be read leaves the store
    // invalid (DeviceStore::isClaimed() then still counts the Core as
    // claimed), and is never replaced.
    loadExisting();
    m_valid = m_lastError.isEmpty();
}

QString TokenStore::defaultDirectory()
{
    // profileOverride() when there is one, kDaemonProfileName otherwise.
    // See the header for why this follows --profile at all.
    //
    // isEmpty(), not isNull(), and the fallback is deliberate rather than
    // defensive. server_main.cpp calls setProfileOverride() only for a
    // NON-empty resolved profile, so `nereusd --profile ""` -- the
    // documented escape hatch back to the shared settings directory --
    // leaves the override unset. Falling back to the reserved daemon
    // profile keeps the token and the private key out of the user's shared
    // config directory even then, which is the conservative choice for a
    // secret and is not what that escape hatch was asking for. It also
    // means any process that never sets an override still gets the
    // reserved directory rather than the shared one.
    const QString profile = AppSettings::profileOverride();
    return AppSettings::resolveConfigDir(
        profile.isEmpty() ? QString::fromLatin1(AppSettings::kDaemonProfileName)
                          : profile);
}

bool TokenStore::loadExisting()
{
    QFile file(m_tokenPath);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Token file %1 exists but could not be opened: %2")
                          .arg(m_tokenPath, file.errorString());
        return false;
    }
    const QByteArray raw = file.read(kMaxTokenFileBytes);
    file.close();

    const QString candidate = QString::fromUtf8(raw).trimmed();
    if (candidate.isEmpty()) {
        // Empty or whitespace-only is treated as "not present", exactly
        // like a genuine first run: there is no secret here to protect,
        // so regenerating loses nothing.
        return false;
    }
    m_token = candidate;
    // LINK minor 6: a token file loosened since it was written is made
    // owner-only again on every load. Best effort: a failure is logged and
    // the token still loads.
    if (!QFile::setPermissions(m_tokenPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qCWarning(lcTokenStore) << "Could not make the token file owner-only:" << m_tokenPath;
    }
    return true;
}

bool TokenStore::retire()
{
    if (!isActive()) {
        return m_valid;
    }
    QFile file(m_tokenPath);
    if (file.exists() && !file.remove()) {
        qCWarning(lcTokenStore) << "Could not retire the pairing token:" << file.errorString();
        return false;
    }
    m_token.fill(QLatin1Char('\0'));
    m_token.clear();
    m_failures.clear();
    return true;
}

bool TokenStore::moveDamagedAside()
{
    if (m_valid) {
        return true;
    }
    if (!QDir(m_directory).exists()) {
        return false;
    }
    if (QFile::exists(m_tokenPath)) {
        const QString aside = m_tokenPath + QStringLiteral(".damaged-")
                              + QDateTime::currentDateTimeUtc().toString(
                                  QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'"));
        if (!QFile::rename(m_tokenPath, aside)) {
            qCWarning(lcTokenStore) << "Could not move the damaged token file aside";
            return false;
        }
    }
    m_token.clear();
    m_lastError.clear();
    m_valid = true;
    m_failures.clear();
    return true;
}

void TokenStore::setRateLimit(int maxFailures, int lockoutMs)
{
    m_maxFailures = maxFailures < 1 ? 1 : maxFailures;
    m_lockoutMs = lockoutMs < 0 ? 0 : lockoutMs;
}

int TokenStore::consecutiveFailures(const QString& source) const
{
    const auto it = m_failures.constFind(source);
    return it == m_failures.constEnd() ? 0 : it->consecutive;
}

bool TokenStore::isRateLimited(const QString& source) const
{
    const auto it = m_failures.constFind(source);
    if (it == m_failures.constEnd() || it->consecutive < m_maxFailures) {
        return false;
    }
    if (!it->sinceLastFailure.isValid()) {
        return false;
    }
    return it->sinceLastFailure.elapsed() < m_lockoutMs;
}

void TokenStore::prune()
{
    if (m_failures.size() <= kMaxTrackedSources) {
        return;
    }
    for (auto it = m_failures.begin(); it != m_failures.end();) {
        const bool refused = it->consecutive >= m_maxFailures && it->sinceLastFailure.isValid()
                             && it->sinceLastFailure.elapsed() < m_lockoutMs;
        it = refused ? std::next(it) : m_failures.erase(it);
    }
}

TokenStore::VerifyResult TokenStore::verify(const QString& candidate, const QString& source)
{
    auto existing = m_failures.find(source);
    if (existing != m_failures.end() && existing->consecutive >= m_maxFailures
        && existing->sinceLastFailure.isValid()
        && existing->sinceLastFailure.elapsed() >= m_lockoutMs) {
        // The lockout ran out. Start a fresh count rather than leaving the
        // counter pinned at the limit, which would make every later
        // failure re-trigger the lockout immediately.
        existing->consecutive = 0;
    }

    if (isRateLimited(source)) {
        qCWarning(lcTokenStore)
            << "Authentication attempt refused: rate limited after"
            << consecutiveFailures(source) << "consecutive failures";
        return VerifyResult::RateLimited;
    }

    // An unprovisioned store has no secret to compare against. Refuse
    // rather than accepting anything (including an empty candidate).
    const bool ok = m_valid && !m_token.isEmpty()
                    && constantTimeEqual(digestOf(m_token), digestOf(candidate));
    if (ok) {
        m_failures.remove(source);
        return VerifyResult::Accepted;
    }

    SourceFailures& failures = m_failures[source];
    ++failures.consecutive;
    failures.sinceLastFailure.start();
    prune();
    return VerifyResult::Rejected;
}

} // namespace NereusSDR
