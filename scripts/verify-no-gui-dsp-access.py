#!/usr/bin/env python3
"""Two checks over src/gui/'s reach into the local DSP engine.

CHECK 1 -- rxChannel() is BANNED in src/gui/.

Phase 3F Sub-Epic J. The per-slice pipeline is SliceModel property ->
RadioModel push -> rxChannel(slice->sliceIndex()). Controls that call
rxChannel() from the GUI bypass it and, historically, hardcode channel 0,
which is how ANF on slice B ended up toggling slice A.

Allowlist entries are files whose rxChannel() use is engine-internal rather
than a control write.

CHECK 2 -- rxChannelForSlice() is INVENTORIED in src/gui/, not banned.

Remote-daemon R2 Task 20, fix round 2. This is a different problem with a
deliberately different answer, which is why it is a second check rather
than a second pattern bolted onto the first.

rxChannelForSlice() is the per-slice-correct wrapper the tree is actively
migrating reach-through onto (see setup/DspOptionsPage.cpp, which carries a
comment recording exactly that migration). In LOCAL direct mode these calls
are legitimate and correct: banning them would undo Sub-Epic J's own fix.

The hazard is remote-station mode (R2). A Role::Remote RadioModel holds a
WdspEngine with no channels, so rxChannelForSlice() returns nullptr, every
call site's `if (RxChannel* ch = ...)` guard swallows it, and the control
silently does nothing. RadioModel's local-DSP hand-out audit -- which is
what disables the Setup pages that reach for this process's DSP -- does NOT
count this wrapper, deliberately: two of its call sites run at page
construction time, so counting it would disable Setup > DSP > Options and
Setup > DSP > MNF outright, and both pages exist mainly to edit
Station-scoped settings that must round-trip. That reasoning is written out
at src/models/RadioModel.h's "KNOWN BLIND SPOT" block.

So the accessor is silent on remote and uncounted by the audit, and a new
Setup page that reaches DSP through it comes up ENABLED and dead with
nothing in the tree noticing. A ban is the wrong answer (the calls are
legitimate); an inventory is the right one. Adding a call in a file not
listed below fails, and so does changing the count in a file that is
listed. Either way the person making the change has to come here and say
what they are doing, which is the whole point.

If you are adding a call: add or bump the entry AND check whether the
surface you are adding it to is reachable in remote mode. If it is a Setup
page, it will be enabled and dead there. If you are removing one: drop the
count, and drop the entry entirely when it reaches zero.

CHECK 3 -- localAudioDevices() is ALLOW-LISTED in src/.

R3 remote window Setup plan, Task 1 (R-R3-23). RadioModel::localAudioDevices()
returns the same AudioEngine as audioEngine() but does NOT bump the
local-DSP hand-out audit, because a remote window's engine is live for this
computer's own sound devices: it plays remote audio through the speakers and
opens the microphone for Test Mic. That makes it an escape hatch from the
Setup gate, so it is allowed only in the files below, each of which uses the
engine for this computer's devices and nothing else. A call anywhere else in
src/ fails; a new caller has to come here and say why the engine is live for
it on a remote model. RadioModel.h, where it is defined, is not a caller.

The allow-list pins a call COUNT per file, as check 2 does (R3 Setup fix
wave, final review I3): a whole-file entry let SetupDialog.cpp or
MainWindow.cpp hand the engine to a Core or Mixed page unnoticed. An extra
call in a listed file fails, and so does a listed file whose count drops.
Text inside a string literal (a log message naming the accessor) is not a
call and is not counted.

Usage: verify-no-gui-dsp-access.py [--root PATH]. --root checks another
checkout (or a copy of one), which is how a check is shown to fail.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUI = ROOT / "src" / "gui"
PATTERN = re.compile(r"rxChannel\s*\(")
ALLOWLIST = {
    # Meter driver: resolves per-slice channels for polling, not a control
    # write. Lives under src/gui/ for packaging reasons.
    "src/gui/meters/MeterPoller.cpp",
}

# Check 2's pattern. Distinct from PATTERN on purpose: PATTERN does not
# match this spelling at all (the character after "rxChannel" is "F", not
# whitespace or an open paren), which is exactly why this wrapper slipped
# past check 1 and needed its own.
FOR_SLICE_PATTERN = re.compile(r"rxChannelForSlice\s*\(")

# Repo-relative path -> number of rxChannelForSlice() CALLS in it. Comment
# lines mentioning the name are not calls and are not counted (see
# _is_comment_line). Verified against the tree on 2026-08-08.
FOR_SLICE_INVENTORY = {
    # Applet and flag wiring: the notch-width binding and the two
    # meter-poller hookups. The two demodulator-shift writes in the flag's
    # frequency hook now go through RadioModel::applySliceStreamCentre
    # (R-R3-49), so the model's slice offset moves with them.
    "src/gui/MainWindow.cpp": 3,
    # Setup > DSP > Options. Runs at page-construction time, which is why
    # the audit cannot count this wrapper -- see the module docstring.
    "src/gui/setup/DspOptionsPage.cpp": 1,
    # Setup > DSP > MNF's minimum-notch-width readout.
    "src/gui/setup/DspSetupPages.cpp": 1,
}


# Check 3's pattern and allow-list (repo-relative paths). The definition
# site is listed separately: it declares the accessor, it does not call it.
LOCAL_AUDIO_PATTERN = re.compile(r"\blocalAudioDevices\s*\(")
LOCAL_AUDIO_DEFINITION = "src/models/RadioModel.h"
# Repo-relative path -> number of localAudioDevices() CALLS in it.
LOCAL_AUDIO_ALLOWLIST = {
    # wrapWithAudioBackendStrip(): the backend strip (backend name, Rescan,
    # Open logs) above every Setup > Audio page.
    "src/gui/SetupDialog.cpp": 1,
    # Setup > Audio > Devices: Speakers, Headphones and Microphone cards.
    "src/gui/setup/AudioDevicesPage.cpp": 1,
    # Setup > Audio > TX Input: PC microphone device, backend, buffer and
    # Test Mic.
    "src/gui/setup/AudioTxInputPage.cpp": 1,
    # Setup > Audio > VAX: this computer's VAX outputs, which a remote
    # window feeds from the Core's receiver streams (R-R3-44).
    "src/gui/setup/AudioVaxPage.cpp": 1,
    # Setup > Audio > Advanced: VAX feedback tuning, the VAX flags and Reset
    # of this computer's audio (R-R3-44). Its DSP group writes the Core's
    # settings through AppSettings and follows the Core's availability.
    "src/gui/setup/AudioAdvancedPage.cpp": 1,
    # The title-bar master output (volume, mute, output device picker):
    # the TitleBar's engine and the speaker-change wiring beside it; and a
    # remote window's VAX outputs and their feeders (R-R3-44); and the
    # container VAX 1 / VAX 2 buttons, which open and close this computer's
    # VAX outputs as Setup > Audio > VAX does (R-R3-49); and the VAX
    # first-run check's Apply, which binds this computer's VAX outputs to
    # its new virtual cables in a remote window too (R-R3-44, parity Task 11);
    # and this computer's MON output choice, which a remote window sends the
    # Core as the transmit monitor's route (R-IOS-13, parity Task 32).
    "src/gui/MainWindow.cpp": 6,
    # The TX applet's MON output pair (Speakers / Phones): this computer's
    # monitor routing, a window-scope setting, live in a remote window as
    # in a local one (R-R3-49, parity Task 2).
    "src/gui/applets/TxApplet.cpp": 1,
    # The Core's catalogue: whether the station computer publishes VAX
    # devices (AudioEngine::vaxOutputsAllowed), read only, so the VAX Audio
    # tool is offered only where it can open (iPhone app plan Task 25, D41).
    "src/core/session/StationCatalog.cpp": 1,
    # The Core's `vax` object and vaxLevels stream (iPhone app plan Task 25,
    # R-IOS-18): the Core computer's own VAX channels, bound on a local
    # model only, the VAX switch read for vaxVersion, and the meters read
    # while a device subscribes.
    "src/core/session/StationServer.cpp": 3,
}

# A double-quoted C++ string literal, escapes included. Removed from a line
# before check 3 counts calls in it.
STRING_LITERAL = re.compile(r'"(?:[^"\\]|\\.)*"')


def _is_comment_line(stripped: str) -> bool:
    return stripped.startswith("//") or stripped.startswith("*")


def check_rx_channel_ban() -> int:
    """Check 1: no direct rxChannel() calls in src/gui/."""
    failures = []
    for path in sorted(GUI.rglob("*.cpp")):
        rel = path.relative_to(ROOT).as_posix()
        if rel in ALLOWLIST:
            continue
        for num, line in enumerate(path.read_text().splitlines(), 1):
            stripped = line.strip()
            if _is_comment_line(stripped):
                continue
            if PATTERN.search(line):
                failures.append(f"{rel}:{num}: {stripped}")
    if failures:
        print("[gui-dsp-access] GUI code must not call rxChannel() directly.")
        print("Route through SliceModel; RadioModel pushes to the right channel.")
        for f in failures:
            print(f"  {f}")
        return 1
    print(f"[gui-dsp-access] OK: no direct rxChannel() use in src/gui/")
    return 0


def check_rx_channel_for_slice_inventory() -> int:
    """Check 2: rxChannelForSlice() call sites match FOR_SLICE_INVENTORY."""
    found = {}
    sites = {}
    # Headers too, not only .cpp: an inline call in a header would be just
    # as silent on a remote client, and check 1's .cpp-only walk is the
    # reason nothing would notice.
    for pattern in ("*.cpp", "*.h"):
        for path in sorted(GUI.rglob(pattern)):
            rel = path.relative_to(ROOT).as_posix()
            for num, line in enumerate(path.read_text().splitlines(), 1):
                stripped = line.strip()
                if _is_comment_line(stripped):
                    continue
                # findall, not search: two calls on one physical line must
                # count as two, or a second call appended to an already
                # matching line would leave the total unchanged and slip
                # past the very check that exists to catch it.
                hits = len(FOR_SLICE_PATTERN.findall(line))
                if hits:
                    found[rel] = found.get(rel, 0) + hits
                    sites.setdefault(rel, []).append(f"{rel}:{num}: {stripped}")

    failures = []
    for rel in sorted(set(found) | set(FOR_SLICE_INVENTORY)):
        actual = found.get(rel, 0)
        expected = FOR_SLICE_INVENTORY.get(rel)
        if expected is None:
            failures.append(
                f"{rel}: {actual} call(s) to rxChannelForSlice(), but this "
                f"file is not in the inventory")
            failures.extend(f"    {s}" for s in sites[rel])
        elif actual == 0:
            failures.append(
                f"{rel}: inventory expects {expected} call(s) to "
                f"rxChannelForSlice() but the file has none; remove the entry")
        elif actual != expected:
            failures.append(
                f"{rel}: inventory says {expected} call(s) to "
                f"rxChannelForSlice(), found {actual}")
            failures.extend(f"    {s}" for s in sites[rel])

    if failures:
        print("[gui-dsp-access] The rxChannelForSlice() call-site inventory in")
        print("scripts/verify-no-gui-dsp-access.py no longer matches src/gui/.")
        print("These calls are legitimate in LOCAL mode and are not banned. On a")
        print("remote-station client they resolve to nullptr and the control")
        print("silently does nothing, and RadioModel's hand-out audit does not")
        print("count this wrapper, so nothing else in the tree notices. Update")
        print("the inventory, and check that the surface you added it to is not")
        print("a Setup page that would come up enabled and dead in remote mode.")
        for f in failures:
            print(f"  {f}")
        return 1

    total = sum(found.values())
    print(f"[gui-dsp-access] OK: {total} rxChannelForSlice() call(s) in "
          f"src/gui/ across {len(found)} file(s), all inventoried")
    return 0


def check_local_audio_devices_allowlist() -> int:
    """Check 3: localAudioDevices() calls match LOCAL_AUDIO_ALLOWLIST's counts."""
    found = {}
    sites = {}
    src = ROOT / "src"
    for pattern in ("*.cpp", "*.h", "*.mm"):
        for path in sorted(src.rglob(pattern)):
            rel = path.relative_to(ROOT).as_posix()
            if rel == LOCAL_AUDIO_DEFINITION:
                continue
            for num, line in enumerate(path.read_text().splitlines(), 1):
                stripped = line.strip()
                if _is_comment_line(stripped):
                    continue
                hits = len(LOCAL_AUDIO_PATTERN.findall(STRING_LITERAL.sub('""', line)))
                if not hits:
                    continue
                found[rel] = found.get(rel, 0) + hits
                sites.setdefault(rel, []).append(f"{rel}:{num}: {stripped}")

    failures = []
    for rel in sorted(set(found) | set(LOCAL_AUDIO_ALLOWLIST)):
        actual = found.get(rel, 0)
        expected = LOCAL_AUDIO_ALLOWLIST.get(rel)
        if expected is None:
            failures.append(
                f"{rel}: {actual} call(s) to localAudioDevices(), but this "
                f"file is not in the allow-list")
            failures.extend(f"    {s}" for s in sites[rel])
        elif actual == 0:
            failures.append(
                f"{rel}: allow-list expects {expected} call(s) to "
                f"localAudioDevices() but the file has none; remove the entry")
        elif actual != expected:
            failures.append(
                f"{rel}: allow-list says {expected} call(s) to "
                f"localAudioDevices(), found {actual}")
            failures.extend(f"    {s}" for s in sites[rel])

    if failures:
        print("[gui-dsp-access] RadioModel::localAudioDevices() calls no longer")
        print("match the allow-list in scripts/verify-no-gui-dsp-access.py. It")
        print("hands out the audio engine without the local-DSP audit, so the")
        print("Setup gate cannot see a page that uses it. Use audioEngine()")
        print("instead, or update LOCAL_AUDIO_ALLOWLIST (file and count) with the")
        print("reason the engine is live for it in a remote window (this")
        print("computer's own devices).")
        for f in failures:
            print(f"  {f}")
        return 1
    total = sum(found.values())
    print(f"[gui-dsp-access] OK: {total} localAudioDevices() call(s) in src/ "
          f"across {len(found)} file(s), all allow-listed")
    return 0


def main() -> int:
    global ROOT, GUI
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=pathlib.Path, default=None,
                        help="check this checkout instead of the one holding "
                             "this script")
    args = parser.parse_args()
    if args.root is not None:
        ROOT = args.root.resolve()
        GUI = ROOT / "src" / "gui"
    # All checks always run, so one invocation reports every problem
    # rather than hiding one behind another.
    rc = check_rx_channel_ban()
    rc |= check_rx_channel_for_slice_inventory()
    rc |= check_local_audio_devices_allowlist()
    return rc

if __name__ == "__main__":
    sys.exit(main())
