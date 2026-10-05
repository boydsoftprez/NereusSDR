# WDSP 2.10, NNR and PureSignal 3 operator notes

The desktop and Core share the pinned WDSP 2.10 implementation. Existing NR
choices and the Thetis CFC Qg/Qe profiles are retained. Platform and RF evidence
is tracked separately in the [verification ledger](wdsp210-verification/README.md).

## NNR

Select NNR for the receiver, then right-click its control to open that receiver's
settings without toggling processing. Standard/Premium model and suppression
are the quick controls. Advanced settings expose position relative to AGC,
alpha, alpha knee, normalization time, maximum gain, attack and release.
Setup provides the same accepted values and access to Models.

Normal tuning and NR selection persist per radio and stable receiver identity,
including edits to an inactive receiver. Reset tuning keeps the model and NR
selection. Diagnostics are explicitly session-only and return to normal when
the station session is replaced.

Models belong to the station. Importing validates and stores a model; choosing
an override marks it pending. Apply rebuilds the receivers while preserving
their identities and active selection. Stop transmitting before applying.
A missing saved override is shown explicitly and never silently substitutes a
different model. Remote changes and files are validated and stored by Core.

## PureSignal 3

The existing PureSignal dialog and applet provide Off, Single, Start Auto,
Apply Current, two-tone, Save, Restore and AmpView. Advanced controls expose
normal configuration and native status. Processing pause and Off are distinct:
pausing calibration can leave a correction active; Off stops correction.
Apply Current requires an existing retained correction.

Normal timing, automatic-calibration preference, attenuation and hardware-peak
configuration persist per radio. Dialog geometry, On Top, advanced expansion,
measurement overlay and AmpView display choices remain local GUI preferences.
A reload never repeats Single, Restore, two-tone, PTT or pending commands.

Save produces a station-owned correction asset. The manager supports list,
import and export independently of permission to restore. Restore can activate
correction and requires actuation permission. PS3 uses version-2 correction
files; legacy version-1 files are refused with an explanation. Recalibrate to
produce a compatible correction rather than assuming old curves transfer.

Remote settings, status, assets and AmpView are available. The connected Core
advertises which actions the device may perform. Single, Start Auto, Apply
Current and Restore require the offered PureSignal arming capability; older
Cores or devices without the required authority receive a refusal. Starting
two-tone is a transmit-keying action and follows the current device's transmit
ownership rules. Stopping it follows the holder's release rules.

The legacy PS-RX/PS-TX spectra checkbox has no working display route and is
explicitly unavailable; its saved preference is retained. AmpView displays the
supported PS3 sample and correction curves.

## Persistence and verification

The radio-owning process is authoritative. A refused remote edit restores its
accepted value, and a settings-file write failure is visible and retried.
Saved correction selection is separate from applying that correction.

Native software tests do not establish RF improvement, safe feedback level or
Rock 5C capacity. Those require the arranged, authorized hardware checks listed
in the verification ledger.
