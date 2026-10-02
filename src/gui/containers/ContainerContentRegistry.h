#pragma once
// no-port-check: NereusSDR-original content catalog and lossless meter adapter.
#include "ContainerDocument.h"
#include <QObject>
#include <QMetaType>
namespace NereusSDR {
class MeterItem;
enum class ContentRenderMode { Live, Preview, Validation };
struct ContentDescriptor {
    QString typeId, title;
    bool singleton = false, available = false;
    QString unavailableReason;
};
class ContainerContentRegistry {
public:
    QVector<ContentDescriptor> descriptors() const;
    ContentEntry makeEntry(const QString& typeId) const;
    QString validateEntry(const ContentEntry& entry) const;
    MeterItem* createMeterItem(const ContentEntry& entry, QObject* parent,
                              ContentRenderMode mode = ContentRenderMode::Live) const;
    ContentEntry captureMeterItem(const MeterItem& item, const ContentEntry& prior = {}) const;
};
} // namespace NereusSDR
Q_DECLARE_METATYPE(NereusSDR::ContentEntry)
