#pragma once

// =================================================================
// src/gui/widgets/RotorLogbookPanel.h  (Longpath)
// =================================================================
//
// Longpath-original. Thetis has neither a rotator display nor a
// logbook; AetherSDR's callsign card is lookup-only.
//
// Extracted from MainWindow::openRotorDial(), where the same UI had
// grown into a 200-line function of nested lambdas capturing each
// other's widgets. Everything below is the same behaviour, moved
// somewhere it can grow — and the move is what lets the panel be
// docked rather than living in a window of its own.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "core/CallsignInfo.h"
#include "core/RotctldProcess.h"
#include "core/WorkedBefore.h"
#include "models/LogEntry.h"

#include <QHash>
#include <QPointer>
#include <QVector>
#include <QWidget>

#include <limits>

class QAction;
class QTimer;

class QLabel;
namespace Longpath { class StationPhoto; }
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QResizeEvent;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

namespace Longpath {

class QrzClient;
class QrzLogbookUploader;
class QsoUploader;
class RotorController;
class RotctldClient;
class RadioModel;
class GlobeWidget;
class LogbookWindow;
class RotorDialWidget;

// Aim the antenna and log the contact, in one surface.
//
// The two belong together: the callsign you are about to log is the
// same one you just turned the antenna towards, and both need the same
// locator, distance and bearing. Splitting them would mean typing the
// callsign twice.
//
// None of the three collaborators is owned, and all may be null — the
// panel degrades to manual locators and local-only logging rather than
// refusing to appear.
class RotorLogbookPanel : public QWidget {
    Q_OBJECT
public:
    RotorLogbookPanel(RadioModel* radio, QrzClient* qrz,
                      QrzLogbookUploader* uploader,
                      QWidget* parent = nullptr);
    // Eigener Destruktor, weil der Abbau sonst in schon abgeraeumte
    // Kinder greift: ~QWidget loescht den Rotctld-Client, dessen
    // Socket meldet beim Abbau noch disconnected(), und der Rueckruf
    // schreibt ins Statusschild, das als frueheres Kind bereits weg
    // ist. Sechs Abstuerze beim Beenden, 2026-10-10/11. Siehe
    // tests/tst_rotor_panel_abbau.cpp.
    ~RotorLogbookPanel() override;

    // Where the ADIF log is written.
    static QString logbookPath();

    /// Der Zeiger selbst. Damit MainWindow ihn in ein durchsichtiges
    /// Bild malen und dem Panadapter geben kann (siehe
    /// SpectrumWidget::setCompassOverlay) — das Panel behaelt dabei
    /// die Hoheit ueber seinen Zeiger, es gibt nur Einblick.
    RotorDialWidget* dial() const { return m_dial; }

    // Destinations offered by the logbook window's Upload menu. Passed
    // straight through — the panel does not own them and does not use
    // them itself; live logging still goes to the single uploader given
    // to the constructor.
    void setUploadTargets(const QVector<QsoUploader*>& targets);

    /// Satelliten in Sicht: beim Loggen wird der Eintrag mit den
    /// Satelliten gestempelt, die zum QSO-Zeitpunkt ueber dem eigenen
    /// Horizont standen (ADIF APP_LONGPATH_SATS); das Logbuchfenster
    /// bekommt den Dienst fuer die Karte weitergereicht.
    void setSatellites(class SatelliteService* svc);
    /// Der Eintrag, wie „Log" ihn schreiben wuerde — fuer Pruefstaende.
    LogEntry buildEntryForTest() const { return buildEntry(); }

    // Open the logbook window. Public so the Tools menu can reach it
    // without the operator having to find the dock first — but still
    // the panel's window, so there is only ever one of them over one
    // file.
    void showLogbook();

    // Hide the logbook window without destroying it -- counterpart to
    // showLogbook(). 2026-08-27: applyWindowVisibility("WinLogbook", false)
    // used to be a no-op ("the logbook closes itself"), which is true
    // only for the operator clicking the window's own close button. A
    // layout profile switch that turns WinLogbook's visibility off (e.g.
    // activating a fresh, deliberately empty profile while the logbook
    // is open from a previous one) needs an actual hide to call, or the
    // window keeps showing on top of a profile that never opened it.
    // No-op if the window was never created.
    void hideLogbook();
    // Das offene Logbuch nach vorne holen, ohne es neu einzulesen (Start:
    // Panadapter und Rotor/Log gehen nach ihm auf und lagen davor).
    void raiseLogbookIfOpen();
    // Das Logbuchfenster, solange eines angelegt ist -- fuer den
    // Aufrufer, der es auf den Arbeitsschirm holt
    // (MainWindow::openLogbookWindow, 2026-11-15: „wenn ich das logbuch
    // aufmache mit dem plus sehe ich es nur am macbook"). Nullptr,
    // bevor es das erste Mal geoeffnet wurde.
    // Als QWidget*, nicht als LogbookWindow*: der Aufrufer will es
    // platzieren und nach vorne holen, nichts vom Logbuch selbst. So
    // braucht er LogbookWindow.h nicht (MainWindow.cpp kennt die Klasse
    // nur vorangemeldet). Rumpf in der .cpp, wo sie vollstaendig ist.
    QWidget* logbuchFenster() const;
    // Den Rotor mit den gespeicherten Einstellungen verbinden, wie
    // Rotor... -> Connect. Drei Wege: der eigene GS-232-Treiber
    // (`Rotor/Backend=gs232`, braucht KEIN rotctld), ein lokal
    // gestartetes rotctld, oder ein rotctld im Netz. false + Grund, wenn
    // nichts eingerichtet ist oder rotctld nicht startet.
    bool connectRotorFromSettings(QString* err = nullptr);

    // Open the rotator setup dialog. Public for the same reason as
    // showLogbook(): an operator whose rotator is not working yet
    // should not have to discover the dock in order to find the place
    // that installs Hamlib and connects the controller.
    void showRotorSetup();

    // 2026-08-10: a spot elsewhere in the app named a station — point
    // the panel at it and, if a bearing can be computed, start the
    // turn. Setting the callsign runs the panel's whole existing
    // pipeline (worked-before line, prefix bearing, QRZ auto-lookup),
    // so this is one line of behaviour, not a second code path.
    void workSpot(const QString& call);

    /// Ein gespottetes Rufzeichen uebernehmen, OHNE den Mast zu drehen.
    ///
    /// Auf Ansage des Betreibers (2026-08-19): Doppelklick auf das
    /// Rufzeichen im Panadapter soll das Log oeffnen, die
    /// Rotor-Einstellungen zeigen und den ZEIGER auf die Zielposition
    /// stellen — „so sehe ich sofort, ob der Rotor in diese Richtung
    /// steht". Genau darum wird hier nicht gedreht: der Vergleich
    /// zwischen Ist und Ziel ist der Zweck.
    ///
    /// Unterschied zu workSpot(): das ist der Rechtsklick-Weg „Turn
    /// rotor to <call>", der ausdruecklich dreht.
    ///
    /// Alles Weitere haengt schon am Rufzeichenfeld: Land und Flagge aus
    /// cty.dat, Zielpeilung aus den Koordinaten der DXCC-Einheit (ohne
    /// Netz), Entfernung in der Statuszeile, und die QRZ-Abfrage
    /// verfeinert es, sobald sie antwortet.
    void takeSpot(const QString& call);

    // 2026-08-10: a contact logged by another program (WSJT-X's
    // Logged-ADIF message). Written to the same file through the same
    // duplicate check and upload path as a hand-logged QSO — one log,
    // however the contact arrived.
    void logExternalQso(const LogEntry& entry);

signals:
    void qsoLogged(const LogEntry& entry);
    // Der Betreiber hat das Logbuchfenster selbst zugemacht (roter Knopf).
    // hideLogbook() loest es NICHT aus -- nur QDialog::finished, also ein
    // echtes Schliessen, kein Verstecken durch Profil oder Verbinden-Maske.
    void logbookClosed();

protected:
    // Every resize re-decides which rows still fit — see
    // updateCompactness().
    void resizeEvent(QResizeEvent* ev) override;

private:
    void buildUi();
    void wireQrz();

    void onCallsignEdited(const QString& raw);   // country estimate
    void onLookupRequested();                    // QRZ, on demand
    // Start the clock on an automatic QRZ lookup. Debounced, because
    // firing on every keystroke would send four requests for one
    // callsign and get the answers back out of order.
    void scheduleAutoLookup(const QString& call);
    // Take a locator from a QRZ result into the DX field, unless the
    // operator has typed one themselves.
    void adoptGridFromQrz(const CallsignInfo& info);
    void onLogQso();
    // Rotator: set up the link, and start or stop a turn. With nothing
    // connected these fall back to the simulated needle, so the dial
    // stays demonstrable — but the status line says which it is.
    void openRotorSetupDialog();
    void ensureRotor();
    /// Den Rotortreiber wechseln (rotctld <-> GS-232 direkt), waehrend
    /// das Fenster offen ist -- siehe RotorLogbookPanel.cpp.
    void rebuildRotorBackend();
    void beginTurn();
    void haltTurn();
    // Hide rows from the bottom of the priority list as height runs
    // out, so the compass survives being dragged small.
    void updateCompactness();
    void applyLocators();
    void refreshRecentList();
    // Note in the log file that this contact reached QRZ.
    void markUploaded(const QString& call);
    void openLogbookWindow();
    void setStatus(const QString& text, bool warn = false);

    // ── Teachable presets, park, long path (2026-08-11) ──────────────
    //
    // Four slots the operator teaches by right-click — "EU", "JA", the
    // repeater — plus a park position and a long-path flip. All of them
    // AIM; none of them turns the mast. That contract comes from the
    // cardinal preset row above them and holds for every control on
    // this panel: the turn lives behind Rotate, always.
    // Where a right-click teach would point: the dial's current aim if
    // one is set, else the rotator's fresh reading. False when neither
    // exists — teaching from a stale needle would store a lie.
    bool teachableBearing(double* outDeg) const;
    void contextMenuEvent(QContextMenuEvent* ev) override;

public:
#ifdef LONGPATH_BUILD_TESTS
    /// Prüfstandsnaht: den Rotor anlegen, ohne dass jemand auf
    /// "Verbinden" klickt — damit ein Lauf belegen kann, WELCHER
    /// Treiber aus der Einstellung `Rotor/Backend` entsteht.
    void ensureRotorForTest() { ensureRotor(); }
    RotorController* rotorForTest() const { return m_rotor; }
    /// Prüfstandsnaht zum Treiberwechsel, den der Verbinden-Knopf im
    /// Rotordialog auslöst.
    void rebuildRotorBackendForTest() { rebuildRotorBackend(); }
    /// Prüfstandsnaht: läuft ein rotctld-Prozess? Der eigene
    /// GS-232-Treiber braucht keinen, und genau das ist die Aussage,
    /// die ein Lauf festhalten können muss.
    bool rotctldRunningForTest() const { return m_rotorProc.isRunning(); }
    /// Prüfstandsnaht: den Rotordialog öffnen (modal; der Lauf schließt
    /// ihn selbst wieder). 2026-11-11, Netz-Rotor im falschen Modus.
    void openRotorSetupDialogForTest() { openRotorSetupDialog(); }
#endif

    /// Das Ziel-Menue bauen, ohne es aufzuklappen. Fuer Pruefungen:
    /// exec() haelt die Runde an.
    void buildAimMenu(QMenu& menu, const QPoint& globalPos);
private:
    void userPresetMenu(int slot, const QPoint& globalPos);
    void parkMenu(const QPoint& globalPos);
    // Fetch and show the QRZ portrait. Cached on disk so working the
    // same station twice costs one request, not two.
    void showStationVisuals(const CallsignInfo& info);
    // Flag emoji for the callsign's DXCC prefix, or empty. Kept as a
    // separate step from displaying it so the lookup can be tested and
    // so the flag survives a station line being rewritten.
    void updateFlagFor(const QString& call);
    // Station line, with the current flag in front of it.
    void setStationLine(const QString& text);
    // Name, country and distance from a lookup answer, one line
    // (2026-09-01 — trimmed from the earlier two-line everything-it-
    // carries version once the portrait grew large enough to want the
    // room). One place, so the fresh-answer and the cached-answer paths
    // cannot drift apart.
    QString stationText(const CallsignInfo& info) const;
    // "New DXCC" / "worked 3x, last 2024-05-02" for the callsign being
    // typed, on the current band and mode.
    void updateWorkedLine(const QString& call);
    // Current band and mode as the log would record them, so what the
    // worked-before line answers is the same question logging asks.
    void currentBandMode(QString& band, QString& mode) const;
    // Point the globe at home and, if known, at the worked station.
    void updateGlobeFromLocators();
    // Sunrise, sunset and grey line at the far end. Refreshed on a
    // timer as well as on edit, because the sun moves while you talk.
    void updateSolarLine();
    // Ask for an equirectangular world image and remember the choice.
    void chooseWorldImage();
    // Fetch one from NASA. Public-domain imagery, so nothing has to be
    // bundled and nothing has to be agreed to. Tries the candidates in
    // order — deep image paths move, and one dead link should cost a
    // second, not the whole feature.
    void downloadWorldImage(const QStringList& candidates, int index,
                            const QString& label);
    bool appendToLogFile(const LogEntry& entry, QString* error);
    LogEntry buildEntry() const;
    // Das Geruest jedes neuen QSOs: Rufzeichen, Zeit, eigener Locator,
    // und Frequenz / Band / Betriebsart vom Funkgeraet (2026-09-26, damit
    // die Eingabezeile im Logbuch dasselbe loggt wie dieses Feld).
    LogEntry draftFromRadio(const QString& call) const;
    // Der eine Weg ins Log: Entfernung/Peilung, Satellitenstempel, die
    // Doppelt-Frage (an askParent), Datei, Hochladen, Liste, qsoLogged.
    // false, wenn nicht geloggt; *message sagt dann warum.
    bool commitEntry(LogEntry e, QWidget* askParent, QString* message);
    // Ein QSO aus der Eingabezeile des Logbuch-Fensters.
    void logFromLogbook(const LogEntry& partial);
    void stampSatellites(LogEntry& e) const;

    // Letzte echte Ablesung des Rotors, NaN solange keiner verbunden ist.
    double              m_lastRotorAz{std::numeric_limits<double>::quiet_NaN()};
    RadioModel*         m_radio{nullptr};
    QrzClient*          m_qrz{nullptr};
    class SatelliteService* m_satellites{nullptr};
    QrzLogbookUploader* m_uploader{nullptr};

    RotorDialWidget* m_dial{nullptr};
    GlobeWidget*     m_globe{nullptr};
    QStackedWidget*  m_viewStack{nullptr};
    class QAction*   m_globeBtn{nullptr};   // Rose/Kugel, hinter dem Zahnrad
    StationPhoto*    m_photo{nullptr};
    QString          m_flagEmoji;
    // Created on first portrait fetch — a panel that never looks anyone
    // up should not open a network stack.
    QNetworkAccessManager* m_net{nullptr};
    QLineEdit* m_callEdit{nullptr};
    QLineEdit* m_myGrid{nullptr};
    QLineEdit* m_dxGrid{nullptr};
    QLineEdit* m_rstSent{nullptr};
    QLineEdit* m_rstRcvd{nullptr};
    QLineEdit* m_comment{nullptr};
    QLabel*    m_status{nullptr};
    QLabel*    m_stationLine{nullptr};
    QLabel*    m_solarLine{nullptr};
    QLabel*    m_workedLine{nullptr};

    // Rebuilt whenever the log file changes, so the answer beside a
    // callsign is never older than the last contact written.
    WorkedBefore m_worked;
    QTableWidget* m_recent{nullptr};
    QPushButton*  m_lookupBtn{nullptr};

    // Created on first use and kept. Two windows over one file is how
    // one of them ends up writing over the other's correction.
    QPointer<LogbookWindow> m_logWindow;
    QVector<QsoUploader*> m_uploadTargets;

    // The rotator link, and the timer that stands in for one. Exactly
    // one of the two drives the needle at any moment.
    // Der Rotor, wie ihn dieses Fenster bedient: seit 2026-10-21 die
    // Schnittstelle, nicht mehr der rotctld-Weg allein. m_rotctld zeigt
    // auf DASSELBE Objekt, wenn der rotctld-Weg gewaehlt ist, und ist
    // sonst leer — alles, was nur rotctld kann (Adresse, Port, der
    // Wachhund auf den Dienst), haengt an diesem Zeiger und nicht an
    // m_rotor.
    RotorController* m_rotor{nullptr};
    RotctldClient*   m_rotctld{nullptr};
    // Der zuletzt protokollierte Status -- damit nur WECHSEL ins
    // Protokoll gehen, nicht jede Wiederholung (siehe setStatus()).
    QString        m_letzterStatus;
    // rotctld started by us, when the controller is on this computer.
    // Owned so it is stopped on the way out rather than left holding
    // the serial port.
    RotctldProcess m_rotorProc;
    QTimer*        m_simTimer{nullptr};

    // Best known position of the far end, from whichever source spoke
    // last: a typed locator, a QRZ grid, or the DXCC entity centre
    // resolved from the prefix. Kept separately from the locator field
    // so the grey line still works while only a prefix is known.
    double m_dxLat{0.0};
    double m_dxLon{0.0};
    bool   m_hasDxPos{false};

    // Detail from the last successful QRZ lookup. Only folded into a
    // logged contact when it belongs to the callsign being logged —
    // never a leftover card from the previous station.
    CallsignInfo m_lastInfo;

    // The contact just written, so a later upload result can be
    // matched to the right record rather than to every past QSO
    // with the same station.
    LogEntry m_lastLogged;

    // Automatic lookup while typing. The timer debounces; the cache
    // means working a station twice in a contest, or backspacing over
    // a callsign and retyping it, costs one request rather than
    // several — which matters when a subscription is metered and a
    // contest is three hundred callsigns.
    QTimer* m_lookupTimer{nullptr};
    QHash<QString, CallsignInfo> m_qrzCache;

    // True while a QRZ grid is being written into the DX field, so the
    // field's own textChanged does not treat it as the operator typing.
    bool m_adoptingGrid{false};
    // Set when the operator edits the locator by hand. A later QRZ
    // answer must not overwrite what a person deliberately entered.
    bool m_dxGridIsManual{false};

    // ── Shrinking down to the compass (2026-08-10) ───────────────────
    //
    // A panel with ten rows has a minimum height of all ten, and the
    // dial was what got squeezed when the dock got narrow. Backwards:
    // the dial is the one part that is readable at a glance from across
    // the room.
    //
    // These rows are hidden from the front of the list as height runs
    // out, so the compass keeps whatever is left. Wrapper widgets
    // rather than the layouts themselves, because QLayout has no
    // setVisible().
    // ── Entwurf R (2026-11-12) ──────────────────────────────────────
    class QPushButton*     m_optionenBtn{nullptr};
    class QPushButton*     m_rotateBtn{nullptr};
    class QPushButton*     m_lpBtn{nullptr};
    class RotorBandWidget* m_rotorBand{nullptr};
    QLabel*                m_roKurz{nullptr};
    QLabel*                m_roLang{nullptr};
    QLabel*                m_roWeite{nullptr};
    double                 m_kurzGrad{-1.0};   // Kurzweg zur Station, < 0: keine
    void zeigeOptionen();
    void zeigeWeltbildMenue(const QPoint& at);
    void aktualisiereAblesung(double kurzGrad, double km);
    void aktualisiereDrehknoepfe();
    QWidget* m_rowCall{nullptr};
    QWidget* m_rowCard{nullptr};
    QWidget* m_rowGrid{nullptr};
    QWidget* m_rowBtn{nullptr};
    QWidget* m_rowRst{nullptr};
    QWidget* m_rowLog{nullptr};

    // Teachable preset slots + park + long-path flip (2026-08-11).
    static constexpr int kUserPresetSlots = 4;

    QVBoxLayout*     m_column{nullptr};
    QVector<QWidget*> m_shedOrder;      // least useful first
    // Measured while each row was last visible. A hidden widget's hint
    // is still valid, but a row laid out for the first time reports one
    // before its children have theirs.
    QVector<int>      m_shedHeights;
};

} // namespace Longpath
