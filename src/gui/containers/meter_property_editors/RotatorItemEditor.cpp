// no-port-check: NereusSDR-original Qt property adapter; it cites Thetis Setup ranges and row enabling as facts.
// Modification history (NereusSDR):
//   2026-10-26 - Thetis rotator settings: beam width 0-180 in 0.05 steps
//                 and padding 0.001-1 (the Thetis Setup ranges), the beam
//                 width rows disabled when the beam is off, a full background
//                 row; setting padding lets it size the face. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "RotatorItemEditor.h"
#include "../../meters/RotatorItem.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>
#include <QFormLayout>
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
    m_chkFullBackground->setChecked(r->fullBackground());
    m_editBgPath->setText(r->backgroundImagePath());
    applyBtnColor(m_btnBigBlob,    r->bigBlobColour());
    applyBtnColor(m_btnSmallBlob,  r->smallBlobColour());
    applyBtnColor(m_btnOuterText,  r->outerTextColour());
    applyBtnColor(m_btnArrow,      r->arrowColour());
    applyBtnColor(m_btnBeamWidth,  r->beamWidthColour());
    applyBtnColor(m_btnBackground, r->backgroundColour());
    updateBeamWidthRows();
    endProgrammaticUpdate();
}

// From Thetis setup.cs:31263-31271 [v2.10.3.15] (updateShowBeamWidthControls):
// the beam width colour, degrees and alpha are enabled only while the beam
// width is shown.
void RotatorItemEditor::updateBeamWidthRows()
{
    const bool en = m_chkShowBeamWidth->isChecked();
    for (QWidget* field : {static_cast<QWidget*>(m_spinBeamWidth), static_cast<QWidget*>(m_spinBeamAlpha),
                           static_cast<QWidget*>(m_btnBeamWidth)}) {
        if (!field) { continue; }
        field->setEnabled(en);
        if (QWidget* label = m_form->labelForField(field)) { label->setEnabled(en); }
    }
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
    m_chkShowBeamWidth->setObjectName(QStringLiteral("rotatorShowBeamWidth"));
    connect(m_chkShowBeamWidth, &QCheckBox::toggled, this, [this](bool on) {
        updateBeamWidthRows();
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setShowBeamWidth(on);
        notifyChanged();
    });

    // From Thetis setup.designer.cs:64527-64556 [v2.10.3.15]
    // (nudMeterItemRotatorBeamWidth): 0 to 180, step 0.05, one decimal.
    m_spinBeamWidth = makeDoubleRow(QStringLiteral("Beam width (deg)"), 0.0, 180.0, 0.05, 1);
    m_spinBeamWidth->setObjectName(QStringLiteral("rotatorBeamWidth"));
    connect(m_spinBeamWidth, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setBeamWidth(static_cast<float>(v));
        notifyChanged();
    });

    m_spinBeamAlpha = makeDoubleRow(QStringLiteral("Beam alpha"), 0.0, 1.0, 0.05, 2);
    m_spinBeamAlpha->setObjectName(QStringLiteral("rotatorBeamAlpha"));
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

    // From Thetis setup.designer.cs:64344-64373 [v2.10.3.15]
    // (nudMeterItemRotator_padding): 0.001 to 1, step 0.001, three decimals.
    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.001, 1.0, 0.001, 3);
    m_spinPadding->setObjectName(QStringLiteral("rotatorPadding"));
    m_spinPadding->setToolTip(QStringLiteral(
        "Size of the azimuth or elevation face, as a share of the item's width."));
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setPadding(static_cast<float>(v));
        // An older item starts sizing its face by padding once it is set.
        r->setPaddingLayout(true);
        notifyChanged();
    });

    m_chkFullBackground = makeCheckRow(QStringLiteral("Full background"));
    m_chkFullBackground->setObjectName(QStringLiteral("rotatorFullBackground"));
    m_chkFullBackground->setToolTip(QStringLiteral(
        "Fill the whole item with the background colour, not only the compass."));
    connect(m_chkFullBackground, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        RotatorItem* r = qobject_cast<RotatorItem*>(m_item);
        if (!r) { return; }
        r->setFullBackground(on);
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
    m_btnBeamWidth->setObjectName(QStringLiteral("rotatorBeamWidthColour"));
    updateBeamWidthRows();
}

} // namespace NereusSDR
