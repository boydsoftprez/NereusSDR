#pragma once
// =================================================================
// src/gui/setup/hardware/Hl2IoBoardTab.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/OpenHPSDR-Thetis source:
//   Project Files/Source/Console/HPSDR/IoBoardHl2.cs
//
// IoBoardHl2.cs is an mi0bot-fork-unique file authored solely by Reid
// Campbell (MI0BOT) — it does not exist in ramdor/Thetis upstream and
// carries no FlexRadio / Wigley / Samphire contributions. Its header
// attribution is reproduced verbatim below, per GNU GPL preservation
// requirements.
//
// Original copyright and license (preserved from IoBoardHl2.cs header):
//
//   Copyright (C) 2025 Reid Campbell, MI0BOT, mi0bot@trom.uk
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code.
// =================================================================

// Hl2IoBoardTab.h
//
// "HL2 I/O Board" sub-tab of HardwarePage.
// Surfaces the Hermes Lite 2 I/O board GPIO/PTT/aux-output configuration.
//
// Source: mi0bot Thetis-HL2 HPSDR/IoBoardHl2.cs —
//   GPIO_DIRECT_BASE = 170 (line 79), I2C address 0x1d (line 139),
//   register REG_TX_FREQ_BYTE* (lines 194-198).
//
// NOTE: IoBoardHl2.cs wraps closed-source I2C register code. The pin names
// and GPIO counts here (0-3) are reasonable defaults based on the HL2 open
// hardware spec (Hermes Lite 2 BOM/schematic). Pending real HL2 smoke test.

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class Hl2IoBoardTab : public QWidget {
    Q_OBJECT
public:
    explicit Hl2IoBoardTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private:
    RadioModel*  m_model{nullptr};

    // I/O board present indicator (read-only once connected)
    QLabel*      m_ioBoardPresentLabel{nullptr};

    // GPIO pin combo boxes
    QComboBox*   m_extPttInputCombo{nullptr};
    QComboBox*   m_cwKeyInputCombo{nullptr};

    // Aux output assignment combo boxes
    QComboBox*   m_auxOut1Combo{nullptr};
    QComboBox*   m_auxOut2Combo{nullptr};
};

} // namespace NereusSDR
