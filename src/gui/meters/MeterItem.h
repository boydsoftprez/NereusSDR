#pragma once

#include <QObject>
#include <QVector>
#include <QRect>
#include <QColor>
#include <QImage>
#include <QString>

class QPainter;

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

    virtual Layer renderLayer() const = 0;
    virtual void paint(QPainter& p, int widgetW, int widgetH) = 0;
    virtual void emitVertices(QVector<float>& verts, int widgetW, int widgetH) {
        Q_UNUSED(verts); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    }

    virtual QString serialize() const;
    virtual bool deserialize(const QString& data);

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

    // Override setValue() for exponential smoothing
    // From Thetis MeterManager.cs — attack/decay smoothing
    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
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
};

} // namespace NereusSDR
