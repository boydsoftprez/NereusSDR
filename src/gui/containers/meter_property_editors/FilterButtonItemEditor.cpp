#include "FilterButtonItemEditor.h"
#include "../../meters/FilterButtonItem.h"

namespace NereusSDR {

FilterButtonItemEditor::FilterButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildFilterSpecific();
}

void FilterButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    FilterButtonItem* f = qobject_cast<FilterButtonItem*>(item);
    if (!f) { return; }
    // FilterButtonItem adds no extra settable properties beyond ButtonBoxItem.
    // Individual filter labels are runtime state managed by the radio.
}

void FilterButtonItemEditor::buildFilterSpecific()
{
    // FilterButtonItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
