// no-port-check: NereusSDR-original Qt property adapter; global-unit control disabled in container drafts.
#include "SignalTextItemEditor.h"
#include "../../meters/SignalTextItem.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QColorDialog>
#include <QSpinBox>

namespace NereusSDR {

SignalTextItemEditor::SignalTextItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void SignalTextItemEditor::useGlobalUnits()
{
    m_comboUnits->setEnabled(false);
    m_comboUnits->setToolTip(tr("Signal units and decimal precision follow the global multimeter preferences."));
    auto* note = new QLabel(m_comboUnits->toolTip(), this);
    note->setWordWrap(true);
    addRow(tr("Global preferences"), note);
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

    const int typeIdx = m_comboTypeStyle->findData(static_cast<int>(x->typeStyle()));
    m_comboTypeStyle->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);
    applyColor(m_btnTypeColour, x->typeColour());
    m_chkShowSubMarker->setChecked(x->showSubMarker());
    applyColor(m_btnSubMarkerColour, x->subMarkerColour());
    applyColor(m_btnBackgroundColour, x->backgroundColour());
    m_chkPeakNeedsHold->setChecked(x->peakValueNeedsPeakHold());
    m_spinHistoryMs->setValue(x->historyDurationMs());
    m_spinIgnoreHistoryMs->setValue(x->ignoreHistoryMs());

    endProgrammaticUpdate();
}

void SignalTextItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Signal Text"));

    // Units
    m_comboUnits = new QComboBox(this);
    m_comboUnits->setObjectName("signalGlobalUnits");
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
    addRow(QStringLiteral("Color"), m_btnColour);

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
    addRow(QStringLiteral("Peak color"), m_btnPeakColour);

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
    addRow(QStringLiteral("Marker color"), m_btnMarkerColour);

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

    // A color button row that sets one color on the item.
    const auto colourRow = [this](QPushButton*& button, const QString& label,
                                  QColor (SignalTextItem::*get)() const,
                                  void (SignalTextItem::*set)(const QColor&)) {
        button = new QPushButton(this);
        button->setFixedSize(40, 18);
        QPushButton* b = button;
        connect(b, &QPushButton::clicked, this, [this, b, label, get, set]() {
            SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
            if (!x) { return; }
            const QColor chosen = QColorDialog::getColor((x->*get)(), this, label,
                                                         QColorDialog::ShowAlphaChannel);
            if (!chosen.isValid()) { return; }
            (x->*set)(chosen);
            b->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        });
        addRow(label, button);
    };

    // Type: the units under the value, or the reading's PK / AVG / FFT mark.
    m_comboTypeStyle = new QComboBox(this);
    m_comboTypeStyle->addItem(QStringLiteral("Units"),
                              static_cast<int>(SignalTextItem::TypeStyle::UnitsLabel));
    m_comboTypeStyle->addItem(QStringLiteral("Reading (PK, AVG or FFT)"),
                              static_cast<int>(SignalTextItem::TypeStyle::ReadingGlyph));
    m_comboTypeStyle->setToolTip(QStringLiteral(
        "What Show type draws: the units under the value, or a mark in the top "
        "right for the reading: PK for peak, AVG for average, FFT for the "
        "strongest spectrum bin."));
    addRow(QStringLiteral("Type shows"), m_comboTypeStyle);
    connect(m_comboTypeStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setTypeStyle(static_cast<SignalTextItem::TypeStyle>(m_comboTypeStyle->currentData().toInt()));
        notifyChanged();
    });
    colourRow(m_btnTypeColour, QStringLiteral("Type color"),
              &SignalTextItem::typeColour, &SignalTextItem::setTypeColour);

    // The uV and dBm sub line.
    m_chkShowSubMarker = makeCheckRow(QStringLiteral("Show μV and dBm"));
    m_chkShowSubMarker->setToolTip(QStringLiteral(
        "Show the reading in μV and dBm under the value, and beside the peak."));
    connect(m_chkShowSubMarker, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setShowSubMarker(on);
        notifyChanged();
    });
    colourRow(m_btnSubMarkerColour, QStringLiteral("μV and dBm color"),
              &SignalTextItem::subMarkerColour, &SignalTextItem::setSubMarkerColour);
    colourRow(m_btnBackgroundColour, QStringLiteral("Background color"),
              &SignalTextItem::backgroundColour, &SignalTextItem::setBackgroundColour);

    m_chkPeakNeedsHold = makeCheckRow(QStringLiteral("Peak value only with peak hold"));
    m_chkPeakNeedsHold->setToolTip(QStringLiteral(
        "On: the peak value shows only while Peak hold is on, and reads the held "
        "peak. Off: Show peak value alone shows it, reading the highest value "
        "over the history time."));
    connect(m_chkPeakNeedsHold, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setPeakValueNeedsPeakHold(on);
        notifyChanged();
    });

    // History
    addHeader(QStringLiteral("History"));
    m_spinHistoryMs = makeIntRow(QStringLiteral("History (ms)"),
                                 kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
    m_spinHistoryMs->setToolTip(QStringLiteral("How far back the peak value looks."));
    connect(m_spinHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setHistoryDurationMs(v);
        notifyChanged();
    });
    m_spinIgnoreHistoryMs = makeIntRow(QStringLiteral("Ignore history (ms)"),
                                       kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
    m_spinIgnoreHistoryMs->setToolTip(QStringLiteral(
        "After switching between receive and transmit, or changing band, let "
        "the meter settle this long before it gathers history."));
    connect(m_spinIgnoreHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        SignalTextItem* x = qobject_cast<SignalTextItem*>(m_item);
        if (!x) { return; }
        x->setIgnoreHistoryMs(v);
        notifyChanged();
    });
}

} // namespace NereusSDR
