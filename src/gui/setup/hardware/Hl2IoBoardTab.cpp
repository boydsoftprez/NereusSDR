// =================================================================
// src/gui/setup/hardware/Hl2IoBoardTab.cpp  (NereusSDR)
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
//                Claude Code. Structural template follows AetherSDR
//                (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// Hl2IoBoardTab.cpp
//
// Source: mi0bot Thetis-HL2 HPSDR/IoBoardHl2.cs
//   - GPIO_DIRECT_BASE = 170 (line 79): base register offset for direct GPIO
//   - I2C address 0x1d (line 139): I/O board I2C address
//   - REG_TX_FREQ_BYTE* registers (lines 194-198): TX frequency bytes
//     written via I2CWrite(1, 0x1d, ...)
//
// NOTE: IoBoardHl2.cs wraps closed-source I2C register code. The GPIO pin
// names/counts (GPIO 0–3) are reasonable defaults based on the HL2 open
// hardware specification. Exact capabilities pending real HL2 smoke testing.
// This tab is only shown when caps.hasIoBoardHl2 is set by HardwarePage.

#include "Hl2IoBoardTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

namespace NereusSDR {

// ── helpers ───────────────────────────────────────────────────────────────────
static void populateGpioCombo(QComboBox* combo)
{
    combo->addItem(QStringLiteral("None"),   -1);
    combo->addItem(QStringLiteral("GPIO 0"),  0);
    combo->addItem(QStringLiteral("GPIO 1"),  1);
    combo->addItem(QStringLiteral("GPIO 2"),  2);
    combo->addItem(QStringLiteral("GPIO 3"),  3);
}

static void populateAuxOutputCombo(QComboBox* combo)
{
    combo->addItem(QStringLiteral("None"),           0);
    combo->addItem(QStringLiteral("RX LED"),         1);
    combo->addItem(QStringLiteral("TX LED"),         2);
    combo->addItem(QStringLiteral("Band indicator"), 3);
}

// ── Constructor ───────────────────────────────────────────────────────────────

Hl2IoBoardTab::Hl2IoBoardTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(8);

    // ── Board present indicator ───────────────────────────────────────────────
    // Source: IoBoardHl2.cs I2CReadInitiate (line 135) — version read-back
    //   on startup determines whether the board is physically present.
    auto* statusGroup = new QGroupBox(tr("I/O Board Status"), this);
    auto* statusForm  = new QFormLayout(statusGroup);
    statusForm->setLabelAlignment(Qt::AlignRight);

    m_ioBoardPresentLabel = new QLabel(tr("Auto-detected on connect"), statusGroup);
    m_ioBoardPresentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusForm->addRow(tr("I/O board:"), m_ioBoardPresentLabel);

    outerLayout->addWidget(statusGroup);

    // ── Input pin assignments ─────────────────────────────────────────────────
    auto* inputGroup = new QGroupBox(tr("Input Pin Assignments"), this);
    auto* inputForm  = new QFormLayout(inputGroup);
    inputForm->setLabelAlignment(Qt::AlignRight);

    // External PTT input pin
    // Source: IoBoardHl2.cs GPIO_DIRECT_BASE=170 (line 79); PTT GPIO mapping
    m_extPttInputCombo = new QComboBox(inputGroup);
    populateGpioCombo(m_extPttInputCombo);
    inputForm->addRow(tr("External PTT input:"), m_extPttInputCombo);

    // CW key input pin
    m_cwKeyInputCombo = new QComboBox(inputGroup);
    populateGpioCombo(m_cwKeyInputCombo);
    inputForm->addRow(tr("CW key input:"), m_cwKeyInputCombo);

    outerLayout->addWidget(inputGroup);

    // ── Auxiliary output assignments ──────────────────────────────────────────
    auto* outputGroup = new QGroupBox(tr("Aux Output Assignments"), this);
    auto* outputForm  = new QFormLayout(outputGroup);
    outputForm->setLabelAlignment(Qt::AlignRight);

    m_auxOut1Combo = new QComboBox(outputGroup);
    populateAuxOutputCombo(m_auxOut1Combo);
    outputForm->addRow(tr("Aux output 1:"), m_auxOut1Combo);

    m_auxOut2Combo = new QComboBox(outputGroup);
    populateAuxOutputCombo(m_auxOut2Combo);
    outputForm->addRow(tr("Aux output 2:"), m_auxOut2Combo);

    outerLayout->addWidget(outputGroup);
    outerLayout->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_extPttInputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit settingChanged(QStringLiteral("hl2IoBoard/extPttPin"),
                            m_extPttInputCombo->itemData(idx));
    });

    connect(m_cwKeyInputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit settingChanged(QStringLiteral("hl2IoBoard/cwKeyPin"),
                            m_cwKeyInputCombo->itemData(idx));
    });

    connect(m_auxOut1Combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit settingChanged(QStringLiteral("hl2IoBoard/auxOut1"),
                            m_auxOut1Combo->itemData(idx));
    });

    connect(m_auxOut2Combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit settingChanged(QStringLiteral("hl2IoBoard/auxOut2"),
                            m_auxOut2Combo->itemData(idx));
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void Hl2IoBoardTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& /*caps*/)
{
    // HardwarePage already gates visibility; nothing board-specific here.
    // The ioBoardPresentLabel could be updated from discovery info in a
    // future phase when HL2 detection lands.
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void Hl2IoBoardTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    struct ComboEntry { const char* key; QComboBox* widget; };
    const ComboEntry entries[] = {
        { "extPttPin", m_extPttInputCombo },
        { "cwKeyPin",  m_cwKeyInputCombo  },
        { "auxOut1",   m_auxOut1Combo     },
        { "auxOut2",   m_auxOut2Combo     },
    };
    for (const auto& e : entries) {
        auto it = settings.constFind(QString::fromLatin1(e.key));
        if (it == settings.constEnd()) { continue; }
        const int pinVal = it.value().toInt();
        QSignalBlocker blocker(e.widget);
        for (int i = 0; i < e.widget->count(); ++i) {
            if (e.widget->itemData(i).toInt() == pinVal) {
                e.widget->setCurrentIndex(i);
                break;
            }
        }
    }
}

} // namespace NereusSDR
