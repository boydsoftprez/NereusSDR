#pragma once
#include "BaseItemEditor.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QTableWidget;

namespace NereusSDR {

class NeedleItem;

// Phase 3G-6 block 4 — per-item property editor for NeedleItem.
// Exposes full Thetis parity for every setter that exists on NeedleItem,
// including ANANMM/CrossNeedle calibration table editing.
class NeedleItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit NeedleItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    // Repopulate the calibration table from the current item's map.
    void refreshCalTable(NeedleItem* needle);
    // Rebuild the calibration map from the table and push to item.
    void commitCalTable(NeedleItem* needle);

    // General
    QLineEdit*      m_editSourceLabel{nullptr};

    // Visual
    QPushButton*    m_btnNeedleColor{nullptr};
    QDoubleSpinBox* m_spinAttack{nullptr};
    QDoubleSpinBox* m_spinDecay{nullptr};
    QDoubleSpinBox* m_spinLengthFactor{nullptr};
    QDoubleSpinBox* m_spinOffsetX{nullptr};
    QDoubleSpinBox* m_spinOffsetY{nullptr};
    QDoubleSpinBox* m_spinRadiusX{nullptr};
    QDoubleSpinBox* m_spinRadiusY{nullptr};
    QDoubleSpinBox* m_spinStrokeWidth{nullptr};
    QComboBox*      m_comboDirection{nullptr};

    // History
    QCheckBox*      m_chkHistory{nullptr};
    QSpinBox*       m_spinHistoryDuration{nullptr};
    QPushButton*    m_btnHistoryColor{nullptr};

    // Power normalisation
    QCheckBox*      m_chkNormaliseTo100W{nullptr};
    QDoubleSpinBox* m_spinMaxPower{nullptr};

    // Calibration table
    QTableWidget*   m_calTable{nullptr};
};

} // namespace NereusSDR
