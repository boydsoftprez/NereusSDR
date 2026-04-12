#include "SignalTextItemEditor.h"
#include "../../meters/SignalTextItem.h"

#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QColorDialog>

namespace NereusSDR {

SignalTextItemEditor::SignalTextItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void SignalTextItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    SignalTextItem* x = qobject_cast<SignalTextItem*>(item);
    if (!x) { return; }

    auto applyColor = [](QPushButton* btn, const QColor& c) {
        btn->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };

    beginProgrammaticUpdate();

    const int unitsIdx = m_comboUnits->findData(static_cast<int>(x->units()));
    m_comboUnits->setCurrentIndex(unitsIdx >= 0 ? unitsIdx : 0);

    m_chkShowValue->setChecked(x->showValue());
    m_chkShowPeak->setChecked(x->showPeakValue());
    m_chkShowType->setChecked(x->showType());
    m_chkPeakHold->setChecked(x->peakHold());

    applyColor(m_btnColour, x->colour());
    applyColor(m_btnPeakColour, x->peakValueColour());
    applyColor(m_btnMarkerColour, x->markerColour());

    m_fontCombo->setCurrentFont(QFont(x->fontFamily()));
    m_spinFontSize->setValue(static_cast<double>(x->fontSize()));

    const int barIdx = m_comboBarStyle->findData(static_cast<int>(x->barStyleMode()));
    m_comboBarStyle->setCurrentIndex(barIdx >= 0 ? barIdx : 0);

    endProgrammaticUpdate();
}

void SignalTextItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Signal Text"));

    // Units
    m_comboUnits = new QComboBox(this);
    m_comboUnits->addItem(QStringLiteral("dBm"),    static_cast<int>(SignalTextItem::Units::Dbm));
    m_comboUnits->addItem(QStringLiteral("S-Units"), static_cast<int>(SignalTextItem::Units::SUnits));
    m_comboUnits->addItem(QStringLiteral("μV"),      static_cast<int>(SignalTextItem::Units::Uv));
    addRow(QStringLiteral("Units"), m_comboUnits);
    connect(m_comboUnits, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setUnits(static_cast<SignalTextItem::Units>(m_comboUnits->currentData().toInt()));
        notifyChanged();
    });

    // Visibility toggles
    m_chkShowValue = makeCheckRow(QStringLiteral("Show value"));
    connect(m_chkShowValue, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setShowValue(on);
        notifyChanged();
    });

    m_chkShowPeak = makeCheckRow(QStringLiteral("Show peak value"));
    connect(m_chkShowPeak, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setShowPeakValue(on);
        notifyChanged();
    });

    m_chkShowType = makeCheckRow(QStringLiteral("Show type"));
    connect(m_chkShowType, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setShowType(on);
        notifyChanged();
    });

    m_chkPeakHold = makeCheckRow(QStringLiteral("Peak hold"));
    connect(m_chkPeakHold, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setPeakHold(on);
        notifyChanged();
    });

    // Colors
    m_btnColour = new QPushButton(this);
    m_btnColour->setFixedSize(40, 18);
    connect(m_btnColour, &QPushButton::clicked, this, [this]() {
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->colour(), this);
        if (chosen.isValid()) {
            x->setColour(chosen);
            m_btnColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour"), m_btnColour);

    m_btnPeakColour = new QPushButton(this);
    m_btnPeakColour->setFixedSize(40, 18);
    connect(m_btnPeakColour, &QPushButton::clicked, this, [this]() {
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->peakValueColour(), this);
        if (chosen.isValid()) {
            x->setPeakValueColour(chosen);
            m_btnPeakColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Peak colour"), m_btnPeakColour);

    m_btnMarkerColour = new QPushButton(this);
    m_btnMarkerColour->setFixedSize(40, 18);
    connect(m_btnMarkerColour, &QPushButton::clicked, this, [this]() {
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->markerColour(), this);
        if (chosen.isValid()) {
            x->setMarkerColour(chosen);
            m_btnMarkerColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Marker colour"), m_btnMarkerColour);

    // Font
    m_fontCombo = new QFontComboBox(this);
    addRow(QStringLiteral("Font"), m_fontCombo);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setFontFamily(f.family());
        notifyChanged();
    });

    m_spinFontSize = makeDoubleRow(QStringLiteral("Font size"), 4.0, 200.0, 1.0, 1);
    connect(m_spinFontSize, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setFontSize(static_cast<float>(v));
        notifyChanged();
    });

    // Bar style
    m_comboBarStyle = new QComboBox(this);
    m_comboBarStyle->addItem(QStringLiteral("None"),            static_cast<int>(SignalTextItem::BarStyle::None));
    m_comboBarStyle->addItem(QStringLiteral("Line"),            static_cast<int>(SignalTextItem::BarStyle::Line));
    m_comboBarStyle->addItem(QStringLiteral("Solid Filled"),    static_cast<int>(SignalTextItem::BarStyle::SolidFilled));
    m_comboBarStyle->addItem(QStringLiteral("Gradient Filled"), static_cast<int>(SignalTextItem::BarStyle::GradientFilled));
    m_comboBarStyle->addItem(QStringLiteral("Segments"),        static_cast<int>(SignalTextItem::BarStyle::Segments));
    addRow(QStringLiteral("Bar style"), m_comboBarStyle);
    connect(m_comboBarStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setBarStyleMode(static_cast<SignalTextItem::BarStyle>(m_comboBarStyle->currentData().toInt()));
        notifyChanged();
    });
}

} // namespace NereusSDR
