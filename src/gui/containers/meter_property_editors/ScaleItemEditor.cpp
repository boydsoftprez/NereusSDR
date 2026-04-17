// =================================================================
// src/gui/containers/meter_property_editors/ScaleItemEditor.cpp  (NereusSDR)
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

#include "ScaleItemEditor.h"
#include "../../meters/MeterItem.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

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
} // namespace

ScaleItemEditor::ScaleItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void ScaleItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    ScaleItem* scale = qobject_cast<ScaleItem*>(item);
    if (!scale) { return; }

    beginProgrammaticUpdate();

    m_comboOrientation->setCurrentIndex(
        scale->orientation() == ScaleItem::Orientation::Vertical ? 1 : 0);
    m_spinMin->setValue(scale->minVal());
    m_spinMax->setValue(scale->maxVal());
    m_spinMajorTicks->setValue(scale->majorTicks());
    m_spinMinorTicks->setValue(scale->minorTicks());
    m_spinFontSize->setValue(scale->fontSize());

    auto applyTickColor = [this](const QColor& c) {
        m_btnTickColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    auto applyLabelColor = [this](const QColor& c) {
        m_btnLabelColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    applyTickColor(scale->tickColor());
    applyLabelColor(scale->labelColor());

    // Phase B4 — ShowType + title colour
    m_chkShowType->setChecked(scale->showType());
    m_btnTitleColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(scale->titleColour().name(QColor::HexArgb)));

    endProgrammaticUpdate();
}

void ScaleItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Scale"));

    // Orientation
    m_comboOrientation = new QComboBox(this);
    m_comboOrientation->setStyleSheet(kComboStyle);
    m_comboOrientation->addItem(QStringLiteral("Horizontal"),
                                static_cast<int>(ScaleItem::Orientation::Horizontal));
    m_comboOrientation->addItem(QStringLiteral("Vertical"),
                                static_cast<int>(ScaleItem::Orientation::Vertical));
    connect(m_comboOrientation, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setOrientation(
            static_cast<ScaleItem::Orientation>(m_comboOrientation->currentData().toInt()));
        notifyChanged();
    });
    addRow(QStringLiteral("Orientation"), m_comboOrientation);

    // Min / Max
    m_spinMin = makeDoubleRow(QStringLiteral("Min"), -1000.0, 1000.0, 1.0, 2);
    connect(m_spinMin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setRange(v, scale->maxVal());
        notifyChanged();
    });

    m_spinMax = makeDoubleRow(QStringLiteral("Max"), -1000.0, 1000.0, 1.0, 2);
    connect(m_spinMax, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setRange(scale->minVal(), v);
        notifyChanged();
    });

    // Major / Minor ticks
    m_spinMajorTicks = makeIntRow(QStringLiteral("Major ticks"), 2, 50);
    connect(m_spinMajorTicks, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setMajorTicks(v);
        notifyChanged();
    });

    m_spinMinorTicks = makeIntRow(QStringLiteral("Minor ticks"), 0, 20);
    connect(m_spinMinorTicks, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setMinorTicks(v);
        notifyChanged();
    });

    // Tick color
    m_btnTickColor = new QPushButton(this);
    m_btnTickColor->setFixedSize(40, 18);
    auto applyTickBtn = [this](const QColor& c) {
        m_btnTickColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    connect(m_btnTickColor, &QPushButton::clicked, this, [this, applyTickBtn]() {
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        const QColor chosen = QColorDialog::getColor(scale->tickColor(), this,
                                                     QStringLiteral("Tick color"));
        if (chosen.isValid()) { scale->setTickColor(chosen); applyTickBtn(chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Tick color"), m_btnTickColor);

    // Label color
    m_btnLabelColor = new QPushButton(this);
    m_btnLabelColor->setFixedSize(40, 18);
    auto applyLabelBtn = [this](const QColor& c) {
        m_btnLabelColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    connect(m_btnLabelColor, &QPushButton::clicked, this, [this, applyLabelBtn]() {
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        const QColor chosen = QColorDialog::getColor(scale->labelColor(), this,
                                                     QStringLiteral("Label color"));
        if (chosen.isValid()) { scale->setLabelColor(chosen); applyLabelBtn(chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Label color"), m_btnLabelColor);

    // Font size
    m_spinFontSize = makeIntRow(QStringLiteral("Font size"), 6, 72);
    connect(m_spinFontSize, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setFontSize(v);
        notifyChanged();
    });

    // --- Phase B4: ShowType + title colour ---
    // From Thetis clsScaleItem.ShowType (MeterManager.cs:14827). When
    // checked, ScaleItem::paint draws readingName(bindingId) centered
    // in the top strip — used by every bar-row preset.
    m_chkShowType = makeCheckRow(QStringLiteral("Show type title"));
    connect(m_chkShowType, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        scale->setShowType(on);
        notifyChanged();
    });

    m_btnTitleColor = new QPushButton(this);
    m_btnTitleColor->setFixedSize(40, 18);
    auto applyTitleBtn = [this](const QColor& c) {
        m_btnTitleColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    connect(m_btnTitleColor, &QPushButton::clicked, this, [this, applyTitleBtn]() {
        ScaleItem* scale = qobject_cast<ScaleItem*>(m_item);
        if (!scale) { return; }
        const QColor chosen = QColorDialog::getColor(scale->titleColour(), this,
                                                     QStringLiteral("Title color"));
        if (chosen.isValid()) {
            scale->setTitleColour(chosen);
            applyTitleBtn(chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Title color"), m_btnTitleColor);
}

} // namespace NereusSDR
