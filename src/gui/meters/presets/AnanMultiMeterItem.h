// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class AnanMultiMeterItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit AnanMultiMeterItem(QObject* parent=nullptr);
};
}
