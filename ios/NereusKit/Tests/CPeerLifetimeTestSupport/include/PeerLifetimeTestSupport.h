// NereusSDR for iOS: native teardown ordering test support
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Returns after the queue is held, or null after a bounded test failure.
void *nereusTestHoldPeerTeardown(void);
void nereusTestReleasePeerTeardown(void *gate);
#ifdef __cplusplus
}
#endif
