// NereusSDR for iOS: the microphone while transmitting, against a fake Core: the key, the keepalive on the media channel, VOX, interruptions and the band's sound
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Combine
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing
import UIKit

/// R-IOS-20, R-IOS-13 (Task 55): the real app keying a fake Core whose
/// media carries the microphone line, which reaches no radio and starts no
/// microphone (a stand-in counts starts and stops). The key waits for the
/// microphone; the keepalive goes on the media connection's "tx" channel,
/// not the session; the band is silent on the speaker while keyed; a call
/// or Siri unkeys and stops the microphone, and its end resumes the band
/// but not transmit; VOX starts the microphone before it arms.
@Suite("The microphone while transmitting", .serialized)
@MainActor
struct MicrophoneTransmitTests {
    private let center = NotificationCenter()
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()

    @MainActor
    struct Rig {
        let model: AppModel
        let station: FakeStation
        let microphone: FakeMicrophone
        let output: FakePlaybackOutput
        let audio: AudioSessionController

        var transmit: TransmitModel { model.main.transmit }

        var peer: FakeMediaPeer? { station.mediaPeers.last }
    }

    private func connected(microphone: FakeMicrophone = FakeMicrophone(),
                           session: FakeAudioSession = FakeAudioSession(),
                           replies: HeldCommandReplies? = nil) async throws -> Rig {
        let suite = "MicrophoneTransmitTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let station = try FakeStation(additions: [.remoteTx, .wideband])
        let output = FakePlaybackOutput()
        let audio = AudioSessionController(session: session, output: output, notificationCenter: center)
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             mediaPeerFactory: station.mediaPeerFactory, audio: audio,
                             microphone: { _ in microphone },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: { endpoint, trust in
                                let transport = station.transportFactory(endpoint, trust)
                                return replies.map { ReplyHoldingTransport(inner: transport, replies: $0) } ?? transport
                            })
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle(seconds: 30) {
            model.main.transmit.permitted && model.connection == .connected && model.main.transmit.microphoneLine
        })
        await audio.settle()
        return Rig(model: model, station: station, microphone: microphone, output: output, audio: audio)
    }

    @Test("the start asks for the microphone line on the Core's derivation of its SSRC")
    func theStartAsksForTheLine() async throws {
        let rig = try await connected()
        let start = rig.station.messages.compactMap { message -> [String: LinkJSON]? in
            if case .mediaControl(let control) = message, control.payload["op"] == .string("start") {
                return control.payload
            }
            return nil
        }.last
        #expect(start?["remoteTxVersion"] == .number(1))
        let id = try #require(await rig.model.media.connectionId)
        #expect(rig.peer?.requestedMicrophoneSsrc == MediaControlClient.microphoneSsrc(forConnection: id))
        await rig.model.disconnect()
    }

    @Test("a tap starts the microphone, then keys; the keepalive rides the tx channel; the band is silent on the speaker")
    func keyingWithTheMicrophone() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        #expect(rig.audio.route == .speaker)
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        #expect(rig.microphone.starts == 1)
        #expect(rig.microphone.isRunning)
        #expect(rig.station.keyed)
        // The keepalive goes on the media connection's "tx" channel, not the session.
        let peer = try #require(rig.peer)
        #expect(await settle(seconds: 30) { peer.txMessages.count >= 2 })
        #expect(TransmitScreenTests.keepalives(rig.station) == 0)
        #expect(peer.txMessages.allSatisfy { $0.count == 13 && $0.first == 1 })
        // Keyed on the speaker the band is silent.
        #expect(await settle(seconds: 30) { rig.output.isMuted })

        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!rig.microphone.isRunning)
        #expect(rig.microphone.stops >= 1)
        #expect(await settle(seconds: 30) { !rig.output.isMuted })
        await rig.model.disconnect()
    }

    @Test("the Core closing the microphone line's track says the line is gone, and VOX greys with the Core's words")
    func theMicrophoneTrackClosing() async throws {
        let rig = try await connected()
        let peer = try #require(rig.peer)
        #expect(TxPanel.voxReason(rig.transmit) == nil)
        peer.closeMicrophoneLine()
        #expect(await settle(seconds: 30) { !rig.transmit.microphoneLine })
        #expect(TxPanel.voxReason(rig.transmit) == TransmitModel.voxNeedsMicrophone)
        #expect(rig.model.connection == .connected)
        await rig.model.disconnect()
    }

    /// fix-tx concern 1 (M6): the Core closing the microphone line's track
    /// while the link and the media connection stay up ends a key held on
    /// it through the PTT's release, and the band says why in the Core's
    /// words for a key without its microphone line (TxRefusal.cpp:147-153).
    @Test("the Core closing the microphone line's track while keyed unkeys through the PTT and says why")
    func theMicrophoneTrackClosingWhileKeyed() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        let peer = try #require(rig.peer)
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        #expect(rig.station.keyed)
        let keysBefore = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count

        peer.closeMicrophoneLine()
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!rig.station.keyed)
        #expect(rig.station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.unkey" })
        #expect(!transmit.ptt.keepaliveRunning)
        #expect(transmit.ptt.stop?.text == TransmitModel.voxNeedsMicrophone)
        #expect(await settle(seconds: 30) { !rig.microphone.isRunning })
        #expect(rig.model.connection == .connected)
        let keysAfter = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count
        #expect(keysAfter == keysBefore, "nothing keys again by itself")
        await rig.model.disconnect()
    }

    @Test("a microphone that does not start keys nothing and says why")
    func aMicrophoneThatFailsKeysNothing() async throws {
        let microphone = FakeMicrophone()
        microphone.result = .failed(reason: MicCapture.notAllowedText)
        let rig = try await connected(microphone: microphone)
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 30) {
            if case .refused(let refusal) = rig.transmit.ptt.state {
                return refusal.reason == MicCapture.notAllowedText
            }
            return false
        })
        #expect(!rig.station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.key" })
        #expect(!rig.station.keyed)
        await rig.model.disconnect()
    }

    @Test("an interruption unkeys at once and stops the microphone; its end resumes the band, not transmit")
    func anInterruptionUnkeys() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        let keysBefore = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count

        center.post(name: AVAudioSession.interruptionNotification, object: nil,
                    userInfo: [AVAudioSessionInterruptionTypeKey: AVAudioSession.InterruptionType.began.rawValue])
        #expect(!rig.microphone.isRunning, "the microphone stops at once")
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!rig.station.keyed)
        #expect(rig.station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.unkey" })
        #expect(rig.audio.state == .interrupted)

        center.post(name: AVAudioSession.interruptionNotification, object: nil,
                    userInfo: [AVAudioSessionInterruptionTypeKey: AVAudioSession.InterruptionType.ended.rawValue,
                               AVAudioSessionInterruptionOptionKey: AVAudioSession.InterruptionOptions.shouldResume.rawValue])
        await rig.audio.settle()
        #expect(rig.audio.state == .playing)
        #expect(transmit.ptt.state == .idle)
        #expect(!rig.station.keyed)
        #expect(!rig.microphone.isRunning)
        let keysAfter = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count
        #expect(keysAfter == keysBefore, "nothing keys again by itself")
        await rig.model.disconnect()
    }

    @Test("a media services reset unkeys at once and stops the microphone; nothing keys again by itself")
    func aMediaServicesResetUnkeys() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        let keysBefore = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count

        center.post(name: AVAudioSession.mediaServicesWereResetNotification, object: nil)
        #expect(!rig.microphone.isRunning, "the microphone stops at once")
        #expect(rig.microphone.events.suffix(2) == ["stop", "reset"], "the microphone stops, then is built again")
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!rig.station.keyed)
        #expect(rig.station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.unkey" })
        await rig.audio.settle()
        #expect(rig.output.resets == 1)
        #expect(rig.audio.state == .playing)
        #expect(transmit.ptt.state == .idle)
        let keysAfter = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count
        #expect(keysAfter == keysBefore, "nothing keys again by itself")
        await rig.model.disconnect()
    }

    @Test("a microphone that stops by itself while keyed unkeys and says why; nothing keys again")
    func aLostMicrophoneUnkeys() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        transmit.tapPtt()
        #expect(await settle(seconds: 30) { transmit.ptt.state.isKeyed })
        let keysBefore = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count

        rig.microphone.lose(MicCapture.lostText)
        #expect(await settle(seconds: 30) { transmit.ptt.state == .idle })
        #expect(!rig.station.keyed)
        #expect(rig.station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.unkey" })
        #expect(!transmit.ptt.keepaliveRunning)
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: transmit.heldNote)
            == .stop(TransmitStopNotice(reason: PttController.microphoneLostReason, text: MicCapture.lostText,
                                        serial: -1)))
        #expect(PttButton.look(transmit.ptt) == .ready)
        let keysAfter = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.key" }.count
        #expect(keysAfter == keysBefore, "nothing keys again by itself")
        await rig.model.disconnect()
    }

    /// Fix wave (fix-tx2 item 3): a loss reaches the main queue after it
    /// happened. One from the microphone run that carried an earlier key,
    /// heard after a new key started the microphone again, belongs to that
    /// earlier run: the new key stays on and nothing is sent. A loss of
    /// the new run still ends it.
    @Test("a loss of an earlier key's microphone heard after a new key started leaves the new key on")
    func aLateLossLeavesANewKeyOn() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        transmit.tapPtt()
        await when(transmit) { transmit.ptt.state.isKeyed }
        #expect(transmit.microphoneLossesForTesting == 0)
        rig.microphone.lose(MicCapture.lostText)
        await when(transmit) { transmit.ptt.state == .idle }
        await rig.microphone.when { !$0.isRunning }
        let earlier = transmit.microphoneLossesForTesting
        #expect(earlier == 1)
        transmit.dismissNotice()
        await when(transmit) { transmit.ptt.stop == nil }

        transmit.tapPtt()
        await when(transmit) { transmit.ptt.state.isKeyed }
        await rig.microphone.when { $0.isRunning }
        await TransmitScreenTests.barrier(rig.model, rig.station)
        #expect(rig.station.keyed)
        let unkeys = rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.unkey" }.count
        // The earlier run's loss, heard late.
        transmit.microphoneLost(MicCapture.lostText, loss: earlier)
        #expect(transmit.ptt.state.isKeyed)
        #expect(rig.microphone.isRunning)
        await TransmitScreenTests.barrier(rig.model, rig.station)
        #expect(transmit.ptt.state.isKeyed && rig.station.keyed)
        #expect(rig.station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.unkey" }.count
                == unkeys)
        #expect(transmit.ptt.stop == nil)

        // This run's own loss ends the key.
        rig.microphone.lose(MicCapture.lostText)
        await when(transmit) { transmit.ptt.state == .idle }
        await TransmitScreenTests.barrier(rig.model, rig.station)
        #expect(!rig.station.keyed)
        await rig.model.disconnect()
    }

    @Test("VOX starts the microphone, then arms; the Core's refusal stops it again")
    func voxStartsTheMicrophone() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        transmit.toggleVox()
        let write = try #require(await answerVoxWrite(rig.station, accepted: true))
        #expect(write == .bool(true))
        #expect(rig.microphone.starts == 1)
        #expect(await settle(seconds: 30) { transmit.ptt.voxArmed && transmit.ptt.keepaliveRunning })
        #expect(rig.microphone.isRunning)

        transmit.toggleVox()
        #expect(try #require(await answerVoxWrite(rig.station, accepted: true, skip: 1)) == .bool(false))
        #expect(await settle(seconds: 30) { !transmit.ptt.voxArmed })
        #expect(!rig.microphone.isRunning)

        // Refused: the microphone does not stay on.
        transmit.toggleVox()
        _ = await answerVoxWrite(rig.station, accepted: false, skip: 2)
        #expect(await settle(seconds: 30) { !rig.microphone.isRunning && rig.microphone.starts == 2 })
        #expect(!transmit.ptt.voxArmed)
        await rig.model.disconnect()
    }

    // Q10: each step waits on the change it needs (the PTT's published
    // state, the microphone's start or stop, the held unkey), never on the
    // clock; the time limit only ends a test that never gets there.
    @Test("accepted off restarts the actual microphone for already armed VOX", .timeLimit(.minutes(1)))
    func acceptedOffRestartsVoxMicrophone() async throws {
        let rig = try await connected()
        rig.transmit.toggleVox()
        _ = await answerVoxWrite(rig.station, accepted: true)
        await when(rig.transmit) { rig.transmit.ptt.voxArmed }
        await rig.microphone.when { $0.isRunning }
        #expect(rig.transmit.ptt.voxArmed && rig.microphone.isRunning)
        rig.transmit.tapPtt()
        await when(rig.transmit) { rig.transmit.ptt.state.isKeyed }
        let hold = Gate()
        rig.model.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if TransmitScreenTests.invoke(message)?.verb == "tx.unkey" { await hold.wait() }
        }
        rig.transmit.tapPtt()
        await hold.whenWaiting()
        await rig.microphone.when { !$0.isRunning }
        #expect(hold.waiting && !rig.microphone.isRunning)
        #expect(!rig.transmit.ptt.microphoneWanted)
        let starts = rig.microphone.starts
        rig.model.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        hold.open()
        await when(rig.transmit) { rig.transmit.ptt.state == .idle && rig.transmit.ptt.microphoneWanted }
        await rig.microphone.when { $0.starts == starts + 1 && $0.isRunning }
        #expect(rig.microphone.isRunning)
        #expect(rig.microphone.starts == starts + 1)
        await rig.model.disconnect()
    }

    @Test("late accepted key compensation stops and then restarts the real VOX microphone")
    func acceptedCompensationRestartsMicrophone() async throws {
        let replies = HeldCommandReplies()
        replies.hold("tx.key")
        let rig = try await connected(replies: replies)
        rig.transmit.toggleVox()
        _ = await answerVoxWrite(rig.station, accepted: true)
        #expect(await settle(seconds: 2) { rig.transmit.ptt.voxArmed })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { replies.count("tx.key") == 3 })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { rig.transmit.ptt.state == .idle && rig.microphone.isRunning })
        let starts = rig.microphone.starts
        replies.hold("tx.unkey")
        await replies.release("tx.key")
        #expect(await settle(seconds: 2) { replies.count("tx.unkey") == 3 && !rig.microphone.isRunning })
        #expect(!rig.transmit.ptt.microphoneWanted)
        await replies.release("tx.unkey")
        #expect(await settle(seconds: 2) { rig.transmit.ptt.microphoneWanted && rig.microphone.isRunning })
        #expect(rig.microphone.starts == starts + 1)
        await rig.model.disconnect()
    }

    @Test("refused or unanswered off never restarts VOX microphone", arguments: [true, false])
    func unresolvedOffKeepsActualMicrophoneStopped(refused: Bool) async throws {
        let replies = HeldCommandReplies()
        if !refused { replies.hold("tx.unkey") }
        let rig = try await connected(replies: replies)
        rig.transmit.toggleVox()
        _ = await answerVoxWrite(rig.station, accepted: true)
        #expect(await settle(seconds: 2) { rig.transmit.ptt.voxArmed })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { rig.transmit.ptt.state.isKeyed })
        if refused { rig.station.refuseNext("tx.unkey", reason: "Reconnect before transmitting.") }
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { !rig.transmit.ptt.microphoneWanted && !rig.microphone.isRunning })
        if refused {
            #expect(await settle(seconds: 2) {
                if case .refused = rig.transmit.ptt.state { return true }
                return false
            })
        } else {
            #expect(await settle(seconds: 2) { replies.count("tx.unkey") == 3 })
        }
        let starts = rig.microphone.starts
        rig.transmit.tapPtt()
        // A fresh command traverses the actual queue after the release.
        let barrier = await rig.model.commands.start("test.barrier", arguments: [], copies: 1, timeout: .seconds(1))
        await barrier.sent()
        #expect(!rig.microphone.isRunning)
        #expect(rig.microphone.starts == starts)
        await rig.model.disconnect()
    }

    @Test("an asynchronous VOX restart loses to a stop or replacement owner", arguments: [true, false])
    func pendingRestartCannotOutliveDemand(replace: Bool) async throws {
        let replies = HeldCommandReplies()
        replies.hold("tx.unkey")
        let rig = try await connected(replies: replies)
        rig.transmit.toggleVox()
        _ = await answerVoxWrite(rig.station, accepted: true)
        #expect(await settle(seconds: 2) { rig.transmit.ptt.voxArmed })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { rig.transmit.ptt.state.isKeyed })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { replies.count("tx.unkey") == 3 && !rig.microphone.isRunning })
        let hold = Gate()
        rig.microphone.hold = { await hold.wait() }
        let starts = rig.microphone.starts
        await replies.release("tx.unkey")
        #expect(await settle(seconds: 2) { hold.waiting })
        if replace {
            await rig.model.disconnect()
            let newer = try FakeStation(additions: [.remoteTx, .wideband])
            await rig.model.connect(to: newer.endpoint, trust: newer.trust, authenticator: newer.authenticator,
                                    transportFactory: newer.transportFactory)
            #expect(await settle(seconds: 2) { rig.model.connection == .connected })
            await newer.deliver(.objectCreate(.init(key: "txState", className: "TransmitState",
                properties: TransmitScreenTests.holder("", short: "", keyed: false))))
            await TransmitScreenTests.fillTransmit(newer)
            #expect(await settle(seconds: 2) { rig.transmit.permitted && rig.transmit.microphoneLine })
            rig.microphone.hold = nil
            rig.transmit.toggleVox()
            _ = await answerVoxWrite(newer, accepted: true)
            #expect(await settle(seconds: 2) { rig.transmit.ptt.voxArmed && rig.microphone.isRunning })
            #expect(rig.microphone.starts == starts + 1, "NEW start cannot wait for the held OLD restart")
        } else {
            await rig.transmit.controller.setTune(true)
            #expect(await settle(seconds: 2) { rig.transmit.ptt.state.isKeyed })
            replies.hold("tx.tune")
            await rig.transmit.controller.stopAll()
            #expect(await settle(seconds: 2) { !rig.transmit.ptt.microphoneWanted })
        }
        hold.open()
        #expect(await settle(seconds: 2) { rig.microphone.starts == starts + (replace ? 2 : 1) })
        #expect(await settle(seconds: 2) { rig.microphone.isRunning == replace })
        #expect(rig.transmit.ptt.voxArmed)
        await rig.model.disconnect()
    }

    @Test("failed VOX restart reports its reason and disarms local heartbeat demand")
    func failedRestartIsVisible() async throws {
        let replies = HeldCommandReplies()
        replies.hold("tx.unkey")
        let rig = try await connected(replies: replies)
        rig.transmit.toggleVox()
        _ = await answerVoxWrite(rig.station, accepted: true)
        #expect(await settle(seconds: 2) { rig.transmit.ptt.voxArmed })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { rig.transmit.ptt.state.isKeyed })
        rig.transmit.tapPtt()
        #expect(await settle(seconds: 2) { replies.count("tx.unkey") == 3 && !rig.microphone.isRunning })
        rig.microphone.result = .failed(reason: "Microphone unavailable after release.")
        await replies.release("tx.unkey")
        #expect(await settle(seconds: 2) { rig.transmit.note == "Microphone unavailable after release." })
        #expect(await settle(seconds: 2) { !rig.transmit.ptt.voxArmed && !rig.transmit.ptt.keepaliveRunning })
        #expect(!rig.microphone.isRunning)
        await rig.model.disconnect()
    }

    @Test("with no microphone line VOX stays off with the Core's words")
    func noLineNoVox() async throws {
        let rig = try await connected()
        rig.transmit.microphoneLineChanged(false)
        rig.transmit.toggleVox()
        #expect(rig.transmit.note == TransmitModel.voxNeedsMicrophone)
        #expect(rig.microphone.starts == 0)
        await rig.model.disconnect()
    }

    // MARK: Stopping everything here (D24, the Live Activity's UNKEY)

    /// VOX armed and keyed by the Core on this phone's voice.
    private func voxKeyed(_ rig: Rig) async throws {
        let transmit = rig.transmit
        transmit.toggleVox()
        _ = try #require(await answerVoxWrite(rig.station, accepted: true))
        #expect(await settle(seconds: 30) { transmit.ptt.voxArmed && rig.microphone.isRunning })
        await rig.station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder(TransmitScreenTests.thisDeviceId, short: "iPhone", keyed: true)))
        #expect(await settle(seconds: 30) { transmit.transmittingHere })
        #expect(transmit.ptt.state == .idle, "a VOX key is the Core's, not the PTT's")
    }

    @Test("locking the phone during a VOX key disarms VOX and stops the microphone; the card speaks only after")
    func lockDuringVoxKey() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        let host = FakeLiveActivityHost()
        let activity = LiveActivityController(host: host)
        activity.observe(app: rig.model, flow: nil)
        activity.sceneChanged(active: true)
        try await voxKeyed(rig)
        activity.sceneChanged(active: false)

        platform.lock()
        #expect(await settle(seconds: 30) { !rig.microphone.isRunning }, "the microphone stops at once")
        #expect(await settle(seconds: 30) { !transmit.ptt.voxArmed })
        // The Core lets go, but the stop hasn't had its answer yet: the card says nothing of it.
        await rig.station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder("", short: "", keyed: false)))
        #expect(await settle(seconds: 30) { !transmit.transmittingHere })
        #expect(transmit.localStop == nil)
        #expect(host.current?.shown.message == "")
        // VOX off reaches the Core, as a write of voxEnabled false.
        #expect(try #require(await answerVoxWrite(rig.station, accepted: true, skip: 1)) == .bool(false))
        #expect(await settle(seconds: 30) { transmit.localStop?.cause == .lock })
        #expect(transmit.localStop?.wasTransmitting == true)
        #expect(await settle(seconds: 30) {
            host.current?.shown.message.hasPrefix("Unkeyed when you locked the phone. TX ran ") == true
        })
        #expect(!transmit.ptt.voxArmed)
        #expect(!rig.microphone.isRunning)
        await rig.model.disconnect()
    }

    @Test("the card's UNKEY during a VOX key disarms VOX and stops the microphone")
    func unkeyDuringVoxKey() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        let host = FakeLiveActivityHost()
        let activity = LiveActivityController(host: host)
        activity.observe(app: rig.model, flow: nil)
        activity.sceneChanged(active: true)
        try await voxKeyed(rig)
        activity.sceneChanged(active: false)

        let unkey = Task { await activity.perform(.unkey) }
        #expect(await settle(seconds: 30) { !rig.microphone.isRunning && !transmit.ptt.voxArmed })
        #expect(try #require(await answerVoxWrite(rig.station, accepted: true, skip: 1)) == .bool(false))
        await unkey.value
        #expect(transmit.localStop?.cause == .unkeyButton)
        await rig.station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder("", short: "", keyed: false)))
        #expect(await settle(seconds: 30) { host.current?.shown.message.hasPrefix("Unkeyed. TX ran ") == true })
        // Nothing keyed by this: no tx.key went.
        #expect(!rig.station.messages.compactMap(TransmitScreenTests.invoke).contains { $0.verb == "tx.key" })
        await rig.model.disconnect()
    }

    @Test("VOX tapped, then the phone locked before the arm completes: no microphone, VOX disarmed, VOX off sent")
    func lockWhileVoxArming() async throws {
        let microphone = FakeMicrophone()
        let gate = Gate()
        microphone.hold = { await gate.wait() }
        let rig = try await connected(microphone: microphone)
        let transmit = rig.transmit
        transmit.toggleVox()
        #expect(await settle(seconds: 30) { gate.waiting })
        platform.lock()
        // The arm was on its way: the stop sends VOX off.
        #expect(try #require(await answerVoxWrite(rig.station, accepted: true)) == .bool(false))
        #expect(await settle(seconds: 30) { transmit.localStop?.cause == .lock })
        // The microphone's start completes only now; nothing arms after the stop.
        gate.open()
        // The late start has been answered and the stop has closed it again.
        #expect(await settle(seconds: 30) { rig.microphone.starts == 1 && !rig.microphone.isRunning })
        #expect(!rig.microphone.isRunning)
        #expect(!transmit.ptt.voxArmed)
        let writes = rig.station.messages.compactMap { message -> LinkMessage.PropertyValue? in
            if case .propertyWrite(let write) = message, write.key == "transmit",
               write.properties.first?.name == "voxEnabled" {
                return write.properties.first?.value
            }
            return nil
        }
        #expect(writes == [.bool(false)], "only VOX off went to the Core")
        await rig.model.disconnect()
    }

    @Test("idle sleep revokes a VOX arm held before transport handoff and leaves its microphone stopped")
    func sleepWhileVoxArmHeldBeforeSend() async throws {
        let rig = try await connected()
        let owner = try #require(rig.model.sleepSessionOwner)
        let gate = Gate()
        rig.model.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .propertyWrite(let write) = message, write.key == "transmit",
               write.properties.first?.name == "voxEnabled",
               write.properties.first?.value == .bool(true) {
                await gate.wait()
            }
        }
        rig.transmit.toggleVox()
        #expect(await settle(seconds: 5) { gate.waiting })
        #expect(rig.microphone.isRunning)

        let permit = CommandSendPermit()
        let stop = Task {
            await rig.transmit.stopIdleVoxForSleep(owner: owner.media, authority: permit,
                                                    stillAllowed: { rig.model.sleepSessionOwner?.session === owner.session })
        }
        #expect(try #require(await answerVoxWrite(rig.station, accepted: true)) == .bool(false))
        #expect(await stop.value)
        #expect(!rig.microphone.isRunning)
        gate.open()
        rig.model.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await rig.model.disconnect()
        await rig.transmit.releaseIdleSleepRetirement(owner: owner.media, authority: permit)
        let values = rig.station.messages.compactMap { message -> LinkMessage.PropertyValue? in
            if case .propertyWrite(let write) = message, write.key == "transmit",
               write.properties.first?.name == "voxEnabled" {
                return write.properties.first?.value
            }
            return nil
        }
        #expect(values == [.bool(false)])
        #expect(!rig.microphone.isRunning)
    }

    @Test("a local stop parked before controller admission cannot stop or disarm a replacement owner")
    func delayedLocalStopCannotTouchReplacementOwner() async throws {
        let rig = await TransmitOwnershipRig.make()
        await rig.keyAndArm()
        let gate = Gate()
        rig.transmit.beforeLocalStopAdmissionForTesting = { await gate.wait() }
        let stopping = Task { await rig.transmit.stopEverythingHere(.interruption) }
        await gate.whenWaiting()

        await rig.replaceOwner()
        await rig.keyAndArm()
        #expect(await rig.transmit.controller.snapshot.state.isKeyed)
        #expect(await rig.transmit.controller.snapshot.voxArmed)
        let before = rig.transport.messages(owner: 2).count
        gate.open()
        await stopping.value

        #expect(await rig.transmit.controller.snapshot.state.isKeyed,
                "OLD's delayed stop cannot release NEW's key")
        #expect(await rig.transmit.controller.snapshot.voxArmed,
                "OLD's delayed stop cannot disarm NEW's VOX")
        #expect(rig.transport.messages(owner: 2).count == before,
                "no OLD unkey or VOX off reaches NEW's final transport handoff")
        #expect(rig.transmit.localStop == nil, "OLD cannot publish a completed stop on NEW")
        await rig.transmit.sessionChanged(.stopped, owner: 2)
    }

    @Test("an old microphone close parked at admission cannot release the replacement PTT", arguments: [false, true])
    func delayedMicrophoneCloseCannotTouchReplacementOwner(insideController: Bool) async throws {
        let rig = await TransmitOwnershipRig.make()
        await rig.transmit.controller.tap()
        await rig.transmit.controller.settle()
        let gate = Gate()
        if insideController {
            await rig.transmit.controller.setMicrophoneCloseAdmissionForTesting { await gate.wait() }
        } else {
            rig.transmit.beforeMicrophoneCloseAdmissionForTesting = { await gate.wait() }
        }
        rig.transmit.microphoneTrackClosed()
        let closing = try #require(rig.transmit.lastMicrophoneCloseOperationForTesting)
        await gate.whenWaiting()

        await rig.replaceOwner()
        await rig.transmit.controller.tap()
        await rig.transmit.controller.settle()
        let before = rig.transport.messages(owner: 2).count
        gate.open()
        await closing.value
        await rig.transmit.controller.settle()

        #expect(await rig.transmit.controller.snapshot.state.isKeyed,
                "OLD's delayed microphone close cannot release NEW's PTT")
        #expect(await rig.transmit.controller.snapshot.stop == nil,
                "OLD cannot publish a microphone stop on NEW")
        #expect(await rig.transmit.controller.snapshot.microphoneWanted)
        #expect(rig.transport.messages(owner: 2).count == before,
                "no OLD unkey reaches NEW's transport")
        // A genuine current close still releases manual PTT and says exactly why.
        rig.transmit.beforeMicrophoneCloseAdmissionForTesting = nil
        await rig.transmit.controller.setMicrophoneCloseAdmissionForTesting(nil)
        rig.transmit.microphoneTrackClosed()
        await rig.transmit.lastMicrophoneCloseOperationForTesting?.value
        await rig.transmit.controller.settle()
        #expect(await rig.transmit.controller.snapshot.state == .idle)
        #expect(await rig.transmit.controller.snapshot.stop?.text == TransmitModel.voxNeedsMicrophone)
        #expect(await rig.transmit.controller.snapshot.stop?.reason == PttController.microphoneLostReason)
        #expect(await !rig.transmit.controller.snapshot.microphoneWanted)
        await rig.transmit.sessionChanged(.stopped, owner: 2)
    }

    @Test("a source-buffered old microphone close cannot borrow the replacement owner")
    func bufferedMicrophoneCloseCannotBorrowReplacementOwner() async throws {
        let rig = try await connected()
        let oldOwner = try #require(rig.model.mediaOwnerForTesting)
        let oldPeer = try #require(rig.peer)
        let delivery = Gate()
        let consumed = Gate()
        rig.model.beforeMediaEventForTesting = { event in
            if event == .microphoneLine(false) { await delivery.wait() }
        }
        rig.model.afterMediaEventForTesting = { event in
            if event == .microphoneTrackClosed { consumed.open() }
        }
        oldPeer.closeMicrophoneLine()
        await delivery.whenWaiting()
        // OLD's false-line event is held at delivery and its close is buffered
        // on the real MediaControlClient stream while NEW activates.
        await rig.model.disconnect()
        let station = try FakeStation(additions: [.remoteTx, .wideband])
        await rig.model.connect(to: station.endpoint, trust: station.trust,
                                authenticator: station.authenticator,
                                transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let newOwner = try #require(rig.model.mediaOwnerForTesting)
        #expect(newOwner > oldOwner)
        await rig.transmit.sessionChanged(.ready, owner: newOwner)
        rig.transmit.microphoneLineChanged(true)
        await rig.transmit.controller.tap()
        await rig.transmit.controller.settle()
        #expect(await rig.transmit.controller.snapshot.state.isKeyed)
        let before = station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.unkey" }.count

        // Keep subsequent media news queued while observing the exact OLD close.
        let successorDelivery = Gate()
        rig.model.beforeMediaEventForTesting = { event in
            if event != .microphoneTrackClosed { await successorDelivery.wait() }
        }
        delivery.open()
        await consumed.wait()
        await rig.transmit.lastMicrophoneCloseOperationForTesting?.value
        await rig.transmit.controller.settle()
        #expect(await rig.transmit.controller.snapshot.state.isKeyed,
                "an OLD source event cannot be admitted as NEW's microphone close")
        #expect(await rig.transmit.controller.snapshot.stop == nil)
        #expect(await rig.transmit.controller.snapshot.microphoneWanted)
        #expect(rig.transmit.microphoneLine, "OLD's false-line event cannot clear NEW's line availability")
        #expect(station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == "tx.unkey" }.count == before)
        rig.model.beforeMediaEventForTesting = nil
        rig.model.afterMediaEventForTesting = nil
        successorDelivery.open()
        await rig.model.disconnect()
    }

    /// Holds a microphone start until opened.
    final class Gate: @unchecked Sendable {
        private let lock = NSLock()
        private var continuation: CheckedContinuation<Void, Never>?
        private var isOpen = false
        private var parked = false
        private var parkedWaiters: [CheckedContinuation<Void, Never>] = []

        var waiting: Bool { lock.withLock { parked } }

        /// Returns once something waits at the gate.
        func whenWaiting() async {
            await withCheckedContinuation { (go: CheckedContinuation<Void, Never>) in
                let now = lock.withLock { () -> Bool in
                    if parked {
                        return true
                    }
                    parkedWaiters.append(go)
                    return false
                }
                if now {
                    go.resume()
                }
            }
        }

        func wait() async {
            await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
                let (go, watchers) = lock.withLock { () -> (Bool, [CheckedContinuation<Void, Never>]) in
                    if isOpen {
                        return (true, [])
                    }
                    self.continuation = continuation
                    parked = true
                    defer { parkedWaiters.removeAll() }
                    return (false, parkedWaiters)
                }
                for watcher in watchers {
                    watcher.resume()
                }
                if go {
                    continuation.resume()
                }
            }
        }

        func open() {
            let waiting = lock.withLock { () -> CheckedContinuation<Void, Never>? in
                isOpen = true
                defer { continuation = nil }
                return continuation
            }
            waiting?.resume()
        }
    }

    // MARK: Helpers

    /// Answers the `skip`th write of `transmit.voxEnabled` after it arrives,
    /// and returns the value written.
    private func answerVoxWrite(_ station: FakeStation, accepted: Bool, skip: Int = 0) async -> LinkMessage.PropertyValue? {
        let writes: @Sendable () -> [LinkMessage.PropertyWrite] = {
            station.messages.compactMap { message -> LinkMessage.PropertyWrite? in
                if case .propertyWrite(let write) = message, write.key == "transmit",
                   write.properties.first?.name == "voxEnabled" {
                    return write
                }
                return nil
            }
        }
        // Checked at each message the fake receives, not by polling.
        if await station.waitUntil({ writes().count > skip }) {
            let writes = writes()
            if let writeId = writes[skip].writeId, let entry = writes[skip].properties.first {
                await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "transmit", writeId: writeId,
                                                                                 results: [
                    .init(property: "voxEnabled", accepted: accepted,
                          reason: accepted ? "" : TransmitModel.voxNeedsMicrophone, value: accepted ? entry : nil),
                ])))
                if accepted {
                    await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [entry])))
                }
                return entry.value
            }
        }
        Issue.record("no write of voxEnabled reached the Core")
        return nil
    }

    /// Returns once `condition` holds: now, or after a change `transmit`
    /// publishes makes it hold. No clock: the test's time limit ends one
    /// that never gets there.
    private func when(_ transmit: TransmitModel, _ condition: () -> Bool) async {
        // Every change is kept from before the first look, so none is missed
        // between a look and the next wait.
        let (changes, sink) = AsyncStream.makeStream(of: Void.self)
        let watch = transmit.objectWillChange.sink { _ in sink.yield() }
        defer { watch.cancel() }
        if condition() {
            return
        }
        for await _ in changes {
            // The change lands once the publisher's turn on the main actor ends.
            await Task.yield()
            if condition() {
                return
            }
        }
    }

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }
}

/// A frozen-clock model/controller/CommandClient chain. The stand-in replaces
/// only the far transport and Core answers, and observes actual permit handoff.
@MainActor
final class TransmitOwnershipRig {
    let transport: TransmitOwnershipTransport
    let mirror: MirrorStore
    let commands: CommandClient
    let slices: BandSlicesModel
    let transmit: TransmitModel
    let take: TransmitTakeModel

    private init() {
        let transport = TransmitOwnershipTransport()
        let clock = FrozenClock()
        let mirror = MirrorStore(send: { try await transport.send($0) }, clock: clock)
        let commands = CommandClient(clock: clock, send: { try await transport.send($0) },
                                     captureSender: { transport.captureSender() })
        transport.commands = commands
        transport.mirror = mirror
        mirror.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(.init(properties: [
            .init(name: "remoteTxVersion", value: .i64(2)),
            .init(name: "txStateVersion", value: .i64(2)),
            .init(name: SeveralDevices.capability, value: .i64(1)),
            .init(name: "txPermitted", value: .bool(true)),
            .init(name: "propertyResultVersion", value: .i64(1)),
        ])))
        mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState",
                                         properties: TransmitScreenTests.holder("", short: "", keyed: false))))
        mirror.apply(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
            .init(ordinal: 97, name: "voxEnabled", value: .bool(false)),
        ])))
        mirror.apply(.snapshotComplete)
        let slices = BandSlicesModel(store: mirror, commands: commands)
        let controller = PttController(commands: TransmitCommandClient(commands: commands), clock: clock)
        let transmit = TransmitModel(mirror: mirror, commands: commands, slices: slices, subscriber: nil,
                                     controller: controller, captureVoxSender: { transport.captureSender() },
                                     platform: .init(backgroundWork: { _ in {} }, isScreenAwake: { false },
                                                     setScreenAwake: { _ in }))
        self.transport = transport
        self.mirror = mirror
        self.commands = commands
        self.slices = slices
        self.transmit = transmit
        transmit.refresh()
        take = TransmitTakeModel(mirror: mirror, commands: commands, slices: slices, transmit: transmit, devices: nil,
                                 captureTakeSender: { transport.captureSender() })
    }

    static func make() async -> TransmitOwnershipRig {
        let rig = TransmitOwnershipRig()
        await rig.commands.handle(.stateChanged(.ready))
        await rig.transmit.sessionChanged(.ready, owner: 1)
        return rig
    }

    func replaceOwner() async {
        take.sessionChanged(.stopped)
        transport.owner = 2
        await commands.handle(.stateChanged(.stopped))
        await commands.handle(.stateChanged(.ready))
        await transmit.sessionChanged(.ready, owner: 2)
    }

    func keyAndArm() async {
        await transmit.controller.tap()
        await transmit.controller.settle()
        await transmit.controller.setVoxArmed(true)
    }

    private struct FrozenClock: LinkClock {
        let nowMilliseconds: Int64 = 0
        func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
            Timer()
        }
        private struct Timer: LinkTimer { func cancel() {} }
    }
}

final class TransmitOwnershipTransport: @unchecked Sendable {
    private let lock = NSLock()
    private var currentOwner: UInt64 = 1
    private var handedOff: [(UInt64, LinkMessage)] = []
    var commands: CommandClient?
    @MainActor weak var mirror: MirrorStore?
    var beforeHandoff: (@Sendable (LinkMessage) async -> Void)?

    var owner: UInt64 {
        get { lock.withLock { currentOwner } }
        set { lock.withLock { currentOwner = newValue } }
    }

    func messages(owner: UInt64) -> [LinkMessage] {
        lock.withLock { handedOff.filter { $0.0 == owner }.map { $0.1 } }
    }

    func captureSender() -> CommandClient.CommandSender {
        let admitted = owner
        return { [self] message, permit in
            await beforeHandoff?(message)
            guard permit.handoff({
                lock.withLock {
                    guard currentOwner == admitted else { return false }
                    handedOff.append((admitted, message))
                    return true
                }
            }) else { throw LinkSendError.notConnected }
            await answer(message)
        }
    }

    func send(_ message: LinkMessage) async throws {
        await beforeHandoff?(message)
        lock.withLock { handedOff.append((currentOwner, message)) }
        await answer(message)
    }

    private func answer(_ message: LinkMessage) async {
        switch message {
        case .commandInvoke(let invoke):
            await commands?.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: true,
                                                         reason: "", affected: [], values: [
                .init(name: "epoch", value: .i64(7)),
            ])))
        case .propertyWrite(let write):
            await MainActor.run {
                if let id = write.writeId {
                    mirror?.apply(.propertyResult(.init(key: write.key, writeId: id, results: write.properties.map {
                        .init(property: $0.name, accepted: true, reason: "", value: $0)
                    })))
                }
                mirror?.apply(.delta(.init(key: write.key, properties: write.properties)))
            }
        default:
            break
        }
    }
}

/// Retain selected actual transport replies without blocking the connection's
/// event pump, allowing an off answer to overtake an earlier key answer.
final class HeldCommandReplies: @unchecked Sendable {
    private let lock = NSLock()
    private var held: Set<String> = []
    private var buffered: [(String, LinkTransportEvent, @Sendable (LinkTransportEvent) async -> Void)] = []
    func hold(_ verb: String) { _ = lock.withLock { held.insert(verb) } }
    func count(_ verb: String) -> Int { lock.withLock { buffered.filter { $0.0 == verb }.count } }
    func receive(_ event: LinkTransportEvent, forward: @escaping @Sendable (LinkTransportEvent) async -> Void) async {
        if case .text(let text) = event, let message = try? LinkCodec.decode(text),
           case .commandResult(let result) = message {
            let saved = lock.withLock {
                if held.contains(result.verb) { buffered.append((result.verb, event, forward)); return true }
                return false
            }
            if saved { return }
        }
        await forward(event)
    }
    func release(_ verb: String) async {
        let saved = lock.withLock {
            held.remove(verb)
            let saved = buffered.filter { $0.0 == verb }
            buffered.removeAll { $0.0 == verb }
            return saved
        }
        for (_, event, forward) in saved { await forward(event) }
    }
}

final class ReplyHoldingTransport: LinkTransport, @unchecked Sendable {
    let inner: any LinkTransport
    let replies: HeldCommandReplies
    init(inner: any LinkTransport, replies: HeldCommandReplies) { self.inner = inner; self.replies = replies }
    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        try await inner.open { [replies] event in await replies.receive(event, forward: onEvent) }
    }
    @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
    func ping() { inner.ping() }
    func close() { inner.close() }
}
