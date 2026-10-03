// =================================================================
// src/gui/VintageMeterFace.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// QPainter rendering of a vintage moving-coil panel meter: turned
// three-ring bezel, graduated card face, anti-parallax mirror strip,
// ink scale with coloured bands, a lance pointer with a drop shadow,
// and a pivot cap with a brass screw.
//
// The design (geometry, proportions, the six face themes and the lance
// pointer) is carried over from Lee's TubeMeter ESP32 / LVGL project
// (TubeMeter/design_vintage.cpp, themes.cpp, ui_common.cpp), re-drawn
// here for a rectangular Qt widget.  All dimensions are expressed in
// "units" of the original 480 px round display (arc radius = 250 units)
// so the proportions stay those of the original at any widget size.
//
// The class is stateless: callers cache paintStatic() into a pixmap and
// draw the live parts (pointer, hub) on top each frame, mirroring the
// original's build_static / build_live split.
//
// Modification history (NereusSDR):
//   2026-09-18 — Created for the vintage S-Meter face (Lee, AI-assisted
//                 via Anthropic Claude Code).
// =================================================================
#pragma once

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace NereusSDR {

// One face theme.  Field-for-field the TubeMeter FaceTheme.
struct VintageFaceTheme {
    const char* name;
    QRgb bezelDark, bezelMid, bezelLight;   // the three bezel rings
    QRgb faceTop, faceBottom;               // vertical gradient on the card
    QRgb ink, inkSoft;                      // scale / legend text
    QRgb mirror; int mirrorOpa;             // anti-parallax strip, opa 0 = none
    QRgb cap, screwLight, screwDark;        // pivot hardware
    QRgb red, green, yellow;                // band colours
    QRgb pointer;                           // pointer colour
    bool glare;                             // glass reflection highlight
};

struct VintageScaleTick {
    float   frac;       // 0 = left end of the scale, 1 = right end
    bool    major;
    QString label;      // empty = no numeral
};

struct VintageScaleBand {
    enum class Color { Red, Green, Yellow };
    float from;
    float to;
    Color color;
};

struct VintageScale {
    QVector<VintageScaleTick> ticks;
    QVector<VintageScaleBand> bands;
};

class VintageMeterFace {
public:
    // Scale sweep, degrees, centred on straight-up.
    static constexpr double kSweepDeg = 110.0;

    struct Geometry {
        QRectF  face;       // the card, inside the bezel
        QPointF pivot;
        double  radius;     // scale arc radius, px
        double  unit;       // px per original-design unit (radius / 250)
    };

    static int themeCount();
    static const VintageFaceTheme& theme(int index);   // index is clamped

    static Geometry geometryFor(const QRectF& widgetRect);

    // Needle angle in degrees from straight-up (negative = left).
    static double angleDeg(float frac);
    // Point on the circle of radius `r` about the pivot at scale fraction `frac`.
    static QPointF pointAt(const Geometry& g, float frac, double r);

    // Futura-style face lettering (Jost if installed, else the nearest
    // geometric sans the platform has).  letterSpacingPct 100 = normal.
    static QFont faceFont(double pixelSize, bool bold, double letterSpacingPct = 100.0);

    // Everything that does not move: surround, bezel, card, mirror strip,
    // scale, title above the scale, legend below it, glass highlight.
    // Fills `widgetRect` completely (safe under WA_OpaquePaintEvent).
    static void paintStatic(QPainter& p, const QRectF& widgetRect, const QColor& surround,
                            const VintageFaceTheme& t, const VintageScale& scale,
                            const QString& title, const QString& legend);

    // Lance pointer with its shadow, at scale fraction `frac`.
    static void paintPointer(QPainter& p, const Geometry& g, const VintageFaceTheme& t,
                             float frac);

    // Pivot cap and brass screw; drawn over the pointer.
    static void paintHub(QPainter& p, const Geometry& g, const VintageFaceTheme& t);
};

} // namespace NereusSDR
