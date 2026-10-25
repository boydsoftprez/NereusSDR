#pragma once

// =================================================================
// src/gui/applets/RxSectionContent.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file. It offers the RX applet's jobs
// to the container as content the operator can place on their own. It
// contains no Thetis code and derives from no Thetis source.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23 - The RX applet's jobs as independent container content.
//                 The applet stays the wiring authority. Brian Sutton,
//                 AI-assisted via xAI Grok.
// =================================================================

namespace NereusSDR {

class ContainerContentRegistry;
class RxApplet;

// The RX applet's jobs, offered to the container as content the operator
// can place on their own. Each entry is a fresh copy, made and wired by
// the applet. The applet's own panel is unchanged.
void registerRxSectionContent(ContainerContentRegistry& registry, RxApplet* rxApplet);

} // namespace NereusSDR
