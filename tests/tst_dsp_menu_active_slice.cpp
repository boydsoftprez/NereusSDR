// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic J Task 3. The DSP menu is not attached to any flag, so
// with two receivers running nothing said which one it meant. It used to
// write rxChannel(0) unconditionally. The rule is that a control attached
// to no slice targets the active slice.
//
// Task 4 extends this file with the same precondition check for the
// container S-meter fix (MainWindow.cpp): MeterPoller::setRxChannel() is
// re-invoked from a MainWindow lambda connected to activeSliceChanged, and
// that lambda resolves the new channel via activeSlice()->sliceIndex().
// MainWindow itself is too heavyweight to construct in a unit-test binary
// (see tst_mainwindow_status_bar_safety.cpp), so this test pins the
// RadioModel-level contract the lambda depends on instead of the lambda
// itself.

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstDspMenuActiveSlice : public QObject {
    Q_OBJECT
private slots:
    void anf_from_a_detached_control_targets_the_active_slice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        SliceModel* sb = radio.sliceById(b);
        QVERIFY(sa != nullptr);
        QVERIFY(sb != nullptr);

        // Slice A is active on creation.
        QVERIFY(radio.activeSlice() == sa);

        // Simulate what the menu action does: resolve active, then set.
        if (SliceModel* target = radio.activeSlice()) {
            target->setAnfEnabled(true);
        }
        QCOMPARE(sa->anfEnabled(), true);
        QCOMPARE(sb->anfEnabled(), false);

        // Operator clicks B's flag. The menu must follow.
        radio.setActiveSlice(b);
        QVERIFY(radio.activeSlice() == sb);

        if (SliceModel* target = radio.activeSlice()) {
            target->setAnfEnabled(true);
        }
        QCOMPARE(sb->anfEnabled(), true);
    }

    // The container S-meter is attached to no flag. It must show the
    // receiver the operator is working, not always slice A.
    //
    // This does not exercise MainWindow's re-bind lambda directly (nothing
    // in this suite constructs MainWindow: it needs a live RadioModel,
    // WDSP init and audio/network threads). It pins the two facts the
    // lambda relies on: activeSliceChanged actually fires when focus moves,
    // and activeSlice()->sliceIndex() resolves to the new slice's id (which
    // doubles as its WDSP RX channel id), not the old one. Both of those
    // were already true before this task (Task 3's test exercises the same
    // RadioModel path), so this test passes before and after the MainWindow
    // fix -- it is a precondition guard for the fix, not a regression test
    // for it.
    void the_container_meter_rebinds_when_focus_moves()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        const int a = radio.addSlice(QStringLiteral("pan-0"));
        const int b = radio.addSlice(QStringLiteral("pan-0"));

        QSignalSpy spy(&radio, &RadioModel::activeSliceChanged);
        radio.setActiveSlice(b);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(radio.activeSlice()->sliceIndex(), b);
        QVERIFY(radio.activeSlice()->sliceIndex() != a);
    }
};

QTEST_MAIN(TstDspMenuActiveSlice)
#include "tst_dsp_menu_active_slice.moc"
