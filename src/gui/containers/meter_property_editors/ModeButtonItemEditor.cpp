#include "ModeButtonItemEditor.h"
#include "../../meters/ModeButtonItem.h"

namespace NereusSDR {

ModeButtonItemEditor::ModeButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildModeSpecific();
}

void ModeButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    ModeButtonItem* m = qobject_cast<ModeButtonItem*>(item);
    if (!m) { return; }
    // ModeButtonItem adds no extra settable properties beyond ButtonBoxItem.
}

void ModeButtonItemEditor::buildModeSpecific()
{
    // ModeButtonItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
