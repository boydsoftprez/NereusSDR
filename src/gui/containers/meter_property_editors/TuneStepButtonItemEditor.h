#pragma once
#include "ButtonBoxItemEditor.h"

namespace NereusSDR {
class TuneStepButtonItem;

class TuneStepButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit TuneStepButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTuneStepSpecific();
};

} // namespace NereusSDR
