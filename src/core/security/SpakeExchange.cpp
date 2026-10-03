// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/SpakeExchange.cpp  (NereusSDR)
// =================================================================
//
// See SpakeExchange.h. The only NereusSDR file that includes libsodium's
// and spake2-ee's headers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
// =================================================================

#include "core/security/SpakeExchange.h"

#include <sodium.h>

extern "C" {
#include "crypto_spake.h"
}

#include <cstring>
#include <string>

namespace NereusSDR {

static_assert(SpakeExchange::kPublicDataBytes == crypto_spake_PUBLICDATABYTES);
static_assert(SpakeExchange::kResponse1Bytes == crypto_spake_RESPONSE1BYTES);
static_assert(SpakeExchange::kResponse2Bytes == crypto_spake_RESPONSE2BYTES);
static_assert(SpakeExchange::kResponse3Bytes == crypto_spake_RESPONSE3BYTES);
static_assert(SpakeExchange::kStoredBytes == crypto_spake_STOREDBYTES);
static_assert(SpakeExchange::kNonceBytes == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(SpakeExchange::kTagBytes == crypto_aead_xchacha20poly1305_ietf_ABYTES);
static_assert(crypto_spake_SHAREDKEYBYTES == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);

namespace {

const auto* uchars(const QByteArray& bytes)
{
    return reinterpret_cast<const unsigned char*>(bytes.constData());
}

auto* uchars(QByteArray& bytes)
{
    return reinterpret_cast<unsigned char*>(bytes.data());
}

constexpr size_t idLength(const char* id)
{
    return std::char_traits<char>::length(id);
}

} // namespace

struct SpakeExchange::Secrets {
    crypto_spake_server_state server{};
    crypto_spake_client_state client{};
    crypto_spake_shared_keys keys{};
    bool step0Taken = false;
    bool step1Taken = false;
    bool step2Taken = false;
    bool complete = false;

    ~Secrets()
    {
        sodium_memzero(&server, sizeof server);
        sodium_memzero(&client, sizeof client);
        sodium_memzero(&keys, sizeof keys);
    }
};

bool SpakeExchange::isAvailable()
{
    // sodium_init() is safe to call more than once and from any thread;
    // 0 is the first success, 1 an earlier one.
    static const bool available = sodium_init() >= 0;
    return available;
}

QByteArray SpakeExchange::storedData(const QString& normalisedCode)
{
    if (!isAvailable() || normalisedCode.isEmpty()) {
        return {};
    }
    QByteArray password = normalisedCode.toUtf8();
    QByteArray stored(kStoredBytes, '\0');
    const int result = crypto_spake_server_store(
        uchars(stored), password.constData(),
        static_cast<unsigned long long>(password.size()),
        crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);
    wipe(password);
    if (result != 0) {
        wipe(stored);
        return {};
    }
    return stored;
}

void SpakeExchange::wipe(QByteArray& bytes)
{
    if (!bytes.isEmpty()) {
        sodium_memzero(bytes.data(), static_cast<size_t>(bytes.size()));
    }
    bytes.clear();
}

SpakeExchange::SpakeExchange(Role role)
    : m_role(role)
    , m_secrets(std::make_unique<Secrets>())
{
}

SpakeExchange::~SpakeExchange() = default;

QByteArray SpakeExchange::stationStep0(const QByteArray& stored)
{
    if (m_role != Role::Station || !isAvailable() || stored.size() != kStoredBytes
        || m_secrets->step0Taken) {
        return {};
    }
    QByteArray publicData(kPublicDataBytes, '\0');
    if (crypto_spake_step0(&m_secrets->server, uchars(publicData), uchars(stored)) != 0) {
        return {};
    }
    m_secrets->step0Taken = true;
    return publicData;
}

QByteArray SpakeExchange::stationStep2(const QByteArray& stored, const QByteArray& response1)
{
    if (m_role != Role::Station || !m_secrets->step0Taken || m_secrets->step2Taken
        || stored.size() != kStoredBytes || response1.size() != kResponse1Bytes) {
        return {};
    }
    // Taken whatever the outcome: one share per exchange.
    m_secrets->step2Taken = true;
    // Part C fix wave (R1-M2): step 1 must be a valid point before
    // spake2-ee sees it. crypto_spake_step2 (spake2-ee crypto_spake.c:345
    // [@fd3ea61f]) ignores crypto_core_ed25519_sub's return value, so a
    // step 1 that is not a curve point would leave its `gx` unset;
    // libsodium's scalar multiplication then refuses it, but the check
    // belongs here. crypto_core_ed25519_is_valid_point (libsodium 1.0.22
    // core_ed25519.c:10-23, crypto_core_ed25519.h:31-33) returns 1 only for
    // a canonical encoding of a point on the curve, on the main subgroup
    // and not of small order, which an honest step 1 (g^x plus spake2-ee's
    // M) always is.
    if (crypto_core_ed25519_is_valid_point(uchars(response1)) != 1) {
        return {};
    }
    QByteArray response2(kResponse2Bytes, '\0');
    if (crypto_spake_step2(&m_secrets->server, uchars(response2), kClientId,
                           idLength(kClientId), kServerId, idLength(kServerId),
                           uchars(stored), uchars(response1))
        != 0) {
        return {};
    }
    return response2;
}

bool SpakeExchange::stationStep4(const QByteArray& response3)
{
    if (m_role != Role::Station || !m_secrets->step2Taken || m_secrets->complete
        || response3.size() != kResponse3Bytes) {
        return false;
    }
    crypto_spake_shared_keys keys{};
    // spake2-ee wipes its state whatever the outcome, so a second step 3
    // cannot be tried against the same step 2.
    const bool agreed = crypto_spake_step4(&m_secrets->server, &keys, uchars(response3)) == 0;
    if (agreed) {
        m_secrets->keys = keys;
        m_secrets->complete = true;
    }
    sodium_memzero(&keys, sizeof keys);
    return agreed;
}

QByteArray SpakeExchange::deviceStep1(const QByteArray& publicData, const QString& normalisedCode)
{
    if (m_role != Role::Device || !isAvailable() || m_secrets->step1Taken
        || publicData.size() != kPublicDataBytes || normalisedCode.isEmpty()) {
        return {};
    }
    // The parameters are fixed on both sides: a peer naming weaker ones
    // (or another algorithm) is refused before the code is hashed.
    if (crypto_spake_validate_public_data(uchars(publicData), crypto_pwhash_alg_default(),
                                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                          crypto_pwhash_MEMLIMIT_INTERACTIVE)
        != 0) {
        return {};
    }
    m_secrets->step1Taken = true;
    QByteArray password = normalisedCode.toUtf8();
    QByteArray response1(kResponse1Bytes, '\0');
    const int result =
        crypto_spake_step1(&m_secrets->client, uchars(response1), uchars(publicData),
                           password.constData(), static_cast<unsigned long long>(password.size()));
    wipe(password);
    if (result != 0) {
        return {};
    }
    return response1;
}

QByteArray SpakeExchange::deviceStep3(const QByteArray& response2)
{
    if (m_role != Role::Device || !m_secrets->step1Taken || m_secrets->complete
        || response2.size() != kResponse2Bytes) {
        return {};
    }
    QByteArray response3(kResponse3Bytes, '\0');
    crypto_spake_shared_keys keys{};
    if (crypto_spake_step3(&m_secrets->client, uchars(response3), &keys, kClientId,
                           idLength(kClientId), kServerId, idLength(kServerId),
                           uchars(response2))
        != 0) {
        sodium_memzero(&keys, sizeof keys);
        return {};
    }
    m_secrets->keys = keys;
    m_secrets->complete = true;
    sodium_memzero(&keys, sizeof keys);
    return response3;
}

bool SpakeExchange::isComplete() const
{
    return m_secrets->complete;
}

QByteArray SpakeExchange::sealConfirmation(const QByteArray& plaintext) const
{
    if (!m_secrets->complete) {
        return {};
    }
    const unsigned char* key =
        m_role == Role::Device ? m_secrets->keys.client_sk : m_secrets->keys.server_sk;
    QByteArray box(kNonceBytes + plaintext.size() + kTagBytes, '\0');
    randombytes_buf(uchars(box), kNonceBytes);
    unsigned long long written = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            uchars(box) + kNonceBytes, &written, uchars(plaintext),
            static_cast<unsigned long long>(plaintext.size()), nullptr, 0, nullptr,
            uchars(box), key)
        != 0) {
        return {};
    }
    box.truncate(kNonceBytes + static_cast<qsizetype>(written));
    return box;
}

std::optional<QByteArray> SpakeExchange::openConfirmation(const QByteArray& box) const
{
    if (!m_secrets->complete || box.size() < kNonceBytes + kTagBytes) {
        return std::nullopt;
    }
    const unsigned char* key =
        m_role == Role::Device ? m_secrets->keys.server_sk : m_secrets->keys.client_sk;
    const qsizetype cipherBytes = box.size() - kNonceBytes;
    QByteArray plaintext(cipherBytes - kTagBytes, '\0');
    unsigned long long written = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            uchars(plaintext), &written, nullptr, uchars(box) + kNonceBytes,
            static_cast<unsigned long long>(cipherBytes), nullptr, 0, uchars(box), key)
        != 0) {
        return std::nullopt;
    }
    plaintext.truncate(static_cast<qsizetype>(written));
    return plaintext;
}

} // namespace NereusSDR
