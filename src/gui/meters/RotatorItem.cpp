#include "RotatorItem.h"

#include <QPainter>

namespace NereusSDR {

void RotatorItem::setBackgroundImagePath(const QString& path)
{
    m_bgImagePath = path;
    m_bgImage.load(path);
}

void RotatorItem::setValue(double v)
{
    MeterItem::setValue(v);
    // TODO: smooth azimuth (Phase 3G-4)
    m_smoothedAz = static_cast<float>(v);
}

bool RotatorItem::participatesIn(Layer layer) const
{
    Q_UNUSED(layer);
    return true;
}

void RotatorItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    Q_UNUSED(layer);
    paint(p, widgetW, widgetH);
}

void RotatorItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // TODO: implement compass dial (Phase 3G-4)
    const QRect rect = pixelRect(widgetW, widgetH);
    p.fillRect(rect, m_backgroundColour);
}

void RotatorItem::paintCompassFace(QPainter& p, const QRect& compassRect)
{
    Q_UNUSED(p); Q_UNUSED(compassRect);
}

void RotatorItem::paintHeading(QPainter& p, const QRect& compassRect)
{
    Q_UNUSED(p); Q_UNUSED(compassRect);
}

void RotatorItem::paintElevationArc(QPainter& p, const QRect& eleRect)
{
    Q_UNUSED(p); Q_UNUSED(eleRect);
}

QRect RotatorItem::squareRect(int widgetW, int widgetH) const
{
    return pixelRect(widgetW, widgetH);
}

QString RotatorItem::serialize() const
{
    return QStringLiteral("ROTATOR|%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder);
}

bool RotatorItem::deserialize(const QString& data)
{
    Q_UNUSED(data);
    return false;
}

} // namespace NereusSDR
