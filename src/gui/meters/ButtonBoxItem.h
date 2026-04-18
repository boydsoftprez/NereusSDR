#pragma once

// =================================================================
// src/gui/meters/ButtonBoxItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "MeterItem.h"

#include <QColor>
#include <QFont>
#include <QTimer>
#include <QVector>

class QMouseEvent;

namespace NereusSDR {

// Shared base for all button grid items.
// Ported from Thetis clsButtonBox (MeterManager.cs:12307+).
// Handles configurable grid layout, rounded rect buttons, 3-state colors,
// indicator system, icon support, click highlight timer.
class ButtonBoxItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis MeterManager.cs:12309-12327 IndicatorType enum
    enum class IndicatorType {
        Ring, BarLeft, BarRight, BarBottom, BarTop,
        DotLeft, DotRight, DotBottom, DotTop,
        DotTopLeft, DotTopRight, DotBottomLeft, DotBottomRight,
        TextIconColour
    };

    explicit ButtonBoxItem(QObject* parent = nullptr);
    ~ButtonBoxItem() override;

    // Grid configuration
    void setColumns(int cols) { m_columns = cols; }
    int columns() const { return m_columns; }

    void setBorderWidth(float w) { m_borderWidth = w; }
    float borderWidth() const { return m_borderWidth; }

    void setMargin(float m) { m_margin = m; }
    float margin() const { return m_margin; }

    void setCornerRadius(float r) { m_cornerRadius = r; }
    float cornerRadius() const { return m_cornerRadius; }

    void setHeightRatio(float r) { m_heightRatio = r; }
    float heightRatio() const { return m_heightRatio; }

    // Button count
    int buttonCount() const { return m_buttonCount; }

    // Per-button state
    struct ButtonState {
        QString text;
        QColor fillColour{0x1a, 0x2a, 0x3a};       // STYLEGUIDE button base
        QColor hoverColour{0x20, 0x40, 0x60};       // STYLEGUIDE button hover
        QColor clickColour{0x00, 0xb4, 0xd8};       // STYLEGUIDE accent
        QColor borderColour{0x20, 0x50, 0x70};      // STYLEGUIDE border
        QColor onColour{0x00, 0x70, 0xc0};          // STYLEGUIDE blue active bg
        QColor offColour{0x1a, 0x2a, 0x3a};
        QColor textColour{0xc8, 0xd8, 0xe8};        // STYLEGUIDE primary text
        IndicatorType indicatorType{IndicatorType::Ring};
        float indicatorWidth{0.005f};
        bool on{false};
        bool visible{true};
        bool enabled{true};
    };

    void setButtonCount(int count);
    ButtonState& button(int index);
    const ButtonState& button(int index) const;

    // Visibility bitmask (from Thetis clsButtonBox _visible_bits)
    void setVisibleBits(uint32_t bits);
    uint32_t visibleBits() const { return m_visibleBits; }

    // FadeOnRx/FadeOnTx (from Thetis clsButtonBox)
    void setFadeOnRx(bool v) { m_fadeOnRx = v; }
    bool fadeOnRx() const { return m_fadeOnRx; }
    void setFadeOnTx(bool v) { m_fadeOnTx = v; }
    bool fadeOnTx() const { return m_fadeOnTx; }

    void setTransmitting(bool tx) { m_transmitting = tx; }

    // Rendering
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;

    // Mouse interaction
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    // Emitted when any button is clicked. Subclasses connect or override.
    void buttonClicked(int index, Qt::MouseButton button);

protected:
    // Returns the button index at the given widget-pixel position, or -1.
    int buttonAt(const QPointF& pos, int widgetW, int widgetH) const;

    // Subclasses call this to set up their button labels/colors after setButtonCount.
    void setupButton(int index, const QString& text, const QColor& onColour = QColor());

    int m_buttonCount{0};
    QVector<ButtonState> m_buttons;

private:
    // From Thetis clsButtonBox layout calculation
    QRectF buttonRect(int index, const QRectF& area) const;
    void paintButton(QPainter& p, int index, const QRectF& rect);
    void paintIndicator(QPainter& p, int index, const QRectF& rect);

    int m_columns{4};
    float m_borderWidth{0.005f};    // From Thetis clsButtonBox default
    float m_margin{0.0f};
    float m_cornerRadius{3.0f};
    float m_heightRatio{1.0f};
    uint32_t m_visibleBits{0xFFFFFFFF};
    bool m_fadeOnRx{false};
    bool m_fadeOnTx{false};
    bool m_transmitting{false};

    // Click highlight (from Thetis 100ms timer pattern)
    int m_hoveredIndex{-1};
    int m_clickedIndex{-1};
    QTimer m_clickTimer;
};

} // namespace NereusSDR
