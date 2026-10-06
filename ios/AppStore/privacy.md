# Privacy label: NereusSDR for iPhone and iPad

The answers for App Store Connect's App Privacy section, and why each is
true to the code. JJ reviews them and enters them. Paths are relative to
`ios/`.

## The answers

- Privacy Policy URL: https://nereussdr.com/iphone-privacy
  (`website/public/iphone-privacy.html`, live once JJ deploys the website)
- "Do you or your third-party partners collect data from this app?": **No,
  we do not collect data from this app.** The label then reads **Data Not
  Collected**.
- Tracking: none. The app has no App Tracking Transparency prompt and no
  `NSUserTrackingUsageDescription`.

## Why "Data Not Collected" is true

Apple counts data as collected when it leaves the device in a way the
developer or a partner can keep beyond serving the request in real time.

1. **No analytics, advertising, crash-reporting or tracking code.** The app,
   its widget and NereusKit import only Apple frameworks and NereusKit's own
   modules, and bundle only the libraries in `THIRD-PARTY.md` (Opus,
   libdatachannel, libjuice, libsrtp, usrsctp, plog, Mbed TLS, libsodium,
   spake2-ee), none of which reports anywhere.
   `tests/compliance/test_ios_store_readiness.py` fails if any source,
   package or project file imports or links a known analytics, advertising
   or tracking framework, Apple's AdSupport, AppTrackingTransparency or
   AdServices included, or names a known tracking endpoint.
2. **The station is the operator's own.** Everything the app sends (the
   device's name and public key at pairing, its sign-in, its commands, its
   microphone while transmitting) goes to the operator's own NereusSDR Core,
   not to the developer.
3. **The remote access service keeps nothing.** When the phone reaches its
   Core from outside the LAN it goes through the NereusSDR remote access
   service (`rv.nereussdr.com`, `NereusKit/Sources/NereusLink/RendezvousServer.swift:14`).
   The service and its relay hold what they need in memory while a
   connection is being made or relayed and write nothing to disk
   (`docs/architecture/2026-09-23-rendezvous-v1.md` sections 9.3 and 12.6,
   checked by `rendezvous/tests/test_nothing_on_disk.py`). Their log lines
   never hold an address, a whole id, a key or anything the phone sends
   (same document, sections 9.4 and 12.6). Relayed audio and control are
   encrypted end to end between the phone and the Core, so the relay cannot
   read them (`export-compliance.md`, items 2 to 4).
4. **Support files go only where the operator sends them.** The Support
   Bundle page writes this phone's log and the Core's bundle as files and
   hands them to the share sheet; the app sends them nowhere itself
   (`NereusApp/Tools/SupportBundleModel.swift`).
5. **Nothing leaves for the developer's website.** Links on the About screen
   and elsewhere open in the browser only when the operator taps them.

## Points for JJ to weigh

- The relay's TURN server (coturn) logs two kinds of failure line: a refused
  credential, naming the Core's station id and an expiry time, and a refused
  peer, naming the address a client asked to relay to
  (`rendezvous/deploy/turnserver.conf`, the "Logs" comments). These are
  error lines in the server's journal, readable by root and the adm group
  only, and rotate on their own. They are kept for running the service, not tied to a person,
  and do not change the answer, but JJ should know they exist.
- Crash reports and usage figures that operators choose to share with
  developers through Apple (TestFlight feedback, "Share with App
  Developers") come from Apple, not from code in the app. JJ should check
  Apple's current App Privacy guidance on whether they need a line on the
  label.
- If the app ever adds a crash reporter, analytics, or a server of the
  developer's that keeps what the phone sends, this label and the website's
  privacy page must change in the same commit.
