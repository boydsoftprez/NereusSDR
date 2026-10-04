#pragma once
// no-port-check: NereusSDR-original. The Core's accessory records and
// settings as the mirrored, read-only `accessoryData` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/AccessoryDataModel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-47 / R-R3-22.
//
// What the Core keeps for its Power Genius XL, Tuner Genius XL and RF-Kit
// RF2K-S beyond their live readings: the fault history of each (with a
// revision that moves whenever any list changes), the connection counters
// of the Power Genius and Tuner Genius (and, from accessoryDataVersion 2,
// the RF-Kit's REST polling counts), the transmit interlock policy, the
// Power Genius output limit and its alert, the Tuner Genius tune memory,
// and the antenna names. The Core mirrors it as the read-only
// `accessoryData` object (accessoryDataVersion 1); every window shows the
// Core's copy. A window changes the policy, the output limit and the fault
// history only through the setTxInterlockPolicy, setPgxlPowerCap and
// clearAccessoryFaults commands; never by writing this object.
//
// The wire contract: docs/architecture/2026-09-23-remote-accessory-control-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10): the RF-Kit's
//                                    connection counts (rfkit*,
//                                    accessoryDataVersion 2). AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/ConnectionDiagnostics.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace NereusSDR {

class NEREUS_CORE_EXPORT AccessoryDataModel : public QObject {
    Q_OBJECT

public:
    /// The transmit interlock mode. Values are fixed on the wire and match
    /// TxInterlockPolicy::Mode.
    enum class InterlockMode { Disabled = 0, Warn = 1, Block = 2 };
    Q_ENUM(InterlockMode)

private:
    // Fault history (one change notice for the three lists).
    Q_PROPERTY(qint64 faultRevision READ faultRevision NOTIFY faultsChanged)
    Q_PROPERTY(QString pgxlFaults READ pgxlFaults NOTIFY faultsChanged)
    Q_PROPERTY(QString tgxlFaults READ tgxlFaults NOTIFY faultsChanged)
    Q_PROPERTY(QString rfkitFaults READ rfkitFaults NOTIFY faultsChanged)

    // Power Genius connection counters.
    Q_PROPERTY(qint64 pgxlConnectedSinceMs READ pgxlConnectedSinceMs NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 pgxlLastRttMs READ pgxlLastRttMs NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(int pgxlKeepaliveMissed READ pgxlKeepaliveMissed NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(int pgxlReconnectCount READ pgxlReconnectCount NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 pgxlFramesIn READ pgxlFramesIn NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 pgxlFramesOut READ pgxlFramesOut NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 pgxlBytesIn READ pgxlBytesIn NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 pgxlBytesOut READ pgxlBytesOut NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 pgxlLastFrameMs READ pgxlLastFrameMs NOTIFY pgxlDiagnosticsChanged)
    Q_PROPERTY(int pgxlFaultsSession READ pgxlFaultsSession NOTIFY pgxlDiagnosticsChanged)

    // Tuner Genius connection counters.
    Q_PROPERTY(qint64 tgxlConnectedSinceMs READ tgxlConnectedSinceMs NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 tgxlLastRttMs READ tgxlLastRttMs NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(int tgxlKeepaliveMissed READ tgxlKeepaliveMissed NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(int tgxlReconnectCount READ tgxlReconnectCount NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 tgxlFramesIn READ tgxlFramesIn NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 tgxlFramesOut READ tgxlFramesOut NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 tgxlBytesIn READ tgxlBytesIn NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 tgxlBytesOut READ tgxlBytesOut NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(qint64 tgxlLastFrameMs READ tgxlLastFrameMs NOTIFY tgxlDiagnosticsChanged)
    Q_PROPERTY(int tgxlFaultsSession READ tgxlFaultsSession NOTIFY tgxlDiagnosticsChanged)

    // Transmit interlock policy (enforced on the Core).
    Q_PROPERTY(InterlockMode interlockMode READ interlockMode NOTIFY interlockChanged)
    Q_PROPERTY(int interlockGraceMs READ interlockGraceMs NOTIFY interlockChanged)
    Q_PROPERTY(bool interlockSwrGateEnabled READ interlockSwrGateEnabled NOTIFY interlockChanged)
    Q_PROPERTY(double interlockSwrGateMax READ interlockSwrGateMax NOTIFY interlockChanged)

    // Power Genius output limit and its alert.
    Q_PROPERTY(bool powerCapEnabled READ powerCapEnabled NOTIFY powerCapChanged)
    Q_PROPERTY(int powerCapW READ powerCapW NOTIFY powerCapChanged)
    Q_PROPERTY(bool powerCapExceeded READ powerCapExceeded NOTIFY powerCapChanged)
    // The text comes before the count: a window applies a delta in this
    // order and raises the alert when the count moves.
    Q_PROPERTY(QString powerCapAlertText READ powerCapAlertText NOTIFY powerCapChanged)
    Q_PROPERTY(qint64 powerCapAlertCount READ powerCapAlertCount NOTIFY powerCapChanged)

    // Tuner Genius tune memory.
    Q_PROPERTY(QString tuneMemory READ tuneMemory NOTIFY tuneMemoryChanged)
    Q_PROPERTY(bool autoTuneMemoryRecall READ autoTuneMemoryRecall NOTIFY tuneMemoryChanged)

    // Antenna names ("" means the default name).
    Q_PROPERTY(QString tgxlAntenna1Label READ tgxlAntenna1Label NOTIFY labelsChanged)
    Q_PROPERTY(QString tgxlAntenna2Label READ tgxlAntenna2Label NOTIFY labelsChanged)
    Q_PROPERTY(QString tgxlAntenna3Label READ tgxlAntenna3Label NOTIFY labelsChanged)
    Q_PROPERTY(QString rfkitAntenna1Label READ rfkitAntenna1Label NOTIFY labelsChanged)
    Q_PROPERTY(QString rfkitAntenna2Label READ rfkitAntenna2Label NOTIFY labelsChanged)
    Q_PROPERTY(QString rfkitAntenna3Label READ rfkitAntenna3Label NOTIFY labelsChanged)
    Q_PROPERTY(QString rfkitAntenna4Label READ rfkitAntenna4Label NOTIFY labelsChanged)

    // R-R3-49 (parity Task 10, accessoryDataVersion 2): the RF-Kit's
    // connection counts, as Rf2ksConnection keeps them on the Core (ms since
    // the epoch; 0 when not connected, or before the first answer). Last,
    // so the older properties keep their ordinals.
    Q_PROPERTY(qint64 rfkitConnectedSinceMs READ rfkitConnectedSinceMs NOTIFY rfkitDiagnosticsChanged)
    Q_PROPERTY(int rfkitPollsOk READ rfkitPollsOk NOTIFY rfkitDiagnosticsChanged)
    Q_PROPERTY(int rfkitPollsFailed READ rfkitPollsFailed NOTIFY rfkitDiagnosticsChanged)
    Q_PROPERTY(int rfkitReconnectCount READ rfkitReconnectCount NOTIFY rfkitDiagnosticsChanged)
    Q_PROPERTY(qint64 rfkitLastPollMs READ rfkitLastPollMs NOTIFY rfkitDiagnosticsChanged)
    // Group B fix wave (M7, accessoryDataVersion 3): the amp's average
    // response time over its last ten polls, as the local page shows it.
    Q_PROPERTY(int rfkitRttAvgMs READ rfkitRttAvgMs NOTIFY rfkitDiagnosticsChanged)

public:
    static constexpr int kTgxlAntennas = 3;
    static constexpr int kRfKitAntennas = 4;

    /// R-R3-49 (parity Task 10): the RF-Kit's connection counts.
    struct RfKitCounters {
        qint64 connectedSinceMs{0};
        int pollsOk{0};
        int pollsFailed{0};
        int reconnectCount{0};
        qint64 lastPollMs{0};
        int rttAvgMs{0};
        bool operator==(const RfKitCounters&) const = default;
    };

    struct PowerCap {
        bool enabled{false};
        int watts{1500};
        bool exceeded{false};
        qint64 alertCount{0};
        QString alertText;
        bool operator==(const PowerCap&) const = default;
    };

    /// Why a window cannot write this object: the Core refuses every write.
    static QString readOnlyReason();

    explicit AccessoryDataModel(QObject* parent = nullptr);

    // ---- Faults ----
    qint64 faultRevision() const { return m_faultRevision; }
    QString pgxlFaults() const { return m_pgxlFaults; }
    QString tgxlFaults() const { return m_tgxlFaults; }
    QString rfkitFaults() const { return m_rfkitFaults; }
    /// "pgxl", "tgxl" or "rfkit".
    QString faultsFor(const QString& device) const;

    // ---- Diagnostics ----
    ConnectionDiagnostics::Counters pgxlDiagnostics() const { return m_pgxl; }
    ConnectionDiagnostics::Counters tgxlDiagnostics() const { return m_tgxl; }
    qint64 pgxlConnectedSinceMs() const { return m_pgxl.connectedSinceMs; }
    qint64 pgxlLastRttMs() const { return m_pgxl.lastRttMs; }
    int pgxlKeepaliveMissed() const { return m_pgxl.keepaliveMissed; }
    int pgxlReconnectCount() const { return m_pgxl.reconnectCount; }
    qint64 pgxlFramesIn() const { return m_pgxl.framesIn; }
    qint64 pgxlFramesOut() const { return m_pgxl.framesOut; }
    qint64 pgxlBytesIn() const { return m_pgxl.bytesIn; }
    qint64 pgxlBytesOut() const { return m_pgxl.bytesOut; }
    qint64 pgxlLastFrameMs() const { return m_pgxl.lastFrameMs; }
    int pgxlFaultsSession() const { return m_pgxl.faultsSession; }
    qint64 tgxlConnectedSinceMs() const { return m_tgxl.connectedSinceMs; }
    qint64 tgxlLastRttMs() const { return m_tgxl.lastRttMs; }
    int tgxlKeepaliveMissed() const { return m_tgxl.keepaliveMissed; }
    int tgxlReconnectCount() const { return m_tgxl.reconnectCount; }
    qint64 tgxlFramesIn() const { return m_tgxl.framesIn; }
    qint64 tgxlFramesOut() const { return m_tgxl.framesOut; }
    qint64 tgxlBytesIn() const { return m_tgxl.bytesIn; }
    qint64 tgxlBytesOut() const { return m_tgxl.bytesOut; }
    qint64 tgxlLastFrameMs() const { return m_tgxl.lastFrameMs; }
    int tgxlFaultsSession() const { return m_tgxl.faultsSession; }
    RfKitCounters rfkitDiagnostics() const { return m_rfkit; }
    qint64 rfkitConnectedSinceMs() const { return m_rfkit.connectedSinceMs; }
    int rfkitPollsOk() const { return m_rfkit.pollsOk; }
    int rfkitPollsFailed() const { return m_rfkit.pollsFailed; }
    int rfkitReconnectCount() const { return m_rfkit.reconnectCount; }
    qint64 rfkitLastPollMs() const { return m_rfkit.lastPollMs; }
    int rfkitRttAvgMs() const { return m_rfkit.rttAvgMs; }

    // ---- Interlock ----
    InterlockMode interlockMode() const { return m_interlockMode; }
    int interlockGraceMs() const { return m_interlockGraceMs; }
    bool interlockSwrGateEnabled() const { return m_interlockSwrGateEnabled; }
    double interlockSwrGateMax() const { return m_interlockSwrGateMax; }

    // ---- Power cap ----
    PowerCap powerCap() const { return m_powerCap; }
    bool powerCapEnabled() const { return m_powerCap.enabled; }
    int powerCapW() const { return m_powerCap.watts; }
    bool powerCapExceeded() const { return m_powerCap.exceeded; }
    qint64 powerCapAlertCount() const { return m_powerCap.alertCount; }
    QString powerCapAlertText() const { return m_powerCap.alertText; }

    // ---- Tune memory ----
    QString tuneMemory() const { return m_tuneMemory; }
    bool autoTuneMemoryRecall() const { return m_autoTuneMemoryRecall; }

    // ---- Labels ----
    QStringList tgxlAntennaLabels() const { return m_tgxlLabels; }
    QStringList rfkitAntennaLabels() const { return m_rfkitLabels; }
    QString tgxlAntenna1Label() const { return m_tgxlLabels.value(0); }
    QString tgxlAntenna2Label() const { return m_tgxlLabels.value(1); }
    QString tgxlAntenna3Label() const { return m_tgxlLabels.value(2); }
    QString rfkitAntenna1Label() const { return m_rfkitLabels.value(0); }
    QString rfkitAntenna2Label() const { return m_rfkitLabels.value(1); }
    QString rfkitAntenna3Label() const { return m_rfkitLabels.value(2); }
    QString rfkitAntenna4Label() const { return m_rfkitLabels.value(3); }

    // ---- The Core (or a test) ----
    /// The three lists at once; the revision moves when any of them changed.
    void setFaults(const QString& pgxl, const QString& tgxl, const QString& rfkit);
    void setPgxlDiagnostics(const ConnectionDiagnostics::Counters& counters);
    void setTgxlDiagnostics(const ConnectionDiagnostics::Counters& counters);
    void setRfKitDiagnostics(const RfKitCounters& counters);
    void setInterlock(InterlockMode mode, int graceMs, bool swrGateEnabled, double swrGateMax);
    void setPowerCap(const PowerCap& cap);
    void setTuneMemory(const QString& json, bool autoRecall);
    void setLabels(const QStringList& tgxl, const QStringList& rfkit);

    /// A remote window: one of the Core's values arriving.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);

signals:
    void faultsChanged();
    void pgxlDiagnosticsChanged();
    void tgxlDiagnosticsChanged();
    void rfkitDiagnosticsChanged();
    void interlockChanged();
    void powerCapChanged();
    void tuneMemoryChanged();
    void labelsChanged();

private:
    static QStringList sized(const QStringList& labels, int count);
    static bool applyCounter(ConnectionDiagnostics::Counters* counters, const QByteArray& name,
                             const QVariant& value);

    qint64 m_faultRevision{0};
    QString m_pgxlFaults{QStringLiteral("[]")};
    QString m_tgxlFaults{QStringLiteral("[]")};
    QString m_rfkitFaults{QStringLiteral("[]")};
    ConnectionDiagnostics::Counters m_pgxl;
    ConnectionDiagnostics::Counters m_tgxl;
    RfKitCounters m_rfkit;
    InterlockMode m_interlockMode{InterlockMode::Disabled};
    int m_interlockGraceMs{3000};
    bool m_interlockSwrGateEnabled{false};
    double m_interlockSwrGateMax{3.0};
    PowerCap m_powerCap;
    QString m_tuneMemory{QStringLiteral("[]")};
    bool m_autoTuneMemoryRecall{false};
    QStringList m_tgxlLabels;
    QStringList m_rfkitLabels;
};

} // namespace NereusSDR
