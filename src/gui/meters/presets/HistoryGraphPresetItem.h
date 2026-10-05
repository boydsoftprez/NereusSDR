// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class HistoryGraphPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit HistoryGraphPresetItem(QObject* parent=nullptr);
};
}
