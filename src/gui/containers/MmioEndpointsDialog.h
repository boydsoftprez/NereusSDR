#pragma once

// =================================================================
// src/gui/containers/MmioEndpointsDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

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

#include <QDialog>
#include <QUuid>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTreeWidget;

namespace NereusSDR {

class MmioEndpoint;

// Phase 3G-6 block 5 — MMIO endpoint manager dialog. Lists all
// endpoints from ExternalVariableEngine, lets the user add /
// remove / edit endpoints inline, and shows the discovered-
// variables tree for the currently-selected endpoint.
//
// Replaces the block 3 commit 18 stub that informed the user
// "MMIO is not yet implemented."
class MmioEndpointsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MmioEndpointsDialog(QWidget* parent = nullptr);
    ~MmioEndpointsDialog() override;

private slots:
    void onEndpointSelectionChanged();
    void onAddEndpoint();
    void onRemoveEndpoint();
    void onApplyEdits();
    void onVariablesDiscovered();

private:
    void rebuildEndpointList();
    void loadEditorFromEndpoint(MmioEndpoint* ep);
    void refreshVariablesTree(MmioEndpoint* ep);
    MmioEndpoint* currentEndpoint() const;

    // Left column: endpoint list + Add/Remove
    QListWidget* m_list{nullptr};
    QPushButton* m_btnAdd{nullptr};
    QPushButton* m_btnRemove{nullptr};

    // Center column: endpoint editor
    QLineEdit*   m_editName{nullptr};
    QComboBox*   m_comboTransport{nullptr};
    QComboBox*   m_comboFormat{nullptr};
    QLineEdit*   m_editHost{nullptr};
    QSpinBox*    m_spinPort{nullptr};
    QLineEdit*   m_editDevice{nullptr};
    QSpinBox*    m_spinBaud{nullptr};
    QPushButton* m_btnApply{nullptr};
    QLabel*      m_lblStatus{nullptr};

    // Right column: discovered variables tree
    QTreeWidget* m_treeVariables{nullptr};

    // Bottom
    QPushButton* m_btnClose{nullptr};
};

} // namespace NereusSDR
