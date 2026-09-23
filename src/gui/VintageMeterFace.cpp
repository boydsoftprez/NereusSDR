// =================================================================
// src/gui/VintageMeterFace.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file.  See header.
// =================================================================
#include "gui/VintageMeterFace.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// name, bezel dark/mid/light, face top/bottom, ink/inkSoft, mirror+opa,
// cap/screw light/dark, red/green/yellow, pointer, glare
// Values from TubeMeter themes.cpp.
constexpr VintageFaceTheme kThemes[] = {
    { "Aged Cream",
      0x6E5322, 0xA88434, 0xD9B65A,
      0xEFE3C8, 0xD3C39C,
      0x1E1A16, 0x4A4238,
      0xC9C4B8, 180,
      0x1E1A16, 0xD9B65A, 0x6E5322,
      0xB3261E, 0x3D7A44, 0xD9A400,
      0x1A1512, true },
    { "VU Amber",
      0x141311, 0x2C2A26, 0x3E3B36,
      0xF3DF9C, 0xE2C674,
      0x14110E, 0x4A3F2A,
      0x000000, 0,
      0x14110E, 0x8A8A8A, 0x3A3A3A,
      0xD0341E, 0x3C7A3E, 0xC98F00,
      0x14110E, true },
    { "Collins White",
      0x101010, 0x262626, 0x383838,
      0xF5F2EB, 0xE7E2D6,
      0x101010, 0x454545,
      0xDADADA, 130,
      0x141414, 0xC8C8C8, 0x505050,
      0xB02020, 0x2F7D3A, 0xC99700,
      0x101010, false },
    { "Blackface",
      0x2A2A2A, 0x4A4A4A, 0x6C6C6C,
      0x1F1F1F, 0x0C0C0C,
      0xF0EDE0, 0xB8B4A6,
      0x000000, 0,
      0x3A3A3A, 0xD8D8D8, 0x606060,
      0xE0402A, 0x58C060, 0xFFD60A,
      0xF2EDDC, true },
    { "Carbon",
      0x1A1D21, 0x3A3F46, 0x596069,
      0x1C2026, 0x0E1013,
      0xE8ECF0, 0x8A929C,
      0x000000, 0,
      0x2B3037, 0xC9CED4, 0x5A6068,
      0xFF3B30, 0x34C759, 0xFFD60A,
      0xFF9F0A, false },
    { "Ice",
      0x9AA0A6, 0xC5CAD0, 0xE4E7EA,
      0xF9FAFB, 0xE6EAEF,
      0x1C1F24, 0x6B7280,
      0x000000, 0,
      0x2A2E34, 0xE4E7EA, 0x8B9096,
      0xE5484D, 0x30A46C, 0xF5B301,
      0x0A84FF, false },
};
constexpr int kThemeCount = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));

// Original-design dimensions, in units (arc radius = 250).
constexpr double kArcRadiusU   = 250.0;
constexpr double kNeedleLenU   = 262.0;
constexpr double kNeedleTailU  = 18.0;
constexpr double kArcWidthU    = 3.0;
constexpr double kBandWidthU   = 9.0;
constexpr double kMajorLenU    = 22.0;
constexpr double kMajorWidthU  = 3.0;
constexpr double kMinorLenU    = 12.0;
constexpr double kMinorWidthU  = 2.0;
constexpr double kLabelPadU    = 8.0;
constexpr double kMirrorOuterU = 6.0;    // mirror strip spans arc .. arc + 6
constexpr double kMirrorLineU  = 12.0;   // hairline outside the mirror strip
constexpr double kCapRadiusU   = 26.0;
constexpr double kScrewRadiusU = 11.0;

QColor rgb(QRgb c, int alpha = 255)
{
    QColor q(c);
    q.setAlpha(alpha);
    return q;
}

// Qt's drawArc wants 1/16 degrees, counter-clockwise from 3 o'clock.
void drawScaleArc(QPainter& p, const VintageMeterFace::Geometry& g, double r,
                  float fracFrom, float fracTo)
{
    const double a0 = VintageMeterFace::angleDeg(fracFrom);
    const double a1 = VintageMeterFace::angleDeg(fracTo);
    const QRectF box(g.pivot.x() - r, g.pivot.y() - r, 2.0 * r, 2.0 * r);
    p.drawArc(box, qRound((90.0 - a1) * 16.0), qRound((a1 - a0) * 16.0));
}

void drawCentered(QPainter& p, const QPointF& c, const QString& text)
{
    const QFontMetricsF fm(p.font());
    const double tw = fm.horizontalAdvance(text) + 4.0;
    const double th = fm.height();
    p.drawText(QRectF(c.x() - tw / 2.0, c.y() - th / 2.0, tw, th), Qt::AlignCenter, text);
}

QFont fitted(QFont f, const QString& text, double maxWidth, int minPx)
{
    QFontMetricsF fm(f);
    while (f.pixelSize() > minPx && fm.horizontalAdvance(text) > maxWidth) {
        f.setPixelSize(f.pixelSize() - 1);
        fm = QFontMetricsF(f);
    }
    return f;
}

} // namespace

// ---------------------------------------------------------------------------
int VintageMeterFace::themeCount() { return kThemeCount; }

const VintageFaceTheme& VintageMeterFace::theme(int index)
{
    return kThemes[std::clamp(index, 0, kThemeCount - 1)];
}

VintageMeterFace::Geometry VintageMeterFace::geometryFor(const QRectF& widgetRect)
{
    Geometry g;
    const QRectF r = widgetRect.adjusted(1, 1, -1, -1);
    const double bezelW = std::clamp(r.height() * 0.06, 5.0, 14.0);
    g.face = r.adjusted(bezelW, bezelW, -bezelW, -bezelW);

    const double H = g.face.height();
    const double W = g.face.width();
    // Height normally limits the arc; on a narrow widget the ends of the
    // mirror hairline must still land on the card.
    const double sinHalf = std::sin(qDegreesToRadians(kSweepDeg / 2.0));
    const double outerK  = (kArcRadiusU + kMirrorLineU + 4.0) / kArcRadiusU;
    const double byH = 0.72 * H;
    const double byW = (0.5 * W - 4.0) / (sinHalf * outerK);
    g.radius = std::max(20.0, std::min(byH, byW));
    g.unit   = g.radius / kArcRadiusU;
    g.pivot  = QPointF(g.face.center().x(), g.face.top() + 0.915 * H);
    return g;
}

double VintageMeterFace::angleDeg(float frac)
{
    return -kSweepDeg / 2.0 + static_cast<double>(std::clamp(frac, 0.0f, 1.0f)) * kSweepDeg;
}

QPointF VintageMeterFace::pointAt(const Geometry& g, float frac, double r)
{
    const double a = qDegreesToRadians(angleDeg(frac));
    return QPointF(g.pivot.x() + r * std::sin(a), g.pivot.y() - r * std::cos(a));
}

QFont VintageMeterFace::faceFont(double pixelSize, bool bold, double letterSpacingPct)
{
    // Resolve once: asking Qt for a family that is not installed triggers a
    // slow font-alias scan (and a warning) on macOS.
    static const QStringList families = [] {
        QStringList have;
        for (const char* name : { "Jost", "Futura", "Avenir Next", "Century Gothic",
                                  "Gill Sans", "Helvetica Neue", "Arial" }) {
            const QString fam = QString::fromLatin1(name);
            if (QFontDatabase::hasFamily(fam)) {
                have << fam;
            }
        }
        return have;
    }();

    QFont f;
    if (!families.isEmpty()) {
        f.setFamilies(families);
    }
    f.setPixelSize(std::max(6, qRound(pixelSize)));
    f.setWeight(bold ? QFont::DemiBold : QFont::Medium);
    if (letterSpacingPct != 100.0) {
        f.setLetterSpacing(QFont::PercentageSpacing, letterSpacingPct);
    }
    return f;
}

// ---------------------------------------------------------------------------
void VintageMeterFace::paintStatic(QPainter& p, const QRectF& widgetRect, const QColor& surround,
                                   const VintageFaceTheme& t, const VintageScale& scale,
                                   const QString& title, const QString& legend)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.fillRect(widgetRect, surround);

    const Geometry g = geometryFor(widgetRect);
    const double u = g.unit;
    const double H = g.face.height();
    const double W = g.face.width();

    // ── Bezel: three rings give a turned bevel (dark 16 / mid 6 / light 4 /
    //    dark 3 in the original) ─────────────────────────────────────────────
    const QRectF outer = widgetRect.adjusted(1, 1, -1, -1);
    const double bezelW = g.face.left() - outer.left();
    const double faceCorner = std::max(4.0, bezelW * 1.1);
    auto ring = [&](double inset, const QColor& c) {
        QPainterPath path;
        const double corner = std::max(faceCorner, faceCorner + bezelW - inset);
        path.addRoundedRect(outer.adjusted(inset, inset, -inset, -inset), corner, corner);
        p.fillPath(path, c);
    };
    ring(0.0,                     rgb(t.bezelDark));
    ring(bezelW * 16.0 / 29.0,    rgb(t.bezelMid));
    ring(bezelW * 22.0 / 29.0,    rgb(t.bezelLight));
    ring(bezelW * 26.0 / 29.0,    rgb(t.bezelDark));

    // ── Card ─────────────────────────────────────────────────────────────────
    QPainterPath facePath;
    facePath.addRoundedRect(g.face, faceCorner, faceCorner);
    {
        QLinearGradient grad(g.face.topLeft(), g.face.bottomLeft());
        grad.setColorAt(0.0, rgb(t.faceTop));
        grad.setColorAt(1.0, rgb(t.faceBottom));
        p.fillPath(facePath, grad);
    }

    p.setClipPath(facePath);

    // The bezel stands proud of the card: soft shadow around the inside edge.
    {
        p.setBrush(Qt::NoBrush);
        const double depth = std::max(6.0, H * 0.09);
        // Many faint strokes of shrinking width: fine enough steps that the
        // falloff reads as continuous.
        constexpr int kSteps = 14;
        for (int i = 0; i < kSteps; ++i) {
            p.setPen(QPen(QColor(0, 0, 0, 4), depth * (kSteps - i) / kSteps * 2.0));
            p.drawPath(facePath);
        }
    }

    // ── Anti-parallax mirror strip ───────────────────────────────────────────
    if (t.mirrorOpa > 0) {
        const float over = static_cast<float>(2.0 / kSweepDeg);
        const double stripW = kMirrorOuterU * u;
        p.setPen(QPen(rgb(t.mirror, t.mirrorOpa), stripW, Qt::SolidLine, Qt::FlatCap));
        // drawScaleArc clamps to 0..1, so widen by hand.
        const double a0 = -kSweepDeg / 2.0 - over * kSweepDeg;
        const double a1 =  kSweepDeg / 2.0 + over * kSweepDeg;
        auto arcAt = [&](double r) {
            const QRectF box(g.pivot.x() - r, g.pivot.y() - r, 2.0 * r, 2.0 * r);
            p.drawArc(box, qRound((90.0 - a1) * 16.0), qRound((a1 - a0) * 16.0));
        };
        arcAt(g.radius + stripW / 2.0);
        p.setPen(QPen(rgb(t.inkSoft, 153), std::max(0.7, 1.0 * u), Qt::SolidLine, Qt::FlatCap));
        arcAt(g.radius + kMirrorLineU * u);
    }

    // ── Scale: coloured bands, arc, ticks, numerals ──────────────────────────
    auto bandColor = [&](VintageScaleBand::Color c) {
        switch (c) {
        case VintageScaleBand::Color::Red:    return rgb(t.red);
        case VintageScaleBand::Color::Green:  return rgb(t.green);
        case VintageScaleBand::Color::Yellow: return rgb(t.yellow);
        }
        return rgb(t.ink);
    };
    // A tick or numeral inside a band takes the band's colour.
    auto colorAt = [&](float frac, const QColor& fallback) {
        for (const VintageScaleBand& b : scale.bands) {
            if (frac >= b.from - 0.0005f && frac <= b.to + 0.0005f) {
                return bandColor(b.color);
            }
        }
        return fallback;
    };

    const double arcW  = std::max(1.3, kArcWidthU * u);
    const double bandW = std::max(3.0, kBandWidthU * u);
    p.setPen(QPen(rgb(t.ink), arcW, Qt::SolidLine, Qt::FlatCap));
    drawScaleArc(p, g, g.radius - arcW / 2.0, 0.0f, 1.0f);
    for (const VintageScaleBand& b : scale.bands) {
        p.setPen(QPen(bandColor(b.color), bandW, Qt::SolidLine, Qt::FlatCap));
        drawScaleArc(p, g, g.radius - bandW / 2.0, b.from, b.to);
    }

    const double majorLen = kMajorLenU * u;
    const double minorLen = kMinorLenU * u;
    const double labelPx  = std::max(9.0, 30.0 * u);
    p.setFont(faceFont(labelPx, false));
    const QFontMetricsF lfm(p.font());
    const double labelR = g.radius - majorLen - kLabelPadU * u - lfm.capHeight() * 0.75;
    for (const VintageScaleTick& tk : scale.ticks) {
        const QColor c = colorAt(tk.frac, rgb(tk.major ? t.ink : t.inkSoft));
        const double len = tk.major ? majorLen : minorLen;
        const double wid = tk.major ? std::max(1.3, kMajorWidthU * u)
                                    : std::max(0.9, kMinorWidthU * u);
        p.setPen(QPen(c, wid, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(pointAt(g, tk.frac, g.radius), pointAt(g, tk.frac, g.radius - len));
        if (!tk.label.isEmpty()) {
            p.setPen(colorAt(tk.frac, rgb(t.ink)));
            drawCentered(p, pointAt(g, tk.frac, labelR), tk.label);
        }
    }

    // ── Lettering ────────────────────────────────────────────────────────────
    if (!title.isEmpty()) {
        p.setPen(rgb(t.ink));
        p.setFont(fitted(faceFont(std::max(9.0, H * 0.105), true, 114.0), title, W * 0.9, 7));
        const double arcTop = g.pivot.y() - g.radius - kMirrorLineU * u;
        drawCentered(p, QPointF(g.face.center().x(), (g.face.top() + arcTop) / 2.0 + 1.0), title);
    }
    if (!legend.isEmpty()) {
        p.setPen(rgb(t.ink));
        p.setFont(fitted(faceFont(std::max(8.0, 25.0 * u), false, 112.0), legend,
                         g.radius * 1.1, 6));
        drawCentered(p, QPointF(g.pivot.x(), g.pivot.y() - 0.50 * g.radius), legend);
    }

    // ── Glass highlight ──────────────────────────────────────────────────────
    if (t.glare) {
        QLinearGradient grad(g.face.topLeft(),
                             QPointF(g.face.left() + W * 0.25, g.face.top() + H * 0.55));
        grad.setColorAt(0.0, QColor(255, 255, 255, 60));
        grad.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillPath(facePath, grad);
    }

    p.restore();
}

// ---------------------------------------------------------------------------
void VintageMeterFace::paintPointer(QPainter& p, const Geometry& g, const VintageFaceTheme& t,
                                    float frac)
{
    const double u    = g.unit;
    const double len  = kNeedleLenU * u;
    const double tail = kNeedleTailU * u;

    // Lance: a fine shaft to 40 % of the length carrying a spear that opens
    // from 28 %, is widest at 56 %, and runs out to the tip.  Drawn along +X
    // and rotated about the hub, as the original does with its bitmap.
    const double shaftHalf = std::max(0.6, 1.0 * u);
    const double spearHalf = std::max(2.2, 6.0 * u);
    QPainterPath lance;
    lance.setFillRule(Qt::WindingFill);
    lance.addRect(QRectF(-tail, -shaftHalf, tail + 0.40 * len, 2.0 * shaftHalf));
    QPainterPath spear;
    spear.moveTo(0.28 * len, 0.0);
    spear.lineTo(0.56 * len, -spearHalf);
    spear.lineTo(len, 0.0);
    spear.lineTo(0.56 * len, spearHalf);
    spear.closeSubpath();
    lance.addPath(spear);

    const double rot = angleDeg(frac) - 90.0;   // +X -> straight up at angle 0

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(g.face);
    p.setPen(Qt::NoPen);

    p.save();
    p.translate(g.pivot + QPointF(std::max(1.2, 3.0 * u), std::max(2.0, 5.0 * u)));
    p.rotate(rot);
    p.fillPath(lance, QColor(0, 0, 0, 51));     // LV_OPA_20
    p.restore();

    p.translate(g.pivot);
    p.rotate(rot);
    p.fillPath(lance, rgb(t.pointer));
    p.restore();
}

void VintageMeterFace::paintHub(QPainter& p, const Geometry& g, const VintageFaceTheme& t)
{
    const double capR   = std::max(6.0, kCapRadiusU * g.unit);
    const double screwR = std::max(2.6, kScrewRadiusU * g.unit);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(g.face);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 45));
    p.drawEllipse(g.pivot + QPointF(0.0, capR * 0.12), capR * 1.04, capR * 1.04);
    p.setBrush(rgb(t.cap));
    p.drawEllipse(g.pivot, capR, capR);

    QLinearGradient grad(QPointF(g.pivot.x(), g.pivot.y() - screwR),
                         QPointF(g.pivot.x(), g.pivot.y() + screwR));
    grad.setColorAt(0.0, rgb(t.screwLight));
    grad.setColorAt(1.0, rgb(t.screwDark));
    p.setBrush(grad);
    p.drawEllipse(g.pivot, screwR, screwR);
    p.restore();
}

} // namespace NereusSDR
