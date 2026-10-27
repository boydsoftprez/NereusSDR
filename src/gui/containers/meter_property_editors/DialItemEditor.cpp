// no-port-check: NereusSDR-original Qt property adapter; the dial setting ranges are cited facts.
// Modification history (NereusSDR):
//   2026-10-26 - Colour swatches show the colour drawn and say when it
//                 follows the workspace look (ItemColourLook::paintSwatch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Pad pressed color, size, text size, Tunes (Source slice or
//                 Any slice), degrees per step, align, and the acceleration
//                 rows (accelerate, thresholds kept 90 apart, interval, most
//                 changes). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
#include "DialItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/DialItem.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

namespace NereusSDR {

// Helper macro-style lambda factory used locally to reduce repetition for
// color buttons in this file only.
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

DialItemEditor::DialItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void DialItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    DialItem* d = qobject_cast<DialItem*>(item);
    if (!d) { return; }

    beginProgrammaticUpdate();
    applyBtnColor(m_btnTextColour, QStringLiteral("Dial.textColour"), d->textColour());
    applyBtnColor(m_btnCircleColour, QStringLiteral("Dial.circleColour"), d->circleColour());
    applyBtnColor(m_btnPadColour, QStringLiteral("Dial.padColour"), d->padColour());
    applyBtnColor(m_btnPadPressedColour, QStringLiteral("Dial.padPressedColour"), d->padPressedColour());
    applyBtnColor(m_btnRingColour, QStringLiteral("Dial.ringColour"), d->ringColour());
    applyBtnColor(m_btnBtnOn, QStringLiteral("Dial.buttonOnColour"), d->buttonOnColour());
    applyBtnColor(m_btnBtnOff, QStringLiteral("Dial.buttonOffColour"), d->buttonOffColour());
    applyBtnColor(m_btnBtnHighlight, QStringLiteral("Dial.buttonHighlightColour"), d->buttonHighlightColour());
    applyBtnColor(m_btnSlowColour, QStringLiteral("Dial.slowColour"), d->slowColour());
    applyBtnColor(m_btnHoldColour, QStringLiteral("Dial.holdColour"), d->holdColour());
    applyBtnColor(m_btnFastColour, QStringLiteral("Dial.fastColour"), d->fastColour());
    m_spinVerticalRatio->setValue(static_cast<double>(d->verticalRatio()));
    m_spinFontScale->setValue(static_cast<double>(d->fontScale()));
    m_comboSliceSelect->setCurrentIndex(
        m_comboSliceSelect->findData(static_cast<int>(d->sliceSelect())));
    m_spinDegreesPerChange->setValue(d->degreesPerChange());
    m_chkAlign->setChecked(d->alignWithTuneStep());
    m_chkAccelerate->setChecked(d->accelEnabled());
    m_spinUpThreshold->setValue(d->upThreshold());
    m_spinDownThreshold->setValue(d->downThreshold());
    m_spinInterval->setValue(d->tuneStepIntervalSeconds());
    m_spinMaxChanges->setValue(d->maxTuneStepChanges());
    endProgrammaticUpdate();
}

void DialItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Dial colors"));

    // Helper: create a 40x18 color button and wire it.
    auto makeColorBtn = [this](const QString& label, QPushButton*& out,
                               const QString& field, auto getter, auto setter) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        QPushButton* btn = out; // capture by value — points to the same object
        connect(btn, &QPushButton::clicked, this, [this, btn, field, getter, setter]() {
            DialItem* d = qobject_cast<DialItem*>(m_item);
            if (!d) { return; }
            const QColor chosen = QColorDialog::getColor(
                (d->*getter)(), this, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                (d->*setter)(chosen);
                applyBtnColor(btn, field, chosen);
                notifyChanged();
            }
        });
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Text color"),       m_btnTextColour, QStringLiteral("Dial.textColour"),
                 &DialItem::textColour,   &DialItem::setTextColour);
    makeColorBtn(QStringLiteral("Circle color"),     m_btnCircleColour, QStringLiteral("Dial.circleColour"),
                 &DialItem::circleColour, &DialItem::setCircleColour);
    makeColorBtn(QStringLiteral("Pad color"),        m_btnPadColour, QStringLiteral("Dial.padColour"),
                 &DialItem::padColour,    &DialItem::setPadColour);
    makeColorBtn(QStringLiteral("Pad pressed color"), m_btnPadPressedColour, QStringLiteral("Dial.padPressedColour"),
                 &DialItem::padPressedColour, &DialItem::setPadPressedColour);
    makeColorBtn(QStringLiteral("Ring color"),       m_btnRingColour, QStringLiteral("Dial.ringColour"),
                 &DialItem::ringColour,   &DialItem::setRingColour);

    addHeader(QStringLiteral("Button colors"));
    makeColorBtn(QStringLiteral("Button on"),        m_btnBtnOn, QStringLiteral("Dial.buttonOnColour"),
                 &DialItem::buttonOnColour,        &DialItem::setButtonOnColour);
    makeColorBtn(QStringLiteral("Button off"),       m_btnBtnOff, QStringLiteral("Dial.buttonOffColour"),
                 &DialItem::buttonOffColour,       &DialItem::setButtonOffColour);
    makeColorBtn(QStringLiteral("Button highlight"), m_btnBtnHighlight, QStringLiteral("Dial.buttonHighlightColour"),
                 &DialItem::buttonHighlightColour, &DialItem::setButtonHighlightColour);

    addHeader(QStringLiteral("Speed indicator colors"));
    makeColorBtn(QStringLiteral("Slow color"),  m_btnSlowColour, QStringLiteral("Dial.slowColour"),
                 &DialItem::slowColour, &DialItem::setSlowColour);
    makeColorBtn(QStringLiteral("Hold color"),  m_btnHoldColour, QStringLiteral("Dial.holdColour"),
                 &DialItem::holdColour, &DialItem::setHoldColour);
    makeColorBtn(QStringLiteral("Fast color"),  m_btnFastColour, QStringLiteral("Dial.fastColour"),
                 &DialItem::fastColour, &DialItem::setFastColour);

    // Ranges from Thetis MeterManager.cs:27749-27760 [v2.10.3.15] (the
    // dial's setup settings); each setter clamps as well.
    addHeader(QStringLiteral("Dial"));
    m_spinVerticalRatio = makeDoubleRow(QStringLiteral("Size"), 0.01, 1.0, 0.01, 2);
    m_spinVerticalRatio->setToolTip(QStringLiteral("The dial's size as a share of its area."));
    connect(m_spinVerticalRatio, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setVerticalRatio(static_cast<float>(v));
        notifyChanged();
    });
    m_spinFontScale = makeDoubleRow(QStringLiteral("Text size"), 0.01, 1.1, 0.01, 2);
    m_spinFontScale->setToolTip(QStringLiteral("Scales the dial's labels."));
    connect(m_spinFontScale, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setFontScale(static_cast<float>(v));
        notifyChanged();
    });

    addHeader(QStringLiteral("Tuning"));
    m_comboSliceSelect = new QComboBox(this);
    m_comboSliceSelect->setStyleSheet(kComboStyle);
    m_comboSliceSelect->addItem(QStringLiteral("Source slice"),
                                static_cast<int>(DialItem::SliceSelect::SourceSlice));
    m_comboSliceSelect->addItem(QStringLiteral("Any slice"),
                                static_cast<int>(DialItem::SliceSelect::AnySlice));
    m_comboSliceSelect->setToolTip(QStringLiteral(
        "Source slice tunes the slice this item's Source gives. Any slice tunes the "
        "active slice, and the top buttons select the slice before or after it."));
    addRow(QStringLiteral("Tunes"), m_comboSliceSelect);
    connect(m_comboSliceSelect, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setSliceSelect(static_cast<DialItem::SliceSelect>(m_comboSliceSelect->currentData().toInt()));
        notifyChanged();
    });
    m_spinDegreesPerChange = makeIntRow(QStringLiteral("Degrees per step"), 1, 90);
    m_spinDegreesPerChange->setToolTip(QStringLiteral(
        "How far the dial turns for one tuning step."));
    connect(m_spinDegreesPerChange, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setDegreesPerChange(v);
        notifyChanged();
    });
    m_chkAlign = makeCheckRow(QStringLiteral("Align to step"));
    m_chkAlign->setToolTip(QStringLiteral(
        "Round the frequency to a whole number of tuning steps as the dial tunes."));
    connect(m_chkAlign, &QCheckBox::toggled, this, [this](bool on) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setAlignWithTuneStep(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Acceleration"));
    m_chkAccelerate = makeCheckRow(QStringLiteral("Accelerate"));
    m_chkAccelerate->setToolTip(QStringLiteral(
        "Turning fast moves to a larger tuning step; letting go restores the step."));
    connect(m_chkAccelerate, &QCheckBox::toggled, this, [this](bool on) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setAccelEnabled(on);
        notifyChanged();
    });
    // Up and down stay 90 degrees per second apart: the item's setter moves
    // the value, and both boxes show what it kept.
    const auto showThresholds = [this](DialItem* d) {
        const QSignalBlocker upBlock(m_spinUpThreshold);
        const QSignalBlocker downBlock(m_spinDownThreshold);
        m_spinUpThreshold->setValue(d->upThreshold());
        m_spinDownThreshold->setValue(d->downThreshold());
    };
    m_spinUpThreshold = makeIntRow(QStringLiteral("Larger step above (deg/s)"), 90, 720);
    m_spinUpThreshold->setToolTip(QStringLiteral(
        "Turning faster than this moves to the next larger tuning step."));
    m_spinDownThreshold = makeIntRow(QStringLiteral("Smaller step below (deg/s)"), 90, 720);
    m_spinDownThreshold->setToolTip(QStringLiteral(
        "Turning slower than this moves back toward the starting step."));
    connect(m_spinUpThreshold, qOverload<int>(&QSpinBox::valueChanged),
            this, [this, showThresholds](int v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setUpThreshold(v);
        showThresholds(d);
        notifyChanged();
    });
    connect(m_spinDownThreshold, qOverload<int>(&QSpinBox::valueChanged),
            this, [this, showThresholds](int v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setDownThreshold(v);
        showThresholds(d);
        notifyChanged();
    });
    m_spinInterval = makeIntRow(QStringLiteral("Seconds between changes"), 1, 10);
    m_spinInterval->setToolTip(QStringLiteral(
        "The shortest time between two changes of the tuning step."));
    connect(m_spinInterval, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setTuneStepIntervalSeconds(v);
        notifyChanged();
    });
    m_spinMaxChanges = makeIntRow(QStringLiteral("Most larger steps"), 1, 30);
    m_spinMaxChanges->setToolTip(QStringLiteral(
        "How many times one turn may move to a larger tuning step."));
    connect(m_spinMaxChanges, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        DialItem* d = qobject_cast<DialItem*>(m_item);
        if (isProgrammaticUpdate() || !d) { return; }
        d->setMaxTuneStepChanges(v);
        notifyChanged();
    });
}

} // namespace NereusSDR
