
// =================================================================
// src/gui/containers/MmioVariablePickerPopup.cpp  (NereusSDR)
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

#include "MmioVariablePickerPopup.h"

#include "../../core/mmio/ExternalVariableEngine.h"
#include "../../core/mmio/MmioEndpoint.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>

namespace NereusSDR {

namespace {

constexpr const char* kPickerStyle =
    "QDialog { background: #0f0f1a; color: #c8d8e8; }"
    "QLabel { color: #c8d8e8; font-size: 11px; }"
    "QTreeWidget {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #203040;"
    "  alternate-background-color: #111122;"
    "}"
    "QPushButton {"
    "  background: #1a2a3a; color: #c8d8e8;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  padding: 3px 10px; min-height: 20px;"
    "}"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }";

} // namespace

MmioVariablePickerPopup::MmioVariablePickerPopup(const QUuid& initialGuid,
                                                 const QString& initialVariable,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_selectedGuid(initialGuid)
    , m_selectedVariable(initialVariable)
{
    setWindowTitle(QStringLiteral("Pick MMIO Variable"));
    setMinimumSize(420, 420);
    setStyleSheet(QLatin1String(kPickerStyle));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* lbl = new QLabel(
        QStringLiteral("Select an endpoint variable to bind this item to, "
                       "or click Clear to unbind."),
        this);
    lbl->setWordWrap(true);
    root->addWidget(lbl);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("Variable"),
                              QStringLiteral("Value")});
    m_tree->setAlternatingRowColors(true);
    root->addWidget(m_tree, 1);

    auto* btnRow = new QHBoxLayout();
    m_btnClear = new QPushButton(QStringLiteral("Clear binding"), this);
    btnRow->addWidget(m_btnClear);
    btnRow->addStretch();
    m_btnCancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_btnOk     = new QPushButton(QStringLiteral("OK"), this);
    m_btnOk->setDefault(true);
    btnRow->addWidget(m_btnCancel);
    btnRow->addWidget(m_btnOk);
    root->addLayout(btnRow);

    connect(m_btnOk,     &QPushButton::clicked, this, &MmioVariablePickerPopup::onAccept);
    connect(m_btnClear,  &QPushButton::clicked, this, &MmioVariablePickerPopup::onClear);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    buildTree();
}

MmioVariablePickerPopup::~MmioVariablePickerPopup() = default;

void MmioVariablePickerPopup::buildTree()
{
    m_tree->clear();
    const QList<MmioEndpoint*> eps = ExternalVariableEngine::instance().endpoints();
    for (MmioEndpoint* ep : eps) {
        const QString label = ep->name().isEmpty()
            ? ep->guid().toString(QUuid::WithoutBraces).left(8)
            : ep->name();
        auto* root = new QTreeWidgetItem(m_tree);
        root->setText(0, label);
        root->setData(0, Qt::UserRole, ep->guid());
        root->setData(0, Qt::UserRole + 1, QString());  // endpoint row, no var
        root->setFirstColumnSpanned(true);

        const QStringList names = ep->variableNames();
        for (const QString& n : names) {
            auto* leaf = new QTreeWidgetItem(root);
            leaf->setText(0, n);
            leaf->setText(1, ep->valueForName(n).toString());
            leaf->setData(0, Qt::UserRole, ep->guid());
            leaf->setData(0, Qt::UserRole + 1, n);

            // Pre-select the current binding.
            if (ep->guid() == m_selectedGuid && n == m_selectedVariable) {
                m_tree->setCurrentItem(leaf);
            }
        }
        root->setExpanded(true);
    }
}

void MmioVariablePickerPopup::onAccept()
{
    QTreeWidgetItem* cur = m_tree->currentItem();
    if (!cur) { reject(); return; }
    const QString varName = cur->data(0, Qt::UserRole + 1).toString();
    if (varName.isEmpty()) {
        // User highlighted an endpoint row rather than a variable leaf.
        reject();
        return;
    }
    m_selectedGuid = cur->data(0, Qt::UserRole).toUuid();
    m_selectedVariable = varName;
    m_cleared = false;
    accept();
}

void MmioVariablePickerPopup::onClear()
{
    m_selectedGuid = QUuid();
    m_selectedVariable.clear();
    m_cleared = true;
    accept();
}

} // namespace NereusSDR
