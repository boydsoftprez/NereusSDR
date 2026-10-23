# NereusSDR Roundtable

Roundtable groups stations into rooms by frequency, shows each station's
own S-meter reading of whoever is talking, works out turns, and carries
short messages about each frequency. Stations reach it over a WebSocket;
anyone can read the live rooms. Its wire is
[the Roundtable document](../docs/architecture/2026-10-23-roundtable-v1.md),
and that document is the authority for every protocol detail.

NereusSDR uses `roundtable.nereussdr.com`. Anyone can run their own server;
every default is nereussdr.com's, and each one is a setting.

- `server/`: the server, a small Python program (`nereus_roundtable`).
  Caddy sits in front of it for TLS and forwards by path to its two
  loopback listeners: WebSockets on port 8720 (`/v1/station`, `/v1/live`)
  and HTTP on port 8721 (the read API and the web pages).
  `roundtable.conf.sample` lists every setting with its default.
- `conformance/v1/`: the fixtures the server, the Core and other station
  software all run: crypto vectors, one message per control fixture,
  scripted sessions and scripted rooms (the document's section 11).
- `tests/`: the server's tests, its conformance runner among them.

The server keeps no client address in any file or log. Addresses are held
in memory only, to count connections per network, and a log line names at
most the first 6 characters of a station id.

## Running the tests

The server needs Python 3.9 or later with `websockets` (10.4 or later) and
`cryptography`, and the tests need `pytest`. From the repository root:

```
python3 -m pytest roundtable/tests -q -p no:cacheprovider
```

The authoritative run is Ubuntu 24.04's Python 3.12 with apt's
`python3-websockets`, `python3-cryptography` and `python3-pytest`, which is
what CI's `roundtable` job installs. The tests also run on macOS's
`/usr/bin/python3` (3.9) with websockets 15. `server/nereus_roundtable/transport.py`
holds every difference between the two websockets APIs.

The tests need no network beyond loopback and write nothing outside
pytest's temporary directories. The conformance runner plays every
connection of a session fixture against a real server on loopback, with a
clock that moves only when a fixture says so.

## Running the server

```
cd roundtable/server
python3 -m nereus_roundtable --config /etc/nereus-roundtable/roundtable.conf
```

An unknown section or key, or a value the server cannot use, stops it at
start with exit code 2 and a one-line message.
