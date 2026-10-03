// no-port-check: NereusSDR-original test file. Pins the SpectrumDetectorMode
// enumerator values extracted from gui/SpectrumWidget.h in R1 Task 2.
#include <QtTest>
#include "core/spectrum/SpectrumDetectorMode.h"

class TstSpectrumDetectorMode : public QObject {
    Q_OBJECT
private slots:
    // The enumerator values are WDSP analyzer.c detector modes.  Pin them so a
    // future reorder cannot silently change what the display draws.
    void valuesArePinned()
    {
        using M = NereusSDR::SpectrumDetectorMode;
        QCOMPARE(static_cast<int>(M::Peak),      0);
        QCOMPARE(static_cast<int>(M::Rosenfell), 1);
        QCOMPARE(static_cast<int>(M::Average),   2);
        QCOMPARE(static_cast<int>(M::Sample),    3);
        QCOMPARE(static_cast<int>(M::RMS),       4);
    }

    // NOTE: an earlier draft had a headerIsWidgetFree() test here that was a
    // bare QVERIFY(true).  It asserted nothing.  The property it claimed to
    // check is genuinely verified by tst_core_has_no_gui_includes in Task 4,
    // which walks src/core and src/models and fails on any '#include "gui/'.
    // Do not reintroduce a tautological test here.
};

QTEST_MAIN(TstSpectrumDetectorMode)
#include "tst_spectrum_detector_mode.moc"
