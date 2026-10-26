#include "HistoryGraphItemEditor.h"
#include "../../meters/HistoryGraphItem.h"
#include "../../meters/MeterPoller.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QColorDialog>

namespace NereusSDR {

HistoryGraphItemEditor::HistoryGraphItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void HistoryGraphItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(item);
    if (!x) { return; }

    auto applyColor = [](QPushButton* btn, const QColor& c) {
        btn->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };

    beginProgrammaticUpdate();

    m_spinCapacity->setValue(x->capacity());
    applyColor(m_btnLineColor0, x->lineColor0());
    applyColor(m_btnLineColor1, x->lineColor1());
    m_chkShowGrid->setChecked(x->showGrid());
    m_chkAutoScale0->setChecked(x->autoScale0());
    m_chkAutoScale1->setChecked(x->autoScale1());
    m_chkShowScale0->setChecked(x->showScale0());
    m_chkShowScale1->setChecked(x->showScale1());

    const int idx = m_comboBinding1->findData(x->bindingId1());
    m_comboBinding1->setCurrentIndex(idx >= 0 ? idx : 0);

    m_spinVerticalRatio->setValue(x->verticalRatio());
    applyColor(m_btnBackColour, x->backColour());
    applyColor(m_btnLinesColour, x->linesColour());
    applyColor(m_btnTimeColour, x->timeColour());
    m_spinMin0->setValue(x->manualMin(0));
    m_spinMax0->setValue(x->manualMax(0));
    m_spinMin1->setValue(x->manualMin(1));
    m_spinMax1->setValue(x->manualMax(1));
    syncManualRangeEnabled();

    endProgrammaticUpdate();
}

// The manual range applies while an axis does not auto scale.
void HistoryGraphItemEditor::syncManualRangeEnabled()
{
    m_spinMin0->setEnabled(!m_chkAutoScale0->isChecked());
    m_spinMax0->setEnabled(!m_chkAutoScale0->isChecked());
    m_spinMin1->setEnabled(!m_chkAutoScale1->isChecked());
    m_spinMax1->setEnabled(!m_chkAutoScale1->isChecked());
}

void HistoryGraphItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("History Graph"));

    m_spinCapacity = makeIntRow(QStringLiteral("Capacity"), 10, 3000);
    connect(m_spinCapacity, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setCapacity(v);
        notifyChanged();
    });

    // Line colors
    m_btnLineColor0 = new QPushButton(this);
    m_btnLineColor0->setFixedSize(40, 18);
    connect(m_btnLineColor0, &QPushButton::clicked, this, [this]() {
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->lineColor0(), this);
        if (chosen.isValid()) {
            x->setLineColor0(chosen);
            m_btnLineColor0->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Line color 0"), m_btnLineColor0);

    m_btnLineColor1 = new QPushButton(this);
    m_btnLineColor1->setFixedSize(40, 18);
    connect(m_btnLineColor1, &QPushButton::clicked, this, [this]() {
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->lineColor1(), this);
        if (chosen.isValid()) {
            x->setLineColor1(chosen);
            m_btnLineColor1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Line color 1"), m_btnLineColor1);

    // Grid and scaling
    m_chkShowGrid = makeCheckRow(QStringLiteral("Show grid"));
    connect(m_chkShowGrid, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setShowGrid(on);
        notifyChanged();
    });

    m_chkAutoScale0 = makeCheckRow(QStringLiteral("Auto scale axis 0"));
    connect(m_chkAutoScale0, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setAutoScale0(on);
        syncManualRangeEnabled();
        notifyChanged();
    });

    m_chkAutoScale1 = makeCheckRow(QStringLiteral("Auto scale axis 1"));
    connect(m_chkAutoScale1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setAutoScale1(on);
        syncManualRangeEnabled();
        notifyChanged();
    });

    m_chkShowScale0 = makeCheckRow(QStringLiteral("Show scale axis 0"));
    connect(m_chkShowScale0, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setShowScale0(on);
        notifyChanged();
    });

    m_chkShowScale1 = makeCheckRow(QStringLiteral("Show scale axis 1"));
    connect(m_chkShowScale1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setShowScale1(on);
        notifyChanged();
    });

    // Second axis binding
    addHeader(QStringLiteral("Axis 1 Reading"));

    m_comboBinding1 = new QComboBox(this);
    populateReadingCombo(m_comboBinding1);
    addRow(QStringLiteral("Axis 1 shows"), m_comboBinding1);
    connect(m_comboBinding1, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setBindingId1(m_comboBinding1->currentData().toInt());
        notifyChanged();
    });

    // Manual range, used while an axis does not auto scale.
    // Ranges and step: the kHistoryAxis constants (HistoryGraphItem.h).
    addHeader(QStringLiteral("Scale range"));
    struct RangeRow { QDoubleSpinBox** spin; const char* label; int axis; bool isMax; };
    const RangeRow rangeRows[] = {
        {&m_spinMin0, "Axis 0 minimum", 0, false},
        {&m_spinMax0, "Axis 0 maximum", 0, true},
        {&m_spinMin1, "Axis 1 minimum", 1, false},
        {&m_spinMax1, "Axis 1 maximum", 1, true},
    };
    for (const RangeRow& row : rangeRows) {
        QDoubleSpinBox* spin = row.isMax
            ? makeDoubleRow(QString::fromLatin1(row.label), kMinHistoryAxisMax, kMaxHistoryAxisMax, kHistoryAxisStep, 1)
            : makeDoubleRow(QString::fromLatin1(row.label), kMinHistoryAxisMin, kMaxHistoryAxisMin, kHistoryAxisStep, 1);
        *row.spin = spin;
        const int axis = row.axis;
        const bool isMax = row.isMax;
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, axis, isMax](double v) {
            if (isProgrammaticUpdate()) { return; }
            HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
            if (!x) { return; }
            if (isMax) {
                x->setManualMax(axis, static_cast<float>(v));
            } else {
                x->setManualMin(axis, static_cast<float>(v));
            }
            notifyChanged();
        });
    }

    // Look
    addHeader(QStringLiteral("Graph look"));
    // Range and step: the kHistoryVerticalRatio constants (HistoryGraphItem.h).
    m_spinVerticalRatio = makeDoubleRow(QStringLiteral("Height to width"),
                                        kMinHistoryVerticalRatio, kMaxHistoryVerticalRatio,
                                        kHistoryVerticalRatioStep, 3);
    connect(m_spinVerticalRatio, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setVerticalRatio(static_cast<float>(v));
        notifyChanged();
    });

    struct ColourRow {
        QPushButton** button;
        const char* label;
        QColor (HistoryGraphItem::*get)() const;
        void (HistoryGraphItem::*set)(const QColor&);
    };
    const ColourRow colourRows[] = {
        {&m_btnBackColour, "Background color", &HistoryGraphItem::backColour, &HistoryGraphItem::setBackColour},
        {&m_btnLinesColour, "Grid line color", &HistoryGraphItem::linesColour, &HistoryGraphItem::setLinesColour},
        {&m_btnTimeColour, "Label color", &HistoryGraphItem::timeColour, &HistoryGraphItem::setTimeColour},
    };
    for (const ColourRow& row : colourRows) {
        auto* button = new QPushButton(this);
        button->setFixedSize(40, 18);
        *row.button = button;
        const auto get = row.get;
        const auto set = row.set;
        connect(button, &QPushButton::clicked, this, [this, button, get, set]() {
            HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
            if (!x) { return; }
            const QColor chosen = QColorDialog::getColor((x->*get)(), this);
            if (chosen.isValid()) {
                (x->*set)(chosen);
                button->setStyleSheet(
                    QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                        .arg(chosen.name(QColor::HexArgb)));
                notifyChanged();
            }
        });
        addRow(QString::fromLatin1(row.label), button);
    }
}

} // namespace NereusSDR
