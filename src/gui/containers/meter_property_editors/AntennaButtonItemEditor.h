#pragma once
#include "ButtonBoxItemEditor.h"

namespace NereusSDR {
class AntennaButtonItem;

class AntennaButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit AntennaButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildAntennaSpecific();
};

} // namespace NereusSDR
