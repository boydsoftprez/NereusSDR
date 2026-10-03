# Remote TGXL Controls Implementation Plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and acceptance
> cases are binding; test order follows the risk-based policy. One task; the whole-branch
> review is this task's own read-only check before it reaches the operator's radios.

**Goal:** A window connected to a remote Core switches the Tuner Genius XL's antenna (ANT 1, 2,
3) and its OPERATE and BYPASS, exactly as a local window does, whenever the radio is not
transmitting.

**Architecture:** The Core owns the TGXL (R3's Core-owned accessories). The remote window's
tuner applet sends new commands in the remote accessory control document's own pattern; the
Core applies them through its own `TunerModel`, and the mirrored state flows back as it does
today. A change is refused while the radio is on the air (the operator's decision D60).

**Tech Stack:** C++20, Qt6, the station link (`StationServer`, `StationClient`), the remote
accessory control document, the link conformance runners.

**Why now:** the operator found ANT 1/2/3 greyed out in a remote window (2026-09-24) and ruled
that it is done now: the antenna switch, OPERATE and BYPASS work from a remote window whenever
nobody is transmitting, and TUNE stays with remote transmit because it puts a carrier on the
air. His standing rule: a remote window does everything a local one does.

## Global Constraints

- CLAUDE.md and CONTRIBUTING.md bind: AppSettings (never QSettings), no raw new/delete, braces
  on all control flow, `qCWarning` for errors, platform guards with `Q_OS_*`.
- The link: every new command and capability value goes in
  `docs/architecture/2026-09-23-remote-accessory-control-v1.md` and the link document's
  tables, `tests/data/link/v1/surface.json` is changed only with its regen target, and
  `python3 scripts/render-link-tables.py --check` passes. `kSessionProtocolMinor` does not
  change.
- User-facing strings in plain operator words (`OperatorWording::isPlain`, the wording sweep's
  "Core" rule); no em dash.
- Tests run with `QT_QPA_PLATFORM=offscreen` and never open real audio devices. No RF, no
  device writes, no pushes: the operator switches his TGXL himself.
- Commits GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`); never
  `--no-verify` or `--no-gpg-sign`; no `Co-Authored-By`; explicit pathspecs; requirement IDs at
  the end of the subject (R-R3-49, and the accessory control requirements the document names).

## Task 1: The TGXL's antenna, OPERATE and BYPASS from a remote window

**Requirements:** R-R3-49 (every control a user can see does what its label says); the remote
accessory control document; the operator's D60 (transmitter-path changes wait while the radio
is on the air) and his ruling of 2026-09-24 above.

**Files:**
- Modify: `docs/architecture/2026-09-23-remote-accessory-control-v1.md`
  (`remoteTgxlControlVersion` 2 and its commands), the link document's tables, `surface.json`
- Modify: the Core's command handling for accessory commands (`StationServer`, or wherever the
  existing `setTgxlName` and `configureTgxl` commands are handled), applying through the Core's
  `TunerModel` (`setAntennaA`, `setOperate`, `setBypass`)
- Modify: `src/core/session/StationClient.{h,cpp}` (send the commands) and
  `src/gui/applets/TunerApplet.{h,cpp}` (in a remote window: the ANT, OPERATE and BYPASS
  buttons follow the Core's capability and transmit state instead of the local transmit
  permission; TUNE and the relay bars stay as they are)
- Test: a Core-side command test, a client or applet test, a conformance fixture with its
  refusals

**Interfaces:**
- Produces: `remoteTgxlControlVersion` 2, advertised when the Core owns its TGXL. It adds three
  commands in the document's existing style:
  - `setTgxlAntenna {port}`, with port 1, 2 or 3;
  - `setTgxlOperate {on}`;
  - `setTgxlBypass {on}`.
  Check the exact casing and argument names against the document's other `setTgxl*`
  commands and follow them.
- Each is refused, with a plain reason and no change applied:
  - while the radio is transmitting (MOX, TUNE or two-tone): "The radio is on the air. Try
    again when it stops.";
  - with no TGXL connected;
  - for `setTgxlAntenna`, on a TGXL with no antenna switch, or with a port outside 1 to 3.
- A window on a Core that advertises less than 2 keeps today's greyed buttons, with its
  tooltip.

**Acceptance:**
- A remote window on a Core with a connected three-antenna TGXL enables ANT 1/2/3, OPERATE and
  BYPASS while the radio is not transmitting. A click switches the Core's TGXL, and the
  window's buttons follow the Core's reported state (the mirrored `antennaA`, operate and
  bypass), not the click.
- While the radio transmits, the buttons are disabled with the reason as a tooltip, and a
  command that arrives anyway is refused with that reason.
- A local window behaves exactly as today.
- The receive-only station policy does not grey these three out: they do not transmit.
- TUNE and the relay bars in a remote window are unchanged (still remote transmit, Task 42 of
  the iPhone plan).
- Tests show each refusal and the success path. The fixture passes all three conformance
  runners.

**Verification:** it touches an accessory's RF path, so tests come first for the refusals.
Build `NereusSDR`, `nereusd` and the named tests in `build-integration`. Run them by exact name,
then the conformance runners, `tst_link_surface_manifest`, the accessory and tuner tests, and
the wording sweep. Hardware is pending: the operator switches ANT 1/2/3 on his TGXL from the
remote window, after the controller deploys the Core to the Rock and relaunches the window.

**Execution note (advisory):** opus. It runs in the integration worktree, which no lane is
carrying into at the moment.

- [ ] **Step 1:** Refusal tests (Core side), then the commands, the capability value and the
  document.
- [ ] **Step 2:** The client and the applet in a remote window, their tests, the fixture.
