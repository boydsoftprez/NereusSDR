#pragma once
// no-port-check: NereusSDR-original raw-first legacy presentation importer.
#include "ContainerDocument.h"
#include <QByteArray>
namespace NereusSDR {
class AppSettings;
class LegacyContainerImporter {
public:
    static DocumentResult fromSettings(const AppSettings& settings);
    static DocumentResult fromContainerFile(const QByteArray& bytes);
    static DocumentResult fromClipboard(const QString& text);
};
} // namespace NereusSDR
