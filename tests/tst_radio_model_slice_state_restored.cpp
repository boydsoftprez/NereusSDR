// no-port-check: NereusSDR-original test for the sliceStateRestored
// emit contract on RadioModel::loadSliceState.  The console.cs reference
// in the body comment is a Thetis source-of-truth cite for design
// context, not a port — no Thetis code is translated in this file.
//
// tst_radio_model_slice_state_restored.cpp
//
// Verifies RadioModel::sliceStateRestored(int) is emitted at the end of
// loadSliceState() so MainWindow can push the now-correct slice state into
// the spectrum widget on connect (DDC center, VFO marker, filter offset).
//
// Bug being guarded: on initial HL2 connect, MainWindow::wireSliceToSpectrum
// runs at sliceAdded() time and seeds m_spectrumWidget->setDdcCenterFrequency()
// with the slice's *pre-restore* default frequency.  loadSliceState() then
// calls slice->restoreFromSettings() which conditionally emits frequencyChanged.
// In CTUN mode (default true), the MainWindow lambda's offScreen guard skips
// setDdcCenterFrequency() when the persisted freq sits within +/- halfBw of
// the default seed.  Result: spectrum bin labels point to the wrong band of
// RF until the first dial-tune crosses the offScreen threshold.
//
// This test pins the contract that loadSliceState() ALWAYS emits
// sliceStateRestored(idx) at completion, giving listeners an unconditional
// hook to push final state into views — mirroring the unconditional state
// push Thetis performs from chkPower_CheckedChanged via txtVFOAFreq_LostFocus
// (console.cs:27204 [v2.10.3.13]).

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRadioModelSliceStateRestored : public QObject
{
    Q_OBJECT

private slots:
    // ── Contract: emit fires once per loadSliceState call ────────────────────

    void emitsSliceStateRestoredAfterLoadSliceState()
    {
        RadioModel model;
        const int idx = model.addSlice();
        QCOMPARE(idx, 0);
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        QSignalSpy spy(&model, &RadioModel::sliceStateRestored);
        model.loadSliceState(slice);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), idx);
    }

    // ── Contract: emit carries the slice index that was restored ─────────────

    void emitCarriesSliceIndex()
    {
        RadioModel model;
        const int idx0 = model.addSlice();
        QCOMPARE(idx0, 0);

        QSignalSpy spy(&model, &RadioModel::sliceStateRestored);
        model.loadSliceState(model.slices().at(idx0));

        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(0).toInt(), idx0);
    }

    // ── Contract: addSlice() alone does NOT emit ─────────────────────────────
    //
    // This is what wireSliceToSpectrum's seed-with-default behaviour relies
    // on — sliceStateRestored is the loadSliceState completion signal, not
    // the slice creation signal.  sliceAdded covers creation.

    void addSliceDoesNotEmitSliceStateRestored()
    {
        RadioModel model;
        QSignalSpy spy(&model, &RadioModel::sliceStateRestored);

        const int idx = model.addSlice();
        QCOMPARE(idx, 0);

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestRadioModelSliceStateRestored)
#include "tst_radio_model_slice_state_restored.moc"
