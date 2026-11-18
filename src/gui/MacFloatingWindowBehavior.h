#pragma once

// =================================================================
// src/gui/MacFloatingWindowBehavior.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber, wiederholt gemeldet: im nativen macOS-Vollbild bleiben die
// schwebenden Qt::Tool-Fenster (AppletFloatingWindow, ToolWindow,
// FloatingContainer, PanFloatingWindow) auf dem alten Schreibtisch
// zurueck, statt mit dem Hauptfenster in dessen eigenen Vollbild-Space
// zu wandern.
//
// Der Code an allen vier Stellen behauptete bislang nur in Kommentaren,
// Qt::Tool ergaebe auf macOS automatisch ein NSPanel mit der
// Sammelregel "FullScreenAuxiliary" (schwebt ueber einem Vollbild-
// Elternfenster). Das stimmt nicht von selbst -- Qt setzt dieses
// NSWindowCollectionBehavior nicht automatisch fuer Qt::Tool-Fenster.
// Ohne diesen expliziten Aufruf zaehlt das Fenster fuer macOS als
// gewoehnliches Fenster auf dem gewoehnlichen Schreibtisch-Space und
// bleibt dort liegen, wenn das Elternfenster in seinen eigenen
// Vollbild-Space wechselt.
//
// Betreiber 2026-08-31, per geteiltem Bildschirm live beobachtet: ein
// zweiter, schwererer Teil desselben Problems. Diese vier Fenster
// standen nicht nur ueber dem eigenen Hauptfenster, sondern ueber
// JEDER anderen App auf dem Mac -- sogar ueber dem nackten Schreibtisch
// auf einem voellig anderen Space, waehrend das Hauptfenster laengst
// in seinem eigenen Vollbild-Space stand. Grund: Qt::Tool erzeugt ein
// NSPanel mit einem "schwebenden" Fensterlevel, das laut Cocoa
// AUSDRUECKLICH ueber allen normalen Fenstern JEDER App liegt, nicht
// nur denen von Longpath. Die FullScreenAuxiliary-Sammelregel oben
// aendert daran nichts -- sie regelt nur, welchem Space das Fenster
// folgt, nicht seine Ebene relativ zu ANDEREN Apps. Diese Funktion
// setzt darum zusaetzlich NSPanel.hidesOnDeactivate: das Fenster
// versteckt sich automatisch, sobald Longpath nicht mehr die aktive
// App ist, und kommt zurueck, sobald Longpath wieder aktiv wird --
// das Standardverhalten jeder normalen macOS-Werkzeugpalette.
//
// Muss aufgerufen werden, NACHDEM das Fenster sein natives Handle hat
// (winId() erzwingt das noetigenfalls). No-op auf allen anderen
// Plattformen.
//
// <QtGlobal> statt eines Vorwärtsdeklarations-only-Headers: Q_OS_MAC
// muss hier zuverlässig definiert sein, unabhängig davon, in welcher
// Reihenfolge der Aufrufer seine eigenen Includes setzt (in
// MacFloatingWindowBehavior.mm z.B. VOR <QWidget>).
//
// Betreiber 2026-09-21, dritter Fall dieser Art (nach dem Antennenfenster
// am 2026-09-01): "wenn ich ein Channel Strip oder etwas anderes aufmache,
// ist es im Hintergrund" -- jedes gewoehnliche Fenster (QDialog,
// Logbuch, Setup, auch ein QMessageBox) liegt auf der NORMALEN Ebene und
// damit unter den NSPanels der Paletten, egal wie oft raise() gerufen
// wird. setPaletteWindowLevel() hebt ein solches Fenster auf dieselbe
// Ebene wie die Paletten (NSFloatingWindowLevel) und gibt ihm dieselbe
// FullScreenAuxiliary-Sammelregel; mit floating=false faellt es auf die
// normale Ebene zurueck. Den Wechsel beim (De-)Aktivieren der App macht
// AuxiliaryWindowLeveler -- ein NSWindow kennt kein hidesOnDeactivate,
// und ein Fenster auf der schwebenden Ebene stuende sonst ueber jeder
// anderen App (genau der Fehler von 2026-08-31, siehe oben).
#include <QtGlobal>

class QWidget;

#ifdef Q_OS_MAC
namespace Longpath {
void enableFullScreenAuxiliaryBehavior(QWidget* window);
void setPaletteWindowLevel(QWidget* window, bool floating);
/// `fenster` in seiner Fensterebene unmittelbar UEBER `anker` einordnen,
/// ohne es zu aktivieren und ohne das Programm nach vorne zu holen
/// (`orderWindow:NSWindowAbove relativeTo:`).
///
/// Gebraucht, wenn Longpath nicht die aktive App ist (2026-11-18): die
/// Paletten werden dann auf die normale Ebene heruntergestuft -- und
/// rutschten dabei HINTER das eigene Hauptfenster. Siehe
/// AuxiliaryWindowLeveler.h.
void ordneUeber(QWidget* fenster, QWidget* anker);
// Holt das ganze Programm nach vorne (2026-11-08, fuer den zweiten Start:
// SingleInstanceGuard). activateWindow() allein macht unter macOS nur das
// Fenster zum Schluesselfenster eines schon aktiven Programms.
void activateThisApplication();
/// Ist gerade eine Maustaste gedrueckt? Waehrend das System ein Fenster
/// an der Titelleiste zieht, sieht Qt keine Maustasten (die Geste gehoert
/// dem Fenstersystem) -- [NSEvent pressedMouseButtons] schon. Gebraucht
/// vom Seitenbereich: ein Fenster, das ueber ihm LOSGELASSEN wird, legt
/// sich hinein (2026-09-25).
bool anyMouseButtonDown();
}
#else
#include <QGuiApplication>
namespace Longpath {
inline void enableFullScreenAuxiliaryBehavior(QWidget*) {}
inline void setPaletteWindowLevel(QWidget*, bool) {}
inline void ordneUeber(QWidget*, QWidget*) {}
inline void activateThisApplication() {}
inline bool anyMouseButtonDown()
{ return QGuiApplication::mouseButtons() != Qt::NoButton; }
}
#endif
