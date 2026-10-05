// no-port-check: NereusSDR-original test-only access; no shipping target uses it.
// Modification history (NereusSDR): 2026-10-03, J.J. Boyd (KG4VCF), OpenAI Codex.
#include "agent.h"
#include "conn.h"
// Creation/gathering has already initialized the connection synchronously.
// The test owns the agent for the entire lifetime of the returned registry.
const void *nereus_juice_poll_test_registry(juice_agent_t *agent) {
    return agent->registry;
}
int nereus_juice_poll_test_interrupt(juice_agent_t *agent) {
    return conn_interrupt(agent);
}
