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
    // BYPASS again. The chain-1 word goes back to "no decision".
    void saturn_moving_back_to_ant1_recombines_the_two_slices()
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
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);

        model.slices().at(b)->setRxAntenna(QStringLiteral("ANT1"));

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);
        QVERIFY(!mock->bpfCalls.isEmpty());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, 0x20);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc1, -1);

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
