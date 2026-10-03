// =================================================================
// tests/tst_zero_slice_core.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Zero slices is a valid idle Core
// (slice control and shared listening plan Task 7).
//
// Modification history (NereusSDR):
//   2026-09-29: created for NereusSDR by J.J. Boyd (KG4VCF), slice control
//               and shared listening plan Task 7, with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: RADE threads: the zero-slice RADE check wires a real
//               channel and sees its route and decoder go. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-03: dynamic Diversity target and real software route lifecycle,
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================
//
// The model side of a Core with no slice: the claims rule's close of an
// unclaimed slice (the last one included), the state it leaves (no active
// slice, no transmit binding, no receiver streaming, no audio view, no saved
// layout), a working new slice from zero, and each first-slice reader
// returning a safe value at zero. The station side (release, removeSlice,
// the key refusal, the window's object.destroy and object.create) is in
// tst_slice_access_verbs. No radio, no RF.

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/DdcAssignment.h"
#include "core/P2RadioConnection.h"
#include "core/RadeChannel.h"
#include "core/WdspEngine.h"
#include "core/ReceiveLayoutStore.h"
#include "core/ReceiverManager.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "core/codec/P2CodecSaturn.h"
#include "core/session/SessionMessages.h"
#include "models/RxDspWorker.h"
#define private public
#include "core/P1RadioConnection.h"
#include "core/TciServer.h"
#include "models/RadioModel.h"
#undef private
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kCmdRxEnableByte = 7;
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:77");

quint8 cmdRxEnableMask(const P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdRxForTest(buf);
    return buf[kCmdRxEnableByte];
}

// RadioModel gates the wire push on isConnected(); the same test-local seam
// tst_p2_ddc_mask_ownership uses.
class TestableP2Connection : public P2RadioConnection {
    Q_OBJECT
public:
    using P2RadioConnection::P2RadioConnection;
    void markConnectedForTest() { setState(ConnectionState::Connected); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// A Saturn-class Core with one slice nobody owns, as a radio connect or a
// restart with no layout makes it (ruling Q10).
struct ZeroCore {
    RadioModel model;
    TestableP2Connection conn;
    DetachConnection detach{&model};

    ZeroCore()
    {
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.markConnectedForTest();
        model.injectConnectionForTest(&conn);
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }
    }
};

} // namespace

class TstZeroSliceCore : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("zero-slice-core-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    // Only an unclaimed slice closes this way: one with a controller, a
    // listener or a hold stays.
    void aClaimedSliceIsNeverClosedByTheClaimsRule()
    {
        ZeroCore core;
        const int a = core.model.addSlice();
        QVERIFY(a >= 0);
        SliceOwnership* ownership = core.model.sliceOwnership();
        ownership->setOwner(a, QByteArrayLiteral("device-a"));
        QVERIFY(!core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.sliceById(a) != nullptr);

        ownership->setOwner(a, QByteArray());
        QVERIFY(ownership->join(QByteArrayLiteral("device-b"), a));
        QVERIFY(!core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.sliceById(a) != nullptr);

        QVERIFY(ownership->leave(QByteArrayLiteral("device-a"), a)
                || !ownership->isListening(QByteArrayLiteral("device-a"), a));
        ownership->leave(QByteArrayLiteral("device-b"), a);
        ownership->hold(a, QByteArrayLiteral("device-a"));
        QVERIFY(!core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.sliceById(a) != nullptr);
    }

    // The last slice, unclaimed, closes and leaves a valid empty Core.
    void closingTheLastUnclaimedSliceLeavesAnIdleCore()
    {
        ZeroCore core;
        const int a = core.model.addSlice();
        QVERIFY(a >= 0);
        core.model.sliceById(a)->setFrequency(14200000.0);
        TxSliceArbiter* arbiter = core.model.txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), a);
        QVERIFY(core.model.hasTransmitSlice());
        QVERIFY(cmdRxEnableMask(core.conn) != 0);
        QVERIFY(core.model.sliceOwnership()->unclaimed().contains(a));
        QSignalSpy bound(arbiter, &TxSliceArbiter::txBoundSliceChanged);
        QSignalSpy removed(&core.model, &RadioModel::sliceRemoved);

        QVERIFY(core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.slices().isEmpty());
        QCOMPARE(removed.count(), 1);
        QVERIFY(core.model.activeSlice() == nullptr);
        QCOMPARE(arbiter->txBoundSliceId(), -1);
        QVERIFY(!arbiter->isHandoffPending());
        QVERIFY(!core.model.hasTransmitSlice());
        QVERIFY(core.model.txBoundSlice() == nullptr);
        QCOMPARE(bound.count(), 1);
        QCOMPARE(bound.first().at(0).toInt(), a);
        QCOMPARE(bound.first().at(1).toInt(), -1);
        // No receiver streams.
        QCOMPARE(cmdRxEnableMask(core.conn), quint8(0));
        for (int st = 0; st < core.model.streamAllocator().streamCount(); ++st) {
            QVERIFY(!core.model.streamAllocator().isStreamActive(st));
        }
        // No audio view (the audio thread sees no slice).
        if (AudioEngine* engine = core.model.m_audioEngine) {
            for (int id = 0; id < AudioEngine::kMaxSliceAudioViews; ++id) {
                QVERIFY(!engine->sliceAudioView(id).present);
            }
        }
        // Nothing left to close.
        QVERIFY(!core.model.closeUnclaimedSlice(a));
    }

    // From zero, a new slice works: a receiver, the transmit binding, the
    // active slice, and its audio view.
    void aNewSliceFromZeroWorks()
    {
        ZeroCore core;
        const int a = core.model.addSlice();
        QVERIFY(core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.slices().isEmpty());

        const int b = core.model.addSlice();
        QVERIFY(b >= 0);
        SliceModel* fresh = core.model.sliceById(b);
        QVERIFY(fresh != nullptr);
        fresh->setFrequency(7150000.0);
        QVERIFY(fresh->streamIndex() >= 0);
        QVERIFY(core.model.streamAllocator().isStreamActive(fresh->streamIndex()));
        QVERIFY(cmdRxEnableMask(core.conn) != 0);
        QCOMPARE(core.model.txSliceArbiter()->txBoundSliceId(), b);
        QVERIFY(fresh->isTxSlice());
        QVERIFY(core.model.hasTransmitSlice());
        QCOMPARE(core.model.activeSlice(), fresh);
        if (AudioEngine* engine = core.model.m_audioEngine) {
            QVERIFY(engine->sliceAudioView(b).present);
        }
    }

    // External diversity on Slice A stops when A closes at zero and is not
    // started again for a later new slice unless asked.
    void externalDiversityStopsAtZeroAndStaysOff()
    {
        RxDspWorker worker;
        P2CodecSaturn codec;
        ZeroCore core;
        core.model.setBoardForTest(HPSDRHW::Saturn);
        core.model.receiverManager()->setP2Codec(&codec);
        core.model.attachDspWorkerForTest(&worker);
        worker.setEngines(core.model.wdspEngine(), nullptr);
        worker.setBufferSizes(64, 64);
        // Real lane/worker route lifecycle with the established offline WDSP
        // API seam; no DSP sample processing, radio socket or RF.
        core.model.wdspEngine()->setExternalDiversityApiForTest({
            +[](int, int, int, int) {}, +[](int) {},
            +[](int, int, double**, double*) {}, +[](int, int) {},
            +[](int, int) {}, +[](int, int) {}, +[](int, int, double*, double*) {}});
        const int a = core.model.addSlice();
        QVERIFY(a >= 0);
        auto* own = core.model.sliceOwnership();
        own->setOwner(a, SliceOwnership::stationDevice());
        QVERIFY(core.model.diversityEligibility(a).isEmpty());
        const auto integer = [](const QByteArray& name, qint64 number) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, number};
        };
        const auto enable = SessionMessages::commandInvoke("diversity.setTarget", 901,
            {{0, "enabled", MirrorWireKind::Bool, true},
             integer("stateRevision", qint64(core.model.diversityStateRevision())),
             integer("sourceSliceId", -1), integer("sourceIncarnation", 0),
             integer("sourceControlRevision", 0), integer("targetSliceId", a),
             integer("targetIncarnation", qint64(own->incarnation(a))),
             integer("targetControlRevision", qint64(own->controlRevision(a)))});
        QVERIFY(core.model.invokeDiversityAsStationDevice(enable).accepted);
        QVERIFY(core.model.waitForReceiveLaneForTest());
        QVERIFY(core.model.sliceById(a)->diversityEnabled());
        QCOMPARE(core.model.diversityTargetSlice(), core.model.sliceById(a));
        QVERIFY(core.model.m_externalDiversityRouteActive);
        QVERIFY(core.model.m_externalDiversityPrimaryDdc >= 0);
        const auto oldIncarnation = own->incarnation(a);
        // The claims close remains inadmissible while a device controls it.
        QVERIFY(!core.model.closeUnclaimedSlice(a));
        own->setOwner(a, {});
        QVERIFY(own->leave(SliceOwnership::stationDevice(), a));
        QVERIFY(own->unclaimed().contains(a));
        QVERIFY(core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.waitForReceiveLaneForTest());
        QVERIFY(core.model.slices().isEmpty());
        QVERIFY(core.model.diversityTargetSlice() == nullptr);
        QVERIFY(!core.model.m_externalDiversityRouteActive);
        QCOMPARE(core.model.m_externalDiversityPrimaryDdc, -1);

        const int b = core.model.addSlice();
        QCOMPARE(b, a); // Actual freed id is reused, with a fresh identity.
        QVERIFY(own->incarnation(b) != oldIncarnation);
        QVERIFY(!core.model.sliceById(b)->diversityEnabled());
        // The legacy blend editor may resolve A while off; requested owner
        // and authoritative summary remain empty after reuse.
        QCOMPARE(core.model.m_diversityTargetSliceId, -1);
        const auto state = QJsonDocument::fromJson(core.model.diversityState().toUtf8()).object();
        QVERIFY(!state.value("requested").toBool());
        QVERIFY(state.value("live").isNull());
        QVERIFY(!core.model.m_externalDiversityRouteActive);
    }

    // The RADE receive target goes with the slice. RADE threads: the
    // slice's route and its decoder both go.
    void theRadeTargetIsClearedAtZero()
    {
        ZeroCore core;
        const int a = core.model.addSlice();
        SliceModel* slice = core.model.sliceById(a);
        RadeChannel* const channel = core.model.wdspEngine()->createRadeChannel(a);
        QVERIFY(channel);
        core.model.wireRadeChannel(a, channel, slice);
        QVERIFY(core.model.m_radeRxRoutes.contains(a));
        QVERIFY(core.model.closeUnclaimedSlice(a));
        QVERIFY(!core.model.m_radeRxRoutes.contains(a));
        QVERIFY(core.model.wdspEngine()->radeChannel(a) == nullptr);
    }

    // Ruling Q10: a Core with no slice saves no layout, so a restart starts
    // as a first start does (one Slice A nobody owns, made at connect).
    void aCoreWithNoSliceSavesNoLayout()
    {
        ZeroCore core;
        const int a = core.model.addSlice();
        core.model.sliceById(a)->setFrequency(14200000.0);
        AppSettings& settings = AppSettings::instance();
        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(
                     settings, kMac,
                     {{a, QStringLiteral("pan-0"), 14200000.0, DSPMode::USB, {}, {}}}, &error),
                 qPrintable(error));
        QCOMPARE(ReceiveLayoutStore::load(settings, kMac).state,
                 ReceiveLayoutStore::LoadState::Loaded);
        core.model.m_receiveLayoutManaged = true;
        core.model.m_receiveLayoutMac = kMac;

        QVERIFY(core.model.closeUnclaimedSlice(a));
        QVERIFY(core.model.captureReceiveLayout(&error));
        QVERIFY(error.isEmpty());
        QCOMPARE(ReceiveLayoutStore::load(settings, kMac).state,
                 ReceiveLayoutStore::LoadState::Missing);
    }

    // Every first-slice reader returns a safe value at zero.
    void firstSliceReadersAreSafeAtZero()
    {
        ZeroCore core;
        const int a = core.model.addSlice();
        QVERIFY(core.model.closeUnclaimedSlice(a));

        QVERIFY(core.model.coreFilterResponseSlice() == nullptr);
        QVERIFY(core.model.sliceById(0) == nullptr);
        QVERIFY(core.model.txBoundSlice() == nullptr);
        // Nothing to apply, nothing active, and no crash.
        core.model.applyActiveSlices();
        QVERIFY(core.model.activeSlice() == nullptr);
        core.model.publishSliceAudioViewForTest();
        core.model.requestDdcAssignment();
        QCOMPARE(cmdRxEnableMask(core.conn), quint8(0));
        core.model.txSliceArbiter()->syncToSliceList();
        QCOMPARE(core.model.txSliceArbiter()->txBoundSliceId(), -1);

        TciServer tci(&core.model);
        QCOMPARE(tci.remoteReceiverLevelDbm(), -140.0);
    }

    // Protocol 1 always streams its first receiver: a count of zero asks
    // for one.
    void protocolOneStillStreamsOneReceiverAtZero()
    {
        P1RadioConnection conn(nullptr);
        conn.setActiveReceiverCount(0);
        QCOMPARE(conn.m_panRxCount, 1);
    }
};

QTEST_MAIN(TstZeroSliceCore)
#include "tst_zero_slice_core.moc"
