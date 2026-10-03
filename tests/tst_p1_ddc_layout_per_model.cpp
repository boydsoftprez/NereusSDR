// no-port-check: test file. It exercises the Protocol 1 codecs (which are the
// port); the Thetis cites below record where each expected value comes from,
// they are not ported code.
//
// Every Protocol 1 radio gets Thetis's receiver layout for its own model
// (plan Task 11; Phase 3F design section 16.3.2). One row per model and per
// combination of PureSignal armed, diversity, MOX and RX2 live. The expected
// values are Thetis's, read from three places:
//
//   console.cs UpdateDDCs [v2.10.3.15]: P1_DDCConfig, DDCEnable, SyncEnable,
//     Rate[0..3], cntrl1/cntrl2, P1_diversity, P1_rxcount, nddc.
//       Orion case 8220-8303, REDPITAYA 8305-8385 (//DH1KLM),
//       Hermes case 8387-8458 (//N1GP G2E added), HermesII 8461-8531.
//   console.cs GetDDC, Protocol 1 half [v2.10.3.15] 8647-8781: which frame
//     slot each receiver reads and the PureSignal pair (psrx, pstx).
//   cmaster.cs CMLoadRouterAll, Protocol 1 half [v2.10.3.15] 566-760, with
//     networkproto1.c MetisReadThreadMainLoop 377-384: which slots the frame
//     hands to RX1, RX2 and PureSignal.
//
// Stream values are FRAME SLOT numbers: the index of the receiver inside the
// Protocol 1 EP6 frame (GetDDC's Protocol 1 numbering), not UpdateDDCs's
// Protocol 2-style DDC numbering. Orion class: RX1 slot 0, RX2 slot 2, pair
// slots 3 + 4. Hermes class: RX1 slot 0, RX2 slot 1, pair 2 + 3. HermesII:
// RX1 slot 0, RX2 slot 1, pair 0 + 1.
//
// The PureSignal pair in DdcAssignment must equal the one psDdcConfig emits
// (the one P1RadioConnection's EP6 paired emit actually uses), and the other
// fields the two share must agree too; every row checks that.
#include <QtTest/QtTest>
#include <array>

#include "core/codec/P1CodecAnvelinaPro3.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"

using namespace NereusSDR;

namespace {

constexpr int R1 = 96000;   // rx1_rate (slice A's rate)
constexpr int R2 = 48000;   // rx2_rate (slice B's rate)
constexpr int PS = 192000;  // ps_rate = cmaster.PSrate, cmaster.cs:425 [v2.10.3.15]

// P1 ADC control word used for the Orion-class rows: low byte = cntrl1 source,
// high byte = cntrl2 source. Thetis: cntrl1 = rx_adc_ctrl1 & 0xff, cntrl2 =
// rx_adc_ctrl2 & 0x3f; under PureSignal transmit cntrl1 = (rx_adc_ctrl1 &
// 0xf3) | 0x08 (console.cs:8262-8263 [v2.10.3.15]).
constexpr quint16 kAdc = 0x0215;
constexpr int C1   = 0x15;
constexpr int C1PS = (0x15 & 0xf3) | 0x08;  // 0x19
constexpr int C2   = 0x02;

struct Row {
    bool ps, div, mox, rx2;
    int cfg, en, sync, r0, r1, r2, r3, c1, c2;
    int s0, s1, fwd, rev, p1Div;
};

// HermesII (ANAN10E, ANAN100B). UpdateDDCs console.cs:8461-8531
// [v2.10.3.15]: P1_rxcount = 2; nddc = 2. GetDDC HermesII (2 adc)
// console.cs:8746-8779: rx1 = 0; rx2 = 1 in every case but 5 and 7 (PS
// transmit), where psrx = 0; pstx = 1. Under PS transmit NereusSDR suspends
// both user streams (the operator's ruling; Thetis's router also duplicates
// slots 0 and 1 onto RX1 and RX2 there, cmaster.cs:664-682 //MW0LGE_21d DUP
// on top panadaptor, a deliberate divergence). Diversity keeps RX2 on slot
// 1 (GetDDC cases 2, 3, 6: rx2 = 1).
const Row kHermesII[] = {
    // ps  div    mox    rx2    cfg en sync r0  r1  r2 r3 c1 c2  s0  s1 fwd rev div
    {false,false,false,false,  4, 1, 0, R1, 0,  0, 0, 0, 0,  0, -1, -1, -1, 0},
    {false,false,false,true,   4, 3, 0, R1, R2, 0, 0, 0, 0,  0,  1, -1, -1, 0},
    {false,false,true, false,  4, 1, 0, R1, 0,  0, 0, 0, 0,  0, -1, -1, -1, 0},
    {false,false,true, true,   4, 3, 0, R1, R2, 0, 0, 0, 0,  0,  1, -1, -1, 0},
    {false,true, false,false,  5, 1, 2, R1, R1, 0, 0, 0, 0,  0, -1, -1, -1, 1},
    {false,true, false,true,   5, 1, 2, R1, R1, 0, 0, 0, 0,  0,  1, -1, -1, 1},
    {false,true, true, false,  5, 1, 2, R1, R1, 0, 0, 0, 0,  0, -1, -1, -1, 1},
    {false,true, true, true,   5, 1, 2, R1, R1, 0, 0, 0, 0,  0,  1, -1, -1, 1},
    {true, false,false,false,  4, 1, 0, R1, 0,  0, 0, 0, 0,  0, -1, -1, -1, 0},
    {true, false,false,true,   4, 3, 0, R1, R2, 0, 0, 0, 0,  0,  1, -1, -1, 0},
    {true, false,true, false,  5, 1, 2, PS, PS, 0, 0, 4, 0, -1, -1,  0,  1, 0},
    {true, false,true, true,   5, 1, 2, PS, PS, 0, 0, 4, 0, -1, -1,  0,  1, 0},
    {true, true, false,false,  5, 1, 2, R1, R1, 0, 0, 0, 0,  0, -1, -1, -1, 1},
    {true, true, false,true,   5, 1, 2, R1, R1, 0, 0, 0, 0,  0,  1, -1, -1, 1},
    {true, true, true, false,  5, 1, 2, PS, PS, 0, 0, 4, 0, -1, -1,  0,  1, 1},
    {true, true, true, true,   5, 1, 2, PS, PS, 0, 0, 4, 0, -1, -1,  0,  1, 1},
};

// Hermes class (HERMES, ANAN10, ANAN100, ANAN_G2E //N1GP G2E added).
// UpdateDDCs console.cs:8387-8458 [v2.10.3.15]: P1_rxcount = 4; nddc = 4;
// PS transmit is P1_DDCConfig = 6. GetDDC Hermes / HermesC10 console.cs:
// 8704-8745: rx1 = 0; rx2 = 1 in every case, psrx = 2; pstx = 3 in 5 and 7.
const Row kHermes[] = {
    {false,false,false,false,  4, 1, 0, R1, 0,  0, 0, 0, 0,  0, -1, -1, -1, 0},
    {false,false,false,true,   4, 3, 0, R1, R2, 0, 0, 0, 0,  0,  1, -1, -1, 0},
    {false,false,true, false,  4, 1, 0, R1, 0,  0, 0, 0, 0,  0, -1, -1, -1, 0},
    {false,false,true, true,   4, 3, 0, R1, R2, 0, 0, 0, 0,  0,  1, -1, -1, 0},
    {false,true, false,false,  5, 1, 2, R1, R1, 0, 0, 0, 0,  0, -1, -1, -1, 1},
    {false,true, false,true,   5, 1, 2, R1, R1, 0, 0, 0, 0,  0,  1, -1, -1, 1},
    {false,true, true, false,  5, 1, 2, R1, R1, 0, 0, 0, 0,  0, -1, -1, -1, 1},
    {false,true, true, true,   5, 1, 2, R1, R1, 0, 0, 0, 0,  0,  1, -1, -1, 1},
    {true, false,false,false,  4, 1, 0, R1, 0,  0, 0, 0, 0,  0, -1, -1, -1, 0},
    {true, false,false,true,   4, 3, 0, R1, R2, 0, 0, 0, 0,  0,  1, -1, -1, 0},
    {true, false,true, false,  6, 1, 2, PS, PS, 0, 0, 4, 0,  0, -1,  2,  3, 0},
    {true, false,true, true,   6, 1, 2, PS, PS, 0, 0, 4, 0,  0,  1,  2,  3, 0},
    {true, true, false,false,  5, 1, 2, R1, R1, 0, 0, 0, 0,  0, -1, -1, -1, 1},
    {true, true, false,true,   5, 1, 2, R1, R1, 0, 0, 0, 0,  0,  1, -1, -1, 1},
    {true, true, true, false,  6, 1, 2, PS, PS, 0, 0, 4, 0,  0, -1,  2,  3, 1},
    {true, true, true, true,   6, 1, 2, PS, PS, 0, 0, 4, 0,  0,  1,  2,  3, 1},
};

// Orion class (ANAN100D, ANAN200D, ORIONMKII, ANAN7000D, ANAN8000D, ANAN_G2,
// ANAN_G2_1K, ANVELINAPRO3). UpdateDDCs console.cs:8220-8303 [v2.10.3.15]:
// P1_rxcount = 5; nddc = 5; `P1_DDCConfig = DDCEnable = DDC0;` (so 1) for
// diversity without MOX; `if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE
// p1 !`; the rx2 addendum adds DDC3 at rx2_rate in every state. GetDDC
// Angelia / Orion / OrionMKII console.cs:8651-8702: rx1 = 0 (or sync1 = 0),
// rx2 = 2, psrx = 3, pstx = 4.
const Row kOrion[] = {
    {false,false,false,false,  1,  4, 0, R1, 0,  R1, 0,  C1,   C2, 0, -1, -1, -1, 0},
    {false,false,false,true,   1, 12, 0, R1, 0,  R1, R2, C1,   C2, 0,  2, -1, -1, 0},
    {false,false,true, false,  1,  4, 0, R1, 0,  R1, 0,  C1,   C2, 0, -1, -1, -1, 0},
    {false,false,true, true,   1, 12, 0, R1, 0,  R1, R2, C1,   C2, 0,  2, -1, -1, 0},
    {false,true, false,false,  1,  1, 2, R1, R1, 0,  0,  C1,   C2, 0, -1, -1, -1, 1},
    {false,true, false,true,   1,  9, 2, R1, R1, 0,  R2, C1,   C2, 0,  2, -1, -1, 1},
    {false,true, true, false,  2,  1, 2, R1, R1, 0,  0,  C1,   C2, 0, -1, -1, -1, 1},
    {false,true, true, true,   2,  9, 2, R1, R1, 0,  R2, C1,   C2, 0,  2, -1, -1, 1},
    {true, false,false,false,  1,  4, 0, R1, 0,  R1, 0,  C1,   C2, 0, -1, -1, -1, 0},
    {true, false,false,true,   1, 12, 0, R1, 0,  R1, R2, C1,   C2, 0,  2, -1, -1, 0},
    {true, false,true, false,  3,  5, 2, PS, PS, R1, 0,  C1PS, C2, 0, -1,  3,  4, 0},
    {true, false,true, true,   3, 13, 2, PS, PS, R1, R2, C1PS, C2, 0,  2,  3,  4, 0},
    {true, true, false,false,  1,  1, 2, R1, R1, 0,  0,  C1,   C2, 0, -1, -1, -1, 1},
    {true, true, false,true,   1,  9, 2, R1, R1, 0,  R2, C1,   C2, 0,  2, -1, -1, 1},
    {true, true, true, false,  3,  5, 2, PS, PS, R1, 0,  C1PS, C2, 0, -1,  3,  4, 1},
    {true, true, true, true,   3, 13, 2, PS, PS, R1, R2, C1PS, C2, 0,  2,  3,  4, 1},
};

// REDPITAYA (//DH1KLM). UpdateDDCs console.cs:8305-8385 [v2.10.3.15]: the
// Orion layout plus the `// REDPITAYA PAVEL` lines: P1_DDCConfig = 2 for
// diversity without MOX, Rate[1] = rx1_rate in plain receive, Rate[2] =
// rx1_rate under diversity. GetDDC and the router treat it as OrionMKII.
const Row kRedPitaya[] = {
    {false,false,false,false,  1,  4, 0, R1, R1, R1, 0,  C1,   C2, 0, -1, -1, -1, 0},
    {false,false,false,true,   1, 12, 0, R1, R1, R1, R2, C1,   C2, 0,  2, -1, -1, 0},
    {false,false,true, false,  1,  4, 0, R1, R1, R1, 0,  C1,   C2, 0, -1, -1, -1, 0},
    {false,false,true, true,   1, 12, 0, R1, R1, R1, R2, C1,   C2, 0,  2, -1, -1, 0},
    {false,true, false,false,  2,  1, 2, R1, R1, R1, 0,  C1,   C2, 0, -1, -1, -1, 1},
    {false,true, false,true,   2,  9, 2, R1, R1, R1, R2, C1,   C2, 0,  2, -1, -1, 1},
    {false,true, true, false,  2,  1, 2, R1, R1, R1, 0,  C1,   C2, 0, -1, -1, -1, 1},
    {false,true, true, true,   2,  9, 2, R1, R1, R1, R2, C1,   C2, 0,  2, -1, -1, 1},
    {true, false,false,false,  1,  4, 0, R1, R1, R1, 0,  C1,   C2, 0, -1, -1, -1, 0},
    {true, false,false,true,   1, 12, 0, R1, R1, R1, R2, C1,   C2, 0,  2, -1, -1, 0},
    {true, false,true, false,  3,  5, 2, PS, PS, R1, 0,  C1PS, C2, 0, -1,  3,  4, 0},
    {true, false,true, true,   3, 13, 2, PS, PS, R1, R2, C1PS, C2, 0,  2,  3,  4, 0},
    {true, true, false,false,  2,  1, 2, R1, R1, R1, 0,  C1,   C2, 0, -1, -1, -1, 1},
    {true, true, false,true,   2,  9, 2, R1, R1, R1, R2, C1,   C2, 0,  2, -1, -1, 1},
    {true, true, true, false,  3,  5, 2, PS, PS, R1, 0,  C1PS, C2, 0, -1,  3,  4, 1},
    {true, true, true, true,   3, 13, 2, PS, PS, R1, R2, C1PS, C2, 0,  2,  3,  4, 1},
};

struct ModelClass {
    HPSDRModel model;
    const char* name;
    const Row* rows;
    int p1RxCount;
    int nDdc;
};

const ModelClass kModels[] = {
    {HPSDRModel::ANAN10E,      "ANAN10E",      kHermesII,  2, 2},
    {HPSDRModel::ANAN100B,     "ANAN100B",     kHermesII,  2, 2},
    {HPSDRModel::HERMES,       "HERMES",       kHermes,    4, 4},
    {HPSDRModel::ANAN10,       "ANAN10",       kHermes,    4, 4},
    {HPSDRModel::ANAN100,      "ANAN100",      kHermes,    4, 4},
    {HPSDRModel::ANAN_G2E,     "ANAN_G2E",     kHermes,    4, 4},
    {HPSDRModel::ANAN100D,     "ANAN100D",     kOrion,     5, 5},
    {HPSDRModel::ANAN200D,     "ANAN200D",     kOrion,     5, 5},
    {HPSDRModel::ORIONMKII,    "ORIONMKII",    kOrion,     5, 5},
    {HPSDRModel::ANAN7000D,    "ANAN7000D",    kOrion,     5, 5},
    {HPSDRModel::ANAN8000D,    "ANAN8000D",    kOrion,     5, 5},
    {HPSDRModel::ANAN_G2,      "ANAN_G2",      kOrion,     5, 5},
    {HPSDRModel::ANAN_G2_1K,   "ANAN_G2_1K",   kOrion,     5, 5},
    {HPSDRModel::ANVELINAPRO3, "ANVELINAPRO3", kOrion,     5, 5},
    {HPSDRModel::REDPITAYA,    "REDPITAYA",    kRedPitaya, 5, 5},
};

// The codec P1RadioConnection::selectCodec picks for the model.
std::unique_ptr<P1CodecStandard> codecFor(HPSDRModel model)
{
    switch (model) {
        case HPSDRModel::ANVELINAPRO3: return std::make_unique<P1CodecAnvelinaPro3>();
        case HPSDRModel::REDPITAYA:    return std::make_unique<P1CodecRedPitaya>();
        default:                       return std::make_unique<P1CodecStandard>();
    }
}

QString describe(int cfg, int en, int sync, int r0, int r1, int r2, int r3,
                 int c1, int c2, int s0, int s1, int fwd, int rev, int p1Div,
                 int rxCount, int nDdc)
{
    return QStringLiteral("cfg=%1 en=%2 sync=%3 rate=[%4,%5,%6,%7] c1=0x%8 c2=0x%9 "
                          "s0=%10 s1=%11 pair=%12/%13 div=%14 rxcount=%15 nddc=%16")
        .arg(cfg).arg(en).arg(sync).arg(r0).arg(r1).arg(r2).arg(r3)
        .arg(c1, 0, 16).arg(c2, 0, 16).arg(s0).arg(s1).arg(fwd).arg(rev)
        .arg(p1Div).arg(rxCount).arg(nDdc);
}

} // namespace

class TestP1DdcLayoutPerModel : public QObject {
    Q_OBJECT
private slots:
    void layout_matches_thetis_data();
    void layout_matches_thetis();
    void slices_c_and_d_use_the_pair_slots_only_in_plain_receive_data();
    void slices_c_and_d_use_the_pair_slots_only_in_plain_receive();
    void pair_slots_carry_the_tx_frequency_while_ps_transmits_data();
    void pair_slots_carry_the_tx_frequency_while_ps_transmits();
};

void TestP1DdcLayoutPerModel::layout_matches_thetis_data()
{
    QTest::addColumn<int>("model");
    QTest::addColumn<int>("rowIndex");
    for (const ModelClass& m : kModels) {
        for (int i = 0; i < 16; ++i) {
            const Row& r = m.rows[i];
            QTest::addRow("%s ps%d div%d mox%d rx2%d", m.name,
                          int(r.ps), int(r.div), int(r.mox), int(r.rx2))
                << int(m.model) << i;
        }
    }
}

void TestP1DdcLayoutPerModel::layout_matches_thetis()
{
    QFETCH(int, model);
    QFETCH(int, rowIndex);
    const HPSDRModel hm = static_cast<HPSDRModel>(model);
    const ModelClass* mc = nullptr;
    for (const ModelClass& m : kModels) {
        if (m.model == hm) { mc = &m; }
    }
    QVERIFY(mc != nullptr);
    const Row& r = mc->rows[rowIndex];

    CodecContext ctx{};
    ctx.model         = hm;
    ctx.puresignalRun = r.ps;
    ctx.diversity     = r.div;
    ctx.mox           = r.mox;
    ctx.p1AdcCntrl    = kAdc;

    std::array<SliceConfig, 5> streams{};
    streams[0].live = true;
    streams[0].sampleRateHz = R1;
    streams[1].live = r.rx2;
    streams[1].sampleRateHz = R2;

    const auto codec = codecFor(hm);
    const DdcAssignment a = codec->applyDdcAssignment(ctx, streams);

    const QString expected = describe(r.cfg, r.en, r.sync, r.r0, r.r1, r.r2, r.r3,
                                      r.c1, r.c2, r.s0, r.s1, r.fwd, r.rev, r.p1Div,
                                      mc->p1RxCount, mc->nDdc);
    const QString actual = describe(a.p1DdcConfig, a.ddcEnable, a.syncEnable,
                                    a.rate[0], a.rate[1], a.rate[2], a.rate[3],
                                    a.adcCtrl1, a.adcCtrl2,
                                    a.streamDdc[0], a.streamDdc[1],
                                    a.psFwdDdc, a.psRevDdc, a.p1Diversity,
                                    a.p1RxCount, a.nDdc);
    QCOMPARE(actual, expected);

    // psDdcConfig is what P1RadioConnection latches for the wire and for the
    // EP6 paired PureSignal emit. Both halves of the codec must agree.
    const PsDdcConfig cfg = codec->applyPureSignalDdcConfig(
        hm, r.ps, r.div, r.mox, R1, R2, r.rx2, quint8(C1), quint8(C2));
    const QString fromPs = describe(cfg.p1DdcConfig, cfg.ddcEnable, cfg.syncEnable,
                                    int(cfg.rate[0]), int(cfg.rate[1]),
                                    int(cfg.rate[2]), int(cfg.rate[3]),
                                    cfg.cntrl1, cfg.cntrl2,
                                    a.streamDdc[0], a.streamDdc[1],
                                    cfg.psFbDdc, cfg.txMonDdc, a.p1Diversity,
                                    cfg.p1RxCount, cfg.nDdc);
    QCOMPARE(fromPs, actual);
}

// Slices C and D (streams 2 and 3) take the PureSignal pair's slots in
// plain receive only (the operator's ruling of 2026-09-24: four streams on
// Protocol 1): slots 3 + 4 on the Orion class, 2 + 3 on the Hermes class. They
// suspend while PureSignal transmits (the pair) and under diversity. Stream 4
// never gets a slot on Protocol 1, and HermesII has no room for either. Before
// this, Orion slices D and E were routed by position onto slots 3 and 4.
void TestP1DdcLayoutPerModel::slices_c_and_d_use_the_pair_slots_only_in_plain_receive_data()
{
    QTest::addColumn<int>("model");
    QTest::addColumn<int>("slotC");
    QTest::addColumn<int>("slotD");
    QTest::newRow("ANAN7000D")    << int(HPSDRModel::ANAN7000D)    << 3 << 4;
    QTest::newRow("ANAN100D")     << int(HPSDRModel::ANAN100D)     << 3 << 4;
    QTest::newRow("ANAN_G2")      << int(HPSDRModel::ANAN_G2)      << 3 << 4;
    QTest::newRow("ANVELINAPRO3") << int(HPSDRModel::ANVELINAPRO3) << 3 << 4;
    QTest::newRow("REDPITAYA")    << int(HPSDRModel::REDPITAYA)    << 3 << 4;
    QTest::newRow("HERMES")       << int(HPSDRModel::HERMES)       << 2 << 3;
    QTest::newRow("ANAN_G2E")     << int(HPSDRModel::ANAN_G2E)     << 2 << 3;
    QTest::newRow("ANAN10E")      << int(HPSDRModel::ANAN10E)      << -1 << -1;
}

void TestP1DdcLayoutPerModel::slices_c_and_d_use_the_pair_slots_only_in_plain_receive()
{
    QFETCH(int, model);
    QFETCH(int, slotC);
    QFETCH(int, slotD);
    const auto codec = codecFor(static_cast<HPSDRModel>(model));

    struct State { bool ps; bool div; bool mox; bool plain; };
    const State states[] = {
        {false, false, false, true}, {false, false, true, true},
        {true,  false, false, true}, {true,  false, true, false},
        {false, true,  false, false}, {true, true, true, false},
    };
    for (const State& st : states) {
        CodecContext ctx{};
        ctx.model = static_cast<HPSDRModel>(model);
        ctx.puresignalRun = st.ps;
        ctx.diversity = st.div;
        ctx.mox = st.mox;
        std::array<SliceConfig, 5> streams{};
        for (int i = 0; i < 5; ++i) {
            streams[i].live = true;
            streams[i].sampleRateHz = R1;
        }
        const DdcAssignment a = codec->applyDdcAssignment(ctx, streams);
        const QString where = QStringLiteral("ps %1 div %2 mox %3")
                                  .arg(st.ps).arg(st.div).arg(st.mox);
        QVERIFY2(a.streamDdc[2] == (st.plain ? slotC : -1), qPrintable(where));
        QVERIFY2(a.streamDdc[3] == (st.plain ? slotD : -1), qPrintable(where));
        QVERIFY2(a.streamDdc[4] == -1, qPrintable(where));
    }
}

// The PureSignal pair's slots carry the TX frequency while PureSignal
// transmits (Thetis networkproto1.c:525-551 [v2.10.3.15]: banks 5-7 send
// prn->tx[0].frequency for the pair), and follow slices C and D otherwise
// (the operator's ruling of 2026-09-24: four streams on the Hermes and Orion
// classes, a deliberate divergence from Thetis in plain receive). Pair slots:
// 3 + 4 (banks 6, 7) on nddc 5; 2 + 3 (banks 5, 6) on nddc 4.
void TestP1DdcLayoutPerModel::pair_slots_carry_the_tx_frequency_while_ps_transmits_data()
{
    QTest::addColumn<int>("model");
    QTest::addColumn<int>("nddc");
    QTest::addColumn<int>("firstPairBank");
    QTest::newRow("ANAN7000D")    << int(HPSDRModel::ANAN7000D)    << 5 << 6;
    QTest::newRow("ANAN100D")     << int(HPSDRModel::ANAN100D)     << 5 << 6;
    QTest::newRow("ANVELINAPRO3") << int(HPSDRModel::ANVELINAPRO3) << 5 << 6;
    QTest::newRow("REDPITAYA")    << int(HPSDRModel::REDPITAYA)    << 5 << 6;
    QTest::newRow("HERMES")       << int(HPSDRModel::HERMES)       << 4 << 5;
    QTest::newRow("ANAN_G2E")     << int(HPSDRModel::ANAN_G2E)     << 4 << 5;
}

void TestP1DdcLayoutPerModel::pair_slots_carry_the_tx_frequency_while_ps_transmits()
{
    QFETCH(int, model);
    QFETCH(int, nddc);
    QFETCH(int, firstPairBank);
    const auto codec = codecFor(static_cast<HPSDRModel>(model));

    struct State { bool mox; bool ps; };
    for (const State st : {State{false, false}, State{true, false},
                           State{false, true}, State{true, true}}) {
        CodecContext ctx{};
        ctx.model           = static_cast<HPSDRModel>(model);
        ctx.mox             = st.mox;
        ctx.p1PuresignalRun = st.ps;
        ctx.activeRxCount   = nddc;
        ctx.p1PsNDdc        = nddc;
        ctx.txFreqHz        = 14200000;
        for (int i = 0; i < 7; ++i) {
            ctx.rxFreqHz[i] = 7000000 + i * 10000;
        }
        const bool psTransmit = st.mox && st.ps;
        for (int bank : {firstPairBank, firstPairBank + 1}) {
            quint8 out[5] = {};
            codec->composeCcForBank(bank, ctx, out);
            const quint32 hz = (quint32(out[1]) << 24) | (quint32(out[2]) << 16)
                             | (quint32(out[3]) << 8) | quint32(out[4]);
            const quint32 want = psTransmit ? 14200000u
                                            : quint32(ctx.rxFreqHz[bank - 3]);
            QVERIFY2(hz == want, qPrintable(QStringLiteral("bank %1 mox %2 ps %3: %4, want %5")
                     .arg(bank).arg(st.mox).arg(st.ps).arg(hz).arg(want)));
        }
    }
}

QTEST_APPLESS_MAIN(TestP1DdcLayoutPerModel)
#include "tst_p1_ddc_layout_per_model.moc"
