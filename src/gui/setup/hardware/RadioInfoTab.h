#pragma once
// =================================================================
// src/gui/setup/hardware/RadioInfoTab.h  (NereusSDR)
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

// RadioInfoTab.h
//
// "Radio Info" sub-tab of HardwarePage.
// Displays read-only identity info from the connected radio and lets the user
// choose sample rate and active-RX count. All controls are populated via
// populate(info, caps); before a radio is connected the fields are blank.
//
// Source: Thetis Setup.cs Hardware Config / General section — board model
// combo, sample-rate combo, active-RX count (numericUpDownNr), firmware/MAC/IP
// read-only text boxes.

#include <QWidget>
#include <QVariant>

class QLabel;
class QComboBox;
class QSpinBox;
class QPushButton;
class QFormLayout;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class RadioInfoTab : public QWidget {
    Q_OBJECT
public:
    explicit RadioInfoTab(RadioModel* model, QWidget* parent = nullptr);
    // Called by HardwarePage when the connected radio changes.
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    // Restore persisted control values (Phase 3I Task 21).
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onSampleRateChanged(int index);

private:
    RadioModel*  m_model{nullptr};

    QLabel*      m_boardLabel{nullptr};
    QLabel*      m_protocolLabel{nullptr};
    QLabel*      m_adcCountLabel{nullptr};
    QLabel*      m_maxRxLabel{nullptr};
    QLabel*      m_firmwareLabel{nullptr};
    QLabel*      m_macLabel{nullptr};
    QLabel*      m_ipLabel{nullptr};
    QComboBox*   m_sampleRateCombo{nullptr};
    QSpinBox*    m_activeRxSpin{nullptr};
    QPushButton* m_copySupportInfoButton{nullptr};

    // Cached for copy-to-clipboard
    QString m_currentInfo;
};

} // namespace NereusSDR
