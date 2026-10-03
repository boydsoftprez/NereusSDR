// =================================================================
// tests/tst_spectrum_status_overlay.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic E Task 1: SpectrumStatusOverlay construct + setters.
// =================================================================
#include <QtTest/QtTest>

#include "OperatorWording.h"
#include "PanStatusSamples.h"

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

    // R-R3-37: the row paints the short line, the hover text is the full
    // explanation, and the RF reason for WIDE stays alongside it.
    void remote_status_paints_short_line_and_explains_on_hover()
    {
        SpectrumStatusOverlay overlay;
        overlay.resize(200, 44);
        overlay.setWideBpf(true, QStringLiteral("Receive preselector bypassed"));
        overlay.setRemoteDisplayStatus({QStringLiteral("Paused: Core busy"),
                                        QStringLiteral("The Core has no room.")});
        QCOMPARE(overlay.remoteDisplayStatus(), QStringLiteral("Paused: Core busy"));
        QCOMPARE(overlay.remoteDisplayExplanation(), QStringLiteral("The Core has no room."));
        QCOMPARE(overlay.height(), 44);
        QCOMPARE(overlay.toolTip(),
                 QStringLiteral("The Core has no room.\nReceive preselector bypassed"));

        // Full quality: no row, but hovering still explains.
        overlay.setRemoteDisplayStatus({QString(), QStringLiteral("Full quality.")});
        QCOMPARE(overlay.height(), 22);
        QVERIFY(overlay.visibleRemoteDisplayStatus().isEmpty());
        QCOMPARE(overlay.toolTip(), QStringLiteral("Full quality.\nReceive preselector bypassed"));
    }

    // I1: every state paints a whole form of its short line in a 200 px
    // strip, the longest that fits in the row's font on this platform.
    void every_short_line_fits_200_px_without_elision()
    {
        SpectrumStatusOverlay overlay;
        int checked = 0;
        for (const PanDisplayState& state : PanStatusSamples::all()) {
            const PanStatusText text = buildPanStatusText(state);
            overlay.setRemoteDisplayStatus(text);
            overlay.resize(200, overlay.height());
            const QString painted = overlay.visibleRemoteDisplayStatus();
            if (text.shortLine.isEmpty()) {
                QVERIFY(painted.isEmpty());
                continue;
            }
            ++checked;
            const QStringList forms = text.shortForms();
            QVERIFY2(forms.contains(painted), qPrintable(painted));
            QVERIFY2(overlay.remoteStatusTextWidth(painted) <= overlay.remoteStatusRowWidth(),
                     qPrintable(painted));
            for (const QString& form : forms.mid(0, forms.indexOf(painted))) {
                QVERIFY2(overlay.remoteStatusTextWidth(form) > overlay.remoteStatusRowWidth(),
                         qPrintable(form));
            }
        }
        QVERIFY2(checked >= 200, qPrintable(QString::number(checked)));
    }

    // I1: a row narrower than the longest form paints a shorter one, whole;
    // one too narrow for every form paints the shortest, never an elided line.
    void a_narrower_row_paints_a_shorter_form()
    {
        SpectrumStatusOverlay overlay;
        const PanStatusText text{QStringLiteral("Refused: out of range"), QStringLiteral("Why."),
                                 {QStringLiteral("Refused")}};
        overlay.setRemoteDisplayStatus(text);
        QCOMPARE(overlay.remoteDisplayForms(), text.shortForms());
        const int longest = overlay.remoteStatusTextWidth(text.shortLine);
        overlay.resize(longest + 8, overlay.height());
        QCOMPARE(overlay.remoteStatusRowWidth(), longest);
        QCOMPARE(overlay.visibleRemoteDisplayStatus(), text.shortLine);
        overlay.resize(longest + 7, overlay.height());
        QCOMPARE(overlay.visibleRemoteDisplayStatus(), QStringLiteral("Refused"));
        overlay.resize(12, overlay.height());
        QCOMPARE(overlay.visibleRemoteDisplayStatus(), QStringLiteral("Refused"));
        QVERIFY(!overlay.visibleRemoteDisplayStatus().contains(QChar(0x2026)));
    }
};

QTEST_MAIN(TestSpectrumStatusOverlay)
#include "tst_spectrum_status_overlay.moc"
