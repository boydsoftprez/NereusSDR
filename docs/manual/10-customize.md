# Display, meters, and desktop layout

This chapter covers what the spectrum and waterfall show, how to make weak signals easier to read, how to inspect recent waterfall history, and how to build a desktop meter container. Display tuning changes presentation and analyzer requests. It does not change the receiver's demodulation, filter, transmit power, or RF path. Where a control changes a shared analyzer or Core setting, its scope is called out.

For the main operating window, slice selection, spectrum tuning, and context menus, see [chapter 2](02-desktop-window.md) and [chapter 3](03-receive.md). Receiver DSP and filter changes are in [chapter 13](13-receiver-dsp.md); transmit setup is in [chapter 5](05-transmit.md), voice processing in [chapter 14](14-voice-profiles.md), and radio calibration in [chapter 17](17-hardware-antennas-calibration.md). For the phone's full display and session preferences, see [chapter 20](20-phone-preferences.md), with touch operation in [chapter 7](07-iphone-operate.md). Chapter [12](12-reference.md) indexes scopes and availability. Related advanced procedures: [RADE and FreeDV](15-rade-freedv.md), [spots and reporters](16-spots-reporters.md), [PureSignal and diversity](18-puresignal-diversity.md), and [amplifiers and tuners](19-amplifiers-tuners.md).

## Read the display without changing the receiver

A panadapter is a live FFT view over the selected receive window. The waterfall paints successive FFT results vertically so time runs down the display. The trace and waterfall represent received energy; changing their color, scale, averaging, or history does not retune the slice. A trace may look smoother or more prominent after display changes while the received audio remains the same.

On desktop, use **File > Settings... > Display**. Its pages are **Spectrum**, **Levels**, **Waterfall**, **View**, **Peaks & Markers**, **FFT & Engine**, **Transmit Display** and **Multimeter**. Each pan also has a **Display** pill, opened from the **Display** button on the pan's strip, with **Spectrum**, **Levels**, **Waterfall**, **View** and **FFT** tabs; **Setup defaults →** on a tab opens the Setup page that holds that tab's defaults (the **FFT** tab's opens **FFT & Engine**). Setup sets the default for every pan and the pill changes one pan. On the first five pages a **Settings for:** strip chooses **All pans** or one pan. A value a pan has set for itself is marked with an amber dot; click the dot, choose **Reset this pan**, or set the value back to the default to make that pan follow Setup again. Most choices are saved presentation preferences; some analyzer choices are shared or sent to the Core. Remote desktop settings can be held when the Core does not describe or grant the control. Read the disabled control's explanation and its scope before trying another value. Do not interpret a gray field as a radio fault.

The right-click menu on a panadapter is short. **Display Range** has **Ref Level** and **Dyn Range**, which move the pan's range for its current band, the same band value the pill's **dB Max** and **dB Min** edit. While the pan is transmitting, they move the transmit grid instead and store nothing for the band. **Tuning** has **CTUN (independent pan)**, which enables center tuning when available. **Notch** has **Add notch here**. The **Display settings...** button opens the pan's Display pill, where the spectrum, waterfall, 3D view and FFT controls now live. Palette, color gain, black level, fill and the 3D controls are no longer in this menu. If a command is unavailable, use its displayed capability reason rather than assuming it will work on every radio/Core.

The spectrum's right-hand dBm scale also has direct gestures. Click its small up/down controls to move the reference level by 10 dB while keeping the bottom fixed. Scroll over the scale to adjust dynamic range in 5 dB steps. Drag with the left button to move the displayed range, or right-drag to stretch it. Ctrl/Command-drag zooms that range. Away from the scale, plain scroll tunes by the selected step, Shift-scroll moves reference level, and Ctrl/Command-scroll zooms spectrum bandwidth around the active VFO. On macOS, Command is accepted for the Control-modified interactions. Click-drag the frequency scale to change displayed bandwidth; drag the spectrum/waterfall field to pan. A short click in the pan tunes, subject to slice ownership and lock. Use the visible menu labels if the platform presents a different modifier glyph.

## Make a weak signal easier to see

Use this sequence when a signal is present in audio or known to be in the receiver passband but is hard to distinguish visually. It changes the view only.

1. Select the intended slice and panadapter. Open the pan's **Display** pill and its **Levels** tab (or **File > Settings... > Display > Levels**) and note the current **dB Max** and **dB Min**. They are kept for each band, so these per-band limits determine how much vertical signal range is visible. Narrow the displayed range around the current noise floor and signal, then check the dBm labels and ensure stronger nearby signals are not clipped. If the noise floor moves with conditions, set **Spectrum scale** to **Track floor** and set **Floor Offset**; dB Min then follows the live noise estimate plus that offset and is grayed. **Keep range** also moves dB Max so the span stays the same, and grays dB Max too. Set the scale back to **Manual** if tracking hides the region you need.
2. Open the **Waterfall** tab (or **Waterfall** in Setup) and choose a **Scheme** with useful contrast. Then, under **Waterfall levels** on **Levels**, choose how the colors are set. **Clarity** sets Low and High from the noise floor. **Auto** sets Low from the weakest signals and you set **High**. **Floor** puts Low at the noise floor plus **NF Offset** and you set **High**. **Manual** lets you set **Low** and **High**. **Scale** uses the spectrum's dB Min and dB Max. **Black Level** and **Color Gain** stay available in every choice except while the pan is transmitting. These choices change waterfall colors only; they do not change receiver AGC or audio gain.
3. For adaptive tuning, choose **Clarity** under **Waterfall levels** on **Levels**. Clarity sets Low and High from the noise floor, so compare its result with manual levels; choose **Manual** before expecting thresholds to stay fixed. **Re-tune** restarts its measurement. After a transmission Clarity waits before it measures again, about 11 seconds at a 262144-point FFT and 48 kHz. **Reset to Smooth Defaults** (on Setup's **Spectrum** page) asks for confirmation, then applies the smooth display profile to the pans chosen in **Settings for:** (All pans, or the one pan): Clarity Blue palette, log-recursive spectrum averaging at 650 ms, a white trace without fill, and a 30 ms waterfall rate, and it changes Manual waterfall levels to Auto. It does not change FFT size, frequency, band stack, or per-band grid ranges. Cancel the confirmation to keep the current profile; otherwise inspect the affected trace and waterfall and adjust from the **Spectrum**, **Levels** and **Waterfall** pages.
4. Return to the panadapter and check the trace, passband, dBm labels, and waterfall together. If a peak disappears, widen the grid range or move the reference level. If waterfall noise is washed out, adjust the levels, **Black Level** and **Color Gain** on the pan's **Levels** tab. Change one group at a time so the useful setting is easy to identify and undo.

### Set spectrum resolution, update rate, and smoothing

Open **File > Settings... > Display > FFT & Engine**, or the **FFT** tab of the pan's Display pill. Read both **FFT Size** and **Bin Width (Hz)**. The sizes run from 4096 to 262144 in powers of two. Bin width is sample rate divided by FFT size: a larger transform makes narrower bins and finer frequency detail, at greater processing cost. **Sizing** chooses what sets the FFT size. **Auto** lets the zoom pick the size to keep the picture sharp. **Hz/bin** makes the size follow the bin width in **Hz/bin Target** (0.5 to 200 Hz) at every zoom. **Fixed** uses the **FFT Size** slider's size whatever the zoom. **FFT Size** is live in **Fixed** only and **Hz/bin Target** in **Hz/bin** only; grayed, each shows what is running or remembered and says which **Sizing** choice frees it. These FFT settings are shared: one value serves every pan and every window connected to the Core, and a change on one pan's FFT tab shows on all of them. The **Window** choice changes leakage versus main-lobe width: Rectangular is narrow but has stronger sidelobes; Hann/Hamming/Blackman-Harris/Kaiser trade some width for sidelobe reduction; Flat-Top favors amplitude accuracy. After a change, read the actual FFT/bin-width value; remote display may show a Core grant different from the request.

**FPS** (on **FFT & Engine**, and the pill's **FFT** tab; 10 to 60) sets the approximate redraw rate, trading animation smoothness against CPU work. **Detector** (on **Spectrum**, in the **Trace** group) selects how input bins become display pixels: **Peak** keeps the maximum, **Rosenfell** alternates max/min for a classic trace, **Average** averages bins, **Sample** selects one bin, and **RMS** computes root-mean-square power. **Averaging** is time smoothing across frames: **None** shows each frame; **Recursive** smooths linear power; **Time Window** approximates a sliding window; **Log Recursive** smooths in dB. **Avg Time** sets the smoothing time constant (1 to 9999 ms in Setup; the pill's slider stops at 500 ms); longer time smooths more and responds more slowly. Try a detector for the view shape first, then choose averaging and time based on how quickly you want the trace to follow changes.

**Normalize to 1 Hz** is available for Average, Sample, or RMS detectors. It scales displayed noise power to a 1 Hz reference bandwidth; it is not a receiver calibration adjustment. The checkbox is grayed, with the reason, when the chosen detector does not support it. **Decimation** (on **FFT & Engine**) reduces display resolution as its value increases. Decimation is applied to the FFT engines across pans; use it only when lower display detail is acceptable to reduce processing load. **Cal Offset** (Spectrum page, **Calibration** group) shifts displayed levels on a local radio, but is disabled in a remote window because the Core already calibrates its radio data. **Peak Hold** and **Peak Delay** (Spectrum page, **Peak hold** group) are a separate whole-trace maximum hold; the delay controls when the held trace starts to decay. For the peak trace with decay and the labeled peak markers, use **Configure peaks →** to open **Peaks & Markers**.

The **Noise Floor (NF)** overlay is a visual estimate. Its shift moves the rendered line, line width changes its visibility, and color controls change its appearance. It is not a calibrated measurement or receiver threshold. **Cursor Freq** is under **Readouts** on **Spectrum**. **Bin Width**, **Peak Value** and **Noise Floor** are on **Peaks & Markers**: **Peak Value** can be placed in a corner and given an update interval (the corner list and the ms box beside it), and **Noise Floor** has **NF shift**, line width and color settings. These overlays are presentation aids, not receiver thresholds or calibrated measurements.

### Hold peaks and label strong signals

Open **File > Settings... > Display > Peaks & Markers**. Use **Peak trace with decay** for a second, dashed per-bin trace: enable it, choose **Hold duration**, then set **Drop rate** for the fall after each bin's hold expires. **Fill to trace**, **Update during TX**, and **Trace color** control its presentation and operation during transmit. Check that the colored held trace decays while the live trace continues.

Use **Peak markers** for circle markers on the highest spectrum peaks. Enable **Peak markers**, set **Number of peaks** from 1 to 20, and optionally restrict markers to the current RX passband (**Inside RX filter only**). The value beside each circle uses **Text color**; **Blob color** changes the marker. There is no user-set amplitude threshold or minimum frequency separation control: this page selects the number of highest peaks, not a list of threshold-qualified stations. To retain a marker at its maximum, enable **Hold before decay** and set the duration. **Decay after hold** makes it fall at **Fall rate** dB/s; with decay off it disappears when the hold ends. Change these settings while watching a known peak, then verify marker count, passband filtering, and hold/fall behavior on the live pan. These marks do not establish signal identity or decodability.

## Waterfall levels, timestamps, and rewind

### Tune waterfall levels, row processing, and time labels

Open **File > Settings... > Display > Levels** for the waterfall's levels and **Waterfall** for its drawing, smoothing, overlays and time. Under **Waterfall levels**, **Clarity**, **Auto**, **Floor**, **Manual** and **Scale** choose how the colors are set. In **Manual**, **Low** and **High** map the low and high signal colors; set them around the background noise and signals. In **Scale**, Low and High are the spectrum's dB Min and dB Max, so they are grayed. **Auto** sets Low from the weakest signals and lets you set **High**; **Floor** puts Low at the noise floor plus **NF Offset**. These affect waterfall colors only, not receiver AGC. **Copy from spectrum scale** and **Copy from waterfall levels** copy the endpoints between the two. On **Waterfall**, the live **Delay** readout shows the approximate time span in the visible waterfall; **Stop on TX** freezes new waterfall rows during transmit and resumes after unkeying. **Opacity** blends the waterfall with the spectrum background. **Color Scheme** selects the palette (the pill and the phone call it **Scheme**). With **Custom** chosen, **Edit Custom gradient…** opens a small editor for the receive waterfall's gradient; the transmit waterfall's is on **Transmit Display**.

In the **Smoothing** group on **Waterfall**, **WF Detector** offers **Peak**, **Rosenfell**, **Average**, and **Sample** (waterfall has no RMS choice). These reduce FFT bins into each waterfall pixel/column. **WF Averaging** uses **None**, **Recursive**, **Time Window**, or **Log Recursive** to smooth successive waterfall frames/rows; its **WF Avg Time** is independent from the spectrum averaging time. Longer time suppresses rapid change and leaves more temporal persistence. Choose the detector for how narrow signals render, then adjust averaging while comparing a changing signal so detail does not smear more than you intend.

In **Overlays**, enable RX filter edges, TX filter edges on the RX waterfall, and/or RX/TX zero lines to compare passbands and center-frequency references against stored history. In the **Time** group, **Timestamp Position** chooses **None**, **Left**, or **Right** for each row; **Timestamp Mode** chooses **UTC** or **Local** (the pill says **Timestamp**; the phone says **Timestamp** and **Time Zone**). Enable a side and select the time basis, then compare a newly painted row to the desktop clock. Historical rows retain their own time; the label is not a live tuning indicator. **Depth** (the pill and phone say **Rewind**) requests 60 seconds, 5, 15, or 20 minutes. Read the rewind line under **Depth** ("Keeps ... at this Rate") after changing depth or update period: the ring is capped at 16,384 rows, so faster updates can shorten actual history. Update period also changes the waterfall's temporal detail. Set the requested depth, then adjust update period and check the effective readback again.

Both waterfalls have a custom gradient editor. For the receive waterfall, choose **Custom** as the **Color Scheme** on **File > Settings... > Display > Waterfall**, then click **Edit Custom gradient…** (it is grayed, with a reason, until Custom is chosen). For the transmit waterfall, do the same on **Transmit Display**. In the editor, click to add a stop, drag a stop to move it, double-click it to change its color, or right-click a middle stop to remove it. The two end stops stay in place. Confirm the scheme remains **Custom** and inspect the live waterfall before relying on the gradient. The receive and transmit gradients are separate, and each is kept on this computer.

To look back on desktop, use the narrow time-scale strip at the waterfall's right edge. Left-click **LIVE** to pause at the current view. While paused, drag the time scale upward to move farther back and downward toward the present. The scale labels show elapsed seconds while live and absolute UTC timestamps while paused. You can also drag into history directly. The view remains paused while new live rows arrive; the historical position is held. Click the enlarged **LIVE** button to return to current data. Rewind is a stored picture: tuning and audio continue live and are not rewound. Disconnecting clears the ring buffer.

On iPhone/iPad, open **Panadapter > Display** and drag the **Look back** slider away from zero to move the waterfall to an older position. The band displays a **LIVE** button while looked back; tap **LIVE** to return to the newest row. New rows continue arriving while the view is back in history. To set the maximum buffer, open **More display options in Setup > Waterfall: more > Look back up to**; the footer reports the retained interval at the current update period. This adjusts how far back the control can travel, while the slider selects the current viewing position. Timestamps can show **UTC** or **This phone's** time zone. Tuning and audio remain live while the picture is historical. See [chapter 20](20-phone-preferences.md) for mobile display settings.

## 3D, duplex, and transmit display

### Switch to and read the 3D stacked trace

On desktop, open the pan's Display pill, go to the **View** tab, and choose **3D Stacked Trace**, or open **File > Settings... > Display > View** and choose it there. The 3D surface stacks recent spectrum traces; the ordinary waterfall remains the time history beneath it. In the page, adjust **3D Floor** to set how far below the estimated noise floor the surface begins, **3D Gain** to move the color range toward weaker or stronger signals, **3D Span** to control how far near traces extend beyond plot edges, **3D Angle** to choose edge-on versus top-down perspective, **3D Speed** to set how fast the stack moves (**Match** at 0 follows the waterfall rate), and **Slice Shadow** (**3D Slice Shadow** in Setup) to project slice passbands onto the surface. Floor is per-band; the other display geometry controls apply to the view. The pill changes the one pan; verify in that pan. **Background** (picture, color and opacity) is also on **View**, and it is not drawn in the 3D view.

The pill's **View** tab has the 3D controls: floor, gain, span, angle, speed and slice shadow. To change floor from the plot, drag vertically on the right dBm strip below its arrow controls; while 3D is active this gesture changes floor depth, and the strip's readout shows the new value. There is no free mouse-orbit or perspective zoom control: adjust **3D Angle** on the pill or in Setup. The ordinary bandwidth zoom gestures still change frequency span, not camera perspective. To return to the shipped state, choose **Reset 3D to defaults** on Setup's **View** page and confirm. It restores **2D Waterfall**, 6 dB floor, 70% gain, 100% span, 50% angle, **Match** speed and slice shadow off; a pan with its own value keeps it. The reset is limited to these 3D view settings.

The 3D surface draws recent rows, so **WF Detector**, **WF Averaging**, and **WF Avg Time** still affect its incoming waterfall history. **Decimation** (FFT & Engine) lowers FFT display resolution and is applied across pans; it can reduce the detail visible in each stacked trace. **Depth** (Waterfall page, **Time** group) controls the separate flat-waterfall lookback buffer. The 3D stack uses its own fixed-row ring; changing the rewind depth or its effective-depth readout does not add 3D rows. The waterfall update period affects the time represented by successive stack rows. If the surface has little time depth, compare the update period and let its live stack fill; if it looks coarse, check decimation before raising FFT size. On a remote radio, read capability reasons for unsupported settings. In the mobile quick sheet, select **View > 3D Stacked Trace** when offered; its **3D Floor**, **3D Gain**, **3D Span**, **3D Angle**, **3D Slice Shadow**, and confirmed **Reset 3D to defaults** appear there.

**View > Display duplex (DUP)** controls whether the receive display remains visible during transmit when the current analyzer and radio allow it. The TX applet's **DUP** button, next to **PS-A**, and a container's **DUP** button change the same setting, and all three are grayed with the same reason when duplex is not available. DUP is a display behavior; it is not full duplex transmit/receive capability. The two-tone IMD readout needs DUP: with **Show 2Tone measurements** on, the rail above the spectrum offers **Enable DUP** when it is the missing piece. FDX remains unavailable in this build.

**File > Settings... > Display > Transmit Display** configures the station's transmitter analyzer. Its **FFT** group's size slider selects 4096 through 262144; read the FFT size and bin-width fields after the Core returns its value. **Window** choices are Rectangular, Blackman-Harris 4T, Hann, Flat-Top, Hamming, Kaiser, and Blackman-Harris 7T. Under **Panadapter**, choose detector (**Peak**, **Rosenfell**, **Average**, **Sample**, or **RMS**), averaging (**None**, **Recursive**, **Time Window**, or **Log Recursive**), and the averaging time. **Normalize to 1 Hz** is only enabled for compatible pan detectors. Under **Waterfall**, choose its detector (**Peak**, **Rosenfell**, **Average**, or **Sample**), averaging mode, and independent time. The amplitude scale's low/high levels and palette affect the TX waterfall. These analyzer settings are station/Core settings in a remote window and can affect every watcher; wait for their values/readbacks after a write. Use these controls only when the supported TX analyzer has data. A no-data banner means there are no current TX analyzer samples, not that RX display has failed. The rest of the desktop TX grid (step, grid, fill and label alignment) is unbuilt. Under **Transmit scale**, **dB Max** and **dB Min** set the transmit scale, as on the phone's page; a pan that is transmitting is drawn with the transmit scale and the transmit levels until it stops. The mobile TX page has a local grid and palette alongside Core analyzer controls; follow its scope badges rather than transferring desktop procedures.

## Set grid range and labels

The grid is set on two pages. On **File > Settings... > Display > Spectrum**, the **Grid** group has **Grid** (on or off, with the frequency numbers), **Grid Spacing** (Auto or a fixed step from 1 kHz to 100 kHz) and **dB Step**, which sets the spacing of the horizontal dB lines (1 to 40 dB, default 10). The **Scales and labels** group has **Scale Text** (text size), **Freq Label Align**, **dBm scale strip** and **Show zero line**. On **Levels**, **dB Max** and **dB Min** set the signal level at the top and bottom of the display for the band named beside them; each band retains its own pair, and the **Band:** picker on that page edits another band's pair without tuning the radio when **Settings for:** is on All pans or on a pan that is not open. Set Max and Min for the active band, check the band named, then switch bands and verify that the other band's range remains appropriate. These are Setup defaults for every pan; use the **Settings for:** strip, or the pan's Display pill, to set one pan.

**Freq Label Align** places frequency callouts Left, Center, Right, automatically, or Off; **Show zero line** draws the horizontal 0 dBm reference; **Show FPS overlay** adds the display's frame-rate readout. On **Levels**, **Spectrum scale** is **Manual** or **Track floor**. With **Track floor**, dB Min follows the noise-floor estimate plus **Floor Offset** (-60 to 60 dB). A negative offset places the minimum below the noise floor; a positive offset places it above. **Keep range** moves dB Max too, preserving the dB span, and grays dB Max. Tracking needs a usable live floor estimate. If an automatic range hides a signal, set the scale back to **Manual** or adjust the offset, then set the per-band endpoints yourself. **Copy from waterfall levels** copies the waterfall's Low and High into the spectrum scale; **Copy from spectrum scale** does the reverse. Inspect the band header and scale after using either.

## Colors and meter styles

Open **File > Settings... > Appearance > Colors & Theme** for grid, text, band-edge, zero-line, passband and related display colors. **Trace Color** and **Fill Color** are on **File > Settings... > Display > Spectrum** (Trace and Fill groups) and are two separate choices; until you choose a trace color, the trace uses the fill color. A color chosen on either page is the default for every pan, and a pan with its own color keeps it. **Reset all colors to defaults** returns the nine colors on Colors & Theme to their defaults. Color choices are local presentation. The custom waterfall palette is edited on **Waterfall** (**Edit Custom gradient…**).

**Appearance > Meter Styles** selects the S-meter face/style and its peak-hold and decay presentation. S-meter peak hold is implemented. For container meters, open **File > Settings... > Display > Multimeter**. **Polling delay** sets how often live meter values are sampled (10 to 2000 ms): shorter responds faster and costs more CPU. **Show decimal point in readouts** controls a decimal digit in S-meter/dBm text values. **Display units** applies S, dBm, or uV to supported meter items; read the item after changing units to confirm formatting. `S` uses S1 to S9 plus dB above S9, dBm reports dB relative to 1 mW, and uV reports microvolts at 50 ohms. The implemented controls include polling delay, decimal presentation, units and **History duration**, which updates existing container History Graph items. Peak/text/digital hold controls, the global history-enable checkbox and meter averaging are hidden/unbuilt. The separate container **History Graph** has a live first-series producer; create and bind that item as described below. The hidden global checkbox is not how you enable it. PBSNR is not a supported live meter value in this build. A designer property name or saved layout entry does not establish live data availability.

## Arrange applets and containers

The **Containers** menu controls installed applets and desktop containers.
A container can hold meters, existing applets and individual controls. Its
layout arranges those views; it does not create a receiver or grant transmit
rights. **View > Pan Layout** separately arranges panadapter windows.

The container editor is a draft editor with a preview and **Apply**, **OK**
and **Cancel**. Follow the procedure below; the historical figures do not show
this Canvas UI.

### Create and configure a source-associated container

This example adds a signal readout and clock, checks their sources, then saves
and applies a reusable arrangement. Begin in receive with the desired slice
running; keep keying controls unused during layout work.

1. Choose **Containers > New Container...**. In **Container Settings**, name the
   container and choose its **Slice** association. Select **Panel**, **Overlay**
   or **Floating** for its placement. Choose **Vertical stack** for automatic
   rows, **Canvas** for individual positions, or **Saved legacy canvas** to
   retain an imported normalized composition.
2. In **Available**, add **Signal text** and **Clock**. Select each under
   **Contents** to see its **Properties**. The preview updates as you edit,
   while **Pending Apply** tells you the arrangement is still a draft.
   Preview meters can show cached readings; applet previews and meter controls
   are inert. Their appearance does not authorize tuning or transmission.
3. For a source-aware meter, read **Advanced bindings > Source**. Choose
   **Container default** to follow the container's slice, or an explicit
   slice when that object must follow another receiver. A clock uses local
   time independently of the radio/MMIO source. Existing applets generally
   keep their current radio and slice; their disabled source controls explain
   this. Moving an RX applet does not retarget it to the container's meter source.
4. Set the object's available appearance/readout properties. Composite faces
   have calibrated source scales; a fixed min/max is not an editable custom
   meter range. **Custom bar face** exposes its own range. Signal units and
   decimal precision follow the global multimeter preferences. Check a live
   reading against the intended slice after applying; unavailable telemetry
   must not be interpreted as zero signal or zero RF.
5. Set the container's title/header, border, **Show RX**, **Show TX**, **Auto
   height** as required; the workspace look has no **Lock** setting, and the
   header choices are **Always visible** and **Hidden**. **Minimizes** and
   **Hide when RX unused** are unavailable in this editor, with stored
   preferences retained.
   **Highlight** is a live arrangement aid and is disabled in drafts. A disabled
   preference is not an implemented visibility rule.
6. Click **Apply**. Success says **All drafts saved.** It commits the pending
   container drafts and updates the live workspace without closing the editor.
   **OK** applies and closes after a successful save. **Cancel** discards edits
   since the last successful Apply; it does not undo committed changes. To undo
   an applied layout, restore a saved copy or make a deliberate new edit.
7. Use **Save…** to write a `.nscontainer` file containing the selected
   container and its contents. This exports the current draft; exporting is
   not applying it to the live workspace. Keep that copy before importing or
   replacing an arrangement you want to recover later.

If Apply reports a conflict because the workspace changed elsewhere, read
which containers conflict. **Reload conflicting containers** adopts their
current committed state while retaining unrelated draft edits. Inspect the
result and retry Apply. A storage/save failure leaves a reason and is not a
successful commit; do not assume the preview is now the persisted layout.

### Put TX applet rows or a large frequency readout in a container

**Available** also lists the TX applet's rows as separate items: **TX
Monitor** (MON, its outputs and volume), **TX Power** (the power and SWR
gauges with the RF Power and Tune Power rows), **TX Filter** (the transmit
filter cutoffs), **TX Chain** (LEV, EQ, CFC and the TX profile), **TX VOX**,
**TX Processing** (2-TONE, PS-A, DUP and the SWR protection light) and **TX
Keying** (TUNE, MOX and the transmit slice row). Add one to keep that row where
you want it. Each copy shows and sets the same state as the TX applet's own
row and follows the same transmit rules, so a grayed row in the applet is
grayed in the container too. The TX applet itself keeps every row. A
container's **TX Keying** has **Hide slice indicators**, which leaves only TUNE
and MOX.

The RX applet's rows are listed the same way: **RX Antennas**, **RX Mode**,
**RX Step**, **RX Filter**, **RX Pan**, **RX Squelch**, **RX Attenuator**,
**RX AGC**, **RX RIT** and **RX XIT**. Each acts on the same slice as the RX
applet. A container's **RX Filter** has **Hide width label** and **Hide
passband**, so it can show just the filter buttons, plus **Columns** (1 to 32)
and the **Button text**, **Background top**, **Background bottom**, **Selected
top** and **Selected bottom** colors for those buttons. The RX applet itself
keeps every row.

**Nereus Band Buttons**, under **Controls** in the add-item menu beside **Band
Buttons**, draws the band buttons with the raised look of the PureSignal
button. It has the same settings as **Band Buttons**, plus a **Nereus colours**
group: **Button text**, **Background top**, **Background bottom**, **Selected
top** and **Selected bottom**.

**Frequency (Custom)** is a frequency readout you can size and style, beside
the fixed **Frequency (Longpath)** item. Its properties are **Font size** (12
to 160), **Fit font to the width**, **Font color**, **Center in the box**, and
**A/B line: this slice** and **A/B line: other slice** to show or hide each
slice line under the digits.

### Place, resize and layer objects on Canvas

1. Select **Canvas**. The first explicit switch from **Vertical stack** uses
   the separated preview rows when there are no saved free positions. Switching
   away and back reuses saved positions. **Start from stacked layout** replaces
   the free positions with the current rows; **Use saved legacy positions**
   converts the retained old positions. Save a copy before either reseeding action.
2. Select an object in **Contents** or the preview. Drag its external top-left
   grip to move it, or bottom-right handle to resize. With a handle focused,
   arrow keys change position/size by one logical pixel; Shift changes the step
   to ten. Escape cancels the current gesture.
3. For exact placement, edit **X**, **Y**, **Width** and **Height** under
   **Canvas position and size**. **Layer** explicitly sets overlap order;
   larger values paint in front. Selecting an object does not bring it forward.
   New size edits respect the native object's minimum size.
4. Make the viewport smaller and use its scrollbars to reach objects outside
   it. Window resizing retains the objects' saved size/position. **Auto height**
   can grow a floating/overlay viewport only within the available screen.
5. Apply and verify the live arrangement. Live grip/resize gestures commit when
   completed; a storage or concurrent-placement failure restores committed
   geometry. This differs from the editor preview, which remains pending Apply.

### Move applets and add individual controls

An available applet's catalog entry ends with **Move**. Adding it transfers the
existing view into the draft destination; it does not create a second applet,
receiver model or network subscription. Use **Move to container…** to choose
another destination. Existing applets cannot be duplicated; **Duplicate object**
can make another copy of a supported meter or individual control.

The fifteen individual controls in **Available** are **MOX**, **Tune**,
**Monitor**, **2Tone**, **PureSignal**, **ANF**, **SNB**, **MNF**, **Peak**,
**CTUN**, **VAX 1**, **VAX 2**, **Mute**, **Binaural** and **Duplex**. Each can
be moved/resized independently. They use the existing native actions and
permission gates. In particular, MOX, Tune and 2Tone can produce RF in the
live workspace; use the [ordinary transmit checks](05-transmit.md) before
operating them. A preview button is inert. **Duplex** is the existing DUP
display action, not the unbuilt FDX function.

**Return / hide view** returns or hides an applet; **Remove object** removes an
ordinary object from the draft. **Remove / return contents** removes a
non-main container after returning its contents to remembered homes. A missing
home recovers to the main area; a destination that cannot take the contents can refuse the action.
Read the result rather than assuming views were deleted. A duplicated ordinary object starts without the original object's remembered
home. The main area is retained. Use the container settings to
apply the intended result, and verify each returned view afterward.

The available catalog contains composite meter faces, individual controls,
existing applets and retained raw item types. A stored or selectable item does
not establish a measurement producer or hardware action. Read the item's
unavailable explanation, [the control reference](12-reference.md), and the
specific operating procedure. **Presets…**, **Copy settings** and **Paste
settings** from the earlier editor are hidden in this draft editor;
choose its offered faces or duplicate a compatible ordinary object instead.

### Work with the workspace look

The workspace look is how the main window opens. Its container headers, Arrange mode and profiles are introduced in [chapter 2](02-desktop-window.md#arrange-the-workspace); this section covers what they change when you build containers.

**Headers.** Each container's header is either shown or hidden; there is no hover choice. The eye button on the header switches it: an open eye means the header stays, a crossed-out eye means it hides when you are not arranging. **Keep Header Showing** in the container's right-click menu is the same switch. A hidden header takes no room, so the content fills the container.

**Arranging.** Hold **Shift** by itself for about a third of a second and every header appears, hidden ones included, until you let go. Shift with a click, a wheel turn or another key does not do this, so tuning with Shift held works as before. To keep arranging without holding a key, choose **Containers > Arrange Workspace** (Ctrl+Shift+A); **Escape** leaves it. **Containers > Align to 8 px Grid** makes free and floating containers settle on an 8 px grid. Arranging does not change which headers are shown or hidden. **Containers > Hidden** lists the containers you have hidden, including those hidden on receive, on transmit or by a macro, so you can bring one back.

**Moving a container.** While arranging, drag a container by its header and let go where you want it. Near any edge of the main window a box shows where it will land, and it docks there. Over the spectrum it sits free where you dropped it; with **Containers > Align to 8 px Grid** on, it settles on an 8 pixel grid (hold **Alt** to place it freely). Outside the main window it becomes a window of its own. This works the same for a docked container, a free one and a popped-out window. **Escape** during a drag puts the container back where it started. When you are not arranging, dragging a window only moves it, so you can park a window over the main window without docking it.

**Pop Out, pin and return.** **Pop Out** is in the container's right-click menu as well as on the header. A popped-out window has a pin on its header: click it to keep the window on top of other windows, click it again to stop. The return arrow puts the container back where it was popped out from: the same strip, beside the same neighbor, or the same free place. A window that was never docked joins the Main Panel's strip.

**No Lock.** The workspace look has no Lock setting. A container whose header is hidden has nothing to grab, so it cannot be moved by accident, and a container saved as locked opens unlocked.

**Where things start.** Every container and window starts where you left it, at the size you left it, on the monitor you left it on. If that monitor is not connected the window opens on the main window's screen, and goes back to its own monitor when that is connected again.

**Workspace profiles.** **Containers > Manage Workspace Profiles…** opens **Workspace Profiles**, where you can make a **New** profile, **Duplicate**, **Rename**, **Save**, **Export**, **Import** and **Remove** one. The profile rail and Ctrl+1 to Ctrl+9 switch between profiles; see [chapter 2](02-desktop-window.md#workspace-profiles).

**S-Meter.** The **S-Meter** checkbox in **Containers > Applets** shows or hides the S-Meter applet. **Containers > Reset Default Layout** puts the S-Meter first and showing. The **S-Meter dial (additional)** item can be added to any container from **Available**.

### Copy a container item layout between containers

Use **Save… / Load…** for the selected container's portable file, including
its configuration and contents. Use **Export / Import** for its complete
contents through the clipboard. The new portable format retains object
properties, bindings, unsupported records and recovery data; the old Base64
item-list recipe does not describe this editor.

1. Select the source container and click **Export**. The editor reports
   **Complete contents copied to clipboard.** This copies its draft contents.
2. Select the destination and click **Import**. Successful clipboard import
   adds the contents to that destination's draft. It does not replace the
   whole list. **Load…** imports a selected-container file and replaces its
   container configuration/contents, with the main area's existing contents
   specially retained. Save the destination first if you need its exact old state.
3. Inspect the contents, sources, layout and messages before Apply. An existing
   singleton applet is moved from its draft source rather than duplicated.
   Invalid data or a duplicate singleton can refuse
   import; a refused import is not a partial successful layout.
4. Click **Apply** only when the pending result is right. Cancel discards an
   uncommitted import, including edits across inspected containers. After
   successful Apply, recovery requires the saved original or another edit;
   Cancel cannot roll back that commit.

Portable imports preserve unavailable content for recovery rather than
silently inventing a working view. A retained raw record or MMIO binding still
needs a supported producer on this computer and the intended station.

### Graph a live meter reading

In **Containers > Edit Container**, add the raw **HistoryGraph** item from **Available** and
associate the container with the intended RX source. Select the item and set
its common **Reading** to an available live value, such as **RX: Signal Peak**.
Keep **Axis 1 shows** at **(none)**: this build exposes that second-axis editor
but does not supply a live second-series update path.

Set **Capacity** (10 to 3000 samples), **Line color 0**, **Show grid**,
**Auto scale axis 0** and **Show scale axis 0** for a readable plot. **File > Settings... >
Display > Multimeter > History duration** changes duration/capacity on existing
graph items, so recheck the item after changing the global duration. Use Apply
to inspect the live result and OK to accept it. Cancel discards changes since the last successful Apply. It does not restore
the opening layout after a commit.

Observe a changing received signal and confirm samples accumulate in the
first trace. If it is blank, check the container source, Reading binding,
receiver state and whether that measurement is produced by the connected
hardware/Core. No-reading values are excluded from the graph. A stored item
or the optional second-axis selector does not provide missing telemetry.

### Show the RADE waveforms

**Microphone waveform** and **Decoded speech waveform** are meters for RADE digital voice. In **Containers > Edit Container**, set the container's **Slice** to a slice that is in a RADE mode, select the meter in **Available**, press **Add →**, and set its **Time span**, **Face Height** and **Trace Color** under **Appearance and waveform**. The microphone meter also has **Microphone stage**. They watch audio that is already running and change nothing. A meter on a slice that is not in a RADE mode reads "Select a RADE slice for this waveform". The full procedure, and what each waiting message means, is in [chapter 15](15-rade-freedv.md#watch-the-rade-audio-in-a-container).

### Add and tune a Filter Display

**Filter Display** is implemented and can be added from **Available > FilterDisplay** in the catalog (older editors group it under **Special**). It is a compact receiver display that draws a panadapter, waterfall, or both, with the current receive/transmit filter edges and notch markers. It needs live analyzer frames to show signal detail; without a feed it can still appear as a blank configured item. The display follows its effective source: the container's **Slice** unless **Advanced bindings > Source** overrides it. Check both before tuning its item.

1. Create or edit a container, choose its **Slice** association, then add **Filter Display**. Select it in **Contents** to show its properties.
2. Set **Display mode** to **Panadapter**, **Waterfall**, or **Panafall**; **None** hides the rendering while retaining the item. For a spectrum view, enable **Fill spectrum** if desired and set **Padding**, **Min dB**, and **Max dB** using the live preview to frame the signal detail. Keep Min below Max so the displayed level scale remains useful; the exact padding effect depends on the item's rendered geometry.
3. Choose **WF palette** when the waterfall is shown. Use the color swatches for **Data line**, **Data fill**, **RX edges**, **TX edges**, **Notch**, **Background**, and **Text** when contrast needs adjustment. Color changes update that item's stored presentation.
4. Apply the editor changes, then confirm that the container is showing the intended slice and a live frame. The RX and TX edge markers identify the passband boundaries; notch markers show notch positions. If only the frame is missing, check the selected source and whether that receiver is active before rebuilding the item. If the item is present but blank, test another mode or inspect it again while that source has live display data.

This item is a visual aid; it does not edit filter edges or notch frequencies. Make those changes through the receiver's filter or notch controls, then return to the container to inspect the resulting markers. Its availability is a current build fact; stale saved layouts or another build may still display an unavailable explanation.

### Rotator indication and external meter data

A **Rotator** item can render azimuth, elevation, or both. It shows the rotor of the window it sits in, which is set up under **File > Settings... > CAT & Network > Rotor** (see [chapter 8](08-shared-core.md#play-the-core-speaker) for how that rotor is shared). Drag the pointer to a new heading and let go to turn the rotor; the turn is sent on release, not while you drag. Set its display mode to **Azimuth**, **Elevation**, or **Both**. If no rotor is set up, the item stays a visual object with no live value.

The item can also show an externally supplied meter value. It keeps existing MMIO bindings and exposes the object's **MMIO source UUID** and **MMIO variable** under **Advanced bindings**, but **Meter Data Sources (MMIO)…** is disabled in its draft editor, so this chapter cannot give a supported route for a new endpoint there. Existing verified feeds can still supply indication: use the retained source UUID and exact variable name in **Advanced bindings**; entering text alone does not establish a feed. In older builds where the endpoint editor is available, add or select a source, give it a name, choose its connection type (**UDP listener**, **TCP listener**, **TCP client**, or **Serial**), choose **JSON**, **XML**, or **RAW (key:value)**, enter the host/port or serial device/baud rate, click **Apply changes**, and inspect **Values received** for the variable and changing value before binding it. If no external value appears, check endpoint status, transport, format, host/port/device, and incoming variable name.

The MMIO endpoint editor configures a data connection and exposes received variable values. It does not certify that a sender is connected or that arbitrary external devices are compatible. Start with a non-critical test value and confirm the value readback before relying on it at the operating position.

## Phone display scope

The phone **Setup > Display** page separates **On this phone** rendering preferences from **Core** analyzer controls and any **Both** rows. The phone-local group includes band plan presentation, palette, levels, Clarity/NF, trace fill/line, passband, spectrum height, peaks, grid, CTUN and extended view. **More** contains local detector/averaging/decimation, overlays, rewind depth, time display, gradients and related display options. The **Core** group describes the shared FFT/analyzer choices that the Core supplies to clients. A local row affects this handset's rendering; it does not silently alter another operator's phone. If a requested local rendering choice is unavailable for the connected Core, the UI explains the reason.

The quick **Display** sheet opens from the Panadapter toolbar and labels itself **Pan N · this phone**. It adjusts the selected pan's local palette and levels, fill, trace line/height, reference **Top**, dynamic **Range**, and display extras. The waterfall levels row has five choices: **Clarity**, **Auto**, **Floor**, **Manual** and **Scale**. **Sizing**, **FFT size** and **Hz/bin** are Core-owned controls, shared by every device. **Sizing** chooses what sets the FFT size: **Auto** lets the zoom pick it, **Hz/bin** makes it follow the bin width you set, and **Fixed** uses the **FFT size** you set. **FFT size** can be changed in **Fixed** only and **Hz/bin** in **Hz/bin** only; a control that is not in charge is gray (in **Auto** both are), shows what is running or remembered, and a line under it says which **Sizing** choice frees it. Check the live bin-width readback after a change. With a Core that does not offer **Sizing**, the sheet shows **FFT size** and **Hz/bin** without it. **LIVE** returns from rewind. **More display options in Setup** opens **Setup > Display > On this phone**.

The setup page is a long grouped list. **Band plan** selects from the Core's offered plan list: the selected plan is shared by the desktop and all devices, while **Size** changes only this phone's band-plan presentation. **Core** groups show the settings described by the connected Core, including **Sizing**, FFT size, Hz/bin target, window, and frame rate where offered. **Sizing** is a choice of **Auto**, **Hz/bin** and **Fixed**: **Size** can be changed in **Fixed** only and **Hz/bin Target** in **Hz/bin** only; a control that is not in charge is gray (in **Auto** both are), shows what is running or remembered, and is named in the group's footer with the choice that frees it. The same three rows are on the described **FFT & Engine** page, where **Sizing** is a menu. The Core's **Spectrum Avg Time** and **WF Avg Time** go from 1 to 9999 ms in 1 ms steps where the Core offers that; with a Core that starts at 10 ms the phone asks for no less than 10 ms, in 10 ms steps. Read each group badge: Core settings are shared; phone settings affect this device's current pan. A Core without the display catalog shows FFT size, Window, Hz per bin target, Frames a second, and bin-width readback grayed with the newer-Core explanation.

On the local page, **Waterfall** has palette; **Levels** (**Clarity**, **Auto**, **Floor**, **Manual**, or **Scale**; Clarity, Auto and Floor take effect only when the Core sends display extras); manual **High level/Low level**; color gain and black level. Manual levels remain useful while another mode is selected and are used until Core levels arrive. In **Scale** the phone colors against the spectrum scale's current bottom and top. **Waterfall: more** adds detector/averaging when not supplied in the Core groups, update period, opacity, stop while transmitting, scale/level copy actions, look-back depth, timestamp position/time zone, RX/TX filter and zero-line overlays, and a custom gradient. A custom gradient has two to eight stops; choose each stop's color and position, add/remove middle stops, then select the Custom palette to use it. The readback reports retained look-back time at the current period.

**Spectrum** adjusts local fill, fill strength and gradient, trace line width/color, spectrum height, passband color/strength, and normalization where supported. The Core page owns analyzer FFT/window choices when described. On an older Core, the local **Spectrum: more** group supplies detector, averaging, and decimation subject to the app's capability gate; classic **Peak hold** delay and **Zero line** are local. **Grid & Scales** controls grid visibility/step and scale top/bottom; **Grid & Scales: more** controls dBm scale, frequency-label alignment, noise-following bottom and keeping range when the Core supplies that estimate. The **Noise floor** group enables its line and shift when the Core sends display extras. **Peaks** distinguishes classic peak hold, active peak hold, and peak blobs; the latter two require display extras. The **On the band** group enables cursor frequency, bin width, frame rate, and strongest-signal readout with corner and interval. **Colors** changes local grid, lines, labels, markers, peaks, overlays, and text presentation. **View** controls extended view when offered and CTUN.

With a 2026.10.1 Core, **Setup > Display** also lists the Core's own pages in this order after **On this phone**: **Spectrum**, **Levels**, **Waterfall**, **View**, **Peaks & Markers**, **FFT & Engine**, **Transmit Display** and **Multimeter**. Two buttons at the top of **Spectrum**, **Levels**, **Waterfall**, **View** and **Peaks & Markers**, **All pans** and **Pan 1**, choose what you edit. A change under **All pans** sets this phone's default and clears Pan 1's own value for that row. With **Pan 1** chosen, **Follow All pans on this page** drops Pan 1's own values on the page, and **Copy own values to All pans** makes them this phone's default; both are gray when the page has no own values. Read each group's badge to see whether a row is shared by the station or kept on this phone.

The transmit page has local **Top**, **Range**, waterfall high/low levels, palette, and **DUP**. Its **The Core's transmit display** group changes the station analyzer's FFT/window, spectrum detector/averaging/time/normalization and waterfall detector/averaging/time when supported. This group affects every device watching that Core's TX display. A disabled control or reason in the page is the availability readback; do not substitute a desktop procedure. **Reset to defaults** asks first and resets spectrum/waterfall display preferences while retaining each band's scale, band-plan size, and extended-view state. It is distinct from the desktop's Reset to Smooth Defaults profile.

To make a local presentation change, select the target pan first, open **Display > More display options in Setup**, change one group, return to the band, and inspect the trace/scale/waterfall or transmit display that it controls. To change the station analyzer, read the **Core** badge, consider other connected operators, change the value, and check its returned value/bin width. The phone may keep unsupported values grayed; use the accompanying reason to decide whether an update or different Core is required. See [chapter 7](07-iphone-operate.md) for iPhone/iPad layout and live display interactions and [chapter 20](20-phone-preferences.md) for a field-by-field phone setup reference.

## Boundaries to keep in mind

This build has several visible or stored features that are not operating controls. Desktop TX grid is unbuilt; the phone's local TX grid is implemented. Multimeter holds and averaging are unbuilt, while S-meter peak hold is available. FDX is hidden/unbuilt; DUP is a separate display option. Container macros, Var1/Var2, RX2/SUB RX/Pan Swap, Click Box, and voice record/play are unbuilt. PBSNR has no live producer. The Rotator item needs a rotor set up under **CAT & Network > Rotor**, or a verified external MMIO value, to show a position. For the wider list of hidden, gated and unsupported features, consult [chapter 12](12-reference.md).
