// no-port-check: Explicit optional signal bar, never the primary needle.
#pragma once
#include "BarPresetItem.h"
namespace NereusSDR {
class SMeterPresetItem : public BarPresetItem {
    Q_OBJECT
public:
    explicit SMeterPresetItem(QObject* parent=nullptr);
};
}
