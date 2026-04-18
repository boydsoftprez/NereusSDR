// =================================================================
// src/gui/containers/meter_property_editors/NeedleItemEditor.cpp  (NereusSDR)
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

#include "NeedleItemEditor.h"
#include "../../meters/MeterItem.h"

#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>

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

// Phase 3G-7 polish C: dark-theme group box style for the 5 needle
// editor sections. Border + title chip mirror the applet style guide
// so the editor reads like the rest of the app.
constexpr const char* kGroupStyle =
    "QGroupBox {"
    "  color: #8aa8c0; font-weight: bold; font-size: 11px;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  margin-top: 8px; padding: 6px 4px 4px 4px;"
    "}"
    "QGroupBox::title {"
    "  subcontrol-origin: margin; subcontrol-position: top left;"
    "  background: #1a2a38; padding: 0 4px; left: 6px;"
    "}";

constexpr const char* kSpinStyle =
    "QDoubleSpinBox, QSpinBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 1px 4px; min-height: 18px;"
    "}";

constexpr const char* kCheckStyle = "QCheckBox { color: #c8d8e8; }";
constexpr const char* kLabelStyle = "QLabel { color: #8090a0; font-size: 10px; }";

constexpr int kFieldWidth = 140;

} // namespace

NeedleItemEditor::NeedleItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void NeedleItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    NeedleItem* needle = qobject_cast<NeedleItem*>(item);
    if (!needle) { return; }

    beginProgrammaticUpdate();

    m_editSourceLabel->setText(needle->sourceLabel());

    // Needle color button
    const QColor nc = needle->needleColor();
    m_btnNeedleColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(nc.name(QColor::HexArgb)));

    m_spinAttack->setValue(static_cast<double>(needle->attackRatio()));
    m_spinDecay->setValue(static_cast<double>(needle->decayRatio()));
    m_spinLengthFactor->setValue(static_cast<double>(needle->lengthFactor()));
    m_spinOffsetX->setValue(needle->needleOffset().x());
    m_spinOffsetY->setValue(needle->needleOffset().y());
    m_spinRadiusX->setValue(needle->radiusRatio().x());
    m_spinRadiusY->setValue(needle->radiusRatio().y());
    m_spinStrokeWidth->setValue(static_cast<double>(needle->strokeWidth()));

    m_comboDirection->setCurrentIndex(
        needle->direction() == NeedleItem::NeedleDirection::CounterClockwise ? 1 : 0);

    m_chkHistory->setChecked(needle->historyEnabled());
    m_spinHistoryDuration->setValue(needle->historyDuration());

    const QColor hc = needle->historyColor();
    m_btnHistoryColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(hc.name(QColor::HexArgb)));

    m_chkNormaliseTo100W->setChecked(needle->normaliseTo100W());
    m_spinMaxPower->setValue(static_cast<double>(needle->maxPower()));

    refreshCalTable(needle);

    endProgrammaticUpdate();
}

void NeedleItemEditor::refreshCalTable(NeedleItem* needle)
{
    if (!needle) { return; }
    const QMap<float, QPointF>& cal = needle->scaleCalibration();

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

void NeedleItemEditor::commitCalTable(NeedleItem* needle)
{
    if (!needle) { return; }
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
    needle->setScaleCalibration(cal);
}

void NeedleItemEditor::buildTypeSpecific()
{
    // Phase 3G-7 polish C: wrap the type-specific section in 5 group
    // boxes so the 17 needle-specific controls + the calibration table
    // read as discrete sections instead of a flat stack of headers.
    //
    // Each group has its own QFormLayout. The five groups are appended
    // to the inherited m_form as wide rows, sitting below the base
    // "Common" section.

    auto styleSpin = [](QDoubleSpinBox* s, double lo, double hi, double step, int dec) {
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setDecimals(dec);
        s->setStyleSheet(kSpinStyle);
        s->setMinimumWidth(kFieldWidth);
    };
    auto styleSpinI = [](QSpinBox* s, int lo, int hi) {
        s->setRange(lo, hi);
        s->setStyleSheet(kSpinStyle);
        s->setMinimumWidth(kFieldWidth);
    };
    auto addLabeled = [](QFormLayout* form, const QString& label, QWidget* w) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(kLabelStyle);
        form->addRow(lbl, w);
    };
    auto makeGroup = [this](const QString& title, QFormLayout*& outForm) -> QGroupBox* {
        auto* g = new QGroupBox(title, this);
        g->setStyleSheet(kGroupStyle);
        outForm = new QFormLayout(g);
        outForm->setContentsMargins(6, 12, 6, 6);
        outForm->setSpacing(3);
        outForm->setLabelAlignment(Qt::AlignRight);
        return g;
    };

    // ===== Group 1: Needle =====
    QFormLayout* needleForm = nullptr;
    QGroupBox* needleGroup = makeGroup(QStringLiteral("Needle"), needleForm);

    m_editSourceLabel = new QLineEdit(needleGroup);
    m_editSourceLabel->setStyleSheet(
        QStringLiteral("QLineEdit {"
                        "  background: #0a0a18; color: #c8d8e8;"
                        "  border: 1px solid #1e2e3e; border-radius: 3px;"
                        "  padding: 1px 4px; min-height: 18px;"
                        "}"));
    m_editSourceLabel->setMinimumWidth(kFieldWidth);
    connect(m_editSourceLabel, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setSourceLabel(m_editSourceLabel->text());
        notifyChanged();
    });
    addLabeled(needleForm, QStringLiteral("Source label"), m_editSourceLabel);

    m_btnNeedleColor = new QPushButton(needleGroup);
    m_btnNeedleColor->setFixedSize(40, 18);
    connect(m_btnNeedleColor, &QPushButton::clicked, this, [this]() {
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        const QColor chosen = QColorDialog::getColor(
            needle->needleColor(), this, QStringLiteral("Needle color"),
            QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            needle->setNeedleColor(chosen);
            m_btnNeedleColor->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addLabeled(needleForm, QStringLiteral("Needle color"), m_btnNeedleColor);

    m_spinAttack = new QDoubleSpinBox(needleGroup);
    styleSpin(m_spinAttack, 0.0, 1.0, 0.01, 3);
    connect(m_spinAttack, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setAttackRatio(static_cast<float>(v));
        notifyChanged();
    });
    addLabeled(needleForm, QStringLiteral("Attack ratio"), m_spinAttack);

    m_spinDecay = new QDoubleSpinBox(needleGroup);
    styleSpin(m_spinDecay, 0.0, 1.0, 0.01, 3);
    connect(m_spinDecay, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setDecayRatio(static_cast<float>(v));
        notifyChanged();
    });
    addLabeled(needleForm, QStringLiteral("Decay ratio"), m_spinDecay);

    m_comboDirection = new QComboBox(needleGroup);
    m_comboDirection->setStyleSheet(kComboStyle);
    m_comboDirection->setMinimumWidth(kFieldWidth);
    m_comboDirection->addItem(QStringLiteral("Clockwise"),
                              static_cast<int>(NeedleItem::NeedleDirection::Clockwise));
    m_comboDirection->addItem(QStringLiteral("Counter-clockwise"),
                              static_cast<int>(NeedleItem::NeedleDirection::CounterClockwise));
    connect(m_comboDirection, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setDirection(
            static_cast<NeedleItem::NeedleDirection>(m_comboDirection->currentData().toInt()));
        notifyChanged();
    });
    addLabeled(needleForm, QStringLiteral("Direction"), m_comboDirection);

    m_form->addRow(needleGroup);

    // ===== Group 2: Geometry =====
    QFormLayout* geomForm = nullptr;
    QGroupBox* geomGroup = makeGroup(QStringLiteral("Geometry"), geomForm);

    m_spinLengthFactor = new QDoubleSpinBox(geomGroup);
    styleSpin(m_spinLengthFactor, 0.1, 5.0, 0.01, 3);
    connect(m_spinLengthFactor, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setLengthFactor(static_cast<float>(v));
        notifyChanged();
    });
    addLabeled(geomForm, QStringLiteral("Length factor"), m_spinLengthFactor);

    m_spinOffsetX = new QDoubleSpinBox(geomGroup);
    styleSpin(m_spinOffsetX, -2.0, 2.0, 0.001, 4);
    connect(m_spinOffsetX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setNeedleOffset(QPointF(v, m_spinOffsetY->value()));
        notifyChanged();
    });
    addLabeled(geomForm, QStringLiteral("Offset X"), m_spinOffsetX);

    m_spinOffsetY = new QDoubleSpinBox(geomGroup);
    styleSpin(m_spinOffsetY, -2.0, 2.0, 0.001, 4);
    connect(m_spinOffsetY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setNeedleOffset(QPointF(m_spinOffsetX->value(), v));
        notifyChanged();
    });
    addLabeled(geomForm, QStringLiteral("Offset Y"), m_spinOffsetY);

    m_spinRadiusX = new QDoubleSpinBox(geomGroup);
    styleSpin(m_spinRadiusX, 0.01, 5.0, 0.01, 3);
    connect(m_spinRadiusX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setRadiusRatio(QPointF(v, m_spinRadiusY->value()));
        notifyChanged();
    });
    addLabeled(geomForm, QStringLiteral("Radius X"), m_spinRadiusX);

    m_spinRadiusY = new QDoubleSpinBox(geomGroup);
    styleSpin(m_spinRadiusY, 0.01, 5.0, 0.01, 3);
    connect(m_spinRadiusY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setRadiusRatio(QPointF(m_spinRadiusX->value(), v));
        notifyChanged();
    });
    addLabeled(geomForm, QStringLiteral("Radius Y"), m_spinRadiusY);

    m_spinStrokeWidth = new QDoubleSpinBox(geomGroup);
    styleSpin(m_spinStrokeWidth, 0.1, 20.0, 0.1, 2);
    connect(m_spinStrokeWidth, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setStrokeWidth(static_cast<float>(v));
        notifyChanged();
    });
    addLabeled(geomForm, QStringLiteral("Stroke width"), m_spinStrokeWidth);

    m_form->addRow(geomGroup);

    // ===== Group 3: History =====
    QFormLayout* histForm = nullptr;
    QGroupBox* histGroup = makeGroup(QStringLiteral("History"), histForm);

    m_chkHistory = new QCheckBox(histGroup);
    m_chkHistory->setStyleSheet(kCheckStyle);
    connect(m_chkHistory, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setHistoryEnabled(on);
        notifyChanged();
    });
    addLabeled(histForm, QStringLiteral("History enabled"), m_chkHistory);

    m_spinHistoryDuration = new QSpinBox(histGroup);
    styleSpinI(m_spinHistoryDuration, 100, 60000);
    connect(m_spinHistoryDuration, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setHistoryDuration(v);
        notifyChanged();
    });
    addLabeled(histForm, QStringLiteral("Duration (ms)"), m_spinHistoryDuration);

    m_btnHistoryColor = new QPushButton(histGroup);
    m_btnHistoryColor->setFixedSize(40, 18);
    connect(m_btnHistoryColor, &QPushButton::clicked, this, [this]() {
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        const QColor chosen = QColorDialog::getColor(
            needle->historyColor(), this, QStringLiteral("History color"),
            QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            needle->setHistoryColor(chosen);
            m_btnHistoryColor->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addLabeled(histForm, QStringLiteral("History color"), m_btnHistoryColor);

    m_form->addRow(histGroup);

    // ===== Group 4: Power =====
    QFormLayout* pwrForm = nullptr;
    QGroupBox* pwrGroup = makeGroup(QStringLiteral("Power"), pwrForm);

    m_chkNormaliseTo100W = new QCheckBox(pwrGroup);
    m_chkNormaliseTo100W->setStyleSheet(kCheckStyle);
    connect(m_chkNormaliseTo100W, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setNormaliseTo100W(on);
        notifyChanged();
    });
    addLabeled(pwrForm, QStringLiteral("Normalise to 100 W"), m_chkNormaliseTo100W);

    m_spinMaxPower = new QDoubleSpinBox(pwrGroup);
    styleSpin(m_spinMaxPower, 1.0, 10000.0, 1.0, 1);
    connect(m_spinMaxPower, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setMaxPower(static_cast<float>(v));
        notifyChanged();
    });
    addLabeled(pwrForm, QStringLiteral("Max power (W)"), m_spinMaxPower);

    m_form->addRow(pwrGroup);

    // ===== Group 5: Calibration =====
    QFormLayout* calForm = nullptr;
    QGroupBox* calGroup = makeGroup(QStringLiteral("Calibration"), calForm);

    m_calTable = new QTableWidget(0, 3, calGroup);
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
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        commitCalTable(needle);
        notifyChanged();
    });

    auto* calBtns = new QWidget(calGroup);
    auto* btnLayout = new QHBoxLayout(calBtns);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);

    auto* btnAdd = new QPushButton(QStringLiteral("Add row"), calBtns);
    btnAdd->setStyleSheet(kBtnStyle);
    connect(btnAdd, &QPushButton::clicked, this, [this]() {
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        const int row = m_calTable->rowCount();
        beginProgrammaticUpdate();
        m_calTable->insertRow(row);
        m_calTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("0.000")));
        m_calTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0.5000")));
        m_calTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0.5000")));
        endProgrammaticUpdate();
        commitCalTable(needle);
        notifyChanged();
    });
    btnLayout->addWidget(btnAdd);

    auto* btnRemove = new QPushButton(QStringLiteral("Remove row"), calBtns);
    btnRemove->setStyleSheet(kBtnStyle);
    connect(btnRemove, &QPushButton::clicked, this, [this]() {
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        const int row = m_calTable->currentRow();
        if (row < 0) { return; }
        beginProgrammaticUpdate();
        m_calTable->removeRow(row);
        endProgrammaticUpdate();
        commitCalTable(needle);
        notifyChanged();
    });
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();

    calForm->addRow(m_calTable);
    calForm->addRow(calBtns);

    m_form->addRow(calGroup);
}

} // namespace NereusSDR
