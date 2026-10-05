// no-port-check: NereusSDR-original fixture-only access to actual libjuice.
// Modification history: 2026-10-05, J.J. Boyd (KG4VCF), OpenAI Codex.
#include "agent.h"
#include "conn.h"
void nereus_dtls_startup_connected(juice_agent_t* agent, int state)
{
    conn_lock(agent);
    agent_change_state(agent, (juice_state_t)state);
    conn_unlock(agent);
}
