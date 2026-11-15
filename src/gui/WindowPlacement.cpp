// =================================================================
// src/gui/WindowPlacement.cpp  (Longpath)
// =================================================================
// Siehe WindowPlacement.h für Herkunft und die zwei bekannten
// Schwächen, die hier bewusst unverändert bleiben.
// =================================================================

#include "gui/WindowPlacement.h"
#include "gui/WindowPlacementBruecke.h"

#include <QSet>
#include "gui/MacFloatingWindowBehavior.h"
#include "gui/StyleConstants.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QLoggingCategory>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <QWidget>

Q_LOGGING_CATEGORY(lcWindowPlacement, "longpath.windowplacement")

namespace Longpath {

void ensureOnVisibleScreen(QWidget* w, QWidget* anchor, QSize minSize)
{
    if (!w) { return; }

    // A frameless Qt::Window with no explicit position defaults to (0,0)
    // on Windows, where it's usually obscured by the main application
    // window — which is how Container #0 "disappeared" when first
    // floated. Saved geometry at (0,0) (e.g., after the user closed the
    // invisible form) perpetuates the trap across restarts.
    const QRect g = w->geometry();
    const int minW = minSize.width();
    const int minH = minSize.height();

    // ── Genug, um es wieder greifen zu koennen (2026-10-30) ─────
    //
    // Hier stand `intersects()`: EIN Bildpunkt Ueberlappung zaehlte als
    // sichtbar. Ein Fenster, das zu 99 % ueber die Kante haengt, galt
    // damit als in Ordnung -- und wurde bei jedem Start wieder genau
    // dorthin gesetzt.
    //
    // Das stand seit 2026-08-16 als „Schwaeche 2" im Kopf von
    // WindowPlacement.h, samt der Groeszenordnung, die stattdessen
    // gelten sollte: „eine Mindestflaeche in der Groessenordnung von
    // 80 x 40 px, also genug, um das Fenster mit der Maus wieder
    // hereinziehen zu koennen." Dieselben zwei Zahlen benutzt
    // `nearAnchor` weiter unten schon -- es gab sie also im selben
    // Unterprogramm zweimal, einmal angewandt und einmal nur als
    // Vorschlag im Kommentar.
    //
    // Am 2026-10-30 am Geraet gesehen: das FT8-Panel stand links
    // ueber der Schirmkante, der Fenstertitel war auf „T8 DECODER"
    // abgeschnitten.
    //
    // Die Mindestflaeche wird am FENSTER gedeckelt: ein Fenster, das
    // selbst schmaler als 80 Punkte ist, koennte die Bedingung sonst
    // nie erfuellen und wuerde bei jedem Start in die Mitte gezogen.
    static constexpr int kMinSichtbarW = 80;
    static constexpr int kMinSichtbarH = 40;
    const int noetigW = qMin(kMinSichtbarW, g.width());
    const int noetigH = qMin(kMinSichtbarH, g.height());

    bool onScreen = false;
    for (QScreen* s : QGuiApplication::screens()) {
        if (!s) { continue; }
        const QRect sichtbar = s->availableGeometry().intersected(g);
        if (sichtbar.width() >= noetigW && sichtbar.height() >= noetigH) {
            onScreen = true;
            break;
        }
    }
    const bool atOrigin = (g.x() == 0 && g.y() == 0);

    QScreen* screen = nullptr;
    QRect anchorRect;
    if (anchor && anchor->window()) {
        screen = anchor->window()->screen();
        anchorRect = anchor->window()->geometry();
    }

    // Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" -- der
    // reine Schirm-Test oben erklaert das nicht, ein Fenster, das
    // grossteils auf dem physischen Bildschirm liegt, gilt danach immer
    // als "in Ordnung", selbst wenn das Hauptfenster laengst kleiner
    // geworden oder woanders hin verschoben ist und die gespeicherte
    // Position weit ausserhalb seiner heutigen Flaeche liegt. Gespeichert
    // wurde die Position ja oft in einer BREITEREN/Vollbild-Sitzung.
    //
    // Zusaetzlicher Test, nur wenn ein anchor da ist: die gespeicherte
    // Geometrie muss das HEUTIGE Hauptfenster in einer nennenswerten
    // Flaeche ueberlappen -- nicht deckungsgleich (ein Fenster darf
    // bewusst an der Kante angedockt sein), aber auch nicht nur um ein
    // paar Bildpunkte. Erster Anlauf hier pruefte nur "beruehrt sich
    // ueberhaupt" (mit einem um die eigene Fenstergroesse aufgeweiteten
    // Rand) -- am Betreiber-Fall gemessen liess das ein Fenster durch,
    // das nur mit 61 von 597 Bildpunkten Breite ueberhaupt noch die
    // Hauptfenster-Kante beruehrte, der Rest hing frei auf dem
    // Schreibtisch. kMinOverlap ist dieselbe Groessenordnung, die
    // WindowPlacement.h fuer die Sichtbarkeitspruefung selbst vorschlaegt
    // (Schwaeche 2 dort): genug Flaeche, um das Fenster mit der Maus
    // wieder greifen zu koennen, nicht nur ein Pixel Kontakt.
    // Betreiber 2026-08-31: "vor allem rotor, bandfilter und panadapter
    // sind immer anders als abgespeichert" -- IMMER, nicht gelegentlich.
    // Ursache: alle drei werden waehrend MainWindows eigenem Konstruktor
    // wiederhergestellt (Rotor-ToolWindow ueber detachRotorPanel() beim
    // Start, Bandfilter/Frequenz ueber das Profil-Apply, beides lange vor
    // dem ersten show()). Zu diesem Zeitpunkt hat `anchor` (MainWindow)
    // die Bildschirm-Ebene noch nie erreicht: restoreGeometry() setzt nur
    // die FENSTER-Grosse (z.B. 1280x800), der Vollbild/Maximiert-Zustand
    // greift erst, wenn das Betriebssystem das Fenster tatsaechlich zeigt
    // -- showFullScreen()/showMaximized() dafuer laufen selbst noch
    // spaeter in derselben Profil-Anwenden-Kette. Eine Position, die in
    // einer BREITEN/Vollbild-Sitzung gespeichert wurde, ueberlappt die zu
    // diesem fruehen Zeitpunkt noch schmale Ersatzgroesse fast nie -- und
    // wurde dadurch bei JEDEM Start/Import verworfen, nicht nur manchmal.
    // Deshalb gilt die Ueberlapp-Pruefung nur, wenn der Anker schon
    // sichtbar ist; vorher ist seine jetzige Groesse kein verlaesslicher
    // Massstab, und die gespeicherte Position verdient das Vertrauen.
    bool nearAnchor = true;
    if (anchorRect.isValid() && anchor->window()->isVisible()) {
        static constexpr int kMinOverlapW = 80;
        static constexpr int kMinOverlapH = 40;
        const QRect overlap = anchorRect.intersected(g);
        nearAnchor = overlap.width() >= kMinOverlapW
                  && overlap.height() >= kMinOverlapH;
    }

    // ── Zu klein ist kein Grund, das Fenster zu VERSCHIEBEN (2026-11-12) ─
    //
    // Betreiber, mit drei Bildern: „dann lade ich das Profil 01 und die
    // frequenz ist wieder in der mitte, so war sie nicht abgespeichert".
    // Hier stand die Groesse in derselben Bedingung wie die Lage: ein
    // Fenster an tadelloser Stelle, aber niedriger als minSize, fiel
    // durch und wurde in die Mitte des Hauptfensters gesetzt. Genau das
    // passiert den schwebenden Applets ohne Verbindung -- ihr Inhalt
    // fehlt, das Fenster ist kurz nur 93 hoch (gegen 120 als Mindest-
    // hoehe), und das Profil-Herstellen ruft diese Stelle. Frequenz und
    // SWR lagen danach auf demselben Punkt (656,400 am 12.11. frueh),
    // verdeckt hinter dem Panadapter, und der naechste Profilwechsel
    // sicherte die Mitte ins Profil.
    //
    // Jetzt: liegt es gut, waechst es an Ort und Stelle. In die Mitte
    // kommt nur, was wirklich verloren ist.
    if (onScreen && !atOrigin && nearAnchor) {
        if (g.width() < minW || g.height() < minH) {
            w->resize(qMax(g.width(), minW), qMax(g.height(), minH));
        }
        return;
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect avail = screen ? screen->availableGeometry()
                               : QRect(100, 100, 800, 600);

    const int width  = qMax(g.width(),  minW > 0 ? minW : 260);
    const int height = qMax(g.height(), minH > 24 ? minH : 300);

    int x = anchorRect.isValid()
        ? anchorRect.center().x() - width / 2
        : avail.x() + (avail.width()  - width) / 2;
    int y = anchorRect.isValid()
        ? anchorRect.center().y() - height / 2
        : avail.y() + (avail.height() - height) / 2;

    // Ist das Fenster breiter/hoeher als die verfuegbare Flaeche, laege
    // die obere Grenze UNTER der unteren. qBound(min, v, max) mit
    // max < min ist ungueltig: im Debug-Bau bricht Qts Zusicherung ab
    // ("!(max < min)", qminmax.h -- tst_pan_floating_window,
    // tst_panadapter_stack_layouts, tst_real_pan_float_state unter
    // Debug/ASAN, 2026-09-25), im Release-Bau kam still die linke bzw.
    // obere Kante heraus. Genau das wird jetzt ausdruecklich: ein
    // uebergrosses Fenster beginnt an der linken/oberen Kante der Flaeche.
    x = qBound(avail.x(), x, qMax(avail.x(), avail.right()  - width));
    y = qBound(avail.y(), y, qMax(avail.y(), avail.bottom() - height));

    w->setGeometry(x, y, width, height);
    qCDebug(lcWindowPlacement) << "repositioned to visible area"
                               << QRect(x, y, width, height) << "on screen"
                               << (screen ? screen->name()
                                          : QStringLiteral("(null)"));
}

QPoint snappedTopLeft(const QPoint& pos, int grid)
{
    if (grid <= 1) { return pos; }
    const auto snapAxis = [grid](int v) {
        return static_cast<int>(std::lround(v / static_cast<double>(grid))) * grid;
    };
    return QPoint(snapAxis(pos.x()), snapAxis(pos.y()));
}

// Letzter Rasterversuch je Fenster, siehe snapToGridAfterSettle().
static QHash<QWidget*, QRect> s_lastSnapAttempt;

QRect snappedFrameRect(const QRect& frame, const QSize& minSize, int grid,
                       int abstand)
{
    if (grid <= 1 || !frame.isValid()) { return frame; }
    const auto snapAxis = [grid](int v) {
        return static_cast<int>(std::lround(v / static_cast<double>(grid))) * grid;
    };
    const int left   = snapAxis(frame.x());
    const int top    = snapAxis(frame.y());
    int right  = snapAxis(frame.x() + frame.width());
    int bottom = snapAxis(frame.y() + frame.height());

    // ── Der Abstand gehoert INS Raster (2026-10-31) ──────────────────
    //
    // Betreiber, mit einem Bildschirmfoto: „und der raster funktioniert
    // fuer dich ausrichtung auch nicht perfekt."
    //
    // Nachgemessen an genau diesem Bild (Hauptfenster 1470 Punkte breit,
    // Bild 2000 Bildpunkte, also Faktor 1,3605):
    //
    //   BANDWIDTH FILTER  links  16, rechts  640   (beide auf 16er-Linien)
    //   FREQUENCY         links  16, rechts  288   (desgleichen)
    //   TX                links 642            ->  ZWEI daneben
    //   PANADAPTER        links 290, oben 358  ->  ZWEI daneben
    //   FREQUENCY         oben  356
    //
    // Frequenz und Panadapter stehen also um zwei Punkte versetzt
    // nebeneinander, obwohl beide „eingerastet" sind. Der Grund steht
    // in `snapToGridAfterSettle`: dort rastete erst `snappedFrameRect`
    // alle vier Kanten auf Vielfache von 16 (zwei Nachbarn liegen dann
    // BUENDIG), und erst DANACH schob `mitMindestabstand` den einen um
    // den einen um die Panelluft weg. Damit faellt dieses Fenster neben das
    // Raster — und bleibt dort: jede spaetere Rasterung rundet seine
    // Kanten wieder auf dieselben 16er-Linien, der Abstand schiebt sie
    // wieder um 2 heraus. Zwei Regeln, die gegeneinander arbeiten.
    //
    // Jetzt traegt das Raster den Abstand selbst: die NAHE Kante
    // (links/oben) liegt auf der Linie, die FERNE (rechts/unten) um
    // `abstand` davor. Zwei Fenster an derselben Linie haben damit
    // automatisch genau `abstand` Luft, und ALLE Kanten stammen weiter
    // aus demselben Raster. `mitMindestabstand` findet dann nichts mehr
    // zu korrigieren (sein `zuNah` verlangt `spalt < abstand`, und der
    // Spalt ist genau `abstand`) — es bleibt fuer die Faelle, fuer die
    // es gebaut wurde: Fenster, die gar nicht gerastert wurden.
    //
    // Preis: jedes Fenster wird um `abstand` schmaler und niedriger als
    // seine Rasterzelle. 272 Punkte Breite werden zu 270. Das ist die
    // Luft, die vorher auf einer Seite fehlte.
    const int luft = qMax(0, abstand);
    const int minW = qMax(grid - luft, minSize.width());
    const int minH = qMax(grid - luft, minSize.height());
    while (right - luft - left < minW)  { right  += grid; }
    while (bottom - luft - top < minH)  { bottom += grid; }
    return QRect(left, top, right - luft - left, bottom - luft - top);
}

QPoint bezugspunktFuerProfil(Qt::WindowStates zustand, const QRect& rahmen,
                             const QRect& verfuegbar)
{
    if ((zustand & Qt::WindowMaximized) && !(zustand & Qt::WindowFullScreen)
        && verfuegbar.isValid()) {
        return verfuegbar.topLeft();
    }
    return rahmen.topLeft();
}

bool hauptrahmenSteht(Qt::WindowStates zustand, const QRect& rahmen,
                      const QRect& verfuegbar)
{
    return rahmen.topLeft() == bezugspunktFuerProfil(zustand, rahmen, verfuegbar);
}

QRect mitMindestabstand(const QRect& wunsch, const QList<QRect>& nachbarn,
                        int abstand, int maxUeberlappung)
{
    if (abstand <= 0 || !wunsch.isValid()) { return wunsch; }

    QRect ziel = wunsch;

    // Halboffen gerechnet: zwei Bereiche ueberlappen, wenn der eine
    // beginnt, bevor der andere endet. Beruehrende Kanten (a2 == b1)
    // sind KEINE Ueberlappung -- sonst waere jedes Fenster, das mit
    // einer Ecke anstoesst, in beiden Achsen Nachbar und bekaeme zwei
    // Korrekturen fuer eine Beruehrung.
    const auto ueberlappt = [](int a1, int a2, int b1, int b2) {
        return a1 < b2 && b1 < a2;
    };
    // Trifft der Spalt den schmalen Bereich um die Beruehrung? Der Spalt
    // ist die Luft zwischen beiden Kanten: 2 heisst zwei Punkte Luft,
    // 0 buendig, -5 fuenf Punkte uebereinander.
    const auto zuNah = [abstand, maxUeberlappung](int spalt) {
        return spalt < abstand && spalt >= -maxUeberlappung;
    };

    for (const QRect& o : nachbarn) {
        if (!o.isValid() || o.isEmpty()) { continue; }
        // Eines ganz im anderen ist kein Nebeneinander, sondern ein
        // Uebereinander -- und der haeufigste Fall davon ist ein
        // schwebendes Fenster auf dem Hauptfenster.
        if (o.contains(ziel) || ziel.contains(o)) { continue; }

        // Waagerechte Nachbarschaft: die Fenster ueberlappen senkrecht.
        if (ueberlappt(ziel.top(), ziel.bottom() + 1, o.top(), o.bottom() + 1)) {
            if (ziel.left() >= o.left()) {
                if (zuNah(ziel.left() - o.right() - 1)) {
                    ziel.moveLeft(o.right() + 1 + abstand);
                }
            } else {
                if (zuNah(o.left() - ziel.right() - 1)) {
                    ziel.moveLeft(o.left() - abstand - ziel.width());
                }
            }
        }

        // Senkrechte Nachbarschaft: die Fenster ueberlappen waagerecht.
        // Gerechnet auf dem schon waagerecht korrigierten `ziel`: ist es
        // seitlich weggeruecht, steht es nicht mehr darueber.
        if (ueberlappt(ziel.left(), ziel.right() + 1, o.left(), o.right() + 1)) {
            if (ziel.top() >= o.top()) {
                if (zuNah(ziel.top() - o.bottom() - 1)) {
                    ziel.moveTop(o.bottom() + 1 + abstand);
                }
            } else {
                if (zuNah(o.top() - ziel.bottom() - 1)) {
                    ziel.moveTop(o.top() - abstand - ziel.height());
                }
            }
        }
    }
    return ziel;
}

namespace {
std::function<void(QWidget*)> s_zugmelder;
// Wieviele `EigeneBewegung` gerade leben. Ein Zaehler, nicht ein
// Schalter: die Stellen schachteln sich (das Raster laeuft waehrend des
// Herausholens weiter).
int s_eigeneBewegungen = 0;
}

void setzeZugmelder(std::function<void(QWidget*)> melder)
{
    s_zugmelder = std::move(melder);
}

EigeneBewegung::EigeneBewegung()  { ++s_eigeneBewegungen; }
EigeneBewegung::~EigeneBewegung() { --s_eigeneBewegungen; }

bool eigeneBewegungLebt() { return s_eigeneBewegungen > 0; }

bool istRasterNachhall(const QRect& jetzt, const QRect& vorher, int grid)
{
    if (!jetzt.isValid() || !vorher.isValid()) { return false; }
    return qAbs(jetzt.x() - vorher.x()) < grid
        && qAbs(jetzt.y() - vorher.y()) < grid
        && qAbs(jetzt.width() - vorher.width()) < grid + grid / 2
        && qAbs(jetzt.height() - vorher.height()) < grid + grid / 2;
}
bool ragtUeberDenFreienPlatz(const QRect& rahmen, const QRect& frei, int grid)
{
    if (!rahmen.isValid() || !frei.isValid()) { return false; }
    // Um weniger als das Raster daneben ist kein Ueberstand: das
    // maximierte Fenster steht mal bei (1,33 1468x865), mal eingerastet
    // bei (0,33 1470x867).
    return rahmen.left()   < frei.left()   - grid
        || rahmen.top()    < frei.top()    - grid
        || rahmen.right()  > frei.right()  + grid
        || rahmen.bottom() > frei.bottom() + grid;
}

EigeneBewegungBruecke::EigeneBewegungBruecke()  { ++s_eigeneBewegungen; }
EigeneBewegungBruecke::~EigeneBewegungBruecke() { --s_eigeneBewegungen; }

// Siehe WindowPlacementBruecke.h: dieselben zwei Zeilen wie in
// PanFloatingWindow::moveEvent, fuer Fenster, die nicht von uns sind.
void rasterWieSchwebendesFenster(QWidget* w)
{
    snapToGridAfterSettle(w);
    meldeZug(w);
}

void meldeZug(QWidget* bewegtes)
{
    if (!bewegtes) { return; }
    // Wir haben selbst geschoben -- das ist kein Ziehen. Siehe
    // EigeneBewegung in WindowPlacement.h.
    if (s_eigeneBewegungen > 0) { return; }
    // Was man nicht sieht, kann man nicht ziehen.
    //
    // Gefunden am 2026-10-20: Qt schickt die aufgestauten Lage- und
    // Groessenereignisse eines Fensters, BEVOR es sichtbar wird --
    // `QWidgetPrivate::show_helper()` ruft
    // `sendPendingMoveAndResizeEvents()` zuerst und das `showEvent`
    // erst danach. Von `meldeZug()` aus sahen zwei davon hintereinander
    // wie ein Ziehen aus, und der Hinweis stand da, bevor jemand die
    // Maus angefasst hatte. Dasselbe beim Malen eines verborgenen
    // Fensters in ein Bild (Vorschaubilder der Fensteranordnung).
    if (!bewegtes->isVisible()) { return; }
    if (s_zugmelder) { s_zugmelder(bewegtes); }
}

KlickHoltNachVorne::KlickHoltNachVorne(std::function<bool(QWidget*)> melder, QObject* eltern)
    : QObject(eltern), m_melder(std::move(melder))
{
    if (qApp) { qApp->installEventFilter(this); }
}

bool KlickHoltNachVorne::eventFilter(QObject* ziel, QEvent* ereignis)
{
    if (ereignis->type() != QEvent::MouseButtonPress) { return false; }
    auto* widget = qobject_cast<QWidget*>(ziel);
    return widget && m_melder(widget);
}

QRect freierPlatz(const QSize& groesse, const QRect& flaeche,
                  const QList<QRect>& belegt, int schritt)
{
    const auto aufRaster = [schritt](int wert) {
        const int rest = ((wert % schritt) + schritt) % schritt;
        return rest == 0 ? wert : wert + schritt - rest;
    };
    for (int y = aufRaster(flaeche.top()); y + groesse.height() - 1 <= flaeche.bottom();
         y += schritt) {
        for (int x = aufRaster(flaeche.left()); x + groesse.width() - 1 <= flaeche.right();
             x += schritt) {
            const QRect lage(QPoint(x, y), groesse);
            const bool frei = std::none_of(belegt.cbegin(), belegt.cend(),
                [&lage](const QRect& r) { return r.intersects(lage); });
            if (frei) { return lage; }
        }
    }
    return {};
}

QRect ausSperrstreifenSchieben(const QRect& wunsch,
                               const QList<QRect>& sperren,
                               const QRect& platz)
{
    if (!wunsch.isValid() || sperren.isEmpty()) { return wunsch; }

    // Nur die Streifen zaehlen, die das Fenster UEBERHAUPT treffen.
    // Einer auf einem anderen Schirm geht uns nichts an.
    QList<QRect> treffer;
    for (const QRect& sp : sperren) {
        if (sp.isValid() && sp.intersects(wunsch)) { treffer << sp; }
    }
    if (treffer.isEmpty()) { return wunsch; }

    // ── Ueber die LUECKEN, nicht ueber Ausweichschritte ─────────────────
    //
    // Der erste Entwurf schob das Fenster aus dem getroffenen Streifen
    // heraus und sah dann noch einmal nach. Das PENDELT: bei zwei nahen
    // Streifen weicht es aus dem oberen nach unten in den unteren aus,
    // aus dem unteren nach oben in den oberen, und nach vier Durchgaengen
    // kommt es auf einem Streifen zu liegen. Der eigene Prueffall
    // `mehrereStreifenNacheinander` hat genau das gefunden.
    //
    // Jetzt umgekehrt gedacht: die freien Baender zwischen den Streifen
    // aufzaehlen, das naechstgelegene nehmen, in das die Hoehe passt.
    // Das endet immer und kann nicht auf einem Streifen landen.
    QList<QRect> alle = sperren;
    std::sort(alle.begin(), alle.end(),
              [](const QRect& a, const QRect& b) { return a.top() < b.top(); });

    const QRect flaeche = platz.isValid()
        ? platz
        : QRect(wunsch.left(), -100000, wunsch.width(), 200000);

    QList<QPair<int, int>> luecken;    // (oben, unten), jeweils einschliesslich
    int kante = flaeche.top();
    for (const QRect& sp : alle) {
        if (!sp.isValid()) { continue; }
        if (sp.top() - 1 >= kante) { luecken << qMakePair(kante, sp.top() - 1); }
        kante = qMax(kante, sp.bottom() + 1);
    }
    if (flaeche.bottom() >= kante) { luecken << qMakePair(kante, flaeche.bottom()); }

    const int h = wunsch.height();
    int besterOben = 0;
    int besteEntfernung = -1;
    for (const auto& l : luecken) {
        if (l.second - l.first + 1 < h) { continue; }   // zu schmal
        // In diesem Band so wenig wie moeglich verschieben.
        const int oben = qBound(l.first, wunsch.top(), l.second - h + 1);
        const int weg = qAbs(oben - wunsch.top());
        if (besteEntfernung < 0 || weg < besteEntfernung) {
            besteEntfernung = weg;
            besterOben = oben;
        }
    }
    if (besteEntfernung < 0) {
        // Nirgends mehr hin. Unveraendert zurueck: lieber verdeckt als
        // halb vom Schirm geschoben -- ein Fenster, dessen Titelleiste
        // ausserhalb liegt, laesst sich nicht mehr fassen.
        return wunsch;
    }

    QRect r = wunsch;
    r.moveTop(besterOben);
    return r;
}

// ── „Der Bediener zieht gerade" ──────────────────────────────────────
//
// Am 2026-10-15 stand der Mindestabstand schon einmal hier und musste
// wieder heraus (5922f1b6). Betreiber, mit einem Bild: „warum ist das
// alles verstellt". Die Funktion lief bei JEDEM moveEvent, also auch
// waehrend das Layoutprofil seine Fenster setzt -- dort liegen Fenster
// fuer einen Moment uebereinander, sie schob sie auseinander, und die
// Anordnung war zerlegt. Der Ausbau-Commit nannte auch, was fehlte:
// „Eine Mindestabstand-Automatik braucht also einen Zustand ('der
// Bediener zieht gerade'), den WindowPlacement nicht hat."
//
// Das ist dieser Zustand. Zwei Bedingungen, die sich gegenseitig
// abdecken, weil jede fuer sich ein Loch hat:
//
//  1. KEINE `EigeneBewegung`. Wer selbst schiebt, sagt es -- das Raster
//     hier, das Herausholen aus dem Sperrstreifen, und seit heute
//     `LayoutProfiles::runApply()`, der Trichter JEDER Profilanwendung
//     (Wechsel, Start, Import). Das Loch: Qt liefert nach einem
//     `setGeometry()` auf manchen Fensterarten noch ein spaetes,
//     asynchrones moveEvent -- das kommt an, wenn die `EigeneBewegung`
//     schon gestorben ist.
//  2. MEHR ALS ZWEI Meldungen mit wirklich veraenderter Lage. Genau die
//     Unterscheidung aus #280 („die ERSTE Meldung eines Fensters zaehlt
//     nicht", „nur, wenn sich die Lage WIRKLICH geaendert hat"), nur
//     eine Stufe strenger: ein Platzieren bewegt ein Fenster einmal,
//     mit dem spaeten Nachhall zweimal. Ein Zug am Titelbalken schickt
//     Dutzende Meldungen, da ist die Schwelle in Millisekunden erreicht.
//
// Die Spur haelt NUR Zahlen und ein Rechteck, nie einen Zugriff auf das
// Fenster: ein toter Schluessel ist darum harmlos. Aufgeraeumt wird er
// trotzdem, zusammen mit Timer und letztem Rasterversuch.
//
// Zurueckgesetzt wird erst, wenn das Fenster steht (Ziel == Lage) --
// ABSICHTLICH nicht bei der eigenen Bewegung, die das Ruecken selbst
// ausloest. Sonst fiele das Tor zwischen Ruecken und Nachhall zu, das
// naechste Rastern zoege das Fenster auf die Rasterlinie zurueck, und
// die zwei Punkte waeren nach einem Augenblick wieder weg (gemessen:
// mit Raster 16 und Abstand 2 ist die Rasterlinie genau die buendige
// Kante, von der wir gerade weggeruckt sind).
namespace {
struct Zugspur {
    QRect letzteLage;
    int bewegungen = 0;
};
QHash<QWidget*, Zugspur> s_zugspur;
constexpr int kZugMindestMeldungen = 3;
}

void snapToGridAfterSettle(QWidget* w, int grid, int delayMs, bool snapSize)
{
    if (!w) { return; }

    {
        Zugspur& spur = s_zugspur[w];
        const QRect jetzt = w->frameGeometry();
        if (!w->isVisible()) {
            // Was man nicht sieht, zieht niemand. Qt schickt die
            // aufgestauten Lageereignisse, BEVOR ein Fenster sichtbar
            // wird (siehe meldeZug()); ohne diese Zeile sahen allein sie
            // schon wie ein Zug aus.
            spur.bewegungen = 0;
        } else if (s_eigeneBewegungen == 0 && jetzt != spur.letzteLage) {
            ++spur.bewegungen;
        }
        spur.letzteLage = jetzt;
    }

    // Ein Debounce-Timer je Fenster, angelegt beim ersten Aufruf und an
    // `w` gebunden (QObject-Elternschaft), damit keine der vier
    // aufrufenden Klassen selbst ein Timer-Feld fuehren muss -- der
    // Aufruf aus moveEvent() bleibt eine Zeile. `timers` lebt nur
    // hauptthread-seitig, wie jeder Widget-/Timer-Code ohnehin.
    static QHash<QWidget*, QTimer*> timers;
    QTimer*& timer = timers[w];
    if (!timer) {
        timer = new QTimer(w);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, w, [w, grid, delayMs, timer, snapSize]() {
            // Betreiber 2026-09-02: TX-Fenster liess sich waehrend
            // eines Kanten-Groessenzugs (oben/links) nicht mehr
            // bewegen. Ursache: ein solcher Zug verschiebt den
            // Fensterursprung genauso wie ein Titelbalken-Zug -- beide
            // laufen ueber denselben nativen OS-Griff
            // (startSystemResize), FramelessResizer haelt selbst
            // keinen abfragbaren Zugzustand, den man hier pruefen
            // koennte. Haengt beim Ablauf dieses Timers noch eine
            // Maustaste, ist der Bediener vermutlich mitten in genau
            // so einem Zug (kurze Pause, nicht losgelassen) --
            // verschieben statt schnappen, sonst kaempft dieses
            // move() mit dem noch laufenden nativen Zug.
            //
            // anyMouseButtonDown() statt QGuiApplication::mouseButtons():
            // waehrend das System ein Fenster zieht oder groessert,
            // sieht Qt die Maustaste nicht (die Geste gehoert dem
            // Fenstersystem) -- das Raster griff sonst mitten in einen
            // langsamen Zug (2026-09-26).
            if (anyMouseButtonDown()) {
                timer->start(delayMs);
                return;
            }
            if (w->windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen
                                    | Qt::WindowMinimized)) {
                return;
            }
            if (!snapSize) {
                const QPoint snapped = snappedTopLeft(w->pos(), grid);
                // Nur bewegen, wenn es tatsaechlich abweicht -- der
                // Rueckstoss dieses move() loest selbst ein moveEvent()
                // aus, das diese Funktion erneut aufruft; beim zweiten
                // Mal stimmt die Position schon, also keine dritte Runde.
                if (snapped != w->pos()) {
                    const EigeneBewegung wirSelbst;
                    w->move(snapped);
                }
                return;
            }
            // Der RAHMEN rastet ein: bei rahmenlosen Fenstern dasselbe wie
            // geometry(), beim Hauptfenster samt Titelleiste -- sonst
            // stuenden dessen Aussenkanten neben den schwebenden daneben.
            const QRect frame = w->frameGeometry();
            const QSize deco = frame.size() - w->size();
            QRect target = snappedFrameRect(frame, w->minimumSize() + deco, grid);
            // Nie ueber die nutzbare Flaeche hinaus nach oben/links: das
            // Hauptfenster liegt z. B. bei y = 33 direkt unter der
            // Menueleiste; gerundet auf 32 schoebe macOS es zurueck, und
            // das Raster versuchte es alle 180 ms von neuem. Dann auf die
            // naechste Rasterlinie INNERHALB (40), untere Kante bleibt.
            if (const QScreen* scr = w->screen()) {
                const QRect avail = scr->availableGeometry();
                const auto ceilTo = [grid](int v) {
                    return static_cast<int>(std::ceil(v / static_cast<double>(grid))) * grid;
                };
                int left = target.left();
                int top = target.top();
                int right = target.left() + target.width();
                int bottom = target.top() + target.height();
                if (top < avail.top() && frame.top() >= avail.top()) {
                    top = ceilTo(avail.top());
                }
                if (left < avail.left() && frame.left() >= avail.left()) {
                    left = ceilTo(avail.left());
                }
                const QSize minFrame = w->minimumSize() + deco;
                while (right - left < qMax(grid, minFrame.width()))   { right += grid; }
                while (bottom - top < qMax(grid, minFrame.height()))  { bottom += grid; }
                target = QRect(left, top, right - left, bottom - top);
            }
            // ── Und dann zwei Punkte Luft, aber nur beim Ziehen ──────
            //
            // NACH dem Raster und nach der Flaechengrenze: der Abstand
            // ist die feinere Regel und darf das Ergebnis der groberen
            // um zwei Punkte korrigieren, nicht umgekehrt.
            if (s_zugspur.value(w).bewegungen >= kZugMindestMeldungen) {
                QList<QRect> nachbarn;
                for (const QWidget* other : QApplication::topLevelWidgets()) {
                    if (other == w || !other->isVisible() || other->isMinimized()) {
                        continue;
                    }
                    // Menues, Hinweisfenster und Werkzeugspitzen stehen
                    // nicht „neben" etwas -- sie liegen kurz darueber.
                    const Qt::WindowType art = other->windowType();
                    if (art == Qt::Popup || art == Qt::ToolTip
                        || art == Qt::SplashScreen) {
                        continue;
                    }
                    nachbarn << other->frameGeometry();
                }
                target = mitMindestabstand(target, nachbarn, Style::kFensterLuftPx);
            }
            if (target == frame) {
                s_lastSnapAttempt.remove(w);   // gelungen
                s_zugspur[w].bewegungen = 0;   // der Zug ist zu Ende
                return;
            }
            // Steht das Fenster nach einem Versuch nicht dort, und waere
            // der Versuch derselbe: das System haelt es fest (Menueleiste,
            // Dock, Bildschirmrand). Nicht weiter draengen.
            if (s_lastSnapAttempt.value(w) == target) {
                // Auch das ist ein Ende -- das Fenster steht, wo es
                // stehen kann. Der Zug gilt als vorbei, sonst zaehlte
                // die naechste einzelne Meldung auf dieser Zahl weiter.
                s_zugspur[w].bewegungen = 0;
                return;
            }
            s_lastSnapAttempt.insert(w, target);
            // Ein Aufruf fuer Lage und Groesse: move() und resize()
            // einzeln loesten zwei Runden aus. setGeometry() meint den
            // Innenbereich, also um die Rahmenstaerke versetzt. Wie beim
            // Punkt oben stimmt beim Rueckstoss schon alles.
            const QPoint inset = w->geometry().topLeft() - frame.topLeft();
            const EigeneBewegung wirSelbst;
            w->setGeometry(QRect(target.topLeft() + inset, target.size() - deco));
            // Sitzt es, ist der Versuch JETZT abgeschlossen -- nicht erst
            // im Nachlauf 180 ms spaeter. Der Nachlauf faellt aus, sobald
            // der Bediener vorher wieder zieht (jeder Zug startet den
            // Timer neu), und dann galt der naechste Zug auf dieselben
            // Rasterlinien als "das System haelt das Fenster fest" und
            // wurde verworfen (gefunden 2026-11-05 am Contest-Panel, das
            // nach einem kurzen Zug einfach neben dem Raster stehen blieb).
            if (w->frameGeometry() == target) {
                s_lastSnapAttempt.remove(w);
            }
        });
        QObject::connect(w, &QObject::destroyed, w, [w]() {
            timers.remove(w);
            s_lastSnapAttempt.remove(w);
            s_zugspur.remove(w);
        });
    }
    timer->start(delayMs);
}

// ── Auf den Schirm, an dem gearbeitet wird (2026-11-15) ─────────────
// Herleitung und Messung: WindowPlacement.h.

QRect uebertrageAufFlaeche(const QRect& rahmen, const QRect& vonFlaeche,
                           const QRect& nachFlaeche)
{
    if (!rahmen.isValid() || !vonFlaeche.isValid() || !nachFlaeche.isValid()) {
        return rahmen;
    }

    // Nie groesser als die Zielflaeche: ein Fenster von einem 2560er
    // Schirm passt nicht unveraendert auf einen 1710er.
    const int breite = qMin(rahmen.width(),  nachFlaeche.width());
    const int hoehe  = qMin(rahmen.height(), nachFlaeche.height());

    // Der Anteil des freien Platzes, der links (bzw. oben) liegt. Mit
    // dieser Rechnung bleibt die linke Kante links, die rechte rechts
    // und die Mitte in der Mitte -- ein blosses Umrechnen der
    // Koordinate (x * neueBreite / alteBreite) wuerde ein rechts
    // angelegtes Fenster auf dem kleineren Schirm ueber die Kante
    // schieben.
    const auto anteil = [](int lage, int flaecheAnfang, int flaecheMass,
                           int fensterMass) {
        const int frei = flaecheMass - fensterMass;
        if (frei <= 0) { return 0.0; }
        const double a = double(lage - flaecheAnfang) / double(frei);
        return qBound(0.0, a, 1.0);
    };
    const double ax = anteil(rahmen.x(), vonFlaeche.x(),
                             vonFlaeche.width(),  rahmen.width());
    const double ay = anteil(rahmen.y(), vonFlaeche.y(),
                             vonFlaeche.height(), rahmen.height());

    const int freiX = qMax(0, nachFlaeche.width()  - breite);
    const int freiY = qMax(0, nachFlaeche.height() - hoehe);
    const int x = nachFlaeche.x() + int(std::lround(ax * freiX));
    const int y = nachFlaeche.y() + int(std::lround(ay * freiY));
    return QRect(x, y, breite, hoehe);
}

int flaecheMitDerGroesstenUeberlappung(const QRect& rahmen,
                                       const QList<QRect>& flaechen)
{
    int beste = -1;
    qint64 besteFlaeche = 0;
    for (int i = 0; i < flaechen.size(); ++i) {
        const QRect schnitt = flaechen.at(i).intersected(rahmen);
        const qint64 f = qint64(schnitt.width()) * qint64(schnitt.height());
        if (f > besteFlaeche) { besteFlaeche = f; beste = i; }
    }
    return beste;
}

namespace {

// Welcher Schirm traegt `r` am meisten? An der Geometrie, nicht an
// QWidget::screen(): ein noch nie gezeigtes Fenster meldet dort den
// Hauptschirm, egal wohin seine wiederhergestellte Lage zeigt.
QScreen* schirmUnter(const QRect& r)
{
    const QList<QScreen*> schirme = QGuiApplication::screens();
    QList<QRect> flaechen;
    flaechen.reserve(schirme.size());
    for (QScreen* s : schirme) {
        flaechen << (s ? s->geometry() : QRect());
    }
    const int i = flaecheMitDerGroesstenUeberlappung(r, flaechen);
    return (i >= 0) ? schirme.at(i) : nullptr;
}

} // namespace

bool aufDenArbeitsschirm(QWidget* w, const QWidget* anker)
{
    if (!w || !anker) { return false; }
    const QWidget* ankerFenster = anker->window();
    if (!ankerFenster) { return false; }
    QScreen* ziel = ankerFenster->screen();
    if (!ziel) { return false; }

    const QRect jetzt = w->geometry();
    if (!jetzt.isValid()) { return false; }

    QScreen* steht = schirmUnter(jetzt);
    if (!steht) {
        // Ganz ausserhalb aller Schirme -- dafuer ist
        // ensureOnVisibleScreen() da, nicht diese Funktion.
        return false;
    }
    if (steht == ziel) { return false; }

    const QRect soll = uebertrageAufFlaeche(jetzt, steht->availableGeometry(),
                                            ziel->availableGeometry());
    if (soll == jetzt) { return false; }

    // Wer selbst schiebt, sagt es: sonst sieht meldeZug() hier ein
    // Ziehen und der Schutzrahmen-Hinweis geht auf (2026-10-20).
    const EigeneBewegung wirSelbst;
    w->setGeometry(soll);
    qCDebug(lcWindowPlacement) << "auf den Arbeitsschirm" << ziel->name()
                               << "geholt:" << jetzt << "->" << soll;
    return true;
}

double coveredFraction(const QRect& target, const QList<QRect>& covers)
{
    if (target.isEmpty()) { return 1.0; }
    constexpr int kSteps = 16;
    int covered = 0;
    for (int iy = 0; iy < kSteps; ++iy) {
        for (int ix = 0; ix < kSteps; ++ix) {
            // Zellmitten, damit die Raender nicht doppelt zaehlen.
            const QPoint p(target.x() + (2 * ix + 1) * target.width() / (2 * kSteps),
                           target.y() + (2 * iy + 1) * target.height() / (2 * kSteps));
            for (const QRect& c : covers) {
                if (c.contains(p)) { ++covered; break; }
            }
        }
    }
    return static_cast<double>(covered) / (kSteps * kSteps);
}

QRect fitFrameIntoArea(const QRect& frame, const QRect& area,
                       const QSize& minFrame)
{
    if (area.isEmpty()) { return frame; }
    const QSize size = frame.size().boundedTo(area.size()).expandedTo(minFrame);
    // So wenig wie moeglich schieben: erst die rechte/untere Kante
    // herein, dann die linke/obere -- die gewinnt, falls die Mindest-
    // groesse nicht hineinpasst (Titelleiste bleibt erreichbar).
    int x = std::min(frame.x(), area.x() + area.width() - size.width());
    int y = std::min(frame.y(), area.y() + area.height() - size.height());
    x = std::max(x, area.x());
    y = std::max(y, area.y());
    return QRect(QPoint(x, y), size);
}

void fitIntoAvailableScreen(QWidget* w)
{
    if (!w || !w->isVisible()) { return; }
    QScreen* scr = w->screen();
    if (!scr) { return; }
    const QRect frame = w->frameGeometry();
    // Rahmen (Titelleiste) = Unterschied zwischen aussen und innen.
    const QSize extra = frame.size() - w->size();
    // Kleiner als sein Layout erlaubt, wird es ohnehin nicht.
    const QSize minClient = w->minimumSize().expandedTo(w->minimumSizeHint());
    const QRect fit = fitFrameIntoArea(frame, scr->availableGeometry(),
                                       minClient + extra);
    if (fit == frame) { return; }
    w->resize(fit.size() - extra);
    w->move(fit.topLeft());
}

} // namespace Longpath
