// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
// 2026-10-26  J.J. Boyd / KG4VCF; skip the GPU frame case where QRhi is unavailable. AI tooling: Anthropic Claude Code.
// 2026-10-11  J.J. Boyd / KG4VCF; original waveform observation. AI tooling: OpenAI Codex.
// no-port-check: Original fake-data native QRhi dynamic-layer regression.
#include "NativeWindowFrames.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/WaveformMeterItem.h"
#include <QDir>
#include <QtTest>
using namespace NereusSDR;
class TestWaveformNative : public QObject
{
    Q_OBJECT
  private slots:
    void dynamicTraceUpdatesNativeTexture()
    {
        NEREUS_SKIP_WITHOUT_QRHI();
        MeterWidget meter;
        meter.resize(500, 300);
        auto face = std::make_unique<WaveformMeterItem>();
        auto positive = std::make_shared<WaveformSnapshot>();
        positive->sampleRate = 16000;
        positive->asOfNs = 5000000000LL;
        positive->availability = WaveformAvailability::Available;
        positive->samples.fill(0.8f, 80000);
        positive->segments = {{0, 80000, 0, 0}};
        face->setSnapshot(positive);
        WaveformMeterItem* trace = face.get();
        meter.addItem(face.release());
        meter.show();
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy submitted(&meter, &QRhiWidget::frameSubmitted);
        NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(meter, submitted, 5000));
        const QImage before = meter.grabFramebuffer();
#else
        QCoreApplication::processEvents();
        const QImage before = meter.grab().toImage();
#endif
        QVERIFY(!before.isNull());
        trace->takeStaticPresentationChange();
        auto negative = std::make_shared<WaveformSnapshot>(*positive);
        negative->samples.fill(-0.8f, 80000);
        QVERIFY(trace->setSnapshot(negative));
        QVERIFY(!trace->takeStaticPresentationChange());
        meter.invalidatePresentation(trace);
#ifdef NEREUS_GPU_SPECTRUM
        NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(meter, submitted, 5000));
        const QImage after = meter.grabFramebuffer();
#else
        QCoreApplication::processEvents();
        const QImage after = meter.grab().toImage();
#endif
        QVERIFY(!after.isNull());
        QVERIFY(before != after);
        // Dynamic trace moves; the cached static axis/title pixels stay fixed.
        const int dpr = int(after.devicePixelRatio());
        QCOMPARE(before.copy(0, 0, after.width(), 30 * dpr), after.copy(0, 0, after.width(), 30 * dpr));
        const QString directory = qEnvironmentVariable("NEREUS_WAVEFORM_SHOTS");
        if (!directory.isEmpty()) {
            QVERIFY(before.save(directory + "/native-positive.png"));
            QVERIFY(after.save(directory + "/native-negative.png"));
        }
    }
};
QTEST_MAIN(TestWaveformNative)
#include "tst_waveform_native.moc"
