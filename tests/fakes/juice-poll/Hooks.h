// no-port-check: NereusSDR-original test-only instrumentation.
// Modification history (NereusSDR): 2026-10-03, J.J. Boyd (KG4VCF), OpenAI Codex.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
enum NereusJuicePollEvent {
    NereusJuiceBookkeeping, NereusJuicePrepared, NereusJuicePollEntering,
    NereusJuicePollReturned, NereusJuiceSocketErrorFinished,
    NereusJuiceSnapshotMismatch, NereusJuiceWorkerExited, NereusJuiceWorkerJoined
};
void nereus_juice_poll_test_event(const void *, enum NereusJuicePollEvent, long, long);
int nereus_juice_poll_test_error(const void *);
void nereus_juice_poll_test_barrier(const void *);
#ifdef __cplusplus
}
#endif
