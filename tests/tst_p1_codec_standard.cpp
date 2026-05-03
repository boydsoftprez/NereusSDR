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
        //   Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]: ((prn->mic.mic_ptt & 1) << 6)
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
