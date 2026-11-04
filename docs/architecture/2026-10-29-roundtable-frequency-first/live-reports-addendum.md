# Automatic active-frequency reports and doubles

Approved by J.J. Boyd in the release chat on 2026-11-02: live receiver
reports belong in the spectrum inset without opening a pill, submenu, or
panel. The iPhone presentation is the reference for a compact native
client experience. J.J. also approved visible overlapping keyed stations.

The active listening frequency shows the current talker and each available
listener's measurement automatically. One valid listener report is useful;
the card must not require two cochannel directory entries. Current report
content takes priority over a redundant list of people. Other frequencies
retain compact presence information. Show as many complete report rows as
the available geometry permits, disclose any omitted reports explicitly,
and retain access to details without retuning. Keep the existing local
S-units/dBm/Both choice, readable fonts, measured widths, and keepouts.

When the authoritative turn says two or more stations overlap, show
Double and the supplied keyed callsigns (including duplicate identity
suffixes). Receiver measurements during that turn are combined RF,
never individual reports of either transmitter. Exclude transmitters from
the receiver rows. After one unkeys, the remaining talker's new turn uses
only readings matching that new turn; old combined readings cannot carry
over. An ended overlap is explicitly historical, never a live Double.
Unknown RF remains unknown. Directory presence or a stronger S-meter alone
is not evidence of a double, and no external-RF classifier is introduced.

Freshness, source-room identity, frequency, mode family, and turn identity
remain explicit. Stale/missing values must not look live or become zero.
Ended reports retain their age and a last-heard caption. Quiet does not
produce a band full of empty room markers.

The existing server already records doubled turn participants and raw
combined samples: `turns.py` opens a doubled turn on overlapping keys and
ends it when only one remains; the web report table already labels raw
combined readings. This work changes presentation and client data wants,
not I/Q, WDSP, transmit control, wire schemas, or RF detection.

Alternatives considered: one-click reports still hide the primary data;
an always-expanded card on every frequency obscures the spectrum. The
approved choice is passive reports at the active frequency, compact
presence elsewhere, with interaction reserved for additional detail.

Performance must be preserved: receiver sample updates may update the
tag widget and existing report rows, but must not invalidate static GPU
textures or reconstruct the entire panel. Structural changes remain
prompt. CPU rendering must repaint without waiting for a new FFT.
