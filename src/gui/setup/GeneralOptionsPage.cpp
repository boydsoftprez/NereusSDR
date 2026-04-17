// =================================================================
// src/gui/setup/GeneralOptionsPage.cpp  (NereusSDR)
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

// src/gui/setup/GeneralOptionsPage.cpp
//
// Porting from Thetis setup.cs: grpHermesStepAttenuator, groupBoxTS47.

#include "GeneralOptionsPage.h"
#include "models/RadioModel.h"
#include "core/StepAttenuatorController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

namespace NereusSDR {

namespace {

void applyDarkStyle(QWidget* w)
{
    w->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #8090a0; font-size: 11px;"
        "  border: 1px solid #203040; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #c8d8e8; }"
        "QComboBox { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
        "  selection-background-color: #00b4d8; }"
        "QSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        "QSpinBox::up-button, QSpinBox::down-button"
        "  { background: #203040; border: none; }"
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    ));
}

// Helper: create a dB spinbox (0-31, suffix " dB", width 80).
QSpinBox* makeDbSpinBox(QWidget* parent)
{
    auto* spn = new QSpinBox(parent);
    spn->setRange(0, 31);
    spn->setSuffix(QStringLiteral(" dB"));
    spn->setFixedWidth(80);
    return spn;
}

// Helper: create an auto-att mode combo (Classic/Adaptive, width 100).
QComboBox* makeModeCombo(QWidget* parent)
{
    auto* cmb = new QComboBox(parent);
    cmb->addItem(QStringLiteral("Classic"));
    cmb->addItem(QStringLiteral("Adaptive"));
    cmb->setFixedWidth(100);
    // NereusSDR native — Classic mirrors Thetis bump+stack, Adaptive adds
    // 1 dB/tick attack with hold/decay and per-band floor memory.
    cmb->setToolTip(QStringLiteral(
        "Classic: bump ATT on red overload, stack-based undo.\n"
        "Adaptive: 1 dB/tick attack, configurable hold/decay, per-band memory."));
    return cmb;
}

// Helper: create hold-seconds spinbox (1-3600, default 5, suffix " sec").
QSpinBox* makeHoldSpinBox(QWidget* parent)
{
    auto* spn = new QSpinBox(parent);
    spn->setRange(1, 3600);
    spn->setValue(5);
    spn->setSuffix(QStringLiteral(" sec"));
    spn->setFixedWidth(80);
    return spn;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// GeneralOptionsPage
// ---------------------------------------------------------------------------

GeneralOptionsPage::GeneralOptionsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Options"), model, parent)
{
    applyDarkStyle(this);
    m_ctrl = model ? model->stepAttController() : nullptr;

    buildStepAttGroup();
    buildAutoAttGroup();

    if (m_ctrl) {
        // Re-range setup spinboxes from board capabilities (may be 61 dB).
        int maxDb = m_ctrl->maxAttenuation();
        m_spnRx1StepAttValue->setRange(0, maxDb);
        m_spnRx2StepAttValue->setRange(0, maxDb);
        connectController();
    }
}

// ---------------------------------------------------------------------------
// Step Attenuator group
// From Thetis setup.cs: grpHermesStepAttenuator
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildStepAttGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Step Attenuator"), this);
    auto* vbox  = new QVBoxLayout(group);
    vbox->setSpacing(6);

    // --- RX1 row ---
    auto* rx1Row = new QHBoxLayout;
    m_chkRx1StepAttEnable = new QCheckBox(QStringLiteral("RX1 Enable"), group);
    // From Thetis setup.cs: chkHermesStepAttenuator
    m_chkRx1StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_spnRx1StepAttValue = makeDbSpinBox(group);
    m_spnRx1StepAttValue->setEnabled(false);
    rx1Row->addWidget(m_chkRx1StepAttEnable);
    rx1Row->addWidget(m_spnRx1StepAttValue);
    rx1Row->addStretch();
    vbox->addLayout(rx1Row);

    // --- RX2 row ---
    auto* rx2Row = new QHBoxLayout;
    m_chkRx2StepAttEnable = new QCheckBox(QStringLiteral("RX2 Enable"), group);
    // From Thetis setup.cs: chkHermesStepAttenuator (RX2 mirror)
    m_chkRx2StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_spnRx2StepAttValue = makeDbSpinBox(group);
    m_spnRx2StepAttValue->setEnabled(false);
    rx2Row->addWidget(m_chkRx2StepAttEnable);
    rx2Row->addWidget(m_spnRx2StepAttValue);
    rx2Row->addStretch();
    vbox->addLayout(rx2Row);

    // --- ADC linked label ---
    m_lblAdcLinked = new QLabel(QStringLiteral("ADC linked — both RX share the same ADC"), group);
    m_lblAdcLinked->setStyleSheet(QStringLiteral("color: #ff4444; font-weight: bold;"));
    m_lblAdcLinked->setVisible(false);
    vbox->addWidget(m_lblAdcLinked);

    // --- Enable/disable cascade ---
    connect(m_chkRx1StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx1StepAttValue->setEnabled(on);
        if (m_ctrl) {
            m_ctrl->setStepAttEnabled(on);
        }
    });
    connect(m_chkRx2StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx2StepAttValue->setEnabled(on);
        // Controller is single-RX for step-att enable; RX2 is future expansion.
    });

    // --- Spinbox → controller ---
    connect(m_spnRx1StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setAttenuation(dB, 0);
        }
    });
    connect(m_spnRx2StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setAttenuation(dB, 1);
        }
    });

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Auto Attenuate groups (RX1 + RX2)
// From Thetis setup.cs: groupBoxTS47
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildAutoAttGroup()
{
    // --- Helper lambda to build one auto-att group ---
    auto buildOneRx = [this](const QString& title, int rx,
                             QCheckBox*& chkEnable, QComboBox*& cmbMode,
                             QCheckBox*& chkUndo, QSpinBox*& spnHold)
    {
        auto* group = new QGroupBox(title, this);
        auto* vbox  = new QVBoxLayout(group);
        vbox->setSpacing(6);

        // Enable checkbox
        chkEnable = new QCheckBox(QStringLiteral("Enable"), group);
        // From Thetis setup.cs: chkAutoATTRx1 / chkAutoATTRx2
        chkEnable->setToolTip(
            QStringLiteral("Auto attenuate RX%1 on ADC overload").arg(rx + 1));
        vbox->addWidget(chkEnable);

        // Mode combo row
        auto* modeRow = new QHBoxLayout;
        auto* modeLabel = new QLabel(QStringLiteral("Mode:"), group);
        cmbMode = makeModeCombo(group);
        cmbMode->setEnabled(false);
        modeRow->addWidget(modeLabel);
        modeRow->addWidget(cmbMode);
        modeRow->addStretch();
        vbox->addLayout(modeRow);

        // Undo/Decay checkbox
        chkUndo = new QCheckBox(QStringLiteral("Undo"), group);
        // From Thetis setup.cs: chkAutoATTRx1Undo concept
        chkUndo->setToolTip(
            QStringLiteral("Undo the changes made after the hold period."));
        chkUndo->setEnabled(false);
        vbox->addWidget(chkUndo);

        // Hold seconds row
        auto* holdRow = new QHBoxLayout;
        auto* holdLabel = new QLabel(QStringLiteral("Hold:"), group);
        spnHold = makeHoldSpinBox(group);
        spnHold->setEnabled(false);
        holdRow->addWidget(holdLabel);
        holdRow->addWidget(spnHold);
        holdRow->addStretch();
        vbox->addLayout(holdRow);

        // --- Enable/disable cascade ---
        // mode + undo + hold only enabled when auto-att enabled
        connect(chkEnable, &QCheckBox::toggled, this, [cmbMode, chkUndo, spnHold](bool on) {
            cmbMode->setEnabled(on);
            chkUndo->setEnabled(on);
            spnHold->setEnabled(on && chkUndo->isChecked());
        });

        // hold only enabled when undo is checked (and auto-att enabled)
        connect(chkUndo, &QCheckBox::toggled, this, [chkEnable, spnHold](bool on) {
            spnHold->setEnabled(on && chkEnable->isChecked());
        });

        // Mode change → relabel undo checkbox
        connect(cmbMode, &QComboBox::currentIndexChanged, this, [chkUndo](int idx) {
            if (idx == static_cast<int>(AutoAttMode::Adaptive)) {
                chkUndo->setText(QStringLiteral("Decay"));
            } else {
                chkUndo->setText(QStringLiteral("Undo"));
            }
        });

        // --- Wire to controller (RX1 only — controller is single-RX) ---
        if (m_ctrl && rx == 0) {
            connect(chkEnable, &QCheckBox::toggled, this, [this](bool on) {
                m_ctrl->setAutoAttEnabled(on);
            });
            connect(cmbMode, &QComboBox::currentIndexChanged, this, [this](int idx) {
                m_ctrl->setAutoAttMode(static_cast<AutoAttMode>(idx));
            });
            connect(chkUndo, &QCheckBox::toggled, this, [this](bool on) {
                m_ctrl->setAutoAttUndo(on);
            });
            connect(spnHold, &QSpinBox::valueChanged, this, [this, cmbMode](int sec) {
                if (cmbMode->currentIndex() == static_cast<int>(AutoAttMode::Adaptive)) {
                    m_ctrl->setAutoAttHoldSeconds(static_cast<double>(sec));
                } else {
                    m_ctrl->setAutoUndoDelaySec(sec);
                }
            });
        }

        contentLayout()->addWidget(group);
    };

    buildOneRx(QStringLiteral("Auto Attenuate RX1"), 0,
               m_chkAutoAttRx1, m_cmbAutoAttRx1Mode,
               m_chkAutoAttUndoRx1, m_spnAutoAttHoldRx1);

    buildOneRx(QStringLiteral("Auto Attenuate RX2"), 1,
               m_chkAutoAttRx2, m_cmbAutoAttRx2Mode,
               m_chkAutoAttUndoRx2, m_spnAutoAttHoldRx2);
}

// ---------------------------------------------------------------------------
// connectController — wire signals from the controller back to UI
// ---------------------------------------------------------------------------

void GeneralOptionsPage::connectController()
{
    Q_ASSERT(m_ctrl);

    // ADC-linked label visibility
    connect(m_ctrl, &StepAttenuatorController::adcLinkedChanged,
            m_lblAdcLinked, &QLabel::setVisible);

    // Attenuation changed → update RX1 spinbox (controller is single-RX)
    connect(m_ctrl, &StepAttenuatorController::attenuationChanged,
            this, [this](int dB) {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setValue(dB);
    });
}

// ---------------------------------------------------------------------------
// syncFromModel — restore UI state from controller on page show
// ---------------------------------------------------------------------------

void GeneralOptionsPage::syncFromModel()
{
    if (!m_ctrl) {
        return;
    }

    // Sync auto-att enable/mode from controller accessors
    {
        QSignalBlocker blk(m_chkAutoAttRx1);
        m_chkAutoAttRx1->setChecked(m_ctrl->autoAttEnabled());
    }
    {
        QSignalBlocker blk(m_cmbAutoAttRx1Mode);
        m_cmbAutoAttRx1Mode->setCurrentIndex(static_cast<int>(m_ctrl->autoAttMode()));
    }

    // Sync attenuation value
    {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setValue(m_ctrl->attenuatorDb());
    }

    // Re-cascade enable states
    bool autoOn = m_chkAutoAttRx1->isChecked();
    m_cmbAutoAttRx1Mode->setEnabled(autoOn);
    m_chkAutoAttUndoRx1->setEnabled(autoOn);
    m_spnAutoAttHoldRx1->setEnabled(autoOn && m_chkAutoAttUndoRx1->isChecked());
}

} // namespace NereusSDR
