// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys first-look fixes.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// macOS only. Calls back when any menu in this process begins tracking, the
// Apple menu among them, which has no QMenu to watch.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QObject>

#include <functional>

namespace NereusSDR {

class NEREUS_GUI_EXPORT MacMenuTrackingWatch : public QObject {
public:
    MacMenuTrackingWatch(std::function<void()> onTrackingBegan, QObject* parent);
    ~MacMenuTrackingWatch() override;

    static void postForTest();

private:
    const void* m_observer{nullptr};
};

} // namespace NereusSDR
