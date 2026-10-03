# no-port-check: NereusSDR-original.
"""The client side of rendezvous/tests/memory-check.sh: loads the service
with idle stations, idle clients, connections holding an unfinished message
and stations that stop reading, and asks the server container to measure
after each step.

Not a pytest module (no test_ prefix); the check runs it in its client
container, on Ubuntu's python3 and python3-websockets.

  python3 memory_probe.py URI --sync DIR --cacert F --stations N --clients N
      --partial N --stalled N

Each step writes DIR/<step> and waits for DIR/<step>.done, which the server
container writes once it has measured.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import ssl
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "server"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

from nereus_rendezvous import identity, protocol  # noqa: E402

try:  # websockets 13 and later
    from websockets.asyncio.client import connect as _connect

    _HEADERS = "additional_headers"
except ImportError:  # Ubuntu 24.04's 10.4
    from websockets.legacy.client import connect as _connect  # type: ignore[no-redef]

    _HEADERS = "extra_headers"


class Probe:
    def __init__(self, args) -> None:
        self.args = args
        self.sync = Path(args.sync)
        self.ssl = ssl.create_default_context(cafile=args.cacert) if args.cacert else None
        self.held = []

    async def step(self, name: str) -> None:
        (self.sync / name).write_text(str(len(self.held)))
        while not (self.sync / (name + ".done")).exists():
            await asyncio.sleep(0.2)
        print(json.dumps({"step": name, "open": len(self.held)}), flush=True)

    async def connect(self):
        kwargs = {"max_size": None, "ping_interval": None, "compression": None, "open_timeout": 120}
        if self.ssl is not None:
            kwargs["ssl"] = self.ssl
        return await _connect(self.args.uri, **kwargs)

    async def recv(self, ws):
        return json.loads(await asyncio.wait_for(ws.recv(), 60))

    async def station(self, sem):
        async with sem:
            key = ec.generate_private_key(ec.SECP256R1())
            spki = identity.spki_of(key.public_key())
            sid = identity.rendezvous_id(spki)
            ws = await self.connect()
            assert (await self.recv(ws))["type"] == "hello"
            await ws.send(protocol.encode({"type": "register", "id": sid, "publicKey": identity.to_b64url(spki)}))
            nonce = identity.from_b64url((await self.recv(ws))["nonce"])
            sig = identity.sign_raw(key, identity.register_transcript(nonce))
            await ws.send(protocol.encode({"type": "prove", "signature": identity.to_b64url(sig)}))
            assert (await self.recv(ws))["type"] == "registered"
            self.held.append(ws)
            return ws, sid

    async def client(self, sem):
        async with sem:
            ws = await self.connect()
            hello = await self.recv(ws)
            assert hello["type"] == "hello"
            self.held.append(ws)
            return ws, hello

    async def partial(self, sem):
        """A connection that sends all but the last byte of a 131072-byte
        text frame (the cap of section 2) and then nothing."""
        ws, _ = await self.client(sem)
        length = protocol.MAX_MESSAGE_BYTES
        mask = os.urandom(4)
        header = bytes([0x81, 0x80 | 127]) + struct.pack("!Q", length) + mask
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(b"a" * (length - 1)))
        ws.transport.write(header + payload)
        return ws

    async def run(self) -> int:
        sem = asyncio.Semaphore(64)
        await self.step("baseline")
        started = time.monotonic()
        stations = await asyncio.gather(*(self.station(sem) for _ in range(self.args.stations)))
        print(json.dumps({"registered": len(stations), "seconds": round(time.monotonic() - started, 1)}), flush=True)
        await self.step("stations")
        await asyncio.gather(*(self.client(sem) for _ in range(self.args.clients)))
        await self.step("clients")
        await asyncio.gather(*(self.partial(sem) for _ in range(self.args.partial)))
        await self.step("partial")
        # Stations that stop reading while introductions carrying a 60000-byte
        # offer are sent to them, until everything between the service's
        # queue and the peer's socket is full.
        stalled = stations[: self.args.stalled]
        for ws, _ in stalled:
            ws.transport.pause_reading()
        offer = "v=0\r\n" + "a" * 60000
        for ws, sid in stalled:
            for _ in range(3):
                cl, hello = await self.client(sem)
                device = ec.generate_private_key(ec.SECP256R1())
                n = identity.from_b64url(hello["nonce"])
                sig = identity.sign_raw(device, identity.introduce_transcript(sid, n))
                await cl.send(protocol.encode({
                    "type": "introduce",
                    "id": sid,
                    "device": identity.to_b64url(identity.fingerprint(identity.spki_of(device.public_key()))),
                    "deviceSignature": identity.to_b64url(sig),
                    "offer": offer,
                }))
        await asyncio.sleep(3)
        await self.step("stalled")
        return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="memory_probe")
    parser.add_argument("uri")
    parser.add_argument("--sync", required=True)
    parser.add_argument("--cacert")
    parser.add_argument("--stations", type=int, default=2000)
    parser.add_argument("--clients", type=int, default=256)
    parser.add_argument("--partial", type=int, default=500)
    parser.add_argument("--stalled", type=int, default=100)
    args = parser.parse_args(argv)
    return asyncio.run(Probe(args).run())


if __name__ == "__main__":
    sys.exit(main())
