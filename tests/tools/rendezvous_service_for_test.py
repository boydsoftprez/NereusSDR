#!/usr/bin/env python3
# no-port-check: NereusSDR-original.
"""The remote access service (rendezvous/server, nereus_rendezvous) as a
test on this computer runs it: the service's own main, unchanged, plus a
watch on the test process that started it.

A test killed at its ctest timeout, or one that crashes, runs no destructor
to stop the service it started (LocalService in tests/RendezvousTestHarness.h),
and before this launcher the service was left running with parent 1 for as
long as the computer stayed up (found up to 38 hours on). With --parent-pid
it exits within half a second of that process ending, however it ended, by
the same check as tests/tools/fake_turn_server.py. The service's main blocks
in its event loop, so a daemon thread watches the parent and ends the
process with os._exit. POSIX only; on Windows the option is accepted and
does nothing.

Usage: rendezvous_service_for_test.py [--parent-pid PID] --config FILE
Every argument but --parent-pid goes to the service's main as it is.
PYTHONPATH must include rendezvous/server, as for `python3 -m
nereus_rendezvous`.

Modification history (NereusSDR):
  2026-09-30: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
              with AI-assisted implementation via Anthropic Claude Code.
"""

from __future__ import annotations

import argparse
import os
import sys
import threading
import time

# The same parent check and interval as the fake relay; this file's own
# directory is first on sys.path when it runs.
from fake_turn_server import PARENT_POLL_S, process_alive


def watch_parent(pid: int) -> None:
    while process_alive(pid):
        time.sleep(PARENT_POLL_S)
    # The service's main is still in its event loop; end the whole process.
    os._exit(0)


def main() -> int:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--parent-pid", type=int, default=None)
    args, service_args = parser.parse_known_args()
    if args.parent_pid is not None:
        threading.Thread(target=watch_parent, args=(args.parent_pid,),
                         name="parent watch", daemon=True).start()
    from nereus_rendezvous.__main__ import main as service_main
    return service_main(service_args)


if __name__ == "__main__":
    sys.exit(main())
