// src/gui/ColorSwatchButton.cpp
#include "ColorSwatchButton.h"

#include <QColorDialog>
#include <QStringLiteral>

namespace NereusSDR {

ColorSwatchButton::ColorSwatchButton(const QColor& initial, QWidget* parent)
    : QPushButton(parent)
    , m_color(initial.isValid() ? initial : QColor(Qt::black))
{
    setAutoDefault(false);
    setDefault(false);
    setFixedHeight(24);
    setMinimumWidth(56);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Click to choose a color"));
    updateSwatchStyle();
    connect(this, &QPushButton::clicked, this, &ColorSwatchButton::openPicker);
}

void ColorSwatchButton::setColor(const QColor& c)
{
    if (!c.isValid() || c == m_color) {
        return;
    }
    m_color = c;
    updateSwatchStyle();
    emit colorChanged(m_color);
}

void ColorSwatchButton::openPicker()
{
    const QColor picked = QColorDialog::getColor(
        m_color, this, tr("Choose color"),
        QColorDialog::ShowAlphaChannel);
    if (picked.isValid()) {
        setColor(picked);
    }
}

void ColorSwatchButton::updateSwatchStyle()
{
    // Pick a readable border/text color based on the swatch luminance so the
    // button stays visible against any chosen fill.
    const int luminance = qGray(m_color.red(), m_color.green(), m_color.blue());
    const QString fg    = (luminance < 128) ? QStringLiteral("#e8e8e8")
                                             : QStringLiteral("#1a1a1a");
    const QString border = QStringLiteral("#203040");

    setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2;"
        " border: 1px solid %3; border-radius: 3px; padding: 2px 8px; }"
        "QPushButton:hover { border-color: #00b4d8; }"
        "QPushButton:pressed { background: %1; }")
        .arg(m_color.name(QColor::HexArgb), fg, border));
}

QString ColorSwatchButton::colorToHex(const QColor& c)
{
    // AppSettings format: "#RRGGBBAA" (Qt's HexArgb returns "#AARRGGBB",
    // so rearrange).
    const QString argb = c.name(QColor::HexArgb); // "#AARRGGBB"
    if (argb.length() != 9) {
        return c.name(QColor::HexRgb);
    }
    const QString alpha = argb.mid(1, 2);
    const QString rgb   = argb.mid(3, 6);
    return QStringLiteral("#") + rgb + alpha;
}

QColor ColorSwatchButton::colorFromHex(const QString& hex)
{
    if (hex.length() == 9 && hex.startsWith(QLatin1Char('#'))) {
        // "#RRGGBBAA" → rebuild as "#AARRGGBB" for QColor::fromString.
        const QString rgb   = hex.mid(1, 6);
        const QString alpha = hex.mid(7, 2);
        const QColor c = QColor::fromString(QStringLiteral("#") + alpha + rgb);
        if (c.isValid()) {
            return c;
        }
    }
    return QColor::fromString(hex);
}

} // namespace NereusSDR
