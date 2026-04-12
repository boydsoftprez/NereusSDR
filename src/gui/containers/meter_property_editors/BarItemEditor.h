#pragma once
#include "BaseItemEditor.h"

class QComboBox;
class QDoubleSpinBox;
class QPushButton;

namespace NereusSDR {

class MeterItem;
class BarItem;

class BarItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit BarItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    // Enum selectors
    QComboBox*      m_comboOrientation{nullptr};
    QComboBox*      m_comboBarStyle{nullptr};

    // Range
    QDoubleSpinBox* m_spinMinVal{nullptr};
    QDoubleSpinBox* m_spinMaxVal{nullptr};

    // Threshold / smoothing
    QDoubleSpinBox* m_spinRedThreshold{nullptr};
    QDoubleSpinBox* m_spinAttack{nullptr};
    QDoubleSpinBox* m_spinDecay{nullptr};

    // Color buttons — Filled mode
    QPushButton*    m_btnBarColor{nullptr};
    QPushButton*    m_btnBarRedColor{nullptr};

    // Color buttons — Edge mode
    QPushButton*    m_btnEdgeBg{nullptr};
    QPushButton*    m_btnEdgeLow{nullptr};
    QPushButton*    m_btnEdgeHigh{nullptr};
    QPushButton*    m_btnEdgeAvg{nullptr};
};

} // namespace NereusSDR
