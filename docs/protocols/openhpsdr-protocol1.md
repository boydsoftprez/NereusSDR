# OpenHPSDR Protocol 1

## Status: Reference

Protocol 1 is the original OpenHPSDR protocol used by Metis, Hermes, Angelia,
Orion, and Hermes Lite 2 boards. UDP-only on port 1024, 1032-byte Metis
frames, 24-bit big-endian interleaved I/Q.

## Primary Reference

For byte-level layouts, command/status decode, cadence, and HL2-specific
quirks, see the annotated capture reference:

**[openhpsdr-protocol1-capture-reference.md](openhpsdr-protocol1-capture-reference.md)**

That document is derived from a live HL2↔Thetis capture and is the
authoritative source for NereusSDR Phase 3L (P1 implementation).

## Quick Summary

- **Transport:** UDP only, port 1024
- **Frame size:** 1032 bytes (Metis frame: 8-byte header + two 512-byte USB frames)
- **Header:** `EF FE 01 <endpoint> <seq32_big_endian>` — endpoint `0x06` for EP6 (radio→host data), `0x02` for EP2 (host→radio commands)
- **I/Q format (EP6):** 24-bit big-endian signed, interleaved I/Q + 16-bit BE mic
- **Samples per USB frame:** `spr = 504 / (6*nddc + 2)` (from `networkproto1.c:358`)
- **Control:** 5 C&C bytes per USB frame — 3-byte sync `7F 7F 7F`, C0 round-robin status/command index, C1..C4 payload
- **Discovery:** UDP broadcast to port 1024, `EF FE 02 ...`

## Thetis P1 Source Map

Most P1 byte-level code lives in the unmanaged ChannelMaster C source, not C#:

- **Discovery:** `Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs`
- **Metis start/stop, EP2 build, EP6 parse, seq#:** `Project Files/Source/ChannelMaster/networkproto1.c`
- **Metis socket setup:** `Project Files/Source/ChannelMaster/network.c`
- **C# P/Invoke surface:** `Project Files/Source/Console/HPSDR/NetworkIOImports.cs`

## Official Specification

See: https://openhpsdr.org/wiki/index.php?title=Protocol_1
