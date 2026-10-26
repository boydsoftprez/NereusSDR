#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QComboBox;

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
    QDoubleSpinBox* m_spinGreenThreshold{nullptr};
    QDoubleSpinBox* m_spinAmberThreshold{nullptr};
    QDoubleSpinBox* m_spinRedThreshold{nullptr};
};

} // namespace NereusSDR
