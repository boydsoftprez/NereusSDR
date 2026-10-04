// NereusSDR for iOS: test-only relay-entry state seam
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#ifndef NEREUS_JUICE_CLOSE_TEST_SUPPORT_H
#define NEREUS_JUICE_CLOSE_TEST_SUPPORT_H

#include <juice/juice.h>

// Return 1 when a granted relay entry was marked failed, 0 when none was
// present. This models a failed entry that still has an allocation to release.
int juice_test_mark_granted_relay_failed(juice_agent_t *agent);

// Return 1 only when begin-close cleared every callback and the user pointer.
int juice_test_callbacks_cleared(juice_agent_t *agent);

#endif
