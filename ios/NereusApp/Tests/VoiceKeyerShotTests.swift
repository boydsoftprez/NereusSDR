// NereusSDR for iOS: pictures of Tools > Voice Keyer, VOICE beside PTT and its messages over the pan, for comparing with the mockups
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Recording plan Task 18 (A16 for the iPhone's voice keyer): the Voice
/// Keyer page and the pan's VOICE, drawn as the app draws them on a phone
/// against a recordings client fed by hand. The page: its messages, light,
/// one going out, recording, transmit held elsewhere, and an older Core.
/// The pan: VOICE idle, sending with its ring, waiting to repeat, dimmed
/// while transmit is held elsewhere, and its messages open over the pan.
/// With `NEREUS_MAIN_SHOTS` (the page) and `NEREUS_BAND_SHOTS` (the pan)
/// set, each picture is written there.
@Suite("Voice keyer pictures", .serialized)
@MainActor
struct VoiceKeyerShotTests {
    typealias Keyer = VoiceKeyerTests

    static let heldWords = "Transmit is with the Shack Mac. Take it here to send messages."

    // MARK: The page

    @Test("Tools > Voice Keyer draws its messages, light, one going out, recording, held elsewhere and an older Core")
    func pagePictures() async throws {
        let rig = await Keyer.rig()
        Keyer.publish(rig, Keyer.document(), revision: 1)
        rig.model.recordInto = 3
        try await shoot("tools-voice-keyer") { Self.page(rig.model) }
        try await shoot("tools-voice-keyer-light", scheme: .light) { Self.page(rig.model) }

        // Sent from this phone, so it reads as this phone's own.
        #expect(await Keyer.send(rig, 2, at: 0) == .sent)
        Keyer.publish(rig, Keyer.document(sending: Keyer.sending(2, deviceId: "wire-7")), revision: 2)
        #expect(rig.model.progress()?.mine == true)
        try await shoot("tools-voice-keyer-sending") { Self.page(rig.model) }
        Keyer.publish(rig, Keyer.document(), revision: 3)

        await rig.model.record()
        rig.capture.written = 3.4
        rig.capture.level = 0.55
        await rig.model.tick()
        #expect(rig.model.recorder.isRecording)
        try await shoot("tools-voice-keyer-recording") { Self.page(rig.model) }
        rig.model.recorder.cancel()

        rig.model.held = TxRefusalInfo(reason: Self.heldWords, code: "otherDeviceHolds", fix: TxRefusalInfo.takeTransmit)
        let main = Self.mainModel()
        try await shoot("tools-voice-keyer-held") { Self.page(rig.model, take: main.take) }

        let older = await Keyer.rig(version: RecordingsClient.onAirVersion)
        try await shoot("tools-voice-keyer-older-core") { Self.page(older.model) }
        let away = await Keyer.rig(connected: false)
        try await shoot("tools-voice-keyer-not-connected") { Self.page(away.model) }
    }

    /// The page under the Tools tab's bar, as ToolsTab draws it.
    private static func page(_ model: VoiceKeyerModel, take: TransmitTakeModel? = nil) -> some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Voice Keyer", back: "Tools") {
                SetupTagBadge(tag: .core)
            }
            VoiceKeyerPage(model: model, take: take)
        }
    }

    /// A screen model for the TX panel's Take transmit and the PTT.
    private static func mainModel() -> MainScreenModel {
        MainScreenModel(mirror: MirrorStore(send: { _ in }), settings: SettingsProxyClient(send: { _ in }),
                        commands: nil,
                        operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                               retuneClarity: { _ in }))
    }

    /// One page over the tab bar on an upright phone, written with `NEREUS_MAIN_SHOTS` set.
    private func shoot<Content: View>(_ name: String, height: CGFloat = 1200, scheme: ColorScheme = .dark,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = CGSize(width: 402, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(.tools), sideways: false)
        }
        .background(ChromeColours.page)
        .preferredColorScheme(scheme)
        let host = ShotHosting.host(root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        #expect(image.size == size)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try? data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    // MARK: The pan

    enum PanFrame: String, CaseIterable, Sendable, CustomStringConvertible {
        case idle
        case sending
        case waiting
        case held
        case panel
        case panelHeld = "panel-held"
        case olderCore = "older-core"

        var description: String { "pan-voice-\(rawValue)" }
        static let size = CGSize(width: 402, height: 640)
    }

    @Test(arguments: PanFrame.allCases)
    func panPictures(_ frame: PanFrame) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
        ])))
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        mirror.apply(FlagButtonsShotTests.slice(0, hz: 7_236_400, active: true))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))

        let rig = await Keyer.rig(version: frame == .olderCore ? RecordingsClient.onAirVersion
                                      : RecordingsClient.voiceKeyerVersion)
        if frame != .olderCore {
            Keyer.publish(rig, Keyer.document(), revision: 1)
        }
        switch frame {
        case .sending:
            #expect(await Keyer.send(rig, 2, at: 0) == .sent)
            Keyer.publish(rig, Keyer.document(sending: Keyer.sending(2, deviceId: "wire-7")), revision: 2)
        case .waiting:
            #expect(await Keyer.send(rig, 2, at: 0) == .sent)
            Keyer.publish(rig, Keyer.document(sending: Keyer.sending(2, deviceId: "wire-7", waitingMs: 4_200),
                                              repeat: true), revision: 2)
        case .held, .panelHeld:
            rig.model.held = TxRefusalInfo(reason: Self.heldWords, code: "otherDeviceHolds",
                                           fix: TxRefusalInfo.takeTransmit)
        default:
            break
        }

        let band = main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 1 && main.slices.catalog != nil })

        let window = try BandFlagShotTests.window(size: PanFrame.size)
        let panel = frame == .panel || frame == .panelHeld || frame == .olderCore
        let root = ZStack(alignment: .bottomLeading) {
            BandView(model: band)
            PttButton(transmit: main.transmit, linkUp: frame != .olderCore)
                .padding(.leading, PttButton.inset)
                .padding(.bottom, PttButton.bottomInset)
            VoiceButton(model: rig.model) {}
                .padding(.leading, PttButton.inset + PttButton.diameter + VoiceButton.gap)
                .padding(.bottom, PttButton.bottomInset + (PttButton.diameter - VoiceButton.diameter) / 2)
            if panel {
                ChromeColours.scrim
                VoiceKeyerPanel(model: rig.model, take: main.take, close: {}, openPage: {})
            }
        }
        .frame(width: PanFrame.size.width, height: PanFrame.size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: PanFrame.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(PanFrame.size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)

        // What the frame is for is there.
        let progress = rig.model.progress()
        switch frame {
        case .idle, .panel:
            #expect(progress == nil && rig.model.sendReason == nil)
        case .sending:
            #expect(progress?.n == 2 && progress?.waiting == false && progress?.mine == true)
        case .waiting:
            #expect(progress?.waiting == true && (1...5).contains(progress?.waitSeconds ?? 0))
        case .held, .panelHeld:
            #expect(rig.model.sendReason == Self.heldWords && rig.model.offersTake)
        case .olderCore:
            #expect(rig.model.reason == RecordingsClient.updateReason)
        }
        // The VOICE sits beside the PTT, centred on it.
        let ptt = PttButton.frame(bandSize: PanFrame.size, leadingInset: 0)
        let voice = VoiceButton.frame(besidePtt: ptt)
        #expect(voice.minX == ptt.maxX + VoiceButton.gap && voice.midY == ptt.midY)
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: frame.description)
    }
}
