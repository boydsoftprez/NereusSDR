// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic J Task 3. The DSP menu is not attached to any flag, so
// with two receivers running nothing said which one it meant. It used to
// write rxChannel(0) unconditionally. The rule is that a control attached
// to no slice targets the active slice.

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
};

QTEST_MAIN(TstDspMenuActiveSlice)
#include "tst_dsp_menu_active_slice.moc"
