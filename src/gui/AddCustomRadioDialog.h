#pragma once
// =================================================================
// src/gui/AddCustomRadioDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmAddCustomRadio.cs
//   Project Files/Source/Console/frmAddCustomRadio.Designer.cs
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

// src/gui/AddCustomRadioDialog.h
//
// Port of Thetis frmAddCustomRadio.cs / frmAddCustomRadio.Designer.cs
//
// Thetis fields (Designer.cs):
//   comboNICS         — NIC selector (comboNICS, labelTS2 "Via NIC:")
//   txtSpecificRadio  — "Radio IP:Port" text box (default "192.168.0.155:1024")
//   comboProtocol     — "Protocol 1" / "Protocol 2"
//   txtBoard          — read-only board display
//   btnOK / btnCancel
//
// NereusSDR expands the NIC+IP:Port pair into separate fields and adds
// Name, MAC (optional), Board combo (from BoardCapsTable), and two
// convenience check boxes (Pin to MAC, Auto-connect).
//
// Source citations:
//   frmAddCustomRadio.Designer.cs:50  — labelTS2 "Via NIC:"
//   frmAddCustomRadio.Designer.cs:55  — comboNICS DropDownList
//   frmAddCustomRadio.Designer.cs:61  — txtSpecificRadio default "192.168.0.155:1024"
//   frmAddCustomRadio.Designer.cs:79  — comboProtocol items "Protocol 1" / "Protocol 2"
//   frmAddCustomRadio.Designer.cs:90  — labelTS5 "Board:", txtBoard ReadOnly
//   frmAddCustomRadio.cs:60           — comboProtocol.SelectedIndex = 0 (P1 default)
//   frmAddCustomRadio.cs:70           — Protocol property returns comboProtocol.SelectedIndex
//   frmAddCustomRadio.cs:74           — RadioIPPort property returns txtSpecificRadio.Text
//   frmAddCustomRadio.cs:77           — Board setter writes txtBoard.Text

#include "core/RadioDiscovery.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

namespace NereusSDR {

class AddCustomRadioDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddCustomRadioDialog(QWidget* parent = nullptr);
    ~AddCustomRadioDialog() override;

    // Returns the constructed RadioInfo after the user clicks OK.
    // Call only after exec() returns QDialog::Accepted.
    RadioInfo result() const;

    // Returns whether the user ticked "Pin to MAC"
    bool pinToMac() const;

    // Whether the user ticked "Auto-connect on launch"
    bool autoConnect() const;

private slots:
    void onTestClicked();
    void onAccept();
    void validateFields();
    void onBoardChanged(int index);

private:
    void buildUi();
    void populateBoardCombo();
    void populateProtocolCombo();

    QLineEdit*   m_nameEdit{nullptr};
    QLineEdit*   m_ipEdit{nullptr};
    QSpinBox*    m_portSpin{nullptr};
    QLineEdit*   m_macEdit{nullptr};
    QComboBox*   m_boardCombo{nullptr};
    QComboBox*   m_protocolCombo{nullptr};
    QCheckBox*   m_pinToMacCheck{nullptr};
    QCheckBox*   m_autoConnectCheck{nullptr};
    QPushButton* m_testButton{nullptr};
    QLabel*      m_testResultLabel{nullptr};
    QPushButton* m_okButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
};

} // namespace NereusSDR
