#pragma once

#include "AppletWidget.h"

class QComboBox;
class QSlider;
class QPushButton;
class QLabel;

namespace NereusSDR {

// VAC/DAX digital audio routing applet — NYI shell (Phase 3-DAX).
//
// Controls:
//   1. VAC 1 enable       — QPushButton("VAC 1", green, checkable)
//   2. VAC 1 device combo — QComboBox (device list, empty)
//   3. VAC 2 enable       — QPushButton("VAC 2", green, checkable)
//   4. VAC 2 device combo — QComboBox (device list, empty)
//   5. Sample rate combo  — QComboBox: "48000", "96000", "192000"
//   6. Stereo/Mono toggle — QPushButton("Stereo", blue, checkable)
//   7. Buffer size combo  — QComboBox: "256", "512", "1024", "2048"
//
// Additional: TX/RX gain sliders (x2), rigctld channel combo.
// All controls disabled (NYI). Body margins: (4,2,4,2), spacing 2.
class DigitalApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit DigitalApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("DIG"); }
    QString appletTitle() const override { return QStringLiteral("Digital / VAC"); }
    void syncFromModel() override;

private:
    void buildUI();
    void buildSliderRow(QVBoxLayout* root, const QString& label,
                        QSlider*& sliderOut, QLabel*& valueLblOut);

    // Control 1 — VAC 1 enable
    QPushButton* m_vac1Btn        = nullptr;
    // Control 2 — VAC 1 device
    QComboBox*   m_vac1DevCombo   = nullptr;

    // Control 3 — VAC 2 enable
    QPushButton* m_vac2Btn        = nullptr;
    // Control 4 — VAC 2 device
    QComboBox*   m_vac2DevCombo   = nullptr;

    // Control 5 — Sample rate
    QComboBox*   m_sampleRateCombo = nullptr;

    // Control 6 — Stereo/Mono
    QPushButton* m_stereoBtn      = nullptr;

    // Control 7 — Buffer size
    QComboBox*   m_bufferSizeCombo = nullptr;

    // Additional — RX/TX gain sliders
    QSlider*     m_rxGainSlider   = nullptr;
    QLabel*      m_rxGainLbl      = nullptr;
    QSlider*     m_txGainSlider   = nullptr;
    QLabel*      m_txGainLbl      = nullptr;

    // Additional — rigctld channel combo
    QComboBox*   m_rigctldCombo   = nullptr;
};

} // namespace NereusSDR
