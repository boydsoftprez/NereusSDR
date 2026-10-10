// no-port-check: NereusSDR-original causal regression using real pinned RTC.
// Modification history: 2026-10-05, J.J. Boyd (KG4VCF), OpenAI Codex.
// 2026-10-08, J.J. Boyd (KG4VCF), Anthropic Claude Code: the hooks hold the
// gate by shared_ptr, so a case that returns while a processor thread is
// still waking from the gate no longer destroys it under that thread.
#include <QtTest>
#include "fakes/dtls-startup/Access.h"
#include "impl/tls.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
using namespace std::chrono_literals;
using rtc::impl::NereusDtlsStartupTestAccess;
namespace {
struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    rtc::impl::DtlsTransport* transport = nullptr;
    void release() { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
    bool wait() { std::unique_lock lock(mutex); return changed.wait_for(lock, 2s, [&] { return entered; }); }
    // Run by a hook on an RTC worker: records arrival, then blocks until release.
    void pass(rtc::impl::DtlsTransport* observed)
    {
        std::unique_lock lock(mutex);
        transport = observed;
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
    }
};
// The hooks run on RTC workers and copy the gate out under the slot's lock,
// so each keeps its own reference while it waits. release() only wakes the
// worker; under load the worker can still be inside condition_variable::wait
// after the case has returned. A gate owned by the case's stack frame was
// destroyed under it there ("condition_variable wait failed: Invalid
// argument", then abort): 159 of 400 runs at 16 in parallel.
class GateSlot {
public:
    void set(std::shared_ptr<Gate> gate) { std::lock_guard lock(m_mutex); m_gate = std::move(gate); }
    std::shared_ptr<Gate> get() { std::lock_guard lock(m_mutex); return m_gate; }
private:
    std::mutex m_mutex;
    std::shared_ptr<Gate> m_gate;
};
GateSlot startGate;
GateSlot initGate;
std::atomic<unsigned> startCount{0};
std::atomic<bool> throwOnStart{false};
bool drain(const std::shared_ptr<rtc::impl::PeerConnection>& peer)
{
    auto reached = std::make_shared<std::promise<void>>();
    std::future<void> result = reached->get_future();
    NereusDtlsStartupTestAccess::enqueue(peer, [reached] { reached->set_value(); });
    return result.wait_for(3s) == std::future_status::ready;
}
struct ProcessorHold {
    std::shared_ptr<Gate> gate = std::make_shared<Gate>();
    explicit ProcessorHold(const std::shared_ptr<rtc::impl::PeerConnection>& peer)
    {
        NereusDtlsStartupTestAccess::enqueue(peer, [held = gate] {
            std::unique_lock lock(held->mutex);
            held->entered = true;
            held->changed.notify_all();
            held->changed.wait(lock, [&] { return held->released; });
        });
    }
    ~ProcessorHold() { gate->release(); }
};
struct Fixture {
    std::shared_ptr<int> owner = std::make_shared<int>(42);
    std::shared_ptr<rtc::impl::PeerConnection> peer;
    std::shared_ptr<rtc::impl::IceTransport> ice;
    Fixture() {
        rtc::Configuration config;
        config.bindAddress = "127.0.0.1";
        config.iceTransportLifetime = owner;
        peer = std::make_shared<rtc::impl::PeerConnection>(config);
        ice = peer->initIceTransport();
        ice->gatherLocalCandidates("0");
    }
    ~Fixture() { dispose(); }
    void dispose()
    {
        if (peer) { peer->close(); }
        ice.reset();
        peer.reset();
        owner.reset();
    }
    void event(juice_state_t state) { nereus_dtls_startup_connected(NereusDtlsStartupTestAccess::agent(ice), state); }
};
}
void nereusDtlsStartupInit(rtc::impl::PeerConnection*)
{
    if (const std::shared_ptr<Gate> gate = initGate.get()) {
        gate->pass(nullptr);
    }
}
void nereusDtlsStartupStart(rtc::impl::DtlsTransport* transport)
{
    ++startCount;
    if (throwOnStart.exchange(false)) {
        // Exercise the pinned vendor's existing fatal-I/O error path.
        rtc::openssl::check_error(SSL_ERROR_SYSCALL, "fixture DTLS startup failure");
    }
    if (const std::shared_ptr<Gate> gate = startGate.get()) {
        gate->pass(transport);
    }
}
class TstDtlsStartup : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        startCount = 0;
        throwOnStart = false;
        startGate.set(nullptr);
        initGate.set(nullptr);
    }
    void connectedCallbackReturnsWhileSslMutexHeld()
    {
        Fixture fixture;
        const auto gate = std::make_shared<Gate>();
        startGate.set(gate);
        std::promise<void> returned;
        std::future<void> result = returned.get_future();
        std::thread callback([&] { fixture.event(JUICE_STATE_CONNECTED); returned.set_value(); });
        const bool reached = gate->wait();
        bool prompt = false;
        if (reached) {
            std::unique_lock ssl(NereusDtlsStartupTestAccess::sslMutex(gate->transport));
            gate->release();
            prompt = result.wait_for(500ms) == std::future_status::ready;
            // Always release S before joining, including the expected red run.
        } else {
            gate->release();
        }
        callback.join();
        startGate.set(nullptr);
        fixture.peer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!fixture.peer->getDtlsTransport(), 3000);
        QVERIFY2(reached, "Actual OpenSSL DTLS startup must reach the observer.");
        QVERIFY2(prompt, "The ICE Connected callback must return while the real SSL mutex is held.");
    }
    void queuedObsoleteStartupIsSkipped_data()
    {
        QTest::addColumn<int>("event");
        QTest::newRow("closed") << -1;
        QTest::newRow("failed") << int(JUICE_STATE_FAILED);
        QTest::newRow("disconnected") << int(JUICE_STATE_DISCONNECTED);
        QTest::newRow("replaced ICE") << -2;
    }
    void queuedObsoleteStartupIsSkipped()
    {
        QFETCH(int, event);
        Fixture fixture;
        ProcessorHold hold(fixture.peer);
        const bool blocked = hold.gate->wait();
        fixture.event(JUICE_STATE_CONNECTED);
        if (event == -1) {
            fixture.peer->close();
        } else if (event == -2) {
            auto replacement = std::make_shared<rtc::impl::IceTransport>(
                fixture.peer->config, [](const rtc::Candidate&) {},
                [](rtc::impl::Transport::State) {},
                [](rtc::impl::IceTransport::GatheringState) {});
            NereusDtlsStartupTestAccess::replaceIce(fixture.peer, replacement);
        } else {
            fixture.event(juice_state_t(event));
        }
        hold.gate->release();
        const bool progressed = drain(fixture.peer);
        const unsigned starts = startCount.load();
        const bool absent = !fixture.peer->getDtlsTransport();
        std::weak_ptr<int> owner = fixture.owner;
        fixture.dispose();
        QVERIFY(blocked);
        QVERIFY(progressed);
        QCOMPARE(starts, 0u);
        QVERIFY(absent);
        QTRY_VERIFY_WITH_TIMEOUT(owner.expired(), 3000);
    }
    void queuedWorkOwnsPeerUntilProcessorFinishes()
    {
        Fixture fixture;
        ProcessorHold hold(fixture.peer);
        const bool blocked = hold.gate->wait();
        fixture.event(JUICE_STATE_CONNECTED);
        std::weak_ptr<rtc::impl::PeerConnection> peer = fixture.peer;
        std::weak_ptr<rtc::impl::IceTransport> ice = fixture.ice;
        std::weak_ptr<int> owner = fixture.owner;
        fixture.dispose();
        const bool retained = !peer.expired() && !ice.expired() && !owner.expired();
        hold.gate->release();
        QVERIFY(blocked);
        QVERIFY(retained);
        QTRY_VERIFY_WITH_TIMEOUT(peer.expired(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(ice.expired(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(owner.expired(), 3000);
    }
    void completionBeforeQueuedStartupStillStartsOnce()
    {
        Fixture fixture;
        ProcessorHold hold(fixture.peer);
        const bool blocked = hold.gate->wait();
        fixture.event(JUICE_STATE_CONNECTED);
        fixture.event(JUICE_STATE_COMPLETED);
        hold.gate->release();
        const bool progressed = drain(fixture.peer);
        QCOMPARE(fixture.peer->iceState.load(), rtc::PeerConnection::IceState::Completed);
        QVERIFY(blocked);
        QVERIFY(progressed);
        QCOMPARE(startCount.load(), 1u);
        QVERIFY(fixture.peer->getDtlsTransport());
    }
    void closeDuringAdmittedStartup_data()
    {
        QTest::addColumn<bool>("published");
        QTest::newRow("after guard before publication") << false;
        QTest::newRow("after publication before start") << true;
    }
    void closeDuringAdmittedStartup()
    {
        QFETCH(bool, published);
        Fixture fixture;
        const auto gate = std::make_shared<Gate>();
        if (published) {
            startGate.set(gate);
        } else {
            initGate.set(gate);
        }
        fixture.event(JUICE_STATE_CONNECTED);
        const bool reached = gate->wait();
        fixture.peer->close();
        gate->release();
        const bool progressed = drain(fixture.peer);
        startGate.set(nullptr);
        initGate.set(nullptr);
        const bool absent = !fixture.peer->getIceTransport() && !fixture.peer->getDtlsTransport()
                            && !fixture.peer->getSctpTransport();
        const auto state = fixture.peer->state.load();
        const auto iceState = fixture.peer->iceState.load();
        std::weak_ptr<int> owner = fixture.owner;
        fixture.dispose();
        QVERIFY(reached);
        QVERIFY(progressed);
        QVERIFY(absent);
        QCOMPARE(state, rtc::PeerConnection::State::Closed);
        QCOMPARE(iceState, rtc::PeerConnection::IceState::Closed);
        QTRY_VERIFY_WITH_TIMEOUT(owner.expired(), 3000);
    }
    void startupFailureClearsMemberAndAdvancesProcessor()
    {
        Fixture fixture;
        throwOnStart = true;
        fixture.event(JUICE_STATE_CONNECTED);
        const bool progressed = drain(fixture.peer);
        QVERIFY(progressed);
        QCOMPARE(startCount.load(), 1u);
        QCOMPARE(fixture.peer->state.load(), rtc::PeerConnection::State::Failed);
        QVERIFY(!fixture.peer->getDtlsTransport());
    }

};
QTEST_APPLESS_MAIN(TstDtlsStartup)
#include "tst_dtls_startup.moc"
