// NereusSDR for iOS: the parts the Core's tool pages share: a reason line, a slider row, a row of choices
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

/// The parts the station tool pages (TX Equalizer, PureSignal, Diversity,
/// TCI Server, VAX Audio, Support Bundle) share with Spot Hub's cards and
/// rows. Every control is visible; one that cannot act now is greyed and
/// its reason is written under it.
enum ToolPageParts {
    /// Why the controls above cannot change now, in plain words.
    struct Reason: View {
        let text: String
        let identifier: String

        var body: some View {
            Text(text)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.buttonOnAmberText)
                .frame(maxWidth: .infinity, alignment: .leading)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .accessibilityIdentifier(identifier)
        }
    }

    /// The Core's words for a change it refused.
    struct Refusal: View {
        let text: String
        let identifier: String

        var body: some View {
            ConnectChrome.NoticeBox(tone: .bad, title: nil, text: text)
                .accessibilityIdentifier(identifier)
        }
    }

    /// A reading: its name and its value, read-only.
    struct Reading: View {
        let title: String
        let value: String
        var identifier: String?

        var body: some View {
            HStack(spacing: 10) {
                Text(title)
                    .font(.system(size: 13))
                    .foregroundStyle(ChromeColours.textDim)
                Spacer(minLength: 8)
                Text(value)
                    .font(.system(size: 13).monospacedDigit())
                    .foregroundStyle(ChromeColours.textBright)
                    .multilineTextAlignment(.trailing)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier(identifier ?? "")
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 7)
            .accessibilityElement(children: .combine)
        }
    }

    /// A value box, as the board's inset readouts.
    struct ValueBox: View {
        let text: String
        var width: CGFloat = 56

        var body: some View {
            Text(text)
                .font(.system(size: 12).monospacedDigit())
                .foregroundStyle(ChromeColours.text)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
                .frame(width: width, height: 28)
                .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        }
    }

    /// One physical touch owns the edit until UIKit delivers its terminal event.
    @MainActor final class SliderReleaseLifecycle {
        private(set) var editing = false
        var visible = false
        private var generation: UInt64 = 0
        private var physical: UInt64?
        private var ending: UInt64?
        private var publicationGeneration: UInt64 = 0
        private var terminalCommit: SliderCommitOffer?

        func retainTerminalCommit(_ offer: SliderCommitOffer) { terminalCommit = offer }
        func revalueTerminalCommit(_ value: Double) -> SliderCommitOffer? { terminalCommit?.revalue?(value) }
        var hasPhysicalLifetime: Bool { physical != nil || ending != nil }

        func presentedValue(draft: Double?, model: Double?, fallback: Double,
                            observesPhysicalLifetime: Bool) -> Double {
            let held = !observesPhysicalLifetime || hasPhysicalLifetime ? draft : nil
            return held ?? model ?? fallback
        }

        @discardableResult func begin(_ started: () -> Void = {}) -> Bool {
            guard physical == nil || physical == generation,
                  ending == nil || ending == generation else { return false }
            guard !editing else { return true }
            editing = true
            terminalCommit = nil
            publicationGeneration &+= 1
            started()
            return true
        }
        func release(_ finish: () -> Void) {
            guard !hasPhysicalLifetime, editing else { return }
            editing = false
            finish()
        }
        func physicalBegan(started: () -> Void, discarded: () -> Void) {
            // A new pointer cannot inherit a prior gesture's pending final delivery.
            retire(discarded)
            generation &+= 1
            physical = generation
            ending = nil
            begin(started)
        }
        func physicalEnded() -> UInt64? {
            guard let physical else { return nil }
            self.physical = nil
            ending = physical
            return physical
        }
        func finishPhysical(_ token: UInt64, _ finish: () -> Void) {
            guard ending == token, physical == nil else { return }
            ending = nil
            guard generation == token else { return }
            release(finish)
        }
        func retire(_ discard: () -> Void) {
            terminalCommit = nil
            generation &+= 1
            // Keep a retired pointer fenced until its actual terminal event. A late binding
            // update must not look like a new accessibility adjustment in a new context.
            guard editing else { return }
            editing = false
            publicationGeneration &+= 1
            discard()
        }
        struct CapturedFinish {
            let generation: UInt64
            let publicationGeneration: UInt64
            let valueToSend: Double?
        }
        func capturePhysicalFinish(_ final: Double, pacer: SliderSendPacer) -> CapturedFinish? {
            guard physical != nil, editing else { return nil }
            physical = nil; ending = nil; editing = false
            terminalCommit = nil
            publicationGeneration &+= 1
            var valueToSend: Double?
            let previousSend = pacer.send
            defer { pacer.send = previousSend }
            // Capture the existing throttle's exact release decision without publishing or sending.
            pacer.send = { valueToSend = $0 }
            pacer.move(to: final); pacer.release()
            return CapturedFinish(generation: generation, publicationGeneration: publicationGeneration,
                                  valueToSend: valueToSend)
        }
        func finishCapturedPhysical(_ token: CapturedFinish, sending: (Bool) -> Void, publishing: () -> Void) {
            func current() -> Bool {
                publicationGeneration == token.publicationGeneration && !editing && !hasPhysicalLifetime
            }
            guard current() else { return }
            sending(generation == token.generation)
            guard current() else { return } // A send can reenter with a replacement row lifetime.
            publicationGeneration &+= 1
            publishing()
        }

        func cancelPhysical(retiring: () -> Void = {}, _ discard: @escaping () -> Void) {
            let wasEditing = editing
            retire(retiring)
            physical = nil
            ending = nil
            guard wasEditing else { return }
            let token = publicationGeneration
            // UIKit can cancel while UIViewRepresentable is being updated or dismantled.
            // Retire safety now; publish only after that update, for this row gesture alone.
            DispatchQueue.main.async { [weak self] in
                guard let self, self.publicationGeneration == token, !self.editing else { return }
                discard()
            }
        }
    }

    /// Ownership is captured when the value is offered, before pacing can defer its send.
    struct SliderCommitOffer {
        let value: Double
        let generation: UInt64
        // Revalue only the captured offer; it must not read or synchronize model context.
        var revalue: ((Double) -> SliderCommitOffer)? = nil
        let send: () -> Void
        func callAsFunction() { send() }
    }

    /// The hit-tested Diversity control owns input origin and native terminal value.
    /// Native-call closures are also the boundary exercised by deterministic tests.
    @MainActor final class DiversitySliderControl: UISlider {
        struct Callbacks {
            let canBegin: () -> Bool
            let began: () -> Void
            let moved: (Double, UInt64) -> Bool
            let ended: (Double, UInt64) -> Void
            let cancelled: () -> Void
            let adjusted: (Double, UInt64) -> Bool
            var endInViewUpdate: ((Double, UInt64) -> ((Bool) -> Void)?)? = nil
        }
        private final class Input {
            enum Phase { case starting, active, ending, retired, finished }
            let generation: UInt64
            let pointer: AnyObject?
            let range: ClosedRange<Double>
            let step: Double
            let callbacks: Callbacks
            var phase = Phase.starting
            var buffered: Float?
            var started = false
            var terminalValue: Double?
            var terminalModel: Double?
            init(generation: UInt64, pointer: AnyObject?, range: ClosedRange<Double>, step: Double, callbacks: Callbacks) {
                self.generation = generation; self.pointer = pointer
                self.range = range; self.step = step; self.callbacks = callbacks
            }
        }
        private var physical: Input?
        private var deferredTerminal: Input?
        private var adjusting = false
        private var nativeCallDepth = 0
        private var ignoringNativeValues = 0
        private var viewUpdateDepth = 0
        private var generation: UInt64 = 0
        private var allowed = false
        private var range: ClosedRange<Double> = 0...1
        private var step: Double = 1
        private var callbacks: Callbacks?
        private(set) var presentationValue: Double?
        private var modelValue: Double?
        var formatValue: (Double) -> String = { String($0) }

        override init(frame: CGRect) {
            super.init(frame: frame)
            isContinuous = true
            addTarget(self, action: #selector(nativeValueChanged), for: .valueChanged)
            setContentHuggingPriority(.defaultLow, for: .horizontal)
            setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        }
        @available(*, unavailable)
        required init?(coder: NSCoder) { fatalError("init(coder:) is not used") }

        func configure(value: Double?, presentation: Double? = nil, range: ClosedRange<Double>, step: Double,
                       generation: UInt64, enabled: Bool, callbacks: Callbacks,
                       nativePresentation: (() -> Void)? = nil) {
            viewUpdateDepth += 1
            defer { viewUpdateDepth -= 1 }
            let retiresContext = self.generation != generation || !enabled || value == nil
            if retiresContext { deferredTerminal = nil; retirePhysical() }
            self.generation = generation
            self.range = range; self.step = step; self.callbacks = callbacks
            modelValue = value
            let retiresDraft = retiresContext || physical?.phase == .retired
            let terminal = deferredTerminal
            let holdsTerminal = !retiresContext && terminal?.generation == generation && terminal?.terminalModel == value
            presentationValue = holdsTerminal ? terminal?.terminalValue
                : (value == nil ? nil : (retiresDraft ? value : (presentation ?? value))) // Core Double never passes through Float.
            allowed = enabled && value != nil
            minimumValue = Float(range.lowerBound); maximumValue = Float(range.upperBound)
            isEnabled = allowed
            restorePresentation(nativePresentation)
        }

        func dismantle() {
            viewUpdateDepth += 1
            defer { viewUpdateDepth -= 1 }
            cancelTracking(with: nil)
        }

        override func beginTracking(_ touch: UITouch, with event: UIEvent?) -> Bool {
            #if DEBUG
            beginPhysical(pointer: touch) {
                nativeProbe("begin.before", touch: touch, geometry: true)
                let accepted = super.beginTracking(touch, with: event)
                nativeProbe("begin.after", touch: touch, accepted: accepted, geometry: true)
                return accepted
            }
            #else
            beginPhysical(pointer: touch) { super.beginTracking(touch, with: event) }
            #endif
        }
        override func endTracking(_ touch: UITouch?, with event: UIEvent?) {
            let retainedPhase = (physical?.pointer as? UITouch)?.phase
            #if DEBUG
            nativeProbe("end.entry", touch: touch)
            endPhysical(hasTouch: touch != nil, pointer: touch, retainedPhase: retainedPhase) {
                nativeProbe("end.before", touch: touch, geometry: true)
                super.endTracking(touch, with: event)
                nativeProbe("end.after", touch: touch, geometry: true)
            }
            #else
            endPhysical(hasTouch: touch != nil, pointer: touch, retainedPhase: retainedPhase) { super.endTracking(touch, with: event) }
            #endif
        }
        override func cancelTracking(with event: UIEvent?) {
            #if DEBUG
            nativeProbe("cancel.entry")
            #endif
            cancelPhysical { super.cancelTracking(with: event) }
        }

        @discardableResult func beginPhysical(pointer: AnyObject? = nil, _ native: () -> Bool) -> Bool {
            guard physical == nil, nativeCallDepth == 0, !adjusting, allowed,
                  let callbacks, callbacks.canBegin() else { return false }
            nativeCallDepth += 1
            defer { nativeCallDepth -= 1 }
            let input = Input(generation: generation, pointer: pointer, range: range, step: step, callbacks: callbacks)
            physical = input // Reserve before super can synchronously send valueChanged.
            let accepted = native()
            guard accepted, physical === input, input.phase == .starting,
                  input.generation == generation, allowed, callbacks.canBegin() else {
                if physical === input { physical = nil }
                restorePresentation()
                return false
            }
            deferredTerminal = nil
            input.phase = .active; input.started = true
            callbacks.began()
            if let buffered = input.buffered { offer(buffered, input: input) }
            return physical === input && input.phase == .active
        }

        func endPhysical(hasTouch: Bool, pointer: AnyObject? = nil,
                         retainedPhase: UITouch.Phase? = nil, _ native: () -> Void) {
            nativeCallDepth += 1
            defer { nativeCallDepth -= 1 }
            let input = physical
            if let tracked = input?.pointer, hasTouch, tracked !== pointer {
                ignoringNativeValues += 1
                native() // An old touch cannot lend this pointer its synchronous valueChanged.
                ignoringNativeValues -= 1
                restorePresentation()
                return
            }
            // Nil alone is ambiguous. Only an ended captured pointer supplies affirmative terminal evidence.
            let terminalHasTouch = hasTouch || (retainedPhase == .ended && input?.pointer != nil)
            if !terminalHasTouch { retirePhysical() }
            if let input, input.phase == .active { input.phase = .ending }
            native()
            if let input, terminalHasTouch, physical === input, input.phase == .ending,
               input.generation == generation, allowed,
               let final = canonical(value, range: input.range, step: input.step) {
                input.phase = .finished // Reserve before final flush can reenter the control.
                presentationValue = final
                if viewUpdateDepth > 0, let prepare = input.callbacks.endInViewUpdate {
                    input.terminalValue = final; input.terminalModel = modelValue
                    if let publish = prepare(final, input.generation) {
                        deferredTerminal = input
                        DispatchQueue.main.async { [weak self] in
                            let maySend = self?.deferredTerminal === input
                                && self?.generation == input.generation && self?.allowed == true
                            defer {
                                if self?.deferredTerminal === input { self?.deferredTerminal = nil }
                            }
                            publish(maySend)
                        }
                    }
                } else {
                    input.callbacks.ended(final, input.generation)
                }
            } else if let input, physical === input, input.phase == .ending {
                retirePhysical() // An invalid terminal value cannot strand a held row edit.
            }
            if physical === input { physical = nil }
            restorePresentation()
        }

        func cancelPhysical(_ native: () -> Void) {
            nativeCallDepth += 1
            defer { nativeCallDepth -= 1 }
            let input = physical
            retirePhysical() // Fence before super can call valueChanged or nil-touch end.
            native()
            if physical === input { physical = nil }
            restorePresentation()
        }
        func retirePhysical() {
            guard let input = physical, input.phase != .retired, input.phase != .finished else { return }
            input.phase = .retired
            presentationValue = modelValue
            if input.started { input.callbacks.cancelled() }
            // Keep this retired pointer fenced until its real native terminal callback.
        }

        @objc private func nativeValueChanged() {
            #if DEBUG
            if physical?.phase == .ending {
                nativeProbe("value.inEnd", touch: physical?.pointer as? UITouch)
            }
            #endif
            guard ignoringNativeValues == 0 else {
                #if DEBUG
                nativeProbe("value.ignored", reason: 1)
                #endif
                restorePresentation(); return
            }
            guard let input = physical else {
                #if DEBUG
                nativeProbe("value.ignored", reason: 2)
                #endif
                restorePresentation(); return
            }
            if input.phase == .starting { input.buffered = value; return }
            offer(value, input: input)
        }
        private func offer(_ value: Float, input: Input) {
            // super.endTracking can offer values while configure is on the stack. Its
            // post-super native value owns the final; row publication must wait for the captured end.
            if viewUpdateDepth > 0, input.phase == .ending { return }
            guard physical === input, input.phase == .active || input.phase == .ending,
                  input.generation == generation, allowed,
                  let next = canonical(value, range: input.range, step: input.step) else {
                #if DEBUG
                nativeProbe("value.ignored", touch: input.pointer as? UITouch, reason: 3, raw: value)
                #endif
                restorePresentation(); return
            }
            if !input.callbacks.moved(next, input.generation) {
                #if DEBUG
                nativeProbe("value.ignored", touch: input.pointer as? UITouch, reason: 4, raw: value)
                #endif
                retirePhysical(); restorePresentation(); return
            }
            guard physical === input, input.generation == generation, input.phase != .retired else { return }
            presentationValue = next
            accessibilityValue = formatValue(next)
        }

        #if DEBUG
        /// Numeric observations only; no layout, value, input or pacing mutation.
        private func nativeProbe(_ event: String, touch: UITouch? = nil, accepted: Bool? = nil,
                                 reason: Int = 0, raw: Float? = nil, geometry: Bool = false) {
            guard UITestDiversityFixture.schemeProbe(ProcessInfo.processInfo.arguments) else { return }
            let phase: Double
            switch physical?.phase {
            case .starting?: phase = 1
            case .active?: phase = 2
            case .ending?: phase = 3
            case .retired?: phase = 4
            case .finished?: phase = 5
            case nil: phase = 0
            }
            let location = touch?.location(in: self)
            let retainedTouch = physical?.pointer as? UITouch
            let retainedLocation = retainedTouch?.location(in: self)
            var fields: [(String, Double?)] = [
                ("generation", Double(generation)), ("inputGeneration", physical.map { Double($0.generation) }),
                ("inputPhase", phase), ("nativeDepth", Double(nativeCallDepth)), ("reason", Double(reason)),
                ("touchPresent", touch == nil ? 0 : 1),
                ("retainedTouchPresent", retainedTouch == nil ? 0 : 1),
                ("retainedTouchPhase", retainedTouch.map { Double($0.phase.rawValue) }),
                ("retainedTouchTimestamp", retainedTouch?.timestamp),
                ("retainedTouchX", retainedLocation.map { Double($0.x) }),
                ("retainedTouchY", retainedLocation.map { Double($0.y) }),
                ("isTracking", isTracking ? 1 : 0),
                ("touchMatches", touch.map { physical?.pointer === $0 ? 1 : 0 }),
                ("touchPhase", touch.map { Double($0.phase.rawValue) }),
                ("touchTimestamp", touch?.timestamp),
                ("touchX", location.map { Double($0.x) }), ("touchY", location.map { Double($0.y) }),
                ("accepted", accepted.map { $0 ? 1 : 0 }),
                ("rawValue", Double(raw ?? value)), ("nativeValue", Double(value)),
                ("presentation", presentationValue), ("model", modelValue),
                ("minimum", Double(minimumValue)), ("maximum", Double(maximumValue)), ("step", step)
            ]
            if geometry {
                func rect(_ name: String, _ rect: CGRect) -> [(String, Double?)] {
                    [(name + "X", Double(rect.origin.x)), (name + "Y", Double(rect.origin.y)),
                     (name + "Width", Double(rect.width)), (name + "Height", Double(rect.height))]
                }
                let track = trackRect(forBounds: bounds)
                fields += rect("bounds", bounds) + rect("track", track)
                fields += rect("thumbMin", thumbRect(forBounds: bounds, trackRect: track, value: minimumValue))
                fields += rect("thumbMax", thumbRect(forBounds: bounds, trackRect: track, value: maximumValue))
                fields += rect("thumbCurrent", thumbRect(forBounds: bounds, trackRect: track, value: value))
            }
            let identifier = accessibilityIdentifier ?? ""
            let capturedFields = fields
            // Preserve normal touch trace order; only a view update must defer observable publication.
            if viewUpdateDepth > 0 {
                DispatchQueue.main.async {
                    UITestDiversityFixture.sliderLifecycle.native(event, identifier: identifier,
                                                                 fields: capturedFields)
                }
            } else {
                UITestDiversityFixture.sliderLifecycle.native(event, identifier: identifier,
                                                             fields: capturedFields)
            }
        }
        #endif

        override func accessibilityIncrement() { adjust(by: 1) }
        override func accessibilityDecrement() { adjust(by: -1) }
        private func adjust(by direction: Double) {
            // AX never inherits or discards a physical pointer, including a retired one.
            guard physical == nil, nativeCallDepth == 0, !adjusting, allowed, let current = presentationValue,
                  let callbacks, callbacks.canBegin() else { return }
            let captured = generation
            let next = canonicalDouble(current + direction * step, range: range, step: step)
            guard let next else { return }
            deferredTerminal = nil
            adjusting = true
            if callbacks.adjusted(next, captured), generation == captured, allowed {
                presentationValue = next
            }
            adjusting = false
            restorePresentation()
        }

        private func canonical(_ value: Float, range: ClosedRange<Double>, step: Double) -> Double? {
            canonicalDouble(Double(value), range: range, step: step)
        }
        private func canonicalDouble(_ value: Double, range: ClosedRange<Double>, step: Double) -> Double? {
            guard value.isFinite, step.isFinite, step > 0 else { return nil }
            let bounded = min(max(value, range.lowerBound), range.upperBound)
            let grid = ((bounded - range.lowerBound) / step).rounded() * step + range.lowerBound
            let scale = 1 / step
            return min(max((grid * scale).rounded() / scale, range.lowerBound), range.upperBound)
        }
        private func restorePresentation(_ native: (() -> Void)? = nil) {
            // UISlider.h: setValue(animated:) sends no action. Programmatic presentation is not input.
            if let native { native() }
            else { setValue(Float(presentationValue ?? range.lowerBound), animated: false) }
            accessibilityValue = presentationValue.map(formatValue) ?? "Not sent"
        }
    }

    private struct NativeDiversitySlider: UIViewRepresentable {
        let value: Double?
        let presentation: Double
        let range: ClosedRange<Double>
        let step: Double
        let generation: UInt64
        let enabled: Bool
        let title: String
        let identifier: String
        let shown: (Double) -> String
        let callbacks: DiversitySliderControl.Callbacks

        func makeUIView(context: Context) -> DiversitySliderControl { DiversitySliderControl(frame: .zero) }
        func updateUIView(_ view: DiversitySliderControl, context: Context) {
            view.formatValue = shown
            view.tintColor = UIColor(ChromeColours.accent)
            view.accessibilityLabel = title
            view.accessibilityIdentifier = identifier
            view.configure(value: value, presentation: presentation, range: range, step: step, generation: generation,
                           enabled: enabled, callbacks: callbacks)
        }
        static func dismantleUIView(_ view: DiversitySliderControl, coordinator: ()) {
            view.dismantle()
        }
    }

    /// A slider with its name and value. A drag sends at most one value
    /// every 50 ms, the desktop's rate, and the final value when the finger
    /// lifts (``SliderSendPacer``); the mirror then holds the value sent
    /// until the Core answers.
    struct SliderRow: View {
        let title: String
        let value: Double?
        let range: ClosedRange<Double>
        var step: Double = 1
        let shown: (Double) -> String
        var enabled = true
        var titleWidth: CGFloat = 72
        let identifier: String
        var notConfirmed = false
        var editingChanged: ((Bool) -> Void)?
        // Diversity keeps one paced lifetime across repeated primary callbacks until pointer-up.
        var observesPhysicalLifetime = false
        var interactionGeneration: UInt64? = nil
        var captureCommit: ((Double) -> SliderCommitOffer)? = nil
        let commit: (Double) -> Void

        @State private var gestureDraft = SliderGestureDraft()
        @State private var pacer = SliderSendPacer()
        @State private var releaseLifecycle = SliderReleaseLifecycle()
        @State private var offeredGeneration: UInt64?
        @State private var queuedOffer: SliderCommitOffer?

        var body: some View {
            let current = releaseLifecycle.presentedValue(draft: gestureDraft.value, model: value,
                fallback: range.lowerBound, observesPhysicalLifetime: observesPhysicalLifetime)
            HStack(spacing: 10) {
                Text(title)
                    .font(.system(size: 13))
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.textDim)
                    .frame(width: titleWidth, alignment: .leading)
                if observesPhysicalLifetime {
                    NativeDiversitySlider(value: value, presentation: current, range: range, step: step,
                        generation: interactionGeneration ?? 0, enabled: enabled, title: title,
                        identifier: identifier, shown: shown, callbacks: .init(
                            canBegin: { releaseLifecycle.visible && enabled && value != nil },
                            began: physicalBegan, moved: nativeMove, ended: nativeEnded,
                            cancelled: physicalCancelled, adjusted: nativeAdjustment,
                            endInViewUpdate: prepareDeferredNativeEnd))
                        .frame(minHeight: 44)
                } else {
                    Slider(value: Binding(get: { current }, set: { next in
                        #if DEBUG
                        recordOrigin("binding.move", offered: next)
                        #endif
                        if let captureCommit {
                            let offer = captureCommit(next)
                            synchronizeGeneration(offer.generation)
                            queuedOffer = offer
                        }
                        if observesPhysicalLifetime {
                            guard releaseLifecycle.visible, enabled, value != nil, beginEditing() else { return }
                        }
                        gestureDraft.move(to: next)
                        pacer.send = pacedCommit
                        pacer.move(to: next)
                    }), in: range, step: step) { editing in
                        #if DEBUG
                        recordOrigin("primary.\(editing)")
                        #endif
                        if editing {
                            if observesPhysicalLifetime {
                                guard releaseLifecycle.visible, enabled, value != nil, beginEditing() else { return }
                            }
                            if !observesPhysicalLifetime {
                                editingChanged?(true)
                                gestureDraft.begin()
                            }
                        }
                        if !editing { finishEditing() }
                    }
                    .tint(ChromeColours.accent)
                    .disabled(!enabled || value == nil)
                    .accessibilityLabel(title)
                    .accessibilityValue(value == nil ? "Not sent" : shown(current))
                    .accessibilityIdentifier(identifier)
                }
                ValueBox(text: value == nil ? "--" : shown(current))
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .frame(minHeight: 44)
            .onAppear {
                if observesPhysicalLifetime { releaseLifecycle.visible = true }
                if let interactionGeneration { synchronizeGeneration(interactionGeneration) }
            }
            .onChange(of: interactionGeneration) {
                if let interactionGeneration { synchronizeGeneration(interactionGeneration) }
            }
            .onChange(of: value) {
                gestureDraft.modelChanged(notConfirmed: notConfirmed)
            }
            .onChange(of: notConfirmed) { gestureDraft.modelChanged(notConfirmed: notConfirmed) }
            .onChange(of: enabled) {
                if !enabled {
                    #if DEBUG
                    recordOrigin("retire.disabled")
                    #endif
                    if observesPhysicalLifetime { retireEditing() }
                    else { gestureDraft.disable() }
                }
            }
            .onDisappear {
                if observesPhysicalLifetime { releaseLifecycle.visible = false }
                #if DEBUG
                recordOrigin("retire.disappear")
                #endif
                if observesPhysicalLifetime { retireEditing() }
            }
        }

        private func synchronizeGeneration(_ generation: UInt64) {
            guard offeredGeneration != generation else { return }
            // Model admission rejects old offers synchronously; this cancels their local timer too.
            if offeredGeneration != nil { retireEditing() }
            pacer.retire()
            queuedOffer = nil
            offeredGeneration = generation
        }

        private func beginEditing() -> Bool {
            releaseLifecycle.begin {
                gestureDraft.release(model: value)
                editingChanged?(true)
                gestureDraft.begin()
            }
        }

        private func finishDraft() {
            #if DEBUG
            recordOrigin("release.accepted")
            #endif
            pacer.send = pacedCommit
            pacer.release()
            // Flush the final finger value before handing presentation back to the Core.
            gestureDraft.release(model: value)
            editingChanged?(false)
        }

        private func finishEditing() {
            if observesPhysicalLifetime { releaseLifecycle.release(finishDraft) }
            else { finishDraft() }
        }

        private func discardDraft() {
            #if DEBUG
            recordOrigin("retire.accepted")
            #endif
            pacer.retire()
            gestureDraft.release(model: value)
            editingChanged?(false)
        }

        private func retireEditing() { releaseLifecycle.retire(discardDraft) }

        private func physicalBegan() {
            guard releaseLifecycle.visible, enabled, value != nil else { return }
            #if DEBUG
            recordOrigin("physical.begin")
            #endif
            releaseLifecycle.physicalBegan(started: {
                gestureDraft.release(model: value)
                editingChanged?(true)
                gestureDraft.begin()
                if let offer = captureCommit?(value ?? range.lowerBound),
                   offer.generation == (interactionGeneration ?? 0) {
                    releaseLifecycle.retainTerminalCommit(offer)
                }
            }, discarded: discardDraft)
        }

        private func nativeMove(_ next: Double, _ generation: UInt64) -> Bool {
            guard releaseLifecycle.visible, enabled, value != nil else { return false }
            #if DEBUG
            recordOrigin("binding.move", offered: next)
            #endif
            if let captureCommit {
                let offer = captureCommit(next)
                // The native token cannot capture a fresh context just because rendering is late.
                guard offer.generation == generation else {
                    synchronizeGeneration(offer.generation)
                    return false
                }
                synchronizeGeneration(offer.generation)
                releaseLifecycle.retainTerminalCommit(offer)
                queuedOffer = offer
            }
            gestureDraft.move(to: next)
            pacer.send = pacedCommit
            pacer.move(to: next)
            return true
        }

        private func nativeEnded(_ final: Double, _ generation: UInt64) {
            guard let token = releaseLifecycle.physicalEnded() else { return }
            #if DEBUG
            recordOrigin("physical.end", offered: final)
            #endif
            guard nativeMove(final, generation) else {
                retireEditing()
                releaseLifecycle.finishPhysical(token) {}
                return
            }
            // The owned UISlider supplied its logical value after super.endTracking.
            releaseLifecycle.finishPhysical(token, finishDraft)
        }

        private func prepareDeferredNativeEnd(_ final: Double, _ generation: UInt64) -> ((Bool) -> Void)? {
            // The reference lifecycle cached this pure factory during normal input admission/moves.
            // Capturing a fresh model offer here can publish while UIKit is inside configure.
            let offer = releaseLifecycle.revalueTerminalCommit(final)
            guard releaseLifecycle.visible, enabled, value != nil,
                  let token = releaseLifecycle.capturePhysicalFinish(final, pacer: pacer) else { return nil }
            let admitted = captureCommit == nil || offer?.generation == generation
            return { nativeMaySend in
                var accepted = false
                releaseLifecycle.finishCapturedPhysical(token, sending: { rowMaySend in
                    guard nativeMaySend, rowMaySend, admitted else { return }
                    accepted = true
                    #if DEBUG
                    recordOrigin("physical.end", offered: final)
                    recordOrigin("binding.move", offered: final)
                    #endif
                    gestureDraft.move(to: final)
                    if captureCommit != nil { queuedOffer = offer; offeredGeneration = generation }
                    if let released = token.valueToSend {
                        #if DEBUG
                        recordOrigin("paced.send", offered: released)
                        #endif
                        gestureDraft.submitted(released)
                        if let offer { offer() }
                        else { commit(released) }
                    }
                }, publishing: {
                    #if DEBUG
                    recordOrigin(accepted ? "release.accepted" : "retire.accepted")
                    #endif
                    gestureDraft.release(model: value)
                    editingChanged?(false)
                })
            }
        }

        private func nativeAdjustment(_ next: Double, _ generation: UInt64) -> Bool {
            guard releaseLifecycle.visible, enabled, value != nil, beginEditing() else { return false }
            guard nativeMove(next, generation) else { retireEditing(); return false }
            finishEditing()
            return true
        }

        private func physicalCancelled() {
            guard releaseLifecycle.hasPhysicalLifetime else { return }
            releaseLifecycle.cancelPhysical(retiring: { pacer.retire() }) {
                #if DEBUG
                recordOrigin("physical.cancelled")
                #endif
                discardDraft()
            }
        }

        private func pacedCommit(_ next: Double) {
            #if DEBUG
            recordOrigin("paced.send", offered: next)
            #endif
            gestureDraft.submitted(next)
            if captureCommit != nil {
                guard let queuedOffer, queuedOffer.value == next else { return }
                queuedOffer()
            } else { commit(next) }
        }

        #if DEBUG
        private func recordOrigin(_ event: String, offered: Double? = nil) {
            guard observesPhysicalLifetime else { return }
            UITestDiversityFixture.sliderLifecycle.origin(event, identifier: identifier,
                releaseEditing: releaseLifecycle.editing, draftEditing: gestureDraft.editing,
                draft: gestureDraft.value, model: value, offered: offered)
        }
        #endif
    }

    /// A row of choices, the chosen one blue. Greyed, the chosen one keeps
    /// a lighter grey outline and text, as a greyed Setup list keeps its
    /// tick in grey: still readable, but plainly not something to press.
    struct Choices: View {
        let options: [(id: Int64, label: String)]
        let selected: Int64?
        var enabled = true
        let identifier: String
        let pick: (Int64) -> Void

        /// How one choice is drawn.
        struct Look: Equatable {
            let fill: Color
            let border: Color
            let text: Color
            let opacity: Double
        }

        static func look(chosen: Bool, enabled: Bool) -> Look {
            switch (chosen, enabled) {
            case (true, true):
                return Look(fill: ChromeColours.buttonOnBlue, border: ChromeColours.buttonOnBlueBorder,
                            text: .white, opacity: 1)
            case (false, true):
                return Look(fill: ChromeColours.button, border: ChromeColours.buttonBorder,
                            text: ChromeColours.text, opacity: 1)
            case (true, false):
                return Look(fill: ChromeColours.buttonOff, border: ChromeColours.caption,
                            text: ChromeColours.caption, opacity: 1)
            case (false, false):
                return Look(fill: ChromeColours.buttonOff, border: ChromeColours.buttonOffBorder,
                            text: ChromeColours.buttonOffText, opacity: 0.7)
            }
        }

        var body: some View {
            HStack(spacing: 5) {
                ForEach(options, id: \.id) { option in
                    let on = option.id == selected
                    let look = Self.look(chosen: on, enabled: enabled)
                    Button {
                        pick(option.id)
                    } label: {
                        Text(option.label)
                            .font(.system(size: 12, weight: .bold))
                            .lineLimit(1)
                            .minimumScaleFactor(0.8)
                            .foregroundStyle(look.text)
                            .frame(maxWidth: .infinity, minHeight: 32)
                            .background(look.fill, in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(look.border, lineWidth: 1))
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .disabled(!enabled)
                    .opacity(look.opacity)
                    .accessibilityLabel(option.label)
                    .accessibilityAddTraits(on ? .isSelected : [])
                    .accessibilityIdentifier("\(identifier).\(option.id)")
                }
            }
        }
    }
}
