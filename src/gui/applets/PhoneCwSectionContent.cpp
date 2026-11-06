// =================================================================
// src/gui/applets/PhoneCwSectionContent.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file. It offers a copy of the Phone/CW
// panel to the container. It contains no Thetis code and derives from no
// Thetis source.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - The Phone/CW panel as container content, with a toggle to
//                 hide each row and the page buttons. Brian Sutton,
//                 AI-assisted via xAI Grok.
// =================================================================

#include "PhoneCwSectionContent.h"

#include "PhoneCwApplet.h"
#include "SectionParts.h"
#include "gui/containers/ContainerContentRegistry.h"

#include <memory>

namespace NereusSDR {

namespace {

inline constexpr auto kType = "phcw.panel";

ContentProperty pageProperty()
{
    ContentProperty page;
    page.key = QStringLiteral("page");
    page.label = QStringLiteral("Page");
    page.kind = ContentProperty::Kind::Choice;
    page.defaultValue = QStringLiteral("phone");
    ContentPropertyChoice phone;
    phone.label = QStringLiteral("Phone");
    phone.value = QStringLiteral("phone");
    ContentPropertyChoice cw;
    cw.label = QStringLiteral("CW");
    cw.value = QStringLiteral("cw");
    page.choices = {phone, cw};
    return page;
}

// The page choice, then a hide toggle for the page buttons and for each
// row. Taken from an inert copy, which is gone on return. No order list:
// the rows sit on two pages, and one order cannot move them.
QVector<ContentProperty> copyProperties(PhoneCwApplet* phoneCwApplet)
{
    QVector<ContentProperty> properties{pageProperty()};
    if (!phoneCwApplet) {
        return properties;
    }
    const std::unique_ptr<PhoneCwApplet> probe(phoneCwApplet->createPanelCopy(nullptr, true));
    const SectionParts* parts = probe ? probe->containerParts() : nullptr;
    if (!parts) {
        return properties;
    }
    for (const QString& id : parts->ids()) {
        ContentProperty hide;
        hide.key = SectionParts::hideKey(id);
        hide.label = QStringLiteral("Hide ") + parts->label(id);
        hide.kind = ContentProperty::Kind::Toggle;
        hide.defaultValue = false;
        properties.append(hide);
    }
    return properties;
}

} // namespace

void registerPhoneCwSectionContent(ContainerContentRegistry& registry, PhoneCwApplet* phoneCwApplet)
{
    if (!phoneCwApplet) {
        return;
    }

    registry.registerWidgetFactory(
        QLatin1String(kType),
        {QLatin1String(kType), QStringLiteral("Phone / CW sections"), false, true, {},
         ContentGroup::PanelSections},
        [phoneCwApplet](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            PhoneCwApplet* copy = phoneCwApplet->createPanelCopy(parent, mode != ContentRenderMode::Live);
            if (copy) {
                copy->applyContainerConfig(entry.config);
            }
            return copy;
        },
        copyProperties(phoneCwApplet));
}

} // namespace NereusSDR
