#include "BarItemEditor.h"
#include "../../meters/MeterItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

BarItemEditor::BarItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void BarItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    BarItem* bar = qobject_cast<BarItem*>(item);
    if (!bar) { return; }

    beginProgrammaticUpdate();

    // Orientation
    m_comboOrientation->setCurrentIndex(
        m_comboOrientation->findData(static_cast<int>(bar->orientation())));

    // Range
    m_spinMinVal->setValue(bar->minVal());
    m_spinMaxVal->setValue(bar->maxVal());

    // Red threshold & smoothing
    m_spinRedThreshold->setValue(bar->redThreshold());
    m_spinAttack->setValue(static_cast<double>(bar->attackRatio()));
    m_spinDecay->setValue(static_cast<double>(bar->decayRatio()));

    // Bar style
    m_comboBarStyle->setCurrentIndex(
        m_comboBarStyle->findData(static_cast<int>(bar->barStyle())));

    // Filled-mode colors
    m_btnBarColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(bar->barColor().name(QColor::HexArgb)));
    m_btnBarRedColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(bar->barRedColor().name(QColor::HexArgb)));

    // Edge-mode colors
    m_btnEdgeBg->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(bar->edgeBackgroundColor().name(QColor::HexArgb)));
    m_btnEdgeLow->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(bar->edgeLowColor().name(QColor::HexArgb)));
    m_btnEdgeHigh->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(bar->edgeHighColor().name(QColor::HexArgb)));
    m_btnEdgeAvg->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(bar->edgeAvgColor().name(QColor::HexArgb)));

    endProgrammaticUpdate();
}

void BarItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Bar"));

    // --- Orientation ---
    m_comboOrientation = new QComboBox(this);
    m_comboOrientation->addItem(QStringLiteral("Horizontal"),
                                static_cast<int>(BarItem::Orientation::Horizontal));
    m_comboOrientation->addItem(QStringLiteral("Vertical"),
                                static_cast<int>(BarItem::Orientation::Vertical));
    addRow(QStringLiteral("Orientation"), m_comboOrientation);
    connect(m_comboOrientation, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setOrientation(static_cast<BarItem::Orientation>(
            m_comboOrientation->currentData().toInt()));
        notifyChanged();
    });

    // --- Range ---
    m_spinMinVal = makeDoubleRow(QStringLiteral("Min value"), -200.0, 200.0, 1.0, 1);
    connect(m_spinMinVal, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setRange(v, bar->maxVal());
        notifyChanged();
    });

    m_spinMaxVal = makeDoubleRow(QStringLiteral("Max value"), -200.0, 200.0, 1.0, 1);
    connect(m_spinMaxVal, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setRange(bar->minVal(), v);
        notifyChanged();
    });

    // --- Red threshold ---
    m_spinRedThreshold = makeDoubleRow(QStringLiteral("Red threshold"), -200.0, 200.0, 1.0, 1);
    connect(m_spinRedThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setRedThreshold(v);
        notifyChanged();
    });

    // --- Smoothing ---
    m_spinAttack = makeDoubleRow(QStringLiteral("Attack ratio"), 0.0, 1.0, 0.01, 3);
    connect(m_spinAttack, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setAttackRatio(static_cast<float>(v));
        notifyChanged();
    });

    m_spinDecay = makeDoubleRow(QStringLiteral("Decay ratio"), 0.0, 1.0, 0.01, 3);
    connect(m_spinDecay, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setDecayRatio(static_cast<float>(v));
        notifyChanged();
    });

    // --- Bar style ---
    addHeader(QStringLiteral("Filled mode"));

    m_comboBarStyle = new QComboBox(this);
    m_comboBarStyle->addItem(QStringLiteral("Filled"),
                             static_cast<int>(BarItem::BarStyle::Filled));
    m_comboBarStyle->addItem(QStringLiteral("Edge"),
                             static_cast<int>(BarItem::BarStyle::Edge));
    addRow(QStringLiteral("Bar style"), m_comboBarStyle);
    connect(m_comboBarStyle, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        bar->setBarStyle(static_cast<BarItem::BarStyle>(
            m_comboBarStyle->currentData().toInt()));
        notifyChanged();
    });

    // Bar color
    m_btnBarColor = new QPushButton(this);
    m_btnBarColor->setFixedSize(40, 18);
    connect(m_btnBarColor, &QPushButton::clicked, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(bar->barColor(), this,
                                                     QStringLiteral("Bar color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            bar->setBarColor(chosen);
            m_btnBarColor->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Bar color"), m_btnBarColor);

    // Bar red color
    m_btnBarRedColor = new QPushButton(this);
    m_btnBarRedColor->setFixedSize(40, 18);
    connect(m_btnBarRedColor, &QPushButton::clicked, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(bar->barRedColor(), this,
                                                     QStringLiteral("Bar red color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            bar->setBarRedColor(chosen);
            m_btnBarRedColor->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Bar red color"), m_btnBarRedColor);

    // --- Edge mode colors ---
    addHeader(QStringLiteral("Edge mode colors"));

    m_btnEdgeBg = new QPushButton(this);
    m_btnEdgeBg->setFixedSize(40, 18);
    connect(m_btnEdgeBg, &QPushButton::clicked, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(bar->edgeBackgroundColor(), this,
                                                     QStringLiteral("Edge background color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            bar->setEdgeBackgroundColor(chosen);
            m_btnEdgeBg->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Edge bg color"), m_btnEdgeBg);

    m_btnEdgeLow = new QPushButton(this);
    m_btnEdgeLow->setFixedSize(40, 18);
    connect(m_btnEdgeLow, &QPushButton::clicked, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(bar->edgeLowColor(), this,
                                                     QStringLiteral("Edge low color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            bar->setEdgeLowColor(chosen);
            m_btnEdgeLow->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Edge low color"), m_btnEdgeLow);

    m_btnEdgeHigh = new QPushButton(this);
    m_btnEdgeHigh->setFixedSize(40, 18);
    connect(m_btnEdgeHigh, &QPushButton::clicked, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(bar->edgeHighColor(), this,
                                                     QStringLiteral("Edge high color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            bar->setEdgeHighColor(chosen);
            m_btnEdgeHigh->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Edge high color"), m_btnEdgeHigh);

    m_btnEdgeAvg = new QPushButton(this);
    m_btnEdgeAvg->setFixedSize(40, 18);
    connect(m_btnEdgeAvg, &QPushButton::clicked, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(bar->edgeAvgColor(), this,
                                                     QStringLiteral("Edge avg color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            bar->setEdgeAvgColor(chosen);
            m_btnEdgeAvg->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Edge avg color"), m_btnEdgeAvg);
}

} // namespace NereusSDR
