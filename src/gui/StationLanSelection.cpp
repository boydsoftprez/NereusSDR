// no-port-check: NereusSDR-original. R-R3-38 saved trust for untrusted discovery.
#include "gui/StationLanSelection.h"
namespace NereusSDR {
QList<SavedCoreTarget> matchingSavedCores(const StationLanEndpoint& endpoint,
                                         const QList<SavedCoreTarget>& saved)
{
    QList<SavedCoreTarget> matches;
    // Validate even callers other than the UDP decoder. Empty or malformed
    // identity can never select credentials by matching another empty field.
    if (encodeStationLanAnnouncement(endpoint.announcement).isEmpty()) { return matches; }
    const QByteArray& identity = endpoint.announcement.identity;
    for (const SavedCoreTarget& target : saved) {
        // iPhone app Task 18 (R-IOS-08): a Core this computer paired with
        // is found by the identity it announces (schema 2). The connection
        // still proves that identity before anything is sent.
        const bool pairedMatch = !target.connection.identityFingerprint.isEmpty()
            && identity.size() == kStationLanIdentityBytes
            && target.connection.identityFingerprint == identity;
        // A Core saved with its token and pin is found by the pin it
        // announces, as before, whether or not its identity has since been
        // learned (an announcement from before identities carries none).
        const bool pinMatch = !target.connection.token.isEmpty()
            && target.connection.fingerprint.compare(endpoint.announcement.fingerprint,
                                                     Qt::CaseInsensitive) == 0;
        if (pairedMatch || pinMatch) {
            matches.append(target);
        }
    }
    return matches;
}
}
