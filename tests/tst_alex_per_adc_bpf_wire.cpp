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
#include "core/RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "core/codec/AlexFilterMap.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P2CodecOrionMkII.h"
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

        // Every stream sits on ADC0 today (setAdcForReceiver is called once,
        // receiver 0 -> ADC 0), so both slices share one chain.
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
    void lpf_bits_are_untouched_by_every_bpf_decision()
    {
        const quint8 lpf = codec::alex::computeLpf(14.2);   // 30/20 m LPF
        const QVector<int> adc0Decisions {
            int(codec::alex::computeHpf(1.9)),
            int(codec::alex::computeHpf(7.15)),
            int(codec::alex::computeHpf(14.2)),
            0x20,                                            // bypass
        };
        const QVector<int> adc1Decisions { -1, 0x20, int(codec::alex::computeHpf(7.15)) };

        quint32 reference = 0;
        bool haveReference = false;

        for (int hpf0 : adc0Decisions) {
            for (int hpf1 : adc1Decisions) {
                CodecContext ctx;
                ctx.alexHpfBits     = quint8(hpf0);
                ctx.alexHpfBitsAdc1 = hpf1;
                ctx.alexLpfBits     = lpf;

                quint32 alex0 = 0, alex1 = 0;
                composeAlexRegisters(ctx, &alex0, &alex1);

                const quint32 lpfInAlex0 = alex0 & kAlexLpfMask;
                const quint32 lpfInAlex1 = alex1 & kAlexLpfMask;
                QCOMPARE(lpfInAlex0, lpfInAlex1);
                if (!haveReference) {
                    reference = lpfInAlex0;
                    haveReference = true;
                    QVERIFY(reference != 0);   // the LPF really is asserted
                } else {
                    QCOMPARE(lpfInAlex0, reference);
                }
            }
        }
    }
};

QTEST_MAIN(TestAlexPerAdcBpfWire)
#include "tst_alex_per_adc_bpf_wire.moc"
