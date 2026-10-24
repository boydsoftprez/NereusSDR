// src/gui/instruments/FrequencyInstrument.cpp

// --- From src/gui/instruments/FrequencyInstrument.h ---
// =================================================================
// src/gui/instruments/FrequencyInstrument.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Frequenz in der Handschrift der Instrumente ──────────────────
//
// OE5SOS, 2026-08-16, Entwurf ~/Downloads/frequenz-im-zeigerstil.html.
// Drei Varianten, im Widget umschaltbar:
//
//   A Bandstreifen — die Rille gerade gezogen, Verlauf bis zur
//     aktuellen Stelle im Band, Bandkanten als rote Striche, die
//     naechstgelegene Beschriftung hell.
//   B Flacher Bogen — derselbe Bogen wie die Instrumente, sehr weit
//     und flach, Marke auf der Frequenz.
//   C Nur die Zahl — Glut darunter, sonst nichts.
//
// Mulde, Verlauf und Glut kommen aus InstrumentPainter, die Geometrie
// aus LinearSpine bzw. ArcSpine. Dieselben drei Mittel wie beim Zeiger
// und beim Balken; hier steht nichts davon ein zweites Mal.
//
// ── Es ZEIGT nicht nur, es BEDIENT ──────────────────────────────────
//
// Auflage des Betreibers, 2026-08-17: „Das Frequenz-Widget muss Rad
// und Klick-Eingabe tragen, nicht nur anzeigen — sonst nimmt Punkt 3
// das Abstimmen weg."
//
// Also: jede Ziffer eine eigene Trefferflaeche. Mausrad darueber dreht
// DIESE Dekade. Doppelklick auf die Zahl oeffnet ein Eingabefeld.
// Die beiden Hertz-Stellen stehen matt, weil sie beim Abstimmen
// dauernd laufen.
//
// ── Bandkanten ──────────────────────────────────────────────────────
//
// Rot, und zwar kraeftig. CLAUDE.local.md: „Die Bandkante bleibt rot.
// Sie ist keine dezente Markierung und faellt nicht unter die
// Zwei-Prozent-Regel des Hausstils." Die Grenzen kommen aus
// AmateurBands::containing() — dieselbe Tabelle, die auch der
// SWR-Durchlauf benutzt.
//
// Der Guard selbst wird hier NICHT umgangen und auch nicht nachgebaut:
// dieses Widget aendert die Frequenz ueber dasselbe Signal wie jede
// andere Abstimmung. Stellenweises Drehen fuehrt so wenig ueber eine
// Bandkante wie das Rad auf der Flagge.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/instruments/FrequencyInstrument.cpp ---
// =================================================================
// src/gui/instruments/FrequencyInstrument.cpp  (Longpath)
// =================================================================
// Siehe FrequencyInstrument.h.
// =================================================================

// =================================================================
// src/gui/instruments/FrequencyInstrument.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/FrequencyInstrument.h
//   src/gui/instruments/FrequencyInstrument.cpp
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Namespace Longpath becomes NereusSDR.
//   Style:: resolves to the Longpath values in WorkspacePalette and
//   WorkspaceStyle through gui/instruments/InstrumentStyle.h, never to
//   NereusSDR::Style in gui/StyleConstants.h.
//   Longpath's LP_MONO_QSS #define is not ported: the edit field's
//   font-family comes from Workspace::kMonoQssFamilies through .arg().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - minimumSizeHint() adds the strip or arc reservation:
//                 upstream's 74 clipped the digits in BandStrip and
//                 FlatArc at the minimum. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-07 - NumberOnly glow centred on the digits rather than on
//                 m_stack (which also spans the row's trailing stretch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-08 - VFO row letters from setSliceLetters() instead of the
//                 hard-coded A and B. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-15 - minimumSizeHint() never exceeds sizeHint()'s height:
//                 with Windows fonts the measured height came out below
//                 74 plus the reservation. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-20 - Display controls for Frequency (Custom): digit size,
//                 colour, centering, and each slice line on its own.
//                 Defaults leave the ported instrument unchanged.
//                 Brian Sutton, AI-assisted via xAI Grok.
// =================================================================

#include "gui/instruments/FrequencyInstrument.h"

#include "core/antenna/AmateurBands.h"
#include "gui/instruments/InstrumentStyle.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/instruments/InstrumentSpine.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>
#include <memory>

namespace NereusSDR {

// NereusSDR: the Longpath values, see gui/instruments/InstrumentStyle.h.
namespace Style = ::NereusSDR::InstrumentStyle;

namespace {

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:33 [@551576e3]
/// Wie im Entwurf: 7.139.700 — acht Ziffern, Punkt nach der ersten
/// und nach der vierten. Die Dekade je Ziffer ergibt sich daraus.
constexpr double kDecades[8] = {
    1e7, 1e6, 1e5, 1e4, 1e3, 1e2, 1e1, 1e0
};

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:39 [@551576e3]
/// Die beiden Hertz-Stellen stehen matt — sie laufen beim Abstimmen
/// dauernd und sollen den Blick nicht halten.
constexpr int kDimFromIndex = 6;

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:42 [@551576e3]
/// Hoehe der Mulde beim Bandstreifen (Entwurf: H = 14 auf 520 Breite).
constexpr double kStripHeight = 14.0;

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:52 [@551576e3]
/// Reservierte Hoehe fuer den Streifen im Konstruktor-Layout (root-
/// >addSpacing) UND fuer die Flaeche, in die paintEvent() ihn malt.
/// EIN Wert fuer beide Stellen, statt zwei getrennt gepflegter Zahlen:
/// bis 2026-08-30 berechnete paintEvent() seine Flaeche aus der
/// GEOMETRIE der (oft ausgeblendeten) A/B-Zeile -- fehleranfaellig,
/// weil ein verstecktes Widget in einem QBoxLayout nicht zuverlaessig
/// dieselbe Position traegt, mit der der Konstruktor gerechnet hat.
/// Jetzt rechnen beide Stellen mit derselben Zahl.
constexpr int kStripReserve = int(kStripHeight) + 10;

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:58 [@551576e3]
/// Wie viel Hoehe root() fuer eine gegebene Form reserviert -- die
/// EINZIGE Stelle, die das entscheidet. m_stripSpacer traegt das ins
/// echte Layout, sizeHint() liest es indirekt ueber layout()->sizeHint()
/// mit (siehe dort).
int stripReserveFor(FrequencyInstrument::Form f)
{
    switch (f) {
        case FrequencyInstrument::Form::NumberOnly: return 0;
        case FrequencyInstrument::Form::FlatArc:    return kStripReserve + 16;
        case FrequencyInstrument::Form::BandStrip:
        default:                                     return kStripReserve;
    }
}

// ── Zwei Abstaende, nicht einer ──────────────────────────────────────
//
// OE5SOS, 2026-08-18: „Der Trenner steht genauso weit ab wie die
// Ziffern untereinander, damit sieht das Auge acht Einzelzeichen statt
// drei Bloecke — und die Gruppierung, die wir gerade repariert haben,
// wird optisch wieder aufgehoben."
//
// Warum es dazu kam: die Zeile ist in Monospace gesetzt, und dort ist
// der Punkt genau so breit wie eine Ziffer. Das Zeichen ist schmal,
// seine ZELLE nicht — links und rechts davon steht je eine halbe leere
// Zelle. Der Trenner brachte damit von sich aus mehr Luft mit als jede
// Ziffernfuge, und die Zeile zerfiel in acht gleich weit stehende
// Zeichen.
//
// QFont::setLetterSpacing hilft hier nicht: jede Ziffer ist ein eigenes
// Schild mit eigener Trefferflaeche (Rad ueber der Stelle dreht diese
// Dekade), also setzt das Layout die Abstaende und nicht die Schrift.
// Dann sind es zwei Werte, wie der Betreiber gesagt hat — der Abstand
// INNERHALB einer Gruppe und der ZWISCHEN zweien.
//
// Alle drei als Anteil der Zeichenzelle, damit sie ueber die
// Schriftstufen halten.

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:93 [@551576e3]
/// Luft je Ziffer. Nur so viel, dass die Trefferflaeche nicht auf die
/// Glyphe schrumpft — innerhalb einer Gruppe soll es eng sein.
constexpr double kDigitPadOfCell = 0.09;

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:97 [@551576e3]
/// Der Trenner bekommt seine eigene, schmale Breite statt der vollen
/// Monospace-Zelle.
constexpr double kSeparatorOfCell = 0.30;

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:101 [@551576e3]
/// Der Gruppenabstand. Steht NUR hinter dem Trenner: „der Trenner darf
/// schmal bleiben und braucht keine Luft auf beiden Seiten."
constexpr double kGroupGapOfCell = 0.35;

} // namespace

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:105 [@551576e3] [modified in NereusSDR]
FrequencyInstrument::FrequencyInstrument(QWidget* parent)
    : QWidget(parent)
{
    // ── Nie mehr Hoehe nehmen, als sizeHint() sagt ────────────────────
    //
    // Betreiber 2026-08-30, zum wiederholten Mal: der Abstand zwischen
    // den Ziffern und dem, was darunter haengt (SWR-/Leistungszeilen im
    // FrequencyApplet), soll klein bleiben UND sich der Groesse
    // anpassen. Die vorige Reparatur (Stretch ans Ende dieses Layouts)
    // half nur INNERHALB dieses Widgets — sie sagte nichts darueber,
    // WIE VIEL Hoehe das AEUSSERE Layout (FrequencyApplet) diesem
    // Widget ueberhaupt zuteilt.
    //
    // Ohne eine eigene Policy ist ein QWidget vertikal "Preferred":
    // wird das Schwebefenster hoeher gezogen, als die Summe aller
    // Kinder braucht, verteilt Qt den Rest nach eigenem Ermessen — und
    // gab ihn bisher bevorzugt HIER hinein, weil dieses Widget durch
    // seinen eigenen Stretch elastisch wirkt. Sichtbar wurde das genau
    // als Luecke zwischen Ziffern und Balken, die mit jedem Ziehen
    // wuchs.
    //
    // QSizePolicy::Maximum sagt: sizeHint() ist die OBERGRENZE, kleiner
    // darf es werden (bis minimumSizeHint()), groesser nie. Wer das
    // Fenster jetzt hoeher zieht, bekommt die zusaetzliche Hoehe bei
    // den Balken darunter oder als Rand am Fensterende — nie mehr
    // hier eingeklemmt.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(4);

    // Die Zahl, und darunter Platz fuer Streifen oder Bogen. Der
    // Zwischenraum wird gemalt, nicht ausgelegt: paintEvent kennt die
    // Form und weiss, wieviel Platz sie braucht.
    m_stack = new QStackedWidget(this);
    m_digitRow = new QWidget(m_stack);
    m_digitLayout = new QHBoxLayout(m_digitRow);
    m_digitLayout->setContentsMargins(0, 0, 0, 0);
    m_digitLayout->setSpacing(0);
    buildDigits();

    m_edit = new QLineEdit(m_stack);
    m_edit->setPlaceholderText(QStringLiteral("MHz"));
    applyEditStyle();
    connect(m_edit, &QLineEdit::editingFinished,
            this, &FrequencyInstrument::commitEdit);

    m_stack->addWidget(m_digitRow);
    m_stack->addWidget(m_edit);
    root->addWidget(m_stack, 0);

    // Hier malt paintEvent Streifen oder Bogen -- WENN die Form ueberhaupt
    // einen braucht. Betreiber 2026-08-30, zu NumberOnly (seit heute die
    // Vorgabe): "ziffern sind abgeschnitten". Ursache: die Reservierung
    // stand hier bis eben als FESTES root->addSpacing(kStripReserve),
    // unabhaengig von m_form -- sizeHint() zog fuer NumberOnly zwar
    // kStripReserve wieder ab (formDelta), aber das eigene Layout
    // (root) reservierte den Platz bei der eigentlichen Auslegung
    // trotzdem weiter fest. Root bekam dadurch WENIGER Hoehe von aussen
    // zugeteilt, als es fuer Ziffernzeile + fixe Reservierung selbst
    // brauchte -- und ein addSpacing()-Posten kann, anders als ein
    // Stretch, nicht nachgeben. Die Ziffernzeile wich aus, oben
    // abgeschnitten.
    //
    // Der Ausweg: ein ECHTER QSpacerItem*, dessen Groesse setForm()
    // mitzieht -- root reserviert dann fuer JEDE Form immer genau so
    // viel, wie sizeHint() dafuer meldet. Keine zwei Zahlen mehr, die
    // auseinanderlaufen koennen.
    m_stripSpacer = new QSpacerItem(0, stripReserveFor(m_form));
    root->addItem(m_stripSpacer);

    m_vfoRow = new QLabel(this);
    m_vfoRow->setTextFormat(Qt::RichText);
    m_vfoRow->setFont(Style::monoFont(m_vfoRow->font(), Style::kFontBody));
    root->addWidget(m_vfoRow, 0);

    // Ueberschuss (falls das Fenster hoeher gezogen wird) gehoert ans
    // ENDE, nicht zwischen die Zeilen.
    root->addStretch(1);

    refreshDigits();
    refreshVfoRow();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:194 [@551576e3] [modified in NereusSDR]
QSize FrequencyInstrument::minimumSizeHint() const
{
    // War {240, 74}, geraten statt gerechnet -- bei Style::kFontDisplay
    // (38px) brauchte die Ziffernzeile tatsaechlich naeher an 300px,
    // und das Fenster liess sich unter das, was die kleinste Stufe
    // braucht, gar nicht erst ziehen. Jetzt aus derselben Rechnung wie
    // resizeEvent()/buildDigits(), bei der kleinsten erlaubten Stufe:
    // das ist die einzige Breite, unter die dieses Widget nie faellt.
    // A locked operator size cannot shrink, so the floor is that size.
    // The ported instrument stays on the 12 px floor.
    const int fontFloor = m_fontLocked ? m_digitFontPx : kMinDigitFontPx;
    const int w = digitRowWidthAt(fontFloor) + 28;

    // Betreiber 2026-08-30, wieder: "das ist das kleinste Fenster" --
    // und trotzdem zu viel Abstand. Die 74 hier blieben stehen, auch
    // wenn die A/B-Zeile ausgeblendet ist -- derselbe Fehler, den
    // sizeHint() weiter unten fuer die BEVORZUGTE Groesse schon behebt
    // ("A/B ausblenden" gab bislang nur bei sizeHint() Platz zurueck,
    // nicht bei der UNTERGRENZE). Damit liess sich das Fenster nie so
    // weit ziehen, wie das Ausblenden eigentlich erlaubt haette.
    int h = 74;
    if (m_vfoRow && !m_vfoRow->isVisible()) {
        h = qMax(40, h - m_vfoRow->sizeHint().height() - 4);
    }
    // NereusSDR: the strip or arc reservation (m_stripSpacer) is added
    // to the minimum height. Upstream's 74 counts only the digit and A/B
    // rows, so BandStrip and FlatArc at the minimum squeezed the digit
    // row and clipped the tops of the digits (seen in the
    // instruments-sheet preview scene). NumberOnly is unchanged (its
    // reservation is 0).
    h += stripReserveFor(m_form);
    // NereusSDR: never above the preferred height. sizeHint() measures the
    // layout, and with Windows fonts that came out below 74 plus the
    // reservation, so the minimum left a gap under the digits there. On
    // macOS the measured height is the larger one and nothing changes.
    h = qMin(h, sizeHint().height());
    return {qMax(240, w), h};
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:218 [@551576e3]
QSize FrequencyInstrument::sizeHint() const
{
    // Betreiber 2026-08-30, zum siebenten Mal: der Abstand zwischen
    // Ziffern und dem, was darunter haengt, sei zu gross -- auch nach
    // drei vorigen Anlaeufen an dieser Stelle (Stretch ans Ende der
    // Anordnung, Reservierung fuer den Streifen verkleinert, Ziffern
    // verkleinert). Jeder davon senkte den tatsaechlichen INHALT, aber
    // hier stand weiterhin eine GERATENE Zahl (116/132/86 -- fest im
    // Quelltext, unabhaengig davon, was root (das eigene Layout)
    // wirklich braucht). Verlangte der echte Inhalt WENIGER, als die
    // geratene Zahl versprach -- etwa weil die A/B-Zeile ausgeblendet
    // ist oder die Ziffern jetzt kleiner sind -- schluckte root's
    // eigener Stretch(1) am Ende genau diese Differenz als Leerraum,
    // GENAU an der Stelle, an der FrequencyApplet gleich die SWR-/
    // Leistungszeilen anschliesst. Jede vorige Reparatur hat den Inhalt
    // kleiner gemacht, ohne dass die geratene Zahl hier mitgezogen
    // waere -- der Rest wanderte einfach weiter in den Stretch.
    //
    // Der Ausweg: nicht mehr raten. layout()->sizeHint() fragt root
    // selbst, was es braucht -- Ziffernzeile bei der AKTUELLEN
    // Schriftgroesse (m_digitFontPx), die A/B-Zeile nur wenn sichtbar
    // (ein verstecktes Widget zaehlt in einem QBoxLayout mit Null),
    // alle Raender. Stimmt IMMER mit dem ueberein, was tatsaechlich
    // gezeichnet wird, weil es dieselbe Rechnung ist -- kein zweiter
    // Ort mehr, der veralten kann.
    //
    // m_stripSpacer traegt die Streifen-/Bogen-Reservierung direkt ins
    // Layout ein (Groesse haengt von m_form ab, siehe setForm() und
    // stripReserveFor()) -- root->sizeHint() zaehlt sie darum schon
    // richtig mit, ohne dass es hier noch einen Aufschlag braucht.
    if (QLayout* lay = layout()) {
        const QSize fromLayout = lay->sizeHint();
        return {qMax(360, fromLayout.width()), qMax(40, fromLayout.height())};
    }
    return {360, 104};
}

// ── Die Ziffern ──────────────────────────────────────────────────────

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:257 [@551576e3] [modified in NereusSDR]
void FrequencyInstrument::buildDigits()
{
    // Zweiter Aufruf (aus resizeEvent(), neue Schriftstufe): die alte
    // Zeile erst abraeumen. Ohne das wuerden Ziffern/Trenner/Einheit
    // sich hinter den neuen anhaeufen -- Qt loescht Kindwidgets nicht
    // von selbst, nur weil ein neues addWidget() kommt.
    if (!m_digits.isEmpty()) {
        QLayoutItem* item = nullptr;
        while ((item = m_digitLayout->takeAt(0)) != nullptr) {
            if (QWidget* w = item->widget()) { w->deleteLater(); }
            delete item;
        }
        m_digits.clear();
        m_decades.clear();
    }

    // ── Gruppen zu drei, von RECHTS gezaehlt ─────────────────────────
    //
    // Hier stand {0, 3}, und das war um eine Stelle verschoben: die
    // Trennpunkte sassen an festen Stellen von links, waehrend die
    // Gruppierung vom kleinsten Hertz her zaehlt. 7.139.700 Hz kam als
    // „0.713.9700" heraus — die fuehrende Null wurde mitgezaehlt.
    // Befund des Betreibers, 2026-08-18, gegen die VFO-Zeile darunter,
    // die es richtig zeigte.
    //
    // Die Stellen sind 10 MHz .. 1 Hz. Gruppen von rechts:
    //   (1 Hz, 10 Hz, 100 Hz) = 5,6,7
    //   (1 k, 10 k, 100 k)    = 2,3,4
    //   (1 M, 10 M)           = 0,1
    // Also Punkte NACH Stelle 1 und NACH Stelle 4.
    static const int kSepAfter[] = {1, 4};

    // Die Zeichenzelle der Ziffernschrift — Bezugsmass fuer alle drei
    // Abstaende. In Monospace ist sie fuer jedes Zeichen gleich, also
    // genuegt eine Abfrage.
    const QFont digitFont = Style::monoFont(m_digitRow->font(),
                                            m_digitFontPx,
                                            QFont::Light);
    const int cell = QFontMetrics(digitFont).horizontalAdvance(
        QStringLiteral("0"));
    const int digitPad = qMax(1, qRound(cell * kDigitPadOfCell));
    const int sepWidth = qMax(2, qRound(cell * kSeparatorOfCell));
    const int groupGap = qMax(2, qRound(cell * kGroupGapOfCell));

    // Frequency (Custom): equal slack on both sides centres the digits.
    // The ported instrument keeps the trailing slack only.
    if (m_centered) {
        m_digitLayout->addStretch(1);
    }

    for (int i = 0; i < kDigitCount; ++i) {
        auto* d = new QLabel(QStringLiteral("0"), m_digitRow);
        d->setAlignment(Qt::AlignCenter);
        d->setFont(digitFont);
        // Jede Ziffer ist eine eigene Trefferflaeche. Der Zeiger sagt
        // das an: senkrecht ziehen ist die Geste, die das Rad meint.
        d->setCursor(Qt::SizeVerCursor);
        d->setAttribute(Qt::WA_Hover, true);
        d->installEventFilter(this);
        d->setToolTip(tr("Scroll to tune this digit"));
        // Feste Mindestbreite: die fuehrende Stelle wird unter 10 MHz
        // LEER gezeigt, und ein leeres Schild ohne Breite naehme seine
        // eigene Trefferflaeche mit. Man koennte dann 7 MHz nicht mehr
        // auf 17 MHz drehen.
        // Feste Breite, nicht nur eine Mindestbreite: sonst bestimmte
        // der Schilder-sizeHint die Fuge, und der waere je nach Ziffer
        // verschieden — eine Spalte, die beim Abstimmen atmet.
        d->setFixedWidth(cell + digitPad);
        m_digits.append(d);
        m_decades.append(kDecades[i]);
        m_digitLayout->addWidget(d);

        for (int sep : kSepAfter) {
            if (i == sep) {
                auto* dot = new QLabel(QStringLiteral("."), m_digitRow);
                dot->setFont(digitFont);
                // Auf die schmale Breite geklemmt. Ohne das brachte der
                // Punkt die volle Monospace-Zelle mit und riss die
                // Gruppe auf, die er zusammenhalten soll.
                dot->setFixedWidth(sepWidth);
                dot->setAlignment(Qt::AlignCenter);
                dot->setStyleSheet(Style::themed(
                    QStringLiteral("QLabel { color: %1; }")
                        .arg(digitDimColor())));
                m_digitLayout->addWidget(dot);
                // Die Luft steht HINTER dem Trenner, nicht um ihn
                // herum: er gehoert zur Gruppe links von ihm.
                m_digitLayout->addSpacing(groupGap);
            }
        }
    }

    auto* unit = new QLabel(QStringLiteral("MHz"), m_digitRow);
    // „MHz" ist eine Beschriftung, keine Zahl: Versalzeile auf der
    // kleinen Stufe, mit derselben Laufweite wie jede andere.
    unit->setFont(Style::capsFont(unit->font(), Style::kFontSmall));
    unit->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: %1; }").arg(Style::kTextSecondary)));
    m_digitLayout->addSpacing(10);
    m_digitLayout->addWidget(unit, 0, Qt::AlignBottom);
    m_digitLayout->addStretch(1);
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:354 [@551576e3]
int FrequencyInstrument::digitRowWidthAt(int px)
{
    // Dieselbe Rechnung wie buildDigits(), aber ohne ein einziges
    // Schild zu bauen -- fuer resizeEvent() (welche Stufe passt?) und
    // minimumSizeHint() (was braucht die kleinste Stufe wirklich?).
    // Zwei Stellen, an denen dieselbe Breite entsteht, waeren zwei
    // Stellen, die auseinanderlaufen koennten; bleibt trotzdem als
    // eigene Funktion, weil buildDigits() echte Schilder braucht und
    // hier nur eine Zahl gefragt ist.
    const QFont f = Style::monoFont(QFont(), px, QFont::Light);
    const QFontMetrics fm(f);
    const int cell = fm.horizontalAdvance(QStringLiteral("0"));
    const int digitPad = qMax(1, qRound(cell * kDigitPadOfCell));
    const int sepWidth = qMax(2, qRound(cell * kSeparatorOfCell));
    const int groupGap = qMax(2, qRound(cell * kGroupGapOfCell));

    int w = kDigitCount * (cell + digitPad);
    w += 2 * (sepWidth + groupGap);   // zwei Trenner, nach Stelle 1 und 4

    const QFont unitFont = Style::capsFont(QFont(), Style::kFontSmall);
    w += 10 + QFontMetrics(unitFont).horizontalAdvance(QStringLiteral("MHz"));
    return w;
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:378 [@551576e3] [modified in NereusSDR]
int FrequencyInstrument::fittingDigitFontPx(int availableWidth) const
{
    const int ceiling = m_fontCeiling > 0 ? m_fontCeiling : kMaxDigitFontPx;
    const int floor = qMin(kMinDigitFontPx, ceiling);
    for (int px = ceiling; px > floor; --px) {
        if (digitRowWidthAt(px) <= availableWidth) { return px; }
    }
    return floor;
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:386 [@551576e3] [modified in NereusSDR]
void FrequencyInstrument::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);

    // A locked operator size stays put. The ported instrument still fits.
    if (m_fontLocked) { return; }

    // 28 = die seitlichen Raender aus dem VBoxLayout-Aufbau (14 je
    // Seite, siehe der Konstruktor).
    const int available = qMax(0, width() - 28);
    const int wanted = fittingDigitFontPx(available);
    if (wanted == m_digitFontPx) { return; }

    m_digitFontPx = wanted;
    buildDigits();
    refreshDigits();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:401 [@551576e3] [modified in NereusSDR]
void FrequencyInstrument::refreshDigits()
{
    const qint64 hz = static_cast<qint64>(std::llround(m_hz));
    // Ohne Frequenz ein Strich je Stelle, keine Nullen (Hausstil Regel
    // 7): "0.000.000 MHz" ohne Funkgeraet liest sich als eingestellte
    // Frequenz — auf dem Foto vom 2026-09-17 stand es so da. Ein
    // Strich je Stelle haelt die Zeile in ihrer Breite und sagt
    // "nichts", nicht "null".
    const bool none = (hz <= 0);
    for (int i = 0; i < m_digits.size(); ++i) {
        const qint64 dec = static_cast<qint64>(m_decades.at(i));
        const int digit = static_cast<int>((hz / dec) % 10);
        // Unter 10 MHz bleibt die vorderste Stelle leer statt „0". Die
        // Zeile darunter macht es genauso (0 wird dort nicht gepolstert),
        // und eine fuehrende Null liest sich als Teil der Zahl.
        const bool blank = (i == 0 && hz < 10000000);
        m_digits[i]->setText(none ? QStringLiteral("\u2013")
                             : blank ? QString()
                                     : QString::number(digit));
        const bool dim = none || (i >= kDimFromIndex);
        m_digits[i]->setStyleSheet(Style::themed(
            QStringLiteral("QLabel { color: %1; padding: 2px 1px;"
                           " border-radius: 6px; }"
                           "QLabel:hover { background: %2; }")
                .arg(dim ? digitDimColor() : digitBrightColor(),
                     Style::kBadgeWarnBg)));
    }
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:430 [@551576e3]
QString FrequencyInstrument::groupedText() const
{
    // Liest die WIRKLICHEN Schilder in der WIRKLICHEN Reihenfolge des
    // Layouts — keine zweite Fassung der Formatierung, die gruen bleiben
    // koennte, waehrend auf dem Schirm etwas anderes steht.
    QString out;
    QLayout* lay = m_digitRow ? m_digitRow->layout() : nullptr;
    if (!lay) { return out; }
    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem* it = lay->itemAt(i);
        auto* l = it ? qobject_cast<QLabel*>(it->widget()) : nullptr;
        if (!l) { continue; }
        // Die Einheit gehoert nicht zur Zahl.
        if (l->text() == QStringLiteral("MHz")) { continue; }
        out += l->text();
    }
    return out;
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:449 [@551576e3] [modified in NereusSDR]
void FrequencyInstrument::refreshVfoRow()
{
    // Beide Scheiben, die aktive hervorgehoben — Split ist damit ohne
    // ein zweites Feld zu sehen.
    auto fmt = [](double hz) {
        const qint64 v = static_cast<qint64>(std::llround(hz));
        // Ohne Frequenz ein Strich, keine Nullen (Regel 7) — wie die
        // grossen Ziffern darueber.
        if (v <= 0) { return QStringLiteral("\u2013.\u2013\u2013\u2013.\u2013\u2013\u2013"); }
        return QStringLiteral("%1.%2.%3")
            .arg(v / 1000000)
            .arg((v / 1000) % 1000, 3, 10, QLatin1Char('0'))
            .arg(v % 1000, 3, 10, QLatin1Char('0'));
    };
    const QString on = digitBrightColor();
    const QString off = digitDimColor();
    // NereusSDR: the letters come from setSliceLetters() (upstream
    // hard-codes "A %2" and "B %4"; the defaults are the same). With
    // both slice lines shown the text is that same string. Frequency
    // (Custom) may omit either line.
    QStringList parts;
    if (m_showOwn) {
        parts.append(QStringLiteral("<font color='%1'>%2 %3</font>")
                         .arg(m_activeIsThis ? on : off, QString(m_ownLetter), fmt(m_hz)));
    }
    if (m_showOther) {
        parts.append(QStringLiteral("<font color='%1'>%2 %3</font>")
                         .arg(m_activeIsThis ? off : on, QString(m_otherLetter), fmt(m_otherHz)));
    }
    m_vfoRow->setText(parts.join(QStringLiteral("&nbsp;&nbsp;")));
}

// ── Bedienen ─────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:474 [@551576e3]
bool FrequencyInstrument::eventFilter(QObject* watched, QEvent* ev)
{
    const int idx = m_digits.indexOf(qobject_cast<QLabel*>(watched));
    if (idx < 0) { return QWidget::eventFilter(watched, ev); }

    if (ev->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(ev);
        const int steps = we->angleDelta().y() > 0 ? 1 : -1;
        // Diese Dekade, nicht die eingestellte Schrittweite. Das ist der
        // Sinn der eigenen Trefferflaeche: man greift die Stelle an, die
        // man drehen will.
        const double next = m_hz + steps * m_decades.at(idx);
        if (next >= 0.0) {
            emit frequencyEdited(next);
        }
        return true;
    }
    if (ev->type() == QEvent::MouseButtonDblClick) {
        beginEdit();
        return true;
    }
    return QWidget::eventFilter(watched, ev);
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:498 [@551576e3]
void FrequencyInstrument::beginEdit()
{
    m_edit->setText(QString::number(m_hz / 1e6, 'f', 6));
    m_stack->setCurrentWidget(m_edit);
    m_edit->selectAll();
    m_edit->setFocus();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:506 [@551576e3]
double FrequencyInstrument::parseUserFrequency(const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) { return -1.0; }

    // Detect and strip unit suffix (longest match first so "MHz" wins over "Hz").
    double mult = 0.0;
    bool hasUnit = false;
    const auto tryStripSuffix = [&](const char* suffix, double m) {
        if (s.endsWith(QLatin1String(suffix), Qt::CaseInsensitive)) {
            s.chop(qstrlen(suffix));
            mult = m;
            hasUnit = true;
            return true;
        }
        return false;
    };
    tryStripSuffix("MHz", 1e6)
        || tryStripSuffix("kHz", 1e3)
        || tryStripSuffix("Hz",  1.0)
        || tryStripSuffix("M",   1e6)
        || tryStripSuffix("K",   1e3);
    s = s.trimmed();
    if (s.isEmpty()) { return -1.0; }

    const int nDots   = s.count(QLatin1Char('.'));
    const int nCommas = s.count(QLatin1Char(','));

    // Normalize separators. The goal: end up with at most one '.' as the
    // decimal separator, with any grouping separators removed.
    if (nDots >= 2 && nCommas == 0) {
        // "7.230.000" (or with unit: "7.230.000 Hz") — dots are thousand
        // separators. When no unit was given, default to Hz since that's
        // the only sensible interpretation of a multi-dot number.
        s.remove(QLatin1Char('.'));
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas >= 2 && nDots == 0) {
        // "7,230,000" — US thousand-separated Hz value.
        s.remove(QLatin1Char(','));
        if (!hasUnit) { mult = 1.0; }
    } else if (nDots > 0 && nCommas > 0) {
        // Mixed: the last occurrence is the decimal, the rest are thousands.
        // The presence of thousand separators means the user is writing a
        // Hz value (e.g. "7,230,000.50"); no unit makes no other sense.
        if (s.lastIndexOf(QLatin1Char('.')) > s.lastIndexOf(QLatin1Char(','))) {
            s.remove(QLatin1Char(','));
        } else {
            s.remove(QLatin1Char('.'));
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas == 1 && nDots == 0) {
        // Single comma — ambiguous. If a unit suffix was already parsed, a
        // three-digit tail is a US-style thousands separator
        // (e.g. "7,230 kHz" → 7,230 kHz), anything else is EU decimal
        // ("7,23 MHz" → 7.23 MHz). Without a unit, fall through to EU
        // decimal — the historical behavior — because a bare "7,23" with
        // no grouping context reads as a decimal in every locale that
        // writes it that way.
        const int commaIdx  = s.indexOf(QLatin1Char(','));
        const int tailCount = s.size() - commaIdx - 1;
        if (hasUnit && tailCount == 3) {
            s.remove(QLatin1Char(','));
        } else {
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
    }
    // else: at most a single '.' (C-locale ready) or a plain integer.

    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || v < 0.0) { return -1.0; }

    if (mult != 0.0) {
        return v * mult;
    }

    // From Longpath src/gui/instruments/FrequencyInstrument.cpp:589 [@551576e3]
    // Plain number, no unit, no grouping separators. Matches the Thetis
    // MHz-decimal convention when a decimal point is present. For bare
    // integers, pick the first unit (MHz → kHz → Hz) whose interpretation
    // lies in the Red Pitaya tuning range — this rescues users who typed
    // "7230" (intending kHz) or "7230000" (intending Hz) without guessing
    // wrong like the prior heuristic did (issue #73).
    constexpr double kMinHz = 100000.0;     // 100 kHz floor
    constexpr double kMaxHz = 61440000.0;   // 61.44 MHz ceiling
    const bool isDecimal = (nDots == 1);
    if (isDecimal) {
        return v * 1e6;  // Thetis convention: decimal number is MHz
    }
    const double asMHz = v * 1e6;
    const double asKHz = v * 1e3;
    const double asHz  = v;
    if (asMHz >= kMinHz && asMHz <= kMaxHz) { return asMHz; }
    if (asKHz >= kMinHz && asKHz <= kMaxHz) { return asKHz; }
    if (asHz  >= kMinHz && asHz  <= kMaxHz) { return asHz;  }
    // No interpretation in range: fall back to MHz; caller will clamp.
    return asMHz;
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:605 [@551576e3]
void FrequencyInstrument::commitEdit()
{
    m_stack->setCurrentWidget(m_digitRow);
    // Unlesbares verwerfen und die alte Zahl stehen lassen. Eine
    // Frequenz, die aus einem Tippfehler entsteht, ist schlimmer als
    // eine, die sich nicht geaendert hat.
    //
    // Hier stand bis 2026-08-18 ein blosses toDouble() mit der Annahme
    // MHz. Das haette „7.230.000" still verworfen und „7230" als
    // 7230 MHz gelesen — Fehlerbericht #73 noch einmal.
    const double hz = parseUserFrequency(m_edit->text());
    if (hz > 0.0) {
        emit frequencyEdited(hz);
    }
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:621 [@551576e3]
void FrequencyInstrument::setFrequency(double hz)
{
    if (qFuzzyCompare(1.0 + m_hz, 1.0 + hz)) { return; }
    m_hz = hz;
    refreshDigits();
    refreshVfoRow();
    update();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:630 [@551576e3]
void FrequencyInstrument::setOtherFrequency(double hz)
{
    m_otherHz = hz;
    refreshVfoRow();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:636 [@551576e3]
void FrequencyInstrument::setActiveIsThis(bool active)
{
    m_activeIsThis = active;
    refreshVfoRow();
}

// NereusSDR-original setter; see the header.
void FrequencyInstrument::setSliceLetters(QChar own, QChar other)
{
    m_ownLetter = own;
    m_otherLetter = other;
    refreshVfoRow();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:642 [@551576e3]
void FrequencyInstrument::setVfoRowVisible(bool on)
{
    if (!m_vfoRow) { return; }
    m_vfoRow->setVisible(on);
    // sizeHint() liest m_vfoRow->isVisible() -- ohne updateGeometry()
    // fragt das umgebende Layout (FrequencyApplet) nicht neu nach und
    // der frei gewordene Platz bliebe als Luecke stehen.
    updateGeometry();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:652 [@551576e3]
bool FrequencyInstrument::vfoRowVisible() const
{
    return m_vfoRow && m_vfoRow->isVisible();
}

// NereusSDR: Frequency (Custom). The ported instrument never calls these.

QString FrequencyInstrument::digitBrightColor() const
{
    return m_digitColor.isValid() ? m_digitColor.name(QColor::HexRgb)
                                  : QString::fromLatin1(Style::kAmberText);
}

QString FrequencyInstrument::digitDimColor() const
{
    if (!m_digitColor.isValid()) {
        return QString::fromLatin1(Style::kAmberDim);
    }
    return m_digitColor.darker(165).name(QColor::HexRgb);
}

void FrequencyInstrument::applyEditStyle()
{
    if (!m_edit) {
        return;
    }
    // NereusSDR: LP_MONO_QSS was a string-literal #define; the same
    // families come from Workspace::kMonoQssFamilies. With no operator
    // colour, digitBrightColor() is Style::kAmberText.
    m_edit->setStyleSheet(Style::themed(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; font-family: %5; font-size: %4px; }")
        .arg(Style::kInsetBg, digitBrightColor(), Style::kBorder)
        .arg(Style::kFontReading)
        .arg(QLatin1String(Style::kMonoQssFamilies))));
}

void FrequencyInstrument::setDigitFont(int fontPx, bool fitWidth)
{
    const int clamped = qBound(kMinDigitFontPx, fontPx, 160);
    m_fontLocked = !fitWidth;
    m_fontCeiling = m_fontLocked ? 0 : clamped;
    const int wanted = m_fontLocked ? clamped
                                    : (width() > 28 ? fittingDigitFontPx(width() - 28) : clamped);
    if (wanted == m_digitFontPx && !m_digits.isEmpty()) {
        updateGeometry();
        return;
    }
    m_digitFontPx = wanted;
    buildDigits();
    refreshDigits();
    updateGeometry();
}

void FrequencyInstrument::setDigitColor(const QColor& color)
{
    const QColor next = color.isValid() ? color : QColor();
    if (next == m_digitColor) {
        return;
    }
    m_digitColor = next;
    applyEditStyle();
    if (m_digitLayout) {
        for (int i = 0; i < m_digitLayout->count(); ++i) {
            QLayoutItem* item = m_digitLayout->itemAt(i);
            auto* label = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
            if (label && label->text() == QLatin1String(".")) {
                label->setStyleSheet(Style::themed(
                    QStringLiteral("QLabel { color: %1; }").arg(digitDimColor())));
            }
        }
    }
    refreshDigits();
    refreshVfoRow();
    update();
}

void FrequencyInstrument::setContentCentered(bool on)
{
    if (m_centered == on) {
        return;
    }
    m_centered = on;
    // The leading stretch exists only while centering is on, so the
    // ported layout (no item above the digits) stays as it was.
    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        if (on) {
            root->insertStretch(0, 1);
        } else if (root->count() > 0 && root->itemAt(0)->spacerItem() != nullptr) {
            const std::unique_ptr<QLayoutItem> stretch(root->takeAt(0));
        }
    }
    if (m_vfoRow) {
        m_vfoRow->setAlignment(on ? Qt::AlignHCenter : (Qt::AlignLeft | Qt::AlignVCenter));
    }
    setSizePolicy(QSizePolicy::Preferred,
                  on ? QSizePolicy::Expanding : QSizePolicy::Maximum);
    buildDigits();
    refreshDigits();
    updateGeometry();
}

void FrequencyInstrument::setSliceLinesVisible(bool own, bool other)
{
    m_showOwn = own;
    m_showOther = other;
    refreshVfoRow();
    setVfoRowVisible(own || other);
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:657 [@551576e3]
void FrequencyInstrument::setForm(Form f)
{
    if (m_form == f) { return; }
    m_form = f;
    // Das echte Layout muss mitziehen, nicht nur sizeHint()s Meldung
    // darueber -- siehe die Begruendung am m_stripSpacer-Konstruktor-
    // aufruf.
    if (m_stripSpacer) {
        m_stripSpacer->changeSize(0, stripReserveFor(m_form));
        if (layout()) { layout()->invalidate(); }
    }
    updateGeometry();
    update();
}

// ── Zeichnen ─────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:674 [@551576e3]
double FrequencyInstrument::bandFraction() const
{
    const AmateurBands::Band b = AmateurBands::containing(m_hz);
    if (!b.isValid()) { return -1.0; }
    return (m_hz - b.lowHz) / b.widthHz();
}

// From Longpath src/gui/instruments/FrequencyInstrument.cpp:681 [@551576e3]
void FrequencyInstrument::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor col = Instrument::measured();
    const double f = bandFraction();

    // Der Bereich unter der Zahl. Betreiber 2026-08-30: die Luecke
    // blieb ueber mehrere Anlaeufe hinweg gleich gross, obwohl jeder
    // einzelne Wert hier nachweislich kleiner wurde -- der Verdacht
    // fiel zurecht auf diese Stelle. Ursache: "bot" hing an
    // m_vfoRow->geometry(), und ein VERSTECKTES Widget in einem
    // QBoxLayout bekommt keine verlaesslich aktuelle Position
    // zugewiesen (das Layout ueberspringt seine setGeometry()-Zuteilung
    // fuer leere Eintraege) -- die Flaeche konnte dadurch auf einer
    // veralteten, zu grossen Position sitzen, unabhaengig davon, ob
    // "area.height() < 16.0" je ausloeste. Jetzt kommt "bot" direkt aus
    // derselben Zahl (kStripReserve), die der Konstruktor reserviert --
    // keine zweite Quelle mehr, die auseinanderlaufen kann.
    const int top = m_stack ? m_stack->geometry().bottom() + 3 : 40;
    const int bot = top + stripReserveFor(m_form) - 6;
    const QRectF area(14, top, qMax(0, width() - 28), qMax(0, bot - top));

    if (m_form == Form::NumberOnly) {
        // Nur die Glut, und zwar unter der ZAHL — nicht in der leeren
        // Flaeche darunter, die es in dieser Form gar nicht gibt.
        if (m_stack) {
            QRectF r = m_stack->geometry();
            // NereusSDR: centred on the digits, not on m_stack. The digit
            // row ends in addStretch(1), so the digits sit at the left of
            // a stack that fills the instrument's width; upstream's
            // m_stack centre lay right of the number (under "MHz" at the
            // 360 px preferred width). Longpath shares this. The span is
            // first to last digit; height stays the stack's.
            if (m_stack->currentWidget() == m_digitRow && !m_digits.isEmpty()) {
                const QRect first(m_digits.first()->mapTo(this, QPoint(0, 0)),
                                  m_digits.first()->size());
                const QRect last(m_digits.last()->mapTo(this, QPoint(0, 0)),
                                 m_digits.last()->size());
                const QRect digits = first.united(last);
                r = QRectF(digits.left(), r.top(), digits.width(), r.height());
            }
            const QRectF glow(r.center().x() - r.width() * 0.39,
                              r.center().y() - r.height() * 0.75,
                              r.width() * 0.78, r.height() * 1.5);
            Instrument::paintGlow(p, glow, QPainterPath(), col);
        }
        return;
    }

    if (area.height() < 16.0 || area.width() < 60.0) { return; }

    // Ausserhalb jedes Bandes gibt es keine Stelle im Band, auf die ein
    // Verlauf zeigen koennte. Dann nur die leere Mulde: sie sagt, dass
    // hier eine Bandlage stuende, wenn es eine gaebe.
    const bool inBand = (f >= 0.0);

    if (m_form == Form::BandStrip) {
        LinearSpine spine(QRectF(area.left(), area.top(),
                                 area.width(), kStripHeight));
        Instrument::paintTrough(p, spine);
        if (inBand) {
            Instrument::paintFade(p, spine, f, col);
            Instrument::paintValueEdge(p, spine, f, col);
        }
        // Die Bandkanten. Rot, kraeftig, nicht entsaettigt —
        // CLAUDE.local.md nennt sie ausdruecklich als Ausnahme vom
        // Hausstil.
        if (inBand) {
            const QColor edge(Style::kGaugeDanger);
            p.setPen(QPen(edge, 1.4));
            p.drawLine(spine.crossAt(0.0, 4.0, 4.0));
            p.drawLine(spine.crossAt(1.0, 4.0, 4.0));
        }
        return;
    }

    // Form::FlatArc — derselbe Bogen wie die Instrumente, nur sehr weit
    // und flach. Der Entwurf rechnet mit R = 700 auf 520 Breite; das
    // Verhaeltnis waechst mit.
    const qreal radius = area.width() * (700.0 / 520.0);
    const QPointF pivot(area.center().x(), area.top() + radius);
    ArcSpine spine(pivot, radius, 12.0, 100.6, 79.4);
    spine.setSectorInnerRadius(radius * 0.93);
    Instrument::paintTrough(p, spine);
    if (inBand) {
        Instrument::paintFade(p, spine, f, col);
        Instrument::paintValueEdge(p, spine, f, col);
        const QColor edge(Style::kGaugeDanger);
        p.setPen(QPen(edge, 1.6));
        p.drawLine(spine.crossAt(0.0, 3.0, 3.0));
        p.drawLine(spine.crossAt(1.0, 3.0, 3.0));
    }
}

} // namespace NereusSDR
