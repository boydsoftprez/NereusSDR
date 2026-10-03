// =================================================================
// tests/tst_display_duplex.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. The Thetis behaviour it checks
// is cited in src/gui/MoxDisplayController.h, SpectrumWidget.cpp
// (displayCalOffsetDb, setDisplayDuplex) and RadioModel.cpp (the noise
// blanking rule).
//
// Remote-window parity Task 31 (A11, R-R3-49): display duplex (DUP) in both
// windows, off by default as Thetis (JJ's ruling Q5, 2026-09-26).
//   - The calibration rule (Thetis RX1Offset): keyed with DUP on, the
//     receive trace takes the transmit calibration plus the receive
//     calibration plus the transmit attenuator offset.
//   - Noise blanking: on before the key, off while keyed with DUP on, back
//     after; untouched with DUP off; on a Core, the transmit holder's DUP.
//   - DUP on, local and remote: the transmitting pan keeps the receiver's
//     span, bins and frames under the red border, the transmit grid and the
//     transmit waterfall levels; XIT does not move the view; the TX filter
//     sits at the VFO against the receive span.
//   - A change while keyed swaps the view at once and resets the peaks.
//   - An older Core: DUP unavailable, with the reason, and the pan as DUP
//     off.
//   - The setting: window-scoped, "False" by default, survives a restart.
// Nothing keys a radio: the Core's own MoxController is keyed with the
// receive-only pre-check lifted, against no hardware. No audio device.
//
// Modification history (NereusSDR):
//   2026-09-27 : Created for remote-window parity Task 31 by J.J. Boyd
//                 (KG4VCF). AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest>

#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <memory>

#define private public
#include "gui/SpectrumWidget.h"
#include "core/session/media/DaemonMediaController.h"
#undef private

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/StepAttenuatorController.h"
#include "core/TxAnalyzer.h"
#include "core/TxDisplayFeed.h"
#include "core/WdspTypes.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/IMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/MoxDisplayController.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/TxDisplaySource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr char kUnavailable[] =
    "This Core does not show the receiver while transmitting for this app. "
    "Updating the Core may help.";

struct PanView {
    double centreHz{0.0};
    double bandwidthHz{0.0};
    double sampleRate{0.0};
    double ddcHz{0.0};

    static PanView of(const SpectrumWidget* sw)
    {
        return {sw->m_centerHz, sw->m_bandwidthHz, sw->m_sampleRateHz, sw->m_ddcCenterHz};
    }
    bool operator==(const PanView&) const = default;
};

// A window running its own DSP, as tst_mox_display_controller stands it up:
// two pans, the transmit slice on pan "two".
struct LocalWindow {
    QTemporaryDir directory;
    std::unique_ptr<AppSettings> settings;
    RadioModel radio;
    std::unique_ptr<TxAnalyzer> analyzer;
    PanadapterStack stack;
    int txSliceId{-1};
    int otherSliceId{-1};
    std::unique_ptr<LocalTxDisplaySource> source;
    std::unique_ptr<MoxDisplayController> controller;

    LocalWindow()
        : settings(std::make_unique<AppSettings>(
              directory.filePath(QStringLiteral("local.settings"))))
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(5, 5, 192000);
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
        pan()->setSampleRate(192000.0);
        pan()->setDdcCenterFrequency(14'180'000.0);
        pan()->setDisplayWindowPreservingHistory(14'185'000.0, 48000.0);
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

    SpectrumWidget* pan() const { return stack.spectrum(QStringLiteral("two")); }
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

// A remote window on an in-process Core, as tst_mox_display_controller
// stands it up, with DUP wired as MainWindow wires it: the controller's
// displayDuplexChanged to RemoteMediaController::setDisplayDuplex.
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
        QObject::connect(controller.get(), &MoxDisplayController::displayDuplexChanged,
                         gui.get(), &RemoteMediaController::setDisplayDuplex);
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
        return QTest::qWaitFor([this] {
            SliceModel* tx = remote.sliceById(txSliceId);
            return daemon->activeEndpointCount() == 2 && tx != nullptr
                && tx->panKey() == QStringLiteral("two");
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

    // Whether the Core's endpoints all asked `duplex` as expected (the
    // window sends it on every subscription while DUP is on).
    bool coreEndpointsDuplex(bool expected) const
    {
        const QList<quint32> ids = daemon->endpointIds();
        if (ids.isEmpty()) { return false; }
        for (quint32 id : ids) {
            if (daemon->endpointDuplex(id) != expected) { return false; }
        }
        return true;
    }
    bool anyCoreViewer() const
    {
        for (quint32 id : daemon->endpointIds()) {
            if (daemon->transmitDisplayActive(id)) { return true; }
        }
        return false;
    }

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

class TstDisplayDuplex : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "nereus.*.debug=false\nnereus.*.info=false\nnereussdr.*.info=false\n"
            "nereus.connection.warning=false\nnereus.stationclient.warning=false"));
    }

    // ── The setting ────────────────────────────────────────────────────────

    void settingIsTheWindowsOwnOffByDefaultAndSurvivesARestart()
    {
        QCOMPARE(classifySettingsKey(QStringLiteral("DisplayDuplex")),
                 SettingsScope::OperatorLocal);
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("window.settings"));
        {
            AppSettings settings(path);
            QVERIFY(!MoxDisplayController::savedDisplayDuplex());
            MoxDisplayController::saveDisplayDuplex(true);
            QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayDuplex")).toString(),
                     QStringLiteral("True"));
            QVERIFY(settings.save());
        }
        {
            // The window comes back: the same file, read again.
            AppSettings settings(path);
            settings.load();
            QVERIFY(MoxDisplayController::savedDisplayDuplex());
            MoxDisplayController::saveDisplayDuplex(false);
            QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayDuplex")).toString(),
                     QStringLiteral("False"));
        }
        // The widget and the controller start with DUP off, as Thetis.
        LocalWindow window;
        QVERIFY(!window.pan()->displayDuplex());
        QVERIFY(!window.controller->displayDuplex());
        QVERIFY(window.controller->displayDuplexAvailable());
        QVERIFY(OperatorWording::isPlain(QString::fromLatin1(kUnavailable)));
        QVERIFY(OperatorWording::isPlain(QStringLiteral("Display duplex (DUP)")));
    }

    // ── The calibration rule (test first) ──────────────────────────────────

    // Thetis RX1Offset: receiving, the receive calibration with its preamp;
    // keyed with DUP off, the TX Display Cal Offset alone; keyed with DUP
    // on, that plus the receive calibration without the preamp plus the TX
    // attenuator offset.
    void keyedTheTraceTakesThetisTransmitCalibration()
    {
        SpectrumWidget sw;
        sw.setDbmCalOffset(-12.5f);          // RXOffset: preamp + calibration
        sw.setRxPreampOffsetDb(4.5f);        // its preamp half
        sw.setTxDisplayCalOffsetDb(3.0f);    // TX Display Cal Offset
        sw.setTxAttenuatorOffsetDb(7.0f);    // the TX attenuator applied
        QCOMPARE(sw.displayCalOffsetDb(), -12.5f);
        sw.setDisplayDuplex(true);
        QCOMPARE(sw.displayCalOffsetDb(), -12.5f);
        // Keyed with DUP on: 3 + (-12.5 - 4.5) + 7.
        sw.setMoxOverlay(true);
        QCOMPARE(sw.displayCalOffsetDb(), -7.0f);
        const QRect rect(0, 0, 400, 300);
        const float withDuplex = sw.dbmToYf(-60.0f, rect);
        // The TX attenuator moves the DUP trace.
        sw.setTxAttenuatorOffsetDb(12.0f);
        QCOMPARE(sw.displayCalOffsetDb(), -2.0f);
        QCOMPARE(sw.dbmToYf(-65.0f, rect), withDuplex);
        // Keyed with DUP off: the TX Display Cal Offset alone.
        sw.setDisplayDuplex(false);
        QCOMPARE(sw.displayCalOffsetDb(), 3.0f);
        sw.setTxDisplayCalOffsetDb(-1.5f);
        QCOMPARE(sw.displayCalOffsetDb(), -1.5f);
        // The transmit waterfall row takes it too (display.cs:6588-6601).
        const QVector<float> row = sw.txWaterfallRow(QVector<float>(64, -40.0f));
        QVERIFY(!row.isEmpty());
        QCOMPARE(row.first(), -41.5f);
        sw.setMoxOverlay(false);
        QCOMPARE(sw.displayCalOffsetDb(), -12.5f);

        // A remote pan's frames carry the Core's calibration, receive and
        // transmit: it adds none, keyed or not.
        SpectrumWidget remote;
        remote.m_remoteSpectrum = true;
        remote.setDbmCalOffset(-12.5f);
        remote.setTxDisplayCalOffsetDb(3.0f);
        remote.setTxAttenuatorOffsetDb(7.0f);
        QCOMPARE(remote.displayCalOffsetDb(), 0.0f);
        remote.setMoxOverlay(true);
        QCOMPARE(remote.displayCalOffsetDb(), 0.0f);
        remote.setDisplayDuplex(true);
        QCOMPARE(remote.displayCalOffsetDb(), 0.0f);
    }

    // RadioModel::keyedDisplayOffsetDb, what the Core gives a remote pan's
    // frames: the TX Display Cal Offset; with DUP the receive calibration
    // without its preamp and the TX attenuator applied.
    void theCoresKeyedOffsetIsThetisRx1Offset()
    {
        LocalWindow window;
        StepAttenuatorController att;
        window.radio.setStepAttController(&att);
        window.radio.calibrationControllerMutable().setTxDisplayOffsetDb(2.5);
        const double receiveCal = window.radio.rxMeterOffsetDb()
            - window.radio.rxPreampOffsetDb();
        QCOMPARE(window.radio.keyedDisplayOffsetDb(false), 2.5);
        QCOMPARE(window.radio.keyedDisplayOffsetDb(true), 2.5 + receiveCal);
        att.setAttOnTxEnabled(true);
        att.setAttOnTxValue(9);
        QCOMPARE(att.txAttenuatorOffsetDb(), 9);
        QCOMPARE(window.radio.keyedDisplayOffsetDb(true), 2.5 + receiveCal + 9.0);
        QCOMPARE(window.radio.keyedDisplayOffsetDb(false), 2.5);
        window.radio.setStepAttController(nullptr);
    }

    // Thetis sets Display.TXAttenuatorOffset beside every SetTxAttenData:
    // the value applied with ATT on TX on, 0 with it off and at the unkey.
    void theTxAttenuatorOffsetFollowsWhatIsApplied()
    {
        StepAttenuatorController att;
        QSignalSpy changed(&att, &StepAttenuatorController::txAttenuatorOffsetChanged);
        att.setAttOnTxEnabled(true);
        att.setForceAttWhenPsOff(false);
        att.setAttOnTxValue(12);
        QCOMPARE(att.txAttenuatorOffsetDb(), 12);
        QCOMPARE(changed.count(), 1);
        att.onMoxHardwareFlipped(true);
        QCOMPARE(att.txAttenuatorOffsetDb(), 12);
        att.onMoxHardwareFlipped(false);
        QCOMPARE(att.txAttenuatorOffsetDb(), 0);
        // Forced to 31 (PS off with Force ATT, or CW): the display follows.
        att.setForceAttWhenPsOff(true);
        att.setPsActive(false);
        att.onMoxHardwareFlipped(true);
        QCOMPARE(att.txAttenuatorOffsetDb(), 31);
        att.onMoxHardwareFlipped(false);
        // ATT on TX off: 0 at the key.
        att.setAttOnTxEnabled(false);
        att.onMoxHardwareFlipped(true);
        QCOMPARE(att.txAttenuatorOffsetDb(), 0);
        att.onMoxHardwareFlipped(false);
        att.setAttOnTxValue(20);
        QCOMPARE(att.txAttenuatorOffsetDb(), 0);
    }

    void theTxFilterSitsAtTheVfoAgainstTheReceiveSpan()
    {
        SpectrumWidget sw;
        sw.setDisplayWindowPreservingHistory(14'185'000.0, 48000.0);
        sw.setTxVfoOffsetHz(700);
        QCOMPARE(sw.txFilterXitHz(), 700.0);
        sw.setDisplayDuplex(true);
        sw.setMoxOverlay(true);
        // Keyed with DUP on: no XIT, and the receive span stays.
        QCOMPARE(sw.txFilterXitHz(), 0.0);
        QCOMPARE(sw.m_bandwidthHz, 48000.0);
        QCOMPARE(sw.m_centerHz, 14'185'000.0);
        QVERIFY(!sw.showsTransmitView());
        sw.setMoxOverlay(false);
        QCOMPARE(sw.txFilterXitHz(), 700.0);
    }

    // ── Noise blanking (test first) ────────────────────────────────────────

    void noiseBlankingIsOffWhileKeyedWithDuplexAndBackAfter()
    {
        LocalWindow window;
        SliceModel* slice = window.txSlice();
        QCOMPARE(window.radio.txBoundSlice(), slice);
        slice->setNbMode(NbMode::NB2);

        // DUP off: untouched.
        MoxController* mox = window.radio.moxController();
        QVERIFY(window.key(true));
        QTRY_VERIFY(mox->state() == MoxState::Tx);
        QCOMPARE(slice->nbMode(), NbMode::NB2);
        QVERIFY(window.key(false));
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        QCOMPARE(slice->nbMode(), NbMode::NB2);

        // DUP on: off while keyed, back after.
        window.radio.setLocalDisplayDuplex(true);
        QVERIFY(window.key(true));
        QTRY_COMPARE(slice->nbMode(), NbMode::Off);
        QVERIFY(window.key(false));
        QTRY_COMPARE(slice->nbMode(), NbMode::NB2);

        // Saved at every key, restored only with DUP on at the unkey
        // (Thetis's UIMOXChangedFalse reads _display_duplex then).
        slice->setNbMode(NbMode::NB);
        QVERIFY(window.key(true));
        QTRY_COMPARE(slice->nbMode(), NbMode::Off);
        window.radio.setLocalDisplayDuplex(false);
        QVERIFY(window.key(false));
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        QCOMPARE(slice->nbMode(), NbMode::Off);
    }

    void onACoreTheTransmitHoldersDuplexCounts()
    {
        LocalWindow window;
        SliceModel* slice = window.txSlice();
        slice->setNbMode(NbMode::NB);
        const QByteArray device("phone-1");
        window.radio.setTransmitHolder(device);
        // This window's DUP does not count while a device holds transmit.
        window.radio.setLocalDisplayDuplex(true);
        QVERIFY(!window.radio.transmitDisplayDuplex());
        window.radio.setDeviceDisplayDuplex(device, true);
        QVERIFY(window.radio.transmitDisplayDuplex());
        QVERIFY(window.key(true));
        QTRY_COMPARE(slice->nbMode(), NbMode::Off);
        QVERIFY(window.key(false));
        QTRY_COMPARE(slice->nbMode(), NbMode::NB);
        window.radio.setDeviceDisplayDuplex(device, false);
        QVERIFY(!window.radio.transmitDisplayDuplex());
        window.radio.setTransmitHolder({});
        QVERIFY(window.radio.transmitDisplayDuplex());
    }

    // The window's DUP reaches the Core's RadioModel as its device's
    // (DaemonMediaController, from the subscriptions), which is the DUP the
    // Core's key reads while that device holds transmit
    // (onACoreTheTransmitHoldersDuplexCounts keys it). Here the Core's own
    // key is a station key, which takes transmit from the device, so the
    // two halves are checked apart.
    void aRemoteWindowsDuplexReachesTheCore()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        QCOMPARE(window.client->capabilities().txDisplayVersion, 3);
        QVERIFY(window.gui->displayDuplexNegotiated());
        QVERIFY(window.daemon->displayDuplexDevice().isEmpty());

        window.controller->setDisplayDuplex(true);
        QTRY_VERIFY_WITH_TIMEOUT(window.coreEndpointsDuplex(true), 5000);
        const QByteArray device = window.daemon->displayDuplexDevice();
        QVERIFY(!device.isEmpty());
        QCOMPARE(device, window.server->mediaSessionDevice(window.client->sessionEpoch()));
        window.station.setTransmitHolder(device);
        QVERIFY(window.station.transmitDisplayDuplex());

        // DUP off in the window: the Core forgets it.
        window.controller->setDisplayDuplex(false);
        QTRY_VERIFY_WITH_TIMEOUT(window.coreEndpointsDuplex(false), 5000);
        QVERIFY(window.daemon->displayDuplexDevice().isEmpty());
        QVERIFY(!window.station.transmitDisplayDuplex());

        // And when the media goes.
        window.controller->setDisplayDuplex(true);
        QTRY_VERIFY(window.station.transmitDisplayDuplex());
        window.daemon.reset();
        QVERIFY(!window.station.transmitDisplayDuplex());
        window.station.setTransmitHolder({});
    }

    // ── DUP on, a local window ─────────────────────────────────────────────

    void localDuplexKeepsTheReceiverUnderTheTransmitGrid()
    {
        LocalWindow window;
        SpectrumWidget* two = window.pan();
        const PanView before = PanView::of(two);
        const float rxRef = two->m_refLevel;
        const float txRef = two->m_txRefLevel;
        window.controller->setDisplayDuplex(true);
        QVERIFY(window.controller->displayDuplex());

        window.controller->setCallRecording(true);
        window.controller->setKeyed(true, window.txSliceId);
        // The overlay only.
        QCOMPARE(window.controller->callLog(), QStringList{QStringLiteral("setMoxOverlay(true)")});
        QVERIFY(two->m_moxOverlay);
        QVERIFY(two->displayDuplex());
        QVERIFY(!two->showsTransmitView());
        QCOMPARE(two->m_refLevel, txRef);
        QCOMPARE(two->m_txWfLowLevel, -70);
        QCOMPARE(two->m_txWfHighLevel, 30);
        QVERIFY(!two->m_txExternalWaterfall);
        QCOMPARE(PanView::of(two), before);
        // The receive frames keep coming to this pan, and the analyzer has
        // no viewer.
        QVERIFY(window.controller->transmitPanId().isEmpty());
        QCOMPARE(window.controller->keyedPanId(), QStringLiteral("two"));
        QCOMPARE(window.radio.txDisplayFeed()->viewerCount(), 0);

        // XIT does not move the view.
        SliceModel* slice = window.txSlice();
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        window.controller->carrierChanged(
            double(window.radio.txFrequencyForSlice(slice)));
        QCOMPARE(PanView::of(two), before);
        QCOMPARE(two->txFilterXitHz(), 0.0);

        window.controller->clearCallLog();
        window.controller->setKeyed(false, -1);
        QCOMPARE(window.controller->callLog(),
                 QStringList{QStringLiteral("setMoxOverlay(false)")});
        QVERIFY(!two->m_moxOverlay);
        QCOMPARE(two->m_refLevel, rxRef);
        QCOMPARE(PanView::of(two), before);
    }

    void localDuplexOffIsTheTransmitDisplay()
    {
        LocalWindow window;
        window.controller->setKeyed(true, window.txSliceId);
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(window.pan()->showsTransmitView());
        QVERIFY(window.pan()->m_txExternalWaterfall);
        QCOMPARE(window.radio.txDisplayFeed()->viewerCount(), 1);
        window.controller->setKeyed(false, -1);
    }

    // ── A change while keyed ───────────────────────────────────────────────

    void aChangeWhileKeyedSwapsTheViewAtOnceAndResetsThePeaks()
    {
        LocalWindow window;
        SpectrumWidget* two = window.pan();
        const PanView before = PanView::of(two);
        const double carrier = double(window.radio.txFrequencyForSlice(window.txSlice()));
        window.controller->setKeyed(true, window.txSliceId);
        QCOMPARE(two->m_centerHz, carrier);
        // Peaks the receiver or the transmitter left.
        two->m_peakBlobs.setEnabled(true);
        two->m_peakBlobs.m_blobs.resize(3);
        two->m_peakBlobs.m_blobs[0].enabled = true;
        // A sized trace that has not been reset, so no display delay runs.
        two->m_activePeakHold = ActivePeakHoldTrace(8);
        two->m_activePeakHold.setEnabled(true);
        // Keyed, the trace runs only with "Update during TX" on (Thetis
        // display.cs:5011 [v2.10.3.15]); this test is about the reset.
        two->m_activePeakHold.setTxActive(true);
        two->m_activePeakHold.setOnTx(true);
        two->m_activePeakHold.update(QVector<float>(8, -40.0f));
        QCOMPARE(two->m_activePeakHold.peak(0), -40.0f);
        QSignalSpy changed(window.controller.get(), &MoxDisplayController::displayDuplexChanged);

        // On: at once, no frame period passes.
        window.controller->setDisplayDuplex(true);
        QCOMPARE(changed.count(), 1);
        QVERIFY(window.controller->transmitPanId().isEmpty());
        QVERIFY(two->m_moxOverlay);
        QVERIFY(!two->showsTransmitView());
        QVERIFY(!two->m_txExternalWaterfall);
        QCOMPARE(PanView::of(two), before);
        QCOMPARE(window.radio.txDisplayFeed()->viewerCount(), 0);
        QVERIFY(two->m_peakBlobs.m_blobs.isEmpty());
        QVERIFY(std::isinf(two->m_activePeakHold.peak(0)));
        // Both resets hold the peaks back 500 ms (Thetis display.cs:4527-4530,
        // 4542-4544, 859-877 [v2.10.3.15]).
        QVERIFY(two->m_activePeakHold.displayDelayed());
        QVERIFY(two->m_peakBlobs.displayDelayed());

        // Off again: the transmit view comes back on the carrier.
        two->m_activePeakHold.update(QVector<float>(8, -30.0f));
        window.controller->setDisplayDuplex(false);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(window.controller->transmitPanId(), QStringLiteral("two"));
        QVERIFY(two->showsTransmitView());
        QVERIFY(two->m_txExternalWaterfall);
        QCOMPARE(two->m_centerHz, carrier);
        QCOMPARE(window.radio.txDisplayFeed()->viewerCount(), 1);
        QVERIFY(std::isinf(two->m_activePeakHold.peak(0)));

        window.controller->setKeyed(false, -1);
        QCOMPARE(PanView::of(two), before);
        QVERIFY(!two->m_moxOverlay);
    }

    // Thetis resets every receiver's peaks on a MOX edge (display.cs:1582-1593,
    // console.cs:24243-24254 [v2.10.3.15], PurgeBuffers) and when the radio
    // comes on (display.cs:818-823 [v2.10.3.15]); every pan of the window
    // does the same, the transmitting one and the others.
    static void seedPeaks(SpectrumWidget* pan)
    {
        pan->m_activePeakHold = ActivePeakHoldTrace(8);
        pan->m_activePeakHold.setEnabled(true);
        pan->m_activePeakHold.setOnTx(true);
        pan->m_activePeakHold.update(QVector<float>(8, -40.0f));
        pan->m_peakBlobs.setEnabled(true);
        pan->m_peakBlobs.m_blobs.resize(3);
        pan->m_peakBlobs.m_blobs[0].enabled = true;
    }
    static bool wasReset(const SpectrumWidget* pan)
    {
        return std::isinf(pan->m_activePeakHold.peak(0))
            && pan->m_activePeakHold.displayDelayed()
            && pan->m_peakBlobs.m_blobs.isEmpty()
            && pan->m_peakBlobs.displayDelayed();
    }

    void aMoxEdgeResetsEveryPansPeaks()
    {
        LocalWindow window;
        SpectrumWidget* one = window.stack.spectrum(QStringLiteral("one"));
        SpectrumWidget* two = window.pan();
        for (SpectrumWidget* pan : {one, two}) { seedPeaks(pan); }
        window.controller->setKeyed(true, window.txSliceId);
        QVERIFY(wasReset(one));
        QVERIFY(wasReset(two));
        for (SpectrumWidget* pan : {one, two}) { seedPeaks(pan); }
        window.controller->setKeyed(false, -1);
        QVERIFY(wasReset(one));
        QVERIFY(wasReset(two));
    }

    void theRadioComingOnResetsEveryPansPeaks()
    {
        LocalWindow window;
        SpectrumWidget* one = window.stack.spectrum(QStringLiteral("one"));
        SpectrumWidget* two = window.pan();
        window.radio.setConnectionStateForTest(ConnectionState::Disconnected);
        for (SpectrumWidget* pan : {one, two}) { seedPeaks(pan); }
        window.radio.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(wasReset(one));
        QVERIFY(wasReset(two));
    }

    void aRemoteWindowsMoxEdgeResetsEveryPansPeaks()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* one = window.pan(QStringLiteral("one"));
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(one != nullptr && two != nullptr);
        for (SpectrumWidget* pan : {one, two}) { seedPeaks(pan); }
        window.controller->setKeyed(true, window.txSliceId);
        QVERIFY(wasReset(one));
        QVERIFY(wasReset(two));
        window.controller->setKeyed(false, -1);
    }

    // ── DUP on, a remote window ────────────────────────────────────────────

    void remoteDuplexKeepsTheReceiveFramesWhileKeyed()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        const PanView before = PanView::of(two);
        const float txRef = two->m_txRefLevel;
        window.controller->setDisplayDuplex(true);
        QVERIFY(window.gui->displayDuplex());
        QTRY_VERIFY_WITH_TIMEOUT(window.coreEndpointsDuplex(true), 5000);
        QSignalSpy transmitContexts(window.gui.get(),
                                    &RemoteMediaController::transmitContextReceived);

        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        // Red border, transmit grid and waterfall levels, on the receiver.
        QVERIFY(two->m_moxOverlay);
        QCOMPARE(two->m_refLevel, txRef);
        QCOMPARE(two->m_txWfLowLevel, -70);
        QVERIFY(!two->showsTransmitView());
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(!window.gui->panDisplayState(QStringLiteral("two")).transmitDisplayMissing);
        QCOMPARE(PanView::of(two), before);
        // The Core keeps the receiver for this pan: no viewer, no transmit
        // context, and receive frames draw while keyed.
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        QVERIFY(!window.anyCoreViewer());
        QCOMPARE(transmitContexts.count(), 0);

        // XIT does not move the view.
        SliceModel* coreSlice = window.station.sliceById(window.txSliceId);
        coreSlice->setXitHz(700);
        coreSlice->setXitEnabled(true);
        QTest::qWait(200);
        QCOMPARE(two->m_centerHz, before.centreHz);

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
        QVERIFY(!two->m_moxOverlay);
        QCOMPARE(PanView::of(two), before);
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
    }

    void remoteChangeWhileKeyedSwapsTheView()
    {
        RemoteWindow window(/*withAnalyzer=*/true);
        QVERIFY(window.connect());
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
        QTRY_VERIFY_WITH_TIMEOUT(window.anyCoreViewer(), 5000);

        // DUP on while keyed: the pan lets go of the transmit display at
        // once, and the Core goes back to the receiver for it.
        window.controller->setDisplayDuplex(true);
        QVERIFY(!window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(!two->showsTransmitView());
        QTRY_VERIFY_WITH_TIMEOUT(!window.anyCoreViewer(), 5000);
        QVERIFY(window.receiveDraws(QStringLiteral("two")));

        // Off again: the transmit display.
        window.controller->setDisplayDuplex(false);
        QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(two->showsTransmitView());
        QTRY_VERIFY_WITH_TIMEOUT(window.anyCoreViewer(), 5000);

        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());
    }

    // ── An older Core ──────────────────────────────────────────────────────

    void anOlderCoreLeavesDuplexUnavailableAndThePanAsDuplexOff()
    {
        // A Core without the transmit display sends 0: below 3.
        RemoteWindow window(/*withAnalyzer=*/false);
        QVERIFY(window.connect());
        QCOMPARE(window.client->capabilities().txDisplayVersion, 0);
        QTRY_COMPARE(window.remote.stationTxDisplayVersion(), 0);
        QCOMPARE(MoxDisplayController::displayDuplexUnavailableReason(&window.remote),
                 QString::fromLatin1(kUnavailable));
        QVERIFY(!window.controller->displayDuplexAvailable());
        window.controller->setDisplayDuplex(true);
        QVERIFY(!window.controller->displayDuplex());
        QVERIFY(!window.gui->displayDuplex());
        QVERIFY(!window.gui->displayDuplexNegotiated());
        SpectrumWidget* two = window.pan(QStringLiteral("two"));
        QVERIFY(window.receiveDraws(QStringLiteral("two")));
        window.key(true);
        QTRY_VERIFY(window.controller->isKeyed());
        // As DUP off: the pan is held and says the Core sends no transmit
        // display.
        QVERIFY(!two->displayDuplex());
        QVERIFY(window.gui->isPanTransmitting(QStringLiteral("two")));
        QVERIFY(window.gui->panDisplayState(QStringLiteral("two")).transmitDisplayMissing);
        window.key(false);
        QTRY_VERIFY(!window.controller->isKeyed());

        // A window running its own DSP always has DUP.
        RadioModel local;
        QVERIFY(MoxDisplayController::displayDuplexUnavailableReason(&local).isEmpty());
        // A remote window whose Core sends 3 has it too.
        RadioModel remote{RadioModel::Role::Remote};
        remote.setStationTxDisplayVersion(2);
        QCOMPARE(MoxDisplayController::displayDuplexUnavailableReason(&remote),
                 QString::fromLatin1(kUnavailable));
        remote.setStationTxDisplayVersion(3);
        QVERIFY(MoxDisplayController::displayDuplexUnavailableReason(&remote).isEmpty());
    }
};

QTEST_MAIN(TstDisplayDuplex)
#include "tst_display_duplex.moc"
