// tests/tst_nb_tuning_fanout.cpp  (NereusSDR)
// NereusSDR-original test; pins a ported Thetis behaviour.
//
// Phase 3F chip task_c1e6fbad: the NB/SNB Setup page wrote WDSP channel 0 and
// nothing else, so on a two-slice radio the second receiver kept whatever NB
// tuning it was created with.
//
// Upstream treats every knob on that page as a radio-wide setting written to
// each receiver in turn, not as a property of a selected one:
//
//   udDSPNB_ValueChanged            setup.cs:8603-8608   [v2.10.3.15]
//   udDSPNBTransition_ValueChanged  setup.cs:16260-16265 [v2.10.3.15]
//   udDSPNBLead_ValueChanged        setup.cs:16267-16272 [v2.10.3.15]
//   udDSPNBLag_ValueChanged         setup.cs:16274-16279 [v2.10.3.15]
//       console.radio.GetDSPRX(0, 0).X = v;
//       console.radio.GetDSPRX(1, 0).X = v;
//   comboDSPNOBmode_SelectedIndexChanged setup.cs:17007-17019 [v2.10.3.15]
//       console.radio.GetDSPRX(0, 0).NBMode = nbmode;
//       console.radio.GetDSPRX(1, 0).NBMode = nbmode;
//   udDSPSNBThresh1_ValueChanged    setup.cs:17647-17653 [v2.10.3.15]
//   udDSPSNBThresh2_ValueChanged    setup.cs:17655-17661 [v2.10.3.15]
//       WDSP.SetRXASNBAk1(WDSP.id(0, 0), v);
//       WDSP.SetRXASNBAk1(WDSP.id(0, 1), v);
//       WDSP.SetRXASNBAk1(WDSP.id(2, 0), v);
//
// This pins the fan-out SET, which is where the defect lived. The write
// itself is a thin per-id loop over RxChannel::nb(); resolving real WDSP
// channels needs a live WdspEngine and is out of reach here, so the
// assertion is on which channels a write is addressed to.
//
// no-port-check: not a port of Thetis code. The setup.cs line numbers above
// anchor the behaviour being asserted; the port itself lives in
// src/models/RadioModel.cpp with full attribution.

#include <QtTest/QtTest>

#include "models/RadioModel.h"

using namespace NereusSDR;

class TestNbTuningFanout : public QObject {
    Q_OBJECT
private slots:
    void targets_every_live_slice_not_just_channel_zero();
    void targets_follow_slice_ids_after_a_removal();
    void no_slices_means_no_targets();
};

// The defect: two slices, but only channel 0 was ever written.
void TestNbTuningFanout::targets_every_live_slice_not_just_channel_zero()
{
    RadioModel model;

    const int slice0 = model.addSlice();
    const int slice1 = model.addSlice();
    QVERIFY2(slice0 != slice1, "addSlice() must hand out distinct ids");

    const QVector<int> targets = model.nbTuningTargetChannels();

    QVERIFY2(targets.contains(slice0),
        qPrintable(QStringLiteral("slice %1 missing from NB fan-out set %2")
            .arg(slice0).arg(QDebug::toString(targets))));
    QVERIFY2(targets.contains(slice1),
        qPrintable(QStringLiteral("NB tuning must reach every receiver "
            "(Thetis writes GetDSPRX(0,0) AND GetDSPRX(1,0), "
            "setup.cs:8603-8608 [v2.10.3.15]); slice %1 missing from %2")
            .arg(slice1).arg(QDebug::toString(targets))));
    QCOMPARE(targets.size(), 2);
}

// Slice ids are ids, not list positions: removeSlice does not renumber the
// survivors. The fan-out set must carry the surviving slice's real id, or the
// write lands on a channel that is not the one the operator is listening to.
void TestNbTuningFanout::targets_follow_slice_ids_after_a_removal()
{
    RadioModel model;

    const int slice0 = model.addSlice();
    const int slice1 = model.addSlice();
    QCOMPARE(model.nbTuningTargetChannels().size(), 2);

    model.removeSlice(slice0);

    const QVector<int> targets = model.nbTuningTargetChannels();
    QCOMPARE(targets.size(), 1);
    QVERIFY2(targets.contains(slice1),
        qPrintable(QStringLiteral("survivor id %1 expected, got %2")
            .arg(slice1).arg(QDebug::toString(targets))));
    QVERIFY2(!targets.contains(slice0),
        "removed slice must not stay in the NB fan-out set");
}

// Setup can be opened before a radio is connected. No slices means nothing to
// address; the setters must be a no-op rather than defaulting to channel 0.
void TestNbTuningFanout::no_slices_means_no_targets()
{
    RadioModel model;
    QCOMPARE(model.nbTuningTargetChannels().size(), 0);

    // Must not crash or reach into a channel that does not exist.
    model.setNbThresholdAllRx(4.95);
    model.setNbTauMsAllRx(0.01);
    model.setNbLeadMsAllRx(0.01);
    model.setNbLagMsAllRx(0.01);
    model.setNb2ModeAllRx(0);
    model.setSnbK1AllRx(8.0);
    model.setSnbK2AllRx(20.0);
    model.setSnbOutputBandwidthAllRx(6000);
}

QTEST_GUILESS_MAIN(TestNbTuningFanout)
#include "tst_nb_tuning_fanout.moc"
