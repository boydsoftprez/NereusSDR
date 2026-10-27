// =================================================================
// src/gui/containers/meter_property_editors/ClockItemEditor.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Colour swatches show the colour drawn and say when it
//                 follows the workspace look (ItemColourLook::paintSwatch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Layout and background rows; the title colour row is
//                 enabled only while the title is shown, as Thetis setup's
//                 updateTitleControlsClock does. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "ClockItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/ClockItem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QString& field, const QColor& c)
{
    ItemColourLook::paintSwatch(btn, field, c);
}
} // namespace

ClockItemEditor::ClockItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void ClockItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    ClockItem* c = qobject_cast<ClockItem*>(item);
    if (!c) { return; }

    beginProgrammaticUpdate();
    m_chkShow24Hour->setChecked(c->show24Hour());
    m_chkShowType->setChecked(c->showType());
    m_comboLayout->setCurrentIndex(m_comboLayout->findData(static_cast<int>(c->layout())));
    m_chkBackEnabled->setChecked(c->backEnabled());
    applyBtnColor(m_btnTimeColour, QStringLiteral("Clock.timeColour"), c->timeColour());
    applyBtnColor(m_btnDateColour, QStringLiteral("Clock.dateColour"), c->dateColour());
    applyBtnColor(m_btnTitleColour, QStringLiteral("Clock.typeTitleColour"), c->typeTitleColour());
    applyBtnColor(m_btnBackColour, QStringLiteral("Clock.backColour"), c->backColour());
    // From Thetis setup.cs:26468-26471 [v2.10.3.15] (updateTitleControlsClock)
    m_btnTitleColour->setEnabled(c->showType());
    m_btnBackColour->setEnabled(c->backEnabled());
    endProgrammaticUpdate();
}

void ClockItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Clock"));

    m_comboLayout = new QComboBox(this);
    m_comboLayout->setObjectName(QStringLiteral("clockLayout"));
    m_comboLayout->addItem(QStringLiteral("Centered"), static_cast<int>(ClockItem::Layout::NereusSDR));
    m_comboLayout->addItem(QStringLiteral("Large time"), static_cast<int>(ClockItem::Layout::Thetis));
    connect(m_comboLayout, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        c->setLayout(static_cast<ClockItem::Layout>(m_comboLayout->currentData().toInt()));
        notifyChanged();
    });
    addRow(QStringLiteral("Layout"), m_comboLayout);

    m_chkShow24Hour = makeCheckRow(QStringLiteral("24-hour format"));
    connect(m_chkShow24Hour, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        c->setShow24Hour(on);
        notifyChanged();
    });

    m_chkShowType = makeCheckRow(QStringLiteral("Show UTC/local label"));
    m_chkShowType->setObjectName(QStringLiteral("clockShowType"));
    connect(m_chkShowType, &QCheckBox::toggled, this, [this](bool on) {
        // The title colour is enabled only while the title is shown.
        // From Thetis setup.cs:26468-26471 [v2.10.3.15] (updateTitleControlsClock)
        m_btnTitleColour->setEnabled(on);
        if (isProgrammaticUpdate()) { return; }
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        c->setShowType(on);
        notifyChanged();
    });

    m_chkBackEnabled = makeCheckRow(QStringLiteral("Background"));
    m_chkBackEnabled->setObjectName(QStringLiteral("clockBackEnabled"));
    connect(m_chkBackEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_btnBackColour->setEnabled(on);
        if (isProgrammaticUpdate()) { return; }
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        c->setBackEnabled(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Colors"));

    m_btnTimeColour = new QPushButton(this);
    m_btnTimeColour->setFixedSize(40, 18);
    connect(m_btnTimeColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::white, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { markColourPicked(QStringLiteral("Clock.timeColour")); c->setTimeColour(chosen); applyBtnColor(m_btnTimeColour, QStringLiteral("Clock.timeColour"), chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Time color"), m_btnTimeColour);

    m_btnDateColour = new QPushButton(this);
    m_btnDateColour->setFixedSize(40, 18);
    connect(m_btnDateColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::gray, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { markColourPicked(QStringLiteral("Clock.dateColour")); c->setDateColour(chosen); applyBtnColor(m_btnDateColour, QStringLiteral("Clock.dateColour"), chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Date color"), m_btnDateColour);

    m_btnTitleColour = new QPushButton(this);
    m_btnTitleColour->setObjectName(QStringLiteral("clockTitleColour"));
    m_btnTitleColour->setFixedSize(40, 18);
    connect(m_btnTitleColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::darkGray, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { markColourPicked(QStringLiteral("Clock.typeTitleColour")); c->setTypeTitleColour(chosen); applyBtnColor(m_btnTitleColour, QStringLiteral("Clock.typeTitleColour"), chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Title color"), m_btnTitleColour);

    m_btnBackColour = new QPushButton(this);
    m_btnBackColour->setObjectName(QStringLiteral("clockBackColour"));
    m_btnBackColour->setFixedSize(40, 18);
    connect(m_btnBackColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            c->backColour(), this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { markColourPicked(QStringLiteral("Clock.backColour")); c->setBackColour(chosen); applyBtnColor(m_btnBackColour, QStringLiteral("Clock.backColour"), chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Background color"), m_btnBackColour);
}

} // namespace NereusSDR
