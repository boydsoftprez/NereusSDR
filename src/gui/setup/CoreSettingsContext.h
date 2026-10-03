#pragma once
// no-port-check: NereusSDR-original presentation snapshot; no session ownership.
// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QByteArray>
#include <QString>
#include <QMetaType>

namespace NereusSDR {
struct CoreRenameRequest {
    quint64 operationId = 0;
    QString targetId;
    QByteArray pairedIdentity;
    quint64 incarnation = 0;
    quint64 epoch = 0;
    QString name;
    bool operator==(const CoreRenameRequest&) const = default;
};
/// The host supplies only authenticated current-session facts. targetId is the
/// saved stable UUID, never a name/address. Epoch changes retire pending UI work.
struct CoreSettingsContext {
    QString targetId;
    QByteArray pairedIdentity;
    quint64 epoch = 0;
    bool authenticated = false;
    QString coreName;
    QString reachedThrough;
    QString controls;
    QString audioAndDisplay;
    QString listener;
    QString radio;
    bool stationSettingsAvailable = false;
    QString stationSettingsReason;
    QString renameTargetId;
    QByteArray renamePairedIdentity;
    quint64 renameIncarnation = 0;
    quint64 renameEpoch = 0;
    bool renameAvailable = false;
    QString renameReason;
    bool connectionDetailsAvailable = false;
    bool diagnosticsAvailable = false;
    bool audioAvailable = false;
};
} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::CoreRenameRequest)
