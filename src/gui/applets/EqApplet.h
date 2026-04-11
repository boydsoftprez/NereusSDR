#pragma once
#include "AppletWidget.h"

class QPushButton;
class QSlider;
class QComboBox;
class QLabel;

namespace NereusSDR {

// EqApplet — 10-band graphic equalizer panel (NYI until Phase 3I-3).
//
// Layout mirrors AetherSDR EqApplet.cpp exactly:
//   Control row:  ON (green toggle) | Reset (painted 3/4-circle icon, 22×22)
//                 | RX (blue toggle) | TX (blue toggle)
//   Band label row: 32/63/125/250/500/1k/2k/4k/8k/16k Hz labels (10 bands)
//   Slider area:  left dB scale | 10 vertical sliders | right dB scale
//   Value labels: current dB value under each slider
//   Preset row:   preset combo (Flat/Voice/Music/Custom)
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
    QPushButton* m_txBtn    = nullptr;   // control 3: TX — blue checkable 36×22
    QPushButton* m_resetBtn = nullptr;   // control 4: Reset — 22×22, painted 3/4-circle icon

    // Band sliders and value labels (controls 7–16): 10-band 32/63/125/250/500/1k/2k/4k/8k/16k Hz
    QSlider* m_sliders[10]     = {};  // vertical, range -10..+10, fixedHeight 100
    QLabel*  m_valueLabels[10] = {};  // current dB text under each slider

    // Preset combo (control 17)
    QComboBox* m_presetCombo = nullptr;  // Flat/Voice/Music/Custom
};

} // namespace NereusSDR
