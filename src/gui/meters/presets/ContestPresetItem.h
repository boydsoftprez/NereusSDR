// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class ContestPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit ContestPresetItem(QObject* parent=nullptr);
};
}
