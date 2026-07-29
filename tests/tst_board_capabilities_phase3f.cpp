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
#include "core/HpsdrModel.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecAnvelinaPro3.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecHermes.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/codec/P2CodecSaturn.h"

#include <memory>

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
int assignableStreams(const Codec& codec)
{
    CodecContext ctx{};
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

    void hl2_max_slices_is_1()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMESLITE);
        QCOMPARE(caps.maxSlices, 1);
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
    // P1 boards: widebandAdcs = 0 (P1 mechanism deferred to 3F-W).
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
        QCOMPARE(capabilitiesFor(HPSDRModel::HERMESLITE).userDdcCount, 1); // DDC0 only
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

    // Angelia (ANAN-100D) and Orion (ANAN-200D) are P1-native 2-ADC boards
    // that Thetis drives through its ORION-class branch: nddc = 5,
    // P1_rxcount = 5, RX1 on DDC2 and RX2 on DDC3
    // (console.cs:8220-8304 [v2.10.3.15]). userDdcCount = 5 is right for the
    // hardware.
    //
    // P1CodecStandard, however, implements ONLY Thetis's HERMES-class branch
    // (console.cs:8387-8459 [v2.10.3.15]): it hard-codes p1RxCount = 4,
    // nDdc = 4 and streams 0-3 onto DDC0-3. The ORION-class P1 branch has
    // never been ported, so on P1 these two boards are served by a codec
    // that describes different hardware.
    //
    // Deliberately NOT closed by trimming userDdcCount to 4: that would
    // record a codec scope gap in the hardware capability table, which is
    // the wrong place for it, and would be wrong the moment the ORION-class
    // P1 branch lands. Expected-fail instead, so the day it is ported this
    // XPASSes and forces the row to be revisited.
    void orion_class_p1_capacity_gap_is_known()
    {
        const int p1 = assignableStreams(P1CodecStandard{});

        QEXPECT_FAIL("", "P1CodecStandard implements only Thetis's HERMES-class "
                         "branch (nddc=4); the ORION-class P1 branch "
                         "(console.cs:8220-8304, nddc=5) is not ported",
                     Continue);
        QVERIFY(BoardCapsTable::forBoard(HPSDRHW::Angelia).userDdcCount <= p1);

        QEXPECT_FAIL("", "P1CodecStandard implements only Thetis's HERMES-class "
                         "branch (nddc=4); the ORION-class P1 branch "
                         "(console.cs:8220-8304, nddc=5) is not ported",
                     Continue);
        QVERIFY(BoardCapsTable::forBoard(HPSDRHW::Orion).userDdcCount <= p1);
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

private:
    static void expectFits(HPSDRHW hw, int codecCapacity)
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
        QVERIFY2(caps.userDdcCount <= codecCapacity,
                 qPrintable(QStringLiteral("%1: userDdcCount %2 exceeds the %3 "
                                           "streams its codec will assign")
                                .arg(QString::fromLatin1(caps.displayName))
                                .arg(caps.userDdcCount)
                                .arg(codecCapacity)));
        // A pool smaller than the codec can drive is not a defect, but a pool
        // of zero on a board that has DDCs means no slice can ever bind.
        QVERIFY2(caps.userDdcCount >= 1,
                 qPrintable(QStringLiteral("%1: userDdcCount must be at least 1")
                                .arg(QString::fromLatin1(caps.displayName))));
    }

    static BoardCapabilities capabilitiesFor(HPSDRModel m)
    {
        // forModel returns a const reference; copy it for comparison.
        return BoardCapsTable::forModel(m);
    }
};

QTEST_MAIN(TestBoardCapabilitiesPhase3F)
#include "tst_board_capabilities_phase3f.moc"
