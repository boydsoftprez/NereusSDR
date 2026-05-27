// =================================================================
// tests/tst_panadapter_applet_slice_assoc.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 1: PanadapterApplet skeleton + slice assoc.
// =================================================================
#include <QtTest/QtTest>
#include "gui/PanadapterApplet.h"

using namespace NereusSDR;

class TestPanadapterAppletSliceAssoc : public QObject {
    Q_OBJECT
private slots:
    void applet_constructs_with_pan_id()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QCOMPARE(applet.panId(), QStringLiteral("pan-0"));
    }

    void add_slice_makes_it_active_when_no_active_yet()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QCOMPARE(applet.activeSliceIndex(), -1);
        applet.addSlice(2);
        QCOMPARE(applet.activeSliceIndex(), 2);
    }

    void add_second_slice_does_not_change_active()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.addSlice(1);
        QCOMPARE(applet.activeSliceIndex(), 0);
        QCOMPARE(applet.associatedSlices().size(), 2);
    }

    void remove_active_slice_promotes_another()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.addSlice(1);
        applet.removeSlice(0);
        QCOMPARE(applet.activeSliceIndex(), 1);
    }

    void remove_last_slice_sets_active_to_minus_1()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.removeSlice(0);
        QCOMPARE(applet.activeSliceIndex(), -1);
    }
};

QTEST_MAIN(TestPanadapterAppletSliceAssoc)
#include "tst_panadapter_applet_slice_assoc.moc"
