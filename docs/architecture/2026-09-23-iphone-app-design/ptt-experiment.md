# Hardware PTT experiment: observations

Plan Task 65, step 1 (R-IOS-15, D26, spec section 5.5 items 6 to 8). This sheet holds what
JJ observes on a real iPhone with the PTT button test (a Debug copy of the app: Setup,
Transmit, PTT buttons, "PTT button test"). The test joins a Push to Talk channel of its
own and never keys the radio. Each cell stays "pending (JJ, device)" until it is observed.

In scope (JJ, 2026-09-30): Apple's Push to Talk framework, a wired headset's button, a
Bluetooth PTT button through CoreBluetooth, and the Action button through
`PushToTalkTransmissionIntent`. Out of scope: the AirPods stem press and the Camera
Control button without a camera session (both undocumented by Apple), so they have no
rows here.

## The device

| What | Observed |
| --- | --- |
| iPhone model | pending (JJ, device) |
| iOS version | pending (JJ, device) |
| Build (Setup's build line) | pending (JJ, device) |
| Passcode set (needed for the locked readings) | pending (JJ, device) |
| Wired headset used | pending (JJ, device) |
| Bluetooth PTT button used | pending (JJ, device) |
| Date | pending (JJ, device) |
| Shared log attached | pending (JJ, device) |

## Question 1: does station audio keep playing while joined to a Push to Talk channel?

Connected to the station and listening, then joined to the test channel.

| Circumstance | Station audio keeps playing | Log lines (interruptions, route, "still running") |
| --- | --- | --- |
| App in front, joined | pending (JJ, device) | pending (JJ, device) |
| Another app in front, joined | pending (JJ, device) | pending (JJ, device) |
| Phone locked, joined, for 5 minutes | pending (JJ, device) | pending (JJ, device) |
| Phone locked, joined, during a test transmission | pending (JJ, device) | pending (JJ, device) |
| Phone locked, after the test transmission ends | pending (JJ, device) | pending (JJ, device) |
| After leaving the channel | pending (JJ, device) | pending (JJ, device) |

## Question 2: which presses begin and end a Push to Talk transmission?

"Source logged" is the source on the BEGIN and END lines (for example Headset button,
Bluetooth button, Action button, System Talk button).

### Wired headset button

| Circumstance | Press reaches the app | Begins | Ends | Source logged |
| --- | --- | --- | --- | --- |
| Unlocked, app in front | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Unlocked, another app in front | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Locked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Headset button events off (media buttons logged) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

### Bluetooth PTT button (CoreBluetooth)

| Circumstance | Press reaches the app | Begins | Ends | Source logged |
| --- | --- | --- | --- | --- |
| Unlocked, app in front | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Unlocked, another app in front | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Locked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| What one press reports (values in the log, rule that fits) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

### Action button (PushToTalkTransmissionIntent)

| Circumstance | Press reaches the app | Begins | Ends | Source logged |
| --- | --- | --- | --- | --- |
| Unlocked, app in front | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Unlocked, another app in front | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Locked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| App not running (closed from the app switcher) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

### The system's Talk button (for comparison)

| Circumstance | Press reaches the app | Begins | Ends | Source logged |
| --- | --- | --- | --- | --- |
| Locked, from the Push to Talk interface | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

## Question 3: can a press toggle?

The first press begins a transmission that stays active, with the microphone capturing
(the microphone line's buffer count rising, the END line's "microphone buffers" above
zero), until the second press ends it.

### Wired headset button

| Circumstance | First press begins and stays | Microphone captures throughout | Second press ends | Longest held |
| --- | --- | --- | --- | --- |
| Unlocked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Locked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

### Bluetooth PTT button (CoreBluetooth)

| Circumstance | First press begins and stays | Microphone captures throughout | Second press ends | Longest held |
| --- | --- | --- | --- | --- |
| Unlocked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Locked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

### Action button (PushToTalkTransmissionIntent)

| Circumstance | First press begins and stays | Microphone captures throughout | Second press ends | Longest held |
| --- | --- | --- | --- | --- |
| Unlocked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |
| Locked | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) | pending (JJ, device) |

## Decision (plan Task 65)

If questions 1 and 3 hold, step 2 builds every button that answered in question 2 and
lists the ones that did not in the release notes and on Setup's PTT buttons page. If 1
or 3 fails, the observations go to JJ before anything is built.

| Outcome | Observed |
| --- | --- |
| Question 1 holds | pending (JJ, device) |
| Question 3 holds | pending (JJ, device) |
| Buttons that answered in question 2 | pending (JJ, device) |
| Buttons that did not | pending (JJ, device) |
