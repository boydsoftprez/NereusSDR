// no-port-check: NereusSDR-original unit-test file.  All Thetis source cites
// are in TransmitModel.h/cpp.
// =================================================================
// tests/tst_transmit_model_cfc_cpdr_cessb_phrot.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel CFC + CPDR + CESSB + Phase Rotator
// properties added in Phase 3M-3a-ii Batch 2.
//
//   Phase Rotator (4 — was deferred from 3M-3a-i §7.1):
//     phaseRotatorEnabled  (bool)  — CFCPhaseRotatorEnabled
//     phaseReverseEnabled  (bool)  — CFCPhaseReverseEnabled
//     phaseRotatorFreqHz   (int)   — CFCPhaseRotatorFreq, default 338
//     phaseRotatorStages   (int)   — CFCPhaseRotatorStages, default 8
//
//   CFC (8):
//     cfcEnabled              (bool)            — CFCEnabled
//     cfcPostEqEnabled        (bool)            — CFCPostEqEnabled
//     cfcPrecompDb            (int)             — CFCPreComp scalar
//     cfcPostEqGainDb         (int)             — CFCPostEqGain scalar
//     cfcEqFreqHz[10]         (array<int,10>)   — CFCEqFreq0..9
//     cfcCompressionDb[10]    (array<int,10>)   — CFCPreComp0..9 (per-band G[])
//     cfcPostEqBandGainDb[10] (array<int,10>)   — CFCPostEqGain0..9 (per-band E[])
//     cfcParaEqData           (QString)         — CFCParaEQData (opaque blob)
//
//   CPDR (2):
//     cpdrOn       (bool)  — global console state, NOT in TXProfile
//     cpdrLevelDb  (int)   — CompanderLevel
//
//   CESSB (1):
//     cessbOn (bool)       — CESSB_On
//
// Total: 15 new properties (14 TXProfile + 1 global cpdrOn).
//
// Defaults from Thetis database.cs:4724-4768 [v2.10.3.13] (TXProfile factory)
// except cpdrOn which comes from console.cs:36430 [v2.10.3.13].
//
// Range clamps from Thetis Designer (setup.Designer.cs and
// frmCFCConfig.Designer.cs [v2.10.3.13]).
//
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");
static const QString kMacB = QStringLiteral("ff:ee:dd:cc:bb:aa");

class TstTransmitModelCfcCpdrCessbPhrot : public QObject {
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

    // Phase Rotator defaults — database.cs:4726-4730 [v2.10.3.13]
    void default_phaseRotatorEnabled_isFalse()
    {
        TransmitModel t;
        QCOMPARE(t.phaseRotatorEnabled(), false);
    }

    void default_phaseReverseEnabled_isFalse()
    {
        TransmitModel t;
        QCOMPARE(t.phaseReverseEnabled(), false);
    }

    void default_phaseRotatorFreqHz_is338()
    {
        // database.cs:4729 [v2.10.3.13]: dr["CFCPhaseRotatorFreq"] = 338;
        TransmitModel t;
        QCOMPARE(t.phaseRotatorFreqHz(), 338);
    }

    void default_phaseRotatorStages_is8()
    {
        // database.cs:4730 [v2.10.3.13]: dr["CFCPhaseRotatorStages"] = 8;
        TransmitModel t;
        QCOMPARE(t.phaseRotatorStages(), 8);
    }

    // CFC scalar defaults — database.cs:4724-4733 [v2.10.3.13]
    void default_cfcEnabled_isFalse()
    {
        TransmitModel t;
        QCOMPARE(t.cfcEnabled(), false);
    }

    void default_cfcPostEqEnabled_isFalse()
    {
        TransmitModel t;
        QCOMPARE(t.cfcPostEqEnabled(), false);
    }

    void default_cfcPrecompDb_is0()
    {
        TransmitModel t;
        QCOMPARE(t.cfcPrecompDb(), 0);
    }

    void default_cfcPostEqGainDb_is0()
    {
        TransmitModel t;
        QCOMPARE(t.cfcPostEqGainDb(), 0);
    }

    // CFC per-band defaults
    void default_cfcEqFreq_matchesThetisFactory()
    {
        // database.cs:4757-4766 [v2.10.3.13]:
        //   CFCEqFreq0..9 = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000}
        TransmitModel t;
        QCOMPARE(t.cfcEqFreq(0),     0);
        QCOMPARE(t.cfcEqFreq(1),   125);
        QCOMPARE(t.cfcEqFreq(2),   250);
        QCOMPARE(t.cfcEqFreq(3),   500);
        QCOMPARE(t.cfcEqFreq(4),  1000);
        QCOMPARE(t.cfcEqFreq(5),  2000);
        QCOMPARE(t.cfcEqFreq(6),  3000);
        QCOMPARE(t.cfcEqFreq(7),  4000);
        QCOMPARE(t.cfcEqFreq(8),  5000);
        QCOMPARE(t.cfcEqFreq(9), 10000);
    }

    void default_cfcCompression_allFive()
    {
        // database.cs:4735-4744 [v2.10.3.13]: CFCPreComp0..9 all 5.
        TransmitModel t;
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(t.cfcCompression(i), 5);
        }
    }

    void default_cfcPostEqBandGain_allZero()
    {
        // database.cs:4746-4755 [v2.10.3.13]: CFCPostEqGain0..9 all 0.
        TransmitModel t;
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(t.cfcPostEqBandGain(i), 0);
        }
    }

    void default_cfcParaEqData_isEmpty()
    {
        // database.cs:4768 [v2.10.3.13]: dr["CFCParaEQData"] = "";
        TransmitModel t;
        QCOMPARE(t.cfcParaEqData(), QString());
    }

    // CPDR defaults
    void default_cpdrOn_isFalse()
    {
        // WDSP TXA.c:253 [v2.10.3.13]: txa[ch].compressor.run = 0.
        TransmitModel t;
        QCOMPARE(t.cpdrOn(), false);
    }

    void default_cpdrLevelDb_is2()
    {
        // database.cs:4580 [v2.10.3.13]: dr["CompanderLevel"] = 2;
        TransmitModel t;
        QCOMPARE(t.cpdrLevelDb(), 2);
    }

    // CESSB default
    void default_cessbOn_isFalse()
    {
        // database.cs:4689 [v2.10.3.13]: dr["CESSB_On"] = false;
        TransmitModel t;
        QCOMPARE(t.cessbOn(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §2  SIGNAL EMISSION + IDEMPOTENCY
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setPhaseRotatorEnabled_emitsChangedOnce()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::phaseRotatorEnabledChanged);
        t.setPhaseRotatorEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setPhaseRotatorEnabled_idempotent_noEmit()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::phaseRotatorEnabledChanged);
        t.setPhaseRotatorEnabled(false);  // already false
        QCOMPARE(spy.count(), 0);
    }

    void setCfcEnabled_emitsChangedOnce()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcEnabledChanged);
        t.setCfcEnabled(true);
        QCOMPARE(spy.count(), 1);
    }

    void setCfcEnabled_idempotent_noEmit()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcEnabledChanged);
        t.setCfcEnabled(false);  // already false
        QCOMPARE(spy.count(), 0);
    }

    void setCpdrOn_emitsChangedOnce()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cpdrOnChanged);
        t.setCpdrOn(true);
        QCOMPARE(spy.count(), 1);
    }

    void setCpdrLevelDb_change_emits()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cpdrLevelDbChanged);
        t.setCpdrLevelDb(10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 10);
    }

    void setCpdrLevelDb_idempotent_noEmit()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cpdrLevelDbChanged);
        t.setCpdrLevelDb(2);  // already 2
        QCOMPARE(spy.count(), 0);
    }

    void setCessbOn_emitsChangedOnce()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cessbOnChanged);
        t.setCessbOn(true);
        QCOMPARE(spy.count(), 1);
    }

    void setCfcEqFreq_changeOneIndex_emitsIndex()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcEqFreqChanged);
        t.setCfcEqFreq(3, 750);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
        QCOMPARE(spy.at(0).at(1).toInt(), 750);
    }

    void setCfcCompression_changeOneIndex_emitsIndex()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcCompressionChanged);
        t.setCfcCompression(0, 10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).toInt(), 10);
    }

    void setCfcPostEqBandGain_changeOneIndex_emitsIndex()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcPostEqBandGainChanged);
        t.setCfcPostEqBandGain(5, -8);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
        QCOMPARE(spy.at(0).at(1).toInt(), -8);
    }

    void setCfcParaEqData_change_emits()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcParaEqDataChanged);
        t.setCfcParaEqData(QStringLiteral("foo"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("foo"));
    }

    void setCfcParaEqData_idempotent_noEmit()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcParaEqDataChanged);
        t.setCfcParaEqData(QString());  // already empty
        QCOMPARE(spy.count(), 0);
    }

    // ── A paired curve: a write it already holds is no change to it ──

    // A ten-band paired curve with fractional pre-comp and compression, so
    // a write of the rounded value would snap them if it re-encoded.
    static QString fractionalPairedCurve()
    {
        CfcProfile::Profile p;
        for (int i = 0; i < 10; ++i) {
            p.f.push_back(200.0 * (i + 1));
            p.postF.push_back(200.0 * (i + 1));
            p.g.push_back(3.4 + i);
            p.e.push_back(-2.0);
            p.qg.push_back(2.0);
            p.qe.push_back(2.0);
        }
        p.minHz = p.postMinHz = 200.0;
        p.maxHz = p.postMaxHz = 2000.0;
        p.precompDb = 6.4;
        p.postEqGainDb = -3.0;
        return CfcProfile::encode(p);
    }

    void pairedCurve_precompOfItsRoundedValue_keepsTheCurve()
    {
        TransmitModel t;
        const QString curve = fractionalPairedCurve();
        QVERIFY(!curve.isEmpty());
        t.setCfcParaEqData(curve);
        QCOMPARE(t.cfcPrecompDb(), 6);
        QSignalSpy blobSpy(&t, &TransmitModel::cfcParaEqDataChanged);
        t.setCfcPrecompDb(6);
        QCOMPARE(t.cfcParaEqData(), curve);
        QCOMPARE(blobSpy.count(), 0);
    }

    void pairedCurve_compressionJsonOfItsRoundedValues_keepsTheCurve()
    {
        TransmitModel t;
        const QString curve = fractionalPairedCurve();
        QVERIFY(!curve.isEmpty());
        t.setCfcParaEqData(curve);
        const QString rounded = t.cfcCompressionJson();
        QCOMPARE(t.cfcCompression(0), 3);
        QSignalSpy blobSpy(&t, &TransmitModel::cfcParaEqDataChanged);
        t.setCfcCompressionJson(rounded);
        QCOMPARE(t.cfcParaEqData(), curve);
        QCOMPARE(blobSpy.count(), 0);
    }

    // A restored Thetis profile can hold its integer pre-comp apart from
    // the curve (the legacy panel and frmCFCConfig are independent). A
    // write of the curve's rounded value is a real change to the integer:
    // it follows, and the curve stays byte for byte.
    void restoredMirrorApartFromTheCurve_followsAWriteOfTheCurvesValue()
    {
        TransmitModel t;
        const QString curve = fractionalPairedCurve();
        QVERIFY(!curve.isEmpty());
        t.beginCfcProfileRestore();
        t.setCfcParaEqData(curve);
        t.setCfcPrecompDb(3);
        t.endCfcProfileRestore();
        QCOMPARE(t.cfcPrecompDb(), 3);
        QCOMPARE(t.cfcParaEqData(), curve);

        QSignalSpy precompSpy(&t, &TransmitModel::cfcPrecompDbChanged);
        t.setCfcPrecompDb(6);
        QCOMPARE(t.cfcPrecompDb(), 6);
        QCOMPARE(precompSpy.count(), 1);
        QCOMPARE(t.cfcParaEqData(), curve);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §3  RANGE CLAMPING
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setPhaseRotatorFreqHz_aboveMax_clampsTo2000()
    {
        // setup.Designer.cs:46250-46259 [v2.10.3.13]: Maximum=2000.
        TransmitModel t;
        t.setPhaseRotatorFreqHz(99999);
        QCOMPARE(t.phaseRotatorFreqHz(), 2000);
    }

    void setPhaseRotatorFreqHz_belowMin_clampsTo10()
    {
        // setup.Designer.cs:46255 [v2.10.3.13]: Minimum=10.
        TransmitModel t;
        t.setPhaseRotatorFreqHz(0);
        QCOMPARE(t.phaseRotatorFreqHz(), 10);
    }

    void setPhaseRotatorStages_aboveMax_clampsTo16()
    {
        // setup.Designer.cs:46209 [v2.10.3.13]: Maximum=16.
        TransmitModel t;
        t.setPhaseRotatorStages(99);
        QCOMPARE(t.phaseRotatorStages(), 16);
    }

    void setPhaseRotatorStages_belowMin_clampsTo2()
    {
        // setup.Designer.cs:46214 [v2.10.3.13]: Minimum=2.
        TransmitModel t;
        t.setPhaseRotatorStages(0);
        QCOMPARE(t.phaseRotatorStages(), 2);
    }

    void setCfcPrecompDb_aboveMax_clampsTo16()
    {
        // frmCFCConfig.Designer.cs:408 [v2.10.3.13]: Maximum=16.
        TransmitModel t;
        t.setCfcPrecompDb(999);
        QCOMPARE(t.cfcPrecompDb(), 16);
    }

    void setCfcPrecompDb_belowMin_clampsTo0()
    {
        // frmCFCConfig.Designer.cs:413 [v2.10.3.13]: Minimum=0.
        TransmitModel t;
        t.setCfcPrecompDb(-50);
        QCOMPARE(t.cfcPrecompDb(), 0);
    }

    void setCfcPostEqGainDb_aboveMax_clampsTo24()
    {
        // frmCFCConfig.Designer.cs:337 [v2.10.3.13]: Maximum=24.
        TransmitModel t;
        t.setCfcPostEqGainDb(999);
        QCOMPARE(t.cfcPostEqGainDb(), 24);
    }

    void setCfcPostEqGainDb_belowMin_clampsToMinus24()
    {
        // frmCFCConfig.Designer.cs:342 [v2.10.3.13]: Minimum=-24
        // (decimal sign-bit encoding).
        TransmitModel t;
        t.setCfcPostEqGainDb(-999);
        QCOMPARE(t.cfcPostEqGainDb(), -24);
    }

    void setCfcEqFreq_aboveMax_clampsTo20000()
    {
        // frmCFCConfig.Designer.cs:267 [v2.10.3.13]: Maximum=20000.
        TransmitModel t;
        t.setCfcEqFreq(0, 99999);
        QCOMPARE(t.cfcEqFreq(0), 20000);
    }

    void setCfcCompression_aboveMax_clampsTo16()
    {
        // frmCFCConfig.Designer.cs:217 [v2.10.3.13]: Maximum=16.
        TransmitModel t;
        t.setCfcCompression(0, 999);
        QCOMPARE(t.cfcCompression(0), 16);
    }

    void setCfcPostEqBandGain_belowMin_clampsToMinus24()
    {
        // frmCFCConfig.Designer.cs:569 [v2.10.3.13]: Minimum=-24.
        TransmitModel t;
        t.setCfcPostEqBandGain(0, -999);
        QCOMPARE(t.cfcPostEqBandGain(0), -24);
    }

    void setCpdrLevelDb_aboveMax_clampsTo20()
    {
        // console.Designer.cs:6042 [v2.10.3.13]: ptbCPDR.Maximum=20.
        TransmitModel t;
        t.setCpdrLevelDb(999);
        QCOMPARE(t.cpdrLevelDb(), 20);
    }

    void setCpdrLevelDb_belowMin_clampsTo0()
    {
        // console.Designer.cs:6043 [v2.10.3.13]: ptbCPDR.Minimum=0.
        TransmitModel t;
        t.setCpdrLevelDb(-99);
        QCOMPARE(t.cpdrLevelDb(), 0);
    }

    // Out-of-range index for indexed arrays
    void setCfcEqFreq_outOfIndex_isNoOp()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcEqFreqChanged);
        t.setCfcEqFreq(-1, 100);
        t.setCfcEqFreq(10, 100);
        QCOMPARE(spy.count(), 0);
    }

    void cfcEqFreq_readOutOfIndex_returnsZero()
    {
        TransmitModel t;
        QCOMPARE(t.cfcEqFreq(-1), 0);
        QCOMPARE(t.cfcEqFreq(10), 0);
        QCOMPARE(t.cfcEqFreq(99), 0);
    }

    void setCfcCompression_outOfIndex_isNoOp()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcCompressionChanged);
        t.setCfcCompression(-1, 5);
        t.setCfcCompression(10, 5);
        QCOMPARE(spy.count(), 0);
    }

    void cfcCompression_readOutOfIndex_returnsZero()
    {
        TransmitModel t;
        QCOMPARE(t.cfcCompression(-1), 0);
        QCOMPARE(t.cfcCompression(10), 0);
    }

    void setCfcPostEqBandGain_outOfIndex_isNoOp()
    {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::cfcPostEqBandGainChanged);
        t.setCfcPostEqBandGain(-1, 5);
        t.setCfcPostEqBandGain(10, 5);
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §4  PERSISTENCE ROUND-TRIP (per-MAC)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void roundtrip_phaseRotatorEnabled()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setPhaseRotatorEnabled(true);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.phaseRotatorEnabled(), true);
    }

    void roundtrip_phaseRotatorFreqHz()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setPhaseRotatorFreqHz(500);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.phaseRotatorFreqHz(), 500);
    }

    void roundtrip_phaseRotatorStages()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setPhaseRotatorStages(12);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.phaseRotatorStages(), 12);
    }

    void roundtrip_cfcEnabled()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcEnabled(true);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcEnabled(), true);
    }

    void roundtrip_cfcPostEqEnabled()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcPostEqEnabled(true);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcPostEqEnabled(), true);
    }

    void roundtrip_cfcPrecompDb()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcPrecompDb(8);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcPrecompDb(), 8);
    }

    void roundtrip_cfcPostEqGainDb()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcPostEqGainDb(-12);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcPostEqGainDb(), -12);
    }

    void roundtrip_cfcEqFreq_singleIndex()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcEqFreq(4, 1500);  // change band 4 freq from default 1000 to 1500
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcEqFreq(4), 1500);
        // Other freqs keep Thetis factory shape
        QCOMPARE(t2.cfcEqFreq(0),     0);
        QCOMPARE(t2.cfcEqFreq(9), 10000);
    }

    void roundtrip_cfcCompression_singleIndex()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcCompression(7, 12);  // change band 7 from default 5 to 12
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcCompression(7), 12);
        QCOMPARE(t2.cfcCompression(0),  5);  // unchanged
    }

    void roundtrip_cfcPostEqBandGain_singleIndex()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcPostEqBandGain(2, -6);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcPostEqBandGain(2), -6);
        QCOMPARE(t2.cfcPostEqBandGain(0),  0);  // unchanged
    }

    void roundtrip_cfcParaEqData()
    {
        const QString blob = QStringLiteral("opaque-thetis-blob-v1|0.5|1.0|2.0");
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCfcParaEqData(blob);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cfcParaEqData(), blob);
    }

    void roundtrip_cpdrOn()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCpdrOn(true);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cpdrOn(), true);
    }

    void roundtrip_cpdrLevelDb()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCpdrLevelDb(15);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cpdrLevelDb(), 15);
    }

    void roundtrip_cessbOn()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCessbOn(true);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMacA);
        QCOMPARE(t2.cessbOn(), true);
    }

    // Verify that the global cpdrOn key sits at the expected
    // hardware/<mac>/tx/cpdr/on path (NOT in a profile bundle).
    void cpdrOn_persistsAtExpectedPath()
    {
        TransmitModel t;
        t.loadFromSettings(kMacA);
        t.setCpdrOn(true);
        const QString key =
            QStringLiteral("hardware/%1/tx/cpdr/on").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(key).toString(),
                 QStringLiteral("True"));
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // §5  MULTI-MAC ISOLATION (no bleed-through)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void multiMac_phaseRotatorFreqHz_isolated()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setPhaseRotatorFreqHz(400);
        }
        {
            TransmitModel t;
            t.loadFromSettings(kMacB);
            t.setPhaseRotatorFreqHz(800);
        }
        TransmitModel ta, tb;
        ta.loadFromSettings(kMacA);
        tb.loadFromSettings(kMacB);
        QCOMPARE(ta.phaseRotatorFreqHz(), 400);
        QCOMPARE(tb.phaseRotatorFreqHz(), 800);
    }

    void multiMac_cpdrLevelDb_isolated()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMacA);
            t.setCpdrLevelDb(7);
        }
        {
            TransmitModel t;
            t.loadFromSettings(kMacB);
            t.setCpdrLevelDb(13);
        }
        TransmitModel ta, tb;
        ta.loadFromSettings(kMacA);
        tb.loadFromSettings(kMacB);
        QCOMPARE(ta.cpdrLevelDb(), 7);
        QCOMPARE(tb.cpdrLevelDb(), 13);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelCfcCpdrCessbPhrot)
#include "tst_transmit_model_cfc_cpdr_cessb_phrot.moc"
