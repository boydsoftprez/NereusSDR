#pragma once
// =================================================================
// src/core/daemon/DaemonApp.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R1 Task 10
// (docs/architecture/2026-08-02-remote-daemon-r1-plan.md, Task 10;
// design docs/architecture/2026-07-28-remote-daemon-architecture-design.md
// section 9.4): the piece that actually makes nereusd useful. Task 9 gave
// the daemon a config file and a quit path but never touched a radio --
// server_main.cpp just logged the parsed slice count. Every existing
// slice-creation call site is GUI-resident (RadioModel::addSliceOnPan is
// wired from MainWindow's "+RX"/"+PAN" buttons) and
// RadioModel::connectToRadio itself only ever creates ONE slice (the
// no-argument addSlice() call that seeds Slice A, RadioModel.cpp connect
// path), so a headless process that just constructs a RadioModel and
// calls connectToRadio() comes up with Slice A and nothing else --
// exactly the gap this class exists to close.
//
// DaemonApp owns the RadioModel, resolves which radio to talk to (network
// discovery in production, or a primed board in tests -- see
// primeBoardForTest below), connects, and then tops the slice list up to
// min(cfg.sliceCount, connected-board-maxSlices) using RadioModel's own
// addSlice() -- the same entry point connectToRadio() uses for Slice A,
// not the GUI-only addSliceOnPan() (which additionally wants a pan id and
// enforces the cap by REJECTING the add with a sliceAddRejected signal; a
// headless daemon has no toast to show, so it clamps before asking rather
// than asking and handling the rejection).
//
// Endpoint ids, not pan ids (design section 9.4): "A pan is a client
// concept. An endpoint is the wire concept. They are not the same object
// and must not be conflated." Section 9.4's mature model has the CLIENT
// mint the endpoint id at subscribe time, over a wire protocol this task
// does not build. With no remote client yet, this class mints a
// placeholder id per slice it creates so FftTopology has something
// non-pan-shaped to key on; a real client-minted id will replace it once
// the wire protocol exists. Deliberately NOT the "pan-0"-style string
// RadioModel::connectToRadio stamps on Slice A via setPanKey(), which is
// exactly the pan/endpoint conflation section 9.4 calls out.
//
// FftEnginePool (Task 6) is listed among this task's brief-stated
// interfaces but is NOT constructed here: Step 3's own description of
// start() never mentions it, none of this task's tests exercise it, and
// an FftEnginePool with no I/Q ever pushed into it is inert scaffolding
// with no call site -- exactly what CLAUDE.md's ISpectrumSink note warns
// against adding speculatively. Feeding real I/Q into per-stream FFT
// engines needs a tap on RxDspWorker/ReceiverManager this task's brief
// does not describe. Fix round 1 correction: an earlier version of this
// comment named Task 11 (wideband FFT thread) as the natural owner of
// that future FftEnginePool construction. Task 11's actual scope is
// narrowly giving the wideband FFT dispatch its own QThread -- it does
// not construct an FftEnginePool, and no task currently in the R1 plan
// does. The deferral itself still stands; there is just no specific
// task to point future readers at yet.
//
// FIX ROUND 1, FINDING 1: subscribing endpoints in m_topology is only
// half the job -- RadioModel already unconditionally owns a live
// FFTRouter (m_fftRouter = new FFTRouter(this), RadioModel.cpp:794,
// exposed via RadioModel::fftRouter()), and the daemon-minted
// subscriptions have to actually reach it via FftTopology::applyTo(),
// the same way MainWindow::rebuildFftRouting() ends with
// m_topology.applyTo(*router) (MainWindow.cpp:2274). Without that call
// m_topology was 100% inert: subscribed but never pushed anywhere.
// publishFftTopology() below is that call, run from start() (after
// minting) and from clearFftTopology() (see FIX ROUND 2 note below).
//
// FIX ROUND 2, FINDING 1 (reopened): the round 1 fix's stop() half
// replaced m_topology wholesale before calling publishFftTopology(),
// which silently wiped FftTopology's own bookkeeping of what it had
// last pushed -- the removal call ran, but had nothing left to tell it
// what to remove, so it removed nothing. See clearFftTopology()'s own
// doc comment for the full explanation and the fix (per-consumer
// unsubscribe(), matching MainWindow::rebuildFftRouting()'s pattern
// exactly instead of only citing it).
//
// IMPORTANT, found while verifying this task (see task-10-report.md):
// RadioModel::connectToRadio() is not safely exercisable end-to-end from
// an automated test. It contains a synchronous nested QEventLoop
// (RadioModel.cpp, "Block here while the wisdom worker finishes, pumping
// the Qt event loop") that blocks the CALLING thread until WdspEngine
// finishes generating FFTW wisdom -- by design, so the GUI can show a
// progress dialog during a cold-cache first connect (CLAUDE.md: "First
// run generates FFTW wisdom (~15 min)"). Measured directly during this
// task: >5 minutes on a cold cache, at which point QtTest's own 300 s
// per-function watchdog aborted the process (SIGABRT) rather than the
// call ever returning within a plausible test budget. No test in this
// 513-test suite calls RadioModel::connectToRadio() for exactly this
// reason -- every RadioModel-level test instead primes board state via
// RadioModel::setBoardForTest() + configureStreamPool() (see
// tst_p1_hl2_rx2_wiring.cpp). primeBoardForTest() below exists so this
// class's tests can follow the same established, wisdom-free pattern.
// The production discovery/connectToRadio() path is unchanged and is
// exercised only by manual verification (task-10-report.md), not ctest.
//
// FIX ROUND 1, FINDING 3: this same nested wait is reachable in
// production, and before this task server_main.cpp never constructed a
// RadioModel at all, so nothing could ever hit it -- this task's own
// commit was the first to make it reachable by nereusd. Calling
// DaemonApp::start() inline in main(), before app.exec(), meant the
// nested wisdom loop was the ONLY event loop alive during a cold-cache
// first connect, so a SIGTERM landing in that window had no outer loop
// to be serviced on (confirmed live: required SIGKILL after 10+ seconds
// unresponsive). server_main.cpp now schedules start() via a queued
// QMetaObject::invokeMethod so it runs AFTER app.exec() begins; verified
// live afterward that a SIGTERM sent mid-wisdom-generation now unwinds
// within about a second (qApp->quit() interrupts the nested loop too,
// same as any other nested QEventLoop wait in this codebase -- see
// server_main.cpp's own comment for the mechanism).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/RadioDiscovery.h"       // RadioInfo, RadioDiscovery, HPSDRHW
#include "core/daemon/DaemonConfig.h"
#include "core/spectrum/FftTopology.h"

#include <QObject>

#include <memory>
#ifdef NEREUS_BUILD_TESTS
#include <optional>
#endif

class QThread;

namespace NereusSDR {

class RadioModel;

// Connects a headless nereusd process to a radio and keeps its slice list
// in sync with the resolved DaemonConfig. See the file header above for
// why this exists and how it differs from the GUI's addSliceOnPan path.
//
// Lifecycle: start() may be called again after stop() (and, defensively,
// even without an intervening stop() -- it tears down any previous run
// first) and each cycle begins from a fresh RadioModel, so slice ids,
// FFT-topology subscriptions and connection state never carry over from
// a previous run. stop() always leaves sliceCount() == 0, including when
// called before any start() (test-covered: a daemon's SIGTERM handler
// calls stop() unconditionally on the quit path, with no prior knowledge
// of whether start() ever succeeded).
//
// R1 Task 11: also owns a dedicated QThread for the wideband FFT dispatch
// hop that RadioModel::wireConnectionSignals wires off the P2 connection
// thread (see widebandThread()'s own doc comment below). Unlike the
// RadioModel/FFT-topology state above, this thread is NOT torn down and
// rebuilt every start()/stop() cycle -- it is created once (lazily, on
// the first start()) and reused: stop() quit()+wait()s it (joins) without
// destroying it, and the next start() calls start() on the same QThread
// again, which Qt supports for a thread that has already finished.
class DaemonApp : public QObject {
    Q_OBJECT

public:
    explicit DaemonApp(QObject* parent = nullptr);
    ~DaemonApp() override;

    // Connects to a radio (or runs discovery when cfg.radioMac is empty --
    // see resolveRadioInfo()) and creates min(cfg.sliceCount,
    // connected-board-maxSlices) slices. Always returns true: a radio
    // that cannot be found or reached is not a startup failure for a
    // headless daemon (it may be powered on later, or discoverable once
    // the network settles), so the daemon still comes up with its
    // disconnected-default slice (see RadioModel::maxSlices()'s own
    // "returns 1 when disconnected" contract, mirrored here via
    // BoardCapabilities::maxSlices rather than that accessor -- see
    // resolveRadioInfo()'s call site in the .cpp for why the accessor
    // itself is the wrong read).
    //
    // NOTE: on a genuine first connect (cold FFTW wisdom cache) this call
    // blocks for as long as RadioModel::connectToRadio() takes to finish
    // wisdom generation -- see the file header above. That is inherited,
    // pre-existing behaviour, not something this method adds or can
    // avoid while still calling the real connectToRadio().
    bool start(const DaemonConfig& cfg);

    // Tears down in reverse: drops every FFT-topology subscription this
    // run created (via clearFftTopology(), which pushes the removal to
    // RadioModel's live FFTRouter BEFORE destroying anything -- see that
    // method's own doc comment for why the order matters), then destroys
    // the RadioModel (whose destructor disconnects the radio and deletes
    // every slice). Safe to call with no prior start() (a fresh DaemonApp
    // has no RadioModel to tear down) and safe to call twice in a row.
    void stop();

    // 0 before the first start(), after stop(), and whenever no radio
    // was ever connected. Otherwise the RadioModel's live slice count.
    int sliceCount() const;

#ifdef NEREUS_BUILD_TESTS
    // Test-only observer, only compiled when NEREUS_BUILD_TESTS is
    // defined, like every other test hook on this class. Production code
    // never asks: start() injects the thread into RadioModel itself and
    // stop()/~DaemonApp() own its lifetime, so the only caller is
    // tests/tst_wideband_thread.cpp.
    //
    // R1 Task 11. The dedicated thread start() injects into RadioModel
    // (RadioModel::setWidebandDispatchThread) as the target for the
    // wideband FFT dispatch hop -- see that method's own doc comment in
    // RadioModel.h for why nereusd needs this instead of relying on
    // RadioModel's implicit "hop to my own thread" default.
    //
    // nullptr before the first start() has ever run on this instance.
    // Running (isRunning() == true) after a successful start(). Joined --
    // quit()+wait()'d, but the SAME pointer, still non-null, still
    // isRunning() == false -- after stop(). NOT destroyed by stop(): the
    // pointer stays valid so a caller holding it across a stop() can
    // still observe the joined state, exactly as widebandThread() itself
    // continues to report it. Only ~DaemonApp() (via the owning
    // unique_ptr) or a later start() destroys/replaces it.
    QThread* widebandThread() const { return m_widebandThread.get(); }

    // Test-only seam, only compiled when NEREUS_BUILD_TESTS is defined.
    // Forces the NEXT start() (and every start() after a stop(), since
    // this persists across a restart on the same instance --
    // restartIsClean needs the second start() to prime the same board
    // again) to prime the RadioModel via setBoardForTest() +
    // configureStreamPool() instead of running resolveRadioInfo() /
    // calling the real connectToRadio(). See the file header above for
    // why: connectToRadio() blocks on a cold-cache WDSP wisdom
    // generation that took over 5 minutes when measured directly for
    // this task, which is incompatible with an automated test's budget.
    // Not part of the public API.
    void primeBoardForTest(HPSDRHW board) { m_testBoard = board; }

    // Test-only: the receiver/stream indices FFTRouter currently has
    // mapped for `consumerId` (RadioModel::fftRouter()->
    // receiversForPan(consumerId), despite the "Pan" name in that
    // method -- FFTRouter predates the pan/endpoint distinction design
    // section 9.4 draws; a daemon-minted endpoint id is just another
    // string key to it). Returns an empty list both when the id has no
    // mapping AND when there is no active RadioModel at all (before the
    // first start(), or after stop()) -- from a caller's perspective
    // those two cases are indistinguishable, and deliberately so: this
    // is what proves publishFftTopology() actually reached a live
    // router, not a probe into a dangling one. Not part of the public
    // API.
    QList<int> fftRouterMappingsForTest(const QString& consumerId) const;

    // Test-only: runs clearFftTopology() (stop()'s FFT-topology teardown
    // step) WITHOUT destroying the RadioModel, so a test can query
    // fftRouterMappingsForTest() immediately afterward and observe the
    // router's mappings actually gone while the router itself is still
    // alive to be queried. Fix round 2, Finding 1 (reopened): a query
    // made only AFTER a full stop() cannot tell "the router was cleared"
    // apart from "the router no longer exists" -- stop() destroys the
    // RadioModel (and the FFTRouter Qt-parented to it) in the very next
    // step -- so that alone was not a real assertion on this behaviour.
    // Not part of the public API.
    void clearFftTopologyForTest() { clearFftTopology(); }
#endif

signals:
    // Relays RadioModel::connectionStateChanged, collapsed to the
    // boolean isConnected() already reports via RadioModel's own
    // `connected` Q_PROPERTY. Fires whenever the underlying connection's
    // state actually transitions -- start() does not emit this itself,
    // so a caller only ever sees a real state change, never a synthetic
    // "just started" emission. Never fires in the primeBoardForTest()
    // path: that seam does not construct a real connection, by design.
    void radioConnected(bool connected);

private:
    // Resolves the radio to connect to via production discovery
    // (RadioDiscovery::startDiscovery(), a synchronous NIC walk -- see
    // RadioDiscovery.h -- filtered to cfg.radioMac when non-empty, or the
    // first responder when empty, per DaemonConfig.h's "empty = first
    // discovered" contract). Returns false, leaving `out` untouched, when
    // nothing is found; the caller (start()) treats that as "no radio
    // yet", not a hard failure. Not called at all when primeBoardForTest()
    // is armed -- see start()'s implementation.
    bool resolveRadioInfo(const DaemonConfig& cfg, RadioInfo& out) const;

    // Seeds the AppSettings keys the shared connect path reads, so that
    // config-file values actually take effect, from `cfg`:
    //
    //   sample_rate_hz -> hardware/<mac>/radioInfo/sampleRate
    //   audio_device   -> audio/Speakers/DeviceName
    //
    // Seeding the settings store rather than passing values down through
    // new parameters is deliberate. Both keys already have a single
    // production reader that does more than store the value:
    // resolveSampleRate() (SampleRateCatalog.h) validates the rate
    // against the connected board's allowed list and falls back to the
    // board default with a warning if it is not supported, and
    // AudioEngine::ensureSpeakersOpen() resolves an empty device name to
    // the platform default. A config file that bypassed those would be
    // able to ask for a rate the board cannot do. This way the daemon
    // and the GUI take the identical path, and the config file simply
    // decides what the persisted value is on this start.
    //
    // Requires a resolved `mac`, which is why it is called after
    // resolveRadioInfo() and before RadioModel::connectToRadio(): with
    // an empty radio_mac the MAC is not known until discovery answers.
    void applyConfigToSettings(const DaemonConfig& cfg, const QString& mac) const;

    // Tops up m_radioModel's slice list to min(cfg.sliceCount,
    // connected-board-maxSlices), starting from however many slices
    // connectToRadio() (or the disconnected-default / primed-board path)
    // already created. Idempotent-safe: does nothing if the target is
    // already met, and stops early (rather than looping forever) if
    // RadioModel::addSlice() ever refuses.
    void createConfiguredSlices(int sliceCountRequested);

    // Mints one placeholder endpoint id per current slice whose
    // streamIndex() is bound (>= 0) and subscribes it in m_topology. See
    // the file header above for why these ids are daemon-minted rather
    // than pan ids. Does NOT reach the router by itself -- see
    // publishFftTopology().
    void mintFftEndpoints();

    // Pushes m_topology's current subscription set to
    // m_radioModel->fftRouter() (FftTopology::applyTo() does a full
    // rebuild, not an incremental patch, so this is safe to call
    // whenever m_topology changes, not just once). No-op if there is no
    // active RadioModel. Called from start() (after mintFftEndpoints())
    // and from clearFftTopology() below.
    void publishFftTopology();

    // Drops every subscription this run created and pushes the removal
    // to the router, called from stop() before the RadioModel (and the
    // FFTRouter Qt-parented to it) is destroyed.
    //
    // Fix round 2, Finding 1 (reopened): an earlier version replaced
    // m_topology wholesale (`m_topology = FftTopology{};`) before calling
    // publishFftTopology(). That silently wiped
    // FftTopology::m_lastAppliedConsumers along with the subscriptions --
    // applyTo()'s removal loop iterates exactly that member to know what
    // to remove, so with it empty the call removed nothing at all. The
    // router's mappings were only ever cleared as a side effect of the
    // very next m_radioModel.reset() destroying the FFTRouter, not
    // because that call did anything; the comment claiming otherwise was
    // false, and fftRouterMappingsForTest() could not catch it because it
    // already reports empty once m_radioModel is null, whether or not
    // stop() cleared anything first.
    //
    // Unsubscribing each currently-held consumer instead -- mirroring
    // MainWindow::rebuildFftRouting()'s own per-consumer
    // m_topology.unsubscribe(panId) at MainWindow.cpp:2225 -- empties
    // m_streamsByConsumer while leaving m_lastAppliedConsumers untouched,
    // so the subsequent applyTo() genuinely has the previous push to
    // remove. Iterates a copy (subscriptions() builds a fresh QList, not
    // a live view), so mutating m_topology mid-loop is safe; a consumer
    // holding several streams appears once per stream in that list, and
    // unsubscribe(consumerId) on an id with nothing left is a no-op, so
    // no special-casing is needed for that.
    void clearFftTopology();

    std::unique_ptr<RadioModel> m_radioModel;
    FftTopology m_topology;

    // R1 Task 11. See widebandThread()'s doc comment above for the
    // lifecycle (lazily created on first start(), reused and restarted
    // across a stop()/start() cycle rather than destroyed on stop()).
    std::unique_ptr<QThread> m_widebandThread;

    // Monotonic; deliberately NOT reset on stop(), so ids never repeat
    // across a restart within one process. No correctness requirement
    // forces this (a future subscriber cannot yet exist to be confused
    // by reuse), but a monotonic id is cheap and one less thing to
    // reason about later.
    int m_nextEndpointId {0};

#ifdef NEREUS_BUILD_TESTS
    std::optional<HPSDRHW> m_testBoard;
#endif
};

} // namespace NereusSDR
