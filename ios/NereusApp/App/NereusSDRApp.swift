// NereusSDR for iOS: the app's entry point
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import SwiftUI
import UIKit

/// Where the app starts: the app itself, or, in a debug build hosting the
/// app's unit tests, an empty scene. The unit tests build every model they
/// test themselves; the host then reads no Keychain, looks for no Core on
/// the network and starts no sound or Live Activity of its own, so nothing
/// it does can land in a test or slow the host's launch on a busy
/// simulator. The UI tests launch the app itself, in its own process.
@main
enum NereusSDRLaunch {
    static func main() {
        #if DEBUG
        if UnitTestHost.isHosting {
            UnitTestHost.main()
            return
        }
        #endif
        NereusSDRApp.main()
    }
}

#if DEBUG
/// The app as a host for its unit tests: one empty window scene, which the
/// tests' pictures put their own windows in.
struct UnitTestHost: App {
    /// XCTest names its configuration file in the process it hosts tests in;
    /// the app it launches for UI tests never has it.
    static var isHosting: Bool {
        ProcessInfo.processInfo.environment["XCTestConfigurationFilePath"] != nil
    }

    var body: some Scene {
        WindowGroup {
            Color.black
        }
    }
}
#endif

/// NereusSDR for iPhone and iPad: a client of the Core, which runs the
/// radio and all of its signal processing.
struct NereusSDRApp: App {
    @StateObject private var model: AppModel
    @StateObject private var flow: ConnectionFlow
    /// The lock-screen card and the Dynamic Island.
    @StateObject private var activity: LiveActivityController
    @Environment(\.scenePhase) private var scenePhase

    init() {
        #if DEBUG
        let arguments = ProcessInfo.processInfo.arguments
        let diversityFixture = UITestDiversityFixture.isEnabled(arguments)
        let model = diversityFixture ? UITestDiversityFixture.makeApp(arguments: arguments) : AppModel.live()
        #else
        let model = AppModel.live()
        #endif
        let kind: DeviceKeyAuthenticator.Kind = UIDevice.current.userInterfaceIdiom == .pad ? .tablet : .phone
        #if DEBUG
        let flow = Self.launchFlow(app: model, kind: kind, arguments: arguments) {
            ConnectionFlow.live(app: model, kind: kind)
        }
        #else
        let flow = ConnectionFlow.live(app: model, kind: kind)
        #endif
        #if DEBUG
        // The welcome flow initializes admission from its ephemeral identity.
        // Restore only this fixture's synthetic admission after constructing that flow.
        if diversityFixture { UITestDiversityFixture.finishLaunch(app: model) }
        // The UI tests look at the band and its tabs without a Core.
        if ProcessInfo.processInfo.arguments.contains(ConnectionFlow.showBandArgument) || diversityFixture {
            flow.show(.band)
        }
        // The UI tests drag on a slice's flag, its panels and the band with no Core.
        if !diversityFixture {
            UITestBand.show(model, arguments: ProcessInfo.processInfo.arguments,
                            environment: ProcessInfo.processInfo.environment)
        }
        // The UI tests hand it the Core's question over whatever screen shows.
        UITestQuestion.listen(model.devices, arguments: ProcessInfo.processInfo.arguments)
        #endif
        let activity = LiveActivityController(host: SystemLiveActivityHost())
        activity.observe(app: model, flow: flow)
        // The card's and the island's buttons reach the app through here.
        ActivityAction.handler = { [weak activity] action in
            await activity?.perform(action)
        }
        // A long session: the sleep timer ends it through the flow, and the
        // screen stays on as Setup chooses (spec section 5.5 items 10 and 11).
        model.longSession.setDisconnect { [weak flow, weak model] expiry in
            guard let flow, let model, let captured = model.sleepSessionOwner,
                  captured.listening == expiry.listeningOwner else { return false }
            let stillAllowed: @MainActor () -> Bool = {
                guard let current = model.sleepSessionOwner else { return false }
                return model.longSession.isCurrent(expiry)
                    && current.listening == captured.listening && current.media == captured.media
                    && current.session === captured.session
            }
            guard model.connection == .connected, stillAllowed(),
                  await model.main.transmit.stopIdleVoxForSleep(owner: captured.media,
                      authority: expiry.timer.permit, stillAllowed: stillAllowed) else { return false }
            defer {
                Task {
                    await model.main.transmit.releaseIdleSleepRetirement(owner: captured.media,
                                                                          authority: expiry.timer.permit)
                }
            }
            guard stillAllowed() else { return false }
            return await flow.disconnect(ifCurrent: captured.session, authority: expiry.timer.permit,
                                         stillAllowed: stillAllowed)
        }
        model.longSession.start()
        _model = StateObject(wrappedValue: model)
        _flow = StateObject(wrappedValue: flow)
        _activity = StateObject(wrappedValue: activity)
    }

    #if DEBUG
    /// Selects UI-test fixtures before the live flow can access this phone's key.
    static func launchFlow(app: AppModel, kind: DeviceKeyAuthenticator.Kind, arguments: [String],
                           live: () -> ConnectionFlow) -> ConnectionFlow {
        if UITestDiversityFixture.isEnabled(arguments) {
            return freshWelcome(app: app, kind: kind)
        } else if arguments.contains("-NereusFreshWelcome") || arguments.contains(ConnectionFlow.uiTestWelcomeArgument) {
            return freshWelcome(app: app, kind: kind)
        } else if arguments.contains(ConnectionFlow.shotCoresArgument) {
            return shotCores(app: app, kind: kind)
        } else if arguments.contains(ConnectionFlow.showBandArgument) {
            return freshWelcome(app: app, kind: kind)
        }
        return live()
    }

    /// A fresh Welcome for UI tests regardless of Cores kept on the simulator.
    private static func freshWelcome(app: AppModel, kind: DeviceKeyAuthenticator.Kind) -> ConnectionFlow {
        ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: kind,
            transportFactory: WebSocketLinkTransport.factory, clock: SystemLinkClock(),
            microphone: SystemMicrophoneAccess(), network: nil,
            appMajors: LinkVersionPolicy.supportedMajors, now: Date.init, browser: nil))
    }

    /// Your Cores with three made-up Cores, kept only in memory (documentation
    /// addresses, made-up keys): one named, one with no name, and one this
    /// phone last found too old to rename.
    private static func shotCores(app: AppModel, kind: DeviceKeyAuthenticator.Kind) -> ConnectionFlow {
        let stations = PairedStationStore(item: InMemorySecretItem())
        let cores = [
            PairedStation(identityKey: Data(repeating: 1, count: 91), label: "KG4VCF/shack",
                          endpoints: [StationEndpoint(host: "2001:db8:5a1:e8f0:dea6:32ff:fe12:3456")]),
            PairedStation(identityKey: Data(repeating: 2, count: 91), label: "",
                          endpoints: [StationEndpoint(host: "192.0.2.40")]),
            PairedStation(identityKey: Data(repeating: 3, count: 91), label: "KG4VCF/field",
                          endpoints: [StationEndpoint(host: "field.example", port: 50055)]),
        ]
        for core in cores {
            try? stations.save(core)
        }
        app.phoneSettings.coresWithoutRename = [Base64URL.encode(cores[2].identityKey)]
        return ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()), stations: stations, kind: kind,
            transportFactory: WebSocketLinkTransport.factory, clock: SystemLinkClock(),
            microphone: SystemMicrophoneAccess(), network: nil, appMajors: LinkVersionPolicy.supportedMajors,
            now: Date.init, browser: nil))
    }
    #endif

    var body: some Scene {
        WindowGroup {
            #if DEBUG
            if ProcessInfo.processInfo.arguments.contains("-NereusAboutOpenFailure") {
                RootView()
                    .environmentObject(model)
                    .environmentObject(flow)
                    .environment(\.openURL, OpenURLAction { _ in .discarded })
                    .preferredColorScheme(.dark)
            } else {
                RootView()
                    .environmentObject(model)
                    .environmentObject(flow)
                    .overlay {
                        if UITestDiversityFixture.schemeProbe(ProcessInfo.processInfo.arguments) {
                            DiversityFixtureSchemeProbe()
                        } else if nativeTxLightValidation {
                            NativeTxValidationSchemeProbe()
                        }
                    }
                    .preferredColorScheme(UITestDiversityFixture.isEnabled(ProcessInfo.processInfo.arguments)
                        ? UITestDiversityFixture.scheme(ProcessInfo.processInfo.arguments)
                        : nativeTxLightValidation ? .light : .dark)
            }
            #else
            RootView()
                .environmentObject(model)
                .environmentObject(flow)
                .preferredColorScheme(.dark)
            #endif
        }
        .onChange(of: scenePhase, initial: true) { _, phase in
            AppSceneDiagnostics.changed(phase)
            activity.sceneChanged(active: phase == .active)
            // Locked or in another app is the background; a passing
            // Control Center or app switcher is not.
            model.longSession.sceneChanged(inForeground: phase != .background)
        }
    }

    #if DEBUG
    /// Only the existing offline UI fixture can opt into light validation.
    private var nativeTxLightValidation: Bool {
        let arguments = ProcessInfo.processInfo.arguments
        return arguments.contains("-NereusShowBand") && arguments.contains("-NereusTxPanelLightValidation")
    }
    #endif
}

#if DEBUG
/// Reads the actual RootView environment without changing its geometry.
private struct NativeTxValidationSchemeProbe: View {
    @Environment(\.colorScheme) private var scheme

    var body: some View {
        Color.clear.frame(width: 1, height: 1)
            .allowsHitTesting(false)
            .accessibilityElement(children: .ignore)
            .accessibilityIdentifier("nativeTxValidationScheme")
            .accessibilityLabel(scheme == .light ? "Light" : "Dark")
    }
}
#endif

/// Typed scene boundary shared by the live SwiftUI hook and deterministic tests.
enum AppSceneDiagnostics {
    static func changed(_ phase: ScenePhase, diagnostics: LinkDiagnostics = .shared) {
        switch phase {
        case .active: diagnostics.sceneChanged(to: .foreground)
        case .background: diagnostics.sceneChanged(to: .background)
        case .inactive: diagnostics.sceneChanged(to: .inactive)
        @unknown default: diagnostics.sceneChanged(to: .inactive)
        }
    }
}
