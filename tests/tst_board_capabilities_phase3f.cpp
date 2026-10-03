// =================================================================
// tests/tst_board_capabilities_phase3f.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 1: verify BoardCapabilities gains
// maxSlices and widebandAdcs fields with correct per-SKU values per
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
// =================================================================

#include <QtTest/QtTest>
#include "core/BoardCapabilities.h"
#include "core/DdcAssignment.h"
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecAnvelinaPro3.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecHermes.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/codec/P2CodecSaturn.h"

#include <QSet>

#include <algorithm>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

// Streams the codec will ACTUALLY assign, measured rather than asserted:
// hand it five live plain-RX streams and count the ones that come back with
// a real DDC. A stream the allocator can claim but the codec leaves at
// streamDdc[st] == -1 gets no enable bit, so its slice is silently dead --
// no I/Q, no rejection, nothing on screen.
//
// Plain RX (no MOX, no PureSignal, no diversity) is the right context: it is
// the steady state userDdcCount describes, and it is the widest the codec
// ever goes. The PS/diversity branches on the 1-ADC families collapse harder
// still (Thetis console.cs:8448-8457 [v2.10.3.15]).
template <typename Codec>
int assignableStreams(const Codec& codec, HPSDRModel model = HPSDRModel::FIRST)
{
    CodecContext ctx{};
    ctx.model = model;
    std::array<SliceConfig, 5> streams{};
    for (int i = 0; i < 5; ++i) {
        streams[i].live         = true;
        streams[i].sampleRateHz = 192000;
        streams[i].frequencyHz  = 14200000 + qint64(i) * 1000000;
        streams[i].antennaIndex = 1;
    }

    const DdcAssignment a = codec.applyDdcAssignment(ctx, streams);

    int n = 0;
    for (int st = 0; st < 5; ++st) {
        if (a.streamDdc[st] >= 0) { ++n; }
    }
    return n;
}

} // namespace

class TestBoardCapabilitiesPhase3F : public QObject {
    Q_OBJECT
private slots:
    void struct_has_max_slices_field()
    {
        BoardCapabilities caps{};
        caps.maxSlices = 5;
        QCOMPARE(caps.maxSlices, 5);
    }

    void struct_has_wideband_adcs_field()
    {
        BoardCapabilities caps{};
        caps.widebandAdcs = 2;
        QCOMPARE(caps.widebandAdcs, 2);
    }

    // Phase 3F Task 2: per-SKU maxSlices population tests.
    // Values from docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.

    void hl2_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMESLITE);
        QCOMPARE(caps.maxSlices, 5);
    }

    void metis_max_slices_is_3()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HPSDR);
        QCOMPARE(caps.maxSlices, 3);
    }

    void hermes_max_slices_is_4()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMES);
        QCOMPARE(caps.maxSlices, 4);
    }

    void hermesII_max_slices_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN10E);
        QCOMPARE(caps.maxSlices, 2);
    }

    void angelia_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN100D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void orion_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN200D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void orionMkII_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ORIONMKII);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_g2_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_g2e_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2E);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anvelinapro3_max_slices_is_5()
    {
        // AnvelinaPro3 maps to kOrionMKII (P2, dual-ADC, 7 DDCs).
        // Design doc §2 table: AnvelinaPro3 row = 5 slices (same as OrionMkII).
        const auto caps = capabilitiesFor(HPSDRModel::ANVELINAPRO3);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_7000d_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN7000D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_8000d_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN8000D);
        QCOMPARE(caps.maxSlices, 5);
    }

    // Phase 3F Task 3: per-SKU widebandAdcs population tests.
    // P1-only boards: widebandAdcs = 0 (and plan Task 5's widebandAdcsFor
    // gives 0 on Protocol 1 for every row).
    // P2 boards: widebandAdcs = adcCount, since an ADC that is not on the board
    // cannot carry a wideband stream. 2 for the dual-ADC SKUs, 1 for the
    // single-ADC ANAN-G2E (clsHardwareSpecific.cs:130 [v2.10.3.15] SetRxADC(1)).

    void hl2_wideband_adcs_is_0()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMESLITE);
        QCOMPARE(caps.widebandAdcs, 0);  // P1 mechanism, deferred to 3F-W
    }

    void hermes_wideband_adcs_is_0()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMES);
        QCOMPARE(caps.widebandAdcs, 0);  // P1 mechanism
    }

    void anan_g2_wideband_adcs_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2);
        QCOMPARE(caps.widebandAdcs, 2);  // 2-ADC P2 board
    }

    // The G2E is the one P2 SKU with a single ADC, so it is the one row where
    // widebandAdcs is not 0 or 2. It read 2 until 2026-07-25, inherited from a
    // design doc §2 table row that mis-filed the G2E among the 2-ADC boards.
    void anan_g2e_wideband_adcs_is_1()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2E);
        QCOMPARE(caps.widebandAdcs, 1);
        // The bound that makes it 1, asserted alongside so the two cannot drift.
        QCOMPARE(caps.adcCount, 1);
    }

    // Guards the rule rather than the row: no P2 board may claim more wideband
    // ADCs than it has ADCs. This is what would have caught the G2E defect.
    void wideband_adcs_never_exceeds_adc_count()
    {
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY2(caps.widebandAdcs <= caps.adcCount,
                     qPrintable(QStringLiteral("%1: widebandAdcs %2 exceeds adcCount %3")
                                    .arg(QString::fromLatin1(caps.displayName))
                                    .arg(caps.widebandAdcs)
                                    .arg(caps.adcCount)));
        }
    }

    void anan_7000d_wideband_adcs_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN7000D);
        QCOMPARE(caps.widebandAdcs, 2);
    }

    // ── rxFilterChainCount (defect D4) ───────────────────────────────────
    //
    // Design doc §16.1.2. A chain is one independently addressable RX
    // preselector bank plus the ADC behind it, and it is the unit the two
    // Alex words address. Guards the rule, not the rows.

    // A chain includes the ADC behind it, so a board cannot drive more
    // chains than it has ADCs (§16.1.1).
    void rx_filter_chain_count_never_exceeds_adc_count()
    {
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY2(caps.rxFilterChainCount >= 1,
                     qPrintable(QStringLiteral("%1: rxFilterChainCount must be at least 1")
                                    .arg(QString::fromLatin1(caps.displayName))));
            QVERIFY2(caps.rxFilterChainCount <= caps.adcCount,
                     qPrintable(QStringLiteral("%1: rxFilterChainCount %2 exceeds adcCount %3")
                                    .arg(QString::fromLatin1(caps.displayName))
                                    .arg(caps.rxFilterChainCount)
                                    .arg(caps.adcCount)));
        }
    }

    // The upstream rule, expressed as a set rather than per row.
    //
    // From Thetis console.cs:15435-15443 [v2.10.3.15] UpdateRX2DDSFreq:
    //[2.10.3.13]MW0LGE
    //   ORIONMKII, ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, ANVELINAPRO3,
    //   REDPITAYA -> setAlex2HPF(rx2_dds_freq_mhz)
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    // Resolving those seven through clsHardwareSpecific.cs:86-190
    // [v2.10.3.15] gives exactly HPSDRHW.OrionMKII and HPSDRHW.Saturn.
    //
    // SaturnMKII and Andromeda are NereusSDR rows upstream cannot answer for
    // (no HPSDRModel resolves to HPSDRHW.SaturnMKII, and Thetis has no
    // Andromeda HW value at all). Both are Saturn-derived rows for
    // Saturn-class hardware and both dispatch to a codec that antenna-routes
    // to ADC1, so declaring one chain on them would recreate D1 there.
    void rx_filter_chain_count_matches_the_upstream_model_list()
    {
        const QSet<HPSDRHW> twoChain {
            HPSDRHW::OrionMKII,    // ORIONMKII / 7000D / 8000D / ANVELINAPRO3 / REDPITAYA
            HPSDRHW::Saturn,       // ANAN_G2 / ANAN_G2_1K
            HPSDRHW::SaturnMKII,   // NereusSDR: Saturn-derived
            HPSDRHW::Andromeda,    // NereusSDR: Saturn-derived
        };

        for (const auto& caps : BoardCapsTable::all()) {
            const int expected = twoChain.contains(caps.board) ? 2 : 1;
            QCOMPARE(caps.rxFilterChainCount, expected);
        }
    }

    // Why the field exists rather than reusing adcCount. ANAN-100D (Angelia)
    // and ANAN-200D (Orion) are NetworkIO.SetRxADC(2)
    // (clsHardwareSpecific.cs:123 and :140 [v2.10.3.15]) yet are absent from
    // the setAlex2HPF list, so their Alex1 HPF nibble is never written. Two
    // ADCs, one driven filter chain.
    void adc_count_does_not_predict_filter_chain_count()
    {
        for (HPSDRHW hw : {HPSDRHW::Angelia, HPSDRHW::Orion}) {
            const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
            QCOMPARE(caps.adcCount, 2);
            QCOMPARE(caps.rxFilterChainCount, 1);
        }
    }

    // And why not hasAlex2 either. Ours tracks Thetis's SetMKIIBPF, a
    // different concept: HermesC10 (ANAN-G2E) is SetMKIIBPF(1) but
    // SetRxADC(1) (clsHardwareSpecific.cs:128-133 [v2.10.3.15]) and is not
    // in the setAlex2HPF list.
    void has_alex2_does_not_predict_filter_chain_count()
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(HPSDRHW::HermesC10);
        QVERIFY(caps.hasAlex2);
        QCOMPARE(caps.rxFilterChainCount, 1);
    }

    // Phase 3F Sub-Epic I Task 1: per-SKU userDdcCount population tests.
    // Values from docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
    // §2 "Resolved values per SKU" (User DDCs column). User DDCs are the DDCs
    // available for operator slices after PS/diversity reservations.

    void user_ddc_count_matches_design_doc_table()
    {
        QCOMPARE(capabilitiesFor(HPSDRModel::ANAN_G2).userDdcCount, 5);     // DDC2-6
        QCOMPARE(capabilitiesFor(HPSDRModel::HERMESLITE).userDdcCount, 2); // DDC0 + DDC1
        QCOMPARE(capabilitiesFor(HPSDRModel::ANAN10E).userDdcCount, 2);    // HermesII: DDC0-1
        QCOMPARE(capabilitiesFor(HPSDRModel::HERMES).userDdcCount, 4);     // DDC0-3
        QCOMPARE(capabilitiesFor(HPSDRModel::HPSDR).userDdcCount, 3);      // Metis: DDC0-2
    }

    // ── Phase 3F Sub-Epic I closeout, defect F2 ─────────────────────────
    //
    // userDdcCount sizes the stream pool verbatim
    // (SliceStreamAllocator::configure via RadioModel::configureStreamPool),
    // so a value above what the board's codec will assign creates streams
    // the allocator hands out and the codec then leaves at
    // streamDdc[st] == -1. No enable bit, no I/Q, no rejection: the slice is
    // silently dead and publishDdcAssignment skips setDdcMapping for it,
    // leaving stale routing behind.
    //
    // user_ddc_count_never_exceeds_max_slices below masked all of this: 5
    // <= 5 held on every over-counted board.
    //
    // Boards are named one by one rather than walked from
    // BoardCapsTable::all(), because a caps row carries no board enum and
    // the codec is selected from HPSDRHW (P2RadioConnection.cpp:1971-2008)
    // or HPSDRModel (P1RadioConnection.cpp:1979-1984), not from the row.
    // Naming them keeps this list visibly parallel to those two switches.

    void user_ddc_count_never_exceeds_native_codec_capacity()
    {
        // P2-native boards, codec picked by HPSDRHW exactly as
        // P2RadioConnection::selectCodec does.
        expectFits(HPSDRHW::Saturn,     assignableStreams(P2CodecSaturn{}));
        expectFits(HPSDRHW::SaturnMKII, assignableStreams(P2CodecSaturn{}));
        expectFits(HPSDRHW::OrionMKII,  assignableStreams(P2CodecOrionMkII{}));
        expectFits(HPSDRHW::Andromeda,  assignableStreams(P2CodecOrionMkII{}));
        // ANAN-G2E. Was 5 against a 4-stream codec until defect F2.
        expectFits(HPSDRHW::HermesC10,  assignableStreams(P2CodecHermes{}));

        // P1-native boards, codec picked by HPSDRModel exactly as
        // P1RadioConnection::selectCodec does.
        expectFits(HPSDRHW::Atlas,      assignableStreams(P1CodecStandard{}));
        expectFits(HPSDRHW::Hermes,     assignableStreams(P1CodecStandard{}));
        expectFits(HPSDRHW::HermesII,   assignableStreams(P1CodecStandard{}));
        expectFits(HPSDRHW::HermesLite, assignableStreams(P1CodecHl2{}));
        // HermesLiteRxOnly has no HPSDRModel of its own, so
        // HardwareProfile's model walk falls through to HERMES and P1
        // selectCodec lands on P1CodecStandard. Asserting the codec that
        // actually runs, not the one the name suggests.
        expectFits(HPSDRHW::HermesLiteRxOnly,
                   assignableStreams(P1CodecStandard{}));
    }

    // Plan Task 11: the Protocol 1 stream count is one function of the row
    // and the protocol (BoardCapsTable::userDdcCountFor), and it matches what
    // each Protocol 1 codec assigns for its model: HermesII 2 (slots 0, 1),
    // Hermes class 4 (slots 0-3), Orion class 4 (slots 0, 2, 3, 4; slot 1 is
    // tied to RX1's frequency), HL2 2. This closes the gap the old
    // orion_class_p1_capacity_gap_is_known recorded as an expected failure:
    // the Orion-class rows keep their Protocol 2 count of five, and Protocol
    // 1 reads four. The Atlas stays a documented under-exposure (3 against
    // the Hermes layout's 4).
    void protocol1_stream_count_matches_the_codec_for_the_model_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("expected");
        QTest::newRow("ANAN10E")      << int(HPSDRModel::ANAN10E)      << 2;
        QTest::newRow("ANAN100B")     << int(HPSDRModel::ANAN100B)     << 2;
        QTest::newRow("HERMES")       << int(HPSDRModel::HERMES)       << 4;
        QTest::newRow("ANAN10")       << int(HPSDRModel::ANAN10)       << 4;
        QTest::newRow("ANAN100")      << int(HPSDRModel::ANAN100)      << 4;
        QTest::newRow("ANAN_G2E")     << int(HPSDRModel::ANAN_G2E)     << 4;
        QTest::newRow("ANAN100D")     << int(HPSDRModel::ANAN100D)     << 4;
        QTest::newRow("ANAN200D")     << int(HPSDRModel::ANAN200D)     << 4;
        QTest::newRow("ORIONMKII")    << int(HPSDRModel::ORIONMKII)    << 4;
        QTest::newRow("ANAN7000D")    << int(HPSDRModel::ANAN7000D)    << 4;
        QTest::newRow("ANAN8000D")    << int(HPSDRModel::ANAN8000D)    << 4;
        QTest::newRow("ANAN_G2")      << int(HPSDRModel::ANAN_G2)      << 4;
        QTest::newRow("ANAN_G2_1K")   << int(HPSDRModel::ANAN_G2_1K)   << 4;
        QTest::newRow("ANVELINAPRO3") << int(HPSDRModel::ANVELINAPRO3) << 4;
        QTest::newRow("REDPITAYA")    << int(HPSDRModel::REDPITAYA)    << 4;
        QTest::newRow("HERMESLITE")   << int(HPSDRModel::HERMESLITE)   << 2;
    }

    void protocol1_stream_count_matches_the_codec_for_the_model()
    {
        QFETCH(int, model);
        QFETCH(int, expected);
        const HPSDRModel m = static_cast<HPSDRModel>(model);
        const BoardCapabilities& caps = BoardCapsTable::forModel(m);

        QCOMPARE(BoardCapsTable::userDdcCountFor(caps, ProtocolVersion::Protocol1), expected);

        int assigned = 0;
        switch (m) {
            case HPSDRModel::HERMESLITE:   assigned = assignableStreams(P1CodecHl2{}, m); break;
            case HPSDRModel::ANVELINAPRO3: assigned = assignableStreams(P1CodecAnvelinaPro3{}, m); break;
            case HPSDRModel::REDPITAYA:    assigned = assignableStreams(P1CodecRedPitaya{}, m); break;
            default:                       assigned = assignableStreams(P1CodecStandard{}, m); break;
        }
        QCOMPARE(assigned, expected);
    }

    // Protocol 2 reads the row unchanged, on every row.
    void protocol2_stream_count_is_the_row()
    {
        for (const auto& caps : BoardCapsTable::all()) {
            QCOMPARE(BoardCapsTable::userDdcCountFor(caps, ProtocolVersion::Protocol2),
                     caps.userDdcCount);
            QVERIFY(BoardCapsTable::userDdcCountFor(caps, ProtocolVersion::Protocol1)
                    <= caps.userDdcCount);
        }
    }

    // ── Plan Tasks 5 and 15: offered sample rates per board and protocol ─
    //
    // Every model, and every row no model reaches, on both protocols. The
    // expected lists are Thetis's (setup.cs:847-850 [v2.10.3.15]): Protocol 1
    // 48/96/192 kHz, plus 384 kHz for the RedPitaya
    //   bool include_extra_p1_rate = HardwareSpecific.Model == HPSDRModel.REDPITAYA; //DH1KLM
    // and for the HL2 (mi0bot-Thetis setup.cs:849-851 [v2.10.3.13-beta2],
    // "The HL supports 384K"); Protocol 2 48 to 1536 kHz for every model,
    // the HL2 included (mi0bot's p2_rates, setup.cs:854 [v2.10.3.13-beta2],
    // has no HL2 case). No row is excepted (Task 15, the operator's ruling
    // of 2026-09-25: the Atlas, Hermes, HermesII and HL2 rows follow Thetis
    // on Protocol 2 too).
    void offered_sample_rates_per_board_and_protocol_data()
    {
        const QList<int> p1 {48000, 96000, 192000};
        const QList<int> p1Extra {48000, 96000, 192000, 384000};
        const QList<int> p2 {48000, 96000, 192000, 384000, 768000, 1536000};

        QTest::addColumn<int>("board");
        QTest::addColumn<int>("model");
        QTest::addColumn<QList<int>>("protocol1");
        QTest::addColumn<QList<int>>("protocol2");

        auto row = [](const char* name, HPSDRModel m, const QList<int>& a,
                      const QList<int>& b) {
            QTest::newRow(name) << int(boardForModel(m)) << int(m) << a << b;
        };
        row("HPSDR",        HPSDRModel::HPSDR,        p1, p2);
        row("HERMES",       HPSDRModel::HERMES,       p1, p2);
        row("ANAN10",       HPSDRModel::ANAN10,       p1, p2);
        row("ANAN100",      HPSDRModel::ANAN100,      p1, p2);
        row("ANAN10E",      HPSDRModel::ANAN10E,      p1, p2);
        row("ANAN100B",     HPSDRModel::ANAN100B,     p1, p2);
        row("ANAN100D",     HPSDRModel::ANAN100D,     p1, p2);
        row("ANAN200D",     HPSDRModel::ANAN200D,     p1, p2);
        row("ORIONMKII",    HPSDRModel::ORIONMKII,    p1, p2);
        row("ANAN7000D",    HPSDRModel::ANAN7000D,    p1, p2);
        row("ANAN8000D",    HPSDRModel::ANAN8000D,    p1, p2);
        row("ANAN_G2",      HPSDRModel::ANAN_G2,      p1, p2);
        row("ANAN_G2_1K",   HPSDRModel::ANAN_G2_1K,   p1, p2);
        row("ANVELINAPRO3", HPSDRModel::ANVELINAPRO3, p1, p2);
        row("REDPITAYA",    HPSDRModel::REDPITAYA,    p1Extra, p2);
        row("ANAN_G2E",     HPSDRModel::ANAN_G2E,     p1, p2);
        row("HERMESLITE",   HPSDRModel::HERMESLITE,   p1Extra, p2);

        // Rows no model resolves to, with the model defaultModelForBoard
        // gives them on connect. The HL2 receive-only kit is an HL2
        // (mi0bot has one HL2 model, HERMESLITE), so it gets the HL2's
        // 384 kHz on Protocol 1.
        QTest::newRow("HermesLiteRxOnly")
            << int(HPSDRHW::HermesLiteRxOnly) << int(HPSDRModel::HERMESLITE) << p1Extra << p2;
        QTest::newRow("SaturnMKII")
            << int(HPSDRHW::SaturnMKII) << int(HPSDRModel::ANAN_G2) << p1 << p2;
        QTest::newRow("Andromeda")
            << int(HPSDRHW::Andromeda) << int(HPSDRModel::HERMES) << p1 << p2;
    }

    // Every row, with the model a connect resolves it to, offers Thetis's
    // list for each protocol (mi0bot's for the HL2): no row excepted.
    void every_row_offers_the_protocol_list()
    {
        const std::vector<int> p1 {48000, 96000, 192000};
        const std::vector<int> p1Extra {48000, 96000, 192000, 384000};
        const std::vector<int> p2 {48000, 96000, 192000, 384000, 768000, 1536000};
        for (const auto& caps : BoardCapsTable::all()) {
            if (caps.board == HPSDRHW::Unknown) { continue; }
            const HPSDRModel m = defaultModelForBoard(caps.board);
            const bool extra = (m == HPSDRModel::HERMESLITE || m == HPSDRModel::REDPITAYA);
            QVERIFY2(BoardCapsTable::sampleRatesFor(caps, ProtocolVersion::Protocol1, m)
                         == (extra ? p1Extra : p1),
                     caps.displayName);
            QVERIFY2(BoardCapsTable::sampleRatesFor(caps, ProtocolVersion::Protocol2, m) == p2,
                     caps.displayName);
        }
    }

    void offered_sample_rates_per_board_and_protocol()
    {
        QFETCH(int, board);
        QFETCH(int, model);
        QFETCH(QList<int>, protocol1);
        QFETCH(QList<int>, protocol2);
        const BoardCapabilities& caps = BoardCapsTable::forBoard(static_cast<HPSDRHW>(board));
        const HPSDRModel m = static_cast<HPSDRModel>(model);

        auto asList = [](const std::vector<int>& v) { return QList<int>(v.begin(), v.end()); };
        QCOMPARE(asList(BoardCapsTable::sampleRatesFor(caps, ProtocolVersion::Protocol1, m)),
                 protocol1);
        QCOMPARE(asList(BoardCapsTable::sampleRatesFor(caps, ProtocolVersion::Protocol2, m)),
                 protocol2);
        QCOMPARE(BoardCapsTable::maxSampleRateFor(caps, ProtocolVersion::Protocol1, m),
                 protocol1.last());
        QCOMPARE(BoardCapsTable::maxSampleRateFor(caps, ProtocolVersion::Protocol2, m),
                 protocol2.last());
        // The row's own top spans every protocol it serves.
        QCOMPARE(caps.maxSampleRate, std::max(protocol1.last(), protocol2.last()));
    }

    // Every row in the table appears above, so a row added later has to be
    // given its rates here.
    void offered_sample_rates_cover_every_row()
    {
        QSet<HPSDRHW> covered;
        for (int i = int(HPSDRModel::FIRST) + 1; i < int(HPSDRModel::LAST); ++i) {
            covered.insert(boardForModel(static_cast<HPSDRModel>(i)));
        }
        covered.insert(HPSDRHW::HermesLiteRxOnly);
        covered.insert(HPSDRHW::SaturnMKII);
        covered.insert(HPSDRHW::Andromeda);
        for (const auto& caps : BoardCapsTable::all()) {
            if (caps.board == HPSDRHW::Unknown) { continue; }
            QVERIFY2(covered.contains(caps.board),
                     qPrintable(QStringLiteral("%1 has no expected rates")
                                    .arg(QString::fromLatin1(caps.displayName))));
        }
    }

    // ── Plan Task 5: wideband per board and protocol ────────────────────
    //
    // The ANAN-100D and ANAN-200D run either protocol. Protocol 1: none
    // (Thetis networkproto1.c:181-201 [v2.10.3.15] takes EP6 only).
    // Protocol 2: ADC0, as Thetis's wideband menu enables for every model
    // (console.cs:43552-43558 [v2.10.3.15] NetworkIO.SetWBEnable(0, 1)).
    void angelia_and_orion_wideband_follows_the_protocol()
    {
        for (HPSDRModel m : {HPSDRModel::ANAN100D, HPSDRModel::ANAN200D}) {
            const BoardCapabilities& caps = BoardCapsTable::forModel(m);
            QCOMPARE(BoardCapsTable::widebandAdcsFor(caps, ProtocolVersion::Protocol1), 0);
            QCOMPARE(BoardCapsTable::widebandAdcsFor(caps, ProtocolVersion::Protocol2), 1);
        }
    }

    // Protocol 2 reads the row unchanged; no other row's wideband moved.
    void protocol2_wideband_is_the_row()
    {
        for (const auto& caps : BoardCapsTable::all()) {
            QCOMPARE(BoardCapsTable::widebandAdcsFor(caps, ProtocolVersion::Protocol2),
                     caps.widebandAdcs);
        }
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::OrionMKII).widebandAdcs, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::Saturn).widebandAdcs, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::SaturnMKII).widebandAdcs, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::Andromeda).widebandAdcs, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesC10).widebandAdcs, 1);
    }

    void user_ddc_count_never_exceeds_max_slices()
    {
        // A SKU may host more slices than DDCs (slices share a DDC), but
        // never more DDCs than slices, since a stream with no slice is idle.
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY2(caps.userDdcCount <= caps.maxSlices,
                     qPrintable(QStringLiteral("userDdcCount must not exceed maxSlices: ")
                                + caps.displayName));
        }
    }

    // HL2 is the second row where maxSlices exceeds userDdcCount, after the
    // ANAN-G2E. Two DDC windows (mi0bot console.cs:8425-8429
    // [v2.10.3.13-beta2]) with up to five flags sharing them: a slice inside
    // an active window costs no DDC and no wire bandwidth.
    void hermeslite_flags_exceed_panadapters()
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QCOMPARE(caps.userDdcCount, 2);
        QCOMPARE(caps.maxSlices, 5);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly).userDdcCount, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly).maxSlices, 5);
    }

    // The guard has to fail on an undocumented gap, not just pass on the
    // documented ones. HermesLite is deliberately absent from
    // documentedUnderExposure(): its row matches its codec exactly, so if
    // anyone lowers it again this is what fires.
    void under_exposure_needs_a_written_reason()
    {
        QVERIFY(underExposureReason(HPSDRHW::HermesLite) == nullptr);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesLite).userDdcCount,
                 assignableStreams(P1CodecHl2{}));

        for (const UnderExposed& row : documentedUnderExposure()) {
            QVERIFY2(qstrlen(row.reason) > 0,
                     "every documentedUnderExposure entry needs a reason");
        }
    }

private:
    // Boards whose userDdcCount is deliberately BELOW what their codec will
    // assign. Every entry needs a reason, because an unexplained gap is how
    // the HL2 shipped at 1 against a 2-stream codec from 2026-05-26 to
    // 2026-07-31.
    //
    // All three entries here are the same shape: the board has fewer hardware
    // DDCs than the shared P1CodecStandard, which implements only ramdor's
    // HERMES-class arm (nddc = 4, console.cs:8387-8459 [v2.10.3.15]).
    struct UnderExposed {
        HPSDRHW     hw;
        const char* reason;
    };

    static const std::vector<UnderExposed>& documentedUnderExposure()
    {
        static const std::vector<UnderExposed> kRows = {
            {HPSDRHW::Atlas,
             "Metis has 3 DDCs (design doc section 2); P1CodecStandard is "
             "shared with the 4-DDC Hermes class"},
            {HPSDRHW::HermesII,
             "ANAN-10E/100B have 2 DDCs (Thetis console.cs:8461-8464 "
             "[v2.10.3.15], nddc = 2); P1CodecStandard is shared with the "
             "4-DDC Hermes class"},
            {HPSDRHW::HermesLiteRxOnly,
             "HL2 has 2 user DDCs (mi0bot console.cs:8425-8429 "
             "[v2.10.3.13-beta2]); this SKU has no HPSDRModel of its own so "
             "P1 selectCodec lands on the 4-stream P1CodecStandard"},
        };
        return kRows;
    }

    static const char* underExposureReason(HPSDRHW hw)
    {
        for (const UnderExposed& row : documentedUnderExposure()) {
            if (row.hw == hw) { return row.reason; }
        }
        return nullptr;
    }

    static void expectFits(HPSDRHW hw, int codecCapacity)
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
        const QString name = QString::fromLatin1(caps.displayName);

        QVERIFY2(caps.userDdcCount <= codecCapacity,
                 qPrintable(QStringLiteral("%1: userDdcCount %2 exceeds the %3 "
                                           "streams its codec will assign")
                                .arg(name)
                                .arg(caps.userDdcCount)
                                .arg(codecCapacity)));

        // A pool smaller than the codec can drive is not a defect, but a pool
        // of zero on a board that has DDCs means no slice can ever bind.
        QVERIFY2(caps.userDdcCount >= 1,
                 qPrintable(QStringLiteral("%1: userDdcCount must be at least 1")
                                .arg(name)));

        // The other side. Exposing fewer DDCs than the codec will assign
        // silently costs the operator a panadapter, and nothing else in the
        // build notices. Allowed only with a written reason.
        if (caps.userDdcCount < codecCapacity) {
            const char* why = underExposureReason(hw);
            QVERIFY2(why != nullptr,
                     qPrintable(QStringLiteral(
                         "%1: userDdcCount %2 is below the %3 streams its "
                         "codec will assign, with no entry in "
                         "documentedUnderExposure(). Either raise the row or "
                         "add an entry saying why the hardware cannot use "
                         "them.")
                             .arg(name)
                             .arg(caps.userDdcCount)
                             .arg(codecCapacity)));
            QVERIFY2(qstrlen(why) > 0,
                     qPrintable(QStringLiteral("%1: under-exposure reason is empty")
                                    .arg(name)));
        }
    }

    static BoardCapabilities capabilitiesFor(HPSDRModel m)
    {
        // forModel returns a const reference; copy it for comparison.
        return BoardCapsTable::forModel(m);
    }
};

QTEST_MAIN(TestBoardCapabilitiesPhase3F)
#include "tst_board_capabilities_phase3f.moc"
