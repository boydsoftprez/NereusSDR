// =================================================================
// src/gui/setup/PgxlInterlockPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native Setup -> Transmit -> PGXL Interlock page.
//
// Three controls that configure TxInterlockPolicy:
//   1. Mode combo (Disabled / Warn / Block)
//   2. Grace spinbox (0..30000 ms)
//   3. SWR gate: enable checkbox + max-SWR spinbox (1.0..10.0)
//
// The page does NOT own TxInterlockPolicy. It obtains a non-owning
// pointer from RadioModel::txInterlockPolicy() and forwards changes
// to it. TxInterlockPolicy persists each change to AppSettings
// immediately in its own setters, so the page needs no extra save step.
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
//   Task 86 (line 2820), spec section 5.8.
//
// AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;

namespace NereusSDR {

class RadioModel;
class TxInterlockPolicy;

class PgxlInterlockPage : public QWidget {
    Q_OBJECT
public:
    explicit PgxlInterlockPage(RadioModel* model, QWidget* parent = nullptr);

private slots:
    void onModeChanged(int idx);
    void onGraceChanged(int ms);
    void onSwrGateToggled(bool on);
    void onSwrGateMaxChanged(double val);

private:
    void buildUi();
    void loadFromPolicy();

    RadioModel*         m_model{nullptr};
    TxInterlockPolicy*  m_policy{nullptr};  // non-owning

    QComboBox*        m_modeCombo{nullptr};
    QSpinBox*         m_graceSpinbox{nullptr};
    QCheckBox*        m_swrGateCheckbox{nullptr};
    QDoubleSpinBox*   m_swrGateMaxSpinbox{nullptr};
    QLabel*           m_helpText{nullptr};

    // Guard against feedback loops when populating controls from policy.
    bool m_loading{false};
};

}  // namespace NereusSDR
