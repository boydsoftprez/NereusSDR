# EQ and CFC editor design

Status: layout approved in the interactive preview; connecting all CFC band counts and Q to audio and profiles explicitly approved by the maintainer. Implementation plan prepared for review.

## Intended result

Make transmitter EQ and CFC understandable through dragging, while retaining exact entry and the inputs experienced users need. Use the approved layout with the selected-band editor **below** the graph. Implement native Qt widgets using the application's dark theme; the browser preview demonstrates layout and interaction, not production DSP mathematics, defaults, or colors.

## TX EQ

Expose a clear **Graphic / Parametric** selector and retain independent settings for both modes. Keep the existing `TxEqDialog/UsingLegacyEQ` preference. Switching mode applies the selected mode through the existing audio path.

Graphic mode has the ten familiar vertical T-bar band sliders, individual gain and frequency entries, and a separate preamp slider. Keep gain/preamp integers from **-12 to +15 dB**, and center-frequency entry from **10 to 22000 Hz**. Do not add a second selected-band form duplicating these controls.

Parametric mode has a large, quiet graph with numbered band points, a strong configured curve, subdued grid, and selected-band shading. Put **Frequency (Hz), Gain (dB), Width (Q)** below it. Provide width dragging on the graph and a wider-to-narrower slider alongside exact Q entry. Retain **5 / 10 / 18** bands and the existing **10-band** default. Retain gain/preamp **-24 to +24 dB**, Q **0.2 to 20**, and frequency entry **0 to 20000 Hz**. Preserve the actual graph bounds loaded or seeded by existing code; do not substitute the preview's frequency range or logarithmic defaults.

Changing band count retains the existing reset behavior, but first shows an inline “Changing band count resets the curve” notice with Apply and Cancel. Applying it is one undoable action. Selection alone does not edit audio. Retain existing mouse-wheel modifiers and endpoint-frequency locks.

## CFC

Keep two vertically stacked graphs, **Compression** above **EQ after compression**, with exactly aligned frequency axes. Selection and frequencies are shared; the two amounts and Q values are independent. Put one selected-band editor below both graphs: Frequency, Compression, Compression Width (Q), EQ Gain, and EQ Width (Q). Keep global **Pre-compression** and **Post-EQ gain** controls separate from band values. Preserve existing ranges and defaults from the dialog.

CFC frequency entry is **0 to 20000 Hz**; compression and pre-compression are **0 to 16 dB**; post-EQ band and global gain are **-24 to +24 dB**; both Q entries are **0.2 to 20**. The existing ten-band fallback frequencies are **0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000 Hz**, with **5 dB** per-band compression, **0 dB** per-band EQ, and **0 dB** globals. Do not clip the loaded 10000 Hz endpoint to the widget's nominal 4000 Hz initial range.

Keep measured compression bars, their 50 ms refresh interval, and the show/hide timer lifecycle. Label the bars as live measured compression and the drawn line as the configured curve. An audio spectrum/analyzer is outside this first implementation.

Reset Compression zeros compression amounts, sets compression Q to 4, and zeros pre-compression. Reset EQ zeros post-EQ band gains, sets EQ Q to 4, and zeros post-EQ gain. Both preserve the shared frequencies, selection, and the other graph's values. This deliberately resolves existing asymmetric resets that can desynchronize graphs. Changing band count resets both graphs using existing widget defaults, as one action, with the same Apply/Cancel notice as TX EQ.

### Functional CFC connection

The current dialog displays 5/10/18 bands and Q, but its model/audio routing uses ten integer bands and no Q vectors. The completed editor should connect these controls using the existing WDSP `setTxCfcProfile(F, G, E, Qg, Qe)` path. No WDSP algorithm change is needed.

Follow Thetis v2.10.3.15 (`3759d096`) `frmCFCConfig.cs:333-392`: pass every configured band, and pass both Q arrays only when **both** graphs use Q. Otherwise pass empty Q arrays. Use the existing `CFCParaEQData` profile field and `ParaEqEnvelope` compression. Thetis `frmCFCConfig.cs:492-575` stores `compression JSON + "<SEP>" + post-EQ JSON`; `ucParametricEq.cs:1460-1486` uses PascalCase JSON members. Read both that format and the existing Nereus widget's snake_case members. Emit Thetis-compatible PascalCase JSON; do not change TX EQ's existing serialization.

Empty, invalid, or unsupported CFC blobs fall back to the existing ten-band arrays and integer global values, with empty Q arrays. Preserve unrecognized blobs until an explicit user edit replaces them; opening a dialog must not overwrite saved data. Existing 10-band defaults must produce the same audio configuration before the first edit. New profiles preserve configured decimals within the existing save precision (Hz: 3 decimals, dB: 1, Q: 2); legacy arrays remain integer-compatible.

Do not ship active-looking controls that only alter the display. The maintainer selected the full functional connection, including all band counts and Q.

## Shared interactions and state

Dragging a point edits frequency and amount. Dragging a width handle edits only Q. Width handles derive from the current widget's linear-frequency Gaussian FWHM, `span / (3 * Q)`, with its existing minimum width. Do not replace it with the browser preview's octave formula or claim the curve is a measured WDSP response. Clipping a handle at the graph edge must not change stored Q. Width handles remain discoverable at zero gain, work at negative gain, and are unavailable when Use Q Factors is off.

Undo/redo is session-local: one entry per drag, slider gesture, committed text edit, wheel event, reset, range change, or applied band-count change. TX has independent Graphic and Parametric histories; CFC has one history for the two graphs together. Histories survive hide/reopen of the existing modeless singleton dialogs. Profile replacement or authoritative external model changes rebase history; undo cannot resurrect another profile. Capture full-precision runtime state rather than round-tripping the rounded profile serializer. Exclude selection-only changes and live measured samples from history equality. Restore selection for usability without making selection itself an undo step. Keep existing Live Update behavior: no audio changes during a drag when off, then one committed update on release.

Retain enable/bypass switches, log/linear choice, range inputs, guide, and all existing algorithm inputs. Fold expert settings into **Advanced** rather than removing them. Range controls currently rescale band frequencies: label them “Curve range” and explain that effect. Preserve the 1000 Hz minimum spread.

## Project constraints and acceptance

- Native C++20 and Qt6; no new runtime dependencies or web view.
- Use `StyleConstants.h`, Qt ownership, existing model guards, and `QSignalBlocker`; no new audio-thread locks.
- Preserve existing TX EQ DSP conversion, defaults, and profile formats. In particular, its 5/18-band curves are still sampled into the existing ten-band WDSP EQ profile.
- Preserve existing independent legacy/parametric settings and profile-bank ownership in TxApplet/Setup. Do not create another profile selector or bank.
- All new user-facing strings use `tr()`. Dialog buttons have `autoDefault(false)`. Provide accessible labels, keyboard focus, exact entry, and Undo/Redo shortcuts scoped to the dialog; text-field undo takes precedence while editing text.
- No changes to the core RX path, WDSP algorithms, radio protocol, or release/version.
- Preserve upstream license headers and inline tags; new ports include provenance entries and cites pinned to v2.10.3.15. Follow `docs/attribution/HOW-TO-PORT.md`.
- Verify native layout on a 1280×800 display at normal and 125%/150% scaling: editors and band labels readable, no clipped controls, both CFC plot bounds aligned. Scroll the control area if needed without stealing graph dragging/wheel gestures.
- Meaningful tests cover actual UI → model → DSP/profile behavior, profile changes while open, all supported counts, range/reset/undo interactions, and measured-bar lifecycle. Build matching test targets before running them, and run the full suite before claiming completion.
