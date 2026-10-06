# App Review notes: NereusSDR for iPhone and iPad

What JJ pastes into App Store Connect's "Notes" field under App Review
Information, and what he checks before submitting. The notes themselves are
the block under "Notes for App Review".

## Notes for App Review

NereusSDR is a remote console for the operator's own amateur radio station.
It controls a NereusSDR Core, software that runs at the station on a computer
connected to an OpenHPSDR radio (for example an Apache Labs ANAN or a Hermes
Lite 2). The Core runs the radio and does all the signal processing; the app
shows the band the Core sends, plays the Core's receive audio, and sends the
operator's voice to the Core when the operator transmits.

The app has no demo mode, by design: it is a console for a real station, and
it does nothing useful without the operator's own Core. We cannot give
reviewers a sign-in, because there are no accounts: a phone pairs with a Core
the operator owns, on the same Wi-Fi with one tap, or with a short code the
Core shows.

A video of the app running against a real station is here:
VIDEO LINK (JJ adds it after recording the review video)

The video shows: finding the Core on Wi-Fi and pairing, the band and the VFO
flags, tuning, receive audio, keying with the PTT on the screen, the Live
Activity with the phone locked, and Setup, About this app.

Why the app asks for each permission:
- Microphone: the operator's voice goes to their own station while they
  transmit. Apart from that, the microphone runs only for the microphone
  level meter when the operator opens it; that level stays on the phone.
- Local network: to find the operator's Core on the same Wi-Fi
  (Bonjour service `_nereus-station._tcp`) and connect straight to it.
- Background audio: receive audio keeps playing with the screen locked, as a
  radio does.

Encryption: the app uses TLS, DTLS and SRTP for its links to the station, as
described in the export compliance answers.

Transmitting on the air requires an amateur radio licence; the station's
software and the operator's licence govern what is transmitted.

## Before submitting (JJ)

- Record the review video on a real station and replace VIDEO LINK above
  with its link (an unlisted video, or a file attached in App Store Connect).
- Fill in App Review Information's contact name, phone and email in App
  Store Connect. They are not kept in the repository.
- Leave "Sign-in required" off: the app has no accounts.
- `Info.plist` declares two background modes the app does not use today:
  `push-to-talk` and `bluetooth-central` (with a Bluetooth usage
  description). The Release app currently keys only from the PTT on the
  screen. The button experiment imports PushToTalk and CoreBluetooth only
  under `PTT_PROBE`; it cannot reach the radio's transmit path and is not
  production hardware-button support. Complete the device experiment and
  the chosen production implementation, then reconcile the Release modes,
  Bluetooth permission and provisioning with the features actually shipped.
  The store-build decision remains pending; this note does not change the
  plist or enable hardware keying.
