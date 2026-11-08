// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys first-look fixes.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/MacMenuTrackingWatch.h"

#import <AppKit/AppKit.h>

#include <utility>

namespace NereusSDR {

MacMenuTrackingWatch::MacMenuTrackingWatch(std::function<void()> onTrackingBegan, QObject* parent)
    : QObject(parent)
{
    // No queue: the block runs in the post, on the main thread. The main queue
    // would wait out the very tracking this is here to notice.
    const std::function<void()> callback = std::move(onTrackingBegan);
    id observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSMenuDidBeginTrackingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification*) {
                    callback();
                }];
    m_observer = CFBridgingRetain(observer);
}

MacMenuTrackingWatch::~MacMenuTrackingWatch()
{
    id observer = CFBridgingRelease(m_observer);
    [[NSNotificationCenter defaultCenter] removeObserver:observer];
}

void MacMenuTrackingWatch::postForTest()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:NSMenuDidBeginTrackingNotification
                                                        object:nil];
}

} // namespace NereusSDR
