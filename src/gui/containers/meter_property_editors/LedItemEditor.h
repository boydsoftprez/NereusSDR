#pragma once

// Independently implemented from Thetis interface: this header declares
// the NereusSDR editor's rows; the Thetis Setup ranges and their licence
// are in LedItemEditor.cpp.
//
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I3): the marker above. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace NereusSDR {

class LEDItem;

class LedItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit LedItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    // Padding range and the offset / size rows follow the look: placed,
    // a ratio of the width; fill, pixels.
    void applyPlacedRows(bool placed);
    // The condition's inline error, and the thresholds enabled only while
    // no condition is set.
    void refreshConditionRows();

    QLabel*         m_lblId{nullptr};
    QPushButton*    m_btnCopyId{nullptr};
    QComboBox*      m_comboShape{nullptr};
    QComboBox*      m_comboStyle{nullptr};
    QPushButton*    m_btnTrueColour{nullptr};
    QPushButton*    m_btnFalseColour {nullptr};
    QCheckBox*      m_chkShowPanel{nullptr};
    QPushButton*    m_btnPanelBack1{nullptr};
    QPushButton*    m_btnPanelBack2{nullptr};
    QCheckBox*      m_chkBlink{nullptr};
    QCheckBox*      m_chkPulsate{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
    QCheckBox*      m_chkPlaced{nullptr};
    QDoubleSpinBox* m_spinOffsetX{nullptr};
    QDoubleSpinBox* m_spinOffsetY{nullptr};
    QDoubleSpinBox* m_spinSizeX{nullptr};
    QDoubleSpinBox* m_spinSizeY{nullptr};
    QCheckBox*      m_chkShowTrue{nullptr};
    QCheckBox*      m_chkShowFalse{nullptr};
    QCheckBox*      m_chkProcessWhenHidden{nullptr};
    QCheckBox*      m_chkNoTxTrue{nullptr};
    QCheckBox*      m_chkNoTxFalse{nullptr};
    QLineEdit*      m_editCondition{nullptr};
    QLabel*         m_lblConditionError{nullptr};
    QDoubleSpinBox* m_spinGreenThreshold{nullptr};
    QDoubleSpinBox* m_spinAmberThreshold{nullptr};
    QDoubleSpinBox* m_spinRedThreshold{nullptr};
};

} // namespace NereusSDR
