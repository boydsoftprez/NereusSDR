#pragma once
#include "BaseItemEditor.h"

class QPushButton;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;

namespace NereusSDR {

class MagicEyeItem;

class MagicEyeItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit MagicEyeItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QPushButton* m_btnGlowColor{nullptr};
    QLineEdit*   m_editBezelPath{nullptr};
    QPushButton* m_btnBrowseBezel{nullptr};
    QDoubleSpinBox* m_spinEyeScale{nullptr};
    QDoubleSpinBox* m_spinBezelScale{nullptr};
    QDoubleSpinBox* m_spinAttack{nullptr};
    QDoubleSpinBox* m_spinDecay{nullptr};
    QCheckBox*      m_chkShowHistory{nullptr};
    QSpinBox*       m_spinHistoryMs{nullptr};
    QPushButton*    m_btnHistoryColour{nullptr};
    QSpinBox*       m_spinIgnoreHistoryMs{nullptr};
    QLineEdit*      m_editCalibration{nullptr};
};

} // namespace NereusSDR
