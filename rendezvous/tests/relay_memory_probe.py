# no-port-check: NereusSDR-original.
"""What the relay holds in memory at its default sizing (rendezvous
document section 12.5), measured on Linux.

Not a pytest module (no test_ prefix). Run it in an ubuntu:24.04 container
with Ubuntu's python3-websockets, for example:

  docker run --rm -v "$PWD:/repo:ro" nereus-rv-test:24.04 \\
      python3 /repo/rendezvous/tests/relay_memory_probe.py

It starts the relay as its own process (python3 -m nereus_relay), as
systemd runs it, and reads its resident memory (VmRSS) after each step,
each on top of the one before:

  baseline   nothing connected
  pending    MAX_PENDING connections that have not joined (the default 64)
  sessions   SLOTS sessions, both legs joined, idle (the default 16)
  stalled    every leg stops reading while its peer sends 1500-byte
             datagrams in both lanes as fast as it can for FLOOD_S seconds,
             so every lane's queue, library buffer and socket buffers are
             full

It checks nothing; the numbers go into the rendezvous document.
"""

from __future__ import annotations

import asyncio
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SERVER = HERE.parent / "server"
sys.path.insert(0, str(SERVER))
sys.path.insert(0, str(HERE))

from nereus_rendezvous import relaygrant  # noqa: E402
from relay_helpers import connect  # noqa: E402

SLOTS = int(os.environ.get("SLOTS", "16"))
MAX_PENDING = int(os.environ.get("MAX_PENDING", "64"))
FLOOD_S = float(os.environ.get("FLOOD_S", "5"))


def rss_kib(pid: int) -> int:
    for line in Path(f"/proc/{pid}/status").read_text().splitlines():
        if line.startswith("VmRSS:"):
            return int(line.split()[1])
    raise RuntimeError("no VmRSS")


async def flood(ws, seconds: float) -> int:
    # Both lanes (control and media), so every queue of every leg fills.
    frames = (b"\x01" + bytes(1500), b"\x02" + bytes(1500))
    sent = 0
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        try:
            await asyncio.wait_for(ws.send(frames[sent % 2]), 0.5)
        except asyncio.TimeoutError:
            # The relay stopped reading this leg: its peer's queue is full
            # and this leg's inbound buffers are too.
            continue
        sent += 1
        if sent % 20 == 0:
            await asyncio.sleep(0)
    return sent


async def probe(sock_path: str, pid: int, secret: bytes) -> None:
    uri = "unix:" + sock_path

    def report(step: str, extra: str = "") -> None:
        print(f"{step:10s} relay VmRSS {rss_kib(pid) / 1024:7.1f} MiB {extra}", flush=True)

    report("baseline")
    pending = [await connect(uri, f"10.1.{n // 200}.{n % 200 + 1}") for n in range(MAX_PENDING)]
    await asyncio.sleep(1)
    report("pending", f"({len(pending)} connections)")
    for ws in pending:
        await ws.close()
    await asyncio.sleep(1)
    legs = []
    expires = int(time.time()) + 600
    for n in range(SLOTS):
        session = os.urandom(relaygrant.SESSION_BYTES)
        pair = []
        for leg, address in ((relaygrant.LEG_CORE, f"10.2.0.{n + 1}"), (relaygrant.LEG_DEVICE, f"10.3.0.{n + 1}")):
            ws = await connect(uri, address, max_queue=1)
            await ws.send(b"\x80" + relaygrant.mint(secret, leg, session, relaygrant.station_of(secret, "probe%021d" % n), expires).encode())
            assert (await asyncio.wait_for(ws.recv(), 5))[0] == 0x81
            pair.append(ws)
        legs.append(pair)
    await asyncio.sleep(1)
    report("sessions", f"({SLOTS} sessions, {2 * SLOTS} legs)")
    counts = await asyncio.gather(*(flood(ws, FLOOD_S) for pair in legs for ws in pair))
    await asyncio.sleep(1)
    report("stalled", f"({sum(counts)} frames sent into stalled legs)")


def main() -> int:
    work = Path(tempfile.mkdtemp())
    secret = os.urandom(24).hex().encode()
    (work / "relay-secret").write_bytes(secret + b"\n")
    sock_path = str(work / "relay.sock")
    (work / "relay.conf").write_text(
        "[relay]\n"
        f"socket = {sock_path}\n"
        "socket_group =\n"
        f"relay_secret_file = {work / 'relay-secret'}\n"
        "log_level = warning\n"
        "[limits]\n"
        f"slots = {SLOTS}\n"
        f"max_pending = {MAX_PENDING}\n"
        f"connections_per_address = {2 * SLOTS + MAX_PENDING}\n"
        "join_timeout_ms = 600000\n"
        "idle_timeout_ms = 600000\n"
    )
    env = dict(os.environ, PYTHONPATH=str(SERVER), PYTHONDONTWRITEBYTECODE="1")
    proc = subprocess.Popen([sys.executable, "-m", "nereus_relay", "--config", str(work / "relay.conf")], env=env)
    try:
        deadline = time.time() + 10
        while True:
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe_sock:
                    probe_sock.connect(sock_path)
                    break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.05)
        print(f"# python {sys.version.split()[0]}, slots {SLOTS}, max_pending {MAX_PENDING}, flood {FLOOD_S} s")
        asyncio.run(probe(sock_path, proc.pid, secret))
    finally:
        proc.terminate()
        proc.wait(timeout=20)
    return 0


if __name__ == "__main__":
    sys.exit(main())
