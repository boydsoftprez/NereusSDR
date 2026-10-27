#include "ButtonBoxItemEditor.h"
#include "../../meters/ButtonBoxItem.h"

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

namespace NereusSDR {

ButtonBoxItemEditor::ButtonBoxItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    // Subclass constructors call buildButtonBoxSection() themselves after this
    // base constructor returns, ensuring the section appears after the base
    // common fields. Do not call buildButtonBoxSection() here — the subclass
    // vtable is not yet active and each subclass controls the call order.
}

void ButtonBoxItemEditor::buildTypeSpecific()
{
    // This override exists so that if ButtonBoxItemEditor is ever instantiated
    // directly (not via a subclass), the button-box section is still built.
    // Concrete subclasses call buildButtonBoxSection() from their own ctors.
    buildButtonBoxSection();
}

void ButtonBoxItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(item);
    if (!b) { return; }

    beginProgrammaticUpdate();
    // The grid type's column range. A saved count outside it shows clamped
    // and stays as saved until the count is edited.
    const auto [minColumns, maxColumns] = b->columnRange();
    m_spinColumns->setRange(minColumns, maxColumns);
    m_spinColumns->setValue(b->columns());
    m_spinBorderWidth->setValue(static_cast<double>(b->borderWidth()));
    m_spinMargin->setValue(static_cast<double>(b->margin()));
    m_spinCornerRadius->setValue(static_cast<double>(b->cornerRadius()));
    m_spinHeightRatio->setValue(static_cast<double>(b->heightRatio()));
    m_chkFadeOnRx->setChecked(b->fadeOnRx());
    m_chkFadeOnTx->setChecked(b->fadeOnTx());
    endProgrammaticUpdate();
}

void ButtonBoxItemEditor::refreshColumnRange()
{
    ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
    if (!b || !m_spinColumns) { return; }
    const auto [minColumns, maxColumns] = b->columnRange();
    if (m_spinColumns->minimum() == minColumns && m_spinColumns->maximum() == maxColumns) { return; }
    beginProgrammaticUpdate();
    m_spinColumns->setRange(minColumns, maxColumns);
    endProgrammaticUpdate();
    if (b->columns() > maxColumns) {
        b->setColumns(maxColumns);
        beginProgrammaticUpdate();
        m_spinColumns->setValue(maxColumns);
        endProgrammaticUpdate();
        notifyChanged();
    }
}

void ButtonBoxItemEditor::buildButtonBoxSection()
{
    addHeader(QStringLiteral("Button box"));

    m_spinColumns = makeIntRow(QStringLiteral("Columns"), ButtonBoxItem::kDefaultMinColumns,
                               ButtonBoxItem::kDefaultMaxColumns);
    m_spinColumns->setObjectName(QStringLiteral("buttonBoxColumns"));
    // A grid whose range follows its shown buttons (antenna, tune step,
    // other buttons) gets a new range when a button is shown or hidden; a
    // count above the new maximum comes down to it.
    connect(this, &BaseItemEditor::propertyChanged, this, [this]() { refreshColumnRange(); });
    connect(m_spinColumns, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setColumns(v);
        notifyChanged();
    });

    m_spinBorderWidth = makeDoubleRow(QStringLiteral("Border width"), 0.0, 0.1, 0.001, 4);
    connect(m_spinBorderWidth, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setBorderWidth(static_cast<float>(v));
        notifyChanged();
    });

    m_spinMargin = makeDoubleRow(QStringLiteral("Margin"), 0.0, 0.5, 0.005, 3);
    connect(m_spinMargin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setMargin(static_cast<float>(v));
        notifyChanged();
    });

    m_spinCornerRadius = makeDoubleRow(QStringLiteral("Corner radius"), 0.0, 20.0, 0.5, 1);
    connect(m_spinCornerRadius, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setCornerRadius(static_cast<float>(v));
        notifyChanged();
    });

    m_spinHeightRatio = makeDoubleRow(QStringLiteral("Height ratio"), 0.1, 5.0, 0.05, 2);
    connect(m_spinHeightRatio, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setHeightRatio(static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Fade behavior"));

    m_chkFadeOnRx = makeCheckRow(QStringLiteral("Fade on RX"));
    connect(m_chkFadeOnRx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setFadeOnRx(on);
        notifyChanged();
    });

    m_chkFadeOnTx = makeCheckRow(QStringLiteral("Fade on TX"));
    connect(m_chkFadeOnTx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        ButtonBoxItem* b = qobject_cast<ButtonBoxItem*>(m_item);
        if (!b) { return; }
        b->setFadeOnTx(on);
        notifyChanged();
    });
}

} // namespace NereusSDR
