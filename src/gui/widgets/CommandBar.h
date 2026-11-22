#pragma once

// =================================================================
// src/gui/widgets/CommandBar.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Kopfleiste ───────────────────────────────────────────────────
//
// Der größte Einzelunterschied zwischen Longpath und der Vorlage des
// Betreibers. In der Vorlage liegen MODE, FILTER, BAND, FAVORITES, STEP und
// FRONT-END als beschriftete Pillengruppen quer über das Fenster; hier
// stand bisher links eine Spalte senkrechter Knöpfe und der Rest
// verteilt in den Panels.
//
// Aufgabe #7 der Projektliste behauptete, so eine Leiste sei gebaut.
// Am 2026-08-15 nachgesehen: im Quellbaum existierte kein solches
// Widget. Der Eintrag war falsch.
//
// ── Was die Leiste ausmacht, und was nicht ───────────────────────────
//
// Nicht die Farben — die sind seit heute Nacht in Ordnung. Es ist die
// FORM:
//
//   · Über jeder Knopfreihe eine Versalzeile mit weiter Laufweite, in
//     Skalenfarbe. Ohne sie ist eine Reihe Pillen nur eine Reihe
//     Pillen; mit ihr ist sie eine benannte Gruppe.
//
//   · Drei sichtbare Einträge, dann ein „…". Nicht dreizehn
//     nebeneinander — das ist die Filterwand im RX-Panel, und die liest
//     niemand, der sie nicht auswendig kann.
//
//   · Genau einer je Gruppe leuchtet. Der Rest ist fast unsichtbar.
//
//   · Radius 6, Höhe 27. Radius 3 ist der Qt-Standardwert und lässt
//     alles nach Qt aussehen.
//
// ── Was bewusst fehlt ────────────────────────────────────────────────
//
// BAND, FILTER und FAVORITES sind nicht dabei. Nicht aus Faulheit: bei
// denen ist die Frage „welche drei stehen vorne" eine Entscheidung des
// Betreibers, keine des Programms. Die drei letzten Bänder? Die
// Favoriten? Die des aktuellen Antennenanschlusses?
//
// MODE und STEP haben diese Frage nicht — da gibt es eine natürliche
// Auswahl und einen vollständigen Rest im Menü. Also sind sie zuerst
// da, und die Leiste beweist ihre Form, bevor jemand über Bandlisten
// entscheiden muss.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-28 -- the filter "…" reads FilterPresetStore (Thetis names +
//                 the operator's own presets), Martin Fischer, OE5SOS,
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/WdspTypes.h"
#include "core/DigiFrequenzen.h"
#include "models/Band.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <QVector>
#include <functional>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;

namespace Longpath {

class FilterPresetStore;
class SliceModel;

class CommandBar : public QWidget {
    Q_OBJECT
signals:
    /// Bandwechsel gewuenscht.
    ///
    /// Die Leiste kennt nur die Scheibe, ein Bandwechsel geht aber
    /// ueber RadioModel::onBandButtonClicked (Bandplan, Antennenwahl,
    /// gespeicherte Frequenz je Band). Deshalb ein Signal statt eines
    /// zweiten Modellzeigers — MainWindow verdrahtet es.
    void bandRequested(Longpath::Band band);

    /// Aus dem Schnellregler heraus: die Einstellungsseite dieser
    /// Rauschminderung oeffnen. Wird vom MainWindow verdrahtet.
    void openNrSetupRequested(Longpath::NrSlot slot);

    /// „Digital" gewaehlt: Betriebsart, Anrufstelle, Filter und das
    /// Dekoderfenster sollen sich selbst stellen.
    ///
    /// Betreiber am 2026-10-21 vor der Antenne: „ich will nicht die
    /// freq suchen", „quasi wie jtdx". Die Leiste weiss nur, WAS
    /// gewuenscht ist; das Herrichten gehoert MainWindow, weil es
    /// Scheibe, Filter und die Fensterverwaltung zugleich braucht.
    void digiRequested(Longpath::DigiFrequenzen::Digimodus modus);

    /// Der Bediener hat DIGU oder DIGL gewaehlt. Traegt KEINEN Modus:
    /// welcher Digimodus zuletzt gemeint war, weiss das Hauptfenster.
    /// Siehe die Begruendung bei pushModeToModel().
    void digimodusGewaehlt();

public:
    /// Pruef-Naht (2026-10-28): dieselbe Stelle, die das
    /// Ueberlaufmenue benutzt. Noetig, weil DIGU/DIGL nicht unter den
    /// drei sichtbaren Pillen sind und `QMenu::exec()` einen Lauf auf
    /// macOS endlos haengen laesst (siehe
    /// docs/architecture/2026-10-15-pruefstaende-und-der-schirm.md).
    /// Ein Lauf, der das Signal selbst sendet, pruefte nur die Haelfte
    /// dahinter -- genau das ist mir hier erst passiert.
    void waehleBetriebsartForTest(DSPMode m) { pushModeToModel(m); }

    explicit CommandBar(QWidget* parent = nullptr);
    ~CommandBar() override;

    /// Zweiseitig an eine Empfangskette hängen. Ein zweiter Aufruf löst
    /// die vorige wieder — beim Umschalten zwischen Pans darf die
    /// Leiste nicht an der alten weiterhängen und deren Modus melden.
    void attach(SliceModel* slice);

    /// RATE rechts neben NR: die Rate, mit der der Empfangskanal WIRKLICH
    /// laeuft, und daneben (im Tooltip) die gemessene Eingangsrate. Weichen
    /// die beiden zwei Sekunden in Folge um mehr als 10 % ab, wird die
    /// Anzeige bernsteinfarben -- am 2026-09-24 lief der QRP-Kanal auf
    /// 192 kHz, waehrend 48 kHz ankamen, und nichts auf dem Bildschirm
    /// hat es verraten. channelRateHz <= 0 heisst: keine Verbindung.
    void setRateReadout(int channelRateHz, int measuredRateHz);
    QString rateReadoutText() const;
    bool rateReadoutWarns() const { return m_rateWarn; }

    /// Ein Widget ans rechte Ende der Leiste hängen — dort sitzt das
    /// Plus. Getrennt vom Konstruktor, weil der Sichtbarkeitsverwalter
    /// im MainWindow erst lange nach dem Fensteraufbau entsteht und die
    /// Leiste nicht auf ihn warten soll.
    void addTrailing(QWidget* w);

    /// Ein Bauteil als eigene Gruppe ans Ende der Gruppen haengen --
    /// also NACH der Rate und VOR der Dehnung, damit es neben der Rate
    /// steht und nicht am rechten Rand.
    ///
    /// Betreiber am 2026-10-06: „meine profile links im eck sollten oben
    /// in die taksleite neben 48 khz".
    void addGroupWidget(const QString& caption, QWidget* w);

    /// Das Gegenstueck: nimmt das Bauteil UND seine Versalzeile wieder
    /// aus der Leiste. Das Bauteil selbst wird nicht geloescht.
    void removeGroupWidget(QWidget* w);

    /// Woher das „…" der Filtergruppe seine Vorgaben nimmt: aus dem
    /// Vorgabenspeicher, also mit den Thetis-Namen („1.0k", „800" …)
    /// und mit dem, was der Betreiber unter Setup › Filter Presets
    /// selbst eingetragen hat. Ohne Speicher (Tests, frueher Aufbau)
    /// die Thetis-Vorgaben selbst. Ein Zeiger auf den Speicher, nicht
    /// aufs RadioModel — die Leiste kennt sonst nur die Scheibe.
    void setFilterPresetStore(FilterPresetStore* store);

    /// Rechnet die MASCHINE die Rauschminderung (Flex)? Dann zeigt
    /// der Schnellgriff ihre Staerke statt der WDSP-Werte, die dort
    /// nichts tun. Leer = wir rechnen selbst.
    void setMaschinenNr(const std::function<QString(NrSlot)>& namen);

    /// Die Flex-Fassung der Rauschminderungs-Leiste (2026-11-20). Die
    /// Maschine fuehrt nicht alle Verfahren, die Longpath hat, und nennt
    /// die uebrigen anders. `angeboten` sagt, ob ein Verfahren an der
    /// Flex in der Leiste stehen darf; `anzeige` liefert den Namen, unter
    /// dem die Flex es fuehrt — leer heisst: bisheriger Name. Wirksam nur,
    /// solange `setNrFlexModus(true)` gilt.
    void setNrFlexRegeln(const std::function<bool(NrSlot)>& angeboten,
                         const std::function<QString(NrSlot)>& anzeige);
    /// Ist eine FlexRadio verbunden? Dann zeigt die Leiste die Flex-
    /// Fassung; sonst (Anvelina, kein Funkgeraet) alles wie bisher.
    void setNrFlexModus(bool flex);

    /// Was das „…" der Filtergruppe gerade anbietet, in Reihenfolge:
    /// Name und Kanten. Ohne Scheibe leer.
    struct FilterMenuEntry {
        QString name;
        int     low{0};
        int     high{0};
    };
    QVector<FilterMenuEntry> filterMenuEntries() const;

    // ── Für Tests ────────────────────────────────────────────────────
    //
    // Ob die Leiste hübsch ist, kann kein Test sagen. Ob genau eine
    // Pille je Gruppe eingeschaltet ist und ob sie dem Modell folgt,
    // schon — und das sind die beiden Eigenschaften, an denen so eine
    // Leiste tatsächlich scheitert.

    /// Die Beschriftungen einer Gruppe, in Reihenfolge, ohne das „…".
    QStringList pillsIn(const QString& group) const;
    /// Alle Rauschminderungen, die diese Leiste anbietet — die drei
    /// vorne UND die im „…". Damit ein Test sagen kann, was NICHT
    /// angeboten wird (DFNR/BNR ohne HAVE_DFNR/HAVE_BNR, 2026-09-17).
    QStringList allNrLabels() const;
    /// Die eingeschaltete Pille einer Gruppe, oder leer.
    QString activePill(const QString& group) const;
    /// Eine Pille auslösen, als hätte jemand geklickt.
    bool clickPill(const QString& group, const QString& label);
    /// Die Gruppennamen in Reihenfolge.
    QStringList groups() const;

    // Die Pillen-Optik der oberen Leiste — oeffentlich, seit die UNTERE
    // Leiste sie auch benutzt (2026-08-19, auf Ansage des Betreibers:
    // „die Taskleiste unten sollte auch wie die Taskleiste oben
    // aussehen, gleiches Design").
    //
    // Eine Funktion statt zweier Stylesheets: zwei Leisten, die
    // „gleich" aussehen sollen und ihre Farben getrennt fuehren, sehen
    // nach dem naechsten Feinschliff wieder verschieden aus.
    static QString pillStyle();
    /// Dieselbe Pille, nur leise: fuer eine Gruppe, die ohne Funkgeraet
    /// nichts anzuzeigen hat. Gleiche Masse wie pillStyle(), damit beim
    /// Verbinden nichts springt -- nur Fuellung, Rand und Schrift
    /// treten zurueck.
    static QString pillStyleGedaempft();

    static constexpr int kPillHeight = 27;
    static constexpr int kPillRadius = 6;
    /// Sichtbare Einträge je Gruppe, bevor das „…" übernimmt.
    static constexpr int kVisiblePerGroup = 3;

protected:
    void paintEvent(QPaintEvent* e) override;
    // Nach jeder neuen Anordnung die ganze Leiste neu zeichnen -- sonst
    // bleiben alte Trennstriche als Bildreste stehen (2026-11-21).
    bool event(QEvent* e) override;

private:
    std::function<QString(NrSlot)> m_maschinenNr;
    /// Wo die Dehnung steht -- gesucht, nicht geraten (siehe .cpp).
    int dehnungsPlatz() const;
    /// Bauteil -> die Gruppe, in der es steckt (fuer removeGroupWidget).
    QHash<QWidget*, QLayout*> m_gruppenWidgets;

    struct Group {
        QString name;
        QHBoxLayout* row{nullptr};   // wo die Pillen hineinwandern
        QVector<QPushButton*> pills;
        QPushButton* overflow{nullptr};
        /// Die Versalzeile ueber den Pillen. Gemerkt, damit eine Gruppe
        /// ohne Inhalt samt ihrer Ueberschrift zuruecktreten kann --
        /// eine helle Ueberschrift ueber gedaempften Pillen saehe aus
        /// wie ein halb geladener Zustand.
        QLabel* caption{nullptr};
    };

    /// ── Die Leiste als Reihe lesbar machen (2026-10-07) ────────────
    ///
    /// Betreiber: "suche das, welches am besten harmonisch dazu passt".
    /// Drei Dinge zusammen, alle drei folgen dem, was im Fenster schon
    /// gilt:
    ///
    ///   Trennstriche  -- jedes andere Bauteil ist eingefasst
    ///                    (BANDWIDTH FILTER, PANADAPTER, S-METER, TX).
    ///                    Die Leiste war die einzige Stelle ohne Grenze.
    ///   Verteilen     -- rechts standen 168 Pixel tot. Dafuer gibt es
    ///                    im Fenster kein Gegenstueck.
    ///   Zuruecknehmen -- die "—" bei POWER/SWR/SIGNAL AVERAGE stehen
    ///                    gedaempft da. FILTER und RATE schrien dagegen
    ///                    in voller Helligkeit "hier ist nichts".
    void verteileGruppen();
    void zieheLeereGruppenZurueck();
    /// Hat die Gruppe ohne Funkgeraet nichts zu zeigen? Nicht statisch,
    /// weil RATE seinen Wert in einem eigenen Schild traegt und nicht
    /// in Pillen.
    bool gruppeIstLeer(const Group& g) const;
    /// Die Gruppenkaesten in der Reihenfolge der Reihe. Alles andere
    /// darin (Dehnungen, das angehaengte Plus) faellt heraus.
    QVector<QLayout*> gruppenKaesten() const;

    Group* group(const QString& name);
    const Group* group(const QString& name) const;

    /// Eine Gruppe anlegen: Überschrift, Pillen, Überlaufknopf.
    Group& addGroup(const QString& caption, QHBoxLayout* row);
    QPushButton* addPill(Group& g, const QString& label);
    /// Das „…" ans Ende einer Gruppe, mit dem vollstaendigen Rest.
    template <typename Entry, typename Apply>
    void addOverflow(Group& g, const QVector<Entry>& all, Apply apply);
    void setActive(Group& g, const QString& label);

    void buildModeGroup(QHBoxLayout* row);
    void buildDigiGroup(QHBoxLayout* row);
    void aktualisiereDigiLampe();

public:
    /// Welche Betriebsart der FT8-Dekoder faehrt, und ob er
    /// ueberhaupt laeuft. Laeuft er, hat SEINE Betriebsart Vorrang
    /// vor der Frequenz -- siehe `aktualisiereDigiLampe`.
    void setzeDigiDekoder(bool laeuft, DigiFrequenzen::Digimodus modus);

private:
    bool m_digiDekoderLaeuft{false};
    DigiFrequenzen::Digimodus m_digiBetriebsart{
        DigiFrequenzen::Digimodus::Ft8};
    void buildStepGroup(QHBoxLayout* row);

    // ── Filter und Rauschminderung, 2026-08-21 ───────────────────────
    //
    // Auf Ansage: „bitte folgende sachen in die leiste oben!
    // bandbreite (3 Anzeigen, den 4 Punkt oeffnen um mehr zu sehen
    // individuell) eingeben. NR1-NR4 eben so." Mit Bildschirmfoto der
    // Vorlage: FILTER 2.7k · 2.9k · 3.3k · …
    //
    // Dasselbe Muster wie Modus und Schrittweite, kein neues.
    void buildFilterGroup(QHBoxLayout* row);
    void buildNrGroup(QHBoxLayout* row);
    /// Die Rauschminderungen, die die Leiste JETZT anbietet, mit dem Namen,
    /// unter dem sie stehen. Ohne Flex-Modus: genau `m_allNr`.
    QVector<QPair<QString, Longpath::NrSlot>> nrListe() const;
    /// Schreibt die Beschriftung der NR-Pillen neu aus `nrListe()`.
    void aktualisiereNrPillen();
    void buildBandGroup(QHBoxLayout* row);
    void buildRateGroup(QHBoxLayout* row);
    void applyRateStyle(bool warn);

    /// Die Filterbreiten haengen am Modus — CW hat andere als SSB.
    /// Deshalb werden die drei Pillen NEU BESCHRIFTET statt neu
    /// gebaut: Widgets wegzuwerfen und anzulegen laesst die Leiste bei
    /// jedem Moduswechsel zucken, und die Faltung muesste jedes Mal
    /// neu rechnen.
    void relabelFilterPills();
    void pushFilterToModel(int low, int high);
    void pushNrToModel(Longpath::NrSlot slot);
    void askForCustomFilter();
    /// Wie eine Breite in der Leiste heisst: „2.9k" statt „-2900…-100".
    static QString filterLabel(int low, int high);
    /// Die Pille des laufenden Filters: Name der Vorgabe oder Breite.
    QString runningFilterLabel(int low, int high) const;
    /// Die drei Breiten, die vorne stehen (Einstellung
    /// „CommandBarFilterFront", Vorgabe 2700,2900,3500 Hz).
    static QVector<int> frontWidthsHz();
    /// Eine Breite auf Flanken umrechnen; die Mitte bleibt stehen.
    QPair<int, int> edgesForWidth(int widthHz) const;

    void pushModeToModel(DSPMode m);
    void pushStepToModel(int hz);
    void pushBandToModel(Longpath::Band band);
    void pullFromModel();

    // Zeiger, nicht Werte. addGroup() gibt eine Referenz zurueck, und
    // ein QVector<Group> verschiebt beim naechsten append() seinen
    // Speicher — die Referenz waere dann tot. Heute ginge es zufaellig
    // gut, weil keine Gruppe nach ihrer Benutzung noch eine weitere
    // anlegt; das ist keine Eigenschaft, auf die sich der naechste
    // Gruppentyp verlassen sollte.
    QVector<Group*> m_groups;
    QHBoxLayout* m_row{nullptr};   // die äußere Reihe, für addTrailing()
    QLabel* m_rateLabel{nullptr};
    bool m_rateWarn{false};
    int m_rateMismatchSeconds{0};
    QPointer<SliceModel> m_slice;
    QPointer<FilterPresetStore> m_presetStore;
    QList<QMetaObject::Connection> m_links;

    // Der vollständige Rest, den das „…" anbietet.
    QVector<QPair<QString, DSPMode>> m_allModes;
    QVector<QPair<QString, int>> m_allSteps;
    QVector<QPair<QString, Longpath::Band>> m_allBands;
    QVector<QPair<QString, Longpath::NrSlot>> m_allNr;

    // Die Flex-Fassung (siehe setNrFlexRegeln / setNrFlexModus).
    bool m_nrFlex{false};
    std::function<bool(NrSlot)>    m_nrFlexAngeboten;
    std::function<QString(NrSlot)> m_nrFlexAnzeige;
};

} // namespace Longpath
