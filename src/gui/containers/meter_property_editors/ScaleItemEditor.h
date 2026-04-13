#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;

namespace NereusSDR {

class ScaleItem;

class ScaleItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ScaleItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox*      m_comboOrientation{nullptr};
    QDoubleSpinBox* m_spinMin{nullptr};
    QDoubleSpinBox* m_spinMax{nullptr};
    QSpinBox*       m_spinMajorTicks{nullptr};
    QSpinBox*       m_spinMinorTicks{nullptr};
    QPushButton*    m_btnTickColor{nullptr};
    QPushButton*    m_btnLabelColor{nullptr};
    QSpinBox*       m_spinFontSize{nullptr};
    // Phase B4 — ShowType centered title (Thetis clsScaleItem.ShowType)
    QCheckBox*      m_chkShowType{nullptr};
    QPushButton*    m_btnTitleColor{nullptr};
};

} // namespace NereusSDR
