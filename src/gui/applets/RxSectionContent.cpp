// =================================================================
// src/gui/applets/RxSectionContent.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file. It offers the RX applet's jobs
// to the container as content the operator can place on their own. It
// contains no Thetis code and derives from no Thetis source.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - A preview (ContentRenderMode::Preview) of a job is inert,
//                 as the TX rows are; the color defaults come from
//                 NereusButtonFace.h. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-23 - RX Filter settings: hide the width readout, hide the
//                 passband. Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-23 - The RX applet's jobs as independent container content.
//                 The applet stays the wiring authority. Brian Sutton,
//                 AI-assisted via xAI Grok.
// =================================================================

#include "RxSectionContent.h"

#include "RxApplet.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/NereusButtonFace.h"

namespace NereusSDR {

namespace {

inline constexpr auto kAntennas = "rx.antennas";
inline constexpr auto kMode = "rx.mode";
inline constexpr auto kStep = "rx.step";
inline constexpr auto kFilter = "rx.filter";
inline constexpr auto kPan = "rx.pan";
inline constexpr auto kSquelch = "rx.squelch";
inline constexpr auto kAttenuator = "rx.attenuator";
inline constexpr auto kAgc = "rx.agc";
inline constexpr auto kRit = "rx.rit";
inline constexpr auto kXit = "rx.xit";
inline constexpr auto kHideTop = "hideTop";
inline constexpr auto kHideBottom = "hideBottom";

ContentProperty colorProperty(const char* key, const QString& label, const char* hex)
{
    ContentProperty property;
    property.key = QLatin1String(key);
    property.label = label;
    property.kind = ContentProperty::Kind::Color;
    property.defaultValue = QString::fromLatin1(hex);
    return property;
}

QVector<ContentProperty> filterProperties()
{
    ContentProperty top;
    top.key = QLatin1String(kHideTop);
    top.label = QStringLiteral("Hide width label");
    top.kind = ContentProperty::Kind::Toggle;
    top.defaultValue = false;

    ContentProperty bottom;
    bottom.key = QLatin1String(kHideBottom);
    bottom.label = QStringLiteral("Hide passband");
    bottom.kind = ContentProperty::Kind::Toggle;
    bottom.defaultValue = false;

    // The Nereus band button Columns spin: 1 through 32.
    ContentProperty columns;
    columns.key = QStringLiteral("columns");
    columns.label = QStringLiteral("Columns");
    columns.kind = ContentProperty::Kind::Integer;
    columns.minimum = 1;
    columns.maximum = 32;
    columns.defaultValue = 3;

    return {top, bottom, columns,
            colorProperty("buttonText", QStringLiteral("Button text"), NereusButtonFace::kText),
            colorProperty("backgroundTop", QStringLiteral("Background top"),
                          NereusButtonFace::kRestingTop),
            colorProperty("backgroundBottom", QStringLiteral("Background bottom"),
                          NereusButtonFace::kRestingBottom),
            colorProperty("selectedTop", QStringLiteral("Selected top"),
                          NereusButtonFace::kSelectedTop),
            colorProperty("selectedBottom", QStringLiteral("Selected bottom"),
                          NereusButtonFace::kSelectedBottom)};
}

// Any mode but Live builds an inert copy (ContainerContentRegistry.h:
// "In ContentRenderMode::Preview the widget is inert"): it is not wired to
// the slice and is not one of the applet's copies.
bool isPreview(ContentRenderMode mode)
{
    return mode != ContentRenderMode::Live;
}

} // namespace

void registerRxSectionContent(ContainerContentRegistry& registry, RxApplet* rxApplet)
{
    if (!rxApplet) {
        return;
    }

    registry.registerWidgetFactory(
        QLatin1String(kAntennas),
        {QLatin1String(kAntennas), QStringLiteral("RX Antennas"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createAntennasSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kMode),
        {QLatin1String(kMode), QStringLiteral("RX Mode"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createModeSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kStep),
        {QLatin1String(kStep), QStringLiteral("RX Step"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createStepSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kFilter),
        {QLatin1String(kFilter), QStringLiteral("RX Filter"), false, true, {}},
        [rxApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createFilterSection(parent, entry.config, isPreview(mode));
        },
        filterProperties());
    registry.registerWidgetFactory(
        QLatin1String(kPan),
        {QLatin1String(kPan), QStringLiteral("RX Pan"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createPanSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kSquelch),
        {QLatin1String(kSquelch), QStringLiteral("RX Squelch"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createSquelchSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kAttenuator),
        {QLatin1String(kAttenuator), QStringLiteral("RX Attenuator"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createAttenuatorSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kAgc),
        {QLatin1String(kAgc), QStringLiteral("RX AGC"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createAgcSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kRit),
        {QLatin1String(kRit), QStringLiteral("RX RIT"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createRitSection(parent, isPreview(mode));
        });
    registry.registerWidgetFactory(
        QLatin1String(kXit),
        {QLatin1String(kXit), QStringLiteral("RX XIT"), false, true, {}},
        [rxApplet](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return rxApplet->createXitSection(parent, isPreview(mode));
        });
}

} // namespace NereusSDR
