# Display, meters, and desktop layout

This chapter covers what the spectrum and waterfall show, how to make weak signals easier to read, how to inspect recent waterfall history, and how to build a desktop meter container. Display tuning changes presentation and analyzer requests. It does not change the receiver's demodulation, filter, transmit power, or RF path. Where a control changes a shared analyzer or Core setting, its scope is called out.

For the main operating window, slice selection, spectrum tuning, and context menus, see [chapter 2](02-desktop-window.md) and [chapter 3](03-receive.md). Receiver DSP and filter changes are in [chapter 13](13-receiver-dsp.md); transmit setup is in [chapter 5](05-transmit.md), voice processing in [chapter 14](14-voice-profiles.md), and radio calibration in [chapter 17](17-hardware-antennas-calibration.md). For the phone's full display and session preferences, see [chapter 20](20-phone-preferences.md), with touch operation in [chapter 7](07-iphone-operate.md). Chapter [12](12-reference.md) indexes scopes and availability. Related advanced procedures: [RADE and FreeDV](15-rade-freedv.md), [spots and reporters](16-spots-reporters.md), [PureSignal and diversity](18-puresignal-diversity.md), and [amplifiers and tuners](19-amplifiers-tuners.md).

## Read the display without changing the receiver

A panadapter is a live FFT view over the selected receive window. The waterfall paints successive FFT results vertically so time runs down the display. The trace and waterfall represent received energy; changing their color, scale, averaging, or history does not retune the slice. A trace may look smoother or more prominent after display changes while the received audio remains the same.

On desktop, use **File > Settings > Display**. The relevant pages include **Spectrum Defaults**, **Waterfall Defaults**, **Grid & Scales**, **Spectrum Peaks**, **Multimeter**, **3D View**, and **TX Display**. Most choices are saved presentation preferences; some analyzer choices are shared or sent to the Core. Remote desktop settings can be held when the Core does not describe or grant the control. Read the disabled control's explanation and its scope before trying another value. Do not interpret a grey field as a radio fault.

The right-click menu on a panadapter is a quick way to change the active pan's visible settings. The **Waterfall** section changes palette, color gain, and black level. The **Spectrum** section changes fill/alpha and the displayed reference level and range. **Tuning > CTUN** enables center tuning when available. The **3D View** section changes rendering mode and its floor, gain, row span when supported, angle, and slice shadow. This popup affects the pan under the pointer; Setup pages expose broader defaults and additional controls. If a command is unavailable, use its displayed capability reason rather than assuming it will work on every radio/Core.

The spectrum's right-hand dBm scale also has direct gestures. Click its small up/down controls to move the reference level by 10 dB while keeping the bottom fixed. Scroll over the scale to adjust dynamic range in 5 dB steps. Drag with the left button to move the displayed range, or right-drag to stretch it. Ctrl/Command-drag zooms that range. Away from the scale, plain scroll tunes by the selected step, Shift-scroll moves reference level, and Ctrl/Command-scroll zooms spectrum bandwidth around the active VFO. On macOS, Command is accepted for the Control-modified interactions. Click-drag the frequency scale to change displayed bandwidth; drag the spectrum/waterfall field to pan. A short click in the pan tunes, subject to slice ownership and lock. Use the visible menu labels if the platform presents a different modifier glyph.

## Make a weak signal easier to see

Use this sequence when a signal is present in audio or known to be in the receiver passband but is hard to distinguish visually. It changes the view only.

1. Select the intended slice and panadapter. Open **File > Settings > Display > Grid & Scales** and note the current **dB Max** and **dB Min** for the band. These per-band limits determine how much vertical signal range is visible. Narrow the displayed range around the current noise floor and signal, then check the dBm labels and ensure stronger nearby signals are not clipped. If the noise floor moves with conditions, enable **Adjust grid minimum to noise floor** and set **NF offset**; the grid minimum then follows the live estimate plus that offset. **Maintain grid range** preserves the max-to-min span while the minimum follows. Turn this off if it hides the region you need.
2. Open **Waterfall Defaults** and choose a palette with useful contrast. Adjust **Waterfall Low** and **Waterfall High** so ordinary background noise occupies darker/intermediate colors and the signal rises clearly above it. If manual levels are difficult to keep aligned as conditions change, test **NF-AGC** and its offset. NF-AGC is a waterfall intensity aid; it does not change receiver AGC or audio gain.
3. For another waterfall tuning behavior, enable **Clarity (adaptive waterfall tuning)** on **Spectrum Defaults**. Clarity adapts waterfall thresholds to the observed signal distribution. It may control Waterfall Low/High while active, so compare its result with manual levels and disable it before expecting manual thresholds to remain fixed. **Reset to Smooth Defaults** asks for confirmation, then applies the smooth display profile: Clarity Blue palette, log-recursive spectrum averaging at 650 ms, white trace without fill, waterfall AGC, and a 30 ms waterfall update period. It does not change FFT size, frequency, band stack, or per-band grid ranges. Cancel the confirmation to keep the current profile; otherwise inspect the affected trace and waterfall and adjust from the Spectrum Defaults and Waterfall Defaults pages.
4. Return to the panadapter and check the trace, passband, dBm labels, and waterfall together. If a peak disappears, widen the grid range or move the reference level. If waterfall noise is washed out, adjust low/high levels or black level and color gain from the pan's right-click menu. Change one group at a time so the useful setting is easy to identify and undo.

### Set spectrum resolution, update rate, and smoothing

Open **File > Settings > Display > Spectrum Defaults** with the pan you want to assess. Under **Fast Fourier Transform**, move **Size** one step at a time and read both **FFT Size** and **Bin Width (Hz)**. The sizes run from 4096 to 262144 in powers of two. Bin width is sample rate divided by FFT size: a larger transform makes narrower bins and finer frequency detail, at greater processing cost. **Hz/bin Target** controls how zoom changes resolution: **Off** lets the FFT replan as you zoom, while a positive target asks for roughly constant bin width across zoom levels. **Size** remains the minimum FFT size when a target is active. The page's **Window** choice changes leakage versus main-lobe width: Rectangular is narrow but has stronger sidelobes; Hann/Hamming/Blackman-Harris/Kaiser trade some width for sidelobe reduction; Flat-Top favors amplitude accuracy. After a change, read the actual FFT/bin-width value; remote display may show a Core grant different from the request.

In **Rendering**, **FPS** sets the approximate redraw rate, trading animation smoothness against CPU work. **Spectrum Detector** selects how input bins become display pixels: **Peak** keeps the maximum, **Rosenfell** alternates max/min for a classic trace, **Average** averages bins, **Sample** selects one bin, and **RMS** computes root-mean-square power. **Spectrum Averaging** is time smoothing across frames: **None** shows each frame; **Recursive** smooths linear power; **Time Window** approximates a sliding window; **Log Recursive** smooths in dB. **Spectrum Avg Time** sets the smoothing time constant; longer time smooths more and responds more slowly. Try a detector for the view shape first, then choose averaging and time based on how quickly you want the trace to follow changes.

**1 Hz BW: Av / Sa** normalization is available for Average, Sample, or RMS detectors. It scales displayed noise power to a 1 Hz reference bandwidth; it is not a receiver calibration adjustment. The checkbox disables itself when the chosen detector does not support it. **Display Decimation** reduces display resolution as its value increases. Decimation is applied to the FFT engines across pans; use it only when lower display detail is acceptable to reduce processing load. **Cal Offset** shifts displayed levels on a local radio, but is disabled in a remote window because the Core already calibrates its radio data. The **Peak hold** control in this page is a separate whole-trace maximum hold; its delay controls when the held trace starts to decay. For per-bin peak decay and labeled top-N markers, use **Configure peaks** to open Spectrum Peaks.

The **Noise Floor (NF)** overlay is a visual estimate. Its shift moves the rendered line, line width changes its visibility, and color controls change its appearance. It is not a calibrated measurement or receiver threshold. To show the live bin width or cursor frequency, enable those readouts under **Spectrum Overlays**. The strongest-signal readout can be enabled, placed in a corner, and given an update interval. The NF text stays beside its line; its retained position control is disabled and has no visual effect. These overlays are presentation aids, not receiver thresholds or calibrated measurements.

### Hold peaks and label strong signals

Open **File > Settings > Display > Spectrum Peaks**. Use **Active Peak Hold** for a second, dashed per-bin trace: enable it, choose **Hold duration**, then set **Drop rate** for the fall after each bin's hold expires. **Fill area between peak trace and current trace**, **Update during TX**, and **Trace color** control its presentation and operation during transmit. Check that the colored held trace decays while the live trace continues.

Use **Peak Blobs** for circle markers on the highest spectrum peaks. Enable **Show top-N peak markers**, set **Number of peaks** from 1 to 20, and optionally restrict markers to the current RX passband. The value beside each circle uses **Text color**; **Blob color** changes the marker. There is no user-set amplitude threshold or minimum frequency separation control: this page selects the number of highest peaks, not a list of threshold-qualified stations. To retain a marker at its maximum, enable **Hold peaks before decay** and set the duration. **Decay after hold** makes it fall at **Fall rate** dB/s; with decay off it disappears when the hold ends. Change these settings while watching a known peak, then verify marker count, passband filtering, and hold/fall behavior on the live pan. These marks do not establish signal identity or decodability.

## Waterfall levels, timestamps, and rewind

### Tune waterfall levels, row processing, and time labels

Open **File > Settings > Display > Waterfall Defaults**. In **Levels**, **Waterfall High** maps its threshold to the high signal color and **Waterfall Low** to the low color; set them around the background noise and signals. **Use spectrum min/max** makes the spectrum's range authoritative and disables manual thresholds and AGC controls. Otherwise choose **AGC** or **NF-AGC** to track changing levels; the NF-AGC offset adjusts where the estimated floor maps. These affect waterfall colors only, not receiver AGC. Use the live **Delay** readout to see the approximate time span in the visible waterfall; **Stop on TX** freezes new waterfall rows during transmit and resumes after unkeying. **Opacity** blends the waterfall with the spectrum background. **Color Scheme** selects the palette. The desktop RX Waterfall Defaults page does not provide a working low-level custom-gradient editor; **Custom** does not imply that the unavailable low-level color editor is built. Low-level waterfall color editing is listed as unbuilt.

In **Detector/Averaging**, **WF Detector** offers **Peak**, **Rosenfell**, **Average**, and **Sample** (waterfall has no RMS choice). These reduce FFT bins into each waterfall pixel/column. **WF Averaging** uses **None**, **Recursive**, **Time Window**, or **Log Recursive** to smooth successive waterfall frames/rows; its **WF Avg Time** is independent from the spectrum averaging time. Longer time suppresses rapid change and leaves more temporal persistence. Choose the detector for how narrow signals render, then adjust averaging while comparing a changing signal so detail does not smear more than you intend.

In **Overlays**, enable RX filter edges, TX filter edges on the RX waterfall, and/or RX/TX zero lines to compare passbands and center-frequency references against stored history. **Time > Timestamp Position** chooses **None**, **Left**, or **Right** for each row; **Timestamp Mode** chooses **UTC** or **Local**. Enable a side and select the time basis, then compare a newly painted row to the desktop clock. Historical rows retain their own time; the label is not a live tuning indicator. **Rewind history > Depth** requests 60 seconds, 5, 15, or 20 minutes. Read **Effective rewind** after changing depth or update period: the ring is capped at 16,384 rows, so faster updates can shorten actual history. Update period also changes the waterfall's temporal detail. Set the requested depth, then adjust update period and check the effective readback again.

The RX page exposes palette selection, but custom low-level gradient editing is not implemented there in this build. Where an actual **GradientPicker** is present, such as **Setup > Display > TX Display** after choosing **Custom** for the TX waterfall palette, click an existing stop to select it, drag it along the strip to reposition, double-click it to choose its color, click between stops to add a stop, or right-click a middle stop to remove it. The end stops cannot be removed. Confirm the palette remains **Custom** and inspect the live TX waterfall before relying on the gradient. The TX gradient is a separate transmitter display setting.

To look back on desktop, use the narrow time-scale strip at the waterfall's right edge. Left-click **LIVE** to pause at the current view. While paused, drag the time scale upward to move farther back and downward toward the present. The scale labels show elapsed seconds while live and absolute UTC timestamps while paused. You can also drag into history directly. The view remains paused while new live rows arrive; the historical position is held. Click the enlarged **LIVE** button to return to current data. Rewind is a stored picture: tuning and audio continue live and are not rewound. Disconnecting clears the ring buffer.

On iPhone/iPad, open **Panadapter > Display** and drag the **Look back** slider away from zero to move the waterfall to an older position. The band displays a **LIVE** button while looked back; tap **LIVE** to return to the newest row. New rows continue arriving while the view is back in history. To set the maximum buffer, open **More display options in Setup > Waterfall: more > Look back up to**; the footer reports the retained interval at the current update period. This adjusts how far back the control can travel, while the slider selects the current viewing position. Timestamps can show **UTC** or **This phone's** time zone. Tuning and audio remain live while the picture is historical. See [chapter 20](20-phone-preferences.md) for mobile display settings.

## 3D, duplex, and transmit display

### Switch to and read the 3D stacked trace

On desktop, open the pan's right-click menu and set **3D View > Spectrum** to **3D Stacked Trace**, or open **File > Settings > Display > 3D View** and choose it there. The 3D surface stacks recent spectrum traces; the ordinary waterfall remains the time history beneath it. In the page, adjust **3D Floor** to set how far below the estimated noise floor the surface begins, **3D Gain** to move the color range toward weaker or stronger signals, **3D Span** to control how far near traces extend beyond plot edges, **3D Angle** to choose edge-on versus top-down perspective, and **3D Slice Shadow** to project slice passbands onto the surface. Floor is per-band; the other display geometry controls apply to the view. Use the context menu for immediate changes on the pan under the pointer and verify in that pan.

The context menu's 3D view controls include floor, gain, row span when supported, angle, and slice shadow. To change floor from the plot, drag vertically on the right dBm strip below its arrow controls; while 3D is active this gesture changes floor depth, and the strip's readout shows the new value. There is no free mouse-orbit or perspective zoom control: adjust **3D Angle** in Setup or the context menu. The ordinary bandwidth zoom gestures still change frequency span, not camera perspective. To return to the shipped state, choose **Reset 3D to defaults** and confirm. It restores **2D Waterfall**, 6 dB floor, 70% gain, 100% span, 50% angle, and slice shadow off. The reset is limited to these 3D view settings.

The 3D surface draws recent rows, so **WF Detector**, **WF Averaging**, and **WF Avg Time** still affect its incoming waterfall history. Spectrum **Display Decimation** lowers FFT display resolution and is applied across pans; it can reduce the detail visible in each stacked trace. **Rewind history** controls the separate flat-waterfall lookback buffer. The 3D stack uses its own fixed-row ring; changing the rewind depth or its effective-depth readout does not add 3D rows. The waterfall update period affects the time represented by successive stack rows. If the surface has little time depth, compare the update period and let its live stack fill; if it looks coarse, check decimation before raising FFT size. On a remote radio, read capability reasons for unsupported settings. In the mobile quick sheet, select **View > 3D Stacked Trace** when offered; its **3D Floor**, **3D Gain**, **3D Span**, **3D Angle**, **3D Slice Shadow**, and confirmed **Reset 3D to defaults** appear there.

**View > Display duplex (DUP)** controls whether the receive display remains visible during transmit when the current analyzer and radio allow it. DUP is a display behavior; it is not full duplex transmit/receive capability. FDX remains unavailable in this build.

**Setup > Display > TX Display** configures the station's transmitter analyzer. Its **Fast Fourier Transform** size slider selects 4096 through 262144; read the FFT size and bin-width fields after the Core returns its value. **Window** choices are Rectangular, Blackman-Harris 4T, Hann, Flat-Top, Hamming, Kaiser, and Blackman-Harris 7T. Under **Panadapter**, choose detector (**Peak**, **Rosenfell**, **Average**, **Sample**, or **RMS**), averaging (**None**, **Recursive**, **Time Window**, or **Log Recursive**), and the averaging time. **1 Hz BW: Av / Sa** normalization is only enabled for compatible pan detectors. Under **Waterfall**, choose its detector (**Peak**, **Rosenfell**, **Average**, or **Sample**), averaging mode, and independent time. The amplitude scale's low/high levels and palette affect the TX waterfall. These analyzer settings are station/Core settings in a remote window and can affect every watcher; wait for their values/readbacks after a write. Use these controls only when the supported TX analyzer has data. A no-data banner means there are no current TX analyzer samples, not that RX display has failed. Desktop TX grid controls are unbuilt. The mobile TX page has a local grid and palette alongside Core analyzer controls; follow its scope badges rather than transferring desktop procedures.

## Set grid range and labels

Open **File > Settings > Display > Grid & Scales**. **Show grid** controls the major grid and frequency numbers; **Show dBm scale strip (right edge)** displays the reference-level scale and can be turned off to give the plot that width back. The highlighted **Editing per-band grid** header names the currently selected band. **dB Max** sets signal level at the top of the display and **dB Min** the bottom; each band retains its own pair. **dB Step (global)** sets horizontal grid spacing in dB across bands. Set Max and Min for the active band, check the band named in the header, then switch bands and verify that the other band's range remains appropriate.

Under **Labels**, **Freq Label Align** places frequency callouts Left, Center, Right, automatically, or Off; **Show zero line** draws the horizontal 0 dBm reference; **Show FPS overlay** adds the display's frame-rate readout. In **Noise-Floor Tracking**, **Adjust grid min to track noise floor** moves the minimum to the estimate plus **NF offset**. A negative offset places the minimum below the noise floor; a positive offset places it above. **Maintain grid range (move max with min)** moves the top too, preserving the dB span. These tracking controls require a usable live floor estimate. If an automatic range hides a signal, turn tracking off or adjust its offset, then set the per-band endpoints manually. **Copy waterfall thresholds to spectrum min/max** copies the current Waterfall High/Low into the current band's spectrum top/bottom; inspect the band header and scale after using it.

## Colors and meter styles

Open **File > Settings > Appearance > Colors & Theme** for trace, grid, text, band-edge, zero-line, passband and related display colors. Reset controls return the associated color group to its defaults. Color choices are local presentation. The custom waterfall palette is configured in **Waterfall Defaults**; low-level color editing is not available in this build.

**Appearance > Meter Styles** selects the S-meter face/style and its peak-hold and decay presentation. S-meter peak hold is implemented. For container meters, open **File > Settings > Display > Multimeter**. **Polling delay** sets how often live meter values are sampled (10 to 2000 ms): shorter responds faster and costs more CPU. **Show decimal point in readouts** controls a decimal digit in S-meter/dBm text values. **Display units** applies S, dBm, or uV to supported meter items; read the item after changing units to confirm formatting. `S` uses S1 to S9 plus dB above S9, dBm reports dB relative to 1 mW, and uV reports microvolts at 50 ohms. The implemented controls include polling delay, decimal presentation, units and **History duration**, which updates existing container History Graph items. Peak/text/digital hold controls, the global history-enable checkbox and meter averaging are hidden/unbuilt. The separate container **History Graph** has a live first-series producer; create and bind that item as described below. The hidden global checkbox is not how you enable it. PBSNR is not a supported live meter value in this build. A designer property name or saved layout entry does not establish live data availability.

## Arrange applets and containers

The **Containers** menu controls installed applets and desktop containers.
A container can hold meters, existing applets and individual controls. Its
layout arranges those views; it does not create a receiver or grant transmit
rights. **View > Pan Layout** separately arranges panadapter windows.

The 2026.10 candidate uses a draft editor with a preview and **Apply**, **OK**
and **Cancel**. An older development build can have different catalog labels,
clipboard formats and cancellation behavior. Follow the candidate procedure
below for the new editor; the historical figures do not show this Canvas UI.

### Create and configure a source-associated container

This example adds a signal readout and clock, checks their sources, then saves
and applies a reusable arrangement. Begin in receive with the desired slice
running; keep keying controls unused during layout work.

1. Choose **Containers > New Container…**. In **Container Settings**, name the
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
   height** and **Lock** as required. **Minimizes** and **Hide when RX unused**
   are unavailable in this candidate's editor, with stored preferences retained.
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
5. **Lock** prevents placement, size, layer and reseeding changes until unlocked.
   Apply and verify the live arrangement. Live grip/resize gestures commit when
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
home recovers to the main area; a locked destination can refuse the action.
Read the result rather than assuming views were deleted. A duplicated ordinary object starts without the original object's remembered
home. The main area is retained. Use the container settings to
apply the intended result, and verify each returned view afterward.

The available catalog contains composite meter faces, individual controls,
existing applets and retained raw item types. A stored or selectable item does
not establish a measurement producer or hardware action. Read the item's
unavailable explanation, [the control reference](12-reference.md), and the
specific operating procedure. **Presets…**, **Copy settings** and **Paste
settings** from the earlier editor are hidden in this candidate's draft editor;
choose its offered faces or duplicate a compatible ordinary object instead.

### Copy a container item layout between containers

Use **Save… / Load…** for the selected container's portable file, including
its configuration and contents. Use **Export / Import** for its complete
contents through the clipboard. The new portable format retains object
properties, bindings, unsupported records and recovery data; the old Base64
item-list recipe does not describe this candidate.

1. Select the source container and click **Export**. The editor reports
   **Complete contents copied to clipboard.** This copies its draft contents.
2. Select the destination and click **Import**. Successful clipboard import
   adds the contents to that destination's draft. It does not replace the
   whole list. **Load…** imports a selected-container file and replaces its
   container configuration/contents, with the main area's existing contents
   specially retained. Save the destination first if you need its exact old state.
3. Inspect the contents, sources, layout and messages before Apply. An existing
   singleton applet is moved from its draft source rather than duplicated.
   A locked destination/source, invalid data or duplicate singleton can refuse
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
**Auto scale axis 0** and **Show scale axis 0** for a readable plot. **Setup >
Display > Multimeter > History duration** changes duration/capacity on existing
graph items, so recheck the item after changing the global duration. Use Apply
to inspect the live result and OK to accept it. Cancel discards changes since the last successful Apply. It does not restore
the opening layout after a commit.

Observe a changing received signal and confirm samples accumulate in the
first trace. If it is blank, check the container source, Reading binding,
receiver state and whether that measurement is produced by the connected
hardware/Core. No-reading values are excluded from the graph. A stored item
or the optional second-axis selector does not provide missing telemetry.

### Add and tune a Filter Display

**Filter Display** is implemented and can be added from **Available > FilterDisplay** in the candidate catalog (older editors group it under **Special**). It is a compact receiver display that draws a panadapter, waterfall, or both, with the current receive/transmit filter edges and notch markers. It needs live analyzer frames to show signal detail; without a feed it can still appear as a blank configured item. The display follows its effective source: the container's **Slice** unless **Advanced bindings > Source** overrides it. Check both before tuning its item.

1. Create or edit a container, choose its **Slice** association, then add **Filter Display**. Select it in **Contents** to show its properties.
2. Set **Display mode** to **Panadapter**, **Waterfall**, or **Panafall**; **None** hides the rendering while retaining the item. For a spectrum view, enable **Fill spectrum** if desired and set **Padding**, **Min dB**, and **Max dB** using the live preview to frame the signal detail. Keep Min below Max so the displayed level scale remains useful; the exact padding effect depends on the item's rendered geometry.
3. Choose **WF palette** when the waterfall is shown. Use the color swatches for **Data line**, **Data fill**, **RX edges**, **TX edges**, **Notch**, **Background**, and **Text** when contrast needs adjustment. Color changes update that item's stored presentation.
4. Apply the editor changes, then confirm that the container is showing the intended slice and a live frame. The RX and TX edge markers identify the passband boundaries; notch markers show notch positions. If only the frame is missing, check the selected source and whether that receiver is active before rebuilding the item. If the item is present but blank, test another mode or inspect it again while that source has live display data.

This item is a visual aid; it does not edit filter edges or notch frequencies. Make those changes through the receiver's filter or notch controls, then return to the container to inspect the resulting markers. Its availability is a current build fact; stale saved layouts or another build may still display an unavailable explanation.

### Rotator indication and external meter data

A **Rotator** item can render azimuth, elevation, or both. Its value path is an externally supplied meter value, not a built-in connection to a rotator or a command to turn one. The candidate retains existing MMIO bindings and exposes the object's **MMIO source UUID** and **MMIO variable** under **Advanced bindings**, but **Meter Data Sources (MMIO)…** is disabled in its draft editor. This chapter cannot give a supported new-endpoint setup route there. Existing verified feeds can still supply indication. In older builds where that endpoint editor is available, configure a source there as follows. Add or select a source, give it a name, choose its connection type (**UDP listener**, **TCP listener**, **TCP client**, or **Serial**), choose **JSON**, **XML**, or **RAW (key:value)**, and enter the appropriate host/port or serial device/baud rate. Click **Apply changes** and inspect **Values received** for the variable and changing value. Select the Rotator item and bind the verified source/variable only after its incoming name is present. In the candidate, use the retained source UUID and exact variable name in **Advanced bindings**; entering text alone does not establish a feed. Set its display mode to **Azimuth**, **Elevation**, or **Both**. If no value appears, check endpoint status, transport, format, host/port/device, and incoming variable name. The item can still be configured as a visual object with no live value. The application does not provide rotator hardware control through this item.

The MMIO endpoint editor configures a data connection and exposes received variable values. It does not certify that a sender is connected or that arbitrary external devices are compatible. Start with a non-critical test value and confirm the value readback before relying on it at the operating position.

## Phone display scope

The phone **Setup > Display** page separates **On this phone** rendering preferences from **Core** analyzer controls and any **Both** rows. The phone-local group includes band plan presentation, palette, levels, Clarity/NF, trace fill/line, passband, spectrum height, peaks, grid, CTUN and extended view. **More** contains local detector/averaging/decimation, overlays, rewind depth, time display, gradients and related display options. The **Core** group describes the shared FFT/analyzer choices that the Core supplies to clients. A local row affects this handset's rendering; it does not silently alter another operator's phone. If a requested local rendering choice is unavailable for the connected Core, the UI explains the reason.

The quick **Display** sheet opens from the Panadapter toolbar and labels itself **Pan N · this phone**. It adjusts the selected pan's local palette and levels, fill, trace line/height, reference **Top**, dynamic **Range**, and display extras. **FFT size** and **Hz/bin** use Core-owned controls; check the live bin-width readback after a change. **LIVE** returns from rewind. **More display options in Setup** opens **Setup > Display > On this phone**.

The setup page is a long grouped list. **Band plan** selects from the Core's offered plan list: the selected plan is shared by the desktop and all devices, while **Size** changes only this phone's band-plan presentation. **Core** groups show the settings described by the connected Core, including FFT, window, Hz/bin target, and frame rate where offered. Read each group badge: Core settings are shared; phone settings affect this device's current pan. A Core without the display catalogue shows FFT size, Window, Hz per bin target, Frames a second, and bin-width readback greyed with the newer-Core explanation.

On the local page, **Waterfall** has palette; level mode (**Clarity**, **Auto**, **Noise floor**, or **Manual**) when display extras are supported; manual **High level/Low level**; color gain and black level. Manual levels remain useful while another mode is selected and are used until Core levels arrive. **Waterfall: more** adds detector/averaging when not supplied in the Core groups, update period, opacity, stop while transmitting, scale/level copy actions, look-back depth, timestamp position/time zone, RX/TX filter and zero-line overlays, and a custom gradient. A custom gradient has two to eight stops; choose each stop's color and position, add/remove middle stops, then select the Custom palette to use it. The readback reports retained look-back time at the current period.

**Spectrum** adjusts local fill, fill strength and gradient, trace line width/color, spectrum height, passband color/strength, and normalization where supported. The Core page owns analyzer FFT/window choices when described. On an older Core, the local **Spectrum: more** group supplies detector, averaging, and decimation subject to the app's capability gate; classic **Peak hold** delay and **Zero line** are local. **Grid & Scales** controls grid visibility/step and scale top/bottom; **Grid & Scales: more** controls dBm scale, frequency-label alignment, noise-following bottom and keeping range when the Core supplies that estimate. The **Noise floor** group enables its line and shift when the Core sends display extras. **Peaks** distinguishes classic peak hold, active peak hold, and peak blobs; the latter two require display extras. The **On the band** group enables cursor frequency, bin width, frame rate, and strongest-signal readout with corner and interval. **Colours** changes local grid, lines, labels, markers, peaks, overlays, and text presentation. **View** controls extended view when offered and CTUN.

The transmit page has local **Top**, **Range**, waterfall high/low levels, palette, and **DUP**. Its **The Core's transmit display** group changes the station analyzer's FFT/window, spectrum detector/averaging/time/normalization and waterfall detector/averaging/time when supported. This group affects every device watching that Core's TX display. A disabled control or reason in the page is the availability readback; do not substitute a desktop procedure. **Reset to defaults** asks first and resets spectrum/waterfall display preferences while retaining each band's scale, band-plan size, and extended-view state. It is distinct from the desktop's Reset to Smooth Defaults profile.

To make a local presentation change, select the target pan first, open **Display > More display options in Setup**, change one group, return to the band, and inspect the trace/scale/waterfall or transmit display that it controls. To change the station analyzer, read the **Core** badge, consider other connected operators, change the value, and check its returned value/bin width. The phone may keep unsupported values greyed; use the accompanying reason to decide whether an update or different Core is required. See [chapter 7](07-iphone-operate.md) for iPhone/iPad layout and live display interactions and [chapter 20](20-phone-preferences.md) for a field-by-field phone setup reference.

## Boundaries to keep in mind

This build has several visible or stored features that are not operating controls. Desktop TX grid is unbuilt; the phone's local TX grid is implemented. Multimeter holds and averaging are unbuilt, while S-meter peak hold is available. FDX is hidden/unbuilt; DUP is a separate display option. Waterfall low-level custom color editing is unbuilt. Container macros, Var1/Var2, RX2/SUB RX/Pan Swap, Click Box, and voice record/play are unbuilt. PBSNR has no live producer. A configured Rotator graphic alone does not supply position or control hardware; bind a verified external MMIO value for indication. For the wider list of hidden, gated and unsupported features, consult [chapter 12](12-reference.md).
