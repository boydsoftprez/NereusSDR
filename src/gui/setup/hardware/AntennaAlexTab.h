#pragma once
// =================================================================
// src/gui/setup/hardware/AntennaAlexTab.h  (NereusSDR)
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

// AntennaAlexTab.h
//
// "Antenna / ALEX" sub-tab of HardwarePage.
//
// Source: Thetis Setup.cs InitAlexAntTables() (line 13393) — per-band
// per-receiver RX antenna radio buttons (radAlexR1_*/R2_*/R3_*), per-band
// TX antenna radio buttons (radAlexT1_*/T2_*/T3_*), per-band RX-only
// checkboxes (chkAlex*R1/R2/XV).
// Also Setup.cs:2892-2898 — chkRxOutOnTx, chkEXT1OutOnTx, chkEXT2OutOnTx,
// chkHFTRRelay, chkBPF2Gnd, chkEnableXVTRHF.

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QScrollArea;
class QTableWidget;
class QTableWidgetItem;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class AntennaAlexTab : public QWidget {
    Q_OBJECT
public:
    explicit AntennaAlexTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onRxAntTableChanged(QTableWidgetItem* item);
    void onTxAntTableChanged(QTableWidgetItem* item);

private:
    void buildAntennaTable(QTableWidget* table,
                           const QStringList& colHeaders,
                           const QString& signalKeyPrefix);

    RadioModel*   m_model{nullptr};

    // Per-band RX antenna: 14 rows × 3 columns (ANT1 / ANT2 / ANT3)
    // Each cell is a radio-button-like exclusive selection stored via
    // Qt::CheckStateRole. We use QTableWidget + exclusive logic in itemChanged.
    QTableWidget* m_rxAntTable{nullptr};

    // Per-band TX antenna: same shape
    QTableWidget* m_txAntTable{nullptr};

    // ALEX bypass / relay checkboxes
    // Source: Thetis Setup.cs:2892-2898
    QCheckBox*    m_rxOutOnTx{nullptr};
    QCheckBox*    m_ext1OutOnTx{nullptr};
    QCheckBox*    m_ext2OutOnTx{nullptr};
    QCheckBox*    m_hfTrRelay{nullptr};
    QCheckBox*    m_bpf2Gnd{nullptr};
    QCheckBox*    m_enableXvtrHf{nullptr};

    bool m_updating{false};
};

} // namespace NereusSDR
