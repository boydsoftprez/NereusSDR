#pragma once

// =================================================================
// src/gui/containers/meter_property_editors/PresetItemEditor.h
// (NereusSDR)
// =================================================================
//
// Edit-container refactor critical fix — shared property-pane editor
// for the 11 first-class preset MeterItem subclasses introduced when
// the legacy ItemGroup factories were collapsed into concrete classes.
// Those presets serialize as JSON (e.g. `{"kind":"AnanMM",...}`) with
// no leading "TYPE|" pipe tag, so the pipe-delimited dispatcher in
// ContainerSettingsDialog::buildTypeSpecificEditor() cannot route
// them to a type-specific editor and the property pane would fall
// through to the empty page. PresetItemEditor handles the shared
// X/Y/W/H rect plus class-specific toggles (AnanMultiMeter anchor/
// debug/needle visibility, BarPreset kindString read-only, etc.)
// so the user sees populated properties instead of an empty page.
//
// no-port-check: NereusSDR-original editor — no Thetis source text
// transcribed. Style and structural pattern follow BaseItemEditor /
// SpacerItemEditor etc. (Phase 3G-6 block 4).
// =================================================================

#include "BaseItemEditor.h"

class QCheckBox;
class QLabel;

namespace NereusSDR {

class MeterItem;

class PresetItemEditor : public BaseItemEditor {
    Q_OBJECT

public:
    explicit PresetItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    // AnanMultiMeterItem-specific controls (only visible when the
    // bound item is an AnanMultiMeterItem; hidden for everything else).
    QCheckBox* m_chkAnchorBg{nullptr};
    QCheckBox* m_chkDebugOverlay{nullptr};
    QCheckBox* m_chkNeedleSignal{nullptr};
    QCheckBox* m_chkNeedleVolts{nullptr};
    QCheckBox* m_chkNeedleAmps{nullptr};
    QCheckBox* m_chkNeedlePower{nullptr};
    QCheckBox* m_chkNeedleSwr{nullptr};
    QCheckBox* m_chkNeedleCompression{nullptr};
    QCheckBox* m_chkNeedleAlc{nullptr};

    // BarPresetItem-specific — read-only kindString display so the
    // user can see which canned bar flavour (e.g. "AlcGain") is
    // bound to the row. Hidden for non-BarPresetItem presets.
    QLabel* m_lblBarKind{nullptr};

    // Track which optional groups are live so setItem() can hide
    // the controls for a class that doesn't apply. addRow() stores
    // widget+label pairs; we stash the labels so we can show/hide
    // them alongside the control.
    QList<QWidget*> m_ananRowWidgets;
    QList<QWidget*> m_barRowWidgets;

    void hideAnanRows();
    void hideBarRows();
    void showAnanRows();
    void showBarRows();
    void addAnanRow(const QString& label, QWidget* widget);
    void addBarRow(const QString& label, QWidget* widget);
};

} // namespace NereusSDR
