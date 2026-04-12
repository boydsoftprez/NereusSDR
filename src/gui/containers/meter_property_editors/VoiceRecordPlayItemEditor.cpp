#include "VoiceRecordPlayItemEditor.h"
#include "../../meters/VoiceRecordPlayItem.h"

namespace NereusSDR {

VoiceRecordPlayItemEditor::VoiceRecordPlayItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildVoiceSpecific();
}

void VoiceRecordPlayItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    VoiceRecordPlayItem* v = qobject_cast<VoiceRecordPlayItem*>(item);
    if (!v) { return; }
    // VoiceRecordPlayItem adds no extra settable properties beyond ButtonBoxItem.
}

void VoiceRecordPlayItemEditor::buildVoiceSpecific()
{
    // VoiceRecordPlayItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
