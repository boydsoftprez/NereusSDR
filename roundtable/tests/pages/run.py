# no-port-check: NereusSDR-original.
"""Run the page checks by hand: the scripted server, one headless Chrome,
the screenshots (with the size and contrast check on each view), then
every probe in order.

    python3 run.py --shots DIR --scratch DIR [--only NAME ...]

Prints one line a check and exits 1 if any failed.
"""

from __future__ import annotations

import argparse
import os
import sys
import time
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import probes  # noqa: E402
from cdp import chrome_path  # noqa: E402


def where(chrome) -> None:
    """What the page showed when a check failed."""
    try:
        print("  at:", chrome.eval("location.href"))
        print("  text:", (chrome.eval("document.body.innerText") or "")[:600].replace("\n", " | "))
        print("  errors:", chrome.exceptions()[:5])
    except Exception as exc:  # noqa: BLE001 - diagnostics only
        print("  (no page state:", exc, ")")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shots", required=True)
    parser.add_argument("--scratch", required=True)
    parser.add_argument("--only", nargs="*")
    parser.add_argument("--no-shots", action="store_true")
    args = parser.parse_args()
    if chrome_path() is None:
        print("SKIP: Chrome is not installed")
        return 0
    try:
        print(f"load average {os.getloadavg()[0]:.1f}", flush=True)
    except OSError:
        pass
    began = time.monotonic()
    server = probes.Server()
    print(f"server ready in {time.monotonic() - began:.0f} s: {server.facts['rooms']} rooms, {server.facts['stations']} stations", flush=True)
    chrome = probes.chrome_for(server, Path(args.scratch))
    failed = 0
    try:
        if not args.no_shots and not args.only:
            t = time.monotonic()
            try:
                names = probes.shots(chrome, server, Path(args.shots))
                print(f"PASS shots and contrast ({len(names)} shots, {time.monotonic() - t:.0f} s)", flush=True)
            except Exception:  # noqa: BLE001 - reported and counted
                failed += 1
                print("FAIL shots and contrast", flush=True)
                traceback.print_exc()
        for probe in probes.PROBES:
            name = probe.__name__
            if args.only and name not in args.only and name[len("probe_"):] not in args.only:
                continue
            t = time.monotonic()
            try:
                probe(chrome, server)
                print(f"PASS {name} ({time.monotonic() - t:.0f} s)", flush=True)
            except Exception:  # noqa: BLE001 - reported and counted
                failed += 1
                print(f"FAIL {name} ({time.monotonic() - t:.0f} s)", flush=True)
                traceback.print_exc()
                where(chrome)
    finally:
        chrome.close()
        server.close()
    try:
        print(f"load average {os.getloadavg()[0]:.1f}; {failed} failed; {time.monotonic() - began:.0f} s in all", flush=True)
    except OSError:
        pass
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
