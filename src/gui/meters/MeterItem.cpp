#include "MeterItem.h"

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QStringList>
#include <cmath>
#include <algorithm>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// MeterItem base — serialize / deserialize
// Format: x|y|w|h|bindingId|zOrder
// ---------------------------------------------------------------------------

QString MeterItem::serialize() const
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder);
}

bool MeterItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 6) {
        return false;
    }

    bool ok = true;
    const float x  = parts[0].toFloat(&ok);  if (!ok) { return false; }
    const float y  = parts[1].toFloat(&ok);  if (!ok) { return false; }
    const float w  = parts[2].toFloat(&ok);  if (!ok) { return false; }
    const float h  = parts[3].toFloat(&ok);  if (!ok) { return false; }
    const int bid  = parts[4].toInt(&ok);    if (!ok) { return false; }
    const int zo   = parts[5].toInt(&ok);    if (!ok) { return false; }

    setRect(x, y, w, h);
    setBindingId(bid);
    setZOrder(zo);
    return true;
}

// ---------------------------------------------------------------------------
// Helper: build the base fields string (indices 1-6) for concrete types.
// Concrete types prepend their prefix and append type-specific fields.
// ---------------------------------------------------------------------------
static QString baseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

// Helper: parse base fields from indices 1..6 of parts (index 0 is the prefix).
static bool parseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    // Re-join indices 1..6 and delegate to the base deserialize
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

// ---------------------------------------------------------------------------
// SolidColourItem
// Format: SOLID|x|y|w|h|bindingId|zOrder|#aarrggbb
// ---------------------------------------------------------------------------

void SolidColourItem::paint(QPainter& p, int widgetW, int widgetH)
{
    p.fillRect(pixelRect(widgetW, widgetH), m_colour);
}

QString SolidColourItem::serialize() const
{
    return QStringLiteral("SOLID|%1|%2")
        .arg(baseFields(*this))
        .arg(m_colour.name(QColor::HexArgb));
}

bool SolidColourItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("SOLID")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    m_colour = QColor(parts[7]);
    return m_colour.isValid();
}

// ---------------------------------------------------------------------------
// ImageItem
// Format: IMAGE|x|y|w|h|bindingId|zOrder|path
// ---------------------------------------------------------------------------

void ImageItem::setImagePath(const QString& path)
{
    m_imagePath = path;
    m_image.load(path);
}

void ImageItem::paint(QPainter& p, int widgetW, int widgetH)
{
    if (m_image.isNull()) {
        return;
    }
    const QRect rect = pixelRect(widgetW, widgetH);
    p.drawImage(rect, m_image);
}

QString ImageItem::serialize() const
{
    return QStringLiteral("IMAGE|%1|%2")
        .arg(baseFields(*this))
        .arg(m_imagePath);
}

bool ImageItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("IMAGE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    setImagePath(parts[7]);
    return true;
}

// ---------------------------------------------------------------------------
// BarItem
// Format: BAR|x|y|w|h|bindingId|zOrder|orientation|minVal|maxVal|
//             barColor|barRedColor|redThreshold|attackRatio|decayRatio
// ---------------------------------------------------------------------------

// Override setValue() for exponential smoothing.
// From Thetis MeterManager.cs — attack when rising, decay when falling.
void BarItem::setValue(double v)
{
    MeterItem::setValue(v);
    if (v > m_smoothedValue) {
        m_smoothedValue = static_cast<double>(m_attackRatio) * v
                        + (1.0 - static_cast<double>(m_attackRatio)) * m_smoothedValue;
    } else {
        m_smoothedValue = static_cast<double>(m_decayRatio) * v
                        + (1.0 - static_cast<double>(m_decayRatio)) * m_smoothedValue;
    }
}

void BarItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const double range = m_maxVal - m_minVal;
    const double frac  = (range > 0.0)
        ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
        : 0.0;

    const QColor& fillColor = (m_smoothedValue >= m_redThreshold) ? m_barRedColor : m_barColor;

    if (m_orientation == Orientation::Horizontal) {
        const int fillW = static_cast<int>(frac * rect.width());
        p.fillRect(QRect(rect.left(), rect.top(), fillW, rect.height()), fillColor);
    } else {
        const int fillH = static_cast<int>(frac * rect.height());
        // Vertical fills from bottom
        p.fillRect(QRect(rect.left(), rect.bottom() - fillH, rect.width(), fillH), fillColor);
    }
}

void BarItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    Q_UNUSED(widgetW); Q_UNUSED(widgetH);

    const double range = m_maxVal - m_minVal;
    const double frac  = (range > 0.0)
        ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
        : 0.0;

    // Convert normalized item rect to NDC
    const float ndcL = m_x * 2.0f - 1.0f;
    const float ndcR = (m_x + m_w) * 2.0f - 1.0f;
    const float ndcT = 1.0f - m_y * 2.0f;
    const float ndcB = 1.0f - (m_y + m_h) * 2.0f;

    const QColor& fillColor = (m_smoothedValue >= m_redThreshold) ? m_barRedColor : m_barColor;
    const float r = static_cast<float>(fillColor.redF());
    const float g = static_cast<float>(fillColor.greenF());
    const float b = static_cast<float>(fillColor.blueF());
    const float a = static_cast<float>(fillColor.alphaF());

    float fillR = ndcL;
    float fillT = ndcT;
    float fillL = ndcL;
    float fillB = ndcB;

    if (m_orientation == Orientation::Horizontal) {
        fillR = ndcL + static_cast<float>(frac) * (ndcR - ndcL);
        fillT = ndcT;
        fillB = ndcB;
    } else {
        // Vertical: fill from bottom
        fillL = ndcL;
        fillR = ndcR;
        fillB = ndcB + static_cast<float>(frac) * (ndcT - ndcB);
        fillT = fillB; // reassign; build quad properly below
        fillB = ndcB;
    }

    if (m_orientation == Orientation::Horizontal) {
        // Triangle strip: TL, BL, TR, BR
        // Top-left
        verts << fillL << fillT << r << g << b << a;
        // Bottom-left
        verts << fillL << fillB << r << g << b << a;
        // Top-right
        verts << fillR << fillT << r << g << b << a;
        // Bottom-right
        verts << fillR << fillB << r << g << b << a;
    } else {
        // Vertical fill from bottom: compute top of fill
        const float topOfFill = ndcB + static_cast<float>(frac) * (ndcT - ndcB);
        // Top-left
        verts << ndcL << topOfFill << r << g << b << a;
        // Bottom-left
        verts << ndcL << ndcB      << r << g << b << a;
        // Top-right
        verts << ndcR << topOfFill << r << g << b << a;
        // Bottom-right
        verts << ndcR << ndcB      << r << g << b << a;
    }
}

QString BarItem::serialize() const
{
    return QStringLiteral("BAR|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(baseFields(*this))
        .arg(m_orientation == Orientation::Horizontal ? QStringLiteral("H") : QStringLiteral("V"))
        .arg(m_minVal)
        .arg(m_maxVal)
        .arg(m_barColor.name(QColor::HexArgb))
        .arg(m_barRedColor.name(QColor::HexArgb))
        .arg(m_redThreshold)
        .arg(static_cast<double>(m_attackRatio))
        .arg(static_cast<double>(m_decayRatio));
}

bool BarItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 15 || parts[0] != QLatin1String("BAR")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_orientation = (parts[7] == QLatin1String("H")) ? Orientation::Horizontal : Orientation::Vertical;

    bool ok = true;
    m_minVal       = parts[8].toDouble(&ok);  if (!ok) { return false; }
    m_maxVal       = parts[9].toDouble(&ok);  if (!ok) { return false; }
    m_barColor     = QColor(parts[10]);       if (!m_barColor.isValid()) { return false; }
    m_barRedColor  = QColor(parts[11]);       if (!m_barRedColor.isValid()) { return false; }
    m_redThreshold = parts[12].toDouble(&ok); if (!ok) { return false; }
    m_attackRatio  = parts[13].toFloat(&ok);  if (!ok) { return false; }
    m_decayRatio   = parts[14].toFloat(&ok);  if (!ok) { return false; }

    return true;
}

// ---------------------------------------------------------------------------
// ScaleItem
// Format: SCALE|x|y|w|h|bindingId|zOrder|orientation|minVal|maxVal|
//               majorTicks|minorTicks|tickColor|labelColor|fontSize
// ---------------------------------------------------------------------------

void ScaleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const double range = m_maxVal - m_minVal;
    if (range <= 0.0 || m_majorTicks < 2) {
        return;
    }

    QFont font = p.font();
    font.setPointSize(m_fontSize);
    p.setFont(font);

    if (m_orientation == Orientation::Horizontal) {
        // Major ticks with labels — ticks go downward from top, labels below
        for (int i = 0; i < m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / (m_majorTicks - 1);
            const int x = rect.left() + static_cast<int>(frac * rect.width());

            p.setPen(m_tickColor);
            p.drawLine(x, rect.top(), x, rect.top() + rect.height() / 3);

            const double val = m_minVal + frac * range;
            const QString label = QString::number(val, 'f', 0);
            p.setPen(m_labelColor);
            const QFontMetrics fm(font);
            const int labelW = fm.horizontalAdvance(label);
            p.drawText(x - labelW / 2, rect.top() + rect.height() / 3 + fm.ascent(), label);

            // Minor ticks (between major ticks, except after last major)
            if (i < m_majorTicks - 1 && m_minorTicks > 0) {
                const double nextFrac = static_cast<double>(i + 1) / (m_majorTicks - 1);
                for (int j = 1; j <= m_minorTicks; ++j) {
                    const double minorFrac = frac + (nextFrac - frac) * (static_cast<double>(j) / (m_minorTicks + 1));
                    const int mx = rect.left() + static_cast<int>(minorFrac * rect.width());
                    p.setPen(m_tickColor);
                    p.drawLine(mx, rect.top(), mx, rect.top() + rect.height() / 6);
                }
            }
        }
    } else {
        // Vertical — ticks go left from right edge, labels to the left
        for (int i = 0; i < m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / (m_majorTicks - 1);
            // Top = maxVal, bottom = minVal for vertical orientation
            const int y = rect.top() + static_cast<int>((1.0 - frac) * rect.height());

            p.setPen(m_tickColor);
            p.drawLine(rect.right(), y, rect.right() - rect.width() / 3, y);

            const double val = m_minVal + frac * range;
            const QString label = QString::number(val, 'f', 0);
            p.setPen(m_labelColor);
            const QFontMetrics fm(font);
            const int labelW = fm.horizontalAdvance(label);
            const int labelX = rect.right() - rect.width() / 3 - labelW - 2;
            p.drawText(labelX, y + fm.ascent() / 2, label);

            // Minor ticks
            if (i < m_majorTicks - 1 && m_minorTicks > 0) {
                const double nextFrac = static_cast<double>(i + 1) / (m_majorTicks - 1);
                for (int j = 1; j <= m_minorTicks; ++j) {
                    const double minorFrac = frac + (nextFrac - frac) * (static_cast<double>(j) / (m_minorTicks + 1));
                    const int my = rect.top() + static_cast<int>((1.0 - minorFrac) * rect.height());
                    p.setPen(m_tickColor);
                    p.drawLine(rect.right(), my, rect.right() - rect.width() / 6, my);
                }
            }
        }
    }
}

QString ScaleItem::serialize() const
{
    return QStringLiteral("SCALE|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(baseFields(*this))
        .arg(m_orientation == Orientation::Horizontal ? QStringLiteral("H") : QStringLiteral("V"))
        .arg(m_minVal)
        .arg(m_maxVal)
        .arg(m_majorTicks)
        .arg(m_minorTicks)
        .arg(m_tickColor.name(QColor::HexArgb))
        .arg(m_labelColor.name(QColor::HexArgb))
        .arg(m_fontSize);
}

bool ScaleItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 15 || parts[0] != QLatin1String("SCALE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_orientation = (parts[7] == QLatin1String("H")) ? Orientation::Horizontal : Orientation::Vertical;

    bool ok = true;
    m_minVal     = parts[8].toDouble(&ok);  if (!ok) { return false; }
    m_maxVal     = parts[9].toDouble(&ok);  if (!ok) { return false; }
    m_majorTicks = parts[10].toInt(&ok);    if (!ok) { return false; }
    m_minorTicks = parts[11].toInt(&ok);    if (!ok) { return false; }
    m_tickColor  = QColor(parts[12]);       if (!m_tickColor.isValid()) { return false; }
    m_labelColor = QColor(parts[13]);       if (!m_labelColor.isValid()) { return false; }
    m_fontSize   = parts[14].toInt(&ok);    if (!ok) { return false; }

    return true;
}

// ---------------------------------------------------------------------------
// TextItem
// Format: TEXT|x|y|w|h|bindingId|zOrder|label|textColor|fontSize|bold(0/1)|suffix|decimals
// ---------------------------------------------------------------------------

void TextItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);

    QFont font = p.font();
    font.setPointSize(m_fontSize);
    font.setBold(m_bold);
    p.setFont(font);
    p.setPen(m_textColor);

    QString text;
    if (m_bindingId >= 0) {
        text = QString::number(m_value, 'f', m_decimals) + m_suffix;
    } else {
        text = m_label;
    }

    p.drawText(rect, Qt::AlignCenter, text);
}

QString TextItem::serialize() const
{
    return QStringLiteral("TEXT|%1|%2|%3|%4|%5|%6|%7")
        .arg(baseFields(*this))
        .arg(m_label)
        .arg(m_textColor.name(QColor::HexArgb))
        .arg(m_fontSize)
        .arg(m_bold ? 1 : 0)
        .arg(m_suffix)
        .arg(m_decimals);
}

bool TextItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 13 || parts[0] != QLatin1String("TEXT")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_label     = parts[7];
    m_textColor = QColor(parts[8]);
    if (!m_textColor.isValid()) { return false; }

    bool ok = true;
    m_fontSize  = parts[9].toInt(&ok);   if (!ok) { return false; }
    m_bold      = parts[10].toInt(&ok) != 0; if (!ok) { return false; }
    m_suffix    = parts[11];
    m_decimals  = parts[12].toInt(&ok); if (!ok) { return false; }

    return true;
}

} // namespace NereusSDR
