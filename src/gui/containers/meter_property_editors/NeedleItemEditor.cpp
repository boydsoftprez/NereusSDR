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
    addHeader(QStringLiteral("Needle"));

    // ---- Source label ----
    m_editSourceLabel = new QLineEdit(this);
    m_editSourceLabel->setStyleSheet(
        QStringLiteral("QLineEdit {"
                        "  background: #0a0a18; color: #c8d8e8;"
                        "  border: 1px solid #1e2e3e; border-radius: 3px;"
                        "  padding: 1px 4px; min-height: 18px;"
                        "}"));
    connect(m_editSourceLabel, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setSourceLabel(m_editSourceLabel->text());
        notifyChanged();
    });
    addRow(QStringLiteral("Source label"), m_editSourceLabel);

    // ---- Needle color ----
    m_btnNeedleColor = new QPushButton(this);
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
    addRow(QStringLiteral("Needle color"), m_btnNeedleColor);

    // ---- Attack / Decay ----
    m_spinAttack = makeDoubleRow(QStringLiteral("Attack ratio"), 0.0, 1.0, 0.01, 3);
    connect(m_spinAttack, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setAttackRatio(static_cast<float>(v));
        notifyChanged();
    });

    m_spinDecay = makeDoubleRow(QStringLiteral("Decay ratio"), 0.0, 1.0, 0.01, 3);
    connect(m_spinDecay, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setDecayRatio(static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Geometry"));

    // ---- Length factor ----
    m_spinLengthFactor = makeDoubleRow(QStringLiteral("Length factor"), 0.1, 5.0, 0.01, 3);
    connect(m_spinLengthFactor, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setLengthFactor(static_cast<float>(v));
        notifyChanged();
    });

    // ---- Needle offset X/Y ----
    m_spinOffsetX = makeDoubleRow(QStringLiteral("Offset X"), -2.0, 2.0, 0.001, 4);
    connect(m_spinOffsetX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setNeedleOffset(QPointF(v, m_spinOffsetY->value()));
        notifyChanged();
    });

    m_spinOffsetY = makeDoubleRow(QStringLiteral("Offset Y"), -2.0, 2.0, 0.001, 4);
    connect(m_spinOffsetY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setNeedleOffset(QPointF(m_spinOffsetX->value(), v));
        notifyChanged();
    });

    // ---- Radius ratio X/Y ----
    m_spinRadiusX = makeDoubleRow(QStringLiteral("Radius X"), 0.01, 5.0, 0.01, 3);
    connect(m_spinRadiusX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setRadiusRatio(QPointF(v, m_spinRadiusY->value()));
        notifyChanged();
    });

    m_spinRadiusY = makeDoubleRow(QStringLiteral("Radius Y"), 0.01, 5.0, 0.01, 3);
    connect(m_spinRadiusY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setRadiusRatio(QPointF(m_spinRadiusX->value(), v));
        notifyChanged();
    });

    // ---- Stroke width ----
    m_spinStrokeWidth = makeDoubleRow(QStringLiteral("Stroke width"), 0.1, 20.0, 0.1, 2);
    connect(m_spinStrokeWidth, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setStrokeWidth(static_cast<float>(v));
        notifyChanged();
    });

    // ---- Direction ----
    m_comboDirection = new QComboBox(this);
    m_comboDirection->setStyleSheet(kComboStyle);
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
    addRow(QStringLiteral("Direction"), m_comboDirection);

    // ---- History section ----
    addHeader(QStringLiteral("History"));

    m_chkHistory = makeCheckRow(QStringLiteral("History enabled"));
    connect(m_chkHistory, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setHistoryEnabled(on);
        notifyChanged();
    });

    m_spinHistoryDuration = makeIntRow(QStringLiteral("Duration (ms)"), 100, 60000);
    connect(m_spinHistoryDuration, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setHistoryDuration(v);
        notifyChanged();
    });

    m_btnHistoryColor = new QPushButton(this);
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
    addRow(QStringLiteral("History color"), m_btnHistoryColor);

    // ---- Power normalisation section ----
    addHeader(QStringLiteral("Power"));

    m_chkNormaliseTo100W = makeCheckRow(QStringLiteral("Normalise to 100 W"));
    connect(m_chkNormaliseTo100W, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setNormaliseTo100W(on);
        notifyChanged();
    });

    m_spinMaxPower = makeDoubleRow(QStringLiteral("Max power (W)"), 1.0, 10000.0, 1.0, 1);
    connect(m_spinMaxPower, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        needle->setMaxPower(static_cast<float>(v));
        notifyChanged();
    });

    // ---- Scale calibration section ----
    addHeader(QStringLiteral("Calibration"));

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

    // Editable cells: on change rebuild map and push to item
    connect(m_calTable, &QTableWidget::cellChanged, this, [this](int, int) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        NeedleItem* needle = qobject_cast<NeedleItem*>(m_item);
        if (!needle) { return; }
        commitCalTable(needle);
        notifyChanged();
    });

    // Add / Remove buttons for rows
    auto* calBtns = new QWidget(this);
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

    // Embed table and buttons into the form using a container widget
    auto* calContainer = new QWidget(this);
    auto* calLayout = new QVBoxLayout(calContainer);
    calLayout->setContentsMargins(0, 0, 0, 0);
    calLayout->setSpacing(2);
    calLayout->addWidget(m_calTable);
    calLayout->addWidget(calBtns);

    addRow(QStringLiteral("Cal. points"), calContainer);
}

} // namespace NereusSDR
