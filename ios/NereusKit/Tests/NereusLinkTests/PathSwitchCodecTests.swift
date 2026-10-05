// NereusSDR for iOS: the bounded control-path switch wire messages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

@Suite struct PathSwitchCodecTests {
    @Test func joinEncodesAndDecodesItsTicket() throws {
        let ticket = String(repeating: "A", count: 43)
        let message = LinkMessage.pathJoin(.init(ticket: ticket))
        #expect(message.kind == .pathJoin)
        #expect(message.kind.sentByClient)
        #expect(!message.kind.sentByStation)
        #expect(try LinkJSON.parse(LinkCodec.encode(message)) == .object([
            "type": .string("path.join"), "ticket": .string(ticket),
        ]))
        #expect(try LinkCodec.decode(LinkCodec.encode(message)) == message)
    }

    @Test func switchIsAnEmptyBarrierInBothDirections() throws {
        let message = LinkMessage.pathSwitch
        #expect(message.kind == .pathSwitch)
        #expect(message.kind.sentByClient)
        #expect(message.kind.sentByStation)
        #expect(try LinkJSON.parse(LinkCodec.encode(message)) == .object(["type": .string("path.switch")]))
        #expect(try LinkCodec.decode("{\"type\":\"path.switch\"}") == message)
    }

    @Test(arguments: [
        "{\"type\":\"path.join\"}",
        "{\"type\":\"path.join\",\"ticket\":null}",
        "{\"type\":\"path.join\",\"ticket\":0}",
        "{\"type\":\"path.join\",\"ticket\":\"\"}",
        "{\"type\":\"path.join\",\"ticket\":\"\(String(repeating: "x", count: 129))\"}",
    ]) func invalidTicketIsRefused(_ wire: String) {
        #expect(throws: LinkCodecError.self) { try LinkCodec.decode(wire) }
    }

    @Test func unknownSwitchKeysAreIgnoredByWireConvention() throws {
        #expect(try LinkCodec.decode("{\"type\":\"path.switch\",\"future\":true}") == .pathSwitch)
    }

    @Test func ticketDoesNotAppearInDiagnostics() {
        let ticket = "do-not-log-this-path-ticket"
        let join = LinkMessage.PathJoin(ticket: ticket)
        #expect(!String(describing: join).contains(ticket))
        #expect(!String(reflecting: join).contains(ticket))
        #expect(!String(reflecting: LinkMessage.pathJoin(join)).contains(ticket))
    }
}
