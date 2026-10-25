// =================================================================
// tests/tst_mox_display_controller.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. The Thetis behaviour it checks
// is cited in src/gui/MoxDisplayController.h.
//
// Remote-window parity Task 29 (A11, R-R3-49, R-R3-12, rows 14 and 16):
// one MOX display controller for both windows.
//   - The local window unchanged: the rise and fall make the calls the old
//     MainWindow lambda made, on the pan hosting the transmit slice.
//   - A remote window: the Core's keyed txState puts the MOX overlay (red
//     border, transmit grid, palette, waterfall levels) on the transmitting
//     pan only, which draws the Core's transmit frames at the context's
//     centre and span; the other pan keeps its receive frames; the fall
//     restores the pan exactly.
//   - A Core that sends no transmit display: the overlay still, no receive
//     frame drawn while keyed, and the status line says why.
//   - Row 16 (XIT while keyed) in both windows, tuning while keyed in both,
//     the high-SWR border in a remote window, and a layout change while
//     keyed (row 14) in both.
// Nothing keys a radio: the Core's own MoxController is keyed with the
// receive-only pre-check lifted, against no hardware.
//
// Modification history (NereusSDR):
//   2026-10-05 — J.J. Boyd (KG4VCF). Independent per-pan Clarity ownership.
//                 AI-assisted via OpenAI Codex.
//   2026-09-26 : Created for remote-window parity Task 29 by J.J. Boyd
//                 (KG4VCF). AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-26 : Tasks 27-29 fix wave (R-R3-49): the rise pan keeps the
//                 transmit display on both sides through a rebinding and a
//                 layout change while keyed; no transmit trace under the
//                 receive axis after a remote fall; a transmit context that
//                 beat the rise is replayed; an older Core is not asked
//                 again. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-10 : Display pill plan Task 13: the rise takes Clarity off no
//                 pan and neither edge re-primes the waterfall AGC; both
//                 edges reach every pan first, with its band and its hold
//                 length. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest>

#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#define private public
#include "gui/SpectrumWidget.h"
#include "core/TwoToneController.h"
// The test seam for a rebinding while keyed (TxSliceArbiter::flipTo moves
// the transmit flag without requestHandoff's unkey).
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "core/WdspEngine.h"
#undef private

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/TxAnalyzer.h"
#include "core/TxDisplayFeed.h"
#include "core/TxChannel.h"
#include "core/DspControlThread.h"
#include "core/RadioConnection.h"
#include "core/wdsp_api.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/IMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "fakes/NoCaptureHelper.h"
#include "gui/MoxDisplayController.h"
#include "gui/PanStatusText.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/TxDisplaySource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

class MeasurementPreferenceGuard {
public:
    MeasurementPreferenceGuard()
        : m_hadKey(AppSettings::instance().contains(m_key))
        , m_previous(AppSettings::instance().value(m_key)) {}
    ~MeasurementPreferenceGuard()
    {
        if (m_hadKey) { AppSettings::instance().setValue(m_key, m_previous); }
        else { AppSettings::instance().remove(m_key); }
    }
private:
    const QString m_key{QStringLiteral("puresignal/showTwoToneMeasurements")};
    const bool m_hadKey;
    const QVariant m_previous;
};

class MeasurementCoordinator : public PureSignal {
public:
    MeasurementCoordinator() : PureSignal(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) {}
    int displayConnectionCount() const
    {
        return receivers(SIGNAL(show2ToneMeasurementsChanged(bool)));
    }
};

// Set authoritative state without invoking activation, MOX or hardware.
void publishTwoToneState(RadioModel& radio, bool active)
{
    TwoToneController* controller = radio.twoToneController();
    controller->m_active = active;
    emit controller->twoToneActiveChanged(active);
}

// The real WDSP channel writes only to this in-process discard connection.
class WaveformConnection final : public RadioConnection {
public:
    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

constexpr char kMissingDisplay[] =
    "This Core does not send its transmit display. Updating the Core may help.";

// The old MainWindow lambda's widget calls on the rise, in its order (read
// from MainWindow.cpp at 9ef6c710, MoxController::moxStateChanged). Its
// second setTxCenterFrequency / setTxSampleRate pair (the explicit sync
// after the view move) set the same two values again and is folded in.
// Display pill plan Task 13 took two of its calls out: Clarity is no longer
// taken off the pan and the waterfall AGC is no longer re-primed, because
// every pan keeps its level through a key.
const QStringList kOldLambdaRise{
    QStringLiteral("setMoxOverlay(true)"),
    QStringLiteral("save sampleRate and ddcCenterFrequency"),
    QStringLiteral("setTxCenterFrequency"),
    QStringLiteral("setTxSampleRate"),
    QStringLiteral("connect txViewWindowChanged"),
    QStringLiteral("setDisplayWindowPreservingHistory(carrier)"),
    QStringLiteral("connect updateSpectrumFromTxPixels"),
    QStringLiteral("setTxExternalWaterfall(true)"),
    QStringLiteral("connect pushTxWaterfallRow"),
    QStringLiteral("clearAvengers"),
};

// What an edge records ahead of the rise and the fall: keyedChanged, then
// one setDisplayKeyed for each pan (in no fixed order).
bool isEdgeCall(const QString& call)
{
    return call.startsWith(QStringLiteral("keyedChanged("))
        || call.startsWith(QStringLiteral("setDisplayKeyed("));
}

QStringList edgeCalls(const QStringList& calls)
{
    QStringList out;
    for (const QString& call : calls) {
        if (isEdgeCall(call)) { out.append(call); }
    }
    return out;
}

QStringList withoutEdgeCalls(const QStringList& calls)
{
    QStringList out;
    for (const QString& call : calls) {
        if (!isEdgeCall(call)) { out.append(call); }
    }
    return out;
}

// The edge's calls: keyedChanged first, then every pan of the stack (its
// own first pan, "one" and "two") once, each before anything of the rise or
// the fall.
bool edgeLeadsTheLog(const QStringList& calls, bool keyed)
{
    const QString state = keyed ? QStringLiteral("true") : QStringLiteral("false");
    const QStringList edge = edgeCalls(calls);
    QStringList pans = edge.mid(1);
    pans.sort();
    const QString told = QStringLiteral("setDisplayKeyed(%1) ").arg(state);
    bool everyPanOnce = pans.size() >= 2 && pans.removeDuplicates() == 0
        && pans.contains(told + QStringLiteral("one"))
        && pans.contains(told + QStringLiteral("two"));
    for (const QString& pan : std::as_const(pans)) {
        everyPanOnce = everyPanOnce && pan.startsWith(told);
    }
    const bool leads = calls.value(0) == QStringLiteral("keyedChanged(%1)").arg(state)
        && everyPanOnce && calls.mid(0, edge.size()) == edge;
    if (!leads) {
        qWarning().noquote() << "edge calls not leading:" << calls.join(QStringLiteral(" | "));
    }
    return leads;
}

// A flat receive row at `dbm`.
QVector<float> flatRow(float dbm) { return QVector<float>(512, dbm); }

// The calls the transmit source owns now (its bins' centre and span, and
// its trace and waterfall); the controller owns the rest.
const QStringList kSourceCalls{
    QStringLiteral("setTxCenterFrequency"),
    QStringLiteral("setTxSampleRate"),
    QStringLiteral("connect updateSpectrumFromTxPixels"),
    QStringLiteral("connect pushTxWaterfallRow"),
};

// The old lambda's fall, in its order.
const QStringList kOldLambdaFall{
    QStringLiteral("setMoxOverlay(false)"),
    QStringLiteral("setTxExternalWaterfall(false)"),
    QStringLiteral("disconnect updateSpectrumFromTxPixels"),
    QStringLiteral("disconnect pushTxWaterfallRow"),
    QStringLiteral("restore sampleRate and ddcCenterFrequency"),
    QStringLiteral("disconnect txViewWindowChanged"),
    QStringLiteral("setTxCenterFrequency(0)"),
    QStringLiteral("clearAvengers"),
};

QStringList only(const QStringList& calls, const QStringList& keep, bool keepThem)
{
    QStringList out;
    for (const QString& call : calls) {
        if (keep.contains(call) == keepThem) {
            out.append(call);
        }
    }
    return out;
}

// What the fall must restore exactly.
struct PanState {
    bool overlay{false};
    float refLevel{0.0f};
    float dynamicRange{0.0f};
    double centreHz{0.0};
    double bandwidthHz{0.0};
    double sampleRate{0.0};
    double ddcHz{0.0};
    bool externalWaterfall{false};
    double txCentreHz{0.0};

    static PanState of(const SpectrumWidget* sw)
    {
        return {sw->m_moxOverlay, sw->m_refLevel,      sw->m_dynamicRange,
                sw->m_centerHz,   sw->m_bandwidthHz,   sw->m_sampleRateHz,
                sw->m_ddcCenterHz, sw->m_txExternalWaterfall, sw->m_txCenterHz};
    }
    bool operator==(const PanState&) const = default;
};

void wheel(SpectrumWidget* sw)
{
    const QPointF pos(sw->width() / 2.0, sw->height() / 4.0);
    QWheelEvent event(pos, sw->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(sw, &event);
}

void click(SpectrumWidget* sw)
{
    const QPoint pos(sw->width() / 3, sw->height() / 6);
    QTest::mousePress(sw, Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseRelease(sw, Qt::LeftButton, Qt::NoModifier, pos);
}

// A window running its own DSP: its RadioModel with a transmit analyzer
// (and so a TxDisplayFeed), two pans, the transmit slice on pan "two".
struct LocalWindow {
    QTemporaryDir directory;
    // One AppSettings per process at a time: a test that stands up both
    // windows gives only the first one its own.
    std::unique_ptr<AppSettings> settings;
    RadioModel radio;
    std::unique_ptr<TxAnalyzer> analyzer;
    PanadapterStack stack;
    int txSliceId{-1};
    int otherSliceId{-1};
    std::unique_ptr<LocalTxDisplaySource> source;
    std::unique_ptr<MoxDisplayController> controller;

    explicit LocalWindow(bool ownSettings = true)
        : settings(ownSettings ? std::make_unique<AppSettings>(
                                     directory.filePath(QStringLiteral("local.settings")))
                               : nullptr)
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        txSliceId = radio.addSlice();
        otherSliceId = radio.addSlice();
        analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId);
        radio.setTxAnalyzer(analyzer.get());
        auto* one = stack.addPanadapter(QStringLiteral("one"));
        auto* two = stack.addPanadapter(QStringLiteral("two"));
        one->setActiveSliceIndex(otherSliceId);
        two->setActiveSliceIndex(txSliceId);
        radio.sliceById(otherSliceId)->setPanKey(QStringLiteral("one"));
        radio.sliceById(txSliceId)->setPanKey(QStringLiteral("two"));
        radio.sliceById(txSliceId)->setFrequency(14'200'000.0);
        radio.sliceById(otherSliceId)->setFrequency(7'100'000.0);
        // A receive view away from the carrier, as an operator leaves it.
        pan(QStringLiteral("two"))->setSampleRate(192000.0);
        pan(QStringLiteral("two"))->setDdcCenterFrequency(14'180'000.0);
        pan(QStringLiteral("two"))->setDisplayWindowPreservingHistory(14'185'000.0, 48000.0);
        stack.resize(900, 700);
        stack.setAttribute(Qt::WA_DontShowOnScreen);
        stack.show();
        source = std::make_unique<LocalTxDisplaySource>(radio.txDisplayFeed());
        controller = std::make_unique<MoxDisplayController>(&stack, &radio, nullptr);
        controller->setSource(source.get());
    }
    ~LocalWindow()
    {
        controller.reset();
        source.reset();
        radio.setTxAnalyzer(nullptr);
    }

    SpectrumWidget* pan(const QString& id) const { return stack.spectrum(id); }
    SliceModel* txSlice() const { return radio.sliceById(txSliceId); }
    bool key(bool on)
    {
        MoxController* mox = radio.moxController();
        if (mox == nullptr) { return false; }
        mox->setMoxCheck({});
        mox->setMox(on);
        return true;
    }
};

class DisplayTransport final : public IMediaTransport {
public:
    explicit DisplayTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
    void stop() override { active = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& packet) override
    {
        if (!active) { return false; }
        if (other) { other->deliver(packet); }
        return true;
    }
    bool sendRtp(const QByteArray&) override { return active; }
    bool isReady() const override { return active; }
    std::optional<MediaTransportTelemetry> telemetry() const override { return std::nullopt; }
    void activate() { active = true; emit ready(); }
    void deliver(const QByteArray& packet) { emit displayReceived(packet); }
    bool active = false;
    QPointer<DisplayTransport> other;
};

// A remote window on an in-process Core: the Core's RadioModel (with or
// without a transmit analyzer), its StationServer and media, and a window
// with two pans whose second hosts the Core's transmit slice.
struct RemoteWindow {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath(QStringLiteral("station.settings"))};
    RadioModel station;
    std::unique_ptr<TxAnalyzer> analyzer;
    int txSliceId{-1};
    int otherSliceId{-1};
    std::unique_ptr<StationServer> server;
    QPointer<DisplayTransport> sourceMedia;
    std::unique_ptr<DaemonMediaController> daemon;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    PanadapterStack stack;
    QPointer<DisplayTransport> sinkMedia;
    std::unique_ptr<RemoteMediaController> gui;
    std::unique_ptr<RemoteTxDisplaySource> source;
    std::unique_ptr<MoxDisplayController> controller;
    QVector<float> iq;

    explicit RemoteWindow(bool withAnalyzer)
    {
        // No real helper beside the test binary may start (NoCaptureHelper.h).
        Test::useNoCaptureHelper(station);
        Test::useNoCaptureHelper(remote);
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        txSliceId = station.addSlice();
        otherSliceId = station.addSlice();
        station.sliceById(txSliceId)->setPanKey(QStringLiteral("two"));
        station.sliceById(otherSliceId)->setPanKey(QStringLiteral("one"));
        if (withAnalyzer) {
            analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId);
            station.setTxAnalyzer(analyzer.get());
        }
        server = std::make_unique<StationServer>(
            &station, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        server->setMediaEnabled(true);
        daemon = std::make_unique<DaemonMediaController>(
            server.get(), &station, nullptr, [this](QObject* owner) -> IMediaTransport* {
                sourceMedia = new DisplayTransport(owner);
                return sourceMedia;
            });
        remote.audioEngine()->setMasterMuted(true);  // No speaker opens.
        client = std::make_unique<StationClient>(&remote, &proxy);
        auto* one = stack.addPanadapter(QStringLiteral("one"));
        auto* two = stack.addPanadapter(QStringLiteral("two"));
        one->setActiveSliceIndex(otherSliceId);
        two->setActiveSliceIndex(txSliceId);
        stack.resize(900, 700);
        stack.setAttribute(Qt::WA_DontShowOnScreen);
        stack.show();
        gui = std::make_unique<RemoteMediaController>(
            client.get(), &remote, &stack, nullptr,
            [this](QObject* owner) -> IMediaTransport* {
                sinkMedia = new DisplayTransport(owner);
                return sinkMedia;
            });
        source = std::make_unique<RemoteTxDisplaySource>(gui.get(), client.get(), &stack);
        controller = std::make_unique<MoxDisplayController>(&stack, &remote, nullptr);
        controller->setSource(source.get());
        controller->followStation(client.get());
        iq.resize(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
    }
    ~RemoteWindow()
    {
        if (client) {
            client->disconnectFromStation(QStringLiteral("test complete"));
        }
        controller.reset();
        source.reset();
        gui.reset();
        client.reset();
        daemon.reset();
        station.setTxAnalyzer(nullptr);
    }

    SpectrumWidget* pan(const QString& id) const { return stack.spectrum(id); }

    bool connect()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client->startSession(clientLink, server->token());
        server->acceptTransport(stationLink);
        if (!QTest::qWaitFor([this] { return sourceMedia && sinkMedia; }, 5000)) {
            return false;
        }
        sourceMedia->other = sinkMedia;
        sourceMedia->activate();
        sinkMedia->activate();
        // Both pans subscribed and their slices mirrored with their pans.
        return QTest::qWaitFor([this] {
            SliceModel* tx = remote.sliceById(txSliceId);
            return daemon->activeEndpointCount() == 2 && tx != nullptr
                && tx->panKey() == station.sliceById(txSliceId)->panKey();
        }, 5000);
    }

    void feed()
    {
        for (int id : {txSliceId, otherSliceId}) {
            SliceModel* slice = station.sliceById(id);
            QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, slice->streamIndex()),
                                      Q_ARG(QVector<float>, iq));
        }
    }

    void key(bool on)
    {
        MoxController* mox = station.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(on);
    }

    // The Core's analyzer poll, as the test seam.
    void emitPlanes(float dbm, int count)
    {
        const QVector<float> plane(count, dbm);
        emit analyzer->txFftReady(-1, plane);
        emit analyzer->txWaterfallReady(-1, plane);
    }

    // Waits for the pan's receive frames to draw (a rendered spectrum).
    bool receiveDraws(const QString& panId, int atLeast = 2)
    {
        QSignalSpy drawn(pan(panId), &SpectrumWidget::spectrumFrameRendered);
        return QTest::qWaitFor([&] {
            feed();
            return drawn.count() >= atLeast;
        }, 5000);
    }
};

} // namespace

class TstMoxDisplayController : public QObject {
    Q_OBJECT

private slots:
    void savedMeasurementPreferenceWinsLateCoordinator_data()
    {
        QTest::addColumn<bool>("saved");
        QTest::newRow("saved-true-default-false") << true;
        QTest::newRow("saved-false-current-true") << false;
    }

    void savedMeasurementPreferenceWinsLateCoordinator()
    {
        QFETCH(bool, saved);
        const MeasurementPreferenceGuard restorePreference;
        LocalWindow window;
        const QString key = QStringLiteral("puresignal/showTwoToneMeasurements");
        AppSettings::instance().setValue(key, saved);
        MeasurementCoordinator coordinator;
        coordinator.setShow2ToneMeasurements(!saved);
        emit window.radio.pureSignalCoordinatorReady(&coordinator);
        QCOMPARE(window.pan(QStringLiteral("two"))->m_showIMDMeasurements, saved);
        MeasurementCoordinator replacement;
        replacement.setShow2ToneMeasurements(!saved);
        emit window.radio.pureSignalCoordinatorReady(&replacement);
        QCOMPARE(window.pan(QStringLiteral("two"))->m_showIMDMeasurements, saved);
        QCOMPARE(AppSettings::instance().value(key).toBool(), saved);
        replacement.setShow2ToneMeasurements(saved);
        replacement.setShow2ToneMeasurements(!saved);
        QCOMPARE(window.pan(QStringLiteral("two"))->m_showIMDMeasurements, !saved);
        QCOMPARE(AppSettings::instance().value(key).toBool(), saved);
    }

    void imdGatesFollowTxHostAndCoordinatorReplacement()
    {
        LocalWindow window;
        window.stack.setActivePan(QStringLiteral("one"));
        MeasurementCoordinator first;
        first.setShow2ToneMeasurements(true);
        emit window.radio.pureSignalCoordinatorReady(&first);
        publishTwoToneState(window.radio, true);
        window.controller->setDisplayDuplex(true);
        window.controller->setKeyed(true, window.txSliceId);
        SpectrumWidget* host = window.pan(QStringLiteral("two"));
        QCOMPARE(window.controller->keyedPanId(), QStringLiteral("two"));
        QVERIFY(host->m_moxOverlay);
        QVERIFY(host->m_displayDuplex);
        QVERIFY2(host->m_showIMDMeasurements, "Late coordinator's current Show Measurements must seed the TX host");
        QVERIFY2(host->m_testingIMD, "Two-tone updates must reach the TX host rather than startup focus");
        window.stack.setActivePan(QStringLiteral("one"));
        first.setShow2ToneMeasurements(false);
        QVERIFY(!host->m_showIMDMeasurements);
        first.setShow2ToneMeasurements(true);
        QVERIFY(host->m_showIMDMeasurements);
        MeasurementCoordinator replacement;
        replacement.setShow2ToneMeasurements(true);
        emit window.radio.pureSignalCoordinatorReady(&replacement);
        emit window.radio.pureSignalCoordinatorReady(&replacement);
        QCOMPARE(first.displayConnectionCount(), 0);
        QCOMPARE(replacement.displayConnectionCount(), 1);
        first.setShow2ToneMeasurements(false);
        QVERIFY2(host->m_showIMDMeasurements, "A replaced coordinator must no longer write display state");
        replacement.setShow2ToneMeasurements(false);
        QVERIFY(!host->m_showIMDMeasurements);
        publishTwoToneState(window.radio, false);
        QVERIFY(!host->m_testingIMD);
        window.controller->setDisplayDuplex(false);
        QVERIFY(!host->m_displayDuplex);
        window.controller->setKeyed(false, window.txSliceId);
        QVERIFY(!host->m_moxOverlay);
    }

    void imdStateSeedsCreatedFloatedAndRestoredPan()
    {
        LocalWindow window;
        PureSignal coordinator(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        coordinator.setShow2ToneMeasurements(true);
        emit window.radio.pureSignalCoordinatorReady(&coordinator);
        publishTwoToneState(window.radio, true);
        PanadapterApplet* created = window.stack.addPanadapter(QStringLiteral("created"));
        SpectrumWidget* spectrum = created->spectrumWidget();
        QVERIFY2(spectrum->m_showIMDMeasurements, "A newly created pan must inherit current measurement gates");
        QVERIFY(spectrum->m_testingIMD);
        created->setActiveSliceIndex(window.txSliceId);
        window.txSlice()->setPanKey(QStringLiteral("created"));
        window.controller->setDisplayDuplex(true);
        window.controller->setKeyed(true, window.txSliceId);
        QCOMPARE(window.controller->keyedPanId(), QStringLiteral("created"));
        QVERIFY(spectrum->m_moxOverlay);
        QVERIFY(spectrum->m_displayDuplex);
        window.stack.floatPanadapter(QStringLiteral("created"));
        QVERIFY(spectrum->m_showIMDMeasurements);
        QVERIFY(spectrum->m_moxOverlay);
        window.stack.dockPanadapter(QStringLiteral("created"));
        QCOMPARE(window.stack.spectrum(QStringLiteral("created")), spectrum);
        QVERIFY(spectrum->m_testingIMD);
        QVERIFY(spectrum->m_moxOverlay);
        window.controller->setKeyed(false, window.txSliceId);
        window.stack.removePanadapter(QStringLiteral("created"));
        created = window.stack.addPanadapter(QStringLiteral("created"));
        created->setActiveSliceIndex(window.txSliceId);
        QVERIFY(created->spectrumWidget()->m_showIMDMeasurements);
        QVERIFY(created->spectrumWidget()->m_testingIMD);
        window.controller->setKeyed(true, window.txSliceId);
        QVERIFY(created->spectrumWidget()->m_moxOverlay);
        QVERIFY(created->spectrumWidget()->m_displayDuplex);
        coordinator.setShow2ToneMeasurements(false);
        QVERIFY(!created->spectrumWidget()->m_showIMDMeasurements);
        window.controller->setKeyed(false, window.txSliceId);
    }

    void remoteFacadeSeedsAndUpdatesTestingGate()
    {
        const MeasurementPreferenceGuard restorePreference;
        const QString key = QStringLiteral("puresignal/showTwoToneMeasurements");
        AppSettings::instance().setValue(key, true);
        RadioModel radio(RadioModel::Role::Remote);
        PanadapterStack stack;
        SpectrumWidget* before = stack.addPanadapter(QStringLiteral("before"))->spectrumWidget();
        QVERIFY(radio.pureSignalFacade()->applyRemoteProperty("twoToneOn", true));
        MoxDisplayController controller(&stack, &radio, nullptr);
        QVERIFY2(before->m_testingIMD, "Already-active remote two-tone state must seed existing pans");
        QVERIFY2(before->m_showIMDMeasurements, "A remote window with no local coordinator keeps its saved Show preference");
        SpectrumWidget* after = stack.addPanadapter(QStringLiteral("after"))->spectrumWidget();
        QVERIFY(after->m_testingIMD);
        QVERIFY(after->m_showIMDMeasurements);
        QVERIFY(radio.pureSignalFacade()->applyRemoteProperty("twoToneOn", false));
        QVERIFY(!before->m_testingIMD);
        QVERIFY(!after->m_testingIMD);
    }

    void keyedNotificationIsAnEdgeAndInitialTruth()
    {
        RadioModel model(RadioModel::Role::Remote);
        PanadapterStack stack;
        MoxDisplayController display(&stack, &model, nullptr);
        QSignalSpy edges(&display, &MoxDisplayController::keyedChanged);
        bool observed = false;
        connect(&display, &MoxDisplayController::keyedChanged, &display,
                [&](bool keyed) { observed = display.isKeyed() == keyed; });
        display.setKeyed(true, -1); QVERIFY(observed); QCOMPARE(edges.size(), 1);
        display.setKeyed(true, -1); QCOMPARE(edges.size(), 1);
        display.setKeyed(false, -1); QCOMPARE(edges.size(), 2); QVERIFY(observed);
    }
    void waterfallWindowChangeDuringResizeKeepsRfAlignment_data()
    {
        QTest::addColumn<int>("newWidth");
        QTest::addColumn<double>("centerShiftHz");
        QTest::newRow("grow-same-window") << 960 << 0.0;
        QTest::newRow("shrink-same-window") << 240 << 0.0;
        QTest::newRow("grow-new-window") << 960 << 1000.0;
        QTest::newRow("shrink-new-window") << 240 << 1000.0;
    }

    void waterfallWindowChangeDuringResizeKeepsRfAlignment()
    {
        QFETCH(int, newWidth);
        QFETCH(double, centerShiftHz);
        SpectrumWidget pan;
        pan.setWaterfallTickerPausedForTest(true);
        pan.setWfAgcEnabled(false);
        pan.setWaterfallNFAGCEnabled(false);
        pan.m_remoteSpectrum = true;
        pan.m_waterfall = QImage(480, 1, QImage::Format_RGB32);
        pan.m_waterfall.fill(Qt::black);
        constexpr double centerHz = 14'200'000.0;
        constexpr double spanHz = 48000.0;
        pan.setDisplayWindowPreservingHistory(centerHz, spanHz);
        QVector<float> row(480, -160.0f);
        row[240] = 10.0f;
        row[241] = 10.0f;
        // Two retained rows keep the red probe inside the history
        // allocation even when the old width assumption reads a second
        // scanline. The assertion observes wrong RF placement safely.
        pan.pushWaterfallRowForTest(row);
        pan.pushWaterfallRowForTest(row);
        QCOMPARE(pan.m_waterfallHistory.width(), 480);
        // Model resizeEvent's immediate image replacement before its
        // debounced history resize has had a chance to run.
        pan.m_waterfall = QImage(newWidth, 1, QImage::Format_RGB32);
        pan.m_waterfall.fill(Qt::black);
        pan.rebuildWaterfallViewport(centerHz + centerShiftHz, spanHz);
        int brightest = -1;
        int brightness = -1;
        for (int x = 0; x < newWidth; ++x) {
            const QColor color = pan.m_waterfall.pixelColor(x, 0);
            const int value = color.red() + color.green() + color.blue();
            if (value > brightness) { brightness = value; brightest = x; }
        }
        const int expected = int((spanHz / 2.0 - centerShiftHz) / spanHz * newWidth);
        QVERIFY2(std::abs(brightest - expected) <= 1,
                 "History must track the resized live width before RF projection");
        QCOMPARE(pan.m_waterfallHistory.width(), newWidth);
        QCOMPARE(pan.waterfallHistoryRowsForTest(), 2);
    }

    void transmitHistoryReturnsAtItsCapturedRfFrequency_data()
    {
        QTest::addColumn<double>("txOffsetHz");
        QTest::newRow("carrier-above-rx-centre") << 15000.0;
        QTest::newRow("carrier-below-rx-centre") << -15000.0;
    }

    void transmitHistoryReturnsAtItsCapturedRfFrequency()
    {
        QFETCH(double, txOffsetHz);
        SpectrumWidget pan;
        pan.setWaterfallTickerPausedForTest(true);
        pan.setWfAgcEnabled(false);
        pan.setWaterfallNFAGCEnabled(false);
        pan.setWaterfallStopOnTx(false);
        pan.m_remoteSpectrum = true;
        pan.m_waterfall = QImage(480, 16, QImage::Format_RGB32);
        pan.m_waterfall.fill(Qt::black);
        constexpr double rxCentreHz = 14'200'000.0;
        constexpr double rxSpanHz = 48000.0;
        const double txCentreHz = rxCentreHz + txOffsetHz;
        const double toneHz = txCentreHz + 1000.0;
        pan.setDisplayWindowPreservingHistory(rxCentreHz, rxSpanHz);
        pan.m_txViewBandwidthHz = 8000.0;

        const auto columnFor = [&pan](double hz) {
            return int((hz - pan.centerFrequency() + pan.bandwidth() / 2.0)
                       / pan.bandwidth() * pan.m_waterfall.width());
        };
        const auto pushTone = [&pan, &columnFor](double hz) {
            QVector<float> row(pan.m_waterfall.width(), -160.0f);
            // A finite RF-width marker survives the narrower TX pixel
            // grid being projected onto RX pixels (100 Hz wide here).
            const int columns = std::max(1, int(std::ceil(200.0 / pan.bandwidth()
                                               * pan.m_waterfall.width())));
            for (int x = columnFor(hz); x < columnFor(hz) + columns; ++x) {
                row[x] = 10.0f;
            }
            pan.pushWaterfallRowForTest(row);
        };
        const auto liveRow = [&pan](int age) {
            const int y = (pan.liveWaterfallWriteRowForTest() + age)
                        % pan.liveWaterfallForTest().height();
            return pan.liveWaterfallForTest().copy(0, y, pan.m_waterfall.width(), 1);
        };
        const auto brightestColumn = [](const QImage& row) {
            int best = -1;
            int brightness = -1;
            for (int x = 0; x < row.width(); ++x) {
                const QColor color = row.pixelColor(x, 0);
                const int value = color.red() + color.green() + color.blue();
                if (value > brightness) { brightness = value; best = x; }
            }
            return best;
        };

        // The original RX row lies outside the TX window. It must return
        // byte-for-byte, rather than be lost by an RX->TX->RX image stretch.
        pushTone(rxCentreHz - txOffsetHz);
        const QImage originalRx = liveRow(0);
        for (int cycle = 0; cycle < 2; ++cycle) {
            pan.setMoxOverlay(true);
            pan.setDisplayWindowPreservingHistory(txCentreHz, pan.bandwidth());
            pushTone(toneHz);
            QCOMPARE(brightestColumn(liveRow(0)), columnFor(toneHz));
            pan.setMoxOverlay(false);
            QCOMPARE(pan.centerFrequency(), rxCentreHz);
            QCOMPARE(pan.bandwidth(), rxSpanHz);
            QVERIFY2(std::abs(brightestColumn(liveRow(0)) - columnFor(toneHz)) <= 1,
                     "Retained TX row must use its RF frequency on the restored RX axis");
            QCOMPARE(liveRow(cycle + 1), originalRx);
        }
        // Fresh RX rows and the retained TX rows must now share one axis.
        pushTone(toneHz);
        QCOMPARE(brightestColumn(liveRow(0)), columnFor(toneHz));
        QVERIFY(std::abs(brightestColumn(liveRow(1)) - columnFor(toneHz)) <= 1);
        const int rows = pan.waterfallHistoryRowsForTest();
        pan.m_wfLive = false;
        pan.m_wfHistoryOffsetRows = 1;
        pan.rebuildWaterfallViewport();
        // Rewind and a subsequent accepted window change must also use
        // the captured TX geometry, without rewriting the history ring.
        pan.setDisplayWindowPreservingHistory(rxCentreHz + 100.0, rxSpanHz);
        QVERIFY(std::abs(brightestColumn(liveRow(0)) - columnFor(toneHz)) <= 1);
        QCOMPARE(pan.waterfallHistoryRowsForTest(), rows);
        pan.setDisplayWindowPreservingHistory(rxCentreHz, rxSpanHz);
        pan.m_wfLive = true;
        pan.m_wfHistoryOffsetRows = 0;
        pan.rebuildWaterfallViewport();
        QCOMPARE(liveRow(3), originalRx);
    }

    // Opt-in, offscreen evidence capture for the iPhone Task 54f software gate.
    // This is the real remote pan stack and media path, without a radio.
    void remoteTransmitDisplayEvidence()
    {
        const QString output = qEnvironmentVariable("NEREUS_TX_DISPLAY_EVIDENCE");
        if (output.isEmpty()) { QSKIP("Set NEREUS_TX_DISPLAY_EVIDENCE to capture PNGs"); }
        QStandardPaths::setTestModeEnabled(true);
        RemoteWindow window(/*withAnalyzer=*/true);
        auto* coreSlice = window.station.sliceById(window.txSliceId);
        coreSlice->setFrequency(7'236'400.0);
        coreSlice->setDspMode(DSPMode::LSB);
        coreSlice->setFilterLow(-3000);
        coreSlice->setFilterHigh(-100);
        coreSlice->setTxSlice(true);
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        two->setSampleRate(96'000.0);
        two->setDdcCenterFrequency(7'236'400.0);
        two->setDisplayWindowPreservingHistory(7'236'400.0, 8'000.0);
        QVERIFY(window.connect());
        two->setTxMode(DSPMode::LSB);
        two->setVfoFrequency(7'236'400.0);
        two->setTxFilterRange(100, 2900);
        two->setTxFilterVisible(true);
        two->setDisplayFps(15);
        window.stack.applyLayout(QStringLiteral("2h"),
                                 {QStringLiteral("one"), QStringLiteral("two")});
        window.stack.resize(1206, 900);
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QTRY_VERIFY_WITH_TIMEOUT(two->m_txCenterHz != 0.0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!contexts.isEmpty(), 5000);
        const auto context = contexts.last().at(1).value<SpectrumContextMessage>();
        QVERIFY(context.traceSamples > 0);
        two->setDisplayWindowPreservingHistory(7'236'400.0, 8'000.0);
        QCoreApplication::processEvents();
        const int count = context.traceSamples;
        const double low = 7'236'400.0 - 4'000.0;
        const double binHz = 8'000.0 / count;
        for (int row = 0; row < 600; ++row) {
            QVector<float> trace(count);
            for (int index = 0; index < count; ++index) {
                const double hz = low + (index + 0.5) * binHz;
                const float wobble = float(std::sin(index * 0.37 + row * 0.21)) * 3.0f;
                if (hz >= 7'233'500.0 && hz <= 7'236'300.0) {
                    const float syllable = float(std::sin(hz / 420.0 + row * 0.09)) * 9.0f;
                    trace[index] = -18.0f + syllable + wobble;
                } else {
                    const double skirt = hz > 7'236'300.0 ? hz - 7'236'300.0 : 7'233'500.0 - hz;
                    trace[index] = std::max(-66.0f + wobble, -30.0f - float(skirt / 60.0));
                }
            }
            emit window.analyzer->txFftReady(-1, trace);
            emit window.analyzer->txWaterfallReady(-1, trace);
            // The media budget drops burst rows. Fill the screenshot's
            // history through the same widget paint input while the real
            // context and transmitted trace travel through media.
            two->pushTxWaterfallRow(-1, trace);
            QCoreApplication::processEvents();
        }
        QTRY_VERIFY_WITH_TIMEOUT(!two->m_renderedPixels.isEmpty(), 5000);
        QVERIFY(window.stack.grab().save(output + QStringLiteral("/desktop-keyed-pan-stack.png")));

        window.station.swrProt().setEnabled(true);
        window.station.swrProt().setWindBackEnabled(true);
        for (int sample = 0; sample < 50 && !window.station.swrProt().highSwr(); ++sample) {
            window.station.swrProt().ingest(50.0f, 30.0f, false);
        }
        QTRY_VERIFY(two->isHighSwrOverlayActive());
        QVERIFY(two->isHighSwrFoldback());
        QCoreApplication::processEvents();
        QVERIFY(window.stack.grab().save(output + QStringLiteral("/desktop-high-swr-pan-stack.png")));
        window.key(false);
    }

    // ── The local window unchanged ─────────────────────────────────────────

    void localRiseAndFallMakeTheOldLambdasCalls()
    {
        LocalWindow window;
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        const PanState before = PanState::of(two);
        const PanState oneBefore = PanState::of(one);
        const float rxRef = two->m_refLevel;
        const float txRef = two->m_txRefLevel;
        const double carrier =
            double(window.radio.txFrequencyForSlice(window.txSlice()));

        // Clarity is showing on both pans before the key.
        one->setClarityActive(true);
        two->setClarityActive(true);

        window.controller->setCallRecording(true);
        window.controller->setKeyed(true, window.txSliceId);
        // The edge comes first: keyedChanged, then every pan.
        QVERIFY(edgeLeadsTheLog(window.controller->callLog(), true));
        const QStringList rise = withoutEdgeCalls(window.controller->callLog());

        // The same calls as the old lambda, less the two a key no longer
        // makes (Clarity off, the waterfall AGC re-primed): the controller's
        // own in the old order, then the transmit source's in the old order,
        // then the view signal hooked last (it was hooked before the view
        // move; the move then fed the analyzer the view the source is now
        // handed directly).
        const QStringList expectedRise{
            QStringLiteral("setMoxOverlay(true)"),
            QStringLiteral("save sampleRate and ddcCenterFrequency"),
            QStringLiteral("setDisplayWindowPreservingHistory(carrier)"),
            QStringLiteral("setTxExternalWaterfall(true)"),
            QStringLiteral("clearAvengers"),
            QStringLiteral("setTxCenterFrequency"),
            QStringLiteral("setTxSampleRate"),
            QStringLiteral("connect updateSpectrumFromTxPixels"),
            QStringLiteral("connect pushTxWaterfallRow"),
            QStringLiteral("connect txViewWindowChanged"),
        };
        QCOMPARE(rise, expectedRise);
        QStringList sortedRise = rise;
        QStringList sortedOld = kOldLambdaRise;
        sortedRise.sort();
        sortedOld.sort();
        QCOMPARE(sortedRise, sortedOld);
        const QStringList controllerOwned{
            QStringLiteral("setMoxOverlay(true)"),
            QStringLiteral("save sampleRate and ddcCenterFrequency"),
            QStringLiteral("setDisplayWindowPreservingHistory(carrier)"),
            QStringLiteral("setTxExternalWaterfall(true)"),
            QStringLiteral("clearAvengers"),
        };
        QVERIFY(!rise.contains(QStringLiteral("setClarityActive(false)")));
        QVERIFY(!rise.contains(QStringLiteral("resetWaterfallAgc")));
        QCOMPARE(only(rise, controllerOwned, true),
                 only(kOldLambdaRise, controllerOwned, true));
        QCOMPARE(only(rise, kSourceCalls, true), only(kOldLambdaRise, kSourceCalls, true));

        // On the pan hosting the transmit slice, and only there.
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(two->m_moxOverlay);
        QCOMPARE(two->m_refLevel, txRef);
        QVERIFY(two->m_txExternalWaterfall);
        // Clarity stays on every pan through the key (#359).
        QVERIFY(two->m_clarityActive);
        QVERIFY(one->m_clarityActive);
        QCOMPARE(two->m_centerHz, carrier);
        QCOMPARE(two->m_txWfLowLevel, -70);
        QCOMPARE(two->m_txWfHighLevel, 30);
        QCOMPARE(PanState::of(one), oneBefore);
        // This window's pan is the feed's local viewer, and it governs.
        TxDisplayFeed* feed = window.radio.txDisplayFeed();
        QCOMPARE(feed->viewerCount(), 1);
        QVERIFY(feed->isGoverning(window.source->viewerId()));
        QCOMPARE(two->m_txCenterHz, feed->currentView().centreHz());
        QCOMPARE(two->m_txSampleRateHz, feed->currentView().spanHz());

        window.controller->clearCallLog();
        window.controller->setKeyed(false, -1);
        QVERIFY(edgeLeadsTheLog(window.controller->callLog(), false));
        QCOMPARE(withoutEdgeCalls(window.controller->callLog()), kOldLambdaFall);
        QVERIFY(!window.controller->callLog().contains(QStringLiteral("resetWaterfallAgc")));
        QVERIFY(two->m_clarityActive);
        QVERIFY(one->m_clarityActive);
        QCOMPARE(PanState::of(two), before);
        QCOMPARE(two->m_refLevel, rxRef);
        QVERIFY(window.controller->transmitPanId().isEmpty());
        QCOMPARE(feed->viewerCount(), 0);
    }

    // The rise follows MoxController with the TX-bound slice, and the feed
    // (keyed on the same edge) gets the pan's view.
    void localKeyFollowsMoxController()
    {
        LocalWindow window;
        window.controller->followLocalRadio();
        QVERIFY(window.key(true));
        QTRY_VERIFY(window.controller->isKeyed());
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        TxDisplayFeed* feed = window.radio.txDisplayFeed();
        QVERIFY(feed->isKeyed());
        QVERIFY(feed->isGoverning(window.source->viewerId()));
        QVERIFY(window.key(false));
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(!window.pan(QStringLiteral("two"))->m_moxOverlay);
    }

    // ── Display pill plan Task 13: a key edge on every pan ─────────────────

    // Every pan keeps its waterfall AGC level through a key, DUP on or off:
    // rows 40 dB hotter move nothing while keyed, the un-key puts back the
    // level each pan had, and each pan then holds for its own length.
    void keyKeepsEveryPansLevelAndTheFallRestoresIt_data()
    {
        QTest::addColumn<bool>("duplex");
        QTest::newRow("transmit view") << false;
        QTest::newRow("display duplex") << true;
    }
    void keyKeepsEveryPansLevelAndTheFallRestoresIt()
    {
        QFETCH(bool, duplex);
        LocalWindow window;
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        qint64 now = 5'000'000;
        one->setDisplayClockForTest([&now]() { return now; });
        two->setDisplayClockForTest([&now]() { return now; });
        // Pan one shows a long FFT; pan two has no engine of its own.
        one->setLocalFftShape(262144, 1, 48000.0);
        QCOMPARE(one->moxHoldLengthMs(), qint64(10922));
        QCOMPARE(two->moxHoldLengthMs(), qint64(1000));
        window.controller->setDisplayDuplex(duplex);

        for (int row = 0; row < 200; ++row) {
            one->composeWaterfallActiveThresholds(flatRow(-110.0f));
            two->composeWaterfallActiveThresholds(flatRow(-120.0f));
        }
        QVERIFY(one->waterfallAgcRunLevel().has_value());
        QVERIFY(two->waterfallAgcRunLevel().has_value());
        const float levelOne = *one->waterfallAgcRunLevel();
        const float levelTwo = *two->waterfallAgcRunLevel();
        const float lowOne = one->wfActiveLowThreshold();
        QVERIFY(std::abs(levelOne - levelTwo) > 5.0f);

        QSignalSpy edges(window.controller.get(), &MoxDisplayController::keyedChanged);
        window.controller->setCallRecording(true);
        window.controller->setKeyed(true, window.txSliceId);
        QCOMPARE(edges.count(), 1);
        QVERIFY(edgeLeadsTheLog(window.controller->callLog(), true));
        QVERIFY(one->displayKeyed());
        QVERIFY(two->displayKeyed());
        // The transmitting pan draws with its transmit levels.
        QVERIFY(two->m_moxOverlay);
        QCOMPARE(two->effectiveWaterfallLevelsForTest(),
                 (std::pair<float, float>(-70.0f, 30.0f)));

        // While keyed the receiver hears the transmitter.
        for (int row = 0; row < 50; ++row) {
            one->composeWaterfallActiveThresholds(flatRow(-70.0f));
            two->composeWaterfallActiveThresholds(flatRow(-80.0f));
        }
        QCOMPARE(*one->waterfallAgcRunLevel(), levelOne);
        QCOMPARE(one->wfActiveLowThreshold(), lowOne);
        QCOMPARE(*two->waterfallAgcRunLevel(), levelTwo);

        now += 4000;
        window.controller->clearCallLog();
        window.controller->setKeyed(false, -1);
        QCOMPARE(edges.count(), 2);
        QVERIFY(edgeLeadsTheLog(window.controller->callLog(), false));
        QVERIFY(!window.controller->callLog().contains(QStringLiteral("resetWaterfallAgc")));
        QVERIFY(!one->displayKeyed());
        QVERIFY(!two->displayKeyed());
        // The first receive rows are still hot (the FFT is refilling): the
        // level is the one from before the key.
        one->composeWaterfallActiveThresholds(flatRow(-70.0f));
        two->composeWaterfallActiveThresholds(flatRow(-80.0f));
        QCOMPARE(*one->waterfallAgcRunLevel(), levelOne);
        QCOMPARE(one->wfActiveLowThreshold(), lowOne);
        QCOMPARE(*two->waterfallAgcRunLevel(), levelTwo);
        // Each pan holds for its own length.
        QVERIFY(one->m_wfLevels.inHold(now + 10921));
        QVERIFY(!one->m_wfLevels.inHold(now + 10922));
        QVERIFY(two->m_wfLevels.inHold(now + 999));
        QVERIFY(!two->m_wfLevels.inHold(now + 1000));
        // The noise floor goes to fast attack on every pan.
        QVERIFY(one->noiseFloorFastAttack());
        QVERIFY(two->noiseFloorFastAttack());
    }

    // A band changed while keyed: a band with no saved level starts at the
    // restart floor, and a band with one gets it back.
    void bandChangedWhileKeyedStartsAtTheRestartFloor()
    {
        LocalWindow window;
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QHash<QString, int> bands{{QStringLiteral("one"), 3}, {QStringLiteral("two"), 5}};
        window.controller->setPanBandProvider(
            [&bands](const QString& panId) { return bands.value(panId, -1); });
        for (int row = 0; row < 200; ++row) {
            one->composeWaterfallActiveThresholds(flatRow(-110.0f));
            two->composeWaterfallActiveThresholds(flatRow(-120.0f));
        }
        const float levelOne = *one->waterfallAgcRunLevel();
        const float levelTwo = *two->waterfallAgcRunLevel();

        window.controller->setKeyed(true, window.txSliceId);
        bands[QStringLiteral("one")] = 4;
        window.controller->setKeyed(false, -1);
        QCOMPARE(*one->waterfallAgcRunLevel(), WaterfallLevelFollower::kAgcRestartFloorDbm);
        QCOMPARE(*one->waterfallAgcRunLevel(), -150.0f);
        QCOMPARE(*two->waterfallAgcRunLevel(), levelTwo);

        // Back on the first band while keyed again: its level was kept.
        window.controller->setKeyed(true, window.txSliceId);
        bands[QStringLiteral("one")] = 3;
        window.controller->setKeyed(false, -1);
        QCOMPARE(*one->waterfallAgcRunLevel(), levelOne);
    }

    // A key shorter than the RF delay never reaches the display: no edge,
    // no purge, no hold and no change on any pan.
    void keyShorterThanTheRfDelayChangesNothing()
    {
        LocalWindow window;
        window.controller->followLocalRadio();
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        qint64 now = 5'000'000;
        one->setDisplayClockForTest([&now]() { return now; });
        two->setDisplayClockForTest([&now]() { return now; });
        one->setClarityActive(true);
        for (int row = 0; row < 200; ++row) {
            two->composeWaterfallActiveThresholds(flatRow(-120.0f));
        }
        const float levelTwo = *two->waterfallAgcRunLevel();
        const bool fastOne = one->noiseFloorFastAttack();
        const bool fastTwo = two->noiseFloorFastAttack();
        const PanState oneBefore = PanState::of(one);
        const PanState twoBefore = PanState::of(two);

        MoxController* mox = window.radio.moxController();
        QVERIFY(mox != nullptr);
        QSignalSpy radioEdges(mox, &MoxController::moxStateChanged);
        QSignalSpy edges(window.controller.get(), &MoxDisplayController::keyedChanged);
        window.controller->setCallRecording(true);
        // Released before the RF delay's timer can run.
        QVERIFY(window.key(true));
        QVERIFY(window.key(false));
        // The radio reports only the release.
        QTRY_COMPARE(radioEdges.count(), 1);
        QCOMPARE(radioEdges.at(0).at(0).toBool(), false);
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();

        QVERIFY(!window.controller->isKeyed());
        QCOMPARE(edges.count(), 0);
        QVERIFY(window.controller->callLog().isEmpty());
        QVERIFY(!one->displayKeyed());
        QVERIFY(!two->displayKeyed());
        QVERIFY(one->clarityActive());
        QCOMPARE(*two->waterfallAgcRunLevel(), levelTwo);
        QVERIFY(!one->m_wfLevels.inHold(now));
        QVERIFY(!two->m_wfLevels.inHold(now));
        QCOMPARE(one->noiseFloorFastAttack(), fastOne);
        QCOMPARE(two->noiseFloorFastAttack(), fastTwo);
        QCOMPARE(PanState::of(one), oneBefore);
        QCOMPARE(PanState::of(two), twoBefore);

        // A repeat of the state the display is already in does nothing too.
        window.controller->setKeyed(false, -1);
        QCOMPARE(edges.count(), 0);
        QVERIFY(window.controller->callLog().isEmpty());
    }

    // MOX, TUNE and a CAT key all arrive as the one edge, with one result.
    void everyKeySourceMakesTheSameEdge_data()
    {
        QTest::addColumn<QString>("source");
        QTest::newRow("MOX") << QStringLiteral("mox");
        QTest::newRow("TUNE") << QStringLiteral("tune");
        QTest::newRow("CAT") << QStringLiteral("cat");
    }
    void everyKeySourceMakesTheSameEdge()
    {
        QFETCH(QString, source);
        LocalWindow window;
        window.controller->followLocalRadio();
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        one->setClarityActive(true);
        two->setClarityActive(true);
        MoxController* mox = window.radio.moxController();
        QVERIFY(mox != nullptr);
        mox->setMoxCheck({});
        const auto key = [&](bool on) {
            if (source == QStringLiteral("tune")) {
                mox->setTune(on);
            } else if (source == QStringLiteral("cat")) {
                mox->onCatPtt(on);
            } else {
                mox->setMox(on);
            }
        };
        QSignalSpy edges(window.controller.get(), &MoxDisplayController::keyedChanged);
        window.controller->setCallRecording(true);

        key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QCOMPARE(edges.count(), 1);
        QVERIFY(edgeLeadsTheLog(window.controller->callLog(), true));
        QStringList rise = withoutEdgeCalls(window.controller->callLog());
        QStringList expectedRise = kOldLambdaRise;
        rise.sort();
        expectedRise.sort();
        QCOMPARE(rise, expectedRise);
        QVERIFY(one->displayKeyed());
        QVERIFY(two->displayKeyed());
        QVERIFY(one->clarityActive());
        QVERIFY(two->clarityActive());
        QVERIFY(two->m_moxOverlay);
        QVERIFY(!one->m_moxOverlay);

        window.controller->clearCallLog();
        key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QCOMPARE(edges.count(), 2);
        QVERIFY(edgeLeadsTheLog(window.controller->callLog(), false));
        QCOMPARE(withoutEdgeCalls(window.controller->callLog()), kOldLambdaFall);
        QVERIFY(!one->displayKeyed());
        QVERIFY(!two->displayKeyed());
        QVERIFY(one->clarityActive());
        QVERIFY(two->clarityActive());
        QVERIFY(one->noiseFloorFastAttack());
        QVERIFY(two->noiseFloorFastAttack());
    }

    // ── Row 16: XIT while keyed ────────────────────────────────────────────

    void localXitWhileKeyedMovesTheViewAndRestores()
    {
        LocalWindow window;
        window.controller->followLocalRadio();
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        const PanState before = PanState::of(two);
        QVERIFY(window.key(true));
        QTRY_VERIFY(window.controller->isKeyed());
        const double centreKeyed = two->m_centerHz;
        const double txCentreKeyed = two->m_txCenterHz;
        SliceModel* slice = window.txSlice();
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        // Synchronously from the feed: no frame period passes.
        QCOMPARE(two->m_centerHz, centreKeyed + 700.0);
        QCOMPARE(two->m_txCenterHz, txCentreKeyed + 700.0);
        QCOMPARE(two->m_txVfoOffsetHz, 700);
        QCOMPARE(window.controller->carrierHz(),
                 double(window.radio.txFrequencyForSlice(slice)));
        slice->setXitHz(-300);
        QCOMPARE(two->m_centerHz, centreKeyed - 300.0);
        QCOMPARE(two->m_txVfoOffsetHz, -300);
        QVERIFY(window.key(false));
        QTRY_VERIFY(!window.controller->isKeyed());
        QCOMPARE(PanState::of(two), before);
    }

    void tuneDisplayOffsetRestoresCurrentXit_data()
    {
        QTest::addColumn<bool>("remoteWindow");
        QTest::addColumn<DSPMode>("mode");
        QTest::addColumn<int>("xitHz");
        QTest::addColumn<bool>("duplexBeforeRelease");
        for (bool remoteWindow : {false, true}) {
            for (DSPMode mode : {DSPMode::LSB, DSPMode::USB}) {
                for (int xitHz : {0, 350}) {
                    for (bool duplexBeforeRelease : {false, true}) {
                        const QByteArray name = QByteArray(remoteWindow ? "remote-" : "local-")
                            + (mode == DSPMode::LSB ? "LSB-" : "USB-")
                            + QByteArray::number(xitHz) + (duplexBeforeRelease ? "-DUP" : "-TX");
                        QTest::newRow(name.constData()) << remoteWindow << mode << xitHz
                                                     << duplexBeforeRelease;
                    }
                }
            }
        }
    }

    void tuneDisplayOffsetRestoresCurrentXit()
    {
        QFETCH(bool, remoteWindow);
        QFETCH(DSPMode, mode);
        QFETCH(int, xitHz);
        QFETCH(bool, duplexBeforeRelease);
        // Inject the already-compensated display context without asking a
        // RadioModel to Tune or key a radio. Both production display sources
        // report this NCO origin to the same presentation controller.
        const auto exercise = [&](RadioModel& model, int sliceId, SpectrumWidget* pan,
                                  MoxDisplayController* controller,
                                  const std::function<void(double)>& context) {
            SliceModel* slice = model.sliceById(sliceId);
            QVERIFY(slice);
            slice->setFrequency(3'865'000.0);
            slice->setDspMode(mode);
            slice->setXitHz(xitHz);
            slice->setXitEnabled(xitHz != 0);
            pan->setTxVfoOffsetHz(xitHz);
            const double carrier = double(model.txFrequencyForSlice(slice));
            const double signedTone = mode == DSPMode::LSB ? -600.0 : 600.0;
            // Give the local feed the same authoritative origin as the
            // injected remote context, without running Tune's side effects.
            model.m_isTuning = true;
            model.m_savedTxDspMode = mode;
            controller->setKeyed(true, sliceId);
            context(carrier - signedTone);
            QCOMPARE(pan->m_txVfoOffsetHz, xitHz - int(signedTone));

            if (duplexBeforeRelease) {
                controller->setDisplayDuplex(true);
            }
            model.m_isTuning = false;
            controller->setKeyed(false, sliceId);
            QCOMPARE(pan->m_txVfoOffsetHz, xitHz);
            controller->setDisplayDuplex(false);
            controller->setKeyed(true, sliceId);
            context(carrier); // Ordinary SSB context equals the rise's seed.
            QCOMPARE(pan->m_txVfoOffsetHz, xitHz);
            controller->setKeyed(false, sliceId);
        };
        if (remoteWindow) {
            RemoteWindow window(/*withAnalyzer=*/true);
            QVERIFY(window.connect());
            exercise(window.remote, window.txSliceId, window.pan(QStringLiteral("two")),
                     window.controller.get(), [&](double origin) {
                SpectrumContextMessage context;
                context.sourceCentreHz = origin;
                context.centreHz = origin;
                context.spanHz = 8000.0;
                emit window.gui->transmitContextReceived(QStringLiteral("two"), context);
            });
        } else {
            LocalWindow window;
            exercise(window.radio, window.txSliceId, window.pan(QStringLiteral("two")),
                     window.controller.get(), [&](double origin) {
                TxDisplayView view;
                view.carrierHz = origin;
                view.lowHz = -4000;
                view.highHz = 4000;
                view.pixels = 1024;
                emit window.radio.txDisplayFeed()->viewChanged(view);
            });
        }
    }

    void remoteXitWhileKeyedMovesTheViewAndRestores()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        const PanState before = PanState::of(two);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        // The first transmit context lands on the carrier view.
        QTRY_VERIFY_WITH_TIMEOUT(two->m_txCenterHz != 0.0, 5000);
        const double centreKeyed = two->m_centerHz;
        SliceModel* coreSlice = window.station.sliceById(window.txSliceId);
        coreSlice->setXitHz(700);
        coreSlice->setXitEnabled(true);
        // The Core's renewed context moves the pan's view and bins.
        QTRY_COMPARE_WITH_TIMEOUT(two->m_centerHz, centreKeyed + 700.0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(two->m_txCenterHz - two->m_centerHz) < 100.0, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(two->m_txVfoOffsetHz, 700, 5000);
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        const PanState after = PanState::of(two);
        QCOMPARE(after.overlay, before.overlay);
        QCOMPARE(after.refLevel, before.refLevel);
        QCOMPARE(after.dynamicRange, before.dynamicRange);
        QCOMPARE(after.centreHz, before.centreHz);
        QCOMPARE(after.bandwidthHz, before.bandwidthHz);
    }

    // ── A remote window ────────────────────────────────────────────────────

    void realTuneFrameKeepsRfCoordinatesThroughRemoteDisplay_data()
    {
        QTest::addColumn<DSPMode>("mode");
        QTest::addColumn<double>("toneHz");
        QTest::addColumn<int>("localGovernorSpanHz");
        QTest::addColumn<int>("remoteSpanHz");
        QTest::addColumn<double>("remoteCentreOffsetHz");
        QTest::addColumn<bool>("toneVisible");
        for (DSPMode mode : {DSPMode::LSB, DSPMode::USB}) {
            const double toneHz = mode == DSPMode::LSB ? -600.0 : 600.0;
            const QByteArray name = mode == DSPMode::LSB ? "LSB" : "USB";
            QTest::newRow((name + "-remote-governs").constData()) << mode << toneHz << 0 << 8000 << 0.0 << true;
            QTest::newRow((name + "-local8k-remote4k").constData()) << mode << toneHz << 8000 << 4000 << 0.0 << true;
            QTest::newRow((name + "-local8k-remote50k").constData()) << mode << toneHz << 8000 << 50000 << 0.0 << true;
            const double partialOffsetHz = mode == DSPMode::LSB ? -2500.0 : 2500.0;
            QTest::newRow((name + "-partial-source").constData()) << mode << toneHz << 8000 << 4000 << partialOffsetHz << true;
            QTest::newRow((name + "-outside-source").constData()) << mode << toneHz << 8000 << 4000 << 12000.0 << false;
        }
    }

    void realTuneFrameKeepsRfCoordinatesThroughRemoteDisplay()
    {
#ifndef HAVE_WDSP
        QSKIP("Real generator/analyzer coordinates require WDSP");
#else
        QFETCH(DSPMode, mode);
        QFETCH(double, toneHz);
        QFETCH(int, localGovernorSpanHz);
        QFETCH(int, remoteSpanHz);
        QFETCH(double, remoteCentreOffsetHz);
        QFETCH(bool, toneVisible);
        constexpr double kDialHz = 3'865'000.0;
        constexpr int kMicFrames = 64;
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true; // No device, wisdom generation or audio worker.
        engine.setTransmitLane(&lane);
        TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId, kMicFrames,
            WdspEngine::kTxDspBufferSize, 48000, WdspEngine::kTxDspSampleRate, 48000);
        QVERIFY(tx);
        QVERIFY(lane.waitIdleForTest(5000));
        WaveformConnection connection;
        tx->setConnection(&connection);
        RemoteWindow window(/*withAnalyzer=*/true);
        int localViewer = 0;
        const auto cleanup = qScopeGuard([&] {
            window.station.m_isTuning = false;
            window.key(false);
            tx->setTuneTone(false, toneHz, 0.0);
            tx->setRunningAsync(false);
            lane.waitIdleForTest(5000);
            if (localViewer != 0) {
                window.station.txDisplayFeed()->removeViewer(localViewer);
            }
            window.station.injectTxChannelForTest(nullptr);
            window.station.setTxAnalyzer(nullptr);
            window.analyzer.reset();
            engine.shutdown();
            engine.setTransmitLane(nullptr);
            lane.stop();
        });
        SliceModel* slice = window.station.sliceById(window.txSliceId);
        slice->setFrequency(kDialHz);
        slice->setDspMode(mode);
        window.station.m_isTuning = true;
        window.station.m_savedTxDspMode = mode;
        window.station.injectTxChannelForTest(tx);
        SpectrumWidget* pan = window.pan(QStringLiteral("two"));
        pan->setDisplayWindowPreservingHistory(kDialHz + 2000.0, 32000.0);
        pan->m_txViewBandwidthHz = remoteSpanHz;
        if (localGovernorSpanHz != 0) {
            localViewer = window.station.txDisplayFeed()->addViewer(
                double(window.station.txNcoFrequencyForSlice(slice)), localGovernorSpanHz,
                1024, /*local=*/true);
        }
        window.analyzer->setFftSize(32768);
        window.analyzer->setBlockSize(tx->txDspBlockSize());
        QVERIFY(window.connect());
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        QSignalSpy frames(window.gui.get(), &RemoteMediaController::transmitFrameReceived);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        const int expectedSpanHz = localGovernorSpanHz != 0 ? localGovernorSpanHz : remoteSpanHz;
        const double expectedCentreHz = double(window.station.txNcoFrequencyForSlice(slice));
        QTRY_VERIFY_WITH_TIMEOUT(!contexts.isEmpty()
            && contexts.last().at(1).value<SpectrumContextMessage>().spanHz == expectedSpanHz
            && contexts.last().at(1).value<SpectrumContextMessage>().centreHz == expectedCentreHz
            && pan->centerFrequency() == expectedCentreHz, 5000);
        window.analyzer->stop(); // Drain actual WDSP planes explicitly below.
        const TxDisplayView view = window.station.txDisplayFeed()->currentView();
        const TxAnalyzerArgs args = window.analyzer->currentArgs();
        const double ncoHz = double(window.station.txNcoFrequencyForSlice(slice));
        const SpectrumContextMessage context = contexts.last().at(1).value<SpectrumContextMessage>();
        QCOMPARE(ncoHz, kDialHz - toneHz);
        QCOMPARE(context.sourceCentreHz, ncoHz);
        QCOMPARE(context.centreHz, view.centreHz());
        QCOMPARE(context.spanHz, view.spanHz());
        if (remoteCentreOffsetHz != 0.0) {
            pan->setDisplayWindowPreservingHistory(ncoHz + remoteCentreOffsetHz, remoteSpanHz);
        }

        // The production PostGen setter, zero-mic fexchange pump, siphon,
        // analyzer and sign convention; no radio connection receives I/Q.
        tx->setTuneTone(true, toneHz, TxChannel::kMaxToneMag);
        tx->setRunningAsync(true);
        QTRY_VERIFY_WITH_TIMEOUT(tx->isRunning(), 5000);
        QVERIFY(lane.waitIdleForTest(5000));
        QVector<float> trace(args.nPix), waterfall(args.nPix);
        std::vector<double> mic(2 * kMicFrames, 0.0);
        int captured = 0;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            for (int block = 0; block < 16; ++block) {
                tx->pumpDexp(mic.data());
                tx->driveOneTxBlockFromInterleaved(mic.data());
            }
            int traceReady = 0;
            int waterfallReady = 0;
            GetPixels(TxAnalyzer::kTxDispId, 0, trace.data(), &traceReady);
            GetPixels(TxAnalyzer::kTxDispId, 1, waterfall.data(), &waterfallReady);
            if (traceReady && waterfallReady) { ++captured; }
            return captured >= 3;
        }()), 5000);
        const auto peakHz = [](const QVector<float>& values, double centre, double span) {
            const int index = int(std::max_element(values.cbegin(), values.cend()) - values.cbegin());
            return centre - span / 2.0 + index * span / values.size();
        };
        const double sourceTolerance = 3.0 * view.spanHz() / args.nPix;
        const double rawTraceHz = peakHz(trace, view.centreHz(), view.spanHz());
        const double rawWaterfallHz = peakHz(waterfall, view.centreHz(), view.spanHz());
        qInfo("real Tune source: mode=%d NCO=%.0f context=%.0f/%.0f raw trace=%.2f WF=%.2f",
              int(mode), ncoHz, context.centreHz, context.spanHz, rawTraceHz, rawWaterfallHz);
        QVERIFY(std::abs(rawTraceHz - kDialHz) <= sourceTolerance);
        QVERIFY(std::abs(rawWaterfallHz - kDialHz) <= sourceTolerance);
        const int rowsBefore = pan->waterfallHistoryRowsForTest();
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            emit window.analyzer->txFftReady(-1, trace);
            emit window.analyzer->txWaterfallReady(-1, waterfall);
            return !frames.isEmpty() && pan->waterfallHistoryRowsForTest() > rowsBefore;
        }()), 5000);
        const DisplayCodecFrame decoded = frames.last().at(1).value<DisplayCodecFrame>();
        const QVector<float> croppedWaterfall = pan->txWaterfallRow(decoded.waterfallDbm);
        QVERIFY(!pan->m_renderedPixels.isEmpty());
        QVERIFY(!croppedWaterfall.isEmpty());
        const double drawnTraceHz = peakHz(pan->m_renderedPixels, pan->centerFrequency(), pan->bandwidth());
        const double drawnWaterfallHz = peakHz(croppedWaterfall, pan->centerFrequency(), pan->bandwidth());
        const double displayTolerance = 4.0 * pan->bandwidth()
            / std::min(decoded.traceDbm.size(), decoded.waterfallDbm.size());
        qInfo("real Tune remote: trace=%.2f WF=%.2f tolerance=%.2f", drawnTraceHz, drawnWaterfallHz,
              displayTolerance);
        const bool sourceDoesNotCoverView = pan->centerFrequency() - pan->bandwidth() / 2.0
                < context.centreHz - context.spanHz / 2.0
            || pan->centerFrequency() + pan->bandwidth() / 2.0
                > context.centreHz + context.spanHz / 2.0;
        if (sourceDoesNotCoverView) {
            QCOMPARE(croppedWaterfall.size(), decoded.waterfallDbm.size());
            const auto uncoveredAtFloor = [&](const QVector<float>& values, float floor) {
                int checked = 0;
                const double marginHz = 2.0 * pan->bandwidth() / values.size();
                for (int x = 0; x < values.size(); ++x) {
                    const double hz = pan->centerFrequency() - pan->bandwidth() / 2.0
                        + x * pan->bandwidth() / values.size();
                    if (hz < context.centreHz - context.spanHz / 2.0 - marginHz
                        || hz > context.centreHz + context.spanHz / 2.0 + marginHz) {
                        ++checked;
                        if (std::abs(values[x] - floor) >= 0.01f) { return false; }
                    }
                }
                return checked > 0;
            };
            QVERIFY(uncoveredAtFloor(pan->m_renderedPixels,
                *std::min_element(decoded.traceDbm.cbegin(), decoded.traceDbm.cend())));
            QVERIFY(uncoveredAtFloor(croppedWaterfall,
                *std::min_element(decoded.waterfallDbm.cbegin(), decoded.waterfallDbm.cend())
                    + pan->displayCalOffsetDb()));
        }
        if (!toneVisible) {
            const float traceFloor = *std::min_element(decoded.traceDbm.cbegin(), decoded.traceDbm.cend());
            const float waterfallFloor = *std::min_element(decoded.waterfallDbm.cbegin(), decoded.waterfallDbm.cend())
                + pan->displayCalOffsetDb();
            QVERIFY(std::all_of(pan->m_renderedPixels.cbegin(), pan->m_renderedPixels.cend(),
                                [traceFloor](float value) { return std::abs(value - traceFloor) < 0.01f; }));
            QVERIFY(std::all_of(croppedWaterfall.cbegin(), croppedWaterfall.cend(),
                                [waterfallFloor](float value) { return std::abs(value - waterfallFloor) < 0.01f; }));
            return;
        }
        QVERIFY(std::abs(drawnTraceHz - kDialHz) <= displayTolerance);
        QVERIFY(std::abs(drawnWaterfallHz - kDialHz) <= displayTolerance);

        // The same painted TX row must keep its RF coordinate when the
        // wider, differently centered receive view is restored on unkey.
        const auto paintedPeakHz = [](const QImage& image, int row, double centre, double span) {
            const QRgb* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(row));
            int peak = 0;
            int brightest = -1;
            for (int x = 0; x < image.width(); ++x) {
                const int brightness = qRed(pixels[x]) + qGreen(pixels[x]) + qBlue(pixels[x]);
                if (brightness > brightest) {
                    brightest = brightness;
                    peak = x;
                }
            }
            return centre - span / 2.0 + peak * span / image.width();
        };
        const double txPaintedHz = paintedPeakHz(pan->m_waterfall, pan->m_wfWriteRow,
                                                 pan->centerFrequency(), pan->bandwidth());
        window.station.m_isTuning = false;
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QCOMPARE(pan->centerFrequency(), kDialHz + 2000.0);
        QCOMPARE(pan->bandwidth(), 32000.0);
        const double rxHistoryHz = paintedPeakHz(pan->m_waterfall, pan->m_wfWriteRow,
                                                 pan->centerFrequency(), pan->bandwidth());
        const double historyTolerance = 3.0 * pan->bandwidth() / pan->m_waterfall.width();
        qInfo("real Tune history: TX=%.2f restored RX=%.2f tolerance=%.2f", txPaintedHz,
              rxHistoryHz, historyTolerance);
        QVERIFY(std::abs(txPaintedHz - kDialHz) <= displayTolerance);
        QVERIFY(std::abs(rxHistoryHz - txPaintedHz) <= historyTolerance);
#endif
    }

    void remoteTransmitUsesAssociatedHostWhenPanKeyDoesNotResolve_data()
    {
        QTest::addColumn<QString>("corePanKey");
        QTest::newRow("primary slice without a pan key") << QString();
        QTest::newRow("slice with a stale pan key") << QStringLiteral("retired-pan");
    }

    void remoteTransmitUsesAssociatedHostWhenPanKeyDoesNotResolve()
    {
        QFETCH(QString, corePanKey);
        RemoteWindow window(/*withAnalyzer=*/true);
        window.station.sliceById(window.txSliceId)->setPanKey(corePanKey);
        window.stack.panadapter(QStringLiteral("two"))->addSlice(window.txSliceId);
        window.stack.panadapter(QStringLiteral("one"))->addSlice(window.otherSliceId);
        window.stack.setActivePan(QStringLiteral("one"));
        QVERIFY(window.connect());
        QCOMPARE(window.remote.sliceById(window.txSliceId)->panKey(), corePanKey);
        QCOMPARE(window.stack.activePanId(), QStringLiteral("one"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        const PanState before = PanState::of(two);
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);

        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(two->m_moxOverlay);
        QVERIFY(!one->m_moxOverlay);
        QVERIFY(two->m_txExternalWaterfall);
        QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("one")));
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -30.0f));
        // The Core's waterfall plane reaches the hosted widget as well as
        // its trace; its TX row must advance the live waterfall history.
        const int rowsBefore = two->waterfallHistoryRowsForTest();
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const auto context = contexts.last().at(1).value<SpectrumContextMessage>();
            window.emitPlanes(-30.0f, context.traceSamples);
            window.feed();
            return two->waterfallHistoryRowsForTest() > rowsBefore;
        }()), 5000);

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
        QCOMPARE(PanState::of(two), before);
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
    }

    void remoteTransmitWithoutUniqueAssociatedHostDoesNotUseActivePan_data()
    {
        QTest::addColumn<bool>("ambiguousHost");
        QTest::newRow("no actual host") << false;
        QTest::newRow("multiple actual hosts") << true;
    }

    void remoteTransmitWithoutUniqueAssociatedHostDoesNotUseActivePan()
    {
        QFETCH(bool, ambiguousHost);
        RemoteWindow window(/*withAnalyzer=*/true);
        window.station.sliceById(window.txSliceId)->setPanKey(QString());
        window.stack.panadapter(QStringLiteral("one"))->addSlice(window.otherSliceId);
        if (ambiguousHost) {
            window.stack.panadapter(QStringLiteral("one"))->addSlice(window.txSliceId);
            window.stack.panadapter(QStringLiteral("two"))->addSlice(window.txSliceId);
        }
        window.stack.setActivePan(QStringLiteral("one"));
        QVERIFY(window.connect());
        QCOMPARE(window.stack.panadapter(QStringLiteral("two"))->associatedSlices()
                     .contains(window.txSliceId), ambiguousHost);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QVERIFY(window.controller->transmitPanId().isEmpty());
        QVERIFY(!window.pan(QStringLiteral("one"))->m_moxOverlay);
        QVERIFY(!window.pan(QStringLiteral("two"))->m_moxOverlay);
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("one")));
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
    }

    void remoteRiseDrawsTheTransmitDisplayAndFallRestores()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        QVERIFY(window.client->capabilities().txStateVersion >= 1);
        QCOMPARE(window.client->capabilities().txDisplayVersion, 3);
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        const PanState before = PanState::of(two);
        const float txRef = two->m_txRefLevel;
        const float txRange = two->m_txDynamicRange;
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);

        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        // Pan 2 only: red border, transmit grid, palette and waterfall levels.
        QVERIFY(two->m_moxOverlay);
        QVERIFY(!one->m_moxOverlay);
        QCOMPARE(two->m_refLevel, txRef);
        QCOMPARE(two->m_dynamicRange, txRange);
        QCOMPARE(two->m_txWfLowLevel, -70);
        QCOMPARE(two->m_txWfHighLevel, 30);
        QVERIFY(two->m_txExternalWaterfall);
        QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("one")));
        QVERIFY(!window.gui->panDisplayState(QStringLiteral("two")).transmitDisplayMissing);

        // The transmit context: pan 2's bins sit at its centre and span.
        QTRY_VERIFY_WITH_TIMEOUT(!contexts.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const auto context = contexts.last().at(1).value<SpectrumContextMessage>();
            return contexts.last().at(0).toString() == QStringLiteral("two")
                && qFuzzyCompare(two->m_txCenterHz, context.centreHz)
                && qFuzzyCompare(two->m_txSampleRateHz, context.spanHz);
        }()), 5000);
        const auto context = contexts.last().at(1).value<SpectrumContextMessage>();

        // Pan 2 draws the transmit frames; pan 1 keeps its receive frames.
        QSignalSpy oneDrawn(one, &SpectrumWidget::spectrumFrameRendered);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            window.emitPlanes(-30.0f, context.traceSamples);
            window.feed();
            return !two->m_renderedPixels.isEmpty()
                && std::abs(two->m_renderedPixels.at(two->m_renderedPixels.size() / 2) + 30.0f)
                    < 1.0f
                && oneDrawn.count() >= 2;
        }()), 5000);

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
        // Exactly the values before the rise.
        QCOMPARE(PanState::of(two), before);
        // Receive frames draw again (once the Core's receive context lands).
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        QCOMPARE(two->m_refLevel, before.refLevel);
    }

    void remoteJoinWhileAlreadyKeyedShowsTransmitDisplay()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        // Initial transmit state arrives before the slice mirrors. Complete
        // the rise once the snapshot supplies the TX slice and its pan key.
        window.key(true);
        QVERIFY(window.station.isTransmitting());
        QVERIFY(window.connect());
        QTRY_VERIFY(window.controller->isKeyed());
        QTRY_COMPARE(window.client->transmitState()->txSliceId(), window.txSliceId);
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(two->m_moxOverlay);
        QVERIFY(!one->m_moxOverlay);
        QVERIFY(two->m_txExternalWaterfall);
        QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("one")));
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -30.0f));

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(window.controller->transmitPanId().isEmpty());
        QVERIFY(!two->m_moxOverlay);
        QVERIFY(!two->m_txExternalWaterfall);
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
    }

    void olderCoreHoldsThePanAndSaysWhy()
    {
        RemoteWindow window(/*withAnalyzer=*/false);
        QVERIFY(window.connect());
        QCOMPARE(window.client->capabilities().txDisplayVersion, 0);
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        const float txRef = two->m_txRefLevel;
        // The GPU path had built a receive trace (offscreen has no GPU, so
        // stand in for the trace it would have drawn).
        two->m_visibleBinCount = 100;
        QVERIFY(two->drawsSpectrumTrace());

        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QVERIFY(two->m_moxOverlay);
        QCOMPARE(two->m_refLevel, txRef);
        QVERIFY(!one->m_moxOverlay);
        // Coordinator ruling: no receive trace under the transmit axis (its
        // frequencies would read wrong); the pan is blank with its status.
        QVERIFY(two->m_renderedPixels.isEmpty());
        QVERIFY(!two->drawsSpectrumTrace());
        const PanDisplayState state = window.gui->panDisplayState(QStringLiteral("two"));
        QVERIFY(state.transmitDisplayMissing);
        // The whole sentence on hover; the line itself in forms that fit.
        const PanStatusText text = buildPanStatusText(state);
        QCOMPARE(text.explanation, QString::fromLatin1(kMissingDisplay));
        QCOMPARE(text.shortForms(),
                 (QStringList{QStringLiteral("No transmit display: update the Core"),
                              QStringLiteral("No transmit display"),
                              QStringLiteral("Update Core")}));
        QVERIFY(!window.gui->panDisplayState(QStringLiteral("one")).transmitDisplayMissing);

        // No receive frame reaches pan 2 while keyed (trace and waterfall
        // hold; the receiver's leakage never reaches the waterfall); pan 1
        // keeps drawing.
        QSignalSpy twoDrawn(two, &SpectrumWidget::spectrumFrameRendered);
        QSignalSpy oneDrawn(one, &SpectrumWidget::spectrumFrameRendered);
        QTRY_VERIFY_WITH_TIMEOUT((window.feed(), oneDrawn.count() >= 4), 5000);
        QCOMPARE(twoDrawn.count(), 0);
        QVERIFY(!two->m_pendingWfPixelsDbmDirty);

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(!window.gui->panDisplayState(QStringLiteral("two")).transmitDisplayMissing);
        QVERIFY(buildPanStatusText(window.gui->panDisplayState(QStringLiteral("two")))
                    .explanation != QString::fromLatin1(kMissingDisplay));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
    }

    void remoteHighSwrShowsOnTheTransmittingPan()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        // The Core's protection trips with fold-back on.
        window.station.swrProt().setEnabled(true);
        window.station.swrProt().setWindBackEnabled(true);
        for (int sample = 0; sample < 50 && !window.station.swrProt().highSwr(); ++sample) {
            window.station.swrProt().ingest(50.0f, 30.0f, /*tuneActive=*/false);
        }
        QVERIFY(window.station.swrProt().highSwr());
        QVERIFY(window.station.swrProt().windBackLatched());
        QTRY_VERIFY_WITH_TIMEOUT(window.client->transmitState()->highSwr(), 5000);
        QTRY_VERIFY(window.client->transmitState()->swrWindBackLatched());
        QTRY_VERIFY(two->isHighSwrOverlayActive());
        QVERIFY(two->isHighSwrFoldback());
        QVERIFY(!one->isHighSwrOverlayActive());
        // Protection off clears it.
        window.station.swrProt().setEnabled(false);
        window.station.swrProt().ingest(50.0f, 30.0f, false);
        QTRY_VERIFY_WITH_TIMEOUT(!two->isHighSwrOverlayActive(), 5000);
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
    }

    // ── Tuning while keyed: one test drives both windows ───────────────────

    void tuningWhileKeyedIsTheSameInBothWindows()
    {
        // WDSP has one transmit display per process (display id 5), so the
        // Core here has no analyzer; tuning is the pan's own gate either way.
        RemoteWindow remote(/*withAnalyzer=*/false);
        LocalWindow local(/*ownSettings=*/false);
        QVERIFY(remote.connect());
        SpectrumWidget* localPan = local.pan(QStringLiteral("two"));
        SpectrumWidget* remotePan = remote.pan(QStringLiteral("two"));
        QVERIFY(remote.receiveDraws(QStringLiteral("two")));
        for (SpectrumWidget* sw : {localPan, remotePan}) {
            sw->setConnectionState(ConnectionState::Connected);
            sw->setVfoFrequency(sw->m_centerHz);
        }
        const auto tunes = [](SpectrumWidget* sw) {
            QSignalSpy clicked(sw, &SpectrumWidget::frequencyClicked);
            wheel(sw);
            const int wheels = int(clicked.count());
            click(sw);
            return std::make_pair(wheels, int(clicked.count()) - wheels);
        };
        // Unkeyed, both windows tune on a wheel and a click.
        const auto localIdle = tunes(localPan);
        const auto remoteIdle = tunes(remotePan);
        QCOMPARE(localIdle, remoteIdle);
        QCOMPARE(localIdle.first, 1);
        QCOMPARE(localIdle.second, 1);

        local.controller->setKeyed(true, local.txSliceId);
        remote.key(true);
        QTRY_VERIFY(remote.controller->isKeyed());
        QVERIFY(localPan->m_moxOverlay);
        QVERIFY(remotePan->m_moxOverlay);
        // Keyed, neither retunes: the same result in both windows.
        const auto localKeyed = tunes(localPan);
        const auto remoteKeyed = tunes(remotePan);
        QCOMPARE(localKeyed, remoteKeyed);
        QCOMPARE(localKeyed, std::make_pair(0, 0));
        // A spot click goes through the same gate.
        QSignalSpy localSpot(localPan, &SpectrumWidget::frequencyClicked);
        QSignalSpy remoteSpot(remotePan, &SpectrumWidget::frequencyClicked);
        localPan->requestTune(localPan->m_centerHz + 1000.0);
        remotePan->requestTune(remotePan->m_centerHz + 1000.0);
        QCOMPARE(localSpot.count(), 0);
        QCOMPARE(remoteSpot.count(), 0);

        local.controller->setKeyed(false, -1);
        remote.key(false);
        QTRY_VERIFY(!remote.controller->isKeyed());
        QCOMPARE(tunes(localPan), tunes(remotePan));
    }

    // ── Row 14: a layout change while keyed ────────────────────────────────

    void layoutChangeWhileKeyedLeavesNoPanTransmitting()
    {
        {
            LocalWindow window;
            window.controller->setKeyed(true, window.txSliceId);
            QVERIFY(window.pan(QStringLiteral("two"))->m_moxOverlay);
            window.stack.removePanadapter(QStringLiteral("two"));
            window.stack.addPanadapter(QStringLiteral("three"));
            window.controller->setKeyed(false, -1);
            QVERIFY(window.controller->transmitPanId().isEmpty());
            for (PanadapterApplet* applet : window.stack.allApplets()) {
                QVERIFY(!applet->spectrumWidget()->m_moxOverlay);
            }
            QCOMPARE(window.radio.txDisplayFeed()->viewerCount(), 0);
        }
        {
            RemoteWindow window(/*withAnalyzer=*/true);
            QVERIFY(window.connect());
            window.key(true);
            QTRY_VERIFY(window.controller->isKeyed());
            QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
            window.stack.removePanadapter(QStringLiteral("two"));
            window.stack.addPanadapter(QStringLiteral("three"));
            window.key(false);
            QTRY_VERIFY(!window.controller->isKeyed());
            QVERIFY(window.controller->transmitPanId().isEmpty());
            QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
            for (PanadapterApplet* applet : window.stack.allApplets()) {
                QVERIFY(!applet->spectrumWidget()->m_moxOverlay);
            }
        }
    }

    // ── Parity Tasks 27-29 fix wave ────────────────────────────────────────

    // Waits until pan `two` draws the Core's transmit frames at `dbm` and
    // pan `one` draws receive frames: the rise pan kept the display.
    static bool riseTwoDrawsTransmit(RemoteWindow& window, QSignalSpy& contexts, float dbm)
    {
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QSignalSpy oneDrawn(one, &SpectrumWidget::spectrumFrameRendered);
        return QTest::qWaitFor([&] {
            int samples = 0;
            for (int i = contexts.size() - 1; i >= 0 && samples == 0; --i) {
                if (contexts.at(i).at(0).toString() == QStringLiteral("two")) {
                    samples = contexts.at(i).at(1).value<SpectrumContextMessage>().traceSamples;
                }
            }
            if (samples <= 0) { return false; }
            window.emitPlanes(dbm, samples);
            window.feed();
            return two->m_moxOverlay && !two->m_renderedPixels.isEmpty()
                && std::abs(two->m_renderedPixels.at(two->m_renderedPixels.size() / 2) - dbm)
                    < 1.0f
                && oneDrawn.count() >= 2;
        }, 5000);
    }

    void rebindingWhileKeyedKeepsTheRisePanOnBothSides()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -30.0f));

        // The Core's transmit binding moves to the slice on pan one while
        // keyed. Both sides keep pan two, recorded at the rise, until the
        // fall: pan two keeps drawing the transmit frames (nothing freezes)
        // and pan one keeps drawing receive (nothing blanks).
        TxSliceArbiter* arbiter = window.station.txSliceArbiter();
        QVERIFY(arbiter);
        arbiter->flipTo(window.station.sliceById(window.otherSliceId));
        QCOMPARE(window.station.txBoundSlice()->sliceIndex(), window.otherSliceId);
        QTRY_VERIFY_WITH_TIMEOUT(window.client->transmitState()->txSliceId()
                                     == window.otherSliceId, 5000);
        QVERIFY(window.controller->isKeyed());
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -20.0f));
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("one")));

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
    }

    void layoutChangeWhileKeyedKeepsTheRisePanOnBothSides()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -30.0f));

        // A layout change on the Core while keyed rehomes the slices: the
        // transmit slice to pan one, the other slice to pan two; the window
        // follows (each pan shows the slice it now hosts). Pan two, recorded
        // at the rise, keeps the transmit display on both sides.
        window.station.sliceById(window.txSliceId)->setPanKey(QStringLiteral("one"));
        window.station.sliceById(window.otherSliceId)->setPanKey(QStringLiteral("two"));
        QTRY_VERIFY_WITH_TIMEOUT(
            window.remote.sliceById(window.otherSliceId)->panKey() == QStringLiteral("two"),
            5000);
        window.stack.panadapter(QStringLiteral("one"))->setActiveSliceIndex(window.txSliceId);
        window.stack.panadapter(QStringLiteral("two"))->setActiveSliceIndex(window.otherSliceId);
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -20.0f));
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
    }

    void remoteFallDrawsNoTransmitTraceUnderTheReceiveAxis()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QVERIFY(riseTwoDrawsTransmit(window, contexts, -30.0f));
        // The GPU path built the transmit trace (offscreen has no GPU, so
        // stand in for the vertices it would have built).
        two->m_visibleBinCount = 100;
        QVERIFY(two->drawsSpectrumTrace());

        // The fall, before any receive frame: nothing from the transmit
        // display is drawn under the restored receive axis.
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(!two->m_moxOverlay);
        QVERIFY(two->m_renderedPixels.isEmpty());
        QVERIFY(!two->drawsSpectrumTrace());
        // The first receive frame draws again.
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        QVERIFY(two->drawsSpectrumTrace());
    }

    void aTransmitContextBeforeTheRiseIsReplayed()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        // The window's rise is held back, so the Core's transmit context
        // lands first (media and transmit state travel on different
        // channels).
        QObject::disconnect(window.client->transmitState(), nullptr,
                            window.controller.get(), nullptr);
        QObject::disconnect(&window.remote, nullptr, window.controller.get(), nullptr);
        QSignalSpy contexts(window.gui.get(), &RemoteMediaController::transmitContextReceived);
        window.key(true);
        QTRY_VERIFY_WITH_TIMEOUT(!contexts.isEmpty(), 5000);
        QCOMPARE(contexts.last().at(0).toString(), QStringLiteral("two"));
        const auto context = contexts.last().at(1).value<SpectrumContextMessage>();
        const auto held = window.gui->heldTransmitContext(QStringLiteral("two"));
        QVERIFY(held.has_value());
        QCOMPARE(held->centreHz, context.centreHz);
        QVERIFY(!window.gui->heldTransmitContext(QStringLiteral("one")).has_value());
        two->m_txCenterHz = 1.0;
        two->m_txSampleRateHz = 1.0;

        // The rise takes the held context at once: its bins sit at the
        // context's centre and span with no new context from the Core.
        const int before = contexts.size();
        window.controller->setKeyed(true, window.txSliceId);
        QCOMPARE(contexts.size(), before);
        QCOMPARE(two->m_txCenterHz, context.centreHz);
        QCOMPARE(two->m_txSampleRateHz, context.spanHz);

        window.controller->setKeyed(false, -1);
        window.key(false);
        // The fall's receive context replaces it.
        QTRY_VERIFY([&] {
            window.feed();
            return !window.gui->heldTransmitContext(QStringLiteral("two")).has_value();
        }());
    }

    void olderCoreIsNotAskedAgainAtTheRiseOrFall()
    {
        QStringList calls;
        {
            RemoteWindow window(/*withAnalyzer=*/false);
            QVERIFY(window.connect());
            QCOMPARE(window.client->capabilities().txDisplayVersion, 0);
            window.source->setCallLog(&calls);
            window.key(true);
            QTRY_VERIFY(window.controller->isKeyed());
            window.key(false);
            QTRY_VERIFY(!window.controller->isKeyed());
            window.source->setCallLog(nullptr);
        }
        QVERIFY2(!calls.contains(QStringLiteral("refreshTransmitView")), qPrintable(calls.join(u',')));
        // A Core that sends the transmit display is asked at the rise and
        // at the fall.
        calls.clear();
        {
            RemoteWindow window(/*withAnalyzer=*/true);
            QVERIFY(window.connect());
            window.source->setCallLog(&calls);
            window.key(true);
            QTRY_VERIFY(window.controller->isKeyed());
            window.key(false);
            QTRY_VERIFY(!window.controller->isKeyed());
            window.source->setCallLog(nullptr);
        }
        QCOMPARE(calls.count(QStringLiteral("refreshTransmitView")), 2);
    }
    void coreWithoutTransmitDisplayStillSendsTheHighSwrState()
    {
        // The link document (section 18.8): txState carries highSwr and
        // swrWindBackLatched from any Core that sends txState, whatever its
        // txDisplayVersion. A Core with no transmit display still shows the
        // border on the transmitting pan.
        RemoteWindow window(/*withAnalyzer=*/false);
        QVERIFY(window.connect());
        QCOMPARE(window.client->capabilities().txDisplayVersion, 0);
        QVERIFY(window.client->capabilities().txStateVersion >= 1);
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        window.station.swrProt().setEnabled(true);
        window.station.swrProt().setWindBackEnabled(true);
        for (int sample = 0; sample < 50 && !window.station.swrProt().highSwr(); ++sample) {
            window.station.swrProt().ingest(50.0f, 30.0f, /*tuneActive=*/false);
        }
        QVERIFY(window.station.swrProt().highSwr());
        QTRY_VERIFY_WITH_TIMEOUT(window.client->transmitState()->highSwr(), 5000);
        QTRY_VERIFY(two->isHighSwrOverlayActive());
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
    }
    void remoteRiseOnTheCarrierDrawsNoReceiveTrace()
    {
        // A pan already centred on the carrier does not move at the rise,
        // so the move cannot be what retires its receive trace: the rise
        // itself does, on an older Core (blank with its status) and a new
        // one (blank until the transmit frames land) alike.
        for (const bool withAnalyzer : {false, true}) {
            RemoteWindow window(withAnalyzer);
            QVERIFY(window.connect());
            SpectrumWidget* two = window.pan(QStringLiteral("two"));
            SliceModel* tx = window.remote.sliceById(window.txSliceId);
            QVERIFY(tx);
            const double carrier = static_cast<double>(window.remote.txFrequencyForSlice(tx));
            two->setDisplayWindowPreservingHistory(carrier, two->bandwidth());
            QVERIFY(window.receiveDraws(QStringLiteral("two")));
            QVERIFY(!two->m_renderedPixels.isEmpty());
            two->m_visibleBinCount = 100;  // the GPU path's receive trace
            const double centreBefore = two->centerFrequency();

            window.key(true);
            QTRY_VERIFY(window.controller->isKeyed());
            QCOMPARE(two->centerFrequency(), centreBefore);
            QVERIFY(two->m_renderedPixels.isEmpty());
            QVERIFY(!two->drawsSpectrumTrace());
            window.key(false);
            QTRY_VERIFY(!window.controller->isKeyed());
        }
    }
};

QTEST_MAIN(TstMoxDisplayController)
#include "tst_mox_display_controller.moc"
