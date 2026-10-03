// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class MagicEyePresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit MagicEyePresetItem(QObject* parent=nullptr);
};
}
