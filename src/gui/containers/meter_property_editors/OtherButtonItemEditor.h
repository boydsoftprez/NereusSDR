#pragma once
#include "ButtonBoxItemEditor.h"

namespace NereusSDR {
class OtherButtonItem;

class OtherButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit OtherButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildOtherSpecific();
};

} // namespace NereusSDR
