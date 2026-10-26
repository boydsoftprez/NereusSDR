#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QSpinBox;
class QComboBox;
class QDoubleSpinBox;

namespace NereusSDR {

class HistoryGraphItem;

class HistoryGraphItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit HistoryGraphItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QSpinBox*    m_spinCapacity{nullptr};
    QPushButton* m_btnLineColor0{nullptr};
    QPushButton* m_btnLineColor1{nullptr};
    QCheckBox*   m_chkShowGrid{nullptr};
    QCheckBox*   m_chkAutoScale0{nullptr};
    QCheckBox*   m_chkAutoScale1{nullptr};
    QCheckBox*   m_chkShowScale0{nullptr};
    QCheckBox*   m_chkShowScale1{nullptr};
    QComboBox*   m_comboBinding1{nullptr};
    QDoubleSpinBox* m_spinVerticalRatio{nullptr};
    QPushButton* m_btnBackColour{nullptr};
    QPushButton* m_btnLinesColour{nullptr};
    QPushButton* m_btnTimeColour{nullptr};
    QDoubleSpinBox* m_spinMin0{nullptr};
    QDoubleSpinBox* m_spinMax0{nullptr};
    QDoubleSpinBox* m_spinMin1{nullptr};
    QDoubleSpinBox* m_spinMax1{nullptr};

    void syncManualRangeEnabled();
};

} // namespace NereusSDR
