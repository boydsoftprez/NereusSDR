// NereusSDR for iOS: the PTT button test's page (debug copies only, never keys the radio)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if PTT_PROBE
import SwiftUI

/// The PTT button test (plan Task 65 step 1), opened from Setup's PTT
/// buttons page in a debug copy. It joins a Push to Talk channel of its
/// own and logs what each hardware button does. Nothing here keys the
/// radio.
struct PttProbePage: View {
    @ObservedObject var probe: PttProbe
    @ObservedObject private var log: PttProbeLog
    @ObservedObject private var microphone: PttProbeMicrophone
    @ObservedObject private var bluetooth: PttProbeBluetooth

    init(probe: PttProbe) {
        self.probe = probe
        log = probe.log
        microphone = probe.microphone
        bluetooth = probe.bluetooth
    }

    static let warning = "Debug copy only. This test never keys the radio: a test transmission is Push to Talk's alone, and the microphone's sound goes nowhere."
    static let actionButtonSteps = "In the iPhone's Settings, Action Button, choose Shortcut, then NereusSDR, PTT button test. Each press begins or ends a test transmission."
    /// The newest lines shown on the page; the shared log has them all.
    static let shownLines = 200

    var body: some View {
        List {
            Section {
                Text(Self.warning)
                    .font(.footnote)
                    .accessibilityIdentifier("pttProbeWarning")
            }
            channel
            transmission
            bluetoothSection
            Section {
                Text(Self.actionButtonSteps)
                    .font(.footnote)
            } header: {
                Text("Action button")
            }
            logSection
        }
        .navigationTitle("PTT button test")
        .task {
            await probe.prepare()
        }
    }

    private var channel: some View {
        Section {
            LabeledContent("Channel", value: probe.joined ? "Joined" : "Not joined")
            if probe.joined {
                Button("Leave the test channel") {
                    probe.leave()
                }
            } else {
                Button("Join the test channel") {
                    Task { await probe.join() }
                }
                .accessibilityIdentifier("pttProbeJoin")
            }
            Toggle("Headset button begins and ends", isOn: Binding(
                get: { probe.headsetEvents },
                set: { on in Task { await probe.setHeadsetEvents(on) } }))
                .disabled(!probe.joined)
            Toggle("Also log media buttons", isOn: Binding(
                get: { probe.mediaCommands },
                set: { probe.setMediaCommands($0) }))
            if let problem = probe.problem {
                Text(problem)
                    .font(.footnote)
                    .foregroundStyle(.red)
            }
        } header: {
            Text("Test channel")
        } footer: {
            Text("Join with the app in front, then lock the phone or leave the app to try each button.")
        }
    }

    private var transmission: some View {
        Section {
            if let since = log.talkingSince {
                LabeledContent("Test transmission",
                               value: "On since \(since.formatted(date: .omitted, time: .standard))")
                if let source = log.talkingSource {
                    LabeledContent("Begun by", value: source.label)
                }
            } else {
                LabeledContent("Test transmission", value: log.beginPending ? "Asked to begin" : "Off")
            }
            LabeledContent("Microphone", value: microphone.running
                           ? "Capturing, \(microphone.buffers) buffers, peak \(String(format: "%.2f", microphone.peak))"
                           : "Off")
            Button(log.talking || log.beginPending ? "End the test transmission" : "Begin a test transmission") {
                probe.toggle(by: .probeScreen)
            }
            .disabled(!probe.joined)
            .accessibilityIdentifier("pttProbeToggle")
        } header: {
            Text("Test transmission")
        }
    }

    private var bluetoothSection: some View {
        Section {
            LabeledContent("Bluetooth", value: bluetooth.status)
            if let name = bluetooth.connectedName {
                LabeledContent("Connected", value: name)
                LabeledContent("Values listened to", value: "\(bluetooth.listening)")
                Button("Disconnect") {
                    bluetooth.disconnect()
                }
            } else if bluetooth.looking {
                Button("Stop looking") {
                    bluetooth.stopLooking()
                }
                ForEach(bluetooth.devices) { device in
                    Button {
                        bluetooth.connect(device.id)
                    } label: {
                        LabeledContent(device.name, value: "\(device.signal) dBm")
                    }
                }
            } else {
                Button("Look for Bluetooth buttons") {
                    bluetooth.look()
                }
            }
            Picker("A press is", selection: $bluetooth.rule) {
                ForEach(PttProbeBluetoothRule.allCases) { rule in
                    Text(rule.label).tag(rule)
                }
            }
        } header: {
            Text("Bluetooth button")
        } footer: {
            Text("Every change the button reports is logged. Choose Non-zero only for a button that reports both press and release.")
        }
    }

    private var logSection: some View {
        Section {
            ShareLink(item: log.text()) {
                Label("Share the log", systemImage: "square.and.arrow.up")
            }
            Button("Clear the log", role: .destructive) {
                log.clear()
            }
            ForEach(log.events.suffix(Self.shownLines).reversed()) { event in
                Text(event.line)
                    .font(.caption.monospaced())
                    .foregroundStyle(event.kind == .begin || event.kind == .end ? .primary : .secondary)
            }
        } header: {
            Text("Log, newest first")
        }
    }
}
#endif
