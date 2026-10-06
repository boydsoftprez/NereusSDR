# Export compliance: NereusSDR for iPhone and iPad

The encryption the app uses, where each use lives in the code, and the
answers for App Store Connect's encryption questions. JJ reviews these
answers and submits them; nothing here has been entered in App Store
Connect. This is a record of facts from the code, not legal advice.

Paths are relative to `ios/`. Line numbers are at the commit that added this
file.

## What the app encrypts, and with what

### 1. The control link to the Core, on the LAN or by address: TLS

A WebSocket over TLS 1.2 or later, made by Apple's Network framework
(`NWProtocolTLS`). The Core's certificate is checked against a pin, or its
SHA-256 is bound to the Core's identity key before anything is sent.

- `NereusKit/Sources/NereusLink/WebSocketLinkTransport.swift:157-176`
  (TLS options, minimum TLS 1.2, `NWParameters(tls:)`)
- `NereusKit/Sources/NereusLink/CertificatePin.swift` (SHA-256 of the
  certificate, CryptoKit)

Provider: Apple's operating system.

### 2. The remote access service and its relay: TLS

The connection to the NereusSDR remote access service
(`rv.nereussdr.com`) and to its WebSocket relay, TLS 1.2 or later with the
system's ordinary certificate trust, made by the Network framework.

- `NereusKit/Sources/NereusLink/RendezvousWebSocket.swift:143-151`
- `NereusKit/Sources/NereusLink/NetworkRelaySocket.swift:27-38`
- `NereusKit/Sources/NereusLink/RendezvousServer.swift:14` (the service's name)

Provider: Apple's operating system. What the relay carries is itself
encrypted end to end by items 3 and 4; the relay cannot read it
(`NereusKit/Sources/NereusLink/RelayFrame.swift:92-93`,
`RelayLeg.swift:35-37`).

### 3. The control link through the remote access service: DTLS

Away from the LAN, the session runs on a reliable data channel (SCTP over
DTLS) on a peer connection of its own. The Core's DTLS certificate is read
from the handshake and bound to the Core's identity key, as in item 1.

- `NereusKit/Sources/NereusLink/DataChannelSessionTransport.swift`
- `NereusKit/Sources/NereusLink/ControlChannelConnection.swift:10-14`
- `NereusKit/Sources/NereusMedia/ControlPeer.swift`
- `NereusKit/Sources/NereusMedia/RtcBridge.swift:77-108` (peer connection
  and its certificate)

Provider: bundled libraries. libdatachannel v0.24.5
(`NereusKit/Sources/CDataChannel`, built with `USE_MBEDTLS=1`,
`NereusKit/Package.swift:196`) doing DTLS through Mbed TLS 3.6.7
(`NereusKit/Sources/CMbedTLS`), and usrsctp (`NereusKit/Sources/CUsrsctp`)
for SCTP. DTLS is an IETF standard (RFC 6347, RFC 9147).

### 4. Media: DTLS and SRTP

Receive audio from the Core and the phone's microphone to the Core go as RTP
protected by SRTP, keyed from the DTLS handshake (DTLS-SRTP). Display frames
go on a data channel over DTLS. The Core's DTLS fingerprint in its offer is
checked.

- `NereusKit/Sources/NereusMedia/MediaPeer.swift:1-22`
- `NereusKit/Sources/NereusMedia/RtcBridge.swift:208-212` (the remote
  fingerprint), `:283-297` (data channel and audio tracks)
- `NereusKit/Package.swift:12` and `:185` (`MBEDTLS_SSL_DTLS_SRTP`, keying
  SRTP from DTLS)

Provider: bundled libraries. libdatachannel with Mbed TLS for DTLS, libsrtp
(`NereusKit/Sources/CSrtp`) for SRTP. SRTP (RFC 3711, AES) and DTLS-SRTP
(RFC 5764) are IETF standards.

### 5. Pairing by code: SPAKE2+EE, Argon2id and XChaCha20-Poly1305

A phone and a Core that share a short code agree two keys with SPAKE2+EE;
the code is stretched with Argon2id first. The keys then seal the two
confirmation boxes, each holding a public key and a name, with
XChaCha20-Poly1305. Its purpose is authentication: each side proves it held
the code and learns the other's identity key.

- `NereusKit/Sources/NereusLink/SpakeExchange.swift:86` (Argon2id, libsodium
  `crypto_pwhash`), `:111`, `:139`, `:191`, `:228`, `:254` (the SPAKE2+EE
  steps)
- `NereusKit/Sources/NereusLink/PairingBox.swift:83`, `:107` (the boxes,
  libsodium `crypto_aead_xchacha20poly1305_ietf_encrypt` and `_decrypt`)

Provider: bundled libraries. libsodium 1.0.22 (`NereusKit/Sources/CSodium`)
and spake2-ee (`NereusKit/Sources/CSpake2EE`). Standards status: SPAKE2 is
RFC 9382 and Argon2 is RFC 9106, but SPAKE2+EE is a variant published by
libsodium's author rather than an IETF standard, and XChaCha20-Poly1305 is
an IRTF draft built on ChaCha20-Poly1305 (RFC 8439).

### 6. Device keys: ECDSA P-256

Each device has an ECDSA P-256 signing key, in the Secure Enclave where
there is one (the Keychain otherwise), made and used through CryptoKit. It
signs the device's sign-in and its introductions through the remote access
service. Signing only; it encrypts nothing.

- `NereusKit/Sources/NereusLink/SecureEnclaveKeyStore.swift:48-55`
- `NereusKit/Sources/NereusLink/KeychainKeyStore.swift:23`, `:35`
- `NereusKit/Sources/NereusLink/DeviceSigningKey.swift:22-23`
- `NereusKit/Sources/NereusLink/DeviceKeyAuthenticator.swift`

Provider: Apple's operating system.

### 7. Integrity and hashing, no encryption

- SHA-256 through CryptoKit for certificate pins and identity values
  (`NereusKit/Sources/NereusLink/P256Wire.swift:45`,
  `RendezvousIdentity.swift:26`, `FoundStation.swift:73`).
- STUN and TURN message integrity (HMAC) inside libjuice
  (`NereusKit/Sources/CJuice`), with relay credentials the service issues
  (`NereusKit/Sources/NereusLink/RendezvousTurn.swift`).
- Secrets at rest are kept in the iOS Keychain
  (`NereusKit/Sources/NereusLink/KeychainItem.swift`).

Nothing else in the app, the widget extension or NereusKit uses encryption.
The app has no proprietary cipher of its own.

## The answer

JJ chose Reading A on 2026-09-28: the app uses encryption beyond what iOS
provides. Items 3, 4 and 5 are standard algorithms bundled in the app (Mbed
TLS, libsrtp and libsodium) protecting the confidentiality of the link, which
Apple's own table (App Store Connect Help, "Export compliance documentation
for encryption") puts in its own category: an app that "uses an industry
standard algorithm, not provided within the Apple operating system".

The App Store Connect answers are:
- Does the app use encryption: YES, answered in App Store Connect for each
  build (see "Info.plist key" below for why the key is not in the app).
- "What type of encryption algorithms does your app implement?": standard
  encryption algorithms instead of, or in addition to, the encryption within
  Apple's operating system. JJ counts item 5's SPAKE2+EE and
  XChaCha20-Poly1305 as standard for this answer, so the answer is not
  "both" and no US CCATS is needed.
- Available in France: either a French encryption declaration uploaded in
  App Store Connect, or France left out of the app's availability. This is
  JJ's App Store Connect step and it is not done.
- The annual US self-classification report to BIS and the ENC Encryption
  Request Coordinator. This is JJ's step and it is not done.

Rejected: Reading B, that the encryption is exempt under the US Export
Administration Regulations (standard encryption in a mass-market product,
5D992.c, or software whose source is published free of charge), answering NO
as Signal's iOS app does. Builds 10 and 11 were delivered that way, with
non-exempt encryption "No". JJ preferred the reading that follows Apple's
categories word for word, since YES when NO was allowed costs only a French
declaration or leaving France out, while NO when YES was required is a false
declaration.

## Info.plist key

The answer is YES (Reading A above). The app's `NereusApp/Info.plist` does
not carry `ITSAppUsesNonExemptEncryption` for now, and it does not carry
`ITSEncryptionExportComplianceCode`.

Why: build 14 was uploaded with `ITSAppUsesNonExemptEncryption` = true and App
Store Connect refused it with error 90592, "Invalid Export Compliance Code".
With the key at YES, Apple requires `ITSEncryptionExportComplianceCode` from
approved App Encryption Documentation, and that documentation does not exist.
On 2026-09-28 JJ decided to take the key out again, as in builds 10 to 13.
His answer is still YES: standard encryption algorithms in addition to the
encryption within Apple's operating system. He gives that answer in App
Store Connect for each build until Apple issues a code.

The long-term steps are JJ's, and none is done:
1. File App Encryption Documentation with Apple (standard encryption in
   addition to iOS) and receive the code.
2. Put both keys in the app's Info.plist: `ITSAppUsesNonExemptEncryption` =
   true and `ITSEncryptionExportComplianceCode` = the code. Then change the
   two lines below to `YES` and the code, and App Store Connect stops
   asking per build.
3. File the yearly US self-classification report.
4. For France, file a French declaration or leave France out of the app's
   availability.

The widget extension's Info.plist carries neither key: App Store Connect
reads them from the app bundle.
`tests/compliance/test_ios_store_readiness.py` fails when the plist and the
two lines below disagree: `YES-in-App-Store-Connect` and `pending` need both
keys absent; `YES` needs the key true and the code in the plist equal to the
code line; `NO` needs the key false.

ITSAppUsesNonExemptEncryption: YES-in-App-Store-Connect
ITSEncryptionExportComplianceCode: none
