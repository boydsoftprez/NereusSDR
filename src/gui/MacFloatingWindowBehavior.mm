// =================================================================
// src/gui/MacFloatingWindowBehavior.mm  (Longpath)
// =================================================================
// Siehe MacFloatingWindowBehavior.h fuer Zweck und Begruendung.
// =================================================================

#import <AppKit/AppKit.h>

#include "gui/MacFloatingWindowBehavior.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

namespace Longpath {

void enableFullScreenAuxiliaryBehavior(QWidget* widget)
{
    if (!widget) { return; }

    // Bug fix 2026-09-08 (erster echter CI-Testlauf auf macOS, alle
    // *FloatingWindow-Konstruktoren SEGFAULT): unter QT_QPA_PLATFORM=
    // offscreen (CI, ci.yml) erzeugt winId() KEIN echtes natives
    // NSView -- der zurueckgegebene WId-Wert ist eine synthetische
    // Kennung der Offscreen-Plattform-Engine, kein Zeiger auf ein
    // echtes Cocoa-Objekt. Der `!view`-Nullcheck weiter unten faengt
    // das NICHT ab (der Wert ist nicht null, nur ungueltig) -- der
    // (__bridge NSView*)-Cast liefert einen Zeiger, der beim ersten
    // objc_msgSend (hier: `.window`) sofort abstuerzt (SIGSEGV,
    // reproduziert per lldb: objc_msgSend -> enableFullScreenAuxiliary-
    // Behavior). Diese ganze Funktion ist ohnehin nur auf der echten
    // Cocoa-Plattform sinnvoll (schwebende NSPanel-Fensterebenen
    // existieren nicht ausserhalb davon) -- vor jedem Zugriff pruefen.
    if (QGuiApplication::platformName() != QLatin1String("cocoa")) { return; }

    // winId() erzwingt die Anlage des nativen Fensters, falls es noch
    // keines hat -- ohne das waere windowHandle() hier oft noch
    // nullptr, je nachdem wie frueh im Aufbau diese Funktion aufgerufen
    // wird.
    widget->winId();
    QWindow* qw = widget->windowHandle();
    if (!qw) { return; }

    // __bridge, nicht reinterpret_cast: unter ARC ist ein rohes
    // reinterpret_cast auf einen Objective-C-Objektzeiger verboten (ARC
    // kennt die Eigentumsregel nicht). __bridge sagt "kein
    // Eigentumsuebergang" -- richtig hier, winId() gehoert weiterhin
    // Qt/AppKit. Unter MRC ist __bridge ein zulaessiges No-Op.
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(qw->winId());
    if (!view) { return; }
    NSWindow* nsWindow = view.window;
    if (!nsWindow) { return; }

    nsWindow.collectionBehavior = nsWindow.collectionBehavior
        | NSWindowCollectionBehaviorFullScreenAuxiliary;

    // Betreiber 2026-08-31, per Bildschirmfreigabe direkt beobachtet:
    // diese Fenster standen ueber JEDEM anderen Programm auf dem Mac --
    // sogar ueber dem nackten Schreibtisch auf einem VOELLIG anderen
    // Space als das Hauptfenster. FullScreenAuxiliary (oben) regelt nur,
    // WELCHEM Space das Fenster folgt; es aendert nichts an der
    // Fensterebene selbst. Qt::Tool erzeugt auf macOS ein NSPanel mit
    // einem "schwebenden" Level, das laut Cocoa ausdruecklich UEBER
    // allen normalen Fenstern JEDER App liegt -- nicht nur ueber denen
    // von Longpath. hidesOnDeactivate war bis zum
    // 2026-11-15 genau dieser Schalter: das Fenster versteckt sich,
    // sobald Longpath nicht mehr die aktive App ist.
    //
    // ── Warum er wieder AUS ist (2026-11-15) ─────────────────────────
    //
    // Betreiber: „und immer, wenn ich ein anderes fenster ausserhalb
    // von longpath klicke, wird longpath schwarz, das muss geaendert
    // werden."
    //
    // Er hat recht, und es ist dieselbe Zeile. Seine Anordnung besteht
    // fast nur aus schwebenden Fenstern -- in seiner Einstellungsdatei
    // vom 15.11. stehen im Profil „Neu" fuenf davon (Asr, BwFilter,
    // Kombi, SignalInstrument, SwrInstrument) und im Profil „FT8" drei
    // (Ft8Decoder, Ft8Slots, Tx). Verstecken die sich alle auf einmal,
    // bleibt das Hauptfenster mit dem dunklen Spektrumgrund uebrig:
    // Longpath „wird schwarz". Wer kurz in den Browser schaut, um ein
    // Rufzeichen nachzusehen, verliert dabei die ganze Station aus dem
    // Blick.
    //
    // Die Sorge, die am 2026-08-31 zu diesem Schalter fuehrte, war eine
    // andere und bleibt gueltig: ein NSPanel auf NSFloatingWindowLevel
    // steht ueber den Fenstern JEDER App. Dafuer gibt es aber laengst
    // das richtige Mittel, gebaut und geprueft --
    // AuxiliaryWindowLeveler setzt die Ebene beim Verlassen auf
    // NSNormalWindowLevel herunter und beim Zurueckkommen wieder hoch.
    // Seit heute fasst er auch die Paletten an, nicht nur die
    // gewoehnlichen Nebenfenster.
    //
    // Der Unterschied in einem Satz: herunterstufen heisst „liegt wie
    // jedes andere Fenster", verstecken heisst „ist weg". Nur das
    // erste ist das, was eine Werkzeugpalette tun soll.
    if ([nsWindow isKindOfClass:[NSPanel class]]) {
        ((NSPanel*)nsWindow).hidesOnDeactivate = NO;
    }
}

void setPaletteWindowLevel(QWidget* widget, bool floating)
{
    if (!widget) { return; }
    // Dieselbe Offscreen-Falle wie oben: kein Cocoa, kein NSView.
    if (QGuiApplication::platformName() != QLatin1String("cocoa")) { return; }
    widget->winId();
    QWindow* qw = widget->windowHandle();
    if (!qw) { return; }
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(qw->winId());
    if (!view) { return; }
    NSWindow* nsWindow = view.window;
    if (!nsWindow) { return; }
    // Paletten (Qt::Tool) liegen bei Qt auf NSFloatingWindowLevel; ein
    // Fenster derselben Ebene laesst sich per orderFront ueber sie heben.
    nsWindow.level = floating ? NSFloatingWindowLevel : NSNormalWindowLevel;
    if (floating) {
        // Im nativen Vollbild dem Hauptfenster in dessen Space folgen,
        // statt macOS zum Space-Wechsel zu zwingen -- wie die Paletten.
        nsWindow.collectionBehavior = nsWindow.collectionBehavior
            | NSWindowCollectionBehaviorFullScreenAuxiliary;
    }
}

void ordneUeber(QWidget* fenster, QWidget* anker)
{
    if (!fenster || !anker || fenster == anker) { return; }
    // Dieselbe Offscreen-Falle wie oben: kein Cocoa, kein NSView.
    if (QGuiApplication::platformName() != QLatin1String("cocoa")) { return; }
    QWindow* qf = fenster->windowHandle();
    QWindow* qa = anker->windowHandle();
    // Kein winId() hier: ein Fenster, das noch kein natives hat, ist
    // auch nicht zu sehen -- es gibt nichts einzuordnen, und winId()
    // wuerde eines erzwingen.
    if (!qf || !qa) { return; }
    NSView* vf = (__bridge NSView*)reinterpret_cast<void*>(qf->winId());
    NSView* va = (__bridge NSView*)reinterpret_cast<void*>(qa->winId());
    if (!vf || !va) { return; }
    NSWindow* nf = vf.window;
    NSWindow* na = va.window;
    if (!nf || !na || !nf.isVisible) { return; }
    // Unmittelbar ueber den Anker, in derselben Ebene. Aktiviert nichts
    // und holt nichts vor fremde Programme: liegt der Anker hinter dem
    // Fenster einer anderen App, liegt das Panel danach ebenfalls
    // dahinter -- nur eben VOR dem eigenen Hauptfenster.
    [nf orderWindow:NSWindowAbove relativeTo:na.windowNumber];
}

void activateThisApplication()
{
    [NSApp activateIgnoringOtherApps:YES];
}

bool anyMouseButtonDown()
{
    return [NSEvent pressedMouseButtons] != 0;
}

} // namespace Longpath
