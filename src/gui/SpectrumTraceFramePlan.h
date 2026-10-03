// no-port-check: NereusSDR-original. Which GPU trace buffers a spectrum frame writes and binds.

#pragma once

// =================================================================
// src/gui/SpectrumTraceFramePlan.h  (NereusSDR)
// =================================================================
//
// One decision, per GPU frame, of which of the spectrum's 2D trace
// buffers (line, fill, peak hold) the frame writes and which it binds, and
// of whether it writes the 3DSS mesh and waterfall uniform buffers, which
// are dynamic too and bound only by their own draws.
// SpectrumWidget::renderGpuFrame() takes both its uploads and its draw
// calls from it, so the two cannot drift apart.
//
// Why it matters: Qt's Metal backend (and Vulkan and D3D12) keeps each
// partial dynamic-buffer write in the buffer's pending list until the
// buffer is next bound (QRhiMetal::executeBufferHostWritesForSlot,
// qrhimetal.mm, Qt 6.11). A buffer written every frame and never bound
// keeps every frame's copy: a 3D pan wrote the 2D trace unbound and the
// desktop window grew to 23 GB in 2.5 hours. The rule this plan keeps:
// a buffer written in a frame is bound in that frame, and a buffer bound
// without a write in that frame holds the trace the line buffer holds.
//
// Pure and GPU-free, so an offscreen test walks every combination
// (tst_spectrum_trace_frame_plan).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 — Created for the GUI memory leak fix. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 — Fix wave GUI-I2 / GUI-M1: the 3DSS mesh uniform buffer is
//                 written only in a frame that draws the mesh, and the
//                 waterfall's only in a frame that draws the waterfall.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

namespace NereusSDR {

struct SpectrumTraceFrameInputs {
    bool mode3D{false};            // the pan is in 3D (3DSS) mode
    bool dssMeshReady{false};      // the 3DSS mesh pipeline is up
    bool dssHasRows{false};        // the 3DSS ring holds at least one row
    bool dssFallbackReady{false};  // the 3DSS CPU fallback quad can draw
    bool tracePipelines{false};    // the 2D fill and line pipelines exist
    bool traceBuffers{false};      // the 2D line and fill buffers exist
    bool peakBuffer{false};        // the peak hold buffer exists
    bool hasPixels{false};         // this frame has display pixels
    bool panFill{false};           // pan fill is on
    bool peakHoldReady{false};     // peak hold is on and sized to the pixels
    bool showsTransmitView{false}; // keyed with DUP off (the MOX overlay)
    bool waterfallPipeline{false}; // the waterfall pipeline exists
    // What the buffers held entering the frame.
    bool heldTrace{false};         // the line buffer holds a trace
    bool heldFill{false};          // the fill buffer holds that trace's fill
    bool heldPeak{false};          // the peak buffer holds that trace's peak
};

struct SpectrumTraceFramePlan {
    // The spectrum-region draw: at most one of these is true.
    bool drawsDssMesh{false};
    bool drawsDssFallback{false};
    bool drawsTrace{false};
    // The waterfall draw, and the uniform buffers only these draws bind.
    bool drawsWaterfall{false};
    bool writeDssMeshUbo{false};
    bool writeWaterfallUbo{false};
    // Whether the 2D trace region is the one this frame could draw (the
    // trace buffers may be written only then).
    bool traceRegion{false};
    bool writeLine{false};
    bool writeFill{false};
    bool writePeak{false};
    bool bindLine{false};
    bool bindFill{false};
    bool bindPeak{false};
    // What the buffers hold leaving the frame.
    bool heldTrace{false};
    bool heldFill{false};
    bool heldPeak{false};
};

inline SpectrumTraceFramePlan planSpectrumTraceFrame(const SpectrumTraceFrameInputs& in)
{
    SpectrumTraceFramePlan p;

    // The spectrum-region selection, as renderGpuFrame() has always drawn
    // it: the mesh when it is up and has rows, else the 3D fallback quad,
    // else the 2D trace unless 3D owns the region with a live mesh.
    const bool is3D = in.mode3D && in.dssMeshReady;
    p.drawsDssMesh = is3D && in.dssHasRows;
    p.drawsDssFallback = !p.drawsDssMesh && in.mode3D && in.dssFallbackReady;
    p.traceRegion = !p.drawsDssMesh && !p.drawsDssFallback && !is3D && in.tracePipelines;
    // The mesh uniforms are bound only by the mesh draw: a mesh that never
    // came up, or a ring with no rows yet, writes none.
    p.writeDssMeshUbo = p.drawsDssMesh;
    p.drawsWaterfall = in.waterfallPipeline;
    p.writeWaterfallUbo = p.drawsWaterfall;

    // Writes: only when the trace region is drawn this frame.
    p.writeLine = p.traceRegion && in.hasPixels && in.traceBuffers;
    p.writeFill = p.writeLine && in.panFill;
    p.writePeak = p.writeLine && in.peakHoldReady && in.peakBuffer;

    if (p.writeLine) {
        p.heldTrace = true;
        p.heldFill = p.writeFill;
        p.heldPeak = p.writePeak;
    } else if (!p.traceRegion) {
        // Nothing was written, so the buffers hold no trace to redraw
        // later (leaving 3D never redraws a trace from before 3D).
        p.heldTrace = false;
        p.heldFill = false;
        p.heldPeak = false;
    } else {
        // The trace region with nothing new: keep the last trace. A fill or
        // peak belongs to that trace only while the line still holds it (a
        // MOX or remote-axis change drops the trace but not these flags).
        p.heldTrace = in.heldTrace;
        p.heldFill = in.heldTrace && in.heldFill;
        p.heldPeak = in.heldTrace && in.heldPeak;
    }

    // Draws: new pixels, or the last trace unless the transmit view is up
    // without transmit pixels (SpectrumWidget::drawsSpectrumTrace()).
    p.drawsTrace = p.traceRegion && p.heldTrace
        && (in.hasPixels || !in.showsTransmitView);
    p.bindLine = p.drawsTrace;
    p.bindFill = p.drawsTrace && in.panFill && p.heldFill;
    p.bindPeak = p.drawsTrace && in.peakBuffer && p.heldPeak;
    return p;
}

}  // namespace NereusSDR
