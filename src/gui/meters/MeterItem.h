#pragma once

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
    enum class BarStyle { Filled, Edge };

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

    // Rolling high-water-mark of all values seen via setValue(). Consumed
    // by ShowPeakValue text render and (in A3) the peak-hold marker.
    double peakValue() const { return m_peakValue; }

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

    // Override setValue() for exponential smoothing
    // From Thetis MeterManager.cs — attack/decay smoothing
    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::Geometry; }
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
};

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
