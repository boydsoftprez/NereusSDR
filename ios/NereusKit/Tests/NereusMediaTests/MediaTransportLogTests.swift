// NereusSDR for iOS: the media transport's log lines reach the console with no address or secret left in them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusMedia

@Suite struct MediaTransportLogTests {
    @Test(arguments: [
        // Progress lines pass as written.
        ("rtc::impl::PeerConnection::changeState@1346: Changed state to connecting",
         "rtc::impl::PeerConnection::changeState@1346: Changed state to connecting"),
        ("rtc::impl::SctpTransport::processNotification@872: SCTP connected",
         "rtc::impl::SctpTransport::processNotification@872: SCTP connected"),
        ("Changing state to completed", "Changing state to completed"),
        ("rtc::impl::IceTransport::LogCallback@391: juice: agent.c:1253: Changing state to connected",
         "rtc::impl::IceTransport::LogCallback@391: juice: agent.c:1253: Changing state to connected"),
        ("Candidate gathering done", "Candidate gathering done"),
        // Addresses, with and without ports, zones and brackets.
        ("Got STUN mapped address 203.0.113.7:50123 from server", "Got STUN mapped address <address> from server"),
        ("Sending to 192.168.1.20", "Sending to <address>"),
        ("Using [2001:db8::1:42]:3478 now", "Using <address> now"),
        ("Pair fe80::1c2b:3aff:fe4d:5e6f%en0 is up", "Pair <address> is up"),
        ("Loopback ::1 checked", "Loopback <address> checked"),
        // Host names.
        ("Using STUN server stun.example.net:3478", "Using STUN server <host>"),
        ("Resolved 5e2b7c1a-0d3e-4f5a-9b6c-7d8e9f0a1b2c.local", "Resolved <host>"),
        // Fingerprints, candidates, SDP attributes, keys and tokens.
        ("Remote fingerprint 3A:5F:77:0C:DE:11:92:4B:AA:01", "Remote fingerprint <address>"),
        ("a=candidate:1 1 UDP 2122317823 10.0.0.2 55555 typ host", "a=candidate:<removed>"),
        ("Added candidate:1 1 UDP 2122317823 10.0.0.2 55555 typ host", "Added candidate:<removed>"),
        ("a=ice-pwd:asd88fgpdd777uzjYhagZg", "a=ice-pwd:<removed>"),
        ("key 9c1f0e2d3b4a5968778695a4b3c2d1e0 set", "key <removed> set"),
        ("-----BEGIN CERTIFICATE----- MIIB", "<removed>"),
    ])
    func eachLineIsScrubbed(line: String, expected: String) {
        #expect(MediaTransportLog.scrub(line) == expected)
    }
}
