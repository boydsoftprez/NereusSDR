# Phase 3P-III: RF-Kit RF2K-S Bench Verification Matrix

Operator: KG4VCF
Hardware under test: RF-Kit / RF-Power RF2K-S (serial: fill in at bench)
Firmware at test time: capture via `curl -s http://<amp>:8080/info | jq .`
NereusSDR build: record commit SHA at the start of the bench session
Radio under test: ANAN-G2 / HL2 / etc (fill in at bench)

## Pre-flight

- Amp powered on, on the LAN, reachable at known IP and port 8080.
- Amp's operational_interface initially any value (test row 5 sets it to TCI).
- NereusSDR launched; Setup > CAT & Network > RF-Kit > Enable checked.
- TCI Server (Setup > CAT & Network > TCI Server) running on default port.

## Matrix

| # | Test | Pass criteria | Result | Notes |
|---|---|---|---|---|
| 1 | First-time setup (Enable -> host -> port -> Test connection -> Save) | Applet appears in right column within 5 s of Save. Status dot green. | | |
| 2 | Set amp to TCI mode via Setup button | /operational-interface poll returns TCI within 2 s. General tab live status shows TCI. | | |
| 3 | TCI band tracking | Change band on NereusSDR (e.g. 80m -> 40m). /data poll on amp returns matching band within 1 s. | | |
| 4 | Antenna switch | Click ANT 2 in applet. /antennas/active returns INTERNAL #2 within 1 s. Button highlight follows. | | |
| 5 | OPERATE / STANDBY toggle | Click status pill (STANDBY -> OPERATE). /operate-mode poll confirms. SMeterWidget switches to 2 kW scale immediately. | | |
| 6 | TX into amp under OPERATE | Key mic, hold steady-state carrier. Fwd gauge animates with live power. SWR gauge tracks. Telemetry strip shows Vmains, Iamp. | | |
| 7 | Release MOX | Gauges fall to idle. SMeterWidget reverts to S-scale. | | |
| 8 | Network drop + recovery | Pull amp ethernet for 30 s, replace. Status dot yellow during drop, red after 3 failed polls, green again within 5 s of recovery. | | |
| 9 | Master toggle OFF mid-session | Setup > RF-Kit > uncheck Enable. Applet hides. Containers > Applets > RF-Kit greyed. Setting checkboxes do not bounce back. | | |
| 10 | Master toggle ON again | Applet reappears with user's previous visibility preference. Live data resumes. | | |
| 11 | TUNE button greyed | Hover TUNE button. Tooltip explains firmware limitation. Click does nothing. Identical for BYPASS. | | |
| 12 | Tuner status line during front-panel TUNE | Press TUNE on amp's front panel. Applet status line shows "TUNING..." then "TUNED X.XXX MHz (LC)" within 1 s of amp completion. | | |
| 13 | Antenna label round-trip | Set ANT 1 label to "80m dipole" on RF2K-S tab -> Save -> restart NereusSDR. Label persists on applet ANT 1 button. | | |
