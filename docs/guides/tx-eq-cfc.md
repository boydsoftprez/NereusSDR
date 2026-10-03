# Transmitter EQ and CFC

Use EQ to shape your transmitted audio. Use CFC to adjust compression and the EQ applied after compression. The curve settings belong to the active TX profile; save that profile through the existing profile controls when you want to keep your changes. The Graphic/Parametric mode choice is saved with the TX profile and follows the model in local and remote windows.

Right-click **EQ** or **CFC** in the TX panel to open its editor. Left-click either button to enable or bypass that processor.

## Choose an EQ mode

**Graphic** keeps the familiar ten vertical band sliders. Drag a slider to adjust its gain, or enter the gain and center frequency beneath it. The separate preamp control adjusts the overall level. Each band still has an editable frequency.

**Parametric** places the bands on a graph. Select a numbered band, then drag its point: left/right changes frequency and up/down changes gain. The editor below the graph shows the selected band's exact Frequency, Gain and Width (Q). You can enter any of these values directly.

The two modes keep separate settings. Switching modes applies the settings for the mode you select.

## Adjust width

When **Use Q Factors** is enabled, drag either width handle on the selected band, move the Width slider, or enter Q directly. Lower Q makes the band wider; higher Q makes it narrower. Width changes do not move the band or change its gain.

The mouse wheel adjusts the selected band's Q when Q is enabled. Hold Shift to adjust gain, or Ctrl to adjust frequency. The endpoint bands retain their existing frequency locks; use Curve range to change the endpoints.

Choose **5, 10 or 18 bands** in the parametric editor. Changing the count resets the curve: use Apply to accept that reset or Cancel to keep your current settings. Undo can restore an applied count change.

## Use CFC

The upper graph controls **Compression**. The lower graph controls **EQ after compression**. Selecting a band selects it in both graphs; changing its frequency moves it in both. Compression amount, EQ gain, and their widths remain separate.

Use the editor below the graphs for exact Frequency, Compression, Compression Width (Q), EQ Gain and EQ Width (Q). The separate **Pre-compression** and **Post-EQ gain** controls adjust overall levels rather than a selected band.

CFC supports 5, 10 and 18 bands. Band and width changes reach the audio engine and are saved with the TX profile. Older profiles that contain only ten-band settings retain that configuration, with Q factors off until enabled.

The configured curve shows your settings. The compression bars show live measured compression; they are a separate display and are not part of undo history.

**Reset Compression** clears the compression amounts and pre-compression level and returns its widths to Q=4. **Reset EQ** clears the post-EQ gains and overall post-EQ level and returns its widths to Q=4. Each reset keeps the shared frequencies and the other graph's settings.

## Undo and advanced settings

Undo and Redo operate on whole edits: one drag, completed entry, reset or applied count change is one step. Each EQ mode keeps its own history; CFC restores its two graphs together. History remains available when you hide and reopen the editor, and starts fresh when a profile or external setting replaces the current state. While typing in an entry, the usual text Undo shortcut edits the text first.

With **Live Update** on, dragging updates audio as you move. With it off, a drag applies when you release it. Exact entries remain available in either case.

Expand **Advanced** for the existing algorithm controls, range and display options. **Curve range** changes the curve's frequencies, including its endpoints; it is more than a view zoom. Its minimum spread is 1000 Hz.

The TX EQ graph shows the configured curve, rather than a measured filter response. The TX audio engine receives all configured 5, 10 or 18 band frequencies and gains, plus Q factors when enabled. CFC sends all configured bands and both enabled Q vectors directly to its existing WDSP profile path.
