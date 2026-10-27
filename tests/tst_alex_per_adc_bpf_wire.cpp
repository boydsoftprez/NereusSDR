// =================================================================
// tests/tst_alex_per_adc_bpf_wire.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Expected wire
// values are cited to Thetis in comments, but nothing here is a port.
//
// Phase 3F: the per-ADC BPF decision must (a) be fed from the live slice
// set and (b) reach the composed Alex wire bytes.
//
// Reported by CT1IQI on PR #293 (2026-05-31): on a 2-ADC radio the Alex
// filter chain has to be reviewed per ADC over the set of DDCs that ADC
// feeds. Before this test existed, AlexController::recomputeBpf computed
// exactly that answer and nothing consumed it: the wire took its HPF from
// whichever receiver was retuned last, so slice A on 20 m went deaf the
// moment slice B tuned 40 m.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/SliceOwnership.h"
#include "core/session/SessionMessages.h"
#include <QJsonDocument>
#include "core/accessories/AlexController.h"
#include "core/codec/AlexFilterMap.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// Captures the per-ADC BPF decisions RadioModel pushes at the connection.
// File scope, not an anonymous namespace: moc cannot generate a staticMetaObject
// for a Q_OBJECT class with internal linkage.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<AlexRxBpf> bpfCalls;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf b) override { bpfCalls.append(b); }
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
    void sendTxIq(const float*, int) override {}
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

namespace {

// Detaches a stack-injected connection however the scope is left: QCOMPARE
// returns from the enclosing slot on failure, which would otherwise skip the
// trailing detach on exactly the run where it matters.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// Alex0 register bit positions, from Thetis ChannelMaster/network.h:263-307
// [v2.10.3.15] as encoded by P2CodecOrionMkII::buildAlex0.
constexpr quint32 kAlex0Bit13MHz   = 1u << 1;
constexpr quint32 kAlex0Bit9_5MHz  = 1u << 4;
constexpr quint32 kAlex0Bit6_5MHz  = 1u << 5;
constexpr quint32 kAlex0BitBypass  = 1u << 12;

// What a receive bypass goes out as on the Anvelina Pro 3: the 160, 80, 40,
// 20 and 12/10 m rows together (codec::alex::receiveBypassSelection).
constexpr int kAnvelinaReceiveBypass = 0x10 | 0x08 | 0x04 | 0x01 | 0x02;

// LPF bits live in the top half of the same register; the RX BPF work must
// not disturb any of them.
constexpr quint32 kAlexLpfMask =
    (1u << 20) | (1u << 21) | (1u << 22) | (1u << 23) |
    (1u << 29) | (1u << 30) | (1u << 31);

// Encode an Alex LPF mask into its register bit positions. Identical in both
// words: Thetis ChannelMaster/network.h:263-307 (rbpfilter, Alex0) and
// :312-356 (rbpfilter2, Alex1) [v2.10.3.15] declare the same seven fields at
// the same offsets --
//   _30_20_LPF bit 20, _60_40_LPF bit 21, _80_LPF bit 22, _160_LPF bit 23,
//   _6_LPF bit 29, _12_10_LPF bit 30, _17_15_LPF bit 31.
quint32 encodeLpf(quint8 bits)
{
    quint32 reg = 0;
    if (bits & 0x01) { reg |= (1u << 20); }
    if (bits & 0x02) { reg |= (1u << 21); }
    if (bits & 0x04) { reg |= (1u << 22); }
    if (bits & 0x08) { reg |= (1u << 23); }
    if (bits & 0x10) { reg |= (1u << 29); }
    if (bits & 0x20) { reg |= (1u << 30); }
    if (bits & 0x40) { reg |= (1u << 31); }
    return reg;
}

quint32 readBE32(const quint8* buf, int offset)
{
    return (quint32(buf[offset])     << 24)
         | (quint32(buf[offset + 1]) << 16)
         | (quint32(buf[offset + 2]) << 8)
         |  quint32(buf[offset + 3]);
}

// Compose a CmdHighPriority frame and hand back the Alex0 / Alex1 registers.
// Byte offsets from Thetis ChannelMaster/network.c:1040-1050 [v2.10.3.15]:
// Alex1 at 1428-1431, Alex0 at 1432-1435.
void composeAlexRegisters(const CodecContext& ctx, quint32* alex0, quint32* alex1)
{
    P2CodecOrionMkII codec;
    static quint8 buf[1444];
    memset(buf, 0, sizeof(buf));
    codec.composeCmdHighPriority(ctx, buf);
    *alex0 = readBE32(buf, 1432);
    *alex1 = readBE32(buf, 1428);
}

} // namespace

class TestAlexPerAdcBpfWire : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── Half 1: the analysis is fed from the live slice set ──────────────

    // Two slices on the same ADC in different bands: the per-ADC analysis
    // must see both bands and land on BYPASS.
    void two_slices_different_bands_on_one_adc_feed_bypass()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);   // 20 m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);    // 40 m

        // No codec is injected here, so no assignment is published and every
        // stream keeps m_streamAdc's ADC0 default: both slices share one
        // chain. The EXT1 cases below inject one and take the other branch.
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);

        QVERIFY(!mock->bpfCalls.isEmpty());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);   // bypass encoding

        delete mock;
    }

    // Same ADC, same band: the chain stays filtered at that band. This is the
    // case that must not regress into a needless bypass.
    void two_slices_same_band_on_one_adc_stay_filtered()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14250000.0);

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);
        QCOMPARE(model.alexController().adcState(0).currentBpfBand, Band::Band20m);

        QVERIFY(!mock->bpfCalls.isEmpty());
        // 14.2 MHz -> 13 MHz HPF (0x01) per AlexFilterMap::computeHpf.
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(14.2)));
        QVERIFY(mock->bpfCalls.last().hpfBitsAdc0 != 0x20);

        delete mock;
    }

    // Single slice: the pushed bits must be exactly what the frequency-derived
    // path produced before this change, for a spread of representative
    // frequencies across the whole HPF ladder.
    void single_slice_hpf_bits_match_frequency_derived_values()
    {
        struct Row { double hz; double mhz; };
        const QVector<Row> rows {
            {  1900000.0,  1.9 },   // 160 m -> 1.5 MHz HPF
            {  3700000.0,  3.7 },   //  80 m -> 1.5 MHz HPF
            {  7150000.0,  7.15},   //  40 m -> 6.5 MHz HPF
            { 10120000.0, 10.12},   //  30 m -> 9.5 MHz HPF
            { 14200000.0, 14.2 },   //  20 m -> 13 MHz HPF
            { 21200000.0, 21.2 },   //  15 m -> 20 MHz HPF
            { 28400000.0, 28.4 },   //  10 m -> 20 MHz HPF
            { 50150000.0, 50.15},   //   6 m -> 6 m preamp
        };

        for (const Row& r : rows) {
            RadioModel model;
            model.configureStreamPool(5, 5, 192000);
            auto* mock = new MockConnection();
            model.injectConnectionForTest(mock);

            const int a = model.addSlice();
            model.slices().at(a)->setFrequency(r.hz);

            QVERIFY2(!mock->bpfCalls.isEmpty(),
                     qPrintable(QStringLiteral("no push at %1 MHz").arg(r.mhz)));
            QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                     int(codec::alex::computeHpf(r.mhz)));

            model.injectConnectionForTest(nullptr);
            delete mock;
        }
    }

    // ── D1: the grouping key follows the wire, not a pinned ADC0 ─────────
    //
    // Defect D1 (ct1iqi-audit.md). Commit 99709649 made the WIRE route a
    // slice on an RX-only antenna to ADC1, and 7cc35f20 made that reach the
    // ANAN-G2. The filter analysis kept grouping by
    // ReceiverManager::receiverConfig(stream).adcIndex, which only
    // setAdcForReceiver writes and which was only ever called once with 0.
    // So the two disagreed: the radio really moved the DDC, and
    // AlexController still counted the slice on chain 0, saw two bands on one
    // chain, and bypassed BOTH chains.
    //
    // Every case below constructs P2CodecSaturn and sets the board to
    // HPSDRHW::Saturn, because Saturn IS the ANAN-G2 this is wrong on today.
    // D3 hid for two days behind tests that built P2CodecOrionMkII.
    void saturn_slice_on_ext1_gets_its_own_chain_decision()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(5, 5, 192000);
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7150000.0);     // 40 m, ANT1
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);    // 20 m
        // Select it first, as the operator does before touching its flag:
        // AlexController holds the antenna per BAND, and RadioModel syncs the
        // resulting label back onto the ACTIVE slice only
        // (AlexController::antennaChanged handler, T13). Setting it on a
        // background slice would push the label onto whichever slice happened
        // to be selected.
        model.setActiveSlice(b);
        model.slices().at(b)->setRxAntenna(QStringLiteral("EXT1"));

        // Chain 0 keeps 40 m to itself; chain 1 owns 20 m. Neither bypasses.
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);
        QCOMPARE(model.alexController().adcState(0).currentBpfBand, Band::Band40m);
        QCOMPARE(model.alexController().adcState(1).effective,
                 AlexController::BpfEffective::Filtered);
        QCOMPARE(model.alexController().adcState(1).currentBpfBand, Band::Band20m);

        QVERIFY(!mock->bpfCalls.isEmpty());
        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc0,
                 int(codec::alex::computeRxPreselector(7.15, HPSDRHW::Saturn)));
        QCOMPARE(last.hpfBitsAdc1,
                 int(codec::alex::computeRxPreselector(14.2, HPSDRHW::Saturn)));
        QVERIFY(last.hpfBitsAdc0 != 0x20);
        QVERIFY(last.hpfBitsAdc1 != 0x20);

        delete mock;
    }

    // Bench row 15's second half: putting the slice back on ANT1 must put
    // both slices back on one chain, which is two bands again, which is
    // BYPASS again. Idle input 2 follows Slice B's 14.2 MHz row.
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    //   [original inline comment from console.cs:15449]
    //                case HPSDRModel.ANAN_G2E: //N1GP G2E added
    // From Thetis console.cs:15435-15444 [v2.10.3.15]; row bits and edges
    // are in AlexFilterMap.cpp:253-257, 312-314 (11 to 22 MHz -> 0x01).
    void saturn_moving_back_to_ant1_recombines_the_two_slices()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(5, 5, 192000);
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7150000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        model.setActiveSlice(b);
        model.slices().at(b)->setRxAntenna(QStringLiteral("EXT1"));
        const int streamB = model.slices().at(b)->streamIndex();
        QCOMPARE(model.slices().at(b)->chainIndex(), 1);
        QCOMPARE(model.receiverManager()->receiverConfig(streamB).adcIndex, 1);
        QCOMPARE(model.slices().at(b)->ddcIndex(),
                 model.receiverManager()->receiverConfig(streamB).ddcIndex);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);

        model.slices().at(b)->setRxAntenna(QStringLiteral("ANT1"));

        QCOMPARE(model.slices().at(b)->chainIndex(), 0);
        QCOMPARE(model.receiverManager()->receiverConfig(streamB).adcIndex, 0);
        QCOMPARE(model.slices().at(b)->ddcIndex(),
                 model.receiverManager()->receiverConfig(streamB).ddcIndex);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);
        QVERIFY(!mock->bpfCalls.isEmpty());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, 0x01);

        delete mock;
    }

    // The CH tag and the WIDE pill read sliceChainIndex, so it has to move
    // with the wire too. A pan naming CH 0 for a slice the radio put on ADC1
    // is the same defect wearing a different hat.
    void saturn_slice_on_ext1_reports_chain_1_to_the_ui()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(5, 5, 192000);
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7150000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        model.setActiveSlice(b);
        model.slices().at(b)->setRxAntenna(QStringLiteral("EXT1"));

        QCOMPARE(model.sliceChainIndex(model.slices().at(a)->sliceIndex()), 0);
        QCOMPARE(model.sliceChainIndex(model.slices().at(b)->sliceIndex()), 1);

        delete mock;
    }

    // ── D4: a chain the board does not have never gets a word ────────────
    //
    // Angelia is the ANAN-100D: NetworkIO.SetRxADC(2) at
    // clsHardwareSpecific.cs:123 [v2.10.3.15], so adcCount == 2, yet it is
    // absent from the setAlex2HPF model list at console.cs:15435-15443
    // [v2.10.3.15], so rxFilterChainCount == 1. Two ADCs, one driven filter
    // chain. It dispatches to P2CodecOrionMkII (the `default:` case at
    // P2RadioConnection.cpp:2318), which DOES antenna-route, so the wire
    // really can put a DDC on ADC1 here -- and there is still only one
    // preselector in front of both ADCs, so that slice's band must be
    // counted on chain 0 and no chain-1 word may be composed.
    //
    // This is the case that would have shipped broken if D1 had landed
    // without the gate.
    void angelia_two_adcs_one_filter_chain_writes_no_chain1_word()
    {
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::Angelia).adcCount, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::Angelia).rxFilterChainCount, 1);

        RadioModel model;
        model.setBoardForTest(HPSDRHW::Angelia);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7150000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        model.setActiveSlice(b);
        model.slices().at(b)->setRxAntenna(QStringLiteral("EXT1"));

        // Both slices are behind the one chain, so the one chain sees two
        // bands and bypasses. Chain 1 is not a thing on this board.
        QCOMPARE(model.sliceChainIndex(model.slices().at(b)->sliceIndex()), 0);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);

        QVERIFY(!mock->bpfCalls.isEmpty());
        for (const AlexRxBpf& call : std::as_const(mock->bpfCalls)) {
            QCOMPARE(call.hpfBitsAdc1, -1);
        }

        delete mock;
    }

    // ── Diversity: both chains, identically ──────────────────────────────
    //
    // CT1IQI's requirement on PR #293: under diversity Alex0 and Alex1 must
    // be set identical, "and that may mean identically bypassed". The
    // DDC0/DDC1 sync pair samples ONE signal through TWO front ends, so a
    // mismatched preselector between them puts a different group delay on
    // each leg and the combiner has nothing to combine.
    void diversity_filters_both_chains_identically()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(5, 5, 192000);
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.setDdcContextForTest(/*mox*/false, /*ps*/false, /*diversity*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        QVERIFY(!mock->bpfCalls.isEmpty());
        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc0,
                 int(codec::alex::computeRxPreselector(14.2, HPSDRHW::Saturn)));
        QCOMPARE(last.hpfBitsAdc1, last.hpfBitsAdc0);
        QVERIFY(last.hpfBitsAdc1 >= 0);   // a real decision, not the mirror

        delete mock;
    }

    // The "identically bypassed" half. A second slice on another band, on
    // either chain, must take BOTH legs wide together rather than leaving
    // one filtered.
    void diversity_bypasses_both_chains_identically()
    {
        for (const QString ant : {QStringLiteral("ANT1"), QStringLiteral("EXT1")}) {
            RadioModel model;
            model.setBoardForTest(HPSDRHW::Saturn);
            model.configureStreamPool(5, 5, 192000);
            P2CodecSaturn codec;
            model.receiverManager()->setP2Codec(&codec);
            model.setDdcContextForTest(false, false, /*diversity*/true);
            auto* mock = new MockConnection();
            model.injectConnectionForTest(mock);

            const int a = model.addSlice();
            model.slices().at(a)->setFrequency(14200000.0);   // 20 m
            const int b = model.addSlice();
            model.slices().at(b)->setFrequency(7150000.0);    // 40 m
            model.setActiveSlice(b);
            model.slices().at(b)->setRxAntenna(ant);

            QVERIFY(!mock->bpfCalls.isEmpty());
            const AlexRxBpf last = mock->bpfCalls.last();
            QVERIFY2(last.hpfBitsAdc0 == 0x20,
                     qPrintable(QStringLiteral("chain 0 not bypassed with second slice on %1").arg(ant)));
            QVERIFY2(last.hpfBitsAdc1 == last.hpfBitsAdc0,
                     qPrintable(QStringLiteral("chains differ with second slice on %1").arg(ant)));

            model.injectConnectionForTest(nullptr);
            delete mock;
        }
    }

    // The per-ADC path must pick the ladder the BOARD has, not always the
    // legacy high-pass one.  Orion MkII / Saturn / HermesC10 carry a BAND-PASS
    // bank on the same relay bits, so the same frequency answers differently.
    // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
    // Upstream inline attribution preserved verbatim (console.cs:6830):
    //    || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    //
    // 21.2 MHz (15 m) is the sharpest discriminator: band-pass answers 0x01
    // (the 11.0-22.0 MHz row), high-pass answers 0x02 (the 20 MHz HPF row).
    void movable_b_owner_keeps_live_cohosts_coherent_and_excludes_away_unbound()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.configureStreamPool(5, 5, 192000);
        model.receiverManager()->setMaxReceivers(5);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        MockConnection mock;
        model.injectConnectionForTest(&mock);
        DetachConnection detach{&model};
        auto* own = model.sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        const auto add = [&](const QString& pan, double hz) {
            SliceOwnership::CreatorScope creator(own, station);
            const int id = model.addSlice(pan);
            if (auto* slice = model.sliceById(id)) { slice->setFrequency(hz); }
            return id;
        };
        const int a = add("pan-0", 14'200'000);
        const int b = add("pan-1", 7'150'000);
        const int c = add("pan-1", 7'155'000);
        QCOMPARE(a, 0); QCOMPARE(b, 1); QCOMPARE(c, 2);
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(c)->streamIndex());
        QVERIFY(model.sliceById(a)->streamIndex() != model.sliceById(b)->streamIndex());
        const auto i = [](const QByteArray& name, qint64 v) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant::fromValue(v)};
        };
        const auto result = model.invokeDiversityAsStationDevice(SessionMessages::commandInvoke(
            "diversity.setTarget", 900,
            {{0, "enabled", MirrorWireKind::Bool, true},
             i("stateRevision", model.diversityStateRevision()),
             i("sourceSliceId", -1), i("sourceIncarnation", 0), i("sourceControlRevision", 0),
             i("targetSliceId", b), i("targetIncarnation", own->incarnation(b)),
             i("targetControlRevision", own->controlRevision(b))}));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(model.diversityTargetSlice(), model.sliceById(b));
        QVERIFY(!mock.bpfCalls.isEmpty());
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc0, 0x20);
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc1, 0x20);

        own->setOwner(a, "away-owner");
        own->setAwayDevices({QByteArray("away-owner")});
        // The away 20 m receiver no longer widens either coherent leg.
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc0, mock.bpfCalls.last().hpfBitsAdc1);
        QVERIFY(mock.bpfCalls.last().hpfBitsAdc0 != 0x20);
        const int d = add("pan-2", 1'900'000);
        QVERIFY(d >= 0);
        model.sliceById(d)->setStreamIndex(-1);
        model.sliceById(b)->setFrequency(7'151'000); // ordinary production reconciliation
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc0, mock.bpfCalls.last().hpfBitsAdc1);
        QVERIFY(mock.bpfCalls.last().hpfBitsAdc0 != 0x20);
        QCOMPARE(model.diversityTargetSlice(), model.sliceById(b));
        QCOMPARE(model.sliceById(c)->frequency(), 7'155'000.0);
    }

    void per_adc_path_uses_board_appropriate_ladder_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("expectedBits");

        // Saturn-class -> MkII band-pass bank.
        QTest::newRow("OrionMKII")  << int(HPSDRHW::OrionMKII)  << 0x01;
        QTest::newRow("Saturn")     << int(HPSDRHW::Saturn)     << 0x01;
        QTest::newRow("SaturnMKII") << int(HPSDRHW::SaturnMKII) << 0x01;
        QTest::newRow("HermesC10")  << int(HPSDRHW::HermesC10)  << 0x01;

        // Legacy boards must stay bit-identical to pre-fix behaviour.
        QTest::newRow("Hermes")     << int(HPSDRHW::Hermes)     << 0x02;
        QTest::newRow("HermesII")   << int(HPSDRHW::HermesII)   << 0x02;
        QTest::newRow("Angelia")    << int(HPSDRHW::Angelia)    << 0x02;
        QTest::newRow("Orion")      << int(HPSDRHW::Orion)      << 0x02;
    }

    void per_adc_path_uses_board_appropriate_ladder()
    {
        QFETCH(int, board);
        QFETCH(int, expectedBits);

        RadioModel model;
        model.setBoardForTest(static_cast<HPSDRHW>(board));
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(21200000.0);   // 15 m

        QVERIFY(!mock->bpfCalls.isEmpty());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, expectedBits);

        // And it must agree with the shared helper for that board, so the two
        // cannot drift apart.
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeRxPreselector(
                     21.2, static_cast<HPSDRHW>(board))));

        delete mock;
    }

    // Every ham band, on one Saturn-class board, through the per-ADC path.
    // These four bands are the ones an operator actually hears the difference
    // on: 80 m, 60 m, 40 m and 15 m used to engage the neighbouring filter.
    void per_adc_saturn_matches_bpf1_across_bands()
    {
        struct Row { double hz; double mhz; };
        const QVector<Row> rows {
            {  1850000.0,  1.85 },   // 160 m
            {  3700000.0,  3.70 },   //  80 m, was wrong
            {  5350000.0,  5.35 },   //  60 m, was wrong
            {  7150000.0,  7.15 },   //  40 m, was wrong
            { 10100000.0, 10.10 },   //  30 m
            { 14200000.0, 14.20 },   //  20 m
            { 18100000.0, 18.10 },   //  17 m
            { 21200000.0, 21.20 },   //  15 m, was wrong
            { 24900000.0, 24.90 },   //  12 m
        };

        for (const Row& r : rows) {
            RadioModel model;
            model.setBoardForTest(HPSDRHW::Saturn);
            model.configureStreamPool(5, 5, 192000);
            auto* mock = new MockConnection();
            model.injectConnectionForTest(mock);

            const int a = model.addSlice();
            model.slices().at(a)->setFrequency(r.hz);

            QVERIFY2(!mock->bpfCalls.isEmpty(),
                     qPrintable(QStringLiteral("no push at %1 MHz").arg(r.mhz)));
            QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                     int(codec::alex::computeBpf1(r.mhz)));

            model.injectConnectionForTest(nullptr);
            delete mock;
        }
    }

    // Nothing on ADC1 means no decision for ADC1: the sentinel keeps the
    // existing Alex1 behaviour rather than forcing a filter onto an idle
    // chain. Mirrors Thetis, which only calls setAlex2HPF when RX2 exists
    // (console.cs:15435-15442 [v2.10.3.15]).
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    void idle_adc_reports_no_decision()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        QVERIFY(!mock->bpfCalls.isEmpty());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, -1);

        delete mock;
    }

    // ── Anvelina Pro 3: input 2 follows Slice B when nothing is on it ────
    //
    // Thetis sets the Alex-2 high-pass from VFO B on every change, RX2 on or
    // off (UpdateRX2DDSFreq, console.cs:15435-15444 [v2.10.3.15];
    // Thetis-Enhanced (EU2AV) console.cs:15462-15471 [@567a7ec]). Slice B is
    // RX2 / VFO B here. Slice A 7.1 MHz and Slice B 14.2 MHz both on input 1:
    // Alex0's receive half is both rows (0x0012), and Alex1's is Slice B's
    // 11 to 22 MHz row (0x0002), not a copy of Alex0's.
    void anvelina_input2_follows_slice_b_when_nothing_is_on_it()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7100000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(model.slices().at(b)->sliceIndex(), 1);   // Slice B
        QCOMPARE(model.sliceChainIndex(model.slices().at(b)->sliceIndex()), 0);

        QVERIFY(!mock->bpfCalls.isEmpty());
        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc1, 0x01);   // the 11 to 22 MHz row

        CodecContext ctx;
        ctx.model           = HPSDRModel::ANVELINAPRO3;
        ctx.alexHpfBits     = quint8(last.hpfBitsAdc0);
        ctx.alexHpfBitsAdc1 = last.hpfBitsAdc1;
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex0 & 0xFFFFu, 0x0012u);
        QCOMPARE(alex1 & 0xFFFFu, 0x0002u);

        // Keyed, Thetis keeps following VFO B (HdwMOXChanged calls
        // UpdateRX2DDSFreq; setAlex2HPF has no MOX arm), so the receive
        // half is the same word.
        ctx.mox = true;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex1 & 0xFFu, 0x02u);
        QCOMPARE(alex1 & (1u << 12), 0u);

        // Retuning Slice B moves the input-2 row with it.
        model.slices().at(b)->setFrequency(3700000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, 0x08);   // 2.1 to 5.5 MHz row

        delete mock;
    }

    // No Slice B: no decision for input 2, so Alex1 keeps today's mirror of
    // Alex0's selection.
    void anvelina_input2_mirrors_alex0_without_slice_b()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7100000.0);

        QVERIFY(!mock->bpfCalls.isEmpty());
        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc0, 0x04);
        QCOMPARE(last.hpfBitsAdc1, -1);

        CodecContext ctx;
        ctx.model           = HPSDRModel::ANVELINAPRO3;
        ctx.alexHpfBits     = quint8(last.hpfBitsAdc0);
        ctx.alexHpfBitsAdc1 = last.hpfBitsAdc1;
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex0 & 0xFFFFu, 0x0010u);
        QCOMPARE(alex1 & 0xFFFFu, 0x0010u);   // the mirror

        // Removing Slice B goes back to the mirror.
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, 0x01);
        model.removeSlice(model.slices().at(b)->sliceIndex());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, -1);

        delete mock;
    }

    // A slice on input 2 keeps its own rule: Alex1 follows the slices there,
    // not Slice B.
    void anvelina_slice_on_input2_keeps_its_rule()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7100000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        const int c = model.addSlice();
        model.slices().at(c)->setFrequency(3700000.0);
        model.setActiveSlice(c);
        model.slices().at(c)->setRxAntenna(QStringLiteral("EXT1"));
        QCOMPARE(model.sliceChainIndex(model.slices().at(c)->sliceIndex()), 1);

        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, 0x08);   // Slice C's row

        delete mock;
    }

    // ── Every Alex-2 board: input 2 follows Slice B when nothing is on it ──
    //
    // Thetis calls setAlex2HPF(rx2_dds_freq_mhz) from UpdateRX2DDSFreq for
    // these seven models, the same list in both trees:
    //   From Thetis console.cs:15435-15444 [v2.10.3.15] (UpdateRX2DDSFreq)
    //   From Thetis-Enhanced (EU2AV) console.cs:15462-15471 [@567a7ec]
    //     HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    //   Upstream inline attribution preserved verbatim (console.cs:15449):
    //                case HPSDRModel.ANAN_G2E: //N1GP G2E added
    // Slice A 7.1 MHz and Slice B 14.2 MHz, both on input 1: Alex1's receive
    // half is Slice B's 11 to 22 MHz row (0x0002), keyed too. Without Slice B
    // it is the mirror again (-1).
    void alex2_boards_input2_follows_slice_b_data()
    {
        QTest::addColumn<int>("model");
        QTest::newRow("ORIONMKII")    << int(HPSDRModel::ORIONMKII);
        QTest::newRow("ANAN7000D")    << int(HPSDRModel::ANAN7000D);
        QTest::newRow("ANAN8000D")    << int(HPSDRModel::ANAN8000D);
        QTest::newRow("ANAN_G2")      << int(HPSDRModel::ANAN_G2);
        QTest::newRow("ANAN_G2_1K")   << int(HPSDRModel::ANAN_G2_1K);
        QTest::newRow("ANVELINAPRO3") << int(HPSDRModel::ANVELINAPRO3);
        QTest::newRow("REDPITAYA")    << int(HPSDRModel::REDPITAYA);
    }
    void alex2_boards_input2_follows_slice_b()
    {
        QFETCH(int, model);
        const auto m = static_cast<HPSDRModel>(model);
        RadioModel radio;
        radio.setHpsdrModelForTest(m);
        radio.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        radio.receiverManager()->setP2Codec(&codec);
        MockConnection mock;
        radio.injectConnectionForTest(&mock);
        DetachConnection detach{&radio};

        const int a = radio.addSlice();
        radio.slices().at(a)->setFrequency(7100000.0);
        QVERIFY(!mock.bpfCalls.isEmpty());
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc1, -1);   // no Slice B: mirror

        const int b = radio.addSlice();
        radio.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(radio.slices().at(b)->sliceIndex(), 1);   // Slice B
        QCOMPARE(radio.sliceChainIndex(radio.slices().at(b)->sliceIndex()), 0);

        const AlexRxBpf last = mock.bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc1, 0x01);   // the 11 to 22 MHz row

        CodecContext ctx;
        ctx.model           = m;
        ctx.alexHpfBits     = quint8(last.hpfBitsAdc0);
        ctx.alexHpfBitsAdc1 = last.hpfBitsAdc1;
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex1 & 0xFFFFu, 0x0002u);

        ctx.mox = true;   // keyed: Thetis keeps following VFO B
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex1 & 0xFFu, 0x02u);
        QCOMPARE(alex1 & (1u << 12), 0u);

        radio.slices().at(b)->setFrequency(3700000.0);
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc1, 0x08);   // 2.1 to 5.5 MHz row

        radio.removeSlice(radio.slices().at(b)->sliceIndex());
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc1, -1);     // mirror again

    }

    // Boards Thetis does not call setAlex2HPF for from VFO B keep the
    // mirror, Slice B or not (console.cs:15435-15444 [v2.10.3.15]; EU2AV
    // console.cs:15462-15471 [@567a7ec]). The ANAN-200D is given two filter
    // chains here, so only the model check stands between it and Slice B's
    // row; the Hermes and the G2E run as they are.
    void boards_outside_the_alex2_list_keep_the_mirror_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("chains");   // 0 = the board's own topology
        QTest::newRow("ANAN200D two chains") << int(HPSDRModel::ANAN200D) << 2;
        QTest::newRow("HERMES")              << int(HPSDRModel::HERMES)   << 0;
        QTest::newRow("ANAN_G2E")            << int(HPSDRModel::ANAN_G2E) << 0;
    }
    void boards_outside_the_alex2_list_keep_the_mirror()
    {
        QFETCH(int, model);
        QFETCH(int, chains);
        RadioModel radio;
        radio.setHpsdrModelForTest(static_cast<HPSDRModel>(model));
        if (chains > 0) {
            radio.setWidebandTopologyForTest(2, 2, chains);
            QCOMPARE(radio.boardCapabilities().rxFilterChainCount, chains);
        }
        radio.configureStreamPool(5, 5, 192000);
        MockConnection mock;
        radio.injectConnectionForTest(&mock);
        DetachConnection detach{&radio};

        const int a = radio.addSlice();
        radio.slices().at(a)->setFrequency(7100000.0);
        const int b = radio.addSlice();
        radio.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(radio.slices().at(b)->sliceIndex(), 1);   // Slice B
        QCOMPARE(radio.sliceChainIndex(radio.slices().at(b)->sliceIndex()), 0);

        QVERIFY(!mock.bpfCalls.isEmpty());
        QCOMPARE(mock.bpfCalls.last().hpfBitsAdc1, -1);

    }

    // Removing the second slice collapses the chain back to a single band,
    // so the bypass must lift again.
    void removing_the_cross_band_slice_lifts_the_bypass()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);

        model.removeSlice(model.slices().at(b)->sliceIndex());

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(14.2)));

        delete mock;
    }

    // ── One band name, two filters ───────────────────────────────────────
    //
    // WWV on 5 MHz and on 15 MHz share a band name but not a filter (the
    // 2.1 to 5.5 MHz and 11 to 22 MHz rows), and so do two general-coverage
    // slices either side of a row edge. The chain needs two selections at
    // once, so it is a band difference like any other: it was taken for one
    // band, and the higher slice was left behind the lower slice's filter.
    void two_filters_under_one_band_name_are_a_band_difference_data()
    {
        QTest::addColumn<double>("lowHz");
        QTest::addColumn<double>("highHz");
        QTest::newRow("WWV 5 and 15 MHz")              << 5000000.0 << 15000000.0;
        QTest::newRow("general coverage 6.1 and 11.8") << 6100000.0 << 11800000.0;
    }
    void two_filters_under_one_band_name_are_a_band_difference()
    {
        QFETCH(double, lowHz);
        QFETCH(double, highHz);
        QCOMPARE(bandFromFrequency(lowHz), bandFromFrequency(highHz));
        QVERIFY(codec::alex::computeRxPreselector(lowHz / 1e6, HPSDRHW::Saturn)
                != codec::alex::computeRxPreselector(highHz / 1e6, HPSDRHW::Saturn));

        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(lowHz);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(highHz);

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);

        // Back inside one filter, the chain filters again.
        model.slices().at(b)->setFrequency(lowHz + 20000.0);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeRxPreselector(lowHz / 1e6, HPSDRHW::Saturn)));

        delete mock;
    }

    // Where the band filters combine (the Anvelina Pro 3, below), the same
    // two slices get both filters and the chain stays filtered.
    void two_filters_under_one_band_name_combine_on_the_anvelina()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(5000000.0);    // WWV, 2.1 to 5.5 MHz row
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(15000000.0);   // WWV, 11 to 22 MHz row

        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.reasonText, QStringLiteral("WWV"));
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x08 | 0x01);

        delete mock;
    }

    // ── Anvelina Pro 3: the band filters combine ─────────────────────────
    //
    // Measured on an Anvelina Pro 3 (J.J. Boyd, KG4VCF, 2026-10-11,
    // firmware 22, Protocol 2) from the radio's own I/Q, receivers on 40 m
    // and 20 m sharing input 1, dBFS rms for the two:
    //   Alex0 receive half 0x1000 (bypass)            -99.2 / -99.8
    //   0x0000 (nothing selected)                     -99.8 / -99.6
    //   0x0010 (5.5 to 11 MHz filter)                 -53.0 / -94.7
    //   0x0002 (11 to 22 MHz filter)                  -82.5 / -59.2
    //   0x0012 (both filters)                         -53.5 / -59.4
    // Bypass passes nothing on this radio, and both filters together give
    // each receiver the level it has behind its own filter alone
    // (codec::alex::combinesRxBandFilters).
    void anvelina_two_bands_on_one_input_select_both_filters()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7225600.0);    // 40 m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);   // 20 m

        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.reasonText, QStringLiteral("40m + 20m"));

        QVERIFY(!mock->bpfCalls.isEmpty());
        const int bits = mock->bpfCalls.last().hpfBitsAdc0;
        QCOMPARE(bits, 0x04 | 0x01);   // the 5.5 to 11 MHz and 11 to 22 MHz rows

        // On the wire: the word measured to pass both bands.
        CodecContext ctx;
        ctx.alexHpfBits = quint8(bits);
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex0 & 0xFFFFu, 0x0012u);

        delete mock;
    }

    // Any two of the bank's six filters, in either slice order. One pair is
    // not sent as it stands: the 6 m BPF/LNA with the 12/10 m row, where the
    // BPF/LNA stays alone (anvelina_10m_row_stays_out_while_the_lna_is_in).
    void anvelina_every_pair_of_filters_combines()
    {
        struct Row { double hz; int bits; };
        // One frequency per row of the band-pass bank (AlexHpfEdges::
        // thetisDefaults().bpf1, with the rows' selections in kRowBits).
        static constexpr Row kRows[] = {
            {1900000.0, 0x10}, {3800000.0, 0x08}, {7200000.0, 0x04},
            {14200000.0, 0x01}, {28400000.0, 0x02}, {50200000.0, 0x40},
        };

        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        const int b = model.addSlice();
        for (const Row& first : kRows) {
            for (const Row& second : kRows) {
                if (first.bits == second.bits) { continue; }
                model.slices().at(a)->setFrequency(first.hz);
                model.slices().at(b)->setFrequency(second.hz);
                const QByteArray what = QStringLiteral("%1 + %2 Hz")
                    .arg(first.hz, 0, 'f', 0).arg(second.hz, 0, 'f', 0).toUtf8();
                QVERIFY2(model.alexController().adcState(0).effective
                             == AlexController::BpfEffective::Filtered, what.constData());
                int expected = first.bits | second.bits;
                if (expected == (0x02 | 0x40)) { expected = 0x40; }
                QVERIFY2(mock->bpfCalls.last().hpfBitsAdc0 == expected, what.constData());
            }
        }

        delete mock;
    }

    // More than two: every slice's filter is selected, and bands behind one
    // filter count once.
    void anvelina_every_band_in_use_gets_its_filter()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const double hz[] = {1900000.0, 3800000.0, 7200000.0, 14200000.0, 21200000.0};
        int last = -1;
        for (double f : hz) {
            last = model.addSlice();
            model.slices().at(last)->setFrequency(f);
        }
        // 160, 80, 40 m and the 11 to 22 MHz filter once for 20 m and 15 m.
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x10 | 0x08 | 0x04 | 0x01);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);

        // The 15 m slice to 10 m: five slices, five filters.
        model.slices().at(last)->setFrequency(28400000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x10 | 0x08 | 0x04 | 0x01 | 0x02);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);

        delete mock;
    }

    // A slice below the lowest filter has none of its own, so its selection
    // is the bypass. That is left out while another slice has a filter: the
    // bypass passes nothing, and added to two filters it cost them 3 to 5 dB
    // (0x1012: -56.2 / -64.9 dBFS in the measurement above).
    void anvelina_bypass_is_left_out_of_a_combined_selection()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(900000.0);     // below the 1.5 MHz row
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x01);

        delete mock;
    }

    // ── Anvelina Pro 3: the 6 m BPF/LNA on a shared input ───────────────
    //
    // Measured on the same radio and day, all 64 combinations of the six
    // rows, twice (codec::alex::combineRxBandSelections). With the BPF/LNA
    // and the 12/10 m row both selected the board was unstable: the 6 m
    // receiver's level, steady at -80 dBFS with the BPF/LNA and any other
    // row, was -78.4 with the 12/10 m row added (0x000C) and -41.4 with the
    // 20 m row as well (0x000E). With the BPF/LNA in, the other rows pass
    // their bands 8 to 21 dB lower. The maintainer's ruling of 2026-10-11:
    // the BPF/LNA stays, every slice with a row sent hears, and the chain
    // says what that costs.

    // The rule itself, over every selection of the six rows and the bypass.
    void combined_selection_never_holds_the_lna_with_the_10m_row()
    {
        using codec::alex::combineRxBandSelections;
        static constexpr quint8 kRows   = 0x10 | 0x08 | 0x04 | 0x01 | 0x02 | 0x40;
        static constexpr quint8 kBypass = 0x20;
        for (int sel = 1; sel <= 0x7F; ++sel) {
            const quint8 in = quint8(sel);
            const quint8 rows = quint8(in & kRows);
            const QByteArray what = QByteArray("selection 0x") + QByteArray::number(sel, 16);
            const quint8 kept = combineRxBandSelections(in, /*lnaStays=*/true);
            if (rows == 0) {
                QVERIFY2(kept == kBypass, what.constData());   // the bypass alone
                continue;
            }
            const bool both = (rows & 0x40) != 0 && (rows & 0x02) != 0;
            QVERIFY2(kept == (both ? quint8(rows & ~0x02) : rows), what.constData());
            QVERIFY2((kept & kBypass) == 0, what.constData());
            // Where the BPF/LNA is taken out afterward (Disable 6m LNA on
            // RX), every row goes on to the switch, the 12/10 m row with it.
            QVERIFY2(combineRxBandSelections(in, /*lnaStays=*/false) == rows, what.constData());
        }
    }

    // 80 m, 40 m and 6 m on one input: all three rows go out, 6 m with its
    // BPF/LNA (Alex0's receive half 0x0038, a word from the sweep), and the
    // chain says the HF slices read low.
    void anvelina_six_meters_keeps_its_lna_beside_hf()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(3800000.0);    // 80 m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7200000.0);    // 40 m
        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        QCOMPARE(st.reasonText, QStringLiteral("80m + 40m"));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("BPF/LNA")),
                 qPrintable(st.lowPassReason));

        const int c = model.addSlice();
        model.slices().at(c)->setFrequency(50200000.0);   // 6 m
        const int bits = mock->bpfCalls.last().hpfBitsAdc0;
        QCOMPARE(bits, 0x08 | 0x04 | 0x40);
        CodecContext ctx;
        ctx.alexHpfBits = quint8(bits);
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex0 & 0xFFFFu, 0x0038u);

        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.reasonText, QStringLiteral("80m + 40m + 6m (6m LNA in: HF reads low)"));
        const auto letter = [&model](int id) { return model.slices().at(id)->sliceLetter(); };
        QVERIFY2(st.lowPassReason.contains(
                     QStringLiteral("The 6m BPF/LNA is in for slice %1 on 6m.").arg(letter(c))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(
                     QStringLiteral("The meters and panadapters of slices %1 on 80m and %2 on "
                                    "40m read about 10 to 20 dB lower with it in.")
                         .arg(letter(a)).arg(letter(b))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("12/10m BPF")),
                 qPrintable(st.lowPassReason));

        // The 6 m slice leaves for 20 m: the note and the sentences go.
        model.slices().at(c)->setFrequency(14200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x08 | 0x04 | 0x01);
        QCOMPARE(st.reasonText, QStringLiteral("80m + 40m + 20m"));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("BPF/LNA")),
                 qPrintable(st.lowPassReason));

        delete mock;
    }

    // 10 m and 6 m on one input: the 12/10 m row stays out while the BPF/LNA
    // is in, and the chain names the slice left without its filter.
    void anvelina_10m_row_stays_out_while_the_lna_is_in()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(28400000.0);   // 10 m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(50200000.0);   // 6 m
        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        const auto letter = [&model](int id) { return model.slices().at(id)->sliceLetter(); };

        int bits = mock->bpfCalls.last().hpfBitsAdc0;
        QCOMPARE(bits, 0x40);
        CodecContext ctx;
        ctx.alexHpfBits = quint8(bits);
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex0 & 0xFFFFu, 0x0008u);   // the BPF/LNA alone, never 0x000C

        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.reasonText, QStringLiteral("10m + 6m (6m LNA in: 10m filter out)"));
        QVERIFY2(st.lowPassReason.contains(
                     QStringLiteral("The 6m BPF/LNA is in for slice %1 on 6m.").arg(letter(b))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(
                     QStringLiteral("The 12/10m BPF stays out with it, so slice %1 on 10m has "
                                    "no filter and may hear little or nothing.").arg(letter(a))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("dB lower")),
                 qPrintable(st.lowPassReason));

        // A 40 m slice joins: its row goes out beside the BPF/LNA and reads low.
        const int c = model.addSlice();
        model.slices().at(c)->setFrequency(7200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x40);
        QCOMPARE(st.reasonText,
                 QStringLiteral("40m + 10m + 6m (6m LNA in: HF reads low, 10m filter out)"));
        QVERIFY2(st.lowPassReason.contains(
                     QStringLiteral("The meter and panadapter of slice %1 on 40m read about "
                                    "10 to 20 dB lower with it in.").arg(letter(c))),
                 qPrintable(st.lowPassReason));

        // 12 m sits in the same row as 10 m.
        model.slices().at(a)->setFrequency(24950000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x40);
        QCOMPARE(st.reasonText,
                 QStringLiteral("40m + 12m + 6m (6m LNA in: HF reads low, 12m filter out)"));

        // The 6 m slice goes to 20 m: the 12/10 m row is back.
        model.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x01 | 0x02);
        QCOMPARE(st.reasonText, QStringLiteral("40m + 20m + 12m"));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("BPF/LNA")),
                 qPrintable(st.lowPassReason));

        delete mock;
    }

    // Five slices on five bands, 6 m among them: the longest reason the
    // chain can give, the low-pass hold's sentences and the BPF/LNA's. A
    // remote window refuses a reason over 512 characters
    // (RadioModel::applyStationFilterValue), so it has to fit.
    void anvelina_longest_shared_input_reason_fits_a_remote_window()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        for (const double hz : {1900000.0, 3800000.0, 7200000.0, 28400000.0, 50200000.0}) {
            const int id = model.addSlice();
            QVERIFY(id >= 0);
            model.slices().at(id)->setFrequency(hz);
        }
        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x10 | 0x08 | 0x04 | 0x40);
        QCOMPARE(st.reasonText,
                 QStringLiteral("160m + 80m + 40m + 10m + 6m "
                                "(6m LNA in: HF reads low, 10m filter out)"));
        // All three of the BPF/LNA's sentences, after the low-pass hold's.
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("receive low-pass filter")),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("The 6m BPF/LNA is in for slice")),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("read about 10 to 20 dB lower")),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("The 12/10m BPF stays out")),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.size() <= 512,
                 qPrintable(QStringLiteral("%1 characters: %2")
                                .arg(st.lowPassReason.size()).arg(st.lowPassReason)));

        delete mock;
    }

    // The note and the sentences are about the combined selection, so they
    // go when the operator's override or extended view takes its place.
    void anvelina_lna_note_goes_with_an_override()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7200000.0);    // 40 m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(50200000.0);   // 6 m
        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        const QString note = QStringLiteral("40m + 6m (6m LNA in: HF reads low)");
        const QString lna = QStringLiteral("6m BPF/LNA");
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x40);
        QCOMPARE(st.reasonText, note);
        QVERIFY2(st.lowPassReason.contains(lna), qPrintable(st.lowPassReason));

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kAnvelinaReceiveBypass);
        QCOMPARE(st.reasonText, QStringLiteral("BYPASS (operator override)"));
        QVERIFY2(!st.lowPassReason.contains(lna), qPrintable(st.lowPassReason));

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04);
        QVERIFY2(!st.reasonText.contains(QStringLiteral("LNA")), qPrintable(st.reasonText));
        QVERIFY2(!st.lowPassReason.contains(lna), qPrintable(st.lowPassReason));

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::Auto);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x40);
        QCOMPARE(st.reasonText, note);
        QVERIFY2(st.lowPassReason.contains(lna), qPrintable(st.lowPassReason));

        model.alexControllerMutable().setWidebandActive(0, true);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kAnvelinaReceiveBypass);
        QVERIFY2(!st.lowPassReason.contains(lna), qPrintable(st.lowPassReason));
        model.alexControllerMutable().setWidebandActive(0, false);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x40);
        QCOMPARE(st.reasonText, note);
        QVERIFY2(st.lowPassReason.contains(lna), qPrintable(st.lowPassReason));

        delete mock;
    }

    // Input 2's board gets the same rule and says the same (the rows are
    // the same on both chains: codec::alex::computeAlex2Hpf).
    void anvelina_lna_rule_holds_on_the_second_input()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        model.setDdcContextForTest(/*mox*/false, /*ps*/false, /*diversity*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(28400000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(50200000.0);

        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc0, 0x40);
        QCOMPARE(last.hpfBitsAdc1, 0x40);
        const AlexController::AlexAdcState& st = model.alexController().adcState(1);
        QCOMPARE(st.reasonText, QStringLiteral("10m + 6m (6m LNA in: 10m filter out)"));
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("The 12/10m BPF stays out")),
                 qPrintable(st.lowPassReason));

        delete mock;
    }

    // The operator's own choices keep their meaning. Force filter is one
    // filter, the lowest slice's. Force bypass is still reported as the
    // bypass, and goes out as the five ordinary band filters together
    // (codec::alex::receiveBypassSelection): this radio's own bypass passes
    // nothing.
    void anvelina_operator_overrides_keep_their_meaning()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x01);

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kAnvelinaReceiveBypass);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04);

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::Auto);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x01);

        delete mock;
    }

    // ── Anvelina Pro 3: a receive bypass is the five band filters ────────
    //
    // Measured on the same radio and day, receivers on 20 m, 80 m and 40 m
    // sharing input 1, noise floor in dBFS for the three:
    //   Alex0 receive half 0x1000 (bypass)              -99.8 / -99.7 / -99.2
    //   0x0000 (nothing selected)                       -99.4 / -99.9 / -99.6
    //   each band's own filter alone                    -69.3 / -61.7 / -63.2
    //   0x0076 (the 160, 80, 40, 20 and 12/10 m rows)   -69.5 / -61.8 / -63.7
    //   0x0076 again, 20 s later                        -70.3 / -62.7 / -62.8
    // So wherever a bypass would go out on receive, the five rows go out in
    // its place (codec::alex::receiveBypassSelection).
    void anvelina_force_bypass_sends_the_five_band_filters()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7225600.0);
        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);

        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Bypass);
        const int bits = mock->bpfCalls.last().hpfBitsAdc0;
        QCOMPARE(bits, 0x10 | 0x08 | 0x04 | 0x01 | 0x02);
        QCOMPARE(bits, int(codec::alex::receiveBypassSelection(HPSDRModel::ANVELINAPRO3)));

        // On the wire: the word measured, with neither the bypass (bit 12)
        // nor the 6 m amplifier (bit 3) in it.
        CodecContext ctx;
        ctx.alexHpfBits = quint8(bits);
        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);
        QCOMPARE(alex0 & 0xFFFFu, 0x0076u);

        delete mock;
    }

    // Extended view (a pan wider than its receiver) locks the chain wide.
    // It also comes and goes for a moment when a slice takes a new stream,
    // which on this radio was a 188 ms dropout of every receiver on the
    // input (bench log, 2026-10-11 21:08:38).
    void anvelina_extended_view_sends_the_five_band_filters()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7225600.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x01);

        model.alexControllerMutable().setWidebandActive(0, true);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::WidebandLocked);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kAnvelinaReceiveBypass);

        model.alexControllerMutable().setWidebandActive(0, false);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x04 | 0x01);

        delete mock;
    }

    // Input 2's board takes the same word (the rows are the same on both
    // chains: codec::alex::computeAlex2Hpf).
    void anvelina_force_bypass_on_the_second_input_sends_the_five_band_filters()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        model.setDdcContextForTest(/*mox*/false, /*ps*/false, /*diversity*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, 0x04 | 0x01);

        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);
        model.alexControllerMutable().setBpfMode(1, AlexController::BpfMode::ForceBypass);
        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc0, kAnvelinaReceiveBypass);
        QCOMPARE(last.hpfBitsAdc1, kAnvelinaReceiveBypass);

        delete mock;
    }

    // Under diversity both front ends take the same combined selection.
    void anvelina_diversity_combines_both_chains_identically()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        model.configureStreamPool(5, 5, 192000);
        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        model.setDdcContextForTest(/*mox*/false, /*ps*/false, /*diversity*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(!mock->bpfCalls.isEmpty());
        const AlexRxBpf last = mock->bpfCalls.last();
        QCOMPARE(last.hpfBitsAdc0, 0x04 | 0x01);
        QCOMPARE(last.hpfBitsAdc1, last.hpfBitsAdc0);

        delete mock;
    }

    // Only the Anvelina Pro 3 has been measured. The other radios with the
    // same band-pass bank keep the bypass, which passes signal on the
    // ANAN-G2.
    void other_band_pass_radios_still_bypass_on_two_bands_data()
    {
        QTest::addColumn<int>("hpsdrModel");
        QTest::newRow("ANAN-7000DLE") << int(HPSDRModel::ANAN7000D);
        QTest::newRow("ANAN-8000DLE") << int(HPSDRModel::ANAN8000D);
        QTest::newRow("ANAN-G2")      << int(HPSDRModel::ANAN_G2);
        QTest::newRow("ANAN-G2-1K")   << int(HPSDRModel::ANAN_G2_1K);
        QTest::newRow("Orion MkII")   << int(HPSDRModel::ORIONMKII);
    }
    void other_band_pass_radios_still_bypass_on_two_bands()
    {
        QFETCH(int, hpsdrModel);
        RadioModel model;
        model.setHpsdrModelForTest(static_cast<HPSDRModel>(hpsdrModel));
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(7225600.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14200000.0);

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);

        // And so do Force bypass and extended view, on one band.
        model.slices().at(b)->setFrequency(7150000.0);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);
        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);
        model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::Auto);
        QVERIFY(mock->bpfCalls.last().hpfBitsAdc0 != 0x20);
        model.alexControllerMutable().setWidebandActive(0, true);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);
        QCOMPARE(int(codec::alex::receiveBypassSelection(static_cast<HPSDRModel>(hpsdrModel))),
                 0x20);
        model.alexControllerMutable().setWidebandActive(0, false);

        // 10 m and 6 m: the bypass as for any two bands, and nothing said
        // of the 6 m BPF/LNA, which is the Anvelina's rule alone.
        model.slices().at(a)->setFrequency(28400000.0);
        model.slices().at(b)->setFrequency(50200000.0);
        const AlexController::AlexAdcState& st = model.alexController().adcState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);
        QCOMPARE(st.reasonText, QStringLiteral("BYPASS (multi-band: 10m + 6m)"));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("BPF/LNA")),
                 qPrintable(st.lowPassReason));

        delete mock;
    }

    // ── Half 2: the decision reaches the composed wire bytes ─────────────

    // A bypass decision on ADC0 sets Alex0 bit 12 (_Bypass).
    // From Thetis ChannelMaster/netInterface.c:604-621 [v2.10.3.15]:
    //   prbpfilter->_Bypass = (bits & 0x20) != 0;
    void adc0_bypass_decision_sets_alex0_bypass_bit()
    {
        CodecContext ctx;
        ctx.alexHpfBits = 0x20;           // AlexController said BYPASS for ADC0
        ctx.alexLpfBits = 0x01;

        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);

        QVERIFY(alex0 & kAlex0BitBypass);
        // No band filter asserted alongside the bypass.
        QVERIFY(!(alex0 & kAlex0Bit13MHz));
        QVERIFY(!(alex0 & kAlex0Bit9_5MHz));
        QVERIFY(!(alex0 & kAlex0Bit6_5MHz));
    }

    // A filtered decision puts that band's HPF on the wire and leaves the
    // bypass bit clear.
    void adc0_filtered_decision_sets_the_band_filter_bit()
    {
        CodecContext ctx;
        ctx.alexHpfBits = codec::alex::computeHpf(14.2);   // 0x01 -> 13 MHz HPF
        ctx.alexLpfBits = 0x01;

        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);

        QVERIFY(alex0 & kAlex0Bit13MHz);
        QVERIFY(!(alex0 & kAlex0BitBypass));
    }

    // ADC1's decision drives Alex1 independently of ADC0's. This is the shape
    // the reporter asked for: two chains, two answers.
    // From Thetis console.cs:15435-15442 [v2.10.3.15], setAlex2HPF is fed from
    //[2.10.3.13]MW0LGE
    // rx2_dds_freq_mhz while Alex0 is fed from _rx1_dds_freq.
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    void adc1_decision_drives_alex1_independently()
    {
        CodecContext ctx;
        ctx.alexHpfBits     = 0x20;                            // ADC0 bypassed
        ctx.alexHpfBitsAdc1 = int(codec::alex::computeHpf(7.15));  // ADC1 on 40 m
        ctx.alexLpfBits     = 0x01;

        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);

        QVERIFY(alex0 & kAlex0BitBypass);          // ADC0 wide
        QVERIFY(alex1 & kAlex0Bit6_5MHz);          // ADC1 filtered at 6.5 MHz
        QVERIFY(!(alex1 & kAlex0BitBypass));
    }

    // A bypass decision for ADC1 must reach Alex1's own bypass bit.
    // From Thetis ChannelMaster/netInterface.c:634-651 [v2.10.3.15]:
    //   prbpfilter2->_Bypass = (bits & 0x20) != 0;
    void adc1_bypass_decision_sets_alex1_bypass_bit()
    {
        CodecContext ctx;
        ctx.alexHpfBits     = codec::alex::computeHpf(14.2);
        ctx.alexHpfBitsAdc1 = 0x20;
        ctx.alexLpfBits     = 0x01;

        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);

        QVERIFY(!(alex0 & kAlex0BitBypass));
        QVERIFY(alex1 & kAlex0BitBypass);
    }

    // With no ADC1 decision the Alex1 word keeps its pre-change encoding:
    // Alex0's HPF bits mirrored across with the bypass bit masked off.
    // Wire-locked against the Thetis G2E pcap (see P2CodecOrionMkII.cpp
    // buildAlex1); this test exists so the new per-ADC path cannot quietly
    // move it.
    void alex1_unchanged_when_adc1_has_no_decision()
    {
        CodecContext ctx;
        ctx.alexHpfBits     = codec::alex::computeHpf(14.2);   // 0x01
        ctx.alexHpfBitsAdc1 = -1;                              // no slices on ADC1
        ctx.alexLpfBits     = 0x01;

        quint32 alex0 = 0, alex1 = 0;
        composeAlexRegisters(ctx, &alex0, &alex1);

        QVERIFY(alex1 & kAlex0Bit13MHz);      // mirrored from Alex0
        QVERIFY(!(alex1 & kAlex0BitBypass));  // bypass never mirrors
    }

    // The RX BPF decision must not move a single LPF bit: the LPF is TX-only
    // and protects the PA (design doc §4, "LPF: TX-only, never bypassed").
    //
    // This case used to assert `lpfInAlex0 == lpfInAlex1` as well. That held
    // only because both words read one shared mask, which was itself the
    // RF-safety defect fixed in 46e5390d: a receive retune onto a low band
    // dragged the transmit low-pass down with it. Alex0 and Alex1 carry
    // SEPARATE low-pass fields and are not required to agree.
    //
    //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
    //     void SetAlexLPFBits(int bits, bool isTX, bool isMox)
    //     if (isMox || isTX)   -> prbpfilter2 (Alex1, bytes 1428-1431)
    //     if (isMox || !isTX)  -> prbpfilter  (Alex0, bytes 1432-1435)
    //
    // so Alex1 always carries the transmit-derived selection while Alex0
    // carries the receive-derived one until MOX, when the two converge. The
    // hardware reads them as alternatives, not as a redundant pair:
    //   From n1gp-Anvelina_PROIII Orion.v:2348 [@8e86a61]
    //     wire [15:0] Alex_upper =
    //         (FPGA_PTT && Alex_Tx_data_ok) ? Alex_Tx_data : Alex_data[47:32];
    // where Alex_Tx_data is bytes 1428-1429 (High_Priority_CC.v:257
    // [@8e86a61]) and Alex_data[47:32] is bytes 1432-1433
    // (High_Priority_CC.v:261 [@8e86a61]).
    //
    // The equality assertion is therefore replaced with the stronger claim
    // it was standing in for: each word's low-pass field is exactly the
    // encoding of ITS OWN input, for every band-pass decision. Feeding the
    // two inputs different values also makes a regression that re-merges
    // them fail here rather than pass silently.
    void lpf_bits_are_untouched_by_every_bpf_decision()
    {
        const quint8 lpfRx = codec::alex::computeLpf(14.2);   // 30/20 m LPF
        const quint8 lpfTx = codec::alex::computeLpf(3.7);    // 80 m LPF
        QVERIFY(lpfRx != lpfTx);

        const QVector<int> adc0Decisions {
            int(codec::alex::computeHpf(1.9)),
            int(codec::alex::computeHpf(7.15)),
            int(codec::alex::computeHpf(14.2)),
            0x20,                                            // bypass
        };
        const QVector<int> adc1Decisions { -1, 0x20, int(codec::alex::computeHpf(7.15)) };

        const quint32 expectedAlex0 = encodeLpf(lpfRx);
        const quint32 expectedAlex1 = encodeLpf(lpfTx);
        QVERIFY(expectedAlex0 != 0);   // the LPF really is asserted
        QVERIFY(expectedAlex1 != 0);

        for (int hpf0 : adc0Decisions) {
            for (int hpf1 : adc1Decisions) {
                CodecContext ctx;
                ctx.alexHpfBits     = quint8(hpf0);
                ctx.alexHpfBitsAdc1 = hpf1;
                ctx.alexLpfBits     = lpfRx;
                ctx.alexLpfBitsTx   = lpfTx;

                quint32 alex0 = 0, alex1 = 0;
                composeAlexRegisters(ctx, &alex0, &alex1);

                QCOMPARE(alex0 & kAlexLpfMask, expectedAlex0);
                QCOMPARE(alex1 & kAlexLpfMask, expectedAlex1);
            }
        }
    }
};

QTEST_MAIN(TestAlexPerAdcBpfWire)
#include "tst_alex_per_adc_bpf_wire.moc"
