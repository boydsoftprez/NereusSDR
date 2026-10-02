// =================================================================
// src/gui/CoreTargetStore.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 4g.
//
// The desktop-local address book for explicitly saved remote Cores.  This
// deliberately has no station-session or radio ownership: it only persists
// the operator's selection and the credentials needed to make a later
// authenticated connection attempt.
//
// iPhone app Task 18 (R-IOS-08): stored as ConnectionTargets/V2, keyed by
// `id` with `selectedId` as before, and each record gains the Core's
// identity fingerprint (connection.identityFingerprint; empty for a Core
// this computer has not paired with, which stays an address, token and
// pin). A V1 document is migrated once on load and not read again.
//
// iPhone app plan Task 27 (R-IOS-16): each V2 record may carry the Core's
// last good addresses (`lastAddresses`, most recent first), tried before
// its saved address on the next connect; a record without any is written
// exactly as before.
//
// Desktop code-only pairing: ConnectionTargets/V3 is now authoritative.
// It admits a verified paired Core with no direct URL; existing V2 records
// migrate losslessly. V2 and V1 remain older-app rollback documents and
// follow later edits and forgets of records they already contain.
// =================================================================

// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#pragma once

#include "core/session/RemoteStationOptions.h"

#include <QList>
#include <QString>

#include <optional>
#include <functional>

namespace NereusSDR {

class AppSettings;

struct SavedCoreTarget {
    QString id;
    QString label;
    RemoteStationOptions connection;
    QString lastRadioName;
    QString lastRadioMac;
    // This computer's launch preference for this saved Core. Older records
    // retain their former connect-at-launch behavior.
    bool autoConnect{true};
};

class CoreTargetStore {
public:
    explicit CoreTargetStore(AppSettings&, std::function<qint64()> clock = {});

    bool load(QString* error = nullptr);
    QList<SavedCoreTarget> targets() const;
    std::optional<SavedCoreTarget> target(const QString& id) const;
    QString selectedId() const;

    bool upsert(const SavedCoreTarget&, QString* error = nullptr);
    bool remove(const QString& id, QString* error = nullptr);
    /// iPhone app plan Task 27 (R-IOS-16): `url` is where this computer
    /// just reached the saved Core `id`; it goes to the front of the Core's
    /// last good addresses (at most RemoteStationOptions::
    /// kMaxCachedAddresses), which the next connect tries first.
    bool rememberAddress(const QString& id, const QString& url, QString* error = nullptr);
    /// iPhone app plan Task 28 fix wave (R-IOS-16): the controlChannelVersion
    /// the saved Core `id` sent at this computer's sign-in (0 for no
    /// entry), which decides whether connecting from anywhere is offered
    /// (RemoteStationOptions::serviceConnectRefusal()).
    bool rememberControlChannelVersion(const QString& id, int version,
                                       QString* error = nullptr);
    bool invalidateNegativeControlObservations(QString* error = nullptr);
    bool invalidateFutureNegativeObservations(QString* error = nullptr);
    bool observeNetworkFingerprint(const QString& fingerprint, QString* error = nullptr);
    /// iPhone app plan Task 29 (R-IOS-16): the saved Core `id`'s rendezvous
    /// id (from its hello; ignored when not one) and its relay setting as
    /// its last session told it (-1 leaves the recorded one), which a
    /// connect uses to race the internet service beside the addresses.
    bool rememberServiceRoute(const QString& id, const QString& rendezvousId, int relayAllowed,
                              QString* error = nullptr);
    bool select(const QString& id, QString* error = nullptr);

    /// Parse the existing devices.coreAddresses contract. No service/ICE URLs.
    static QStringList parseCoreAddresses(const QString& text);
    bool rememberCoreAddresses(const QString& id, const QByteArray& identity,
                              const QString& text, QString* error = nullptr);
    static QString createId();

private:
    bool persist(const QList<SavedCoreTarget>& targets, const QString& selectedId,
                 QString* error);

    AppSettings& m_settings;
    std::function<qint64()> m_clock;
    QList<SavedCoreTarget> m_targets;
    QString m_selectedId{QStringLiteral("local")};
    bool m_loaded{false};
    std::optional<QString> m_networkFingerprint;
};

} // namespace NereusSDR
