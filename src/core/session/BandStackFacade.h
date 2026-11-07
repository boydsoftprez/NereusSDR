#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/BandStackFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `bandStack` object (bandStackVersion 1): the Core's band stack
// document, so every device can draw every slice's place and change the stack
// through the bandStack.* verbs. Design:
// docs/architecture/2026-11-05-band-stack-design.md, sections 6.2 and 6.3.
//
//   json      the stack's saved document (schema, region, options, bands,
//             entries, places) plus lastExpiry {batchId, count} and lastRecall
//             {seq, sliceId, applyView, centerHz, spanHz}, which are published
//             but not saved. `places` leaves out a slice's place on a band it
//             tuned into (design 5.3). Empty until the Core knows a radio.
//   revision  moves by one each time `json` changes, and at no other time.
//
// On the Core it follows BandStackController. In a remote window it holds what
// the mirror last sent.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

class BandStackController;

class NEREUS_CORE_EXPORT BandStackFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString json READ json NOTIFY changed)
    Q_PROPERTY(quint32 revision READ revision NOTIFY changed)

public:
    explicit BandStackFacade(QObject* parent = nullptr);

    /// Follow `controller`. Rebuilds at once.
    void bind(BandStackController* controller);
    /// Rebuild `json` from the bound controller.
    void refresh();

    /// The published document for a controller.
    static QString jsonFor(const BandStackController& controller);

    /// A remote window's mirror update. Ignored once bound to a controller.
    /// True when the property is one of this object's.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);
    /// A remote window keeps no copy when the session ends.
    void clearStationValues();

    QString json() const { return m_json; }
    quint32 revision() const { return m_revision; }

signals:
    void changed();

private:
    QPointer<BandStackController> m_controller;
    QString m_json;
    quint32 m_revision{0};
};

} // namespace NereusSDR
