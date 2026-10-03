// tests/fakes/ConnectableRadioModel.h
//
// no-port-check: NereusSDR-original test fixture. Wires together two
// already-existing production classes (RadioModel, P1FakeRadio) for test
// use; no Thetis logic is ported or reimplemented here.
//
// Remote-daemon R2 Task 2 -- a connectable RadioModel for tests.
//
// RadioModel::connectToRadio() has never been exercised by any test in
// this suite (see tst_daemon_app.cpp's header comment): on a cold config
// directory -- which tests/TestSandboxInit.cpp forces on every run -- it
// blocks the calling thread inside a QEventLoop until WdspEngine finishes
// generating FFTW wisdom, which takes minutes. WdspEngine::
// setSynchronousInitForTest() (src/core/WdspEngine.h) closes that gap by
// making initialize() skip spawning the "WisdomThread" QThread and instead
// run finishInitialization() synchronously, so the RX/TX-channel-creation
// lambda that RadioModel::connectToRadio() wires to WdspEngine::
// initializedChanged() still fires at the right moment. See that method's
// doc comment, tests/tst_connectable_radio_model.cpp, and
// .superpowers/sdd/2026-08-03-remote-daemon-r2-plan/task-2-report.md for
// the full story and the measured timing.
//
// ConnectableRadioModel packages that seam plus a P1FakeRadio loopback
// fake into one reusable factory so tests 3, 12 and 20 (per the R2 plan)
// don't each have to re-derive the wiring:
//   Task 3  -- teardown-equivalence assertion (RadioModel torn down the
//              same way whether local or remote).
//   Task 12 -- needs a genuinely live RxChannel off the connected model.
//   Task 20 -- Setup-page realization sweep against a connected model.
//
// Remote-daemon R2 Task 4: create() now takes an optional RadioModel::Role
// (defaults to Local, so every existing call site is unaffected). Passing
// Role::Remote builds the model with that role instead of duplicating this
// fixture's wiring in tst_remote_role_inert.cpp -- see create()'s doc
// comment below for how the two roles diverge inside the factory.

#pragma once

#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"

#include "P1FakeRadio.h"

#include <functional>
#include <memory>

#include <QTimer>

namespace NereusSDR::Test {

// Owns a P1FakeRadio loopback fake and a RadioModel connected to it.
//
// Construct only via create() -- the default constructor is private so
// callers can never observe a not-yet-connected (or failed-to-connect)
// instance; create() returns nullptr instead of a half-built object.
//
// Non-copyable: owns two QObjects wired to a live loopback UDP pair and,
// inside RadioModel, a worker QThread. Move is not provided either --
// nothing in tasks 2/3/12/20 needs to relocate an already-built harness,
// and std::unique_ptr<ConnectableRadioModel> covers ownership transfer.
class ConnectableRadioModel {
public:
    ConnectableRadioModel(const ConnectableRadioModel&) = delete;
    ConnectableRadioModel& operator=(const ConnectableRadioModel&) = delete;
    ~ConnectableRadioModel();

    // Builds a P1FakeRadio and starts it, constructs a RadioModel with the
    // given role, arms WdspEngine::setSynchronousInitForTest() on that
    // model's engine BEFORE calling RadioModel::connectToRadio() (order
    // matters -- see that method's doc comment), then:
    //
    //   role == Local  (default): pumps the Qt event loop (via
    //     QTest::qWaitFor) until RadioModel::connectionState() reaches
    //     ConnectionState::Connected or timeoutMs elapses. Returns nullptr
    //     on timeout.
    //
    //   role == Remote: returns immediately after connectToRadio() with no
    //     wait. A Role::Remote model's connectToRadio() is inert by design
    //     (remote-daemon R2 Task 4's early-return guard) -- it never
    //     touches WdspEngine, AudioEngine or the fake's socket, so it never
    //     reaches Connected, and waiting for that would just burn
    //     timeoutMs and report a false failure. timeoutMs is unused on
    //     this path.
    //
    // QVERIFY/QCOMPARE only fail the enclosing QtTest slot when used
    // directly inside it -- their generated `return;` requires the
    // function to return void -- so this factory reports failure through
    // its return value instead of asserting internally, and leaves the
    // QVERIFY(...) to the caller. On a Local timeout, both the
    // partially-connected model and the fake are torn down before
    // returning null; nothing leaks.
    //
    // R-R3-36 Task 5: beforeConnect, when set, runs on the built model
    // immediately before connectToRadio() (after the fixture's own audio
    // test initializer is installed, so it may replace it), letting a test
    // install capture supervisor options or policy flags that must be in
    // place before the connect path starts the AudioEngine.
    //
    // Group B fix wave (I3): FFTW's plans are kept beside the test
    // binaries, since the synchronous test init never loads WDSP's wisdom
    // and planning from nothing took about 45 s on every run. The parity
    // mini-round moved the cache to tests/TestFftwWisdomCache.cpp (one
    // file per build directory, test-fftw-wisdom), linked into every test
    // that starts WDSP this way.
    //
    // Receiver and transmit gaps plan Task 1: `board` is the board type the
    // RadioInfo announces (Protocol 1, the P1FakeRadio's wire either way),
    // so a test can connect a smaller board than the Hermes Lite 2 default.
    //
    // Load findings 4: `stream` false keeps the fake from streaming ep6 on
    // its own; the harness then sends one frame every 500 ms, enough to
    // keep the link alive (P1RadioConnection's 3 s silence rule) and far
    // too few for the receiver to finish a block (about 228 frames), so a
    // test feeds exactly the blocks it wants (fake().sendEp6Frames()).
    static std::unique_ptr<ConnectableRadioModel> create(
        int timeoutMs = 10000,
        NereusSDR::RadioModel::Role role = NereusSDR::RadioModel::Role::Local,
        std::function<void(NereusSDR::RadioModel&)> beforeConnect = {},
        NereusSDR::HPSDRHW board = NereusSDR::HPSDRHW::HermesLite,
        bool stream = true);

    NereusSDR::RadioModel&       model()       { return *m_model; }
    const NereusSDR::RadioModel& model() const { return *m_model; }
    P1FakeRadio&                 fake()        { return *m_fake; }
    const P1FakeRadio&           fake()  const { return *m_fake; }

    // The RadioInfo passed to connectToRadio() -- board/protocol/MAC
    // identity, for callers that want to assert against it directly
    // rather than re-deriving it from fake().
    const NereusSDR::RadioInfo& radioInfo() const { return m_info; }

private:
    ConnectableRadioModel() = default;

    // Declaration order is destruction order (reverse): m_model is torn
    // down (RadioModel::~RadioModel() -> teardownConnection()) BEFORE
    // m_fake, so RadioModel's graceful-disconnect path still has a live
    // fake to talk to. Keep m_fake declared first.
    std::unique_ptr<P1FakeRadio>           m_fake;
    std::unique_ptr<NereusSDR::RadioModel> m_model;
    // The link's keep-alive when the fake does not stream (see create()).
    std::unique_ptr<QTimer>                m_keepAlive;
    NereusSDR::RadioInfo                   m_info;
};

} // namespace NereusSDR::Test
