#pragma once

#include "AppletWidget.h"

class QComboBox;
class QSlider;
class QPushButton;
class QLabel;

namespace NereusSDR {

// FM mode controls applet — NYI shell (Phase 3I-3).
//
// Controls:
//   1. FM Mic slider       — label + QSlider + value label
//   2. Deviation buttons   — "5.0k" (blue) + "2.5k" (blue), mutually exclusive
//   3. CTCSS enable        — QPushButton("CTCSS", green, checkable)
//   4. CTCSS tone combo    — QComboBox with standard CTCSS tones
//   5. Simplex toggle      — QPushButton("Simplex", green, checkable)
//   6. Repeater offset     — Label + TriBtn(◀) + inset value + TriBtn(▶)
//   7. Offset direction    — QPushButton("-") + QPushButton("+") + QPushButton("Rev"),
//                            mutually exclusive
//   8. FM TX Profile combo — QComboBox with "Default"
//
// All controls are disabled (NYI). Body margins: (4,2,4,2), spacing 2.
class FmApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit FmApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("FM"); }
    QString appletTitle() const override { return QStringLiteral("FM"); }
    void syncFromModel() override;

private:
    void buildUI();

    // Control 1 — FM Mic
    QSlider*     m_micSlider    = nullptr;
    QLabel*      m_micValueLbl  = nullptr;

    // Control 2 — Deviation
    QPushButton* m_dev5kBtn     = nullptr;
    QPushButton* m_dev25kBtn    = nullptr;

    // Control 3 — CTCSS enable
    QPushButton* m_ctcssBtn     = nullptr;

    // Control 4 — CTCSS tone
    QComboBox*   m_ctcssCombo   = nullptr;

    // Control 5 — Simplex
    QPushButton* m_simplexBtn   = nullptr;

    // Control 6 — Repeater offset stepper
    QLabel*      m_offsetLbl    = nullptr;

    // Control 7 — Offset direction
    QPushButton* m_offsetNegBtn = nullptr;
    QPushButton* m_offsetPosBtn = nullptr;
    QPushButton* m_offsetRevBtn = nullptr;

    // Control 8 — TX profile
    QComboBox*   m_txProfileCombo = nullptr;
};

} // namespace NereusSDR
