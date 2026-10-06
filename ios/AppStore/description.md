# App Store listing: NereusSDR for iPhone and iPad

Draft text for JJ to enter in App Store Connect after release verification.
Check every named feature against the accepted Release build, and each field
against its limit before pasting. This draft is not store submission evidence.

## Name (30 characters at most)

NereusSDR

## Subtitle (30 characters at most)

Your SDR station, in your hand

## Promotional text (170 characters at most)

Run your own OpenHPSDR station from your iPhone or iPad: the band, the flags, receive audio and transmit, on your Wi-Fi or from anywhere.

## Description (4000 characters at most)

NereusSDR puts your own station in your hand. It is the iPhone and iPad console for NereusSDR, the free, open-source SDR console for OpenHPSDR radios, from the Apache Labs ANAN line to the Hermes Lite 2.

You need your own station. The app connects to a NereusSDR Core running at your station, on your computer or a small computer beside the radio. The Core runs the radio and does all the signal processing; the phone shows what the Core sends and asks it to change things. The app has no demo mode and does nothing without a Core.

THE BAND
- A live spectrum and waterfall, drawn on the GPU
- The VFO flags for each slice, with the band-plan strip, the dBm and frequency scales, and spots on the band
- Tune by dragging the band, with a dial, or by typing a frequency

RECEIVE
- Receive audio from the Core, on the phone's speaker, headphones or AirPods
- Modes, filters, AGC, noise blanking and noise reduction, RIT and XIT, all set on the Core from the phone
- Keeps playing with the screen locked, and shows the station in a Live Activity

TRANSMIT
- Transmit your voice from the phone's microphone with the PTT on the screen
- The Core's transmit time-out and interlocks stay in charge, and locking the phone ends a transmission
- Only one device holds transmit at a time, and the app shows who has it

CONNECT
- Finds your Core on your Wi-Fi and pairs with one tap
- Pairs by a short code or by typed address when you are away
- Reaches your Core from anywhere through the NereusSDR remote access service, with the link encrypted from end to end
- Up to four devices share one Core

ACCESSORIES AND TOOLS
- Operate your Power Genius XL, Tuner Genius XL and RF2K-S through your Core
- Spot Hub, FreeDV Reporter, PureSignal, diversity and the connection and performance page
- On the iPad, a wider layout with an analog S-meter

FREE SOFTWARE
NereusSDR for iPhone and iPad is free software under the GNU General Public License, version 3 or later, with an additional permission for the App Store. Its complete source code is available free of charge at https://nereussdr.com/iphone-source

The app collects no data. It has no analytics, no advertising and no tracking. See https://nereussdr.com/iphone-privacy

Transmitting needs an amateur radio licence and a radio you are permitted to operate.

## URLs

- Support URL: https://github.com/boydsoftprez/NereusSDR/issues
- Marketing URL: https://nereussdr.com/
- Privacy Policy URL: https://nereussdr.com/iphone-privacy
- Source code (named in the description): https://nereussdr.com/iphone-source

The two iphone pages are in `website/public/` (`iphone-privacy.html` and
`iphone-source.html`). Under D134 they go live from main when the phone PR
merges to main. Confirm both live destinations before distributing the build.

## Category

Suggested: Utilities as the primary category and Productivity as the
secondary. App Store Connect has no radio category; JJ picks.

## Age rating

The questionnaire's answers are all "None" or "No": no objectionable
content, no web browser inside the app, no content shared between users
through the developer, no gambling.

## Copyright

2026 J.J. Boyd
