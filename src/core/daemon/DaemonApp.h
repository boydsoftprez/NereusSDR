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
// FftEnginePool (Task 6) is not constructed here. The daemon's display
// FFT engines are built on demand by the media path instead:
// m_mediaHub (DaemonMediaHub, iPhone app Task 76) makes one
// DaemonMediaController per admitted session; they share one
// DaemonSharedSpectrum, whose DaemonSpectrumSource owns an FftEnginePool
// keyed by (stream, tier) and fed straight from RadioModel::rawIqDataForStream
// once a remote subscription activates a source. The controllers decide
// each engine's size and what every endpoint is granted (R3 remote display
// limits plan, Task 1), across every device watching that receiver.
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
// Real connection tests use WdspEngine::setSynchronousInitForTest(true)
// with a loopback radio; primeBoardForTest remains for topology-only tests.
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
//   2026-09-23: display load governor and shared host sampler (R-R3-08,
//               R-R3-37, R-R3-40). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-23: the governor acts only on load display cuts can relieve
//               (thread placement), accepts a step only once published,
//               and does not run for apps older than the budget reason
//               (R-R3-08, R-R3-37, R-R3-40). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: iPhone app Task 17 (R-IOS-08): the status page beside the
//               remote listener, the first start's notice, and the console
//               socket (startControlSocket()). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08, R-IOS-16): the Core
//               registers with the remote access service (startRendezvous()),
//               holds a nameplate while its pairing window is open and pairs
//               through the service's mailbox. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Parity Task 21 (R-IOS-18): StationRadios, the radio switch
//               and the connected rescan. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: R-R3-49: the config file's sample_rate_hz is a starting
//               value only; a rate already saved for the radio wins at start
//               (applyConfigToSettings). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-01: m_radioChangeRestartPending. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: stopRadioRecovery and m_radioChangeRestarting: a radio
//               change ends when the run serving it stops. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: stopRadioRecovery cancels a restart still pending, and
//               radioChangeStoppedReason. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "core/RadioDiscovery.h"       // RadioInfo, RadioDiscovery, HPSDRHW
#include "core/daemon/DaemonConfig.h"
#include "core/daemon/DisplayLoadInputs.h"
#include "core/daemon/StationControlSocket.h"
#include "core/daemon/StationStatusPage.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/platform/ThreadPlacement.h"
#include "core/session/media/DisplayLoadGovernor.h"
#include "core/spectrum/FftTopology.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include "core/ConnectionState.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>

class QThread;
class QTimer;

namespace NereusSDR {

class RadioModel;
class StationRadios;
class StationServer;
class StationHost;
class StationLanAnnouncer;
class DnsSdAdvertiser;
class DaemonMediaController;
class DaemonMediaHub;
class DaemonTelemetryController;
class DaemonAgcSource;
class DisplayLoadGovernor;
class SharedHostSampler;
class SliceModel;
class StepAttenuatorController;
class TxAnalyzer;
class StationControlCommands;
class StationRendezvous;

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
    /// iPhone app Task 12: the address remote_bind names; empty is every
    /// interface, IPv4 and IPv6 (QHostAddress::Any, dual stack), and so is
    /// "::" (DaemonConfig::listenAddressFor, R-R3-26). A null address for
    /// text that is not one.
    static QHostAddress listenerAddressFor(const QString& bind);

    /// The session end every app gets when the Core switches to `radioName`.
    static QString radioChangeReason(const QString& radioName);
    /// The chooser's refusal when the Core's run stops (a stop, a station
    /// release, a disconnect) before its radio change could run.
    static QString radioChangeStoppedReason();

    explicit DaemonApp(QObject* parent = nullptr);
    ~DaemonApp() override;

    // Starts the station/control plane immediately and schedules cancellable
    // selected-radio discovery. Radio absence is not a startup failure; the
    // default slice remains available until the configured radio appears.
    // Returns false only if a previous cancelled connection setup is still
    // draining its non-interruptible WDSP wisdom job. See the recovery design.
    bool start(const DaemonConfig& cfg);

    // iPhone app Task 4 (R-IOS-01): the link majors the station advertises
    // and accepts. Call before start(); empty (the default) keeps the
    // build's own list. Only a debug build's --test-link-majors sets it.
    void setLinkMajors(const QList<quint16>& majors) { m_linkMajors = majors; }

    // Tears down in reverse: drops every FFT-topology subscription this
    // run created (via clearFftTopology(), which pushes the removal to
    // RadioModel's live FFTRouter BEFORE destroying anything -- see that
    // method's own doc comment for why the order matters), then destroys
    // the RadioModel (whose destructor disconnects the radio and deletes
    // every slice). Safe to call with no prior start() (a fresh DaemonApp
    // has no RadioModel to tear down) and safe to call twice in a row.
    void stop();

    // 0 before start and after completed stop; otherwise the live slice count,
    // including the disconnected-default slice while discovery is pending.
    int sliceCount() const;

    // Remote listener observability. A configured listener may exist while a
    // transient bind failure is waiting for its next bounded retry.
    bool stationListenerReady() const;
    bool stationListenerRetryPending() const;

    /// iPhone app Task 17 (R-IOS-08): listens for the console commands at
    /// `path` (StationControlSocket::socketPathFor(), from nereusd.conf's
    /// state_directory or the profile). server_main.cpp calls it once the
    /// Core has started; a test calls it with a scratch path. False (and
    /// logged) when it cannot listen, which does not stop the Core. The
    /// socket closes on stop().
    bool startControlSocket(const QString& path);
    /// What the console commands answer, without the socket (the same
    /// object startControlSocket() serves).
    StationControlReply runControlCommand(const QStringList& args);
    enum class StationReleaseResult { Pending, Failed, Stopped };
    enum class StationReleaseRecoveryResult { Pending, Restored, Unavailable };
    // Called by the entry-point release coordinator. The first call unkeys
    // before any other release work; completion is deferred past a nested
    // WDSP connect stack and refuses to destroy unsaved model state.
    void beginStationRelease();
    StationReleaseResult tryCompleteStationRelease(QString* reason);
    // A refused pre-teardown release keeps the same model and settings.
    // Reopen its ordinary host after the nested radio connect stack unwinds.
    StationReleaseRecoveryResult recoverFailedStationRelease();

    /// The Core's label as the status page and console show it: its
    /// renamed label, or core_name, or this computer's name.
    QString coreLabel() const;
    /// The Core's radio as the status page and console show it.
    StationRadioStatus radioStatus() const;
    /// The status page's address for the operator; "" while it is off.
    QString statusPageAddress() const;
    /// The status page, while it runs; null otherwise.
    StationStatusPage* statusPage() const;
    // Borrowed from the live StationHost for console commands and radio
    // changes. Null before start(), after stop(), or with remote control disabled.
    StationServer* stationServer() const;
    // How the Core's pairing window reads on the announcement and in
    // Bonjour's `pair` key. The announcement builds from it, so it is
    // compiled whether or not the tests are.
    static StationLanPairing stationLanPairingFor(const StationServer& server);

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

    /// Parity Task 21: the Core's radios and its choice of one (kept across
    /// a radio change's restart).
    StationRadios* stationRadios() const { return m_stationRadios.get(); }

    // R-R3-39 / R-IOS-03: the running RadioModel (nullptr before start() and
    // after stop()), so a test can key the model the daemon builds.
    RadioModel* radioModelForTest() const { return m_radioModel.get(); }
    void setRadioConnectInProgressForTest(bool inProgress) {
        m_radioConnectInProgress = inProgress;
    }
    void setStopAllTxForTest(std::function<void()> stop) {
        m_stopAllTxForTest = std::move(stop);
    }
    void setAfterStationStopForTest(std::function<void()> afterStop) {
        m_afterStationStopForTest = std::move(afterStop);
    }
    void setDiscoveryProviderForTest(std::function<QList<RadioInfo>()> provider) {
        m_discoveryProviderForTest = std::move(provider);
    }

    void setStationListenRetryIntervalsForTest(int initialMs, int maximumMs)
    {
        m_stationListenRetryInitialMs = initialMs > 0 ? initialMs : 1;
        m_stationListenRetryMaximumMs = maximumMs >= m_stationListenRetryInitialMs
            ? maximumMs : m_stationListenRetryInitialMs;
    }
    int stationListenAttemptCountForTest() const;
    // iPhone app Task 16: the announcement and Bonjour record the Core last
    // built, whether or not they went out (a loopback listener sends
    // neither), and whether each is going out.
    StationLanAnnouncement stationAnnouncementForTest() const;
    DnsSdRecord dnsSdRecordForTest() const;
    bool stationAnnouncedForTest() const;
    bool dnsSdAdvertisedForTest() const;
    // Before start(): the Bonjour advertiser to use, so a test sees what
    // would be advertised without reaching a real network.
    void setDnsSdAdvertiserForTest(std::unique_ptr<DnsSdAdvertiser> advertiser);

    // Test-only seam, only compiled when NEREUS_BUILD_TESTS is defined.
    // Forces the NEXT start() (and every start() after a stop(), since
    // this persists across a restart on the same instance --
    // restartIsClean needs the second start() to prime the same board
    // again) to prime the RadioModel via setBoardForTest() +
    // configureStreamPool() instead of running asynchronous discovery /
    // calling the real connectToRadio(). See the file header above for
    // why: connectToRadio() blocks on a cold-cache WDSP wisdom
    // generation that took over 5 minutes when measured directly for
    // this task, which is incompatible with an automated test's budget.
    // Not part of the public API.
    void primeBoardForTest(HPSDRHW board, const QString& mac = {}) {
        m_testBoard = board;
        m_testRadioMac = mac;
    }

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

    // Test-only seam for the display load governor (R-R3-08/37/40): the
    // measured inputs (receiver loads, CPU readings, thread placement plan)
    // the governor's clock and the accepted display charge, in place of
    // RadioModel, the host sampler, ThreadPlacement, the elapsed timer and
    // the media controller's endpoints. Set before start().
    void setDisplayLoadSourcesForTest(std::function<DisplayLoadInputs()> inputs,
                                      std::function<qint64()> clock,
                                      std::function<DisplayBudgetCharge()> accepted)
    {
        m_displayLoadInputsForTest = std::move(inputs);
        m_displayGovernorNowForTest = std::move(clock);
        m_acceptedDisplayChargeForTest = std::move(accepted);
    }
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
    void attemptRadioDiscovery();
    void finishRadioDiscovery(const QList<RadioInfo>& found);
    void scheduleRadioDiscovery();
    void cancelRadioDiscovery();
    void onRadioStateForRecovery(ConnectionState state);
    void retireRadioAndRetry();

    // Seeds the AppSettings keys the shared connect path reads, so that
    // config-file values actually take effect, from `cfg`:
    //
    //   sample_rate_hz -> hardware/<mac>/radioInfo/sampleRate, only when
    //                     that radio has no saved rate (a starting value;
    //                     a saved rate wins, R-R3-49)
    //   audio_device   -> audio/Speakers/DeviceName, only when no speaker
    //                     choice is saved (a starting value; a saved
    //                     choice wins, G-16)
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
    // and the GUI take the identical path. For the rate and the audio
    // device alike the config file only fills an empty slot.
    //
    // Requires a resolved `mac`, which is why it is called after
    // identity selection and before RadioModel::connectToRadio(): with
    // an empty radio_mac the MAC is not known until discovery answers.
    void applyConfigToSettings(const DaemonConfig& cfg, const QString& mac) const;

    // Gives the headless daemon the same core-owned RX attenuation,
    // preamp, overload and ATT-on-TX state that MainWindow wires in a
    // desktop-local session. The controller is injected before the radio
    // connect so RadioModel's connect-time capability and PureSignal paths
    // can see it; this finishing step runs after slices exist so the
    // controller can select slice A's band (RadioModel::
    // followReceiveSliceWithStepAttenuator) before loading that band's
    // persisted values.
    void configureStepAttenuatorController(const QString& mac);
    void applyStepAttenuatorConnection(const QString& mac);

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

    // R-R3-39 / R-IOS-03: creates the Core's TX analyzer and runs it on the
    // MOX edge as the desktop does. Called from start() after the model.
    void createTxAnalyzer();

    // R-R3-39 / R-IOS-03: the desktop's TX analyzer (display 5, fed by the
    // TX siphon). Declared before m_radioModel so it outlives the model, as
    // stop() also keeps: the model's TX channel closes first.
    std::unique_ptr<TxAnalyzer> m_txAnalyzer;
    std::unique_ptr<RadioModel> m_radioModel;
    DaemonConfig m_radioConfig;
    QString m_selectedRadioMac;
    // Parity Task 21 (R-IOS-18): the radios this Core finds, its choice of
    // one and the requests that change it. Kept across a restart (a radio
    // change restarts the run), so the list a window shows stays.
    std::unique_ptr<StationRadios> m_stationRadios;
    std::unique_ptr<QThread> m_radioScanThread;
    quint64 m_radioScanGeneration {0};
    void ensureStationRadios();
    void switchRadio(const QString& mac);
    void restartForRadioChange();
    void rescanRadios();
    // Fix wave (C1): a radio change ends (the "switching" flag clears, so a
    // window can choose again) when the new radio connects, its connect
    // fails, its link is lost, a scan does not pick it, or the Core's
    // connect bound passes with none of those. The pending choice is kept
    // as this run's radio either way.
    void endRadioSwitch();
    // The run stops looking for and reconnecting its radio (stop(), a
    // station release, the operator's disconnect). Nothing is left to serve
    // a radio change, so it ends here too, except for the stop inside a
    // radio change's own restart (m_radioChangeRestarting). A change whose
    // restart has not run is cancelled: the restart is not run, the chooser
    // is refused and the choice dropped.
    void stopRadioRecovery();
    // Fix wave (M3): the on-the-air rule, again at the moment the change
    // runs. True when the radio is on the air: the change is refused and
    // nothing is torn down.
    bool refuseRadioChangeOnAir();
    // The connect watchdog budget of both OpenHPSDR connections:
    // P1RadioConnection.h kConnectTimeoutMs (2000 ms, "Connect watchdog:
    // fires this many ms after connectToRadio() if no first ep6 frame
    // arrives") and P2RadioConnection.h kConnectTimeoutMs (2000 ms, "Connect
    // watchdog budget -- 2 s matches P1"). NereusSDR-original values.
    static constexpr int kRadioSwitchConnectBoundMs = 2000;
    int m_radioSwitchBoundMs {kRadioSwitchConnectBoundMs};
    QTimer* m_radioSwitchDeadline {nullptr};
    // The operator's ruling of 2026-09-26: a radio change restarts the run,
    // and every app reconnects by itself. The end reason its sessions get.
    QString m_radioChangeReason;
    // From the choice (switchRadio) until the restart runs: nothing the
    // run has now reports (a failure, a Connected, a discovery or connect
    // that did not start) is the change's, so none of it ends the change
    // (endRadioSwitch).
    bool m_radioChangeRestartPending {false};
    // I1: the console commands survive that restart (stop() keeps them
    // while this is set).
    bool m_keepConsoleOnStop {false};
    // While a radio change's restart runs start(): the stop that start()
    // makes is the change's own, so it does not end the change
    // (stopRadioRecovery). restartForRadioChange decides after start().
    bool m_radioChangeRestarting {false};
    std::unique_ptr<QThread> m_radioDiscoveryThread;
    QTimer* m_radioRetryTimer {nullptr};
    quint64 m_radioRecoveryGeneration {0};
    quint64 m_radioRunGeneration {0};
    bool m_radioRecoveryEnabled {false};
    bool m_stationReleaseRadioRecoveryWasEnabled {false};
    bool m_radioAttempted {false};
    bool m_radioConnectedBefore {false};
    bool m_retiringRadio {false};
    bool m_radioConnectInProgress {false};
    bool m_stopDeferred {false};
    int m_radioRetryInitialMs {1000};
    int m_radioRetryMaximumMs {15000};
    int m_radioRetryNextMs {1000};
    // Non-GUI owner of the controller RadioModel and TransmitModel hold by
    // raw pointer. It must outlive m_radioModel teardown because that path
    // saves its per-MAC state before releasing the RadioConnection.
    std::unique_ptr<StepAttenuatorController> m_stepAttController;
    bool m_stepAttControllerConfigured {false};

    // The shared station owns all network/media sessions around this borrowed
    // RadioModel. It is stopped and destroyed before m_radioModel.reset().
    std::unique_ptr<StationHost> m_stationHost;
    std::unique_ptr<StationControlCommands> m_controlCommands;
    std::unique_ptr<StationControlSocket> m_controlSocket;
    QList<quint16> m_linkMajors;
    int m_stationListenRetryInitialMs {1000};
    int m_stationListenRetryMaximumMs {30000};
    std::unique_ptr<DnsSdAdvertiser> m_testDnsSdAdvertiser;
    std::function<DisplayLoadInputs()> m_displayLoadInputsForTest;
    std::function<qint64()> m_displayGovernorNowForTest;
    std::function<DisplayBudgetCharge()> m_acceptedDisplayChargeForTest;
    std::unique_ptr<DaemonAgcSource> m_agcSource;
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
    std::function<QList<RadioInfo>()> m_discoveryProviderForTest;
    bool m_synchronousWdspForTest {false};
    // Install test audio devices before discovery can enter real DSP startup.
    std::function<void(RadioModel*)> m_radioInitializerForTest;
    std::function<void()> m_stopAllTxForTest;
    std::function<void()> m_afterStationStopForTest;
    std::optional<HPSDRHW> m_testBoard;
    QString m_testRadioMac;
#endif
};

} // namespace NereusSDR
