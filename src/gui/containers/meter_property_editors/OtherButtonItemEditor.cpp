#include "OtherButtonItemEditor.h"
#include "../../meters/OtherButtonItem.h"

namespace NereusSDR {

OtherButtonItemEditor::OtherButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildOtherSpecific();
}

void OtherButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    OtherButtonItem* o = qobject_cast<OtherButtonItem*>(item);
    if (!o) { return; }
    // OtherButtonItem adds no extra settable properties beyond ButtonBoxItem.
    // MacroSettings are runtime configuration, not item visual properties.
}

void OtherButtonItemEditor::buildOtherSpecific()
{
    // OtherButtonItem has no extra configurable visual properties beyond ButtonBoxItem.
    // Macro configuration (MacroSettings) is complex runtime state outside scope of
    // the visual property editor.
}

} // namespace NereusSDR
