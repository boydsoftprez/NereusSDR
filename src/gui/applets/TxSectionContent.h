#pragma once

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
// =================================================================

#include <QString>

namespace NereusSDR {

class ContainerContentRegistry;
class TxApplet;

// The TX applet's rows, offered to the container as content the operator can
// place on their own.
//
// Each entry is a fresh copy of a row, made and wired by the applet, so the
// copy shows and sets the same model state and follows the same
// transmit-permission gates as the applet's own row.
//
// The applet's own panel is unchanged: it still shows every row together.
void registerTxSectionContent(ContainerContentRegistry& registry, TxApplet* txApplet);

} // namespace NereusSDR
