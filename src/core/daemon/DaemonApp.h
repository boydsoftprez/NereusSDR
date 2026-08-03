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
// does not describe; that wiring belongs with Task 11 (wideband FFT
// thread), which can construct its own pool alongside the tap it adds.
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
    // run created, then destroys the RadioModel (whose destructor
    // disconnects the radio and deletes every slice). Safe to call with
    // no prior start() (a fresh DaemonApp has no RadioModel to tear
    // down) and safe to call twice in a row.
    void stop();

    // 0 before the first start(), after stop(), and whenever no radio
    // was ever connected. Otherwise the RadioModel's live slice count.
    int sliceCount() const;

#ifdef NEREUS_BUILD_TESTS
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
    // than pan ids.
    void mintFftEndpoints();

    std::unique_ptr<RadioModel> m_radioModel;
    FftTopology m_topology;

    // Monotonic; deliberately NOT reset on stop(), so ids never repeat
    // across a restart within one process even though m_topology itself
    // is replaced wholesale in stop(). No correctness requirement forces
    // this (a future subscriber cannot yet exist to be confused by
    // reuse), but a monotonic id is cheap and one less thing to reason
    // about later.
    int m_nextEndpointId {0};

#ifdef NEREUS_BUILD_TESTS
    std::optional<HPSDRHW> m_testBoard;
#endif
};

} // namespace NereusSDR
