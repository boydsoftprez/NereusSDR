#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;

namespace NereusSDR {

class MeterItem;
class FadeCoverItem;

class FadeCoverItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit FadeCoverItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QCheckBox*      m_chkFadeOnRx{nullptr};
    QCheckBox*      m_chkFadeOnTx{nullptr};
    QPushButton*    m_btnColour1{nullptr};
    QPushButton*    m_btnColour2{nullptr};
    QDoubleSpinBox* m_spinAlpha{nullptr};
};

} // namespace NereusSDR
