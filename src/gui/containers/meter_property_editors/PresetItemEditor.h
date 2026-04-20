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
// As of the ANAN-MM full Thetis property-editor parity series, the
// AnanMultiMeterItem path here hosts ~20 additional Thetis ANAN-MM
// knobs grouped by QGroupBox sections (Settings / Colors / Per-
// needle Colors / Title / Peak Value / Peak Hold / History / Shadow
// / Segmented / Solid / Fade / Source / Dark Mode). Every knob
// toggles/writes a single AnanMultiMeterItem setter; storage +
// serialization live on the item itself.
//
// no-port-check: NereusSDR-original editor — no Thetis source text
// transcribed. Style and structural pattern follow BaseItemEditor /
// SpacerItemEditor etc. (Phase 3G-6 block 4).
// =================================================================

#include "BaseItemEditor.h"

class QCheckBox;
class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QRadioButton;
class QGroupBox;

namespace NereusSDR {

class MeterItem;
class ColorSwatchButton;

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

    // --- ANAN-MM Thetis property-editor parity controls ---
    //
    // Each group is backed by a single QGroupBox so the user can scan
    // the (large) ANAN-MM editor section by section. The QGroupBox
    // lives in m_ananRowWidgets so setItem() hides the whole block
    // for non-AnanMM presets.
    QGroupBox*      m_settingsGroup{nullptr};
    QSpinBox*       m_spinUpdateMs{nullptr};
    QDoubleSpinBox* m_spinAttack{nullptr};
    QDoubleSpinBox* m_spinDecay{nullptr};

    QGroupBox*         m_colorsGroup{nullptr};
    ColorSwatchButton* m_btnBgColor{nullptr};
    ColorSwatchButton* m_btnLowColor{nullptr};
    ColorSwatchButton* m_btnHighColor{nullptr};
    ColorSwatchButton* m_btnIndicatorColor{nullptr};
    ColorSwatchButton* m_btnSubColor{nullptr};

    QGroupBox*         m_needleColorGroup{nullptr};
    ColorSwatchButton* m_btnNeedleColorSignal{nullptr};
    ColorSwatchButton* m_btnNeedleColorVolts{nullptr};
    ColorSwatchButton* m_btnNeedleColorAmps{nullptr};
    ColorSwatchButton* m_btnNeedleColorPower{nullptr};
    ColorSwatchButton* m_btnNeedleColorSwr{nullptr};
    ColorSwatchButton* m_btnNeedleColorComp{nullptr};
    ColorSwatchButton* m_btnNeedleColorAlc{nullptr};

    QGroupBox*         m_titleGroup{nullptr};
    QCheckBox*         m_chkShowTitle{nullptr};
    ColorSwatchButton* m_btnTitleColor{nullptr};

    QGroupBox*         m_peakValueGroup{nullptr};
    QCheckBox*         m_chkShowPeakValue{nullptr};
    ColorSwatchButton* m_btnPeakValueColor{nullptr};

    QGroupBox*         m_peakHoldGroup{nullptr};
    QCheckBox*         m_chkShowPeakHold{nullptr};
    ColorSwatchButton* m_btnPeakHoldColor{nullptr};

    QGroupBox*         m_historyGroup{nullptr};
    QCheckBox*         m_chkShowHistory{nullptr};
    QSpinBox*          m_spinHistoryMs{nullptr};
    QSpinBox*          m_spinIgnoreHistMs{nullptr};
    ColorSwatchButton* m_btnHistoryColor{nullptr};

    QGroupBox*         m_shadowGroup{nullptr};
    QCheckBox*         m_chkShowShadow{nullptr};
    QDoubleSpinBox*    m_spinShadowLow{nullptr};
    QDoubleSpinBox*    m_spinShadowHigh{nullptr};
    ColorSwatchButton* m_btnShadowColor{nullptr};

    QGroupBox*         m_segmentedGroup{nullptr};
    QCheckBox*         m_chkShowSegmented{nullptr};
    QDoubleSpinBox*    m_spinSegmentedLow{nullptr};
    QDoubleSpinBox*    m_spinSegmentedHigh{nullptr};
    ColorSwatchButton* m_btnSegmentedColor{nullptr};

    QGroupBox*         m_solidGroup{nullptr};
    QCheckBox*         m_chkShowSolid{nullptr};
    ColorSwatchButton* m_btnSolidColor{nullptr};

    QGroupBox*    m_miscGroup{nullptr};
    QCheckBox*    m_chkFadeRx{nullptr};
    QCheckBox*    m_chkFadeTx{nullptr};
    QCheckBox*    m_chkDarkMode{nullptr};
    QRadioButton* m_radioSourceAvg{nullptr};
    QRadioButton* m_radioSourcePeak{nullptr};
    QRadioButton* m_radioSourceMaxBin{nullptr};

    void hideAnanRows();
    void hideBarRows();
    void showAnanRows();
    void showBarRows();
    void addAnanRow(const QString& label, QWidget* widget);
    void addAnanBlock(QWidget* widget);
    void addBarRow(const QString& label, QWidget* widget);

    // Build helpers — each builds and returns a QGroupBox holding
    // one Thetis-parity section. Keeps buildTypeSpecific() readable.
    QGroupBox* buildSettingsGroup();
    QGroupBox* buildColorsGroup();
    QGroupBox* buildNeedleColorsGroup();
    QGroupBox* buildTitleGroup();
    QGroupBox* buildPeakValueGroup();
    QGroupBox* buildPeakHoldGroup();
    QGroupBox* buildHistoryGroup();
    QGroupBox* buildShadowGroup();
    QGroupBox* buildSegmentedGroup();
    QGroupBox* buildSolidGroup();
    QGroupBox* buildMiscGroup();

    // Sync entire parity panel from an AnanMultiMeterItem.
    void loadAnanParityFields(class AnanMultiMeterItem* item);
};

} // namespace NereusSDR
