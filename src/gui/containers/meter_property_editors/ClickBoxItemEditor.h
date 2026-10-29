#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace NereusSDR {
class ClickBoxItem;

class ClickBoxItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ClickBoxItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    void applyButtonMask();
    void updateGoToGroupEnabled();

    QCheckBox* m_chkLeft{nullptr};
    QCheckBox* m_chkRight{nullptr};
    QCheckBox* m_chkMiddle{nullptr};
    QComboBox* m_comboAction{nullptr};
    QSpinBox*  m_spinGoToGroup{nullptr};
};

} // namespace NereusSDR
