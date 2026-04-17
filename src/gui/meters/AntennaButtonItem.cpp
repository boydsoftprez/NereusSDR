// =================================================================
// src/gui/meters/AntennaButtonItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "AntennaButtonItem.h"

namespace NereusSDR {

// From Thetis clsAntennaButtonBox (MeterManager.cs:9502+)
static const char* const kAntennaLabels[] = {
    "Rx1", "Rx2", "Rx3", "Aux1", "Aux2",
    "XVTR", "Tx1", "Tx2", "Tx3", "Rx/Tx"
};

AntennaButtonItem::AntennaButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kAntennaCount);
    setColumns(5);
    setCornerRadius(3.0f);
    setVisibleBits(1023);

    for (int i = 0; i < kAntennaCount; ++i) {
        setupButton(i, QString::fromLatin1(kAntennaLabels[i]));
    }
    // Rx: LimeGreen
    for (int i = 0; i < 3; ++i) { button(i).onColour = QColor(0x00, 0xff, 0x00); }
    // Aux + XVTR: Orange
    button(3).onColour = QColor(0xff, 0xa5, 0x00);
    button(4).onColour = QColor(0xff, 0xa5, 0x00);
    button(5).onColour = QColor(0xff, 0xa5, 0x00);
    // Tx: Red
    for (int i = 6; i < 9; ++i) { button(i).onColour = QColor(0xff, 0x44, 0x44); }
    // Toggle: Yellow
    button(9).onColour = QColor(0xff, 0xff, 0x00);

    connect(this, &ButtonBoxItem::buttonClicked, this, &AntennaButtonItem::onButtonClicked);
}

void AntennaButtonItem::setActiveRxAntenna(int index)
{
    if (m_activeRxAnt == index) { return; }
    if (m_activeRxAnt >= 0 && m_activeRxAnt < 3) { button(m_activeRxAnt).on = false; }
    m_activeRxAnt = index;
    if (m_activeRxAnt >= 0 && m_activeRxAnt < 3) { button(m_activeRxAnt).on = true; }
}

void AntennaButtonItem::setActiveTxAntenna(int index)
{
    if (m_activeTxAnt == index) { return; }
    if (m_activeTxAnt >= 0 && m_activeTxAnt < 3) { button(6 + m_activeTxAnt).on = false; }
    m_activeTxAnt = index;
    if (m_activeTxAnt >= 0 && m_activeTxAnt < 3) { button(6 + m_activeTxAnt).on = true; }
}

void AntennaButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton) { emit antennaSelected(index); }
}

QString AntennaButtonItem::serialize() const
{
    return QStringLiteral("ANTENNABTNS|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(visibleBits());
}

bool AntennaButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("ANTENNABTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setVisibleBits(parts[8].toUInt()); }
    return true;
}

} // namespace NereusSDR
