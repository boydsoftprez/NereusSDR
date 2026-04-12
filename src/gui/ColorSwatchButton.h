// src/gui/ColorSwatchButton.h
#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

namespace NereusSDR {

/// Reusable color picker button: shows a color swatch, opens QColorDialog on click.
///
/// Used across Display Setup pages (Spectrum/Waterfall/Grid colors), meter
/// editors, and future skin editor work. Replaces the dead makeColorSwatch
/// helper in DisplaySetupPages.cpp. Emits colorChanged when the user picks a
/// new color via the dialog or programmatic setColor().
///
/// Persistence helpers colorToHex/colorFromHex round-trip through the
/// AppSettings "#RRGGBBAA" string format.
class ColorSwatchButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorSwatchButton(const QColor& initial, QWidget* parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

    static QString colorToHex(const QColor& c);
    static QColor colorFromHex(const QString& hex);

signals:
    void colorChanged(const QColor& c);

private slots:
    void openPicker();

private:
    void updateSwatchStyle();

    QColor m_color;
};

} // namespace NereusSDR
