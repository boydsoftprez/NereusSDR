#pragma once

// =================================================================
// src/gui/containers/MmioVariablePickerPopup.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
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

#include <QDialog>
#include <QUuid>
#include <QString>

class QTreeWidget;
class QPushButton;

namespace NereusSDR {

// Phase 3G-6 block 5 — popup that lets the user pick an MMIO
// (endpoint, variableName) pair to bind a meter item to. Opened
// by the "Variable…" button next to the Binding row in each
// BaseItemEditor subclass. Matches Thetis frmVariablePicker
// (Console/frmVariablePicker.cs) semantics: OK writes the chosen
// pair back to the caller, Clear resets to null guid + empty name
// (Thetis sentinel "--DEFAULT--"), Cancel leaves the caller's
// state untouched.
class MmioVariablePickerPopup : public QDialog {
    Q_OBJECT

public:
    // Construct pre-selecting an existing (guid, name) pair.
    // Pass null guid and empty name to start with no selection.
    MmioVariablePickerPopup(const QUuid& initialGuid,
                            const QString& initialVariable,
                            QWidget* parent = nullptr);
    ~MmioVariablePickerPopup() override;

    // Outputs — call these after exec() returns Accepted.
    QUuid   selectedGuid() const { return m_selectedGuid; }
    QString selectedVariable() const { return m_selectedVariable; }
    bool    wasCleared() const { return m_cleared; }

private slots:
    void onAccept();
    void onClear();

private:
    void buildTree();

    QTreeWidget* m_tree{nullptr};
    QPushButton* m_btnOk{nullptr};
    QPushButton* m_btnClear{nullptr};
    QPushButton* m_btnCancel{nullptr};

    QUuid   m_selectedGuid;
    QString m_selectedVariable;
    bool    m_cleared{false};
};

} // namespace NereusSDR
