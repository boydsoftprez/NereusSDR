// src/gui/instruments/NeedleInstrument.cpp

// --- From src/gui/instruments/NeedleInstrument.h ---
// =================================================================
// src/gui/instruments/NeedleInstrument.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Das Zeigerinstrument ─────────────────────────────────────────────
//
// OE5SOS, 2026-08-17:
//
//   „Zeigt eine oder zwei Anzeigen, Auswahl je Instrument aus derselben
//    Quelle wie die vorhandenen Gauges. Bogen mit Mulde, Sektor bis zum
//    Zeiger, Glut am Drehpunkt, blasser Nachlaufzeiger für die Spitze
//    der letzten Sekunden, feine Zwischenstriche zwischen den
//    beschrifteten. Über der Schwelle wechseln Zeiger, Sektor und Glut
//    gemeinsam auf danger; die Schwelle selbst bleibt eine harte Kante."
//
// Dieses Widget zeichnet fast nichts selbst. Mulde, Verlauf, Glut,
// Wertkante und Teilung kommen aus InstrumentPainter; die Geometrie
// aus ArcSpine; Bereich, Schwelle und Teilung aus ReadingSource. Was
// hier steht, sind Zeiger, Nachlaufzeiger und die Umrechnung von
// Fenstergrösse auf Bogenmasse.
//
// ── Zwei Anzeigen ────────────────────────────────────────────────────
//
// Die zweite bekommt einen Zeiger in measured-dim und SONST nichts:
// kein zweiter Sektor, keine zweite Glut. Zwei auslaufende Sektoren
// übereinander werden matschig, und die Glut hat nur einen Drehpunkt.
// Vorgeschlagen am 2026-08-17, vom Betreiber nicht widersprochen.
//
// ── Die Fusszeile ────────────────────────────────────────────────────
//
// Gleich aufgebaut wie beim Balkeninstrument, „damit der Blick beim
// Wechsel nicht springt": links die Grösse, mittig Spitze und Grenze,
// rechts der Wert. Sie steht in InstrumentFooter, damit die Gleichheit
// nicht von zwei Absichten abhängt.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/instruments/NeedleInstrument.cpp ---
// =================================================================
// src/gui/instruments/NeedleInstrument.cpp  (Longpath)
// =================================================================
// Siehe NeedleInstrument.h. Fast alles Sichtbare kommt aus
// InstrumentPainter; hier stehen Zeiger, Nachlaufzeiger und die
// Umrechnung von Fenstergrösse auf Bogenmasse.
// =================================================================

// =================================================================
// src/gui/instruments/NeedleInstrument.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/NeedleInstrument.h
//   src/gui/instruments/NeedleInstrument.cpp
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
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Adds clearSecondValue().
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/instruments/NeedleInstrument.h"
#include <cmath>
#include <QtMath>

#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/instruments/ReadingSource.h"

#include <QPainter>
#include <QVBoxLayout>
#include "gui/instruments/InstrumentStyle.h"

namespace NereusSDR {

// NereusSDR: the Longpath values, see gui/instruments/InstrumentStyle.h.
namespace Style = ::NereusSDR::InstrumentStyle;

namespace {

// From Longpath src/gui/instruments/NeedleInstrument.cpp:37 [@551576e3]
// NereusSDR: three of these are unused upstream as well; [[maybe_unused]]
// keeps them, exactly, without a compiler warning.
// ── Bogenmasse ───────────────────────────────────────────────────────
//
// Der Entwurf zeichnet in ein 520 × 190 grosses Feld mit Drehpunkt
// (260, 168), Radius 148 und Rillenbreite 13 — also Drehpunkt knapp
// über der Unterkante, Radius etwa 0,285 der Breite, Rille 0,088 des
// Radius.
//
// Hier stehen nur noch die drei Masse des ZIFFERBLATTS. Alles, was
// darauf gezeichnet wird — Zeigerbreite, Nabe, Teilstriche, Abstände —
// hängt an Spine::unit() und wächst dort mit; siehe InstrumentSpine.h,
// Abschnitt „Der Massstab, in dem die Entwürfe notiert sind".
[[maybe_unused]] constexpr double kRadiusOfWidth   = 148.0 / 520.0;
constexpr double kTroughOfRadius  =  13.0 / 148.0;
[[maybe_unused]] constexpr double kPivotFromBottom =  22.0 / 190.0;   // 190 - 168
// From Longpath src/gui/instruments/NeedleInstrument.cpp:45 [@551576e3]
// Derselbe Abstand, aber am RADIUS gemessen statt an der Feldhoehe.
// Der Entwurf ist 520 x 190 mit Radius 148 und 22 px unter dem
// Drehpunkt — also 22/148. Siehe die Notiz in paintEvent: an der
// Feldhoehe gemessen wandert der Bogen in einem hohen Feld nach oben
// und laesst darunter Luft stehen.
constexpr double kPivotOfRadius   =  22.0 / 148.0;
// From Longpath src/gui/instruments/NeedleInstrument.cpp:50 [@551576e3]
// Radius im Verhaeltnis zur Hoehe UEBER dem Drehpunkt. Im Entwurf
// liegt der Drehpunkt 168 px unter der Oberkante und der Radius ist
// 148 — der Bogen laesst also 20 px Luft. Dieses Verhaeltnis begrenzt
// den Radius, wenn das Feld breiter als hoch wird (2026-08-20).
[[maybe_unused]] constexpr double kRadiusOfPivotHeight = 148.0 / 168.0;

// From Longpath src/gui/instruments/NeedleInstrument.cpp:53 [@551576e3]
/// Der Sweep aus dem Entwurf: 168 Grad links, 12 Grad rechts.
constexpr double kDeg0 = 168.0;
constexpr double kDeg1 =  12.0;

} // namespace

// From Longpath src/gui/instruments/NeedleInstrument.cpp:58 [@551576e3]
NeedleInstrument::NeedleInstrument(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 8, 14, 10);
    lay->setSpacing(0);
    lay->addStretch(1);                 // die Fläche, in die gemalt wird
    m_footer = new InstrumentFooter(this);
    lay->addWidget(m_footer, 0);
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:69 [@551576e3]
bool NeedleInstrument::setPrimary(int bindingId)
{
    const ReadingDescriptor* d = readingFor(bindingId);
    // Ohne belegte Skala kein Instrument. Lieber eine abgelehnte
    // Auswahl als ein Zifferblatt, dessen Bereich niemand verantwortet.
    if (!d || !d->hasScale) { return false; }
    m_primary = bindingId;
    m_ownReading.reset();
    // Kein Startwert. Ein Instrument, dem man die Grösse zuweist, hat
    // deswegen noch nichts gemessen.
    clearValue();
    return true;
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:82 [@551576e3]
bool NeedleInstrument::setSecondary(int bindingId)
{
    if (bindingId < 0) { m_secondary = -1; m_hasSecond = false; update(); return true; }
    const ReadingDescriptor* d = readingFor(bindingId);
    if (!d || !d->hasScale) { return false; }
    m_secondary = bindingId;
    m_hasSecond = false;
    update();
    return true;
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:93 [@551576e3]
void NeedleInstrument::clearValue()
{
    m_hasValue = false;
    m_hasSecond = false;
    m_peak.forget();
    refreshFooter();
    update();
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:102 [@551576e3]
void NeedleInstrument::setOffline(bool offline)
{
    if (m_offline == offline) { return; }
    m_offline = offline;
    update();
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:109 [@551576e3]
void NeedleInstrument::setShowFrequency(bool on)
{
    if (m_showFrequency == on) { return; }
    m_showFrequency = on;
    update();
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:116 [@551576e3]
void NeedleInstrument::setFrequencyHz(double hz)
{
    // Betreiber 2026-09-02: "geht anscheinend nur, wenn es mit
    // funkgeraet verbunden ist... eigentlich sollte 0.0000 da stehen!"
    // Kein hasValue-Torwaechter wie beim Messwert selbst -- die
    // Frequenzanzeige des Programms zeigt ohne Funkgeraet ebenfalls
    // "0.000.000" statt nichts (siehe Frequenz-Panel), also gilt hier
    // dieselbe Regel: jeder gesetzte Wert wird gezeigt, auch 0.
    m_frequencyHz = hz;
    if (m_showFrequency) { update(); }
}

// NereusSDR: see the header.
void NeedleInstrument::clearSecondValue()
{
    m_hasSecond = false;
    update();
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:128 [@551576e3]
void NeedleInstrument::onReading(int bindingId, double value)
{
    if (bindingId == m_primary) {
        m_value = value;
        m_hasValue = true;
        m_peak.note(value);
        refreshFooter();
        update();
    } else if (bindingId == m_secondary) {
        m_secondValue = value;
        m_hasSecond = true;
        update();
    }
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:143 [@551576e3]
const ReadingDescriptor* NeedleInstrument::primaryReading() const
{
    return m_ownReading ? &*m_ownReading : readingFor(m_primary);
}

void NeedleInstrument::setPrimaryScale(const ReadingDescriptor& d)
{
    m_ownReading = d;
    refreshFooter();
    update();
}

void NeedleInstrument::refreshFooter()
{
    const ReadingDescriptor* d = primaryReading();
    if (!d || !m_footer) { return; }
    m_footer->setCaption(d->thetisName());

    if (!m_hasValue) {
        // Gedankenstrich statt Zahl, und in der matten Farbe: ein
        // Strich ist keine Messung und soll auch nicht wie eine
        // aussehen. Die GRENZE bleibt stehen — sie ist eine
        // Eigenschaft der Skala und stimmt auch ohne Messung. Wenn du
        // sie auch weg willst, ist es eine Zeile.
        m_footer->setValueText(QStringLiteral("–"));
        m_footer->setValueColour(Instrument::measuredDim());
        m_footer->setPeakAndLimit(
            QStringLiteral("–"),
            d->threshold.has_value() ? d->text(d->threshold.value()) : QString());
        return;
    }

    m_footer->setValueText(d->textWithUnit(m_value));
    m_footer->setValueColour(Instrument::valueColour(*d, m_value));
    m_footer->setPeakAndLimit(
        m_peak.enabled() ? d->text(m_peak.value()) : QStringLiteral("–"),
        d->threshold.has_value() ? d->text(d->threshold.value()) : QString());
}

// ── Die Lampe hinter der Scheibe ─────────────────────────────────────
//
// Ein breiter, sehr schwacher Verlauf vom Drehpunkt nach oben, in der
// Messfarbe. Er beleuchtet die GANZE Scheibe, nicht nur den gefuellten
// Teil — das ist der Unterschied zwischen „der Wert leuchtet" und „das
// Instrument ist beleuchtet", und gebeten wurde um das zweite
// (2026-08-20).
//
// In der Messfarbe und nicht in Weiss: eine weisse Lampe hinter einem
// bernsteinfarbenen Zeiger macht ihn blass. Nur der obere Halbkreis —
// unter dem Drehpunkt ist keine Scheibe, die man beleuchten koennte.
void NeedleInstrument::paintBacklight(QPainter& p, const QPointF& pivot,
                                      qreal radius, const QColor& c,
                                      double strength)
{
    QRadialGradient lamp(pivot, radius * 1.15);
    QColor warm = c;
    warm.setAlphaF(0.13 * strength);
    lamp.setColorAt(0.0, warm);
    warm.setAlphaF(0.05 * strength);
    lamp.setColorAt(0.55, warm);
    warm.setAlphaF(0.0);
    lamp.setColorAt(1.0, warm);
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(lamp);
    p.drawPie(QRectF(pivot.x() - radius * 1.15, pivot.y() - radius * 1.15,
                     radius * 2.3, radius * 2.3),
              0, 180 * 16);
    p.restore();
}

// ── Frequenz zusaetzlich einblenden ──────────────────────────────────
//
// Betreiber 2026-09-02, nach drei Entwurfsrunden: kein Kasten, keine
// Umrandung, kein eigenes Feld -- die Ziffern stehen frei auf dem
// Zifferblatt, unter dem Bogenscheitel, wie eine Beschriftung im
// Diagramm selbst statt ein aufgesetztes UI-Element. "14.225.000" mit
// Punkten als Tausendertrennzeichen ist dieselbe Schreibweise, die die
// Ziffernanzeige selbst als Eingabe akzeptiert (FrequencyInstrument.cpp).
void NeedleInstrument::paintFrequencyOverlay(QPainter& p, const QPointF& pivot,
                                             qreal radius) const
{
    if (!m_showFrequency) { return; }

    qint64 hzRounded = qRound64(m_frequencyHz);
    QString digits = QString::number(hzRounded);
    for (int pos = digits.length() - 3; pos > 0; pos -= 3) {
        digits.insert(pos, QLatin1Char('.'));
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    QFont digitFont = Style::monoFont(p.font(), qMax(11, qRound(radius * 0.15)),
                                      QFont::DemiBold);
    const QFontMetrics fm(digitFont);
    const int digitsW = fm.horizontalAdvance(digits);

    // Unter dem Bogenscheitel, ueber dem Drehpunkt -- derselbe Platz,
    // den die drei Entwurfsblaetter gezeigt haben.
    //
    // Betreiber 2026-09-03: "bitte loesche dort das MHZ. braucht Platz
    // und wird nicht benoetigt" -- die Einheit ist bei einer achtstelligen
    // Hz-Anzeige (die Millionenstelle allein sagt schon "das ist MHz")
    // selbsterklaerend, die Ziffern jetzt allein zentriert statt neben
    // einer zweiten, kleineren Schrift.
    const QPointF centre(pivot.x(), pivot.y() - radius * 0.32);
    const qreal x = centre.x() - digitsW / 2.0;

    p.setFont(digitFont);
    p.setPen(QColor(Style::kAmberText));
    p.drawText(QPointF(x, centre.y() + fm.ascent() / 2.0), digits);

    p.restore();
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:247 [@551576e3]
void NeedleInstrument::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    paintInto(p, size(), false);
}

// From Longpath src/gui/instruments/NeedleInstrument.cpp:266 [@551576e3]
// ── Malen fuer die Einblendung, ohne das Widget anzufassen ──────────
//
// renderTransparent hat frueher das LEBENDE Instrument auf die
// gewuenschte Groesse umgestellt und danach zurueck — mit dem
// Kommentar, die Spalte merke davon nichts. Sie merkt es sehr wohl:
// ein resize() auf ein Widget in einem Layout ist ein Eingriff, den
// das Layout beantwortet, und die Einblendung malt alle 500 ms neu.
// Beim Rotor hat genau das die Rose plattgedrueckt (2026-08-21,
// Bildschirmfoto: „den rotor bitte links wieder einblenden").
//
// Hier stand derselbe Fehler, nur noch unentdeckt. paintInto() malt
// fuer eine verlangte Groesse in einen fremden Maler; das Widget
// bleibt, wie es ist.
void NeedleInstrument::paintInto(QPainter& painter, QSize forSize, bool bare)
{
    const ReadingDescriptor* d = primaryReading();
    if (!d || !d->hasScale) { return; }

    // Ohne Fusszeile, wenn nur das Gesicht verlangt ist.
    const int footerH = (bare || !m_footer) ? 0 : m_footer->height();
    const QRectF face(0, 0, forSize.width(),
                      qMax(0, forSize.height() - footerH));
    if (face.width() < 40.0 || face.height() < 30.0) { return; }

    // ── Der Bogen muss in BEIDE Richtungen passen ────────────────────
    //
    // Hier stand nur `face.width() * kRadiusOfWidth`. Solange das
    // Instrument die Breite der Applet-Spalte hatte (260 px), ging das
    // gut. Zieht man die Spalte breit — der Betreiber hatte am
    // 2026-08-20 gut 700 px — waechst der Radius mit, die Hoehe aber
    // nicht, und der Zeiger laeuft oben aus dem Feld heraus. Genau das
    // war auf seinem Bild zu sehen: „RX1 laesst sich zwar verkleinern,
    // der inhalt aendert sich aber nicht im massstab".
    //
    // Der Entwurf ist 520 x 190 mit Radius 148 und Drehpunkt 22 ueber
    // dem unteren Rand; senkrecht bleiben damit 168 - 148 = 20 px Luft.
    // Beide Verhaeltnisse werden gerechnet und das KLEINERE genommen —
    // so behaelt der Bogen seine Form und passt in jedes Seiten-
    // verhaeltnis, statt in der einen Richtung zu stimmen und in der
    // anderen hinauszulaufen.
    // ── Und er soll die Flaeche AUSFUELLEN ───────────────────────────
    //
    // Der Betreiber, 2026-08-20: „auch der swr zeiger sollte
    // flaechenfuellender sein."
    //
    // Der Drehpunkt hing an der HOEHE des Feldes
    // (face.height() * 22/190). In einem hohen Feld rutscht er damit
    // weit nach oben, der Bogen sitzt in der oberen Haelfte, und
    // darunter bleibt Luft, die niemand braucht. Auf seinem Bild war
    // das Feld rund 460 x 290; der Abstand unter dem Drehpunkt betrug
    // damit 34 px statt der 22 des Entwurfs.
    //
    // Jetzt haengt der Abstand am RADIUS: der Bogen behaelt sein
    // Verhaeltnis und sitzt immer gleich tief ueber dem unteren Rand,
    // egal wie hoch das Feld ist. Weil der Radius selbst aus der Hoehe
    // kommt, muss beides gemeinsam geloest werden — daher erst der
    // Radius aus der vollen Hoehe, dann der Drehpunkt daraus.
    // ── Aus der ECHTEN Bogenbreite, nicht aus einem Verhaeltnis ──────
    //
    // Hier stand `face.width() * 148/520` — die Zahlen des Entwurfs.
    // Der Bogen laeuft von 168° bis 12°, ist also symmetrisch um die
    // Senkrechte und 2·sin(78°) = 1,956 Radien BREIT. Mit dem festen
    // Verhaeltnis nahm er 55 % der Flaechenbreite ein und liess links
    // und rechts je gut 20 % leer.
    //
    // Der Betreiber, 2026-08-20, nach dem ersten Anlauf: „s meter und
    // swr noch immer nicht formatfuellend." Er hatte recht — ich hatte
    // nur den Drehpunkt nach unten geholt, die Breite aber weiter aus
    // dem Entwurfsverhaeltnis genommen.
    //
    // Jetzt umgekehrt gerechnet: aus der Flaeche wird der groesste
    // Radius bestimmt, dessen Bogen samt Mulde noch hineinpasst —
    // waagerecht UND senkrecht, das Kleinere gewinnt. Damit fuellt der
    // Zeiger jedes Format, statt in einem zu stimmen.
    const qreal halfSpanRad = qDegreesToRadians((kDeg0 - kDeg1) / 2.0);
    const qreal arcWidthPerRadius = 2.0 * std::sin(halfSpanRad);
    const qreal pad = 6.0;

    // Waagerecht: Bogenbreite + halbe Mulde auf jeder Seite.
    const qreal radiusByWidth =
        (face.width() - 2.0 * pad)
        / (arcWidthPerRadius + kTroughOfRadius);

    // Senkrecht: vom Drehpunkt bis zum Scheitel ein Radius, plus halbe
    // Mulde, plus der Abstand unter dem Drehpunkt.
    const qreal radiusByHeight =
        (face.height() - pad)
        / (1.0 + kTroughOfRadius / 2.0 + kPivotOfRadius);

    const qreal radius = qMin(radiusByWidth, radiusByHeight);
    const QPointF pivot(face.center().x(),
                        face.bottom() - radius * kPivotOfRadius);
    const qreal trough = radius * kTroughOfRadius;

    ArcSpine spine(pivot, radius, trough, kDeg0, kDeg1);

    QPainter& p = painter;
    p.setRenderHint(QPainter::Antialiasing, true);

    const double f = d->fraction(m_value);
    const QColor col = Instrument::valueColour(*d, m_value);
    const double thresholdF = d->threshold.has_value()
                                  ? d->fraction(d->threshold.value())
                                  : -1.0;

    // ── Ohne Messung nur das Zifferblatt ─────────────────────────────
    //
    // Mulde und Teilung ja — sie sagen, WAS hier gemessen würde. Glut,
    // Sektor, Zeiger, Nachlaufzeiger und Wertkante nein: jedes davon
    // behauptet eine Zahl.
    if (!m_hasValue) {
        // ── Auch OHNE Messwert beleuchtet ────────────────────────────
        //
        // Der Betreiber, 2026-08-20: „stehwelle noch dunkel."
        //
        // Er hatte recht, und der Grund stand hier: dieser Zweig kehrt
        // zurueck, BEVOR die Hinterleuchtung drankam. Das S-Meter hatte
        // einen Wert und leuchtete, die Stehwelle ohne Verbindung
        // keinen — und blieb schwarz.
        //
        // Ein beleuchtetes Instrument ist beleuchtet, ob es gerade
        // etwas anzeigt oder nicht. Genau das ist der Unterschied
        // zwischen einer Lampe hinter der Scheibe und einem
        // leuchtenden Messwert. Matter als mit Wert, damit die
        // Ruhelage nicht wie eine Messung aussieht.
        // Die volle Messfarbe, nur etwas gedaempft.
        //
        // Erst stand hier measuredDim() mit 0,7 — gedaempfte Farbe MAL
        // gedaempfter Staerke, davon blieb nichts uebrig, und die
        // Stehwelle war weiter schwarz. Eine Lampe ist eine Lampe; dass
        // gerade nichts gemessen wird, sagt der fehlende Zeiger, nicht
        // die Dunkelheit.
        paintBacklight(p, pivot, radius, Instrument::measured(), 0.85);

        Instrument::paintTrough(p, spine, thresholdF);
        Instrument::paintTicks(p, spine, *d);
        // Ohne Verbindung ruht der Zeiger am Anschlag — matt, damit die
        // Ruhelage nicht wie ein gemessener Kleinstwert aussieht. Mit
        // Verbindung, aber ohne Messwert bleibt er weg: dort WAERE eine
        // Messung moeglich, und ein Zeiger behauptete sie.
        if (m_offline) {
            p.save();
            p.setOpacity(0.30);
            Instrument::paintNeedle(p, spine, 0.0,
                                    QColor(Style::kTextScale));
            p.restore();
        }
        // Die Nabe bleibt: sie ist der Drehpunkt des Instruments, kein
        // Messwert. Ein Zifferblatt ohne Nabe sähe unfertig aus statt
        // ruhig.
        Instrument::paintHub(p, spine);
        paintFrequencyOverlay(p, pivot, radius);
        return;
    }

    // ── Hinterleuchtung ──────────────────────────────────────────────
    //
    // Siehe paintBacklight: eine Lampe hinter der Scheibe, in der
    // Messfarbe. Mit Messwert voll, ohne Messwert matter (der Aufruf
    // im Zweig darueber).
    paintBacklight(p, pivot, radius, col, 1.0);

    // Reihenfolge wie im Entwurf: Glut ganz hinten, dann die Mulde,
    // dann der Sektor, dann Teilung, Zeiger und Kante.
    Instrument::paintGlow(p, spine, col, 0.30);
    Instrument::paintTrough(p, spine, thresholdF);
    Instrument::paintFade(p, spine, f, col);
    Instrument::paintTicks(p, spine, *d);

    // Nachlaufzeiger: ein kurzer Strich am äusseren Rand, matt.
    if (m_peak.enabled() && m_peak.value() > m_value) {
        Instrument::paintPeakNeedle(p, spine, d->fraction(m_peak.value()),
                                    Instrument::measuredDim());
    }

    // Die zweite Anzeige — nur ein Zeiger, kein zweiter Sektor und
    // keine zweite Glut. Zwei auslaufende Sektoren übereinander werden
    // matschig, und die Glut hat nur einen Drehpunkt. Auch keine helle
    // Kante: sie sagt „HIER ist der Wert", und das gilt für einen.
    if (const ReadingDescriptor* d2 = m_hasSecond ? readingFor(m_secondary)
                                                  : nullptr) {
        if (d2->hasScale) {
            Instrument::paintNeedle(p, spine, d2->fraction(m_secondValue),
                                    Instrument::measuredDim(),
                                    Instrument::NeedleWeight::Secondary,
                                    /*withEdge=*/false);
        }
    }

    // Der Zeiger, und die helle Kante auf seiner vorderen Flanke.
    Instrument::paintNeedle(p, spine, f, col);

    // Die Nabe deckt den Zeigerfuss ab.
    Instrument::paintHub(p, spine);
    paintFrequencyOverlay(p, pivot, radius);
}

} // namespace NereusSDR
