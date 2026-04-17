#pragma once
// =================================================================
// src/gui/setup/hardware/PureSignalTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/PSForm.cs
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

// PureSignalTab.h
//
// "Pure Signal" sub-tab of HardwarePage.
// Exposes the PureSignal enable toggle, feedback source selector,
// auto-calibrate options, and RX feedback attenuator.
//
// Source: Thetis PSForm.cs — chkPSAutoAttenuate (line 841),
// checkLoopback (line 846), AutoCalEnabled property (line 272),
// _restoreON / _autoON state flags (lines 93-96), AutoAttenuate
// property (lines 291-311).
//
// Phase 3I: PureSignal is COLD — controls persist state via
// settingChanged(key, value) but do NOT start the PS feedback loop.
// DSP hookup is deferred to Phase 3I-4.

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QSlider;
class QStackedWidget;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class PureSignalTab : public QWidget {
    Q_OBJECT
public:
    explicit PureSignalTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private:
    RadioModel*      m_model{nullptr};

    // Two pages: 0 = unsupported notice, 1 = controls
    QStackedWidget*  m_stack{nullptr};

    // Controls page widgets (only valid when stack page == 1)
    QCheckBox*       m_enableCheck{nullptr};
    QComboBox*       m_feedbackSourceCombo{nullptr};
    QCheckBox*       m_autoCalOnBandChangeCheck{nullptr};
    QCheckBox*       m_preserveCalCheck{nullptr};
    QSlider*         m_rxFeedbackAttenSlider{nullptr};
    QLabel*          m_rxAttenValueLabel{nullptr};
};

} // namespace NereusSDR
