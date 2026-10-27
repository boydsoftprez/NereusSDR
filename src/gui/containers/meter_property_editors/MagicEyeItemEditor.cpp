// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I6): a colour dialog pick is
//                 recorded as the user's (markColourPicked). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "MagicEyeItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/MagicEyeItem.h"

#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QWidget>
#include <QColorDialog>
#include <QFileDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QStringList>

namespace NereusSDR {

MagicEyeItemEditor::MagicEyeItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void MagicEyeItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    MagicEyeItem* x = qobject_cast<MagicEyeItem*>(item);
    if (!x) { return; }

    beginProgrammaticUpdate();

    ItemColourLook::paintSwatch(m_btnGlowColor, QStringLiteral("MagicEye.glowColor"), x->glowColor());
    m_editBezelPath->setText(x->bezelImagePath());
    m_spinEyeScale->setValue(static_cast<double>(x->eyeScale()));
    m_spinBezelScale->setValue(static_cast<double>(x->bezelScale()));
    m_spinAttack->setValue(static_cast<double>(x->attackRatio()));
    m_spinDecay->setValue(static_cast<double>(x->decayRatio()));
    m_chkShowHistory->setChecked(x->showHistory());
    m_spinHistoryMs->setValue(x->historyDurationMs());
    ItemColourLook::paintSwatch(m_btnHistoryColour, QStringLiteral("MagicEye.historyColour"), x->historyColour());
    m_spinIgnoreHistoryMs->setValue(x->ignoreHistoryMs());
    QStringList cal;
    const QMap<float, float> points = x->scaleCalibration();
    for (auto it = points.constBegin(); it != points.constEnd(); ++it) {
        cal << QStringLiteral("%1=%2").arg(static_cast<double>(it.key()))
                                      .arg(static_cast<double>(it.value()));
    }
    m_editCalibration->setText(cal.join(QStringLiteral("; ")));

    endProgrammaticUpdate();
}

void MagicEyeItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Magic Eye"));

    // Glow color
    m_btnGlowColor = new QPushButton(this);
    m_btnGlowColor->setFixedSize(40, 18);
    auto applyGlowColor = [this](const QColor& c) {
        ItemColourLook::paintSwatch(m_btnGlowColor, QStringLiteral("MagicEye.glowColor"), c);
    };
    connect(m_btnGlowColor, &QPushButton::clicked, this, [this, applyGlowColor]() {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->glowColor(), this);
        if (chosen.isValid()) { markColourPicked(QStringLiteral("MagicEye.glowColor")); x->setGlowColor(chosen); applyGlowColor(chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Glow color"), m_btnGlowColor);

    // Bezel image path — line edit + Browse button in a container widget
    QWidget* bezelRow = new QWidget(this);
    QHBoxLayout* bezelLayout = new QHBoxLayout(bezelRow);
    bezelLayout->setContentsMargins(0, 0, 0, 0);
    bezelLayout->setSpacing(4);

    m_editBezelPath = new QLineEdit(bezelRow);
    bezelLayout->addWidget(m_editBezelPath);

    m_btnBrowseBezel = new QPushButton(QStringLiteral("Browse\u2026"), bezelRow);
    m_btnBrowseBezel->setFixedWidth(60);
    bezelLayout->addWidget(m_btnBrowseBezel);

    addRow(QStringLiteral("Bezel image"), bezelRow);

    // The built-in eye bezel art Thetis draws is not bundled: the option
    // stays visible, disabled, with the reason.
    QCheckBox* builtInBezel = makeCheckRow(QStringLiteral("Built-in bezel"));
    builtInBezel->setObjectName(QStringLiteral("builtInBezel"));
    builtInBezel->setEnabled(false);
    builtInBezel->setToolTip(noArtworkReason());

    connect(m_editBezelPath, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        x->setBezelImagePath(m_editBezelPath->text());
        notifyChanged();
    });

    connect(m_btnBrowseBezel, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select Bezel Image"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.bmp *.svg);;All files (*)"));
        if (path.isEmpty()) { return; }
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        x->setBezelImagePath(path);
        m_editBezelPath->setText(path);
        notifyChanged();
    });

    // Eye and bezel size: 0.01 to 1, 1 fills the item.
    m_spinEyeScale = makeDoubleRow(QStringLiteral("Eye size"),
                                   MagicEyeItem::kMinEyeScale, MagicEyeItem::kMaxEyeScale, 0.01, 2);
    m_spinEyeScale->setToolTip(QStringLiteral("Size of the eye; 1 fills the item."));
    connect(m_spinEyeScale, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setEyeScale(static_cast<float>(v));
        notifyChanged();
    });
    m_spinBezelScale = makeDoubleRow(QStringLiteral("Bezel size"),
                                     MagicEyeItem::kMinEyeScale, MagicEyeItem::kMaxEyeScale, 0.01, 2);
    m_spinBezelScale->setToolTip(QStringLiteral("Size of the bezel image; 1 fills the item."));
    connect(m_spinBezelScale, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setBezelScale(static_cast<float>(v));
        notifyChanged();
    });

    // Response
    m_spinAttack = makeDoubleRow(QStringLiteral("Attack ratio"), 0.0, 1.0, 0.01, 3);
    connect(m_spinAttack, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setAttackRatio(static_cast<float>(v));
        notifyChanged();
    });
    m_spinDecay = makeDoubleRow(QStringLiteral("Decay ratio"), 0.0, 1.0, 0.01, 3);
    connect(m_spinDecay, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setDecayRatio(static_cast<float>(v));
        notifyChanged();
    });

    // History
    addHeader(QStringLiteral("History"));
    m_chkShowHistory = makeCheckRow(QStringLiteral("Show history"));
    connect(m_chkShowHistory, &QCheckBox::toggled, this, [this](bool on) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setShowHistory(on);
        notifyChanged();
    });
    m_btnHistoryColour = new QPushButton(this);
    m_btnHistoryColour->setFixedSize(40, 18);
    connect(m_btnHistoryColour, &QPushButton::clicked, this, [this]() {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->historyColour(), this,
                                                     QStringLiteral("History color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) { return; }
        markColourPicked(QStringLiteral("MagicEye.historyColour"));
        x->setHistoryColour(chosen);
        ItemColourLook::paintSwatch(m_btnHistoryColour, QStringLiteral("MagicEye.historyColour"), chosen);
        notifyChanged();
    });
    addRow(QStringLiteral("History color"), m_btnHistoryColour);
    m_spinHistoryMs = makeIntRow(QStringLiteral("History (ms)"),
                                 kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
    connect(m_spinHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setHistoryDurationMs(v);
        notifyChanged();
    });
    m_spinIgnoreHistoryMs = makeIntRow(QStringLiteral("Ignore history (ms)"),
                                       kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
    m_spinIgnoreHistoryMs->setToolTip(QStringLiteral(
        "After switching between receive and transmit, or changing band, let "
        "the meter settle this long before it gathers history."));
    connect(m_spinIgnoreHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->setIgnoreHistoryMs(v);
        notifyChanged();
    });

    // Scale
    addHeader(QStringLiteral("Scale"));
    m_editCalibration = new QLineEdit(this);
    m_editCalibration->setPlaceholderText(QStringLiteral("dBm=position; ..."));
    m_editCalibration->setToolTip(QStringLiteral(
        "Scale points as dBm=position pairs, position 0 (closed) to 1 (open). "
        "Empty: the eye runs evenly from S0 to S9+60."));
    addRow(QStringLiteral("Calibration"), m_editCalibration);
    connect(m_editCalibration, &QLineEdit::editingFinished, this, [this]() {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (isProgrammaticUpdate() || !x) { return; }
        x->clearScaleCalibration();
        const QStringList pairs = m_editCalibration->text().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString& pair : pairs) {
            const int eq = pair.indexOf(QLatin1Char('='));
            if (eq <= 0) { continue; }
            bool okK = true;
            bool okV = true;
            const float key = pair.left(eq).trimmed().toFloat(&okK);
            const float pos = pair.mid(eq + 1).trimmed().toFloat(&okV);
            if (okK && okV) { x->addScaleCalibration(key, pos); }
        }
        notifyChanged();
    });
}

} // namespace NereusSDR
