#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;

namespace NereusSDR {

class MeterItem;
class SpacerItem;

class SpacerItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit SpacerItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QPushButton*    m_btnColour1{nullptr};
    QPushButton*    m_btnColour2{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
    QCheckBox*      m_chkRxTxColours{nullptr};
    QCheckBox*      m_chkPaddingSetsHeight{nullptr};
};

} // namespace NereusSDR
