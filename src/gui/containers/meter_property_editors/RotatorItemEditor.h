#pragma once
#include "BaseItemEditor.h"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;

namespace NereusSDR {
class RotatorItem;

class RotatorItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit RotatorItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox*      m_comboMode{nullptr};
    QCheckBox*      m_chkShowValue{nullptr};
    QCheckBox*      m_chkShowCardinals{nullptr};
    QCheckBox*      m_chkShowBeamWidth{nullptr};
    QDoubleSpinBox* m_spinBeamWidth{nullptr};
    QDoubleSpinBox* m_spinBeamAlpha{nullptr};
    QCheckBox*      m_chkDarkMode{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};

    // Background image path
    QLineEdit*      m_editBgPath{nullptr};

    // Colors
    QPushButton*    m_btnBigBlob{nullptr};
    QPushButton*    m_btnSmallBlob{nullptr};
    QPushButton*    m_btnOuterText{nullptr};
    QPushButton*    m_btnArrow{nullptr};
    QPushButton*    m_btnBeamWidth{nullptr};
    QPushButton*    m_btnBackground{nullptr};
};

} // namespace NereusSDR
