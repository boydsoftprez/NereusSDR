#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;

namespace NereusSDR {
class DialItem;

class DialItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit DialItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    // Text/shape colors
    QPushButton* m_btnTextColour{nullptr};
    QPushButton* m_btnCircleColour{nullptr};
    QPushButton* m_btnPadColour{nullptr};
    QPushButton* m_btnPadPressedColour{nullptr};
    QPushButton* m_btnRingColour{nullptr};

    // Button state colors
    QPushButton* m_btnBtnOn{nullptr};
    QPushButton* m_btnBtnOff{nullptr};
    QPushButton* m_btnBtnHighlight{nullptr};

    // Speed indicator colors
    QPushButton* m_btnSlowColour{nullptr};
    QPushButton* m_btnHoldColour{nullptr};
    QPushButton* m_btnFastColour{nullptr};

    // Dial and tuning
    QDoubleSpinBox* m_spinVerticalRatio{nullptr};
    QDoubleSpinBox* m_spinFontScale{nullptr};
    QComboBox*      m_comboSliceSelect{nullptr};
    QSpinBox*       m_spinDegreesPerChange{nullptr};
    QCheckBox*      m_chkAlign{nullptr};

    // Acceleration
    QCheckBox*      m_chkAccelerate{nullptr};
    QSpinBox*       m_spinUpThreshold{nullptr};
    QSpinBox*       m_spinDownThreshold{nullptr};
    QSpinBox*       m_spinInterval{nullptr};
    QSpinBox*       m_spinMaxChanges{nullptr};
};

} // namespace NereusSDR
