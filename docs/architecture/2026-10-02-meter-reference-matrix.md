# Container meter recovery: source matrix

Date: 2026-10-02. Status: Mic/ALC implementation and native macOS CPU/Metal validation; remaining families pending.

Reference: `/Users/j.j.boyd/Thetis`, `git describe --tags` = `v2.10.3.15`.
All Thetis line numbers below refer to `Project Files/Source/Console/MeterManager.cs`
unless another file is named. Nereus base is `dd53da5af`. Recovered files come from
`15de6fbecf`; they are recovery material, not a fidelity oracle.

This matrix distinguishes checked facts from source entry points still requiring
family-specific verification. Add implementation/test/capture evidence as each
family lands. Do not claim parity based on a recovered class name or an old test.

## Mic and ALC: checked source facts

| Aspect | Mic | ALC | Source |
| --- | --- | --- | --- |
| Primary reading | `MIC_PK` | `ALC_PK` | AddMicBar 24327–24413; AddALCBar 24649–24734 |
| Secondary reading | `MIC` (average) | `ALC` (average) | Same constructors |
| Calibration | -30 → 0; 0 → .665; +12 → .99 | Same | Same constructors |
| Attack / release ratio | .8 / .1 | .8 / .1 | Same constructors |
| History duration | 2000 ms | 2000 ms | Same constructors |
| Update interval | Caller `nMSupdate` | Caller `nMSupdate` | Same constructors; not prototype 50 ms |
| Primary marker | Yellow | Yellow | Same constructors |
| Secondary marker | DarkGray | DarkGray | Same constructors |
| Primary history | Red with alpha 128 | LemonChiffon with alpha 128 | Same constructors |
| Secondary history | Off | Off | Same constructors |
| Default indicator style | Line | Line | Same constructors |
| Primary peak value text | On by bar default | On by bar default | clsBarItem 21270–21322 |
| Secondary value / peak text | Off / off | Off / off | Same constructors |
| Layer order | Backdrop 1, primary 2, secondary 3, scale 4; primary marker redrawn after secondary | Same | Constructors; render dispatch 32921–32923 |

`clsBarItem.Update`, 21323–21385, smooths each channel independently:
for rising input, `reading * AttackRatio + Value * (1 - AttackRatio)`;
otherwise use DecayRatio. History stores smoothed values, limits sample count to
`HistoryDuration / UpdateInterval`, and removes oldest samples. It advances on
scheduled updates even when the input equals the previous input.

`MinHistory`/`MaxHistory`, 21427–21448, use the current Value when history is empty.
`ClearHistory`, 21449–21458, clears the range and skips
`IgnoreHistoryDuration / UpdateInterval` subsequent samples; the constructor's
ignore duration is 2000 ms. Do not substitute the old ANAN preset's assumed
250 ms across families. Peak hold draws the history maximum, not a fabricated
separately decaying high-water mark (37388–37423, 37424–37674).

Independent numeric check, starting at -30 for both channels and updating at
100 ms, with primary inputs 0, 0, -30, -30 and secondary inputs -12 each tick:

| Update | Primary | Secondary |
| --- | ---: | ---: |
| 1 | -6 | -15.6 |
| 2 | -1.2 | -12.72 |
| 3 | -4.08 | -12.144 |
| 4 | -6.672 | -12.0288 |

These values follow the source recurrence directly; they must not be generated
by the recovered C++ class under test. Explicitly distinguish first-view cache
seeding from normal smoothing initialization in the test and departure notes.

Rendering entry points: scale 33393–33871; generalScale 33872–33958;
primary-marker redraw 37388–37423; horizontal bar 37424–37674.
Check the face at minimum, accepted-study and normal sizes on CPU and GPU.

## Reading availability on current Nereus main

`MeterPoller.h:116–169` exposes average Mic/ALC bindings only; complete faces need
additional GUI binding IDs for peaks, preserving existing numeric IDs.
`WdspTypes.h:223–285, 371–432` already defines `TxMeterType::MicPeak`, `AlcPeak`
and source-correct `thetisTxReading(ThetisTxReading::MicPk/AlcPk, readRaw)`.
Use those existing model APIs in the single poller; no direct GUI WDSP calls,
new DSP instance, or change to transmit parameters.

`TxChannel.cpp:737–755` returns cached readings and schedules refresh on its
existing lane. The GUI must retain that ownership. Newly added/recreated views
receive the correct window/source cache; they must not reuse another source's
latest value.

The remote Core's current `TxMeterReadings` (`TxMeterPump.h:70–100`) and
`TransmitStateFacade.h` expose average Mic/ALC and stage readings, not separate
Mic/ALC peak fields. This plan does not change the wire protocol. Keep peak
channels explicitly unavailable on those remote versions; do not copy average
into peak or invent a synthetic radio reading. Show the reason through the
existing availability path while retaining useful available channels.

## Other family source entry points

These ranges locate constructors/renderers; their full calibration, properties
and dynamics must be checked by Task 5 before each family is offered as complete.

| Family | Composition/configuration entry | Rendering/update entry | Current review status |
| --- | --- | --- | --- |
| Signal / Avg / Max Bin bars | AddSMeterBarSignal/Avg/MaxBin 22822–22939 | Bar Update 21323–21385; renderHBar 37424 | Locate only |
| ADC and ADC max | AddADCMaxMag 22940–23000; AddADCBar 23063–23150 | Bar/scale rendering | Locate only |
| PBSNR | AddPBSNRBar 23151–23221 | Bar/scale rendering | Locate only |
| AGC / gain | AddAGCGainBar 23222–23283; AddAGCBar 23284–23373 | Bar/scale rendering | Locate only |
| Custom bar | AddCustomBar 23374–23465 | Bar/scale rendering | Locate only |
| Magic Eye | AddMagicEye 23572–23618 | clsMagicEyeItem 17175; renderEye 37233 | Locate only |
| Spacer | AddSpacer 23647–23675 | renderSpacer 36544 | Locate only |
| ANAN Multi Meter | AddAnanMM 23784–24139 | clsNeedleItem 21900; renderNeedle 40740 | Locate only |
| Cross needle | AddCrossNeedle 24140–24326 | Same needle renderer | Locate only |
| EQ | AddEQBar 24414–24501 | Bar/scale rendering | Locate only |
| Leveler / gain | AddLevelerBar 24502–24587; AddLevelerGainBar 24588–24648 | Bar/scale rendering | Locate only |
| ALC gain / group | AddALCGainBar 24735–24795; AddALCGroupBar 24796–24856 | Bar/scale rendering | Locate only |
| CFC / gain | AddCFCBar 24857–24942; AddCFCGainBar 24943–25003 | Bar/scale rendering | Locate only |
| COMP | AddCompBar 25004–25090 | Bar/scale rendering; keep main's correct COMP floor | Locate only |
| Power / reflected power | AddPWRBar overload 25185–25312 | NormaliseTo100W, calibration and power-scale renderer | Constructor checked; full renderer pending |
| SWR | AddSWRBar 25313–25375 | Bar/scale rendering | Constructor checked |
| History graph | AddHistory 25585–25613 | clsHistoryItem 17469; renderHistory 36315 | Locate only |
| VFO display | AddVFODisplay 25751–25814 | renderVfoDisplay 39857 | Locate only |
| Clock | AddClock 25815 onward | clsClock 15395; renderClock 40474 | Locate only |
| Signal text | AddSMeterBarText 23001–23062 | clsSignalText 21606; renderSignalTextDisplay 40538 | Locate only |
| Contest | Recover class's cited sources and compare complete configuration | Its constituent renderers | Not yet studied |

Power constructor calibration at a normalized 100 W rating is
0/5/10/50/100/120 → 0/.1875/.375/.5625/.75/.99; `NormaliseTo100W` is true.
SWR calibration is 1/1.5/2/3/5 → 0/.25/.5/.75/.99; the high point is 3.
Main's primitive `rescalePowerMeters` uses a linear maximum × 1.2 and separate
QRP tick count; do not claim its curve matches Thetis's calibrated power face.
Preserve legacy presentation and use each complete face's verified calibration.

Needle constructor/update 21900–22140 has its own defaults (.8/.2, 500 ms history,
2000 ms ignore-history) and family constructors may override them. Read those
overrides; Mic/ALC's .1 release is not the universal needle default.

## Required evidence for each completed family

Record its exact bindings, source availability, calibration and reset policy;
complete serializable properties and their observed effect; independent numeric
trace checks; CPU/GPU native captures; and deliberate departures with reasons.
New preset/dynamics source files need the verbatim applicable upstream header,
inline comments/cites and provenance rows in the same commit as the port.
No completed-family/runtime-parity claim is made by this source-study document.

## Task 4 implementation and downstream configuration contract

`BarPresetItem` registers `meter.mic`, `meter.alc`, `meter.customBar`.
Historical `BarPreset` JSON with flavor Mic/Alc/Custom is an additional supported
adapter, not a promotion of primitive BAR or GROUP records. Unknown flavors stay
recoverable unavailable data. Exact current bindings are Mic primary113/average103,
ALC primary114/average105 (`MeterPoller.h` named constants). Both independent
channels use shared `MeterDynamics`; absence clears only that channel, while the
available average still paints its marker. Reset uses the configured minimum;
first replay seeds raw input and the next shared frame applies the recurrence.
No new timer, subscription, DSP access, Core/model change or radio operation.

`configuration()` returns the effective JSON object; `applyConfiguration(object)`
merges a partial edit, validates the whole result before mutation, and resets
presentation dynamics for subsequent replay. `serialize`/`deserialize` use compact
JSON, never pipe splitting. Known fields:

| Fields | Meaning / editing contract |
| --- | --- |
| kind, flavor | BarPreset and Mic/Alc/Custom; use configureAsMic/Alc/Custom for new faces |
| x,y,w,h | Legacy normalized canvas geometry; positive size, finite float-representable coordinates |
| bindingId, secondaryBindingId | Primary and independent average; integer IDs, -1 means absent |
| label,titleColor | Visible centered title and its ARGB color |
| barColor,backdropColor,historyColor | Primary marker / Custom low fill, backdrop, recent-range ARGB colors |
| redThreshold | Effective high transition for both scale and filled styles; source default0, historical customization retained |
| showHistory,peakHold | Independent recent-range / red history-maximum marker controls |
| showValue,showPeakValue,units | Current and recent maximum text visibility and displayed unit suffix |
| style | Line, Solid, Segments; filled Mic/ALC low white/high red; Custom low barColor/high red |
| updateIntervalMs,historyMs,ignoreHistoryMs | Caller cadence default100, bounded history default2000, reset ignore default2000 |
| rowHeight | Preferred layout height, minimum72; `preferredFaceHeight()` and `minimumFaceSize()` (width260) consumed by Task6 host |
| minValue,maxValue | Mic/ALC preserve historical/reset range data; their scale is the locked source -30/0/+12→0/.665/.99. Task9 must not offer these as Mic/ALC calibration controls. Custom range is editable and determines its linear scale. |

The registry stores exact original bytes in `config.legacyRecord` and effective
named JSON fields in `config.properties`, including unknown nested fields.
`properties` is a named overlay, distinct from primitive pipe records'
`config.overrides` numeric field-index string map. Capture retains the original
record and imported double canvas geometry when the float renderer did not edit
it. Previously persisted unavailableReason is re-evaluated. Old MeterWidget and
ItemGroup adapters no longer append pipe-tail fragments to edited JSON labels.

Rendering keeps backdrop/title in Background and moving history, readings and
markers in OverlayDynamic. A face-local DPR-aware raster cache replays the static
scale between average and primary marker, matching source PostDrawItem order.
No per-face whole-stack bitmap is retained. The existing QRhi premultiplied
QPainter overlay required `One` source blending; `SrcAlpha` multiplied RGB twice.
Native pixel oracles independently verify 128-alpha red over32-gray as144/16/16,
and LemonChiffon as144/141/119 (small rounding tolerance). The straight-alpha
geometry pipeline remains unchanged.

Deliberate presentation adaptations: approved fixed-pixel padding/fonts and
minimum width260/row72 replace Thetis global ratio geometry; explicit capture
configuration enables red titles and peak hold without changing defaults. Recent
range stays a continuous min/max band for all styles, following the accepted
study, while segment fill uses source block/gap sizing and low/high colors. Source
family calibration remains locked. Timestamp history expiry is Task3's documented
adaptation for missed shared GUI frames, without inventing intermediate samples.

Independent oracle: `tests/fixtures/meters/mic-alc-source-trace.csv`; tests cover
calibration/clipping, separate primary/average recurrence, equal-input history
expiry, availability and transitions, atomic invalid edits, real property effects,
JSON unknown fields and edited pipe-containing labels, renderer geometry/color.
Native CPU QWidget and GPU Metal `frameSubmitted` + `grabFramebuffer` traces use
65 shared100ms timestamps: rise0–14, steady15–29, fall/expiry30–59, unavailable
primary60–64 with useful average. Both capture260/434/640 logical widths, two72px
rows, cocoa platform, DPI72/DPR2. Pixel assertions run even without export flags.
Artifacts/logs are in `.crew/task-4-captures/{cpu,gpu}` and the Task4 report.
Controller inspected/accepted CPU and GPU milestone images. Windows/Linux and
hardware parity are not claimed; remaining family verification belongs to Task5.
