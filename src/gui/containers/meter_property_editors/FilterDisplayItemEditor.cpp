#include "FilterDisplayItemEditor.h"
#include "../../meters/FilterDisplayItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QColor& c)
{
    btn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(c.name(QColor::HexArgb)));
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
} // namespace

FilterDisplayItemEditor::FilterDisplayItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void FilterDisplayItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(item);
    if (!f) { return; }

    beginProgrammaticUpdate();
    m_comboDisplayMode->setCurrentIndex(
        m_comboDisplayMode->findData(static_cast<int>(f->displayMode())));
    // Note: FilterDisplayItem exposes no read-back getters for colors, palette,
    // fill, padding, or spec range — only setters. Color buttons and spin boxes
    // retain their last-set value; they will be correct after the user edits once.
    endProgrammaticUpdate();
}

void FilterDisplayItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Filter Display"));

    m_comboDisplayMode = new QComboBox(this);
    m_comboDisplayMode->setStyleSheet(kComboStyle);
    m_comboDisplayMode->addItem(QStringLiteral("Panadapter"),
        static_cast<int>(FilterDisplayItem::DisplayMode::Panadapter));
    m_comboDisplayMode->addItem(QStringLiteral("Waterfall"),
        static_cast<int>(FilterDisplayItem::DisplayMode::Waterfall));
    m_comboDisplayMode->addItem(QStringLiteral("Panafall"),
        static_cast<int>(FilterDisplayItem::DisplayMode::Panafall));
    m_comboDisplayMode->addItem(QStringLiteral("None"),
        static_cast<int>(FilterDisplayItem::DisplayMode::None));
    addRow(QStringLiteral("Display mode"), m_comboDisplayMode);
    connect(m_comboDisplayMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setDisplayMode(static_cast<FilterDisplayItem::DisplayMode>(
            m_comboDisplayMode->currentData().toInt()));
        notifyChanged();
    });

    m_comboWfPalette = new QComboBox(this);
    m_comboWfPalette->setStyleSheet(kComboStyle);
    using WP = FilterDisplayItem::WaterfallPalette;
    m_comboWfPalette->addItem(QStringLiteral("Enhanced"),  static_cast<int>(WP::Enhanced));
    m_comboWfPalette->addItem(QStringLiteral("Spectran"),  static_cast<int>(WP::Spectran));
    m_comboWfPalette->addItem(QStringLiteral("Black/White"), static_cast<int>(WP::BlackWhite));
    m_comboWfPalette->addItem(QStringLiteral("LinLog"),    static_cast<int>(WP::LinLog));
    m_comboWfPalette->addItem(QStringLiteral("LinRad"),    static_cast<int>(WP::LinRad));
    m_comboWfPalette->addItem(QStringLiteral("LinAuto"),   static_cast<int>(WP::LinAuto));
    m_comboWfPalette->addItem(QStringLiteral("Custom"),    static_cast<int>(WP::Custom));
    addRow(QStringLiteral("WF palette"), m_comboWfPalette);
    connect(m_comboWfPalette, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setWaterfallPalette(static_cast<FilterDisplayItem::WaterfallPalette>(
            m_comboWfPalette->currentData().toInt()));
        notifyChanged();
    });

    m_chkFill = makeCheckRow(QStringLiteral("Fill spectrum"));
    connect(m_chkFill, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setFillSpectrum(on);
        notifyChanged();
    });

    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.0, 0.5, 0.01, 3);
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setPadding(static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Spectrum range"));
    m_spinMinDb = makeDoubleRow(QStringLiteral("Min dB"), -200.0, 0.0, 1.0, 1);
    connect(m_spinMinDb, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setSpecGridRange(static_cast<float>(v), static_cast<float>(m_spinMaxDb->value()));
        notifyChanged();
    });

    m_spinMaxDb = makeDoubleRow(QStringLiteral("Max dB"), -200.0, 0.0, 1.0, 1);
    connect(m_spinMaxDb, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        f->setSpecGridRange(static_cast<float>(m_spinMinDb->value()), static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Colors"));

    auto makeColorBtn = [this](const QString& label, QPushButton*& out) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Data line"),    m_btnDataLine);
    makeColorBtn(QStringLiteral("Data fill"),    m_btnDataFill);
    makeColorBtn(QStringLiteral("RX edges"),     m_btnEdgesRx);
    makeColorBtn(QStringLiteral("TX edges"),     m_btnEdgesTx);
    makeColorBtn(QStringLiteral("Notch"),        m_btnNotch);
    makeColorBtn(QStringLiteral("Background"),   m_btnBack);
    makeColorBtn(QStringLiteral("Text"),         m_btnText);

    // Wire each color button.
    // Note: FilterDisplayItem exposes no color read-back getters so QColorDialog
    // opens with a neutral color; the button swatch reflects the last chosen value.
    connect(m_btnDataLine, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::green, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setDataLineColour(chosen); applyBtnColor(m_btnDataLine, chosen); notifyChanged(); }
    });
    connect(m_btnDataFill, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::green, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setDataFillColour(chosen); applyBtnColor(m_btnDataFill, chosen); notifyChanged(); }
    });
    connect(m_btnEdgesRx, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::yellow, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setEdgesColourRX(chosen); applyBtnColor(m_btnEdgesRx, chosen); notifyChanged(); }
    });
    connect(m_btnEdgesTx, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::red, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setEdgesColourTX(chosen); applyBtnColor(m_btnEdgesTx, chosen); notifyChanged(); }
    });
    connect(m_btnNotch, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::darkYellow, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setNotchColour(chosen); applyBtnColor(m_btnNotch, chosen); notifyChanged(); }
    });
    connect(m_btnBack, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::black, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setMeterBackColour(chosen); applyBtnColor(m_btnBack, chosen); notifyChanged(); }
    });
    connect(m_btnText, &QPushButton::clicked, this, [this]() {
        FilterDisplayItem* f = qobject_cast<FilterDisplayItem*>(m_item);
        if (!f) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::white, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { f->setTextColour(chosen); applyBtnColor(m_btnText, chosen); notifyChanged(); }
    });
}

} // namespace NereusSDR
