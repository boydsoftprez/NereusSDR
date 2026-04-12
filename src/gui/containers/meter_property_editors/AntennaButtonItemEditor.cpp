#include "AntennaButtonItemEditor.h"
#include "../../meters/AntennaButtonItem.h"

namespace NereusSDR {

AntennaButtonItemEditor::AntennaButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildAntennaSpecific();
}

void AntennaButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    AntennaButtonItem* a = qobject_cast<AntennaButtonItem*>(item);
    if (!a) { return; }
    // AntennaButtonItem adds no extra settable properties beyond ButtonBoxItem.
}

void AntennaButtonItemEditor::buildAntennaSpecific()
{
    // AntennaButtonItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
