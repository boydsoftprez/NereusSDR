#pragma once

#include "MeterItem.h"
#include <QColor>
#include <QImage>

namespace NereusSDR {

// From Thetis clsRotatorItem (MeterManager.cs:15042+)
// Antenna rotator compass dial with AZ/ELE/BOTH modes.
class RotatorItem : public MeterItem {
    Q_OBJECT

public:
    enum class RotatorMode { Az, Ele, Both };

    explicit RotatorItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setMode(RotatorMode m) { m_mode = m; }
    RotatorMode mode() const { return m_mode; }

    void setShowValue(bool s) { m_showValue = s; }
    bool showValue() const { return m_showValue; }

    void setShowCardinals(bool s) { m_showCardinals = s; }
    bool showCardinals() const { return m_showCardinals; }

    void setShowBeamWidth(bool s) { m_showBeamWidth = s; }
    bool showBeamWidth() const { return m_showBeamWidth; }

    void setBeamWidth(float deg) { m_beamWidth = deg; }
    float beamWidth() const { return m_beamWidth; }

    void setBeamWidthAlpha(float a) { m_beamWidthAlpha = a; }
    float beamWidthAlpha() const { return m_beamWidthAlpha; }

    void setDarkMode(bool d) { m_darkMode = d; }
    bool darkMode() const { return m_darkMode; }
    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    // Colors (from Thetis clsRotatorItem properties)
    void setBigBlobColour(const QColor& c) { m_bigBlobColour = c; }
    QColor bigBlobColour() const { return m_bigBlobColour; }
    void setSmallBlobColour(const QColor& c) { m_smallBlobColour = c; }
    QColor smallBlobColour() const { return m_smallBlobColour; }
    void setOuterTextColour(const QColor& c) { m_outerTextColour = c; }
    QColor outerTextColour() const { return m_outerTextColour; }
    void setArrowColour(const QColor& c) { m_arrowColour = c; }
    QColor arrowColour() const { return m_arrowColour; }
    void setBeamWidthColour(const QColor& c) { m_beamWidthColour = c; }
    QColor beamWidthColour() const { return m_beamWidthColour; }
    void setBackgroundColour(const QColor& c) { m_backgroundColour = c; }
    QColor backgroundColour() const { return m_backgroundColour; }

    // Background image (file-based, user-replaceable)
    void setBackgroundImagePath(const QString& path);
    QString backgroundImagePath() const { return m_bgImagePath; }

    // Elevation value (for BOTH mode — azimuth uses base m_value)
    void setElevation(float ele) { m_elevation = ele; }
    float elevation() const { return m_elevation; }

    void setValue(double v) override;

    // Multi-layer
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    void paintCompassFace(QPainter& p, const QRect& compassRect);
    void paintHeading(QPainter& p, const QRect& compassRect);
    void paintElevationArc(QPainter& p, const QRect& eleRect);
    QRect squareRect(int widgetW, int widgetH) const;

    // From Thetis clsRotatorItem (MeterManager.cs:15095)
    RotatorMode m_mode{RotatorMode::Both};
    bool   m_showValue{true};
    bool   m_showCardinals{false};
    bool   m_showBeamWidth{false};
    float  m_beamWidth{30.0f};
    float  m_beamWidthAlpha{0.6f};
    bool   m_darkMode{false};
    float  m_padding{0.5f};

    // Colors (from Thetis clsRotatorItem MeterManager.cs:15100-15106)
    QColor m_bigBlobColour{0xff, 0x00, 0x00};     // Red
    QColor m_smallBlobColour{0xff, 0xff, 0xff};    // White
    QColor m_outerTextColour{0x80, 0x80, 0x80};    // Grey (128,128,128)
    QColor m_arrowColour{0xff, 0xff, 0xff};         // White
    QColor m_beamWidthColour{0x40, 0x40, 0x40};    // (64,64,64)
    QColor m_backgroundColour{0x20, 0x20, 0x20};   // (32,32,32)

    // Background image
    QString m_bgImagePath;
    QImage  m_bgImage;

    // Smoothed values (from Thetis clsRotatorItem Update() MeterManager.cs:15290-15312)
    float m_smoothedAz{0.0f};
    float m_elevation{0.0f};
    float m_smoothedEle{0.0f};
};

} // namespace NereusSDR
