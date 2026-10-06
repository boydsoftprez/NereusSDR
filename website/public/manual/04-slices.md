# Slices, receiver windows, and panadapters

A slice is a tuned receiver. A panadapter is the visible spectrum and waterfall window that hosts one or more slices. NereusSDR allocates radio DDC streams automatically; operators do not assign a DDC manually. This chapter covers creating and arranging receivers, choosing their view bandwidth, and diagnosing an allocation refusal. For tuning and CTUN operation, see [Tune and receive](03-receive.md). For shared ownership, see [Core sharing](08-shared-core.md).

## Create, select, and remove receivers

Start with a connected radio and a visible panadapter. Click the panadapter where the new receiver should appear so that it becomes active, then use **View > Add slice on active pan** (shortcut **Ctrl+R**). A new flag should appear on that pan. Click its flag to make it the selected slice before adjusting mode, filter, volume, or DSP. The selected flag identifies which slice follows the desktop RX controls; selecting a flag does not retune it.

If the command is disabled or no flag appears, read the toast/status reason. The radio's reported receiver capacity, active receiver use, hardware capabilities, Core permissions, and available audio/media resources can limit additions. There is no single operator-selected DDC map or universal slice count to override that allocation. Close an unneeded receiver or release a receiver you own before retrying. If an unavailable receiver belongs to another operator, do not remove it; ask the owner to release it or use the shared-slice actions described in [Core sharing](08-shared-core.md).

To remove a receiver you control, right-click its flag and choose **Remove slice**. The last receiver cannot be removed. On a slice you only listen to, the close action stops your listening subscription rather than deleting the Core's receiver. Confirm that the flag disappears from your window; when receiver capacity was the issue, wait for the Core's resource state to settle before adding again. A short notice can identify a receiver stopped by the Core, a radio limit, or unavailable media. Treat that notice as the reason for the missing signal, not as a display redraw problem.

## Listen to and control an existing Core slice

Click the bottom status area's **RX [letter] [state] ▾** picker to open
**Slices on this Core**. It lists the Core's receivers, including ones this
window is not currently listening to. Each row identifies the slice,
frequency/mode/passband, controller and listening/transmitting state. Click a
row to inspect its actions; selecting a row alone does not take control.

| Action | Result to expect |
| --- | --- |
| Listen in | Adds this window as a listener without changing tuning or taking control. Wait for “Listening to slice…” and its flag/audio here. |
| Select RX | Makes an already-listened-to slice the active receiver for the bottom RX area and controls. It does not grant tuning control. |
| Take control | Transfers control of the same slice, retaining its tuning. The previous controller keeps listening. The result explicitly says to select transmit separately. |
| Release | Releases a slice this window controls. Other listeners continue; with nobody left the slice closes. |
| Stop listening | Removes this window's listening subscription to a slice it does not control. The other operator's receiver remains. |
| New slice | Asks for an independent receiver on the active pan. It consumes available receiver resources; sharing an existing slice need not create another. |

To join another operator's receiver, select that row and **Listen in**, wait
for the Core's answer, then use **Select RX** if needed to make it active here.
Set your personal listening volume/output as described in
[chapter 3](03-receive.md). Mode, filter and DSP belong to the shared receiver,
so take control before changing them. Select **Take control** only when the
handoff is intended; verify “This window controls” and the flag's ownership
state before tuning. This action does not select a TX slice or take TX.

While a slice is **Transmitting**, Take control or Release is disabled with its
reason. **Away · reconnecting** means its controller is temporarily absent;
the chooser describes listening/takeover and the three-minute absence policy.
Do not interpret that label as a receiver fault. A pending request displays
**Asking the Core…** and holds duplicate actions until the result. On **The Core
did not answer**, reconnect/check the current row before retrying, since the
controller or receiver may have changed meanwhile. Close **×** to dismiss the
chooser; that does not release or stop listening.

## Arrange panadapters and floating windows

Choose **View > Pan Layout…** (shortcut **Ctrl+L**) to select one of the layouts shown in the dialog. The chosen layout is this desktop's presentation; it does not create radio receivers. The active pan is the one whose controls and add-slice action are targeted. Click inside the intended pan before adding a slice or opening a pan-specific action.

To place one pan on another monitor, select it and choose **View > Float active pan…**. Move the floating window like an ordinary desktop window. Its title-bar dock/return control puts it back into the main layout. Closing the floating window also returns the pan to the main window; it does not remove its slices. If the main layout seems to be missing a receiver, check whether its pan is floating on another display before deleting or recreating slices.

The **RX2**, **SUB RX**, and **Pan Swap** two-receiver layout controls are explicitly unbuilt. Do not use an old screenshot or a Thetis instruction to infer a second-pan arrangement button. Use **Pan Layout…** and the floating-pan action available in this build.

## Set a receiver's spectrum width

Right-click the slice flag and open **Sample rate**. The menu lists only rates accepted by the connected board and checks the resolved stream rate. Select a rate to change the bandwidth represented by that DDC stream. A wider rate shows more spectrum around the tuned signal but consumes more radio/network/DSP bandwidth; a narrower rate reduces the viewed window. The rate is not a private display zoom.

Slices that share one DDC stream share its sample rate, and their flags show the same resolved value. On Protocol 1 radios, the menu says **Sample rate (whole radio)** because the rate applies radio-wide. The exact choices depend on model and protocol. Automatic allocation decides which active slices share streams, so a rate change can affect another co-hosted receiver. In a remote session the Core may ask for confirmation or refuse a rate change because of radio capability, version, ownership, or current operation. Read the confirmation's affected-slice list or the refusal reason before accepting or retrying.

After choosing a rate, verify the checkmark in the flag's context menu and the width of the live spectrum. For Protocol 1, verify the other active receivers too. If the selection appears pending in **Setup > Hardware > Hardware Config > Radio Info**, the page displays both pending and active rate and says **Reconnect to apply new sample rate**. Reconnect the window after receiving that instruction, then recheck the active rate. Do not assume a selected value took effect while the page still reports it as pending.

## Understand tuning limits at the edge of a window

With ordinary tuning, the client recenters the receiver window as needed to keep the selected frequency in view. In CTUN, the pan stays centered while the slice frequency moves within the current DDC view. Toggle CTUN from the panadapter context menu: **Tuning > CTUN**. It is a pan/tuning behavior, not a checkbox on the VFO flag. Use CTUN when you want a fixed spectrum reference while making small tuning changes.

At the DDC window edge, the spectrum cannot show frequencies outside its current sampled span. With CTUN on, a receiver can approach or move beyond the visible/sample window; the receiver may become clipped or unavailable until it is retuned inside the window or CTUN is turned off. Turn CTUN off to let ordinary tuning recenter the view, or select a supported wider sample rate and check that it has resolved. If the trace disappears at an edge, first tune the slice back toward the center and check the sample-rate readback before removing the slice.

## Worked example: monitor two nearby signals

On an active pan, add a slice with **Ctrl+R** and tune it to the second signal. Select each flag in turn to confirm which slice is active before setting its mode, filter, and AF level. If both signals fit inside the current receiver window, leave the sample rate unchanged. If the second frequency falls outside the window, increase the sample rate from the relevant flag's **Sample rate** menu, then inspect the displayed active rate and both flags. If the rate menu labels the setting whole-radio, treat it as a station-wide change and review other receivers. To keep the pan fixed while making a small passband adjustment, use **Tuning > CTUN**; turn it off if the desired station moves beyond the sampled span.

If an add or rate operation is refused, read the exact Core reason, check ownership and other active receivers, then retry only after that constraint is resolved. Do not expect a manual DDC routing editor: **Setup > Hardware > DDC Routing** is explicitly unbuilt. The hardware and stream allocator make that mapping automatically.
