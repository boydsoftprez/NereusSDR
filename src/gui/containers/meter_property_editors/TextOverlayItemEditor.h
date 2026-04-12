#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QFontComboBox;

namespace NereusSDR {

class TextOverlayItem;

class TextOverlayItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit TextOverlayItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    // Line 1
    QLineEdit*      m_editText1{nullptr};
    QPushButton*    m_btnColour1{nullptr};
    QPushButton*    m_btnBackColour1{nullptr};
    QCheckBox*      m_chkShowBack1{nullptr};
    QFontComboBox*  m_fontCombo1{nullptr};
    QDoubleSpinBox* m_spinFontSize1{nullptr};
    QCheckBox*      m_chkBold1{nullptr};

    // Line 2
    QLineEdit*      m_editText2{nullptr};
    QPushButton*    m_btnColour2{nullptr};
    QPushButton*    m_btnBackColour2{nullptr};
    QCheckBox*      m_chkShowBack2{nullptr};
    QFontComboBox*  m_fontCombo2{nullptr};
    QDoubleSpinBox* m_spinFontSize2{nullptr};
    QCheckBox*      m_chkBold2{nullptr};

    // Panel
    QCheckBox*      m_chkShowPanel{nullptr};
    QPushButton*    m_btnPanelBack1{nullptr};
    QPushButton*    m_btnPanelBack2{nullptr};

    // Layout
    QDoubleSpinBox* m_spinScrollX{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
};

} // namespace NereusSDR
