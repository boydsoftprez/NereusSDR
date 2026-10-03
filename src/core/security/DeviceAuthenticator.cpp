// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/DeviceAuthenticator.cpp  (NereusSDR)
// =================================================================
// See DeviceAuthenticator.h for the transcript and the rate limits.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: Fix wave LINK minor 4: a failed proof counts against the
//               address and the relay introduction, never the id it
//               names. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/security/DeviceAuthenticator.h"

#include "core/security/StationIdentity.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include <chrono>

namespace NereusSDR {

namespace {

const QByteArray kTranscriptPrefix = QByteArrayLiteral("NereusSDR device-auth v1\n");
constexpr int kDigestBytes = 32;
// An id the limiter keys on verbatim; a longer text is keyed by its hash so
// a peer cannot make the limiter hold long strings.
constexpr int kMaxKeyTextLength = 64;

QByteArray sha256(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

QString boundedKey(const QString& prefix, const QString& text)
{
    if (text.size() <= kMaxKeyTextLength) {
        return prefix + text;
    }
    return prefix + QString::fromLatin1(sha256(text.toUtf8()).toHex());
}

struct Decoded {
    QByteArray id;
    QByteArray spki;
    QByteArray signature;
};

// The block's own consistency: every field decodes, the key is a canonical
// P-256 key and the id is its fingerprint.
std::optional<Decoded> decode(const DeviceAuthRequest& request)
{
    bool idOk = false;
    bool keyOk = false;
    bool signatureOk = false;
    Decoded d;
    d.id = StationIdentity::fromBase64Url(request.id, &idOk);
    d.spki = StationIdentity::fromBase64Url(request.publicKey, &keyOk);
    d.signature = StationIdentity::fromBase64Url(request.signature, &signatureOk);
    if (!idOk || !keyOk || !signatureOk || d.id.size() != kDigestBytes
        || d.signature.size() != StationIdentity::kSignatureBytes
        || !StationIdentity::isP256Spki(d.spki) || StationIdentity::fingerprintOf(d.spki) != d.id) {
        return std::nullopt;
    }
    return d;
}

} // namespace

DeviceAuthenticator::DeviceAuthenticator(const DeviceStore& store,
                                         const StationIdentity& identity, Clock clock)
    : m_store(store)
    , m_identity(identity)
    , m_clock(std::move(clock))
{
}

qint64 DeviceAuthenticator::now() const
{
    if (m_clock) {
        return m_clock();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

QByteArray DeviceAuthenticator::newChallenge()
{
    // ::system() is the operating system's generator, as TokenStore uses
    // for the token. Filled as aligned words.
    quint32 words[kChallengeBytes / sizeof(quint32)]{};
    QRandomGenerator::system()->fillRange(words);
    return QByteArray(reinterpret_cast<const char*>(words), kChallengeBytes);
}

QByteArray DeviceAuthenticator::transcript(const QByteArray& challenge,
                                           const QByteArray& certSha256,
                                           const QByteArray& stationSpki,
                                           const QByteArray& deviceSpki)
{
    return kTranscriptPrefix + challenge + certSha256 + sha256(stationSpki) + sha256(deviceSpki);
}

QStringList DeviceAuthenticator::limitKeys(const DeviceAuthRequest& request,
                                           bool countId) const
{
    QStringList keys;
    if (countId) {
        keys.append(boundedKey(QStringLiteral("id:"), request.id));
    }
    if (!request.sourceAddress.isEmpty()) {
        keys.append(boundedKey(QStringLiteral("address:"), request.sourceAddress));
    }
    if (!request.introduction.isEmpty()) {
        keys.append(boundedKey(QStringLiteral("introduction:"), request.introduction));
    }
    return keys;
}

bool DeviceAuthenticator::isRateLimited(const DeviceAuthRequest& request) const
{
    const qint64 time = now();
    for (const QString& key : limitKeys(request, /*countId=*/true)) {
        const auto it = m_limits.constFind(key);
        if (it != m_limits.constEnd() && it->refusedUntil > time) {
            return true;
        }
    }
    return false;
}

void DeviceAuthenticator::recordFailure(const DeviceAuthRequest& request, bool countId)
{
    const qint64 time = now();
    for (const QString& key : limitKeys(request, countId)) {
        Limit& limit = m_limits[key];
        while (!limit.failures.isEmpty() && time - limit.failures.first() >= kWindowMs) {
            limit.failures.removeFirst();
        }
        limit.failures.append(time);
        if (limit.failures.size() >= kMaxFailures) {
            limit.refusedUntil = time + kLockoutMs;
            limit.failures.clear();
        }
    }
    prune(time);
}

void DeviceAuthenticator::prune(qint64 time)
{
    if (m_limits.size() <= kMaxTrackedKeys) {
        return;
    }
    for (auto it = m_limits.begin(); it != m_limits.end();) {
        const bool refused = it->refusedUntil > time;
        const bool recent = !it->failures.isEmpty() && time - it->failures.last() < kWindowMs;
        if (!refused && !recent) {
            it = m_limits.erase(it);
        } else {
            ++it;
        }
    }
    // Still full of recent failures: forget the ones that are not refused
    // yet rather than grow. A refusal in force is never forgotten.
    if (m_limits.size() > kMaxTrackedKeys) {
        for (auto it = m_limits.begin(); it != m_limits.end();) {
            it = it->refusedUntil > time ? std::next(it) : m_limits.erase(it);
        }
    }
}

AuthOutcome DeviceAuthenticator::verifyPossession(const DeviceAuthRequest& request,
                                                  const QByteArray& challenge,
                                                  const QByteArray& certSha256) const
{
    AuthOutcome outcome;
    outcome.result = AuthOutcome::Result::ProofFailed;
    const std::optional<Decoded> decoded = decode(request);
    if (!decoded || challenge.size() != kChallengeBytes || certSha256.size() != kDigestBytes
        || !m_identity.isValid()) {
        return outcome;
    }
    outcome.deviceId = decoded->id;
    const QByteArray message =
        transcript(challenge, certSha256, m_identity.publicKeySpki(), decoded->spki);
    if (!StationIdentity::verify(decoded->spki, message, decoded->signature)) {
        return outcome;
    }
    outcome.result = AuthOutcome::Result::Proved;
    outcome.publicKeySpki = decoded->spki;
    return outcome;
}

AuthOutcome DeviceAuthenticator::verify(const DeviceAuthRequest& request,
                                        const QByteArray& challenge,
                                        const QByteArray& certSha256)
{
    if (isRateLimited(request)) {
        AuthOutcome limited;
        limited.result = AuthOutcome::Result::RateLimited;
        return limited;
    }
    AuthOutcome outcome = verifyPossession(request, challenge, certSha256);
    if (outcome.result != AuthOutcome::Result::Proved) {
        // LINK minor 4: a proof that failed did not come from the id's key,
        // so anyone who knows a paired device's id could send it. It counts
        // against the source (address, introduction) only; counted against
        // the id it would let a stranger lock a paired device out.
        recordFailure(request, /*countId=*/false);
        return outcome;
    }
    // From here the request's key signed this connection's transcript and
    // its id is that key's fingerprint: the failures are the key holder's
    // own and count against the id as well.
    const std::optional<PairedDevice> device = m_store.find(outcome.deviceId);
    if (!device) {
        outcome.result = AuthOutcome::Result::NotPaired;
        recordFailure(request, /*countId=*/true);
        return outcome;
    }
    if (device->publicKeySpki != outcome.publicKeySpki) {
        outcome.result = AuthOutcome::Result::ProofFailed;
        recordFailure(request, /*countId=*/true);
        return outcome;
    }
    outcome.result = AuthOutcome::Result::Admitted;
    return outcome;
}

} // namespace NereusSDR
