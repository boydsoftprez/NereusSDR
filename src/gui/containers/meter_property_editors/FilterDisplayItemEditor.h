#pragma once
#include "BaseItemEditor.h"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;

namespace NereusSDR {
class FilterDisplayItem;

class FilterDisplayItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit FilterDisplayItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox*      m_comboDisplayMode{nullptr};
    QComboBox*      m_comboWfPalette{nullptr};
    QCheckBox*      m_chkFill{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
    QDoubleSpinBox* m_spinMinDb{nullptr};
    QDoubleSpinBox* m_spinMaxDb{nullptr};

    QPushButton*    m_btnDataLine{nullptr};
    QPushButton*    m_btnDataFill{nullptr};
    QPushButton*    m_btnEdgesRx{nullptr};
    QPushButton*    m_btnEdgesTx{nullptr};
    QPushButton*    m_btnNotch{nullptr};
    QPushButton*    m_btnBack{nullptr};
    QPushButton*    m_btnText{nullptr};
};

} // namespace NereusSDR
