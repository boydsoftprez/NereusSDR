#pragma once
#include "ButtonBoxItemEditor.h"

namespace NereusSDR {
class BandButtonItem;

class BandButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit BandButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildBandSpecific();
};

} // namespace NereusSDR
