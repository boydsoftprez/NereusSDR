// no-port-check: NereusSDR-original editor for the filter grid's visibility.
// Modification history (NereusSDR):
//   2026-10-26 - One "Visible filters" row per filter button, as the other
//                 grids have; the Var1 and Var2 rows are shown disabled with
//                 the not-built reason (disabled, never hidden). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "FilterButtonItemEditor.h"
#include "../../meters/FilterButtonItem.h"
#include "gui/UnbuiltFeatures.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>

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
    // Individual filter labels are runtime state managed by the radio; the
    // rows show which buttons the grid draws.
    beginProgrammaticUpdate();
    for (int i = 0; i < m_visible.size(); ++i) {
        m_visible[i]->setChecked(((f->visibleBits() >> i) & 1u) != 0);
    }
    endProgrammaticUpdate();
}

void FilterButtonItemEditor::buildFilterSpecific()
{
    // One row per button, in button order. F1-F10 take the mode's preset
    // names on the grid, so their rows name them by position.
    addHeader(tr("Visible filters"));
    for (int i = 0; i < FilterButtonItem::kFilterCount; ++i) {
        const bool var = i == FilterButtonItem::kVar1Button || i == FilterButtonItem::kVar2Button;
        const QString caption = var
            ? tr("Var%1").arg(i - FilterButtonItem::kVar1Button + 1)
            : tr("F%1").arg(i + 1);
        auto* check = makeCheckRow(caption);
        check->setObjectName(QStringLiteral("filterVisible_%1").arg(i));
        // Var1 and Var2 are never drawn while variable filters are not
        // built: their rows are shown disabled with the reason.
        const bool built = !var || UnbuiltFeatures::isBuilt(UnbuiltFeature::VariableFilters);
        const QString tooltip = built ? QString() : UnbuiltFeatures::notBuiltReason();
        check->setEnabled(built);
        check->setToolTip(tooltip);
        if (auto* label = qobject_cast<QLabel*>(m_form->labelForField(check))) {
            label->setToolTip(tooltip);
            label->setEnabled(built);
        }
        m_visible.append(check);
        connect(check, &QCheckBox::toggled, this, [this, i](bool visible) {
            if (isProgrammaticUpdate()) { return; }
            auto* f = qobject_cast<FilterButtonItem*>(m_item);
            if (!f) { return; }
            const uint32_t bit = 1u << i;
            f->setVisibleBits(visible ? (f->visibleBits() | bit) : (f->visibleBits() & ~bit));
            notifyChanged();
        });
    }
}

} // namespace NereusSDR
