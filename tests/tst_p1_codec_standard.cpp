// no-port-check: test file — exercises P1CodecStandard (which is the port);
// the networkproto1.c cites in test-row comments reference the expected
// byte values only, not a direct code port in this file.
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

        // Bank 0 — sample rate 48k, no MOX, NDDC=1, antenna 0
        // dither[0]=random[0]=true (default), so C3 = 0x08|0x10|0x20 = 0x38
        // Source: networkproto1.c:446-471 [@501e3f5]
        QTest::newRow("bank0_rx_48k_ant0")
            << 0 << false
            << 0x00 << 0x00 << 0x00 << 0x38 << 0x04
            << QByteArray(R"({"sampleRateCode":0,"activeRxCount":1,"antennaIdx":0})");

        // Bank 0 with dither + random both off → C3 just has the 0x20 RX-in-mux
        QTest::newRow("bank0_dither_random_off")
            << 0 << false
            << 0x00 << 0x00 << 0x00 << 0x20 << 0x04
            << QByteArray(R"({"sampleRateCode":0,"activeRxCount":1,"antennaIdx":0,"dither":[false,false,false],"random":[false,false,false]})");

        // Bank 11 — RX ATT 20 dB, no MOX (5-bit mask + 0x20 enable)
        // Source: networkproto1.c:601 [@501e3f5]
        QTest::newRow("bank11_rx_att_20dB_ramdor_encoding")
            << 11 << false
            << 0x14 << 0x00 << 0x00 << 0x00 << ((20 & 0x1F) | 0x20)
            << QByteArray(R"({"rxStepAttn":[20,0,0]})");

        // Bank 11 — RX ATT 31 dB max
        // Source: networkproto1.c:601 [@501e3f5]
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

        // Bank 10 — Alex HPF/LPF passthrough, PA enabled
        // Source: networkproto1.c:583-590 [@501e3f5]
        QTest::newRow("bank10_alex_passthrough_pa_on")
            << 10 << false
            << 0x12 << 0x00 << 0x40 << (0x01 | 0x80) << 0x01
            << QByteArray(R"({"alexHpfBits":1,"alexLpfBits":1,"paEnabled":true})");
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
