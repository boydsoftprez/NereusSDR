// no-port-check: NereusSDR-original.
// Regression for the actual patched libjuice poll worker and registry.
// Modification history (NereusSDR):
//   2026-10-03: J.J. Boyd (KG4VCF), AI-assisted implementation via OpenAI Codex.
#include <QtTest>
#include "fakes/juice-poll/Hooks.h"
#include <juice/juice.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
extern "C" const void *nereus_juice_poll_test_registry(juice_agent_t *);
extern "C" int nereus_juice_poll_test_interrupt(juice_agent_t *);
using namespace std::chrono_literals;
namespace {
struct Counters {
    std::array<unsigned, 8> counts{};
    long preparedSockets = -1;
    long descriptorCount = -1;
    long waitMs = -1;
    long finishedState = -1;
    long retainedAgents = -1;
};
struct Fixture {
    std::mutex mutex;
    std::condition_variable changed;
    std::map<const void *, Counters> counters;
    bool barrierArmed = false;
    bool barrierHeld = false;
    bool barrierReleased = false;
    const void *errorAgent = nullptr;
};
Fixture &fixture()
{
    static Fixture state;
    return state;
}
void resume()
{
    auto &state = fixture();
    std::lock_guard lock(state.mutex);
    state.barrierReleased = true;
    state.barrierArmed = false;
    state.changed.notify_all();
}
struct AgentDeleter {
    void operator()(juice_agent_t *agent) const
    {
        // A failed assertion must release the test barrier before joining.
        resume();
        juice_destroy(agent);
    }
};
using Agent = std::unique_ptr<juice_agent_t, AgentDeleter>;
Agent create()
{
    juice_config_t config{};
    config.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;
    config.bind_address = "127.0.0.1";
    Agent agent(juice_create(&config));
    if (agent) {
        auto &state = fixture();
        std::lock_guard lock(state.mutex);
        state.counters[agent.get()] = {};
    }
    return agent;
}
Counters snapshot(const void *object)
{
    auto &state = fixture();
    std::lock_guard lock(state.mutex);
    return state.counters[object];
}
bool waitForEvent(const void *object, NereusJuicePollEvent event)
{
    auto &state = fixture();
    std::unique_lock lock(state.mutex);
    return state.changed.wait_for(lock, 1500ms, [&] {
        return state.counters[object].counts[event] > 0;
    });
}
void armError(const void *agent)
{
    auto &state = fixture();
    std::lock_guard lock(state.mutex);
    state.errorAgent = agent;
}
}
extern "C" void nereus_juice_poll_test_event(const void *object, NereusJuicePollEvent event,
                                            long a, long b)
{
    auto &state = fixture();
    std::lock_guard lock(state.mutex);
    auto &values = state.counters[object];
    ++values.counts[event];
    if (event == NereusJuicePrepared) {
        values.preparedSockets = a;
        values.descriptorCount = b;
    } else if (event == NereusJuicePollEntering) {
        values.waitMs = a;
    } else if (event == NereusJuiceSocketErrorFinished) {
        values.finishedState = a;
        values.retainedAgents = b;
    }
    state.changed.notify_all();
}
extern "C" int nereus_juice_poll_test_error(const void *agent)
{
    auto &state = fixture();
    std::lock_guard lock(state.mutex);
    if (state.errorAgent != agent) {
        return 0;
    }
    state.errorAgent = nullptr;
    return 1;
}
extern "C" void nereus_juice_poll_test_barrier(const void *)
{
    auto &state = fixture();
    std::unique_lock lock(state.mutex);
    if (!state.barrierArmed) {
        return;
    }
    state.barrierArmed = false;
    state.barrierHeld = true;
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return state.barrierReleased; });
}
class TstJuicePollLiveness : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        juice_set_log_level(JUICE_LOG_LEVEL_NONE);
    }
    void init()
    {
        auto &state = fixture();
        std::lock_guard lock(state.mutex);
        state.counters.clear();
        state.barrierArmed = false;
        state.barrierHeld = false;
        state.barrierReleased = false;
        state.errorAgent = nullptr;
    }
    void healthyAnchorSnapshotMismatchRecovers()
    {
        auto anchor = create();
        QVERIFY(anchor);
        QCOMPARE(juice_gather_candidates(anchor.get()), 0);
        QVERIFY(waitForEvent(anchor.get(), NereusJuiceBookkeeping));
        {
            auto &state = fixture();
            std::lock_guard lock(state.mutex);
            state.barrierArmed = true;
            state.barrierReleased = false;
        }
        QCOMPARE(nereus_juice_poll_test_interrupt(anchor.get()), 0);
        {
            auto &state = fixture();
            std::unique_lock lock(state.mutex);
            QVERIFY(state.changed.wait_for(lock, 1500ms, [&] { return state.barrierHeld; }));
        }
        auto next = create();
        QVERIFY(next);
        QCOMPARE(juice_gather_candidates(next.get()), 0);
        QCOMPARE(nereus_juice_poll_test_registry(next.get()),
                 nereus_juice_poll_test_registry(anchor.get()));
        QCOMPARE(snapshot(next.get()).counts[NereusJuiceBookkeeping], 0u);
        resume();
        QVERIFY(waitForEvent(next.get(), NereusJuiceBookkeeping));
        QVERIFY(snapshot(next.get()).counts[NereusJuiceSnapshotMismatch] > 0);
    }
    void retainedFinishedPredecessorMustServiceNewAgent()
    {
        auto old = create();
        QVERIFY(old);
        QCOMPARE(juice_gather_candidates(old.get()), 0);
        QVERIFY(waitForEvent(old.get(), NereusJuiceBookkeeping));
        const void *registry = nereus_juice_poll_test_registry(old.get());
        QVERIFY(registry);
        armError(old.get());
        QCOMPARE(nereus_juice_poll_test_interrupt(old.get()), 0);
        QVERIFY(waitForEvent(old.get(), NereusJuiceSocketErrorFinished));
        QCOMPARE(juice_get_state(old.get()), JUICE_STATE_FAILED);
        QCOMPARE(snapshot(old.get()).finishedState, 2L); // vendor CONN_STATE_FINISHED
        QCOMPARE(snapshot(old.get()).retainedAgents, 1L);
        // The failed predecessor is still owned. No release/restart occurs.
        auto next = create();
        QVERIFY(next);
        QCOMPARE(juice_gather_candidates(next.get()), 0);
        QCOMPARE(nereus_juice_poll_test_registry(next.get()), registry);
        QVERIFY2(waitForEvent(next.get(), NereusJuiceBookkeeping),
                 "A new agent in a retained registry must receive due bookkeeping.");
        QCOMPARE(snapshot(registry).counts[NereusJuiceWorkerExited], 0u);
        QCOMPARE(juice_get_state(old.get()), JUICE_STATE_FAILED);
    }
    void interruptOnlyWaitDoesNotSpinAndLastFailedActorJoins()
    {
        auto old = create();
        QVERIFY(old);
        QCOMPARE(juice_gather_candidates(old.get()), 0);
        QVERIFY(waitForEvent(old.get(), NereusJuiceBookkeeping));
        const void *registry = nereus_juice_poll_test_registry(old.get());
        armError(old.get());
        QCOMPARE(nereus_juice_poll_test_interrupt(old.get()), 0);
        QVERIFY(waitForEvent(old.get(), NereusJuiceSocketErrorFinished));
        auto &state = fixture();
        {
            std::unique_lock lock(state.mutex);
            QVERIFY(state.changed.wait_for(lock, 1500ms, [&] {
                const auto &current = state.counters[registry];
                return current.preparedSockets == 0 && current.descriptorCount == 1
                    && current.waitMs > 0;
            }));
            const unsigned turns = state.counters[registry].counts[NereusJuicePollReturned];
            // An interrupt-only worker should sleep rather than rebuild/spin.
            QVERIFY(!state.changed.wait_for(lock, 200ms, [&] {
                return state.counters[registry].counts[NereusJuicePollReturned] != turns;
            }));
        }
        QCOMPARE(snapshot(registry).counts[NereusJuiceWorkerExited], 0u);
        old.reset(); // real destruction wakes, removes, empties and joins
        QCOMPARE(snapshot(registry).counts[NereusJuiceWorkerExited], 1u);
        QCOMPARE(snapshot(registry).counts[NereusJuiceWorkerJoined], 1u);
    }
    void normalCloseAndLastActorDestructionJoinWorker()
    {
        auto agent = create();
        QVERIFY(agent);
        QCOMPARE(juice_gather_candidates(agent.get()), 0);
        QVERIFY(waitForEvent(agent.get(), NereusJuiceBookkeeping));
        const void *registry = nereus_juice_poll_test_registry(agent.get());
        QCOMPARE(juice_begin_close(agent.get()), 0);
        QTRY_COMPARE_WITH_TIMEOUT(juice_close_status(agent.get()), JUICE_CLOSE_COMPLETE, 1500);
        agent.reset();
        QCOMPARE(snapshot(registry).counts[NereusJuiceWorkerExited], 1u);
        QCOMPARE(snapshot(registry).counts[NereusJuiceWorkerJoined], 1u);
        // A subsequent empty-registry creation gets an ordinary live worker.
        auto replacement = create();
        QVERIFY(replacement);
        QCOMPARE(juice_gather_candidates(replacement.get()), 0);
        QVERIFY(waitForEvent(replacement.get(), NereusJuiceBookkeeping));
    }
};
QTEST_GUILESS_MAIN(TstJuicePollLiveness)
#include "tst_juice_poll_liveness.moc"
