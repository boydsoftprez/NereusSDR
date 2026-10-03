// no-port-check: NereusSDR family adapter.
#include "SMeterPresetItem.h"
namespace NereusSDR { SMeterPresetItem::SMeterPresetItem(QObject* p):BarPresetItem(p) { configureVariant("SignalAvg"); } }
