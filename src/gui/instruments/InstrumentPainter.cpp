// src/gui/instruments/InstrumentPainter.cpp

// --- From src/gui/instruments/InstrumentPainter.h ---
// =================================================================
// src/gui/instruments/InstrumentPainter.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die drei Mittel, an einer Stelle ─────────────────────────────────
//
// OE5SOS, 2026-08-17, für alle drei neuen Widgets:
//
//   „Mulde mit Innenschatten an der oberen Kante (Grund kInsetBg,
//    darüber eine dunkle Kante mit etwa 75 % Deckung).
//    Auslaufender Verlauf bis zum aktuellen Wert: innen null, aussen
//    etwa ein Viertel Deckkraft in Messwertfarbe.
//    Glut bei 16 %, die über gut die halbe Höhe reicht und dort
//    verschwindet.
//    Eine schmale hellere Kante am aktuellen Wert, damit das Auge ihn
//    findet, bevor es den Zeiger sucht."
//
// Diese Datei ist die eine Stelle. Jedes der vier Mittel steht hier
// genau einmal und arbeitet über Spine, also für Bogen und Gerade
// gleichermassen. Wer ein fünftes Instrument baut, ruft hierher und
// schreibt nichts nach.
//
// Die Deckungen sind KEINE freien Zahlen. Sie stammen aus den
// Entwürfen des Betreibers und stehen als benannte Konstanten in der
// .cpp, mit der Zeile, aus der sie kommen.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/instruments/InstrumentPainter.cpp ---
// =================================================================
// src/gui/instruments/InstrumentPainter.cpp  (Longpath)
// =================================================================
// Siehe InstrumentPainter.h — die drei Mittel, an einer Stelle.
// =================================================================

// =================================================================
// src/gui/instruments/InstrumentPainter.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/InstrumentPainter.h
//   src/gui/instruments/InstrumentPainter.cpp
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
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/instruments/InstrumentPainter.h"

#include "gui/instruments/InstrumentStyle.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/instruments/ReadingSource.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRectF>

namespace NereusSDR {

// NereusSDR: the Longpath values, see gui/instruments/InstrumentStyle.h.
namespace Style = ::NereusSDR::InstrumentStyle;
namespace Instrument {

namespace {

// ── Die Zahlen aus den Entwürfen ─────────────────────────────────────
//
// ~/Downloads/zeiger-verfeinert.html, sofern nicht anders vermerkt.
// Keine davon ist frei gewählt; wer eine ändert, ändert die
// Handschrift aller drei Widgets auf einmal — und genau dafür stehen
// sie hier zusammen und nicht verstreut.

// From Longpath src/gui/instruments/InstrumentPainter.cpp:33 [@551576e3]
/// Die dunkle Kante über der Mulde. Entwurf: opacity ".75".
constexpr double kShadowOpacity = 0.75;

/// Der rote Abschnitt oberhalb der Schwelle. Entwurf: opacity ".42".
// kThresholdOpacity ist mit dem eingefaerbten Schwellenbereich
// entfallen (2026-08-23). Nicht auskommentiert stehengelassen:
// ein unbenutzter Wert mit gutem Namen wird beim naechsten Mal
// wieder verdrahtet, ohne dass jemand die Begruendung liest.

// From Longpath src/gui/instruments/InstrumentPainter.cpp:42 [@551576e3]
/// Die helle Kante am Wert. Entwurf: stroke-width 1.4, opacity ".75".
constexpr double kValueEdgeOpacity = 0.75;
constexpr double kValueEdgeOfUnit  = 1.4 / kDesignUnit;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:47 [@551576e3]
/// Wieviel heller die Kante auf der Zeigerflanke ist. Eine Stufe, nicht
/// zwei: sie soll den Blick führen, nicht auffallen.
constexpr int kValueEdgeLighter = 135;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:51 [@551576e3]
/// Wo die Flankenkante am Zeiger beginnt. Entwurf: r = 24, also weiter
/// aussen als der Zeigerfuss (r = 10) — sie läuft nicht in die Nabe.
constexpr double kValueEdgeStartOfUnit = 24.0 / kDesignUnit;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:58 [@551576e3]
// ── Der Zeiger ───────────────────────────────────────────────────────
//
// Entwurf: line von r = 10 bis R − W/2 − 3, stroke-width 2.2, runde
// Kappe, keine Verjüngung und kein Schatten. Nabe: Kreis r = 5, Füllung
// #141417, 1 px Rand in measured-dim.
constexpr double kNeedleWidthOfUnit    =  2.2 / kDesignUnit;
constexpr double kNeedleInnerOfUnit    = 10.0 / kDesignUnit;
constexpr double kNeedleTipGapOfUnit   =  3.0 / kDesignUnit;
constexpr double kHubRadiusOfUnit      =  5.0 / kDesignUnit;
constexpr double kHubStrokeOfUnit      =  1.0 / kDesignUnit;

/// Füllung der Nabe. Entwurf: #141417. Kein Rollenname: das ist kein
/// Flächengrund, sondern das Loch, das der Zeigerfuss abdeckt.
inline QColor hubFill() { return QColor(Style::kTitleGradBot); }

// From Longpath src/gui/instruments/InstrumentPainter.cpp:69 [@551576e3]
/// Der Nachlaufzeiger: kurzer Strich am äusseren Rand, matt.
constexpr double kPeakLengthOfUnit = 13.0 / kDesignUnit;   // 16 − 3
constexpr double kPeakWidthOfUnit  =  2.0 / kDesignUnit;
constexpr double kPeakOpacity      =  0.6;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:74 [@551576e3]
/// Der zweite Zeiger. Entwurf: dünner als der erste.
constexpr double kSecondWidthOfUnit = 1.8 / kDesignUnit;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:78 [@551576e3]
/// Glut: 16 % in der Mitte, 5 % bei etwas über der halben Strecke,
/// null am Rand.
constexpr double kGlowMidStop     = 0.55;
constexpr double kGlowMidFraction = 0.05 / 0.16;   // relativ zur Mitte

// From Longpath src/gui/instruments/InstrumentPainter.cpp:82 [@551576e3]
/// Teilung. Entwurf: fein 1 px bei 55 % Deckung, beschriftet 1,2 px.
constexpr double kMinorTickLenOfUnit   =  4.0 / kDesignUnit;
constexpr double kMinorTickWidthOfUnit =  1.0 / kDesignUnit;
constexpr double kMinorTickOpacity     =  0.55;
constexpr double kMajorTickLenOfUnit   =  8.0 / kDesignUnit;
constexpr double kMajorTickWidthOfUnit =  1.2 / kDesignUnit;
constexpr double kLabelOffsetOfUnit    = 22.0 / kDesignUnit;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:90 [@551576e3]
/// Die Aussenlinie der Mulde. Entwurf 1 px.
constexpr double kOutlineOfUnit = 1.0 / kDesignUnit;

// From Longpath src/gui/instruments/InstrumentPainter.cpp:95 [@551576e3]
/// Untergrenze für jede Strichstärke. Ein Strich, der beim Verkleinern
/// unter etwa einen Punkt fällt, wird von der Kantenglättung
/// weggemittelt — dann verschwindet die Teilung, statt fein zu werden.
constexpr double kMinPenWidth = 0.7;

/// Ein Mass des Entwurfs in Pixel dieser Geometrie.
inline qreal at(qreal ratio, const Spine& s) { return ratio * s.unit(); }
// From Longpath src/gui/instruments/InstrumentPainter.cpp:100 [@551576e3]
/// Dasselbe für Strichstärken, mit der Untergrenze.
inline qreal pen(qreal ratio, const Spine& s)
{
    return qMax(kMinPenWidth, ratio * s.unit());
}

// From Longpath src/gui/instruments/InstrumentPainter.cpp:107 [@551576e3]
/// Rückfallwert für „measured-dim". StyleConstants.h führt ihn nicht;
/// HGauge.cpp:13 hält denselben Wert dateilokal.
constexpr auto kMeasuredDimFallback = Style::kAmberDim;

} // namespace

QColor measured()    { return QColor(Style::role("measured", Style::kAmberText)); }
QColor measuredDim() { return QColor(Style::role("measured-dim", kMeasuredDimFallback)); }
QColor danger()      { return QColor(Style::role("danger", Style::kGaugeDanger)); }

// ── Mulde ────────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/InstrumentPainter.cpp:117 [@551576e3]
void paintTrough(QPainter& p, const Spine& s, double thresholdFraction)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);

    // Grund
    // ── Die Mulde eine Spur heller ───────────────────────────────────
    //
    // „inset-bg" ist die dunkelste Flaeche der Palette — richtig fuer
    // ein Eingabefeld, das sich vom Panel absetzen soll. Auf einem
    // Zifferblatt ist dieselbe Flaeche aber die SKALA, auf der man
    // etwas ablesen will. Der Betreiber am 2026-08-20: „stehwelle noch
    // dunkel."
    //
    // Eine Stufe hoeher: „button" ist die naechste Flaeche der Leiter
    // und hebt den Ring vom Panel ab, ohne ihn zur Flaeche zu machen.
    p.setPen(QPen(QColor(Style::role("button", Style::kButtonBg)),
                  s.troughWidth(), Qt::SolidLine, Qt::FlatCap));
    p.drawPath(s.troughPath());

    // ── Der Bereich ab der Schwelle ist WEG ─────────────────────────
    //
    // Er hat drei Fassungen gebraucht, bis klar war, dass er gar nicht
    // hingehoert. Der Betreiber am 2026-08-23, der Reihe nach:
    //   "die grafik ganz rechts, diese pille gefällt mir nicht, bitte
    //    weglöschen. farbe in weinrot oder so. den bereich ab 2,5"
    //   "bei beiden grafiken"
    //   "bereich bis 2,5 passt, ab 2,5 gefällt nicht. bis 95 watt passt
    //    gut, danach auch wieder nicht. dieses jeweilige ende bitte
    //    immer weg"
    //
    // Erst war es eine Pille (ein Zeichenfehler: der Abschnitt wurde
    // als Pfad mit muldenbreiter Feder GESTRICHEN statt gefuellt),
    // dann eine saubere weinrote Flaeche — und auch die stoert. Zu
    // Recht: die Flaeche steht STAENDIG im Bild, auch wenn alles in
    // Ordnung ist, und faerbt damit dauerhaft ein Viertel der Anzeige
    // ein, ohne je etwas zu melden.
    //
    // Was warnt, ist der ZEIGER, wenn er dort ankommt — und die duenne
    // Schwellenmarke, die weiter unten gezeichnet wird und stehen
    // bleibt. Die sagt dasselbe mit einem Strich statt mit einer
    // Flaeche.
    //
    // thresholdFraction bleibt im Vertrag: paintTroughTube braucht ihn
    // weiterhin nicht, aber die Aufrufer sollen nicht umgebaut werden,
    // falls jemand die Zone je wieder will.
    Q_UNUSED(thresholdFraction);

    // Innenschatten an der oberen bzw. äusseren Kante
    QColor shadow(0, 0, 0);
    shadow.setAlphaF(kShadowOpacity);
    p.setPen(QPen(shadow, s.shadowWidth(), Qt::SolidLine, Qt::FlatCap));
    p.drawPath(s.shadowPath());

    // Aussenlinie, damit die leere Mulde nicht im Panelgrund verschwindet
    // — derselbe Befund wie bei HGauge (HGauge.cpp:60-63).
    p.setPen(QPen(QColor(Style::role("border", Style::kBorder)),
                  pen(kOutlineOfUnit, s)));
    p.drawPath(s.troughPath());

    p.restore();
}

// From Longpath src/gui/instruments/InstrumentPainter.cpp:188 [@551576e3]
// ── Die Mulde als Roehre ─────────────────────────────────────────────
//
// Begruendung in der Kopfdatei. Zur Umsetzung nur so viel: der Verlauf
// laeuft QUER zur Mulde, nicht laengs. Er hat vier Stuetzstellen statt
// zwei, weil eine Roehre oben schneller aufhellt als sie unten
// abdunkelt — mit einem geraden Verlauf sieht sie aus wie eine schiefe
// Ebene.
void paintTroughTube(QPainter& p, const Spine& s, double thresholdFraction)
{
    const auto* lin = dynamic_cast<const LinearSpine*>(&s);
    if (!lin) { paintTrough(p, s, thresholdFraction); return; }

    const QRectF r = lin->troughRect();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor base(Style::role("button", Style::kButtonBg));
    QLinearGradient g(r.left(), r.top(), r.left(), r.bottom());
    g.setColorAt(0.00, base.darker(165));
    g.setColorAt(0.40, base.lighter(126));
    g.setColorAt(0.60, base.lighter(112));
    g.setColorAt(1.00, base.darker(180));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRect(r);

    // Der eingefaerbte Abschnitt ab der Schwelle entfaellt auch hier —
    // siehe die Begruendung in paintTrough. Eine Roehre, deren letztes
    // Viertel staendig rot ist, meldet nichts und stoert dauernd.
    Q_UNUSED(thresholdFraction);

    // Die harte Lichtkante ganz oben und der Schatten ganz unten — sie
    // schliessen die Woelbung ab. Ohne sie franst die Roehre in den
    // Panelgrund aus.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 26), 1.0));
    p.drawLine(r.topLeft() + QPointF(0.5, 0.5), r.topRight() + QPointF(-0.5, 0.5));
    p.setPen(QPen(QColor(0, 0, 0, 170), 1.0));
    p.drawLine(r.bottomLeft() + QPointF(0.5, -0.5),
               r.bottomRight() + QPointF(-0.5, -0.5));

    p.setPen(QPen(QColor(Style::role("border", Style::kBorder)),
                  pen(kOutlineOfUnit, s)));
    p.drawRect(r);

    p.restore();
}

// ── Verlauf ──────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/InstrumentPainter.cpp:231 [@551576e3]
void paintFade(QPainter& p, const Spine& s, double f, const QColor& c)
{
    if (f <= 0.0) { return; }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(s.fadeBrush(c));
    p.drawPath(s.fillArea(f));
    p.restore();
}

void paintSegments(QPainter& p, const Spine& s, double f, const QColor& c,
                   int count, double gapPx)
{
    if (f <= 0.0 || count < 2) { return; }

    // Nur auf einer Geraden: auf einem Bogen waeren gleich breite
    // Segmente ungleich lange Boegen, und die Kette wuerde zur Spitze
    // hin dichter. Wer Segmente auf einem Zeigerwerk will, braucht eine
    // eigene Rechnung — bis dahin ist Nichtstun ehrlicher als etwas
    // Schiefes.
    const auto* lin = dynamic_cast<const LinearSpine*>(&s);
    if (!lin) { paintFade(p, s, f, c); return; }

    const QRectF r = lin->troughRect();
    if (r.width() <= 0.0) { return; }

    const double segW = (r.width() - gapPx * (count - 1)) / count;
    if (segW <= 0.3) { paintFade(p, s, f, c); return; }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);   // Kanten sollen sitzen
    p.setPen(Qt::NoPen);
    p.setClipPath(lin->troughPath());

    for (int i = 0; i < count; ++i) {
        // Mitte des Segments, nicht Kante — siehe die Notiz im Kopf.
        const double mid = (i + 0.5) / static_cast<double>(count);
        if (mid > f) { break; }
        const double x = r.left() + i * (segW + gapPx);
        QColor col = c;
        // Die ersten Felder etwas matter: eine Kette, die auf ganzer
        // Laenge gleich leuchtet, sieht aus wie ein Balken mit Luecken.
        col.setAlphaF(0.72 + 0.28 * mid);
        p.setBrush(col);
        p.drawRect(QRectF(x, r.top(), segW, r.height()));
    }
    p.restore();
}

// From Longpath src/gui/instruments/InstrumentPainter.cpp:281 [@551576e3]
void paintPeakMark(QPainter& p, const Spine& s, double f)
{
    if (f <= 0.0) { return; }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor ink(QString::fromLatin1(Style::kTextPrimary));
    p.setPen(QPen(ink, 1.6));
    p.drawLine(s.crossAt(f, 2.0, 2.0));
    p.restore();
}

// ── Glut ─────────────────────────────────────────────────────────────

void paintGlow(QPainter& p, const QRectF& bounds, const QPainterPath& clip,
               const QColor& c, double intensity)
{
    if (bounds.isEmpty()) { return; }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!clip.isEmpty()) { p.setClipPath(clip); }

    QRadialGradient g(bounds.center(), bounds.width() / 2.0);
    // Eine Ellipse statt eines Kreises, wo die Fläche nicht quadratisch
    // ist: der Verlauf wird über die Höhe gestreckt.
    if (!qFuzzyCompare(bounds.width(), bounds.height())) {
        g.setCoordinateMode(QGradient::ObjectBoundingMode);
        g.setCenter(0.5, 0.5);
        g.setFocalPoint(0.5, 0.5);
        g.setRadius(0.5);
    }
    QColor c0 = c; c0.setAlphaF(intensity);
    QColor c1 = c; c1.setAlphaF(intensity * kGlowMidFraction);
    QColor c2 = c; c2.setAlphaF(0.0);
    g.setColorAt(0.0,           c0);
    g.setColorAt(kGlowMidStop,  c1);
    g.setColorAt(1.0,           c2);

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(g));
    p.drawEllipse(bounds);
    p.restore();
}

// From Longpath src/gui/instruments/InstrumentPainter.cpp:324 [@551576e3]
void paintGlow(QPainter& p, const Spine& s, const QColor& c, double intensity)
{
    paintGlow(p, s.glowBounds(), s.glowClip(), c, intensity);
}

// ── Helle Kante am Wert ──────────────────────────────────────────────
//
// Zwei Formen, weil der Ort des Werts zwei Formen hat. Auf der GERADEN
// ist der Wert eine Stelle in der Rille — dort ist der Querstrich die
// Kante. Auf dem BOGEN ist der Wert der Zeiger selbst; ein Querstrich
// über der Rille zeigt dort auf dieselbe Stelle ein zweites Mal.
//
// Der Entwurf hatte für den Bogen eine zweite, dünnere Linie entlang
// derselben Achse vorgesehen — sie war nur unsichtbar, weil der 2,2
// breite Zeiger danach darüber gezeichnet wurde. OE5SOS, 2026-08-18:
// „die Kante wird sichtbar gemacht, nicht gestrichen … als schmaler
// hellerer Streifen entlang einer Flanke". Also nach dem Zeiger und
// seitlich versetzt, siehe paintNeedle().

// From Longpath src/gui/instruments/InstrumentPainter.cpp:343 [@551576e3]
void paintValueEdge(QPainter& p, const Spine& s, double f, const QColor& c)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor e = c;
    e.setAlphaF(kValueEdgeOpacity);
    p.setPen(QPen(e, pen(kValueEdgeOfUnit, s)));
    p.drawLine(s.crossAt(f, 0.0, 0.0));
    p.restore();
}

// ── Zeiger, Nachlaufzeiger, Nabe ─────────────────────────────────────

void paintNeedle(QPainter& p, const ArcSpine& s, double f, const QColor& c,
                 NeedleWeight weight, bool withEdge)
{
    const qreal u    = s.unit();
    const qreal deg  = s.degreeAt(f);
    const qreal w    = qMax(kMinPenWidth,
                            u * (weight == NeedleWeight::Primary
                                     ? kNeedleWidthOfUnit
                                     : kSecondWidthOfUnit));
    const qreal rIn  = u * kNeedleInnerOfUnit;
    const qreal rOut = s.radius() - s.troughWidth() / 2.0
                       - u * kNeedleTipGapOfUnit;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(s.pointAt(rIn, deg), s.pointAt(rOut, deg));

    if (withEdge) {
        // Bündig auf der Flanke: um die halbe Differenz der Breiten
        // versetzt, dann schliesst die Kante mit dem Zeigerrand ab.
        const qreal ew  = qMax(kMinPenWidth, u * kValueEdgeOfUnit);
        const qreal off = (w - ew) / 2.0;
        // Senkrecht zur Zeigerachse, zur Seite der HÖHEREN Werte — die
        // Kante liegt vorne, in der Richtung, in die der Zeiger wandert.
        const QPointF n = s.pointAt(off, deg - 90.0) - s.centre();
        const qreal rEdge = u * kValueEdgeStartOfUnit;
        p.setPen(QPen(c.lighter(kValueEdgeLighter), ew, Qt::SolidLine,
                      Qt::RoundCap));
        p.drawLine(s.pointAt(rEdge, deg) + n, s.pointAt(rOut, deg) + n);
    }
    p.restore();
}

void paintPeakNeedle(QPainter& p, const ArcSpine& s, double f,
                     const QColor& c)
{
    const qreal u    = s.unit();
    const qreal deg  = s.degreeAt(f);
    const qreal rOut = s.radius() - s.troughWidth() / 2.0
                       - u * kNeedleTipGapOfUnit;
    QColor dim = c;
    dim.setAlphaF(static_cast<float>(kPeakOpacity));

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(dim, qMax(kMinPenWidth, u * kPeakWidthOfUnit),
                  Qt::SolidLine, Qt::RoundCap));
    p.drawLine(s.pointAt(rOut - u * kPeakLengthOfUnit, deg),
               s.pointAt(rOut, deg));
    p.restore();
}

// From Longpath src/gui/instruments/InstrumentPainter.cpp:409 [@551576e3]
void paintHub(QPainter& p, const ArcSpine& s)
{
    const qreal u = s.unit();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(measuredDim(), qMax(kMinPenWidth, u * kHubStrokeOfUnit)));
    p.setBrush(hubFill());
    const qreal r = u * kHubRadiusOfUnit;
    p.drawEllipse(s.centre(), r, r);
    p.restore();
}

// ── Teilung ──────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/InstrumentPainter.cpp:423 [@551576e3]
void paintTicks(QPainter& p, const Spine& s, const ReadingDescriptor& d)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor scale(Style::kTextScale);

    QColor minor = scale;
    minor.setAlphaF(static_cast<float>(kMinorTickOpacity));
    p.setPen(QPen(minor, pen(kMinorTickWidthOfUnit, s)));
    for (double v : d.minorTicks) {
        p.drawLine(s.tick(d.fraction(v), at(kMinorTickLenOfUnit, s)));
    }

    p.setPen(QPen(scale, pen(kMajorTickWidthOfUnit, s)));
    for (const ReadingTick& t : d.ticks) {
        p.drawLine(s.tick(d.fraction(t.value), at(kMajorTickLenOfUnit, s)));
    }

    // Die Teilung traegt Zahlen, also Monospace auf der Versalstufe.
    // Sie erbte bisher die Schrift des Aufrufers — damit sah dieselbe
    // Skala je nach Widget anders aus.
    //
    // Die Schrift waechst NICHT mit dem Bogen: die sechs Stufen aus
    // StyleConstants.h sind eine Leiter aus Entscheidungen, keine
    // Proportion. Eine mitwachsende Skalenschrift faende unterhalb von
    // Stufe kFontCaption ohnehin keine lesbare Groesse mehr.
    p.setFont(Style::monoFont(p.font(), Style::kFontCaption));
    const QFontMetricsF fm(p.font());
    p.setPen(scale);
    for (const ReadingTick& t : d.ticks) {
        const QPointF a = s.labelAnchor(d.fraction(t.value),
                                        at(kLabelOffsetOfUnit, s));
        const qreal w = fm.horizontalAdvance(t.label);
        p.drawText(QPointF(a.x() - w / 2.0, a.y() + fm.ascent() / 2.0),
                   t.label);
    }
    p.restore();
}

// ── Farbe ────────────────────────────────────────────────────────────

// From Longpath src/gui/instruments/InstrumentPainter.cpp:464 [@551576e3]
QColor valueColour(const ReadingDescriptor& d, double value)
{
    if (d.threshold.has_value() && value > d.threshold.value()) {
        return danger();
    }
    return measured();
}

} // namespace Instrument
} // namespace NereusSDR
