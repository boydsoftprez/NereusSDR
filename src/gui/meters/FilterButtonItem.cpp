// =================================================================
// src/gui/meters/FilterButtonItem.cpp  (NereusSDR)
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

#include "FilterButtonItem.h"

namespace NereusSDR {

// From Thetis clsFilterButtonBox (MeterManager.cs:7674+)
static const char* const kFilterLabels[] = {
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "Var1", "Var2"
};

FilterButtonItem::FilterButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kFilterCount);
    setColumns(4);
    setCornerRadius(3.0f);

    for (int i = 0; i < kFilterCount; ++i) {
        setupButton(i, QString::fromLatin1(kFilterLabels[i]));
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }

    connect(this, &ButtonBoxItem::buttonClicked,
            this, &FilterButtonItem::onButtonClicked);
}

void FilterButtonItem::setActiveFilter(int index)
{
    if (m_activeFilter == index) { return; }
    if (m_activeFilter >= 0 && m_activeFilter < buttonCount()) {
        button(m_activeFilter).on = false;
    }
    m_activeFilter = index;
    if (m_activeFilter >= 0 && m_activeFilter < buttonCount()) {
        button(m_activeFilter).on = true;
    }
}

void FilterButtonItem::setFilterLabel(int index, const QString& label)
{
    if (index >= 0 && index < buttonCount()) {
        button(index).text = label;
    }
}

void FilterButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn == Qt::RightButton) {
        emit filterContextRequested(index);
    } else {
        emit filterClicked(index);
    }
}

QString FilterButtonItem::serialize() const
{
    return QStringLiteral("FILTERBTNS|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(m_activeFilter).arg(visibleBits());
}

bool FilterButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("FILTERBTNS")) { return false; }
    m_x = parts[1].toFloat();
    m_y = parts[2].toFloat();
    m_w = parts[3].toFloat();
    m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt();
    m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setActiveFilter(parts[8].toInt()); }
    if (parts.size() > 9) { setVisibleBits(parts[9].toUInt()); }
    return true;
}

} // namespace NereusSDR
