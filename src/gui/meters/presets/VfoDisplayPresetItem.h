// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class VfoDisplayPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit VfoDisplayPresetItem(QObject* parent=nullptr);
};
}
