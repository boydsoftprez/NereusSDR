#include "ClickBoxItemEditor.h"
#include "../../meters/ClickBoxItem.h"

namespace NereusSDR {

ClickBoxItemEditor::ClickBoxItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void ClickBoxItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    // ClickBoxItem has no type-specific properties; the base class populates
    // position, binding, z-order, onlyWhenRx/Tx, and displayGroup.
    (void)qobject_cast<ClickBoxItem*>(item); // type check only
}

void ClickBoxItemEditor::buildTypeSpecific()
{
    // ClickBoxItem has no visual properties beyond the common MeterItem fields.
    // No additional rows are needed.
}

} // namespace NereusSDR
