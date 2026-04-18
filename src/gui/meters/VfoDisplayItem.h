#pragma once

// =================================================================
// src/gui/meters/VfoDisplayItem.h  (NereusSDR)
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
class QMouseEvent;
class QWheelEvent;

namespace NereusSDR {
// VFO frequency display with mouse wheel digit tuning.
// Ported from Thetis clsVfoDisplay (MeterManager.cs:12881+).
class VfoDisplayItem : public MeterItem {
    Q_OBJECT
public:
    enum class DisplayMode { VfoA, VfoB, VfoBoth };

    explicit VfoDisplayItem(QObject* parent = nullptr);

    void setFrequency(int64_t hz) { m_frequencyHz = hz; }
    int64_t frequency() const { return m_frequencyHz; }
    void setFrequencyB(int64_t hz) { m_frequencyBHz = hz; }
    int64_t frequencyB() const { return m_frequencyBHz; }

    void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    DisplayMode displayMode() const { return m_displayMode; }

    void setModeLabel(const QString& label) { m_modeLabel = label; }
    void setFilterLabel(const QString& label) { m_filterLabel = label; }
    void setBandLabel(const QString& label) { m_bandLabel = label; }
    void setTransmitting(bool tx) { m_transmitting = tx; }
    void setSplit(bool split) { m_split = split; }

    void setFrequencyColour(const QColor& c) { m_freqColour = c; }
    QColor frequencyColour() const { return m_freqColour; }
    void setModeColour(const QColor& c) { m_modeColour = c; }
    QColor modeColour() const { return m_modeColour; }
    void setFilterColour(const QColor& c) { m_filterColour = c; }
    QColor filterColour() const { return m_filterColour; }
    void setBandColour(const QColor& c) { m_bandColour = c; }
    QColor bandColour() const { return m_bandColour; }
    void setRxColour(const QColor& c) { m_rxColour = c; }
    QColor rxColour() const { return m_rxColour; }
    void setTxColour(const QColor& c) { m_txColour = c; }
    QColor txColour() const { return m_txColour; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    bool handleWheel(QWheelEvent* event, int widgetW, int widgetH) override;
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void frequencyChangeRequested(int64_t deltaHz);
    void bandStackRequested(int bandIndex);
    void filterContextRequested(int filterIndex);

private:
    int64_t digitWeightAt(float xFraction) const;
    QString formatFrequency(int64_t hz) const;

    int64_t m_frequencyHz{14074000};
    int64_t m_frequencyBHz{14074000};
    DisplayMode m_displayMode{DisplayMode::VfoA};
    QString m_modeLabel{QStringLiteral("USB")};
    QString m_filterLabel{QStringLiteral("2.7k")};
    QString m_bandLabel{QStringLiteral("20m")};
    bool m_transmitting{false};
    bool m_split{false};

    // Colors from Thetis clsVfoDisplay defaults
    QColor m_freqColour{0xff, 0xa5, 0x00};     // Orange
    QColor m_modeColour{0x80, 0x80, 0x80};
    QColor m_filterColour{0x80, 0x80, 0x80};
    QColor m_bandColour{0xff, 0xff, 0xff};
    QColor m_rxColour{0x00, 0xff, 0x00};        // LimeGreen
    QColor m_txColour{0xff, 0x00, 0x00};        // Red
    QColor m_splitColour{0xff, 0xa5, 0x00};
    QColor m_digitHighlight{0x80, 0x80, 0x80};
};
} // namespace NereusSDR
