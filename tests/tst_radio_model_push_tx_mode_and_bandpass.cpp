// no-port-check: NereusSDR-original test for the pushTxModeAndBandpass
// helper.  The TXA.c reference below is a Thetis source-of-truth cite
// for design context (the WDSP defaults that make the bug visible),
// not a port — no Thetis code is translated in this file.
//
// tst_radio_model_push_tx_mode_and_bandpass.cpp
//
// Issue #153 sub-bug 2 — SSB MOX TXA seeding.
//
// Bug being guarded: the SSB voice TX path never calls SetTXAMode or
// SetTXABandpassFreqs.  Only TUN does, via TxChannel::setTuneTone
// (TxChannel.cpp:861-916 [v2.10.3.13]).  Without TUN, WDSP TXA defaults
// from create_txa apply: mode=TXA_LSB, f_low=-5000, f_high=-100
// (TXA.c:33-35 [v2.10.3.13]).  Works by accident on LSB; silent on
// USB / AM / FM / DSB / DIGU / DIGL until TUN is hit once.
//
// Fix: RadioModel::pushTxModeAndBandpass() reads the active slice's
// DSPMode + audio-space filter cutoffs and dispatches them to TxChannel.
// Wired on three triggers (verified at integration layer / bench):
//   - createTxChannel success (initial seed)
//   - SliceModel::dspModeChanged (re-seed after user changes mode)
//   - MoxController::txAboutToBegin (belt-and-suspenders pre-MOX seed)
//
// This test pins the helper-level contract: when called with an active
// slice, it emits txModeAndBandpassPushed(mode, audioLow, audioHigh)
// carrying the slice's current state.

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestRadioModelPushTxModeAndBandpass : public QObject
{
    Q_OBJECT

private slots:
    // ── Contract: helper emits signal with slice mode + TransmitModel filter ──
    //
    // TX bandpass source is TransmitModel (audio-space, low<=high), NOT
    // SliceModel (RX-passband IQ-space, negative for LSB).  See
    // RadioModel::pushTxModeAndBandpass header comment for why.

    void pushEmitsSignalWithSliceModeAndTransmitFilter()
    {
        RadioModel model;
        const int idx = model.addSlice();
        QCOMPARE(idx, 0);
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        slice->setDspMode(DSPMode::USB);
        model.transmitModel().setFilterLow(150);
        model.transmitModel().setFilterHigh(2850);

        QSignalSpy spy(&model, &RadioModel::txModeAndBandpassPushed);
        model.pushTxModeAndBandpass();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<DSPMode>(), DSPMode::USB);
        QCOMPARE(spy.at(0).at(1).toInt(), 150);
        QCOMPARE(spy.at(0).at(2).toInt(), 2850);
    }

    // ── Contract: cutoffs stay audio-space across LSB↔USB mode changes ──────
    //
    // Verifies the bug Codex flagged on PR #189: prior implementation read
    // cutoffs from SliceModel::filterLow/High, which are negative on LSB.
    // applyTxFilterForMode would then double-negate, producing a
    // wrong-sided TXA bandpass on first MOX after an LSB selection.
    // Reading from TransmitModel keeps cutoffs positive across all modes;
    // the mode parameter alone drives the IQ-space sign mapping inside
    // applyTxFilterForMode.

    void pushKeepsCutoffsPositiveAcrossModeChanges()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        // User-configured TX BW stays the same regardless of slice mode.
        model.transmitModel().setFilterLow(150);
        model.transmitModel().setFilterHigh(2850);

        QSignalSpy spy(&model, &RadioModel::txModeAndBandpassPushed);

        slice->setDspMode(DSPMode::LSB);
        model.pushTxModeAndBandpass();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<DSPMode>(), DSPMode::LSB);
        QCOMPARE(spy.at(0).at(1).toInt(), 150);   // audio-space, NOT -2850
        QCOMPARE(spy.at(0).at(2).toInt(), 2850);  // audio-space, NOT -150

        slice->setDspMode(DSPMode::USB);
        model.pushTxModeAndBandpass();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).value<DSPMode>(), DSPMode::USB);
        QCOMPARE(spy.at(1).at(1).toInt(), 150);
        QCOMPARE(spy.at(1).at(2).toInt(), 2850);
    }

    // ── Contract: no emit without an active slice ────────────────────────────
    //
    // pushTxModeAndBandpass must early-return when no slice exists.  Without
    // this guard, dispatching with default-constructed slice values could
    // push junk to TxChannel during odd lifecycle moments (RadioModel
    // construction before connectToRadio runs).

    void pushIsNoOpWhenNoActiveSlice()
    {
        RadioModel model;
        // Intentionally NOT calling addSlice — m_activeSlice is null.
        QSignalSpy spy(&model, &RadioModel::txModeAndBandpassPushed);
        model.pushTxModeAndBandpass();
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestRadioModelPushTxModeAndBandpass)
#include "tst_radio_model_push_tx_mode_and_bandpass.moc"
