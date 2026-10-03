# PC microphone source intent

Requirement: R-R3-36, bounded prerequisite to optional capture startup.

## Behavior

Selecting PC microphone now chooses that source independently of whether its
capture bus is available. Ordinary and RADE worker paths use the selection-only
query and retain their existing zero-fill policy when the bus supplies no
samples. An absent or closed PC microphone therefore cannot substitute the
already-drained radio microphone. The ordinary path supplies that same selected
input to DEXP/VOX before the TX channel.

The existing combined selection/readiness query remains available. Active TCI
still has its earlier source override and return. VAX, explicit radio selection,
and WDSP-generated Tune/two-tone paths retain their existing routing. This is
not a new MOX admission policy, capture opener or remote transmit implementation.

## Source and regression evidence

The source audit checked Thetis `ChannelMaster/cmaster.c:377-389` at
`v2.10.3.15` (`3759d096`): the standard TX block invokes `asioIN` before TCI
override, `xdexp` and `fexchange0`. Nereus already has its own corresponding
input replacement path; the defect was skipping that path when PC input was
selected but unavailable. No DSP algorithm or upstream constant changed.

The four existing focused suites passed before the new regressions. The new
ordinary and RADE cases then reproduced radio-microphone leakage with selected
PC input unavailable. The corrected source passed all four focused targets:
`tst_audio_engine_pull_tx_mic`, `tst_tx_worker_thread`, `tst_tx_mic_source`, and
`tst_rade_tx_pump`.

Coverage includes absent and closed buses on the ordinary worker path, valid
PC samples, zero-filled short reads, switching explicitly back to radio input,
and actual RADE resampler output fed from loud radio samples while PC capture is
absent. Tests observe the TX-channel input or RADE encoder-input payload instead
of asserting only the new predicate. The independent consolidated source review
found no actionable defect. The matching GUI/Core/all-tests build passed,
followed by an unfiltered **740/740** desktop suite in **277.34 seconds**.
Load averages were 4.44 / 2.91 / 2.64 before build, 11.14 / 4.91 / 3.40 before
CTest, and 8.17 / 7.13 / 4.85 afterward. Twelve existing inner Qt cases skipped:
device/UI alternatives, optional capture, a modal-menu case, obsolete RADE TX
harness cases and unopened WDSP TX harness cases. No test executable was
excluded or timed out. Native input-opening tests passed in this run; one
successful run does not close the intermittent startup failure below.

Retained logs: `r3-pc-mic-final-{build,tests,load}.log` and
`r3-pc-mic-final-qt-cases.log` in the private work evidence directory. After the
tested behavior was frozen, two explanatory comments were corrected to describe
the retained readiness query accurately; they do not change executable behavior.

## Limits

`AudioEngine::start()` and `setTxInputConfig()` still open native microphone
capture synchronously. Their optional-capture lifecycle remains R-R3-36 work.
This correction does not make a blocked native open cancellable, redesign bus
ownership during reconfiguration, or prevent keying when selected PC input is
unavailable. The later source-aware admission design must refuse before RF
effects and preserve unconditional release. Worker silence is an independent
defense against source substitution.

No RF transmission or operator PC-microphone acceptance was performed for this
source prerequisite. It is unrelated to the reported remote receive S-meter
symptom, which has not been reproduced in the available live observations.
