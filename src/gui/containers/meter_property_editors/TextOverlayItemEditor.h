#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QFontComboBox;
class QComboBox;

namespace NereusSDR {

class TextOverlayItem;

class TextOverlayItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit TextOverlayItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
    // A check row that writes one flag of the item.
    QCheckBox* makeFlagRow(const QString& label, void (TextOverlayItem::*setter)(bool));
    // A spin row (-10..10) that writes one offset of the item.
    QDoubleSpinBox* makeOffsetRow(const QString& label, void (TextOverlayItem::*setter)(float));

    QComboBox*      m_comboLineMode{nullptr};

    // Line 1
    QLineEdit*      m_editText1{nullptr};
    QPushButton*    m_btnColour1{nullptr};
    QPushButton*    m_btnBackColour1{nullptr};
    QCheckBox*      m_chkShowBack1{nullptr};
    QFontComboBox*  m_fontCombo1{nullptr};
    QDoubleSpinBox* m_spinFontSize1{nullptr};
    QCheckBox*      m_chkBold1{nullptr};
    QCheckBox*      m_chkItalic1{nullptr};
    QCheckBox*      m_chkUnderline1{nullptr};
    QCheckBox*      m_chkStrikeout1{nullptr};
    QDoubleSpinBox* m_spinXOffset1{nullptr};
    QDoubleSpinBox* m_spinYOffset1{nullptr};

    // Line 2
    QLineEdit*      m_editText2{nullptr};
    QPushButton*    m_btnColour2{nullptr};
    QPushButton*    m_btnBackColour2{nullptr};
    QCheckBox*      m_chkShowBack2{nullptr};
    QFontComboBox*  m_fontCombo2{nullptr};
    QDoubleSpinBox* m_spinFontSize2{nullptr};
    QCheckBox*      m_chkBold2{nullptr};
    QCheckBox*      m_chkItalic2{nullptr};
    QCheckBox*      m_chkUnderline2{nullptr};
    QCheckBox*      m_chkStrikeout2{nullptr};
    QDoubleSpinBox* m_spinXOffset2{nullptr};
    QDoubleSpinBox* m_spinYOffset2{nullptr};
    QPushButton*    m_btnCopyFont{nullptr};
    QPushButton*    m_btnCopyOffsets{nullptr};

    // LED gates
    QCheckBox*      m_chkRxLedLogic{nullptr};
    QLineEdit*      m_editRxFourChar{nullptr};
    QCheckBox*      m_chkTxLedLogic{nullptr};
    QLineEdit*      m_editTxFourChar{nullptr};

    // Panel
    QCheckBox*      m_chkShowPanel{nullptr};
    QPushButton*    m_btnPanelBack1{nullptr};
    QPushButton*    m_btnPanelBack2{nullptr};

    // Layout
    QDoubleSpinBox* m_spinScrollX{nullptr};
    QDoubleSpinBox* m_spinPadding{nullptr};
};

} // namespace NereusSDR
