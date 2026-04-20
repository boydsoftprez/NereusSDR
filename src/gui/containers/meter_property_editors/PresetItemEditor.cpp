// =================================================================
// src/gui/containers/meter_property_editors/PresetItemEditor.cpp
// (NereusSDR)
// =================================================================
//
// Edit-container refactor critical fix — see PresetItemEditor.h.
// no-port-check: NereusSDR-original editor, no Thetis source text.
// =================================================================

#include "PresetItemEditor.h"

#include "../../meters/MeterItem.h"
#include "../../meters/presets/AnanMultiMeterItem.h"
#include "../../meters/presets/BarPresetItem.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>

namespace NereusSDR {

namespace {

constexpr const char* kCheckStyle  = "QCheckBox { color: #c8d8e8; }";
constexpr const char* kLabelStyle  = "QLabel { color: #8090a0; font-size: 10px; }";
constexpr const char* kValueStyle  = "QLabel { color: #c8d8e8; font-size: 11px; }";

} // namespace

PresetItemEditor::PresetItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void PresetItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Preset"));

    // --- AnanMultiMeterItem-specific rows ---
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

} // namespace NereusSDR
