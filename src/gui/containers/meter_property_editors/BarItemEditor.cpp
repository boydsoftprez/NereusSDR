// Modification history (NereusSDR):
//   2026-10-26 - Colour swatches show the colour drawn and say when it
//                 follows the workspace look (ItemColourLook::paintSwatch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Rows for the BAR options: style (adds Line, Segments,
//                 Solid), value and peak text, markers, peak hold, history,
//                 ignore history, power limit and calibration. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "BarItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/MeterItem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QColorDialog>
#include <QStringList>

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
    ItemColourLook::paintSwatch(m_btnBarColor, QStringLiteral("Bar.barColor"), bar->barColor());
    ItemColourLook::paintSwatch(m_btnBarRedColor, QStringLiteral("Bar.barRedColor"), bar->barRedColor());

    // Edge-mode colors
    ItemColourLook::paintSwatch(m_btnEdgeBg, QStringLiteral("Bar.edgeBackgroundColor"), bar->edgeBackgroundColor());
    ItemColourLook::paintSwatch(m_btnEdgeLow, QStringLiteral("Bar.edgeLowColor"), bar->edgeLowColor());
    ItemColourLook::paintSwatch(m_btnEdgeHigh, QStringLiteral("Bar.edgeHighColor"), bar->edgeHighColor());
    ItemColourLook::paintSwatch(m_btnEdgeAvg, QStringLiteral("Bar.edgeAvgColor"), bar->edgeAvgColor());

    // Options
    m_chkShowValue->setChecked(bar->showValue());
    m_chkShowPeakValue->setChecked(bar->showPeakValue());
    m_chkShowMarker->setChecked(bar->showMarker());
    m_chkPeakHold->setChecked(bar->peakHold());
    m_chkShowHistory->setChecked(bar->showHistory());
    m_spinHistoryMs->setValue(bar->historyDurationMs());
    m_spinIgnoreHistoryMs->setValue(bar->ignoreHistoryMs());
    m_spinPowerLimit->setValue(bar->powerLimit());
    QStringList cal;
    const QMap<double, float> points = bar->scaleCalibration();
    for (auto it = points.constBegin(); it != points.constEnd(); ++it) {
        cal << QStringLiteral("%1=%2").arg(it.key()).arg(static_cast<double>(it.value()));
    }
    m_editCalibration->setText(cal.join(QStringLiteral("; ")));
    for (const auto& refresh : m_swatchRefreshers) { refresh(*bar); }

    endProgrammaticUpdate();
}

void BarItemEditor::paintSwatch(QPushButton* button, const QString& field, const QColor& colour)
{
    ItemColourLook::paintSwatch(button, field, colour);
}

QPushButton* BarItemEditor::makeColourRow(const QString& label, const QString& field,
                                          std::function<QColor(const BarItem&)> get,
                                          std::function<void(BarItem&, const QColor&)> set)
{
    auto* button = new QPushButton(this);
    button->setFixedSize(40, 18);
    connect(button, &QPushButton::clicked, this, [this, button, label, field, get, set]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (!bar) { return; }
        const QColor chosen = QColorDialog::getColor(get(*bar), this, label,
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            markColourPicked(field);
            set(*bar, chosen);
            paintSwatch(button, field, chosen);
            notifyChanged();
        }
    });
    m_swatchRefreshers.append([button, field, get](const BarItem& bar) { paintSwatch(button, field, get(bar)); });
    addRow(label, button);
    return button;
}

void BarItemEditor::buildOptionRows()
{
    // --- Value and peak text ---
    addHeader(QStringLiteral("Readout"));
    m_chkShowValue = makeCheckRow(QStringLiteral("Show value"));
    connect(m_chkShowValue, &QCheckBox::toggled, this, [this](bool on) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setShowValue(on);
        notifyChanged();
    });
    m_btnFontColour = makeColourRow(QStringLiteral("Value color"), QStringLiteral("Bar.fontColour"),
        [](const BarItem& b) { return b.fontColour(); },
        [](BarItem& b, const QColor& c) { b.setFontColour(c); });
    m_chkShowPeakValue = makeCheckRow(QStringLiteral("Show peak value"));
    connect(m_chkShowPeakValue, &QCheckBox::toggled, this, [this](bool on) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setShowPeakValue(on);
        notifyChanged();
    });
    m_btnPeakValueColour = makeColourRow(QStringLiteral("Peak value color"), QStringLiteral("Bar.peakFontColour"),
        [](const BarItem& b) { return b.peakValueColour(); },
        [](BarItem& b, const QColor& c) { b.setPeakValueColour(c); });

    // --- Markers ---
    addHeader(QStringLiteral("Markers"));
    m_chkShowMarker = makeCheckRow(QStringLiteral("Show marker"));
    connect(m_chkShowMarker, &QCheckBox::toggled, this, [this](bool on) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        // Pin the peak hold marker so it no longer follows this box.
        bar->setPeakHold(bar->peakHold());
        bar->setShowMarker(on);
        notifyChanged();
    });
    m_btnMarkerColour = makeColourRow(QStringLiteral("Marker color"), QStringLiteral("Bar.markerColour"),
        [](const BarItem& b) { return b.markerColour(); },
        [](BarItem& b, const QColor& c) { b.setMarkerColour(c); });
    m_chkPeakHold = makeCheckRow(QStringLiteral("Peak hold"));
    connect(m_chkPeakHold, &QCheckBox::toggled, this, [this](bool on) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setPeakHold(on);
        notifyChanged();
    });
    m_btnPeakHoldColour = makeColourRow(QStringLiteral("Peak hold color"), QStringLiteral("Bar.peakHoldMarkerColour"),
        [](const BarItem& b) { return b.peakHoldMarkerColour(); },
        [](BarItem& b, const QColor& c) { b.setPeakHoldMarkerColour(c); });

    // --- History ---
    addHeader(QStringLiteral("History"));
    m_chkShowHistory = makeCheckRow(QStringLiteral("Show history"));
    connect(m_chkShowHistory, &QCheckBox::toggled, this, [this](bool on) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setShowHistory(on);
        notifyChanged();
    });
    m_btnHistoryColour = makeColourRow(QStringLiteral("History color"), QStringLiteral("Bar.historyColour"),
        [](const BarItem& b) { return b.historyColour(); },
        [](BarItem& b, const QColor& c) { b.setHistoryColour(c); });
    m_spinHistoryMs = makeIntRow(QStringLiteral("History (ms)"),
                                 kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
    m_spinHistoryMs->setToolTip(QStringLiteral(
        "How long the history and the peak value look back."));
    connect(m_spinHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setHistoryDurationMs(v);
        notifyChanged();
    });
    m_spinIgnoreHistoryMs = makeIntRow(QStringLiteral("Ignore history (ms)"),
                                       kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
    m_spinIgnoreHistoryMs->setToolTip(QStringLiteral(
        "After switching between receive and transmit, or changing band, let "
        "the meter settle this long before it gathers history and peak values."));
    connect(m_spinIgnoreHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setIgnoreHistoryMs(v);
        notifyChanged();
    });

    // --- Scale ---
    addHeader(QStringLiteral("Scale"));
    m_spinPowerLimit = makeDoubleRow(QStringLiteral("Power limit (W)"),
                                     kMinBarPowerLimitW, kMaxBarPowerLimitW, 0.1, 1);
    m_spinPowerLimit->setToolTip(QStringLiteral(
        "Power limit of the scale on a forward or reverse power bar."));
    connect(m_spinPowerLimit, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->setPowerLimit(v);
        notifyChanged();
    });
    m_editCalibration = new QLineEdit(this);
    m_editCalibration->setPlaceholderText(QStringLiteral("value=position; ..."));
    m_editCalibration->setToolTip(QStringLiteral(
        "Scale points as value=position pairs, position 0 to 1 across the bar. "
        "Empty: the bar runs from Min value to Max value."));
    addRow(QStringLiteral("Calibration"), m_editCalibration);
    connect(m_editCalibration, &QLineEdit::editingFinished, this, [this]() {
        BarItem* bar = qobject_cast<BarItem*>(m_item);
        if (isProgrammaticUpdate() || !bar) { return; }
        bar->clearScaleCalibration();
        const QStringList pairs = m_editCalibration->text().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString& pair : pairs) {
            const int eq = pair.indexOf(QLatin1Char('='));
            if (eq <= 0) { continue; }
            bool okK = true;
            bool okV = true;
            const double key = pair.left(eq).trimmed().toDouble(&okK);
            const float pos = pair.mid(eq + 1).trimmed().toFloat(&okV);
            if (okK && okV) { bar->addScaleCalibration(key, pos); }
        }
        notifyChanged();
    });
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
    m_comboBarStyle->addItem(QStringLiteral("Line"),
                             static_cast<int>(BarItem::BarStyle::Line));
    m_comboBarStyle->addItem(QStringLiteral("Segments"),
                             static_cast<int>(BarItem::BarStyle::Segments));
    m_comboBarStyle->addItem(QStringLiteral("Solid"),
                             static_cast<int>(BarItem::BarStyle::Solid));
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
            markColourPicked(QStringLiteral("Bar.barColor"));
            bar->setBarColor(chosen);
            ItemColourLook::paintSwatch(m_btnBarColor, QStringLiteral("Bar.barColor"), chosen);
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
            markColourPicked(QStringLiteral("Bar.barRedColor"));
            bar->setBarRedColor(chosen);
            ItemColourLook::paintSwatch(m_btnBarRedColor, QStringLiteral("Bar.barRedColor"), chosen);
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
            markColourPicked(QStringLiteral("Bar.edgeBackgroundColor"));
            bar->setEdgeBackgroundColor(chosen);
            ItemColourLook::paintSwatch(m_btnEdgeBg, QStringLiteral("Bar.edgeBackgroundColor"), chosen);
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
            markColourPicked(QStringLiteral("Bar.edgeLowColor"));
            bar->setEdgeLowColor(chosen);
            ItemColourLook::paintSwatch(m_btnEdgeLow, QStringLiteral("Bar.edgeLowColor"), chosen);
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
            markColourPicked(QStringLiteral("Bar.edgeHighColor"));
            bar->setEdgeHighColor(chosen);
            ItemColourLook::paintSwatch(m_btnEdgeHigh, QStringLiteral("Bar.edgeHighColor"), chosen);
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
            markColourPicked(QStringLiteral("Bar.edgeAvgColor"));
            bar->setEdgeAvgColor(chosen);
            ItemColourLook::paintSwatch(m_btnEdgeAvg, QStringLiteral("Bar.edgeAvgColor"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Edge avg color"), m_btnEdgeAvg);

    buildOptionRows();
}

} // namespace NereusSDR
