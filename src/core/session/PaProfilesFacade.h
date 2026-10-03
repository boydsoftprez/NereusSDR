#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/PaProfilesFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `paProfiles` object (R-R3-49, R-IOS-18; paProfileVersion
// 1): the Core's PA Gain profiles as its own PA Gain page shows them, so a
// phone reads and changes them through the paProfile verbs and never
// handles the desktop's profile storage.
//
//   json      {"names": [...], "active": "...", "factory": bool,
//              "bands": [{"band": "160m", "gain": dB, "adjust": [9 x dB],
//                         "maxPower": W, "useMax": bool} x 14]}
//             names are the profiles the page's combo lists (Thetis's
//             filter, PaProfileManager::userVisibleProfileNames), bands the
//             active profile's rows in Band order (160m .. XVTR). Empty
//             while the Core has no profile bank for a radio.
//   revision  moves by one each time `json` changes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created (R-R3-49, R-IOS-18). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/HpsdrModel.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

namespace NereusSDR {

class PaProfileManager;

class PaProfilesFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString json READ json NOTIFY changed)
    Q_PROPERTY(quint32 revision READ revision NOTIFY changed)

public:
    explicit PaProfilesFacade(QObject* parent = nullptr);

    /// Follow `manager`'s bank; `model` gives the connected radio's model
    /// (the combo's filter). Rebuilds at once.
    void bind(PaProfileManager* manager, std::function<HPSDRModel()> model);
    /// Rebuild `json` from the bound bank (for a model change).
    void refresh();

    /// The JSON for a bank, or an empty string when it has no active
    /// profile.
    static QString jsonFor(const PaProfileManager& manager, HPSDRModel model);

    QString json() const { return m_json; }
    quint32 revision() const { return m_revision; }

signals:
    void changed();

private:
    QPointer<PaProfileManager> m_manager;
    std::function<HPSDRModel()> m_model;
    QString m_json;
    quint32 m_revision = 0;
};

} // namespace NereusSDR
