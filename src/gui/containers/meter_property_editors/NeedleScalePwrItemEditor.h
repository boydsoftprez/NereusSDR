#pragma once
#include "BaseItemEditor.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QFontComboBox;
class QTableWidget;

namespace NereusSDR {

class NeedleScalePwrItem;

// Phase 3G-6 block 4 — per-item property editor for NeedleScalePwrItem.
// Exposes full Thetis parity: colors, font, marks, power, direction,
// geometry, and the calibration map (fully editable table).
class NeedleScalePwrItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit NeedleScalePwrItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    void refreshCalTable(NeedleScalePwrItem* nspi);
    void commitCalTable(NeedleScalePwrItem* nspi);

    QPushButton*    m_btnLowColour{nullptr};
    QPushButton*    m_btnHighColour{nullptr};
    QFontComboBox*  m_fontCombo{nullptr};
    QDoubleSpinBox* m_spinFontSize{nullptr};
    QCheckBox*      m_chkFontBold{nullptr};
    QSpinBox*       m_spinMarks{nullptr};
    QCheckBox*      m_chkShowMarkers{nullptr};
    QDoubleSpinBox* m_spinMaxPower{nullptr};
    QCheckBox*      m_chkDarkMode{nullptr};
    QComboBox*      m_comboDirection{nullptr};
    QDoubleSpinBox* m_spinOffsetX{nullptr};
    QDoubleSpinBox* m_spinOffsetY{nullptr};
    QDoubleSpinBox* m_spinLengthFactor{nullptr};
    QTableWidget*   m_calTable{nullptr};
};

} // namespace NereusSDR
