// =================================================================
// src/gui/widgets/BandwidthFilterPane.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-11-17 — Bewegen-Zeiger ohne Farbprofil (bewegenZeiger):
//                 Absturz 2026-11-10 in QImage::toCGImage, Qt 6.11.1.
//                 Martin Fischer, KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "gui/widgets/BandwidthFilterPane.h"

#include "gui/StyleConstants.h"
#include "gui/ZeigerOhneFarbraum.h"
#include "gui/styles/ThemeQss.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Rand um die Zeichenflaeche. Oben mehr, weil dort die Beschriftungen
// der Kanten und die Breitenpille sitzen; unten fuer die Skala.
constexpr int kPadX      = 6;
constexpr int kPadTop    = 30;
constexpr int kPadBottom = 20;

// Eine Zahl auf der Achse: Megahertz mit drei Nachkommastellen, wie in
// der Vorlage („13.137"). Kilohertz waeren eine Ziffer mehr und sagen
// dasselbe.
QString axisLabel(double hz)
{
    return QString::number(hz / 1.0e6, 'f', 3);
}

// Eine Kantenangabe. Unter 1 kHz in Hertz, darueber in Kilohertz mit
// zwei Stellen — so macht es die Vorlage, und so bleibt die Zahl kurz
// genug, um ueber der Kante zu stehen.
//
// Betreiber 2026-09-03, mit Nachdruck: "minus darf nie!!!!!" — LSB legt
// beide Kanten unterhalb des Traegers, technisch also negativ, aber das
// gehoert nicht in die Anzeige. std::abs() schuetzte bisher nur die
// KILOHERTZ-JA-ODER-NEIN-Schwelle oben, nicht die ausgegebene Zahl
// selbst — "%1" mit dem rohen (moeglicherweise negativen) hz ergab
// "-150 Hz". Jetzt wird ueberall dieselbe Betragszahl verwendet wie die
// LOW/WIDTH/HIGH-Felder daneben (BandwidthFilterApplet::refreshNumbers).
QString cutLabel(int hz)
{
    const int mag = std::abs(hz);
    if (mag < 1000) {
        return QStringLiteral("%1 Hz").arg(mag);
    }
    return QStringLiteral("%1 kHz").arg(mag / 1000.0, 0, 'f', 2);
}

QString widthLabel(int hz)
{
    // Same "minus darf nie" guard as cutLabel() above -- m_high - m_low
    // can go negative if the edges are inverted (a distinct, separately
    // fixed bug), and this label must never expose that as a literal
    // minus sign either.
    const int mag = std::abs(hz);
    if (mag < 1000) { return QStringLiteral("%1 Hz").arg(mag); }
    return QStringLiteral("%1 kHz").arg(mag / 1000.0, 0, 'f', 1);
}

} // namespace

BandwidthFilterPane::BandwidthFilterPane(QWidget* parent)
    : QWidget(parent)
    , m_accent(QColor(Style::role("accent", Style::kAccent)))
{
    // Klein genug fuer den Platz im RxApplet (dort rund 92 px), gross
    // genug, dass Beschriftungen und Achse hineinpassen.
    setMinimumHeight(84);
    setMinimumWidth(180);
    setMouseTracking(true);
}

void BandwidthFilterPane::setLabel(const QString& text)
{
    if (m_label == text) { return; }
    m_label = text;
    update();
}

void BandwidthFilterPane::setAccent(const QColor& c)
{
    if (m_accent == c) { return; }
    m_accent = c;
    update();
}

void BandwidthFilterPane::setVfoFrequency(double hz)
{
    if (qFuzzyCompare(m_vfoHz, hz)) { return; }
    m_vfoHz = hz;
    update();
}

void BandwidthFilterPane::setTrace(const QVector<float>& dbm)
{
    // ── Beruhigen, wie es OpenHPSDR zeigt ───────────────────────────
    //
    // Der Betreiber am 2026-08-23: "extrem unruhig, linie zu dick!
    // baue es genau wie bei openhpsdr."
    //
    // Die Rohwerte kommen mit 20 Bildern je Sekunde und sind
    // Spitzenwerte — jede Stuetzstelle springt bei jedem Bild. Bei
    // OpenHPSDR steht dort eine ruhige, duenne Kurve.
    //
    // Zwei Mittel, in dieser Reihenfolge:
    //
    //   1. ZEITLICH: exponentiell gleiten mit alpha = 0,22. Das ist
    //      dieselbe Familie wie die Glaettung im Panadapter
    //      (LogRecursive), nur ohne dessen Parameterwerk — hier geht
    //      es ums Augenmass beim Kantenziehen, nicht um Messtechnik.
    //      Ein Traeger steigt damit in rund einer Zehntelsekunde auf,
    //      das Zappeln verschwindet.
    //
    // ÖRTLICH wird NICHT geglaettet, und das ist eine Korrektur:
    //
    // Die erste Fassung mittelte ueber drei Stuetzstellen, mit dem
    // Kommentar, das verschlucke keinen echten Traeger — "der ist
    // breiter als drei Punkte". GEMESSEN war das falsch: ein Traeger
    // auf EINER Stuetzstelle, -60 dBm im -120er Rauschen, kam nach dem
    // Fenster bei -100 an. VIERZIG Dezibel weg, durch eine Zeile, die
    // ich fuer harmlos erklaert hatte.
    //
    // Zeitliches Gleiten allein genuegt: Rauschen ist von Bild zu Bild
    // zufaellig und mittelt sich damit ohnehin weg, ein Traeger steht
    // still und bleibt stehen.
    if (dbm.isEmpty()) {
        if (!m_trace.isEmpty()) { m_trace.clear(); update(); }
        return;
    }

    const QVector<float>& in = dbm;

    if (m_trace.size() != in.size()) {
        m_trace = in;                      // Groesse gewechselt: neu setzen
    } else {
        // ── Schnell hoch, gemaechlich runter ────────────────────────
        //
        // Die erste Fassung glich in BEIDE Richtungen gleich schnell
        // (alpha 0,22). Der Betreiber sah daraufhin: "die form bleibt
        // auch immer leicht zu sehen, zeitversetzt" — ein blasses
        // Nachbild, das der Kurve hinterherlaeuft. Genau das macht
        // eine symmetrische Glaettung: sie verzoegert das Steigen
        // ebenso wie das Fallen.
        //
        // Richtig ist ungleich: STEIGEN sofort (ein Traeger, der
        // aufgeht, soll da sein, wenn er da ist), FALLEN gemaechlich
        // (dann beruhigt sich das Rauschen). Dieselbe Bauart wie ein
        // Spitzenwertzeiger mit Ruecklauf — und das ist auch, was
        // OpenHPSDR und Thetis an dieser Stelle zeigen.
        constexpr float aUp   = 0.75f;   // fast sofort
        constexpr float aDown = 0.16f;   // ruhiger Ruecklauf
        for (int i = 0; i < in.size(); ++i) {
            const float a = (in[i] > m_trace[i]) ? aUp : aDown;
            m_trace[i] = m_trace[i] * (1.0f - a) + in[i] * a;
        }
    }
    update();
}

void BandwidthFilterPane::setSpan(int hz)
{
    // Unter 2 kHz wird die Achse unlesbar, ueber 40 kHz verschwindet
    // der Durchlass zu einem Strich.
    const int clamped = std::clamp(hz, 2000, 40000);
    if (m_spanHz == clamped) { return; }
    m_spanHz = clamped;
    update();
}

void BandwidthFilterPane::setFilter(int low, int high)
{
    if (m_low == low && m_high == high) { return; }
    m_low  = low;
    m_high = high;
    update();
}

void BandwidthFilterPane::setHasFrequency(bool on)
{
    if (m_hasFrequency == on) { return; }
    m_hasFrequency = on;
    update();
}

QSize BandwidthFilterPane::sizeHint() const
{
    return QSize(360, 140);
}

QRect BandwidthFilterPane::plotRect() const
{
    return QRect(kPadX, kPadTop,
                 std::max(1, width() - 2 * kPadX),
                 std::max(1, height() - kPadTop - kPadBottom));
}

int BandwidthFilterPane::hzToX(int hz) const
{
    const QRect r = plotRect();
    const double frac = (static_cast<double>(hz) + m_spanHz / 2.0) / m_spanHz;
    return r.left() + static_cast<int>(std::lround(frac * r.width()));
}

int BandwidthFilterPane::xToHz(int x) const
{
    const QRect r = plotRect();
    const double frac = static_cast<double>(x - r.left())
                      / std::max(1, r.width());
    return static_cast<int>(std::lround(frac * m_spanHz - m_spanHz / 2.0));
}

BandwidthFilterPane::Zone BandwidthFilterPane::zoneAt(int x) const
{
    const int xl = hzToX(m_low);
    const int xh = hzToX(m_high);

    // Bei beiden in Reichweite gewinnt die naehere — sonst laesst sich
    // ein sehr schmaler Durchlass nur an einer Seite anfassen.
    const bool nearLow  = std::abs(x - xl) <= kGrabPx;
    const bool nearHigh = std::abs(x - xh) <= kGrabPx;
    if (nearLow && nearHigh) {
        return (std::abs(x - xl) <= std::abs(x - xh)) ? Zone::LowEdge
                                                      : Zone::HighEdge;
    }
    if (nearLow)  { return Zone::LowEdge; }
    if (nearHigh) { return Zone::HighEdge; }
    if (x > xl && x < xh) { return Zone::Body; }
    return Zone::None;
}

// ── Zeichnen ─────────────────────────────────────────────────────────
//
// Richtung „Glas & Tiefe", vom Betreiber am 2026-09-17 aus vier
// Stilblaettern gewaehlt (docs/design/2026-09-17-design-durchsicht.md).
// Die Flaeche ist VERSENKT — Schwarz, Innenschatten oben, Lichtkante —
// wie ein Instrument hinter Glas. Die Kurve traegt einen Hof, der
// Durchlass ist ein Glasstreifen, die Zahlen sitzen in Chips.
//
// Was aus den Tagen davor bleibt, weil es nichts mit dem Look zu tun
// hat, sondern mit dem Lesen:
//   * fester Pegelbereich, 40 dB ueber dem 10. Perzentil (2026-08-23,
//     "wo kein signal ist, ist die linie am boden");
//   * gerade Striche, keine Rundung (2026-08-23, die Vorlage hat
//     scharfe Ecken);
//   * schnell hoch, gemaechlich runter (setTrace);
//   * Kantenwerte ohne Vorzeichen (2026-09-03, "minus darf nie");
//   * Wortmarken tauschen bei LSB die Seite (2026-09-17);
//   * die Kurve bekommt das ganze Feld (2026-09-17, "schaut eher flach").
//
// Was weg ist: die Anteilszellen und die Feinskala im Durchlass. Das
// gewaehlte Blatt hatte beides nicht; die Zellen lagen als Kaesten
// ueber der gefuellten Flaeche, die Feinskala war ein zweiter Rahmen.

namespace {

// Die Mal-Helfer (paintInsetTop, paintGlassChip) liegen seit dem
// 2026-09-18 in StyleConstants.h — der Panadapter-Chrom braucht sie
// auch.
using Style::paintInsetTop;
using Style::paintGlassChip;

} // namespace

void BandwidthFilterPane::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = plotRect();

    // ── Grund: versenkt ─────────────────────────────────────────────
    p.fillRect(rect(), QColor(Style::role("inset-bg", Style::kInsetBg)));
    paintInsetTop(p, rect());
    p.setPen(QColor(255, 255, 255, 6));
    p.drawLine(0, height() - 1, width() - 1, height() - 1);

    // ── Die Marken der Achse: auf RUNDEN Frequenzen ─────────────────
    //
    // Bisher standen sie bei VFO ± k·Schritt. Steht der VFO auf
    // 7.192.500, liegen sie alle auf halben Kilohertz, und die
    // dreistellige Anzeige rundet mal auf, mal ab: 7.189 · 7.191 ·
    // 7.192 · 7.194 · 7.197 — gleiche Abstaende, ungleiche Zahlen, so
    // auf dem Foto vom 2026-09-17. Die Achse gehoert dem Band, nicht
    // dem VFO: Marken auf 7.188 · 7.190 · 7.192 …, die VFO-Linie steht
    // dazwischen, wo sie hingehoert. Gitter und Beschriftung nehmen
    // dieselbe Liste, damit die Zahlen etwas zum Festhalten haben.
    const int stepHz = (m_spanHz <= 6000) ? 1000
                     : (m_spanHz <= 24000) ? 2000 : 5000;
    QVector<int> marks;
    {
        const double first = std::ceil((m_vfoHz - m_spanHz / 2.0) / stepHz) * stepHz;
        for (double f = first; f <= m_vfoHz + m_spanHz / 2.0; f += stepHz) {
            marks << static_cast<int>(std::lround(f - m_vfoHz));
        }
    }

    QColor grid(Style::role("spectrum-grid", Style::kSpectrumGrid));
    grid.setAlpha(28);
    p.setPen(grid);
    for (int hz : marks) {
        const int x = hzToX(hz);
        p.drawLine(x, r.top() - 6, x, r.bottom());
    }

    // ── Der Durchlass als Glasstreifen ──────────────────────────────
    //
    // Von oben bis unten (Betreiber 2026-09-17), als Verlauf: oben
    // etwas dichter, unten leiser, mit einer Lichtkante am oberen
    // Rand. Er liegt UNTER der Kurve — die Kurve ist die Messung, der
    // Streifen nur das Fenster, durch das man sie hoert.
    const int xl = hzToX(m_low);
    const int xh = hzToX(m_high);
    const QColor accent(Style::role("accent", Style::kAccent));
    {
        QLinearGradient pass(0, 0, 0, height());
        QColor c0(accent); c0.setAlpha(54);
        QColor c1(accent); c1.setAlpha(24);
        pass.setColorAt(0.0, c0);
        pass.setColorAt(1.0, c1);
        p.fillRect(QRect(xl, 0, std::max(1, xh - xl), height()), pass);
        p.setPen(QColor(255, 255, 255, 26));
        p.drawLine(xl + 1, 0, xh - 1, 0);
    }

    // Die Nulllinie ist die VFO-Frequenz.
    {
        const int x0 = hzToX(0);
        p.setPen(QColor(Style::role("border", Style::kBorder)));
        p.drawLine(x0, r.top() - 6, x0, r.bottom());
    }

    // ── Das Signal ──────────────────────────────────────────────────
    //
    // Der Ausschnitt kommt vom Panadapter (SpectrumWidget::dbmOverRange)
    // — dieselbe Abbildung, dieselbe Kalibrierung. Boden ist das 10.
    // Perzentil (ein Ausreisser verschiebt sonst die Skala).
    //
    // Die Decke lag bis 2026-09-30 fest 40 dB darueber, und ein starkes
    // Signal stiess oben an — der Gipfel lief flach am Rand entlang, was
    // aussieht, als schluge die Kurve aus dem Rahmen. Betreiber am
    // 2026-09-30 an einem S9+12-Signal auf 40 m: "bandwith schlaegt ueber
    // den rahmen".
    //
    // Jetzt geht die Decke mit, sobald die Spitze sie erreicht: 40 dB
    // bleiben das MINDESTmass, damit ein ruhiges Band nicht zur flachen
    // Linie zusammenfaellt und die Skala zwischen zwei Baendern
    // vergleichbar bleibt — aber ein lautes Signal bekommt den Platz, den
    // es braucht, plus 3 dB Luft, damit der Gipfel nicht am Rahmen klebt.
    const QColor traceLine(Style::role("measured", Style::kAmberText));
    const int top = r.top() + 6;
    const int bot = r.bottom() - 2;
    if (m_trace.size() >= 2 && bot > top) {
        QVector<float> sorted = m_trace;
        std::sort(sorted.begin(), sorted.end());
        const float lo = sorted.at(sorted.size() / 10);
        const float spitze = sorted.constLast();
        const float hi = std::max(lo + 40.0f, spitze + 3.0f);
        const double yScale = (bot - top) / static_cast<double>(hi - lo);

        QPolygonF poly;
        poly.reserve(m_trace.size() + 2);
        poly << QPointF(r.left(), bot);
        for (int i = 0; i < m_trace.size(); ++i) {
            const double x = r.left()
                + (r.width() - 1.0) * i / (m_trace.size() - 1.0);
            const double y = bot - (m_trace[i] - lo) * yScale;
            poly << QPointF(x, qBound<double>(top, y, bot));
        }
        poly << QPointF(r.right(), bot);
        QPainterPath tracePath;
        tracePath.addPolygon(poly);

        // ── Pegelraster mit dBm-Marke ───────────────────────────────
        //
        // Drei waagrechte Linien. Die Zahlen nur, wenn sie Platz
        // haben (2026-09-17: bei elf Punkten Abstand beruehrten sich
        // alle drei) — eine Linie ohne Zahl ist immer noch ein Raster.
        {
            p.setFont(Style::monoFont(font(), Style::kFontCaption));
            QColor gridLine(grid); gridLine.setAlpha(44);
            const QColor gridText(Style::role("text-scale", Style::kTextScale));
            const int spacing = (bot - top) / 4;
            const bool roomForAll = spacing >= 14;
            const bool roomForOne = spacing >= 10;
            for (int k = 1; k <= 3; ++k) {
                const double frac = k / 4.0;
                const int y = static_cast<int>(bot - (bot - top) * frac);
                p.setPen(gridLine);
                p.drawLine(r.left() + 1, y, r.right() - 1, y);
                if (!roomForAll && !(roomForOne && k == 2)) { continue; }
                const int dbm = static_cast<int>(std::lround(lo + (hi - lo) * frac));
                p.setPen(gridText);
                p.drawText(QRect(r.right() - 46, y - 7, 42, 12),
                           Qt::AlignRight | Qt::AlignVCenter,
                           QStringLiteral("%1").arg(dbm));
            }
        }

        const int xlF = qBound(r.left(), xl, r.right());
        const int xhF = qBound(r.left(), xh, r.right());

        // Kantenglaettung NUR fuer den Kurvenzug: Raster, Streifen und
        // Griffe sind waagrecht und senkrecht, Glaettung machte sie
        // nur unscharf.
        p.setRenderHint(QPainter::Antialiasing, true);

        // ── Verlauf unter der ganzen Kurve ──────────────────────────
        {
            QLinearGradient grad(0, top, 0, bot);
            QColor c0(traceLine); c0.setAlpha(96);
            QColor c1(traceLine); c1.setAlpha(0);
            grad.setColorAt(0.0, c0);
            grad.setColorAt(1.0, c1);
            p.setPen(Qt::NoPen);
            p.setBrush(grad);
            p.drawPath(tracePath);
        }

        // ── Im Durchlass gefuellt ───────────────────────────────────
        //
        // Was HOERBAR ist, steht als Flaeche da (2026-09-17: "leider
        // ist da nur ein kleiner strich"); ausserhalb bleibt der zarte
        // Verlauf — man sieht auf einen Blick, was die Kante
        // abschneidet.
        {
            p.save();
            p.setClipRect(QRect(xlF, top, std::max(1, xhF - xlF), bot - top + 1));
            QLinearGradient inner(0, top, 0, bot);
            QColor i0(traceLine); i0.setAlpha(150);
            QColor i1(traceLine); i1.setAlpha(64);
            inner.setColorAt(0.0, i0);
            inner.setColorAt(1.0, i1);
            p.setPen(Qt::NoPen);
            p.setBrush(inner);
            p.drawPath(tracePath);
            p.restore();
        }

        // ── Hof, dann Linie ─────────────────────────────────────────
        //
        // Der Hof ist es, was der Betreiber am OpenHPSDR-Bild als
        // "wirkt eher 3D" beschrieben hat (2026-08-23): kein breiter
        // Strich, sondern Durchgaenge von breit und blass nach schmal
        // und kraeftig. Drei, seit die Stuetzstellen dicht genug
        // liegen, dass keine Zacke zum Klumpen wird.
        {
            struct GlowPass { double width; int alpha; };
            static const GlowPass kGlow[] = { {8.0, 14}, {4.0, 30}, {2.5, 60} };
            p.setBrush(Qt::NoBrush);
            for (const GlowPass& g : kGlow) {
                QColor glow(traceLine);
                glow.setAlpha(g.alpha);
                QPen glowPen(glow, g.width);
                glowPen.setJoinStyle(Qt::RoundJoin);
                glowPen.setCapStyle(Qt::RoundCap);
                p.setPen(glowPen);
                p.drawPath(tracePath);
            }
        }
        p.setPen(QPen(traceLine, 0.75));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tracePath);

        // ── Die blasse Bezugslinie ──────────────────────────────────
        //
        // Der GEGLAETTETE Mittelwert, traege (alpha 0,02): sagt, wo
        // der Empfaenger im Mittel steht, und macht sichtbar, ob eine
        // Spitze wirklich heraussticht oder nur der Flur atmet.
        {
            if (m_avgLine.size() != m_trace.size()) {
                m_avgLine = m_trace;
            } else {
                constexpr float aa = 0.02f;
                for (int i = 0; i < m_trace.size(); ++i) {
                    m_avgLine[i] = m_avgLine[i] * (1.0f - aa) + m_trace[i] * aa;
                }
            }
            QColor avg(Style::role("text-scale", Style::kTextScale));
            avg.setAlpha(60);
            p.setPen(QPen(avg, 1.0));
            QPolygonF ap;
            ap.reserve(m_avgLine.size());
            for (int i = 0; i < m_avgLine.size(); ++i) {
                const double x = r.left()
                    + (r.width() - 1.0) * i / (m_avgLine.size() - 1.0);
                const double y = bot - (m_avgLine[i] - lo) * yScale;
                ap << QPointF(x, qBound<double>(top, y, bot));
            }
            p.drawPolyline(ap);
        }
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    // ── Die Kanten ──────────────────────────────────────────────────
    //
    // Etwas heller als das Auswahlblau, damit sie auf dem Streifen
    // stehen; ueber die ganze Hoehe, weil die Zahlen oben und die
    // Achse unten zu dieser Saeule gehoeren.
    const QColor edge = accent.lighter(140);
    p.setPen(QPen(edge, 1));
    p.drawLine(xl, 0, xl, height());
    p.drawLine(xh, 0, xh, height());

    // ── Griffe: erhabene Pillen in halber Hoehe ─────────────────────
    //
    // Die ganze Kante als Griff waere groesser, sagt aber nicht, WO
    // man fassen soll. Erhaben: Verlauf, Lichtkante oben, dunkle
    // Unterkante — das Gegenstueck zur versenkten Flaeche.
    auto drawHandle = [&](int x, bool active) {
        const QRect h(x - 4, r.center().y() - 11, 9, 22);
        QLinearGradient g(0, h.top(), 0, h.bottom());
        g.setColorAt(0.0, QColor(active ? Style::kGlassSelTop : Style::kGlassBtnTop));
        g.setColorAt(1.0, QColor(active ? Style::kGlassSelBot : Style::kGlassBtnBot));
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QColor(0, 0, 0, 140));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(h.translated(0, 1), 4, 4);        // Schatten
        p.setPen(QPen(edge, active ? 1.5 : 1.0));
        p.setBrush(g);
        p.drawRoundedRect(h, 4, 4);
        p.setPen(QColor(255, 255, 255, Style::kGlassLightAlpha + 8));
        p.drawLine(h.left() + 3, h.top() + 1, h.right() - 3, h.top() + 1);   // Lichtkante
        QColor tick(edge); tick.setAlphaF(0.85f);
        p.setPen(tick);
        p.drawLine(x, h.top() + 6, x, h.bottom() - 6);
        p.restore();
    };
    drawHandle(xl, m_hover == Zone::LowEdge || m_drag == Zone::LowEdge);
    drawHandle(xh, m_hover == Zone::HighEdge || m_drag == Zone::HighEdge);

    // ── Beschriftungen oben ─────────────────────────────────────────
    //
    // Zahlen Monospace (Hausstil Regel 4), auf der Schriftleiter: 11
    // fuer Werte, 9 fuer Achse und Marken. Wortmarken als Versalzeile.
    const QFont value = Style::monoFont(font(), Style::kFontSmall);
    const QFont small = Style::monoFont(font(), Style::kFontCaption);
    const QColor faint(Style::role("text-scale", Style::kTextScale));
    const QColor ink(Style::role("text", Style::kTextPrimary));

    // Eng: zuerst faellt das WORT, dann die Zahl (2026-08-22). Gemessen
    // am Platz zwischen den Griffen, nicht an der Fensterbreite — und
    // fuer die Zahlen am tatsaechlichen Platz NEBEN dem Breitenchip:
    // Monospace ist breiter als die Textschrift von frueher, und auf
    // dem 620×105-Blatt schob sich "3.00 kHz" in den Chip. Wo die Zahl
    // nicht neben den Chip passt, faellt sie; LOW/HIGH stehen ohnehin
    // in der Bedienzeile darunter.
    const int labelRoom = xh - xl;
    const bool wordMarks = labelRoom >= 190;
    const QString widthText = widthLabel(m_high - m_low);
    const int chipW  = QFontMetrics(value).horizontalAdvance(widthText) + 18;
    const int valueW = std::max(QFontMetrics(value).horizontalAdvance(cutLabel(m_low)),
                                QFontMetrics(value).horizontalAdvance(cutLabel(m_high)));
    const bool numbers = labelRoom >= chipW + 2 * (valueW + 14);

    if (wordMarks) {
        // Die Wortmarken folgen dem Seitenband: ohne Vorzeichen muss
        // das Wort sagen, was die Zahl ist. Bei LSB liegt die nahe
        // Kante rechts — links steht also der Audio-HOCHschnitt.
        p.setFont(Style::capsFont(font(), 8));
        p.setPen(faint);
        const bool lowerSideband = (m_low < 0 && m_high <= 0);
        p.drawText(QRect(xl + 5, 1, 90, 10),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   lowerSideband ? QStringLiteral("HIGH CUT")
                                 : QStringLiteral("LOW CUT"));
        p.drawText(QRect(xh - 95, 1, 90, 10),
                   Qt::AlignRight | Qt::AlignVCenter,
                   lowerSideband ? QStringLiteral("LOW CUT")
                                 : QStringLiteral("HIGH CUT"));
    }

    if (numbers) {
        p.setFont(value);
        p.setPen(ink);
        const int yVal = wordMarks ? 12 : 4;
        p.drawText(QRect(xl + 5, yVal, 90, 13),
                   Qt::AlignLeft | Qt::AlignVCenter, cutLabel(m_low));
        p.drawText(QRect(xh - 95, yVal, 90, 13),
                   Qt::AlignRight | Qt::AlignVCenter, cutLabel(m_high));
    }

    // Die Breite in der Mitte, in einem Glaschip: sie ist die Zahl,
    // nach der man den Filter benennt.
    {
        const int cx = (xl + xh) / 2;
        const QRect box(cx - chipW / 2, 7, chipW, 19);
        paintGlassChip(p, box, Style::kGlassChipRadius);
        p.setFont(value);
        p.setPen(ink);
        p.drawText(box, Qt::AlignCenter, widthText);
    }

    // ── Die Beschriftung des Empfaengers ────────────────────────────
    //
    // Eine Kapsel, kein Knopf: grau, nicht blau — sie ist keine
    // Bedienung, und "Blau ist anfassbar".
    {
        const QFont cap = Style::capsFont(font(), 8);
        const QFontMetrics fm(cap);
        const int tw = fm.horizontalAdvance(m_label) + 14;
        const QRect box(4, 4, tw, 14);
        paintGlassChip(p, box, 7);
        p.setFont(cap);
        p.setPen(QColor(Style::role("text-secondary", Style::kTextSecondary)));
        p.drawText(box, Qt::AlignCenter, m_label);
    }

    // ── Die Achse ───────────────────────────────────────────────────
    //
    // Ohne Verbindung steht hier NICHTS. Eine erfundene Frequenz waere
    // eine Behauptung — dieselbe Regel wie beim Panadapter-Kopf.
    p.setFont(small);
    if (!m_hasFrequency) {
        p.setPen(faint);
        p.drawText(QRect(0, height() - kPadBottom, width(), kPadBottom),
                   Qt::AlignCenter, QStringLiteral("no radio"));
        return;
    }

    // Zahlen mittig unter ihrer Marke, am Rand hineingeschoben statt
    // angeschnitten (".111" statt "7.111" ist schlimmer als keine).
    //
    // Eng — die Flaeche im RxApplet ist 180 Punkte breit — stuenden
    // fuenf Zahlen ineinander ("14.22014.222…" auf dem Blatt vom
    // 2026-09-17). Dann bekommt nur jede zweite (dritte, …) Marke eine
    // Zahl; die Gitterlinien bleiben alle. Gemessen an der breitesten
    // Zahl gegen den Markenabstand, nicht geraten.
    int every = 1;
    if (marks.size() >= 2) {
        int widest = 0;
        for (int hz : marks) {
            widest = qMax(widest, p.fontMetrics().horizontalAdvance(axisLabel(m_vfoHz + hz)));
        }
        const int gap = qAbs(hzToX(marks[1]) - hzToX(marks[0]));
        if (gap > 0) {
            while (every * gap < widest + 10) { ++every; }
        }
    }
    // Das Hineinschieben am Rand kann eine Zahl auf ihre Nachbarin
    // schieben (268 px breit: "14.22014.222", Blatt vom 2026-09-18).
    // Darum merkt sich die Schleife den rechten Rand der zuletzt
    // gezeichneten Zahl; was darueber laege, entfaellt — eine Zahl
    // weniger ist besser als zwei ineinander.
    int lastRight = -1000;
    for (int i = 0; i < marks.size(); ++i) {
        if (i % every != 0) { continue; }
        const int hz = marks[i];
        const int x = hzToX(hz);
        const QString t = axisLabel(m_vfoHz + hz);
        const int tw = p.fontMetrics().horizontalAdvance(t) + 8;
        int left = x - tw / 2;
        if (left < 1) { left = 1; }
        if (left + tw > width() - 1) { left = width() - 1 - tw; }
        if (left < lastRight + 4) { continue; }
        lastRight = left + tw;
        p.setPen(faint);
        p.drawText(QRect(left, height() - kPadBottom + 2, tw, 13),
                   Qt::AlignCenter, t);
    }
}

// ── Ziehen ───────────────────────────────────────────────────────────

void BandwidthFilterPane::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }

    m_drag = zoneAt(ev->pos().x());
    if (m_drag == Zone::None) { return; }

    m_dragStartX    = ev->pos().x();
    m_dragStartLow  = m_low;
    m_dragStartHigh = m_high;
    update();
}

void BandwidthFilterPane::mouseMoveEvent(QMouseEvent* ev)
{
    const int x = ev->pos().x();

    if (m_drag == Zone::None) {
        // Der Zeiger sagt VORHER, was ein Ziehen tun wird. Nur setzen,
        // wenn er sich aendert — jedes setCursor kostet auf macOS ein
        // CGImageCreate, und bei jeder Mausbewegung ist das zu viel.
        const Zone z = zoneAt(x);
        if (z != m_hover) {
            m_hover = z;
            // Die Mitte zieht den ganzen Filter: der Bewegen-Zeiger, aber
            // aus einem Bild ohne Farbprofil (gui/ZeigerOhneFarbraum.h --
            // Qt 6.11.1 stuerzte beim Bau des eigenen ab, 2026-11-10).
            if (z == Zone::LowEdge || z == Zone::HighEdge) {
                setCursor(Qt::SizeHorCursor);
            } else if (z == Zone::Body) {
                setCursor(bewegenZeiger());
            } else {
                setCursor(Qt::ArrowCursor);
            }
            update();
        }
        return;
    }

    const int deltaHz = ((xToHz(x) - xToHz(m_dragStartX)) / kStepHz) * kStepHz;

    if (m_drag == Zone::Body) {
        // Die Breite bleibt. Deshalb die MITTE melden und nicht zwei
        // Kanten: nur so kann das Modell mit filterShift begrenzen und
        // die Breite am Rand erhalten.
        const int centre = (m_dragStartLow + m_dragStartHigh) / 2 + deltaHz;
        emit filterCentreChanged(centre);
        return;
    }

    int low  = m_dragStartLow;
    int high = m_dragStartHigh;
    if (m_drag == Zone::LowEdge)  { low  = m_dragStartLow  + deltaHz; }
    if (m_drag == Zone::HighEdge) { high = m_dragStartHigh + deltaHz; }

    // Die Kanten duerfen einander nicht ueberholen. Alles Weitere —
    // Seitenband, Deckel — entscheidet das Modell.
    if (high - low < kStepHz) {
        if (m_drag == Zone::LowEdge) { low  = high - kStepHz; }
        else                          { high = low  + kStepHz; }
    }
    emit filterChanged(low, high);
}

void BandwidthFilterPane::mouseReleaseEvent(QMouseEvent* ev)
{
    Q_UNUSED(ev)
    if (m_drag == Zone::None) { return; }
    m_drag = Zone::None;
    update();
}

void BandwidthFilterPane::leaveEvent(QEvent*)
{
    if (m_hover == Zone::None) { return; }
    m_hover = Zone::None;
    unsetCursor();
    update();
}

} // namespace Longpath
