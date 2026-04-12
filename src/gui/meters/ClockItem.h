#pragma once
#include "MeterItem.h"
#include <QColor>
#include <QTimer>

namespace NereusSDR {
// Dual UTC/Local time display. From Thetis clsClock (MeterManager.cs:14075+).
class ClockItem : public MeterItem {
    Q_OBJECT
public:
    explicit ClockItem(QObject* parent = nullptr);

    void setShow24Hour(bool v) { m_show24Hour = v; }
    bool show24Hour() const { return m_show24Hour; }
    void setShowType(bool v) { m_showType = v; }
    bool showType() const { return m_showType; }

    void setTimeColour(const QColor& c) { m_timeColour = c; }
    QColor timeColour() const { return m_timeColour; }
    void setDateColour(const QColor& c) { m_dateColour = c; }
    QColor dateColour() const { return m_dateColour; }
    void setTypeTitleColour(const QColor& c) { m_typeTitleColour = c; }
    QColor typeTitleColour() const { return m_typeTitleColour; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    bool m_show24Hour{true};
    bool m_showType{true};
    QColor m_timeColour{0xc8, 0xd8, 0xe8};
    QColor m_dateColour{0x80, 0x90, 0xa0};
    QColor m_typeTitleColour{0x70, 0x80, 0x90};
    QTimer m_updateTimer;
};
} // namespace NereusSDR
