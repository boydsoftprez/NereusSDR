// NereusSDR for iOS: hold the native teardown queue in lifetime regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#include "PeerLifetimeTestSupport.h"
#include "impl/processor.hpp"

namespace {
struct Gate {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool released = false;
};
using Handle = std::shared_ptr<Gate>;
}

void *nereusTestHoldPeerTeardown(void) {
    auto gate = std::make_shared<Gate>();
    rtc::impl::TearDownProcessor::Instance().enqueue([gate] {
        std::unique_lock lock(gate->mutex);
        gate->entered = true;
        gate->condition.notify_all();
        gate->condition.wait(lock, [&] { return gate->released; });
    });
    std::unique_lock lock(gate->mutex);
    if (!gate->condition.wait_for(lock, std::chrono::seconds(10), [&] { return gate->entered; })) {
        gate->released = true;
        gate->condition.notify_all();
        return nullptr;
    }
    return std::make_unique<Handle>(std::move(gate)).release();
}

void nereusTestReleasePeerTeardown(void *pointer) {
    if (!pointer) {
        return;
    }
    std::unique_ptr<Handle> handle(static_cast<Handle *>(pointer));
    std::lock_guard lock((*handle)->mutex);
    (*handle)->released = true;
    (*handle)->condition.notify_all();
}
