// src/gui/applets/DiversityApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QComboBox;
class QSlider;
class QLabel;

namespace NereusSDR {

// Diversity reception controls (two-antenna phase/gain combining).
// NYI — Phase 3F (Multi-Panadapter + DDC assignment).
class DiversityApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit DiversityApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("diversity"); }
    QString appletTitle() const override { return QStringLiteral("Diversity"); }
    void    syncFromModel() override;

private:
    QPushButton* m_enableBtn     = nullptr;  // green toggle "DIV"
    QComboBox*   m_sourceCombo   = nullptr;  // RX1 / RX2

    QSlider* m_gainSlider        = nullptr;
    QLabel*  m_gainValue         = nullptr;

    QSlider* m_phaseSlider       = nullptr;  // 0–360°
    QLabel*  m_phaseValue        = nullptr;

    QComboBox* m_escCombo        = nullptr;  // Off / Adaptive / Fixed

    QSlider* m_r2GainSlider      = nullptr;
    QLabel*  m_r2GainValue       = nullptr;

    QSlider* m_r2PhaseSlider     = nullptr;
    QLabel*  m_r2PhaseValue      = nullptr;
};

} // namespace NereusSDR
