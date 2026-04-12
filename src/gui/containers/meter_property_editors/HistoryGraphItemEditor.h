#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QSpinBox;
class QComboBox;

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
};

} // namespace NereusSDR
