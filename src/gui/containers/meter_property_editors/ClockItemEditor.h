#pragma once

// Independently implemented from Thetis interface: this header declares
// the NereusSDR editor's rows; the ported setup logic and its licence
// are in ClockItemEditor.cpp.
#include "BaseItemEditor.h"

class QCheckBox;
class QComboBox;
class QPushButton;

namespace NereusSDR {
class ClockItem;

class ClockItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ClockItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QCheckBox*   m_chkShow24Hour{nullptr};
    QCheckBox*   m_chkShowType{nullptr};
    QComboBox*   m_comboLayout{nullptr};
    QCheckBox*   m_chkBackEnabled{nullptr};
    QPushButton* m_btnBackColour{nullptr};
    QPushButton* m_btnTimeColour{nullptr};
    QPushButton* m_btnDateColour{nullptr};
    QPushButton* m_btnTitleColour{nullptr};
};

} // namespace NereusSDR
