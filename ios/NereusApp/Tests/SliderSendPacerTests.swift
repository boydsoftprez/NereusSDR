// NereusSDR for iOS: a slider drag sends at the desktop's rate and its final value on release
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import UIKit
@testable import NereusSDR
import Testing

/// JJ, 2026-10-01: a slider drag sends at most one value every 50 ms, the
/// desktop's rate (`StationClient.h:644`), and the final value on release.
/// Time moves only on a test clock.
@Suite("Slider pacing")
@MainActor
struct SliderSendPacerTests {
    @Test("the first value goes at once, then one every 50 ms, the latest winning")
    func pacesAtTheDesktopsRate() async {
        let clock = TestLinkClock()
        var sent: [Double] = []
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        pacer.move(to: 10)
        #expect(sent == [10])
        pacer.move(to: 11)
        await clock.advance(by: 20)
        pacer.move(to: 12)
        pacer.move(to: 13)
        await clock.advance(by: 29)
        #expect(sent == [10])
        await clock.advance(by: 1)
        #expect(sent == [10, 13])
        await clock.advance(by: 100)
        #expect(sent == [10, 13])
    }

    @Test("the final value goes on release, once, and a value already sent is not sent again")
    func finalValueOnRelease() async {
        let clock = TestLinkClock()
        var sent: [Double] = []
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        pacer.move(to: 10)
        pacer.move(to: 14)
        pacer.release()
        #expect(sent == [10, 14])
        await clock.advance(by: 100)
        #expect(sent == [10, 14])
        pacer.move(to: 20)
        pacer.release()
        #expect(sent == [10, 14, 20])
    }
    @Test("both rows relinquish a stationary gesture draft after refusal or a clamped answer", arguments: [50.0, 70.0])
    func answerDuringStationaryDrag(coreValue: Double) async {
        let clock = TestLinkClock()
        var model = 50.0
        var sent: [Double] = []
        var gesture = SliderGestureDraft()
        let pacer = SliderSendPacer(clock: clock) { sent.append($0); model = $0 }
        gesture.move(to: 85)
        pacer.move(to: 85)
        #expect(gesture.value == 85)
        #expect(sent == [85])
        model = coreValue
        gesture.modelChanged()
        #expect(gesture.value == 85) // Finger is still down.
        pacer.release()
        gesture.release(model: model)
        #expect(gesture.value == nil)
        #expect((gesture.value ?? model) == coreValue)
        #expect(!gesture.editing)
        await clock.advance(by: 100)
        #expect(sent == [85]) // Stationary lift sends nothing again.
    }

    @Test("a submitted timeout relinquishes only its own draft and preserves a newer unsubmitted gesture")
    func timeoutDraftOwnership() {
        var gesture = SliderGestureDraft()
        gesture.move(to: 85)
        gesture.submitted(85)
        gesture.move(to: 90)
        gesture.modelChanged(notConfirmed: true)
        #expect(gesture.value == 90 && gesture.editing)
        gesture.submitted(90)
        gesture.modelChanged(notConfirmed: true)
        #expect(gesture.value == nil && gesture.editing)
        gesture.modelChanged(notConfirmed: false)
        #expect(gesture.value == nil)
    }

    @Test("an unsent final value reaches the model before the shared draft is relinquished")
    func unsentFinalHandsPresentationToModel() {
        let clock = TestLinkClock()
        var model = 50.0
        var sent: [Double] = []
        var gesture = SliderGestureDraft()
        let pacer = SliderSendPacer(clock: clock) { sent.append($0); model = $0 }
        gesture.move(to: 60)
        pacer.move(to: 60)
        gesture.move(to: 85)
        pacer.move(to: 85)
        model = 50
        gesture.modelChanged()
        pacer.release()
        gesture.release(model: model)
        #expect(sent == [60, 85])
        #expect(gesture.value == nil)
        #expect(model == 85)
    }

    @Test("Diversity native release retires a lost primary callback and shows the next Core value")
    func nativeReleaseAfterLostPrimary() async {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var gesture = SliderGestureDraft()
        var model = 50.0
        var sent: [Double] = []
        var ended = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0); gesture.submitted($0); model = $0 }
        lifecycle.begin()
        gesture.begin()
        gesture.move(to: 60); pacer.move(to: 60)
        gesture.move(to: 85); pacer.move(to: 85)
        model = 50
        gesture.modelChanged()
        #expect(gesture.value == 85 && gesture.editing) // Core echo does not erase the finger.
        // Only the native fallback arrives; the primary editing-end callback was lost.
        lifecycle.release {
            pacer.release()
            gesture.release(model: model)
            ended += 1
        }
        #expect(sent == [60, 85] && model == 85)
        #expect(!lifecycle.editing && !gesture.editing && gesture.value == nil && ended == 1)
        model = -2 // A later authoritative memory recall is immediately presented.
        gesture.modelChanged()
        #expect((gesture.value ?? model) == -2)
        await clock.advance(by: 100)
        #expect(sent == [60, 85])
    }

    @Test("Diversity normal and fallback release finish once in either order", arguments: [true, false])
    func duplicateReleaseIsIdempotent(normalFirst: Bool) async {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var gesture = SliderGestureDraft()
        var sent: [Double] = []
        var paths: [String] = []
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.begin(); gesture.begin()
        gesture.move(to: 10); pacer.move(to: 10)
        gesture.move(to: 14); pacer.move(to: 14)
        let finish: (String) -> Void = { path in
            lifecycle.release {
                paths.append(path)
                // Reserving the release also prevents a reentrant duplicate callback.
                lifecycle.release { paths.append("reentrant") }
                pacer.release()
                gesture.release(model: 14)
            }
        }
        finish(normalFirst ? "normal" : "fallback")
        finish(normalFirst ? "fallback" : "normal")
        #expect(paths == [normalFirst ? "normal" : "fallback"])
        #expect(sent == [10, 14] && !gesture.editing && gesture.value == nil)
        await clock.advance(by: 100)
        #expect(sent == [10, 14])
    }

    @Test("retired Diversity contexts discard a held value and ignore a late release")
    func retiredContextHasNoPendingReplay() async {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var gesture = SliderGestureDraft()
        var sent: [Double] = []
        var ended = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.begin(); gesture.begin()
        gesture.move(to: 10); pacer.move(to: 10)
        gesture.move(to: 14); pacer.move(to: 14)
        lifecycle.retire {
            pacer.retire()
            gesture.release(model: 10)
            ended += 1
        }
        lifecycle.release { pacer.release(); ended += 1 }
        lifecycle.retire { pacer.retire(); ended += 1 }
        await clock.advance(by: 100)
        #expect(sent == [10] && ended == 1 && !gesture.editing && gesture.value == nil)
        // A fresh context's gesture can start without replaying the retired 14.
        lifecycle.begin(); gesture.begin()
        gesture.move(to: 20); pacer.move(to: 20)
        lifecycle.release { pacer.release(); gesture.release(model: 20) }
        await clock.advance(by: 100)
        #expect(sent == [10, 20] && !lifecycle.editing)
    }

    @Test("repeated primary editing-end callbacks during one physical drag keep the 50 ms lifetime")
    func repeatedPrimaryEndsDuringPhysicalDragStayPaced() async throws {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var sent: [Double] = []
        var starts = 0
        var ends = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        let finish = { pacer.release(); ends += 1 }
        lifecycle.physicalBegan(started: { starts += 1 }, discarded: { pacer.retire() })
        // Arm the hold at zero, then replay primary callbacks while the pointer stays down.
        for (advance, value) in [(Int64(0), 10.0), (0, 11.0), (20, 11.0), (20, 12.0)] {
            await clock.advance(by: advance)
            lifecycle.begin { starts += 1 }
            pacer.move(to: value)
            lifecycle.release(finish)
        }
        #expect(clock.pendingDueTimes == [50])
        #expect(sent == [10])
        await clock.advance(by: 9)
        #expect(sent == [10])
        await clock.advance(by: 1)
        #expect(sent == [10, 12])
        #expect(starts == 1 && ends == 0 && lifecycle.editing)
        await clock.advance(by: 10)
        pacer.move(to: 14)
        let token = try #require(lifecycle.physicalEnded())
        lifecycle.release(finish) // Slider's end callback cannot beat the actual end fence.
        #expect(sent == [10, 12] && ends == 0)
        lifecycle.finishPhysical(token, finish)
        lifecycle.finishPhysical(token, finish)
        lifecycle.release(finish)
        #expect(sent == [10, 12, 14] && ends == 1 && !lifecycle.editing)
        await clock.advance(by: 100)
        #expect(sent == [10, 12, 14])
    }

    @Test("a cancelled physical drag discards the held value and cannot deliver a later final", arguments: [false, true])
    func physicalCancellationDiscardsHeldValue(endedBeforeCancel: Bool) async {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var sent: [Double] = []
        var ends = 0
        var discards = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.physicalBegan(started: {}, discarded: { pacer.retire() })
        pacer.move(to: 10); pacer.move(to: 14)
        let token = endedBeforeCancel ? lifecycle.physicalEnded() : nil
        lifecycle.cancelPhysical(retiring: { pacer.retire() }) { discards += 1 }
        if let token { lifecycle.finishPhysical(token) { pacer.release(); ends += 1 } }
        #expect(lifecycle.physicalEnded() == nil)
        lifecycle.release { pacer.release(); ends += 1 }
        await nextMainQueueTurn()
        await clock.advance(by: 100)
        #expect(sent == [10] && discards == 1 && ends == 0 && !lifecycle.editing)
        #expect(!lifecycle.hasPhysicalLifetime)
    }

    @Test("a scheduled physical end cannot flush or finish a newer physical drag")
    func scheduledEndCannotFinishNewPhysicalDrag() async throws {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var sent: [Double] = []
        var ends = 0
        var discards = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        let discard = { pacer.retire(); discards += 1 }
        let finish = { pacer.release(); ends += 1 }
        lifecycle.physicalBegan(started: {}, discarded: discard)
        pacer.move(to: 10); pacer.move(to: 14)
        let old = try #require(lifecycle.physicalEnded())
        lifecycle.physicalBegan(started: {}, discarded: discard)
        pacer.move(to: 20); pacer.move(to: 24)
        lifecycle.finishPhysical(old, finish)
        #expect(sent == [10, 20] && ends == 0 && discards == 1 && lifecycle.editing)
        let current = try #require(lifecycle.physicalEnded())
        lifecycle.finishPhysical(current, finish)
        lifecycle.finishPhysical(old, finish)
        await clock.advance(by: 100)
        #expect(sent == [10, 20, 24] && ends == 1 && !lifecycle.editing)
    }

    @Test("end-event binding updates join the ending generation before one final flush")
    func endEventBindingUpdateJoinsPhysicalGeneration() throws {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var gesture = SliderGestureDraft()
        var sent: [Double] = []
        var starts = 0
        var ends = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.physicalBegan(started: { starts += 1; gesture.begin() }, discarded: { pacer.retire() })
        gesture.move(to: 10); pacer.move(to: 10)
        let token = try #require(lifecycle.physicalEnded())
        #expect(lifecycle.begin({ starts += 1 }))
        gesture.move(to: 14); pacer.move(to: 14)
        lifecycle.release { pacer.release(); ends += 1 }
        #expect(sent == [10] && gesture.value == 14 && gesture.editing && starts == 1)
        lifecycle.finishPhysical(token) {
            pacer.release(); gesture.release(model: 14); ends += 1
        }
        #expect(sent == [10, 14] && ends == 1 && !gesture.editing && gesture.value == nil)
    }

    @Test("accessibility adjustment without a physical pointer keeps normal paired editing")
    func accessibilityPairedLifecycleStillWorks() async {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var sent: [Double] = []
        var starts = 0
        var ends = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.begin { starts += 1 }
        pacer.move(to: 10); pacer.move(to: 14)
        lifecycle.release { pacer.release(); ends += 1 }
        lifecycle.release { pacer.release(); ends += 1 }
        #expect(sent == [10, 14] && starts == 1 && ends == 1 && !lifecycle.hasPhysicalLifetime)
        lifecycle.begin { starts += 1 }
        pacer.move(to: 20)
        lifecycle.release { pacer.release(); ends += 1 }
        await clock.advance(by: 100)
        #expect(sent == [10, 14, 20] && starts == 2 && ends == 2)
    }

    @Test("retiring a held pointer fences late primary updates until its terminal event")
    func retiredPhysicalPointerCannotReopenContext() async throws {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var sent: [Double] = []
        var starts = 0
        var ends = 0
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.physicalBegan(started: { starts += 1 }, discarded: { pacer.retire() })
        pacer.move(to: 10); pacer.move(to: 14)
        lifecycle.retire { pacer.retire() }
        if lifecycle.begin({ starts += 1 }) { pacer.move(to: 18) }
        lifecycle.release { pacer.release(); ends += 1 }
        #expect(sent == [10] && starts == 1 && ends == 0 && !lifecycle.editing)
        let stale = try #require(lifecycle.physicalEnded())
        #expect(!lifecycle.begin({ starts += 1 }))
        lifecycle.finishPhysical(stale) { pacer.release(); ends += 1 }
        // A subsequent AX edit is usable after the retired pointer's end-event turn.
        #expect(lifecycle.begin({ starts += 1 }))
        pacer.move(to: 20)
        lifecycle.release { pacer.release(); ends += 1 }
        await clock.advance(by: 100)
        #expect(sent == [10, 20] && starts == 2 && ends == 1)
    }

    @Test("post-physical AX moves without a primary end cannot shadow model recall")
    func postPhysicalAXDraftUsesModelPresentation() throws {
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var draft = SliderGestureDraft()
        lifecycle.physicalBegan(started: { draft.begin() }, discarded: {})
        draft.move(to: 200)
        #expect(lifecycle.presentedValue(draft: draft.value, model: 123.4, fallback: 0, observesPhysicalLifetime: true) == 200)
        let end = try #require(lifecycle.physicalEnded())
        #expect(lifecycle.presentedValue(draft: draft.value, model: 123.4, fallback: 0, observesPhysicalLifetime: true) == 200)
        lifecycle.finishPhysical(end) { draft.release(model: 200) }
        lifecycle.begin { draft.begin() }
        draft.move(to: 289.2) // AX animation offers after pointer-up; no primary.false follows.
        #expect(lifecycle.editing && draft.editing)
        #expect(lifecycle.presentedValue(draft: draft.value, model: 123.4, fallback: 0, observesPhysicalLifetime: true) == 123.4)
        #expect(lifecycle.presentedValue(draft: draft.value, model: 123.4, fallback: 0, observesPhysicalLifetime: false) == 289.2)
    }

    @Test("intent retirement discards a pending AX value before a late primary end")
    func supersededAXPendingValueCannotFlushOnLateEnd() async {
        let clock = TestLinkClock()
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        var sent: [Double] = []
        let pacer = SliderSendPacer(clock: clock) { sent.append($0) }
        lifecycle.begin()
        pacer.move(to: 10); pacer.move(to: 14)
        lifecycle.retire { pacer.retire() }
        lifecycle.release { pacer.release() }
        await clock.advance(by: 50)
        #expect(sent == [10] && !lifecycle.editing)
        #expect(lifecycle.begin()) // Fresh AX input remains usable after explicit supersession.
        pacer.move(to: 20)
        lifecycle.release { pacer.release() }
        #expect(sent == [10, 20])
    }


    // These call the same owned-control boundary used around UIKit's tracking methods.
    // Native calls are synchronous seams; the control, origin admission and pacer are real.
    @Test("owned native end captures its final value once and rejects anonymous tail commands")
    func ownedNativeTerminalValueAndTail() async {
        let rig = NativeSliderRig()
        #expect(rig.control.beginPhysical { true })
        rig.move(10); rig.move(12)
        rig.control.endPhysical(hasTouch: true) {
            rig.control.value = 14 // Native terminal value differs from the last valueChanged.
        }
        rig.move(99)
        rig.control.endPhysical(hasTouch: true) {}
        await rig.clock.advance(by: 100)
        #expect(rig.sent == [10, 14] && rig.starts == 1 && rig.ends == 1)
        #expect(rig.control.presentationValue == 14)
    }

    @Test("native begin reserves origin before synchronous valueChanged", arguments: [true, false])
    func ownedNativeProvisionalBegin(accepted: Bool) {
        let rig = NativeSliderRig()
        let result = rig.control.beginPhysical {
            rig.move(11)
            return accepted
        }
        #expect(result == accepted)
        #expect(rig.sent == (accepted ? [11] : []))
        #expect(rig.starts == (accepted ? 1 : 0))
        if accepted { rig.control.cancelPhysical {} }
        #expect(rig.ends == 0)
    }

    @Test("native cancellation fences reentrant value and nil-touch end", arguments: [true, false])
    func ownedNativeCancellation(nilEnd: Bool) async {
        let rig = NativeSliderRig()
        #expect(rig.control.beginPhysical { true })
        rig.move(10); rig.move(14)
        rig.control.cancelPhysical {
            rig.move(99)
            if nilEnd { rig.control.endPhysical(hasTouch: false) { rig.move(98) } }
            rig.control.accessibilityIncrement() // Reentrant nil-end cannot reopen AX inside super.cancel.
        }
        rig.control.endPhysical(hasTouch: true) {}
        await rig.clock.advance(by: 100)
        #expect(rig.sent == [10] && rig.ends == 0 && rig.cancels == 1 && rig.ax == 0)
        rig.control.accessibilityIncrement()
        #expect(rig.sent == [10, 10.1] && rig.ax == 1)
    }

    @Test("generation replacement during native end cannot borrow refreshed callbacks")
    func ownedNativeGenerationDuringEnd() async {
        let rig = NativeSliderRig()
        #expect(rig.control.beginPhysical { true })
        rig.move(10); rig.move(14)
        rig.control.endPhysical(hasTouch: true) {
            rig.generation = 1
            rig.refresh(20)
            rig.move(99)
            rig.control.accessibilityIncrement() // Retired native pointer still owns this turn.
        }
        await rig.clock.advance(by: 100)
        #expect(rig.sent == [10] && rig.ends == 0 && rig.cancels == 1 && rig.ax == 0)
        rig.control.accessibilityIncrement()
        #expect(rig.sent == [10, 20.1] && rig.ax == 1)
    }

    @Test("zero-pointer AX remains usable after physical end and retirement")
    func ownedNativeAXAfterTerminalAndRetirement() {
        let rig = NativeSliderRig()
        #expect(rig.control.beginPhysical { true })
        rig.move(10)
        rig.control.accessibilityIncrement() // Cannot discard or inherit the live finger.
        #expect(rig.ax == 0 && rig.sent == [10])
        rig.control.endPhysical(hasTouch: true) { rig.control.value = 14 }
        rig.control.accessibilityIncrement()
        rig.control.accessibilityDecrement()
        #expect(rig.sent == [10, 14, 14.1, 14] && rig.ax == 2)
        #expect(rig.control.beginPhysical { true })
        rig.move(15)
        rig.generation = 1; rig.refresh(20)
        rig.control.accessibilityIncrement()
        #expect(rig.ax == 2)
        rig.control.endPhysical(hasTouch: true) {}
        rig.control.accessibilityDecrement()
        #expect(rig.sent == [10, 14, 14.1, 14, 15, 19.9] && rig.ax == 3)
    }

    @Test("programmatic model refresh retains exact Double without issuing a command")
    func ownedNativeModelRefresh() {
        let rig = NativeSliderRig()
        let exact = 123.45678901234567
        rig.refresh(exact)
        rig.control.sendActions(for: .valueChanged)
        #expect(rig.control.presentationValue?.bitPattern == exact.bitPattern)
        #expect(rig.sent.isEmpty && rig.starts == 0 && rig.ends == 0 && rig.ax == 0)
        rig.refresh(nil)
        rig.control.accessibilityIncrement()
        #expect(rig.control.presentationValue == nil && rig.control.accessibilityValue == "Not sent")
        #expect(rig.sent.isEmpty && rig.ax == 0)
    }

    @Test("owned user control values use the existing gain step", arguments: [(-2.04 as Float, -2.0), (-2.06 as Float, -2.1)])
    func ownedNativeUserStep(input: Float, expected: Double) {
        let rig = NativeSliderRig(range: -20...20)
        rig.refresh(-2)
        #expect(rig.control.beginPhysical { true })
        rig.move(input)
        rig.control.endPhysical(hasTouch: true) {}
        #expect(rig.sent == [expected])
    }

    @Test("old pointer terminal cannot finish a newer owned pointer")
    func ownedNativeOldPointerTerminal() {
        let rig = NativeSliderRig()
        let old = NSObject(), fresh = NSObject()
        #expect(rig.control.beginPhysical(pointer: old) { true })
        rig.move(10)
        rig.control.endPhysical(hasTouch: true, pointer: old) {}
        #expect(rig.control.beginPhysical(pointer: fresh) { true })
        rig.move(20)
        rig.control.endPhysical(hasTouch: true, pointer: old) { rig.move(99) }
        #expect(rig.sent == [10, 20] && rig.ends == 1)
        rig.control.endPhysical(hasTouch: true, pointer: fresh) { rig.control.value = 24 }
        #expect(rig.sent == [10, 20, 24] && rig.ends == 2)
    }

    @Test("nil native end requires affirmative ended phase from the captured pointer",
          arguments: [nil, UITouch.Phase.moved, .cancelled, .ended])
    func ownedNativeNilTerminalRequiresEndedPointer(phase: UITouch.Phase?) async {
        let rig = NativeSliderRig()
        let pointer = NSObject()
        #expect(rig.control.beginPhysical(pointer: pointer) { true })
        rig.move(10); rig.move(12)
        rig.control.endPhysical(hasTouch: false, retainedPhase: phase) {
            rig.control.value = 14 // Actual terminal value differs from the last offered move.
        }
        rig.move(99) // Anonymous post-terminal input cannot reopen the origin.
        rig.control.endPhysical(hasTouch: false, retainedPhase: .ended) {}
        await rig.clock.advance(by: 100)
        #expect(rig.sent == (phase == .ended ? [10, 14] : [10]))
        #expect(rig.ends == (phase == .ended ? 1 : 0))
        #expect(rig.cancels == (phase == .ended ? 0 : 1))
    }

    @Test("ended-phase nil callback cannot borrow missing, canceled or stale ownership",
          arguments: [0, 1, 2, 3, 4])
    func ownedNativeNilTerminalRetainsOwnershipFence(scenario: Int) async {
        let rig = NativeSliderRig()
        let pointer: AnyObject? = scenario == 0 ? nil : NSObject()
        #expect(rig.control.beginPhysical(pointer: pointer) { true })
        rig.move(10); rig.move(12)
        switch scenario {
        case 1:
            rig.control.cancelPhysical {
                rig.control.endPhysical(hasTouch: false, retainedPhase: .ended) { rig.move(99) }
                rig.control.accessibilityIncrement()
            }
        case 2:
            rig.generation = 1; rig.refresh(20)
            rig.control.endPhysical(hasTouch: false, retainedPhase: .ended) { rig.move(99) }
        case 3:
            rig.control.endPhysical(hasTouch: false, retainedPhase: .ended) {
                rig.generation = 1; rig.refresh(20); rig.move(99)
            }
        case 4:
            rig.control.endPhysical(hasTouch: true, pointer: NSObject(), retainedPhase: .ended) { rig.move(99) }
            #expect(rig.ends == 0 && rig.cancels == 0) // Wrong pointer did not end the actual owner.
            rig.control.cancelPhysical {}
        default:
            rig.control.endPhysical(hasTouch: false, retainedPhase: .ended) { rig.move(99) }
        }
        await rig.clock.advance(by: 100)
        #expect(rig.sent == [10] && rig.ends == 0 && rig.cancels == 1 && rig.ax == 0)
    }

    @Test("active native configure retires safety before deferring draft publication", arguments: [0, 1, 2])
    func ownedNativeConfigureRetirementDefersDraftCleanup(reason: Int) async {
        let rig = ConfigureRetirementRig()
        #expect(rig.control.beginPhysical { true })
        rig.move(10); rig.move(14)
        #expect(rig.clock.pendingDueTimes == [50])
        rig.retireThroughConfigure(reason)
        // These safety fences must already hold before the deferred publication gets a turn.
        #expect(!rig.lifecycle.editing && !rig.lifecycle.hasPhysicalLifetime)
        #expect(rig.safetyRetirements == 1 && rig.clock.pendingDueTimes.isEmpty)
        #expect(rig.control.presentationValue == rig.model) // The retired draft cannot override Core snap-back.
        #expect(rig.publications.isEmpty && rig.draft.value == 14 && rig.draft.editing)
        #expect(rig.notifications == [true])
        rig.move(99)
        rig.control.accessibilityIncrement() // The retired native pointer still fences origin admission.
        rig.control.endPhysical(hasTouch: false) { rig.move(98) }
        #expect(rig.sent == [10] && rig.safetyRetirements == 1)
        await nextMainQueueTurn()
        #expect(rig.publications == [false]) // Cleanup ran after configure returned.
        #expect(rig.draft.value == nil && !rig.draft.editing && rig.notifications == [true, false])
        await rig.clock.advance(by: 100)
        #expect(rig.sent == [10])
    }

    @Test("deferred native configure cleanup cannot discard a newer physical or AX gesture",
          arguments: [(0, true), (1, true), (2, true), (0, false), (1, false), (2, false)])
    func ownedNativeDeferredRetirementCannotDiscardNewGesture(reason: Int, physical: Bool) async {
        let rig = ConfigureRetirementRig()
        #expect(rig.control.beginPhysical { true })
        rig.move(10); rig.move(14)
        rig.retireThroughConfigure(reason)
        #expect(rig.safetyRetirements == 1 && rig.clock.pendingDueTimes.isEmpty)
        #expect(rig.control.presentationValue == rig.model)
        rig.control.endPhysical(hasTouch: true) { rig.move(99) }
        rig.refresh(20)
        if physical {
            #expect(rig.control.beginPhysical { true })
            #expect(rig.beginPresentations == [10, 20]) // NEW begin cannot borrow OLD's draft before moving.
            rig.move(20); rig.move(24)
            #expect(rig.clock.pendingDueTimes == [50])
        } else {
            rig.control.accessibilityIncrement() // Real AX admission starts and finishes a fresh row lifetime.
            #expect(rig.beginPresentations == [10, 20]) // Captured at AX begin, before its native move.
        }
        await nextMainQueueTurn()
        #expect(rig.publications.isEmpty) // OLD cleanup cannot release or discard NEW's draft.
        #expect(rig.notifications == (physical ? [true, true] : [true, true, false]))
        if physical {
            #expect(rig.lifecycle.editing && rig.draft.editing && rig.draft.value == 24)
            #expect(rig.clock.pendingDueTimes == [50])
            rig.control.endPhysical(hasTouch: true) { rig.control.value = 26 }
            #expect(rig.sent == [10, 20, 26] && rig.notifications == [true, true, false])
        } else {
            #expect(!rig.lifecycle.editing && !rig.draft.editing && rig.draft.value == nil)
            #expect(rig.sent == [10, 20.1])
        }
        await rig.clock.advance(by: 100)
        #expect(rig.sent == (physical ? [10, 20, 26] : [10, 20.1]))
    }

    // A main-queue FIFO barrier, not a sleep: all cleanup enqueued by the preceding
    // synchronous configure has either run or been rejected before this resumes.
    private func nextMainQueueTurn() async {
        await withCheckedContinuation { continuation in
            DispatchQueue.main.async { continuation.resume() }
        }
    }

    // Uses the real production control, row release lifecycle, draft and pacer. Only
    // the SwiftUI state storage is represented by values whose publication is observed.
    @MainActor private final class ConfigureRetirementRig {
        let control = ToolPageParts.DiversitySliderControl(frame: .zero)
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        let clock = TestLinkClock()
        var draft = SliderGestureDraft()
        var sent: [Double] = []
        var notifications: [Bool] = []
        var publications: [Bool] = []
        var beginPresentations: [Double] = []
        var safetyRetirements = 0
        var generation: UInt64 = 0
        var model: Double? = 10
        var configuring = false
        lazy var pacer = SliderSendPacer(clock: clock) { [unowned self] in sent.append($0) }

        init() { refresh(10) }
        func retireThroughConfigure(_ reason: Int) {
            if reason == 0 { generation = 1 }
            refresh(reason == 2 ? nil : 20, enabled: reason != 1)
        }
        func refresh(_ value: Double?, enabled: Bool = true) {
            model = value
            configuring = true
            defer { configuring = false }
            let presentation = lifecycle.presentedValue(draft: draft.value, model: value, fallback: 0,
                                                       observesPhysicalLifetime: true)
            control.configure(value: value, presentation: presentation, range: 0...360, step: 0.1, generation: generation,
                              enabled: enabled, callbacks: .init(
                canBegin: { true }, began: { [unowned self] in
                    lifecycle.physicalBegan(started: { beginDraft() },
                                            discarded: { pacer.retire(); draft.release(model: model) })
                }, moved: { [unowned self] value, captured in
                    guard captured == generation else { return false }
                    draft.move(to: value); pacer.move(to: value); return true
                }, ended: { [unowned self] value, captured in
                    guard captured == generation, let token = lifecycle.physicalEnded() else { return }
                    draft.move(to: value); pacer.move(to: value)
                    lifecycle.finishPhysical(token) { finishDraft() }
                }, cancelled: { [unowned self] in
                    lifecycle.cancelPhysical(retiring: { pacer.retire(); safetyRetirements += 1 }) {
                        self.publications.append(self.configuring)
                        self.draft.release(model: self.model); self.notifications.append(false)
                    }
                }, adjusted: { [unowned self] value, captured in
                    guard captured == generation else { return false }
                    guard lifecycle.begin({ beginDraft() }) else { return false }
                    draft.move(to: value); pacer.move(to: value)
                    lifecycle.release { finishDraft() }
                    return true
                }))
        }
        func beginDraft() {
            draft.release(model: model)
            notifications.append(true); draft.begin()
            beginPresentations.append(lifecycle.presentedValue(draft: draft.value, model: model, fallback: 0,
                                                               observesPhysicalLifetime: true))
        }
        func finishDraft() {
            pacer.release(); draft.release(model: model); notifications.append(false)
        }
        func move(_ value: Float) {
            control.value = value
            control.sendActions(for: .valueChanged)
        }
    }

    @Test("a native end reentered by configure captures its unsent final before safe publication", arguments: [(false, false), (true, false), (false, true), (true, true)])
    func ownedNativeConfigureReentrantEndDefersPublication(alreadySent: Bool, inEndMove: Bool) async {
        let rig = ConfigureTerminalRig()
        #expect(rig.control.beginPhysical(pointer: NSObject()) { true })
        rig.move(10); rig.move(12)
        #expect(rig.clock.pendingDueTimes == [50])
        rig.endDuringConfigure(final: alreadySent ? 10 : 14, inEndMove: inEndMove)
        #expect(rig.movesDuringConfigure == 0) // super.endTracking cannot publish moved input inside the update.
        #expect(rig.capturesDuringConfigure == 0) // The model capture boundary is used only before reentry.
        #expect(!rig.lifecycle.editing && !rig.lifecycle.hasPhysicalLifetime)
        #expect(rig.clock.pendingDueTimes.isEmpty && rig.sent == [10])
        #expect(rig.publications.isEmpty && rig.notifications == [true])
        #expect(rig.control.presentationValue == (alreadySent ? 10 : 14))
        rig.refresh(10) // A duplicate precomputed Core presentation cannot overwrite the captured terminal.
        #expect(rig.control.presentationValue == (alreadySent ? 10 : 14))
        rig.move(99) // Anonymous native tail is fenced before the queued row publication.
        await nextMainQueueTurn()
        #expect(rig.publications == [false] && rig.notifications == [true, false])
        #expect(rig.sent == (alreadySent ? [10] : [10, 14]))
        #expect(rig.capturedOwners == ["OLD"] && rig.sentOwners.allSatisfy { $0 == "OLD" })
        #expect(!rig.draft.editing && rig.draft.value == nil)
        rig.refresh(10) // Core may restore the same old value after timeout/refusal.
        #expect(rig.control.presentationValue == 10) // Captured terminal protection ended with delivery.
        await rig.clock.advance(by: 100)
        #expect(rig.sent == (alreadySent ? [10] : [10, 14]))
        let withoutMove = ConfigureTerminalRig()
        #expect(withoutMove.control.beginPhysical(pointer: NSObject()) { true })
        withoutMove.endDuringConfigure(final: 14, inEndMove: inEndMove)
        #expect(withoutMove.capturesDuringConfigure == 0 && withoutMove.sent.isEmpty)
        await nextMainQueueTurn()
        #expect(withoutMove.sent == [14] && withoutMove.capturedOwners == ["OLD"])
        #expect(withoutMove.notifications == [true, false] && !withoutMove.draft.editing)
    }

    @Test("a configure-reentered final cannot borrow a newer physical, AX or owner lifetime", arguments: [0, 1, 2, 3, 4])
    func ownedNativeConfigureReentrantEndRejectsReplacement(replacement: Int) async {
        let rig = ConfigureTerminalRig()
        #expect(rig.control.beginPhysical(pointer: NSObject()) { true })
        rig.move(10); rig.move(12)
        rig.endDuringConfigure(final: 14)
        #expect(rig.clock.pendingDueTimes.isEmpty && rig.sent == [10])
        #expect(rig.capturesDuringConfigure == 0)
        let replacementPointer = NSObject()
        switch replacement {
        case 0:
            rig.refresh(20)
            #expect(rig.control.beginPhysical(pointer: replacementPointer) { true })
            rig.move(20); rig.move(24)
        case 1:
            rig.refresh(20); rig.control.accessibilityIncrement()
        case 2:
            rig.owner = "NEW"; rig.generation = 1
            rig.lifecycle.retire { rig.pacer.retire() }
            rig.refresh(20)
        case 3:
            rig.lifecycle.retire { rig.pacer.retire() }
            rig.refresh(20, enabled: false)
        default:
            rig.lifecycle.retire { rig.pacer.retire() }
            rig.refresh(nil)
        }
        await nextMainQueueTurn()
        #expect(rig.capturedOwners == ["OLD"]) // Deferred delivery never captures a replacement route/owner.
        if replacement == 0 {
            #expect(rig.publications.isEmpty && rig.notifications == [true, true])
            #expect(rig.lifecycle.editing && rig.draft.value == 24 && rig.draft.editing)
            #expect(rig.clock.pendingDueTimes == [50] && rig.sent == [10, 20])
            rig.control.endPhysical(hasTouch: true, pointer: replacementPointer) { rig.control.value = 26 }
            #expect(rig.sent == [10, 20, 26] && rig.notifications == [true, true, false])
        } else if replacement == 1 {
            #expect(rig.publications.isEmpty && rig.notifications == [true, true, false])
            #expect(rig.sent == [10, 20.1])
        } else {
            #expect(rig.publications.isEmpty && rig.notifications == [true, false])
            #expect(rig.sent == [10] && !rig.lifecycle.editing && !rig.lifecycle.hasPhysicalLifetime)
            #expect(!rig.draft.editing && rig.draft.value == nil)
        }
        await rig.clock.advance(by: 100)
        #expect(!rig.sent.contains(14))
    }

    @MainActor private final class ConfigureTerminalRig {
        let control = ToolPageParts.DiversitySliderControl(frame: .zero)
        let lifecycle = ToolPageParts.SliderReleaseLifecycle()
        let clock = TestLinkClock()
        var draft = SliderGestureDraft()
        var generation: UInt64 = 0
        var owner = "OLD"
        var model: Double? = 10
        var configuring = false
        var sent: [Double] = []
        var sentOwners: [String] = []
        var capturedOwners: [String] = []
        var notifications: [Bool] = []
        var publications: [Bool] = []
        var movesDuringConfigure = 0
        var capturesDuringConfigure = 0
        private var cachedOwner = "OLD"
        lazy var pacer = SliderSendPacer(clock: clock) { [unowned self] in
            sent.append($0); sentOwners.append(owner)
        }
        init() { refresh(10) }
        func beginDraft() {
            draft.release(model: model); notifications.append(true); draft.begin()
        }
        func finishDraft() { pacer.release(); draft.release(model: model); notifications.append(false) }
        func normalEnd(_ final: Double, _ captured: UInt64) {
            guard captured == generation, let token = lifecycle.physicalEnded() else { return }
            capturedOwners.append(owner)
            publications.append(configuring)
            draft.move(to: final); pacer.move(to: final)
            lifecycle.finishPhysical(token) { finishDraft() }
        }
        func captureOffer(_ value: Double) -> ToolPageParts.SliderCommitOffer {
            if configuring { capturesDuringConfigure += 1 }
            let capturedOwner = owner, capturedGeneration = generation
            cachedOwner = capturedOwner
            let sendValue: (Double) -> Void = { [unowned self] final in
                guard self.owner == capturedOwner, self.generation == capturedGeneration else { return }
                self.sent.append(final); self.sentOwners.append(capturedOwner)
            }
            return ToolPageParts.SliderCommitOffer(value: value, generation: capturedGeneration, revalue: { final in
                ToolPageParts.SliderCommitOffer(value: final, generation: capturedGeneration) { sendValue(final) }
            }) { sendValue(value) }
        }
        func prepareEnd(_ final: Double, _ captured: UInt64) -> ((Bool) -> Void)? {
            let offer = lifecycle.revalueTerminalCommit(final)
            guard let token = lifecycle.capturePhysicalFinish(final, pacer: pacer) else { return nil }
            let capturedOwner = cachedOwner
            capturedOwners.append(capturedOwner)
            return { [unowned self] nativeMaySend in
                self.lifecycle.finishCapturedPhysical(token, sending: { rowMaySend in
                    guard nativeMaySend, rowMaySend, self.generation == captured, self.owner == capturedOwner else { return }
                    self.publications.append(self.configuring)
                    self.draft.move(to: final)
                    if token.valueToSend != nil, let offer, offer.generation == captured { offer() }
                }, publishing: {
                    self.draft.release(model: self.model); self.notifications.append(false)
                })
            }
        }
        func endDuringConfigure(final: Float, inEndMove: Bool = false) {
            refresh(10, nativePresentation: {
                self.control.endPhysical(hasTouch: false, retainedPhase: .ended) {
                    if inEndMove { self.move(final) }
                    else { self.control.value = final }
                }
            })
        }
        func refresh(_ value: Double?, enabled: Bool = true, nativePresentation: (() -> Void)? = nil) {
            model = value; configuring = true
            defer { configuring = false }
            control.configure(value: value, presentation: value, range: 0...360, step: 0.1, generation: generation,
                              enabled: enabled, callbacks: .init(
                canBegin: { true }, began: { [unowned self] in
                    lifecycle.physicalBegan(started: {
                        beginDraft(); lifecycle.retainTerminalCommit(captureOffer(model ?? 0))
                    }, discarded: { pacer.retire(); draft.release(model: model) })
                }, moved: { [unowned self] value, captured in
                    guard captured == generation else { return false }
                    if configuring { movesDuringConfigure += 1 }
                    lifecycle.retainTerminalCommit(captureOffer(value))
                    draft.move(to: value); pacer.move(to: value); return true
                }, ended: { [unowned self] final, captured in normalEnd(final, captured) },
                cancelled: { [unowned self] in
                    lifecycle.cancelPhysical(retiring: { pacer.retire() }) {
                        self.draft.release(model: self.model); self.notifications.append(false)
                    }
                }, adjusted: { [unowned self] value, captured in
                    guard captured == generation, lifecycle.begin({ beginDraft() }) else { return false }
                    draft.move(to: value); pacer.move(to: value); lifecycle.release { finishDraft() }; return true
                }, endInViewUpdate: { [unowned self] final, captured in prepareEnd(final, captured) }),
                nativePresentation: nativePresentation)
        }
        func move(_ value: Float) { control.value = value; control.sendActions(for: .valueChanged) }
    }

    @MainActor private final class NativeSliderRig {
        let control = ToolPageParts.DiversitySliderControl(frame: .zero)
        let clock = TestLinkClock()
        var sent: [Double] = []
        var starts = 0
        var ends = 0
        var cancels = 0
        var ax = 0
        var generation: UInt64 = 0
        let range: ClosedRange<Double>
        lazy var pacer = SliderSendPacer(clock: clock) { [unowned self] in
            sent.append($0); refresh($0) // Synthetic authoritative echo through the real configure boundary.
        }

        init(range: ClosedRange<Double> = 0...360) {
            self.range = range
            refresh(10)
        }
        func refresh(_ value: Double?) {
            control.configure(value: value, range: range, step: 0.1, generation: generation,
                              enabled: true, callbacks: .init(
                canBegin: { true }, began: { [unowned self] in starts += 1 },
                moved: { [unowned self] value, captured in
                    guard captured == generation else { return false }
                    pacer.move(to: value); return true
                }, ended: { [unowned self] value, captured in
                    guard captured == generation else { return }
                    pacer.move(to: value); pacer.release(); ends += 1
                }, cancelled: { [unowned self] in pacer.retire(); cancels += 1 },
                adjusted: { [unowned self] value, captured in
                    guard captured == generation else { return false }
                    pacer.move(to: value); pacer.release(); ax += 1; return true
                }))
        }
        func move(_ value: Float) {
            control.value = value
            control.sendActions(for: .valueChanged)
        }
    }

}
