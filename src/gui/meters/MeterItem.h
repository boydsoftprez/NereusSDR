#pragma once

#include <QObject>
#include <QVector>
#include <QRect>
#include <QColor>

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

    // Stub: full implementations come in Task 4
    virtual QString serialize() const { return {}; }
    virtual bool deserialize(const QString& data) { Q_UNUSED(data); return false; }

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

} // namespace NereusSDR
