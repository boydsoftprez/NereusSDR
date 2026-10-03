# R-R3-34: passive receive-model restoration

Historical passive checkpoint. Daemon/runtime integration has since been
implemented; its final combined verification and hardware acceptance are
tracked in the R3 plan and receive-layout design. The failures below remain
the evidence that prompted the subsequent native-audio investigation.

This checkpoint connects the validated layout representation to local receiver
objects. It does not yet enable daemon startup restoration or hardware
resource admission. The [design](../2026-09-22-core-receive-layout-design.md)
keeps those remaining steps explicit.

`SliceModel::restoreReceiveState` reads conventional preferences while offline,
then applies the authoritative manifest frequency and mode without the normal
mode setter, which could start RADE. It bypasses a saved frequency lock,
preserves compatible legacy filters and selects the manifest-mode filter when
legacy tuning differs. It refuses invalid descriptors, Remote models, missing
Local ownership, a connection, initialized DSP, or any receive/RADE channel.

`RadioModel::hydrateReceiveLayout` validates the full record and MAC before
changing membership. It reuses shared objects, creates new objects with settled
settings before publication, and retires extras without writing bootstrap
preferences. It preserves stable IDs and descriptor order, resolves the
explicit RADE receive owner, and publishes a final settled-state notification.
Scheduled and forced saves remain suppressed pending resource admission.

The new tests exercise real sandboxed AppSettings file reloads, shared-object
identity, noncontiguous membership with no A, active ID versus list position,
settings preservation, reentrant forced flush during retirement, no decoder
activation, explicit second RADE owner, and refusal before changing invalid or
already bound layouts. Slice tests also cover AF/AGC/NNR preferences, compatible
legacy filters, mismatched/missing mode sentinels and signal suppression.

A first parallel run exposed the test suite's shared singleton settings path:
another executable could replace the file between save and reload. Each new
disk-reading test now sets a unique process profile before first use of
AppSettings. The disk assertions remain intact. Five focused targets pass
together after isolation; final expanded coverage is included in the full
build/test run recorded below.

## Verification status

The desktop application, `nereusd`, and all 721 test executables build.
The final five focused targets pass together, including the expanded hydration
cases. The unfiltered full suite is **not passing**: four existing
DSP/audio-startup executables timed out at 120 seconds, and the run was stopped
after 221 completions to investigate rather than continue consuming resources.
An isolated rerun of `tst_connectable_radio_model` also timed out. Its last
messages show speakers opening successfully and a second PortAudio ring being
locked; the following TX-input open is a suspected blocking boundary, not a
confirmed cause. No production behavior or test expectation was relaxed.

The suite-start load average was 15.13 / 6.58 / 5.29. Diagnostic stack sampling
did not return before being terminated, so there is no stack-trace evidence
yet. The full verification gate and commit remain pending. This does not
supersede the prior committed foundation's 719/719 result.

## Remaining acceptance boundary

DaemonApp still has no production caller of this hydration interface. Remaining
work includes board/channel/DDC admission and explicit refusals, sparse-ID DSP
startup, the saved RADE owner's runtime activation, configured-count precedence,
manifest capture/flush, first-discovery full snapshot reseeding and GUI recovery
explanation. The consolidated persistence/lifecycle review belongs at that
integrated boundary. No hardware deployment or real restart acceptance is
claimed by this checkpoint.
