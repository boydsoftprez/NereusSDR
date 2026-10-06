# Receiver DSP and filter setup

The desktop DSP menu and VFO DSP tab provide quick controls for the selected slice. Detailed controls are in **File > Settings > DSP**. These settings act on receiver or transmitter DSP at the Core. A phone presents quick receiver controls in **Modes** and its **RX** sheet, while Core-described Setup controls are available only when that Core advertises them. A control may be omitted or disabled because the radio, Core build, neural asset, or current operating state does not support it.

For routine listening, start with the active slice's mode, filter and AGC controls. Change one DSP method at a time, compare the same signal, and check both intelligibility and the noise between transmissions. Noise processing can suppress wanted speech or create artifacts. Turn a method off if its result is worse. The detailed controls below are tuning tools, not a requirement to enable every processor.

## Choose and tune receiver AGC

Open the selected VFO's **DSP** tab or **Setup > DSP > AGC/ALC**. Confirm that you are editing **RX1 AGC** for the intended receiver. **Mode** offers Off, Long, Slow, Med, Fast and Custom. Begin with a preset mode; Custom exposes the individual response controls.

- **Attack** sets how quickly gain responds when signal level rises.
- **Decay** sets the gain recovery time after a strong signal. Longer values hold down gain longer after a peak.
- **Hang** holds the gain state after a strong signal. **Hang Threshold** sets the level at which the hang behavior engages.
- **Slope** controls gain difference for weak and strong signals.
- **Max Gain** caps AGC gain even on weak signals. **Fixed Gain** is the receiver gain used when AGC is Off.
- **Auto AGC RX1** adjusts AGC from the measured noise floor. **± Offset** shifts its target from that floor in dB.

To reduce pumping from a nearby strong signal, compare a slower mode or longer decay and lower the maximum gain. To make weak signals rise sooner after a peak, reduce decay or hang in Custom. Read the controls back and compare on the same audio. Do not use receiver AGC settings to correct an antenna, preamp or overload problem. The page also contains **TX Leveler** and **TX ALC** controls, which are transmit processing; the RX selector does not make them receiver settings. See [chapter 14](14-voice-profiles.md) for TX processing.

## Select a noise reduction method

Use the selected slice's VFO **NR** control or **DSP > NR** to choose one receiver noise-reduction method. Start with the same wanted signal, filter, AGC and audio level for every comparison. Change only the NR choice, listen with it off and on, and keep the method only if the wanted speech or tone is clearer. A neural or platform-specific option can be unavailable while ordinary methods remain available; use its displayed readiness reason.

Choose a control that matches the symptom before changing an NR method:

| What you hear | First comparison | Readback and next step |
| --- | --- | --- |
| Clicks, pulses or other separate impulses | Compare **NB** and **NB2**; if needed, compare **SNB** separately. These blankers are separate from the NR selector. | Keep the wanted peaks intact. Use [chapter 3](03-receive.md#select-noise-treatment-by-the-problem) for the quick controls and blanker entry points. |
| One steady background wash | Compare one available NR method at a time, with the same speech and the same passband. | Check intelligibility and noise between words. If consonants disappear or processing artifacts become distracting, turn off the last method or restore its prior value. |
| One persistent carrier or tone | Compare **ANF** or a narrow manual **TNF** notch. | Confirm that nearby wanted audio remains. These tone controls are not NR methods; see [Add and adjust tunable notches](#add-and-adjust-tunable-notches). |
| A nearby station overlaps the passband | Narrow the affected filter edge before adding more noise processing. | Verify the wanted signal remains inside the passband, then repeat the listening comparison. See [chapter 3](03-receive.md#choose-and-adjust-the-receive-passband). |
| Speech becomes watery, clipped or unstable after processing | Bypass the most recently changed method or restore the recorded value. | Compare again against the unchanged baseline. Do not add another processor until the first change has been accepted or reversed. |

For a method comparison, use this sequence:

1. Select the receiver you control. Read back its mode, passband and AGC before changing DSP; keep those conditions fixed during the comparison.
2. Listen with NR off and note the wanted speech/tone and the noise between words. Select one candidate method from the NR control and check that its button/readback shows it active.
3. Compare the same signal with the method enabled. If a different method is offered, turn off the first before selecting the next; do not stack alternatives.
4. Keep the method only when intelligibility improves without unacceptable artifacts. If it is worse, turn it off or restore its previous tuning value, then confirm the NR readback.
5. If the option is disabled or steps back, follow the Core/build reason under [Neural receiver methods and model assets](#neural-receiver-methods-and-model-assets). A changed number or enabled indicator alone does not establish better reception.

The detailed entry point is **Setup > DSP > NR/ANF**. These controls apply to the selected receiver as you change them; they are not a staged profile editor. Use the row for the active method only:

| Method | Available tuning and scope | Use the readback to |
| --- | --- | --- |
| **NR1** | The VFO popup has **Taps**, **Delay**, **Gain**, **Leak** and **Position** (**Pre-AGC** / **Post-AGC**). The numeric units for the first four are not defined by this interface. | Make one small change and compare the same signal. Do not infer a control meaning from its name. |
| **NR2** | The popup has **Gain Method** (Linear, Log, Gamma, Trained), **NPE Method** (OSMS, MMSE, NSTAT), **AE Filter**, and **Noise post proc**. Setup provides **T1** (-5.0 to +5.0, default -0.5), **T2** (0.0 to 2.0, default 0.20), **Position**, and post-processing **Enable**, **Level**, **Factor**, **Rate** and **Taper**. The interface describes T1 as noise-estimator asymmetry; it calls T2 a training parameter without a further operator definition. | Compare AE Filter on/off on the same audio. Change one cascade value at a time and reverse it if speech artifacts worsen. |
| **NR3** | RNNoise with **Position** and **Use fixed gain for input samples**. The Core-wide model selector and **Models...** asset dialog are described below. | Read **Status** after a model request. Keep the last available method if the Core refuses the change. |
| **NR4** | **Reduction** (0-20 dB), **Smoothing** (0-100), **Whitening** (0-100), **Rescale** (0-12), **SNRthresh** (-10 to +10 dB), and **Algorithm** (Algo 1/2/3). | Compare one field at a time. The interface does not describe the audible distinction between the three algorithms. |
| **DFNR** | **Attenuation Limit** (0-100 dB) and **Post-Filter Beta**. The quick popup reaches 1.00; Setup limits Beta to 0.30. The Core/build status explains whether it can run. | Read the actual value at the entry point you use. Do not assume an unavailable control is enabled by its parent NR selection. |
| **MNR** | macOS MMSE-Wiener noise reduction. **Strength**, **Aggressiveness**, **Floor**, **Alpha**, **Bias**, **Gsmooth** and **Reset** are available in the popup and Setup. | Read the platform/readiness state, begin with **Reset** if you need a known comparison, and reverse any change that degrades speech. |

The VFO popup and Setup page can present different entry points or ranges. Read the value at the control you changed. Do not reuse a numeric interpretation or range from another method.

For the neural methods, compare against the same short speech passage and note the starting values. **DFNR** offers **Attenuation Limit** from 0 to 100 dB; its Setup **Post-Filter Beta** range is 0.00 to 0.30 while the quick popup reaches 1.00. Reduce attenuation if wanted peaks disappear. If the Core/build disables DFNR, follow its displayed reason. **MNR** has a **Reset** action and reports when the Core platform cannot run it. Neither status nor a saved value proves that the audio improved.

## Neural receiver methods and model assets

DFNR, NR3 and NNR use neural processing; MNR is a separate macOS MMSE-Wiener method. Their readiness paths differ. DFNR depends on build/runtime support and Core resources, MNR on the Core platform, and NNR on Core-managed Standard and Premium model slots. A phone requests Core-side processing; it does not run these receiver methods locally.

### Select or repair an NNR model

1. Open **Setup > DSP > NR/ANF > NNR**. Read the **Standard model** and
   **Premium model** selections, **Active** labels and **Status** before
   replacing a choice. NNR tuning is per slice; these model assignments and
   their files belong to the Core.
2. If a selected model is missing or incompatible, choose a bundled model or
   a valid compatible Core asset from the appropriate selector. A saved name
   is not proof that the Core can run the file.
3. To inspect or transfer an asset, choose **Models...** to open **NNR Model
   Files**. The table shows Label, Format, Encoding, Size, Identity and
   Compatibility. Select the row and expand **Details** for validation errors.
   **Refresh files** reloads the Core list; **Import...** transfers a local
   asset to the Core; **Export...** saves the selected Core asset locally.
   These actions do not download models from a network catalogue.
4. Select the intended compatible model, then inspect the pending/active
   readbacks. The pending choice takes effect through **Apply models and
   reconnect**. Do this off air: it reconnects the radio and the Core refuses
   it while transmitting.
5. After reconnecting, recheck the **Active** labels and **Status**, select the
   intended slice and repeat the audible comparison. If readiness still
   fails, retain an available receiver method while resolving the reported
   compatibility or runtime reason.
6. If you connect to a different Core during a transfer, reopen the asset
   dialog and load that Core’s files before another request. NR3, DFNR and
   MNR have separate model/readiness paths; this NNR procedure does not
   repair their assets.


The VFO **NNR** popup offers **Standard** or **Premium** and **Suppression** (-50 to -10 dB, default -25 dB). More-negative values permit stronger suppression. **Advanced** exposes **Position**, **Alpha**, **Alpha knee**, **Noise time**, **Maximum gain**, **Attack** and **Release**. **Reset tuning** restores these tuning values without changing the model assignment. The **Diagnostics** group is temporary until reconnect: **Runtime**, **Models**, **Rate / latency**, **Source** and **Status** report Core processing state. **Test Mode** offers **Network**, **Identity** or **Low-pass**; **Output Mode** offers **Duplicate I/Q** or **Q zero**. **Apply until reconnect** applies these diagnostic modes to the current session. Restore **Network** and **Q zero** before resuming normal reception; reconnect also resets the diagnostic choices. These diagnostics are status checks, not routine audio adjustments.

| NNR control | Range and default | Meaning shown by the controls |
| --- | --- | --- |
| **Suppression** | -50 to -10 dB; -25 dB | More-negative values permit stronger suppression. |
| **Position** | **Pre-AGC** or **Post-AGC** | Places NNR before or after automatic gain control. |
| **Alpha** / **Alpha knee** | 0-4 (1); 0-40 dB (10 dB) | Alpha reshapes gains below the knee. 1 leaves them unchanged; higher values deepen them and lower values lift them. |
| **Noise time** | 0.05-30 s; 2 s | Time constant for normalizing input power before the neural network. |
| **Maximum gain** | 0-24 dB; 12 dB | Limits gain the neural stage may add. |
| **Attack** / **Release** | 0-500 ms each; 0 ms | Set how quickly suppression engages and relaxes. Zero uses the model response. |

For ordinary NNR tuning, **Position** chooses processing before or after AGC. **Alpha** reshapes gains below **Alpha knee**: 1 leaves them unchanged, above 1 deepens them, and below 1 lifts them. **Noise time** sets the input-power normalization time constant; **Maximum gain** limits added gain. **Attack** and **Release** set suppression transitions, with zero using the model response. For a repeatable comparison:

1. Select the intended slice and listen to steady speech with NNR off. Note consonant clarity and background noise, then select a ready Standard or Premium model and enable NNR.
2. Confirm the active model/runtime readback. Start with **Reset tuning** if you need a known baseline, then adjust **Suppression** and compare the same speech with NNR enabled and bypassed.
3. Change one advanced value at a time and note its prior value. Compare **Alpha** with **Alpha knee** as a pair; compare **Attack** and **Release** during speech transitions. Keep diagnostic Test/Output modes at **Network** and **Q zero**.
4. If consonants, level stability or transitions worsen, restore the recorded value or choose **Reset tuning**, then repeat the comparison. The status indicator confirms processor state, not audio quality.

If Core resource protection steps NNR back, read the reason beside the warning. On the phone, **Modes > Noise** or the **RX** panel offers **Try again** when the Core reports that action. Retry after the stated resource/load condition clears; the Core can step the method back again if it remains constrained. An older Core may show a compatibility reason and omit retry. Phone **NNR model** buttons choose the Core-reported Standard or Premium slot; **Reset** resets the available tuning controls for this receiver. After retry, read back the selected method and compare the sound again. Readiness does not prove improved audio.

## Noise blankers and adaptive notch

Choose **DSP > NB** to cycle Off, NB and NB2. The detailed controls are at **Setup > DSP > NB/SNB**. **NB1 Threshold** ranges 1-1000 (default 30); a lower value detects weaker impulses and is more aggressive. **Transition**, **Lead**, and **Lag** each range 0.01-2.00 ms (default 0.01 ms): transition controls the fade toward/from zero around an impulse, lead blanks before the detected impulse, and lag remains at zero after it. **NB2 Mode** selects how NB2 fills blanked samples: **Zero**, **Sample & Hold**, **Mean-Hold**, **Hold & Sample**, or **Linear Interpolate**. There is no NB2 threshold control in this page. NB1/NB2 settings are shared by receivers on the same DDC; SNB settings apply to the selected receiver only. **SNB Threshold 1** ranges 2.0-20.0 (default 8.0) and marks candidate outliers relative to running noise power. **Threshold 2** ranges 4.0-60.0 (default 20.0) and confirms candidates; lower values increase aggressive blanking. **Output Bandwidth** ranges 100-96000 Hz (default 6000) and sets the width of audio SNB processes, centered on zero. Change one field at a time and check that wanted peaks remain intact.

Choose **DSP > ANF** to toggle the Automatic Notch Filter on the selected slice. The **Setup > DSP > NR/ANF > ANF** tab has a live **Enable ANF** checkbox wired to the same slice setting as the quick toggle. Advanced ANF taps, delay, gain and leakage tuning is not exposed. Do not apply NR1 parameter meanings to ANF. ANF is separate from manual TNF notches. Compare the quick toggle on and off for a stable unwanted tone and check nearby wanted audio.

## APF for CW reception

For CW reception, choose **CWL** or **CWU**, set the receive filter and pitch, then enable **DSP > APF**. **Setup > DSP > CW** provides the APF enable and center-frequency controls. The phone Modes page also offers **APF tune**; its slider works only in CW with APF on. APF bandwidth and gain controls are not implemented. Align the center/tune readback with the desired tone and compare the audio. The CW Setup page can contain unrelated hidden or unavailable keyer/timing items; the presence of a CW receive mode does not establish CW transmit or a built-in keyer. Use the controls the application actually offers.

## Add and adjust tunable notches

Use **DSP > TNF** to enable or bypass the tunable notch filter. To create a notch, use the spectrum's notch context action at the interfering signal, or open **Setup > DSP > TNF** and press **Add**. The spectrum context route targets the frequency under the pointer; the Setup **Add** route creates a row that can be precisely edited. Do not drag a spectrum marker to create a notch unless the displayed build explicitly offers that gesture.

In the TNF table, each row has **Center Frequency (Hz)**, **Width (Hz)**, **Active**, and **Delete**. Change center to place the notch over the unwanted carrier, adjust width only as far as required, and leave **Active** checked to apply it. **Minimum Notch Width** reports the minimum supported width for the current receiver. **Auto-Increase width (if needed) to achieve >100dB attenuation** lets the filter widen a narrower request to meet the stated attenuation. **Visual approximation of notch** affects the spectrum display approximation, not the received audio response. Use the row's Delete action to remove one notch. The notch table is a persistent station-level set fanned out to active receiver channels, not an independent per-slice list. The master TNF switch bypasses all stored notches together; leave individual rows intact for a temporary global bypass.

## Set filter edges and edit the shared preset bank

The active slice's filter edges are independent from the stored preset bank. Use the VFO passband controls or the filter preset buttons to set the current receiver. A normal preset click applies that preset's low/high edges to the active editable receiver. It does not change every slice.

To edit a preset bank, open **Setup > DSP > Filter Presets**. The bank is a Core/station setting, including when accessed from a remote desktop. Choose the **DSP Mode** whose rows you intend to edit. The table has ten slots per mode and shows **Name**, **Low (Hz)**, **High (Hz)**, **Width (Hz)** and **Reorder**. Edit a row's name or edge values and finish editing the field to save the row. Width is read from the edge values. Reorder controls change the order in which the presets are presented. Confirm the rows again after changing the mode selector.

Use **Reset Selected Row** to restore the selected slot, **Reset All Rows for This Mode** to restore that mode's bank, or **Reset Every Mode to Thetis Defaults** to reset the full bank. These actions discard custom bank values; inspect the selected mode/row before reset and verify the table after the confirmation. A stored bank edit is not itself a request to retune every receiver.

Use **Shift-click** on a receive filter preset when you want a one-time **Match RX** operation. It applies the receive preset and copies the resulting passband once to the TX audio filter. For AM, SAM, DSB, FM and DRM the TX range becomes zero to the absolute high edge; other modes use the absolute RX edges in ascending order. The RX change happens first. If the TX settings write is refused, the RX preset remains applied and the TX filter does not change. There is no continuous RX-follow-TX checkbox. On phone, tap **Match RX** in the TX panel or the Modes transmit section to perform the same one-time conversion. See [chapter 14](14-voice-profiles.md) for other TX filter controls.

## DSP Options: buffer and filter tradeoffs

Open **Setup > DSP > Options** and select the RX row for the mode you are using. Each mode group has its own saved RX values and, except CW, TX values. The controls serve different tradeoffs:

| Control | Available values | Effect to compare |
| --- | --- | --- |
| **Buffer Size (IQcomp)** | 64, 128, 256, 512 or 1024 samples | Larger buffers can produce sharper filters, with more interaction latency. |
| **Filter Size (taps)** | 1024, 2048, 4096, 8192 or 16384 taps | More taps sharpen filter skirts, with added CPU work and latency. |
| **Filter Type** | **Linear Phase** or **Low Latency** | Linear Phase has symmetric delay. Low Latency is minimum phase and has less group delay. |

To choose a setting for the current mode:

1. Start off air. Read the current mode group, RX row and values. The Buffer Size group is locked while transmitting; a remote Core can also refuse a change while keyed.
2. If an adjacent signal is entering the receiver passband, first set appropriate filter edges as described in [Set filter edges and edit the shared preset bank](#set-filter-edges-and-edit-the-shared-preset-bank). If you still need sharper skirts, raise **Filter Size (taps)** by one available step and compare the same adjacent-signal condition. Change **Buffer Size (IQcomp)** only when its sharper response is worth the extra interaction delay.
3. If tuning or mode changes feel delayed, compare **Low Latency** with the current filter type. Keep the buffer and tap count fixed during that comparison so the result has one cause.
4. Read back the changed row. When the active slice mode matches the edited group, wait for the DSP rebuild and check **Time to last change**. A setting for another mode takes effect when a slice uses that mode.
5. If sharpness, latency, CPU behavior or audio worsens, restore the recorded value and read it back again. Warning icons indicate differences among mode groups, not a failure.

The defaults are 64 samples / 4096 taps / **Low Latency** for SSB/AM, 256 RX and 128 TX samples / 4096 taps / **Low Latency** for FM, 64 / 4096 / **Low Latency** for CW RX, and 64 / 4096 / **Low Latency** for Digital RX/TX. CW has no TX row.

**Enable impulse caching** keeps filter responses in memory to speed later channel rebuilds at the cost of memory. **Keep impulse cache on disk between launches** saves and reloads the cache, which can avoid the first rebuild cost after restart but can create a large cache file. Both take effect on the next radio connect or channel rebuild. **High-resolution filter characteristics in filter graph** shows the computed FIR response instead of a simplified box-shaped passband; it changes the graph, not the selected filter.

## Related controls and availability limits

Setup also contains **AM/SAM** and **FM** receiver sections. Use the enabled AM/FM receive squelch controls only when the corresponding mode is selected. SAM fade and other disabled groups are not enabled by selecting SAM. FM transmit, tone and repeater controls may be absent because FM TX is not implemented in this build. The **DSP > Equalizer** item is not the TX equalizer; use **Tools > TX Equalizer** for transmit audio processing. For TX leveler, ALC, CFC, EQ, VOX and DEXP, see [chapter 14](14-voice-profiles.md).

Quick controls also exist on iPhone/iPad in **Modes** and the **RX** panel. They alter the Core's selected receiver; device-local audio controls affect only phone playback. If a subcontrol is not offered by the Core description, a working parent method does not imply that missing subcontrol is supported.
