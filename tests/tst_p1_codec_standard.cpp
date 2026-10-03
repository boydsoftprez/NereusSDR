// no-port-check: test file — exercises P1CodecStandard (which is the port);
// the networkproto1.c cites in test-row comments reference the expected
// byte values only, not a direct code port in this file.
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <array>
#include "core/codec/P1CodecStandard.h"

using namespace NereusSDR;

class TestP1CodecStandard : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_16() {
        P1CodecStandard codec;
        QCOMPARE(codec.maxBank(), 16);
    }

    void usesI2cIntercept_is_false() {
        P1CodecStandard codec;
        QVERIFY(!codec.usesI2cIntercept());
    }

    // Table-driven: every (bank × scenario) row asserts the 5-byte output.
    // Citations on each row reference networkproto1.c [@501e3f5]
    // unless explicitly noted.
    void compose_data() {
        QTest::addColumn<int>("bank");
        QTest::addColumn<bool>("mox");
        QTest::addColumn<int>("c0_expected");
        QTest::addColumn<int>("c1_expected");
        QTest::addColumn<int>("c2_expected");
        QTest::addColumn<int>("c3_expected");
        QTest::addColumn<int>("c4_expected");
        QTest::addColumn<QByteArray>("ctx_overrides_json");

        // Bank 0 — sample rate 48k, no MOX, NDDC=1, antenna 0, rxOnlyAnt=0
        // dither[0]=random[0]=true (default), rxOnlyAnt=0 (no RX-only path)
        // C3 = 0x08|0x10 = 0x18  (bits 5-7 = 0 since rxOnlyAnt=0 and rxOut=false)
        // Source: networkproto1.c:446-471, netInterface.c:479-481 [v2.10.3.13 @501e3f5]
        QTest::newRow("bank0_rx_48k_ant0")
            << 0 << false
            << 0x00 << 0x00 << 0x00 << 0x18 << 0x04
            << QByteArray(R"({"sampleRateCode":0,"activeRxCount":1,"antennaIdx":0})");

        // Bank 0 with dither + random both off, rxOnlyAnt=0 → C3 = 0x00 (no bits set)
        QTest::newRow("bank0_dither_random_off")
            << 0 << false
            << 0x00 << 0x00 << 0x00 << 0x00 << 0x04
            << QByteArray(R"({"sampleRateCode":0,"activeRxCount":1,"antennaIdx":0,"dither":[false,false,false],"random":[false,false,false]})");

        // Bank 11 — RX ATT 20 dB, no MOX (5-bit mask + 0x20 enable)
        // Source: networkproto1.c:601 [@501e3f5]
        // C1 = 0x00: bit 6 (mic_ptt, DIRECT polarity) is CLEAR by default
        //   because CodecContext.p1MicPTT defaults false → wire bit = 0.
        //   Thetis networkproto1.c:597-598 [v2.10.3.13+501e3f51]: ((prn->mic.mic_ptt & 1) << 6)
        //   Preamp bits 0-3 = 0; mic_trs bit 4 = 0 (p1MicTipRing default true → !true = 0);
        //   mic_bias bit 5 = 0 (p1MicBias default false).  3M-1b G.5 + P1 full-parity Task 1.1.
        QTest::newRow("bank11_rx_att_20dB_ramdor_encoding")
            << 11 << false
            << 0x14 << 0x00 << 0x00 << 0x00 << ((20 & 0x1F) | 0x20)
            << QByteArray(R"({"rxStepAttn":[20,0,0]})");

        // Bank 11 — RX ATT 31 dB max
        // Source: networkproto1.c:601 [@501e3f5]
        // C1 = 0x00: same mic_ptt direct-polarity default reasoning as bank11_rx_att_20dB.
        // 3M-1b G.5 + P1 full-parity Task 1.1.
        QTest::newRow("bank11_rx_att_31dB_max")
            << 11 << false
            << 0x14 << 0x00 << 0x00 << 0x00 << ((31 & 0x1F) | 0x20)
            << QByteArray(R"({"rxStepAttn":[31,0,0]})");

        // Bank 12 — ADC1 ATT during RX (no MOX), value 7
        // Source: networkproto1.c:606-616 [@501e3f5]
        // Upstream inline attribution preserved verbatim (networkproto1.c:612):
        //   if (HPSDRModel == HPSDRModel_REDPITAYA) //[2.10.3.9]DH1KLM  //model needed as board type (prn->discovery.BoardType) is an OrionII
        QTest::newRow("bank12_rx_adc1_att_7dB")
            << 12 << false
            << 0x16 << ((7 & 0xFF) | 0x20) << ((0 & 0x1F) | 0x20) << 0x00 << 0x00
            << QByteArray(R"({"rxStepAttn":[0,7,0]})");

        // Bank 12 — ADC1 forced to 31 dB during MOX (Standard codec — non-RedPitaya)
        // Source: networkproto1.c:609 [@501e3f5]
        // Upstream inline attribution preserved verbatim (networkproto1.c:612, the RedPitaya branch this test excludes):
        //   if (HPSDRModel == HPSDRModel_REDPITAYA) //[2.10.3.9]DH1KLM  //model needed as board type (prn->discovery.BoardType) is an OrionII
        QTest::newRow("bank12_mox_adc1_forced_31dB")
            << 12 << true
            << 0x17 << (0x1F | 0x20) << (0x00 | 0x20) << 0x00 << 0x00
            << QByteArray(R"({"rxStepAttn":[0,12,0]})");  // 12 ignored under MOX

        // Bank 10 — Alex HPF/LPF passthrough, relay disengaged (default trxRelay=false → bit 7 = 1)
        // Source: networkproto1.c:583-590 [@501e3f5]
        // deskhpsdr/src/old_protocol.c:2909-2910 [@120188f] — bit 7 set when relay disabled.
        QTest::newRow("bank10_alex_passthrough_relay_off")
            << 10 << false
            << 0x12 << 0x00 << 0x40 << (0x01 | 0x80) << 0x01
            << QByteArray(R"({"alexHpfBits":1,"alexLpfBits":1,"trxRelay":false})");

        // Bank 10 — relay engaged (trxRelay=true → bit 7 = 0, relay in normal TX path)
        // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
        //   bit 7 is only SET when disablePA || !pa_enabled; during normal TX it stays 0.
        QTest::newRow("bank10_relay_engaged")
            << 10 << false
            << 0x12 << 0x00 << 0x40 << 0x01 << 0x01
            << QByteArray(R"({"alexHpfBits":1,"alexLpfBits":1,"trxRelay":true})");
    }

    void compose() {
        QFETCH(int, bank);
        QFETCH(bool, mox);
        QFETCH(int, c0_expected);
        QFETCH(int, c1_expected);
        QFETCH(int, c2_expected);
        QFETCH(int, c3_expected);
        QFETCH(int, c4_expected);
        QFETCH(QByteArray, ctx_overrides_json);

        CodecContext ctx;
        ctx.mox = mox;
        applyOverrides(ctx, ctx_overrides_json);

        P1CodecStandard codec;
        quint8 out[5] = {};
        codec.composeCcForBank(bank, ctx, out);

        QCOMPARE(int(out[0]), c0_expected);
        QCOMPARE(int(out[1]), c1_expected);
        QCOMPARE(int(out[2]), c2_expected);
        QCOMPARE(int(out[3]), c3_expected);
        QCOMPARE(int(out[4]), c4_expected);
    }

    // Thetis parity: bank 0 C4 antenna bits per networkproto1.c:463-468
    //   if (prbpfilter->_ANT_3 == 1)       C4 = 0b10;
    //   else if (prbpfilter->_ANT_2 == 1)  C4 = 0b01;
    //   else                                C4 = 0b0;
    // [v2.10.3.13 @501e3f5]
    //
    // Phase 3P-I-a T5: locks the current byte-for-byte encoding before
    // the setAntennaRouting refactor lands in T7.
    void bank0_c4_antennaIdx_matches_thetis() {
        CodecContext ctx;
        ctx.activeRxCount = 1;
        ctx.duplex = true;

        quint8 out[5]{};
        P1CodecStandard codec;

        ctx.antennaIdx = 0;  // ANT1
        codec.composeCcForBank(0, ctx, out);
        QCOMPARE(int(out[4] & 0x03), 0b00);

        ctx.antennaIdx = 1;  // ANT2
        codec.composeCcForBank(0, ctx, out);
        QCOMPARE(int(out[4] & 0x03), 0b01);

        ctx.antennaIdx = 2;  // ANT3
        codec.composeCcForBank(0, ctx, out);
        QCOMPARE(int(out[4] & 0x03), 0b10);
    }

    // Byte-locked against Thetis networkproto1.c:453-468 + netInterface.c:479-481
    // [v2.10.3.13 @501e3f5]. All 8 combinations of {rxOnlyAnt × rxOut}.
    // Mask off bits 0-4 so preamp/dither/random defaults don't contaminate.
    void bank0_c3_rxOnly_and_rxOut_byteLock() {
        struct Case { int rxOnly; bool rxOut; quint8 expectedMask; };
        const std::array<Case, 8> cases = {{
            {0, false, 0b0000'0000},  // no RX-only path, no bypass
            {1, false, 0b0010'0000},  // _Rx_1_In (bit5)
            {2, false, 0b0100'0000},  // _Rx_2_In (bit6)
            {3, false, 0b0110'0000},  // _XVTR_Rx_In (bits5+6)
            {0, true,  0b1000'0000},  // bypass only (bit7)
            {1, true,  0b1010'0000},  // RX1_In + bypass
            {2, true,  0b1100'0000},  // RX2_In + bypass
            {3, true,  0b1110'0000},  // XVTR + bypass
        }};

        for (const auto& tc : cases) {
            CodecContext ctx{};
            ctx.rxOnlyAnt = tc.rxOnly;
            ctx.rxOut     = tc.rxOut;
            quint8 out[5] = {};
            P1CodecStandard codec;
            codec.composeCcForBank(0, ctx, out);

            // Mask off bits 0-4 so preamp/dither/random defaults don't contaminate.
            const quint8 rxOnlyMask = out[3] & 0b1110'0000;
            QCOMPARE(int(rxOnlyMask), int(tc.expectedMask));
        }
    }

    // ── PureSignal with MOX: which DDC each user stream keeps ─────────────
    //
    // Phase 3F design section 16.3.2, table row "P1 Hermes / HermesC10
    // (G2E)": no collapse on Protocol 1. From Thetis console.cs:8704-8743
    // [v2.10.3.15] GetDDC(), P1 branch:
    //   case HPSDRHW.Hermes: // ANAN-10 ANAN-100 Heremes (4 adc)
    //   case HPSDRHW.HermesC10: // ANAN-G2E //N1GP G2E added (HermesC10)
    //   ...
    //   case 5: // on off on    rx1 = 0; rx2 = 1; psrx = 2; pstx = 3;
    //   case 7: // on on on     rx1 = 0; rx2 = 1; psrx = 2; pstx = 3;
    // The PureSignal pair sits on DDC2 + DDC3, so neither user stream
    // loses its DDC while PureSignal transmits. This is the opposite of
    // Protocol 2 Hermes, whose tot=5 body is empty.
    //
    // Only the four nddc == 4 models of UpdateDDCs's Hermes case share this
    // layout (console.cs:8387-8392 [v2.10.3.15]), so each one is a row.
    void ps_mox_keeps_stream0_on_ddc0_and_stream1_on_ddc1_data() {
        QTest::addColumn<int>("model");
        QTest::newRow("HERMES")   << int(HPSDRModel::HERMES);
        QTest::newRow("ANAN10")   << int(HPSDRModel::ANAN10);
        QTest::newRow("ANAN100")  << int(HPSDRModel::ANAN100);
        QTest::newRow("ANAN_G2E") << int(HPSDRModel::ANAN_G2E);
    }
    void ps_mox_keeps_stream0_on_ddc0_and_stream1_on_ddc1() {
        QFETCH(int, model);
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.model = static_cast<HPSDRModel>(model);
        ctx.mox = true;
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 96000;
        slices[1].live = true;
        slices[1].sampleRateHz = 48000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.streamDdc[1], 1);
        QCOMPARE(a.psFwdDdc, 2);
        QCOMPARE(a.psRevDdc, 3);

        // From Thetis console.cs:8449-8456 [v2.10.3.15] (UpdateDDCs,
        // "transmitting and PS is ON"): P1_DDCConfig = 6; DDCEnable = DDC0;
        // SyncEnable = DDC1; Rate[0] = Rate[1] = ps_rate; cntrl1 = 4.
        QCOMPARE(a.p1DdcConfig, 6);
        QCOMPARE(a.ddcEnable, 1);
        QCOMPARE(a.syncEnable, 2);
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.adcCtrl1, 4);
        QCOMPARE(a.p1RxCount, 4);
        QCOMPARE(a.nDdc, 4);
    }

    // The stream-0 mapping is decided inside the branch, so a dormant
    // stream 0 stays -1 under PureSignal as well.
    void ps_mox_leaves_a_dormant_stream0_unassigned() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.model = HPSDRModel::HERMES;
        ctx.mox = true;
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        slices[1].live = true;
        slices[1].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], -1);
        QCOMPARE(a.streamDdc[1], 1);
    }

    // DDC2 and DDC3 are the PureSignal pair (psrx = 2, pstx = 3), so the
    // streams that would sit on them in plain receive get nothing.
    void ps_mox_gives_streams_2_and_3_no_ddc() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.model = HPSDRModel::HERMES;
        ctx.mox = true;
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 4; ++i) {
            slices[i].live = true;
            slices[i].sampleRateHz = 192000;
        }

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[2], -1);
        QCOMPARE(a.streamDdc[3], -1);
        QCOMPARE(a.streamDdc[4], -1);
        QCOMPARE(a.ddcEnable & 0x0c, 0);
    }

    // Diversity plus PureSignal while transmitting is GetDDC case 7, which
    // carries the same mapping as case 5; UpdateDDCs sends it down the
    // same "transmitting and PS is ON" arm.
    void ps_mox_with_diversity_matches_case_7() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.model = HPSDRModel::HERMES;
        ctx.mox = true;
        ctx.puresignalRun = true;
        ctx.diversity = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;
        slices[1].live = true;
        slices[1].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.p1DdcConfig, 6);
        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.streamDdc[1], 1);
        QCOMPARE(a.psFwdDdc, 2);
        QCOMPARE(a.psRevDdc, 3);
    }

    // The other models this codec serves do not share the Hermes layout.
    // HermesII (ANAN10E, ANAN100B) carries the PureSignal pair on DDC0 +
    // DDC1 while it transmits. From Thetis console.cs:8746-8779
    // [v2.10.3.15] GetDDC(), P1 branch:
    //   case HPSDRHW.HermesII: // ANAN-10E ANAN-100B HeremesII (2 adc)
    //   ...
    //   case 5: // on off on    psrx = 0; pstx = 1;
    //   case 7: // on on on     psrx = 0; pstx = 1;
    // so DDC1 is the TX monitor and slice B must not demodulate it; DDC0 is
    // the feedback, so slice A must not either (plan Task 11: both user
    // streams suspend). The Orion/G2-class models (ANAN7000D among them) now
    // keep RX2 on frame slot 2 there, per GetDDC's Protocol 1 Orion case;
    // tst_p1_ddc_layout_per_model carries those rows.
    void ps_mox_leaves_stream1_unassigned_off_the_hermes_models_data() {
        QTest::addColumn<int>("model");
        QTest::newRow("ANAN10E")   << int(HPSDRModel::ANAN10E);
        QTest::newRow("ANAN100B")  << int(HPSDRModel::ANAN100B);
    }
    void ps_mox_leaves_stream1_unassigned_off_the_hermes_models() {
        QFETCH(int, model);
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.model = static_cast<HPSDRModel>(model);
        ctx.mox = true;
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;
        slices[1].live = true;
        slices[1].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], -1);
        QCOMPARE(a.streamDdc[1], -1);
        QCOMPARE(a.psFwdDdc, 0);
        QCOMPARE(a.psRevDdc, 1);
    }

    // PureSignal armed but not transmitting is plain receive: no pair.
    void ps_without_mox_carries_no_pair() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.psFwdDdc, -1);
        QCOMPARE(a.psRevDdc, -1);
        QCOMPARE(a.p1DdcConfig, 4);
    }

private:
    // Tiny JSON helper — keeps the data table compact. Only handles the
    // fields used by the tests above; expand as new rows are added.
    static void applyOverrides(CodecContext& ctx, const QByteArray& json);
};

void TestP1CodecStandard::applyOverrides(CodecContext& ctx, const QByteArray& json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject o = doc.object();
    if (o.contains("sampleRateCode")) { ctx.sampleRateCode = o["sampleRateCode"].toInt(); }
    if (o.contains("activeRxCount"))  { ctx.activeRxCount  = o["activeRxCount"].toInt(); }
    if (o.contains("antennaIdx"))     { ctx.antennaIdx     = o["antennaIdx"].toInt(); }
    if (o.contains("alexHpfBits"))    { ctx.alexHpfBits    = quint8(o["alexHpfBits"].toInt()); }
    if (o.contains("alexLpfBits"))    { ctx.alexLpfBits    = quint8(o["alexLpfBits"].toInt()); }
    if (o.contains("paEnabled"))      { ctx.paEnabled      = o["paEnabled"].toBool(); }
    if (o.contains("trxRelay"))       { ctx.trxRelay       = o["trxRelay"].toBool(); }
    if (o.contains("rxStepAttn")) {
        auto arr = o["rxStepAttn"].toArray();
        for (int i = 0; i < 3 && i < arr.size(); ++i) { ctx.rxStepAttn[i] = arr[i].toInt(); }
    }
    if (o.contains("txStepAttn")) {
        auto arr = o["txStepAttn"].toArray();
        for (int i = 0; i < 3 && i < arr.size(); ++i) { ctx.txStepAttn[i] = arr[i].toInt(); }
    }
    if (o.contains("dither")) {
        auto arr = o["dither"].toArray();
        for (int i = 0; i < 3 && i < arr.size(); ++i) { ctx.dither[i] = arr[i].toBool(); }
    }
    if (o.contains("random")) {
        auto arr = o["random"].toArray();
        for (int i = 0; i < 3 && i < arr.size(); ++i) { ctx.random[i] = arr[i].toBool(); }
    }
}

QTEST_APPLESS_MAIN(TestP1CodecStandard)
#include "tst_p1_codec_standard.moc"
