// =================================================================
// src/core/daemon/DaemonApp.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DaemonApp.h for the design
// rationale (R1 Task 10).
// =================================================================

#include "core/daemon/DaemonApp.h"

#include "core/CoreInit.h"
#include "core/FFTRouter.h"
#include "core/LogCategories.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>

namespace NereusSDR {

DaemonApp::DaemonApp(QObject* parent)
    : QObject(parent)
{
}

DaemonApp::~DaemonApp()
{
    stop();
}

bool DaemonApp::start(const DaemonConfig& cfg)
{
    // Defensive: no test calls start() twice without an intervening
    // stop(), but leaking the previous RadioModel (socket, WdspEngine,
    // discovery timers, ...) would be a silent resource leak the moment
    // one does. Tearing down first makes a second start() behave exactly
    // like stop() + start().
    if (m_radioModel) {
        stop();
    }

    // Idempotent process-wide (CoreInit.cpp's s_initialized guard), so
    // this is a genuine no-op when server_main.cpp already called it
    // before constructing this DaemonApp. Calling it here too means a
    // caller that constructs a DaemonApp directly -- every test in
    // tst_daemon_app.cpp -- does not have to replay that bootstrap step
    // itself.
    NereusSDR::CoreInit::initialize();

    m_radioModel = std::make_unique<RadioModel>();

    // Relay connection-state transitions to this class's own signal.
    // Fires only on a REAL state change (RadioModel emits
    // connectionStateChanged from its connection-thread-marshalled state
    // machine), never synthetically from start() itself.
    connect(m_radioModel.get(), &RadioModel::connectionStateChanged, this,
            [this](NereusSDR::ConnectionState) {
        emit radioConnected(m_radioModel->isConnected());
    });

#ifdef NEREUS_BUILD_TESTS
    if (m_testBoard.has_value()) {
        // Test-only path -- see primeBoardForTest()'s doc comment in
        // DaemonApp.h for why this exists instead of a real
        // connectToRadio() round trip. Mirrors the exact two calls
        // tst_p1_hl2_rx2_wiring.cpp already uses to prime a RadioModel
        // without a live connection: setBoardForTest() populates
        // boardCapabilities() from the real BoardCapabilities table (the
        // same table applyHpsdrModel() reads from inside connectToRadio()),
        // and configureStreamPool() sizes the stream allocator the same
        // way connectToRadio() does right before creating Slice A.
        m_radioModel->setBoardForTest(*m_testBoard);
        const auto& primedCaps = m_radioModel->boardCapabilities();
        const int poolSlices = primedCaps.maxSlices > 0 ? primedCaps.maxSlices : 1;
        m_radioModel->configureStreamPool(primedCaps.userDdcCount, poolSlices,
                                           cfg.sampleRateHz);
    } else
#endif
    {
        RadioInfo info;
        if (resolveRadioInfo(cfg, info)) {
            m_radioModel->connectToRadio(info);
        } else {
            // Not a startup failure -- see start()'s own doc comment. The
            // radio may appear later; today's daemon does not retry, so an
            // operator restarts nereusd (or a future task adds a retry loop)
            // once one is reachable.
            qCWarning(lcApp) << "DaemonApp: no radio found at startup"
                              << (cfg.radioMac.isEmpty()
                                      ? QStringLiteral("(discovery found nothing)")
                                      : QStringLiteral("matching MAC ") + cfg.radioMac)
                              << "- continuing with the disconnected-default slice";
        }
    }

    createConfiguredSlices(cfg.sliceCount);
    mintFftEndpoints();

    // Fix round 1, Finding 1: mintFftEndpoints() only fills m_topology's
    // own private bookkeeping. Without this call the subscriptions never
    // reached RadioModel's live FFTRouter (RadioModel::fftRouter()) and
    // were completely inert. See the file header above.
    publishFftTopology();

    return true;
}

void DaemonApp::stop()
{
    if (!m_radioModel) {
        return;
    }

    // Reverse of start(): drop this run's FFT-topology subscriptions and
    // push the now-empty set to the router BEFORE the RadioModel that
    // owns it is destroyed, so the router explicitly drops every mapping
    // rather than simply ceasing to exist mid-mapping. Both statements
    // matter: clearing m_topology alone (as fix round 1, Finding 1 found)
    // never reaches the router at all without the applyTo() call
    // publishFftTopology() makes.
    m_topology = FftTopology{};
    publishFftTopology();

    // ~RadioModel() calls teardownConnection() and deletes every slice
    // (RadioModel.cpp), so resetting the pointer alone satisfies
    // "stop() leaves sliceCount() == 0." Reset before emitting so any
    // radioConnected(false) listener already sees a consistent
    // (sliceCount() == 0) state if it queries back into this object.
    m_radioModel.reset();

    emit radioConnected(false);
}

int DaemonApp::sliceCount() const
{
    return m_radioModel ? m_radioModel->slices().size() : 0;
}

bool DaemonApp::resolveRadioInfo(const DaemonConfig& cfg, RadioInfo& out) const
{
    // Preconditions: called only from start(), after m_radioModel has
    // just been constructed, and only on the production path (start()
    // does not call this at all when primeBoardForTest() is armed).
    //
    // A real NIC-walk broadcast. RadioDiscovery::
    // startDiscovery() -> scanAllNics() blocks internally (waitForReadyRead
    // per NIC, up to ~1.8 s per NIC at the SafeDefault profile --
    // RadioDiscovery.h's DiscoveryTiming table), so this call IS the
    // daemon's "wait for the radio" step; no separate event loop needed
    // here, and discoveredRadios() below already reflects whatever the
    // walk found by the time startDiscovery() returns.
    RadioDiscovery* discovery = m_radioModel->discovery();
    discovery->startDiscovery();

    const QList<RadioInfo> found = discovery->discoveredRadios();
    if (found.isEmpty()) {
        return false;
    }

    if (cfg.radioMac.isEmpty()) {
        // "empty = first discovered" -- DaemonConfig.h's own contract
        // for this field.
        out = found.first();
        return true;
    }

    for (const RadioInfo& candidate : found) {
        if (candidate.macAddress.compare(cfg.radioMac, Qt::CaseInsensitive) == 0) {
            out = candidate;
            return true;
        }
    }
    return false;
}

void DaemonApp::createConfiguredSlices(int sliceCountRequested)
{
    // caps.maxSlices directly, NOT the maxSlices() accessor: that
    // accessor returns 1 until RadioModel::isConnected() is true.
    // RadioModel::connectToRadio() itself reads boardCapabilities()
    // directly for the identical reason (RadioModel.cpp, the comment
    // beside its own "poolSlices" local: "caps.maxSlices rather than the
    // maxSlices() accessor: that accessor returns 1 until isConnected()
    // is true, and m_connection is not assigned until further down this
    // function") -- isConnected() does not become true until AFTER
    // connectToRadio()'s own synchronous WDSP-wisdom wait completes and
    // m_connection is assigned, well after hardware-profile resolution
    // (and therefore this SKU's maxSlices) is already settled. Reading
    // boardCapabilities() directly means this method sees the right
    // number regardless of which path start() took (a real
    // connectToRadio(), the primeBoardForTest() seam, or neither).
    const auto& caps = m_radioModel->boardCapabilities();
    const int capMaxSlices = caps.maxSlices > 0 ? caps.maxSlices : 1;
    const int target = std::min(sliceCountRequested, capMaxSlices);

    // Tops up from however many slices already exist. connectToRadio()
    // (when a radio was found and connected above) already created
    // Slice A via its own no-argument addSlice() call before this method
    // runs, so this loop runs (target - 1) more times in that case. The
    // primeBoardForTest() seam does NOT create Slice A itself (it only
    // primes boardCapabilities() and sizes the stream pool, mirroring
    // tst_p1_hl2_rx2_wiring.cpp's own setBoardForTest() + configureStreamPool()
    // pattern, which likewise leaves the first addSlice() call to its
    // caller), so in that path -- and in the no-radio-found path -- this
    // loop starts from zero and runs the full target times.
    while (m_radioModel->slices().size() < target) {
        const int id = m_radioModel->addSlice();
        if (id < 0) {
            // The allocator refused (e.g. no stream left to share).
            // "Up to" the target, per this class's own contract: stop
            // rather than loop forever re-asking for something that
            // just failed.
            qCWarning(lcApp) << "DaemonApp: addSlice() refused at"
                              << m_radioModel->slices().size() << "of"
                              << target << "requested slices";
            break;
        }
    }
}

void DaemonApp::mintFftEndpoints()
{
    for (SliceModel* slice : m_radioModel->slices()) {
        const int stream = slice->streamIndex();
        if (stream < 0) {
            // Unbound -- e.g. the disconnected-default slice, created
            // before any stream pool exists. Nothing to subscribe to yet.
            continue;
        }
        const QString endpointId =
            QStringLiteral("daemon-ep-%1").arg(m_nextEndpointId++);
        m_topology.subscribe(endpointId, stream);
    }
}

void DaemonApp::publishFftTopology()
{
    if (!m_radioModel) {
        return;
    }
    m_topology.applyTo(*m_radioModel->fftRouter());
}

#ifdef NEREUS_BUILD_TESTS
QList<int> DaemonApp::fftRouterMappingsForTest(const QString& consumerId) const
{
    if (!m_radioModel) {
        return {};
    }
    return m_radioModel->fftRouter()->receiversForPan(consumerId);
}
#endif

} // namespace NereusSDR
