# Combined Core/GUI and WDSP 2.10 source verification

Source revision: signed local merge `fe9e0dc1`, September 22, 2026.
Parents: Core/GUI `0017ef7c` and WDSP `5b74346f`.

This checkpoint combines the selected-radio recovery and wideband foundation
with WDSP 2.10, persistent NNR settings/assets and PureSignal 3. It does not
close remote wideband transport, receiver membership restoration or hardware
acceptance. The last launched Core/GUI pair is `55e7d49f`; the Rock is currently
unreachable, with the cause unassigned. A new Mac bundle is built at this
checkpoint but has not been launched; the existing GUI process remains open.

## Integration boundaries

The signed merge completed without conflicts. Both sets of CMake additions
are present, including the Core extended-spectrum reducer and the new DSP
controls/tests. NNR's explicit model rebuild still calls
`connectToRadioPreservingSlices`; the daemon's shutdown settings flush remains
present. The only RadioModel changes relative to the WDSP handoff are the
wideband capture-generation checks and their API comment.

The vendor tree, PureSignal, RxChannel, TxChannel, DSP asset/NNR code and
DaemonApp are byte-identical to the verified WDSP handoff. The P2 capture
lifetime implementation, shared spectrum helpers and real daemon recovery
fixture are unchanged from the Core/GUI parent. The build compiles a new
native WDSP archive, rather than reusing a pre-upgrade binary.

The WDSP task's consolidated review and native AddressSanitizer evidence are
retained in [its verification record](../wdsp210-verification/README.md).
The native probe instruments all 83 WDSP C units, but not the linked rnnoise,
specbleach or FFTW dependencies; Apple ASan has no leak detection. That evidence
is reused for the unchanged native source, with combined integration checked
below rather than repeating the independent review.

## Combined checks

Environment: existing macOS arm64 `build-integration`, Ninja RelWithDebInfo.
Build-start load: 4.67 / 5.77 / 6.59; suite-start load: 14.73 / 11.82 / 9.15.
Other worktree build/probe jobs were idle. A mid-run observation also showed
macOS media analysis and audio services consuming CPU; the elapsed time is
verification evidence, not a performance comparison.

- Desktop, `nereusd` and all 718 test executables: built successfully.
- Full CTest suite: **718/718 passed**, zero failed, 267.44 seconds with `-j6`.
  Twelve inner Qt cases are skipped: eleven existing environment/deferred
  harness cases and the opt-in PS form capture (its capture directory is not
  set for the full run). The WDSP handoff records separate native UI captures;
  a passing executable does not convert these skips into runtime coverage.
- The verification commit runs the repository's mandatory full-tree source,
  attribution, inline-citation/author-tag and GUI/DSP ownership gates; detailed
  output is retained in `r3-wdsp-combined-evidence-commit.log`.

The first evidence commit was blocked by 98 native file paths absent from the
new-port checker's registry. Their notices and import inventory were intact,
but the WDSP provenance rewrite had retained grouped basenames rather than
first-column repository paths. The feature's pre-commit diff check looked at
committed `BASE..HEAD`, so it did not cover that then-staged vendor import;
the full-tree new-port sweep covers `src/`, not native vendor directories.
This is a pre-commit coverage limitation to correct separately, not evidence
that the native headers were missing. The combined checkpoint restores a
fixed 167-file registry, checking current hashes against the frozen manifest:
126 pinned-identical, 30 reviewed variants and 11 local files. Future native
additions are not implicitly allowed. DSP/runtime source remains unchanged
from the full-suite run.

The subsequent checker correction audits the cumulative merge-base-to-index
snapshot, including both committed branch changes and staged additions. Source
blobs and all four provenance registries come from that same index snapshot;
an unstaged clean file or registry entry cannot mask a staged violation. The
full-tree mode retains its worktree sweep. Nine isolated real-Git regression
cases cover new/modified staged source, matching staged registration, unstaged
masking, committed PR content, a verified `R100` rename, deletion, and full-tree
behavior. The complete Python compliance suite passes **20/20**. This changes
tooling and tests only, so the unchanged runtime's 718-test evidence above is
retained rather than rerun.

Local detailed logs are retained under `~/.config/nereus/work/` as
`r3-wdsp-combined-build.log`, `r3-wdsp-combined-tests.log`,
`r3-wdsp-combined-load.log`, `r3-wdsp-combined-skips.log` and
`r3-wdsp-combined-merge.log`.

## Remaining acceptance

Linux/Windows and Linux ARM64 checks, Rock load and audio continuity, NNR
listening, complete process-relaunch workflows and radio reconnect remain
pending. No RF operation, deployment, public push or PR change occurred at
this checkpoint. Remote PS3 actuation remains subject to R4.

The Core compositor passes local pixel-parity characterization, but source
descriptors, aggregated ADC/filter demand, remote contexts and GUI admission
remain open in the [wideband contract](../2026-09-22-remote-wideband-design.md).
The existing full DDC row used by 3D remains distinct from ADC-wide survey data.
