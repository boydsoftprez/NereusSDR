// no-port-check: NereusSDR family adapter.
#include "ClockPresetItem.h"
namespace NereusSDR {
ClockPresetItem::ClockPresetItem(QObject* parent) : CompositePresetItem(Face::Clock,parent) {}
}
