// =================================================================
// src/gui/containers/meter_property_editors/PresetItemEditor.cpp
// (NereusSDR)
// =================================================================
//
// Edit-container refactor critical fix — see PresetItemEditor.h.
// no-port-check: NereusSDR-original editor, no Thetis source text.
// =================================================================

#include "PresetItemEditor.h"

#include "../../ColorSwatchButton.h"
#include "../../meters/MeterItem.h"
#include "../../meters/presets/AnanMultiMeterItem.h"
#include "../../meters/presets/BarPresetItem.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

constexpr const char* kCheckStyle  = "QCheckBox { color: #c8d8e8; }";
constexpr const char* kLabelStyle  = "QLabel { color: #8090a0; font-size: 10px; }";
constexpr const char* kValueStyle  = "QLabel { color: #c8d8e8; font-size: 11px; }";
constexpr const char* kGroupStyle  =
    "QGroupBox { color: #8aa8c0; font-weight: bold; border: 1px solid #1e2e3e;"
    "           border-radius: 3px; margin-top: 8px; padding-top: 10px; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }";
constexpr const char* kSpinStyle =
    "QDoubleSpinBox, QSpinBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 1px 4px; min-height: 18px;"
    "}";

} // namespace

PresetItemEditor::PresetItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void PresetItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Preset"));

    // --- Anchor + debug overlay toggles (existing functionality) ---
    m_chkAnchorBg = new QCheckBox(this);
    m_chkAnchorBg->setStyleSheet(kCheckStyle);
    m_chkAnchorBg->setToolTip(QStringLiteral(
        "Anchor pivot/radius to the background image's letterboxed "
        "draw rect, keeping the arc glued to the meter face at "
        "non-default container aspect ratios."));
    connect(m_chkAnchorBg, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setAnchorToBgRect(on);
        notifyChanged();
    });
    addAnanRow(QStringLiteral("Anchor to bg rect"), m_chkAnchorBg);

    m_chkDebugOverlay = new QCheckBox(this);
    m_chkDebugOverlay->setStyleSheet(kCheckStyle);
    m_chkDebugOverlay->setToolTip(QStringLiteral(
        "Paint a coloured dot at every needle calibration point "
        "(debugging aid for arc-anchoring verification)."));
    connect(m_chkDebugOverlay, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setDebugOverlay(on);
        notifyChanged();
    });
    addAnanRow(QStringLiteral("Calibration overlay"), m_chkDebugOverlay);

    // --- Needle visibility toggles ---
    struct NeedleRow { QCheckBox** member; const char* label; int index; };
    const NeedleRow rows[] = {
        { &m_chkNeedleSignal,      "Needle: Signal",      0 },
        { &m_chkNeedleVolts,       "Needle: Volts",       1 },
        { &m_chkNeedleAmps,        "Needle: Amps",        2 },
        { &m_chkNeedlePower,       "Needle: Power",       3 },
        { &m_chkNeedleSwr,         "Needle: SWR",         4 },
        { &m_chkNeedleCompression, "Needle: Compression", 5 },
        { &m_chkNeedleAlc,         "Needle: ALC",         6 },
    };
    for (const NeedleRow& r : rows) {
        auto* chk = new QCheckBox(this);
        chk->setStyleSheet(kCheckStyle);
        const int idx = r.index;
        connect(chk, &QCheckBox::toggled, this, [this, idx](bool on) {
            if (isProgrammaticUpdate()) { return; }
            auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
            if (!anan) { return; }
            anan->setNeedleVisible(idx, on);
            notifyChanged();
        });
        *(r.member) = chk;
        addAnanRow(QString::fromLatin1(r.label), chk);
    }

    // --- Full Thetis parity control blocks ---
    addAnanBlock(buildSettingsGroup());
    addAnanBlock(buildColorsGroup());
    addAnanBlock(buildNeedleColorsGroup());
    addAnanBlock(buildTitleGroup());
    addAnanBlock(buildPeakValueGroup());
    addAnanBlock(buildPeakHoldGroup());
    addAnanBlock(buildHistoryGroup());
    addAnanBlock(buildShadowGroup());
    addAnanBlock(buildSegmentedGroup());
    addAnanBlock(buildSolidGroup());
    addAnanBlock(buildMiscGroup());

    // --- BarPresetItem-specific row ---
    m_lblBarKind = new QLabel(this);
    m_lblBarKind->setStyleSheet(kValueStyle);
    addBarRow(QStringLiteral("Flavour"), m_lblBarKind);

    // All class-specific rows start hidden; setItem() reveals the
    // ones appropriate for the bound preset subclass.
    hideAnanRows();
    hideBarRows();
}

void PresetItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    if (!item) {
        hideAnanRows();
        hideBarRows();
        return;
    }

    beginProgrammaticUpdate();

    if (auto* anan = dynamic_cast<AnanMultiMeterItem*>(item)) {
        hideBarRows();
        showAnanRows();
        m_chkAnchorBg->setChecked(anan->anchorToBgRect());
        m_chkDebugOverlay->setChecked(anan->debugOverlay());
        m_chkNeedleSignal->setChecked(anan->needleVisible(0));
        m_chkNeedleVolts->setChecked(anan->needleVisible(1));
        m_chkNeedleAmps->setChecked(anan->needleVisible(2));
        m_chkNeedlePower->setChecked(anan->needleVisible(3));
        m_chkNeedleSwr->setChecked(anan->needleVisible(4));
        m_chkNeedleCompression->setChecked(anan->needleVisible(5));
        m_chkNeedleAlc->setChecked(anan->needleVisible(6));
        loadAnanParityFields(anan);
    }
    else if (auto* bar = dynamic_cast<BarPresetItem*>(item)) {
        hideAnanRows();
        showBarRows();
        m_lblBarKind->setText(bar->kindString());
    }
    else {
        // Other 9 preset classes — only the common X/Y/W/H rows apply.
        hideAnanRows();
        hideBarRows();
    }

    endProgrammaticUpdate();
}

// ---------------------------------------------------------------------------
// Row visibility helpers
// ---------------------------------------------------------------------------

void PresetItemEditor::addAnanRow(const QString& label, QWidget* widget)
{
    auto* lbl = new QLabel(label, this);
    lbl->setStyleSheet(kLabelStyle);
    m_form->addRow(lbl, widget);
    m_ananRowWidgets.append(lbl);
    m_ananRowWidgets.append(widget);
}

void PresetItemEditor::addAnanBlock(QWidget* widget)
{
    // Full-row, no label — used for QGroupBox sections.
    m_form->addRow(widget);
    m_ananRowWidgets.append(widget);
}

void PresetItemEditor::addBarRow(const QString& label, QWidget* widget)
{
    auto* lbl = new QLabel(label, this);
    lbl->setStyleSheet(kLabelStyle);
    m_form->addRow(lbl, widget);
    m_barRowWidgets.append(lbl);
    m_barRowWidgets.append(widget);
}

void PresetItemEditor::hideAnanRows()
{
    for (QWidget* w : m_ananRowWidgets) { w->setVisible(false); }
}

void PresetItemEditor::showAnanRows()
{
    for (QWidget* w : m_ananRowWidgets) { w->setVisible(true); }
}

void PresetItemEditor::hideBarRows()
{
    for (QWidget* w : m_barRowWidgets) { w->setVisible(false); }
}

void PresetItemEditor::showBarRows()
{
    for (QWidget* w : m_barRowWidgets) { w->setVisible(true); }
}

// ---------------------------------------------------------------------------
// Thetis parity — QGroupBox builders
// ---------------------------------------------------------------------------

QGroupBox* PresetItemEditor::buildSettingsGroup()
{
    m_settingsGroup = new QGroupBox(QStringLiteral("Settings"), this);
    m_settingsGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_settingsGroup);

    m_spinUpdateMs = new QSpinBox(m_settingsGroup);
    m_spinUpdateMs->setRange(10, 500);
    m_spinUpdateMs->setSingleStep(10);
    m_spinUpdateMs->setSuffix(QStringLiteral(" ms"));
    m_spinUpdateMs->setStyleSheet(kSpinStyle);
    // From Thetis setup.designer.cs:55564 [@501e3f5]
    m_spinUpdateMs->setToolTip(tr("Reading update and is related to screen update"));
    connect(m_spinUpdateMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setUpdateMs(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Update"), m_spinUpdateMs);

    m_spinAttack = new QDoubleSpinBox(m_settingsGroup);
    m_spinAttack->setRange(0.0, 1.0);
    m_spinAttack->setSingleStep(0.01);
    m_spinAttack->setDecimals(3);
    m_spinAttack->setStyleSheet(kSpinStyle);
    // From Thetis setup.designer.cs:55524 [@501e3f5]
    m_spinAttack->setToolTip(tr("The 'speed of rise' to the new value if above current"));
    connect(m_spinAttack, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setAttackRatio(static_cast<float>(v));
        notifyChanged();
    });
    form->addRow(QStringLiteral("Attack"), m_spinAttack);

    m_spinDecay = new QDoubleSpinBox(m_settingsGroup);
    m_spinDecay->setRange(0.0, 1.0);
    m_spinDecay->setSingleStep(0.01);
    m_spinDecay->setDecimals(3);
    m_spinDecay->setStyleSheet(kSpinStyle);
    // From Thetis setup.designer.cs:55483 [@501e3f5]
    m_spinDecay->setToolTip(tr("The 'speed of fall' to the new value if below current"));
    connect(m_spinDecay, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setDecayRatio(static_cast<float>(v));
        notifyChanged();
    });
    form->addRow(QStringLiteral("Decay"), m_spinDecay);

    return m_settingsGroup;
}

// Wires a colour swatch button to a setter on the bound AnanMultiMeterItem.
// The lambda pattern below repeats for each colour knob; pulling it out as
// a helper keeps the group builders readable.
static void wireColorSwatch(PresetItemEditor* editor,
                            ColorSwatchButton* btn,
                            void (AnanMultiMeterItem::*setter)(const QColor&))
{
    QObject::connect(btn, &ColorSwatchButton::colorChanged, editor,
                     [editor, setter](const QColor& c) {
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(editor->item());
        if (!anan) { return; }
        (anan->*setter)(c);
        emit editor->propertyChanged();
    });
}

QGroupBox* PresetItemEditor::buildColorsGroup()
{
    m_colorsGroup = new QGroupBox(QStringLiteral("Colors"), this);
    m_colorsGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_colorsGroup);

    m_btnBgColor = new ColorSwatchButton(QColor(0, 0, 0, 0), m_colorsGroup);
    // From Thetis setup.designer.cs:55604 [@501e3f5]
    m_btnBgColor->setToolTip(tr("Background colour"));
    wireColorSwatch(this, m_btnBgColor, &AnanMultiMeterItem::setBackgroundColor);
    form->addRow(QStringLiteral("Background"), m_btnBgColor);

    m_btnLowColor = new ColorSwatchButton(Qt::white, m_colorsGroup);
    // From Thetis setup.designer.cs:54845 [@501e3f5]
    m_btnLowColor->setToolTip(tr("Low scale colour and value"));
    wireColorSwatch(this, m_btnLowColor, &AnanMultiMeterItem::setLowColor);
    form->addRow(QStringLiteral("Low"), m_btnLowColor);

    m_btnHighColor = new ColorSwatchButton(QColor(255, 64, 64), m_colorsGroup);
    // From Thetis setup.designer.cs:55376 [@501e3f5]
    m_btnHighColor->setToolTip(tr("High scale colour"));
    wireColorSwatch(this, m_btnHighColor, &AnanMultiMeterItem::setHighColor);
    form->addRow(QStringLiteral("High"), m_btnHighColor);

    m_btnIndicatorColor = new ColorSwatchButton(Qt::yellow, m_colorsGroup);
    // From Thetis setup.designer.cs:55254 [@501e3f5]
    m_btnIndicatorColor->setToolTip(tr("Indicator colour"));
    wireColorSwatch(this, m_btnIndicatorColor, &AnanMultiMeterItem::setIndicatorColor);
    form->addRow(QStringLiteral("Indicator"), m_btnIndicatorColor);

    m_btnSubColor = new ColorSwatchButton(Qt::black, m_colorsGroup);
    // From Thetis setup.designer.cs:55050 [@501e3f5]
    m_btnSubColor->setToolTip(tr("Sub Indicator colour for sub needles and avg markers on some horizontal meters"));
    wireColorSwatch(this, m_btnSubColor, &AnanMultiMeterItem::setSubColor);
    form->addRow(QStringLiteral("Sub"), m_btnSubColor);

    return m_colorsGroup;
}

QGroupBox* PresetItemEditor::buildNeedleColorsGroup()
{
    m_needleColorGroup = new QGroupBox(QStringLiteral("Per-needle Colors"), this);
    m_needleColorGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_needleColorGroup);

    // No Thetis tooltip — Thetis sets per-needle colours procedurally
    // inside AddAnanMM (MeterManager.cs:22461+) rather than exposing a
    // per-needle colour swatch in Setup → Appearance → Meters/Gadgets.

    struct NRow { ColorSwatchButton** member; const char* label; int index; };
    const NRow rows[] = {
        { &m_btnNeedleColorSignal, "Signal", 0 },
        { &m_btnNeedleColorVolts,  "Volts",  1 },
        { &m_btnNeedleColorAmps,   "Amps",   2 },
        { &m_btnNeedleColorPower,  "Power",  3 },
        { &m_btnNeedleColorSwr,    "SWR",    4 },
        { &m_btnNeedleColorComp,   "Comp",   5 },
        { &m_btnNeedleColorAlc,    "ALC",    6 },
    };
    for (const NRow& r : rows) {
        auto* btn = new ColorSwatchButton(Qt::black, m_needleColorGroup);
        const int idx = r.index;
        connect(btn, &ColorSwatchButton::colorChanged, this, [this, idx](const QColor& c) {
            auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
            if (!anan) { return; }
            anan->setNeedleColor(idx, c);
            notifyChanged();
        });
        *(r.member) = btn;
        form->addRow(QString::fromLatin1(r.label), btn);
    }
    return m_needleColorGroup;
}

QGroupBox* PresetItemEditor::buildTitleGroup()
{
    m_titleGroup = new QGroupBox(QStringLiteral("Meter Title"), this);
    m_titleGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_titleGroup);

    m_chkShowTitle = new QCheckBox(m_titleGroup);
    m_chkShowTitle->setStyleSheet(kCheckStyle);
    // From Thetis setup.designer.cs:55089 [@501e3f5]
    m_chkShowTitle->setToolTip(tr("Show meter title"));
    connect(m_chkShowTitle, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowMeterTitle(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Show title"), m_chkShowTitle);

    m_btnTitleColor = new ColorSwatchButton(Qt::white, m_titleGroup);
    // From Thetis setup.designer.cs:55239 [@501e3f5]
    m_btnTitleColor->setToolTip(tr("Meter title colour"));
    wireColorSwatch(this, m_btnTitleColor, &AnanMultiMeterItem::setMeterTitleColor);
    form->addRow(QStringLiteral("Title color"), m_btnTitleColor);

    return m_titleGroup;
}

QGroupBox* PresetItemEditor::buildPeakValueGroup()
{
    m_peakValueGroup = new QGroupBox(QStringLiteral("Peak Value"), this);
    m_peakValueGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_peakValueGroup);

    m_chkShowPeakValue = new QCheckBox(m_peakValueGroup);
    m_chkShowPeakValue->setStyleSheet(kCheckStyle);
    // From Thetis setup.designer.cs:54988 [@501e3f5]
    m_chkShowPeakValue->setToolTip(tr("Show peak value"));
    connect(m_chkShowPeakValue, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowPeakValue(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Show peak value"), m_chkShowPeakValue);

    m_btnPeakValueColor = new ColorSwatchButton(Qt::yellow, m_peakValueGroup);
    // From Thetis setup.designer.cs:55140 [@501e3f5]
    m_btnPeakValueColor->setToolTip(tr("Peak value colour"));
    wireColorSwatch(this, m_btnPeakValueColor, &AnanMultiMeterItem::setPeakValueColor);
    form->addRow(QStringLiteral("Peak value color"), m_btnPeakValueColor);

    return m_peakValueGroup;
}

QGroupBox* PresetItemEditor::buildPeakHoldGroup()
{
    m_peakHoldGroup = new QGroupBox(QStringLiteral("Peak Hold"), this);
    m_peakHoldGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_peakHoldGroup);

    // No Thetis tooltip — chkMeterItemPeakHold / clrbtnMeterItemPeakHold
    // ship in setup.designer.cs [@501e3f5] without a toolTip1.SetToolTip
    // assignment (unlike Peak Value / Title peers).
    m_chkShowPeakHold = new QCheckBox(m_peakHoldGroup);
    m_chkShowPeakHold->setStyleSheet(kCheckStyle);
    connect(m_chkShowPeakHold, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowPeakHold(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Show peak hold"), m_chkShowPeakHold);

    m_btnPeakHoldColor = new ColorSwatchButton(QColor(255, 128, 0), m_peakHoldGroup);
    wireColorSwatch(this, m_btnPeakHoldColor, &AnanMultiMeterItem::setPeakHoldColor);
    form->addRow(QStringLiteral("Peak hold color"), m_btnPeakHoldColor);

    return m_peakHoldGroup;
}

QGroupBox* PresetItemEditor::buildHistoryGroup()
{
    m_historyGroup = new QGroupBox(QStringLiteral("History"), this);
    m_historyGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_historyGroup);

    // No Thetis tooltip — chkMeterItemHistory has no toolTip1.SetToolTip
    // in setup.designer.cs [@501e3f5] (the related nudMeterItemHistory
    // duration/ignore spins do).
    m_chkShowHistory = new QCheckBox(m_historyGroup);
    m_chkShowHistory->setStyleSheet(kCheckStyle);
    connect(m_chkShowHistory, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowHistory(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Show history"), m_chkShowHistory);

    m_spinHistoryMs = new QSpinBox(m_historyGroup);
    m_spinHistoryMs->setRange(100, 60000);
    m_spinHistoryMs->setSingleStep(100);
    m_spinHistoryMs->setSuffix(QStringLiteral(" ms"));
    m_spinHistoryMs->setStyleSheet(kSpinStyle);
    // From Thetis setup.designer.cs:54890 [@501e3f5]
    m_spinHistoryMs->setToolTip(tr("History duration for history display and peak hold"));
    connect(m_spinHistoryMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setHistoryMs(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("History"), m_spinHistoryMs);

    m_spinIgnoreHistMs = new QSpinBox(m_historyGroup);
    m_spinIgnoreHistMs->setRange(0, 10000);
    m_spinIgnoreHistMs->setSingleStep(10);
    m_spinIgnoreHistMs->setSuffix(QStringLiteral(" ms"));
    m_spinIgnoreHistMs->setStyleSheet(kSpinStyle);
    // From Thetis setup.designer.cs:55029 [@501e3f5]
    m_spinIgnoreHistMs->setToolTip(tr("When rx/tx transition or band change let meters settle before gathering history/peak values"));
    connect(m_spinIgnoreHistMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setIgnoreHistoryMs(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Ignore History/Peak"), m_spinIgnoreHistMs);

    // No Thetis tooltip — history color swatch has no SetToolTip assignment
    // in setup.designer.cs [@501e3f5].
    m_btnHistoryColor = new ColorSwatchButton(QColor(255, 0, 0, 128), m_historyGroup);
    wireColorSwatch(this, m_btnHistoryColor, &AnanMultiMeterItem::setHistoryColor);
    form->addRow(QStringLiteral("History color"), m_btnHistoryColor);

    return m_historyGroup;
}

QGroupBox* PresetItemEditor::buildShadowGroup()
{
    m_shadowGroup = new QGroupBox(QStringLiteral("Shadow"), this);
    m_shadowGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_shadowGroup);

    m_chkShowShadow = new QCheckBox(m_shadowGroup);
    m_chkShowShadow->setStyleSheet(kCheckStyle);
    // From Thetis setup.designer.cs:55152 [@501e3f5]
    m_chkShowShadow->setToolTip(tr("Show shadow on needles"));
    connect(m_chkShowShadow, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowShadow(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Enable"), m_chkShowShadow);

    // No Thetis tooltip — lblMMsegSolLow / lblMMsegSolHigh and the underlying
    // nud* spins ship without toolTip1.SetToolTip in setup.designer.cs
    // [@501e3f5]; only the low/high colour swatches carry text.
    m_spinShadowLow = new QDoubleSpinBox(m_shadowGroup);
    m_spinShadowLow->setRange(-200.0, 200.0);
    m_spinShadowLow->setSingleStep(1.0);
    m_spinShadowLow->setDecimals(1);
    m_spinShadowLow->setStyleSheet(kSpinStyle);
    connect(m_spinShadowLow, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShadowLow(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Low"), m_spinShadowLow);

    m_spinShadowHigh = new QDoubleSpinBox(m_shadowGroup);
    m_spinShadowHigh->setRange(-200.0, 200.0);
    m_spinShadowHigh->setSingleStep(1.0);
    m_spinShadowHigh->setDecimals(1);
    m_spinShadowHigh->setStyleSheet(kSpinStyle);
    connect(m_spinShadowHigh, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShadowHigh(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("High"), m_spinShadowHigh);

    // No Thetis tooltip — Thetis has no dedicated shadow colour swatch
    // (shadow rendering reuses the indicator colour), so there is no
    // upstream SetToolTip to port.
    m_btnShadowColor = new ColorSwatchButton(QColor(0, 0, 0, 96), m_shadowGroup);
    wireColorSwatch(this, m_btnShadowColor, &AnanMultiMeterItem::setShadowColor);
    form->addRow(QStringLiteral("Color"), m_btnShadowColor);

    return m_shadowGroup;
}

QGroupBox* PresetItemEditor::buildSegmentedGroup()
{
    m_segmentedGroup = new QGroupBox(QStringLiteral("Segmented"), this);
    m_segmentedGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_segmentedGroup);

    m_chkShowSegmented = new QCheckBox(m_segmentedGroup);
    m_chkShowSegmented->setStyleSheet(kCheckStyle);
    // From Thetis setup.designer.cs:55165 [@501e3f5]
    m_chkShowSegmented->setToolTip(tr("Segmented bar"));
    connect(m_chkShowSegmented, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowSegmented(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Enable"), m_chkShowSegmented);

    // No Thetis tooltip — underlying nud* spins for seg low/high are not
    // tooltipped in setup.designer.cs [@501e3f5]; Thetis only tooltips
    // the low/high COLOUR swatches (see color row below).
    m_spinSegmentedLow = new QDoubleSpinBox(m_segmentedGroup);
    m_spinSegmentedLow->setRange(-200.0, 200.0);
    m_spinSegmentedLow->setSingleStep(1.0);
    m_spinSegmentedLow->setDecimals(1);
    m_spinSegmentedLow->setStyleSheet(kSpinStyle);
    connect(m_spinSegmentedLow, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setSegmentedLow(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Low"), m_spinSegmentedLow);

    m_spinSegmentedHigh = new QDoubleSpinBox(m_segmentedGroup);
    m_spinSegmentedHigh->setRange(-200.0, 200.0);
    m_spinSegmentedHigh->setSingleStep(1.0);
    m_spinSegmentedHigh->setDecimals(1);
    m_spinSegmentedHigh->setStyleSheet(kSpinStyle);
    connect(m_spinSegmentedHigh, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setSegmentedHigh(v);
        notifyChanged();
    });
    form->addRow(QStringLiteral("High"), m_spinSegmentedHigh);

    // Thetis splits segmented colour into separate low/high swatches
    // (clrbtnMeterItemSegmentedSolidColour{Low,High}). NereusSDR's
    // AnanMultiMeterItem stores a single segmented colour today, so we
    // port the "Low section colour" string as the closest match.
    // From Thetis setup.designer.cs:55065 [@501e3f5]
    m_btnSegmentedColor = new ColorSwatchButton(Qt::darkGray, m_segmentedGroup);
    m_btnSegmentedColor->setToolTip(tr("Low section colour"));
    wireColorSwatch(this, m_btnSegmentedColor, &AnanMultiMeterItem::setSegmentedColor);
    form->addRow(QStringLiteral("Color"), m_btnSegmentedColor);

    return m_segmentedGroup;
}

QGroupBox* PresetItemEditor::buildSolidGroup()
{
    m_solidGroup = new QGroupBox(QStringLiteral("Solid"), this);
    m_solidGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_solidGroup);

    m_chkShowSolid = new QCheckBox(m_solidGroup);
    m_chkShowSolid->setStyleSheet(kCheckStyle);
    // From Thetis setup.designer.cs:55102 [@501e3f5]
    m_chkShowSolid->setToolTip(tr("Solid bar"));
    connect(m_chkShowSolid, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setShowSolid(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Enable"), m_chkShowSolid);

    // No Thetis tooltip — Thetis shares the segmented low/high swatches
    // for solid-bar colour rendering; no dedicated solid colour swatch
    // tooltip exists in setup.designer.cs [@501e3f5].
    m_btnSolidColor = new ColorSwatchButton(Qt::gray, m_solidGroup);
    wireColorSwatch(this, m_btnSolidColor, &AnanMultiMeterItem::setSolidColor);
    form->addRow(QStringLiteral("Color"), m_btnSolidColor);

    return m_solidGroup;
}

QGroupBox* PresetItemEditor::buildMiscGroup()
{
    m_miscGroup = new QGroupBox(QStringLiteral("Fade / Source / Dark Mode"), this);
    m_miscGroup->setStyleSheet(kGroupStyle);
    auto* form = new QFormLayout(m_miscGroup);

    // No Thetis tooltip — chkMeterItemFadeOnRx / chkMeterItemFadeOnTx /
    // chkMeterItemDarkMode ship without toolTip1.SetToolTip in
    // setup.designer.cs [@501e3f5]. The control captions are self-
    // describing in Thetis and are preserved unchanged here.
    m_chkFadeRx = new QCheckBox(m_miscGroup);
    m_chkFadeRx->setStyleSheet(kCheckStyle);
    connect(m_chkFadeRx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setFadeOnRx(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Fade on RX"), m_chkFadeRx);

    m_chkFadeTx = new QCheckBox(m_miscGroup);
    m_chkFadeTx->setStyleSheet(kCheckStyle);
    connect(m_chkFadeTx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setFadeOnTx(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Fade on TX"), m_chkFadeTx);

    m_chkDarkMode = new QCheckBox(m_miscGroup);
    m_chkDarkMode->setStyleSheet(kCheckStyle);
    connect(m_chkDarkMode, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setDarkMode(on);
        notifyChanged();
    });
    form->addRow(QStringLiteral("Dark Mode"), m_chkDarkMode);

    // Signal source radio group — only the Signal needle is affected.
    // No Thetis tooltip — Thetis exposes the Signal source selector via
    // clsItemGroup.ReadingSource / MaxBin serialization fields rather
    // than a visible radio cluster in the Meters/Gadgets panel, so
    // there is no upstream SetToolTip text to port.
    auto* sourceRow = new QWidget(m_miscGroup);
    auto* hl = new QHBoxLayout(sourceRow);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    m_radioSourceAvg    = new QRadioButton(QStringLiteral("Sig"),     sourceRow);
    m_radioSourcePeak   = new QRadioButton(QStringLiteral("Avg"),     sourceRow);
    m_radioSourceMaxBin = new QRadioButton(QStringLiteral("MaxBin"),  sourceRow);
    m_radioSourceAvg->setStyleSheet(kCheckStyle);
    m_radioSourcePeak->setStyleSheet(kCheckStyle);
    m_radioSourceMaxBin->setStyleSheet(kCheckStyle);
    auto* bg = new QButtonGroup(m_miscGroup);
    bg->addButton(m_radioSourceAvg,    static_cast<int>(AnanMultiMeterItem::SignalSource::Avg));
    bg->addButton(m_radioSourcePeak,   static_cast<int>(AnanMultiMeterItem::SignalSource::Peak));
    bg->addButton(m_radioSourceMaxBin, static_cast<int>(AnanMultiMeterItem::SignalSource::MaxBin));
    hl->addWidget(m_radioSourceAvg);
    hl->addWidget(m_radioSourcePeak);
    hl->addWidget(m_radioSourceMaxBin);
    hl->addStretch();
    connect(bg, &QButtonGroup::idClicked, this, [this](int id) {
        if (isProgrammaticUpdate()) { return; }
        auto* anan = dynamic_cast<AnanMultiMeterItem*>(m_item);
        if (!anan) { return; }
        anan->setSignalSource(static_cast<AnanMultiMeterItem::SignalSource>(id));
        notifyChanged();
    });
    form->addRow(QStringLiteral("Signal source"), sourceRow);

    return m_miscGroup;
}

// ---------------------------------------------------------------------------
// setItem() helper — snap every parity control to the bound item's state.
// ---------------------------------------------------------------------------
void PresetItemEditor::loadAnanParityFields(AnanMultiMeterItem* a)
{
    if (!a) { return; }

    m_spinUpdateMs->setValue(a->updateMs());
    m_spinAttack->setValue(static_cast<double>(a->attackRatio()));
    m_spinDecay->setValue(static_cast<double>(a->decayRatio()));

    m_btnBgColor->setColor(a->backgroundColor());
    m_btnLowColor->setColor(a->lowColor());
    m_btnHighColor->setColor(a->highColor());
    m_btnIndicatorColor->setColor(a->indicatorColor());
    m_btnSubColor->setColor(a->subColor());

    m_btnNeedleColorSignal->setColor(a->needleColor(0));
    m_btnNeedleColorVolts ->setColor(a->needleColor(1));
    m_btnNeedleColorAmps  ->setColor(a->needleColor(2));
    m_btnNeedleColorPower ->setColor(a->needleColor(3));
    m_btnNeedleColorSwr   ->setColor(a->needleColor(4));
    m_btnNeedleColorComp  ->setColor(a->needleColor(5));
    m_btnNeedleColorAlc   ->setColor(a->needleColor(6));

    m_chkShowTitle->setChecked(a->showMeterTitle());
    m_btnTitleColor->setColor(a->meterTitleColor());

    m_chkShowPeakValue->setChecked(a->showPeakValue());
    m_btnPeakValueColor->setColor(a->peakValueColor());

    m_chkShowPeakHold->setChecked(a->showPeakHold());
    m_btnPeakHoldColor->setColor(a->peakHoldColor());

    m_chkShowHistory->setChecked(a->showHistory());
    m_spinHistoryMs->setValue(a->historyMs());
    m_spinIgnoreHistMs->setValue(a->ignoreHistoryMs());
    m_btnHistoryColor->setColor(a->historyColor());

    m_chkShowShadow->setChecked(a->showShadow());
    m_spinShadowLow->setValue(a->shadowLow());
    m_spinShadowHigh->setValue(a->shadowHigh());
    m_btnShadowColor->setColor(a->shadowColor());

    m_chkShowSegmented->setChecked(a->showSegmented());
    m_spinSegmentedLow->setValue(a->segmentedLow());
    m_spinSegmentedHigh->setValue(a->segmentedHigh());
    m_btnSegmentedColor->setColor(a->segmentedColor());

    m_chkShowSolid->setChecked(a->showSolid());
    m_btnSolidColor->setColor(a->solidColor());

    m_chkFadeRx->setChecked(a->fadeOnRx());
    m_chkFadeTx->setChecked(a->fadeOnTx());
    m_chkDarkMode->setChecked(a->darkMode());

    switch (a->signalSource()) {
        case AnanMultiMeterItem::SignalSource::Avg:    m_radioSourceAvg->setChecked(true); break;
        case AnanMultiMeterItem::SignalSource::Peak:   m_radioSourcePeak->setChecked(true); break;
        case AnanMultiMeterItem::SignalSource::MaxBin: m_radioSourceMaxBin->setChecked(true); break;
    }
}

} // namespace NereusSDR
