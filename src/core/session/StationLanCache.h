// NereusSDR-original in-memory cache for untrusted Core LAN announcements.
#pragma once

#include "StationLanAnnouncement.h"

#include <QList>

namespace NereusSDR {

class StationLanCache {
public:
    static constexpr int kMaxFingerprints = 128;
    static constexpr int kMaxEndpointsPerFingerprint = 8;

    /// Decodes schema 1 or schema 2. For an endpoint (key()) that already
    /// holds schema-2 fields, a schema-1 datagram updates only the fields
    /// schema 1 carries (iPhone app Task 16).
    bool ingest(const QByteArray& bytes, const QHostAddress& source, uint interfaceIndex,
                qint64 nowMs, QString* error = nullptr);
    bool expire(qint64 nowMs);
    QList<StationLanEndpoint> endpoints() const { return m_endpoints; }
    void clear() { m_endpoints.clear(); }

private:
    QList<StationLanEndpoint> m_endpoints;
};

} // namespace NereusSDR
