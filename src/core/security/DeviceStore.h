#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/DeviceStore.h  (NereusSDR)
// =================================================================
//
// The Core's paired devices (iPhone app plan Task 12, R-IOS-08; the
// pairing design, docs/architecture/2026-08-02-remote-station-identity-
// and-pairing-design.md section 7, "Devices and revocation").
//
// Each paired device holds its own ECDSA P-256 key; the Core keeps its
// public half with a name, a kind and when it was paired and last seen.
// A device's `id` is the fingerprint of its key (SHA-256 of the
// SubjectPublicKeyInfo DER), so the id cannot name one key and hold
// another: add() refuses a record whose id is not its key's fingerprint.
//
// Stored as `paired-devices.json` in the Core's profile directory, mode
// 0600, written atomically on every change. Public keys are not secrets,
// but the file says who can reach the radio, so it is kept like one.
//
// ---- Failing closed ----
//
// A file that exists but cannot be read or parsed leaves the store
// INVALID: it admits no device, refuses every change (so the file is
// never overwritten), and isClaimed() is true. Reporting "unclaimed"
// there would open the pairing window to anyone on the network because a
// file was damaged; refusing sign-ins is the safe way to be wrong.
//
// isClaimed() is also true while the Core's pairing token has not been
// retired (TokenStore::isActive()): a Core upgraded from the token era
// is claimed through its token until each window has enrolled its own
// key and the token is retired, so no stranger can claim it in the
// meantime.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: reset() for the console's reset (iPhone app Task 17).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8b: touch() keeps a signed-in
//               device's current name too. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace NereusSDR {

class TokenStore;

struct PairedDevice {
    QByteArray id;              // SHA-256 of publicKeySpki (raw, 32 bytes)
    QByteArray publicKeySpki;   // SubjectPublicKeyInfo DER, P-256
    QString name;
    QString kind;               // "phone", "tablet", "computer"
    QDateTime pairedAt;
    QDateTime lastSeen;
    QString lastAddress;        // empty over the relay
    bool enrolledThroughToken = false;
    /// Part C fix wave: the short name the device sent at its last sign-in
    /// that carried a usable one; "" until then.
    QString shortName;
};

class DeviceStore : public QObject {
    Q_OBJECT

public:
    static constexpr const char* kFileName = "paired-devices.json";
    /// Longest device name, in UTF-8 bytes.
    static constexpr int kMaxNameBytes = 64;
    /// Longest short name (auth.request's device block `shortName`), in
    /// UTF-8 bytes, counted as kMaxNameBytes is.
    static constexpr int kMaxShortNameBytes = 32;
    /// Most devices one Core keeps.
    static constexpr int kMaxDevices = 64;

    /// Loads `directory`/paired-devices.json (absent is an empty store).
    /// `tokens`, when given, is consulted by isClaimed() and must outlive
    /// this store.
    explicit DeviceStore(const QString& directory, const TokenStore* tokens = nullptr,
                         QObject* parent = nullptr);

    /// False when the file exists but could not be read or parsed; see the
    /// header comment.
    bool isValid() const { return m_valid; }
    QString lastError() const { return m_lastError; }
    QString filePath() const { return m_path; }

    /// Adds a device. False (and nothing changes) when the store is
    /// invalid, the record is not well formed (id not the fingerprint of a
    /// P-256 key, a kind other than the three, a name that is empty, too
    /// long or holds a control character, or a short name that is neither
    /// empty nor usable), the id is already paired, the
    /// store is full, or the file could not be written. pairedAt and
    /// lastSeen default to now when not set.
    bool add(const PairedDevice& device);
    /// Removes a device; false when it is not paired or the file could not
    /// be written. Emits deviceRemoved(id) and devicesChanged().
    bool remove(const QByteArray& id);
    /// iPhone app Task 17 (R-IOS-08): the console's `reset --unclaimed`.
    /// A damaged file is moved aside first (renamed to
    /// `paired-devices.json.damaged-<UTC time>`, never deleted, so nothing
    /// is lost to a reset), then an empty list is written and the store is
    /// valid again. Emits deviceRemoved(id) for each device it held, then
    /// devicesChanged(). False, with lastError() set and nothing changed,
    /// when the damaged file cannot be moved or the empty list cannot be
    /// written. `movedTo`, when given, receives where a damaged file went
    /// ("" when there was none).
    bool reset(QString* movedTo = nullptr);
    std::optional<PairedDevice> find(const QByteArray& id) const;
    QList<PairedDevice> list() const { return m_devices; }
    /// An authenticated connection: lastSeen becomes now and lastAddress
    /// `address` (empty over the relay), and the short name becomes
    /// `shortName` when that is a usable one (isValidShortName); an absent
    /// or unusable one leaves the stored one as it is. Slice control plan
    /// Task 8b: the name likewise becomes `name` when usable (isValidName),
    /// so a device that now tells its profile is listed by it. Nothing for
    /// an unknown id.
    void touch(const QByteArray& id, const QString& address,
               const QString& shortName = QString(), const QString& name = QString());

    /// Any paired device, or a pairing token not yet retired (or a token
    /// file that could not be read), or a store that could not be read
    /// (see the header comment).
    bool isClaimed() const;

    /// Injected clock for pairedAt and lastSeen. Default: the system's
    /// UTC time.
    void setClock(std::function<QDateTime()> clock);

    /// True when `kind` is one of "phone", "tablet", "computer".
    static bool isKnownKind(const QString& kind);
    /// A device name: not empty, at most kMaxNameBytes of UTF-8, no
    /// control characters.
    static bool isValidName(const QString& name);
    /// A short name: the same rules with kMaxShortNameBytes. The operator's
    /// own words, so not held to the Core's wording rules.
    static bool isValidShortName(const QString& shortName);

signals:
    void devicesChanged();
    void deviceRemoved(const QByteArray& id);

private:
    static bool isValidLabel(const QString& name, int maxBytes);
    bool load();
    bool save(const QList<PairedDevice>& devices);
    QDateTime now() const;

    QString m_path;
    const TokenStore* m_tokens = nullptr;
    bool m_valid = true;
    QString m_lastError;
    QList<PairedDevice> m_devices;
    std::function<QDateTime()> m_clock;
};

} // namespace NereusSDR
