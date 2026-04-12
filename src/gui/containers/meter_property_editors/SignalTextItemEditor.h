#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QComboBox;
class QFontComboBox;

namespace NereusSDR {

class SignalTextItem;

class SignalTextItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit SignalTextItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox*      m_comboUnits{nullptr};
    QCheckBox*      m_chkShowValue{nullptr};
    QCheckBox*      m_chkShowPeak{nullptr};
    QCheckBox*      m_chkShowType{nullptr};
    QCheckBox*      m_chkPeakHold{nullptr};
    QPushButton*    m_btnColour{nullptr};
    QPushButton*    m_btnPeakColour{nullptr};
    QPushButton*    m_btnMarkerColour{nullptr};
    QFontComboBox*  m_fontCombo{nullptr};
    QDoubleSpinBox* m_spinFontSize{nullptr};
    QComboBox*      m_comboBarStyle{nullptr};
};

} // namespace NereusSDR
