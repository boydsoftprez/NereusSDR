#pragma once
// no-port-check: NereusSDR-original lossless presentation JSON codec.
#include "ContainerDocument.h"
#include <QByteArray>

namespace NereusSDR {
class ContainerDocumentCodec {
public:
    // Extensions are unknown JSON members at their owning object's level.
    // Reserved-field collisions are invalid rather than silently discarded.
    static DocumentResult decode(const QByteArray& json);
    // Encode validated values; revision uses a decimal string to retain 64 bits.
    static QByteArray encode(const WorkspaceDocument& document);
    static QByteArray exportContainer(const ContainerDocument& container);
    static DocumentResult importContainer(const QByteArray& bytes);
    static QString exportEntries(const QVector<ContentEntry>& entries);
    static DocumentResult importEntries(const QString& text);
    static void retainPortableRecovery(QJsonObject& extensions, const QJsonObject& record);
    static QString validate(const WorkspaceDocument& document);
};
} // namespace NereusSDR
