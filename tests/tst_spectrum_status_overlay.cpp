// =================================================================
// tests/tst_spectrum_status_overlay.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic E Task 1: SpectrumStatusOverlay construct + setters.
// =================================================================
#include <QtTest/QtTest>
#include "gui/widgets/SpectrumStatusOverlay.h"

using namespace NereusSDR;

class TestSpectrumStatusOverlay : public QObject {
    Q_OBJECT
private slots:
    void overlay_constructs_with_slice_letter()
    {
        SpectrumStatusOverlay overlay;
        overlay.setSliceLetter(QChar('A'));
        overlay.setFrequencyHz(14225000);
        overlay.setMode(QStringLiteral("USB"));
        overlay.setChainIndex(0);
        QCOMPARE(overlay.sliceLetter(), QChar('A'));
    }

    void overlay_tracks_optional_state_flags()
    {
        SpectrumStatusOverlay overlay;
        overlay.setTxBound(true);
        overlay.setWideBpf(true, QStringLiteral("operator-forced bypass"));
        overlay.setDiversityActive(true);
        overlay.setPsPaused(true);
        // No public getters yet; smoke test ensures the setters compile + don't crash.
        QVERIFY(true);
    }
};

QTEST_MAIN(TestSpectrumStatusOverlay)
#include "tst_spectrum_status_overlay.moc"
