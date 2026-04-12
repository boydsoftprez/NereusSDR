#pragma once
#include "ButtonBoxItemEditor.h"

namespace NereusSDR {
class VoiceRecordPlayItem;

class VoiceRecordPlayItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit VoiceRecordPlayItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildVoiceSpecific();
};

} // namespace NereusSDR
