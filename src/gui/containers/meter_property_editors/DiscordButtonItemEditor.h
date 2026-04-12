#pragma once
#include "ButtonBoxItemEditor.h"

namespace NereusSDR {
class DiscordButtonItem;

class DiscordButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit DiscordButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildDiscordSpecific();
};

} // namespace NereusSDR
