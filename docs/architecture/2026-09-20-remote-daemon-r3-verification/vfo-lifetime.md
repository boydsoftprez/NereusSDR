# VFO flag lifetime verification (R-R3-30)

The macOS GUI crash at 19:09:12 on September 21 occurred after a two-pan to
one-pan layout change rehomed slice B. The crash stack reached
`QAbstractSlider::value` through `VfoWidget::setAgcThreshold`, from a mirrored
`SliceModel::agcThresholdChanged` update. The running executable and private
library UUIDs matched installed checkpoint `706b9a5f`.

The model-to-flag lambda used MainWindow as its Qt connection context while
capturing the shorter-lived flag. `SpectrumWidget::removeVfoWidget` deleted
the flag, leaving the callback active. Fourteen presentation bindings shared
this lifetime mismatch: RIT/XIT state and offsets, SNB, APF and its tuning,
mute, audio pan, squelch state and threshold, AGC threshold, binaural and lock.

The production wiring was extracted without changing its original context
before the regression ran. The test creates a real SpectrumWidget and
secondary VfoWidget, keeps the SliceModel and former owner alive, removes the
flag through SpectrumWidget, and checks each connection using the result of
`QObject::disconnect(handle)`. This detects a still-live connection without
deliberately invoking freed memory. Checking the handle's boolean conversion
would not establish automatic disconnection.

The fresh baseline build passed. The regression failed at the expected
assertion, `retired VFO flag left model presentation connections live`:
two Qt cases passed, one failed, none skipped (0.87 seconds for CTest).

The correction uses the flag as the receiver for all fourteen typed Qt
connections. It removes the long-lived owner argument entirely and preserves
MainWindow's existing initial state pushes and other guarded bindings.
The regression applies every covered property after flag retirement, creates
a replacement flag, then verifies live toggle, audio-pan and AGC-slider updates.

Fresh focused verification passed all four executables in 4.05 seconds:
`tst_slice_flag_presentation_lifetime`, `tst_remote_gui_gating`,
`tst_vfo_mode_containers` and `tst_pan_layout_dialog_gating`. All 64 inner Qt
cases passed, with none skipped. The independent consolidated review found no
actionable issues. A fresh `all_tests` and `nereusd` build followed by unfiltered
CTest passed all 684 executables in 144.54 seconds. Eleven pre-existing inner
Qt skips remain; the new regression has none. Live two-pan to one-pan rehoming with ongoing Core
updates remains pending. The separate
audio-source loss and stale display after Core restart are not closed by this
widget-lifetime correction.

## Separate audio-source loss

Core stayed active during the incident. A passive capture contained Saturn
DDC2 I/Q and status packets, so this was not established total radio ingress
loss. Three zero-audio intervals in the Core log began immediately after
creation of RADE channel 1; audio resumed after that channel was destroyed in
the first two intervals. The third lasted until the Core restart.

The source audit found that RadioModel passes the RADE channel to RxDspWorker
without its owning slice ID. The worker applies that pointer only to slice 0,
which redirects A's audio into a channel created for B and suppresses A's
ordinary mixer contribution. This is a confirmed routing defect and a strong
explanation for the logged silence while RADE remained unsynchronised.
Whether DDC3 was delivering throughout the incident remains unproved.

This routing repair needs its own concurrency/lifetime contract and regression
before implementation. Changing Opus, media retry policy or network settings
would not repair the missing slice identity.
