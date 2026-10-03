# Remote Station Identity, Pairing and Rendezvous: Design

**Status:** Design, pending approval. Not yet planned or implemented.
**Date:** 2026-08-02
**Author:** J.J. Boyd (KG4VCF), with AI-assisted drafting via Anthropic Claude Code

> **Parent document:**
> [2026-07-28-remote-daemon-architecture-design.md](2026-07-28-remote-daemon-architecture-design.md).
> That document covers the daemon, the split point, state synchronisation, the
> wire, codecs, and the transport ladder. It resolves identity in a single
> sentence at section 7.1: "the token distribution mechanism must be specified
> before R2". **This document is that specification**, and it is larger than one
> sentence implies. It also corrects the parent's carrier-NAT assumption at
> section 10.2 (see section 9 below).

---

## 1. Goal

Let an operator connect a NereusSDR client to their own `nereusd` station from
anywhere, with no account to create, no service that can lock them out, and a
setup process short enough that a first-time user does not give up.

Three requirements, in priority order:

1. **Not centralised.** No component of the normal operating path may depend on
   infrastructure the operator does not control. This restates the parent
   document's section 10.1 governing principle and extends it to identity.
2. **Easy.** A fresh Pi on the operator's own bench should be usable in one
   click. Nothing that expires while the operator walks to another room.
3. **Reasonably secure.** Proportionate to a device that keys a transmitter and
   is reachable from the internet. Not proportionate to a bank.

**Attribution:** everything specified here is NereusSDR-original. There is no
Thetis, AetherSDR, freedv-gui or WDSP equivalent to port from, so no upstream
header and no `THETIS-PROVENANCE.md` row applies. This is consistent with parent
section 11, which already classifies the session layer, state mirror and
transport as NereusSDR-original. Third-party libraries named in section 10.1 are
dependencies to be licence-reviewed, not ports.

**Non-goal:** guest sessions. The parent document defers them (section 16)
while requiring that every connection carry an identity and a role from day
one. (Amended 2026-09-25: this line first also ruled out multi-operator
access; up to four devices now share a Core, each keeping the role of owner,
per the several-devices design,
[2026-09-24-several-devices-on-one-core-design.md](2026-09-24-several-devices-on-one-core-design.md),
section 11.) Nothing here blocks that later work; adding a
guest role becomes new permission checks, not a redesign.

---

## 2. Decisions

| Decision | Choice |
| --- | --- |
| What identity is | An asymmetric key pair, generated on first run, held by the machine. Not an account, not a name |
| Human-readable name | Callsign plus a free-form suffix, e.g. `KG4VCF/shack`. A local label only |
| Where the label lives | Client-side and on the daemon. **Never sent to the rendezvous** |
| Accounts, usernames, passwords | None. Explicitly considered and rejected: they solve none of the hard problems and centralise the one thing the operator cares about |
| Station visibility | Private. The rendezvous cannot enumerate stations or confirm whether a given callsign exists |
| Pairing on the LAN | One click, no code, no fingerprint comparison |
| Pairing remotely | Short human-speakable code, protected so a hostile rendezvous cannot use or learn it |
| Pairing by hand | Copy the key over SSH. Always available, never required |
| Pairing window | Open while the station is unclaimed, with **no timer**. Closes permanently on first pair |
| Reopening the window | From the daemon console, or from an already-paired device |
| Paired devices | A list, each with its own key, name and last-seen time |
| Revocation | Immediate, from any paired device or the console. Drops a live session |
| Lockout recovery | A console command resets the station to unclaimed. Physical access always wins |
| Concurrent sessions | Up to four; a fifth asks to take one's place (amended 2026-09-25 by the several-devices design, section 11) |
| Rendezvous roles | Introduction, reflexive address, relay. One binary, one host |
| Rendezvous implementation | Off-the-shelf `coturn` for STUN and TURN. We write only the small signalling service |
| Self-hosting | Supported and documented, for **both** introduction and relay |
| Server list | Ordered, client-side. The operator's own server first, the default behind it |
| Relay posture | Two rungs (DTLS/UDP 443 preferred, TLS/TCP 443 as the guaranteed floor), raced rather than serial, switchable off, short-lived credentials, rate-limited, media datagrams capped at 1000 bytes |
| Manual address entry | First class. Hostname, IPv4, or bracketed IPv6, with optional port |
| Station discovery on the LAN | New, multicast on both IP stacks, daemon announces and client listens |
| Radio discovery | Unchanged. Same `RadioDiscovery` broadcast, now running on the daemon host |
| Scrambled-name rotation | Deferred. Fixed for v1; self-hosting is the answer for operators who need more |

---

## 3. Identity

### 3.1 The key is the station

`nereusd` generates a key pair on first run and never releases the private half.
That key is the station's identity. The client generates its own key pair for
the same reason, so a device is identified by what it holds rather than by what
it claims.

This is the model SSH, WireGuard and Syncthing arrived at independently. It is
chosen here for one specific property: there is no third party who can grant,
revoke, or withhold an operator's access to their own radio.

### 3.2 The cost, stated plainly

The private key on the daemon **is** the station. Losing it, most realistically
by an SD card failure, means every paired device must pair again. There is no
account to recover from, by design.

Mitigations, both required:

- The daemon writes its key to a single file with a documented path, so backing
  it up is one copy operation.
- First-run output and the Setup page both prompt for that backup. A user who
  loses a card and was never told is a support failure, not a user error.

### 3.3 Naming

The label is `<callsign>/<suffix>`, for example `KG4VCF/shack`. The convention
is deliberately the one hams already read on sight from packet radio and APRS,
where a suffix distinguishes stations under one call.

Rules:

- Callsign portion defaults to the callsign already stored in `AppSettings` for
  spotting and reporting. **Setup asks the operator for nothing.**
- Suffix is free-form, maximum 32 characters, from `[A-Za-z0-9_-]`.
- Comparison is case-insensitive, so `KG4VCF/Shack` and `kg4vcf/shack` cannot
  both exist and be mistaken for each other. Display preserves what was typed.
- Empty suffix is legal and displays as the bare callsign.

**The label is not an identifier.** It is not checked, not unique, and never
transmitted to the rendezvous. Two operators may both label a station
`KG4VCF/shack` with no consequence whatsoever, because nothing matches on it.
This dissolves the callsign-squatting problem rather than policing it.

---

## 4. Pairing

### 4.1 One ceremony

Fingerprint comparison is deliberately **not** used. It is a ritual users click
through without reading, so it costs friction and buys close to nothing. Where a
check is needed, the short code provides it.

### 4.2 On the local network: one click

1. The daemon announces itself (section 6).
2. The client lists it under "Stations on this network", marked unclaimed.
3. The operator clicks Pair. Keys are exchanged. Done.

**Accepted risk, recorded so it is a choice and not an oversight.** Another
device on the same network could claim a brand-new station before the operator
does. It applies only to a station that has never been paired, and it requires
presence on the operator's own network. A configuration key forces the code path
always, for operators on shared or untrusted networks. The default is one click,
because the alternative taxes every user forever against a case most never meet.

### 4.3 Remotely: a short code

The daemon displays a code such as `7-anvil-harbor` on its console, in its log,
and on a local status page. The operator types it into the client from anywhere.

Requirements:

- **Safe against a hostile rendezvous.** The code must be usable for
  authenticated key exchange without the server learning it or being able to
  impersonate either end. This is a password-authenticated key exchange, the
  mechanism Magic Wormhole uses for the same purpose.
- Single use. Consumed on success, burned on a wrong guess.
- Attempts rate-limited, so a short code cannot be ground down.
- Speakable over the phone. Digit plus two words, not hexadecimal.
- Displayed continuously while unclaimed, so there is nothing to miss.

This adds a cryptographic dependency (section 10).

### 4.4 By hand

The operator copies the daemon's public key over SSH. No rendezvous, no code, no
discovery. It exists so the operator is never without an option, and it is not
part of the documented normal flow.

### 4.5 The pairing window

A daemon accepts pairing only while its pairing window is open.

- **Open by default while unclaimed, with no timer.** An unpaired station holds
  nothing worth taking, and the code still gates the remote path. A timer here
  would produce the exact failure this design is trying to avoid: setup silently
  expiring while the operator fetches a laptop.
- **Closes permanently on the first successful pair.**
- **Reopens** from the daemon console, or from an already-paired device.

**Note (2026-09-24, the iPhone plan's Part C fix wave).** "No timer" still
holds for the unclaimed window, but it now has an attempt ceiling: five
burned codes in a row close any open window, and only the console (or, on a
claimed Core, a paired device) reopens it. On an unclaimed Core that means
the console, so physical access decides. A reopened window also closes by
itself 10 minutes after it opened. Reopening starts afresh, with no failures
counted and no wait. A paired device cannot revoke the last device while no
token is active, so the window never reopens by itself after the first pair.
The link document's section 3.6 carries the numbers.

**Note (2026-09-26, the operator's ruling on the iPhone plan's Task 27
review, item I5).** The ceiling counts only codes burned on a direct
connection. Codes burned through the remote access service's mailbox never
close the window: an unclaimed Core holds the lowest free nameplate there,
so anyone could otherwise shut a fresh Core's pairing from the internet in
about a minute. Instead, five of them in a row pause pairing through the
service: 1 minute, then twice as long each time it is hit again with no
pairing in between, at most 60 minutes. While paused, a pairing through the
service is refused before it takes a code, so it burns nothing. Pairing on
a direct connection (the home network) stays open throughout. A pairing, or
reopening the window, ends the pause and starts the ladder over. A burned
code still rotates, after the same wait either way.

That last route is what makes adding a second device pleasant: the operator adds
a phone from an already-paired laptop and never approaches the radio.

---

## 5. The rendezvous

### 5.1 Three jobs, one host

Per parent section 10.3: introduction, reflexive address reporting, and relay.
`coturn` provides STUN and TURN, is packaged in every distribution, and is what
the rest of the industry uses. **We write only the signalling service**, and the
existing Qt WebSocket infrastructure from the TCI work is the natural base.

Correcting an impression left earlier in design discussion: the rendezvous is
not a large new product. The novel surface is identity binding and operations,
not traversal.

### 5.2 What it can and cannot see

The daemon registers under a **scrambled name derived from its public key**, not
from its label. A client holding the daemon's key derives the same value and
asks where it is. The server matches opaque strings. The derivation is defined
in [2026-09-23-rendezvous-v1.md](2026-09-23-rendezvous-v1.md), section 4.2.

| The rendezvous sees | The rendezvous cannot see |
| --- | --- |
| IP addresses of both ends | Callsign or label |
| That an opaque identifier is online | Which operator or station it belongs to |
| Traffic volume and timing when relaying | Any relayed content, which is encrypted end to end |
| The SHA-256 fingerprint of the Core's TLS certificate, in the `a=fingerprint` line of every answer to an introduction (the control connection presents it in DTLS), and of each device's one-off DTLS certificate in its offer. The Core's direct listener serves the same certificate to anyone who reaches it. | The Core's identity key, a device's key pair, or anything a fingerprint could be used to sign or decrypt |

It cannot enumerate stations, and it cannot answer "is KG4VCF on the air".

The certificate fingerprint is a stable value per Core, so like the scrambled
name it lets the service correlate one Core's sessions over time; it adds
nothing the IP addresses and the fixed identifier do not already give it, and a
device never trusts it from the SDP alone (it checks the certificate the DTLS
handshake carried against the Core's signed binding, link document section 20).

**Rotation is deferred.** Rotating the scrambled name would defeat long-term
correlation by an observer, at the cost of clock agreement, drift handling, and
boundary lookups. For v1 the value is fixed. The stronger answer for an operator
who needs more is self-hosting, which is supported.

### 5.3 Not a single point of failure

Three mechanisms, all required:

- **Cached address first.** The client tries where the station was last seen
  before contacting anyone. Normal reconnects never touch the rendezvous.
- **Ordered server list.** The operator's own server first, the default behind
  it. One list covers introduction and relay.
- **Established sessions are unaffected by an outage**, because the rendezvous
  was never in the media path. Only a first-time connection from a new network
  with no cached address is blocked.

### 5.4 Relay

**Relay carries every packet.** Audio, spectrum, meters, control, microphone,
in both directions. The content is opaque to it, but the bytes all pass through.

**Two relay transports, not one.** These have materially different latency
behaviour and different failure conditions, and collapsing them into a single
"relay" tier hides the most consequential transport decision in the project:

- **DTLS over UDP 443**, media datagrams capped at 1000 bytes. Preferred.
  Preserves datagram semantics, so a single loss costs one frame rather than
  stalling the stream.
- **TLS over TCP 443.** The guaranteed floor. Mandatory, always available, but
  not the default. Prefer TLS to plain TCP for a mechanical reason Jitsi
  documents: TURN over TLS performs a real TLS handshake, while ICE over TCP
  uses a hard-coded handshake that some firewalls recognise and drop.

#### Sizing

Egress counts both directions, station-bound microphone included:

| Session shape | Relay egress | 3 hours a day | Left receiving 24/7 |
| --- | --- | --- | --- |
| One pan, receive only | ~145 kbit/s | ~6 GB per month | **~47 GB per month** |
| Four pans plus microphone | ~520 kbit/s | ~21 GB per month | **~168 GB per month** |

**State the usage assumption inline, because it swings the answer by more than
an order of magnitude.** At three hours a day a 1 to 2 TB VPS carries on the
order of a hundred regular users. A station left receiving continuously, which
is entirely normal in this hobby, consumes 47 to 168 GB per month by itself,
which is three to four relayed users per terabyte. The relay therefore needs an
idle-disconnect policy, and the sizing claim must never be quoted without its
assumption attached.

Idle registered stations that are not carrying a session cost only their
check-in.

#### Policy

- **Last resort in preference, raced in practice.** Attempt direct paths and
  both relay transports **concurrently** and take the first that succeeds, then
  keep upgrading in the background. A strictly serial ladder requires predicting
  whether a given carrier permits UDP, which is exactly the prediction we cannot
  make. NetBird documents racing both relay protocols simultaneously and using
  whichever wins; Tailscale starts relayed and upgrades to direct, which is also
  the right feel for a radio because audio arrives immediately and the path
  silently improves. The contrast case is instructive: ZeroTier's own
  documentation admits its agent can take "a few minutes" to realise it needs to
  relay, which here would be minutes of dead air.
- **Path switches must be seamless, and that is not free.** Racing and
  background upgrade mean the path changes mid-session. Tailscale can do this
  invisibly because it is a packet tunnel with its own sequencing; our
  application-level media stream needs dual-receive across a switch window,
  timestamp-based deduplication, and a defined jitter-buffer behaviour across
  the discontinuity. Without that, every upgrade is an audible glitch.
- **Switchable off** for operators who want direct or nothing.
- **Short-lived credentials**, minted at connect time for an already-paired
  pair. A relay with static credentials is an open relay.
- **Per-session and global rate limits.** Syncthing's `strelaysrv` ships
  `-per-session-rate` and `-global-rate` as precedent. The relay is the one
  component where a misbehaving or malicious client can impose unbounded cost on
  whoever is hosting it, and section 5.5 requires that outsiders host it.
- **Bounded send buffer with application-level staleness drop.** When an uplink
  momentarily dips below the media rate, a TCP socket queues silently rather
  than dropping, and latency grows without bound until it recovers. This is the
  failure that produces "the audio went thirty seconds behind and never caught
  up". Drop stale frames rather than letting the socket buffer absorb them.
- **`TCP_NODELAY` on every relay leg.** Classic, trivial, and exactly the kind
  of omission that silently costs 40 ms.
- **Media datagrams capped at 1000 bytes.** See section 9.3.

### 5.5 Self-hosting

Both introduction and relay must be self-hostable, packaged and documented for
someone who is not the author. This is not a nicety: for a station behind a
carrier that forbids direct connections, running a small server on a cheap VPS
is the only path that is both reliable and independent, and it also removes that
operator from the default server's bandwidth bill.

Deliverables: a package or container image, a sample configuration, and
documentation covering DNS, certificates, and firewall rules.

---

## 6. Discovery on the local network

Two discoveries, independent of each other.

**Radio discovery is unchanged.** The existing `RadioDiscovery` UDP broadcast on
port 1024 keeps its behaviour and its code. The only change is that it now runs
on the daemon host, because that is what is attached to the radio. Parent
section 10.6 already records this as unaffected.

**Station discovery is new.** The daemon announces itself, the client listens.
Used both for one-click pairing and, permanently afterwards, for connecting on
the LAN without an address.

- **Multicast, not broadcast**, because it must work over IPv6, which parent
  section 10.6 requires from the start.
- Mirrors the existing radio-discovery pattern rather than adopting mDNS. That
  keeps the shape familiar and adds no Avahi or Bonjour dependency and no
  per-platform service. The cost is no free `.local` name, which is acceptable
  because discovery hands the client an address directly.
- **One way.** The daemon announces, the client only listens. A client never
  advertises itself on a network it does not own.

The connect screen presents three groups, and the operator is never asked to
understand the difference:

- **Radios on this network:** unchanged local direct mode
- **Stations on this network:** daemons found on the LAN
- **Your stations:** every paired station, reachable from anywhere

---

## 7. Devices and revocation

Each paired device holds its own key. The daemon keeps a list.

| Field | Note |
| --- | --- |
| Public key | The identity |
| Name | Defaults to the machine name, so nothing is typed |
| Paired at | |
| Last seen | |

- **Revocation is immediate**, from any paired device or the console. A revoked
  device that is connected at that moment is dropped mid-session.
- **Multiple devices may be paired; up to four may be connected.** *(Amended
  2026-09-25 by the several-devices design, section 11; this line first said
  one may be connected, under parent section 7.1's preemption rule.)* A
  device's own new connection replaces its stale one at once, which is what
  lets an operator reconnect from a phone after a link drop.
- **Recovery from total loss:** a console command returns the station to
  unclaimed. There is no account, so physical access is the recovery path.

---

## 8. Manual address entry

First class, never required, never contacting any server.

Accepted forms, everywhere an address is taken:

- Hostname, `shack.example.net`
- IPv4 literal
- IPv6 literal in brackets, `[2001:db8::1]`
- Optional `:port`, with a documented default

Behaviour:

- Hostnames resolve with the IPv6-preferring staggered approach in parent
  section 10.6.
- **Identity still applies.** A mistyped address that reaches someone else's
  machine fails to authenticate rather than connecting. This is what makes
  manual entry safe to expose.
- Covers the port-forwarded case, the routable-IPv6 case, the LAN case, and an
  operator-run VPN. NereusSDR does not require a VPN and does not obstruct one.

**Manual NAT to NAT, with no server at all.** The daemon surfaces its own
reflexive address, the operator tells each end about the other, and both punch
outward. Documented honestly: fiddly, requires both ends within roughly the same
half-minute, and fails against endpoint-dependent mapping. It exists so the
operator is never without options. Self-hosting a rendezvous is the better
answer to the same problem.

---

## 9. Carrier NAT, and a correction to the parent design

Researched 2026-08-02 by a six-dimension literature sweep with adversarial
verification of every claim. Sourcing is labelled throughout, because the honest
finding here is that **the most decision-relevant fact has never been measured
by anyone**, and an over-stated evidence base invites a later reviewer to
discount this section wholesale, including the parts that are solid.

### 9.0 The mechanism, which is the part most often gotten wrong

**Relaying is what defeats CGNAT. TLS on 443 is what defeats port-blocking
firewalls and deep packet inspection.** These are different problems with
different solutions and conflating them produces the wrong ladder.

A relay learns the client's mapping from the packet it receives, so it does not
care whether that mapping is endpoint-independent or symmetric. RFC 8656
justifies TURN's TCP transport by firewalls that "block UDP entirely" and does
not mention CGNAT anywhere. No source located documents TURN over TLS 443
specifically beating carrier NAT.

The practical consequence: 443 belongs in the design as the **guaranteed floor**
rather than the default, and a UDP relay should be attempted above it.

### 9.1 What the parent design gets wrong

**Parent section 10.2's causal argument is self-refuting and must be deleted.**
It reasons that most carrier NAT is endpoint-independent "because symmetric
behaviour breaks games and VoIP". Third-party reviews put T-Mobile Home Internet
at Xbox Strict and PlayStation Type 3. Gaming **is** broken on these connections
and the carrier shipped it anyway. Replace the reasoning with the measurement:
Richter et al. (ACM IMC 2016) found roughly 40% symmetric and 20% full cone
among CGN-positive cellular ASes, explicitly noting major US cellular networks
deploying symmetric mapping. Cellular is a coin flip, not a safe assumption in
either direction.

**Parent section 10.2's "structural luck" paragraph must go too.** It argues
that CGNAT carriers hand out IPv6 for the same reason they impose CGNAT, so the
affected population is disproportionately likely to succeed at tier 1. T-Mobile
Home Internet hands the gateway a single /64 with no DHCPv6 prefix delegation
(independently observed on MikroTik RouterOS in February 2024 and on pfSense in
2024). Operational consequence worth stating in the operator documentation: **a
Pi behind a second router gets no routable IPv6 at all** and must sit directly
on the gateway LAN.

### 9.2 What is established, and what is not

**Established, high confidence:**

- **T-Mobile Home Internet offers no inbound path.** Double NAT (gateway NATs
  the LAN, then carrier CGNAT), no bridge or IP-passthrough mode, no port
  forwarding, no UPnP, no NAT-PMP, no DMZ, no firewall controls. Verified across
  three gateway generations, KVD21 through G4AR/G4SE to the current G5AR/G5SE,
  against T-Mobile's own support pages. No firmware refresh has changed it.
- **T-Mobile cellular is a different product and is the easy case.** It is
  IPv6-only with 464XLAT (RFC 6877, co-authored by Cameron Byrne at T-Mobile
  US). On the IPv6 path there is no carrier NAT at all. A client dialling out
  from cellular to an ordinary-internet station is close to a solved problem.
  Caveat recorded: the primary 464XLAT evidence is 2013 to 2016 and no 2024-2026
  confirmation was located, so treat as high-plausibility and verify in ten
  minutes rather than assuming.

**NOT established, and previously asserted here in error:**

- **Nobody has ever publicly measured T-Mobile Home Internet's NAT mapping
  behaviour.** Not once. Every "symmetric" assertion traceable in the literature
  is either about mobile CGNAT generically or inferred from console NAT-type
  labels, which cannot measure the property. The nearest thing to direct
  evidence, a Roon engineer's 2022 report that T-Mobile "doesn't preserve port
  assignments", describes port **rewriting**, which is a different property from
  mapping stability; ICE reads the rewritten port off the STUN reflexive
  candidate rather than predicting it, and an endpoint-independent NAT is free
  to rewrite ports and remain perfectly punchable. **Strip that inference and
  the corpus contains zero evidence that this network is symmetric.** Always
  attempt ICE, never depend on it, and measure before writing it off.
- **That TMHI filters unsolicited inbound IPv6** is reported but unverified. The
  commonly cited source is a T-Mobile Community page that renders as a marketing
  shell to any fetch and whose title concerns a Netgear LTE router rather than
  the 5G gateway. Treat as reported, unverified, and testable.

### 9.3 The finding that changes the answer

**"T-Mobile blocks UDP" is most consistent with an MTU artifact rather than a
policy block.**

Every protocol reported failing on this carrier is a large-datagram protocol:
QUIC and HTTP/3, which RFC 9000 section 14 requires to expand Initial packets to
at least 1200 bytes and forbids on paths that cannot carry that; WireGuard at
its 1420 default; 4D's QUIC layer. TCP works because MSS clamping is universal.
Affected users routinely fix VPNs by dropping MTU to 1200 to 1400.

**Our media frames are 120 bytes of Opus and roughly 433 bytes of reduced
spectrum**, an order of magnitude below any plausible floor. A DTLS over UDP
relay with capped datagrams should therefore survive precisely where QUIC
cannot.

Consequences:

1. **Cap all media datagrams at 1000 bytes** and never assume a path MTU of 1200
   or more. This belongs in the parent's section 10.4.
2. **Do not pre-emptively concede TCP's tail-latency penalty.** Giving up
   datagram semantics buys nothing on a path where UDP would have worked.
3. **QUIC and MASQUE are disqualified as the primary media transport if and only
   if the MTU sweep shows the path dropping datagrams below roughly 1250 bytes.**
   State it as a conditional, not a standing rejection: RFC 9221 gives QUIC
   unreliable DATAGRAM frames with no head-of-line blocking, post-handshake
   datagrams are not bound by the Initial floor, it is wire-indistinguishable
   from HTTP/3 to DPI, and it brings connection migration, which addresses a
   cellular client changing towers mid-QSO. The MTU hypothesis is well supported
   but unverified, and a permanent rejection cannot rest on a hypothesis we are
   simultaneously scheduling an experiment to test.

### 9.4 Prior art, which is more useful than the carrier literature

**FlexRadio SmartLink performs no hole punching at all.** Its documented and
supported traversal is UPnP or manual port forwarding, full stop. Its servers
carry no media: FlexRadio staff state "the Flex servers only initiate the
connection but are not part of the communications path at all". Community
members formally asked FlexRadio to add STUN in April 2022 and to add ICE in
October 2024, the latter noting that SmartLink "relies on the ham being able to
open ports in the firewall". Neither drew a commitment.

**Therefore SmartLink's failure on T-Mobile is not evidence that hole punching
fails there.** It is evidence that a product which never attempted hole punching
cannot work when inbound ports are unavailable. ICE remains untested by this
prior art and must be judged on its own merits. (Strictly this is a strong
inference from documented absence rather than a packet capture.)

**FlexRadio's own workaround is architecturally TURN.** Their staff-authored,
explicitly unsupported escape route says "both the radio (server) and the user
(client) must make their initial connections outbound to a common server in the
cloud". The vendor with the strongest possible commercial incentive to avoid
paying relay bandwidth concluded that relay is the fix. That is meaningful
independent convergence.

**Bandwidth comparison, corrected.** SmartLink's published minimum is 500 kbit/s
up at the radio and 500 kbit/s down at the client, and SmartLink is itself a
reduced-spectrum-plus-compressed-audio design rather than raw IQ. Our 145 to 520
kbit/s sits in the **same class**, not thirty times below it. The thirty-times
figure holds only against LAN-mode SmartSDR with DAX. SmartLink is our closest
peer and therefore our most useful precedent, not a heavyweight strawman.

**The rest of the field shows one pattern.** Icom RS-BA1 needs three inbound UDP
ports forwarded at the station (50001 to 50003) while the client needs none.
Kenwood KNS needs TCP 50000 plus UDP 33550 and states outright that a global IP
address is required. RemoteHams RCForb needs TCP 843 and 4524 to 4525. Every ham
remote product predating roughly 2020 assumes an inbound port, and every one of
them fails on this carrier.

**RemoteTX is the counterexample and our closest architectural twin:** a
Raspberry Pi at the station dialling outbound to cloud servers, no port
forwarding, no dynamic DNS, explicitly advertised as CGNAT-compatible, at a
claimed 80 kbit/s. It proves the outbound-dialling model is commercially viable
in this hobby. Its transport is entirely undisclosed, so it must **not** be
cited as evidence that ICE or WebRTC works behind T-Mobile.

**Copy from SmartLink:** the broker shape (authentication, directory, and
reachability test, with media staying off the servers) and the green/red
reachability indicator, which maps directly onto the parent's section 10.7
diagnostics. **Avoid:** rendezvous with no relay tier, which is the entire
reason a Flex behind this carrier is unreachable.

**Nobody in amateur radio documents TURN over TLS 443, or publishes any ICE,
STUN or TURN configuration at all.** No prior art to copy, and equally no field
reports warning us off.

### 9.5 What this changes in our design

1. **Relay is a first-class path, not a rare tier**, and it is two rungs rather
   than one (section 5.4).
2. **Media datagrams capped at 1000 bytes** (section 9.3).
3. **The rendezvous, the STUN responder, and every relay must be dual-stack with
   published AAAA records.** The one documented real-world T-Mobile ICE failure
   (discuss-webrtc, 2018) was caused by IPv4-only TURN infrastructure reporting
   "Server and local address families are not compatible", compounded by the
   client discarding the 464XLAT CLAT address (192.0.0.4, RFC 7335) as not
   internet-capable while IPv4 was in fact working. Both faults were
   self-inflicted and cheap to avoid. This is the highest-probability way we
   reproduce a known bug, and the parent's section 10.6 is entirely client-side
   and does not prevent it.
4. **Prefer IPv6 explicitly on the client side.** RIPE Atlas measurement
   (Boswell et al., 224 probes across 43 networks, February 2024) puts NAT64
   paths roughly 23% longer with about 17% higher RTT than native. That lands on
   CW, not on SSB.
5. **Keepalive cadence is load-bearing and must be measured.** Carrier NAT
   mappings expire far faster than home-router ones: Richter measured a 65 s
   median for cellular CGN UDP with a 10 s floor, from 2014-15 data using a tool
   retired in 2019, so even that is stale. Note the mechanism precisely, because
   the parent's section 12.1 has it backwards: RFC 8445 keepalives default to a
   15 s Tr and MUST NOT be lower, which is slower than a 10 s floor, so **RFC
   7675 consent freshness at 4 to 6 s is what actually holds a hard NAT open**.
   Consent's roughly 30 second figure is the expiry parameter, not the probe
   rate. Also record that a continuously streaming session never triggers an
   idle keepalive at all: mapping expiry binds the control channel, a paused
   session, and reconnect-after-sleep, not the media path.
6. **Replace the 100.64.0.0/10 range check** in the parent's section 10.7 with a
   probe-based test (reflexive address differs from every local interface
   address), keeping the range check only as a supplementary hint. No primary
   source confirms this carrier's WAN address is in 100.64/10; every such
   attribution traces to VPN-affiliate content restating the generic RFC 6598
   rule. A range check that silently fails to fire would make the diagnostics
   page confidently wrong for exactly the population it exists to serve.
7. **Add MTU and outbound-UDP probes to the diagnostics**, so the page can say
   something no competitor's does: "your path drops UDP datagrams above N bytes".
8. **Direction asymmetry belongs in operator documentation.** A client on a
   restrictive carrier reaching a station on ordinary internet is easy. A station
   behind a restrictive carrier is hard. **The station's connection matters far
   more than the client's.**
9. **Tethered client operation is degraded, not broken.** T-Mobile's published
   policy prioritises on-device data over tethered data under congestion. At
   40 kbit/s up this will not starve us, but it inflates jitter and tail latency:
   an SSB annoyance and a CW hazard.

### 9.6 CW, stated more carefully than before

An earlier revision of this section implied TCP relay disqualifies CW outright.
That was too categorical, and there is directly relevant prior art.

**RemoteRig already solves remote CW keying** with an adjustable jitter buffer
for the keyer at the radio end, an operator-set key delay in milliseconds whose
recommended starting value is the ping time between the ends, and **sidetone
generated locally in the control box** rather than waiting for the radio's.
Keying events are a handful of bytes at a few tens of events per second, so
de-jittering them converts CW from a jitter problem into a fixed-offset problem.

Three consequences:

- The head-of-line-blocking argument disqualifies TCP for CW only when a stall
  exceeds the keying buffer depth. At a 200 ms buffer a `TCP_RTO_MIN` stall does
  not; at a 1 second tail-loss RTO it does. That is a numeric design question,
  not a categorical bar.
- **Keying events should be sent redundantly.** At our bitrate, transmitting
  each key-down and key-up three times costs nothing and defeats single-loss
  stalls outright.
- **What cannot be fixed by any of this is QSK full break-in**, and hearing your
  own transmission return. RemoteRig's documentation does not mention QSK, which
  is telling. Separate "remote CW keying works with a delay buffer" from "QSK is
  impossible at relay RTT". Both are true; only the second is a hard limit.

### 9.7 Two hazards nobody had recorded

**Stuck PTT on path loss during transmit.** This is the highest-consequence
failure in the product and it is exactly what a relay drop mid-transmission
produces. The parent's section 12.1 TX watchdog exists for it, but three things
are unspecified: whether the fail-safe deadline is shorter than the reconnect
backoff, what happens when the ladder switches paths mid-transmission and the
watchdog observes the gap, and what the daemon does when the path dies with MOX
asserted and the relay is the path being replaced. A transmitter keyed
indefinitely into an amplifier is a hardware and regulatory problem.

**The station's uplink is a separate axis from reachability, and nobody looked
at it.** Every question in this section is about whether a packet can arrive.
None asks whether a T-Mobile Home Internet uplink can carry 145 to 520 kbit/s
continuously with bounded jitter, which is the actual product requirement for a
station on that carrier. T-Mobile publishes upload as typically 12 to 55 Mbit/s
and latency as typically 16 to 28 ms, which sounds like enormous headroom, but
those are unloaded typicals, no jitter figure is published at all, and the
service is deprioritised below phone customers by policy. Half a megabit is
trivial in throughput and entirely non-trivial in tail latency on a shared,
deprioritised, scheduled 5G uplink with grant-cycle and handover artifacts.

**It is therefore possible for the transport ladder to work perfectly, the relay
to be fine, and CW still to be unusable because the last-hop radio scheduler
injects 100 ms of jitter at 8 pm.** That failure would look exactly like a
transport problem and would be misdiagnosed as one.

### 9.8 Open measurement, expanded to a bench

A STUN classification alone cannot distinguish "UDP is blocked" from "large UDP
datagrams are dropped", and the second is the more likely failure mode here.
Running only the STUN test risks concluding UDP is unusable and committing to a
TCP-only relay on the strength of an MTU problem we could have fixed with a
datagram cap.

**Run before the transport phase is planned**, ordered by value per minute:

| # | Test | Settles | Effort |
| --- | --- | --- | --- |
| 1 | **MTU sweep**, IPv4 and IPv6: don't-fragment ping bisection, then UDP payloads of 100 to 1400 bytes to a VPS echo listener | Whether "blocks UDP" is really a large-datagram drop. Sets the datagram cap. Nobody in the surveyed literature has run it | 10 min |
| 2 | **STUN classification** (RFC 5780): `stunclient --mode full` against two servers on different IPs, or `tailscale netcheck`. Repeat across a day and after a gateway reboot | Whether mapping is endpoint-independent, so whether ICE is real for a station on this carrier. The single highest-value unmeasured fact in the investigation | 20 min |
| 3 | **Diagnose the original Tailscale failure** rather than assuming it: `tailscale netcheck` and `tailscale status` on the same line, checking specifically for the documented 100.64.0.0/10 overlay-versus-CGNAT address collision | Which of four live hypotheses actually caused the founding data point: traversal failure, relay unreachability, the address collision, or relay throughput exhaustion under SmartSDR's multi-megabit stream. Under the last, the experience does not generalise to our product at all | 20 min |
| 4 | **Outbound UDP port and datagram-size sweep**: VPS listening on UDP 443, 3478, 50001 and a random high port; small then large datagrams to each | Whether the relay's UDP listener can live on 443, and whether 443 is treated differently from other UDP ports | 15 min |
| 5 | **IPv6 simultaneous-open pinhole test**: Pi directly on the gateway LAN (no second router, since there is no prefix delegation), both ends sending outbound IPv6 UDP within the same 200 ms window coordinated over an existing TCP channel, with a one-sided control run | Whether the inbound IPv6 filter is stateful, which decides whether tier 1 exists for a station on this carrier | 45 min |
| 6 | **NAT mapping lifetime**: open a UDP flow, go silent for N seconds, probe inbound on the same 5-tuple, bisecting N over 5 to 90 s. Repeat for TCP in minutes | The keepalive interval, which section 9.5 item 5 requires be measured rather than assumed. Feeds the parent's section 12.1 watchdog deadline | 1 hr, mostly waiting |
| 7 | **Uplink jitter at the real media rate**: run 145 to 520 kbit/s from the Pi to a VPS for an hour at peak, logging one-way delay variation and loss | Whether the station's uplink can carry the product at all, independent of reachability (section 9.7) | 1 hr |
| 8 | **Relay tail-latency A/B**, TLS/TCP 443 versus DTLS/UDP 443, keyed CW, `tc netem` at 0.5% loss, logging inter-packet p50/p95/p99/max | Whether TLS/443 is CW-viable in practice or SSB-only. No source anywhere measures relay behaviour below 500 kbit/s | 2 hr |

Tests 1 through 3 take under an hour together and convert most of this section
from hypothesis into measurement.

---

## 10. What this costs

### 10.1 New dependencies, both gated on licence review

| Dependency | For | Licence question |
| --- | --- | --- |
| ICE / DTLS stack, `libdatachannel` the leading candidate | Traversal and data channels | MPL-2.0 plus libjuice LGPL against GPLv2-or-later. Already parent section 17 item 1 |
| Password-authenticated key exchange library | The section 4.3 pairing code | Unresolved, and new with this document |
| `coturn` | STUN and TURN | Server-side only, not linked into our binaries |

**Both licence reviews must complete before any of this is planned.** A failure
on either roughly doubles the corresponding piece of work: the ICE fallback is a
hand-rolled implementation over Qt's `QDtls`, and a PAKE fallback means either
writing one, which is unwise, or dropping section 4.3 to LAN and manual pairing
only.

### 10.2 New components

- Signalling service, small, over existing Qt WebSocket infrastructure
- Packaging and documentation for self-hosting introduction and relay
- Station discovery, multicast on both stacks
- Pairing state machine, code display, and window management
- Paired-device store, plus a Setup page listing devices with revoke
- Key generation, storage, and backup prompting
- Certificate handling, already parent R2

### 10.3 Honest scope note

The parent document allots one sentence to this. It is more than a sentence of
work, and it should appear in the phase plan as its own unit rather than as a
footnote inside R2. That is a consequence of the three requirements in section
1, not scope creep.

---

## 11. End-to-end operator workflow

**First run.** Flash a card, connect the Pi to the same network as the radio,
power it. The daemon discovers the radio exactly as the client does today,
connects, generates its key, names itself from the stored callsign, and waits.
The operator has performed one action.

**Claiming.** Open the client on the same network. The station appears under
"Stations on this network", marked unclaimed. Click Pair. The client offers to
copy station settings up to the daemon so bands, modes, profiles, and PA
calibration travel with it. The pairing window closes.

**At home.** Open the client, click the station under "Your stations". The
client notices it is on the same LAN and connects directly. The local direct
radio connection is released first, with a visible explanation, because the two
must never hold the radio at once (parent section 4.4).

**Away.** Identical interaction. The client tries the cached address, then the
rendezvous, and connects directly if it can. The current path is shown in the
status bar.

**Adding a phone.** On the paired laptop, "Add a Device". A code appears. Type
it on the phone. The radio is not involved.

**A station out of reach.** SSH in once, read the code from the log, and pair
from home with "Add a station by code".

**Losing a laptop.** From the phone: Setup, Devices, Revoke. A live session is
dropped immediately.

**Link failure while transmitting.** The daemon unkeys immediately on its own,
per parent section 12.1. The radio keeps receiving and station state survives.

**Rendezvous outage.** Running sessions are unaffected. New connections use the
cached address or a typed address.

**Card failure.** The key was the station. New card, pair again. A backed-up key
file avoids this, and the client prompts for that backup.

**Everything lost.** A console command returns the station to unclaimed.

---

## 12. Testing

**Unit.** Label parsing including case folding, length, and character rules.
Pairing state machine across window open, closed, reopened, and first-pair
transitions. Code generation, single use, and burn-on-wrong-guess. Device store
add, list, revoke, and revoke-while-connected. Address parsing for hostname,
IPv4, bracketed IPv6, and optional port.

**Security.** Pairing to a station that has already been claimed is refused.
An unpaired key cannot connect. A revoked key cannot reconnect. A wrong code
consumes an attempt. A manual address to a machine with a different key fails to
authenticate rather than connecting.

**Integration.** Client and daemon in one process, full pair, connect, revoke,
reconnect cycle.

**Loopback.** Separate processes on one machine, exercising discovery, pairing,
and both address paths.

**Network bench**, extending the matrix the parent names at section 14:

- Pair on the LAN, one click
- Pair by code across the internet
- Pair by manual key copy
- Connect on the LAN by discovery
- Connect away by cached address
- Connect away by rendezvous
- Connect by typed hostname, typed IPv4, and typed bracketed IPv6
- Direct connection established, verified as not relayed
- Relay fallback, verified as relayed
- **Background upgrade from relay to direct mid-session**
- Self-hosted rendezvous, both introduction and relay
- Rendezvous outage with a session running, and with a new connection
- Revocation dropping a live session
- Station behind a restrictive carrier, both directions
- **The full section 9.8 measurement bench**, all eight tests
- **Relay path switch mid-session**, asserting no audible discontinuity
- **Stuck-PTT fail-safe**: sever the path with MOX asserted, on each relay rung

---

## 13. Open items

1. **The eight-test measurement bench** (section 9.8). Tests 1 to 3 take under
   an hour together and convert most of section 9 from hypothesis into
   measurement. Nothing in the transport phase should be planned before they
   run, and test 3 in particular diagnoses a founding data point that is
   currently load-bearing for two documents and has never been examined.
2. **PAKE library selection and licence review** (section 10.1). Gates
   section 4.3.
3. **`libdatachannel` licence review**, inherited from parent section 17 item 1.
   Gates the transport phase.
4. **Keepalive interval** (section 9.5 item 5), to be measured by bench test 6
   rather than assumed, and reconciled with the parent's section 12.1 TX
   watchdog deadline. Note the parent currently has the consent-freshness
   mechanism backwards.
5. **Whether the station uplink can carry the product at all** on a
   deprioritised carrier connection (section 9.7). Bench test 7. This is a
   separate axis from reachability and a failure here would be misdiagnosed as a
   transport problem.
6. **Stuck-PTT semantics across a mid-transmission path switch** (section 9.7).
   Highest-consequence failure in the product, and the interaction between the
   TX watchdog deadline, the reconnect backoff, and path racing is unspecified.
7. **TURN, or a bespoke DERP-shaped relay?** Standard TURN comes free with any
   ICE library and integrates with candidate racing. A bespoke relay is simpler
   to operate and rate-limit but means implementing path selection ourselves and
   giving up ICE relay candidates. Section 5.4 currently says "TURN over TLS, or
   our own framing" and does not decide.
8. **Mid-session path-switch mechanics** (section 5.4). Dual-receive window,
   timestamp deduplication, and jitter-buffer behaviour across the
   discontinuity are named as required and not specified.
9. **Where the settings copy in section 11 draws its boundary.** It depends on
   parent section 6.3, which the parent itself records as not fully settled.
10. **Default rendezvous hostname, certificate strategy, and operational
    ownership.** Not a code question, but it blocks packaging.
11. **A documented VPS-tunnel escape hatch.** An operator-run outbound VPN or
    `ssh -R` tunnel to a cheap VPS is the most widely deployed real-world
    workaround in this hobby, requires zero relay code from us, works today, and
    is what a technically capable operator will do anyway. The KiwiSDR community
    uses exactly this. Worth documenting as an early-adopter unblock while the
    relay tiers are built, and worth noting that Yggdrasil, Nebula and similar
    all collapse into "self-hosted relay with extra dependencies" because each
    still requires a peer with a reachable address.
