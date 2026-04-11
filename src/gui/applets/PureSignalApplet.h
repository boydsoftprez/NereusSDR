// src/gui/applets/PureSignalApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;

namespace NereusSDR {

class HGauge;

// PureSignal / PS-A feedback predistortion controls.
// NYI — Phase 3I-4 (PureSignal DDC + calcc/IQC engine).
//
// Controls:
//   1. Calibrate button      — QPushButton (non-toggle)
//   2. Auto-cal toggle       — QPushButton green "Auto"
//   3. Feedback level gauge  — HGauge (0-100, yellow@70, red@90, title "FB Level")
//   4. Correction mag gauge  — HGauge (0-100, yellow@80, red@95, title "Correction")
//   5. Save coefficients     — QPushButton "Save"
//   6. Restore coefficients  — QPushButton "Restore"
//   7. Two-tone test         — QPushButton green toggle "2-Tone"
//   8. Status LEDs           — 3x QLabel (8x8 circles: "Cal", "Run", "Fbk")
//
// Plus: info readout labels (Iterations, Feedback dB, Correction dB).
class PureSignalApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit PureSignalApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("pure_signal"); }
    QString appletTitle() const override { return QStringLiteral("PureSignal"); }
    void    syncFromModel() override;

private:
    void buildUI();

    // Control 1 — calibrate button (non-toggle)
    QPushButton* m_calibrateBtn  = nullptr;
    // Control 2 — auto-cal toggle (green)
    QPushButton* m_autoCalBtn    = nullptr;

    // Control 3 — feedback level gauge (0-100, yellow@70, red@90)
    HGauge* m_feedbackGauge      = nullptr;
    // Control 4 — correction magnitude gauge (0-100, yellow@80, red@95)
    HGauge* m_correctionGauge    = nullptr;

    // Control 5 — save coefficients
    QPushButton* m_saveBtn       = nullptr;
    // Control 6 — restore coefficients
    QPushButton* m_restoreBtn    = nullptr;

    // Control 7 — two-tone test (green toggle)
    QPushButton* m_twoToneBtn    = nullptr;

    // Control 8 — status LEDs: "Cal", "Run", "Fbk"
    QLabel* m_led[3]             = {};

    // Info readout labels
    QLabel* m_iterations         = nullptr;
    QLabel* m_feedbackDb         = nullptr;
    QLabel* m_correctionDb       = nullptr;
};

} // namespace NereusSDR
