// Modification history (NereusSDR):
//   2026-10-26 - Colour swatches show the colour drawn and say when it
//                 follows the workspace look (ItemColourLook::paintSwatch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Display mode by slice letter (Source slice, All slices,
//                 Slice A and on) in place of VFO A / VFO B / Both; layout,
//                 band text and the remaining VFO display colours, with a
//                 "Copy to small digits" button. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#include "VfoDisplayItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/VfoDisplayItem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QString& field, const QColor& c)
{
    ItemColourLook::paintSwatch(btn, field, c);
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

VfoDisplayItemEditor::VfoDisplayItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void VfoDisplayItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(item);
    if (!v) { return; }

    beginProgrammaticUpdate();
    m_comboDisplayMode->setCurrentIndex(m_comboDisplayMode->findData(v->displayModeValue()));
    m_comboLayout->setCurrentIndex(m_comboLayout->findData(static_cast<int>(v->layout())));
    m_chkBandText->setChecked(v->showBandText());
    m_btnBandTextColour->setEnabled(v->showBandText());
    for (const ColourRow& row : std::as_const(m_colourRows)) {
        applyBtnColor(row.button, row.field, row.get(*v));
    }
    endProgrammaticUpdate();
}

void VfoDisplayItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("VFO Display"));

    m_comboDisplayMode = new QComboBox(this);
    m_comboDisplayMode->setObjectName(QStringLiteral("vfoDisplayMode"));
    m_comboDisplayMode->setStyleSheet(kComboStyle);
    m_comboDisplayMode->addItem(QStringLiteral("Source slice"),
        static_cast<int>(VfoDisplayItem::DisplayMode::SourceSlice));
    m_comboDisplayMode->addItem(QStringLiteral("All slices"),
        static_cast<int>(VfoDisplayItem::DisplayMode::AllSlices));
    for (int i = 0; i < VfoDisplayItem::kMaxSliceCount; ++i) {
        m_comboDisplayMode->addItem(
            QStringLiteral("Slice ") + QChar(QLatin1Char(static_cast<char>('A' + i))),
            VfoDisplayItem::fixedSliceMode(i));
    }
    addRow(QStringLiteral("Display mode"), m_comboDisplayMode);
    connect(m_comboDisplayMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
        if (!v) { return; }
        v->setDisplayModeValue(m_comboDisplayMode->currentData().toInt());
        notifyChanged();
    });

    m_comboLayout = new QComboBox(this);
    m_comboLayout->setObjectName(QStringLiteral("vfoLayout"));
    m_comboLayout->setStyleSheet(kComboStyle);
    m_comboLayout->addItem(QStringLiteral("Compact"), static_cast<int>(VfoDisplayItem::Layout::Compact));
    m_comboLayout->addItem(QStringLiteral("Detailed"), static_cast<int>(VfoDisplayItem::Layout::Thetis));
    addRow(QStringLiteral("Layout"), m_comboLayout);
    connect(m_comboLayout, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
        if (!v) { return; }
        v->setLayout(static_cast<VfoDisplayItem::Layout>(m_comboLayout->currentData().toInt()));
        notifyChanged();
    });

    m_chkBandText = makeCheckRow(QStringLiteral("Show band text"));
    m_chkBandText->setObjectName(QStringLiteral("vfoShowBandText"));
    connect(m_chkBandText, &QCheckBox::toggled, this, [this](bool on) {
        if (m_btnBandTextColour) { m_btnBandTextColour->setEnabled(on); }
        if (isProgrammaticUpdate()) { return; }
        VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
        if (!v) { return; }
        v->setShowBandText(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Colors"));

    auto makeColorBtn = [this](const QString& label, QPushButton*& out,
                               auto setter, auto getter, const QString& field) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        QPushButton* btn = out;
        connect(btn, &QPushButton::clicked, this, [this, btn, setter, field]() {
            VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
            if (!v) { return; }
            const QColor chosen = QColorDialog::getColor(
                Qt::gray, this, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                (v->*setter)(chosen);
                applyBtnColor(btn, field, chosen);
                notifyChanged();
            }
        });
        m_colourRows.append({btn, field, [getter](const VfoDisplayItem& v) { return (v.*getter)(); }});
        addRow(label, out);
    };

    QPushButton* title = nullptr;
    QPushButton* splitBack = nullptr;
    QPushButton* split = nullptr;
    QPushButton* highlight = nullptr;
    QPushButton* lock = nullptr;
    QPushButton* sync = nullptr;
    QPushButton* back = nullptr;
    makeColorBtn(QStringLiteral("Title"),        title,              &VfoDisplayItem::setTitleColour,          &VfoDisplayItem::titleColour, QStringLiteral("VfoDisplay.titleColour"));
    makeColorBtn(QStringLiteral("Frequency"),    m_btnFreqColour,    &VfoDisplayItem::setFrequencyColour,      &VfoDisplayItem::frequencyColour, QStringLiteral("VfoDisplay.frequencyColour"));
    makeColorBtn(QStringLiteral("Small digits"), m_btnFreqSmallColour, &VfoDisplayItem::setFrequencySmallColour, &VfoDisplayItem::frequencySmallColour, QStringLiteral("VfoDisplay.frequencySmallColour"));

    // The small digits take the frequency colour.
    m_btnCopyToSmall = new QPushButton(QStringLiteral("Copy to small digits"), this);
    m_btnCopyToSmall->setObjectName(QStringLiteral("vfoCopyToSmallDigits"));
    connect(m_btnCopyToSmall, &QPushButton::clicked, this, [this]() {
        VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
        if (!v) { return; }
        v->setFrequencySmallColour(v->frequencyColour());
        applyBtnColor(m_btnFreqSmallColour, QStringLiteral("VfoDisplay.frequencySmallColour"), v->frequencySmallColour());
        notifyChanged();
    });
    addRow(QString(), m_btnCopyToSmall);

    makeColorBtn(QStringLiteral("Mode"),            m_btnModeColour,     &VfoDisplayItem::setModeColour,           &VfoDisplayItem::modeColour, QStringLiteral("VfoDisplay.modeColour"));
    makeColorBtn(QStringLiteral("Filter"),          m_btnFilterColour,   &VfoDisplayItem::setFilterColour,         &VfoDisplayItem::filterColour, QStringLiteral("VfoDisplay.filterColour"));
    makeColorBtn(QStringLiteral("Band"),            m_btnBandColour,     &VfoDisplayItem::setBandColour,           &VfoDisplayItem::bandColour, QStringLiteral("VfoDisplay.bandColour"));
    makeColorBtn(QStringLiteral("Band text"),       m_btnBandTextColour, &VfoDisplayItem::setBandTextColour,       &VfoDisplayItem::bandTextColour, QStringLiteral("VfoDisplay.bandTextColour"));
    makeColorBtn(QStringLiteral("Split back"),      splitBack,           &VfoDisplayItem::setSplitBackColour,      &VfoDisplayItem::splitBackColour, QStringLiteral("VfoDisplay.splitBackColour"));
    makeColorBtn(QStringLiteral("Split"),           split,               &VfoDisplayItem::setSplitColour,          &VfoDisplayItem::splitColour, QStringLiteral("VfoDisplay.splitColour"));
    makeColorBtn(QStringLiteral("Digit highlight"), highlight,           &VfoDisplayItem::setDigitHighlightColour, &VfoDisplayItem::digitHighlightColour, QStringLiteral("VfoDisplay.digitHighlightColour"));
    makeColorBtn(QStringLiteral("RX label"),        m_btnRxColour,       &VfoDisplayItem::setRxColour,             &VfoDisplayItem::rxColour, QStringLiteral("VfoDisplay.rxColour"));
    makeColorBtn(QStringLiteral("TX label"),        m_btnTxColour,       &VfoDisplayItem::setTxColour,             &VfoDisplayItem::txColour, QStringLiteral("VfoDisplay.txColour"));
    makeColorBtn(QStringLiteral("Lock"),            lock,                &VfoDisplayItem::setLockColour,           &VfoDisplayItem::lockColour, QStringLiteral("VfoDisplay.lockColour"));
    makeColorBtn(QStringLiteral("Sync"),            sync,                &VfoDisplayItem::setSyncColour,           &VfoDisplayItem::syncColour, QStringLiteral("VfoDisplay.syncColour"));
    makeColorBtn(QStringLiteral("Background"),      back,                &VfoDisplayItem::setBackgroundColour,     &VfoDisplayItem::backgroundColour, QStringLiteral("VfoDisplay.backgroundColour"));
}

} // namespace NereusSDR
