// =================================================================
// src/gui/applets/TxSectionContent.{h,cpp}  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.  It offers the TX applet's rows to
// the container as content the operator can place on their own.  It contains
// no Thetis code and derives from no Thetis source.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-19 - The TX applet's rows as independent container content, so a
//                 container can hold the monitor controls (or any other row)
//                 on their own.  The applet stays the wiring authority.
//                 Brian Sutton, AI-assisted via OpenCode.
//   2026-10-23 - TX Keying can hide its slice indicators. Brian Sutton,
//                 AI-assisted via xAI Grok.
//   2026-10-21 - A preview of a row (ContentRenderMode::Preview) is inert.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Every row hides and orders its parts from the entry config
//                 (SectionParts), and declares a toggle per part and the
//                 order. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include "TxSectionContent.h"

#include "TxApplet.h"
#include "TxMonitorSection.h"
#include "TxPowerSection.h"
#include "TxFilterSection.h"
#include "TxChainSection.h"
#include "TxVoxSection.h"
#include "TxProcessingSection.h"
#include "TxKeyingSection.h"

#include "gui/containers/ContainerContentRegistry.h"

#include <algorithm>
#include <memory>

namespace NereusSDR {

namespace {

inline constexpr auto kMonitorType = "tx.monitor";
inline constexpr auto kPowerType = "tx.power";
inline constexpr auto kFilterType = "tx.filter";
inline constexpr auto kChainType = "tx.chain";
inline constexpr auto kVoxType = "tx.vox";
inline constexpr auto kProcessingType = "tx.processing";
inline constexpr auto kKeyingType = "tx.keying";
inline constexpr auto kHideSliceIndicators = "hideSliceIndicators";

QVector<ContentProperty> keyingProperties()
{
    ContentProperty hide;
    hide.key = QLatin1String(kHideSliceIndicators);
    hide.label = QStringLiteral("Hide slice indicators");
    hide.kind = ContentProperty::Kind::Toggle;
    hide.defaultValue = false;
    return {hide};
}

// Any mode but Live builds an inert row (ContainerContentRegistry.h:
// "In ContentRenderMode::Preview the widget is inert"): it is not wired to
// the model and is not one of the applet's rows.
bool isPreview(ContentRenderMode mode)
{
    return mode != ContentRenderMode::Live;
}

// The properties a row type declares: the ones it declared before (which
// keep their positions), then a toggle per part and the Order list, from
// an inert probe row the applet makes and that is gone on return.
template <typename Section>
QVector<ContentProperty> partProperties(Section* probe, QVector<ContentProperty> existing = {})
{
    const std::unique_ptr<Section> owned(probe);
    if (!owned) {
        return existing;
    }
    for (const ContentProperty& property : SectionParts::properties(owned->parts())) {
        const bool known = std::any_of(existing.cbegin(), existing.cend(),
            [&property](const ContentProperty& p) { return p.key == property.key; });
        if (!known) {
            existing.append(property);
        }
    }
    return existing;
}

// A copy of a row with its entry's parts hidden and ordered.  The factory
// reads only the entry's config, never its context: the row follows the
// TX panel.
template <typename Section>
QWidget* withParts(Section* section, const ContentEntry& entry)
{
    if (section) {
        section->parts().apply(entry.config);
    }
    return static_cast<QWidget*>(section);
}

} // namespace

void registerTxSectionContent(ContainerContentRegistry& registry, TxApplet* txApplet)
{
    if (!txApplet) {
        return;
    }

    // The MON row on its own: the MON toggle, the SPEAKERS / PHONES output
    // pair, the notice that says why MON is silent, and the monitor volume
    // slider.  The applet makes the copy and wires it, so it behaves exactly
    // as the row inside the TX panel does.
    registry.registerWidgetFactory(
        QLatin1String(kMonitorType),
        {QLatin1String(kMonitorType), QStringLiteral("TX Monitor"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createMonitorSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createMonitorSection(nullptr, true)));

    // The power rows: the forward-power and SWR gauges, the RF Power row and
    // the Tune Power row.
    registry.registerWidgetFactory(
        QLatin1String(kPowerType),
        {QLatin1String(kPowerType), QStringLiteral("TX Power"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createPowerSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createPowerSection(nullptr, true)));

    // The TX bandpass filter rows: the low and high cutoff spinboxes and the
    // filter status label.
    registry.registerWidgetFactory(
        QLatin1String(kFilterType),
        {QLatin1String(kFilterType), QStringLiteral("TX Filter"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createFilterSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createFilterSection(nullptr, true)));

    // The TX chain rows: the LEV, EQ and CFC buttons and the TX Profile
    // combo.
    registry.registerWidgetFactory(
        QLatin1String(kChainType),
        {QLatin1String(kChainType), QStringLiteral("TX Chain"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createChainSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createChainSection(nullptr, true)));

    // The VOX rows: the [VOX] toggle, the threshold and hold sliders, their
    // labels and the peak meter.
    registry.registerWidgetFactory(
        QLatin1String(kVoxType),
        {QLatin1String(kVoxType), QStringLiteral("TX VOX"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createVoxSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createVoxSection(nullptr, true)));

    // The processing rows: the 2-TONE, PS-A and DUP buttons and the SWR
    // protection LED.
    registry.registerWidgetFactory(
        QLatin1String(kProcessingType),
        {QLatin1String(kProcessingType), QStringLiteral("TX Processing"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createProcessingSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createProcessingSection(nullptr, true)));

    // The keying rows: the TUNE and MOX buttons, the transmit-slice row, the
    // holder label and the take-transmit button.  The slice indicators keep
    // their key, hideSliceIndicators, and their place first in the list.
    registry.registerWidgetFactory(
        QLatin1String(kKeyingType),
        {QLatin1String(kKeyingType), QStringLiteral("TX Keying"), false, true, {}},
        [txApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return withParts(txApplet->createKeyingSection(parent, isPreview(mode)), entry);
        },
        partProperties(txApplet->createKeyingSection(nullptr, true), keyingProperties()));
}

} // namespace NereusSDR
