# OpenHPSDR Protocol 2

## Status: Reference placeholder

## Overview

Protocol 2 is the newer OpenHPSDR protocol used by Orion MkII, Saturn (ANAN-G2),
and newer boards. It uses UDP-only communication on multiple dedicated ports
(commands to ports 1024-1027, per-DDC I/Q data on ports 1035-1041).

**CORRECTION (Phase 3A):** Originally assumed to be TCP+UDP, confirmed as
UDP-only by pcap analysis and Thetis ChannelMaster/network.c source.

## Key Characteristics

- **Command channel:** UDP packets to ports 1024-1027
- **Data channel:** UDP with separate streams per DDC (ports 1035-1041)
- **Discovery:** UDP broadcast/multicast on port 1024
- **Independent receiver streams:** Each receiver has its own data stream
- **Wider bandwidth support:** Higher sample rates possible

## Advantages over Protocol 1

- Structured command packets with dedicated ports (vs multiplexed C&C bytes)
- Independent receiver data streams (not multiplexed in EP6 frames)
- Better scalability for multiple receivers
- Per-DDC sample rate control

## Official Specification

See: https://openhpsdr.org/wiki/index.php?title=Protocol_2
