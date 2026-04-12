#include "DiscordButtonItemEditor.h"
#include "../../meters/DiscordButtonItem.h"

namespace NereusSDR {

DiscordButtonItemEditor::DiscordButtonItemEditor(QWidget* parent)
    : ButtonBoxItemEditor(parent)
{
    buildButtonBoxSection();
    buildDiscordSpecific();
}

void DiscordButtonItemEditor::setItem(MeterItem* item)
{
    ButtonBoxItemEditor::setItem(item);
    DiscordButtonItem* d = qobject_cast<DiscordButtonItem*>(item);
    if (!d) { return; }
    // DiscordButtonItem adds no extra settable properties beyond ButtonBoxItem.
}

void DiscordButtonItemEditor::buildDiscordSpecific()
{
    // DiscordButtonItem has no extra configurable properties beyond ButtonBoxItem.
}

} // namespace NereusSDR
