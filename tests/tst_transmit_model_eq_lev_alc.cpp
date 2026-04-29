// no-port-check: NereusSDR-original unit-test file.  All Thetis source cites
// are in TransmitModel.h/cpp.
// =================================================================
// tests/tst_transmit_model_eq_lev_alc.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel TX EQ + Leveler + ALC properties added in
// Phase 3M-3a-i Task C:
//
//   TX EQ (TXProfile, 23 keys):
//     txEqEnabled (bool)         — TXEQEnabled
//     txEqNumBands (int, RO=10)  — TXEQNumBands
//     txEqPreamp (int dB)        — TXEQPreamp
//     txEqBand[i] / setTxEqBand(i, dB) — TXEQ1..TXEQ10
//     txEqFreq[i] / setTxEqFreq(i, hz) — TxEqFreq1..TxEqFreq10
//
//   TX Leveler (TXProfile, 3 keys):
//     txLevelerOn (bool)        — Lev_On
//     txLevelerMaxGain (int dB) — Lev_MaxGain
//     txLevelerDecay (int ms)   — Lev_Decay
//
//   TX ALC (TXProfile, 2 keys):
//     txAlcMaxGain (int dB)     — ALC_MaximumGain
//     txAlcDecay (int ms)       — ALC_Decay
//
//   TX EQ globals (NOT in TXProfile, 4 keys under hardware/<mac>/tx/):
//     txEqNc (int)              — eq/nc       default 2048
//     txEqMp (bool)             — eq/mp       default false
//     txEqCtfmode (int)         — eq/ctfmode  default 0
//     txEqWintype (int)         — eq/wintype  default 0
//
// Total: 28 new properties (23 TXProfile + 5 stand-alone + 4 globals).
//
// Defaults from Thetis database.cs:4552-4594 [v2.10.3.13] (TXProfile schema)
// and WDSP TXA.c:111-128 [v2.10.3.13] (create_eqp G[]/F[] vectors).
//
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");
static const QString kMacB = QStringLiteral("ff:ee:dd:cc:bb:aa");

class TstTransmitModelEqLevAlc : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §1  DEFAULT VALUES (constructor — no MAC scope yet)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void default_txEqEnabled_isFalse()
    {
        // Thetis database.cs:4553 [v2.10.3.13]: dr["TXEQEnabled"] = false;
        TransmitModel t;
        QCOMPARE(t.txEqEnabled(), false);
    }

    void default_txEqNumBands_is10()
    {
        // Thetis database.cs:4552 [v2.10.3.13]: dr["TXEQNumBands"] = 10;
        TransmitModel t;
        QCOMPARE(t.txEqNumBands(), 10);
    }

    void default_txEqPreamp_isZero()
    {
        // Thetis database.cs:4554 [v2.10.3.13]: dr["TXEQPreamp"] = 0;
        TransmitModel t;
        QCOMPARE(t.txEqPreamp(), 0);
    }

    void default_txEqBand_matchesWdspDefaultGShape()
    {
        // From Thetis wdsp/TXA.c:113 [v2.10.3.13]:
        //   default_G[11] = {0.0, -12.0, -12.0, -12.0, -1.0, +1.0, +4.0, +9.0, +12.0, -10.0, -10.0};
        // default_G[1..10] -> txEqBand[0..9].
        TransmitModel t;
        QCOMPARE(t.txEqBand(0),  -12);
        QCOMPARE(t.txEqBand(1),  -12);
        QCOMPARE(t.txEqBand(2),  -12);
        QCOMPARE(t.txEqBand(3),   -1);
        QCOMPARE(t.txEqBand(4),    1);
        QCOMPARE(t.txEqBand(5),    4);
        QCOMPARE(t.txEqBand(6),    9);
        QCOMPARE(t.txEqBand(7),   12);
        QCOMPARE(t.txEqBand(8),  -10);
        QCOMPARE(t.txEqBand(9),  -10);
    }

    void default_txEqFreq_matchesWdspDefaultFShape()
    {
        // From Thetis wdsp/TXA.c:112 [v2.10.3.13]:
        //   default_F[11] = {0.0, 32.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
        // default_F[1..10] -> txEqFreq[0..9] (Hz).
        TransmitModel t;
        QCOMPARE(t.txEqFreq(0),    32);
        QCOMPARE(t.txEqFreq(1),    63);
        QCOMPARE(t.txEqFreq(2),   125);
        QCOMPARE(t.txEqFreq(3),   250);
        QCOMPARE(t.txEqFreq(4),   500);
        QCOMPARE(t.txEqFreq(5),  1000);
        QCOMPARE(t.txEqFreq(6),  2000);
        QCOMPARE(t.txEqFreq(7),  4000);
        QCOMPARE(t.txEqFreq(8),  8000);
        QCOMPARE(t.txEqFreq(9), 16000);
    }

    void default_txLevelerOn_isTrue()
    {
        // Thetis database.cs:4584 [v2.10.3.13]: dr["Lev_On"] = true;
        TransmitModel t;
        QCOMPARE(t.txLevelerOn(), true);
    }

    void default_txLevelerMaxGain_is15()
    {
        // Thetis database.cs:4586 [v2.10.3.13]: dr["Lev_MaxGain"] = 15;
        TransmitModel t;
        QCOMPARE(t.txLevelerMaxGain(), 15);
    }

    void default_txLevelerDecay_is100()
    {
        // Thetis database.cs:4588 [v2.10.3.13]: dr["Lev_Decay"] = 100;
        TransmitModel t;
        QCOMPARE(t.txLevelerDecay(), 100);
    }

    void default_txAlcMaxGain_is3()
    {
        // Thetis database.cs:4592 [v2.10.3.13]: dr["ALC_MaximumGain"] = 3;
        TransmitModel t;
        QCOMPARE(t.txAlcMaxGain(), 3);
    }

    void default_txAlcDecay_is10()
    {
        // Thetis database.cs:4594 [v2.10.3.13]: dr["ALC_Decay"] = 10;
        TransmitModel t;
        QCOMPARE(t.txAlcDecay(), 10);
    }

    void default_txEqNc_is2048()
    {
        // From Thetis wdsp/TXA.c:118 [v2.10.3.13]:
        //   max(2048, ch[].dsp_size) — at standard 2048 dsp_size, this is 2048.
        TransmitModel t;
        QCOMPARE(t.txEqNc(), 2048);
    }

    void default_txEqMp_isFalse()
    {
        // From Thetis wdsp/TXA.c:119 [v2.10.3.13]: minimum-phase flag = 0.
        TransmitModel t;
        QCOMPARE(t.txEqMp(), false);
    }

    void default_txEqCtfmode_isZero()
    {
        // From Thetis wdsp/TXA.c:125 [v2.10.3.13]: cutoff mode = 0.
        TransmitModel t;
        QCOMPARE(t.txEqCtfmode(), 0);
    }

    void default_txEqWintype_isZero()
    {
        // From Thetis wdsp/TXA.c:126 [v2.10.3.13]: window type = 0.
        TransmitModel t;
        QCOMPARE(t.txEqWintype(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §2  SIGNAL EMISSION
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setTxEqEnabled_emitsChangedOnce()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txEqEnabledChanged);
        t.setTxEqEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setTxEqEnabled_idempotent_noEmit()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txEqEnabledChanged);
        t.setTxEqEnabled(false);  // already false — idempotent
        QCOMPARE(spy.count(), 0);
    }

    void setTxLevelerOn_idempotent_noEmit()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txLevelerOnChanged);
        t.setTxLevelerOn(true);   // already true — idempotent
        QCOMPARE(spy.count(), 0);
    }

    void setTxLevelerMaxGain_change_emitsClampedValue()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txLevelerMaxGainChanged);
        t.setTxLevelerMaxGain(10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 10);
    }

    void setTxAlcMaxGain_change_emits()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txAlcMaxGainChanged);
        t.setTxAlcMaxGain(5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
    }

    void setTxEqBand_changeOneIndex_emitsBandIndex()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txEqBandChanged);
        t.setTxEqBand(0, -8);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).toInt(), -8);
    }

    void setTxEqFreq_changeOneIndex_emitsFreqIndex()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txEqFreqChanged);
        t.setTxEqFreq(0, 50);  // change from default 32
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).toInt(), 50);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §3  RANGE CLAMPING
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setTxLevelerMaxGain_aboveMax_clampsTo20()
    {
        // Thetis Designer setup.Designer.cs:38718 [v2.10.3.13]: Maximum=20.
        TransmitModel t;
        t.setTxLevelerMaxGain(999);
        QCOMPARE(t.txLevelerMaxGain(), 20);
    }

    void setTxLevelerMaxGain_belowMin_clampsTo0()
    {
        // Thetis Designer setup.Designer.cs:38723 [v2.10.3.13]: Minimum=0.
        TransmitModel t;
        t.setTxLevelerMaxGain(-50);
        QCOMPARE(t.txLevelerMaxGain(), 0);
    }

    void setTxLevelerDecay_aboveMax_clampsTo5000()
    {
        // Thetis Designer setup.Designer.cs:38750 [v2.10.3.13]: Maximum=5000.
        TransmitModel t;
        t.setTxLevelerDecay(99999);
        QCOMPARE(t.txLevelerDecay(), 5000);
    }

    void setTxLevelerDecay_belowMin_clampsTo1()
    {
        // Thetis Designer setup.Designer.cs:38755 [v2.10.3.13]: Minimum=1.
        TransmitModel t;
        t.setTxLevelerDecay(0);
        QCOMPARE(t.txLevelerDecay(), 1);
    }

    void setTxAlcMaxGain_aboveMax_clampsTo120()
    {
        // Thetis Designer setup.Designer.cs:38814 [v2.10.3.13]: Maximum=120.
        TransmitModel t;
        t.setTxAlcMaxGain(999);
        QCOMPARE(t.txAlcMaxGain(), 120);
    }

    void setTxAlcDecay_aboveMax_clampsTo50()
    {
        // Thetis Designer setup.Designer.cs:38845 [v2.10.3.13]: Maximum=50.
        TransmitModel t;
        t.setTxAlcDecay(999);
        QCOMPARE(t.txAlcDecay(), 50);
    }

    void setTxEqPreamp_aboveMax_clamps()
    {
        // NereusSDR clamp [-12, 15] (matches Thetis spinbox precedent —
        // udTXEQ pre-amp slider; tighter than the unbounded TXEQPreamp int).
        TransmitModel t;
        t.setTxEqPreamp(999);
        QCOMPARE(t.txEqPreamp(), 15);
    }

    void setTxEqBand_outOfIndex_isNoOp()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txEqBandChanged);
        t.setTxEqBand(-1, 5);   // bad index
        t.setTxEqBand(10, 5);   // bad index
        QCOMPARE(spy.count(), 0);
    }

    void setTxEqFreq_outOfIndex_isNoOp()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::txEqFreqChanged);
        t.setTxEqFreq(-1, 100);
        t.setTxEqFreq(10, 100);
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §4  PERSISTENCE ROUND-TRIP (per-MAC)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void roundtrip_txEqEnabled()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxEqEnabled(true);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txEqEnabled(), true);
    }

    void roundtrip_txLevelerOn_canFlipToFalse()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxLevelerOn(false);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txLevelerOn(), false);
    }

    void roundtrip_txLevelerMaxGain()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxLevelerMaxGain(8);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txLevelerMaxGain(), 8);
    }

    void roundtrip_txAlcDecay()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxAlcDecay(25);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txAlcDecay(), 25);
    }

    void roundtrip_txEqBand_singleIndex()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxEqBand(5, 6);  // change band index 5 from default 4 to 6
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txEqBand(5), 6);
        // Other bands keep default shape
        QCOMPARE(t2.txEqBand(0), -12);
    }

    void roundtrip_txEqFreq_singleIndex()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxEqFreq(0, 80);  // change band 0 freq from default 32 to 80
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txEqFreq(0), 80);
        // Other freqs keep WDSP default
        QCOMPARE(t2.txEqFreq(1), 63);
    }

    void roundtrip_txEqGlobals()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxEqNc(4096);
            t.setTxEqMp(true);
            t.setTxEqCtfmode(1);
            t.setTxEqWintype(2);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.txEqNc(), 4096);
        QCOMPARE(t2.txEqMp(), true);
        QCOMPARE(t2.txEqCtfmode(), 1);
        QCOMPARE(t2.txEqWintype(), 2);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §5  MULTI-MAC ISOLATION (no bleed-through)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void multiMac_levelerMaxGain_isolated()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setTxLevelerMaxGain(8);
        }
        {
            TransmitModel t;
            t.loadFromSettings(kMacB);
            t.setTxLevelerMaxGain(12);
        }
        TransmitModel ta, tb;
        ta.loadFromSettings(kMacA);
        tb.loadFromSettings(kMacB);
        QCOMPARE(ta.txLevelerMaxGain(), 8);
        QCOMPARE(tb.txLevelerMaxGain(), 12);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelEqLevAlc)
#include "tst_transmit_model_eq_lev_alc.moc"
