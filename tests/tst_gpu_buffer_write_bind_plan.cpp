// no-port-check: NereusSDR-original. A GPU frame binds every dynamic buffer it writes.
//
// Qt's Metal backend (and Vulkan and D3D12) keeps each partial dynamic-buffer
// write pending until the buffer is next bound. A buffer written every frame
// and never bound keeps every frame's copy: a 3D pan wrote the 2D trace's
// line and fill unbound and the desktop window grew to 23 GB in 2.5 hours.
// SpectrumWidget::renderGpuFrame() takes its trace uploads and its trace
// draws from planSpectrumTraceFrame(), and MeterWidget's geometry write and
// draw both take drawsGeometryLayer(), so these offscreen checks guard the
// rule on every platform CI runs. tst_spectrum_gpu_buffer_growth is the
// end-to-end check on a real Metal QRhi.
//
// Fix wave 2026-09-30 (GUI-I2, GUI-M1), J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code: the 3DSS mesh and waterfall uniform buffers come
// from the plan too, written only in a frame whose draw binds them.

#include <QTest>

#include "gui/SpectrumTraceFramePlan.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {

constexpr int kInputCount = 15;

SpectrumTraceFrameInputs inputsFromBits(unsigned bits)
{
    const auto bit = [bits](int i) { return ((bits >> i) & 1u) != 0; };
    SpectrumTraceFrameInputs in;
    in.mode3D = bit(0);
    in.dssMeshReady = bit(1);
    in.dssHasRows = bit(2);
    in.dssFallbackReady = bit(3);
    in.tracePipelines = bit(4);
    in.traceBuffers = bit(5);
    in.peakBuffer = bit(6);
    in.hasPixels = bit(7);
    in.panFill = bit(8);
    in.peakHoldReady = bit(9);
    in.showsTransmitView = bit(10);
    in.heldTrace = bit(11);
    in.heldFill = bit(12);
    in.heldPeak = bit(13);
    in.waterfallPipeline = bit(14);
    return in;
}

// A 2D pan with every resource up, pixels, fill and peak hold on.
SpectrumTraceFrameInputs live2D()
{
    SpectrumTraceFrameInputs in;
    in.tracePipelines = true;
    in.traceBuffers = true;
    in.peakBuffer = true;
    in.hasPixels = true;
    in.panFill = true;
    in.peakHoldReady = true;
    in.waterfallPipeline = true;
    return in;
}

// Carries what the buffers hold into the next frame, as renderGpuFrame()
// does through m_visibleBinCount, m_fftFillHasData and m_peakHoldHasData.
SpectrumTraceFrameInputs nextFrame(SpectrumTraceFrameInputs in,
                                   const SpectrumTraceFramePlan& p)
{
    in.heldTrace = p.heldTrace;
    in.heldFill = p.heldFill;
    in.heldPeak = p.heldPeak;
    return in;
}

}  // namespace

class TestGpuBufferWriteBindPlan : public QObject {
    Q_OBJECT

private slots:
    // Every combination of the plan's inputs.
    void everyCombination_bindsWhatItWritesAndDrawsNoStaleData()
    {
        for (unsigned bits = 0; bits < (1u << kInputCount); ++bits) {
            const SpectrumTraceFrameInputs in = inputsFromBits(bits);
            const SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
            const QByteArray at = QByteArray("input bits 0x") + QByteArray::number(bits, 16);

            // One spectrum-region draw at most.
            QVERIFY2(int(p.drawsDssMesh) + int(p.drawsDssFallback) + int(p.drawsTrace) <= 1,
                     at.constData());

            // The region selection the draw chain has always made.
            const bool is3D = in.mode3D && in.dssMeshReady;
            QVERIFY2(p.drawsDssMesh == (is3D && in.dssHasRows), at.constData());
            QVERIFY2(p.drawsDssFallback
                         == (!p.drawsDssMesh && in.mode3D && in.dssFallbackReady),
                     at.constData());

            // Write implies bind, for each buffer (the leak). The mesh
            // uniforms are bound only by the mesh draw, the waterfall's
            // only by the waterfall draw.
            QVERIFY2(p.writeDssMeshUbo == p.drawsDssMesh, at.constData());
            QVERIFY2(p.drawsWaterfall == in.waterfallPipeline, at.constData());
            QVERIFY2(p.writeWaterfallUbo == p.drawsWaterfall, at.constData());
            QVERIFY2(!p.writeLine || p.bindLine, at.constData());
            QVERIFY2(!p.writeFill || p.bindFill, at.constData());
            QVERIFY2(!p.writePeak || p.bindPeak, at.constData());

            // Nothing is written outside the 2D trace region: a 3D pan
            // with a live mesh or fallback writes no trace buffer.
            if (!p.traceRegion) {
                QVERIFY2(!p.writeLine && !p.writeFill && !p.writePeak, at.constData());
                QVERIFY2(!p.heldTrace && !p.heldFill && !p.heldPeak, at.constData());
                QVERIFY2(!p.bindLine && !p.bindFill && !p.bindPeak, at.constData());
            }

            // The fill and peak ride on the line: never written, bound or
            // held without it.
            QVERIFY2(!p.writeFill || p.writeLine, at.constData());
            QVERIFY2(!p.writePeak || p.writeLine, at.constData());
            QVERIFY2(!p.bindFill || p.bindLine, at.constData());
            QVERIFY2(!p.bindPeak || p.bindLine, at.constData());
            QVERIFY2(!p.heldFill || p.heldTrace, at.constData());
            QVERIFY2(!p.heldPeak || p.heldTrace, at.constData());

            // The fill is written only while pan fill is on and drawn only
            // then.
            QVERIFY2(!p.writeFill || in.panFill, at.constData());
            QVERIFY2(!p.bindFill || in.panFill, at.constData());

            // Bind without a write in this frame redraws the trace the
            // buffers already hold, never data from another trace: with a
            // fresh line, a fill or peak that was not rewritten is not
            // bound.
            if (p.writeLine) {
                QVERIFY2(!p.bindFill || p.writeFill, at.constData());
                QVERIFY2(!p.bindPeak || p.writePeak, at.constData());
            }
            QVERIFY2(!p.bindLine || p.writeLine || in.heldTrace, at.constData());
            QVERIFY2(!p.bindFill || p.writeFill || (in.heldTrace && in.heldFill),
                     at.constData());
            QVERIFY2(!p.bindPeak || p.writePeak || (in.heldTrace && in.heldPeak),
                     at.constData());

            // No new pixels under the transmit view: no receive trace is
            // redrawn under the transmit axis (drawsSpectrumTrace()).
            if (!in.hasPixels && in.showsTransmitView) {
                QVERIFY2(!p.drawsTrace, at.constData());
            }
        }
    }

    // JJ's case: a 3D pan with a live mesh, frame after frame.
    void mode3D_writesNoTraceBuffer()
    {
        SpectrumTraceFrameInputs in = live2D();
        in.mode3D = true;
        in.dssMeshReady = true;
        for (bool rows : {false, true}) {
            in.dssHasRows = rows;
            const SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
            QVERIFY(!p.writeLine);
            QVERIFY(!p.writeFill);
            QVERIFY(!p.writePeak);
            QCOMPARE(p.drawsDssMesh, rows);
        }
    }

    // GUI-I2: a 3D pan whose mesh is up but whose ring has no rows yet, or
    // whose mesh never came up (the CPU fallback draws), writes no mesh
    // uniforms: nothing binds them.
    void mode3DWithoutMeshDraw_writesNoMeshUniforms()
    {
        SpectrumTraceFrameInputs in = live2D();
        in.mode3D = true;
        in.dssMeshReady = true;
        in.dssHasRows = false;
        SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
        QVERIFY(!p.drawsDssMesh);
        QVERIFY(!p.writeDssMeshUbo);

        in.dssMeshReady = false;
        in.dssHasRows = true;
        in.dssFallbackReady = true;
        p = planSpectrumTraceFrame(in);
        QVERIFY(p.drawsDssFallback);
        QVERIFY(!p.writeDssMeshUbo);

        in.dssMeshReady = true;
        p = planSpectrumTraceFrame(in);
        QVERIFY(p.drawsDssMesh);
        QVERIFY(p.writeDssMeshUbo);
    }

    // GUI-M1: no waterfall pipeline (its shaders failed to load), no
    // waterfall uniform write.
    void noWaterfallPipeline_writesNoWaterfallUniforms()
    {
        SpectrumTraceFrameInputs in = live2D();
        in.waterfallPipeline = false;
        SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
        QVERIFY(!p.drawsWaterfall);
        QVERIFY(!p.writeWaterfallUbo);
        in.waterfallPipeline = true;
        p = planSpectrumTraceFrame(in);
        QVERIFY(p.drawsWaterfall && p.writeWaterfallUbo);
    }

    void panFillOff_writesNoFill()
    {
        SpectrumTraceFrameInputs in = live2D();
        in.panFill = false;
        const SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
        QVERIFY(p.writeLine && p.bindLine);
        QVERIFY(!p.writeFill);
        QVERIFY(!p.bindFill);
        QVERIFY(p.writePeak && p.bindPeak);
    }

    // Leaving 3D with no pixels must not redraw a trace from before 3D.
    void leaving3DWithoutPixels_drawsNoOldTrace()
    {
        SpectrumTraceFrameInputs in = live2D();
        SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
        QVERIFY(p.drawsTrace);

        in = nextFrame(in, p);
        in.mode3D = true;
        in.dssMeshReady = true;
        in.dssHasRows = true;
        p = planSpectrumTraceFrame(in);
        QVERIFY(p.drawsDssMesh);

        in = nextFrame(in, p);
        in.mode3D = false;
        in.hasPixels = false;
        p = planSpectrumTraceFrame(in);
        QVERIFY(!p.drawsTrace);
        QVERIFY(!p.bindLine && !p.bindFill && !p.bindPeak);
    }

    // Turning pan fill on between frames draws no fill left from an
    // earlier trace.
    void fillOnWithoutPixels_drawsNoOldFill()
    {
        SpectrumTraceFrameInputs in = live2D();
        in.panFill = false;
        SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
        QVERIFY(!p.heldFill);

        in = nextFrame(in, p);
        in.panFill = true;
        in.hasPixels = false;
        p = planSpectrumTraceFrame(in);
        QVERIFY(p.bindLine);   // the last trace redraws
        QVERIFY(!p.bindFill);  // its fill was never written
    }

    // A 2D pan with no new pixels redraws the last trace with its fill and
    // peak, as before the fix (the remote pan between frames).
    void noNewPixels_redrawsTheHeldTrace()
    {
        SpectrumTraceFrameInputs in = live2D();
        SpectrumTraceFramePlan p = planSpectrumTraceFrame(in);
        in = nextFrame(in, p);
        in.hasPixels = false;
        p = planSpectrumTraceFrame(in);
        QVERIFY(!p.writeLine && !p.writeFill && !p.writePeak);
        QVERIFY(p.bindLine && p.bindFill && p.bindPeak);
    }

    // MeterWidget's geometry buffer: written and bound only with a
    // pipeline to draw it and vertices to draw.
    void meterGeometry_writtenOnlyWhenBound()
    {
        QVERIFY(!MeterWidget::drawsGeometryLayer(false, 12));
        QVERIFY(!MeterWidget::drawsGeometryLayer(true, 0));
        QVERIFY(!MeterWidget::drawsGeometryLayer(false, 0));
        QVERIFY(MeterWidget::drawsGeometryLayer(true, 12));
    }
};

QTEST_MAIN(TestGpuBufferWriteBindPlan)
#include "tst_gpu_buffer_write_bind_plan.moc"
