// no-port-check: NereusSDR-original. R-R3-38 station selection.
#include "gui/StationStartupSelection.h"

#include "gui/CoreTargetStore.h"

#include <QUrl>

namespace NereusSDR {

std::optional<StationStartupSelection> resolveStationStartup(
    const StationStartupRequest& request, const CoreTargetStore& store, QString* error)
{
    if (error) { error->clear(); }
    const auto fail = [error](const QString& message)
        -> std::optional<StationStartupSelection> {
        if (error) { *error = message; }
        return std::nullopt;
    };
    const bool hasCredentials = request.tokenSpecified || request.fingerprintSpecified
        || request.allowUnpinnedSpecified;
    if (request.local) {
        if (request.stationSpecified || hasCredentials) {
            return fail(QStringLiteral("--local cannot be combined with --station, "
                                       "--station-fingerprint, --station-allow-unpinned "
                                       "or --token."));
        }
        return StationStartupSelection{};
    }

    StationStartupSelection result;
    const std::optional<SavedCoreTarget> saved = store.target(store.selectedId());
    if (request.stationSpecified) {
        if (!RemoteStationOptions::isValidStationUrl(request.connection.url)) {
            return fail(QStringLiteral("Core address must be a valid ws:// or wss:// URL."));
        }
        // Only the selected record may supply credentials. Do not search all
        // records by host: two identities can deliberately share an endpoint.
        if (saved && QUrl(saved->connection.url, QUrl::StrictMode)
                         == QUrl(request.connection.url, QUrl::StrictMode)) {
            result.connection = saved->connection;
            result.savedId = saved->id;
        } else {
            result.savedId.clear();
        }
        result.connection.url = request.connection.url;
    } else if (saved) {
        result.connection = saved->connection;
        result.savedId = saved->id;
    }

    if (result.connection.isRemote() && !result.connection.isValidRemoteTarget()) {
        return fail(QStringLiteral("The selected Core needs a valid address or paired remote access route."));
    }
    if (hasCredentials && result.connection.url.isEmpty() && result.connection.isRemote()) {
        return fail(QStringLiteral("A Core reached through remote access cannot use a token or certificate override."));
    }

    if (hasCredentials && !result.connection.isRemote()) {
        return fail(QStringLiteral("Core credentials require a remote Core target."));
    }
    if (request.tokenSpecified) { result.connection.token = request.connection.token; }
    if (request.fingerprintSpecified) {
        result.connection.fingerprint = request.connection.fingerprint;
        // An explicit pin must not inherit a saved bench bypass.
        result.connection.allowUnpinned = false;
    }
    if (request.allowUnpinnedSpecified) { result.connection.allowUnpinned = true; }
    return result;
}

bool shouldStartStationConnection(const StationStartupRequest& request,
                                  const StationStartupSelection& selection,
                                  const CoreTargetStore& store)
{
    if (request.local || request.stationSpecified || !selection.connection.isRemote()) {
        return true;
    }
    const auto saved = store.target(selection.savedId);
    return !saved || saved->autoConnect;
}

} // namespace NereusSDR
