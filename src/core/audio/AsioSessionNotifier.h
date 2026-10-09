// =================================================================
// src/core/audio/AsioSessionNotifier.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The QObject an AsioSession signals
// through (R-AUD-21).  Its own header, so a test that compiles
// AsioSession.cpp into itself (the allocation hook on Windows) does not
// also run moc on it; the meta object lives in NereusCore only.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-21). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"

#include <QObject>
#include <QString>

namespace NereusSDR {

class NEREUS_CORE_EXPORT AsioSessionNotifier final : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    // The session restarted after a reset or a buffer size change, with
    // the driver's new settings (AsioSession::caps()).
    void restarted();
    // The session could not run any more and is closed.
    void failed(const QString& detail);
};

} // namespace NereusSDR
