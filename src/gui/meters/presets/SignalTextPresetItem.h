// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
#pragma once
#include "CompositePresetItem.h"
namespace NereusSDR {
class SignalTextPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit SignalTextPresetItem(QObject* parent=nullptr);
};
}
