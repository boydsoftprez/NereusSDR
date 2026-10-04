// NereusSDR for iOS: a stand-in audio session for tests that routes the way the phone does
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
@testable import NereusSDR

/// An ``AudioSessionPort`` that records what it is told, and on which
/// thread, and picks its output the way iOS does for `.playAndRecord`: an
/// override to the speaker wins; otherwise a connected device plays when
/// its kind is allowed (Bluetooth needs `.allowBluetoothA2DP`, wired
/// headphones need nothing); otherwise the speaker with `.defaultToSpeaker`,
/// or the earpiece. An iPad has no earpiece and plays through its speaker.
/// As on the phone, a category change drops the override. With
/// ``ignoresDefaultToSpeaker`` it plays the way JJ's iPhone did on
/// 2026-09-26: the receiver, whatever the speaker default says, unless the
/// speaker override is on. ``calls`` keeps every call in order.
/// The controller calls it on its queue while tests read it on the main
/// actor, so everything is behind a lock.
final class FakeAudioSession: AudioSessionPort, @unchecked Sendable {
    struct Category: Equatable {
        let category: AVAudioSession.Category
        let mode: AVAudioSession.Mode
        let options: AVAudioSession.CategoryOptions
    }

    struct Activation: Equatable {
        let active: Bool
        let options: AVAudioSession.SetActiveOptions
        let onMainThread: Bool
    }

    enum Device: Equatable {
        case bluetooth(String)
        case wired(String)
    }

    private let lock = NSLock()
    private var recordedCategories: [Category] = []
    private var recordedOverrides: [AVAudioSession.PortOverride] = []
    private var recordedActivations: [Activation] = []
    private var connected: Device?
    private var overrideOn: AVAudioSession.PortOverride = .none
    private var dropsDefaultToSpeaker = false
    private var recordedCalls: [String] = []
    let isPhone: Bool

    init(isPhone: Bool = true, device: Device? = nil) {
        self.isPhone = isPhone
        connected = device
    }

    private func locked<T>(_ body: () -> T) -> T {
        lock.lock()
        defer {
            lock.unlock()
        }
        return body()
    }

    var categories: [Category] {
        locked { recordedCategories }
    }

    var overrides: [AVAudioSession.PortOverride] {
        locked { recordedOverrides }
    }

    /// Every call, in order: "category <options raw value>", "haptics
    /// allowed", "active true", "override speaker" or "override none".
    var calls: [String] {
        locked { recordedCalls }
    }

    /// The phone ignores `.defaultToSpeaker` (see the type's note).
    var ignoresDefaultToSpeaker: Bool {
        get { locked { dropsDefaultToSpeaker } }
        set { locked { dropsDefaultToSpeaker = newValue } }
    }

    /// iOS dropped the override on its own (a route change, a unit starting).
    func dropOverride() {
        locked { overrideOn = .none }
    }

    var diagnostics: String {
        locked { "override \(overrideOn == .speaker ? "speaker" : "none")" }
    }

    var activations: [Activation] {
        locked { recordedActivations }
    }

    /// How many times haptics and system sounds were allowed while recording.
    var hapticsAllowed: Int {
        locked { recordedCalls.filter { $0 == "haptics allowed" }.count }
    }

    var isActive: Bool {
        locked { recordedActivations.last?.active ?? false }
    }

    /// What is plugged in or paired and in range.
    var device: Device? {
        get { locked { connected } }
        set { locked { connected = newValue } }
    }

    func setCategory(_ category: AVAudioSession.Category, mode: AVAudioSession.Mode,
                     options: AVAudioSession.CategoryOptions) throws {
        locked {
            recordedCategories.append(Category(category: category, mode: mode, options: options))
            recordedCalls.append("category \(options.rawValue)")
            overrideOn = .none
        }
    }

    func overrideOutputAudioPort(_ port: AVAudioSession.PortOverride) throws {
        locked {
            recordedOverrides.append(port)
            recordedCalls.append("override \(port == .speaker ? "speaker" : "none")")
            overrideOn = port
        }
    }

    func setActive(_ active: Bool, options: AVAudioSession.SetActiveOptions) throws {
        let onMain = Thread.isMainThread
        locked {
            recordedActivations.append(Activation(active: active, options: options, onMainThread: onMain))
            recordedCalls.append("active \(active)")
        }
    }

    func allowHapticsWhileRecording() {
        locked { recordedCalls.append("haptics allowed") }
    }

    var currentOutputs: [AudioOutputPort] {
        locked {
            let options = recordedCategories.last?.options ?? []
            if overrideOn == .speaker {
                return [AudioOutputPort(kind: .speaker, name: "Speaker")]
            }
            switch connected {
            case .bluetooth(let name) where options.contains(.allowBluetoothA2DP)
                || options.contains(.allowBluetoothHFP):
                return [AudioOutputPort(kind: .external, name: name, pausesOnRemoval: true, isHeadphones: true)]
            case .wired(let name):
                return [AudioOutputPort(kind: .external, name: name, pausesOnRemoval: true, isHeadphones: true)]
            case .bluetooth, nil:
                break
            }
            if (options.contains(.defaultToSpeaker) && !dropsDefaultToSpeaker) || !isPhone {
                return [AudioOutputPort(kind: .speaker, name: "Speaker")]
            }
            return [AudioOutputPort(kind: .earpiece, name: "Receiver")]
        }
    }
}
