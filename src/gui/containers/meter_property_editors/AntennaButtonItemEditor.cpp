// no-port-check: NereusSDR-original editor for the antenna grid's visibility.
// Modification history (NereusSDR):
//   2026-10-26 - One "Visible buttons" row per antenna button; a row whose
//                 button's feature is not built is shown disabled with the
//                 reason (disabled, never hidden). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include "AntennaButtonItemEditor.h"
#include "../../meters/AntennaButtonItem.h"
#include "gui/UnbuiltFeatures.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>

#include <optional>

namespace NereusSDR {

namespace {
// The feature a button waits for, if any: XVTR goes with the transverters
// and Rx/Tx with the receive/transmit split (AntennaButtonItem's
// constructor hides both until built).
std::optional<UnbuiltFeature> unbuiltFeatureFor(int index)
{
    if (index == AntennaButtonItem::kXvtrButton) { return UnbuiltFeature::Transverters; }
    if (index == AntennaButtonItem::kRxTxButton) { return UnbuiltFeature::AntennaRxTxSplit; }
    return std::nullopt;
}
} // namespace

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
    beginProgrammaticUpdate();
    for (int i = 0; i < m_visible.size(); ++i) {
        m_visible[i]->setChecked(((a->visibleBits() >> i) & 1u) != 0);
    }
    endProgrammaticUpdate();
}

void AntennaButtonItemEditor::buildAntennaSpecific()
{
    // One row per button, in button order. The receive-only inputs take
    // the radio's names on the grid, so their rows name them by position.
    const QStringList captions = {
        tr("Rx Ant 1"), tr("Rx Ant 2"), tr("Rx Ant 3"),
        tr("Receive-only input 1"), tr("Receive-only input 2"), tr("XVTR input"),
        tr("Tx Ant 1"), tr("Tx Ant 2"), tr("Tx Ant 3"), tr("Rx/Tx swap")};
    addHeader(tr("Visible buttons"));
    for (int i = 0; i < AntennaButtonItem::kAntennaCount; ++i) {
        auto* check = makeCheckRow(captions.value(i));
        check->setObjectName(QStringLiteral("antennaVisible_%1").arg(i));
        // A button whose feature is not built is never drawn: its row is
        // shown disabled with the reason (disabled, never hidden).
        const auto feature = unbuiltFeatureFor(i);
        const bool built = !feature || UnbuiltFeatures::isBuilt(*feature);
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
            auto* a = qobject_cast<AntennaButtonItem*>(m_item);
            if (!a) { return; }
            const uint32_t bit = 1u << i;
            a->setVisibleBits(visible ? (a->visibleBits() | bit) : (a->visibleBits() & ~bit));
            notifyChanged();
        });
    }
}

} // namespace NereusSDR
