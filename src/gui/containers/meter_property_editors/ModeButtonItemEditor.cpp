// no-port-check: NereusSDR-original editor for the mode grid's options.
// Modification history (NereusSDR):
//   2026-10-26 - A row per mode (all twelve, in DSPMode order) shows or
//                 hides its button, so a grid saved before SPEC and DRM
//                 can show them. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include "ModeButtonItemEditor.h"
#include "../../meters/ModeButtonItem.h"

#include <QCheckBox>

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
    beginProgrammaticUpdate();
    for (int i = 0; i < m_visible.size(); ++i) {
        m_visible[i]->setChecked(((m->visibleBits() >> i) & 1u) != 0);
    }
    endProgrammaticUpdate();
}

void ModeButtonItemEditor::buildModeSpecific()
{
    addHeader(tr("Visible modes"));
    for (int i = 0; i < ModeButtonItem::kModeCount; ++i) {
        auto* check = makeCheckRow(ModeButtonItem::modeLabel(i));
        check->setObjectName(QStringLiteral("modeVisible_%1").arg(i));
        m_visible.append(check);
        connect(check, &QCheckBox::toggled, this, [this, i](bool visible) {
            if (isProgrammaticUpdate()) { return; }
            auto* m = qobject_cast<ModeButtonItem*>(m_item);
            if (!m) { return; }
            const uint32_t bit = 1u << i;
            m->setVisibleBits(visible ? (m->visibleBits() | bit) : (m->visibleBits() & ~bit));
            notifyChanged();
        });
    }
}

} // namespace NereusSDR
