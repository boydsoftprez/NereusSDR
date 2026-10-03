// no-port-check: NereusSDR-original test of ported behavior. Thetis resets
// a receiver's peak blobs and active peak hold (and holds both back 500 ms)
// when its display centre moves (display.cs:907-921 [v2.10.3.15]) or its
// display edges change (display.cs:1214-1245 [v2.10.3.15]). A pan does the
// same whenever its view moves or is resized, local or remote.
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.

#include <QtTest/QtTest>

#include "core/session/media/DisplayCodec.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "gui/SpectrumWidget.h"

#include <algorithm>
#include <cmath>

using namespace NereusSDR;

namespace {

constexpr double kCentreHz = 14200000.0;
constexpr double kSpanHz = 800.0;

bool anyFinite(const QVector<float>& values)
{
    return std::any_of(values.cbegin(), values.cend(),
                       [](float v) { return std::isfinite(v); });
}

bool anyBlob(const QVector<PeakBlob>& blobs)
{
    return std::any_of(blobs.cbegin(), blobs.cend(),
                       [](const PeakBlob& b) { return b.enabled; });
}

void enablePeaks(SpectrumWidget& w)
{
    w.setActivePeakHoldEnabled(true);
    w.setActivePeakHoldDurationMs(60000);
    w.setPeakBlobsEnabled(true);
}

// A local pan: flat floor and one carrier, as tst_notch_visual's fixture.
void configureLocal(SpectrumWidget& w)
{
    w.setFrequencyRange(kCentreHz, kSpanHz);
    w.setDdcCenterFrequency(kCentreHz);
    w.setSampleRate(kSpanHz);
    w.setVfoFrequency(kCentreHz);
    w.setSpectrumDetector(SpectrumDetector::Peak);
    w.setSpectrumAveraging(SpectrumAveraging::None);
}

void feedLocal(SpectrumWidget& w, int frames)
{
    QVector<float> bins(4096, 1e-12f);
    bins[2816] = 1e-3f;
    for (int i = 0; i < frames; ++i) {
        w.updateSpectrumLinear(0, bins, 2.0, -10.0);
    }
}

// A remote pan: the Core's frames at 10 a second.
SpectrumEndpointContext remoteContext()
{
    SpectrumEndpointContext context;
    context.codec = {51, 1, -180, 0, 16, 16, 0};
    context.exactCentreHz = kCentreHz;
    context.exactSpanHz = 16000;
    context.targetFps = 10;
    return context;
}

void feedRemote(SpectrumWidget& w, int frames)
{
    DisplayCodecFrame frame;
    frame.context = remoteContext().codec;
    frame.traceDbm = QVector<float>(16, -120.0f);
    frame.traceDbm[8] = -40.0f;
    frame.waterfallDbm = frame.traceDbm;
    for (int i = 0; i < frames; ++i) {
        QVERIFY(w.updateRemoteSpectrum(frame));
    }
}

} // namespace

class TestSpectrumPeaksResets : public QObject
{
    Q_OBJECT

private:
    // The peaks are showing: past the first reset's display delay.
    static void expectShowing(const SpectrumWidget& w)
    {
        QVERIFY(w.activePeakHoldActive());
        QVERIFY(anyFinite(w.activePeakHoldPeaksForTest()));
        QVERIFY(anyBlob(w.peakBlobsForTest()));
    }
    // Reset: emptied and held back.
    static void expectReset(const SpectrumWidget& w)
    {
        QVERIFY(!w.activePeakHoldActive());
        QVERIFY(!anyFinite(w.activePeakHoldPeaksForTest()));
        QVERIFY(!anyBlob(w.peakBlobsForTest()));
    }

private slots:
    void aLocalPanResetsWhenItsCentreMoves()
    {
        SpectrumWidget w;
        configureLocal(w);
        enablePeaks(w);
        feedLocal(w, 40);
        expectShowing(w);
        w.setDisplayWindowPreservingHistory(kCentreHz + 100.0, kSpanHz);
        expectReset(w);
    }

    void aLocalPanResetsWhenItsSpanChanges()
    {
        SpectrumWidget w;
        configureLocal(w);
        enablePeaks(w);
        feedLocal(w, 40);
        expectShowing(w);
        w.setDisplayWindowPreservingHistory(kCentreHz, kSpanHz / 2.0);
        expectReset(w);
    }

    void aLocalPanKeepsItsPeaksWhenTheViewDoesNotChange()
    {
        SpectrumWidget w;
        configureLocal(w);
        enablePeaks(w);
        feedLocal(w, 40);
        w.setDisplayWindowPreservingHistory(kCentreHz, kSpanHz);
        expectShowing(w);
    }

    void aRemotePanResetsWhenItsCentreMovesOrItsSpanChanges()
    {
        SpectrumWidget w;
        enablePeaks(w);
        const SpectrumEndpointContext context = remoteContext();
        w.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 4096);
        feedRemote(w, 10);
        expectShowing(w);
        w.setDisplayWindowPreservingHistory(kCentreHz + 1000.0, context.exactSpanHz);
        expectReset(w);

        SpectrumWidget z;
        enablePeaks(z);
        z.setRemoteSpectrumContext(context, context.exactCentreHz, 192000, 4096);
        feedRemote(z, 10);
        expectShowing(z);
        z.setDisplayWindowPreservingHistory(z.centerFrequency(), context.exactSpanHz / 2.0);
        expectReset(z);
    }
};

QTEST_MAIN(TestSpectrumPeaksResets)
#include "tst_spectrum_peaks_resets.moc"
