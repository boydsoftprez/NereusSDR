# Manual authoring notes

These notes are for manual contributors. They are separate from the
operator-facing chapters.

## Agreed audience and purpose

JJ's direction, 2 October 2026: explain how to use the applications for
operators with some SDR experience. An HPSDR radio is already a hobbyist
device. Include screenshots.

Assume familiarity with frequencies, bands, modes, filters, and ordinary
receive/transmit operation. Explain NereusSDR-specific terms and behavior
when they first matter. Do not turn the manual into an introductory SDR
course.

## Writing approach

- Organize the main chapters around operating tasks.
- Use the exact labels shown by the application.
- Explain what a control changes and how the operator checks the result.
- Identify whether a setting applies to the selected slice, this device,
  or the shared Core/radio when that affects operation.
- Give separate desktop and mobile steps when their interaction differs.
- Explain warnings and refusals beside the action that can produce them.
- Keep detailed control descriptions in the reference chapter.
- Cover the implemented operating inventory in [the coverage record](coverage.md).
  A feature name, screenshot label or menu list is not a completed procedure.
  Explain prerequisites, entry path, actions, interactions, scope, readback and
  recovery. Do not impose a word limit that removes needed instructions.

Maintain the manual's source in this directory. Website publication should
render the same chapters and images, avoiding a second independently
maintained copy of the prose. The local review preview renders these Markdown
sources with the website's existing colors and type styles. No website
deployment has been performed.

## Screenshot plan

Capture the running applications for the build the chapter documents.
Design mockups can guide the capture list but cannot establish the final
appearance or behavior. Preserve the original capture alongside its
annotated version in `images/`.

| Capture | What it should explain |
| --- | --- |
| Desktop overview | Spectrum, waterfall, selected VFO flag, applets, meters, and connection status. |
| Desktop radio connection | The actual radio selection and connection workflow. |
| Desktop receive controls | Mode, filter, AGC, AGC-T, gain, and noise controls. |
| Desktop tuning and slices | Selected slice, other slices, passband, CTUN, and panadapter controls. |
| Desktop transmit controls | Microphone source, profile, levels, RF power, and transmit state. |
| Core setup | The controls used to enable the Core and pair a device. |
| iPhone pairing | Core selection, pairing, and the first connected screen. |
| iPhone portrait overview | Toolbar, VFO flag, spectrum, waterfall, PTT, and tabs. |
| iPhone landscape overview | The operating layout and access to controls when turned sideways. |
| iPhone Modes | Selected slice controls and the route to more detailed settings. |
| iPhone transmit | PTT state and microphone, power, and SWR indications. |
| Shared Core operation | Another device's slice, transmit ownership, and a takeover confirmation. |
| Settings scope | A page that clearly distinguishes Core settings from device settings. |
| Troubleshooting | A real connection or permission message discussed in the text. |
| Detailed DSP and filter bank | NR/model status, blanker parameters, AGC, notch table and preset editing. |
| Voice processing/profile editor | Profile lifecycle, TX input, EQ/CFC and VOX/DEXP controls. |
| Hardware and PA | Radio Info, Antenna/ALEX, HL2 I/O and supported calibration/profile readbacks. |
| Display and containers | Spectrum/waterfall settings, history/LIVE, 3D and source-associated container editor. |
| RADE and reporter | Actual decode status, profile, offset and station-list/QSY controls. |
| Spot Hub | Each implemented source's configuration, status, list/filter/display and spot actions. |
| PureSignal and diversity | Feedback/readiness, AmpView and two-ADC gain/phase/resource state. |
| Station accessories | PGXL/TGXL/RF-Kit setup and live operating/telemetry/fault controls. |
| Phone preferences | Navigation, Audio, Battery and sessions, Data use and PTT time-out. |
| Device/session conflicts | Fifth-device choice, Taken Over, receiver takeover and TX handoff readbacks. |

Use short numbered callouts, a matching caption, and descriptive alt text.
Keep controls legible at the width used on the website. Use a whole-window
image for orientation and closer crops for detailed procedures.

Record the desktop/Core version or commit, iPhone build, device orientation,
and relevant operating state for every capture. Use representative station
data; exclude pairing codes, keys, private addresses, and unrelated personal
information.

The draft includes desktop overview callouts, nine focused desktop tool/settings
captures and eleven native phone/iPad simulator captures. Build information,
capture methods, test-state limits, hashes and the remaining release capture
list are in [the screenshot inventory](images/README.md). Simulator fixtures
must be labeled as simulated Core data; they cannot certify a live station
workflow. The existing v0.1.6 desktop image predates the current interface and
is not used as a current manual illustration.

## Checking a chapter

See [the draft verification record](verification.md) for the source build,
review results, and checks still needed before publication.

Verify labels and interactions against the application source and the
running build. Follow the procedure from its stated starting condition.
Check that each screenshot agrees with the steps and expected result.
State any radio-specific or build-specific limitation where it matters.
Recheck screenshots when the relevant interface changes. The 4 October carry
reconciles a bounded set of desktop instructions with the pinned candidate
listed in [verification](verification.md); its historical EQ/CFC figures do
not illustrate the revised editors. Preserve their original provenance and
complete the selected-release Core Settings, Canvas and EQ/CFC captures before
publication. The candidate does not include iOS source, so phone chapters do
not establish native-app release delivery.

For live verification, follow complete sessions rather than merely checking
that a widget appears: local desktop receive; remote desktop receive; first
phone pairing and reconnect; phone receive/audio interruptions; a coordinated
receiver/TX handoff; microphone setup and a controlled transmission; one external
digital audio route; and each supported hardware/accessory calibration procedure.
Use appropriate station equipment and an operator who can verify the wiring
and RF state. Mark the tested build, device, prerequisites, steps and observed
result. Record procedures that remain source-verified only as such.

The PA automatic calibration interface requires separate validation of its
passive, operator-keyed sampling and drive-point progression before it can
be presented as an end-to-end calibration routine. A source-visible Start
button is insufficient evidence for that routine.

The introductory draft's Core explanation and iPhone tab names were checked
against `ios/README.md`, `ios/NereusApp/Connect/WelcomeScreen.swift`,
`ios/NereusApp/Connect/SetUpCoreScreen.swift`,
`ios/NereusApp/App/AppTab.swift`, and `ios/NereusApp/App/RootView.swift`
in this checkout. The proposed chapter outline does not establish that every
listed feature is available in a public release.

## Build the website preview

From the repository root, with Python 3 and `markdown-it-py` installed:

```sh
python3 docs/manual/build_preview.py
python3 docs/manual/check_manual.py --preview build/manual-preview
python3 -m http.server 18766 --bind 127.0.0.1 --directory build/manual-preview
```

Open `http://127.0.0.1:18766/`. Use `--output /path/to/review-directory`
to select another output location. The generated files use the website's
existing CSS plus a separate manual stylesheet, compatible with its
content security policy. They are review output, not a publication step.

The review build includes the operator index and numbered chapters. Contributor
notes and the screenshot inventory remain source documents in this directory;
the preview copies them as Markdown downloads and retains those source links.
Before publication, finish the pending captures and live procedure checks,
choose the supported build/version, then add the generated manual to the site's
navigation and publication process.
