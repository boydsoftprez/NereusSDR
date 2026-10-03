// no-port-check: NereusSDR-original. R-R3-38 saved trust for untrusted discovery.
#pragma once
#include "core/session/StationLanAnnouncement.h"
#include "gui/CoreTargetStore.h"

namespace NereusSDR {
// A matching advertisement only identifies candidate saved credentials. The
// subsequent TLS handshake must prove the pin before any token is sent.
QList<SavedCoreTarget> matchingSavedCores(const StationLanEndpoint& endpoint,
                                         const QList<SavedCoreTarget>& saved);
}
