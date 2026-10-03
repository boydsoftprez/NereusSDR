// =================================================================
// tests/tst_slice_cap_every_path.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Receiver and transmit gaps plan, Task 1 (Phase 3F design section 3, the
// slice cap and its message; R-R3-21, a remote window's request is held to
// the Core's rules). Before this task only RadioModel::addSliceOnPan checked
// the slice cap. RadioModel::addSlice(), and with it the session verb
// `addSlice` a remote window sends, created a slice past the board's limit
// whenever the new slice fitted an existing receiver window. The slice then
// sat on a slice id the Core never opened a demodulator channel for.
//
// Follow-up (controller ruling; R-R3-27, R-R3-34): a Core that has never
// reached its radio holds session adds to WdspEngine::kMaxSliceChannels, and
// when a smaller board then connects, the slices it cannot host are closed,
// highest id first, and named in plain words on the receive-layout restore
// status, so no slice is left looking configured with no channel.
//
// Follow-up (R-R3-34): a local window with no Core shows that closure too,
// through the same receive-layout toast (ReceiveLayoutNotices) a remote
// window uses.
//
// Fix wave 1, I1: addSliceOnPan, the verb every window's +RX sends, is held
// to the same ceiling and words as addSlice, before and after connect, and
// the limit is worded "1 slice" or "N slices".
//
// Task 9 (R-R3-21): before a pool is sized, the refusal names the Core on a
// Core and NereusSDR in a window with no Core.
//
// Covers: a local addSlice() at the cap returns -1, creates nothing and
// emits the cap message; the session verb at the cap is refused with the
// same plain reason and the Core creates nothing, both against a sized
// stream pool and against a real (fake) Hermes Lite 2 connection; below the
// cap nothing changes; addSliceWithStationId on a remote window is not
// refused by the window's own count.
// =================================================================

#include <QtTest/QtTest>
#include <QFile>
#include <QSignalSpy>

#include <atomic>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/daemon/DaemonConfig.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/P1FakeRadio.h"
// DaemonApp's discovery and retry seams are private, as in
// tst_receive_layout_native.cpp.
#define private public
#include "core/daemon/DaemonApp.h"
#include "models/RadioModel.h"
#undef private

#include "core/session/ObjectRegistry.h"
#include "gui/ReceiveLayoutNotices.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "OperatorWording.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/LoopbackStationLink.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LoopbackStationLink;

namespace {

MirrorUpdate strArg(const QByteArray& name, const QString& value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Utf8, QVariant(value) };
}

// Same shape as tst_session_verbs.cpp's DispatchHarness: decode on the
// Core's side, dispatch, send the result back, decode on the window's side.
class DispatchHarness : public QObject {
public:
    explicit DispatchHarness(RadioModel* radioModel)
        : dispatcher(radioModel)
    {
        connect(&link, &LoopbackStationLink::receivedByDaemon, this,
                [this](const QByteArray& wire) {
                    SessionMessage msg;
                    if (SessionMessages::decode(wire, &msg)) {
                        dispatcher.dispatch(msg);
                    }
                });
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [this](const SessionMessage& result) {
                    link.sendFromDaemon(SessionMessages::encode(result));
                });
        connect(&link, &LoopbackStationLink::receivedByClient, this,
                [this](const QByteArray& wire) {
                    SessionMessage msg;
                    if (SessionMessages::decode(wire, &msg)) {
                        results.append(msg);
                    }
                });
    }

    void invokeAddSlice(quint32 commandId)
    {
        link.sendFromClient(SessionMessages::encode(SessionMessages::commandInvoke(
            "addSlice", commandId, { strArg("initialPanId", QStringLiteral("pan-0")) })));
    }

    // The verb every window's +RX sends (MainWindow -> addSliceOnPan).
    void invokeAddSliceOnPan(quint32 commandId, const QString& panId = QStringLiteral("pan-0"))
    {
        link.sendFromClient(SessionMessages::encode(SessionMessages::commandInvoke(
            "addSliceOnPan", commandId, { strArg("panId", panId) })));
    }

    LoopbackStationLink link;
    SessionCommandDispatcher dispatcher;
    QList<SessionMessage> results;
};

void installOpenAudioBuses(AudioEngine& audio)
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    auto speakers = std::make_unique<FakeAudioBus>();
    auto mic = std::make_unique<FakeAudioBus>();
    speakers->open(format);
    mic->open(format);
    audio.setSpeakersBusForTest(std::move(speakers));
    audio.setTxInputBusForTest(std::move(mic));
}

// The closure sentence for one slice, as the operator reads it.
QString closedSentence(QChar letter, double frequencyHz, DSPMode mode)
{
    return QStringLiteral("Receiver %1 (%2\u00A0MHz %3) was closed because this radio "
                          "supports only receivers A and B. Add it again with +RX after "
                          "closing another receiver.")
        .arg(letter)
        .arg(QString::number(frequencyHz / 1.0e6, 'f', 4))
        .arg(SliceModel::modeName(mode));
}

} // namespace

class TestSliceCapEveryPath : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("slice-cap-every-path-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
    }

    void cleanupTestCase() { QFile::remove(AppSettings::instance().filePath()); }

    void localAddSliceAtCapReturnsMinusOneAndEmitsCapMessage()
    {
        RadioModel model;
        // Five receivers but room for only two slices: every add below
        // lands in the first slice's window, so the allocator alone would
        // accept all of them. Only the slice cap may refuse.
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 2, 192000);

        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(model.addSlice(), 1);
        QCOMPARE(rejected.count(), 0); // below the cap nothing changes

        QCOMPARE(model.addSlice(), -1);
        QCOMPARE(model.slices().size(), 2);
        QVERIFY(model.sliceById(2) == nullptr);
        QCOMPARE(rejected.count(), 1);
        const QString reason = rejected.at(0).at(0).toString();
        QCOMPARE(reason, QStringLiteral("This radio supports a maximum of 2 slices"));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    }

    void sessionVerbAddSliceAtCapIsRefusedWithTheCapReason()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 2, 192000);
        DispatchHarness harness(&model);

        harness.invokeAddSlice(1);
        harness.invokeAddSlice(2);
        QCOMPARE(harness.results.size(), 2);
        QVERIFY2(harness.results.at(0).accepted, qPrintable(harness.results.at(0).reason));
        QVERIFY2(harness.results.at(1).accepted, qPrintable(harness.results.at(1).reason));

        harness.invokeAddSlice(3);
        QCOMPARE(harness.results.size(), 3);
        const SessionMessage& refused = harness.results.at(2);
        QVERIFY(!refused.accepted);
        QCOMPARE(refused.commandId, quint32(3));
        QCOMPARE(refused.reason, QStringLiteral("This radio supports a maximum of 2 slices"));
        QVERIFY2(OperatorWording::isPlain(refused.reason), qPrintable(refused.reason));
        QVERIFY(refused.affectedKeys.isEmpty());
        QCOMPARE(model.slices().size(), 2);
        QVERIFY(model.sliceById(2) == nullptr);
    }

    void sessionVerbAddSliceAtCapOnAConnectedCoreIsRefused()
    {
        // The Core in production: a real (fake) Hermes Lite 2 connection.
        // HL2 allows five slices on two receivers, so slices share a window
        // and the allocator never refuses the sixth by itself.
        std::unique_ptr<ConnectableRadioModel> connected = ConnectableRadioModel::create();
        QVERIFY(connected);
        RadioModel& model = connected->model();
        QVERIFY(model.isConnected());
        const int cap = model.maxSlices();
        QCOMPARE(cap, 5);
        QCOMPARE(model.slices().size(), 1); // connectToRadio creates Slice A

        DispatchHarness harness(&model);
        for (quint32 id = 1; model.slices().size() < cap; ++id) {
            harness.invokeAddSlice(id);
            QVERIFY(!harness.results.isEmpty());
            QVERIFY2(harness.results.last().accepted,
                     qPrintable(harness.results.last().reason));
        }

        const qsizetype before = harness.results.size();
        harness.invokeAddSlice(99);
        QCOMPARE(harness.results.size(), before + 1);
        const SessionMessage& refused = harness.results.last();
        QVERIFY(!refused.accepted);
        QVERIFY2(refused.reason.endsWith(QStringLiteral(" supports a maximum of 5 slices")),
                 qPrintable(refused.reason));
        QVERIFY2(OperatorWording::isPlain(refused.reason), qPrintable(refused.reason));
        QVERIFY(refused.affectedKeys.isEmpty());
        QCOMPARE(model.slices().size(), cap);
        QVERIFY(model.sliceById(cap) == nullptr);
    }

    void addSliceOnPanAndAddSliceShareTheCapWording()
    {
        // Fix wave 1, I1: addSliceOnPan (every window's +RX) is held to the
        // same ceiling as addSlice, with the same words.
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 2, 192000);
        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);
        model.addSliceOnPan(QStringLiteral("pan-0"));
        model.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(rejected.count(), 0);
        QCOMPARE(model.slices().size(), 2);

        model.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(model.addSlice(QStringLiteral("pan-0")), -1);
        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(rejected.count(), 2);
        const QString reason = QStringLiteral("This radio supports a maximum of 2 slices");
        QCOMPARE(rejected.at(0).at(0).toString(), reason);
        QCOMPARE(rejected.at(1).at(0).toString(), reason);
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    }

    void aOneSliceLimitIsWordedInTheSingular()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 1, /*maxSlices*/ 1, 192000);
        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);
        model.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(rejected.count(), 0);
        model.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(rejected.count(), 1);
        const QString reason = rejected.at(0).at(0).toString();
        QCOMPARE(reason, QStringLiteral("This radio supports a maximum of 1 slice"));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    }

    void remoteWindowReproducesStationSlicesPastItsOwnCount()
    {
        // A remote window reproduces slices the Core already made. Its own
        // maxSlices() is 1 until the Core advertises a limit, and that must
        // never refuse a slice the Core has already created.
        RadioModel remote(RadioModel::Role::Remote);
        QCOMPARE(remote.maxSlices(), 1);
        QSignalSpy rejected(&remote, &RadioModel::sliceAddRejected);

        QCOMPARE(remote.addSliceWithStationId(0, QStringLiteral("pan-0")), 0);
        QCOMPARE(remote.addSliceWithStationId(1, QStringLiteral("pan-0")), 1);
        QCOMPARE(remote.addSliceWithStationId(2, QStringLiteral("pan-1")), 2);
        QCOMPARE(remote.slices().size(), 3);
        QCOMPARE(rejected.count(), 0);
    }

    // ── Before the first radio connect (R-R3-27) ─────────────────────────

    void neverConnectedWindowAddsFiveSlicesWithPlusRxAndIsRefusedTheSixth()
    {
        // Fix wave 1, I1: a window with no radio yet takes the absolute
        // ceiling on its +RX path, not maxSlices()'s disconnected 1.
        // Task 9: a window with no Core names NereusSDR, not a Core it
        // does not have.
        RadioModel model; // no radio has ever connected: no stream pool
        QCOMPARE(WdspEngine::kMaxSliceChannels, 5);
        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);
        for (int i = 0; i < 5; ++i) {
            model.addSliceOnPan(QStringLiteral("pan-0"));
        }
        QCOMPARE(rejected.count(), 0);
        QCOMPARE(model.slices().size(), 5);

        model.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(rejected.count(), 1);
        const QString reason = rejected.at(0).at(0).toString();
        QCOMPARE(reason, QStringLiteral("NereusSDR supports a maximum of 5 slices"));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QCOMPARE(model.slices().size(), 5);
        QVERIFY(model.sliceById(5) == nullptr);
    }

    void neverConnectedCoreRefusesTheSixthSessionAddSliceOnPan()
    {
        RadioModel model; // no radio has ever connected: no stream pool
        // A Core's model, as DaemonApp sets it up: the station listener
        // rule is what only a Core has.
        model.setStationBind(QString());
        QCOMPARE(WdspEngine::kMaxSliceChannels, 5);
        DispatchHarness harness(&model);

        for (quint32 id = 1; id <= 5; ++id) {
            harness.invokeAddSliceOnPan(id);
            QCOMPARE(harness.results.size(), qsizetype(id));
            QVERIFY2(harness.results.last().accepted,
                     qPrintable(harness.results.last().reason));
        }
        QCOMPARE(model.slices().size(), 5);

        harness.invokeAddSliceOnPan(6);
        QCOMPARE(harness.results.size(), 6);
        const SessionMessage refused = harness.results.last();
        QVERIFY(!refused.accepted);
        QCOMPARE(refused.reason, QStringLiteral("The Core supports a maximum of 5 slices"));
        QVERIFY2(OperatorWording::isPlain(refused.reason), qPrintable(refused.reason));
        QVERIFY(refused.affectedKeys.isEmpty());
        QCOMPARE(model.slices().size(), 5);
        QVERIFY(model.sliceById(5) == nullptr);

        // The addSlice verb is held to the same ceiling, in the same words.
        harness.invokeAddSlice(7);
        QCOMPARE(harness.results.size(), 7);
        QVERIFY(!harness.results.last().accepted);
        QCOMPARE(harness.results.last().reason, refused.reason);
        QCOMPARE(model.slices().size(), 5);
    }

    // ── A smaller board connects (R-R3-34) ───────────────────────────────

    void coreHoldingFourSlicesClosesTwoWhenATwoSliceBoardConnects()
    {
        // The Core in production: DaemonApp started with its radio off. A
        // window adds slices while it waits; then a HermesII (two slices)
        // is switched on and discovered.
        Test::P1FakeRadio fake;
        fake.start();
        RadioInfo info;
        info.address = fake.localAddress();
        info.port = fake.localPort();
        info.boardType = HPSDRHW::HermesII;
        info.protocol = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:61");
        info.firmwareVersion = 72;
        info.name = QStringLiteral("Slice cap loopback");

        auto radioOn = std::make_shared<std::atomic_bool>(false);
        DaemonApp app;
        app.m_synchronousWdspForTest = true;
        app.m_radioRetryInitialMs = 50;
        app.m_radioRetryNextMs = 50;
        app.m_radioRetryMaximumMs = 50;
        app.m_discoveryProviderForTest = [info, radioOn] {
            return radioOn->load() ? QList<RadioInfo>{info} : QList<RadioInfo>{};
        };
        app.m_radioInitializerForTest = [](RadioModel* model) {
            model->audioEngine()->setStartInitializerForTest(installOpenAudioBuses);
        };
        DaemonConfig config = DaemonConfig::defaults();
        config.radioMac = info.macAddress;
        config.remotePort = 0;
        config.sampleRateHz = 48000;
        config.sliceCount = 1;
        QVERIFY(app.start(config));
        RadioModel* model = app.m_radioModel.get();
        QVERIFY(!model->isConnected());
        QCOMPARE(model->slices().size(), 1);

        DispatchHarness harness(model);
        for (quint32 id = 1; id <= 3; ++id) {
            harness.invokeAddSliceOnPan(id);
            QVERIFY2(harness.results.last().accepted,
                     qPrintable(harness.results.last().reason));
        }
        QCOMPARE(model->slices().size(), 4);
        const QString expected = closedSentence(QLatin1Char('C'),
                                                model->sliceById(2)->frequency(),
                                                model->sliceById(2)->dspMode())
            + QLatin1Char(' ')
            + closedSentence(QLatin1Char('D'), model->sliceById(3)->frequency(),
                             model->sliceById(3)->dspMode());

        radioOn->store(true);
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 15000);

        QCOMPARE(model->slices().size(), 2);
        QVERIFY(model->sliceById(2) == nullptr);
        QVERIFY(model->sliceById(3) == nullptr);
        for (int id : {0, 1}) {
            SliceModel* slice = model->sliceById(id);
            QVERIFY(slice);
            QVERIFY(slice->streamIndex() >= 0);
            RxChannel* channel = model->wdspEngine()->rxChannel(id);
            QVERIFY2(channel && channel->isActive(), qPrintable(QString::number(id)));
        }
        QCOMPARE(model->receiveLayoutRestoreState(), QStringLiteral("degraded"));
        QCOMPARE(model->receiveLayoutRestoreMessage(), expected);
        QVERIFY2(OperatorWording::isPlain(expected), qPrintable(expected));
        app.stop();
    }

    void windowReconnectingToASmallerBoardClosesSlicesItCannotHost()
    {
        // A local window (no saved-layout management) runs four slices on a
        // Hermes Lite 2, disconnects, and reconnects to a two-slice
        // HermesII. Before Task 1 the extra slices stayed, bound to a
        // stream, with no WDSP channel behind their ids.
        //
        // Fix wave 1, M5: a real disconnect and reconnect to the smaller
        // board, and the slices kept are asserted running, not merely
        // present. M3: the window's notice memory is cleared on disconnect
        // (MainWindow's local wiring), so the same closure on a later
        // reconnect is toasted again.
        ReceiveLayoutNotices notices;
        QStringList toasts;
        QObject receiver;
        const auto wireLocalWindow = [&](RadioModel& model) {
            // MainWindow's local receive-layout connections: the same toast
            // the remote window shows, fed from the same status signal, and
            // the notice memory forgotten when the radio disconnects.
            QObject::connect(&model, &RadioModel::receiveLayoutRestoreStatusChanged,
                             &receiver, [&] {
                const QString toast = notices.toastFor(
                    model.receiveLayoutRestoreState(),
                    model.receiveLayoutRestoreMessage(), /*viaCore*/ false);
                if (!toast.isEmpty()) {
                    toasts.append(toast);
                }
            });
            QObject::connect(&model, &RadioModel::connectionStateChanged, &receiver,
                             [&](ConnectionState state) {
                if (state == ConnectionState::Disconnected) {
                    notices.forget();
                }
            });
        };
        auto connected = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, wireLocalWindow, HPSDRHW::HermesLite);
        QVERIFY(connected);
        RadioModel& model = connected->model();
        QCOMPARE(model.maxSlices(), 5);

        const double freqC = 14'225'000.0;
        const double freqD = 7'150'000.0;
        const DSPMode modeC = DSPMode::USB;
        const DSPMode modeD = DSPMode::LSB;
        const auto fillToFourSlices = [&] {
            while (model.slices().size() < 4) {
                if (model.addSlice() < 0) {
                    return false;
                }
            }
            model.sliceById(2)->setFrequency(freqC);
            model.sliceById(2)->setDspMode(modeC);
            model.sliceById(3)->setFrequency(freqD);
            model.sliceById(3)->setDspMode(modeD);
            return true;
        };
        const auto reconnectAs = [&](HPSDRHW board) {
            model.disconnectFromRadio();
            if (!QTest::qWaitFor([&] {
                    return model.connectionState() == ConnectionState::Disconnected;
                }, 10000)) {
                return false;
            }
            RadioInfo info = connected->radioInfo();
            info.boardType = board;
            model.connectToRadio(info);
            return QTest::qWaitFor([&] {
                return model.connectionState() == ConnectionState::Connected;
            }, 10000);
        };
        const QString expected = closedSentence(QLatin1Char('C'), freqC, modeC)
            + QLatin1Char(' ') + closedSentence(QLatin1Char('D'), freqD, modeD);

        for (int round = 1; round <= 2; ++round) {
            QVERIFY2(fillToFourSlices(), qPrintable(QString::number(round)));
            QCOMPARE(model.slices().size(), 4);

            QVERIFY2(reconnectAs(HPSDRHW::HermesII), qPrintable(QString::number(round)));
            QCOMPARE(model.maxSlices(), 2);
            QCOMPARE(model.slices().size(), 2);
            QVERIFY(model.sliceById(2) == nullptr);
            QVERIFY(model.sliceById(3) == nullptr);
            for (int id : {0, 1}) {
                SliceModel* slice = model.sliceById(id);
                QVERIFY(slice);
                QVERIFY(slice->streamIndex() >= 0);
                RxChannel* channel = model.wdspEngine()->rxChannel(id);
                QVERIFY2(channel && channel->isActive(),
                         qPrintable(QStringLiteral("round %1, slice %2").arg(round).arg(id)));
            }
            QCOMPARE(model.receiveLayoutRestoreState(), QStringLiteral("degraded"));
            QCOMPARE(model.receiveLayoutRestoreMessage(), expected);
            // R-R3-34: the closure is not silent in a window with no Core,
            // and M3: each reconnect's closure is its own toast, the
            // message as written (no pointer to a Core panel).
            QTRY_COMPARE(toasts.size(), round);
            QCOMPARE(toasts.last(), expected);

            if (round == 1) {
                // Back to the larger board for the second round.
                QVERIFY(reconnectAs(HPSDRHW::HermesLite));
                QCOMPARE(model.maxSlices(), 5);
            }
        }
    }

    void receiveLayoutNoticeIsTheSameToastLocallyAndThroughACore()
    {
        const QString message = closedSentence(QLatin1Char('C'), 14'225'000.0, DSPMode::USB);
        ReceiveLayoutNotices local;
        QCOMPARE(local.toastFor(QStringLiteral("degraded"), message, false), message);
        QVERIFY(local.toastFor(QStringLiteral("degraded"), message, false).isEmpty()); // once
        QVERIFY(local.toastFor(QStringLiteral("pending"), QStringLiteral("x"), false).isEmpty());
        QVERIFY(local.toastFor(QStringLiteral("accepted"), QString(), false).isEmpty());
        QCOMPARE(local.toastFor(QStringLiteral("degraded"), message, false), message); // again
        QVERIFY(local.toastFor(QStringLiteral("degraded"), message, false).isEmpty());
        local.forget(); // the radio disconnected: the next closure is news
        QCOMPARE(local.toastFor(QStringLiteral("degraded"), message, false), message);
        ReceiveLayoutNotices remote;
        QCOMPARE(remote.toastFor(QStringLiteral("fallback"), message, true),
                 message + QStringLiteral(" Details remain in Core connection."));
        QVERIFY(OperatorWording::isPlain(message));
    }
};

QTEST_MAIN(TestSliceCapEveryPath)
#include "tst_slice_cap_every_path.moc"
