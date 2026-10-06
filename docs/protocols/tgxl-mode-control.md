# TGXL native mode control

The native TCP 9010 mode contract below was captured from the maintainer's
Windows Tuner Genius app on 2026-10-04. The app's commands received successful
replies and corresponding state changes from the real TGXL. These observations
replace the earlier inferred native `operate=N` and `bypass=N` command syntax.

Source capture: `tgxl-mode-buttons-20261004-215217.pcapng` (924,492 bytes).
SHA-256: `3a4056d481f5a2dcc700403e312fef025b9c8aa3e8442238054e542e88876de5`.
The original capture is retained in the maintainer's Documents folder; only
the relevant control excerpts are recorded here.

## Commands and resulting reports

| Operation | Command frame and payload | State frame, selected fields | Reply frame and payload |
| --- | --- | --- | --- |
| Enter bypass | 1004: `C891\|bypass set=1` | 1006: `state=1 bypass=1` | 1007: `R891\|0\|` |
| Leave bypass | 1101: `C915\|bypass set=0` | 1103: `state=1 bypass=0` | 1104: `R915\|0\|` |
| Enter standby | 1302: `C965\|operate set=0` | 1304: `state=0 bypass=0` | 1305: `R965\|0\|` |
| Enter operate | 1359: `C979\|operate set=1` | 1361: `state=1 bypass=0` | 1362: `R979\|0\|` |

Native reports use `state`, whereas the radio-facing SmartSDR amplifier reports
use `operate`. For the observed native values, `state=0` means standby and
`state=1` means enabled. While enabled, `bypass=0` means operate and `bypass=1`
means bypass. No other native `state` value is established by this capture.

The model accepts both report formats. A valid native `state` takes precedence
if a map also contains the `operate` alias; unobserved native values do not
override the known mode. Sending a command does not assign the reported mode:
the button follows the next state/status report from the tuner.

## Reference comparison and NereusSDR behavior

The AetherSDR reference at `0cd4559`, `src/models/TunerModel.cpp`, routes mode
commands through the FlexRadio's SmartSDR API:
`tgxl set handle=<handle> mode=N` and `tgxl set handle=<handle> bypass=N`.
Those radio-facing strings are not the native TCP 9010 commands above.
NereusSDR connects directly to the tuner, so its existing command methods use
the captured native strings instead. This correction uses wire facts; it
introduces no new ported logic.

The existing applet cycle remains OPERATE → BYPASS → STANDBY → OPERATE.
Returning from standby clears bypass before enabling operate. Local controls
and remote Core controls share the same TunerModel methods and retain their
connection and on-air guards. This correction does not touch the RX DSP path.

## Capture placement

A capture on the Radxa sees its own TGXL connection and resulting status
updates. The Windows app has a separate direct TGXL connection; its commands
must be captured on Windows or through switch mirroring. The initial Radxa
captures established bypass changes but could not establish command syntax.
