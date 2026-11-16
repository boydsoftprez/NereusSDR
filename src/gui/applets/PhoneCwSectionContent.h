#pragma once

// =================================================================
// src/gui/applets/PhoneCwSectionContent.h  (NereusSDR)
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

namespace NereusSDR {

class ContainerContentRegistry;
class PhoneCwApplet;

// One copy of the Phone/CW panel, placed on its own. The copy hides each
// row, and the Phone and CW buttons, from its entry. The docked panel is
// unchanged.
void registerPhoneCwSectionContent(ContainerContentRegistry& registry, PhoneCwApplet* phoneCwApplet);

} // namespace NereusSDR
