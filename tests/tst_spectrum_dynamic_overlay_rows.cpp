// no-port-check: NereusSDR-original. Which rows of the GPU dynamic overlay
// texture renderGpuFrame() clears and uploads.
//
// 2026-10-09, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// The dynamic overlay (noise floor, peak-hold trace, peak blobs) is a texture
// covering the whole widget, rebuilt and uploaded only over the spectrum band
// to keep the Metal upload small. That band used to be the current spectrum
// height alone, so a spectrum that got shorter left the lines drawn while it
// was taller in the rows below, and a freshly created texture showed memory
// it was never given below the band. SpectrumWidget::DynamicOverlayRows keeps
// the rows that may still hold earlier pixels; renderGpuFrame() takes its
// upload height from it. This test drives that bookkeeping without a QRhi;
// tst_spectrum_gpu_dynamic_overlay_clear checks the frames on a real GPU.

#include <QTest>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;
using Rows = SpectrumWidget::DynamicOverlayRows;

class TestSpectrumDynamicOverlayRows : public QObject {
    Q_OBJECT

private slots:
    // A created texture holds undefined memory: the first upload covers all
    // of it, later uploads only the band.
    void createdTexture_firstUploadCoversWholeTexture()
    {
        Rows rows;
        rows.textureCreated(1200);
        QVERIFY(rows.needsRebuild(960));
        QCOMPARE(rows.beginUpload(960), 1200);
        QVERIFY(!rows.needsRebuild(960));
        QCOMPARE(rows.beginUpload(960), 960);
    }

    // JJ's case: the spectrum gets shorter. The rows the tall band drew are
    // cleared and uploaded once, even when no overlay feature asked for a
    // rebuild, then only the short band.
    void shorterBand_clearsTheRowsTheTallBandDrew()
    {
        Rows rows;
        rows.textureCreated(1200);
        QCOMPARE(rows.beginUpload(960), 1200);
        QCOMPARE(rows.beginUpload(960), 960);

        QVERIFY(rows.needsRebuild(480));
        QCOMPARE(rows.beginUpload(480), 960);
        QVERIFY(!rows.needsRebuild(480));
        QCOMPARE(rows.beginUpload(480), 480);
    }

    // Several shrinks before a rebuild: the tallest band still drawn wins.
    void twoShrinks_clearTheTallestBand()
    {
        Rows rows;
        rows.textureCreated(1200);
        QCOMPARE(rows.beginUpload(1000), 1200);
        QCOMPARE(rows.beginUpload(700), 1000);
        QCOMPARE(rows.beginUpload(300), 700);
        QCOMPARE(rows.beginUpload(300), 300);
    }

    // A taller band holds nothing stale, but the overlay is redrawn for it.
    void tallerBand_uploadsTheNewBand()
    {
        Rows rows;
        rows.textureCreated(1200);
        QCOMPARE(rows.beginUpload(480), 1200);
        QVERIFY(rows.needsRebuild(960));
        QCOMPARE(rows.beginUpload(960), 960);
        QCOMPARE(rows.beginUpload(960), 960);
    }

    // A texture recreated after a shrink (a widget resize) is undefined
    // again, whatever was uploaded before.
    void recreatedTexture_uploadsWholeTextureAgain()
    {
        Rows rows;
        rows.textureCreated(1200);
        QCOMPARE(rows.beginUpload(960), 1200);
        QCOMPARE(rows.beginUpload(960), 960);

        rows.textureCreated(700);
        QVERIFY(rows.needsRebuild(300));
        QCOMPARE(rows.beginUpload(300), 700);
        QCOMPARE(rows.beginUpload(300), 300);
    }

    // The upload never leaves the texture and never has zero rows.
    void band_isClampedToTheTexture()
    {
        Rows rows;
        rows.textureCreated(600);
        QCOMPARE(rows.beginUpload(5000), 600);
        QCOMPARE(rows.beginUpload(5000), 600);
        QCOMPARE(rows.beginUpload(0), 600);
        QCOMPARE(rows.beginUpload(0), 1);
    }
};

QTEST_MAIN(TestSpectrumDynamicOverlayRows)
#include "tst_spectrum_dynamic_overlay_rows.moc"
