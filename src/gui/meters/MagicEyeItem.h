#pragma once

#include "MeterItem.h"
#include <QColor>
#include <QImage>

namespace NereusSDR {

// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// Vacuum tube magic eye — green phosphor arc opens/closes with signal.
class MagicEyeItem : public MeterItem {
    Q_OBJECT

public:
    explicit MagicEyeItem(QObject* parent = nullptr) : MeterItem(parent) {}

    // S-meter scale constants (from NeedleItem / AetherSDR)
    static constexpr float kS0Dbm  = -127.0f;
    static constexpr float kMaxDbm = -13.0f;
    static constexpr float kSmoothAlpha = 0.3f;

    // Shadow wedge range (degrees)
    static constexpr float kMaxShadowDeg = 120.0f; // no signal — wide open
    static constexpr float kMinShadowDeg = 5.0f;   // full signal — nearly closed

    void setGlowColor(const QColor& c) { m_glowColor = c; }
    QColor glowColor() const { return m_glowColor; }

    void setBezelImagePath(const QString& path);
    QString bezelImagePath() const { return m_bezelPath; }

    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    float dbmToFraction(float dbm) const;

    QColor  m_glowColor{0x00, 0xff, 0x88};
    QString m_bezelPath;
    QImage  m_bezelImage;
    float   m_smoothedDbm{kS0Dbm};
};

} // namespace NereusSDR
