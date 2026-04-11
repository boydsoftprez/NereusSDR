#pragma once
#include "AppletWidget.h"

class QPushButton;
class QSlider;
class QComboBox;
class QLabel;
class QWidget;

namespace NereusSDR {

// EqApplet — 8-band equalizer panel (NYI until Phase 3I-3).
//
// Layout mirrors AetherSDR EqApplet.cpp exactly:
//   Control row: ON (green toggle) | RX (blue toggle) | TX (blue toggle, starts checked)
//                + stretch + Reset (painted 3/4-circle icon, 22×22)
//   Band label row: 63 / 125 / 250 / 500 / 1k / 2k / 4k / 8k Hz labels
//   Slider area: left dB scale | 8 vertical sliders | right dB scale
//   Value labels: current dB value under each slider
//   Preset row: preset combo (Flat/Voice/Music/Custom) + 10-Band mode toggle
//
// All controls are NYI/disabled — Phase 3I-3 will wire them.

class EqApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit EqApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("EQ"); }
    QString appletTitle() const override { return QStringLiteral("Equalizer"); }
    void syncFromModel() override;

private:
    void buildUI();

    // Control row (controls 1–4)
    QPushButton* m_onBtn    = nullptr;   // control 1: ON — green checkable 36×22
    QPushButton* m_rxBtn    = nullptr;   // control 2: RX — blue checkable 36×22
    QPushButton* m_txBtn    = nullptr;   // control 3: TX — blue checkable 36×22, starts checked
    QPushButton* m_resetBtn = nullptr;   // control 4: Reset — 22×22, painted 3/4-circle icon

    // Band sliders and value labels (controls 5–12); 8-band: 63/125/250/500/1k/2k/4k/8k Hz
    QSlider* m_sliders[8]      = {};  // vertical, range -10..+10, fixedHeight 100
    QLabel*  m_valueLabels[8]  = {};  // current dB text under each slider

    // Bottom row (controls 13–15)
    QComboBox*   m_presetCombo  = nullptr;  // control 13: Flat/Voice/Music/Custom
    QWidget*     m_freqCurve    = nullptr;  // control 14: frequency response placeholder
    QPushButton* m_tenBandBtn   = nullptr;  // control 15: "10-Band" checkable toggle
};

} // namespace NereusSDR
