// src/gui/applets/PureSignalApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;

namespace NereusSDR {

class HGauge;

// PureSignal / PS-A feedback predistortion controls.
// NYI — Phase 3I-4 (PureSignal DDC + calcc/IQC engine).
class PureSignalApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit PureSignalApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("pure_signal"); }
    QString appletTitle() const override { return QStringLiteral("PureSignal"); }
    void    syncFromModel() override;

private:
    // Row 1 — calibration controls
    QPushButton* m_calibrateBtn  = nullptr;
    QPushButton* m_autoCalBtn    = nullptr;  // green toggle

    // Row 2 — gauges
    HGauge* m_feedbackGauge      = nullptr;  // feedback level 0–100, red@80
    HGauge* m_correctionGauge    = nullptr;  // correction magnitude 0–100

    // Row 3 — coefficient management
    QPushButton* m_saveBtn       = nullptr;
    QPushButton* m_restoreBtn    = nullptr;

    // Row 4 — two-tone test
    QPushButton* m_twoToneBtn    = nullptr;

    // Row 5 — status LEDs (x3)
    QLabel* m_led[3]             = {};

    // Row 6 — info readout (x3)
    QLabel* m_iterations         = nullptr;
    QLabel* m_feedbackDb         = nullptr;
    QLabel* m_correctionDb       = nullptr;
};

} // namespace NereusSDR
