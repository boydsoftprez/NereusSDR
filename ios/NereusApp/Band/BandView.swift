// NereusSDR for iOS: the band on screen, a Metal view drawn by the band's renderer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import MetalKit
import NereusBand
import SwiftUI
import os

/// One pan's band on screen (R-IOS-11, D7): an `MTKView` that
/// ``BandRenderer`` draws whenever ``BandModel`` changes, at the view's full
/// pixel resolution. The flags, markers and gestures above it are the
/// screens' own.
///
/// While the Core's waterfall lines flow, the view reads the band's row
/// clock on every frame of the screen (a display link, at most 60 a second)
/// and draws each line on the frame the clock puts it in, so it goes in on
/// the clock's grid rather than when it came over the network. In 2D the
/// trace still draws on the next frame after it comes; in 3D the band draws
/// on a steady cadence of the Core's frames and the glide follows the
/// clock. With nothing flowing the link rests and the view draws on demand,
/// as before.
struct BandView: UIViewRepresentable {
    @ObservedObject var model: BandModel
    /// The band's tab shows; hidden, the display link stops and the lines
    /// go in as they come.
    @Environment(\.bandOnScreen) private var onScreen

    func makeCoordinator() -> Coordinator {
        Coordinator(model: model)
    }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: context.coordinator.renderer?.device)
        view.colorPixelFormat = .bgra8Unorm
        view.framebufferOnly = true
        // Drawn on demand: when the model changes, not on a timer.
        view.isPaused = true
        view.enableSetNeedsDisplay = true
        view.autoResizeDrawable = true
        view.delegate = context.coordinator
        view.isOpaque = true
        context.coordinator.attach(view)
        return view
    }

    func updateUIView(_ view: MTKView, context: Context) {
        context.coordinator.onScreen = onScreen
        view.setNeedsDisplay()
    }

    static func dismantleUIView(_ view: MTKView, coordinator: Coordinator) {
        coordinator.detach()
        #if DEBUG
        coordinator.model.stoppedDrawing(in: coordinator.viewID)
        #endif
    }

    @MainActor
    final class Coordinator: NSObject, MTKViewDelegate, BandLineDriver {
        let model: BandModel
        let renderer: BandRenderer?
        #if DEBUG
        let viewID = UUID()
        private var drawableSize: (width: Int, height: Int)?
        private var drawableGeneration = UUID()
        private(set) var currentDrawKey: BandModel.DrawKey?
        #endif
        private var changes: AnyCancellable?
        /// The least interval between 3D draws, from when each was due.
        private var stackDraws = StackedDrawThrottle()
        private static let logger = Logger(subsystem: "NereusSDR", category: "band")
        private weak var view: MTKView?
        /// Reads the row clock on every screen frame while lines flow. The
        /// coordinator's deinit invalidates it, so it reads it unisolated.
        nonisolated(unsafe) private var link: CADisplayLink?
        /// Which frames draw while lines flow.
        private var lineDraws = BandLineDraws()
        /// The band changed since the view last drew.
        private var dirty = false
        /// Frames in a row with nothing to draw and no line flowing.
        private var restingFrames = 0
        /// Frames with nothing flowing before the link rests.
        private static let framesBeforeRest = 6
        #if DEBUG
        /// `NEREUS_ROW_CLOCK=0` (or the launch argument `-NereusRowClock 0`)
        /// puts each line in as it comes, the way before the clock, for a
        /// timing trace to compare against.
        private static let rowClockOff = ProcessInfo.processInfo.environment["NEREUS_ROW_CLOCK"] == "0"
            || UserDefaults.standard.string(forKey: "NereusRowClock") == "0"
        #endif

        init(model: BandModel) {
            self.model = model
            do {
                renderer = try BandRenderer()
            } catch {
                renderer = nil
                Self.logger.error("The band cannot be drawn on this device's graphics")
            }
        }

        deinit {
            // Freed without dismantleUIView: a paused link never fires to
            // find its owner gone, and would keep its run loop's hold.
            if let link {
                let held = HeldLink(link: link)
                if Thread.isMainThread {
                    held.link.invalidate()
                } else {
                    DispatchQueue.main.async {
                        held.link.invalidate()
                    }
                }
            }
        }

        func attach(_ view: MTKView) {
            self.view = view
            changes = model.draws.sink { [weak self, weak view] _ in
                self?.requestDraw(view)
            }
            let target = LinkTarget()
            target.owner = self
            let link = CADisplayLink(target: target, selector: #selector(LinkTarget.step(_:)))
            // Never faster than 60 a second (the pacer's recommendation 4).
            link.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 60, preferred: 60)
            link.isPaused = true
            link.add(to: .main, forMode: .common)
            self.link = link
            model.lineDriver = self
        }

        /// The view is gone: its link stops and the band's lines go in as they come.
        func detach() {
            link?.invalidate()
            link = nil
            changes = nil
            if model.lineDriver === self {
                model.lineDriver = nil
            }
        }

        /// The band's tab shows (``EnvironmentValues/bandOnScreen``). Hidden,
        /// the band stops driving its lines: the waiting ones go in, the
        /// link stops on its next frame, and lines go in as they come until
        /// the tab shows again, when the row clock starts afresh as after a
        /// rest.
        var onScreen = true {
            didSet {
                if onScreen != oldValue {
                    model.notePacing()
                }
            }
        }

        /// The display link is running, for the tests.
        var linkRuns: Bool {
            link.map { !$0.isPaused } ?? false
        }

        var drivesLines: Bool {
            #if DEBUG
            if Self.rowClockOff {
                return false
            }
            #endif
            return onScreen && link != nil && renderer != nil && view?.window != nil
        }

        /// A 2D band draws at once. The 3D view draws no faster than the
        /// frames the Core sends, nor 60 a second (JJ's board,
        /// recommendation 4): a change sooner waits for its turn. While lines
        /// flow on the row clock, the display link draws the change on its
        /// next frame (``step(_:)``).
        private func requestDraw(_ view: MTKView?) {
            guard let view else {
                return
            }
            if model.pacingLines, let link {
                dirty = true
                restingFrames = 0
                if link.isPaused {
                    lineDraws.reset()
                    link.isPaused = false
                }
                return
            }
            guard model.drawsStack else {
                view.setNeedsDisplay()
                return
            }
            switch stackDraws.request(at: CACurrentMediaTime(), interval: model.stackDrawInterval) {
            case .now:
                view.setNeedsDisplay()
            case .waiting:
                return
            case .after(let wait):
                DispatchQueue.main.asyncAfter(deadline: .now() + wait) { [weak self, weak view] in
                    MainActor.assumeIsolated {
                        self?.stackDraws.waited()
                        view?.setNeedsDisplay()
                    }
                }
            }
        }

        /// One frame of the screen while lines flow: the row clock puts in
        /// the lines it says, and the band draws: in 2D on each frame with a
        /// line or a change, in 3D when its cadence falls due. With nothing
        /// flowing for a few frames the link rests.
        fileprivate func step(_ link: CADisplayLink) {
            guard let view, view.window != nil else {
                link.isPaused = true
                model.notePacing()
                return
            }
            // The frame just shown, on the same steady clock as each line's
            // arrival: the clock's margin for a line's hand-off holds only
            // when both are read alike (the next frame's time runs a frame
            // ahead of the arrivals).
            let now = link.timestamp
            let frame = max(link.targetTimestamp - link.timestamp, 0)
            let put = model.advanceLines(at: now, frameSeconds: frame)
            guard model.pacingLines else {
                // The clock stopped (no line period, or the view went):
                // back to drawing on demand.
                link.isPaused = true
                if dirty {
                    dirty = false
                    view.setNeedsDisplay()
                }
                return
            }
            let flowing = model.linesFlowing
            // 2D draws on each frame the clock puts a line in, and the trace
            // and other changes on the next frame; 3D glides on the pacer's
            // cadence (BandLineDraws).
            let draws = lineDraws.draws(at: now, frame: frame, interval: model.stackDrawInterval,
                                        stacked: model.drawsStack, linesPut: put, dirty: dirty,
                                        flowing: flowing)
            if draws {
                dirty = false
                restingFrames = 0
                view.draw()
            } else if dirty || flowing {
                restingFrames = 0
            } else {
                restingFrames += 1
                if restingFrames >= Self.framesBeforeRest {
                    restingFrames = 0
                    link.isPaused = true
                }
            }
        }

        nonisolated func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
            MainActor.assumeIsolated {
                view.setNeedsDisplay()
            }
        }

        nonisolated func draw(in view: MTKView) {
            MainActor.assumeIsolated {
                drawBand(in: view)
            }
        }

        private func drawBand(in view: MTKView) {
            guard let renderer, let drawable = view.currentDrawable else {
                return
            }
            let scale = view.contentScaleFactor
            let state = model.prepareToDraw(size: view.drawableSize, scale: scale)
            let stacked = model.drawsStack
            // What a 3D frame cost to draw, for the pacer (recommendation 3).
            var timing: (@Sendable (Double) -> Void)?
            if stacked {
                timing = { @Sendable [weak model] seconds in
                    DispatchQueue.main.async {
                        MainActor.assumeIsolated {
                            model?.noteStackFrame(drawSeconds: seconds)
                        }
                    }
                }
            }
            let timed = timing
            #if DEBUG
            // The lines this draw shows, for the timing trace. The iOS 27 SDK
            // has no presented-time handler on a drawable, so the trace takes
            // the time the GPU finished the frame: it reaches the screen on
            // the refresh after.
            let tracedLines = model.lineTrace?.takeLinesForDraw() ?? []
            let trace = tracedLines.isEmpty ? nil : model.lineTrace
            let drawn = model.revision
            let size = (width: drawable.texture.width, height: drawable.texture.height)
            if drawableSize?.width != size.width || drawableSize?.height != size.height {
                drawableSize = size
                drawableGeneration = UUID()
            }
            let key = BandModel.DrawKey(viewID: viewID, generation: drawableGeneration,
                                        width: size.width, height: size.height)
            currentDrawKey = key
            model.drawingStarted(key)
            renderer.draw(frame: state.frame, history: state.history, stack: state.stack, extras: state.frameExtras,
                          overlays: model.overlays(scale: scale), into: drawable.texture, present: drawable,
                          completed: { [weak model, weak trace] in
                              let finished = CACurrentMediaTime()
                              // The GPU has drawn it; the model notes it on the main queue.
                              DispatchQueue.main.async {
                                  MainActor.assumeIsolated {
                                      model?.notePresented(key, revision: drawn)
                                      trace?.presented(tracedLines, at: finished)
                                  }
                              }
                          }, drawTime: timed)
            #else
            renderer.draw(frame: state.frame, history: state.history, stack: state.stack, extras: state.frameExtras,
                          overlays: model.overlays(scale: scale), into: drawable.texture, present: drawable,
                          drawTime: timed)
            #endif
        }
    }
}

extension EnvironmentValues {
    /// Whether the band's tab is the one showing. A tab kept alive behind
    /// another keeps its window (``RootView``), so the window alone does
    /// not say whether the band can be seen.
    @Entry var bandOnScreen = true
}

extension View {
    /// A tab's screen, kept alive while another tab shows: seen, touched
    /// and read aloud only while `shown`, and its band's display link
    /// stopped while hidden.
    func tabShown(_ shown: Bool) -> some View {
        opacity(shown ? 1 : 0)
            .allowsHitTesting(shown)
            .accessibilityHidden(!shown)
            .environment(\.bandOnScreen, shown)
    }
}

/// A display link carried to the main queue from the coordinator's deinit.
private struct HeldLink: @unchecked Sendable {
    let link: CADisplayLink
}

/// The display link's target: it holds the band's coordinator weakly, so
/// the link does not keep the view's coordinator alive.
@MainActor
private final class LinkTarget: NSObject {
    weak var owner: BandView.Coordinator?

    @objc func step(_ link: CADisplayLink) {
        guard let owner else {
            link.invalidate()
            return
        }
        owner.step(link)
    }
}
