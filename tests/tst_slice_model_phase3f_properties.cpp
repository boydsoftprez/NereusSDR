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

    void slice_letter_setter_round_trips()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('C'));
        QCOMPARE(slice.sliceLetter(), QChar('C'));
    }

    void slice_letter_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toChar(), QChar('B'));
    }

    void slice_letter_setter_idempotent()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('B'));
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));  // same value
        QCOMPARE(spy.count(), 0);
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
};

QTEST_MAIN(TestSliceModelPhase3FProperties)
#include "tst_slice_model_phase3f_properties.moc"
