#pragma once

// =================================================================
// src/gui/AddCustomRadioDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/frmAddCustomRadio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/frmAddCustomRadio.Designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-27 — Phase 3Q Task 4: replaced board (HPSDRHW) dropdown with
//                 16-SKU model (HPSDRModel) picker; replaced OK/Cancel button
//                 row with Probe-and-connect / Save-offline / Cancel; added
//                 inline error/info band and probing overlay helpers. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  frmAddCustomRadio.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//
// Upstream source 'Project Files/Source/Console/frmAddCustomRadio.Designer.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

#include "core/RadioDiscovery.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QFrame;

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

    // Whether the user used "Save offline" (no probe was attempted).
    // When false the dialog accepted via a successful probe.
    bool savedOffline() const { return m_savedOffline; }

    // Pre-populate the form for editing an existing saved radio.
    // Call before exec(). Switches the title to "Edit Radio" and seeds
    // m_probedInfo so result() returns the same MAC on save (otherwise
    // the synthetic-MAC path would create a duplicate row).
    void setEditTarget(const RadioInfo& info, bool pinToMac, bool autoConnect);

private slots:
    void onProbeClicked();
    void onSaveOfflineClicked();
    void validateFields();
    void onModelChanged(int index);

private:
    void buildUi();
    void populateModelCombo();
    void populateProtocolCombo();

    // Inline feedback helpers
    void showInlineError(const QString& message);
    void showInlineInfo(const QString& message);
    void clearInlineBand();
    void showProbingOverlay();
    void hideProbingOverlay();

    // Form fields (unchanged from original)
    QLineEdit*   m_nameEdit{nullptr};
    QLineEdit*   m_ipEdit{nullptr};
    QSpinBox*    m_portSpin{nullptr};
    QLineEdit*   m_macEdit{nullptr};

    // Phase 3Q Task 4: model combo replaces board combo
    QComboBox*   m_modelCombo{nullptr};   // HPSDRModel SKU picker (objectName: "modelCombo")
    QComboBox*   m_protocolCombo{nullptr};
    QCheckBox*   m_pinToMacCheck{nullptr};
    QCheckBox*   m_autoConnectCheck{nullptr};

    // Phase 3Q Task 4: action buttons replace OK/Cancel
    QPushButton* m_probeButton{nullptr};       // objectName: "probeButton"
    QPushButton* m_saveOfflineButton{nullptr}; // objectName: "saveOfflineButton"

    // Inline feedback band (shown/hidden as needed)
    QFrame*      m_feedbackFrame{nullptr};
    QLabel*      m_feedbackLabel{nullptr};

    // Probed result — populated on successful probe, caller reads via result()
    RadioInfo    m_probedInfo;

    // Flag set when user chose "Save offline" instead of probing
    bool         m_savedOffline{false};
};

} // namespace NereusSDR
