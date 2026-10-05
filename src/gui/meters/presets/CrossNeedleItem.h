// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class CrossNeedleItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit CrossNeedleItem(QObject* parent=nullptr);
};
}
