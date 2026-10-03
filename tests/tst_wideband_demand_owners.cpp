// =================================================================
// tests/tst_wideband_demand_owners.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original integration coverage. Thetis file names
// appear only in source-cite comments documenting the P2 byte layout; this
// file ports no upstream logic.
//
// Remote wideband demand owners are endpoints, not slice properties. They
// must aggregate with the local property but remain independently retireable,
// and capture is keyed by physical ADC while Alex bypass is keyed by filter
// chain. The synthetic 2-ADC/1-chain topology below is intentional: it pins
// the identity split without making a claim about a production radio SKU.
// =================================================================

#include <QtTest/QtTest>

#include <atomic>

#include "core/DdcAssignment.h"
#include "core/P2RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// CmdGeneral byte 23 is the per-physical-ADC wideband enable bitmask.
// From Thetis ChannelMaster/network.c:879 [v2.10.3.15]:
//   packetbuf[23] = (char)_InterlockedAnd(&prn->wb_enable, 0xff);
constexpr int kCmdGeneralWbEnableByte = 23;
constexpr quint8 kMaskAdc0 = 0x01;
constexpr quint8 kMaskAdc1 = 0x02;

quint8 cmdGeneralWbMask(const P2RadioConnection& conn)
{
    quint8 buf[60] = {};
    conn.composeCmdGeneralForTest(buf);
    return buf[kCmdGeneralWbEnableByte];
}

void declareLiveP2(RadioModel& model, P2RadioConnection& conn)
{
    conn.setBoardForTest(HPSDRHW::Saturn);
    // Start from the canonical capable profile before the test-only topology
    // copies it. The synthetic counts change routing only; the rest of the
    // fixture must retain a real profile's receiver and Alex capabilities.
    model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    model.injectConnectionForTest(&conn);
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    model.setLastRadioInfoForTest(info);
}

void configureDemandFixture(RadioModel& model, P2RadioConnection& conn)
{
    declareLiveP2(model, conn);
    model.setWidebandTopologyForTest(/*adcCount*/ 2, /*widebandAdcs*/ 2,
                                     /*rxFilterChainCount*/ 1);
    // Demand is meaningful only for a bound slice. Size this before addSlice
    // so the test follows production routing rather than a default chain.
    model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
}

void routeSliceToAdc(RadioModel& model, int sliceId, int adc)
{
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice);
    const int stream = slice->streamIndex();
    QVERIFY(stream >= 0);

    DdcAssignment assignment{};
    assignment.streamDdc[stream] = 0;
    assignment.rate[0] = 192000;
    assignment.ddcEnable = 0x01;
    // DDC0's physical ADC selector occupies adcCtrl1 bits 1:0.
    assignment.adcCtrl1 = adc;
    model.publishDdcAssignmentForTest(assignment);
    QCOMPARE(model.sliceAdcIndex(sliceId), adc);
    // Both physical inputs share filter chain 0 in this synthetic topology.
    QCOMPARE(model.sliceChainIndex(sliceId), 0);
}

void verifyFiltered(const RadioModel& model)
{
    QCOMPARE(model.filterChainState(0).effective,
             AlexController::BpfEffective::Filtered);
}

void verifyWidebandLocked(const RadioModel& model)
{
    QCOMPARE(model.filterChainState(0).effective,
             AlexController::BpfEffective::WidebandLocked);
}

} // namespace

class TestWidebandDemandOwners : public QObject {
    Q_OBJECT

private slots:
    void separate_adcs_share_only_the_filter_bypass_data()
    {
        QTest::addColumn<int>("firstToRelease");
        QTest::newRow("release-adc0-first") << 0;
        QTest::newRow("release-adc1-first") << 1;
    }

    void separate_adcs_share_only_the_filter_bypass()
    {
        QFETCH(int, firstToRelease);
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);
        const int firstId = model.addSlice(QStringLiteral("pan-0"));
        const int secondId = model.addSlice(QStringLiteral("pan-1"));
        const auto* first = model.sliceById(firstId);
        const auto* second = model.sliceById(secondId);
        QVERIFY(first && second);
        QVERIFY(first->streamIndex() >= 0 && second->streamIndex() >= 0);
        QVERIFY(first->streamIndex() != second->streamIndex());

        DdcAssignment assignment{};
        assignment.streamDdc[first->streamIndex()] = 2;
        assignment.streamDdc[second->streamIndex()] = 3;
        assignment.rate[2] = assignment.rate[3] = 192000;
        assignment.ddcEnable = 0x0c;
        assignment.adcCtrl1 = (1 << 6); // DDC2 -> ADC0, DDC3 -> ADC1.
        model.publishDdcAssignmentForTest(assignment);
        QCOMPARE(model.sliceAdcIndex(firstId), 0);
        QCOMPARE(model.sliceAdcIndex(secondId), 1);
        QCOMPARE(model.sliceChainIndex(firstId), 0);
        QCOMPARE(model.sliceChainIndex(secondId), 0);

        const std::array tokens{model.acquireWidebandDemand(firstId),
                                model.acquireWidebandDemand(secondId)};
        QVERIFY(tokens[0] != 0 && tokens[1] != 0);
        QVERIFY(model.setWidebandDemandActive(tokens[0], true));
        QVERIFY(model.setWidebandDemandActive(tokens[1], true));
        QCOMPARE(cmdGeneralWbMask(conn), quint8(0x03));
        verifyWidebandLocked(model);

        model.releaseWidebandDemand(tokens[firstToRelease]);
        const int survivor = 1 - firstToRelease;
        QCOMPARE(cmdGeneralWbMask(conn), quint8(1 << survivor));
        verifyWidebandLocked(model);
        model.releaseWidebandDemand(tokens[survivor]);
        QCOMPARE(cmdGeneralWbMask(conn), quint8(0));
        verifyFiltered(model);
    }

    void two_endpoint_leases_on_one_adc_hold_capture_until_the_last_release()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);

        const auto first = model.acquireWidebandDemand(sliceId);
        const auto second = model.acquireWidebandDemand(sliceId);
        QVERIFY(first != 0);
        QVERIFY(second != 0);
        QVERIFY(first != second);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));

        QVERIFY(model.setWidebandDemandActive(first, true));
        QVERIFY(model.setWidebandDemandActive(second, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);
        verifyWidebandLocked(model);

        model.releaseWidebandDemand(first);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);
        verifyWidebandLocked(model);

        model.releaseWidebandDemand(second);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));
        verifyFiltered(model);
    }

    void local_slice_and_endpoint_owners_are_aggregated_separately()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice);

        const auto token = model.acquireWidebandDemand(sliceId);
        QVERIFY(token != 0);
        slice->setWidebandExtensionRequested(true);
        QVERIFY(model.setWidebandDemandActive(token, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);

        // Local clear must not release an endpoint owner.
        slice->setWidebandExtensionRequested(false);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);
        verifyWidebandLocked(model);

        QVERIFY(model.setWidebandDemandActive(token, false));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));
        verifyFiltered(model);

        // The inactive lease still exists and can be shown again.
        QVERIFY(model.setWidebandDemandActive(token, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);
        model.releaseWidebandDemand(token);
    }

    void deactivate_hides_an_owner_but_release_retires_its_token()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);
        const auto token = model.acquireWidebandDemand(sliceId);
        QVERIFY(token != 0);

        QVERIFY(model.setWidebandDemandActive(token, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);
        QVERIFY(model.setWidebandDemandActive(token, false));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));

        // Hide/deactivate is reversible.
        QVERIFY(model.setWidebandDemandActive(token, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);
        model.releaseWidebandDemand(token);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));

        // Release is idempotent but the retired capability can never revive.
        model.releaseWidebandDemand(token);
        QVERIFY(!model.setWidebandDemandActive(token, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));
    }

    void an_active_endpoint_follows_adc_remap_while_its_shared_chain_stays_bypassed()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);
        const auto token = model.acquireWidebandDemand(sliceId);
        QVERIFY(token != 0);
        QVERIFY(model.setWidebandDemandActive(token, true));
        QCOMPARE(cmdGeneralWbMask(conn) & (kMaskAdc0 | kMaskAdc1), kMaskAdc0);
        verifyWidebandLocked(model);

        routeSliceToAdc(model, sliceId, 1);
        // Capture follows the physical ADC. The one physical filter bank has
        // not changed identity and remains bypassed throughout the remap.
        QCOMPARE(cmdGeneralWbMask(conn) & (kMaskAdc0 | kMaskAdc1), kMaskAdc1);
        verifyWidebandLocked(model);
    }

    void removing_a_slice_retires_its_lease_before_the_numeric_id_is_reused()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        // RadioModel intentionally retains one slice, so keep an unrelated
        // anchor while removing the leased slice whose lowest-free ID will be
        // reused by the next add.
        const int anchorId = model.addSlice();
        QVERIFY(anchorId >= 0);
        const int oldId = model.addSlice();
        QVERIFY(oldId >= 0);
        QVERIFY(oldId != anchorId);
        routeSliceToAdc(model, oldId, 0);
        const auto oldToken = model.acquireWidebandDemand(oldId);
        QVERIFY(oldToken != 0);
        QVERIFY(model.setWidebandDemandActive(oldToken, true));
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, kMaskAdc0);

        model.removeSlice(oldId);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));
        QVERIFY(!model.setWidebandDemandActive(oldToken, true));

        const int reusedId = model.addSlice();
        QCOMPARE(reusedId, oldId);
        routeSliceToAdc(model, reusedId, 0);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));
        QVERIFY(!model.setWidebandDemandActive(oldToken, true));
    }

    void invalid_or_ineligible_slices_cannot_mint_a_demand_owner()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);
        QCOMPARE(model.acquireWidebandDemand(-1), RadioModel::WidebandDemandToken(0));
        QCOMPARE(model.acquireWidebandDemand(999), RadioModel::WidebandDemandToken(0));

        RadioModel remote(RadioModel::Role::Remote);
        QCOMPARE(remote.acquireWidebandDemand(0), RadioModel::WidebandDemandToken(0));

        P2RadioConnection unsupportedConn;
        RadioModel unsupported;
        declareLiveP2(unsupported, unsupportedConn);
        unsupported.setWidebandTopologyForTest(/*adcCount*/ 2,
                                               /*widebandAdcs*/ 1,
                                               /*rxFilterChainCount*/ 1);
        unsupported.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
        const int sliceId = unsupported.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(unsupported, sliceId, 1);
        SliceModel* unsupportedSlice = unsupported.sliceById(sliceId);
        QVERIFY(unsupportedSlice);
        unsupportedSlice->setWidebandExtensionRequested(true);
        QCOMPARE(unsupported.acquireWidebandDemand(sliceId),
                 RadioModel::WidebandDemandToken(0));
        QCOMPARE(cmdGeneralWbMask(unsupportedConn), quint8(0));
        verifyFiltered(unsupported);
    }

    void detaching_a_radio_retires_old_owners_before_a_new_connection_can_mint()
    {
        P2RadioConnection oldConnection;
        P2RadioConnection newConnection;
        RadioModel model;
        configureDemandFixture(model, oldConnection);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);
        const auto oldToken = model.acquireWidebandDemand(sliceId);
        QVERIFY(oldToken != 0);
        QVERIFY(model.setWidebandDemandActive(oldToken, true));
        QCOMPARE(cmdGeneralWbMask(oldConnection) & kMaskAdc0, kMaskAdc0);
        verifyWidebandLocked(model);

        // The model no longer owns this connection. Retiring its leases must
        // happen as it leaves Connected, before observers of that transition.
        model.injectConnectionForTest(nullptr);
        QVERIFY(!model.setWidebandDemandActive(oldToken, true));
        verifyFiltered(model);

        declareLiveP2(model, newConnection);
        const auto newToken = model.acquireWidebandDemand(sliceId);
        QVERIFY(newToken > oldToken);
        QVERIFY(!model.setWidebandDemandActive(oldToken, true));
        QVERIFY(model.setWidebandDemandActive(newToken, true));
        QCOMPARE(cmdGeneralWbMask(newConnection) & kMaskAdc0, kMaskAdc0);
        verifyWidebandLocked(model);
    }

    void idempotent_demand_requests_do_not_advance_capture_epochs_twice()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);
        const auto token = model.acquireWidebandDemand(sliceId);
        QVERIFY(token != 0);
        const auto epoch = conn.widebandCaptureEpoch(0);
        QVERIFY(epoch);

        QVERIFY(model.setWidebandDemandActive(token, true));
        const quint64 afterEnable = epoch->load(std::memory_order_acquire);
        QVERIFY(model.setWidebandDemandActive(token, true));
        QCOMPARE(epoch->load(std::memory_order_acquire), afterEnable);

        QVERIFY(model.setWidebandDemandActive(token, false));
        const quint64 afterDisable = epoch->load(std::memory_order_acquire);
        QVERIFY(model.setWidebandDemandActive(token, false));
        QCOMPARE(epoch->load(std::memory_order_acquire), afterDisable);
    }

    void observer_release_during_alex_notification_cannot_restore_a_stale_enable()
    {
        P2RadioConnection conn;
        RadioModel model;
        configureDemandFixture(model, conn);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        routeSliceToAdc(model, sliceId, 0);
        const auto token = model.acquireWidebandDemand(sliceId);
        QVERIFY(token != 0);

        bool releasedFromObserver = false;
        connect(&model.alexControllerMutable(),
                &AlexController::bpfStateChanged, &model,
                [&](int chain, const AlexController::AlexAdcState& state) {
                    if (chain == 0
                        && state.effective == AlexController::BpfEffective::WidebandLocked
                        && !releasedFromObserver) {
                        releasedFromObserver = true;
                        model.releaseWidebandDemand(token);
                    }
                });

        QVERIFY(model.setWidebandDemandActive(token, true));
        QVERIFY(releasedFromObserver);
        QCOMPARE(cmdGeneralWbMask(conn) & kMaskAdc0, quint8(0));
        verifyFiltered(model);
        QVERIFY(!model.setWidebandDemandActive(token, true));
    }
};

QTEST_MAIN(TestWidebandDemandOwners)
#include "tst_wideband_demand_owners.moc"
