#pragma once
// =================================================================
// src/gui/setup/hardware/OcOutputsTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs
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

// OcOutputsTab.h
//
// "OC Outputs" sub-tab of HardwarePage.
//
// Source: Thetis Setup.cs UpdateOCBits() lines 12877-12934.
// Per-band RX mask: chkPenOCrcv{band}{1-7} — 7 OC output bits per band.
// Per-band TX mask: chkPenOCxmit{band}{1-7} — same shape.
// Also Penny.getBandABitMask/setBandABitMask pattern — 7-bit mask per band.
//
// NereusSDR represents each grid as a QTableWidget:
//   Rows = 14 bands (Band::Count), Columns = 7 OC outputs.
// Each cell is a checkable QTableWidgetItem.

#include <QVariant>
#include <QWidget>

class QLabel;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class OcOutputsTab : public QWidget {
    Q_OBJECT
public:
    explicit OcOutputsTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onRxMaskChanged(QTableWidgetItem* item);
    void onTxMaskChanged(QTableWidgetItem* item);

private:
    static QTableWidget* makeGrid(QWidget* parent);

    RadioModel*   m_model{nullptr};

    QTableWidget* m_rxGrid{nullptr};
    QTableWidget* m_txGrid{nullptr};
    QSpinBox*     m_settleDelaySpin{nullptr};
    QLabel*       m_noOcLabel{nullptr};
};

} // namespace NereusSDR
