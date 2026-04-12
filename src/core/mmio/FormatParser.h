#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QUuid>
#include <QVariant>

namespace NereusSDR {

// Phase 3G-6 block 5 — format-driven parsers for MMIO transport
// payloads. Each function consumes a raw payload and returns a
// flat key->value map where keys are dotted paths prefixed with
// the endpoint's guid string, matching Thetis's discovery scheme
// from MeterManager.cs:41325-41422.
//
// Keys look like "550e8400-e29b-41d4-a716-446655440000.gateway.temperature".
// The guid prefix keeps variables from different endpoints distinct
// inside the shared per-endpoint cache.
//
// These are free functions with no class; the functional form keeps
// parsers side-effect-free and trivially testable.
namespace FormatParser {

// JSON recursive descent. Every leaf node becomes a key whose path
// reflects nested object / array structure. Numbers become double /
// int, bool becomes bool, strings become QString. Null values are
// skipped. From Thetis MeterManager.cs:41325-41402.
QHash<QString, QVariant> parseJson(const QByteArray& payload, const QUuid& endpointGuid);

// XML element walk. Element text content and attributes become keys.
// From Thetis MeterManager.cs:41404-41422.
QHash<QString, QVariant> parseXml(const QByteArray& payload, const QUuid& endpointGuid);

// RAW colon-delimited key:value pairs. Values are stored as QString.
// Numeric auto-detection happens downstream if the consumer needs it.
// From Thetis MeterManager.cs:41344-41349.
QHash<QString, QVariant> parseRaw(const QByteArray& payload, const QUuid& endpointGuid);

} // namespace FormatParser

} // namespace NereusSDR
