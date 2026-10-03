#!/usr/bin/python3
# no-port-check: NereusSDR-original.
"""The server's data-use report (rendezvous/README.md, "Data use").

Run every hour by nereus-data-use.timer. It reads how many bytes the
server has sent on one network interface (the kernel's counter,
/sys/class/net/<interface>/statistics/tx_bytes), adds what was sent since
the last reading to the calendar month's total (UTC), and once a day
writes one line to the journal with the total so far this month, plus a
warning line when the total has passed the threshold. It changes nothing
and caps nothing.

The kernel's counter starts again from zero at every boot. The report
keeps the boot's id with each reading, so a restart is seen and the
counter since boot is added whole. What was sent between the last hourly
reading and the restart is not known and is not counted, so after a
reboot the total can be short by up to an hour's traffic; the line says
so for the rest of the month. What was sent before the report was first
installed this month is not counted either, and the line gives the time
the count starts from.

Usage: data-use.py --interface IFACE --threshold-gb N --state FILE
       [--statistics DIR] [--boot-id FILE] [--now UNIX_SECONDS]
(the last three exist for the tests)
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import sys
import tempfile

GB = 1000 ** 3


def month_of(ts: float) -> str:
    return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m")


def day_of(ts: float) -> str:
    return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m-%d")


def stamp(ts: float) -> str:
    return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")


def read_int(path: str) -> int:
    with open(path) as handle:
        return int(handle.read().strip())


def load_state(path: str):
    try:
        with open(path) as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return None


def save_state(path: str, state) -> None:
    directory = os.path.dirname(path) or "."
    fd, staged = tempfile.mkstemp(dir=directory, prefix=".data-use.")
    with os.fdopen(fd, "w") as handle:
        json.dump(state, handle)
    os.replace(staged, path)


def gb(n: int) -> str:
    return "%.2f GB" % (n / GB)


def run(args) -> list:
    """Takes a reading, updates the state file, and returns the lines to
    log as (priority, text); priority 4 is a warning, 6 information."""
    now = args.now if args.now is not None else datetime.datetime.now(datetime.timezone.utc).timestamp()
    counter = read_int(os.path.join(args.statistics, args.interface, "statistics", "tx_bytes"))
    with open(args.boot_id) as handle:
        boot = handle.read().strip()
    month = month_of(now)
    state = load_state(args.state)
    lines = []
    if state is None or state.get("interface") != args.interface:
        # The first reading: nothing is known about what was sent before.
        state = {
            "interface": args.interface,
            "month": month,
            "total": 0,
            "since": now,
            "short": False,
            "boot": boot,
            "counter": counter,
            "reading": now,
            "logged": "",
        }
    else:
        if state["boot"] == boot and counter >= state["counter"]:
            delta = counter - state["counter"]
        else:
            # The counter started again (a reboot): everything since boot is
            # new; what was sent after the last reading before the reboot is
            # lost.
            delta = counter
            state["short"] = True
        # Bytes since the last reading belong to the month of that reading
        # (they are at most an hour old when the timer runs on time).
        if month_of(state["reading"]) == state["month"]:
            state["total"] += delta
        if state["month"] != month:
            lines.append((6, "data use: %s ended at %s sent on %s%s" % (
                state["month"], gb(state["total"]), args.interface,
                " (short: the server restarted during the month)" if state["short"] else "")))
            state.update(month=month, total=0, since=now, short=False)
        state.update(boot=boot, counter=counter, reading=now)
    today = day_of(now)
    if state["logged"] != today:
        state["logged"] = today
        # Name when the count started unless it covers the whole month
        # (the first reading within the month's first hour).
        month_start = datetime.datetime.strptime(month + "-01", "%Y-%m-%d").replace(tzinfo=datetime.timezone.utc)
        since = ""
        if state["since"] > month_start.timestamp() + 3600:
            since = ", counted from %s" % stamp(state["since"])
        lines.append((6, "data use: %s so far this month (%s) on %s%s%s; report threshold %d GB" % (
            gb(state["total"]), month, args.interface, since,
            ", short: the server restarted and up to an hour before each restart is not counted" if state["short"] else "",
            args.threshold_gb)))
        if state["total"] > args.threshold_gb * GB:
            lines.append((4, "data use: warning: %s sent this month on %s, past the report threshold of %d GB "
                          "(RV_TRANSFER_GB_PER_MONTH); check the provider's allowance" % (
                              gb(state["total"]), args.interface, args.threshold_gb)))
    save_state(args.state, state)
    return lines


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="data-use")
    parser.add_argument("--interface", required=True)
    parser.add_argument("--threshold-gb", type=int, required=True)
    parser.add_argument("--state", required=True)
    parser.add_argument("--statistics", default="/sys/class/net")
    parser.add_argument("--boot-id", default="/proc/sys/kernel/random/boot_id")
    parser.add_argument("--now", type=float)
    args = parser.parse_args(argv)
    try:
        lines = run(args)
    except (OSError, ValueError) as exc:
        print("<3>data use: cannot read the counter for %s: %s" % (args.interface, exc), flush=True)
        return 1
    for priority, text in lines:
        # systemd reads the <N> prefix as the line's priority.
        print("<%d>%s" % (priority, text), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
