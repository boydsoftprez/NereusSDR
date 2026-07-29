// =================================================================
// tests/tst_slice_model_phase3f_properties.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Tasks 4-11: verify SliceModel gains 7 new
// Q_PROPERTYs per
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceModelPhase3FProperties : public QObject {
    Q_OBJECT

private slots:
    // ── Task 4: sliceLetter ──────────────────────────────────────────────
    void slice_letter_default_is_A()
    {
        SliceModel slice;
        QCOMPARE(slice.sliceLetter(), QChar('A'));
    }

    // sliceLetter is DERIVED from sliceIndex, not stored, and the property is
    // CONSTANT: there is no setter and no change signal. This test previously
    // exercised a stored letter with setSliceLetter() plus a
    // sliceLetterChanged signal, and was replaced when that storage was
    // removed (SliceModel.h:479-497).
    //
    // The stored version had no production caller for its setter, so every
    // slice reported the default 'A'. Readers guarded with
    // `sliceLetter().isNull() ? QChar('A' + sliceIndex()) : ...` never took
    // the fallback, because a defaulted QChar is 'A' and not null. Slice
    // buttons therefore read A, A, A instead of A, B, C, and
    // AntennaPickerMenu mislabelled every slice.
    void slice_letter_is_derived_from_slice_index()
    {
        SliceModel slice;
        slice.setSliceIndex(1);
        QCOMPARE(slice.sliceLetter(), QChar('B'));
        slice.setSliceIndex(2);
        QCOMPARE(slice.sliceLetter(), QChar('C'));
    }

    // The regression the derivation exists to prevent: distinct slices must
    // report distinct letters without anyone having to assign them.
    void distinct_slices_report_distinct_letters()
    {
        SliceModel a, b, c;
        a.setSliceIndex(0);
        b.setSliceIndex(1);
        c.setSliceIndex(2);
        QCOMPARE(a.sliceLetter(), QChar('A'));
        QCOMPARE(b.sliceLetter(), QChar('B'));
        QCOMPARE(c.sliceLetter(), QChar('C'));
    }

    // ── Task 5: chainIndex ──────────────────────────────────────────────
    void chain_index_default_is_0()
    {
        SliceModel slice;
        QCOMPARE(slice.chainIndex(), 0);
    }

    void chain_index_setter_round_trips()
    {
        SliceModel slice;
        slice.setChainIndex(1);
        QCOMPARE(slice.chainIndex(), 1);
    }

    void chain_index_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::chainIndexChanged);
        slice.setChainIndex(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 1);
    }

    void chain_index_setter_idempotent()
    {
        SliceModel slice;
        slice.setChainIndex(1);
        QSignalSpy spy(&slice, &SliceModel::chainIndexChanged);
        slice.setChainIndex(1);  // same value
        QCOMPARE(spy.count(), 0);
    }

    // ── Task 6: ddcIndex (read-only from operator perspective; setter for codec) ──
    void ddc_index_default_is_negative_1()
    {
        SliceModel slice;
        QCOMPARE(slice.ddcIndex(), -1);  // unassigned
    }

    void ddc_index_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::ddcIndexChanged);
        slice.setDdcIndex(2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(slice.ddcIndex(), 2);
    }

    void ddc_index_setter_round_trips()
    {
        SliceModel slice;
        slice.setDdcIndex(3);
        QCOMPARE(slice.ddcIndex(), 3);
    }

    void ddc_index_setter_idempotent()
    {
        SliceModel slice;
        slice.setDdcIndex(2);
        QSignalSpy spy(&slice, &SliceModel::ddcIndexChanged);
        slice.setDdcIndex(2);  // same value
        QCOMPARE(spy.count(), 0);
    }

    // ── Task 7: sampleRateHz ────────────────────────────────────────────
    void sample_rate_default_is_192000()
    {
        SliceModel slice;
        QCOMPARE(slice.sampleRateHz(), 192000);
    }

    void sample_rate_setter_round_trips()
    {
        SliceModel slice;
        slice.setSampleRateHz(1536000);
        QCOMPARE(slice.sampleRateHz(), 1536000);
    }

    void sample_rate_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::sampleRateHzChanged);
        slice.setSampleRateHz(384000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 384000);
    }

    void sample_rate_setter_idempotent()
    {
        SliceModel slice;
        slice.setSampleRateHz(96000);
        QSignalSpy spy(&slice, &SliceModel::sampleRateHzChanged);
        slice.setSampleRateHz(96000);
        QCOMPARE(spy.count(), 0);
    }
    // ── Task 8: diversityEnabled ────────────────────────────────────────
    void diversity_enabled_default_is_false()
    {
        SliceModel slice;
        QCOMPARE(slice.diversityEnabled(), false);
    }

    void diversity_enabled_setter_round_trips()
    {
        SliceModel slice;
        slice.setDiversityEnabled(true);
        QCOMPARE(slice.diversityEnabled(), true);
    }

    void diversity_enabled_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::diversityEnabledChanged);
        slice.setDiversityEnabled(true);
        QCOMPARE(spy.count(), 1);
    }

    // ── Task 9: widebandExtensionRequested ──────────────────────────────
    void wideband_extension_default_is_false()
    {
        SliceModel slice;
        QCOMPARE(slice.widebandExtensionRequested(), false);
    }

    void wideband_extension_setter_round_trips()
    {
        SliceModel slice;
        slice.setWidebandExtensionRequested(true);
        QCOMPARE(slice.widebandExtensionRequested(), true);
    }

    void wideband_extension_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::widebandExtensionRequestedChanged);
        slice.setWidebandExtensionRequested(true);
        QCOMPARE(spy.count(), 1);
    }

    // ── Task 10: psPaused ───────────────────────────────────────────────
    void ps_paused_default_is_false()
    {
        SliceModel slice;
        QCOMPARE(slice.psPaused(), false);
    }

    void ps_paused_setter_round_trips()
    {
        SliceModel slice;
        slice.setPsPaused(true);
        QCOMPARE(slice.psPaused(), true);
    }

    void ps_paused_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::psPausedChanged);
        slice.setPsPaused(true);
        QCOMPARE(spy.count(), 1);
    }

    // ── Sub-Epic I Task 3: streamIndex / shiftOffsetHz ──────────────────
    void stream_index_defaults_to_unbound()
    {
        SliceModel slice;
        QCOMPARE(slice.streamIndex(), -1);
    }

    void stream_index_change_emits_once()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::streamIndexChanged);
        slice.setStreamIndex(2);
        slice.setStreamIndex(2);           // idempotent
        QCOMPARE(spy.count(), 1);
        QCOMPARE(slice.streamIndex(), 2);
    }

    void shift_offset_round_trips()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::shiftOffsetHzChanged);
        slice.setShiftOffsetHz(-25000.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(slice.shiftOffsetHz(), -25000.0);
    }

    // ── Sub-Epic J Task 1: anfEnabled ────────────────────────────────────
    // ANF was the one RXA setting with no home on the slice, which is why
    // MainWindow routed it to rxChannel(0) while its neighbours SNB and APF
    // went through SliceModel. Give it the same shape they have.
    void anf_enabled_defaults_off_and_round_trips()
    {
        SliceModel slice;
        QCOMPARE(slice.anfEnabled(), false);

        QSignalSpy spy(&slice, &SliceModel::anfEnabledChanged);
        slice.setAnfEnabled(true);
        QCOMPARE(slice.anfEnabled(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    void anf_enabled_setter_is_idempotent()
    {
        SliceModel slice;
        slice.setAnfEnabled(true);
        QSignalSpy spy(&slice, &SliceModel::anfEnabledChanged);
        slice.setAnfEnabled(true);
        QCOMPARE(spy.count(), 0);
    }

    // ── Sub-Epic J follow-up: NB1 / NB2 / SNB detailed tuning ────────────
    // Setup -> DSP -> NB/SNB wrote these eight straight to WDSP with a
    // hardcoded channel 0, so tuning the blanker always hit receiver A
    // whichever receiver the operator was working.  The bypass was invisible
    // to the Sub-Epic J rxChannel() audit because the calls go to WDSP by
    // another route (SetEXTANB* / SetEXTNOB* / SetRXASNBA*), not through
    // WdspEngine::rxChannel at all.
    //
    // Defaults are Thetis's, carried over from the global AppSettings keys
    // this page used to write: udDSPNB=30, udDSPNBTransition / Lead / Lag
    // =0.01 ms, comboDSPNOBmode=0, udDSPSNBThresh1=8.0, udDSPSNBThresh2=20.0
    // (setup.designer.cs grpDSPNB + grpDSPSNB [v2.10.3.13]), and NereusSDR's
    // own 6000 Hz SNB output bandwidth.
    void nb_and_snb_tuning_defaults_match_thetis()
    {
        SliceModel slice;
        QCOMPARE(slice.nb1Threshold(), 30);
        QCOMPARE(slice.nb1TransitionMs(), 0.01);
        QCOMPARE(slice.nb1LeadMs(), 0.01);
        QCOMPARE(slice.nb1LagMs(), 0.01);
        QCOMPARE(slice.nb2Mode(), 0);
        QCOMPARE(slice.snbK1(), 8.0);
        QCOMPARE(slice.snbK2(), 20.0);
        QCOMPARE(slice.snbOutputBandwidthHz(), 6000);
    }

    void nb_and_snb_tuning_round_trips_and_signals()
    {
        SliceModel slice;

        QSignalSpy thr(&slice, &SliceModel::nb1ThresholdChanged);
        slice.setNb1Threshold(250);
        QCOMPARE(slice.nb1Threshold(), 250);
        QCOMPARE(thr.count(), 1);

        QSignalSpy trans(&slice, &SliceModel::nb1TransitionMsChanged);
        slice.setNb1TransitionMs(0.5);
        QCOMPARE(slice.nb1TransitionMs(), 0.5);
        QCOMPARE(trans.count(), 1);

        QSignalSpy lead(&slice, &SliceModel::nb1LeadMsChanged);
        slice.setNb1LeadMs(0.25);
        QCOMPARE(slice.nb1LeadMs(), 0.25);
        QCOMPARE(lead.count(), 1);

        QSignalSpy lag(&slice, &SliceModel::nb1LagMsChanged);
        slice.setNb1LagMs(0.75);
        QCOMPARE(slice.nb1LagMs(), 0.75);
        QCOMPARE(lag.count(), 1);

        QSignalSpy mode(&slice, &SliceModel::nb2ModeChanged);
        slice.setNb2Mode(3);
        QCOMPARE(slice.nb2Mode(), 3);
        QCOMPARE(mode.count(), 1);

        QSignalSpy k1(&slice, &SliceModel::snbK1Changed);
        slice.setSnbK1(12.5);
        QCOMPARE(slice.snbK1(), 12.5);
        QCOMPARE(k1.count(), 1);

        QSignalSpy k2(&slice, &SliceModel::snbK2Changed);
        slice.setSnbK2(40.0);
        QCOMPARE(slice.snbK2(), 40.0);
        QCOMPARE(k2.count(), 1);

        QSignalSpy bw(&slice, &SliceModel::snbOutputBandwidthHzChanged);
        slice.setSnbOutputBandwidthHz(3000);
        QCOMPARE(slice.snbOutputBandwidthHz(), 3000);
        QCOMPARE(bw.count(), 1);
    }

    // Idempotency guard, same rule the rest of SliceModel follows.  It also
    // matters structurally here: the NB1 / NB2 setters feed a cross-slice
    // mirror in RadioModel, and a setter that re-emits on an unchanged value
    // would make that mirror recurse.
    void nb_and_snb_tuning_setters_are_idempotent()
    {
        SliceModel slice;
        slice.setNb1Threshold(250);
        slice.setNb1TransitionMs(0.5);
        slice.setNb2Mode(3);
        slice.setSnbK1(12.5);

        QSignalSpy thr(&slice, &SliceModel::nb1ThresholdChanged);
        QSignalSpy trans(&slice, &SliceModel::nb1TransitionMsChanged);
        QSignalSpy mode(&slice, &SliceModel::nb2ModeChanged);
        QSignalSpy k1(&slice, &SliceModel::snbK1Changed);

        slice.setNb1Threshold(250);
        slice.setNb1TransitionMs(0.5);
        slice.setNb2Mode(3);
        slice.setSnbK1(12.5);

        QCOMPARE(thr.count(), 0);
        QCOMPARE(trans.count(), 0);
        QCOMPARE(mode.count(), 0);
        QCOMPARE(k1.count(), 0);
    }
};

QTEST_MAIN(TestSliceModelPhase3FProperties)
#include "tst_slice_model_phase3f_properties.moc"
