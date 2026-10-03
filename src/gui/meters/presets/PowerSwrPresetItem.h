// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class PowerSwrPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit PowerSwrPresetItem(QObject* parent=nullptr);
};
}
