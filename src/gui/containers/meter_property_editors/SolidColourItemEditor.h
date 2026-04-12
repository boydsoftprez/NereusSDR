#pragma once
#include "BaseItemEditor.h"

class QPushButton;

namespace NereusSDR {

class MeterItem;
class SolidColourItem;

class SolidColourItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit SolidColourItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QPushButton* m_btnColour{nullptr};
};

} // namespace NereusSDR
