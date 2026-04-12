#pragma once
#include "BaseItemEditor.h"

class QCheckBox;
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
    QPushButton* m_btnTimeColour{nullptr};
    QPushButton* m_btnDateColour{nullptr};
    QPushButton* m_btnTitleColour{nullptr};
};

} // namespace NereusSDR
