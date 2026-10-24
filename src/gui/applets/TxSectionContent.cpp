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
//   2026-10-21 - A preview of a row (ContentRenderMode::Preview) is inert.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

namespace NereusSDR {

namespace {

inline constexpr auto kMonitorType = "tx.monitor";
inline constexpr auto kPowerType = "tx.power";
inline constexpr auto kFilterType = "tx.filter";
inline constexpr auto kChainType = "tx.chain";
inline constexpr auto kVoxType = "tx.vox";
inline constexpr auto kProcessingType = "tx.processing";
inline constexpr auto kKeyingType = "tx.keying";

// Any mode but Live builds an inert row (ContainerContentRegistry.h:
// "In ContentRenderMode::Preview the widget is inert"): it is not wired to
// the model and is not one of the applet's rows.
bool isPreview(ContentRenderMode mode)
{
    return mode != ContentRenderMode::Live;
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
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createMonitorSection(parent, isPreview(mode)));
        });

    // The power rows: the forward-power and SWR gauges, the RF Power row and
    // the Tune Power row.
    registry.registerWidgetFactory(
        QLatin1String(kPowerType),
        {QLatin1String(kPowerType), QStringLiteral("TX Power"), false, true, {}},
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createPowerSection(parent, isPreview(mode)));
        });

    // The TX bandpass filter rows: the low and high cutoff spinboxes and the
    // filter status label.
    registry.registerWidgetFactory(
        QLatin1String(kFilterType),
        {QLatin1String(kFilterType), QStringLiteral("TX Filter"), false, true, {}},
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createFilterSection(parent, isPreview(mode)));
        });

    // The TX chain rows: the LEV, EQ and CFC buttons and the TX Profile
    // combo.
    registry.registerWidgetFactory(
        QLatin1String(kChainType),
        {QLatin1String(kChainType), QStringLiteral("TX Chain"), false, true, {}},
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createChainSection(parent, isPreview(mode)));
        });

    // The VOX rows: the [VOX] toggle, the threshold and hold sliders, their
    // labels and the peak meter.
    registry.registerWidgetFactory(
        QLatin1String(kVoxType),
        {QLatin1String(kVoxType), QStringLiteral("TX VOX"), false, true, {}},
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createVoxSection(parent, isPreview(mode)));
        });

    // The processing rows: the 2-TONE, PS-A and DUP buttons and the SWR
    // protection LED.
    registry.registerWidgetFactory(
        QLatin1String(kProcessingType),
        {QLatin1String(kProcessingType), QStringLiteral("TX Processing"), false, true, {}},
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createProcessingSection(parent, isPreview(mode)));
        });

    // The keying rows: the TUNE and MOX buttons, the transmit-slice row, the
    // holder label and the take-transmit button.
    registry.registerWidgetFactory(
        QLatin1String(kKeyingType),
        {QLatin1String(kKeyingType), QStringLiteral("TX Keying"), false, true, {}},
        [txApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return static_cast<QWidget*>(txApplet->createKeyingSection(parent, isPreview(mode)));
        });
}

} // namespace NereusSDR
