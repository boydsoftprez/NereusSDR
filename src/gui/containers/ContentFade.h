#pragma once

// =================================================================
// src/gui/containers/ContentFade.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Fade on RX / Fade on TX for every container item: a port
//                 of Thetis fade() (one step of the animated dim), the fade
//                 cover rectangle and colour of renderFadeCover, and an
//                 overlay that dims per-instance widget content.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "ContainerContentRegistry.h"

#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace NereusSDR {

class ContentFadeOverlay;

namespace ContentFade {

// Entry keys: a MeterItem entry keeps them in entry.context beside
// onlyWhenRx / onlyWhenTx; a widget-factory entry keeps them in
// entry.config. Absent means off.
inline constexpr auto kFadeOnRxKey = "fadeOnRx";
inline constexpr auto kFadeOnTxKey = "fadeOnTx";

// From Thetis MeterManager.cs:33155 [v2.10.3.15]
constexpr int kDimmedLevel = 48; // the level to dim to
// The undimmed level, FadeValue's initial value.
// From Thetis MeterManager.cs:6778 [v2.10.3.15]
constexpr int kUndimmedLevel = 255;

// One step of Thetis fade() (MeterManager.cs:33153-33177 [v2.10.3.15]):
// the next fade value (kDimmedLevel..kUndimmedLevel) after `current`.
// A stage-disabled item passes true for both flags (Thetis's Disabled).
// [2.10.1.0] MW0LGE
int nextValue(int current, bool fadeOnRx, bool fadeOnTx, bool transmitting, int updateIntervalMs);

// The fade cover rectangle: `itemRect` inflated by max(2 px, 1%) of its
// width and height (renderFadeCover, MeterManager.cs:37826-37845).
QRectF coverRect(const QRectF& itemRect);

// The colour the cover paints: the background of the enclosing container
// (ContainerWidget::contentBackground of its background colour), or the
// content's own window colour when it is in no container.
QColor coverColour(const QWidget* content);

// The two declared properties ("Fade on RX", "Fade on TX") for a
// widget-factory entry, on kFadeOnRxKey / kFadeOnTxKey, default false.
QVector<ContentProperty> properties();

// Gives live widget content a fade overlay when `config` turns either
// fade on. Nothing in Preview or Validation mode, or with both keys off.
void attach(QWidget* content, const QJsonObject& config, ContentRenderMode mode);

// Hands the transmit state to the overlay attach() created, if any.
void setTransmitting(QWidget* content, bool transmitting);

// The overlay attach() created for `content`, or nullptr.
ContentFadeOverlay* overlayFor(QWidget* content);

} // namespace ContentFade

// The dimming veil over a per-instance widget: a child of the content that
// takes no mouse input and paints ContentFade::coverColour at alpha
// 255 - value(), stepping by ContentFade::nextValue on a timer.
class ContentFadeOverlay : public QWidget {
    Q_OBJECT
public:
    ContentFadeOverlay(QWidget* content, bool fadeOnRx, bool fadeOnTx);

    int value() const { return m_value; }
    bool fadeOnRx() const { return m_fadeOnRx; }
    bool fadeOnTx() const { return m_fadeOnTx; }
    bool transmitting() const { return m_transmitting; }
    void setTransmitting(bool transmitting);
    // One step; the timer calls it, tests call it directly.
    void advance();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool settled() const;
    void follow();

    QTimer m_timer;
    int  m_value{ContentFade::kUndimmedLevel};
    bool m_fadeOnRx{false};
    bool m_fadeOnTx{false};
    bool m_transmitting{false};
};

} // namespace NereusSDR
