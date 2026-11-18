// =================================================================
// src/gui/AuxiliaryWindowLeveler.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-09-21: "wenn ich ein Channel Strip oder etwas anderes
// aufmache, ist es im Hintergrund." Die schwebenden Paletten (Applets,
// Panadapter, Rotor/Log, Bandbreitenfilter ...) sind Qt::Tool -- auf
// macOS NSPanels auf der SCHWEBENDEN Fensterebene. Jedes gewoehnliche
// Fenster, das aus dem Hauptfenster heraus aufgeht (Channel Strip,
// Logbuch, Setup, Spot-Hub, ein QMessageBox ...), liegt auf der normalen
// Ebene darunter; raise()/activateWindow() hebt nur innerhalb der
// eigenen Ebene und ist gegen ein NSPanel darueber machtlos. Das
// Antennenfenster bekam dafuer am 2026-09-01 von Hand Qt::Tool -- der
// dritte Fall zeigt, dass das keine Frage einzelner Fenster ist.
//
// Dieser Filter haengt am QApplication-Objekt und hebt JEDES
// Nebenfenster beim Anzeigen auf die Ebene der Paletten: ein
// Top-Level-Widget mit Elternteil (also aus einem anderen Fenster heraus
// geoeffnet), das selbst keine Palette, kein Popup, kein Tooltip und
// kein Splash ist. Es behaelt sein normales Aussehen (kein
// Werkzeugfenster-Titel) und liegt mit den Paletten auf einer Ebene, wo
// das zuletzt gezeigte vorne steht -- wie man es erwartet.
//
// Ein NSWindow auf der schwebenden Ebene stuende ueber jeder anderen
// App (der Fehler vom 2026-08-31 bei den Paletten). Darum wechselt der
// Filter beim Deaktivieren von Longpath alle gehobenen Fenster auf die
// normale Ebene zurueck und beim Aktivieren wieder hoch.
//
// ── Seit 2026-11-15 auch die Paletten selbst ────────────────────────
//
// Betreiber: „und immer, wenn ich ein anderes fenster ausserhalb von
// longpath klicke, wird longpath schwarz."
//
// Die Paletten (Qt::Tool) hatten fuer dieselbe Frage ihr eigenes
// Mittel: `hidesOnDeactivate` in MacFloatingWindowBehavior.mm. Das
// loest das Problem „steht ueber fremden Apps" -- aber mit dem
// groben Werkzeug. Bei einer Anordnung, die fast nur aus schwebenden
// Fenstern besteht (seine), verschwindet beim Wegklicken die halbe
// Station, und zurueck bleibt der dunkle Spektrumgrund.
//
// Jetzt macht dieser Filter beides: er fasst auch Qt::Tool an und
// stuft es beim Verlassen herunter, statt es zu verstecken.
// `hidesOnDeactivate` ist dafuer auf NO gegangen. Ein Mittel fuer
// beide Fensterarten statt zweier, und das sanftere von beiden.
//
// Auf allen anderen Plattformen ein No-Op (setPaletteWindowLevel ist
// dort leer): unter Windows/X11 liegen Werkzeugfenster nicht auf einer
// hoeheren Ebene, dort war nie etwas verdeckt.
//
// ── Heruntergestuft heisst nicht: hinter das eigene Fenster (2026-11-18) ─
//
// Betreiber, mit Bild: „sobald ich auf claude zum beispiel klicke
// verschwinden alle fenster vom longpath! das darf nicht sein."
//
// Meine Behebung vom 2026-11-15 war die halbe. Die Paletten verstecken
// sich seither nicht mehr -- aber beim Herunterstufen auf die normale
// Ebene ordnet AppKit sie neu ein, und die meisten landen HINTER dem
// eigenen Hauptfenster. An seiner laufenden App gemessen, waehrend
// Claude vorne war (Fensterdienst, von vorne nach hinten):
//
//     Claude
//     FT8 Decoder
//     Longpath-Hauptfenster
//     FT8 Slots            <- dahinter
//     Panadapter           <- dahinter
//     TX                   <- dahinter
//
// Am 15.11. hatte ich nur `kCGWindowIsOnscreen` gelesen -- das sagt, ob
// ein Fenster in der Liste steht, nicht, ob man es sieht.
//
// Jetzt ordnet der Filter jedes gestufte Fenster nach dem Herunterstufen
// unmittelbar UEBER das Fenster ein, aus dem heraus es aufgegangen ist
// (`ordneUeber`, MacFloatingWindowBehavior). Das aktiviert nichts und
// holt nichts vor fremde Programme: die Panels liegen danach genau wie
// das Hauptfenster hinter der aktiven App -- nur eben vor ihm.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <functional>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>

class QWidget;

namespace Longpath {

class AuxiliaryWindowLeveler : public QObject {
    Q_OBJECT
public:
    explicit AuxiliaryWindowLeveler(QObject* parent = nullptr);

    // Ein Fenster, das der Filter anfasst: Top-Level, mit Elternteil,
    // gewoehnlicher Typ (Window/Dialog), nicht "bleibt oben".
    static bool isAuxiliaryWindow(const QWidget* w);

    // Eine Palette: Top-Level, mit Elternteil, Qt::Tool, nicht "bleibt
    // oben". Das sind die schwebenden Fenster von Longpath selbst --
    // Applets, Panadapter, Rotor/Log, Bandbreitenfilter, Seitenflaeche.
    // Sie liegen von Qt aus schon auf der Palettenebene; der Filter
    // stuft sie herunter, solange Longpath nicht die aktive App ist
    // (2026-11-15, siehe Kopf).
    static bool istPalette(const QWidget* w);

    // Was der Filter ueberhaupt in die Hand nimmt -- das eine ODER das
    // andere.
    static bool wirdGestuft(const QWidget* w);

    bool eventFilter(QObject* watched, QEvent* event) override;

    /// Der Wechsel selbst: Ebenen setzen und, wenn Longpath NICHT mehr
    /// die aktive App ist, jedes gestufte Fenster ueber sein
    /// Elternfenster einordnen. Oeffentlich, damit ein Pruefstand ihn
    /// ohne echten Programmwechsel fahren kann.
    void wechsel(bool aktiv);

    /// Wer einordnet. Vorgabe: `ordneUeber` (Cocoa; sonst ein No-Op). Ein
    /// Pruefstand haengt hier eine Attrappe ein und liest die Reihenfolge.
    void setOrdner(std::function<void(QWidget* fenster, QWidget* anker)> ordner);

    // Fuer Tests: was der Filter gerade gehoben hat.
    QSet<QWidget*> trackedForTest() const { return m_tracked; }
    // Fuer Tests: die Fenster, zuletzt benutztes zuerst.
    QList<QWidget*> recentOrderForTest() const;

private:
    void track(QWidget* w);
    void applyLevel(QWidget* w) const;
    void onApplicationStateChanged();

    QSet<QWidget*> m_tracked;
    // Die Fenster der App, zuletzt benutztes (angeklickt, aktiviert,
    // gezeigt) zuerst. Beim Zurueckkommen in die App stellt macOS alle
    // Paletten gemeinsam nach vorne -- auch vor das Logbuch, das der
    // Betreiber gerade vor sich hatte (2026-09-26). Danach wird diese
    // Reihenfolge wiederhergestellt.
    QList<QPointer<QWidget>> m_recent;
    void touch(QWidget* top);
    void restoreRecentOrder();
    /// Jedes gestufte, sichtbare Fenster ueber sein Elternfenster --
    /// das zuletzt benutzte zuoberst.
    void ordneUeberDieEltern();
    std::function<void(QWidget*, QWidget*)> m_ordner;
};

} // namespace Longpath
