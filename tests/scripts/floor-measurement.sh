#!/usr/bin/env bash
# no-port-check: NereusSDR-original.
# =================================================================
# tests/scripts/floor-measurement.sh  (NereusSDR)
# =================================================================
#
# iPhone app plan Task 29 Step 1 (R-IOS-16): measures the relay floor's
# options for docs/architecture/2026-09-23-relay-floor-measurement.md on the
# traversal harness (tests/scripts/traversal-harness.sh, scenario
# floor-measure), then prints the tables the document holds.
#
# Each run introduces one media connection through the service (the Core's
# own LibDataChannelMediaTransport, with tests/tools/nereus_floor_probe at
# both ends) and sends, device to Core, Opus-shaped RTP every 20 ms, display
# frames 30 times a second and a keyed event every 100 ms, for
# FLOOR_DURATION_S seconds, then reports inter-arrival p50, p95, p99 and
# maximum for each, the cold time from the device's start to the first
# audio packet at the Core, CPU time at each end and on the server, and,
# for the reset runs, what a TCP reset of the floor connection cost.
#
# Options: none (today's TURN over UDP, UDP open, the direct path blocked,
# for reference), c (TURN over TLS through a local shim to coturn's TLS
# listener) and e (the service's WebSocket relay). Option (B) is measured
# through (C): see the document. c and e run with the client's network
# passing only DNS and TCP 443.
#
# Needs what the traversal harness needs (Linux, root); run it in the
# harness's container with --network none. Nothing leaves the computer.
#
# Usage: floor-measurement.sh --peer PATH --probe PATH --source DIR
#                             --out DIR [--summary-only]
# The FLOOR_* settings of the harness's floor-measure scenario pass through
# the environment.
#
# =================================================================
# Modification history (NereusSDR):
#   2026-09-26: original implementation for NereusSDR by J.J. Boyd
#               (KG4VCF), with AI-assisted implementation via Anthropic
#               Claude Code.
# =================================================================

set -euo pipefail

PEER=""
PROBE=""
SOURCE=""
OUT=""
SUMMARY_ONLY=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --peer) PEER="$2"; shift 2 ;;
        --probe) PROBE="$2"; shift 2 ;;
        --source) SOURCE="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --summary-only) SUMMARY_ONLY=1; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
if [[ -z "$OUT" ]]; then
    echo "usage: $0 --peer PATH --probe PATH --source DIR --out DIR [--summary-only]" >&2
    exit 2
fi

status=0
if (( ! SUMMARY_ONLY )); then
    if [[ -z "$PEER" || -z "$PROBE" || -z "$SOURCE" ]]; then
        echo "usage: $0 --peer PATH --probe PATH --source DIR --out DIR" >&2
        exit 2
    fi
    FLOOR_OUT="$OUT" bash "$SOURCE/tests/scripts/traversal-harness.sh" --peer "$PEER" \
        --probe "$PROBE" --source "$SOURCE" --only floor-measure || status=$?
fi

python3 - "$OUT/results.jsonl" <<'PY'
import json
import sys

rows = [json.loads(line) for line in open(sys.argv[1]) if line.strip()]
names = {"none": "today (TURN/UDP)", "c": "(C) shim, TURN/TLS", "e": "(E) WebSocket relay"}


def ms(value):
    return "-" if value is None else f"{value:.1f}"


def stream(core, name):
    s = (core or {}).get(name) or {}
    return s


def cell(s):
    return f"{ms(s.get('p50'))} / {ms(s.get('p95'))} / {ms(s.get('p99'))} / {ms(s.get('max'))}"


def arrived(s):
    sent = s.get("sent")
    received = s.get("received")
    if not sent:
        return "-"
    return f"{100.0 * received / sent:.1f} %"


print("### Inter-arrival (ms: p50 / p95 / p99 / max) and what arrived\n")
print("| Option | Loss, delay each way | Rate | Audio (20 ms) | Display (33 ms) | Keyed (100 ms) "
      "| Worst stall | Arrived (audio, display, keyed) | Cold first audio |")
print("|---|---|---|---|---|---|---|---|---|")
for r in rows:
    if r["resetAtS"]:
        continue
    core = r.get("core")
    a, d, k = stream(core, "audio"), stream(core, "display"), stream(core, "keyed")
    stalls = [x.get("max") for x in (a, d, k) if x.get("max") is not None]
    worst = max(stalls) if stalls else None
    cold = (core or {}).get("coldFirstAudioMs")
    print(f"| {names[r['option']]} | {r['lossPercent']:g} %, {r['delayMs']:g} ms | {r['rateKbps']} kbit/s "
          f"| {cell(a)} | {cell(d)} | {cell(k)} | {ms(worst)} "
          f"| {arrived(a)}, {arrived(d)}, {arrived(k)} | {ms(cold)} |")

print("\n### CPU (seconds of CPU per second of session)\n")
print("| Option | Loss, delay | Rate | Core | Device | coturn UDP | coturn TLS | Service |")
print("|---|---|---|---|---|---|---|---|")
for r in rows:
    if r["resetAtS"]:
        continue
    core = r.get("core") or {}
    device = r.get("device") or {}
    seconds = core.get("sessionSeconds") or 0
    wall = device.get("wallSeconds") or 0
    server = r.get("serverCpuSeconds") or {}

    def share(value, over):
        return "-" if not over else f"{value / over:.3f}"

    print(f"| {names[r['option']]} | {r['lossPercent']:g} %, {r['delayMs']:g} ms | {r['rateKbps']} kbit/s "
          f"| {share(core.get('cpuSecondsSession', 0), seconds)} | {share(device.get('cpuSeconds', 0), wall)} "
          f"| {share(server.get('coturnUdp', 0), wall)} | {share(server.get('coturnTls', 0), wall)} "
          f"| {share(server.get('service', 0), wall)} |")

print("\n### A TCP reset of the floor connection\n")
print("| Option | Reset at | Largest audio gap after | First audio after | Largest display gap "
      "| Largest keyed gap | Floor connections | Media connection failed at |")
print("|---|---|---|---|---|---|---|---|")
for r in rows:
    if not r["resetAtS"]:
        continue
    core = r.get("core") or {}
    device = r.get("device") or {}
    reset = core.get("reset") or {}
    failed = device.get("failedMs")
    first_after = ms(reset.get("firstAudioAfterMs"))
    largest = ms(reset.get("largestAudioGapMs"))
    if not reset:
        # No done message came back: the floor connection did not survive.
        last = core.get("lastAudioEpochMs")
        reset_at = device.get("resetEpochMs") or 0
        if last is not None and reset_at and last < reset_at + 1000:
            first_after = "never (audio stopped at the reset)"
            largest = "the rest of the run"
    print(f"| {names[r['option']]} | {r['resetAtS']} s | {largest} "
          f"| {first_after} | {ms(reset.get('largestDisplayGapMs'))} "
          f"| {ms(reset.get('largestKeyedGapMs'))} | {device.get('floorConnections', '-')} "
          f"| {'never' if failed in (None, -1) else ms(failed)} |")
PY
exit "$status"
