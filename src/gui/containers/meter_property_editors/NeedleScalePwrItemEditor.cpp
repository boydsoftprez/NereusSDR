// =================================================================
// src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.cpp  (NereusSDR)
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

#include "NeedleScalePwrItemEditor.h"
#include "../../meters/MeterItem.h"
#include "../../meters/NeedleScalePwrItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFontComboBox>
#include <QColorDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>

namespace NereusSDR {

namespace {

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

constexpr const char* kTableStyle =
    "QTableWidget {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; gridline-color: #1e2e3e;"
    "}"
    "QTableWidget::item:selected {"
    "  background: #00b4d8; color: #0a0a18;"
    "}"
    "QHeaderView::section {"
    "  background: #1a2a38; color: #8aa8c0; font-size: 10px;"
    "  border: 1px solid #1e2e3e; padding: 2px;"
    "}";

constexpr const char* kBtnStyle =
    "QPushButton {"
    "  background: #1a2a38; color: #c8d8e8;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  padding: 1px 6px; min-height: 18px;"
    "}";

} // namespace

NeedleScalePwrItemEditor::NeedleScalePwrItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void NeedleScalePwrItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(item);
    if (!nspi) { return; }

    beginProgrammaticUpdate();

    // Colors
    auto applyColor = [](QPushButton* btn, const QColor& c) {
        btn->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    applyColor(m_btnLowColour,  nspi->lowColour());
    applyColor(m_btnHighColour, nspi->highColour());

    // Font
    m_fontCombo->setCurrentFont(QFont(nspi->fontFamily()));
    m_spinFontSize->setValue(static_cast<double>(nspi->fontSize()));
    m_chkFontBold->setChecked(nspi->fontBold());

    // Scale marks
    m_spinMarks->setValue(nspi->marks());
    m_chkShowMarkers->setChecked(nspi->showMarkers());

    // Power / dark mode
    m_spinMaxPower->setValue(static_cast<double>(nspi->maxPower()));
    m_chkDarkMode->setChecked(nspi->darkMode());

    // Direction
    m_comboDirection->setCurrentIndex(
        nspi->direction() == NeedleScalePwrItem::Direction::CounterClockwise ? 1 : 0);

    // Geometry
    m_spinOffsetX->setValue(nspi->needleOffset().x());
    m_spinOffsetY->setValue(nspi->needleOffset().y());
    m_spinLengthFactor->setValue(static_cast<double>(nspi->lengthFactor()));

    // Calibration table
    refreshCalTable(nspi);

    endProgrammaticUpdate();
}

void NeedleScalePwrItemEditor::refreshCalTable(NeedleScalePwrItem* nspi)
{
    if (!nspi) { return; }
    const QMap<float, QPointF>& cal = nspi->scaleCalibration();

    m_calTable->setRowCount(0);
    m_calTable->setRowCount(cal.size());
    int row = 0;
    for (auto it = cal.cbegin(); it != cal.cend(); ++it, ++row) {
        m_calTable->setItem(row, 0,
            new QTableWidgetItem(QString::number(static_cast<double>(it.key()), 'f', 3)));
        m_calTable->setItem(row, 1,
            new QTableWidgetItem(QString::number(it.value().x(), 'f', 4)));
        m_calTable->setItem(row, 2,
            new QTableWidgetItem(QString::number(it.value().y(), 'f', 4)));
    }
}

void NeedleScalePwrItemEditor::commitCalTable(NeedleScalePwrItem* nspi)
{
    if (!nspi) { return; }
    QMap<float, QPointF> cal;
    for (int row = 0; row < m_calTable->rowCount(); ++row) {
        QTableWidgetItem* valItem = m_calTable->item(row, 0);
        QTableWidgetItem* xItem   = m_calTable->item(row, 1);
        QTableWidgetItem* yItem   = m_calTable->item(row, 2);
        if (!valItem || !xItem || !yItem) { continue; }
        bool okV = false, okX = false, okY = false;
        const float val = valItem->text().toFloat(&okV);
        const double nx = xItem->text().toDouble(&okX);
        const double ny = yItem->text().toDouble(&okY);
        if (okV && okX && okY) {
            cal.insert(val, QPointF(nx, ny));
        }
    }
    nspi->setScaleCalibration(cal);
}

void NeedleScalePwrItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Needle Scale Power"));

    // ---- Colors ----
    m_btnLowColour = new QPushButton(this);
    m_btnLowColour->setFixedSize(40, 18);
    connect(m_btnLowColour, &QPushButton::clicked, this, [this]() {
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        const QColor chosen = QColorDialog::getColor(nspi->lowColour(), this,
                                                     QStringLiteral("Low color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            nspi->setLowColour(chosen);
            m_btnLowColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Low color"), m_btnLowColour);

    m_btnHighColour = new QPushButton(this);
    m_btnHighColour->setFixedSize(40, 18);
    connect(m_btnHighColour, &QPushButton::clicked, this, [this]() {
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        const QColor chosen = QColorDialog::getColor(nspi->highColour(), this,
                                                     QStringLiteral("High color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            nspi->setHighColour(chosen);
            m_btnHighColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("High color"), m_btnHighColour);

    addHeader(QStringLiteral("Font"));

    // ---- Font family ----
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setStyleSheet(kComboStyle);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& font) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setFontFamily(font.family());
        notifyChanged();
    });
    addRow(QStringLiteral("Font family"), m_fontCombo);

    // ---- Font size ----
    m_spinFontSize = makeDoubleRow(QStringLiteral("Font size"), 4.0, 200.0, 1.0, 1);
    connect(m_spinFontSize, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setFontSize(static_cast<float>(v));
        notifyChanged();
    });

    // ---- Font bold ----
    m_chkFontBold = makeCheckRow(QStringLiteral("Font bold"));
    connect(m_chkFontBold, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setFontBold(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Scale Marks"));

    // ---- Marks ----
    m_spinMarks = makeIntRow(QStringLiteral("Marks"), 2, 20);
    connect(m_spinMarks, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setMarks(v);
        notifyChanged();
    });

    // ---- Show markers ----
    m_chkShowMarkers = makeCheckRow(QStringLiteral("Show markers"));
    connect(m_chkShowMarkers, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setShowMarkers(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Power"));

    // ---- Max power ----
    m_spinMaxPower = makeDoubleRow(QStringLiteral("Max power (W)"), 1.0, 10000.0, 1.0, 1);
    connect(m_spinMaxPower, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setMaxPower(static_cast<float>(v));
        notifyChanged();
    });

    // ---- Dark mode ----
    m_chkDarkMode = makeCheckRow(QStringLiteral("Dark mode"));
    connect(m_chkDarkMode, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setDarkMode(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Geometry"));

    // ---- Direction ----
    m_comboDirection = new QComboBox(this);
    m_comboDirection->setStyleSheet(kComboStyle);
    m_comboDirection->addItem(QStringLiteral("Clockwise"),
                              static_cast<int>(NeedleScalePwrItem::Direction::Clockwise));
    m_comboDirection->addItem(QStringLiteral("Counter-clockwise"),
                              static_cast<int>(NeedleScalePwrItem::Direction::CounterClockwise));
    connect(m_comboDirection, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setDirection(
            static_cast<NeedleScalePwrItem::Direction>(m_comboDirection->currentData().toInt()));
        notifyChanged();
    });
    addRow(QStringLiteral("Direction"), m_comboDirection);

    // ---- Needle offset X/Y ----
    m_spinOffsetX = makeDoubleRow(QStringLiteral("Offset X"), -2.0, 2.0, 0.001, 4);
    connect(m_spinOffsetX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setNeedleOffset(QPointF(v, m_spinOffsetY->value()));
        notifyChanged();
    });

    m_spinOffsetY = makeDoubleRow(QStringLiteral("Offset Y"), -2.0, 2.0, 0.001, 4);
    connect(m_spinOffsetY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setNeedleOffset(QPointF(m_spinOffsetX->value(), v));
        notifyChanged();
    });

    // ---- Length factor ----
    m_spinLengthFactor = makeDoubleRow(QStringLiteral("Length factor"), 0.1, 5.0, 0.01, 3);
    connect(m_spinLengthFactor, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        nspi->setLengthFactor(static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Calibration"));

    // ---- Calibration table ----
    m_calTable = new QTableWidget(0, 3, this);
    m_calTable->setStyleSheet(kTableStyle);
    m_calTable->setHorizontalHeaderLabels({
        QStringLiteral("Value"),
        QStringLiteral("Norm X"),
        QStringLiteral("Norm Y")
    });
    m_calTable->horizontalHeader()->setStretchLastSection(true);
    m_calTable->verticalHeader()->setVisible(false);
    m_calTable->setMinimumHeight(120);
    m_calTable->setMaximumHeight(240);

    connect(m_calTable, &QTableWidget::cellChanged, this, [this](int, int) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        commitCalTable(nspi);
        notifyChanged();
    });

    // Add / Remove row buttons
    auto* calBtns = new QWidget(this);
    auto* btnLayout = new QHBoxLayout(calBtns);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);

    auto* btnAdd = new QPushButton(QStringLiteral("Add row"), calBtns);
    btnAdd->setStyleSheet(kBtnStyle);
    connect(btnAdd, &QPushButton::clicked, this, [this]() {
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        const int row = m_calTable->rowCount();
        beginProgrammaticUpdate();
        m_calTable->insertRow(row);
        m_calTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("0.000")));
        m_calTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0.5000")));
        m_calTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0.5000")));
        endProgrammaticUpdate();
        commitCalTable(nspi);
        notifyChanged();
    });
    btnLayout->addWidget(btnAdd);

    auto* btnRemove = new QPushButton(QStringLiteral("Remove row"), calBtns);
    btnRemove->setStyleSheet(kBtnStyle);
    connect(btnRemove, &QPushButton::clicked, this, [this]() {
        NeedleScalePwrItem* nspi = qobject_cast<NeedleScalePwrItem*>(m_item);
        if (!nspi) { return; }
        const int row = m_calTable->currentRow();
        if (row < 0) { return; }
        beginProgrammaticUpdate();
        m_calTable->removeRow(row);
        endProgrammaticUpdate();
        commitCalTable(nspi);
        notifyChanged();
    });
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();

    auto* calContainer = new QWidget(this);
    auto* calLayout = new QVBoxLayout(calContainer);
    calLayout->setContentsMargins(0, 0, 0, 0);
    calLayout->setSpacing(2);
    calLayout->addWidget(m_calTable);
    calLayout->addWidget(calBtns);

    addRow(QStringLiteral("Cal. points"), calContainer);
}

} // namespace NereusSDR
