// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class ClockPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit ClockPresetItem(QObject* parent=nullptr);
};
}
