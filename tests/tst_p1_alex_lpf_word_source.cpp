// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// Which frequency selects the Protocol 1 Alex low-pass (bank 10 C4).
// =================================================================
//
// Protocol 1 carries ONE low-pass field, not the Alex0/Alex1 pair that
// Protocol 2 has. Thetis fills it from prbpfilter, the Alex0 struct:
//   From Thetis ChannelMaster/networkproto1.c:587-590 [v2.10.3.15]
//     C4 = (prbpfilter->_30_20_LPF & 1) | ((prbpfilter->_60_40_LPF & 1) << 1) |
//         ((prbpfilter->_80_LPF & 1) << 2) | ((prbpfilter->_160_LPF & 1) << 3) |
//         ((prbpfilter->_6_LPF & 1) << 4) | ((prbpfilter->_12_10_LPF & 1) << 5) |
//         ((prbpfilter->_17_15_LPF & 1) << 6);
// and mi0bot's HL2 loop emits the same struct at networkproto1.c:1085-1088
// [v2.10.3.14-beta1], so the HL2 is not a carve-out here.
//
// Alex0 is written by the `isMox || !isTX` arm:
//   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
//     void SetAlexLPFBits(int bits, bool isTX, bool isMox)
//     if (isMox || isTX)   -> Alex1LPFMask (prbpfilter2)
//     if (isMox || !isTX)  -> AlexLPFMask  (prbpfilter)
//   Upstream comment preserved verbatim (netInterface.c:676-680):
//     // LPF bits can be used in older radioas as part of RX filtering too.
//     // Change to protocol 2 from 4.3 onwards: TX settings are encoded in
//     // the Alex1 word to remain comparible with older hardware, the logic
//     // will be:
//     // if MOX, write settings to alex0 and alex1
//     // if not MOX, write to alex1 if a TX setting else write to alex0
//
// So the single P1 field carries:
//   keyed   (isMox true)  -> the TRANSMIT selection, from UpdateTXDDSFreq
//                            (console.cs:15464-15468 [v2.10.3.15])
//   unkeyed (isMox false) -> the RECEIVE selection, from UpdateAlexTXFilter
//                            (console.cs:15487-15498 [v2.10.3.15])
//
// and UpdateAlexTXFilter picks WHICH receive frequency by board:
//   From Thetis console.cs:15487-15498 UpdateAlexTXFilter [v2.10.3.15]
//     private void UpdateAlexTXFilter()
//     {
//         if (!_mox)
//         {
//             if (!_rx2_preamp_present && chkRX2.Checked)
//             {
//                 if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
//                 else setAlexLPF(rx2_dds_freq_mhz, false);
//             }
//             else setAlexLPF(rx1_dds_freq_mhz, false);
//         }
//     }
//
// The higher frequency wins because a low-pass passes everything BELOW its
// corner: selecting the lower receiver's filter would attenuate the higher
// receiver. Its mirror image, UpdateAlexRXFilter (console.cs:15500-15510),
// takes the LOWER frequency for the high-pass, and together the pair spans
// both receivers.
//
// _rx2_preamp_present is a per-model flag: true means RX2 has its own front
// end and does not share the Alex chain, so RX1 alone decides.
//   From Thetis console.cs:14783-14857 SetupForHPSDRModel [v2.10.3.15]
//   Upstream inline attribution preserved verbatim, every tag in that
//   range:
//     case HPSDRModel.ANAN_G2E: //N1GP G2E added
//     case HPSDRModel.ANAN_G2_1K:                          // G8NJJ: likely to need further changes for PA
//     case HPSDRModel.REDPITAYA: //DH1KLM
//     RX2PreampPresent = _rx2_preamp_present; //[2.10.3.11]MW0LGE we were setting the member var above, but this was not actually having any effect/update
//     HERMES / ANAN10 / ANAN10E / ANAN100 / ANAN100B / ANAN_G2E -> false
//     ANAN100D / ANAN200D / ORIONMKII / ANAN7000D / ANAN8000D /
//     ANAN_G2 / ANAN_G2_1K / ANVELINAPRO3 / REDPITAYA          -> true
//   HERMESLITE is absent from the switch, so it keeps the field initializer
//   at console.cs:15068 [v2.10.3.15]: `private bool _rx2_preamp_present = false;`
//
// Bank 10 byte layout for C0=0x12 (networkproto1.c:578-591 [v2.10.3.15]):
//   out[0] = C0 (0x12, ORed with MOX bit 0)
//   out[1] = TX drive level
//   out[2] = mic flags
//   out[3] = Alex HPF bits | T/R relay disable (bit 7, inverted)
//   out[4] = Alex LPF bits   <- the byte under test
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/P1RadioConnection.h"
#include "core/TxSliceArbiter.h"
#include "core/codec/AlexFilterMap.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kLpfByte = 4;   // bank 10 C4

constexpr quint64 k80mHz  =  3700000ULL;   // computeLpf -> 0x04
constexpr quint64 k10mHz  = 28400000ULL;   // computeLpf -> 0x20
constexpr quint64 k20mHz  = 14200000ULL;   // computeLpf -> 0x01

// setState is protected on RadioConnection, so the model-level case needs a
// subclass to come up Connected the way a real session would.
class ConnectedP1RadioConnection final : public P1RadioConnection {
public:
    ConnectedP1RadioConnection() { setState(ConnectionState::Connected); }
};

quint8 lpfByte(const P1RadioConnection& conn)
{
    const QByteArray bank10 = conn.captureBank10ForTest();
    Q_ASSERT(bank10.size() == 5);
    return static_cast<quint8>(bank10[kLpfByte]);
}

} // namespace

class TestP1AlexLpfWordSource : public QObject {
    Q_OBJECT
private slots:

    // ── RF-SAFETY: keyed, the field carries the TRANSMIT selection ───────
    //
    // This is the property the whole two-mask split exists to protect, and
    // the one that gates the two-band TX bench row. A receive slice parked
    // on 80 m must not put a ~4 MHz low-pass in front of a 28.4 MHz carrier.
    //   From Thetis console.cs:15464-15468 UpdateTXDDSFreq [v2.10.3.15]
    //     private void UpdateTXDDSFreq()
    //     { if (initializing) return;
    //       setAlexLPF(tx_dds_freq_mhz, true); ... }
    // Upstream inline attribution preserved verbatim (console.cs:15471):
    //   if (MOX)//[2.10.3.13]MW0LGE
    // with isMox true, so the Alex0 arm of SetAlexLPFBits runs too.
    void keyed_lpfByteCarriesTheTransmitSelection()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setReceiverFrequency(0, k80mHz);   // listening low
        conn.setTxFrequency(k10mHz);            // transmitting high
        conn.setMox(true);

        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(28.4));
        QVERIFY2(lpfByte(conn) != codec::alex::computeLpf(3.7),
                 "P1 transmit low-pass followed a receive frequency");
    }

    // A receive retune arriving mid-transmission must not move the byte.
    // Thetis cannot even reach the receive-derived write while keyed:
    // UpdateAlexTXFilter is wrapped in `if (!_mox)`
    // (console.cs:15487-15498 [v2.10.3.15]).
    void keyed_receiveRetuneDoesNotMoveTheLpfByte()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(0, k10mHz);
        conn.setMox(true);

        const quint8 before = lpfByte(conn);
        conn.setReceiverFrequency(1, k80mHz);
        QCOMPARE(lpfByte(conn), before);
        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(28.4));
    }

    // ── Unkeyed, the field carries the RECEIVE selection ─────────────────
    //
    // The half Protocol 1 never had. Before this test, P1 wrote the LPF
    // only from setTxFrequency and emitted it keyed or not, so with the
    // transmitter bound to an 80 m slice and the operator listening on
    // 10 m the receiver sat behind a ~4 MHz low-pass.
    //   From Thetis console.cs:15496 UpdateAlexTXFilter [v2.10.3.15]
    //     else setAlexLPF(rx1_dds_freq_mhz, false);
    void unkeyed_lpfByteCarriesTheReceiveSelection()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setTxFrequency(k80mHz);            // transmitter bound low
        conn.setReceiverFrequency(0, k10mHz);   // listening high

        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(28.4));
    }

    // Dropping MOX hands the byte back to the receive selection, and taking
    // it again hands it back to the transmit selection.
    //   From Thetis console.cs:29140-29148 HdwMOXChanged [v2.10.3.15]: the
    //   MOX-off arm calls UpdateRX1DDSFreq() (-> UpdateAlexTXFilter, now
    //   unguarded) before UpdateTXDDSFreq().
    void moxEdges_swapTheByteBetweenTheTwoSelections()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setReceiverFrequency(0, k80mHz);
        conn.setTxFrequency(k10mHz);

        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(3.7));
        conn.setMox(true);
        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(28.4));
        conn.setMox(false);
        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(3.7));
    }

    // ── Which receive frequency, on a board whose RX2 shares the chain ───
    //
    // ANAN-10E (HermesII) is `_rx2_preamp_present = false`
    // (console.cs:14800-14803 [v2.10.3.15]), so with a second receiver live
    // the HIGHER of the two frequencies selects the low-pass.
    //   From Thetis console.cs:15493-15494 [v2.10.3.15]
    //     if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
    //     else setAlexLPF(rx2_dds_freq_mhz, false);
    void unkeyed_sharedChainTakesTheHigherReceiveFrequency_data()
    {
        QTest::addColumn<quint64>("rx0");
        QTest::addColumn<quint64>("rx1");
        QTest::addColumn<double>("expectMhz");
        // Both orders, so a fix that merely makes the last writer win fails.
        QTest::newRow("low then high") << k80mHz << k10mHz << 28.4;
        QTest::newRow("high then low") << k10mHz << k80mHz << 28.4;
        QTest::newRow("both mid")      << k20mHz << k20mHz << 14.2;
    }

    void unkeyed_sharedChainTakesTheHigherReceiveFrequency()
    {
        QFETCH(quint64, rx0);
        QFETCH(quint64, rx1);
        QFETCH(double, expectMhz);

        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setReceiverFrequency(0, rx0);
        conn.setReceiverFrequency(1, rx1);

        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(expectMhz));
    }

    // ── ...and on a board whose RX2 has its own front end ────────────────
    //
    // ANAN-100D (Angelia) is `_rx2_preamp_present = true`
    // (console.cs:14815-14817 [v2.10.3.15]), so RX2 never influences the
    // selection however it is tuned.
    //   From Thetis console.cs:15496 [v2.10.3.15]
    //     else setAlexLPF(rx1_dds_freq_mhz, false);
    void unkeyed_ownFrontEndBoardUsesTheFirstReceiverOnly()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Angelia);

        conn.setReceiverFrequency(0, k80mHz);
        conn.setReceiverFrequency(1, k10mHz);

        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(3.7));
    }

    // ── The low-pass has no bypass encoding ──────────────────────────────
    //
    // Thetis's fall-through arm picks 0x10 (6 m, the widest low-pass)
    // rather than zero (console.cs:7237-7241 [v2.10.3.15]), so no state
    // reachable here may emit an unfiltered byte once a frequency is known.
    void lpfByteIsNeverZeroOnceAFrequencyIsKnown()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setReceiverFrequency(0, k80mHz);
        QVERIFY(lpfByte(conn) != 0);

        conn.setTxFrequency(k10mHz);
        QVERIFY(lpfByte(conn) != 0);

        conn.setMox(true);
        QVERIFY(lpfByte(conn) != 0);

        conn.setMox(false);
        QVERIFY(lpfByte(conn) != 0);
    }

    // ── No regression for the single-slice case ─────────────────────────
    //
    // One slice, simplex: receive and transmit are the same frequency, so
    // the byte is identical keyed or not and matches pre-fix output.
    void singleSliceSimplex_byteIsTheSameKeyedOrNot()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setReceiverFrequency(0, k20mHz);
        conn.setTxFrequency(k20mHz);

        const quint8 unkeyed = lpfByte(conn);
        conn.setMox(true);
        QCOMPARE(lpfByte(conn), unkeyed);
        QCOMPARE(unkeyed, codec::alex::computeLpf(14.2));
    }

    // ── The HL2 path composes through its own codec ─────────────────────
    //
    // mi0bot's HL2 loop emits the same prbpfilter struct
    // (networkproto1.c:1085-1088 [v2.10.3.14-beta1]), and NereusSDR routes
    // the HL2 through P1CodecHl2 rather than P1CodecStandard, so the byte
    // has to be right on both composition paths.
    void hl2Codec_alsoSwapsTheByteOnTheMoxEdge()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);

        conn.setReceiverFrequency(0, k80mHz);
        conn.setTxFrequency(k10mHz);

        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(3.7));
        conn.setMox(true);
        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(28.4));
    }

    // ── The connect-time push happens before m_caps exists ──────────────
    //
    // RadioModel queues the first setReceiverFrequency BEFORE dispatching
    // connectToRadio, so the opening C&C frame carries the persisted VFO.
    // connectToRadio is where m_caps is assigned, so at that moment the
    // filter gate has only the hardware profile to go on. Gating on m_caps
    // alone skipped the selection on every Alex board but the HL2, and left
    // the receive low-pass at zero until the operator turned the VFO.
    //
    // Reproduces that ordering: profile handed over, no setBoardForTest,
    // exactly as a real connect looks at the moment the push runs.
    void receiveSelectionSurvivesTheConnectTimePushOrdering()
    {
        P1RadioConnection conn;

        HardwareProfile profile;
        profile.model          = HPSDRModel::ANAN100D;
        profile.effectiveBoard = HPSDRHW::Angelia;
        profile.caps           = &BoardCapsTable::forBoard(HPSDRHW::Angelia);
        conn.setHardwareProfile(profile);

        conn.setReceiverFrequency(0, k80mHz);

        QVERIFY2(lpfByte(conn) != 0,
                 "connect-time receive push left the low-pass unselected");
        QCOMPARE(lpfByte(conn), codec::alex::computeLpf(3.7));
    }

    // ── End to end: the byte follows the TX-BOUND slice, not slice 0 ─────
    //
    // The property that gates the two-band TX bench row, exercised through
    // the model rather than by calling setTxFrequency directly. Slice A sits
    // on 10 m and is the ACTIVE slice; slice B on 80 m holds the
    // transmitter. Keying must put the 80 m low-pass on the wire, and
    // turning slice A's knob must not disturb it.
    //
    // Sourcing this from the active slice, from slice 0, or from the last
    // slice retuned would each give the wrong answer here, and each has been
    // a real defect in this tree.
    void keyed_byteFollowsTheTxBoundSliceNotTheActiveOne()
    {
        AppSettings::instance().clear();

        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);

        auto* conn = new ConnectedP1RadioConnection();
        conn->setBoardForTest(HPSDRHW::HermesII);
        model.injectConnectionForTest(conn);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(28400000.0);   // 10 m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(3700000.0);    // 80 m

        // The operator is watching slice A while slice B transmits.
        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), b);

        conn->setMox(true);
        QCOMPARE(lpfByte(*conn), codec::alex::computeLpf(3.7));

        // Turning the active (receive-only) slice's knob mid-transmission
        // must not drag the transmit low-pass onto its band.
        model.slices().at(a)->setFrequency(28450000.0);
        QCOMPARE(lpfByte(*conn), codec::alex::computeLpf(3.7));

        // ...and handing the transmitter back moves it, on its own trigger,
        // without waiting for a retune.
        conn->setMox(false);
        QVERIFY(model.txSliceArbiter()->requestHandoff(a));
        conn->setMox(true);
        QCOMPARE(lpfByte(*conn), codec::alex::computeLpf(28.45));

        model.injectConnectionForTest(nullptr);
        delete conn;
        AppSettings::instance().clear();
    }

    // ── The capability table itself ─────────────────────────────────────
    //
    // rx2PreampPresent decides which receive frequency selects the low-pass,
    // so a wrong row is a wrong filter on a radio nobody here owns. Pin the
    // whole table against the upstream switch rather than only the SKUs a
    // bench can reach.
    //   From Thetis console.cs:14783-14857 SetupForHPSDRModel [v2.10.3.15]
//   Upstream inline attribution preserved verbatim, every tag in that
//   range:
//     case HPSDRModel.ANAN_G2E: //N1GP G2E added
//     case HPSDRModel.ANAN_G2_1K:                          // G8NJJ: likely to need further changes for PA
//     case HPSDRModel.REDPITAYA: //DH1KLM
//     RX2PreampPresent = _rx2_preamp_present; //[2.10.3.11]MW0LGE we were setting the member var above, but this was not actually having any effect/update
    //   plus the field initializer at console.cs:15068 [v2.10.3.15] for the
    //   models absent from that switch.
    void capabilityTableMatchesTheUpstreamPerModelSwitch_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<bool>("expected");

        // false: absent from the switch, so the field initializer holds.
        QTest::newRow("Atlas")            << int(HPSDRHW::Atlas)      << false;
        // false: case HPSDRModel.HERMES (console.cs:14790-14793)
        QTest::newRow("Hermes")           << int(HPSDRHW::Hermes)     << false;
        // false: case HPSDRModel.ANAN10E / ANAN100B (console.cs:14800-14813)
        QTest::newRow("HermesII")         << int(HPSDRHW::HermesII)   << false;
        // true:  case HPSDRModel.ANAN100D (console.cs:14815-14817)
        QTest::newRow("Angelia")          << int(HPSDRHW::Angelia)    << true;
        // true:  case HPSDRModel.ANAN200D (console.cs:14819-14821)
        QTest::newRow("Orion")            << int(HPSDRHW::Orion)      << true;
        // true:  ORIONMKII / ANAN7000D / ANAN8000D / ANVELINAPRO3 / REDPITAYA
        QTest::newRow("OrionMKII")        << int(HPSDRHW::OrionMKII)  << true;
        // false: case HPSDRModel.ANAN_G2E (console.cs:14835-14837)
        QTest::newRow("HermesC10")        << int(HPSDRHW::HermesC10)  << false;
        // false: HERMESLITE never appears in the switch.
        QTest::newRow("HermesLite")       << int(HPSDRHW::HermesLite) << false;
        QTest::newRow("HermesLiteRxOnly") << int(HPSDRHW::HermesLiteRxOnly) << false;
        // true:  case HPSDRModel.ANAN_G2 / ANAN_G2_1K (console.cs:14839-14845)
        QTest::newRow("Saturn")           << int(HPSDRHW::Saturn)     << true;
        // true:  inherited from the ANAN-G2 board revision it describes.
        QTest::newRow("SaturnMKII")       << int(HPSDRHW::SaturnMKII) << true;
        // true:  derived from kSaturn; NereusSDR judgement, no upstream row.
        QTest::newRow("Andromeda")        << int(HPSDRHW::Andromeda)  << true;
    }

    void capabilityTableMatchesTheUpstreamPerModelSwitch()
    {
        QFETCH(int, board);
        QFETCH(bool, expected);

        const BoardCapabilities& caps =
            BoardCapsTable::forBoard(static_cast<HPSDRHW>(board));
        QCOMPARE(caps.rx2PreampPresent, expected);
    }

    // Every row in the table is covered by the case list above, so a new SKU
    // cannot be added without deciding what its receive low-pass does.
    void everyBoardRowIsCoveredByTheCaseList()
    {
        const QSet<int> covered = {
            int(HPSDRHW::Atlas),      int(HPSDRHW::Hermes),
            int(HPSDRHW::HermesII),   int(HPSDRHW::Angelia),
            int(HPSDRHW::Orion),      int(HPSDRHW::OrionMKII),
            int(HPSDRHW::HermesC10),  int(HPSDRHW::HermesLite),
            int(HPSDRHW::HermesLiteRxOnly), int(HPSDRHW::Saturn),
            int(HPSDRHW::SaturnMKII), int(HPSDRHW::Andromeda),
            int(HPSDRHW::Unknown),
        };
        for (const BoardCapabilities& caps : BoardCapsTable::all()) {
            QVERIFY2(covered.contains(int(caps.board)),
                     qPrintable(QStringLiteral(
                         "board %1 has no rx2PreampPresent case; decide what "
                         "its receive low-pass does before adding the row")
                         .arg(int(caps.board))));
        }
    }
};

QTEST_MAIN(TestP1AlexLpfWordSource)
#include "tst_p1_alex_lpf_word_source.moc"
