#pragma once

// =================================================================
// src/gui/LogbookWindow.h  (Longpath)
// =================================================================
//
// Longpath-original. Thetis has no logbook at all.
//
// The dock panel shows the last twelve contacts, which is enough to
// confirm a write went through and nothing more. This is the log you
// actually go looking in: search it, fix the callsign you fat-fingered,
// export it, and see what you have worked.
//
// It reads the ADIF file rather than holding a model in memory. The
// file is the log; anything cached beside it is a second version of the
// truth waiting to disagree.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Statistics view (per band / mode / year, unique calls
//                 and squares, furthest DX), Cabrillo skeleton export,
//                 and the Band column coloured by band so a mixed log
//                 can be scanned. AI-assisted via Anthropic Claude
//                 (Cowork), operator Martin Fischer.
//   2026-08-27 — Window position/size persistence (closeEvent +
//                 saveGeometryState/restoreGeometryState), matching the
//                 existing header/splitter-state pattern. Previously the
//                 window always reopened at Qt's default QDialog(parent)
//                 placement with no memory of where it was left.
//                 AI-assisted via Anthropic Claude, operator Martin Fischer.
//   2026-09-21 — QRZ-Logbuch abholen (syncFromQrz): FETCH ueber den
//                 Schluessel des Uploaders, Zusammenfuehren wie ein
//                 Datei-Import (importEntries). Martin Fischer,
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/CallsignCache.h"
#include "core/LogFilter.h"
#include "models/LogEntry.h"
#include "core/SotaNachtrag.h"

#include <QDialog>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QVector>

#include <functional>

class QAction;
class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QMoveEvent;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTableWidget;
class QTimer;
class QVBoxLayout;

namespace Longpath {

class QrzClient;
class QsoDetailPane;
class RadioModel;
class QsoUploader;
class QrzLogbookUploader;
class QrzLogbookFetcher;

class WindowTitleBar;

class LogbookWindow : public QDialog {
    Q_OBJECT
public:
    explicit LogbookWindow(const QString& adifPath, QWidget* parent = nullptr);

    // Re-read the file. Called when the panel logs a contact so the two
    // views cannot drift apart.
    void reload();

protected:
    // 2026-08-27: unlike every ContainerWidget-based floating panel,
    // this window never saved or restored its own position/size — it
    // always reopened wherever Qt's default QDialog(parent) placement
    // policy put it. Standard Qt idiom: capture on close, apply on
    // construction (see restoreGeometryState() call in the constructor).
    void closeEvent(QCloseEvent* event) override;

    // 2026-08-31: closeEvent() alone only caught a CLEAN close. A crash
    // or force-quit between a drag/resize and the next clean close lost
    // that session's move — the same gap found and fixed tonight for
    // ToolWindow (Rotor/Log), and PanFloatingWindow/AppletFloatingWindow
    // already avoid it by saving on every move/resize, not just on
    // close.
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

public:
    // 2026-09-26: beim ersten Zeigen die vorgesehene Groesse (gespeichert
    // oder vom Aufrufer gesetzt) noch einmal anwenden, sobald die
    // umbrechenden Leisten ihre wirkliche Breite kennen -- siehe .cpp.
    void setVisible(bool visible) override;


private:
    // Eigene Titelleiste statt des Systemrahmens -- das Logbuch war das
    // letzte Fenster mit runden Ecken des Betriebssystems, waehrend alle
    // anderen (Rotor, abgeloeste Applets, Panadapter) laengst rahmenlos
    // mit WindowChrome laufen. Betreiber am 2026-10-06: "die fensterecken
    // sind rund, alle anderen widget sind doch eckig?!?!"
    WindowTitleBar* m_titleBar{nullptr};

private:
    bool m_shownOnce{false};
    bool m_geometryRestored{false};
    // Einschwingen nach dem ersten Zeigen (2026-09-27): das Ziel der
    // Nachtakte. Beginnt bei der vorgesehenen Groesse; jede Verkleinerung
    // in dieser Zeit ist gewollt (Qt verkleinert nie von selbst) und wird
    // das neue Ziel, eine Vergroesserung nie.
    bool  m_settling{false};
    QSize m_settleTarget;

    // ── Das Einschwingen laeuft, bis es wirklich eingeschwungen ist ──
    //
    // Bis zum 2026-10-16 standen hier drei feste Takte (50, 300,
    // 1000 ms). Jeder nahm zurueck bis
    // m_settleTarget.expandedTo(minimumSizeHint()) — und wenn die
    // Mindesthoehe beim LETZTEN davon noch nicht eingeschwungen war,
    // war diese Grenze selbst der aufgezogene Wert. Dann unterblieb
    // die Korrektur, und danach lief kein Takt mehr: das Fenster blieb
    // aufgezogen. Im Durchgang vom 2026-10-16 stand es unter -j8-Last
    // auf 780 statt 700 (einzeln gruen).
    //
    // Jetzt ein laufender Takt: er korrigiert alle kSettleTickMs und
    // hoert erst auf, wenn die Mindestgroesse zwei Messungen lang
    // gleich geblieben ist UND nichts mehr zurueckzunehmen war —
    // fruehestens aber nach kSettleMinMs, weil die erste Sekunde dem
    // Betreiber gehoert (wer in ihr kleiner zieht, behaelt es), und
    // spaetestens nach kSettleMaxMs, damit ein Fenster nicht ewig
    // gefangen bleibt.
    static constexpr int kSettleTickMs = 50;    // wie oft nachgesehen wird
    static constexpr int kSettleMinMs  = 1000;  // so lange gehoert die Zeit dem Betreiber
    static constexpr int kSettleMaxMs  = 3000;  // und dann ist Schluss
    QTimer*        m_settleTimer{nullptr};
    QElapsedTimer  m_settleUhr;
    QSize          m_settleLastHint;
    QRect          m_settleIntended;
    bool           m_settleWithPosition{false};
    void onSettleTick();
    void stopSettling();

private:
    void showRowMenu(const QPoint& pos);
    void saveGeometryState();
    void restoreGeometryState();

public:

    // Places selected contacts can be sent. Not owned — the window is
    // one of several users of the same uploaders, and whoever holds the
    // credentials holds the objects.
    void setUploaders(const QVector<QsoUploader*>& uploaders);

private slots:
    /// Eine echte Slot-Methode, damit Qt::UniqueConnection greift --
    /// bei einem Lambda tut die Fahne schweigend nichts.
    void onUploadFinished(const QString& call, bool ok, bool duplicate,
                          const QString& message);

public:

    // Lets the detail pane fetch a name and a portrait for the selected
    // contact. Not owned, and optional: the logbook is opened on
    // machines with no QRZ account, where the pane simply says so.
    void setQrzClient(QrzClient* qrz);
    /// Satelliten ueber dem Horizont auf der Karte; der Dienst gehoert
    /// dem Hauptfenster, hier nur weitergereicht.
    void setSatellites(class SatelliteService* svc);

    // cty.dat for the Kennzahlen (entity per contact, DXCC / continent /
    // zone). Optional: without it the ADIF DXCC field is all there is.
    // Reicht es auch an das Detailpaneel weiter (Landesmitte fuer die
    // Grauzone, wenn der Locator fehlt).
    void setCtyDat(const class CtyDatParser* cty);

signals:
    // ── Point the beam at a contact ──────────────────────────────────
    //
    // Every entry already carries the bearing to it, and the rotor
    // panel that owns this window can already turn. The two have never
    // been connected, so an operator wanting to work a station again
    // read the bearing off the screen and typed it into the dial.
    //
    // A signal rather than a direct call because this window must not
    // know about a rotor: it is opened on machines that have none, and
    // a logbook that needs a rotor to compile is a logbook with a
    // rotor-shaped hole in it.
    void turnRotorRequested(double bearingDeg, const QString& call);

    // ── Loggen aus dem Logbuch (2026-09-26) ──────────────────────────
    //
    // Die Eingabezeile oben gibt nur, was dort steht (Rufzeichen, RST,
    // Kommentar) und was QRZ zur Station weiss. Frequenz, Band,
    // Betriebsart, Zeit und eigener Locator setzt das Rotor/Log-Feld,
    // das auch sonst loggt -- eine Datei, eine Doppelt-Regel, ein
    // Hochladen. Die Antwort kommt ueber reportLogged().
    void logQsoRequested(const LogEntry& partial);

public:

    // Passed straight to the map: the centre of a contact's DXCC entity,
    // for contacts that carry no locator. cty.dat lives with the radio
    // model, which this window has no handle on.
    using PositionFallback =
        std::function<bool(const QString& call, double& lat, double& lon)>;
    void setPositionFallback(PositionFallback fn);

    // Frequenz und Betriebsart fuer die Eingabezeile, live vom
    // Funkgeraet. Ohne Funkgeraet zeigt die Zeile "—".
    void setRadio(RadioModel* radio);
    // Ergebnis von logQsoRequested(): Meldung in der Zeile; bei Erfolg
    // neu laden und die Zeile fuer die naechste Station leeren.
    void reportLogged(bool ok, const QString& message);

    // Wo der Rotor gerade steht (NaN = unbekannt / kein Rotor) und wie
    // breit der Strahl ist. Geht an die Karteikarte ("Rotor at ...") und
    // an das Rotor-Radar neben der Karte (2026-09-26).
    /// Ein Rufzeichen von aussen, etwa per Doppelklick auf einen Spot im
    /// Panadapter (2026-11-12): wie getippt -- Eingabezeile, Filter,
    /// Karteikarte mit QRZ, Karte, Bedarf -- nur ohne die Tipppause.
    void uebernimmRufzeichen(const QString& rufzeichen);
    void setRotorBearing(double deg);
    void setRotorBeamWidth(double deg);
    // Was der Rotor zu einem Drehbefehl sagt: Ablehnung (erlaubter
    // Bereich), Transportfehler (RotorController::errorOccurred). Nach
    // Rotate/LP aus der Eingabezeile steht es dort (2026-11-11).
    void setRotorMeldung(const QString& text);

    QLineEdit*   entryCallForTest() const { return m_entryCall; }
    QLineEdit*   entryCommentForTest() const { return m_entryComment; }
    QLabel*      entryFreqForTest() const { return m_entryFreq; }
    QLabel*      entryModeForTest() const { return m_entryMode; }
    QLabel*      entryHintForTest() const { return m_entryHint; }
    QPushButton* entryLogButtonForTest() const { return m_entryLogBtn; }
    QPushButton* entryRotorForTest() const { return m_entryRotorBtn; }
    QPushButton* entryRotateForTest() const { return m_entryRotate; }
    QPushButton* entryLongPathForTest() const { return m_entryLp; }
    QLabel*      entryPeilungForTest() const { return m_entryPeilung; }
    QLabel*      entryDrehForTest() const { return m_entryDreh; }
    class RotorBandWidget* rotorBandForTest() const { return m_rotorBand; }
    QWidget*     rotorZeileForTest() const { return m_rotorZeileW; }
    void         setStillstandMsForTest(int ms) { m_stillstandMs = ms; }
    class QsoDetailPane* detailForTest() const { return m_detail; }
    QLineEdit*   searchForTest() const { return m_search; }

signals:
    // The file changed underneath other views (edit or delete).
    void logChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void buildFilterBar(QVBoxLayout* col);
    void buildEntryRow(QVBoxLayout* col);
    void rewireSlice();
    void refreshRadioReadout();
    void requestLog();
    void clearEntry();
    // Rufzeichen in der Karteikarte zeigen: der neueste Kontakt, wenn er
    // im Log steht, sonst was QRZ ueber ihn weiss.
    void showStationFor(const QString& call);
    // Refill the band and mode lists from what is actually in the log,
    // keeping the current choice if it is still there. Offering every
    // band in existence would make most of the list a way to get no
    // results.
    void refreshFilterChoices();
    LogFilter currentFilter() const;
    void applyFilter();
    // Order m_visible by the current sort column. Done here rather than
    // by QTableWidget's own sorting, which reorders rows underneath the
    // index mapping and would make every row action touch the wrong
    // contact.
    void applySort();
    void saveHeaderState();
    void restoreHeaderState();
    void restoreSplitState();
    void refreshTable();
    void updateStats();
    void editSelected();
    void deleteSelected();
    void exportAdif();
    void exportCsv();
    // Cabrillo 3.0 skeleton (2026-08-10): QSO lines from the filtered
    // view, exchange column filled with the grid where one is known.
    // Contests differ in what they exchange, so the file is a starting
    // point to edit, and the dialog says so.
    void exportCabrillo();
    void importAdif();
public:
    /// Eine ADIF-Datei einlesen und zusammenfuehren — ohne Dateidialog.
    /// Der Dialog ruft das hier auf; Tests koennen es direkt aufrufen.
    void importAdifFile(const QString& path);

    // ── MY_SOTA_REF aus dem SOTAdata-Log (2026-11-11) ───────────────
    //
    // Knopf "SOTA…": CSV waehlen, Vorschlaege im SotaNachtragDialog
    // ansehen und anhaken, erst "Apply" schreibt. Vorher eine eigene
    // Sicherung neben dem Log (logbook.adi.before-sota-<UTC>), denn die
    // .bak1-3 sind nach drei weiteren Speicherungen weg.
    void sotaNachtragen();
    /// Ohne Dialog -- der Dialog ruft das auf, Tests direkt. Die Indizes
    /// beziehen sich auf das Log, wie es gerade ist. `sicherung` bekommt
    /// den Pfad der Sicherung. false, wenn nichts geschrieben wurde.
    bool sotaVorschlaegeAnwenden(const QVector<SotaNachtrag::Vorschlag>& gewaehlt,
                                 QString* sicherung = nullptr);

    // ── QRZ-Logbuch abholen ─────────────────────────────────────────
    //
    // Der Weg zurueck: was im QRZ-Logbuch steht — dort geloggte
    // Verbindungen, Bestaetigungen, die QRZ-Kennung je Kontakt — kommt
    // hier ins Log, ueber denselben Zusammenfuehr-Weg wie eine Datei:
    // Neues wird ergaenzt, Fehlendes an vorhandenen Kontakten gefuellt,
    // nichts Sichtbares ueberschrieben. Der Schluessel ist der des
    // Uploaders (ein Logbuch, ein Schluessel).
    void setQrzLogbookUploader(QrzLogbookUploader* uploader);
    void syncFromQrz();
    /// Der Kern ohne Netz, fuer Tests: ADIF-Text wie von QRZ geliefert.
    void mergeQrzAdifForTest(const QString& adif) { mergeFetchedEntries(adif); }

    /// Wie viele Verbindungen das Logbuch gerade fuehrt. Nur fuer Tests.
    int entryCountForTesting() const { return m_all.size(); }
    const QVector<LogEntry>& entriesForTest() const { return m_all; }
    QString meinFeldForTest() const { return meinFeld(); }

    /// Rueckfragen und Meldungen des Imports umleiten. Fuer Tests: die
    /// eine Funktion antwortet, die andere schreibt mit. Nicht gesetzt
    /// heisst: ganz normale Fenster.
    void setOperatorHooks(std::function<bool(const QString&)> ask,
                          std::function<void(const QString&)> tell)
    { m_ask = std::move(ask); m_tell = std::move(tell); }

private:
    bool askOperator(const QString& question);
    void tellOperator(const QString& message);

    /// Zusammenfuehren mit Rueckfrage, Sicherung, Speichern, Bericht.
    /// `source` steht im Satz („that file", „your QRZ logbook");
    /// `fromQrz` markiert getroffene Kontakte als bei QRZ vorhanden.
    void importEntries(const QVector<LogEntry>& incoming,
                       const QString& source, bool fromQrz);
    void mergeFetchedEntries(const QString& adif);
    QrzLogbookUploader* m_qrzUploader{nullptr};
    QrzLogbookFetcher*  m_qrzFetcher{nullptr};
    QPushButton*        m_syncBtn{nullptr};

    std::function<bool(const QString&)> m_ask;
    std::function<void(const QString&)> m_tell;

    // Numbers about the filtered view: per band, per mode, per year,
    // unique calls and squares, furthest DX. (2026-08-10)
    void showStatistics();
public:
    /// Fuer Pruefstaende: die drei Logbuch-Bilder.
    class BedarfsMatrixWidget* bedarfForTest() const { return m_bedarf; }
    void aktualisiereBedarfForTest() { aktualisiereBedarf(); }
    class BandStundeWidget* bandStundeForTest() const { return m_bandStunde; }
    class RichtungsRoseWidget* roseForTest() const { return m_rose; }
    void zeigeStatistikSeiteForTest(int seite) { zeigeStatistikSeite(seite); }
    /// Fuer Pruefstaende: die drei Gesamtbilder (2026-11-10) und die
    /// Reiter, ueber die der Betreiber sie erreicht.
    class SonnenzyklusWidget* sonnenzyklusForTest() const { return m_sonne; }
    class KontinentUhrWidget* kontinentUhrForTest() const { return m_uhr; }
    class DxccWandWidget* dxccWandForTest() const { return m_wand; }
    class SolarFluxArchiv* solarFluxForTest() const { return m_flux; }
    QList<QPushButton*> statistikReiterForTest() const { return m_statsSeitenKnoepfe; }
    QDialog* statistikDialogForTest() const { return m_statsDialog; }
    /// Wie gross der Statistikdialog fuer die Gesamtbilder mindestens
    /// werden soll: 1240 x 820, auf kleinen Schirmen weniger. Er waechst
    /// darauf zu und schrumpft nie.
    static QSize gesamtbildGroesse(const QWidget* dialog);
private:
    // Send these records. With rows marked in the table it is those;
    // with none marked it is everything not yet uploaded, which is the
    // question an operator actually has after a session.
    void uploadEntries(const QList<int>& sourceRows, QsoUploader* target);
    QList<int> outstandingRows() const;
    void openMap();

    // Timestamped copy of the log file beside itself. Taken before an
    // import, which is the one operation here that touches many records
    // at once and cannot be undone from the table.
    QString makeBackup(QString* error) const;

    // Index into m_all for the given visible row, or -1.
    int sourceRow(int viewRow) const;
    // Source indices of every selected row, in display order.
    QList<int> selectedSourceRows() const;

public:
    /// Fuer Pruefstaende: die eingebettete Karte und die Kennzahlenreihe.
    class QsoMapWindow* mapPanelForTest() const { return m_mapPanel; }
    class LogbookStatsWidget* statsRowForTest() const { return m_statsRow; }
    QWidget* statsSectionForTest() const { return m_statsSection; }
    /// Seit Entwurf C die Knoepfe „Log" und „Statistics" des Umschalters.
    class QPushButton* mapToggleForTest() const;
    /// Seit Entwurf C der 3D-Knopf in der Kartenleiste.
    class QPushButton* threeDToggleForTest() const;
    class QPushButton* statsToggleForTest() const;

private:
    bool saveAll();

    QString m_path;

    QVector<LogEntry> m_all;      // everything in the file
    QVector<int>      m_visible;  // indices into m_all, after filtering

    QLineEdit*    m_search{nullptr};

    // Eingabezeile "NEW QSO"
    QLineEdit*    m_entryCall{nullptr};
    QLabel*       m_entryLive{nullptr};
    QLabel*       m_entryFreq{nullptr};
    QLabel*       m_entryMode{nullptr};
    QLabel*       m_entryBand{nullptr};
    QLabel*       m_entrySource{nullptr};
    QLineEdit*    m_entryRstS{nullptr};
    QLineEdit*    m_entryRstR{nullptr};
    QLineEdit*    m_entryComment{nullptr};
    QPushButton*  m_entryLogBtn{nullptr};
    QLabel*       m_entryHint{nullptr};
    // ROTOR in der Eingabezeile (2026-11-10): zeigt Rotor und Station,
    // dann drehen -- kurzer oder langer Weg, im erlaubten Bereich.
    QPushButton*  m_entryRotorBtn{nullptr};
    QPushButton*  m_entryRotate{nullptr};
    QPushButton*  m_entryLp{nullptr};
    QLabel*       m_entryPeilung{nullptr};
    QWidget*      m_rotorZeileW{nullptr};   // die Zeile unter der Eingabe (Entwurf C)
    class RotorBandWidget* m_rotorBand{nullptr};
    QLabel*       m_entryGrad{nullptr};
    QLabel*       m_entryKm{nullptr};
    bool          m_rotorZeile{false};
    QString       m_rotorRuf;      // fuer dieses Rufzeichen wurde ROTOR gedrueckt
    // Dreht er wirklich? (2026-11-11, Betreiber: „sobald ich ROTATE klicke,
    // soll ersichtlich sein, dass der Rotor auch wirklich dreht.")
    // m_drehZiel < 0: kein Drehbefehl aus der Eingabezeile offen.
    QLabel*       m_entryDreh{nullptr};
    double        m_drehZiel{-1.0};
    QString       m_drehMeldung;
    QElapsedTimer m_drehUhr;       // seit der letzten neuen Position
    QTimer*       m_drehWache{nullptr};
    int           m_stillstandMs{0};
    void          drehe(double ziel);
    void          drehZeileLeeren();
    void          aktualisiereDrehZeile();
    void          zeigeRotorZeile();
    void          aktualisiereRotorZeile();
    QTimer*       m_entryLookup{nullptr};
    // Bedarfs-Matrix unter der Eingabezeile (2026-11-08).
    class BedarfsMatrixWidget* m_bedarf{nullptr};
    QTimer*       m_bedarfTimer{nullptr};
    void aktualisiereBedarf();
    /// Der eigene Locator: aus dem juengsten QSO, das ihn traegt.
    QString meinFeld() const;
    // Statistikfenster mit drei Seiten (2026-11-08): Kacheln, Band x
    // Stunde, Richtungen.
    QStackedWidget* m_statsSeiten{nullptr};
    QList<QPushButton*> m_statsSeitenKnoepfe;
    class BandStundeWidget* m_bandStunde{nullptr};
    class RichtungsRoseWidget* m_rose{nullptr};
    QComboBox* m_bandStundeFilter{nullptr};
    // Drei weitere Seiten (2026-11-10): Sonnenzyklus, Kontinent-Uhr,
    // DXCC-Wand; der Flux kommt aus einem eigenen Cache (SolarFluxArchiv).
    class SonnenzyklusWidget* m_sonne{nullptr};
    class KontinentUhrWidget* m_uhr{nullptr};
    class DxccWandWidget* m_wand{nullptr};
    class SolarFluxArchiv* m_flux{nullptr};
    void zeigeStatistikSeite(int seite);
    // Tippen in der Suche: Karte und Kennzahlen erst nach einer kurzen
    // Pause nachziehen (Vorschlag 2026-09-27, siehe .cpp).
    QTimer*       m_followUpTimer{nullptr};
    bool          m_deferFollowUp{false};
    QPointer<RadioModel> m_radio;
    QMetaObject::Connection m_sliceFreqConn;
    QMetaObject::Connection m_sliceModeConn;
    double m_rotorDeg{-1.0};
    double m_rotorTargetDeg{-1.0};
    double m_rotorBeamDeg{40.0};
    void applyRotorToMap();
    QComboBox*    m_bandBox{nullptr};
    QComboBox*    m_modeBox{nullptr};
    // Distinct SOTA/POTA activation references found in the log —
    // "any" plus one entry per summit/park actually logged from.
    // Filtering to one and exporting is how a single activation leaves
    // as its own ADIF file (see exportAdif() — it already exports the
    // filtered view, not the whole log).
    QComboBox*    m_activationBox{nullptr};
    QLineEdit*    m_gridEdit{nullptr};
    QLineEdit*    m_countryEdit{nullptr};
    QCheckBox*    m_unconfirmedOnly{nullptr};
    QCheckBox*    m_greyLineOnly{nullptr};     // 2026-11-10, core/Grauzone.h
    QCheckBox*    m_useDates{nullptr};
    QDateEdit*    m_fromDate{nullptr};
    QDateEdit*    m_toDate{nullptr};
    QPushButton*  m_clearBtn{nullptr};

    QTableWidget*  m_table{nullptr};
    QSplitter*     m_split{nullptr};
    QsoDetailPane* m_detail{nullptr};

    // What QRZ said about the callsigns in this log, kept between runs.
    // Owned here rather than shared with the rotor panel: the window
    // outlives a single lookup and a log of two thousand contacts is
    // the only place the persistence actually earns its keep.
    CallsignCache m_callCache;

    // Which column the table is ordered by. Newest first is the useful
    // default: the contact you want is nearly always the last one.
    int m_sortColumn{0};
    Qt::SortOrder m_sortOrder{Qt::DescendingOrder};
    // Set once the operator has resized or hidden a column, so the
    // automatic fit-to-contents stops fighting their choices.
    bool m_headerRestored{false};
    QLabel*       m_stats{nullptr};
    QPushButton*  m_editBtn{nullptr};
    QPushButton*  m_deleteBtn{nullptr};
    QPushButton*  m_uploadBtn{nullptr};

    // ── Kopf nach Entwurf A (2026-11-12) ────────────────────────────
    QWidget*      m_filterChips{nullptr};   // aktive Filter als Kapseln
    QWidget*      m_filterBar{nullptr};     // Filterspalte in „Browse"
    QWidget*      m_bandWahl{nullptr};      // SchnellWahl (LogbookWindow.cpp)
    QWidget*      m_modeWahl{nullptr};
    class QAction* m_importAction{nullptr};
    class QAction* m_exportAdifAction{nullptr};
    class QAction* m_exportCsvAction{nullptr};
    class QAction* m_cabrilloAction{nullptr};
    class QAction* m_uploadAction{nullptr};
    class QAction* m_syncAction{nullptr};
    class QAction* m_sotaAction{nullptr};
    class QAction* m_statsWindowAction{nullptr};
    void uploadMenuAt(const QPoint& globalPos);
    void showOptionsMenu();
    void updateFilterChips();

    // ── Drei Arbeitsweisen (Entwurf C, 2026-11-12) ──────────────────
    enum Modus { ModusLog = 0, ModusBrowse = 1, ModusStats = 2 };
    int                  m_modus{ModusLog};
    class QButtonGroup*  m_modusGruppe{nullptr};
    QLabel*              m_modusHinweis{nullptr};
    QWidget*             m_auswahlBox{nullptr};   // Edit/Delete (Browse)
    QWidget*             m_eingabe{nullptr};      // Eingabe (Log)
    class QStackedWidget* m_seiten{nullptr};
    class QSplitter*     m_logSplit{nullptr};
    class QSplitter*     m_logMitte{nullptr};
    class QSplitter*     m_browseRechts{nullptr};
    class QVBoxLayout*   m_logKarteLay{nullptr};
    class QVBoxLayout*   m_logMapLay{nullptr};
    class QVBoxLayout*   m_logListeLay{nullptr};
    class QVBoxLayout*   m_logRechtsLay{nullptr};
    class QVBoxLayout*   m_browseListeLay{nullptr};
    class QVBoxLayout*   m_browseMapLay{nullptr};
    class QVBoxLayout*   m_browseKarteLay{nullptr};
    class QVBoxLayout*   m_statsSeiteLay{nullptr};
    class QScrollArea*   m_detailScroll{nullptr};
    QLabel*              m_bedarfLeer{nullptr};
    void setModus(int modus);
    void ensureMapPanel();
public:
    int modusForTest() const { return m_modus; }
    void setModusForTest(int m) { setModus(m); }
private:

    QVector<QsoUploader*> m_uploaders;

    // One map window, reused, so a second click raises the existing one
    // instead of stacking copies of the same picture.
    class QsoMapWindow* m_map{nullptr};
    class QrzClient*    m_qrz{nullptr};   // fuer das Kartenfenster, das spaeter entsteht
    class SatelliteService* m_satellites{nullptr};   // dito
    const class CtyDatParser* m_cty{nullptr};
    class QDialog* m_statsDialog{nullptr};
    class LogbookStatsWidget* m_statsView{nullptr};
    void refreshStatsView();

    // ── Eine Seite (2026-09-22) ──────────────────────────────────────
    //
    // Karte als Spalte zwischen Tabelle und Detailkarte, Kennzahlen als
    // Reihe darunter — beides ein- und ausschaltbar, beides folgt dem
    // Filter der Tabelle. Die Fenster („↗") bleiben daneben bestehen.
    class QsoMapWindow*       m_mapPanel{nullptr};    // eingebettet, faul angelegt
    class LogbookStatsWidget* m_statsRow{nullptr};
    QWidget*                  m_statsSection{nullptr};
    void set3dShown(bool on);
    void spiegle3d();
    void setMapPanelShown(bool on);
    void setStatsRowShown(bool on);
    void refreshMapPanel();
    void syncMapPanelSelection();
    void prepareMap(QsoMapWindow* map);
    PositionFallback m_fallback;

    // Outstanding uploads for the current batch, so the summary can be
    // reported once instead of one message box per contact.
    int m_pending{0};
    int m_okCount{0};
    int m_dupCount{0};
    QStringList m_failures;
    // Source indices in the batch being uploaded, so a success can be
    // written back against the right record instead of against every
    // past contact with the same station.
    QList<int> m_uploadBatch;
    // The service this batch went to. The uploader objects are shared
    // with live logging (QsoUploadQueue), which may be sending to
    // ANOTHER service while a batch runs; that answer is not ours.
    QPointer<QsoUploader> m_batchTarget;
};

} // namespace Longpath
