#include "RotatorItemEditor.h"
#include "../../meters/RotatorItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>
#include <QHBoxLayout>

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
    m_editBgPath->setText(r->backgroundImagePath());
    applyBtnColor(m_btnBigBlob,    r->bigBlobColour());
    applyBtnColor(m_btnSmallBlob,  r->smallBlobColour());
    applyBtnColor(m_btnOuterText,  r->outerTextColour());
    applyBtnColor(m_btnArrow,      r->arrowColour());
    applyBtnColor(m_btnBeamWidth,  r->beamWidthColour());
    applyBtnColor(m_btnBackground, r->backgroundColour());
    endProgrammaticUpdate();
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
    connect(m_chkShowBeamWidth, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setShowBeamWidth(on);
        notifyChanged();
    });

    m_spinBeamWidth = makeDoubleRow(QStringLiteral("Beam width (deg)"), 0.0, 360.0, 1.0, 1);
    connect(m_spinBeamWidth, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setBeamWidth(static_cast<float>(v));
        notifyChanged();
    });

    m_spinBeamAlpha = makeDoubleRow(QStringLiteral("Beam alpha"), 0.0, 1.0, 0.05, 2);
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

    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.0, 2.0, 0.05, 2);
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setPadding(static_cast<float>(v));
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
                               auto setter) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        QPushButton* btn = out;
        connect(btn, &QPushButton::clicked, this, [this, btn, setter]() {
            RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
            if (!r) { return; }
            const QColor chosen = QColorDialog::getColor(
                Qt::gray, this, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                (r->*setter)(chosen);
                applyBtnColor(btn, chosen);
                notifyChanged();
            }
        });
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Large dot"),    m_btnBigBlob,   &RotatorItem::setBigBlobColour);
    makeColorBtn(QStringLiteral("Small dot"),    m_btnSmallBlob, &RotatorItem::setSmallBlobColour);
    makeColorBtn(QStringLiteral("Outer text"),   m_btnOuterText, &RotatorItem::setOuterTextColour);
    makeColorBtn(QStringLiteral("Arrow"),        m_btnArrow,     &RotatorItem::setArrowColour);
    makeColorBtn(QStringLiteral("Beam width"),   m_btnBeamWidth, &RotatorItem::setBeamWidthColour);
    makeColorBtn(QStringLiteral("Background"),   m_btnBackground,&RotatorItem::setBackgroundColour);
}

} // namespace NereusSDR
