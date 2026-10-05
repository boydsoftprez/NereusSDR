#pragma once
// no-port-check: NereusSDR-original. The radios a Core finds, which one it
// runs, and the requests that change it.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/station/StationRadios.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-IOS-18 (Manage Radios), R-R3-38,
// R-R3-49 (parity Task 21; the iPhone app plan's Task 25 station half).
//
// The Core's choice of radio, in this order (the operator's rule of
// 2026-09-23): (1) a radio chosen from an app (station.selectRadio), saved
// in the Core's settings; (2) radio_mac from nereusd.conf; (3) the one
// radio it can see, only when exactly one is visible; (4) otherwise it
// waits for a choice and says so. It never picks the first radio found, and
// never a radio because of its model.
//
// This object keeps the list a window shows (the `stationRadios` record
// stream: {id, name, model, mac, address, protocol, inUse}) and answers the
// four requests: station.selectRadio {mac}, station.rescanRadios {},
// station.setRadioModel {mac, model} (the local Edit radio's model override,
// saved for that radio and applied at its next connect) and
// station.forgetRadio {mac} (refused for the radio in use). The Core's
// DaemonApp does the switching and scanning through the handlers.
//
// Fix wave (I6): a choice is this run's pending choice until that radio
// connects; only then is it saved. A Core that restarts before it connected
// (a radio gone for good, a crash at connect) starts from the last radio
// that did connect, or radio_mac, never from the pending choice.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 21, R-IOS-18,
//                                    R-R3-38, R-R3-49). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Phone wire batch: each radio's model
//                                    label and the models it can run as
//                                    (modelLabel, models). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

#include "core/RadioDiscovery.h"

namespace NereusSDR {

class AppSettings;

/// One row of the `stationRadios` stream.
struct StationRadioEntry {
    QString id;        // the stream id: the radio's MAC, upper case
    QString name;      // the radio's name as it reports itself
    int model = 0;     // HPSDRModel the Core runs it as (its override, else
                       // the board's default)
    QString mac;
    QString address;   // IP address, empty when not known
    int protocol = 1;  // OpenHPSDR protocol 1 or 2
    bool inUse = false; // the Core's radio now
    // Phone wire batch (radioModelsVersion 1): the model's name as Setup
    // shows it (displayName), and every model this radio's board can run
    // as, in the model combo's order (compatibleModels of its board), the
    // list station.setRadioModel accepts. Sent only to a peer that declared
    // radioModels (StationServer strips them for any other).
    QString modelLabel;
    QList<int> models;

    QJsonObject toFields() const;
    static std::optional<StationRadioEntry> fromFields(const QString& id,
                                                       const QJsonObject& fields);
    bool operator==(const StationRadioEntry& other) const;
};

class NEREUS_CORE_EXPORT StationRadios : public QObject {
    Q_OBJECT

public:
    /// The Core's saved choice (a radio chosen from an app). The Core's own
    /// key, never proxied (it classifies OperatorLocal).
    static constexpr const char* kChoiceKey = "StationRadioChoice";

    enum class Pick {
        Radio,          // connect `radio`
        WaitForChosen,  // the chosen (or configured) radio is not visible
        WaitForChoice,  // more than one radio is visible and none is chosen
        NoRadio,        // no radio is visible
    };
    struct Choice {
        Pick pick = Pick::NoRadio;
        RadioInfo radio;
        QString reason; // what a window shows while the Core waits
    };

    /// The choice order. `identified` is the radio this run already runs
    /// (a reconnect keeps it); `saved` the choice from an app; `configured`
    /// nereusd.conf's radio_mac. A radio another client holds (inUse) is
    /// never picked.
    static Choice choose(const QList<RadioInfo>& visible, const QString& identified,
                         const QString& saved, const QString& configured);

    explicit StationRadios(AppSettings& settings, QObject* parent = nullptr);

    /// The last choice that connected (the saved one).
    QString savedChoice() const;
    void saveChoice(const QString& mac);
    /// This run's choice, not yet connected (empty when none). Never saved.
    QString pendingChoice() const { return m_pending; }
    /// A radio connected: a pending choice of it becomes the saved choice.
    void confirmChoice(const QString& mac);
    /// The change was refused at the moment it ran: the pending choice goes.
    void dropPendingChoice();
    /// The radio this run is aimed at: connected, reconnecting to or waiting
    /// for (the Core's DaemonApp). Forgetting it is refused.
    void setTarget(const QString& mac);
    QString target() const { return m_target; }

    /// The radios the last scan found (replacing the list).
    void setVisible(const QList<RadioInfo>& found);
    /// The Core's radio now (connected or being connected), or none.
    void setCurrent(const RadioInfo& radio);
    void clearCurrent();
    QString currentMac() const;
    /// What the Core does while it has no radio: empty when it has one or
    /// is connecting one.
    void setWaiting(const QString& reason);
    QString waitingReason() const { return m_waiting; }

    /// A switch is under way: from a select until the new radio connects,
    /// its connect fails or its link is lost, the Core's connect bound
    /// passes, or a scan does not pick it.
    void setSwitching(bool switching);
    bool switching() const { return m_switching; }

    QList<StationRadioEntry> entries() const;
    std::optional<RadioInfo> radioFor(const QString& mac) const;
    /// The model the Core runs a radio as: its override, else its board's.
    int modelFor(const RadioInfo& radio) const;
    /// The saved model override for a radio (HPSDRModel::FIRST: none).
    HPSDRModel overrideFor(const QString& mac) const;

    // The Core's DaemonApp.
    std::function<void(const QString& mac)> onSelect;
    std::function<void()> onRescan;

    // The four requests. Each returns false with a plain reason when it is
    // refused. The on-the-air rule is the dispatcher's, before these.
    bool select(const QString& mac, QString* reason);
    bool rescan(QString* reason);
    bool setModel(const QString& mac, int model, QString* reason);
    bool forget(const QString& mac, QString* reason);

    static QString unknownRadioReason();
    static QString switchingReason();
    static QString inUseReason();
    /// Fix wave (I5): a window signed in with the pairing token and no key.
    static QString pairedDeviceReason();

signals:
    void entriesChanged();

private:
    AppSettings& m_settings;
    QList<RadioInfo> m_visible;
    RadioInfo m_current;
    bool m_hasCurrent = false;
    bool m_switching = false;
    QString m_waiting;
    QString m_pending;
    QString m_target;
};

} // namespace NereusSDR
