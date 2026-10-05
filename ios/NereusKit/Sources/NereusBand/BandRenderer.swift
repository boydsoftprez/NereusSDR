// NereusSDR for iOS: draws the band in Metal: the spectrum, the waterfall, the strip, the scales and the Core's extras
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import Metal
import NereusMedia
import NereusModels
import os

/// Draws one pan's band (R-IOS-11, D7) into any Metal texture: a view's
/// drawable, or an offscreen texture for the tests.
///
/// It draws the Core's frame as the Core sent it (the trace and the
/// waterfall line are already calibrated, normalised and averaged) and the
/// Core's display extras as the Core computed them: the noise-floor line at
/// the datagram's floor, the active peak hold trace and the peak blobs at
/// their trace samples. A frame without its datagram keeps the extras last
/// drawn. Everything else (grid, trace colour and fill, palette, band plan,
/// scales) is the pan's own settings. Over them go the slices' markers
/// (``SliceMarkers``, D10); the flags are the screens' own, above the view.
///
/// The shaders ship as source and compile when the renderer is made, so
/// building needs no Metal toolchain.
public final class BandRenderer {
    public enum SetupError: Error, Equatable {
        case noDevice
        case shaderSourceMissing
        case pipeline
    }

    /// The shader source's resource path, inside the module's bundle.
    public static let shaderResource = "Band"
    public static let shaderSubdirectory = "Shaders"

    // The renderer's own colours.
    /// The desktop's band background, `#0A0A14` (its `SpectrumWidget`
    /// render pass clear colour).
    static let background = SIMD4<Float>(10.0 / 255, 10.0 / 255, 20.0 / 255, 1)
    /// The desktop's frequency-scale row, `#101520` (value from
    /// `src/gui/SpectrumWidget.cpp:4657`).
    static let scaleBackground = SIMD4<Float>(16.0 / 255, 21.0 / 255, 32.0 / 255, 1)
    static let gridColour = SIMD4<Float>(1, 1, 1, 0.12)
    /// The desktop's dBm labels, `#80A0B0` (value from
    /// `src/gui/SpectrumWidget.cpp:4807`).
    static let dbmLabelColour = SIMD4<Float>(128.0 / 255, 160.0 / 255, 176.0 / 255, 1)
    /// The desktop's grid text, yellow `#FFFF00` (value from
    /// `src/gui/SpectrumWidget.h:2656`).
    static let frequencyLabelColour = SIMD4<Float>(1, 1, 0, 1)
    static let stripLabelColour = SIMD4<Float>(1, 1, 1, 1)
    /// The desktop's noise-floor line, its own bright magenta `#FF40FF`
    /// (`SpectrumWidget.h` `m_noiseFloorColor`), and its value in yellow
    /// `#FFFF00` (value from `src/gui/SpectrumWidget.h:2869`).
    static let noiseFloorColour = SIMD4<Float>(1, 64.0 / 255, 1, 1)
    static let noiseFloorTextColour = SIMD4<Float>(1, 1, 0, 1)
    static let noiseFloorFastColour = SIMD4<Float>(200.0 / 255, 200.0 / 255, 200.0 / 255, 1)
    /// The desktop's Active Peak Hold trace, its own gold `#FFD700`
    /// (`SpectrumWidget.cpp` `DisplayActivePeakHoldColor`), and its fill
    /// between the hold and the live trace at 0.18 of it.
    static let peakHoldColour = SIMD4<Float>(1, 215.0 / 255, 0, 1)
    static let peakHoldFillAlpha: Float = 0.18
    /// The desktop's peak blob ring, `#FF4500`, and its value, `#7FFF00`
    /// (values from `src/gui/SpectrumWidget.cpp:792-797`).
    static let peakBlobColour = SIMD4<Float>(1, 69.0 / 255, 0, 1)
    static let peakBlobTextColour = SIMD4<Float>(127.0 / 255, 1, 0, 1)
    static let pausedDim = SIMD4<Float>(0, 0, 0, 0.45)
    /// The 3D scale's column, its left edge and its ticks, as JJ's board
    /// draws them: `#0A0A18` at 220 of 255, `#304050` and `#507080`.
    static let stackScaleBackground = SIMD4<Float>(10.0 / 255, 10.0 / 255, 24.0 / 255, 220.0 / 255)
    static let stackScaleEdge = SIMD4<Float>(48.0 / 255, 64.0 / 255, 80.0 / 255, 1)
    static let stackScaleTick = SIMD4<Float>(80.0 / 255, 112.0 / 255, 128.0 / 255, 1)
    /// The ridge line along each row's top, in points.
    static let stackRidgePoints: CGFloat = 1
    static let spotColour = SIMD4<Float>(1, 1, 1, 1)
    /// The extras' line width, in points. The trace's is the pan's
    /// setting (``BandDisplaySettings/traceWidthPixels(scale:)``).
    static let extrasWidthPoints: CGFloat = 1
    /// A peak blob's ring: its radius and stroke, in points (values from
    /// `src/gui/SpectrumWidget.cpp:2034-2037`). Its value starts this far
    /// right of the blob's centre, its baseline this far above it
    /// (values from `src/gui/SpectrumWidget.cpp:4431`).
    static let blobRadiusPoints: CGFloat = 3
    static let blobStrokePoints: CGFloat = 1
    static let blobValueRightPoints: CGFloat = 5
    static let blobValueUpPoints: CGFloat = 6
    /// The active peak-hold trace's dashes and gaps, in widths of its line:
    /// the desktop's dashed pen, at least 1 wide (values from
    /// `src/gui/SpectrumWidget.cpp:4346-4347`, a dash 4 wide and a gap 2).
    static let peakHoldDashWidths: CGFloat = 4
    static let peakHoldGapWidths: CGFloat = 2
    /// The noise-floor line: dashed from its square marker, set in from the
    /// spectrum's left edge, to as far in from its right edge; the floor's
    /// value beside the marker, its top above the marker's. All in points
    /// (values from `src/gui/SpectrumWidget.cpp:4486-4502, 4528-4534`).
    static let noiseFloorInsetPoints: CGFloat = 40
    static let noiseFloorMarkerPoints: CGFloat = 8
    static let noiseFloorDashPoints: CGFloat = 2
    static let noiseFloorGapPoints: CGFloat = 2
    /// The dashes start this far along their pattern: a gap first.
    static let noiseFloorDashPhasePoints: CGFloat = 2
    static let noiseFloorValueRightPoints: CGFloat = 2
    static let noiseFloorValueUpPoints: CGFloat = 6
    /// The size of the values drawn on the spectrum (the floor's, bold, and
    /// each blob's dBm), in points (values from
    /// `src/gui/SpectrumWidget.cpp:4380, 4530-4531`).
    static let valuePoints: CGFloat = 11
    /// The fine grid: this many parts to a grid step, drawn in dots this
    /// long with gaps twice as long, in points (the desktop's dotted pen,
    /// value from `src/gui/SpectrumWidget.cpp:4094`).
    static let fineGridDivisions = 5
    static let fineGridDotPoints: CGFloat = 1
    static let fineGridGapPoints: CGFloat = 2
    /// The 0 dBm line's dashes and gaps, in points (the desktop's dashed
    /// pen, value from `src/gui/SpectrumWidget.cpp:4275`).
    static let zeroLineDashPoints: CGFloat = 4
    static let zeroLineGapPoints: CGFloat = 2
    /// The receive filter over the waterfall: the desktop's blue at 50 of 255.
    static let rxFilterOnWaterfall = SIMD4<Float>(0, 180.0 / 255, 216.0 / 255, 50.0 / 255)
    /// The classic peak hold: the trace's colour at this strength, dotted
    /// in dots one line wide with gaps two wide, the line three quarters
    /// of the trace's and at least 1 (values from
    /// `src/gui/SpectrumWidget.cpp:4241-4243`).
    static let peakHoldTraceAlpha: Float = 0.55
    static let classicHoldWidthShare: CGFloat = 0.75
    static let classicHoldGapWidths: CGFloat = 2
    /// The split's handle (JJ, 2026-09-26): a small ≡ at the left end of
    /// the frequency-scale row, inside it, marking the row a drag up or
    /// down moves. Its size and inset, in points, and its three bars.
    static let splitHandleSize = CGSize(width: 14, height: 10)
    static let splitHandleInsetPoints: CGFloat = 5
    static let splitHandleBarPoints: CGFloat = 2
    static let splitHandleColour = SIMD4<Float>(0.55, 0.60, 0.68, 1)
    /// The frequency labels' least spacing, in points.
    static let frequencyLabelSpacingPoints: CGFloat = 40

    public let device: any MTLDevice
    public let pixelFormat: MTLPixelFormat

    private let queue: any MTLCommandQueue
    /// The compiled shaders, shared by every renderer on the device.
    let library: any MTLLibrary
    /// Called with each frame's command buffer before anything is encoded
    /// in it: the tests hold a frame on the graphics processor with it.
    var commandBufferMade: ((any MTLCommandBuffer) -> Void)?
    private let colourPipeline: any MTLRenderPipelineState
    private let labelPipeline: any MTLRenderPipelineState
    private let waterfallPipeline: any MTLRenderPipelineState
    private let stackedPipeline: any MTLRenderPipelineState

    /// The 3D view's rows: levels in dBm, and each row's window from
    /// ``stackReferenceHz`` and its span.
    private var stackTexture: (any MTLTexture)?
    private var stackWindows: (any MTLTexture)?
    private var stackReferenceHz: Double = 0
    private var stackGeneration: UInt64?
    private var uploadedRows: UInt64 = 0

    private var waterfallTexture: (any MTLTexture)?
    /// Each waterfall row's coverage: its low edge from
    /// ``waterfallReferenceHz`` and its span, 0 where not known.
    private var coverageTexture: (any MTLTexture)?
    /// The frequency the rows' coverage is measured from, fixed while the
    /// texture lives so a row uploads once.
    private var waterfallReferenceHz: Double = 0
    private var waterfallGeneration: UInt64?
    private var uploadedLines: UInt64 = 0
    private var paletteTexture: (any MTLTexture)?
    private var paletteDrawn: StationCatalog.Palette??
    /// The phone's own gradient last drawn, nil while a catalogue palette is.
    private var customDrawn: [PaletteStop]?
    /// The colour under the waterfall's low level the palette was drawn with.
    private var lowColourDrawn: String?
    private var labelLayers: [String: LabelLayer] = [:]
    /// The values drawn on the spectrum (the floor's, the blobs'), each a
    /// small texture made once for its text and kept while it is drawn.
    private var valueTextures: [ValueKey: ValueTexture] = [:]
    /// This frame's values and where each goes.
    private var valuesDrawn: [(key: ValueKey, origin: CGPoint)] = []
    static let valueTexturesKept = 64
    /// Vertex lists kept between frames so their storage is reused.
    private var underVertices: [ColourVertex] = []
    private var overVertices: [ColourVertex] = []
    private var tracePoints: [SIMD2<Float>] = []
    /// The grid's vertices, and what they were built for.
    private var gridVertices: [ColourVertex] = []
    private var gridKey: GridKey?

    private struct GridKey: Equatable {
        var layout: BandLayout
        var geometry: BandGeometry
        var step: Double
        var colours: [String]
    }
    /// The rows new since the last frame, staged for a copy on the
    /// graphics processor (``RowUploads``): one staging buffer a frame in
    /// flight, reused round the ring as the vertex buffers are.
    private var stagingBuffers: [(any MTLBuffer)?] = Array(repeating: nil, count: BandRenderer.framesInFlight)
    /// Frames in a row each staging buffer has been far bigger than its
    /// frame needed: past ``RowUploads/oversizedFramesKept`` it goes, so a
    /// burst's memory is not held for good.
    private var stagingOversized = Array(repeating: 0, count: BandRenderer.framesInFlight)
    private var rowUploads = RowUploads()
    /// Each staging buffer's length, 0 where none is held (for the tests).
    var stagingBufferLengths: [Int] { stagingBuffers.map { $0?.length ?? 0 } }
    /// Vertex buffers reused round a ring, one pair per frame in flight.
    private var vertexBuffers: [(any MTLBuffer)?] = Array(repeating: nil, count: BandRenderer.framesInFlight * 2)
    private var ring = 0
    private let inFlight = DispatchSemaphore(value: BandRenderer.framesInFlight)
    static let framesInFlight = 3
    /// The extras last drawn, kept for frames that come without theirs.
    public private(set) var heldExtras: DisplayExtras?

    /// What a label layer's text depends on: when it is unchanged, the
    /// layer is not drawn again.
    private struct LabelKey: Equatable {
        var rect: CGRect
        var scale: CGFloat
        var numbers: [Double]
        var plan: StationCatalog.BandPlan?

        init(rect: CGRect, scale: CGFloat, numbers: [Double], plan: StationCatalog.BandPlan? = nil) {
            self.rect = rect
            self.scale = scale
            self.numbers = numbers
            self.plan = plan
        }
    }

    private struct LabelLayer {
        var key: LabelKey
        var rect: CGRect
        var texture: (any MTLTexture)?
    }

    /// One value's text and look.
    struct ValueKey: Hashable {
        var text: String
        var colour: SIMD4<Float>
        var scale: CGFloat
        var bold = false
    }

    /// A value's texture, its size, and where its text's baseline and top
    /// sit in it, in pixels from its top.
    private struct ValueTexture {
        var size: CGSize
        var baseline: CGFloat
        var textTop: CGFloat
        var textLeft: CGFloat
        var texture: (any MTLTexture)?
    }

    struct ColourVertex {
        var position: SIMD2<Float>
        var colour: SIMD4<Float>
    }

    private struct RectUniforms {
        var rect: SIMD4<Float>
        var viewport: SIMD2<Float>
    }

    private struct WaterfallUniforms {
        var rect: SIMD4<Float>
        var columns: UInt32
        var capacity: UInt32
        var count: UInt32
        var nextRow: UInt32
        var background: SIMD4<Float>
        /// The view's low edge from the reference and its span, in hertz.
        var view: SIMD4<Float>
    }

    /// A renderer for `device`, drawing into `pixelFormat` targets.
    public init(device: (any MTLDevice)? = MTLCreateSystemDefaultDevice(),
                pixelFormat: MTLPixelFormat = .bgra8Unorm) throws {
        guard let device, let queue = device.makeCommandQueue() else {
            throw SetupError.noDevice
        }
        self.device = device
        self.pixelFormat = pixelFormat
        self.queue = queue
        let library = try Self.library(for: device)
        self.library = library
        func pipeline(_ vertex: String, _ fragment: String) throws -> any MTLRenderPipelineState {
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = library.makeFunction(name: vertex)
            descriptor.fragmentFunction = library.makeFunction(name: fragment)
            guard descriptor.vertexFunction != nil, descriptor.fragmentFunction != nil else {
                throw SetupError.pipeline
            }
            let attachment = descriptor.colorAttachments[0]
            attachment?.pixelFormat = pixelFormat
            // Every fragment is premultiplied.
            attachment?.isBlendingEnabled = true
            attachment?.sourceRGBBlendFactor = .one
            attachment?.sourceAlphaBlendFactor = .one
            attachment?.destinationRGBBlendFactor = .oneMinusSourceAlpha
            attachment?.destinationAlphaBlendFactor = .oneMinusSourceAlpha
            return try device.makeRenderPipelineState(descriptor: descriptor)
        }
        colourPipeline = try pipeline("bandColourVertex", "bandColourFragment")
        labelPipeline = try pipeline("bandRectVertex", "bandLabelFragment")
        waterfallPipeline = try pipeline("bandRectVertex", "bandWaterfallFragment")
        stackedPipeline = try pipeline("bandStackedVertex", "bandStackedFragment")
    }

    /// The compiled shaders, one library a device. The source compiles the
    /// first time a band is drawn on the device; every renderer after that
    /// takes the library made then, so opening a band does not compile the
    /// shaders again on the main thread.
    private static let libraries = OSAllocatedUnfairLock<[UInt64: any MTLLibrary]>(uncheckedState: [:])

    static func library(for device: any MTLDevice) throws -> any MTLLibrary {
        try libraries.withLockUnchecked { kept in
            if let library = kept[device.registryID] {
                return library
            }
            let library = try device.makeLibrary(source: try shaderSource(), options: nil)
            kept[device.registryID] = library
            return library
        }
    }

    /// The shaders' source, as the package ships it.
    public static func shaderSource() throws -> String {
        guard let url = Bundle.module.url(forResource: shaderResource, withExtension: "metal",
                                          subdirectory: shaderSubdirectory) else {
            throw SetupError.shaderSourceMissing
        }
        return try String(contentsOf: url, encoding: .utf8)
    }

    /// Draws the band into `target`: `frame` (nil before the first), the
    /// waterfall's `history`, the Core's `extras` for this frame (nil when
    /// the frame came without them: the last ones stay) and the `overlays`.
    /// Presents `drawable` when given, and commits. `completed` runs, off the
    /// main thread, once the GPU has finished this draw. Returns the
    /// committed command buffer, or nil when the target is empty.
    @discardableResult
    public func draw(frame: DisplayFrame?, history: WaterfallHistory, stack: StackedTraceHistory? = nil,
                     extras: DisplayExtras?, overlays: BandOverlays, into target: any MTLTexture,
                     present drawable: (any MTLDrawable)? = nil,
                     completed: (@Sendable () -> Void)? = nil,
                     drawTime: (@Sendable (Double) -> Void)? = nil) -> (any MTLCommandBuffer)? {
        let size = CGSize(width: target.width, height: target.height)
        guard target.width > 0, target.height > 0 else {
            return nil
        }
        // At most three frames in flight, so a reused vertex buffer is free.
        inFlight.wait()
        guard let commandBuffer = queue.makeCommandBuffer() else {
            inFlight.signal()
            return nil
        }
        commandBufferMade?(commandBuffer)
        let semaphore = inFlight
        // What this frame cost to draw: its encoding here, from after the
        // wait for a free frame, and the graphics' own time, so frames
        // queued behind others are not counted as slow (the 3D pacer).
        let encodeStart = CFAbsoluteTimeGetCurrent()
        let encodeSeconds = CPUSeconds()
        commandBuffer.addCompletedHandler { buffer in
            semaphore.signal()
            if let drawTime {
                let gpu = buffer.gpuEndTime > buffer.gpuStartTime ? buffer.gpuEndTime - buffer.gpuStartTime : 0
                drawTime(encodeSeconds.value + gpu)
            }
            completed?()
        }
        ring = (ring + 1) % Self.framesInFlight
        rowUploads.begin(with: stagingBuffers[ring])
        let settings = overlays.settings
        let layout = BandLayout(size: size, scale: overlays.scale, settings: settings)
        hold(extras, for: frame)
        let geometry = layout.spectrumGeometry(centerHz: overlays.centerHz, spanHz: overlays.spanHz,
                                               dbmRange: Self.scaleRange(settings, extras: heldExtras))
        syncWaterfall(history, referenceHz: overlays.centerHz - overlays.spanHz / 2)
        if settings.waterfallPaletteId == BandPalette.customPaletteId {
            syncPalette(custom: settings.customPalette, low: settings.waterfallLowColour)
        } else {
            syncPalette(overlays.palette, low: settings.waterfallLowColour)
        }
        // The 3D view, while the pan draws it and has rows to draw.
        let stacked = overlays.stacked.map { overlay in
            (overlay, StackedTraceGeometry(size: geometry.size, settings: settings,
                                           noiseFloorDbm: overlay.noiseFloor(settings), scale: geometry.dbmRange))
        }
        if stacked != nil, let stack {
            syncStack(stack, referenceHz: overlays.centerHz - overlays.spanHz / 2)
        }
        encodeRowUploads(commandBuffer)
        let labels = labelSets(layout: layout, geometry: geometry, overlays: overlays, stacked: stacked?.1)
        for (name, layer) in labels {
            syncLabels(name: name, key: layer.key, items: layer.items)
        }
        for name in labelLayers.keys where labels[name] == nil {
            labelLayers[name] = nil
        }

        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = target
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].storeAction = .store
        pass.colorAttachments[0].clearColor = MTLClearColor(red: Double(Self.background.x),
                                                            green: Double(Self.background.y),
                                                            blue: Double(Self.background.z), alpha: 1)
        guard let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) else {
            commandBuffer.commit()
            return nil
        }
        var viewport = SIMD2<Float>(Float(size.width), Float(size.height))

        // The frequency scale's row, then the waterfall.
        underVertices.removeAll(keepingCapacity: true)
        Self.addRect(layout.frequencyScale, Self.scaleBackground, to: &underVertices)
        Self.addSplitHandle(layout: layout, scale: overlays.scale, to: &underVertices)
        drawColour(underVertices, slot: ring * 2, encoder: encoder, viewport: &viewport)
        drawWaterfall(history, rect: layout.waterfall, view: BandCoverage(centerHz: overlays.centerHz, spanHz: overlays.spanHz),
                      lookBack: overlays.lookBackLines, encoder: encoder, viewport: viewport)
        if let (overlay, stackGeometry) = stacked, let stack {
            drawStack(stack, overlay: overlay, geometry: stackGeometry, settings: settings, markers: overlays.markers,
                      view: BandCoverage(centerHz: overlays.centerHz, spanHz: overlays.spanHz), scale: overlays.scale,
                      target: size, encoder: encoder, viewport: viewport)
        }

        // The spectrum: grid, fill, trace, the Core's extras, the strip.
        var over = overVertices
        overVertices = []
        over.removeAll(keepingCapacity: true)
        defer { overVertices = over }
        addWaterfallOverlays(layout: layout, geometry: geometry, settings: settings, markers: overlays.markers,
                             scale: overlays.scale, to: &over)
        // In 3D the stack takes the flat spectrum's place: no grid, trace,
        // peak hold, zero line or extras over it.
        let flat = stacked == nil
        if flat, settings.grid {
            // The grid changes only with the view, the scale and its colours: kept between frames.
            let key = GridKey(layout: layout, geometry: geometry, step: Self.gridStep(settings),
                              colours: [settings.gridColour, settings.gridFineColour, settings.hGridColour])
            if gridKey != key {
                gridVertices.removeAll(keepingCapacity: true)
                addGrid(layout: layout, geometry: geometry, settings: settings, scale: overlays.scale,
                        to: &gridVertices)
                gridKey = key
            }
            over.append(contentsOf: gridVertices)
        }
        if flat, let hold = overlays.peakHold, hold.count > 1 {
            addPeakHold(hold, coverage: overlays.frameCoverage, geometry: geometry, settings: settings,
                        scale: overlays.scale, to: &over)
        }
        if flat, let frame, !frame.traceDbm.isEmpty {
            addTrace(frame.traceDbm, coverage: overlays.frameCoverage, geometry: geometry, settings: settings,
                     scale: overlays.scale, to: &over)
        }
        if flat, settings.showZeroLine {
            addZeroLine(geometry: geometry, settings: settings, scale: overlays.scale, to: &over)
        }
        valuesDrawn.removeAll(keepingCapacity: true)
        if flat, let extras = heldExtras {
            addExtras(extras, frame: frame, coverage: overlays.frameCoverage, layout: layout, geometry: geometry,
                      settings: settings, scale: overlays.scale, to: &over)
        }
        if let (_, stackGeometry) = stacked, settings.showDbmScale {
            addStackScale(layout: layout, geometry: stackGeometry, step: Self.gridStep(settings), scale: overlays.scale,
                          to: &over)
        }
        if settings.bandPlanStrip {
            addStrip(overlays.bandPlan, layout: layout, geometry: geometry, scale: overlays.scale, to: &over)
        }
        if !overlays.markers.isEmpty {
            // With Slice Shadow on, the passband leans back on the surface
            // instead of lying flat over the spectrum; the waterfall's stays.
            addMarkers(overlays.markers, layout: layout, geometry: geometry, settings: settings, scale: overlays.scale,
                       flatPassband: !(stacked != nil && settings.threeDSliceShadow), to: &over)
        }
        addWaterfallZeroLine(layout: layout, geometry: geometry, settings: settings, markers: overlays.markers,
                             scale: overlays.scale, to: &over)
        if overlays.paused {
            Self.addRect(CGRect(origin: .zero, size: size), Self.pausedDim, to: &over)
        }
        drawColour(over, slot: ring * 2 + 1, encoder: encoder, viewport: &viewport)

        for name in labelLayers.keys.sorted() {
            if let layer = labelLayers[name], let texture = layer.texture {
                var uniforms = RectUniforms(rect: Self.vector(layer.rect), viewport: viewport)
                encoder.setRenderPipelineState(labelPipeline)
                encoder.setVertexBytes(&uniforms, length: MemoryLayout<RectUniforms>.stride, index: 0)
                encoder.setFragmentTexture(texture, index: 0)
                encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
            }
        }
        drawValues(encoder: encoder, viewport: viewport)
        encoder.endEncoding()
        if let drawable {
            commandBuffer.present(drawable)
        }
        encodeSeconds.value = CFAbsoluteTimeGetCurrent() - encodeStart
        commandBuffer.commit()
        return commandBuffer
    }

    /// Forgets the held extras (a new session or a new endpoint).
    public func forgetExtras() {
        heldExtras = nil
    }

    // MARK: The Core's extras

    private func hold(_ extras: DisplayExtras?, for frame: DisplayFrame?) {
        if let extras {
            if let frame, extras.endpointId != frame.endpointId || extras.contextGeneration != frame.contextGeneration {
                // Another endpoint's or an older context's: not this band's.
                return
            }
            heldExtras = extras
            return
        }
        if let held = heldExtras, let frame,
           held.endpointId != frame.endpointId || held.contextGeneration != frame.contextGeneration {
            // A new context: the old extras' samples no longer fit its trace.
            heldExtras = nil
        }
    }

    /// The Core's extras as the desktop draws its own: the peak-hold trace
    /// dashed in gold over the live trace, shaded down to it when its Fill
    /// is on; each peak blob a ring with its dBm beside it; the noise
    /// floor a dashed magenta line from a square marker, with its value.
    private func addExtras(_ extras: DisplayExtras, frame: DisplayFrame?, coverage: BandCoverage?, layout: BandLayout,
                           geometry: BandGeometry, settings: BandDisplaySettings, scale: CGFloat,
                           to vertices: inout [ColourVertex]) {
        if settings.activePeakHold, let hold = extras.peakHoldDbm, hold.count > 1 {
            let points = hold.indices.map { index in
                SIMD2<Float>(Float(Self.x(sample: index, of: hold.count, coverage: coverage, geometry: geometry)),
                             Float(Self.clampY(geometry.y(forDbm: Double(hold[index])), geometry)))
            }
            if settings.activePeakHoldFill, let trace = frame?.traceDbm, trace.count == hold.count {
                var shade = BandPalette.rgbaWithAlpha(settings.peakHoldColour, fallback: Self.peakHoldColour)
                shade.w = Self.peakHoldFillAlpha
                for index in 0..<(points.count - 1) {
                    let a = points[index]
                    let b = points[index + 1]
                    let belowA = Float(Self.traceY(trace[index], geometry))
                    let belowB = Float(Self.traceY(trace[index + 1], geometry))
                    for corner in [a, b, SIMD2(b.x, max(b.y, belowB)), a, SIMD2(b.x, max(b.y, belowB)),
                                   SIMD2(a.x, max(a.y, belowA))] {
                        vertices.append(ColourVertex(position: corner, colour: shade))
                    }
                }
            }
            // At least a point wide, as the desktop's pen is at least 1.
            let width = Float(max(settings.traceWidthPixels(scale: scale), scale))
            Self.addDashedPolyline(points, width: width, dash: width * Float(Self.peakHoldDashWidths),
                                   gap: width * Float(Self.peakHoldGapWidths),
                                   colour: BandPalette.rgbaWithAlpha(settings.peakHoldColour, fallback: Self.peakHoldColour),
                                   to: &vertices)
        }
        if settings.peakBlobs, let blobs = extras.peakBlobs {
            let traceSamples = frame?.traceDbm.count ?? extras.peakHoldDbm?.count ?? 0
            let radius = Float(Self.blobRadiusPoints * scale)
            let stroke = Float(max(1, Self.blobStrokePoints * scale))
            for blob in blobs where blob.traceSample >= 0 && blob.traceSample < traceSamples && blob.dbm.isFinite {
                let centre = SIMD2<Float>(Float(Self.x(sample: blob.traceSample, of: traceSamples, coverage: coverage,
                                                       geometry: geometry)),
                                          Float(Self.clampY(geometry.y(forDbm: Double(blob.dbm)), geometry)))
                Self.addRing(centre, radius: radius, stroke: stroke,
                             colour: BandPalette.rgbaWithAlpha(settings.peakBlobColour, fallback: Self.peakBlobColour),
                             to: &vertices)
                let key = ValueKey(text: Self.valueText(blob.dbm),
                                   colour: BandPalette.rgbaWithAlpha(settings.peakBlobTextColour,
                                                                     fallback: Self.peakBlobTextColour),
                                   scale: scale)
                let value = valueTexture(key)
                // Its text starts right of the centre, its baseline above it.
                let origin = CGPoint(x: CGFloat(centre.x) + Self.blobValueRightPoints * scale - value.textLeft,
                                     y: CGFloat(centre.y) - Self.blobValueUpPoints * scale - value.baseline)
                valuesDrawn.append((key, Self.clampOrigin(origin, size: value.size, within: layout,
                                                          geometry: geometry)))
            }
        }
        if settings.noiseFloorLine, let floor = extras.noiseFloorDbm, floor.isFinite {
            // The desktop's line width, and its fast colour for all three
            // while the floor is in fast attack.
            let fast = extras.noiseFloorFastAttack == true
            let lineWidth = max(1, CGFloat(Self.noiseFloorLineWidth(settings)) * scale)
            let y = Self.clampY(geometry.y(forDbm: Double(floor)), geometry).rounded(.down)
            let marker = Double(Self.noiseFloorMarkerPoints * scale).rounded()
            let inset = Double(Self.noiseFloorInsetPoints * scale).rounded()
            let left = Double(layout.spectrum.minX) + inset
            let right = Double(layout.spectrum.maxX) - inset
            let floorColour = fast
                ? BandPalette.rgbaWithAlpha(settings.noiseFloorFastColour, fallback: Self.noiseFloorFastColour)
                : BandPalette.rgbaWithAlpha(settings.noiseFloorColour, fallback: Self.noiseFloorColour)
            Self.addRect(CGRect(x: left, y: y - marker, width: marker, height: marker), floorColour, to: &vertices)
            Self.addDashesAcross(y: y, height: Double(lineWidth),
                                 from: left + Double(Self.noiseFloorDashPhasePoints * scale), to: right,
                                 dash: Double(Self.noiseFloorDashPoints * scale),
                                 gap: Double(Self.noiseFloorGapPoints * scale), colour: floorColour,
                                 to: &vertices)
            let key = ValueKey(text: Self.valueText(floor),
                               colour: fast ? floorColour
                                   : BandPalette.rgbaWithAlpha(settings.noiseFloorTextColour,
                                                               fallback: Self.noiseFloorTextColour),
                               scale: scale, bold: true)
            let value = valueTexture(key)
            // Its text starts right of the marker, its top above the marker's.
            let origin = CGPoint(x: left + marker + Double(Self.noiseFloorValueRightPoints * scale) - Double(value.textLeft),
                                 y: y - marker - Double(Self.noiseFloorValueUpPoints * scale) - Double(value.textTop))
            valuesDrawn.append((key, Self.clampOrigin(origin, size: value.size, within: layout, geometry: geometry)))
        }
    }

    /// A value as the desktop's spectrum shows it: dBm to one decimal
    /// place (value from `src/gui/SpectrumWidget.cpp:4431, 4528`).
    static func valueText(_ dbm: Float) -> String {
        String(format: "%.1f", Double(dbm))
    }

    /// Keeps a value inside the spectrum, clear of the dBm scale.
    private static func clampOrigin(_ origin: CGPoint, size: CGSize, within layout: BandLayout,
                                    geometry: BandGeometry) -> CGPoint {
        let right = layout.dbmScale.minX - size.width
        let bottom = CGFloat(geometry.size.height) - size.height
        return CGPoint(x: max(0, min(origin.x, right)), y: max(0, min(origin.y, bottom)))
    }

    /// Where a trace sample's level falls, the foot for a missing one.
    private static func traceY(_ dbm: Float, _ geometry: BandGeometry) -> Double {
        dbm.isFinite ? clampY(geometry.y(forDbm: Double(dbm)), geometry) : Double(geometry.size.height)
    }

    // MARK: The slices' markers

    /// Each slice's marker in its turn, the selected slice's last (D10):
    /// the shaded passband in the spectrum above the strip and in the
    /// waterfall, its two edges there too, the centre line down the whole
    /// band, and the triangle under the slice's flag.
    private func addMarkers(_ markers: [SliceMarkers.Marker], layout: BandLayout, geometry: BandGeometry,
                            settings: BandDisplaySettings, scale: CGFloat, flatPassband: Bool = true,
                            to vertices: inout [ColourVertex]) {
        let width = Double(geometry.size.width)
        let spectrumBottom = Double(layout.strip.minY)
        let waterfall = layout.waterfall
        let shade = BandPalette.rgba(settings.passbandColour,
                                     alpha: Float(min(max(settings.passbandOpacity, 0), 1)))
        let edgeWidth = Double(SliceMarkers.edgeWidthPoints * scale)
        for marker in markers {
            if marker.foreign {
                addForeignMarker(marker, layout: layout, geometry: geometry, scale: scale, to: &vertices)
                continue
            }
            let centreX = geometry.x(forHz: marker.centerHz)
            let lowX = geometry.x(forHz: marker.passbandHz.lowerBound)
            let highX = geometry.x(forHz: marker.passbandHz.upperBound)
            let fillLow = min(max(lowX, 0), width)
            let fillHigh = min(max(highX, 0), width)
            if marker.showsPassband, let shade, fillHigh > fillLow {
                if flatPassband {
                    Self.addRect(CGRect(x: fillLow, y: 0, width: fillHigh - fillLow, height: spectrumBottom), shade,
                                 to: &vertices)
                }
                Self.addRect(CGRect(x: fillLow, y: Double(waterfall.minY), width: fillHigh - fillLow,
                                    height: Double(waterfall.height)), shade, to: &vertices)
            }
            for edgeX in [lowX, highX] where edgeX >= 0 && edgeX <= width {
                let x = edgeX.rounded(.down)
                Self.addRect(CGRect(x: x, y: 0, width: edgeWidth, height: spectrumBottom), marker.style.edge,
                             to: &vertices)
                Self.addRect(CGRect(x: x, y: Double(waterfall.minY), width: edgeWidth, height: Double(waterfall.height)),
                             marker.style.edge, to: &vertices)
            }
            guard centreX >= 0, centreX <= width else {
                continue
            }
            let narrow = Double(SliceMarkers.narrowWithinPoints * scale)
            let lineWidth = Double((abs(centreX - lowX) <= narrow || abs(centreX - highX) <= narrow
                ? SliceMarkers.narrowLineWidthPoints : SliceMarkers.lineWidthPoints) * scale)
            Self.addRect(CGRect(x: centreX - lineWidth / 2, y: 0, width: lineWidth, height: Double(layout.size.height)),
                         marker.style.line, to: &vertices)
            let half = Float(SliceMarkers.triangleSize.width * scale / 2)
            let height = Float(SliceMarkers.triangleSize.height * scale)
            let top = Float(max(0, marker.triangleTopPoints * scale))
            let x = Float(centreX)
            for corner in [SIMD2<Float>(x - half, top), SIMD2(x + half, top), SIMD2(x, top + height)] {
                vertices.append(ColourVertex(position: corner, colour: marker.style.triangle))
            }
        }
    }

    /// Another device's slice (``ForeignSliceMarkers``): its two edges
    /// dashed grey in the spectrum and the waterfall, no shading, its
    /// centre line dashed down the whole band, and its triangle hollow.
    private func addForeignMarker(_ marker: SliceMarkers.Marker, layout: BandLayout, geometry: BandGeometry,
                                  scale: CGFloat, to vertices: inout [ColourVertex]) {
        let width = Double(geometry.size.width)
        let spectrumBottom = Double(layout.strip.minY)
        let waterfall = layout.waterfall
        let edgeWidth = Double(SliceMarkers.edgeWidthPoints * scale)
        let edgeDash = Double(ForeignSliceMarkers.edgeDashPoints * scale)
        let edgeGap = Double(ForeignSliceMarkers.edgeGapPoints * scale)
        for hz in [marker.passbandHz.lowerBound, marker.passbandHz.upperBound] {
            let edgeX = geometry.x(forHz: hz)
            guard edgeX >= 0, edgeX <= width else {
                continue
            }
            let x = edgeX.rounded(.down)
            Self.addDashes(x: x, width: edgeWidth, from: 0, to: spectrumBottom, dash: edgeDash, gap: edgeGap,
                           colour: marker.style.edge, to: &vertices)
            Self.addDashes(x: x, width: edgeWidth, from: Double(waterfall.minY), to: Double(waterfall.maxY),
                           dash: edgeDash, gap: edgeGap, colour: marker.style.edge, to: &vertices)
        }
        let centreX = geometry.x(forHz: marker.centerHz)
        guard centreX >= 0, centreX <= width else {
            return
        }
        let lineWidth = Double(SliceMarkers.lineWidthPoints * scale)
        Self.addDashes(x: centreX - lineWidth / 2, width: lineWidth, from: 0, to: Double(layout.size.height),
                       dash: Double(ForeignSliceMarkers.lineDashPoints * scale),
                       gap: Double(ForeignSliceMarkers.lineGapPoints * scale), colour: marker.style.line,
                       to: &vertices)
        let half = Float(SliceMarkers.triangleSize.width * scale / 2)
        let height = Float(SliceMarkers.triangleSize.height * scale)
        let top = Float(max(0, marker.triangleTopPoints * scale))
        let x = Float(centreX)
        let corners = [SIMD2<Float>(x - half, top), SIMD2(x + half, top), SIMD2(x, top + height)]
        for corner in corners {
            vertices.append(ColourVertex(position: corner, colour: marker.style.triangle))
        }
        // Hollow: the band's dark inside, the triangle shrunk about its
        // incentre so a ring of the colour stays at every side.
        let ring = Float(ForeignSliceMarkers.triangleRingPoints * scale)
        let sides = [corners[1] - corners[2], corners[2] - corners[0], corners[0] - corners[1]]
            .map { ($0 * $0).sum().squareRoot() }
        let perimeter = sides.reduce(0, +)
        let area = abs((corners[1].x - corners[0].x) * (corners[2].y - corners[0].y)
            - (corners[2].x - corners[0].x) * (corners[1].y - corners[0].y)) / 2
        let inradius = perimeter > 0 ? 2 * area / perimeter : 0
        if inradius > ring {
            let incentre = (corners[0] * sides[0] + corners[1] * sides[1] + corners[2] * sides[2]) / perimeter
            let shrink = (inradius - ring) / inradius
            for corner in corners {
                vertices.append(ColourVertex(position: incentre + (corner - incentre) * shrink,
                                             colour: ForeignSliceMarkers.triangleInside))
            }
        }
    }

    /// A column `width` wide at `x`, drawn from `top` to `bottom` in dashes
    /// `dash` long with `gap` left out between them.
    private static func addDashes(x: Double, width: Double, from top: Double, to bottom: Double, dash: Double,
                                  gap: Double, colour: SIMD4<Float>, to vertices: inout [ColourVertex]) {
        guard dash > 0, bottom > top else {
            return
        }
        var y = top
        while y < bottom {
            addRect(CGRect(x: x, y: y, width: width, height: min(dash, bottom - y)), colour, to: &vertices)
            y += dash + max(gap, 0)
        }
    }

    // MARK: The spectrum

    /// The band-plan strip as the desktop draws it (D79): each segment
    /// opaque in its colour dimmed by its licence class, a separator at
    /// its left edge, and a white dot at each of the plan's spots, mid-strip.
    private func addStrip(_ plan: StationCatalog.BandPlan?, layout: BandLayout, geometry: BandGeometry,
                          scale: CGFloat, to vertices: inout [ColourVertex]) {
        let rect = layout.strip
        guard rect.width > 0, rect.height > 0 else {
            return
        }
        let strip = BandPlanStrip(plan: plan, geometry: geometry, rightX: Double(rect.maxX))
        let separator = BandPlanStrip.separator
        let separatorColour = SIMD4<Float>(Float(separator.red) / 255, Float(separator.green) / 255,
                                           Float(separator.blue) / 255, Float(separator.alpha) / 255)
        let separatorWidth = max(1, scale.rounded())
        for piece in strip.pieces {
            guard let fill = BandPlanStrip.fill(colour: piece.colour, licence: piece.licence) else {
                continue
            }
            let colour = SIMD4<Float>(Float(fill.red) / 255, Float(fill.green) / 255, Float(fill.blue) / 255, 1)
            Self.addRect(CGRect(x: piece.lowX, y: rect.minY, width: piece.width, height: rect.height), colour,
                         to: &vertices)
            Self.addRect(CGRect(x: piece.lowX.rounded(.down), y: rect.minY, width: separatorWidth, height: rect.height),
                         separatorColour, to: &vertices)
        }
        let radius = Float(BandPlanStrip.spotRadiusPoints * Double(scale))
        let middle = Float(rect.midY)
        for x in strip.spotXs {
            Self.addDisc(SIMD2(Float(x), middle), radius: radius, colour: Self.spotColour, to: &vertices)
        }
    }

    /// The grid as the desktop draws it: the horizontal (dB) lines, the
    /// vertical lines at each frequency label, and between those, fine
    /// dotted lines at a fifth of the step, each in its own colour.
    private func addGrid(layout: BandLayout, geometry: BandGeometry, settings: BandDisplaySettings, scale: CGFloat,
                         to vertices: inout [ColourVertex]) {
        let width = max(1, scale.rounded(.down))
        let horizontal = BandPalette.rgbaWithAlpha(settings.hGridColour, fallback: Self.gridColour)
        let vertical = BandPalette.rgbaWithAlpha(settings.gridColour, fallback: Self.gridColour)
        let fine = BandPalette.rgbaWithAlpha(settings.gridFineColour, fallback: Self.gridColour)
        for level in geometry.dbmTicks(stepDb: Self.gridStep(settings)) {
            let y = geometry.y(forDbm: level).rounded(.down)
            Self.addRect(CGRect(x: 0, y: y, width: geometry.size.width, height: width), horizontal, to: &vertices)
        }
        let ticks = geometry.frequencyTicks(minimumSpacing: Double(Self.frequencyLabelSpacingPoints * scale))
        let height = Double(layout.spectrum.height)
        for hz in ticks.ticks {
            let x = geometry.x(forHz: hz).rounded(.down)
            Self.addRect(CGRect(x: x, y: 0, width: width, height: layout.spectrum.height), vertical, to: &vertices)
        }
        guard ticks.stepHz > 0, fine.w > 0 else {
            return
        }
        let fineStep = ticks.stepHz / Double(Self.fineGridDivisions)
        let low = geometry.centerHz - geometry.spanHz / 2
        var hz = (low / fineStep).rounded(.up) * fineStep
        let dot = Double(Self.fineGridDotPoints * scale)
        while hz <= low + geometry.spanHz {
            let steps = (hz / ticks.stepHz)
            if abs(steps - steps.rounded()) > 1e-6 {
                Self.addDashes(x: geometry.x(forHz: hz).rounded(.down), width: Double(width), from: 0, to: height,
                               dash: dot, gap: Double(Self.fineGridGapPoints * scale), colour: fine, to: &vertices)
            }
            hz += fineStep
        }
    }

    /// The 0 dBm line across the spectrum, dashed, in the receive zero
    /// line's colour, when 0 dBm is on the scale.
    private func addZeroLine(geometry: BandGeometry, settings: BandDisplaySettings, scale: CGFloat,
                             to vertices: inout [ColourVertex]) {
        let y = geometry.y(forDbm: 0)
        guard y >= 0, y <= Double(geometry.size.height) else {
            return
        }
        let colour = BandPalette.rgbaWithAlpha(settings.rxZeroLineColour)
        Self.addDashesAcross(y: y.rounded(.down), height: Double(max(1, scale.rounded(.down))), from: 0,
                             to: Double(geometry.size.width), dash: Double(Self.zeroLineDashPoints * scale),
                             gap: Double(Self.zeroLineGapPoints * scale), colour: colour, to: &vertices)
    }

    /// Over the waterfall: its opacity (the band's background showing
    /// through), the active slice's receive filter as a band, and its zero
    /// line, as the pan's settings ask.
    private func addWaterfallOverlays(layout: BandLayout, geometry: BandGeometry, settings: BandDisplaySettings,
                                      markers: [SliceMarkers.Marker], scale: CGFloat,
                                      to vertices: inout [ColourVertex]) {
        let waterfall = layout.waterfall
        guard waterfall.height > 0 else {
            return
        }
        let opacity = min(max(settings.waterfallOpacityPercent, 0), 100)
        if opacity < 100 {
            var dim = Self.background
            dim.w = 1 - Float(opacity) / 100
            Self.addRect(waterfall, dim, to: &vertices)
        }
        guard let active = markers.last(where: { !$0.foreign }) else {
            return
        }
        if settings.showRxFilterOnWaterfall {
            let low = max(0, geometry.x(forHz: active.passbandHz.lowerBound))
            let high = min(Double(geometry.size.width), geometry.x(forHz: active.passbandHz.upperBound))
            if high > low {
                Self.addRect(CGRect(x: low, y: Double(waterfall.minY), width: high - low, height: Double(waterfall.height)),
                             Self.rxFilterOnWaterfall, to: &vertices)
            }
        }
    }

    /// The receive zero line down the waterfall at the active slice, over
    /// its marker's own line, as the pan's settings ask.
    private func addWaterfallZeroLine(layout: BandLayout, geometry: BandGeometry, settings: BandDisplaySettings,
                                      markers: [SliceMarkers.Marker], scale: CGFloat,
                                      to vertices: inout [ColourVertex]) {
        let waterfall = layout.waterfall
        guard settings.showRxZeroLineOnWaterfall, waterfall.height > 0,
              let active = markers.last(where: { !$0.foreign }) else {
            return
        }
        let x = geometry.x(forHz: active.centerHz)
        if x >= 0, x <= Double(geometry.size.width) {
            let width = Double(max(1, scale.rounded(.down)))
            Self.addRect(CGRect(x: x - width / 2, y: Double(waterfall.minY), width: width, height: Double(waterfall.height)),
                         BandPalette.rgbaWithAlpha(settings.rxZeroLineColour), to: &vertices)
        }
    }

    /// The classic peak hold: dotted, in the trace's colour at a little
    /// over half its strength, three quarters of the trace's width and at
    /// least a point.
    private func addPeakHold(_ hold: [Float], coverage: BandCoverage?, geometry: BandGeometry,
                             settings: BandDisplaySettings, scale: CGFloat, to vertices: inout [ColourVertex]) {
        let points = hold.indices.map { index in
            SIMD2<Float>(Float(Self.x(sample: index, of: hold.count, coverage: coverage, geometry: geometry)),
                         Float(Self.traceY(hold[index], geometry)))
        }
        var colour = BandPalette.rgba(settings.traceColour) ?? SIMD4(0.13, 0.83, 0.93, 1)
        colour.w = Self.peakHoldTraceAlpha
        let width = Float(max(scale, settings.traceWidthPixels(scale: scale) * Self.classicHoldWidthShare))
        Self.addDashedPolyline(points, width: width, dash: width, gap: width * Float(Self.classicHoldGapWidths),
                               colour: colour, to: &vertices)
    }

    /// The scale drawn: the settings' own. The grid following the noise
    /// floor is ``GridFloorTracker``'s, applied to the settings the band
    /// draws with, never per frame here.
    static func scaleRange(_ settings: BandDisplaySettings, extras: DisplayExtras?) -> ClosedRange<Double> {
        settings.scaleRange
    }

    /// The noise-floor line's width in points, the desktop's 1 to 5.
    static func noiseFloorLineWidth(_ settings: BandDisplaySettings) -> Double {
        let width = settings.noiseFloorLineWidth
        return width.isFinite ? min(max(width, 1), 5) : 1
    }

    /// The fill's opacity as the desktop reads its Fill Alpha: four tenths
    /// of it flat, the whole of it at the gradient's top.
    static func fillAlpha(_ settings: BandDisplaySettings) -> Float {
        let strength = Float(min(max(settings.traceFillOpacity.isFinite ? settings.traceFillOpacity : 0, 0), 1))
        return settings.traceGradient ? strength : strength * 0.4
    }

    private static func gridStep(_ settings: BandDisplaySettings) -> Double {
        settings.gridStepDb > 0 ? settings.gridStepDb : 10
    }

    /// Where sample `index` of `samples` sits across the view: at its own
    /// frequency when the frame covers other frequencies than the view (a
    /// pan or zoom ahead of the Core), else at its slice of the view.
    static func x(sample index: Int, of samples: Int, coverage: BandCoverage?, geometry: BandGeometry) -> Double {
        guard let coverage, coverage.spanHz > 0 else {
            return geometry.x(forTraceSample: index, traceSamples: samples)
        }
        return geometry.x(forHz: coverage.hz(forSample: index, samples: samples))
    }

    private func addTrace(_ trace: [Float], coverage: BandCoverage?, geometry: BandGeometry,
                          settings: BandDisplaySettings, scale: CGFloat, to vertices: inout [ColourVertex]) {
        let count = trace.count
        var points = tracePoints
        tracePoints = []
        points.removeAll(keepingCapacity: true)
        defer { tracePoints = points }
        for index in 0..<count {
            let value = trace[index]
            let y = value.isFinite ? Self.clampY(geometry.y(forDbm: Double(value)), geometry) : Double(geometry.size.height)
            points.append(SIMD2(Float(Self.x(sample: index, of: count, coverage: coverage, geometry: geometry)), Float(y)))
        }
        // Carry the ends to the edges of what the frame covers: the band's
        // edges, or, while the view is ahead of the Core, the frame's own
        // edges, leaving the rest of the view empty.
        if let first = points.first, let last = points.last {
            let low = coverage.map { geometry.x(forHz: $0.lowHz) } ?? 0
            let high = coverage.map { geometry.x(forHz: $0.highHz) } ?? Double(geometry.size.width)
            points.insert(SIMD2(Float(low), first.y), at: 0)
            points.append(SIMD2(Float(high), last.y))
        }
        let colour = BandPalette.rgba(settings.traceColour) ?? SIMD4(0.13, 0.83, 0.93, 1)
        if settings.traceFill {
            let bottom = Float(geometry.size.height)
            let fill = Self.fillAlpha(settings)
            // Flat, or the gradient: the whole strength at the spectrum's
            // top, nothing at its foot, so each vertex takes its height's share.
            let gradient = settings.traceGradient && bottom > 0
            let perPixel = gradient ? fill / bottom : 0
            var foot = colour
            foot.w = gradient ? 0 : fill
            var topA = foot
            var topB = foot
            for index in 0..<(points.count - 1) {
                let a = points[index]
                let b = points[index + 1]
                if gradient {
                    topA.w = max(0, fill - a.y * perPixel)
                    topB.w = max(0, fill - b.y * perPixel)
                }
                vertices.append(ColourVertex(position: a, colour: topA))
                vertices.append(ColourVertex(position: b, colour: topB))
                vertices.append(ColourVertex(position: SIMD2(a.x, bottom), colour: foot))
                vertices.append(ColourVertex(position: b, colour: topB))
                vertices.append(ColourVertex(position: SIMD2(b.x, bottom), colour: foot))
                vertices.append(ColourVertex(position: SIMD2(a.x, bottom), colour: foot))
            }
        }
        Self.addPolyline(points, width: Float(settings.traceWidthPixels(scale: scale)), colour: colour, to: &vertices)
    }

    // MARK: Labels

    private struct LabelSet {
        var key: LabelKey
        /// Built only when the key changed.
        var items: () -> [BandLabels.Item]
    }

    /// Each label layer's key, and how to build its labels when the key
    /// changed. The keys are cheap, so an unchanged band builds nothing.
    private func labelSets(layout: BandLayout, geometry: BandGeometry, overlays: BandOverlays,
                           stacked: StackedTraceGeometry? = nil) -> [String: LabelSet] {
        let scale = overlays.scale
        let settings = overlays.settings
        let range = [geometry.dbmRange.lowerBound, geometry.dbmRange.upperBound]
        var sets: [String: LabelSet] = [:]

        // The dBm scale, along the spectrum's right edge.
        let rect = layout.dbmScale
        if let stacked, rect.width > 0, rect.height > 0 {
            // In 3D the scale reads up the front row, from the floor.
            let step = Self.gridStep(settings)
            let key = LabelKey(rect: rect, scale: scale,
                               numbers: [stacked.floorDbm.rounded(), stacked.heightRangeDb, step,
                                         stacked.shape.ridge, geometry.size.height, 3])
            sets["a-dbm"] = LabelSet(key: key) {
                let half = BandLayout.labelPoints * scale * 0.6
                let arrows = layout.dbmArrows.maxY
                var lastY = CGFloat.infinity
                return stacked.scaleTicks(stepDb: step).compactMap { tick -> BandLabels.Item? in
                    let y = CGFloat(tick.y)
                    // Clear of the arrows, and of the label below it.
                    guard y >= arrows + half, y <= rect.height - half,
                          lastY - y >= BandLayout.labelPoints * scale * 1.2 else {
                        return nil
                    }
                    lastY = y
                    return BandLabels.Item(text: String(Int(tick.dbm.rounded())), x: rect.width - 3 * scale, y: y,
                                           alignment: .trailing, colour: Self.dbmLabelColour,
                                           points: BandLayout.labelPoints)
                }
            }
        } else if rect.width > 0, rect.height > 0 {
            let step = Self.gridStep(settings)
            sets["a-dbm"] = LabelSet(key: LabelKey(rect: rect, scale: scale, numbers: range + [step, geometry.size.height])) {
                let half = BandLayout.labelPoints * scale * 0.6
                // Every grid line is labelled while the labels have room;
                // in a short band only every second, third and so on.
                let pixelsPerStep = CGFloat(geometry.y(forDbm: geometry.dbmRange.upperBound - step))
                let every = max(1, Int((BandLayout.labelPoints * scale * 1.4 / max(pixelsPerStep, 1)).rounded(.up)))
                let ticks = geometry.dbmTicks(stepDb: step)
                // Under the arrow buttons at the scale's top, no labels.
                let arrows = layout.dbmArrows.maxY
                return ticks.enumerated().compactMap { offset, level -> BandLabels.Item? in
                    let y = CGFloat(geometry.y(forDbm: level))
                    guard offset % every == 0, y >= arrows + half, y <= rect.height - half else {
                        return nil
                    }
                    return BandLabels.Item(text: String(Int(level.rounded())), x: rect.width - 3 * scale, y: y,
                                           alignment: .trailing, colour: Self.dbmLabelColour,
                                           points: BandLayout.labelPoints)
                }
            }
        }

        // The frequency scale.
        let scaleRect = layout.frequencyScale
        let alignment = settings.frequencyLabelAlignment
        let textColour = BandPalette.rgbaWithAlpha(settings.gridTextColour, fallback: Self.frequencyLabelColour)
        if scaleRect.width > 0, scaleRect.height > 0, alignment != .off {
            let alignmentNumber = Double(FrequencyLabelAlignment.allCases.firstIndex(of: alignment) ?? 0)
            let key = LabelKey(rect: scaleRect, scale: scale,
                               numbers: [geometry.centerHz, geometry.spanHz, alignmentNumber]
                                   + textColour.indices.map { Double(textColour[$0]) })
            sets["b-scale"] = LabelSet(key: key) {
                let ticks = geometry.frequencyTicks(minimumSpacing: Double(Self.frequencyLabelSpacingPoints * scale))
                return ticks.ticks.compactMap { hz -> BandLabels.Item? in
                    let text = BandGeometry.frequencyLabel(hz: hz, stepHz: ticks.stepHz)
                    let x = CGFloat(geometry.x(forHz: hz))
                    let width = BandLabels.width(of: text, points: BandLayout.frequencyLabelPoints, scale: scale)
                    let gap = 3 * scale
                    // Where the text starts and ends for its alignment on the tick.
                    let (start, labelAlignment, anchor): (CGFloat, BandLabels.Alignment, CGFloat)
                    switch alignment {
                    case .left:
                        (start, labelAlignment, anchor) = (x - width - gap, .trailing, x - gap)
                    case .right:
                        (start, labelAlignment, anchor) = (x + gap, .leading, x + gap)
                    case .centre, .auto, .off:
                        (start, labelAlignment, anchor) = (x - width / 2, .centre, x)
                    }
                    // Clear of the split's handle at the row's left end.
                    guard start >= Self.splitHandleRect(layout: layout, scale: scale).maxX + 2 * scale,
                          start + width <= scaleRect.width else {
                        return nil
                    }
                    return BandLabels.Item(text: text, x: anchor, y: scaleRect.height / 2, alignment: labelAlignment,
                                           colour: textColour, points: BandLayout.frequencyLabelPoints)
                }
            }
        }

        // The strip's segment names, where they fit.
        let stripRect = layout.strip
        if settings.bandPlanStrip, stripRect.width > 0, stripRect.height > 0 {
            let points = settings.bandPlanSize.labelPoints
            let key = LabelKey(rect: stripRect, scale: scale, numbers: [geometry.centerHz, geometry.spanHz, points],
                               plan: overlays.bandPlan)
            sets["c-strip"] = LabelSet(key: key) {
                let strip = BandPlanStrip(plan: overlays.bandPlan, geometry: geometry, rightX: Double(stripRect.maxX))
                return strip.pieces.compactMap { piece -> BandLabels.Item? in
                    let fitting = piece.text(scale: scale) { text in
                        BandLabels.width(of: text, points: points, scale: scale, bold: true) + 2 * scale
                            <= CGFloat(piece.width)
                    }
                    guard let text = fitting else {
                        return nil
                    }
                    return BandLabels.Item(text: text, x: CGFloat(piece.labelX), y: stripRect.height / 2,
                                           alignment: .centre, colour: Self.stripLabelColour, points: points,
                                           bold: true)
                }
            }
        }

        if overlays.paused {
            let pausedRect = CGRect(x: 0, y: 0, width: min(layout.size.width, 120 * scale), height: 24 * scale)
            sets["d-paused"] = LabelSet(key: LabelKey(rect: pausedRect, scale: scale, numbers: [])) {
                [BandLabels.Item(text: BandOverlays.pausedText, x: 8 * scale, y: pausedRect.height / 2,
                                 alignment: .leading, colour: Self.stripLabelColour, points: BandLayout.labelPoints)]
            }
        }
        return sets
    }

    private func syncLabels(name: String, key: LabelKey, items: () -> [BandLabels.Item]) {
        if let layer = labelLayers[name], layer.key == key {
            return
        }
        let rect = key.rect
        let width = Int(rect.width.rounded())
        let height = Int(rect.height.rounded())
        guard width > 0, height > 0 else {
            labelLayers[name] = LabelLayer(key: key, rect: rect, texture: nil)
            return
        }
        // A new texture each time, so a frame still in flight keeps the old one.
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: width,
                                                                  height: height, mipmapped: false)
        descriptor.usage = .shaderRead
        let texture = device.makeTexture(descriptor: descriptor)
        let bytes = BandLabels.render(width: width, height: height, scale: key.scale, items: items())
        bytes.withUnsafeBytes { raw in
            if let base = raw.baseAddress {
                texture?.replace(region: MTLRegionMake2D(0, 0, width, height), mipmapLevel: 0, withBytes: base,
                                 bytesPerRow: width * 4)
            }
        }
        labelLayers[name] = LabelLayer(key: key, rect: CGRect(x: rect.minX, y: rect.minY, width: CGFloat(width),
                                                              height: CGFloat(height)), texture: texture)
    }

    /// `key`'s value texture, made the first time the text is drawn. The
    /// textures kept are few: past ``valueTexturesKept`` the ones not
    /// drawn this frame go.
    private func valueTexture(_ key: ValueKey) -> ValueTexture {
        if let kept = valueTextures[key] {
            return kept
        }
        if valueTextures.count >= Self.valueTexturesKept {
            let drawn = Set(valuesDrawn.map(\.key))
            valueTextures = valueTextures.filter { drawn.contains($0.key) }
        }
        let pad = 2 * key.scale
        let width = Int((BandLabels.width(of: key.text, points: Self.valuePoints, scale: key.scale, bold: key.bold)
            + 2 * pad).rounded(.up))
        let height = Int((Self.valuePoints * key.scale * 1.4).rounded(.up))
        // Where BandLabels puts the baseline to centre the glyphs on the middle.
        let metrics = BandLabels.verticalMetrics(of: key.text, points: Self.valuePoints, scale: key.scale,
                                                 bold: key.bold)
        let baseline = (CGFloat(height) / 2 + (metrics.ascent - metrics.descent) / 2).rounded()
        var texture: (any MTLTexture)?
        if width > 0, height > 0 {
            let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: width,
                                                                      height: height, mipmapped: false)
            descriptor.usage = .shaderRead
            texture = device.makeTexture(descriptor: descriptor)
            let item = BandLabels.Item(text: key.text, x: pad, y: CGFloat(height) / 2, alignment: .leading,
                                       colour: key.colour, points: Self.valuePoints, bold: key.bold)
            let bytes = BandLabels.render(width: width, height: height, scale: key.scale, items: [item])
            bytes.withUnsafeBytes { raw in
                if let base = raw.baseAddress {
                    texture?.replace(region: MTLRegionMake2D(0, 0, width, height), mipmapLevel: 0, withBytes: base,
                                     bytesPerRow: width * 4)
                }
            }
        }
        let value = ValueTexture(size: CGSize(width: width, height: height), baseline: baseline,
                                 textTop: baseline - metrics.ascent, textLeft: pad, texture: texture)
        valueTextures[key] = value
        return value
    }

    private func drawValues(encoder: any MTLRenderCommandEncoder, viewport: SIMD2<Float>) {
        for value in valuesDrawn {
            guard let kept = valueTextures[value.key], let texture = kept.texture else {
                continue
            }
            let origin = CGPoint(x: value.origin.x.rounded(), y: value.origin.y.rounded())
            var uniforms = RectUniforms(rect: Self.vector(CGRect(origin: origin, size: kept.size)), viewport: viewport)
            encoder.setRenderPipelineState(labelPipeline)
            encoder.setVertexBytes(&uniforms, length: MemoryLayout<RectUniforms>.stride, index: 0)
            encoder.setFragmentTexture(texture, index: 0)
            encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        }
    }

    // MARK: The waterfall

    private func syncWaterfall(_ history: WaterfallHistory, referenceHz: Double) {
        guard history.columns > 0 else {
            waterfallTexture = nil
            coverageTexture = nil
            waterfallGeneration = nil
            return
        }
        var uploadAll = false
        if waterfallGeneration != history.layoutGeneration || waterfallTexture?.width != history.columns
            || waterfallTexture?.height != history.capacity {
            let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .r8Unorm, width: history.columns,
                                                                      height: history.capacity, mipmapped: false)
            descriptor.usage = .shaderRead
            waterfallTexture = device.makeTexture(descriptor: descriptor)
            let coverages = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rg32Float, width: 1,
                                                                     height: history.capacity, mipmapped: false)
            coverages.usage = .shaderRead
            coverageTexture = device.makeTexture(descriptor: coverages)
            waterfallReferenceHz = referenceHz.isFinite ? referenceHz : 0
            waterfallGeneration = history.layoutGeneration
            uploadAll = true
        }
        guard let texture = waterfallTexture else {
            return
        }
        let fresh = uploadAll ? UInt64(history.count)
            : min(history.linesAppended &- uploadedLines, UInt64(history.capacity))
        for age in 0..<Int(min(fresh, UInt64(history.count))) {
            let row = history.storageRow(age: age)
            var span = SIMD2<Float>(0, 0)
            if let coverage = history.coverage(storageRow: row), coverage.spanHz > 0 {
                span = SIMD2(Float(coverage.lowHz - waterfallReferenceHz), Float(coverage.spanHz))
            }
            if uploadAll {
                // A texture made this frame: no frame in flight reads it.
                Array(history.storageLine(row)).withUnsafeBytes { raw in
                    if let base = raw.baseAddress {
                        texture.replace(region: MTLRegionMake2D(0, row, history.columns, 1), mipmapLevel: 0,
                                        withBytes: base, bytesPerRow: history.columns)
                    }
                }
                coverageTexture?.replace(region: MTLRegionMake2D(0, row, 1, 1), mipmapLevel: 0, withBytes: &span,
                                         bytesPerRow: MemoryLayout<SIMD2<Float>>.stride)
            } else {
                // A row a frame still in flight may be drawing: the copy goes
                // through this frame's command buffer, after those frames.
                history.storageLine(row).withUnsafeBytes { raw in
                    rowUploads.add(raw, to: texture, row: row, width: history.columns, device: device)
                }
                if let coverageTexture {
                    withUnsafeBytes(of: span) { raw in
                        rowUploads.add(raw, to: coverageTexture, row: row, width: 1, device: device)
                    }
                }
            }
        }
        uploadedLines = history.linesAppended
    }

    /// This phone's own gradient as the waterfall's palette.
    private func syncPalette(custom stops: [PaletteStop], low: String?) {
        if customDrawn == stops, paletteTexture != nil, lowColourDrawn == low {
            return
        }
        uploadPalette(BandPalette.table(BandPalette.table(custom: stops), lowColour: low))
        customDrawn = stops
        paletteDrawn = nil
        lowColourDrawn = low
    }

    private func syncPalette(_ palette: StationCatalog.Palette?, low: String?) {
        if let drawn = paletteDrawn, drawn == palette, paletteTexture != nil, customDrawn == nil,
           lowColourDrawn == low {
            return
        }
        uploadPalette(BandPalette.table(BandPalette.table(for: palette), lowColour: low))
        paletteDrawn = .some(palette)
        customDrawn = nil
        lowColourDrawn = low
    }

    private func uploadPalette(_ table: [UInt8]) {
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rgba8Unorm, width: BandPalette.entries,
                                                                  height: 1, mipmapped: false)
        descriptor.usage = .shaderRead
        guard let texture = device.makeTexture(descriptor: descriptor) else {
            return
        }
        table.withUnsafeBytes { raw in
            if let base = raw.baseAddress {
                texture.replace(region: MTLRegionMake2D(0, 0, BandPalette.entries, 1), mipmapLevel: 0, withBytes: base,
                                bytesPerRow: BandPalette.entries * 4)
            }
        }
        paletteTexture = texture
    }

    /// Draws the waterfall `lookBack` lines back from the newest (0, live).
    private func drawWaterfall(_ history: WaterfallHistory, rect: CGRect, view: BandCoverage, lookBack: Int = 0,
                               encoder: any MTLRenderCommandEncoder, viewport: SIMD2<Float>) {
        guard rect.width > 0, rect.height > 0, let lines = waterfallTexture ?? paletteTexture,
              let palette = paletteTexture, let coverages = coverageTexture ?? paletteTexture else {
            return
        }
        var rectUniforms = RectUniforms(rect: Self.vector(rect), viewport: viewport)
        let hasLines = waterfallTexture != nil
        var uniforms = WaterfallUniforms(rect: Self.vector(rect), columns: UInt32(hasLines ? history.columns : 0),
                                         capacity: UInt32(history.capacity), count: UInt32(hasLines ? history.count : 0),
                                         nextRow: UInt32(history.nextStorageRow), background: Self.background,
                                         view: SIMD4(Float(view.lowHz - waterfallReferenceHz), Float(view.spanHz),
                                                     Float(max(0, min(lookBack, history.count))),
                                                     hasLines && coverageTexture != nil ? 1 : 0))
        encoder.setRenderPipelineState(waterfallPipeline)
        encoder.setVertexBytes(&rectUniforms, length: MemoryLayout<RectUniforms>.stride, index: 0)
        encoder.setFragmentBytes(&uniforms, length: MemoryLayout<WaterfallUniforms>.stride, index: 0)
        encoder.setFragmentTexture(lines, index: 0)
        encoder.setFragmentTexture(palette, index: 1)
        encoder.setFragmentTexture(coverages, index: 2)
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
    }

    // MARK: The 3D view

    /// The 3D scale's backing and ticks: the scale's column darkened so the
    /// surface does not show through its labels, a line down its left edge,
    /// and a short tick at each level the labels name.
    private func addStackScale(layout: BandLayout, geometry: StackedTraceGeometry, step: Double, scale: CGFloat,
                               to vertices: inout [ColourVertex]) {
        let rect = layout.dbmScale
        guard rect.width > 0, rect.height > 0 else {
            return
        }
        Self.addRect(rect, Self.stackScaleBackground, to: &vertices)
        let line = max(1, scale.rounded(.down))
        Self.addRect(CGRect(x: rect.minX, y: rect.minY, width: line, height: rect.height), Self.stackScaleEdge,
                     to: &vertices)
        let arrows = layout.dbmArrows.maxY
        for tick in geometry.scaleTicks(stepDb: step) {
            let y = CGFloat(tick.y).rounded(.down)
            guard y > arrows, y < rect.maxY else {
                continue
            }
            Self.addRect(CGRect(x: rect.minX, y: y, width: 4 * scale, height: line), Self.stackScaleTick,
                         to: &vertices)
        }
    }

    /// Uploads the rows the renderer has not seen, all of them after the
    /// rows' width changed.
    private func syncStack(_ stack: StackedTraceHistory, referenceHz: Double) {
        guard stack.columns > 0, stack.count > 0 else {
            return
        }
        var uploadAll = false
        if stackGeneration != stack.layoutGeneration || stackTexture?.width != stack.columns
            || stackTexture?.height != stack.capacity {
            let rows = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .r32Float, width: stack.columns,
                                                                height: stack.capacity, mipmapped: false)
            rows.usage = .shaderRead
            stackTexture = device.makeTexture(descriptor: rows)
            let windows = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rg32Float, width: 1,
                                                                   height: stack.capacity, mipmapped: false)
            windows.usage = .shaderRead
            stackWindows = device.makeTexture(descriptor: windows)
            stackReferenceHz = referenceHz.isFinite ? referenceHz : 0
            stackGeneration = stack.layoutGeneration
            uploadAll = true
        }
        guard let texture = stackTexture else {
            return
        }
        let fresh = uploadAll ? UInt64(stack.count) : min(stack.rowsAppended &- uploadedRows, UInt64(stack.capacity))
        for age in 0..<Int(min(fresh, UInt64(stack.count))) {
            let row = stack.storageRow(age: age)
            var window = SIMD2<Float>(0, 0)
            if let coverage = stack.window(storageRow: row), coverage.spanHz > 0 {
                window = SIMD2(Float(coverage.lowHz - stackReferenceHz), Float(coverage.spanHz))
            }
            if uploadAll {
                // A texture made this frame: no frame in flight reads it.
                Array(stack.storageLine(row)).withUnsafeBytes { raw in
                    if let base = raw.baseAddress {
                        texture.replace(region: MTLRegionMake2D(0, row, stack.columns, 1), mipmapLevel: 0,
                                        withBytes: base, bytesPerRow: stack.columns * MemoryLayout<Float>.stride)
                    }
                }
                stackWindows?.replace(region: MTLRegionMake2D(0, row, 1, 1), mipmapLevel: 0, withBytes: &window,
                                      bytesPerRow: MemoryLayout<SIMD2<Float>>.stride)
            } else {
                // A row a frame still in flight may be drawing: the copy goes
                // through this frame's command buffer, after those frames.
                stack.storageLine(row).withUnsafeBytes { raw in
                    rowUploads.add(raw, to: texture, row: row, width: stack.columns, device: device)
                }
                if let stackWindows {
                    withUnsafeBytes(of: window) { raw in
                        rowUploads.add(raw, to: stackWindows, row: row, width: 1, device: device)
                    }
                }
            }
        }
        uploadedRows = stack.rowsAppended
    }

    /// Copies the rows staged this frame into their textures, in a blit
    /// pass ahead of the frame's drawing. Metal orders the copy after the
    /// frames already committed that read the textures, so a frame in
    /// flight draws the rows it was made with, never one written over it.
    private func encodeRowUploads(_ commandBuffer: any MTLCommandBuffer) {
        let staging = rowUploads.buffer
        stagingBuffers[ring] = staging
        // A buffer far bigger than its frames need (after a burst) goes once
        // it has stayed so for a while; the next frame makes one its size.
        // This frame's copy keeps its own hold on it.
        if let staging, staging.length > max(RowUploads.smallestBuffer, 4 * rowUploads.used) {
            stagingOversized[ring] += 1
            if stagingOversized[ring] >= RowUploads.oversizedFramesKept {
                stagingBuffers[ring] = nil
                stagingOversized[ring] = 0
            }
        } else {
            stagingOversized[ring] = 0
        }
        guard let staging, !rowUploads.copies.isEmpty, let blit = commandBuffer.makeBlitCommandEncoder() else {
            return
        }
        for copy in rowUploads.copies {
            blit.copy(from: staging, sourceOffset: copy.offset, sourceBytesPerRow: copy.bytesPerRow,
                      sourceBytesPerImage: copy.bytesPerRow, sourceSize: MTLSize(width: copy.width, height: 1, depth: 1),
                      to: copy.texture, destinationSlice: 0, destinationLevel: 0,
                      destinationOrigin: MTLOrigin(x: 0, y: copy.row, z: 0))
        }
        blit.endEncoding()
    }

    /// The stacked trace over the spectrum's plot, oldest row first.
    private func drawStack(_ stack: StackedTraceHistory, overlay: StackedTraceOverlay, geometry: StackedTraceGeometry,
                           settings: BandDisplaySettings, markers: [SliceMarkers.Marker], view: BandCoverage,
                           scale: CGFloat, target: CGSize, encoder: any MTLRenderCommandEncoder,
                           viewport: SIMD2<Float>) {
        guard stack.count > 0, stack.columns > 0, let rows = stackTexture, let windows = stackWindows,
              let palette = paletteTexture, view.spanHz > 0, geometry.size.width > 0, geometry.size.height > 0 else {
            return
        }
        var uniforms = StackedUniforms()
        uniforms.plot = SIMD4(0, 0, Float(geometry.size.width), Float(geometry.size.height))
        uniforms.background = Self.background
        uniforms.shape = SIMD4(Float(geometry.shape.backWidth), Float(geometry.shape.depthSpan),
                               Float(geometry.shape.ridge), Float(StackedTrace.heightCurve))
        uniforms.levels = SIMD4(Float(geometry.floorDbm), Float(geometry.heightRangeDb),
                                Float(geometry.colourRangeDb), Float(geometry.gamma))
        let rowSpan = StackedTrace.rowSpan(spanPercent: settings.threeDSpan, shape: geometry.shape,
                                           availableFactor: overlay.availableSpanFactor)
        uniforms.look = SIMD4(Float(StackedTrace.haze), Float(StackedTrace.fillShare),
                              Float(max(1, Self.stackRidgePoints * scale)), Float(rowSpan))
        uniforms.view = SIMD4(Float(view.lowHz - stackReferenceHz), Float(view.spanHz),
                              Float(min(max(overlay.glide, 0), 1)), Float(StackedTrace.visibleRows))
        var shadows = Array(repeating: SIMD4<Float>(0, 0, 0, 0), count: StackedUniforms.shadowSlots)
        var cues = shadows
        var passbands = 0
        if settings.threeDSliceShadow {
            for marker in markers where !marker.foreign && passbands < StackedUniforms.shadowSlots {
                let low = (marker.passbandHz.lowerBound - view.lowHz) / view.spanHz
                let high = (marker.passbandHz.upperBound - view.lowHz) / view.spanHz
                guard high >= 0, low <= 1 else {
                    continue
                }
                shadows[passbands] = SIMD4(Float(low), Float(high), Float((marker.centerHz - view.lowHz) / view.spanHz), 0)
                cues[passbands] = marker.style.triangle
                passbands += 1
            }
        }
        uniforms.setShadows(shadows, cues: cues)
        uniforms.shadowMeta = SIMD4(Float(passbands), Float(StackedTrace.shadowStrength),
                                    Float(StackedTrace.shadowCueStrength), Float(0.75 * scale))
        uniforms.viewport = viewport
        uniforms.columns = UInt32(stack.columns)
        uniforms.capacity = UInt32(stack.capacity)
        uniforms.count = UInt32(stack.count)
        uniforms.newestRow = UInt32(stack.storageRow(age: 0))
        // Only the plot: the surface never reaches the strip or the waterfall.
        let width = min(Int(geometry.size.width), Int(target.width))
        let height = min(Int(geometry.size.height), Int(target.height))
        guard width > 0, height > 0 else {
            return
        }
        encoder.setScissorRect(MTLScissorRect(x: 0, y: 0, width: width, height: height))
        encoder.setRenderPipelineState(stackedPipeline)
        encoder.setVertexBytes(&uniforms, length: MemoryLayout<StackedUniforms>.stride, index: 0)
        encoder.setFragmentBytes(&uniforms, length: MemoryLayout<StackedUniforms>.stride, index: 0)
        encoder.setVertexTexture(rows, index: 0)
        encoder.setVertexTexture(windows, index: 1)
        encoder.setFragmentTexture(palette, index: 0)
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: stack.columns * 2,
                               instanceCount: min(stack.count, stack.capacity))
        encoder.setScissorRect(MTLScissorRect(x: 0, y: 0, width: Int(target.width), height: Int(target.height)))
    }

    // MARK: Geometry to triangles

    private func drawColour(_ vertices: [ColourVertex], slot: Int, encoder: any MTLRenderCommandEncoder,
                            viewport: inout SIMD2<Float>) {
        guard !vertices.isEmpty else {
            return
        }
        let length = vertices.count * MemoryLayout<ColourVertex>.stride
        if (vertexBuffers[slot]?.length ?? 0) < length {
            // Grown with room to spare, so a busier frame does not reallocate.
            vertexBuffers[slot] = device.makeBuffer(length: length * 3 / 2, options: .storageModeShared)
        }
        guard let buffer = vertexBuffers[slot] else {
            return
        }
        vertices.withUnsafeBytes { raw in
            if let base = raw.baseAddress {
                buffer.contents().copyMemory(from: base, byteCount: raw.count)
            }
        }
        encoder.setRenderPipelineState(colourPipeline)
        encoder.setVertexBuffer(buffer, offset: 0, index: 0)
        encoder.setVertexBytes(&viewport, length: MemoryLayout<SIMD2<Float>>.stride, index: 1)
        encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: vertices.count)
    }

    private static func addRect(_ rect: CGRect, _ colour: SIMD4<Float>, to vertices: inout [ColourVertex]) {
        guard rect.width > 0, rect.height > 0 else {
            return
        }
        let a = SIMD2<Float>(Float(rect.minX), Float(rect.minY))
        let b = SIMD2<Float>(Float(rect.maxX), Float(rect.minY))
        let c = SIMD2<Float>(Float(rect.maxX), Float(rect.maxY))
        let d = SIMD2<Float>(Float(rect.minX), Float(rect.maxY))
        // One by one, so no list is made for each rectangle.
        vertices.append(ColourVertex(position: a, colour: colour))
        vertices.append(ColourVertex(position: b, colour: colour))
        vertices.append(ColourVertex(position: c, colour: colour))
        vertices.append(ColourVertex(position: a, colour: colour))
        vertices.append(ColourVertex(position: c, colour: colour))
        vertices.append(ColourVertex(position: d, colour: colour))
    }

    static func addPolyline(_ points: [SIMD2<Float>], width: Float, colour: SIMD4<Float>,
                                    to vertices: inout [ColourVertex]) {
        guard points.count > 1 else {
            return
        }
        let half = width / 2
        for index in 0..<(points.count - 1) {
            addSegment(points[index], points[index + 1], half: half, colour: colour, to: &vertices)
        }
    }

    /// One straight piece of a line `2 * half` wide, from `a` to `b`.
    private static func addSegment(_ a: SIMD2<Float>, _ b: SIMD2<Float>, half: Float, colour: SIMD4<Float>,
                                   to vertices: inout [ColourVertex]) {
        let along = b - a
        let length = (along * along).sum().squareRoot()
        guard length > 0 else {
            return
        }
        let normal = SIMD2<Float>(-along.y, along.x) / length * half
        let a0 = a + normal
        let a1 = a - normal
        let b0 = b + normal
        let b1 = b - normal
        vertices.append(ColourVertex(position: a0, colour: colour))
        vertices.append(ColourVertex(position: b0, colour: colour))
        vertices.append(ColourVertex(position: b1, colour: colour))
        vertices.append(ColourVertex(position: a0, colour: colour))
        vertices.append(ColourVertex(position: b1, colour: colour))
        vertices.append(ColourVertex(position: a1, colour: colour))
    }

    /// A filled circle, as a fan of thin triangles.
    static func addDisc(_ centre: SIMD2<Float>, radius: Float, colour: SIMD4<Float>,
                        to vertices: inout [ColourVertex]) {
        let steps = 24
        for step in 0..<steps {
            let a = Float(step) / Float(steps) * 2 * .pi
            let b = Float(step + 1) / Float(steps) * 2 * .pi
            vertices.append(ColourVertex(position: centre, colour: colour))
            vertices.append(ColourVertex(position: centre + SIMD2(cos(a), sin(a)) * radius, colour: colour))
            vertices.append(ColourVertex(position: centre + SIMD2(cos(b), sin(b)) * radius, colour: colour))
        }
    }

    /// Where the split's handle sits in the frequency-scale row, in pixels.
    static func splitHandleRect(layout: BandLayout, scale: CGFloat) -> CGRect {
        let row = layout.frequencyScale
        let width = (splitHandleSize.width * scale).rounded()
        let height = min((splitHandleSize.height * scale).rounded(), row.height)
        return CGRect(x: row.minX + (splitHandleInsetPoints * scale).rounded(),
                      y: (row.midY - height / 2).rounded(), width: width, height: height)
    }

    /// The split's handle: three bars across, top, middle and foot.
    static func addSplitHandle(layout: BandLayout, scale: CGFloat, to vertices: inout [ColourVertex]) {
        guard layout.frequencyScale.height > 0 else {
            return
        }
        let rect = splitHandleRect(layout: layout, scale: scale)
        let bar = max(1, (splitHandleBarPoints * scale).rounded())
        for y in [rect.minY, (rect.midY - bar / 2).rounded(), rect.maxY - bar] {
            addRect(CGRect(x: rect.minX, y: y, width: rect.width, height: bar), splitHandleColour, to: &vertices)
        }
    }

    /// A ring `stroke` wide about `radius`, as a band of thin quads.
    static func addRing(_ centre: SIMD2<Float>, radius: Float, stroke: Float, colour: SIMD4<Float>,
                        to vertices: inout [ColourVertex]) {
        let steps = 24
        let inner = max(0, radius - stroke / 2)
        let outer = radius + stroke / 2
        for step in 0..<steps {
            let a = Float(step) / Float(steps) * 2 * .pi
            let b = Float(step + 1) / Float(steps) * 2 * .pi
            let da = SIMD2(cos(a), sin(a))
            let db = SIMD2(cos(b), sin(b))
            let corners = [centre + da * inner, centre + da * outer, centre + db * outer,
                           centre + da * inner, centre + db * outer, centre + db * inner]
            for corner in corners {
                vertices.append(ColourVertex(position: corner, colour: colour))
            }
        }
    }

    /// `points` (left to right, as a trace's are) as a line `width` wide,
    /// drawn in dashes `dash` across with `gap` across left out between
    /// them. Measured across, not along the line, so a trace's steep rises
    /// stay whole and the dashes cost no more than the line itself.
    static func addDashedPolyline(_ points: [SIMD2<Float>], width: Float, dash: Float, gap: Float,
                                  colour: SIMD4<Float>, to vertices: inout [ColourVertex]) {
        guard points.count > 1, dash > 0 else {
            return
        }
        let period = dash + max(gap, 0)
        let half = width / 2
        for index in 0..<(points.count - 1) {
            let a = points[index]
            let b = points[index + 1]
            let across = b.x - a.x
            guard across > 0 else {
                // A step straight up or down: whole when it falls in a dash.
                if a.x - (a.x / period).rounded(.down) * period < dash {
                    addSegment(a, b, half: half, colour: colour, to: &vertices)
                }
                continue
            }
            var x = a.x
            while x < b.x {
                let start = (x / period).rounded(.down) * period
                let dashEnd = start + dash
                let next = min(b.x, x < dashEnd ? dashEnd : start + period)
                if x < dashEnd {
                    let from = x == a.x ? a : a + (b - a) * ((x - a.x) / across)
                    let to = next == b.x ? b : a + (b - a) * ((next - a.x) / across)
                    addSegment(from, to, half: half, colour: colour, to: &vertices)
                }
                x = next > x ? next : b.x
            }
        }
    }

    /// A row `height` high at `y`, from `left` to `right` in dashes `dash`
    /// long with `gap` left out between them.
    private static func addDashesAcross(y: Double, height: Double, from left: Double, to right: Double, dash: Double,
                                        gap: Double, colour: SIMD4<Float>, to vertices: inout [ColourVertex]) {
        guard dash > 0, right > left else {
            return
        }
        var x = left
        while x < right {
            addRect(CGRect(x: x, y: y, width: min(dash, right - x), height: height), colour, to: &vertices)
            x += dash + max(gap, 0)
        }
    }

    private static func clampY(_ y: Double, _ geometry: BandGeometry) -> Double {
        min(max(y, 0), Double(geometry.size.height))
    }

    private static func vector(_ rect: CGRect) -> SIMD4<Float> {
        SIMD4(Float(rect.minX), Float(rect.minY), Float(rect.width), Float(rect.height))
    }
}

/// The 3D view's shader uniforms, laid out as `StackedUniforms` in
/// `Band.metal`: nine four-float rows and two arrays of eight, then the
/// viewport and four counts.
struct StackedUniforms {
    typealias Eight = (SIMD4<Float>, SIMD4<Float>, SIMD4<Float>, SIMD4<Float>,
                       SIMD4<Float>, SIMD4<Float>, SIMD4<Float>, SIMD4<Float>)
    static let shadowSlots = 8
    private static let none = SIMD4<Float>(0, 0, 0, 0)

    var plot = SIMD4<Float>(0, 0, 0, 0)
    var background = SIMD4<Float>(0, 0, 0, 1)
    var shadows: Eight = (none, none, none, none, none, none, none, none)
    var cues: Eight = (none, none, none, none, none, none, none, none)
    var shape = SIMD4<Float>(0, 0, 0, 0)
    var levels = SIMD4<Float>(0, 0, 0, 0)
    var look = SIMD4<Float>(0, 0, 0, 0)
    var view = SIMD4<Float>(0, 0, 0, 0)
    var shadowMeta = SIMD4<Float>(0, 0, 0, 0)
    var viewport = SIMD2<Float>(0, 0)
    var columns: UInt32 = 0
    var capacity: UInt32 = 0
    var count: UInt32 = 0
    var newestRow: UInt32 = 0

    mutating func setShadows(_ bands: [SIMD4<Float>], cues colours: [SIMD4<Float>]) {
        func eight(_ list: [SIMD4<Float>]) -> Eight {
            let at = { (index: Int) in index < list.count ? list[index] : Self.none }
            return (at(0), at(1), at(2), at(3), at(4), at(5), at(6), at(7))
        }
        shadows = eight(bands)
        cues = eight(colours)
    }
}

/// A frame's encoding time, set once encoding ends and read when the
/// graphics finish it (always after).
final class CPUSeconds: @unchecked Sendable {
    private let lock = NSLock()
    private var seconds = 0.0

    var value: Double {
        get { lock.withLock { seconds } }
        set { lock.withLock { seconds = newValue } }
    }
}

/// One frame's new rows, written straight into the frame's staging buffer
/// and listed for the frame's blit pass to copy into their textures.
struct RowUploads {
    struct Copy {
        var texture: any MTLTexture
        var row: Int
        var width: Int
        var offset: Int
        var bytesPerRow: Int
    }

    /// Each copy's bytes start on this boundary, as a copy from a buffer asks.
    static let alignment = 16
    /// The size a staging buffer is made at least, and may stay at for good.
    static let smallestBuffer = 64 * 1024
    /// Frames a staging buffer may stay bigger than it needs before it goes.
    static let oversizedFramesKept = 120
    /// This frame's staging buffer: the slot's own, or a bigger one made
    /// when this frame's rows outgrew it.
    private(set) var buffer: (any MTLBuffer)?
    /// The bytes this frame has written into it.
    private(set) var used = 0
    private(set) var copies: [Copy] = []

    /// Starts a frame on its slot's staging buffer, nil when it has none.
    mutating func begin(with buffer: (any MTLBuffer)?) {
        self.buffer = buffer
        used = 0
        copies.removeAll(keepingCapacity: true)
    }

    /// Stages `bytes`, one texture row `width` texels wide, for `row` of
    /// `texture`. A buffer too small is replaced by one twice its size (the
    /// slot's buffer is free: its last frame has finished).
    mutating func add(_ bytes: UnsafeRawBufferPointer, to texture: any MTLTexture, row: Int, width: Int,
                      device: any MTLDevice) {
        guard let source = bytes.baseAddress, bytes.count > 0 else {
            return
        }
        let offset = (used + Self.alignment - 1) / Self.alignment * Self.alignment
        let end = offset + bytes.count
        if (buffer?.length ?? 0) < end {
            let length = max(Self.smallestBuffer, end, 2 * (buffer?.length ?? 0))
            guard let grown = device.makeBuffer(length: length, options: .storageModeShared) else {
                return
            }
            if let buffer, used > 0 {
                grown.contents().copyMemory(from: buffer.contents(), byteCount: used)
            }
            buffer = grown
        }
        guard let buffer else {
            return
        }
        (buffer.contents() + offset).copyMemory(from: source, byteCount: bytes.count)
        used = end
        copies.append(Copy(texture: texture, row: row, width: width, offset: offset, bytesPerRow: bytes.count))
    }
}
