# 19. Amplifiers and tuners

The desktop and iPhone/iPad can show and control supported station accessories through the Core. The Core owns the device connections and telemetry. A remote desktop or phone sends a supported command to the Core, then follows the Core's reported state; a click by itself is not confirmation. Read the device state and any refusal beside the control before transmitting.

Desktop setup pages live at **File > Settings > CAT & Network**. 4O3A devices are under **4O3A**. RF2K-S is under **RF-Kit**. The desktop's right-side applets show live operation: **Amp**, **Tuner**, and **RF-Kit**. The phone uses **Radio > Power Genius XL**, **Tuner Genius**, or **RF2K-S**. The phone pages show the Core's device records and use the same shared controls where its Core connection and capability catalogue support them.

These are station controls. Coordinate with other operators before changing operating state, antenna, network address, pairing, limits or shared tune settings. The source-verified procedures have not been exercised on live hardware; verify physical cabling, RF path, power limits and amplifier/tuner manuals before operating.

## Connect the PGXL or TGXL to the Core

First connect the desktop to the radio whose peripherals you intend to configure. Open **File > Settings > CAT & Network > 4O3A > General**. The yellow or green banner identifies the radio/MAC whose peripheral records are being edited. On a remote desktop, the page acts on the Core's accessory connection through the station link. The phone's accessory pages are already scoped to the connected Core.

1. Check **Enable 4O3A integration**. This is a per-radio master gate. It starts the Core's FlexAPI listener on TCP port **4992**, enables PGXL/TGXL connection attempts, and enables their setup pages and applets. When off, the listener is not bound and the accessory tabs are greyed. On the phone, read the **Power Genius XL** and **Tuner Genius** rows: a missing device, disabled feature, or old Core has an explanatory message.
2. Under **PGXL / TGXL Peripherals**, select **Scan LAN** to discover devices on the local network, or enter the device address and port in the appropriate row, then **Connect**. Verify the row identifies the expected device and reports connected before operating it. On the phone the Core's accessory header shows connection phase, model/nickname and error text; the Core performs the scan and connection.
3. Keep each device address reachable from the Core computer after connecting. The Core, not the phone, must have network access to the amplifier and tuner. A phone being connected to the radio does not establish that the Core can reach an accessory.
4. If a device drops, read its error/status. Use the row's **Reconnect** action or applet's right-click **Reconnect** when shown. If the address has changed, update it in **Peripherals** and reconnect. Do not toggle the 4O3A master gate as a substitute for checking the address and link state.

The General page also contains **PGXL Interlock**. Its settings apply on the Core and affect subsequent TX requests; see the interlock procedure below before enabling TX.

## Operate the Power Genius XL

The **Amp** applet shows forward power, SWR and temperature gauges, and when reported, mains volts, drain amps and **MEffA**. The **OPERATE**/**STANDBY** button follows the PGXL's reported state. Click it to request the other state, then wait for the button and PGXL page's **State** badge to change. `POWERUP`, `STANDBY`, `FAULT`, `IDLE`, `OPERATE` and `TRANSMIT_A`/`TRANSMIT_B` are not interchangeable: a green-looking button is not a fault clear, and `FAULT` requires reading the cause before trying again.

Use the applet's right-click menu to open **Open PGXL Advanced...**, select **Disconnect** or **Reconnect**, or choose **Copy diagnostics to clipboard**. The copied diagnostics are useful when reporting an unexpected connection or meter result.

The phone's **Radio > Power Genius XL** page shows the Core's PGXL state, forward power/SWR/temperature gauges, numeric output and MEffA, band-follow/paired-with status, output-cap alert, and fault history. Tap **OPERATE** or **STANDBY** to request a state change. The page reports why a switch is unavailable, such as the Core being disconnected or the radio being on the air. Wait for the state text to follow the amplifier report. The phone interlock card is readback; use its **Change in Setup** route, when offered, for the Core's interlock settings.

### PGXL output cap

On the desktop, **4O3A > PowerGenius XL > Hardware > TX Power Cap** has **Enable soft cap** and a watts field from 100 to 2000 W (default 1500 W). The cap is a Core-side transmit/output limit and alert, not a new calibrated meter profile. Check **Enable soft cap**, set the intended limit, and confirm the pending indicator. The Core alerts in its windows if PGXL output rises above the limit. On a remote desktop, **Enable soft cap** and its completed watts value are sent to the Core; finish editing the watts field before the request so the entire value is sent once. The phone exposes the Core's cap alert/readback, not this desktop hardware form.

### PGXL interlock policy

Under **4O3A > General > PGXL Interlock**, choose **Interlock Mode**:

| Mode | If PGXL is present but not in OPERATE, or the enabled SWR gate trips | Operator consequence |
|---|---|---|
| **Disabled** | The policy does not interfere with TX. | No policy warning or block; use only when the station's separate safety procedure provides the protection. This is the default. |
| **Warn** | The TX request proceeds and a warning is shown. | Correct the amplifier state or SWR before continuing. The warning is not an interlock. |
| **Block** | The TX request is refused. | Set PGXL to OPERATE or correct the SWR cause, then make a new TX request. |

**Grace Period** is 0 to 30,000 ms, in 100 ms steps, default 3000 ms. It delays enforcement of the SWR gate after the PGXL enters OPERATE so warm-up current/SWR settling does not immediately nuisance-trip. It does not bypass the amplifier-state test. **Enable SWR Gate** turns the second check on; **Max SWR** is 1.0 to 10.0, default 3.0, and only applies while the gate is checked. A value *above* the limit trips the same Warn or Block policy. Changes take effect immediately for later TX requests. Phone accessory records show mode, grace and SWR gate; the app identifies if the Core cannot share those records. Confirm the actual interlock readback before testing a TX path.

## Configure PGXL device settings

Open **File > Settings > CAT & Network > 4O3A > PowerGenius XL**. When connected, the page reads the device's current setup and network configuration. A remote desktop receives the Core's reported device settings and last device answer; its editable controls are available only when the Core offers device-settings capability. **Apply Network Settings**, **Revert**, and **Save & Reboot Amp** also require a live Core-to-PGXL connection. The page shows the unavailable reason instead of applying a local guess.

| Control | Effect and timing | How to apply or verify |
|---|---|---|
| **Nickname** | Local/Core-facing name shown in NereusSDR. Editing and leaving the field sends/stores the name; the live device may echo it. | Enter a clear label such as `Station PGXL`. Reopen the page or inspect the accessory row to confirm the stored name. |
| **Bias Mode: Class A / Class AB** | Sends the selected bias setup to the PGXL and marks hardware changes pending. | Select the documented amplifier mode for the station setup. Read the pending message, then use **Save & Reboot Amp** to persist it in the device. |
| **Fan Mode: Auto / Quiet / Continuous** | Sends the fan setting and marks it pending for device save/reboot. | Choose based on the amplifier's installation and thermal requirements. Confirm selection, then save/reboot. |
| **LED Intensity** | 0 to 100; the completed slider value is sent to the PGXL and marks the setup pending. | Move the slider and read its numeric value; finish at the intended value before saving/rebooting. |
| **Enable soft cap / watts** | Core-side output cap and alert described above; changing either marks the cap settings pending in the local Core state. | Set the watts field, enable the cap, and confirm the page's readback/alert path on the phone or another window. Do not treat this as the amplifier's internal hardware configuration. |
| **Use DHCP** | Selects automatic network configuration. When checked, the manual address fields are disabled. | Select DHCP, then **Apply Network Settings**. Expect the device address to change and the connection to drop/reappear. |
| **IP Address / Netmask / Gateway** | Manual IPv4 configuration. The fields accept dotted IPv4 octets; they are editable only when **Use DHCP** is off. | Confirm the new address is reachable from the Core before applying. Select **Apply Network Settings**. If it becomes unreachable, use LAN discovery and update its row in **Peripherals**. A remote desktop asks for confirmation because it may lose access at the old address. |
| **Auto-pair on connect** | Sends the FlexRadio pairing handshake after a successful PGXL connection. Default is on. | Leave on when this Core should pair automatically; turn off only if pairing is managed externally. Applies on a later successful connection. |
| **TX Antenna: ANT1 / ANT2** | Selects the PGXL transmit antenna used by the Core's PGXL association. | Choose the antenna that is physically wired to the station's intended path. The selection is a Core setting and takes effect on the next PGXL connection. |
| **Slice Binding: Slice A / Slice B** | Selects the slice PGXL uses as the FlexAPI band source. | Bind to the slice whose band should follow the amplifier. This setting takes effect on the next PGXL connection. |
| **Revert** | Requests fresh setup and network values from the connected PGXL and clears the pending local edit state. | Use before applying if you want to discard uncommitted hardware changes; check the reloaded field values. |
| **Save & Reboot Amp** | Saves the device setup to PGXL and reboots it. A confirmation dialog precedes the action. | Use after bias/fan/LED changes; expect a temporary disconnect and wait for a new connection/state. On a remote window, first read the confirmation showing that the Core will save and reboot its amplifier. |
| **Fault History > Clear All** | Clears the locally or Core-recorded history table. It does not reset the PGXL's hardware fault state. | Record useful fault rows before clearing. Fault rows include time, state, forward watts, SWR, temperature and likely cause. |
| **Diagnostics** | Read-only connection measures: uptime, last round-trip time, missed keepalives, session reconnect count, frames and bytes in/out. | Use these to distinguish a link problem from an amplifier state/fault. Resetting history is separate from restoring the connection. |

Changing IP can make the PGXL unreachable. Before applying a static address, check the Core's subnet, gateway and physical LAN. If status does not return, rediscover the amplifier, update its configured address and reconnect before attempting other settings.

## Operate the Tuner Genius XL

### Run a desktop tune cycle

Use the desktop **Tuner** applet with the radio connected, the intended TX
slice/band selected, the physical antenna path known, and no transmission or
other RF flow active. Have the intended drive source/value and the amplifier's
reported state in view. **TUNE** creates a carrier as part of the TGXL sweep;
it is not a receive-only relay command. The cycle temporarily places an
operating PGXL in **STANDBY** and restores its prior operating state when the
cycle ends.

1. Read the applet's TGXL connection/state, antenna, forward power/SWR, and
   **C1**, **L**, **C2** relay reports. Confirm the selected antenna is the one
   physically connected to the intended load. If PGXL is connected, note
   whether it reports **OPERATE** or a non-operating state. Resolve stale or
   unavailable status before requesting a tune.
2. Check the TX owner, transmit readiness, current drive source/value, and
   station protections in [Set up and transmit](05-transmit.md). End any
   existing MOX/TUNE/test transmission and wait for receive. Do not use a
   numerical RF target from this manual; use the station's established safe
   tuner path and verified load.
3. Click the TGXL applet's **TUNE**. Follow the button to **TUNING...** and
   watch for TGXL's tuning state. If PGXL was in **OPERATE**, confirm its
   reported transition to **STANDBY** as the cycle is prepared. Do not start a
   second cycle while one is active.
4. On completion, wait for TGXL to leave its tuning state and for the settled
   SWR result to appear briefly on the button. Confirm the carrier is off and
   receive returns. Check the final relay and meter reports. If the cycle
   reports refusal, **no PTT**, **low RF power**, or does not enter tuning,
   confirm the applet returned to **TUNE**, the carrier was dropped, and the
   refusal/status is visible. Recheck TGXL connection, transmit permission,
   drive source, physical path, and the amplifier state before another
   attempt.
5. Confirm PGXL's reported state. If it was operating before the cycle, the
   Core requests **OPERATE** after tuning once RF and pending key actions have
   cleared. It can remain in **STANDBY** while a key is active or pending;
   wait for the state report before relying on the amplifier. If it was not
   operating before the cycle, the routine leaves its prior non-operating
   state. Save the settled relay values to memory only after confirming the
   antenna and band shown by the applet.

The applet's **TUNE** starts the coordinated Core cycle. The separate cycle
button requests **OPERATE → BYPASS → STANDBY → OPERATE**; read the reported
state after each click. Scroll over a relay bar only for an intentional
one-step adjustment and verify the bar changes. A grey control or refusal
reason is the current permission/state result. The iPhone/iPad **Radio >
Tuner Genius** page exposes reported state, meters, relay-step buttons, and
antenna selection, but manual **TUNE** is unavailable there.

### Tuner antenna and tune memory

Use the desktop applet's context menu for the current antenna and band. The
memory is a stored record of reported relay values, not a physical relay
command.

1. Verify the current antenna and TX band. Right-click the desktop **Tuner**
   applet and choose **Save current tune memory** to store the currently
   reported **C1**, **L**, and **C2** values for that antenna/band.
2. To inspect a saved entry, choose **Recall tune memory**. The applet loads
   the values into its local display/cache; it sends no absolute relay
   positions to TGXL. The reported physical relay values remain authoritative.
   A later physical **TUNE** starts a fresh autotune from the tuner's current
   position. **Clear tune memory** removes only the current antenna/band
   record.
3. On iPhone/iPad, **Radio > Tuner Genius > Tune memory** lists Core-stored
   entries and allows clearing a row. This page does not tune the radio or
   issue relay commands. A recalled-memory match indication compares the
   TGXL report with the stored values; it does not prove a physical recall.
4. The shared **Recall a stored tune on a band or antenna change** setting is
   a separate automatic action. When enabled, a qualifying band change on the
   TX-bound slice checks for a saved entry for the current antenna and new
   band; if one exists and TGXL is connected, the Core sends a fresh
   **autotune** command. The stored relay values are not sent as target
   positions. An antenna-only change does not trigger this handler, and this
   automatic request does not itself key NereusSDR MOX. After a band change,
   inspect TGXL's tuning/state, relay and SWR readbacks and handle any refusal
   before transmitting.

### TGXL advanced settings and recovery

Under **4O3A > Tuner Genius XL**, **Identity & Status** shows nickname, firmware, serial, state and variant. **Nickname** is a device label. **Antenna Labels** names ANT1-3 in the app and shared/Core presentation; it does not rename a physical antenna in TGXL firmware. **Network** has **Use DHCP**, **IP Address**, **Netmask**, **Gateway**, and **Apply Network Settings**. Apply with the address known to be reachable from the Core. It can interrupt the connection; in a remote window confirm the warning naming that the Core will change its Tuner Genius address. Reconnect using the new address if necessary.

**Revert** requests the current network/setup from the connected device and clears pending network edits. **Save & Reboot Tuner** commits the device configuration and reboots TGXL. Confirm the warning before using it and wait for status to return. **Diagnostics** reports uptime, RTT, missed keepalives, reconnects, frames and bytes. Remote desktop exposes **Fault History** and **Clear All** through the Core's recorded faults; clearing history is not a hardware reset. A local desktop does not promise a populated TGXL fault table when no local fault producer is present. The phone's **Advanced** page shows the Core-described identity, antenna labels, network and diagnostic values that its catalogue offers; unavailable Core/device-setting capabilities are explained beside the controls.

## Operate the RF-Kit RF2K-S

Open the **RF-Kit** applet to read its link indicator, model/nickname, **OPERATE/STANDBY** request, forward power, SWR, temperature, volts and amps. The applet's **ANT 1** through **ANT 4** buttons follow the amplifier's reported selection; the active button lights only after the report identifies it. Select the antenna that is physically connected to the desired path. A disabled/missing antenna reflects the amplifier's availability report.

The RF-Kit tuner status reports **TUNING...**, **BYPASS**, **TUNED frequency (setup)**, **NOT TUNED**, or unknown state. **TUNE** and **BYPASS** buttons are intentionally disabled because the RF2K-S firmware used by this interface does not accept tuner write commands. Use the amplifier's front panel for those two functions. Never tell the applet that a tune or bypass succeeded when the firmware did not accept a command.

On iPhone/iPad, **Radio > RF2K-S** shows the Core's model and status, OPERATE/STANDBY, forward power/SWR/temperature and numeric forward watts, SWR, volts and amps. Its four antenna buttons follow Core reports and may have operator labels. It displays the tuner's reported **AUTO**, **MANUAL**, **AUTO TUNING** or **BYPASS** mode, but its **TUNE** and **BYPASS** buttons remain greyed with the front-panel instruction. The phone's **RF-Kit settings** row opens the Core's connection/settings page. The Core's link/error text and the active antenna/band-follow messages take precedence over the last requested action.

### RF-Kit connection and station settings

Open **File > Settings > CAT & Network > RF-Kit**. The General tab shows the radio/MAC whose peripherals are being edited, the Core-local master switch, live status and band-follow status. The RF2K-S tab contains these controls:

| Control | Effect and required readback |
|---|---|
| **Host** and **Port** | Configure the amplifier endpoint for this radio. The default port is 8080; the host is entered for the RF-Kit on the station LAN. In a remote desktop, these fields describe the Core's endpoint. |
| **Enable RF-Kit Amplifier integration** | Per-radio gate for the applet, RF2K-S settings tab, and automatic TCI band tracking. Off by default. Enable only when using the RF2K-S; verify that its applet and tab become available. |
| **Auto-reconnect on disconnect** | Allows the client to reconnect after an RF-Kit disconnect. Default on. Check the live status after reconnect rather than assuming the prior session resumed. |
| **Poll interval** | Set telemetry polling from 250 to 5000 ms; default 1000 ms. Lower values request status more often and increase update traffic. |
| **Test connection** / remote **Connect** | Attempts a connection to the configured endpoint. On a remote desktop this asks the Core to save the address and connect. Verify the link/status label, device identity and updated telemetry. |
| **Disconnect** | Ends the Core's RF-Kit connection. Read status to confirm it is no longer connected. |
| **Set amp to TCI mode** | Requests that the amp use TCI operation. The Core refuses this while on air. When using a Core that automatically follows its TCI server state, the button may be unavailable with an explanation. Verify the amp interface/band-follow status after the request. |
| **Reset amp error state** | Asks the amplifier to reset its reported error state. It does not repair wiring or make an unresolved fault safe. Read the updated state and fault text; if it persists, stop and investigate the amp. |
| **ANT 1** through **ANT 4** labels | Save operator-facing names (maximum 12 characters each). RF2K-S firmware does not supply antenna names; these are NereusSDR labels, not firmware labels or antenna switching rules. |
| **Save** | Saves **Auto-reconnect**, **Poll interval**, and the four antenna labels. On a remote desktop these are written as Core station settings; a request refusal leaves the Core's accepted values displayed with the reason. The Host/Port address is handled with **Connect** on older Core capability levels. |
| **Live diagnostics** | Read-only successful/failed poll counts, RTT, reconnects, connected-since and last poll. Use them to distinguish stale telemetry from a running connection. |

On a remote desktop, a connected Core and catalogue support determine which controls are enabled. It does not open a direct phone-to-amplifier link. **Set amp to TCI mode**, **Reset amp error state**, **Connect**, **Disconnect** and enabled/master changes are Core requests; check the displayed result/reason. On an older Core, it may put the amp in TCI mode itself while its own TCI server is enabled. Changing the saved host address does not automatically dial on older Core versions; select **Connect** to submit it.

If band follow says the RF-Kit does not follow the radio, compare the RF-Kit's TCI server address/port with the displayed Core TCI endpoint, then use its own network/interface configuration to make them match. NereusSDR reports the observed follow state; do not infer successful tracking from enabling the applet alone.

## Phone and remote-device availability

The phone reaches the Core's PGXL, TGXL and RF-Kit records and sends documented station actions through its connection. A device row can be absent because the Core does not report that accessory; an action can be grey because the Core lacks a command/capability, is not connected to the device, or the radio is on the air. The page gives the Core's reason. If a request is accepted, wait for the device's status field, state text, antenna indication, relay value or settings record to update. If refused, the previously accepted value remains and the page shows the refusal.

Remote desktop forms have their own availability. PGXL/TGXL own-device hardware/network forms are editable only if that Core offers device settings. Their Apply/Revert/Save & Reboot actions require an active Core-to-device link. RF-Kit remote settings use the Core's station-settings catalogue and full-control capability; a Core may support status/control but not a given settings write. Keep the Core connection open while applying a shared change, and do not repeat an action until the current status or refusal has arrived.

For the radio-side front-end, antenna connectors, ADC options and general calibration, see [Chapter 17](17-hardware-antennas-calibration.md). For PureSignal feedback and PA gain/watt-meter calibration, see [Chapter 18](18-puresignal-diversity.md). For transmitting, RF drive, TUNE and protective blocks, see [Chapter 05](05-transmit.md).
