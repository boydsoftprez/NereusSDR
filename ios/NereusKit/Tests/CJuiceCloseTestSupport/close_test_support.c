// NereusSDR for iOS: test-only relay-entry state seam
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include "include/juice_close_test_support.h"
#include "../../Sources/CJuice/src/agent.h"

int juice_test_mark_granted_relay_failed(juice_agent_t *agent) {
	if (!agent)
		return 0;
	int found = 0;
	conn_lock(agent);
	for (int i = 0; i < agent->entries_count; ++i) {
		agent_stun_entry_t *entry = agent->entries + i;
		if (entry->type == AGENT_STUN_ENTRY_TYPE_RELAY && entry->turn && entry->relayed.len > 0) {
			entry->state = AGENT_STUN_ENTRY_STATE_FAILED;
			found = 1;
			break;
		}
	}
	conn_unlock(agent);
	return found;
}

int juice_test_callbacks_cleared(juice_agent_t *agent) {
	if (!agent)
		return 0;
	conn_lock(agent);
	int cleared = !agent->config.cb_state_changed && !agent->config.cb_candidate &&
	              !agent->config.cb_gathering_done && !agent->config.cb_recv &&
	              !agent->config.user_ptr;
	conn_unlock(agent);
	return cleared;
}
