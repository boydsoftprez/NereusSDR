#pragma once

// =================================================================
// src/core/QsoUploader.h  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis or AetherSDR equivalent — Thetis hands
// QSOs to an external logger, and AetherSDR's QRZ code is lookup-only.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-17 — serviceId() / shortName() for QsoUploadQueue (every
//                 logged contact goes to every configured service),
//                 AI-assisted via Anthropic Claude Code, operator
//                 Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QObject>
#include <QString>

namespace Longpath {

// One place a logged contact can be sent.
//
// The interface exists from the first implementation rather than after
// the second, because the operator asked for "QRZ and the usual
// loggers" — Club Log, eQSL and LoTW all take the same ADIF record and
// differ only in endpoint and credential. Adding one should mean a new
// subclass, not a second copy of the send path.
//
// Every uploader is expected to:
//   - accept the record from LogEntry::toAdifRecord(), not a private
//     serialisation, so a contact is spelled identically everywhere;
//   - report exactly once per submitted entry, success or failure;
//   - treat a duplicate as success — re-uploading a QSO the service
//     already has is the normal result of a retry, not an error the
//     operator needs to see.
class QsoUploader : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~QsoUploader() override = default;

    // Name for the UI ("QRZ Logbook").
    virtual QString serviceName() const = 0;

    // The word for a status line: "uploaded to QRZ, Wavelog". Short on
    // purpose — the line under the log button has room for one row.
    virtual QString shortName() const { return serviceName(); }

    // Stable key the waiting list is filed under (QsoUploadQueue). NOT
    // the display name: that one may be reworded, and a reworded name
    // must not orphan the contacts still waiting for this service.
    virtual QString serviceId() const = 0;

    // False when the service has no credentials yet; the UI greys the
    // upload rather than letting it fail on every contact.
    virtual bool isConfigured() const = 0;

    // Send one contact. Result arrives via uploadFinished.
    virtual void upload(const LogEntry& entry) = 0;

signals:
    // `call` identifies which contact this is about — uploads may
    // overlap. `duplicate` marks "the service already had it", which
    // callers should treat as done, not as an error.
    void uploadFinished(const QString& call, bool ok, bool duplicate,
                        const QString& message);
};

} // namespace Longpath
