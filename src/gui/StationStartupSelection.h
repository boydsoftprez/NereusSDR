// no-port-check: NereusSDR-original. R-R3-38 station selection.
#pragma once

#include "core/session/RemoteStationOptions.h"

#include <optional>

namespace NereusSDR {

class CoreTargetStore;

// Presence is distinct from an empty value: --token="" must not restore a
// saved token. Parse flags first, resolve only after AppSettings/store load.
struct StationStartupRequest {
    RemoteStationOptions connection;
    bool stationSpecified = false;
    bool tokenSpecified = false;
    bool fingerprintSpecified = false;
    bool allowUnpinnedSpecified = false;
    bool local = false;
};

struct StationStartupSelection {
    RemoteStationOptions connection;
    // "local", a saved record ID, or empty for a one-launch CLI target.
    QString savedId{QStringLiteral("local")};
    // Only for an explicitly selected discovered endpoint of a saved pinned
    // Core. The stored address remains unchanged; keep it to detect later edits.
    QString savedAddressBeforeDiscovery;
};

// Requires a successfully loaded store. Does not persist CLI overrides.
// A failure is a setup error, never an instruction to auto-connect locally.
std::optional<StationStartupSelection> resolveStationStartup(
    const StationStartupRequest&, const CoreTargetStore&, QString* error = nullptr);

// A saved Core's opt-out applies only to implicit startup. Explicit CLI
// selection and local-radio startup remain direct operator requests.
bool shouldStartStationConnection(const StationStartupRequest&,
                                  const StationStartupSelection&, const CoreTargetStore&);

} // namespace NereusSDR
