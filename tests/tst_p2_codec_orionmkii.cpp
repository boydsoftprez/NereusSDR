// no-port-check: test fixture cites Thetis source for expected values, not a port itself
#include <QtTest/QtTest>
#include "core/codec/P2CodecOrionMkII.h"
#include "core/codec/CodecContext.h"
#include "core/P2RadioConnection.h"

#include <array>

using namespace NereusSDR;

class TestP2CodecOrionMkII : public QObject {
    Q_OBJECT
private slots:
    // CmdGeneral byte 4 — command byte always 0x00
    // Source: network.c:826 [@501e3f5]
    void cmdGeneral_byte4_command_zero() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        quint8 buf[60] = {};
        codec.composeCmdGeneral(ctx, buf);
        QCOMPARE(int(buf[4]), 0x00);
    }

    // CmdGeneral bytes 0-3 — sequence number NOT stamped by codec
    // Source: network.c note — seqno stamped by caller [@501e3f5]
    void cmdGeneral_seqno_zero() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        quint8 buf[60] = {};
        codec.composeCmdGeneral(ctx, buf);
        QCOMPARE(int(buf[0]), 0);
        QCOMPARE(int(buf[1]), 0);
        QCOMPARE(int(buf[2]), 0);
        QCOMPARE(int(buf[3]), 0);
    }

    // CmdGeneral port bytes 5-6 — custom port base default 1025
    // Source: network.c:839 [@501e3f5]
    void cmdGeneral_portBase_default() {
        P2CodecOrionMkII codec;
        CodecContext ctx;  // default p2CustomPortBase = 1025
        quint8 buf[60] = {};
        codec.composeCmdGeneral(ctx, buf);
        // Port base 1025 big-endian: 0x04 0x01
        QCOMPARE(int(buf[5]), 0x04);
        QCOMPARE(int(buf[6]), 0x01);
    }

    // CmdGeneral byte 37 — freq/phase-word mode bit (always 0x08)
    // Source: network.c:896 [@501e3f5]
    void cmdGeneral_byte37_phaseWordBit() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        quint8 buf[60] = {};
        codec.composeCmdGeneral(ctx, buf);
        QCOMPARE(int(buf[37]), 0x08);
    }

    // CmdGeneral byte 59 — Alex enable (both boards enabled = 0x03)
    // Source: network.c:906 [@501e3f5]
    void cmdGeneral_byte59_alexEnable() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        quint8 buf[60] = {};
        codec.composeCmdGeneral(ctx, buf);
        QCOMPARE(int(buf[59]), 0x03);
    }

    // CmdHighPriority byte 4 — PTT off (not running, no PTT)
    // Source: network.c:924-925 [@501e3f5]
    void cmdHighPriority_byte4_ptt_off() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        QCOMPARE(int(buf[4]) & 0x02, 0);  // PTT bit clear
        QCOMPARE(int(buf[4]) & 0x01, 0);  // run bit clear
    }

    // CmdHighPriority byte 4 — PTT on (p2Running + p2PttOut)
    // Source: network.c:924-925 [@501e3f5]
    void cmdHighPriority_byte4_ptt_on() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.p2Running = true;
        ctx.p2PttOut  = 1;
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        QCOMPARE(int(buf[4]) & 0x02, 0x02);  // PTT bit set
        QCOMPARE(int(buf[4]) & 0x01, 0x01);  // run bit set
    }

    // CmdHighPriority bytes 1432-1435 — Alex0 HPF 13 MHz (bit 1 in reg = 0x02)
    // Source: network.c:1040-1050 [@501e3f5]
    void cmdHighPriority_alex0_hpf_13MHz() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.alexHpfBits = 0x01;  // 13 MHz HPF
        ctx.alexLpfBits = 0x01;  // 30/20m LPF
        ctx.p2AlexRxAnt = 1;     // ANT1
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        // Alex0 bytes 1432-1435 big-endian. HPF 13MHz → bit 1 → contributes to byte 1435.
        // ANT1 → bit 24 → byte 1432 bit 0. LPF 30/20m → bit 20 → byte 1432 bit 4.
        // Verify register is non-zero (at minimum HPF bit is set)
        QVERIFY(buf[1432] != 0 || buf[1433] != 0 || buf[1434] != 0 || buf[1435] != 0);
        // HPF 13MHz (0x01) → bit 1 → byte 1435 bit 1
        QVERIFY(buf[1435] & 0x02);
    }

    // CmdHighPriority byte 1443 — RX ATT for ADC0
    // Source: network.c:1055-1057 [@501e3f5]
    void cmdHighPriority_rxAtt_20dB() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.rxStepAttn[0] = 20;
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        QCOMPARE(int(buf[1443]), 20);  // ADC0 attenuation raw value
    }

    // CmdHighPriority byte 1442 — RX ATT for ADC1
    // Source: network.c:1055-1057 [@501e3f5]
    void cmdHighPriority_rxAtt_adc1() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.rxStepAttn[1] = 10;
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        QCOMPARE(int(buf[1442]), 10);  // ADC1 attenuation raw value
    }

    // CmdTx bytes 57-59 — TX ATT per ADC
    // Source: network.c:1238-1242 [@501e3f5]
    void cmdTx_txAtt_per_adc() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.txStepAttn[0] = 5;
        ctx.txStepAttn[1] = 7;
        ctx.txStepAttn[2] = 9;
        quint8 buf[60] = {};
        codec.composeCmdTx(ctx, buf);
        QCOMPARE(int(buf[59]), 5);  // ADC0
        QCOMPARE(int(buf[58]), 7);  // ADC1
        QCOMPARE(int(buf[57]), 9);  // ADC2
    }

    // CmdRx bytes 5-6 — dither + random bits per ADC, defaults on
    // Source: network.c:1080-1090 [@501e3f5]
    void cmdRx_dither_random_default_on() {
        P2CodecOrionMkII codec;
        CodecContext ctx;  // defaults: dither[*]=true, random[*]=true
        quint8 buf[1444] = {};
        codec.composeCmdRx(ctx, buf);
        QVERIFY(buf[5] != 0);  // some dither bits set
        QVERIFY(buf[6] != 0);  // some random bits set
    }

    // CmdRx bytes 5-6 — dither + random off when all disabled
    // Source: network.c:1080-1090 [@501e3f5]
    void cmdRx_dither_random_off() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        for (int i = 0; i < 3; ++i) {
            ctx.dither[i] = false;
            ctx.random[i] = false;
        }
        quint8 buf[1444] = {};
        codec.composeCmdRx(ctx, buf);
        QCOMPARE(int(buf[5]), 0);
        QCOMPARE(int(buf[6]), 0);
    }

    // CmdRx byte 7 — DDC enable bitmask: enable DDC0 only
    // Source: network.c:1097-1103 [@501e3f5]
    void cmdRx_enableMask_ddc0_only() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.p2RxEnable[0] = 1;  // DDC0 only
        quint8 buf[1444] = {};
        codec.composeCmdRx(ctx, buf);
        QCOMPARE(int(buf[7]), 0x01);
    }

    // CmdHighPriority RX0 phase word — unity correction factor matches
    // raw Thetis formula (freq_hz * 2^32 / 122880000).
    // Source: network.c:936-1005 [@501e3f5]
    void cmdHighPriority_rx0_phaseWord_unity_factor() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.rxFreqHz[0] = 14'200'000ULL;  // 20m
        ctx.freqCorrectionFactor = 1.0;
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        const quint32 expected =
            static_cast<quint32>((14'200'000.0 * 4294967296.0) / 122880000.0);
        const quint32 got =
            (quint32(buf[9]) << 24) | (quint32(buf[10]) << 16) |
            (quint32(buf[11]) << 8) |  quint32(buf[12]);
        QCOMPARE(got, expected);
    }

    // CmdHighPriority RX0 phase word — non-unity correction factor changes
    // the emitted phase word. Phase 3P-G: codec cutover path must apply
    // CalibrationController::effectiveFreqCorrectionFactor() via ctx.
    // Source: setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged
    // → NetworkIO.FreqCorrectionFactor [@501e3f5]
    void cmdHighPriority_phaseWord_applies_correction_factor() {
        P2CodecOrionMkII codec;
        CodecContext ctxA;
        CodecContext ctxB;
        ctxA.rxFreqHz[0] = 14'200'000ULL;
        ctxA.txFreqHz    = 14'200'000ULL;
        ctxA.freqCorrectionFactor = 1.0;
        ctxB.rxFreqHz[0] = 14'200'000ULL;
        ctxB.txFreqHz    = 14'200'000ULL;
        ctxB.freqCorrectionFactor = 0.9999995;  // ~0.5 ppm trim
        quint8 bufA[1444] = {};
        quint8 bufB[1444] = {};
        codec.composeCmdHighPriority(ctxA, bufA);
        codec.composeCmdHighPriority(ctxB, bufB);

        // RX0 phase word (bytes 9-12) must differ between unity and trimmed factor.
        const quint32 rxA = (quint32(bufA[9]) << 24) | (quint32(bufA[10]) << 16) |
                            (quint32(bufA[11]) << 8) |  quint32(bufA[12]);
        const quint32 rxB = (quint32(bufB[9]) << 24) | (quint32(bufB[10]) << 16) |
                            (quint32(bufB[11]) << 8) |  quint32(bufB[12]);
        QVERIFY(rxA != rxB);

        // TX phase word (bytes 329-332) must likewise differ.
        const quint32 txA = (quint32(bufA[329]) << 24) | (quint32(bufA[330]) << 16) |
                            (quint32(bufA[331]) << 8) |  quint32(bufA[332]);
        const quint32 txB = (quint32(bufB[329]) << 24) | (quint32(bufB[330]) << 16) |
                            (quint32(bufB[331]) << 8) |  quint32(bufB[332]);
        QVERIFY(txA != txB);

        // ctxB factor is below 1.0 → phase word should be strictly smaller.
        QVERIFY(rxB < rxA);
        QVERIFY(txB < txA);
    }

    // CmdHighPriority — sequence bytes 0-3 NOT stamped by codec
    // Source: caller responsibility note [@501e3f5]
    void cmdHighPriority_seqno_zero() {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);
        QCOMPARE(int(buf[0]), 0);
        QCOMPARE(int(buf[1]), 0);
        QCOMPARE(int(buf[2]), 0);
        QCOMPARE(int(buf[3]), 0);
    }

    // Byte-locked against Thetis network.h:279-282 + netInterface.c:479-481
    // [v2.10.3.13 @501e3f5]. Alex0 bits 8-11 encoding:
    //   bit  8: _XVTR_Rx_In (rxOnlyAnt & 0x03 == 0x03)
    //   bit  9: _Rx_2_In    (rxOnlyAnt & 0x03 == 0x02, EXT1)
    //   bit 10: _Rx_1_In    (rxOnlyAnt & 0x03 == 0x01, EXT2)
    //   bit 11: _Rx_1_Out   (rxOut == true, K36 RL17 bypass relay)
    // NOTE: bit 27 is _TR_Relay — NOT an RX-only bit; not set here.
    //
    // Phase 3P-I-b T5: 8-case lock (4 rxOnlyAnt values × 2 rxOut values).
    // Mask isolates bits 8-11 to avoid contamination from ANT (24-26),
    // LPF (20-23, 29-31), and HPF (1-6, 12) bits.
    void alex0_rxOnly_and_rxOut_byteLock() {
        struct Case { int rxOnly; bool rxOut; quint32 expectedBits8_11; };
        const std::array<Case, 8> cases = {{
            {0, false, 0x00000000u},
            {1, false, (1u << 10)},                   // _Rx_1_In  (EXT2)
            {2, false, (1u <<  9)},                   // _Rx_2_In  (EXT1)
            {3, false, (1u <<  8)},                   // _XVTR_Rx_In
            {0, true,  (1u << 11)},                   // _Rx_1_Out only
            {1, true,  (1u << 10) | (1u << 11)},     // _Rx_1_In + _Rx_1_Out
            {2, true,  (1u <<  9) | (1u << 11)},     // _Rx_2_In + _Rx_1_Out
            {3, true,  (1u <<  8) | (1u << 11)},     // _XVTR_Rx_In + _Rx_1_Out
        }};

        constexpr quint32 kMask8_11 = 0b1111u << 8;  // isolate bits 8-11

        for (const auto& tc : cases) {
            P2CodecOrionMkII codec;
            CodecContext ctx;
            ctx.p2AlexRxAnt = 1;    // ANT1 — valid but shouldn't appear in bits 8-11
            ctx.p2AlexTxAnt = 1;
            ctx.rxOnlyAnt   = tc.rxOnly;
            ctx.rxOut       = tc.rxOut;

            quint8 buf[1444] = {};
            codec.composeCmdHighPriority(ctx, buf);

            // Alex0 bytes 1432-1435 big-endian: bit N of reg is in byte[1432 + (31-N)/8].
            // Bits 8-11 map to byte 1434 (bits 8-11 = byte[1434] bits 0-3 in BE).
            // Re-assemble Alex0 word for clean bit-level comparison.
            const quint32 alex0 =
                (quint32(buf[1432]) << 24) | (quint32(buf[1433]) << 16) |
                (quint32(buf[1434]) <<  8) |  quint32(buf[1435]);

            const quint32 masked = alex0 & kMask8_11;
            QCOMPARE(masked, tc.expectedBits8_11);
        }
    }

    // Issue #257 — Mk II BPF encoding split.
    //
    // Thetis ChannelMaster/netInterface.c:461-477 [v2.10.3.13 @501e3f51]
    // SetAntBits():
    //   if (mkiibpf)
    //   {
    //       if (rx_only_ant == 1 || tx)   // EXT2 path or TX
    //       {
    //           prbpfilter->_Rx_1_Out = (rx_out & 0x01) != 0;  // bit 11 RL17
    //           prbpfilter->_10_dB_Atten = 0;                  // bit 14 RL22
    //       }
    //       else                          // EXT1 / BYPS / XVTR while RX
    //       {
    //           prbpfilter->_Rx_1_Out = 0;
    //           prbpfilter->_10_dB_Atten = rx_out & 0x1;       // bit 14 RL22
    //       }
    //   }
    //   else
    //   {
    //       prbpfilter->_Rx_1_Out = (rx_out & 0x01) != 0;      // bit 11 RL17
    //   }
    //
    // Mask isolates bits 11 + 14 (the two relay bits split by the conditional)
    // so the test ignores HPF/LPF/ANT/RX-only-mux contamination.
    void alex0_mkiibpf_relay_split_byteLock() {
        struct Case {
            const char* name;
            bool        mkiiBpf;
            int         rxOnly;
            bool        rxOut;
            bool        mox;
            quint32     expectedBits11_14;  // bits 11 + 14 only
        };
        const std::array<Case, 12> cases = {{
            // Non-Mk II boards — bit 11 carries rx_out unconditionally;
            // bit 14 always clear.
            {"non-mkii  ANT1     rxOut=0 rx", false, 0, false, false, 0u},
            {"non-mkii  EXT1     rxOut=1 rx", false, 2, true,  false, (1u << 11)},
            {"non-mkii  EXT1     rxOut=1 tx", false, 2, true,  true,  (1u << 11)},
            {"non-mkii  XVTR     rxOut=1 rx", false, 3, true,  false, (1u << 11)},

            // Mk II boards — split:
            //   rxOnly==1 (EXT2) OR mox=1  →  bit 11 set, bit 14 clear
            //   else (EXT1 / XVTR / no-mux on RX)  →  bit 14 set, bit 11 clear
            {"mkii      ANT1     rxOut=0 rx", true,  0, false, false, 0u},
            {"mkii      EXT2     rxOut=1 rx", true,  1, true,  false, (1u << 11)},
            {"mkii      EXT1     rxOut=1 rx", true,  2, true,  false, (1u << 14)},  // the bug fix
            {"mkii      XVTR     rxOut=1 rx", true,  3, true,  false, (1u << 14)},
            {"mkii      EXT2     rxOut=1 tx", true,  1, true,  true,  (1u << 11)},
            {"mkii      EXT1     rxOut=1 tx", true,  2, true,  true,  (1u << 11)},
            {"mkii      XVTR     rxOut=1 tx", true,  3, true,  true,  (1u << 11)},
            {"mkii      no-mux   rxOut=1 rx", true,  0, true,  false, (1u << 14)},
        }};

        constexpr quint32 kMask11_14 = (1u << 11) | (1u << 14);

        for (const auto& tc : cases) {
            P2CodecOrionMkII codec;
            CodecContext ctx;
            ctx.p2AlexRxAnt = 1;   // ANT1 — doesn't affect bits 11/14
            ctx.p2AlexTxAnt = 1;
            ctx.rxOnlyAnt   = tc.rxOnly;
            ctx.rxOut       = tc.rxOut;
            ctx.mox         = tc.mox;
            ctx.mkiiBpf     = tc.mkiiBpf;

            quint8 buf[1444] = {};
            codec.composeCmdHighPriority(ctx, buf);

            const quint32 alex0 =
                (quint32(buf[1432]) << 24) | (quint32(buf[1433]) << 16) |
                (quint32(buf[1434]) <<  8) |  quint32(buf[1435]);

            const quint32 masked = alex0 & kMask11_14;
            if (masked != tc.expectedBits11_14) {
                qWarning("alex0_mkiibpf_relay_split: case '%s' failed: got 0x%08x expected 0x%08x",
                         tc.name, masked, tc.expectedBits11_14);
            }
            QCOMPARE(masked, tc.expectedBits11_14);
        }
    }

    // Thetis parity: Alex0/Alex1 antenna bits per netInterface.c:479-485
    //   _ANT_1 → bit 24; _ANT_2 → bit 25; _ANT_3 → bit 26
    // [v2.10.3.13 @501e3f5]
    //
    // Phase 3P-I-a T6: locks independent RX/TX antenna encoding so
    // T7's setAntennaRouting split can't regress the wire bits.
    void buildAlex_rxAnt_txAnt_independent_bits() {
        P2CodecOrionMkII codec;
        CodecContext ctx;

        // RX=ANT2, TX=ANT3 — asymmetric to catch accidental coupling.
        ctx.p2AlexRxAnt = 2;
        ctx.p2AlexTxAnt = 3;

        quint8 buf[1444] = {};
        codec.composeCmdHighPriority(ctx, buf);

        // Alex0 (bytes 1432-1435 big-endian) carries rxAnt in bits 24-26.
        // Big-endian: bit 24 is the LSB of byte 1432; bit 25 is next; bit 26 next.
        // After writeBE32: byte[1432] = (reg >> 24) & 0xff
        //   → bit 24 of reg  = bit 0 of byte[1432]
        //   → bit 25 of reg  = bit 1 of byte[1432]
        //   → bit 26 of reg  = bit 2 of byte[1432]
        QVERIFY(!(buf[1432] & 0x01));   // Alex0 bit 24 clear — not ANT1
        QVERIFY( (buf[1432] & 0x02));   // Alex0 bit 25 set   —     ANT2
        QVERIFY(!(buf[1432] & 0x04));   // Alex0 bit 26 clear — not ANT3

        // Alex1 (bytes 1428-1431 big-endian) carries txAnt in bits 24-26.
        // Same byte-mapping: bit N of reg → bit (N-24) of byte[1428].
        QVERIFY(!(buf[1428] & 0x01));   // Alex1 bit 24 clear — not ANT1
        QVERIFY(!(buf[1428] & 0x02));   // Alex1 bit 25 clear — not ANT2
        QVERIFY( (buf[1428] & 0x04));   // Alex1 bit 26 set   —     ANT3
    }

    // HermesC10 (ANAN-G2E) uses DDC0 for RX1 on P2 (joins Hermes + HermesII family).
    // From Thetis console.cs:8607-8620 [v2.10.3.15] //N1GP G2E added (HermesC10) —
    // case HPSDRHW.HermesC10: rx1 = 0 (same branch as Hermes + HermesII, DDC0).
    void hermesC10_primaryRxDdc_isDDC0()
    {
        // HermesC10 must return DDC0 (index 0) — same as Hermes and HermesII.
        QCOMPARE(P2RadioConnection::primaryRxDdcForBoard(HPSDRHW::HermesC10), 0);
        // Regression: Hermes and HermesII also return 0.
        QCOMPARE(P2RadioConnection::primaryRxDdcForBoard(HPSDRHW::Hermes),    0);
        QCOMPARE(P2RadioConnection::primaryRxDdcForBoard(HPSDRHW::HermesII),  0);
        // Regression: Saturn (2-ADC boards) still returns 2.
        QCOMPARE(P2RadioConnection::primaryRxDdcForBoard(HPSDRHW::Saturn),    2);
        QCOMPARE(P2RadioConnection::primaryRxDdcForBoard(HPSDRHW::OrionMKII), 2);
    }

    // ── Codex PR #293 P1: the synchronized DDC belongs only in syncEnable ────
    //
    // Upstream puts DDC1 in SyncEnable and NEVER in DDCEnable when it is the
    // synchronized leg of a pair. From Thetis console.cs:8266-8268
    // [v2.10.3.15], transmitting with PureSignal on:
    //     DDCEnable  = DDC0 + DDC2;
    //     SyncEnable = DDC1;
    // and console.cs:8289-8291 [v2.10.3.15], transmitting with diversity and
    // no PureSignal:
    //     DDCEnable  = DDC0;
    //     SyncEnable = DDC1;
    //
    // Setting bit 1 in ddcEnable additionally asks the radio to run DDC1 as
    // an independent stream, so the second leg arrives both on its own and
    // folded into DDC0's synchronized packet. P2RadioConnection::processIqPacket
    // assumes the synchronized leg only ever arrives folded in.
    void ddcAssignment_puresignal_keeps_ddc1_out_of_the_enable_mask() {
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        ctx.puresignalRun = true;
        ctx.mox           = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live         = true;
        slices[0].frequencyHz  = 7100000;
        slices[0].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QVERIFY2((a.ddcEnable & 0x02) == 0,
            qPrintable(QStringLiteral("DDC1 is the synchronized leg and must not "
                "be in ddcEnable (Thetis console.cs:8267 [v2.10.3.15]: "
                "DDCEnable = DDC0 + DDC2). ddcEnable=0x%1")
                .arg(a.ddcEnable, 0, 16)));
        QVERIFY2((a.syncEnable & 0x02) != 0,
            "DDC1 must be in syncEnable (console.cs:8268 [v2.10.3.15])");
        // DDC0 (PS forward) + DDC2 (Slice A), exactly as upstream.
        QCOMPARE(a.ddcEnable, 0x05);
    }

    void ddcAssignment_diversity_keeps_ddc1_out_of_the_enable_mask() {
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        ctx.diversity = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live         = true;
        slices[0].frequencyHz  = 7100000;
        slices[0].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QVERIFY2((a.ddcEnable & 0x02) == 0,
            qPrintable(QStringLiteral("DDC1 is the synchronized leg and must not "
                "be in ddcEnable (Thetis console.cs:8290 [v2.10.3.15]: "
                "DDCEnable = DDC0). ddcEnable=0x%1")
                .arg(a.ddcEnable, 0, 16)));
        QVERIFY2((a.syncEnable & 0x02) != 0,
            "DDC1 must be in syncEnable (console.cs:8291 [v2.10.3.15])");
        // DDC0 alone: the diversity branch migrates stream 0 off DDC2.
        QCOMPARE(a.ddcEnable, 0x01);
    }
};

QTEST_APPLESS_MAIN(TestP2CodecOrionMkII)
#include "tst_p2_codec_orionmkii.moc"
