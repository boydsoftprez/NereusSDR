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
struct BandView: UIViewRepresentable {
    @ObservedObject var model: BandModel

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
        view.setNeedsDisplay()
    }

    #if DEBUG
    static func dismantleUIView(_ view: MTKView, coordinator: Coordinator) {
        coordinator.model.stoppedDrawing(in: coordinator.viewID)
    }
    #endif

    @MainActor
    final class Coordinator: NSObject, MTKViewDelegate {
        let model: BandModel
        let renderer: BandRenderer?
        #if DEBUG
        let viewID = UUID()
        private var drawableSize: (width: Int, height: Int)?
        private var drawableGeneration = UUID()
        private(set) var currentDrawKey: BandModel.DrawKey?
        #endif
        private var changes: AnyCancellable?
        /// When the last 3D draw began, and whether a later one is waiting
        /// for the least interval between 3D draws.
        private var lastStackDraw: CFTimeInterval = 0
        private var stackDrawWaiting = false
        private static let logger = Logger(subsystem: "NereusSDR", category: "band")

        init(model: BandModel) {
            self.model = model
            do {
                renderer = try BandRenderer()
            } catch {
                renderer = nil
                Self.logger.error("The band cannot be drawn on this device's graphics")
            }
        }

        func attach(_ view: MTKView) {
            changes = model.$revision.sink { [weak self, weak view] _ in
                self?.requestDraw(view)
            }
        }

        /// A 2D band draws at once. The 3D view draws no faster than the
        /// frames the Core sends, nor 60 a second (JJ's board,
        /// recommendation 4): a change sooner waits for its turn.
        private func requestDraw(_ view: MTKView?) {
            guard let view else {
                return
            }
            guard model.drawsStack else {
                view.setNeedsDisplay()
                return
            }
            let wait = lastStackDraw + model.stackDrawInterval - CACurrentMediaTime()
            guard wait > 0 else {
                view.setNeedsDisplay()
                return
            }
            guard !stackDrawWaiting else {
                return
            }
            stackDrawWaiting = true
            DispatchQueue.main.asyncAfter(deadline: .now() + wait) { [weak self, weak view] in
                MainActor.assumeIsolated {
                    self?.stackDrawWaiting = false
                    view?.setNeedsDisplay()
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
            if stacked {
                lastStackDraw = CACurrentMediaTime()
            }
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
                          completed: { [weak model] in
                              // The GPU has drawn it; the model notes it on the main queue.
                              DispatchQueue.main.async {
                                  MainActor.assumeIsolated {
                                      model?.notePresented(key, revision: drawn)
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
