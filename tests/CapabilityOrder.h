#pragma once
// tests/CapabilityOrder.h
//
// no-port-check: NereusSDR-original test helper.
//
// Where an entry sits on the capabilities list a peer was sent, by name. A
// count back from the end of the list breaks each time an entry is added
// that every peer gets, and says nothing about what moved. These checks say
// what the old counts meant:
//
//   - A peer-gated entry is appended after the older ones, so no entry a peer
//     already reads moves: exactly the entries in
//     kCapabilitiesAfterPeerGated follow the last one a peer declared.
//   - A peer that did not declare it gets the same list without it, compared
//     as encoded bytes.
//
// A new entry that every peer gets, sent after the peer-gated block, is added
// to kCapabilitiesAfterPeerGated here and to no test. A new peer-gated entry
// changes nothing here, because a test's peer has not declared it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05  J.J. Boyd / KG4VCF  Capability order by name, after a new
//                                    entry broke the counts back from the
//                                    end. AI-assisted via Anthropic Claude
//                                    Code.

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

// What follows the peer-gated block for every minor-11 peer, in wire order.
inline const QStringList kCapabilitiesAfterPeerGated{QStringLiteral("txSsbModMonitorVersion")};

// The same for a peer that declared coreBuildInfo on a Core that knows its
// version. coreBuildInfo stays last.
inline const QStringList kCapabilitiesAfterPeerGatedWithBuildInfo{
    QStringLiteral("txSsbModMonitorVersion"), QStringLiteral("coreBuildInfo")};

// The names on a capabilities message's `properties`, in wire order.
inline QStringList capabilityEntryNames(const QJsonArray& entries)
{
    QStringList names;
    for (const QJsonValue& entry : entries) {
        names.append(entry.toObject().value(QStringLiteral("name")).toString());
    }
    return names;
}

// The names after `entry`, in wire order. A list without `entry` gives a value
// no expected list holds, so the comparison fails and names what was missing.
inline QStringList capabilityEntriesAfter(const QStringList& names, const QString& entry)
{
    const qsizetype at = names.indexOf(entry);
    if (at < 0) {
        return {QStringLiteral("<no entry named %1>").arg(entry)};
    }
    return names.mid(at + 1);
}

// `entries` as encoded bytes, leaving out the entries named in `dropped`.
inline QByteArray capabilityBytesWithout(const QJsonArray& entries, const QStringList& dropped = {})
{
    QJsonArray kept;
    for (const QJsonValue& entry : entries) {
        if (!dropped.contains(entry.toObject().value(QStringLiteral("name")).toString())) {
            kept.append(entry);
        }
    }
    return QJsonDocument(kept).toJson(QJsonDocument::Compact);
}
