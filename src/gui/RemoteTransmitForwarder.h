#pragma once
// =================================================================
// src/gui/RemoteTransmitForwarder.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. iPhone app plan, the desktop remote
// window's transmit (R-IOS-13, R-R3-42).
//
// The remote window's TCI server forwards a program's transmit to the Core
// (Task 35's TciServer::RemoteTransmit): its key as tx.key
// {trigger:"tci"} and its release as tx.unkey through the window's
// transmit client, and its transmit audio onto the microphone line in
// place of the microphone (Task 36's RemoteMediaController hook). This is
// the one production forwarder; MainWindow installs it while the Core
// takes this window's keys, and the tests install the same one.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - Created for the desktop remote window's transmit
//                (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

#include "core/TciServer.h"

namespace NereusSDR {

class RemoteMediaController;
class RemoteTransmitClient;

/// The forwarder for `transmit` (keys) and `media` (a program's audio; may
/// be null). Both are held weakly: a forwarder outliving either refuses
/// keys and drops audio.
TciServer::RemoteTransmit remoteTransmitForwarder(RemoteTransmitClient* transmit,
                                                  RemoteMediaController* media);

} // namespace NereusSDR
