#pragma once

// =================================================================
// src/gui/meters/MeterItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include <QObject>
#include <QVector>
#include <QRect>
#include <QColor>
#include <QImage>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QUuid>
#include <limits>

class QPainter;
class QMouseEvent;
class QWheelEvent;

namespace NereusSDR {

// Free function — maps a MeterBinding::* constant to the centered title
// string ScaleItem renders when setShowType(true). Ported verbatim from
// Thetis MeterManager.cs:2258-2318 ReadingName(). Returns an empty
// QString for unmapped binding IDs (ScaleItem skips empty titles).
QString readingName(int bindingId);


class MeterItem : public QObject {
    Q_OBJECT

public:
    enum class Layer {
        Background,      // Pipeline 1
        Geometry,        // Pipeline 2
        OverlayStatic,   // Pipeline 3 (cached)
        OverlayDynamic   // Pipeline 3 (per-update)
    };

    explicit MeterItem(QObject* parent = nullptr) : QObject(parent) {}
    ~MeterItem() override = default;

    // Position (normalized 0-1)
    float x() const { return m_x; }
    float y() const { return m_y; }
    float itemWidth() const { return m_w; }
    float itemHeight() const { return m_h; }
    void setRect(float x, float y, float w, float h) {
        m_x = x; m_y = y; m_w = w; m_h = h;
    }

    int bindingId() const { return m_bindingId; }
    void setBindingId(int id) { m_bindingId = id; }
    double value() const { return m_value; }
    virtual void setValue(double v) { m_value = v; }

    int zOrder() const { return m_zOrder; }
    void setZOrder(int z) { m_zOrder = z; }

    // Visibility filter (from Thetis clsMeterItem, MeterManager.cs:6741-6744 /
    // 6937-7096). The container's paint loop applies the rule from
    // MeterManager.cs:31366-31368 — see MeterWidget::shouldRender().
    // displayGroup == 0 means "always visible" (matches Thetis default).
    bool onlyWhenRx() const { return m_onlyWhenRx; }
    void setOnlyWhenRx(bool v) { m_onlyWhenRx = v; }
    bool onlyWhenTx() const { return m_onlyWhenTx; }
    void setOnlyWhenTx(bool v) { m_onlyWhenTx = v; }
    int  displayGroup() const { return m_displayGroup; }
    void setDisplayGroup(int g) { m_displayGroup = g; }

    // Phase 3G-6 block 5 — MMIO binding. Null guid + empty (or
    // "--DEFAULT--") variable name means "no MMIO binding, fall back
    // to the WDSP-backed bindingId()". Set by the per-item editor's
    // "Variable…" picker popup. Serialization is deliberately out of
    // scope for block 5 — set lives only in the in-memory item.
    QUuid   mmioGuid() const { return m_mmioGuid; }
    QString mmioVariable() const { return m_mmioVariable; }
    bool    hasMmioBinding() const {
        return !m_mmioGuid.isNull()
            && !m_mmioVariable.isEmpty()
            && m_mmioVariable != QLatin1String("--DEFAULT--");
    }
    void setMmioBinding(const QUuid& guid, const QString& variableName) {
        m_mmioGuid = guid;
        m_mmioVariable = variableName;
    }
    void clearMmioBinding() {
        m_mmioGuid = QUuid();
        m_mmioVariable.clear();
    }

    virtual Layer renderLayer() const = 0;
    virtual void paint(QPainter& p, int widgetW, int widgetH) = 0;
    virtual void emitVertices(QVector<float>& verts, int widgetW, int widgetH) {
        Q_UNUSED(verts); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    }

    // Multi-layer support: items that participate in multiple pipelines
    // override participatesIn() to return true for each layer they render to,
    // and override paintForLayer() to dispatch per-layer painting.
    // Default: single-layer (backward-compatible with existing items).
    virtual bool participatesIn(Layer layer) const { return layer == renderLayer(); }
    virtual void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) {
        Q_UNUSED(layer);
        paint(p, widgetW, widgetH);
    }

    virtual QString serialize() const;
    virtual bool deserialize(const QString& data);

    // --- Mouse interaction (Phase 3G-5) ---
    virtual bool hitTest(const QPointF& pos, int widgetW, int widgetH) const;
    virtual bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH);
    virtual bool handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH);
    virtual bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH);
    virtual bool handleWheel(QWheelEvent* event, int widgetW, int widgetH);

public:
    // --- Stacked-row metadata (runtime only, not serialized) ---
    //
    // Thetis-parity stack model with a NereusSDR pixel floor.
    // When m_stackSlot >= 0 the item is part of a bar-row stack.
    // At every MeterWidget reflow the outer m_y/m_h are recomputed
    // from:
    //
    //   slotHNorm = slotHeightPx / widgetHeightPx
    //   slotYNorm = bandTop + stackSlot * slotHNorm
    //   m_y       = slotYNorm + m_slotLocalY * slotHNorm
    //   m_h       = m_slotLocalH * slotHNorm
    //
    // where slotHeightPx = max(0.05 * widgetHeightPx, 24) —
    // Thetis `_fHeight = 0.05f` (MeterManager.cs:21266) with a
    // 24-pixel floor so rows stay readable when the container is
    // small. bandTop is derived per-reflow from the composite
    // sitting above the stack (max of y+h among items with
    // itemHeight() > 0.30), so the stack shifts automatically when
    // a composite is added or removed.
    //
    // m_slotLocalY/m_slotLocalH hold the preset factory's
    // canonical within-slot 0..1 layout (SolidBg 0..1, BarItem
    // 0.2..0.8, ScaleItem 0..1 etc.) and are the source of truth
    // across reflows — the outer m_y/m_h are derived every time.
    //
    // These fields are deliberately NOT serialized: on load
    // MeterWidget::inferStackFromGeometry() walks deserialized
    // items and rebuilds the stack tagging from geometry so
    // existing containers keep working without a format bump.
    int   stackSlot() const { return m_stackSlot; }
    float slotLocalY() const { return m_slotLocalY; }
    float slotLocalH() const { return m_slotLocalH; }
    void  setStackSlot(int s) { m_stackSlot = s; }
    void  setSlotLocalY(float y) { m_slotLocalY = y; }
    void  setSlotLocalH(float h) { m_slotLocalH = h; }
    void  clearStackMetadata() {
        m_stackSlot = -1;
        m_slotLocalY = 0.0f;
        m_slotLocalH = 1.0f;
    }

    // Recompute m_y/m_h from stack metadata. No-op when
    // m_stackSlot < 0. Called from
    // MeterWidget::reflowStackedItems() on every resize.
    // `bandTop` is the normalized y at which the stack starts
    // (0 when no composite is present, else the composite's
    // max y+h).
    void layoutInStackSlot(int widgetHeightPx, int slotHeightPx,
                           float bandTop);

protected:
    QRect pixelRect(int widgetW, int widgetH) const {
        return QRect(
            static_cast<int>(m_x * widgetW),
            static_cast<int>(m_y * widgetH),
            static_cast<int>(m_w * widgetW),
            static_cast<int>(m_h * widgetH)
        );
    }

    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    int m_bindingId{-1};
    double m_value{-140.0};
    int m_zOrder{0};

    // Stack metadata (runtime only — see public accessors).
    int   m_stackSlot{-1};
    float m_slotLocalY{0.0f};
    float m_slotLocalH{1.0f};

    // Visibility filter (see public accessors above)
    bool m_onlyWhenRx{false};
    bool m_onlyWhenTx{false};
    int  m_displayGroup{0};

    // Phase 3G-6 block 5 — MMIO binding (in-memory only, not
    // serialized yet). When hasMmioBinding() is true, MeterPoller
    // reads the value from the bound endpoint's variable cache
    // instead of the WDSP-backed bindingId() path.
    QUuid   m_mmioGuid;
    QString m_mmioVariable;
};

// ---------------------------------------------------------------------------
// SolidColourItem — Background fill
// ---------------------------------------------------------------------------
class SolidColourItem : public MeterItem {
    Q_OBJECT
public:
    explicit SolidColourItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setColour(const QColor& c) { m_colour = c; }
    QColor colour() const { return m_colour; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QColor m_colour{0x0f, 0x0f, 0x1a};
};

// ---------------------------------------------------------------------------
// ImageItem — Static image
// ---------------------------------------------------------------------------
class ImageItem : public MeterItem {
    Q_OBJECT
public:
    explicit ImageItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setImage(const QImage& img) { m_image = img; }
    void setImagePath(const QString& path);
    QString imagePath() const { return m_imagePath; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QImage   m_image;
    QString  m_imagePath;
};

// ---------------------------------------------------------------------------
// BarItem — Horizontal or vertical bar meter
// ---------------------------------------------------------------------------
class BarItem : public MeterItem {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };
    // From Thetis clsBarItem.BarStyle (MeterManager.cs:19927-19934):
    //   None, Line, SolidFilled, GradientFilled, Segments.
    // NereusSDR maps SolidFilled -> Filled for back-compat with pre-A4
    // presets and adds Line. Edge is a NereusSDR extension (mapped from
    // Thetis console.cs edge meters).
    enum class BarStyle { Filled, Edge, Line };

    explicit BarItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setOrientation(Orientation o) { m_orientation = o; }
    Orientation orientation() const { return m_orientation; }

    void setRange(double minVal, double maxVal) { m_minVal = minVal; m_maxVal = maxVal; }
    double minVal() const { return m_minVal; }
    double maxVal() const { return m_maxVal; }

    void setBarColor(const QColor& c) { m_barColor = c; }
    QColor barColor() const { return m_barColor; }

    void setBarRedColor(const QColor& c) { m_barRedColor = c; }
    QColor barRedColor() const { return m_barRedColor; }

    void setRedThreshold(double t) { m_redThreshold = t; }
    double redThreshold() const { return m_redThreshold; }

    void setAttackRatio(float a) { m_attackRatio = a; }
    float attackRatio() const { return m_attackRatio; }

    void setDecayRatio(float d) { m_decayRatio = d; }
    float decayRatio() const { return m_decayRatio; }

    void setBarStyle(BarStyle s) { m_barStyle = s; }
    BarStyle barStyle() const { return m_barStyle; }

    // Edge mode colors (from Thetis console.cs:12612-12678)
    void setEdgeBackgroundColor(const QColor& c) { m_edgeBgColor = c; }
    QColor edgeBackgroundColor() const { return m_edgeBgColor; }

    void setEdgeLowColor(const QColor& c) { m_edgeLowColor = c; }
    QColor edgeLowColor() const { return m_edgeLowColor; }

    void setEdgeHighColor(const QColor& c) { m_edgeHighColor = c; }
    QColor edgeHighColor() const { return m_edgeHighColor; }

    void setEdgeAvgColor(const QColor& c) { m_edgeAvgColor = c; }
    QColor edgeAvgColor() const { return m_edgeAvgColor; }

    // --- Phase A1: ShowValue / ShowPeakValue / FontColour ---
    // From Thetis clsBarItem (MeterManager.cs:19939-19940, 21541, 21550,
    // 21706-21708). ShowValue draws the current value as text top-left of
    // the bar; ShowPeakValue draws the rolling peak top-right. FontColour
    // is the shared text color (Thetis default Yellow).
    void setShowValue(bool on) { m_showValue = on; }
    bool showValue() const { return m_showValue; }

    void setShowPeakValue(bool on) { m_showPeakValue = on; }
    bool showPeakValue() const { return m_showPeakValue; }

    void setFontColour(const QColor& c) { m_fontColour = c; }
    QColor fontColour() const { return m_fontColour; }

    // ShowPeakValue uses a separate colour so the current-value text
    // can be white while the peak text is red (Thetis default — see
    // Setup → Appearance → Meters/Gadgets "Peak Value" swatch, which
    // ships red). If unset, falls back to m_fontColour for
    // backward compat with pre-Phase-E BarItems that only had one
    // font colour.
    void setPeakFontColour(const QColor& c) { m_peakFontColour = c; }
    QColor peakFontColour() const
    {
        return m_peakFontColour.isValid() ? m_peakFontColour : m_fontColour;
    }

    // Rolling high-water-mark of all values seen via setValue(). Consumed
    // by ShowPeakValue text render and (in A3) the peak-hold marker.
    double peakValue() const { return m_peakValue; }

    /// Snap the smoothed value + peak-hold marker to the supplied target
    /// value, bypassing the attack/decay smoothing that setValue() applies.
    /// Used on TX-end transitions: a single setValue(0) only decays one
    /// tick (e.g. 60 → 54 with decayRatio=0.1) so the bar appears stuck
    /// after un-key. clearSmoothing(0.0) snaps it.
    /// NereusSDR-original safety/UX: bench-reported #167 follow-up where
    /// Power and SWR bars stayed at the last sample after MOX-off.
    void clearSmoothing(double v) {
        m_value          = v;
        m_smoothedValue  = v;
        m_peakValue      = v;
    }

    // --- Phase A2: ShowHistory / HistoryColour / HistoryDuration ---
    // From Thetis clsBarItem (MeterManager.cs:19938, 19881, 19945-19946,
    // 20040-20053). History draws a translucent trailing fill behind the
    // live bar showing recent values. Thetis default HistoryDuration is
    // 4000ms (see addSMeterBar:21539 and AddADCMaxMag:21633).
    void setShowHistory(bool on) { m_showHistory = on; }
    bool showHistory() const { return m_showHistory; }

    void setHistoryColour(const QColor& c) { m_historyColour = c; }
    QColor historyColour() const { return m_historyColour; }

    void setHistoryDurationMs(int ms) { m_historyDurationMs = ms; }
    int historyDurationMs() const { return m_historyDurationMs; }

    // Introspection for tests + the render pass.
    int historySampleCount() const { return static_cast<int>(m_history.size()); }

    // --- Phase A3: ShowMarker / MarkerColour / PeakHoldMarkerColour ---
    // From Thetis clsBarItem (MeterManager.cs:21543-21544). ShowMarker
    // draws a thin vertical line at the live smoothed value; the separate
    // peak-hold marker draws at the decaying peakValue(). The peak-hold
    // decay ratio is independent of attack/decay smoothing — when non-zero,
    // peakValue() falls toward the live value when values drop.
    void setShowMarker(bool on) { m_showMarker = on; }
    bool showMarker() const { return m_showMarker; }

    void setMarkerColour(const QColor& c) { m_markerColour = c; }
    QColor markerColour() const { return m_markerColour; }

    void setPeakHoldMarkerColour(const QColor& c) { m_peakHoldMarkerColour = c; }
    QColor peakHoldMarkerColour() const { return m_peakHoldMarkerColour; }

    void setPeakHoldDecayRatio(float r) { m_peakHoldDecayRatio = r; }
    float peakHoldDecayRatio() const { return m_peakHoldDecayRatio; }

    // --- Phase A4: ScaleCalibration (non-linear value → X map) ---
    // From Thetis clsBarItem.ScaleCalibration (MeterManager.cs:20192,
    // 21547-21549 addSMeterBar, 21638-21640 AddADCMaxMag). When calibration
    // waypoints are set, the bar uses piecewise-linear interpolation
    // between them instead of the linear minVal..maxVal mapping. Used by
    // the S-meter to map the non-linear dBm→S-unit curve. When empty,
    // BarItem falls back to linear range mapping (pre-A4 behavior).
    void addScaleCalibration(double value, float normalizedX);
    void clearScaleCalibration() { m_scaleCalibration.clear(); }
    int scaleCalibrationSize() const
    {
        return static_cast<int>(m_scaleCalibration.size());
    }

    // Map a raw value to a normalized X position in [0, 1]. Uses
    // ScaleCalibration if populated, otherwise linear range mapping.
    // Consumed by paint() for bar fill length, live marker, peak marker,
    // and history polyline — the single source of truth for "where on
    // the bar does this value sit".
    float valueToNormalizedX(double value) const;

    // Override setValue() for exponential smoothing
    // From Thetis MeterManager.cs — attack/decay smoothing
    void setValue(double v) override;

    // renderLayer() stays at Geometry for legacy code that queries it,
    // but participatesIn() opts the bar out of the GPU Geometry pass
    // and into the OverlayDynamic QPainter pass. Phase D1b needs every
    // A-phase render (history polyline, ShowValue text, ShowMarker +
    // PeakHold lines, BarStyle::Line baseline, calibrated value
    // positioning) and none of those can be expressed as a GPU vertex
    // quad without major shader work. Running through the overlay
    // QPainter is simpler and matches how ScaleItem + TextItem already
    // render in the same render cycle. emitVertices() below is kept
    // as a safety no-op fall-through in case some other code path
    // still iterates Geometry items directly.
    Layer renderLayer() const override { return Layer::Geometry; }
    bool participatesIn(Layer layer) const override {
        return layer == Layer::OverlayDynamic;
    }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    void paintEdge(QPainter& p, int widgetW, int widgetH);

    Orientation m_orientation{Orientation::Horizontal};
    double      m_minVal{-140.0};
    double      m_maxVal{0.0};
    // AetherSDR cyan bar color
    QColor      m_barColor{0x00, 0xb4, 0xd8};
    QColor      m_barRedColor{0xff, 0x44, 0x44};
    double      m_redThreshold{1000.0}; // disabled by default (above maxVal)
    // From Thetis MeterManager.cs
    float       m_attackRatio{0.8f};
    float       m_decayRatio{0.2f};
    double      m_smoothedValue{-140.0};
    BarStyle    m_barStyle{BarStyle::Filled};
    QColor      m_edgeBgColor{Qt::black};
    QColor      m_edgeLowColor{Qt::white};
    QColor      m_edgeHighColor{Qt::red};
    QColor      m_edgeAvgColor{Qt::yellow};
    // Phase A1 — clsBarItem text/peak fields
    bool        m_showValue{false};
    bool        m_showPeakValue{false};
    QColor      m_fontColour{Qt::yellow};
    QColor      m_peakFontColour{};  // invalid = use m_fontColour
    double      m_peakValue{-std::numeric_limits<double>::infinity()};
    // Phase A2 — clsBarItem history fields
    bool        m_showHistory{false};
    QColor      m_historyColour{255, 0, 0, 128};  // Thetis default Red(128)
    int         m_historyDurationMs{4000};         // Thetis default
    QVector<double> m_history;                     // rolling sample buffer
    // Phase A3 — clsBarItem marker fields
    bool        m_showMarker{false};
    QColor      m_markerColour{Qt::yellow};
    QColor      m_peakHoldMarkerColour{Qt::red};
    float       m_peakHoldDecayRatio{0.0f};        // 0 = hold forever
    // Phase A4 — non-linear value→X calibration waypoints.
    // QMap keeps entries sorted by key (value) which is what the
    // interpolation in valueToNormalizedX() relies on.
    QMap<double, float> m_scaleCalibration;
};

// ---------------------------------------------------------------------------
// ScaleItem — Tick marks and labels
// ---------------------------------------------------------------------------
class ScaleItem : public MeterItem {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };

    explicit ScaleItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setOrientation(Orientation o) { m_orientation = o; }
    Orientation orientation() const { return m_orientation; }

    void setRange(double minVal, double maxVal) { m_minVal = minVal; m_maxVal = maxVal; }
    double minVal() const { return m_minVal; }
    double maxVal() const { return m_maxVal; }

    void setMajorTicks(int n) { m_majorTicks = n; }
    int majorTicks() const { return m_majorTicks; }

    void setMinorTicks(int n) { m_minorTicks = n; }
    int minorTicks() const { return m_minorTicks; }

    void setTickColor(const QColor& c) { m_tickColor = c; }
    QColor tickColor() const { return m_tickColor; }

    void setLabelColor(const QColor& c) { m_labelColor = c; }
    QColor labelColor() const { return m_labelColor; }

    void setFontSize(int sz) { m_fontSize = sz; }
    int fontSize() const { return m_fontSize; }

    // --- Phase B2: ShowType centered title ---
    // From Thetis clsScaleItem (MeterManager.cs:14827 ShowType property,
    // 31879-31886 render pass). When true, paint() draws readingName()
    // of the scale's bindingId centered in the top strip of the scale
    // rect. Used by every bar-row preset to label its row with the
    // canonical Thetis reading name in red.
    void setShowType(bool on) { m_showType = on; }
    bool showType() const { return m_showType; }

    void setTitleColour(const QColor& c) { m_titleColour = c; }
    QColor titleColour() const { return m_titleColour; }

    // --- Phase B3: GeneralScale two-tone render + baseline at y + h*0.85 ---
    // From Thetis MeterManager.cs:32338-32423 generalScale(). When
    // scaleStyle() == GeneralScale, paint() draws a two-tone horizontal
    // baseline split at centrePerc with major+minor ticks extending
    // upward from the baseline, matching Thetis exactly. The default
    // ScaleStyle::Linear leaves the pre-B3 NereusSDR evenly-spaced tick
    // renderer untouched, so existing Filled presets don't regress.
    //
    // Opt in by calling setScaleStyle(GeneralScale) and configuring
    // setGeneralScaleParams(low, high, start, end, lowInc, highInc,
    // centrePerc). setLowColour / setHighColour override the default
    // tick colours for the two halves of the scale.
    enum class ScaleStyle { Linear, GeneralScale };

    void setScaleStyle(ScaleStyle s) { m_scaleStyle = s; }
    ScaleStyle scaleStyle() const { return m_scaleStyle; }

    void setLowColour(const QColor& c) { m_lowColour = c; }
    QColor lowColour() const { return m_lowColour; }

    void setHighColour(const QColor& c) { m_highColour = c; }
    QColor highColour() const { return m_highColour; }

    void setGeneralScaleParams(int lowLongTicks,
                               int highLongTicks,
                               int lowStartNumber,
                               int highEndNumber,
                               int lowIncrement,
                               int highIncrement,
                               float centrePerc)
    {
        m_lowLongTicks   = lowLongTicks;
        m_highLongTicks  = highLongTicks;
        m_lowStartNumber = lowStartNumber;
        m_highEndNumber  = highEndNumber;
        m_lowIncrement   = lowIncrement;
        m_highIncrement  = highIncrement;
        m_centrePerc     = centrePerc;
    }

    int lowLongTicks() const { return m_lowLongTicks; }
    int highLongTicks() const { return m_highLongTicks; }

    Layer renderLayer() const override { return Layer::OverlayStatic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    Orientation m_orientation{Orientation::Horizontal};
    double      m_minVal{-140.0};
    double      m_maxVal{0.0};
    int         m_majorTicks{7};
    int         m_minorTicks{5};
    // AetherSDR colors
    QColor      m_tickColor{0xc8, 0xd8, 0xe8};
    QColor      m_labelColor{0x80, 0x90, 0xa0};
    int         m_fontSize{10};
    // Phase B2 — ShowType title
    bool        m_showType{false};
    QColor      m_titleColour{Qt::red};   // Thetis default per screenshot
    // Phase B3 — GeneralScale two-tone renderer
    ScaleStyle  m_scaleStyle{ScaleStyle::Linear};
    QColor      m_lowColour{0xc8, 0xd8, 0xe8};
    QColor      m_highColour{0xff, 0x80, 0x40};
    int         m_lowLongTicks{6};
    int         m_highLongTicks{3};
    int         m_lowStartNumber{-1};
    int         m_highEndNumber{60};
    int         m_lowIncrement{2};
    int         m_highIncrement{20};
    float       m_centrePerc{-1.0f};  // -1 = auto from tick counts
};

// Forward-declare the private render helper called from ScaleItem::paint.
// (No public API; kept in the cpp file.)

// ---------------------------------------------------------------------------
// TextItem — Dynamic value readout
// ---------------------------------------------------------------------------
class TextItem : public MeterItem {
    Q_OBJECT
public:
    explicit TextItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setLabel(const QString& label) { m_label = label; }
    QString label() const { return m_label; }

    void setTextColor(const QColor& c) { m_textColor = c; }
    QColor textColor() const { return m_textColor; }

    void setFontSize(int sz) { m_fontSize = sz; }
    int fontSize() const { return m_fontSize; }

    void setBold(bool bold) { m_bold = bold; }
    bool bold() const { return m_bold; }

    void setSuffix(const QString& suffix) { m_suffix = suffix; }
    QString suffix() const { return m_suffix; }

    void setDecimals(int decimals) { m_decimals = decimals; }
    int decimals() const { return m_decimals; }

    // Text shown when value is below minValidValue (no signal / TX off)
    void setIdleText(const QString& text) { m_idleText = text; }
    QString idleText() const { return m_idleText; }
    void setMinValidValue(double v) { m_minValidValue = v; }
    double minValidValue() const { return m_minValidValue; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QString m_label;
    // AetherSDR colors
    QColor  m_textColor{0xc8, 0xd8, 0xe8};
    int     m_fontSize{13};
    bool    m_bold{true};
    QString m_suffix{QStringLiteral(" dBm")};
    int     m_decimals{1};
    QString m_idleText;
    double  m_minValidValue{-139.0};  // below default -140 = always show
};

// ---------------------------------------------------------------------------
// NeedleItem — Arc-style S-meter needle (AetherSDR SMeterWidget port)
// Participates in all 4 render layers:
//   P1 Background:     arc colored segments (white S0-S9, red S9+)
//   P2 Geometry:       needle quad + peak marker triangle
//   P3 OverlayStatic:  tick marks, tick labels, source label
//   P3 OverlayDynamic: S-unit readout (left), dBm readout (right)
// ---------------------------------------------------------------------------
class NeedleItem : public MeterItem {
    Q_OBJECT
public:
    // --- ANANMM/CrossNeedle extensions (Phase 3G-4) ---
    enum class NeedleDirection { Clockwise, CounterClockwise };

    explicit NeedleItem(QObject* parent = nullptr);

    // --- Arc geometry constants (from AetherSDR SMeterWidget.cpp) ---
    static constexpr float kArcStartDeg = 55.0f;   // right end (S9+60)
    static constexpr float kArcEndDeg   = 125.0f;  // left end (S0)
    static constexpr float kRadiusRatio = 0.85f;    // radius = width * 0.85
    static constexpr float kCenterYRatio = 0.65f;   // cy = h + radius - h*0.65
    // Aspect ratio for scaling arc when height is constrained.
    // Higher value = wider arc filling more of the panel width.
    static constexpr float kTargetAspect = 2.8f;

    // --- S-meter scale constants (from AetherSDR SMeterWidget.h) ---
    static constexpr float kS0Dbm  = -127.0f;      // S0
    static constexpr float kS9Dbm  = -73.0f;        // S9 = S0 + 9*6
    static constexpr float kMaxDbm = -13.0f;         // S9+60
    static constexpr float kDbPerS = 6.0f;           // dB per S-unit

    // --- Smoothing (from AetherSDR SMOOTH_ALPHA) ---
    static constexpr float kSmoothAlpha = 0.3f;

    // --- Peak hold: Medium preset (from AetherSDR) ---
    static constexpr int   kPeakHoldFrames   = 5;    // 500ms at 100ms poll
    static constexpr float kPeakDecayPerFrame = 1.0f; // 10 dB/sec = 1dB/100ms
    static constexpr int   kPeakResetFrames  = 100;   // 10s hard reset

    void setSourceLabel(const QString& label) { m_sourceLabel = label; }
    QString sourceLabel() const { return m_sourceLabel; }

    // --- ANANMM/CrossNeedle extension setters/getters (Phase 3G-4) ---
    void setScaleCalibration(const QMap<float, QPointF>& cal) { m_scaleCalibration = cal; }
    QMap<float, QPointF> scaleCalibration() const { return m_scaleCalibration; }

    void setNeedleOffset(const QPointF& off) { m_needleOffset = off; }
    QPointF needleOffset() const { return m_needleOffset; }

    void setRadiusRatio(const QPointF& r) { m_radiusRatio = r; }
    QPointF radiusRatio() const { return m_radiusRatio; }

    void setLengthFactor(float f) { m_lengthFactor = f; }
    float lengthFactor() const { return m_lengthFactor; }

    void setStrokeWidth(float w) { m_strokeWidth = w; }
    float strokeWidth() const { return m_strokeWidth; }

    void setDirection(NeedleDirection d) { m_direction = d; }
    NeedleDirection direction() const { return m_direction; }

    void setHistoryEnabled(bool v) { m_historyEnabled = v; }
    bool historyEnabled() const { return m_historyEnabled; }

    void setHistoryDuration(int ms) { m_historyDuration = ms; }
    int historyDuration() const { return m_historyDuration; }

    void setHistoryColor(const QColor& c) { m_historyColor = c; }
    QColor historyColor() const { return m_historyColor; }

    void setNormaliseTo100W(bool v) { m_normaliseTo100W = v; }
    bool normaliseTo100W() const { return m_normaliseTo100W; }

    void setMaxPower(float p) { m_maxPower = p; }
    float maxPower() const { return m_maxPower; }

    void setNeedleColor(const QColor& c) { m_needleColor = c; }
    QColor needleColor() const { return m_needleColor; }

    void setAttackRatio(float a) { m_attackRatio = a; }
    float attackRatio() const { return m_attackRatio; }

    void setDecayRatio(float d) { m_decayRatio = d; }
    float decayRatio() const { return m_decayRatio; }

    // Interpolate needle position from scale calibration map
    // From Thetis MeterManager.cs calibration point interpolation
    QPointF calibratedPosition(float value) const;

    void setValue(double v) override;

    // Smoothed value as currently held by the needle. Named
    // m_smoothedDbm historically because the needle started life as a
    // dBm-only S-meter; for calibrated needles (ANANMM Volts/Amps/
    // Power/SWR/Compression/ALC) it holds the value in the calibration
    // map's native units (volts, amps, watts, etc.) — see setValue()
    // for the per-mode clamp logic. Exposed so tests can assert the
    // post-smoothing value without round-tripping through paint
    // geometry.
    float smoothedValue() const { return m_smoothedDbm; }

    // Multi-layer: participates in all 4 pipeline layers
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From AetherSDR SMeterWidget.cpp dbmToFraction()
    float dbmToFraction(float dbm) const;
    // From AetherSDR SMeterWidget.cpp sUnitsText()
    QString sUnitsText(float dbm) const;

    void paintBackground(QPainter& p, int widgetW, int widgetH);
    void paintOverlayStatic(QPainter& p, int widgetW, int widgetH);
    void paintOverlayDynamic(QPainter& p, int widgetW, int widgetH);

    // Compute aspect-locked drawing rect (2:1 like AetherSDR 280x140),
    // centered within the item's pixel rect.
    QRect meterRect(int widgetW, int widgetH) const;

    QString m_sourceLabel{QStringLiteral("S-Meter")};
    float   m_smoothedDbm{kS0Dbm};
    float   m_peakDbm{kS0Dbm};
    int     m_peakHoldCounter{0};
    int     m_peakResetCounter{0};

    // --- ANANMM/CrossNeedle extensions (Phase 3G-4) ---
    // Custom scale calibration: value -> normalized (x,y) position on gauge face.
    // When non-empty, paint() uses this instead of dbmToFraction().
    // From Thetis MeterManager.cs AddAnanMM() calibration tables
    QMap<float, QPointF> m_scaleCalibration;

    // Needle geometry overrides
    QPointF m_needleOffset{0.0, 0.0};     // Center offset for needle pivot
    QPointF m_radiusRatio{1.0, 1.0};      // Elliptical scaling
    float   m_lengthFactor{1.0f};         // Needle length multiplier
    float   m_strokeWidth{1.5f};          // Needle line thickness

    // Direction
    NeedleDirection m_direction{NeedleDirection::Clockwise};

    // History trail
    bool   m_historyEnabled{false};
    int    m_historyDuration{4000};  // ms
    QColor m_historyColor{Qt::transparent};

    // Power normalisation
    bool  m_normaliseTo100W{false};
    float m_maxPower{100.0f};

    // Needle color (default white for S-meter backward compat)
    QColor m_needleColor{Qt::white};

    // Attack/decay smoothing ratios (defaults match S-meter behavior)
    float m_attackRatio{0.3f};   // kSmoothAlpha default
    float m_decayRatio{0.3f};    // kSmoothAlpha default
};

} // namespace NereusSDR
