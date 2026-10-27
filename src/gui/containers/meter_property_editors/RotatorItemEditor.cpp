// =================================================================
// src/gui/containers/meter_property_editors/RotatorItemEditor.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Colour swatches show the colour drawn and say when it
//                 follows the workspace look (ItemColourLook::paintSwatch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Thetis rotator settings: beam width 0-180 in 0.05 steps
//                 and padding 0.001-1 (the Thetis Setup ranges), the beam
//                 width rows disabled when the beam is off, a full background
//                 row; setting padding lets it size the face. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Thetis rotator control rows: Allow control, control
//                 colour, the command path (rotor link or data source), the
//                 data source 4-char ID and the AZ, ELE and STOP command
//                 text with a pstRotator defaults button, enabled as Thetis
//                 does. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-26 - Final review fix wave (C1): the Thetis setup.cs header
//                 for the ported ranges and row enabling. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---
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
//============================================================================================//

// --- From setup.designer.cs ---
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level LICENSE applies (it is the designer half of the Setup class in setup.cs).

#include "RotatorItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/RotatorItem.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QString& field, const QColor& c)
{
    ItemColourLook::paintSwatch(btn, field, c);
}

constexpr const char* kComboStyle =
    "QComboBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 2px 4px; min-height: 18px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  selection-background-color: #00b4d8;"
    "}";

constexpr const char* kLineStyle =
    "QLineEdit { background: #0a0a18; color: #c8d8e8;"
    " border: 1px solid #1e2e3e; border-radius: 3px;"
    " padding: 1px 4px; min-height: 18px; }";
} // namespace

RotatorItemEditor::RotatorItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void RotatorItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    RotatorItem* r = qobject_cast<RotatorItem*>(item);
    if (!r) { return; }

    beginProgrammaticUpdate();
    m_comboMode->setCurrentIndex(m_comboMode->findData(static_cast<int>(r->mode())));
    m_chkShowValue->setChecked(r->showValue());
    m_chkShowCardinals->setChecked(r->showCardinals());
    m_chkShowBeamWidth->setChecked(r->showBeamWidth());
    m_spinBeamWidth->setValue(static_cast<double>(r->beamWidth()));
    m_spinBeamAlpha->setValue(static_cast<double>(r->beamWidthAlpha()));
    m_chkDarkMode->setChecked(r->darkMode());
    m_spinPadding->setValue(static_cast<double>(r->padding()));
    m_chkFullBackground->setChecked(r->fullBackground());
    m_editBgPath->setText(r->backgroundImagePath());
    applyBtnColor(m_btnBigBlob, QStringLiteral("Rotator.bigBlobColour"), r->bigBlobColour());
    applyBtnColor(m_btnSmallBlob, QStringLiteral("Rotator.smallBlobColour"), r->smallBlobColour());
    applyBtnColor(m_btnOuterText, QStringLiteral("Rotator.outerTextColour"), r->outerTextColour());
    applyBtnColor(m_btnArrow, QStringLiteral("Rotator.arrowColour"), r->arrowColour());
    applyBtnColor(m_btnBeamWidth, QStringLiteral("Rotator.beamWidthColour"), r->beamWidthColour());
    applyBtnColor(m_btnBackground, QStringLiteral("Rotator.backgroundColour"), r->backgroundColour());
    m_chkAllowControl->setChecked(r->allowControl());
    applyBtnColor(m_btnControlColour, QStringLiteral("Rotator.controlColour"), r->controlColour());
    m_comboCommandPath->setCurrentIndex(
        m_comboCommandPath->findData(static_cast<int>(r->commandPath())));
    const MmioEndpoint* ep = r->commandEndpointGuid().isNull()
        ? nullptr : ExternalVariableEngine::instance().endpoint(r->commandEndpointGuid());
    m_editFourChar->setText(ep ? ep->fourChar() : QString());
    m_editAzCommand->setText(r->azControlString());
    m_editEleCommand->setText(r->eleControlString());
    m_editStopCommand->setText(r->stopControlString());
    updateBeamWidthRows();
    updateControlRows();
    endProgrammaticUpdate();
}

// From Thetis setup.cs:31263-31271 [v2.10.3.15] (updateShowBeamWidthControls):
// the beam width colour, degrees and alpha are enabled only while the beam
// width is shown.
void RotatorItemEditor::updateBeamWidthRows()
{
    const bool en = m_chkShowBeamWidth->isChecked();
    for (QWidget* field : {static_cast<QWidget*>(m_spinBeamWidth), static_cast<QWidget*>(m_spinBeamAlpha),
                           static_cast<QWidget*>(m_btnBeamWidth)}) {
        if (!field) { continue; }
        field->setEnabled(en);
        if (QWidget* label = m_form->labelForField(field)) { label->setEnabled(en); }
    }
}

// From Thetis setup.cs:31321-31336 [v2.10.3.15] (updateRotatorControlControls):
// the control rows are enabled only while control is allowed. The data
// source rows (ID and command text) also need the data source path.
void RotatorItemEditor::updateControlRows()
{
    const bool en = m_chkAllowControl->isChecked();
    const bool mmio = en && m_comboCommandPath->currentData().toInt()
        == static_cast<int>(RotatorItem::CommandPath::Mmio);
    const auto enable = [this](QWidget* field, bool on) {
        if (!field) { return; }
        field->setEnabled(on);
        if (QWidget* label = m_form->labelForField(field)) { label->setEnabled(on); }
    };
    enable(m_btnControlColour, en);
    enable(m_comboCommandPath, en);
    enable(m_editFourChar, mmio);
    enable(m_editAzCommand, mmio);
    enable(m_editEleCommand, mmio);
    enable(m_editStopCommand, mmio);
    enable(m_btnPstRotator, mmio);
}

void RotatorItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Rotator"));

    m_comboMode = new QComboBox(this);
    m_comboMode->setStyleSheet(kComboStyle);
    m_comboMode->addItem(QStringLiteral("Azimuth"),   static_cast<int>(RotatorItem::RotatorMode::Az));
    m_comboMode->addItem(QStringLiteral("Elevation"), static_cast<int>(RotatorItem::RotatorMode::Ele));
    m_comboMode->addItem(QStringLiteral("Both"),      static_cast<int>(RotatorItem::RotatorMode::Both));
    addRow(QStringLiteral("Mode"), m_comboMode);
    connect(m_comboMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setMode(static_cast<RotatorItem::RotatorMode>(m_comboMode->currentData().toInt()));
        notifyChanged();
    });

    m_chkShowValue = makeCheckRow(QStringLiteral("Show value"));
    connect(m_chkShowValue, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setShowValue(on);
        notifyChanged();
    });

    m_chkShowCardinals = makeCheckRow(QStringLiteral("Show cardinals"));
    connect(m_chkShowCardinals, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setShowCardinals(on);
        notifyChanged();
    });

    m_chkShowBeamWidth = makeCheckRow(QStringLiteral("Show beam width"));
    m_chkShowBeamWidth->setObjectName(QStringLiteral("rotatorShowBeamWidth"));
    connect(m_chkShowBeamWidth, &QCheckBox::toggled, this, [this](bool on) {
        updateBeamWidthRows();
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setShowBeamWidth(on);
        notifyChanged();
    });

    // From Thetis setup.designer.cs:64527-64556 [v2.10.3.15]
    // (nudMeterItemRotatorBeamWidth): 0 to 180, step 0.05, one decimal.
    m_spinBeamWidth = makeDoubleRow(QStringLiteral("Beam width (deg)"), 0.0, 180.0, 0.05, 1);
    m_spinBeamWidth->setObjectName(QStringLiteral("rotatorBeamWidth"));
    connect(m_spinBeamWidth, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setBeamWidth(static_cast<float>(v));
        notifyChanged();
    });

    m_spinBeamAlpha = makeDoubleRow(QStringLiteral("Beam alpha"), 0.0, 1.0, 0.05, 2);
    m_spinBeamAlpha->setObjectName(QStringLiteral("rotatorBeamAlpha"));
    connect(m_spinBeamAlpha, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setBeamWidthAlpha(static_cast<float>(v));
        notifyChanged();
    });

    m_chkDarkMode = makeCheckRow(QStringLiteral("Dark mode"));
    connect(m_chkDarkMode, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setDarkMode(on);
        notifyChanged();
    });

    // From Thetis setup.designer.cs:64344-64373 [v2.10.3.15]
    // (nudMeterItemRotator_padding): 0.001 to 1, step 0.001, three decimals.
    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.001, 1.0, 0.001, 3);
    m_spinPadding->setObjectName(QStringLiteral("rotatorPadding"));
    m_spinPadding->setToolTip(QStringLiteral(
        "Size of the azimuth or elevation face, as a share of the item's width."));
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setPadding(static_cast<float>(v));
        // An older item starts sizing its face by padding once it is set.
        r->setPaddingLayout(true);
        notifyChanged();
    });

    m_chkFullBackground = makeCheckRow(QStringLiteral("Full background"));
    m_chkFullBackground->setObjectName(QStringLiteral("rotatorFullBackground"));
    m_chkFullBackground->setToolTip(QStringLiteral(
        "Fill the whole item with the background colour, not only the compass."));
    connect(m_chkFullBackground, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setFullBackground(on);
        notifyChanged();
    });

    // Background image path
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);
    m_editBgPath = new QLineEdit(rowWidget);
    m_editBgPath->setStyleSheet(kLineStyle);
    rowLayout->addWidget(m_editBgPath, 1);
    auto* btnBrowse = new QPushButton(QStringLiteral("…"), rowWidget);
    btnBrowse->setFixedWidth(24);
    rowLayout->addWidget(btnBrowse);
    addRow(QStringLiteral("BG image"), rowWidget);

    connect(m_editBgPath, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setBackgroundImagePath(m_editBgPath->text());
        notifyChanged();
    });
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Select background image"), QString(),
            QStringLiteral("Images (*.png *.jpg *.bmp)"));
        if (!path.isEmpty()) {
            m_editBgPath->setText(path);
            RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
            if (r) { r->setBackgroundImagePath(path); notifyChanged(); }
        }
    });

    addHeader(QStringLiteral("Colors"));

    auto makeColorBtn = [this](const QString& label, QPushButton*& out,
                               auto setter, const QString& field) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        QPushButton* btn = out;
        connect(btn, &QPushButton::clicked, this, [this, btn, setter, field]() {
            RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
            if (!r) { return; }
            const QColor chosen = QColorDialog::getColor(
                Qt::gray, this, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                (r->*setter)(chosen);
                applyBtnColor(btn, field, chosen);
                notifyChanged();
            }
        });
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Large dot"),    m_btnBigBlob,   &RotatorItem::setBigBlobColour, QStringLiteral("Rotator.bigBlobColour"));
    makeColorBtn(QStringLiteral("Small dot"),    m_btnSmallBlob, &RotatorItem::setSmallBlobColour, QStringLiteral("Rotator.smallBlobColour"));
    makeColorBtn(QStringLiteral("Outer text"),   m_btnOuterText, &RotatorItem::setOuterTextColour, QStringLiteral("Rotator.outerTextColour"));
    makeColorBtn(QStringLiteral("Arrow"),        m_btnArrow,     &RotatorItem::setArrowColour, QStringLiteral("Rotator.arrowColour"));
    makeColorBtn(QStringLiteral("Beam width"),   m_btnBeamWidth, &RotatorItem::setBeamWidthColour, QStringLiteral("Rotator.beamWidthColour"));
    makeColorBtn(QStringLiteral("Background"),   m_btnBackground,&RotatorItem::setBackgroundColour, QStringLiteral("Rotator.backgroundColour"));
    m_btnBeamWidth->setObjectName(QStringLiteral("rotatorBeamWidthColour"));

    addHeader(QStringLiteral("Control"));

    m_chkAllowControl = makeCheckRow(QStringLiteral("Allow control"));
    m_chkAllowControl->setObjectName(QStringLiteral("rotatorAllowControl"));
    m_chkAllowControl->setToolTip(QStringLiteral(
        "Let a drag on the dial turn the rotor and a click on its centre stop it."));
    connect(m_chkAllowControl, &QCheckBox::toggled, this, [this](bool on) {
        updateControlRows();
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setAllowControl(on);
        notifyChanged();
    });

    makeColorBtn(QStringLiteral("Control color"), m_btnControlColour,
                 &RotatorItem::setControlColour, QStringLiteral("Rotator.controlColour"));
    m_btnControlColour->setObjectName(QStringLiteral("rotatorControlColour"));

    m_comboCommandPath = new QComboBox(this);
    m_comboCommandPath->setObjectName(QStringLiteral("rotatorCommandPath"));
    m_comboCommandPath->setStyleSheet(kComboStyle);
    m_comboCommandPath->addItem(QStringLiteral("Rotor link"),
                                static_cast<int>(RotatorItem::CommandPath::RotorLink));
    m_comboCommandPath->addItem(QStringLiteral("Data source (MMIO)"),
                                static_cast<int>(RotatorItem::CommandPath::Mmio));
    m_comboCommandPath->setToolTip(QStringLiteral(
        "Rotor link turns the rotor set up in this app. Data source sends the command "
        "text below to a meter data source, such as PstRotator."));
    addRow(QStringLiteral("Send commands by"), m_comboCommandPath);
    connect(m_comboCommandPath, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        updateControlRows();
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setCommandPath(static_cast<RotatorItem::CommandPath>(
            m_comboCommandPath->currentData().toInt()));
        notifyChanged();
    });

    // The data source the commands go to, by its 4-char ID (Thetis
    // txtRotator_4charID). An ID no data source has sends nothing.
    m_editFourChar = new QLineEdit(this);
    m_editFourChar->setObjectName(QStringLiteral("rotatorFourCharId"));
    m_editFourChar->setStyleSheet(kLineStyle);
    m_editFourChar->setMaxLength(4);
    m_editFourChar->setPlaceholderText(QStringLiteral("ID"));
    m_editFourChar->setToolTip(QStringLiteral(
        "The 4-character ID of the meter data source the rotor commands are sent to"));
    addRow(QStringLiteral("Data source 4-char ID"), m_editFourChar);
    connect(m_editFourChar, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        const MmioEndpoint* ep =
            ExternalVariableEngine::instance().endpointByFourChar(m_editFourChar->text().trimmed());
        const QUuid guid = ep ? ep->guid() : QUuid();
        if (guid == r->commandEndpointGuid()) { return; }
        r->setCommandEndpointGuid(guid);
        notifyChanged();
    });

    const QString commandTip = QStringLiteral(
        "%AZ% is replaced by the azimuth in whole degrees (007) and %ELE% by the elevation "
        "(05). Type \\r, \\n or \\0 to send a carriage return, line feed or null.");
    auto makeCommandRow = [this, &commandTip](const QString& label, const QString& name,
                                              QLineEdit*& out, auto getter, auto setter) {
        out = new QLineEdit(this);
        out->setObjectName(name);
        out->setStyleSheet(kLineStyle);
        out->setToolTip(commandTip);
        QLineEdit* edit = out;
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, getter, setter]() {
            if (isProgrammaticUpdate()) { return; }
            RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
            if (!r || (r->*getter)() == edit->text()) { return; }
            (r->*setter)(edit->text());
            notifyChanged();
        });
        addRow(label, out);
    };
    makeCommandRow(QStringLiteral("Azimuth command"), QStringLiteral("rotatorAzCommand"),
                   m_editAzCommand, &RotatorItem::azControlString, &RotatorItem::setAzControlString);
    makeCommandRow(QStringLiteral("Elevation command"), QStringLiteral("rotatorEleCommand"),
                   m_editEleCommand, &RotatorItem::eleControlString, &RotatorItem::setEleControlString);
    makeCommandRow(QStringLiteral("Stop command"), QStringLiteral("rotatorStopCommand"),
                   m_editStopCommand, &RotatorItem::stopControlString, &RotatorItem::setStopControlString);

    // From Thetis setup.cs:31337-31342 [v2.10.3.15]
    // (bntMultiMeterItemRotator_default_pstRotator_Click): the pstRotator
    // command text.
    m_btnPstRotator = new QPushButton(QStringLiteral("PstRotator defaults"), this);
    m_btnPstRotator->setObjectName(QStringLiteral("rotatorPstRotatorDefaults"));
    m_btnPstRotator->setToolTip(QStringLiteral("Fill in the PstRotator command text."));
    addRow(QString(), m_btnPstRotator);
    connect(m_btnPstRotator, &QPushButton::clicked, this, [this]() {
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setAzControlString(QString::fromLatin1(RotatorItem::kPstRotatorAzCommand));
        r->setEleControlString(QString::fromLatin1(RotatorItem::kPstRotatorEleCommand));
        r->setStopControlString(QString::fromLatin1(RotatorItem::kPstRotatorStopCommand));
        beginProgrammaticUpdate();
        m_editAzCommand->setText(r->azControlString());
        m_editEleCommand->setText(r->eleControlString());
        m_editStopCommand->setText(r->stopControlString());
        endProgrammaticUpdate();
        notifyChanged();
    });
    updateBeamWidthRows();
    updateControlRows();
}

} // namespace NereusSDR
