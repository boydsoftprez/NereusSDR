#pragma once

// Independently implemented from MeterManager.cs interface: a Qt editor
// declaration; the ported ranges live in FilterDisplayItemEditor.cpp.
#include "BaseItemEditor.h"

#include <functional>
#include <vector>

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
    // Thetis filter display settings and colours (rows added 2026-10-26).
    void buildThetisSettings();
    void addColourRow(const QString& label, const QString& objectName,
                      const QString& field,
                      QColor (FilterDisplayItem::*getter)() const,
                      void (FilterDisplayItem::*setter)(const QColor&));

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

    // Each refreshes one Thetis settings row from the item in setItem().
    std::vector<std::function<void(const FilterDisplayItem&)>> m_refreshers;
};

} // namespace NereusSDR
