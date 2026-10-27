// src/gui/instruments/BarInstrument.cpp

// --- From src/gui/instruments/BarInstrument.h ---
// =================================================================
// src/gui/instruments/BarInstrument.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Das Balkeninstrument ─────────────────────────────────────────────
//
// OE5SOS, 2026-08-17: „Dasselbe in der vorhandenen Balkenform als
// eigenes Widget. HGauge ist da; das Widget ist die Fassung drumherum
// mit Kopf und Fusszeile."
//
// ── Warum es nicht HGauge einbettet ──────────────────────────────────
//
// HGauge malt eine Mulde mit RANDLINIE und füllt sie in drei Zonen
// massiv aus (HGauge.cpp:60-115). Es kennt weder den Innenschatten
// noch den auslaufenden Verlauf noch die Glut — die drei Mittel, die
// dieses Widget mit dem Zeigerinstrument teilen soll.
//
// HGauge dafür umzubauen hiesse, zwanzig bestehende Gauges in zehn
// Applets auf einmal umzugestalten. Das ist eine sichtbare Änderung
// und gehört dem Betreiber, nicht diesem Auftrag. Also zeichnet dieses
// Widget über LinearSpine + InstrumentPainter — dieselbe Form wie
// HGauge, dieselbe Handschrift wie der Zeiger, und HGauge bleibt
// unangetastet.
//
// ── Eine Abweichung vom älteren Entwurf, bewusst ─────────────────────
//
// zeiger-verfeinert.html sagt im Fliesstext: „Der Balken bleibt, wie
// er ist" — also massive Füllung. Der Auftrag desselben Tages sagt
// darüber: „Gemeinsame Handschrift für alle drei — dieselben drei
// Mittel." Hier gilt der Auftrag, also auch beim Balken der
// auslaufende Verlauf. Genau so ein Punkt gehört auf den Schirm
// gesehen, bevor er stehen bleibt.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/instruments/BarInstrument.cpp ---
// =================================================================
// src/gui/instruments/BarInstrument.cpp  (Longpath)
// =================================================================
// Siehe BarInstrument.h.
// =================================================================

// =================================================================
// src/gui/instruments/BarInstrument.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/BarInstrument.h
//   src/gui/instruments/BarInstrument.cpp
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
// 2026-10-12 Keep the table reading name on bound scale overrides.
//            J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Footer fit: sizeHint() asks for the trough, scale
//                 numbers and footer together, and fitFooter() hides the
//                 footer below that height. Upstream drew the footer over
//                 the scale numbers at 74 px and showed it over the
//                 trough at the 56 px minimum. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - fullHeight() counts the second trough and setSecondary()
//                 refits the footer (shared Longpath defect); adds
//                 clearSecondValue().
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - The compact form shows one dash for no reading, as the
//                 footer of the full form does (upstream drew two).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12 - A reading of the caller's own (setReading, setValue):
//                 the places that looked the primary reading and its name
//                 up in the binding table go through primaryReading() and
//                 nameOf(). The compact form's number size and its small
//                 second reading (setCompactValuePx, setCompactNote), and
//                 the footer without its peak and limit text
//                 (setFooterDetailVisible).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/instruments/BarInstrument.h"

#include "gui/instruments/InstrumentStyle.h"
#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/instruments/ReadingSource.h"

#include <QPainter>
#include <QVBoxLayout>

namespace NereusSDR {

// NereusSDR: the Longpath values, see gui/instruments/InstrumentStyle.h.
namespace Style = ::NereusSDR::InstrumentStyle;

namespace {
// From Longpath src/gui/instruments/BarInstrument.cpp:22 [@551576e3]
/// Höhe der Mulde. Entwurf: H = 20 bei einem 300 breiten Feld.
constexpr double kTroughHeight = 20.0;
// From Longpath src/gui/instruments/BarInstrument.cpp:24 [@551576e3]
/// Platz unter der Mulde für die Beschriftung der Teilung.
constexpr double kScaleHeight  = 18.0;
// From Longpath src/gui/instruments/BarInstrument.cpp:26 [@551576e3]
/// Abstand zwischen zwei Mulden, wenn eine zweite Anzeige läuft.
constexpr double kGap          = 6.0;
} // namespace

// From Longpath src/gui/instruments/BarInstrument.cpp:29 [@551576e3]
BarInstrument::BarInstrument(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 8, 14, 10);
    lay->setSpacing(0);
    lay->addStretch(1);
    m_footer = new InstrumentFooter(this);
    lay->addWidget(m_footer, 0);
}

// From Longpath src/gui/instruments/BarInstrument.cpp:40 [@551576e3]
bool BarInstrument::setPrimary(int bindingId)
{
    const ReadingDescriptor* d = readingFor(bindingId);
    if (!d || !d->hasScale) { return false; }
    m_primary = bindingId;
    // NereusSDR: a bound reading replaces the caller's own, see setReading().
    m_ownReading.reset();
    m_ownName.clear();
    // Kein Startwert — siehe NeedleInstrument::setPrimary.
    clearValue();
    return true;
}

// From Longpath src/gui/instruments/BarInstrument.cpp:50 [@551576e3]
bool BarInstrument::setSecondary(int bindingId)
{
    if (bindingId < 0) {
        m_secondary = -1;
        m_hasSecond = false;
        updateGeometry();   // die zweite Mulde braucht Hoehe, siehe sizeHint()
        fitFooter();        // NereusSDR: see fitFooter()
        update();
        return true;
    }
    const ReadingDescriptor* d = readingFor(bindingId);
    if (!d || !d->hasScale) { return false; }
    m_secondary = bindingId;
    m_hasSecond = false;
    updateGeometry();
    fitFooter();   // NereusSDR: see fitFooter()
    update();
    return true;
}

// From Longpath src/gui/instruments/BarInstrument.cpp:68 [@551576e3]
QSize BarInstrument::sizeHint() const
{
    // Betreiber 2026-08-30: SWR/Leistung soll beide Mulden dauerhaft
    // zeigen (setSecondary), nicht nur umschalten -- die feste Groesse
    // von vorher (fuer genau EINE Mulde bemessen) liess die zweite
    // dann am unteren Rand abgeschnitten oder gequetscht. Dieselbe
    // Rechnung wie paintInto()s "lower"-Rechteck, nur rueckwaerts: was
    // braucht eine zweite Mulde an zusaetzlicher Hoehe.
    const int extra = (m_secondary >= 0) ? int(kTroughHeight + kScaleHeight + kGap) : 0;
    // Betreiber 2026-08-30, zum Frequenz-Widget: "abstand noch immer
    // zu gross". setCompact() nimmt die Fusszeile weg, aber sizeHint()
    // fragte bislang trotzdem nach demselben Platz wie MIT Fusszeile
    // (74) -- das umgebende Layout (FrequencyApplet) reservierte also
    // Hoehe, die nirgends mehr hinging, und genau das war die Luecke.
    // Mulde + Skalenzahlen brauchen kTroughHeight+kScaleHeight (38);
    // ein wenig Luft (6) haelt die Zeile atembar.
    const int base = m_compact ? int(kTroughHeight + kScaleHeight) + 6 : 74;
    // NereusSDR: not below what the trough, its scale numbers and the
    // footer need together (fullHeight()). At upstream's 74 the footer
    // sat over the scale numbers.
    // fullHeight() already counts the second trough.
    return {320, m_compact ? base + extra : qMax(base + extra, fullHeight())};
}

// From Longpath src/gui/instruments/BarInstrument.cpp:88 [@551576e3]
QSize BarInstrument::minimumSizeHint() const
{
    const int extra = (m_secondary >= 0) ? int(kTroughHeight + kScaleHeight + kGap) : 0;
    const int base = m_compact ? 30 : 56;
    return {kMinWidth, base + extra};
}

// From Longpath src/gui/instruments/BarInstrument.cpp:95 [@551576e3]
void BarInstrument::clearValue()
{
    m_hasValue = false;
    m_hasSecond = false;
    m_peak.forget();
    refreshFooter();
    update();
}

// NereusSDR: see the header.
void BarInstrument::clearSecondValue()
{
    m_hasSecond = false;
    update();
}

// NereusSDR: see the header.
void BarInstrument::setPrimaryScale(const ReadingDescriptor& d)
{
    m_ownReading = d;
    refreshFooter();
    update();
}

void BarInstrument::setReading(const ReadingDescriptor& d, const QString& name)
{
    m_ownReading = d;
    m_ownName = name;
    m_primary = -1;
    refreshFooter();
    update();
}

// NereusSDR: see the header. The same steps as onReading() for the
// primary binding.
void BarInstrument::setValue(double value)
{
    m_value = value;
    m_hasValue = true;
    m_peak.note(value);
    refreshFooter();
    update();
}

// NereusSDR: see the header.
void BarInstrument::setCompactValuePx(int px)
{
    if (m_compactValuePx == px) { return; }
    m_compactValuePx = px;
    update();
}

// NereusSDR: see the header.
void BarInstrument::setCompactNote(const QString& label, const QString& number)
{
    if (m_noteLabel == label && m_noteNumber == number) { return; }
    m_noteLabel = label;
    m_noteNumber = number;
    update();
}

// NereusSDR: see the header.
void BarInstrument::setFooterDetailVisible(bool on)
{
    if (m_footerDetail == on) { return; }
    m_footerDetail = on;
    if (m_footer) { m_footer->setDetailVisible(on); }
    refreshFooter();
}

// NereusSDR: see the header.
const ReadingDescriptor* BarInstrument::primaryReading() const
{
    if (m_ownReading.has_value()) { return &m_ownReading.value(); }
    return readingFor(m_primary);
}

// NereusSDR: see the header.
QString BarInstrument::readingName() const
{
    const ReadingDescriptor* d = primaryReading();
    return d ? nameOf(*d) : QString();
}

// NereusSDR: see the header.
QString BarInstrument::nameOf(const ReadingDescriptor& d) const
{
    if (m_primary == -1 && m_ownReading.has_value()
        && &d == &m_ownReading.value()) {
        return m_ownName;
    }
    return d.thetisName();
}

// From Longpath src/gui/instruments/BarInstrument.cpp:104 [@551576e3]
void BarInstrument::onReading(int bindingId, double value)
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

// From Longpath src/gui/instruments/BarInstrument.cpp:119 [@551576e3]
void BarInstrument::refreshFooter()
{
    // Wortgleich mit NeedleInstrument::refreshFooter — dieselbe
    // Fusszeile, dieselben Felder, damit der Blick beim Umschalten
    // nicht springt.
    // NereusSDR: primaryReading() and nameOf() for readingFor(m_primary)
    // and d->thetisName(), see setReading().
    const ReadingDescriptor* d = primaryReading();
    if (!d || !m_footer) { return; }
    m_footer->setCaption(nameOf(*d));

    if (!m_hasValue) {
        // Wortgleich mit NeedleInstrument::refreshFooter — dieselbe
        // Fusszeile sagt in beiden Formen dasselbe.
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

// From Longpath src/gui/instruments/BarInstrument.cpp:146 [@551576e3]
void BarInstrument::setSegmented(bool on)
{
    if (m_segmented == on) { return; }
    m_segmented = on;
    update();
}

// From Longpath src/gui/instruments/BarInstrument.cpp:153 [@551576e3]
void BarInstrument::setTube(bool on)
{
    if (m_tube == on) { return; }
    m_tube = on;
    update();
}

// From Longpath src/gui/instruments/BarInstrument.cpp:160 [@551576e3]
void BarInstrument::setCompact(bool on)
{
    if (m_compact == on) { return; }
    m_compact = on;
    if (m_footer) { m_footer->setVisible(!m_compact); }

    // Eine Untergrenze, damit die Zeile im Layout nicht plattgedrueckt
    // wird. 30 Punkte tragen Mulde und Skalenzahlen; darunter faellt
    // die Flaeche wieder unter die Mulde und es wird gar nichts mehr
    // gezeichnet — genau der Fehler vom 2026-08-23. Die Raender
    // schrumpfen zwar inzwischen mit, aber eine Zeile von zehn Punkten
    // waere auch mit Rand null noch unlesbar.
    setMinimumHeight(m_compact ? 30 : 0);
    fitFooter();   // NereusSDR: see fitFooter()

    updateGeometry();
    update();
}

// NereusSDR: the full (non-compact) height: paintInto()'s largest
// margins (8 above, 10 below), the trough, its scale numbers, the
// second trough when a secondary reading is set (the same extra as
// sizeHint()) and the footer's own height. Longpath has the same gap:
// its footer covers the second trough at the minimum height.
int BarInstrument::fullHeight() const
{
    const int footerH = m_footer ? m_footer->sizeHint().height() : 0;
    const int extra = (m_secondary >= 0) ? int(kTroughHeight + kScaleHeight + kGap) : 0;
    return 8 + int(kTroughHeight + kScaleHeight) + 10 + extra + footerH;
}

// NereusSDR: the footer is a child widget, so paintInto()'s
// tooShortForFooter only stopped the painting from making room for it;
// the footer itself still showed, over the trough at the 56 px minimum.
// Below fullHeight() it is hidden, and paintInto() then gives the whole
// height to the trough (its naturalFooterH is 0 for a hidden footer).
void BarInstrument::fitFooter()
{
    if (!m_footer || m_compact) { return; }
    m_footer->setVisible(height() >= fullHeight());
}

void BarInstrument::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    fitFooter();
}

void BarInstrument::paintOne(QPainter& p, const QRectF& area,
                             const ReadingDescriptor& d, double value,
                             bool withGlow, bool hasValue)
{
    const double f = d.fraction(value);
    const QColor col = Instrument::valueColour(d, value);

    // ── Die knappe Form: Haarlinie statt Mulde ────────────────────────
    //
    // Betreiber 2026-08-30, nach drei Entwurfsblaettern ("A -- bisher",
    // "B -- nur schmaler", "C -- Haarlinie statt Mulde"): "c" gewaehlt.
    // Keine gefuellte Flaeche mehr -- Name+Wert stehen in einer
    // Textzeile, darunter nur eine duenne Linie, deren linker Teil bis
    // zum Wert in Messwertfarbe hervortritt. Segmente, Roehre, Glut,
    // Spitzenhaltung und Schwellenmarke gehoeren zur GROSSEN Mulde und
    // haben hier keinen Platz mehr -- wer sie sehen will, nutzt die
    // eigenstaendige Anzeige (setCompact(false)).
    if (m_compact) {
        const QFont capFont = Style::capsFont(p.font(), Style::kFontSmall);
        const qreal textY = area.top() + QFontMetricsF(capFont).ascent();

        p.setFont(capFont);
        p.setPen(QColor(Style::role("text-scale", Style::kTextScale)));
        // NereusSDR: nameOf(d) for d.thetisName(), see setReading().
        p.drawText(QPointF(area.left(), textY), nameOf(d));
        // NereusSDR: the small second reading, see setCompactNote(). A
        // hairline sets it apart from the row's name; its label is a size
        // down and its number is the dim reading color, so it reads as a
        // second, lesser reading. Only the primary row carries it.
        if (&d == primaryReading()
            && (!m_noteLabel.isEmpty() || !m_noteNumber.isEmpty())) {
            const QFontMetricsF capFm(capFont);
            qreal x = area.left() + capFm.horizontalAdvance(nameOf(d)) + 10.0;
            p.setPen(QPen(QColor(Style::role("text-scale", Style::kTextScale)), 1.0));
            p.drawLine(QPointF(x, textY - capFm.ascent() + 2.0),
                       QPointF(x, textY + 1.0));
            x += 9.0;
            const QFont labelFont = Style::capsFont(p.font(), Style::kFontCaption);
            p.setFont(labelFont);
            p.setPen(QColor(Style::role("text-scale", Style::kTextScale)));
            p.drawText(QPointF(x, textY), m_noteLabel);
            if (!m_noteLabel.isEmpty()) {
                x += QFontMetricsF(labelFont).horizontalAdvance(m_noteLabel) + 6.0;
            }
            p.setFont(Style::monoFont(p.font(), Style::kFontSmall, QFont::Normal));
            p.setPen(Instrument::measuredDim());
            p.drawText(QPointF(x, textY), m_noteNumber);
        }

        {
            // Ohne Messung ein Strich, keine leere Stelle (Hausstil
            // Regel 7): auf dem Foto vom 2026-09-17 standen POWER und
            // SWR im Frequenz-Applet ohne jede Zahl da — eine leere
            // Zeile liest sich als "kaputt", ein Strich als "nichts
            // gemessen".
            // NereusSDR: one dash, as the footer of the full form and of
            // the needle shows for no reading (refreshFooter). Upstream
            // drew two here, so the two forms side by side disagreed.
            const QString text = hasValue ? d.textWithUnit(value)
                                          : QStringLiteral("\u2013");
            // NereusSDR: the caller's size when it set one, see
            // setCompactValuePx().
            const QFont valueFont = Style::monoFont(
                p.font(),
                m_compactValuePx > 0 ? m_compactValuePx : Style::kFontCaption,
                QFont::DemiBold);
            p.setFont(valueFont);
            const QFontMetricsF fm(valueFont);
            p.setPen(hasValue ? col : Instrument::measuredDim());
            p.drawText(QPointF(area.right() - fm.horizontalAdvance(text),
                               textY), text);
        }

        const qreal lineY = area.top() + kTroughHeight - 2.0;
        p.setPen(QPen(QColor(Style::role("border-subtle", Style::kBorderSubtle)),
                      1.2, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(QPointF(area.left(), lineY), QPointF(area.right(), lineY));

        if (hasValue) {
            p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(area.left(), lineY),
                      QPointF(area.left() + area.width() * f, lineY));
        }
        return;
    }

    LinearSpine spine(QRectF(area.left(), area.top(), area.width(),
                             kTroughHeight));

    const double thresholdF = d.threshold.has_value()
                                  ? d.fraction(d.threshold.value())
                                  : -1.0;

    // Ohne Messung nur Mulde und Teilung — kein Verlauf, keine Glut,
    // keine Wertkante. Sie behaupten jede eine Zahl.
    // Die Mulde: flach oder als Roehre. Beide Aufrufe muessen
    // mitgehen — die leere Mulde und die gefuellte sind dasselbe
    // Instrument, und eine Roehre, die erst beim ersten Messwert
    // erscheint, waere ein Fehler, den man nur im Betrieb sieht.
    auto trough = [&] {
        if (m_tube) { Instrument::paintTroughTube(p, spine, thresholdF); }
        else        { Instrument::paintTrough(p, spine, thresholdF); }
    };

    if (!hasValue) {
        trough();
        Instrument::paintTicks(p, spine, d);
    } else {
        if (withGlow) { Instrument::paintGlow(p, spine, col); }
        trough();
        if (m_segmented) {
            // Segmente tragen ihre Kante selbst — eine zusaetzliche
            // Wertkante am Ende saehe aus wie ein halbes Feld.
            Instrument::paintSegments(p, spine, f, col);
            Instrument::paintTicks(p, spine, d);
        } else {
            Instrument::paintFade(p, spine, f, col);
            Instrument::paintTicks(p, spine, d);
            Instrument::paintValueEdge(p, spine, f, col);
        }

        // Die Spitze, wie in der Vorlage: ein heller Strich, der stehen
        // bleibt und langsam zurueckfaellt. Nur wenn sie ueberhaupt
        // vor dem Wert liegt — sonst zeichnete sie eine zweite Kante
        // auf die erste.
        const double pf = d.fraction(m_peak.value());
        if (pf > f + 0.005) { Instrument::paintPeakMark(p, spine, pf); }
    }

    // Die Schwellenmarke steht ÜBER der Mulde hinaus, damit sie auch
    // dann zu sehen ist, wenn der Verlauf noch weit davor endet.
    if (d.threshold.has_value()) {
        QColor dg = Instrument::danger();
        dg.setAlphaF(0.85);
        p.setPen(QPen(dg, 1.0));
        p.drawLine(spine.crossAt(d.fraction(d.threshold.value()), 3.0, 3.0));
    }
}

// From Longpath src/gui/instruments/BarInstrument.cpp:286 [@551576e3]
void BarInstrument::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    paintInto(p, size(), false);
}

// From Longpath src/gui/instruments/BarInstrument.cpp:305 [@551576e3]
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
void BarInstrument::paintInto(QPainter& painter, QSize forSize, bool bare)
{
    // NereusSDR: primaryReading() for readingFor(m_primary), see setReading().
    const ReadingDescriptor* d = primaryReading();
    if (!d || !d->hasScale) { return; }

    // Eine VERBORGENE Fusszeile belegt keinen Platz. Ein verborgenes
    // Widget behaelt seine Hoehe, und die wurde weiter abgezogen.
    // (Das war NICHT die Ursache des Fehlers unten — der Zeichenweg
    // fuer die Einblendung ruft mit bare=true und zog sie ohnehin
    // nicht ab. Die Zeile ist trotzdem falsch gewesen.)
    // ── Zu kurz fuer eine Fusszeile? Dann keine ─────────────────────
    //
    // Hier hing es bis zum 2026-08-23 daran, dass jeder Aufrufer
    // setCompact(true) sagt. Das ist die falsche Stelle: der Betreiber
    // sah die Ueberlappung nacheinander im Frequenz-Widget und im
    // SWR/Leistung-Applet, und ein Prueflauf ueber ALLE Balken fand
    // dann noch zwei weitere — die Instrumente Stehwelle und S-Meter,
    // wenn man sie auf Balkenform stellt.
    //
    // Drei Fundstellen fuer dieselbe Sache heisst: die Regel gehoert
    // nicht zum Aufrufer, sondern hierher. Wer weniger Platz hat, als
    // Mulde und Fusszeile zusammen brauchen, bekommt keine Fusszeile —
    // ohne dass irgendwer daran denken muss.
    //
    // setCompact bleibt als ausdrueckliche Wahl bestehen: es blendet
    // die Zeile auch dann aus, wenn Platz waere.
    const int naturalFooterH = (m_footer && !m_footer->isHidden())
                                   ? m_footer->height() : 0;
    const bool tooShortForFooter =
        forSize.height() < int(kTroughHeight) + naturalFooterH + 18;
    const int footerH = (bare || !m_footer || m_compact
                         || m_footer->isHidden() || tooShortForFooter)
                            ? 0 : naturalFooterH;

    // ── Die Raender richten sich nach der Hoehe ─────────────────────
    //
    // Hier standen 8 oben und 10 unten, fest. Zusammen mit
    // kTroughHeight (20) heisst das: unter 38 Punkten Hoehe faellt die
    // Zeichenflaeche unter die Mulde, und die Zeile darunter kehrt
    // OHNE EINEN STRICH zurueck.
    //
    // Der Betreiber am 2026-08-23: "stehwelle und swr wird nicht im
    // frequenz widget angezeigt." Genau das war es — die Zusatzzeilen
    // dort sind rund 30 Punkte hoch. Lautlos, wie so oft bei dieser
    // Art Fehler: kein Absturz, keine Meldung, nur ein leeres Feld.
    //
    // Die Raender schrumpfen jetzt mit. Sie ganz wegzulassen waere
    // falsch — die Skalenzahlen brauchen unten Platz —, aber sie
    // duerfen nicht mehr kosten, als da ist.
    const double padTop = qMin(8.0, forSize.height() * 0.12);
    const double padBottom = qMin(10.0, forSize.height() * 0.16);
    const QRectF box(10.0, padTop, qMax(0.0, forSize.width() - 24.0),
                     qMax(0.0, forSize.height() - footerH - padTop - padBottom));
    if (box.width() < 40.0 || box.height() < kTroughHeight) { return; }

    QPainter& p = painter;
    p.setRenderHint(QPainter::Antialiasing, true);

    const ReadingDescriptor* d2 = readingFor(m_secondary);
    const bool two = (d2 && d2->hasScale);

    paintOne(p, box, *d, m_value, /*withGlow=*/true, m_hasValue);
    if (two) {
        // Zweite Mulde darunter. Ohne Glut: sie hätte keinen eigenen
        // Ort, an dem sie sitzen könnte, und zwei Glutflächen
        // übereinander heben sich gegenseitig auf.
        const QRectF lower(box.left(),
                           box.top() + kTroughHeight + kScaleHeight + kGap,
                           box.width(), kTroughHeight);
        if (lower.bottom() <= box.bottom() + kTroughHeight) {
            paintOne(p, lower, *d2, m_secondValue, /*withGlow=*/false,
                     m_hasSecond);
        }
    }
}

} // namespace NereusSDR
