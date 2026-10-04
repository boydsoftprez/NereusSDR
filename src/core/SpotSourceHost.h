#pragma once
// no-port-check: NereusSDR-original. Starts and stops the spot sources,
// on the Core and in a window, and is the mirrored `spotSources` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/SpotSourceHost.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Parity Task 19 (R-IOS-25, R-R3-49;
// the iPhone app plan's Task 21 station half; remote design section 6.4).
//
// Independently implemented from freedv-gui interface: this declaration
// is NereusSDR's own. The PSK Reporter start and stop in SpotSourceHost.cpp
// are ported from freedv-gui (main.cpp and reporting/pskreporter.cpp
// [@77e793a]); that file carries the upstream headers and its row in
// docs/attribution/FREEDV-GUI-PROVENANCE.md.
//
// The spot clients (RadioModel owns them) are started, stopped and
// followed here, in one place, for three placements:
//
//   - a window running its own radio: every source, as before (the Spot
//     Hub's buttons and the Auto-Connect / Auto-Start restore at startup);
//   - the Core: the station's sources (DX cluster, RBN, POTA, PSK
//     Reporter), started from the Core's own settings with no window;
//   - a remote window: its own WSJT-X and SpotCollector listeners, which
//     listen for programs on its own computer, while the station's sources
//     are the Core's: their buttons send spots.connect / spots.disconnect
//     to the Core, their state comes from the Core's `spotSources` object
//     and their console lines from the Core's spotConsole:<source> streams.
//
// Source names on the wire: dxCluster, rbn, pota, freedvReporter,
// pskReporter (the station's) and wsjtx, spotCollector (each computer's
// own). FreeDV Reporter joined with the iPhone plan's Task 22 (parity Task
// 20, stationFreedvVersion 1): the Core registers with its own callsign,
// grid and message and lists its own RADE slice, and a window's FreeDV
// Reporter dialog and FreeDV tab drive it (freedv.setMessage,
// freedv.sendQsy, freedv.setHidden).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 19, R-IOS-25,
//                                    R-R3-49): the spot sources moved out of
//                                    MainWindow and RadioModel's restore.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  iPhone plan Task 22 / parity Task 20
//                                    (R-IOS-26, R-R3-49): FreeDV Reporter
//                                    as a station source: its state, start
//                                    and stop, status message, QSY request
//                                    and "hide my station". AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Spot resolved mode (R-IOS-25): the
//                                    spot record's resolvedMode.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>

namespace NereusSDR {

class DxccColorProvider;
class DxClusterClient;
class FreeDVReporterClient;
class FreeDVStationModel;
class PotaClient;
class PskReporterClient;
class SpotCollectorClient;
class SpotModel;
class WsjtxClient;
struct SpotData;

class NEREUS_CORE_EXPORT SpotSourceHost : public QObject {
    Q_OBJECT
    // The station's sources, as the Core runs them: `off`, `connecting`,
    // `connected` or `error`, and the text a window shows beside it (the
    // error, or what the source is doing). Mirrored read-only as
    // `spotSources` (recordStreamVersion 1).
    Q_PROPERTY(QString dxClusterState READ dxClusterState NOTIFY sourcesChanged)
    Q_PROPERTY(QString dxClusterText READ dxClusterText NOTIFY sourcesChanged)
    Q_PROPERTY(QString rbnState READ rbnState NOTIFY sourcesChanged)
    Q_PROPERTY(QString rbnText READ rbnText NOTIFY sourcesChanged)
    Q_PROPERTY(QString potaState READ potaState NOTIFY sourcesChanged)
    Q_PROPERTY(QString potaText READ potaText NOTIFY sourcesChanged)
    Q_PROPERTY(QString pskReporterState READ pskReporterState NOTIFY sourcesChanged)
    Q_PROPERTY(QString pskReporterText READ pskReporterText NOTIFY sourcesChanged)
    // iPhone plan Task 22 / parity Task 20 (stationFreedvVersion 1): FreeDV
    // Reporter, the same shape, and whether the operator hid the station
    // from the FreeDV Reporter list ("Hide my station"). After the
    // others, so an older peer's property ordinals do not move.
    Q_PROPERTY(QString freedvReporterState READ freedvReporterState NOTIFY sourcesChanged)
    Q_PROPERTY(QString freedvReporterText READ freedvReporterText NOTIFY sourcesChanged)
    Q_PROPERTY(bool freedvReporterHidden READ freedvReporterHidden NOTIFY sourcesChanged)

public:
    enum class Placement {
        Everything,     // a window running its own radio
        StationSources, // the Core
        WindowSources,  // a remote window
    };

    static const QString kDxCluster;
    static const QString kRbn;
    static const QString kPota;
    static const QString kFreedvReporter;
    static const QString kPskReporter;
    static const QString kWsjtx;
    static const QString kSpotCollector;

    static const QString kOff;
    static const QString kConnecting;
    static const QString kConnected;
    static const QString kError;

    /// The sources the Core runs, in the order the Spot Hub shows them.
    static QStringList stationSources();
    /// The station sources a Core with recordStreamVersion 1 runs whatever
    /// its stationFreedvVersion (every one but FreeDV Reporter).
    static QStringList recordStreamSources();
    /// The sources each computer runs for itself.
    static QStringList windowSources();
    static bool isStationSource(const QString& source);
    static bool isKnownSource(const QString& source);
    /// The console stream for a station source: spotConsole:<source>.
    static QString consoleStream(const QString& source);

    /// The `spots` stream's record for one spot: timeUtc, frequencyHz,
    /// call, mode, source, spotter, comment, band (the Band number the
    /// catalogue uses), dxccColour ("#rrggbb", or empty when the Core does
    /// not colour it) and dxccPriority (4 a new DXCC entity, 3 a new band,
    /// 2 a new mode, 1 worked before, 0 not known or colouring off), and
    /// resolvedMode (recordStreamVersion 2: the DSPMode number
    /// SpotModeResolver::dspModeForSpot gives the spot, absent when none).
    static QJsonObject spotRecordFields(const SpotData& spot, const DxccColorProvider* dxcc);

    /// Why a window cannot write `spotSources`.
    static QString readOnlyReason();

    /// What a remote window's station-source button sends: the verb
    /// (spots.connect, spots.disconnect, spots.sendCommand, spots.clearAll),
    /// the source and the text (sendCommand only). False with the reason
    /// when it could not be sent.
    using StationForwarder = std::function<bool(const QByteArray& verb, const QString& source,
                                                const QString& text, QString* reason)>;

    SpotSourceHost(DxClusterClient* dxCluster, DxClusterClient* rbn, WsjtxClient* wsjtx,
                   SpotCollectorClient* spotCollector, PotaClient* pota,
                   PskReporterClient* pskReporter, SpotModel* spots,
                   QObject* parent = nullptr);

    /// FreeDV Reporter's client (RadioModel owns it). Set once, after
    /// construction.
    void setFreedvReporter(FreeDVReporterClient* client);
    /// The list the client feeds, cleared when the client connects and
    /// when its connection ends (where this computer runs FreeDV Reporter
    /// itself: the Core, or a window with its own radio). After
    /// setFreedvReporter.
    void setFreedvStationList(FreeDVStationModel* list);
    /// Whether the station is hidden from the FreeDV Reporter list, given
    /// whether the slice it lists is in RADE: while "Hide my station" is on
    /// or that slice is not in RADE.
    bool freedvHides(bool listedSliceInRade) const;

    /// Starts every source of this placement whose Auto-Connect or
    /// Auto-Start is on, from the saved settings (RadioModel's restore).
    void restoreAutoStart(Placement placement);

    /// The Core's spots.connect: starts a source from its saved settings.
    /// False with a reason in plain words when it cannot start.
    bool connectSource(const QString& source, QString* reason);
    /// The Core's spots.disconnect. Stopping a source that is not running
    /// changes nothing.
    bool disconnectSource(const QString& source, QString* reason);
    /// The Core's spots.sendCommand: a line typed into a cluster console.
    bool sendCommand(const QString& source, const QString& text, QString* reason);
    /// The Core's spots.clearAll: every spot the Core holds.
    void clearAll();

    // ── FreeDV Reporter (iPhone plan Task 22, stationFreedvVersion 1) ────
    /// The callsign FreeDV Reporter registers with: FreeDvReporter/Callsign,
    /// else User/Callsign, else StationCallsign. Never the Core's label
    /// (R-IOS-26).
    static QString freedvCallsign();
    /// The grid square: FreeDvReporter/GridSquare, else User/GridSquare.
    static QString freedvGridSquare();
    /// The Core's freedv.setMessage: the status message shown beside the
    /// station on the list.
    bool setFreedvMessage(const QString& text, QString* reason);
    /// The Core's freedv.sendQsy: asks the station listed with `callsign`
    /// to move to `frequencyHz`.
    bool sendFreedvQsy(const QString& callsign, qint64 frequencyHz, QString* reason);
    /// The Core's freedv.setHidden: "Hide my station" (saved as
    /// FreeDvReporter/Hidden). The station is shown only while it is not
    /// hidden and its reported slice is in RADE (RadioModel).
    bool setFreedvHidden(bool on, QString* reason);
    bool freedvReporterHidden() const;

    QString state(const QString& source) const;
    QString text(const QString& source) const;
    bool isRunning(const QString& source) const;

    QString dxClusterState() const { return state(kDxCluster); }
    QString dxClusterText() const { return text(kDxCluster); }
    QString rbnState() const { return state(kRbn); }
    QString rbnText() const { return text(kRbn); }
    QString potaState() const { return state(kPota); }
    QString potaText() const { return text(kPota); }
    QString freedvReporterState() const { return state(kFreedvReporter); }
    QString freedvReporterText() const { return text(kFreedvReporter); }
    QString pskReporterState() const { return state(kPskReporter); }
    QString pskReporterText() const { return text(kPskReporter); }

    // ── A remote window ─────────────────────────────────────────────────
    /// The station's sources are the Core's from now on (a remote window).
    void setStationForwarder(StationForwarder forwarder);
    bool forwardsStationSources() const { return static_cast<bool>(m_forwarder); }
    /// One of the Core's `spotSources` values arriving.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);
    /// No Core (session ended): the station's sources read off.
    void clearStationValues();
    /// Console lines from the Core's spotConsole:<source> stream. With
    /// `replace` (the stream's reset: its backlog again, on each subscribe)
    /// the console is cleared first, so a reconnect never repeats it.
    void appendStationConsole(const QString& source, const QStringList& lines,
                              bool replace = false);
    /// The Core refused a request for a source.
    void reportStationRefusal(const QString& source, const QString& reason);

    /// What a remote window's FreeDV Reporter requests send: the verb
    /// (freedv.setMessage, freedv.sendQsy, freedv.setHidden) and its
    /// arguments. False with the reason when it could not be sent.
    using FreedvForwarder = std::function<bool(const QByteArray& verb, const QVariantMap& args,
                                               QString* reason)>;
    void setFreedvForwarder(FreedvForwarder forwarder);

public slots:
    // The Spot Hub's buttons (each tab saves its settings first). In a
    // remote window the station's sources are sent to the Core.
    void connectCluster(const QString& host, quint16 port, const QString& callsign);
    void disconnectCluster();
    void connectRbn(const QString& host, quint16 port, const QString& callsign);
    void disconnectRbn();
    void startWsjtx(const QString& address, quint16 port);
    void stopWsjtx();
    void startSpotCollector(quint16 port);
    void stopSpotCollector();
    void startPota(int intervalSec);
    void stopPota();
    void startPskReporter(const QString& callsign, const QString& gridSquare);
    void stopPskReporter();
    /// FreeDV Reporter's Start and Stop (the FreeDV tab), from the saved
    /// identity. The Core's in a remote window.
    void startFreedvReporter();
    void stopFreedvReporter();
    /// The FreeDV Reporter dialog's Send and Clear, Send QSY and the FreeDV
    /// tab's "Hide my station". The Core's in a remote window; a refusal
    /// comes back as sourceRefused(kFreedvReporter, reason).
    void sendFreedvMessage(const QString& text);
    void requestFreedvQsy(const QString& callsign, qint64 frequencyHz);
    void hideFreedvStation(bool on);
    /// A line typed into a cluster console (sent to the Core in a remote
    /// window). The refusal, if any, comes back as sourceRefused.
    void typeCommand(const QString& source, const QString& text);
    /// The Spot Hub's Clear All Spots: this window's spots, and in a
    /// remote window the Core's as well.
    void clearAllSpots();

signals:
    void sourcesChanged();
    void sourceChanged(const QString& source);
    void consoleLine(const QString& source, const QString& line);
    /// The Core's console for a source starts again (its backlog follows).
    void consoleCleared(const QString& source);
    void sourceRefused(const QString& source, const QString& reason);
    /// "Hide my station" changed (the station shows or hides at once).
    void freedvHiddenChanged(bool hidden);

private:
    struct SourceState {
        QString state;
        QString text;
    };

    void setSource(const QString& source, const QString& state, const QString& text = {});
    bool forward(const QByteArray& verb, const QString& source, const QString& text = {});
    void startPskReporterWith(const QString& callsign, const QString& gridSquare);
    bool startFreedvWith(QString* reason);
    bool forwardFreedv(const QByteArray& verb, const QVariantMap& args);

    QPointer<DxClusterClient> m_dxCluster;
    QPointer<DxClusterClient> m_rbn;
    QPointer<WsjtxClient> m_wsjtx;
    QPointer<SpotCollectorClient> m_spotCollector;
    QPointer<PotaClient> m_pota;
    QPointer<PskReporterClient> m_pskReporter;
    QPointer<FreeDVReporterClient> m_freedv;
    QPointer<FreeDVStationModel> m_freedvList;
    QPointer<SpotModel> m_spots;
    QHash<QString, SourceState> m_local;
    QHash<QString, SourceState> m_station;
    StationForwarder m_forwarder;
    FreedvForwarder m_freedvForwarder;
    bool m_freedvHidden = false;
    bool m_stationFreedvHidden = false;
};

} // namespace NereusSDR
