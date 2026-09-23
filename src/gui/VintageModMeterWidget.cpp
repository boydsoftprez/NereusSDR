// =================================================================
// src/gui/VintageModMeterWidget.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file.  See header.
// =================================================================
#include "gui/VintageModMeterWidget.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {
// Needle sweep: +/- kSweepDeg around straight-up.
constexpr double kSweepDeg = 40.0;
// Peak-reading ballistics: fast rise, slower fall.
constexpr double kAttackMs = 60.0;
constexpr double kDecayMs  = 350.0;

// VU decibel marks printed in red beneath the percent arc.
struct DbMark { double dB; const char* text; };
constexpr DbMark kDbMarks[] = {
    {-20.0, "-20"}, {-10.0, "-10"}, {-7.0, "-7"}, {-5.0, "-5"}, {-3.0, "-3"},
    {-2.0, "-2"}, {-1.0, "-1"}, {0.0, "0"}, {1.0, "+1"}, {2.0, "+2"}, {3.0, "+3"},
};

double pctFromDb(double dB) { return 100.0 * std::pow(10.0, dB / 20.0); }
}

// ---------------------------------------------------------------------------
VintageModMeterWidget::VintageModMeterWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_clock.start();
}

void VintageModMeterWidget::setCaption(const QString& text) { m_caption = text; update(); }
void VintageModMeterWidget::setRange(double minPct, double maxPct)
{
    m_min = minPct; m_max = std::max(minPct + 1.0, maxPct); update();
}
void VintageModMeterWidget::setRedZoneStart(double pct) { m_redStart = pct; update(); }
void VintageModMeterWidget::setValue(double pct)
{
    m_target = std::clamp(pct, m_min, m_max);
    advanceNeedle();
    update();
}
void VintageModMeterWidget::setPeakValue(double pct) { m_peak = pct; update(); }
void VintageModMeterWidget::setLit(bool lit) { if (m_lit != lit) { m_lit = lit; update(); } }

double VintageModMeterWidget::angleFor(double pct) const
{
    const double t = (std::clamp(pct, m_min, m_max) - m_min) / (m_max - m_min);
    return -kSweepDeg + t * 2.0 * kSweepDeg;
}

void VintageModMeterWidget::advanceNeedle()
{
    const qint64 now = m_clock.elapsed();
    const double dt = std::max(0.0, static_cast<double>(now - m_lastMs));
    m_lastMs = now;
    const double tau = (m_target > m_needle) ? kAttackMs : kDecayMs;
    const double a = 1.0 - std::exp(-dt / tau);
    m_needle += (m_target - m_needle) * a;
}

void VintageModMeterWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);

    // ── Bezel ────────────────────────────────────────────────────────────
    {
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0.0, QColor(0x5a, 0x5a, 0x5c));
        g.setColorAt(0.5, QColor(0x2e, 0x2e, 0x30));
        g.setColorAt(1.0, QColor(0x4a, 0x4a, 0x4c));
        QPainterPath bez;
        bez.addRoundedRect(r, 9, 9);
        p.fillPath(bez, g);
        p.setPen(QPen(QColor(0x1a, 0x1a, 0x1c), 1.0));
        p.drawPath(bez);
    }

    // ── Face ─────────────────────────────────────────────────────────────
    const double bezelW = std::clamp(r.height() * 0.05, 3.0, 8.0);
    const QRectF face = r.adjusted(bezelW, bezelW, -bezelW, -bezelW);
    const double H = face.height();
    const double W = face.width();
    QPainterPath facePath;
    facePath.addRoundedRect(face, 7, 7);
    {
        QLinearGradient g(face.topLeft(), face.bottomLeft());
        if (m_lit) {
            g.setColorAt(0.0, QColor(0xe9, 0xd6, 0xa6));
            g.setColorAt(0.6, QColor(0xe2, 0xc3, 0x84));
            g.setColorAt(1.0, QColor(0xd8, 0xa9, 0x5a));
        } else {
            g.setColorAt(0.0, QColor(0x8a, 0x80, 0x66));
            g.setColorAt(1.0, QColor(0x6a, 0x5e, 0x44));
        }
        p.fillPath(facePath, g);
        if (m_lit) {
            // Two warm lamps glowing up from the bottom corners.
            for (double fx : {0.18, 0.82}) {
                QRadialGradient rg(QPointF(face.left() + W * fx, face.bottom()), H * 0.75);
                rg.setColorAt(0.0, QColor(0xff, 0xe8, 0xb0, 150));
                rg.setColorAt(1.0, QColor(0xff, 0xe8, 0xb0, 0));
                p.fillPath(facePath, rg);
            }
        }
        p.setPen(QPen(QColor(0x2a, 0x22, 0x14), 1.0));
        p.drawPath(facePath);
    }

    // ── Layout (fractions of face height) ───────────────────────────────
    //   0.02..0.16  caption
    //   0.40        arc apex (angle 0); arc bows upward, ends lower
    //   0.62..0.70  "DECIBELS"
    //   0.80..0.96  "PERCENTAGE MODULATION"
    // Radius limited by both height and width so the arc ends stay on the face.
    const double sinSweep = std::sin(qDegreesToRadians(kSweepDeg));
    const double radius = std::min(0.95 * H, (0.5 * W - H * 0.20) / sinSweep);
    const QPointF pivot(face.center().x(), face.top() + H * 0.40 + radius);
    auto pointAt = [&](double pct, double rad) {
        const double a = qDegreesToRadians(angleFor(pct));
        return QPointF(pivot.x() + rad * std::sin(a), pivot.y() - rad * std::cos(a));
    };
    auto fitFont = [&](double px, bool bold, const QString& text, double maxWidth) {
        QFont f = font();
        f.setBold(bold);
        f.setPixelSize(std::max(5, static_cast<int>(px)));
        QFontMetricsF fm(f);
        while (f.pixelSize() > 5 && fm.horizontalAdvance(text) > maxWidth) {
            f.setPixelSize(f.pixelSize() - 1);
            fm = QFontMetricsF(f);
        }
        return f;
    };
    auto drawCentered = [&](const QPointF& c, const QString& text) {
        const QFontMetricsF fm(p.font());
        const double tw = fm.horizontalAdvance(text) + 2.0;
        const double th = fm.height();
        p.drawText(QRectF(c.x() - tw / 2.0, c.y() - th / 2.0, tw, th), Qt::AlignCenter, text);
    };

    p.save();
    p.setClipPath(facePath);

    const QColor ink = m_lit ? QColor(0x1a, 0x14, 0x0a) : QColor(0x33, 0x2c, 0x1c);
    const QColor red = m_lit ? QColor(0xc8, 0x22, 0x1e) : QColor(0x7a, 0x22, 0x1e);
    const QRectF arcRect(pivot.x() - radius, pivot.y() - radius, 2 * radius, 2 * radius);

    // ── Red zone arc ─────────────────────────────────────────────────────
    if (m_redStart < m_max) {
        const double a0 = 90.0 - angleFor(m_max);      // Qt: CCW degrees from 3 o'clock
        const double a1 = 90.0 - angleFor(m_redStart);
        p.setPen(QPen(red, std::max(2.5, H * 0.04), Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arcRect, static_cast<int>(a0 * 16), static_cast<int>((a1 - a0) * 16));
    }

    // ── Main arc + percent ticks ─────────────────────────────────────────
    {
        const double a0 = 90.0 - angleFor(m_max);
        const double a1 = 90.0 - angleFor(m_min);
        p.setPen(QPen(ink, 1.2));
        p.drawArc(arcRect, static_cast<int>(a0 * 16), static_cast<int>((a1 - a0) * 16));

        p.setFont(fitFont(H * 0.085, false, QStringLiteral("140"), W));
        const bool narrowFace = W < 170.0;
        const double majorLen = H * 0.065;
        const double minorLen = majorLen * 0.5;
        for (double pct = m_min; pct <= m_max + 0.01; pct += 10.0) {
            const bool major = std::fmod(pct, 20.0) < 0.01;
            const double len = major ? majorLen : minorLen;
            const QColor c = (pct >= m_redStart + 0.01) ? red : ink;
            p.setPen(QPen(c, major ? 1.2 : 1.0));
            p.drawLine(pointAt(pct, radius), pointAt(pct, radius + len));
            // A ~110 px face cannot fit eight numerals; keep every tick but
            // print 0 / 40 / 80 / 140 only (the red zone marks 100).
            const bool printIt = major && (!narrowFace ||
                pct == 0.0 || pct == 40.0 || pct == 80.0 || pct == m_max);
            if (printIt) {
                p.setPen(c);
                drawCentered(pointAt(pct, radius + len + H * 0.07),
                             QString::number(static_cast<int>(pct)));
            }
        }
    }

    // ── VU decibel row (red, inside the arc) ─────────────────────────────
    {
        p.setFont(fitFont(H * 0.062, false, QStringLiteral("-20"), W));
        p.setPen(QPen(red, 1.0));
        const double rIn = radius - H * 0.05;
        const bool narrow = W < 170.0;
        for (const DbMark& m : kDbMarks) {
            const double pct = pctFromDb(m.dB);
            if (pct < m_min || pct > m_max) { continue; }
            p.drawLine(pointAt(pct, radius), pointAt(pct, rIn));
            // On a small face the -7/-2/+2 labels collide with their
            // neighbours; keep their ticks, drop the numerals.
            if (narrow && !(m.dB == -20.0 || m.dB == -10.0 || m.dB == -3.0
                            || m.dB == 0.0 || m.dB == 3.0)) { continue; }
            drawCentered(pointAt(pct, rIn - H * 0.06), QLatin1String(m.text));
        }
    }

    // ── Captions ─────────────────────────────────────────────────────────
    {
        p.setPen(ink);
        p.setFont(fitFont(H * 0.10, true, m_caption, W * 0.90));
        drawCentered(QPointF(face.center().x(), face.top() + H * 0.10), m_caption);

        p.setFont(fitFont(H * 0.06, false, QStringLiteral("DECIBELS"), W * 0.5));
        drawCentered(QPointF(face.center().x(), face.top() + H * 0.66), QStringLiteral("DECIBELS"));

        const QString bottom = QStringLiteral("PERCENTAGE MODULATION");
        p.setFont(fitFont(H * 0.085, true, bottom, W * 0.92));
        drawCentered(QPointF(face.center().x(), face.top() + H * 0.87), bottom);
    }

    // ── Peak-hold dot ────────────────────────────────────────────────────
    if (m_peak >= m_min) {
        const QPointF dp = pointAt(std::min(m_peak, m_max), radius + H * 0.03);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x1e, 0x50, 0xff));
        p.drawEllipse(dp, H * 0.026, H * 0.026);
    }

    // ── Needle ───────────────────────────────────────────────────────────
    {
        advanceNeedle();
        const double a = qDegreesToRadians(angleFor(m_needle));
        const QPointF tip(pivot.x() + (radius + H * 0.075) * std::sin(a),
                          pivot.y() - (radius + H * 0.075) * std::cos(a));
        // Blade runs from the face bottom edge to just past the arc.
        const double baseRad = std::max(0.0, pivot.y() - face.bottom()) / std::max(0.3, std::cos(a));
        const QPointF base(pivot.x() + baseRad * std::sin(a),
                           pivot.y() - baseRad * std::cos(a));
        p.setPen(QPen(QColor(0, 0, 0, 60), 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(base + QPointF(1.5, 1.5), tip + QPointF(1.5, 1.5));
        p.setPen(QPen(QColor(0x10, 0x0c, 0x08), std::max(1.2, H * 0.014), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(base, tip);
        // Teardrop sitting on the arc, bulb up, tapering back toward the
        // pivot, like the original meter's needle.
        const QPointF w = pointAt(m_needle, radius - H * 0.015);
        const double tw = H * 0.028, tl = H * 0.09;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x10, 0x0c, 0x08));
        QPainterPath tear;
        tear.moveTo(w + QPointF(-tw * std::cos(a), -tw * std::sin(a)));
        tear.lineTo(w + QPointF(tw * std::cos(a), tw * std::sin(a)));
        tear.lineTo(w - QPointF(tl * std::sin(a), -tl * std::cos(a)));
        tear.closeSubpath();
        p.drawPath(tear);
        p.drawEllipse(w, tw, tw);
    }

    // Glass highlight.
    {
        QLinearGradient g(face.topLeft(), QPointF(face.left(), face.top() + H * 0.45));
        g.setColorAt(0.0, QColor(255, 255, 255, 46));
        g.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillPath(facePath, g);
    }
    p.restore();

    // Keep the needle moving toward its target between value updates.
    if (std::fabs(m_needle - m_target) > 0.05) {
        update();
    }
}

// ---------------------------------------------------------------------------
AsymmetryBarWidget::AsymmetryBarWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(30);
}

void AsymmetryBarWidget::setValue(double pctPoints)
{
    m_value = std::clamp(pctPoints, -m_range, m_range);
    update();
}

void AsymmetryBarWidget::setRange(double pctPoints)
{
    m_range = std::max(1.0, pctPoints);
    update();
}

void AsymmetryBarWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect().adjusted(1, 1, -1, -1);
    QPainterPath bez;
    bez.addRoundedRect(r, 5, 5);
    p.fillPath(bez, QColor(0x2a, 0x2a, 0x2c));

    QFont f = font();
    f.setPixelSize(std::clamp(static_cast<int>(r.width() * 0.34), 6, 11));
    f.setBold(true);
    p.setFont(f);
    const double labelH = r.height() * 0.11;
    p.setPen(QColor(0x38, 0xd0, 0x60));
    p.drawText(QRectF(r.left(), r.top(), r.width(), labelH), Qt::AlignCenter, QStringLiteral("Pos"));
    p.setPen(QColor(0xff, 0x60, 0x60));
    p.drawText(QRectF(r.left(), r.bottom() - labelH, r.width(), labelH), Qt::AlignCenter, QStringLiteral("Neg"));

    const QRectF bar(r.left() + 9, r.top() + labelH + 2, r.width() - 18, r.height() - 2 * labelH - 4);
    p.fillRect(bar, QColor(0xf2, 0xe6, 0xc4));
    p.setPen(QPen(QColor(0x80, 0x70, 0x50), 1.0));
    p.drawRect(bar);

    const double mid = bar.center().y();
    // Scale ticks at +/- range/2 and 0.
    p.setPen(QPen(QColor(0x50, 0x40, 0x30), 1.0));
    for (double k : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
        const double y = mid - k * bar.height() / 2.0;
        p.drawLine(QPointF(bar.left() - 3, y), QPointF(bar.right() + 3, y));
    }
    // Segmented fill from centre.
    const double h = (m_value / m_range) * bar.height() / 2.0;
    QRectF fill = (h >= 0)
        ? QRectF(bar.left() + 1, mid - h, bar.width() - 2, h)
        : QRectF(bar.left() + 1, mid, bar.width() - 2, -h);
    p.fillRect(fill, (h >= 0) ? QColor(0x30, 0xc0, 0x50) : QColor(0xe0, 0x40, 0x40));
    p.setPen(QPen(QColor(0xf2, 0xe6, 0xc4), 1.0));
    for (double y = bar.top() + 3; y < bar.bottom(); y += 3.0) {
        p.drawLine(QPointF(bar.left() + 1, y), QPointF(bar.right() - 1, y));
    }
}

} // namespace NereusSDR
