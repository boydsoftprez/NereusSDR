# Rock 5C R2 receive-control bench

Verified on September 20, 2026, with the maintainer's ANAN-G2 / Saturn.
This supplements the historical acceptance procedure; it does not mark
all R2 or R3 acceptance complete.

[mac-control-check.json](mac-control-check.json) records the actual remote
protocol probe from the Mac to the installed native aarch64 daemon.
No pairing token or private key is included. The probe checked the TLS
certificate pin, authentication, populated settings, receive-frequency
change and restoration, and changing receive meters.

The same probe passed when run locally on the board. The normal macOS GUI
then connected in profile `radxa_5c_r2`, displayed the Saturn and live VFO
meter, opened Settings, and visibly disabled local-radio connection actions.
The analog S-meter and accessory-property apply hooks still need coverage.

A board reboot at 21:15:55 EDT ended the session as retryable. The open GUI
reconnected automatically at 21:16:28, reporting a completed station
handshake. `nereusd` started from its enabled systemd unit without manual
intervention. The OEM PWM fan was present after boot and remained under the
kernel thermal governor. Service stops before reboot also exited normally
with status 0.

An earlier network slowdown recovered during interface diagnostics and
service restarts. Its cause was not established. Original checksum offload,
EEE and interrupt batching settings were restored, as was Wi-Fi. The
MikroTik port inspection was read-only and found no bandwidth cap or new
port errors during the recovered streaming sample. See the
[restart record](../../../development/remote-daemon-resume.md) for scope and
remaining work.
