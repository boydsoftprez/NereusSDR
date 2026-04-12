#include "TuneStepButtonItemEditor.h"
#include "../../meters/TuneStepButtonItem.h"

namespace NereusSDR {

TuneStepButtonItemEditor::TuneStepButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildTuneStepSpecific();
}

void TuneStepButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    TuneStepButtonItem* t = qobject_cast<TuneStepButtonItem*>(item);
    if (!t) { return; }
    // TuneStepButtonItem adds no extra settable properties beyond ButtonBoxItem.
}

void TuneStepButtonItemEditor::buildTuneStepSpecific()
{
    // TuneStepButtonItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
