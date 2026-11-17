// NereusSDR for iOS: the recordings client's Digital Spectrum Recorder verbs: past rows in chunks, the answers' values, erase and the gate
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusModels
import Testing
@testable import NereusMirror

/// Recording plan Task 28 (design sections 6.7, 6.8, A12, V9): the past
/// rows follow the Core's chunks (`rowsLeft`, `nextToUtcNs`) as they come
/// over a slow link, the answers decode to the Core's bins, `dsr.eraseAll`
/// goes as the station link document lists it, and a Core below version 4
/// is sent nothing. The verbs' bytes against the Core's own traces are in
/// RecordingsClientTests.
@MainActor
@Suite struct RecordingsClientDsrTests {
    private let sent = SentMessages()

    private func rig(version: Int64) async -> (RecordingsClient, CommandClient) {
        let mirror = MirrorStore(send: sent.sender)
        let commands = CommandClient(send: sent.sender)
        mirror.apply(FixtureReplay.stationHello())
        mirror.apply(FixtureReplay.capabilities(["stationName": .utf8(""),
                                                 RecordingsClient.capabilityName: .i64(version)]))
        let client = RecordingsClient(mirror: mirror, commands: commands)
        await commands.handle(.stateChanged(.receivingSnapshot))
        client.handle(.stateChanged(.receivingSnapshot))
        await commands.handle(.stateChanged(.ready))
        client.handle(.stateChanged(.ready))
        return (client, commands)
    }

    private func invokes() -> [LinkMessage.CommandInvoke] {
        sent.messages.compactMap {
            if case .commandInvoke(let invoke) = $0 {
                return invoke
            }
            return nil
        }
    }

    private func answer(_ index: Int, _ commands: CommandClient, values: [LinkMessage.PropertyEntry]) async {
        #expect(await sent.settle(untilCount: index + 1))
        guard index < invokes().count else {
            return
        }
        let invoke = invokes()[index]
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: true, reason: "", affected: [], values: values)))
    }

    /// A chunk as the Core's dispatcher answers it.
    private static func chunk(times: [Int64], bins: [Int16], binCount: Int, rowsLeft: Int,
                              next: Int64) -> [LinkMessage.PropertyEntry] {
        [.init(name: "slice", value: .i64(0)), .init(name: "firstBinHz", value: .f64(7_100_000)),
         .init(name: "hzPerBin", value: .f64(50)), .init(name: "binCount", value: .i64(Int64(binCount))),
         .init(name: "times", value: .utf8(DsrPastFrames.packTimes(times).base64EncodedString())),
         .init(name: "bins", value: .utf8(DsrPastFrames.packBins(bins).base64EncodedString())),
         .init(name: "transmit", value: .utf8(Data(times.map { $0 == 30 ? 1 : 0 }).base64EncodedString())),
         .init(name: "rowsLeft", value: .i64(Int64(rowsLeft))), .init(name: "nextToUtcNs", value: .i64(next))]
    }

    static let view = DsrPanView(fftSize: 4096, windowType: 1, centreHz: 7_236_000, spanHz: 48_000, pixels: 402)

    /// Over the rendezvous server the Core sends the rows in chunks: each
    /// is handed on as it comes, and the next asks from where the last
    /// ended for the rows left, until none are.
    @Test func pastRowsFollowTheChunks() async throws {
        let (client, commands) = await rig(version: 4)
        final class Seen: @unchecked Sendable {
            var chunks: [DsrPastFrames] = []
        }
        let seen = Seen()
        let task = Task {
            try await client.pastRows(slice: 0, view: Self.view, fromUtcNs: 0, toUtcNs: 50, rowCount: 5) { frames in
                seen.chunks.append(frames)
            }
        }
        await answer(0, commands, values: Self.chunk(times: [50, 40], bins: [-1_000, -900, -800, -700], binCount: 2,
                                                     rowsLeft: 3, next: 30))
        await answer(1, commands, values: Self.chunk(times: [30, 20, 10], bins: [1, 2, 3, 4, 5, 6], binCount: 2,
                                                     rowsLeft: 0, next: 0))
        try await task.value
        #expect(seen.chunks.map(\.utcNs) == [[50, 40], [30, 20, 10]])
        #expect(seen.chunks[0].tenthsDb == [-1_000, -900, -800, -700])
        #expect(seen.chunks[1].transmit == [true, false, false])
        let sentRows = invokes()
        #expect(sentRows.map(\.verb) == ["dsr.rows", "dsr.rows"])
        #expect(sentRows[0].args == [.init(name: "slice", value: .i64(0)), .init(name: "fromUtcNs", value: .i64(0)),
                                     .init(name: "toUtcNs", value: .i64(50)), .init(name: "rowCount", value: .i64(5)),
                                     .init(name: "fftSize", value: .i64(4096)), .init(name: "windowType", value: .i64(1)),
                                     .init(name: "centreHz", value: .i64(7_236_000)),
                                     .init(name: "spanHz", value: .i64(48_000)), .init(name: "pixels", value: .i64(402))])
        #expect(sentRows[1].args[2] == .init(name: "toUtcNs", value: .i64(30)))
        #expect(sentRows[1].args[3] == .init(name: "rowCount", value: .i64(3)))
    }

    @Test func aSpectrumHoursAndASaveReadTheirAnswers() async throws {
        let (client, commands) = await rig(version: 4)
        let spectrum = Task { try await client.pastSpectrum(slice: 0, view: Self.view, utcNs: 77) }
        await answer(0, commands, values: Self.chunk(times: [77], bins: [-1_100, -1_050], binCount: 2, rowsLeft: 0,
                                                     next: 0))
        let frame = try await spectrum.value
        #expect(frame.utcNs == [77] && frame.dbm(row: 0, bin: 1) == -105 && frame.firstBinHz == 7_100_000)

        let hours = Task { try await client.hours() }
        await answer(1, commands, values: [
            .init(name: "starts", value: .utf8(DsrPastFrames.packTimes([7_200_000_000_000, 3_600_000_000_000])
                .base64EncodedString())),
            .init(name: "activity", value: .utf8("[0.8,0.1]")),
        ])
        #expect(try await hours.value == [.init(startUtcNs: 7_200_000_000_000, activity: 0.8),
                                          .init(startUtcNs: 3_600_000_000_000, activity: 0.1)])

        let save = Task { try await client.saveStretch(fromUtcNs: 1, toUtcNs: 2, kind: .sound) }
        await answer(2, commands, values: [.init(name: "note", value: .utf8("Saving 14:00 to 14:10 UTC to Recordings."))])
        #expect(try await save.value == "Saving 14:00 to 14:10 UTC to Recordings.")
        #expect(invokes()[2].args.last == .init(name: "kind", value: .utf8("Sound")))
    }

    /// `dsr.eraseAll` takes no arguments (station link document, Task 26).
    @Test func eraseAllGoesWithNoArguments() async throws {
        let (client, commands) = await rig(version: 4)
        let task = Task { try await client.eraseAll() }
        await answer(0, commands, values: [])
        try await task.value
        let invoke = try #require(invokes().first)
        let expected: [String: Any] = ["type": "command.invoke", "verb": "dsr.eraseAll", "id": Int(invoke.id),
                                       "args": [Any]()]
        #expect(LinkCodec.encode(.commandInvoke(invoke)) == (try LinkJSON(foundation: expected).compactText))
    }

    /// V9: a Core at version 3 is sent none of the recorder's verbs.
    @Test func aCoreBeforeTheRecorderIsSentNothing() async {
        let (client, _) = await rig(version: 3)
        #expect(client.unavailableReason(needs: RecordingsClient.dsrVersion) == RecordingsClient.updateReason)
        let calls: [() async throws -> Void] = [
            { try await client.rewind(slice: 0, utcNs: 1, frequencyHz: 7_100_000) },
            { try await client.seekPast(slice: 0, utcNs: 1) }, { try await client.live(slice: 0) },
            { try await client.pastRows(slice: 0, view: Self.view, fromUtcNs: 0, toUtcNs: 1, rowCount: 1) { _ in } },
            { _ = try await client.pastSpectrum(slice: 0, view: Self.view, utcNs: 1) },
            { _ = try await client.hours() },
            { _ = try await client.saveStretch(fromUtcNs: 0, toUtcNs: 1, kind: .wholePan) },
            { _ = try await client.saveSoundOnly(id: "r") }, { try await client.openStretch(id: "r") },
            { try await client.eraseAll() },
            { try await client.setSettings(RecordingsSettingsChange(dsrEnabled: true)) },
        ]
        for call in calls {
            await #expect(throws: RecordingsError.needsCoreUpdate) { try await call() }
        }
        #expect(sent.count == 0)
    }
}
