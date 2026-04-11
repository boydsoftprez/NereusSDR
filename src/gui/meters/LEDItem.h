#pragma once

#include "MeterItem.h"
#include <QColor>

namespace NereusSDR {

// From Thetis clsLed (MeterManager.cs:19448+)
// LED indicator with 3 shapes, 2 styles, blink/pulsate animations.
class LEDItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis clsLed LedShape enum
    enum class LedShape { Square, Round, Triangle };
    // From Thetis clsLed LedStyle enum
    enum class LedStyle { Flat, ThreeD };

    explicit LEDItem(QObject* parent = nullptr);

    void setLedShape(LedShape s) { m_shape = s; }
    LedShape ledShape() const { return m_shape; }

    void setLedStyle(LedStyle s) { m_style = s; }
    LedStyle ledStyle() const { return m_style; }

    void setTrueColour(const QColor& c) { m_trueColour = c; }
    QColor trueColour() const { return m_trueColour; }

    void setFalseColour(const QColor& c) { m_falseColour = c; }
    QColor falseColour() const { return m_falseColour; }

    void setPanelBackColour1(const QColor& c) { m_panelBack1 = c; }
    QColor panelBackColour1() const { return m_panelBack1; }

    void setPanelBackColour2(const QColor& c) { m_panelBack2 = c; }
    QColor panelBackColour2() const { return m_panelBack2; }

    void setShowBackPanel(bool show) { m_showBackPanel = show; }
    bool showBackPanel() const { return m_showBackPanel; }

    void setBlink(bool b) { m_blink = b; }
    bool blink() const { return m_blink; }

    void setPulsate(bool p) { m_pulsate = p; }
    bool pulsate() const { return m_pulsate; }

    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    // Threshold-based mode: value >= threshold activates that color
    void setGreenThreshold(double t) { m_greenThreshold = t; }
    double greenThreshold() const { return m_greenThreshold; }
    void setAmberThreshold(double t) { m_amberThreshold = t; }
    double amberThreshold() const { return m_amberThreshold; }
    void setRedThreshold(double t) { m_redThreshold = t; }
    double redThreshold() const { return m_redThreshold; }

    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QColor currentColor() const;

    LedShape m_shape{LedShape::Round};
    LedStyle m_style{LedStyle::Flat};
    QColor   m_trueColour{0x00, 0xff, 0x88};   // green
    QColor   m_falseColour{0x40, 0x40, 0x40};   // grey (off)
    QColor   m_panelBack1{0x20, 0x20, 0x20};
    QColor   m_panelBack2{0x20, 0x20, 0x20};
    bool     m_showBackPanel{false};
    bool     m_blink{false};
    bool     m_pulsate{false};
    float    m_padding{2.0f};

    // Threshold-based state
    double m_greenThreshold{0.0};
    double m_amberThreshold{1000.0};  // disabled by default (above any real value)
    double m_redThreshold{1000.0};
    QColor m_amberColour{0xff, 0xb8, 0x00};
    QColor m_redColour{0xff, 0x44, 0x44};

    // Animation state
    bool  m_ledOn{false};
    int   m_blinkCounter{0};
    float m_pulsateAlpha{1.0f};
    float m_pulsateDir{-0.05f};  // alpha change per frame
};

} // namespace NereusSDR
