#pragma once
// no-port-check: NereusSDR-original. The Core's accessory records and
// settings, published as the `accessoryData` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/StationAccessoryData.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-47 / R-R3-22.
//
// Runs wherever the accessories are (the Core, or a local window in
// process) and keeps AccessoryDataModel current: the three fault logs, the
// Power Genius and Tuner Genius connection counters, the transmit interlock
// policy, the Power Genius output limit and its alert, the tune memory and
// the antenna names. It is also where the Core applies the three commands a
// window sends (setTxInterlockPolicy, setPgxlPowerCap, clearAccessoryFaults)
// and where a window's change to one of these settings, made through the
// station's settings, reaches the Core's live objects.
//
// The power-cap alert moved here from MainWindow::onAmpMetersForPowerCap
// (Phase 3P-II Phase 4 Task 97): the same rule (above the limit raises one
// alert, back at or below it re-arms), now computed where the amp is.
//
// Nothing here keys a transmitter or operates an accessory: the interlock
// policy's enforcement stays in MoxController and is unchanged.
//
// I3: a fault captured or cleared, and the interlock policy or output limit
// a window sets, reach the settings file within kSaveDelayMs (one save for
// a burst), not only at a clean stop, so a power loss at the station does
// not lose them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Saves the records soon after a change
//                                    (R-R3-47). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10): the RF-Kit's
//                                    connection counts, read from its
//                                    Rf2ksConnection once a second.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

namespace NereusSDR {

class AccessoryDataModel;
class ConnectionDiagnostics;
class FaultLog;
class Rf2ksConnection;
class TuneMemoryStore;
class TxInterlockPolicy;

class StationAccessoryData : public QObject {
    Q_OBJECT
public:
    struct Sources {
        FaultLog* pgxlFaults{nullptr};
        FaultLog* tgxlFaults{nullptr};
        FaultLog* rfkitFaults{nullptr};
        ConnectionDiagnostics* pgxlDiagnostics{nullptr};
        ConnectionDiagnostics* tgxlDiagnostics{nullptr};
        TxInterlockPolicy* interlock{nullptr};
        TuneMemoryStore* tuneMemory{nullptr};
        // R-R3-49 (parity Task 10): the RF-Kit connection whose counts are
        // published (rfkit* on accessoryData).
        Rf2ksConnection* rfkitConnection{nullptr};
    };

    // The output-limit range the Setup page offers and the Core accepts.
    static constexpr int kPowerCapMinW = 100;
    static constexpr int kPowerCapMaxW = 2000;

    // How long after a change the settings file is written (a burst of
    // changes is one write).
    static constexpr int kSaveDelayMs = 500;

    // R-R3-49 (parity Task 10): how often the RF-Kit's connection counts are
    // read. Its REST poll updates them several times a second; a window
    // hears them once a second, as the Power Genius's coalesced counters.
    static constexpr int kRfKitCountersIntervalMs = 1000;

    StationAccessoryData(AccessoryDataModel* model, const Sources& sources,
                         QObject* parent = nullptr);
    ~StationAccessoryData() override;

    /// The alert a window shows when the amp's output passes the limit.
    static QString powerCapAlertText(int forwardW, int limitW);

    /// The amp's peak forward power, W, as the Core reads it.
    void onForwardPower(double watts);

    /// A window changed a station setting (StationServer): bring the Core's
    /// live objects and the model up to date. Other keys are ignored.
    void applySetting(const QString& key);

    // ---- The commands (reason in plain words on refusal) ----
    bool setInterlockPolicy(int mode, int graceMs, bool swrGateEnabled, double swrGateMax,
                            QString* reason);
    bool setPowerCap(bool enabled, int watts, QString* reason);
    /// "pgxl", "tgxl" or "rfkit".
    bool clearFaults(const QString& device, QString* reason);

    /// Re-read everything (at start, and in tests).
    void publishAll();

private:
    void publishFaults();
    void publishInterlock();
    void publishPowerCapSettings();
    void publishTuneMemory();
    void publishLabels();
    void publishRfKitCounters();
    void scheduleSave();
    void saveNow();

    QPointer<AccessoryDataModel> m_model;
    QTimer m_saveTimer;
    QTimer m_rfkitCountersTimer;
    Sources m_sources;
    bool m_alertArmed{true};
};

} // namespace NereusSDR
