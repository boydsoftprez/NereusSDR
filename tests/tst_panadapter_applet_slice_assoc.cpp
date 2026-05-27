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
};

QTEST_MAIN(TestPanadapterAppletSliceAssoc)
#include "tst_panadapter_applet_slice_assoc.moc"
