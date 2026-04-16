// src/gui/setup/GeneralOptionsPage.h
//
// Setup → General → Options page.
// Step attenuator enable/value per RX, auto-attenuate controls
// (enable, Classic/Adaptive mode, undo/decay, hold seconds).
//
// Porting from Thetis setup.cs: grpHermesStepAttenuator, groupBoxTS47.

#pragma once

#include "gui/SetupPage.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLabel;

namespace NereusSDR {

class StepAttenuatorController;

class GeneralOptionsPage : public SetupPage {
    Q_OBJECT
public:
    explicit GeneralOptionsPage(RadioModel* model, QWidget* parent = nullptr);

    void syncFromModel() override;

private:
    void buildStepAttGroup();
    void buildAutoAttGroup();
    void connectController();

    StepAttenuatorController* m_ctrl{nullptr};

    // Step Attenuator group
    QCheckBox* m_chkRx1StepAttEnable{nullptr};
    QSpinBox*  m_spnRx1StepAttValue{nullptr};
    QCheckBox* m_chkRx2StepAttEnable{nullptr};
    QSpinBox*  m_spnRx2StepAttValue{nullptr};
    QLabel*    m_lblAdcLinked{nullptr};

    // Auto Attenuate RX1
    QCheckBox* m_chkAutoAttRx1{nullptr};
    QComboBox* m_cmbAutoAttRx1Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx1{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx1{nullptr};

    // Auto Attenuate RX2
    QCheckBox* m_chkAutoAttRx2{nullptr};
    QComboBox* m_cmbAutoAttRx2Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx2{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx2{nullptr};
};

} // namespace NereusSDR
