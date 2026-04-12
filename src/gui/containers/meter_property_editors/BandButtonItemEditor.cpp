#include "BandButtonItemEditor.h"
#include "../../meters/BandButtonItem.h"

namespace NereusSDR {

BandButtonItemEditor::BandButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildBandSpecific();
}

void BandButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    BandButtonItem* b = qobject_cast<BandButtonItem*>(item);
    if (!b) { return; }
    // BandButtonItem adds no extra settable properties beyond ButtonBoxItem.
    // activeBand is runtime state set by the radio, not user-configured here.
}

void BandButtonItemEditor::buildBandSpecific()
{
    // BandButtonItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
