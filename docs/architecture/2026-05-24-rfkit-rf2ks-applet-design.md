# RF-Kit RF2K-S Applet + Setup Integration

Status: Design, pending implementation plan.
Author: J.J. Boyd (KG4VCF), with AI-assisted drafting via Claude Code.
Date: 2026-05-24.

---

## 1. Goal

Wire NereusSDR up to the **RF-Kit / RF-Power RF2K-S** solid-state HF/6 m linear
amplifier as a first-class accessory, sitting alongside the just-shipped 4O3A
(PGXL + TGXL) integration. The RF2K-S is a single physical box with an
integrated antenna tuner that exposes a REST API on TCP port 8080 plus a TCI
client that subscribes to a TCI server for frequency tracking.

Operator-visible success criteria:

* **Live applet in Container #0 right column** showing forward power, SWR,
  temperature, mains voltage, drain current, efficiency, the four internal
  antenna buttons (with operator-set labels), tuner status (mode + last-tuned
  frequency + L + C topology), and an OPERATE/STANDBY toggle. Reachable any
  time the amp is online.
* **TCI band tracking is automatic** once the operator points the amp at our
  TciServer (`PUT /operational-interface` body `{"operational_interface":"TCI"}`).
  No periodic re-tuning, no polling lag visible to the operator.
* **Analog SMeterWidget switches to 2 kW scale** the moment the amp is in
  OPERATE, reads forward power and SWR from amp telemetry, generalizing the
  existing PGXL-aware behavior so any external amp drives the same scale
  switch.
* **Setup &raquo; CAT &amp; Network &raquo; RF-Kit** has one tree node, two
  tabs inside (General + RF2K-S), mirroring the FourO3APage convention exactly.
  Master toggle on General greys out the RF2K-S tab and removes the applet.
* **Menu surfaces:** Containers &raquo; Applets &raquo; "RF-Kit RF2K-S"
  visibility toggle (greyed on master OFF), applet right-click context menu
  with Open Advanced / Disconnect / Reconnect / Copy diagnostics. Mirrors PGXL
  exactly. No new Tools menu entry, no keyboard shortcut.
* **Honest UX about firmware limitations:** TUNE and BYPASS buttons are
  visually present in the applet but greyed with tooltip pointing at the
  amp's front panel. RF2K-S firmware G200C267 (latest observed) does not
  expose tuner-trigger or bypass-toggle endpoints; NereusSDR cannot wire them
  without future firmware support.

Everything ships in **one combined PR**.

---

## 2. Source provenance and licensing

This epic is **NereusSDR-native** for the `Rf2ksConnection` class, the
`Rf2ksApplet`, and both setup tabs. There is no AetherSDR equivalent
(AetherSDR is FlexRadio-only; no RF-Kit support). The structural template is
the just-shipped FourO3APage / PgxlAdvancedPage / AmpApplet, which are
themselves AetherSDR ports.

### Structural references (no source code ported)

| Local file | What we mirror |
|---|---|
| `src/gui/setup/FourO3APage.{h,cpp}` | One tree node, QTabWidget with General + per-device tabs, master toggle gates detail tabs. We copy the pattern, not the code. |
| `src/gui/setup/PgxlAdvancedPage.{h,cpp}` | Section grouping (Connection / Display / Diagnostics / Faults). |
| `src/gui/applets/AmpApplet.{h,cpp}` | Header + 3 HGauge bars + telemetry strip + right-click context menu signals. |
| `src/core/PgxlConnection.{h,cpp}` | Connection class API shape (signals, reconnect, diagnostics counters). The wire protocol underneath is REST not C/R/S/V. |

No code from AetherSDR is involved in this epic. No upstream attribution
headers required on the new files; they are NereusSDR-original.

### Protocol references

| Source | Use |
|---|---|
| [RF-Power official OpenAPI 3.0 swagger.json](https://rf-power.eu/wp-content/uploads/2024/12/swagger.zip) (RFKIT 0.9.0, Dec 2024) | Authoritative REST surface. 12 documented endpoints, byte-checked against the live amp. |
| [RF-Kit / RF-Power user manual EN 06/25](https://rf-power.eu/wp-content/uploads/2025/06/RF2K-S_User_Manual_EN_06_25.pdf) | UDP port 12060, TCI on port 50001-ish, operational-interface enum, antenna numbering. |
| [ExpertSDR3 TCI Protocol PDF](https://raw.githubusercontent.com/ExpertSDR3/TCI/main/TCI%20Protocol.pdf) | TCI command spec; `vfo:N,M,F;` and `split_enable:` are what the amp subscribes to. TUNE/DRIVE/TUNE_DRIVE are bidirectional but the amp ignores them (per firmware source). |
| [CT1IQI/RF2K-S firmware mirror (v159, stale)](https://github.com/CT1IQI/RF2K-S) | Used to verify the amp's TCI handler (`tciSupport.py`) only parses `vfo:` and `split_enable:`. Also confirms there are exactly 12 REST routes (`restServer.py:232-243`). |
| Live amp probe (J.J.'s amp at 192.168.109.254:8080, G200C267) | Final ground-truth confirmation: OPTIONS Allow headers per endpoint match the swagger, 404s on every speculative path (`/tune`, `/autotune`, `/tuner/tune`, `/config`, etc). |

### License posture

The RF2K-S firmware is proprietary (no LICENSE in CT1IQI mirror, no FOSS
grant from RF-Power). NereusSDR never bundles or distributes any RF-Kit
firmware. NereusSDR talks to the published REST API and TCI subscription
only.

A future firmware modification (private fork or patch-in-place demo) for
proof-of-concept tune-trigger support is **out of scope** for this epic. See
section 12 "Open follow-ups".

---

## 3. Architecture overview

```
+-----------------------+       +----------------------+
|  ANAN / HL2 / etc.    |       |  RF2K-S on LAN       |
|  OpenHPSDR P1/P2      |       |  REST TCP 8080       |
+-----------+-----------+       +-----+----+-----------+
            |                         ^    |
            | OpenHPSDR UDP           |    | TCI WebSocket
            v                         |    v (amp is client)
+-----------+-----------+             |  +----+-----------+
|  RadioConnection      |             |  |  TciServer     |
|  (worker thread)      |             |  |  (existing)    |
+-----------+-----------+             |  +----+-----------+
            |                         |       |
            | queued signals          |       | already wired:
            v                         |       | emits tune:0,t;
+-----------+----------------+        |       | trx:0,t; vfo:..
|  RadioModel (main thread)  |  REST  |       |
|                            +--------+       |
|   m_rfKitConnection (new)  | poll & PUT     |
|   m_rfKitEnabled (new)     |  1 Hz          |
+----+-----+-----------------+                |
     |     |                                  |
     |     +----> Rf2ksApplet (Container #0) <-- statusUpdated
     |
     +----------> SMeterWidget (amp-in-OPERATE -> 2 kW scale)
```

Three independent paths converge on the amp:

1. **REST polling and control** - NereusSDR is the client. 1 Hz GET on
   `/power`, `/tuner`, `/data`, `/antennas/active`, `/operate-mode`,
   `/operational-interface`. On-demand PUT on the writable endpoints.

2. **TCI band/PTT push** - Existing TciServer (Phase 3J-1) already emits
   `vfo:0,0,F;`, `vfo:0,1,F;`, `split_enable:0,bool;`, `trx:0,bool;`, etc.
   The amp connects as a TCI client; firmware source confirms it only
   subscribes to `vfo:` and `split_enable:` (frequency tracking). Our
   `tune:` and `trx:` events are ignored by the amp; we send them anyway
   since they cost nothing.

3. **OpenHPSDR radio path** - Unchanged. The amp does not participate.

No bidirectional pairing like PGXL. No interlock-create. No
flexradio-pair. Just REST + TCI subscription.

---

## 4. Network class

### `src/core/Rf2ksConnection.{h,cpp}` (new)

Single QObject, lives on the main thread (the same thread as `RadioModel`).
Uses `QNetworkAccessManager` for HTTP; no `QTcpSocket` direct. Wraps state +
1 Hz polling + on-demand control verbs.

```cpp
namespace NereusSDR {

struct RfKitPowerSnapshot {
    int   forwardW;
    int   reflectedW;
    int   forwardMaxW;       // session peak hold reported by amp
    int   reflectedMaxW;
    float swr;
    float swrMax;
    float temperatureC;
    float voltageV;
    float currentA;
};

struct RfKitTunerSnapshot {
    enum class Mode { Bypass, Manual, AutoTuning, Auto, Unknown };
    Mode mode;
    QString setup;              // "NOT TUNED" / "BYPASS" / "CL" / "LC"
    int lValuenH;
    int cValuepF;
    int tunedFrequencyKHz;
    int segmentSizeKHz;
};

struct RfKitAntenna {
    enum class Type { Internal, External };
    enum class State { Active, Available, Disabled };
    Type type;
    int number;                 // 1..4 internal, 1..16 external
    State state;
};

class Rf2ksConnection : public QObject {
    Q_OBJECT
public:
    explicit Rf2ksConnection(QObject* parent = nullptr);

    bool    isConnected()    const { return m_connected; }
    QString deviceName()     const { return m_deviceName; }      // /info.custom_device_name
    QString softwareVersion() const { return m_softwareVersion; } // "G200C267"
    QString peerAddress()    const { return m_host; }
    quint16 peerPort()       const { return m_port; }

    // Diagnostics (Setup -> RF-Kit -> RF2K-S tab)
    qint64  connectedSinceMs()   const noexcept { return m_connectedSinceMs; }
    qint64  lastPollMs()         const noexcept { return m_lastPollMs; }
    int     pollsSucceeded()     const noexcept { return m_pollsSucceeded; }
    int     pollsFailed()        const noexcept { return m_pollsFailed; }
    int     rttAvgLast10Ms()     const noexcept { return m_rttAvg; }
    int     reconnectAttempts()  const noexcept { return m_reconnectAttempts; }

    // Latest snapshots (1 Hz refreshed by pollOnce)
    RfKitPowerSnapshot  lastPower() const { return m_lastPower; }
    RfKitTunerSnapshot  lastTuner() const { return m_lastTuner; }
    QList<RfKitAntenna> antennas()  const { return m_antennas; }
    RfKitAntenna        activeAntenna() const { return m_active; }
    QString             operateMode()  const { return m_operateMode; }     // OPERATE / STANDBY
    QString             operationalInterface() const { return m_opIfx; }   // UNIV / CAT / UDP / TCI
    QString             lastError() const { return m_lastError; }

public slots:
    void connectToAmp(const QString& host, quint16 port = 8080);
    void disconnect();
    void setPollIntervalMs(int ms);

    // Control verbs (REST PUT / POST).  All fire-and-forget; result delivered
    // via the matching signal below.
    void setActiveAntenna(RfKitAntenna::Type type, int number);
    void setOperateMode(const QString& mode);              // "OPERATE" or "STANDBY"
    void setOperationalInterface(const QString& iface);    // "UNIV" / "CAT" / "UDP" / "TCI"
    void resetError();

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);

    void powerUpdated(const RfKitPowerSnapshot& snap);
    void tunerUpdated(const RfKitTunerSnapshot& snap);
    void antennasUpdated(const QList<RfKitAntenna>& list);
    void activeAntennaUpdated(const RfKitAntenna& a);
    void operateModeUpdated(const QString& mode);
    void operationalInterfaceUpdated(const QString& iface, const QString& errorField);
    void infoUpdated(const QString& deviceName,
                     const QString& softwareVersion,
                     const QString& nicknameFromAmp);
    void dataUpdated(int bandM, int frequencyKHz, const QString& status);

    void faultObserved(const QString& kind, const QString& detail);  // for FaultHistory

private slots:
    void pollOnce();                  // 1 Hz timer
    void scheduleReconnect();
    void onReplyFinished();

private:
    // ... QNetworkAccessManager, QTimer, snapshot storage ...
};

} // namespace NereusSDR
```

### Polling behavior

`pollOnce` issues 6 parallel GETs each tick: `/power`, `/tuner`, `/data`,
`/antennas/active`, `/operate-mode`, `/operational-interface`. `/antennas`
(full list with state) and `/info` are polled less frequently (every 10 s)
since they change rarely. Each reply parses, updates the snapshot, emits the
matching `*Updated` signal if anything changed.

On any network error (timeout, 5xx, JSON parse fail), `m_pollsFailed`
increments. After 3 consecutive failures, emit `disconnected()`, start
exponential backoff reconnect: 1 s, 2 s, 4 s, 8 s, 16 s, 32 s, 60 s (cap).
On success, reset backoff to 1 s.

### Control verb implementations (reference)

```
setActiveAntenna(INTERNAL, 2)
   -> PUT /antennas/active body {"type":"INTERNAL","number":2}

setOperateMode("OPERATE")
   -> PUT /operate-mode body {"operate_mode":"OPERATE"}

setOperationalInterface("TCI")
   -> PUT /operational-interface body {"operational_interface":"TCI"}
   The amp's TCI host/port comes from a follow-up amp-side config or our
   PUT body (the schema accepts extension; needs probe).  If the
   operational-interface PUT does not accept a host/port hint, the
   operator configures the TCI target on the amp's touchscreen once.

resetError()
   -> POST /error/reset
```

---

## 5. UI surfaces

### 5.1 Rf2ksApplet (`src/gui/applets/Rf2ksApplet.{h,cpp}`, new)

Inherits `AppletWidget`. `appletId() = "RfKit"`, `appletTitle() = "RF-Kit
RF2K-S"`. Three vertical sections inside the applet body:

**Section A - Header row**
* Left: device name + nickname ("RF-Kit RF2K-S" / "KG4VCF") + small
  software-version label ("G200C267")
* Right: status dot (green = connected, yellow = retrying, red = down) +
  OPERATE/STANDBY pill button

**Section B - Gauges + telemetry strip**
* 3 `HGauge` bars: Fwd Power (0..2000 W, yellow 1500, red 1800),
  SWR (1.0..3.0, yellow 2.0, red 2.5), Temp (0..80 C, yellow 60, red 70)
* Telemetry strip: Vmains / Iamp / Efficiency (computed `fwdW / (V * I)`,
  clamped 0..100)

**Section C - Antennas + Tuner**
* Antenna row: 4 buttons for the 4 internal antennas, labels from
  `RfKit_Ant1_Label`..`RfKit_Ant4_Label`. Active antenna highlighted green.
  AVAILABLE in neutral, DISABLED greyed and unclickable. External antennas
  (1..16, BCD-driven) are not shown in the applet (rare; configured via Setup
  if needed).
* Tuner status line: "TUNED 1.2 : 1 @ 14.155 MHz (LC)" or "TUNING..." red
  during AUTO_TUNING, or "NOT TUNED" / "BYPASS".
* Action row: **TUNE** and **BYPASS** buttons, both greyed permanently
  (firmware limitation; tooltip explains).

Right-click context menu (mirrors AmpApplet exactly):
* Open RF-Kit Advanced... -> `navigationRequested("rfKitAdvanced")`
* (separator)
* Disconnect / Reconnect -> `connectionToggleRequested()`
* Copy diagnostics to clipboard -> `diagnosticsCopyRequested()`

### 5.2 Setup &raquo; CAT &amp; Network &raquo; RF-Kit (one tree node, two tabs)

New class `src/gui/setup/RfKitPage.{h,cpp}`. Constructor takes `RadioModel*`,
internal `QTabWidget` with two tabs:

**Tab 1 - General**
* Master toggle checkbox: "Enable RF-Kit Amplifier integration"
  -> writes `RfKit_Enabled` AppSetting + sets `RadioModel::rfKitEnabled`
* Live status section: connection pill, IP:port, software_version,
  current operate_mode, current operational_interface, last poll RTT
* Limitations & feature-request section: paragraph about the TUNE/BYPASS
  firmware gap, action button "Open feature-request email to RF-Power"
  (composes mailto: with pre-filled body)

**Tab 2 - RF2K-S**
* Connection: Host, Port, Auto-reconnect checkbox, Poll interval ms,
  buttons (Test connection, Set amp to TCI mode, Reset amp error state)
* Display: Nickname (read-only, sourced from `/info.custom_device_name`)
* Antenna labels: 4 editable rows (ANT 1..4), persisted to
  `RfKit_Ant1_Label`..`RfKit_Ant4_Label`
* Live diagnostics: uptime, RTT avg/max, polls succeeded/failed, last
  poll age, reconnect count, amp software_version
* Fault history: scrollable list of last 10 FAULT events,
  "Clear fault history" button

Master toggle in Tab 1 controls the enabled state of Tab 2's contents (greys
out when master is OFF, same pattern as PgxlAdvancedPage / TgxlAdvancedPage
under 4O3A master).

### 5.3 SMeterWidget extension

The existing `SMeterWidget` already has PGXL-aware scale switching. Extract
the trigger into a generic "external amp in OPERATE" signal on RadioModel:

```cpp
// RadioModel additions
signals:
    void externalAmpOperateChanged(bool inOperate);      // any external amp
    void externalAmpFwdSwrUpdated(int forwardW, float swr);
```

`RadioModel` aggregates `PgxlConnection::operateModeChanged` and
`Rf2ksConnection::operateModeUpdated`, emits the generic signals.
`SMeterWidget` subscribes to the generic signals instead of PGXL-specific.

No new chrome on the widget. Operator sees the same 2 kW scale they already
see from PGXL.

### 5.4 Menu integration (mirrors PGXL)

`MainWindow` edits in `populateContainersMenu` (the existing Containers >
Applets menu builder):

```cpp
// Register the applet with AppletVisibilityController
m_appletsById["RfKit"] = m_rfKitApplet;
m_appletVis->registerApplet("RfKit", "RF-Kit RF2K-S", /*initialVisible=*/true);
m_appletVis->setAvailable("RfKit", m_radioModel->rfKitEnabled());

// Live-update availability on master toggle
connect(m_radioModel, &RadioModel::rfKitEnabledChanged, this,
        [this](bool enabled){ m_appletVis->setAvailable("RfKit", enabled); });
```

Applet right-click signals are wired in MainWindow:

```cpp
connect(m_rfKitApplet, &Rf2ksApplet::navigationRequested, this,
        [this](const QString& key){ openSetup(key); });
connect(m_rfKitApplet, &Rf2ksApplet::connectionToggleRequested, this,
        [this]{ /* toggle m_rfKitConnection state */ });
connect(m_rfKitApplet, &Rf2ksApplet::diagnosticsCopyRequested, this,
        [this]{ /* build diagnostics string + copy to clipboard */ });
```

No new entries in the menubar. No Tools menu entry. No keyboard shortcut.

---

## 6. Protocol details

### 6.1 REST surface (confirmed via swagger + live probe)

| Verb | Path | Body | Use |
|---|---|---|---|
| GET | `/info` | n/a | Pre-fill nickname, log software_version, sanity-check reachability on Test Connection |
| GET | `/data` | n/a | Cross-check amp's perceived band/frequency against our TCI emit |
| GET | `/power` | n/a | Drive the 3 HGauges + telemetry strip + SMeterWidget |
| GET | `/tuner` | n/a | Drive tuner status line (mode, last-tuned freq, L+C) |
| GET | `/antennas` | n/a | Populate antenna button row at 10 s cadence (rare change) |
| GET | `/antennas/active` | n/a | Highlight the active antenna button |
| PUT | `/antennas/active` | `{"type":"INTERNAL","number":N}` | Switch antenna when operator clicks |
| GET | `/operate-mode` | n/a | Status pill state |
| PUT | `/operate-mode` | `{"operate_mode":"OPERATE"|"STANDBY"}` | Toggle on click |
| GET | `/operational-interface` | n/a | Live status row on General tab |
| PUT | `/operational-interface` | `{"operational_interface":"TCI"}` | "Set amp to TCI mode" button |
| POST | `/error/reset` | n/a | "Reset amp error state" button |

### 6.2 TCI subscription (existing TciServer reused)

The amp connects as a TCI WebSocket client to NereusSDR's TciServer port
(default 50001). Per `tciSupport.py:82-89` (firmware v159, structurally
unchanged through G200C267), the amp parses only:

* `vfo:N,M,F;` - VFO N (we only use N=0), M=0 main / 1 split, F = frequency Hz
* `split_enable:0,true|false;`

Our existing `TciProtocol::buildVfoLine(0, vfo, freq)` and
`buildSplitEnableLine` already emit these. **No code changes to TciServer
required** for RF-Kit. The amp follows our band changes automatically.

The amp does NOT parse `tune:`, `trx:`, `mode:`, `drive:`, or anything else
from us. Sending them is harmless (the amp's regex match silently fails).

### 6.3 UDP and CAT (not used)

UDP and CAT operational-interface modes on the amp expect frequency broadcasts
from a logger (N1MM RadioInfo XML) or a CAT serial passthrough. NereusSDR
delivers frequency via TCI instead. We never emit on UDP 12060 toward the
amp, and we don't open a serial link. If the operator manually switches the
amp to UDP or CAT mode, our `/operational-interface` poll detects the mode
mismatch and the General tab live status shows it; the "Set amp to TCI mode"
button is the recovery.

---

## 7. Data flow / signal wiring

```
1 Hz timer in Rf2ksConnection
   -> pollOnce() issues 6 GETs in parallel
   -> on each reply: parse JSON, update m_lastPower / m_lastTuner / etc
   -> emit powerUpdated(snap)                    -> Rf2ksApplet::setPower(snap)
                                                 -> RadioModel aggregator
                                                    -> emit externalAmpFwdSwrUpdated
                                                    -> SMeterWidget gauge
   -> emit operateModeUpdated("OPERATE"/"STANDBY")
                                                 -> Rf2ksApplet::setOperateMode
                                                 -> RadioModel aggregator
                                                    -> emit externalAmpOperateChanged
                                                    -> SMeterWidget scale switch
   -> emit antennasUpdated(list)                 -> Rf2ksApplet::refreshAntennaRow
   -> emit activeAntennaUpdated(a)               -> Rf2ksApplet::highlightActive

User clicks ANT 2 in applet
   -> Rf2ksApplet emits antennaRequested(INTERNAL, 2)
   -> MainWindow connects to Rf2ksConnection::setActiveAntenna
   -> PUT /antennas/active fires
   -> next poll returns INTERNAL 2 ACTIVE
   -> activeAntennaUpdated fires; button highlight confirms

User clicks OPERATE/STANDBY pill
   -> Rf2ksApplet emits operateToggled(requestedOperate)
   -> Rf2ksConnection::setOperateMode("OPERATE"/"STANDBY")
   -> PUT /operate-mode fires
   -> next poll returns the new mode

Master toggle flipped via Setup
   -> RfKitPage emits rfKitEnabledChanged
   -> RadioModel::setRfKitEnabled(bool)
   -> RadioModel writes RfKit_Enabled AppSetting
   -> if enabled and host configured: Rf2ksConnection::connectToAmp
   -> if disabled: Rf2ksConnection::disconnect, AppletVisibilityController
      setAvailable("RfKit", false)
```

---

## 8. Persistence (AppSettings)

All keys are PascalCase, prefixed `RfKit_`, persisted in NereusSDR's XML
settings file (NOT QSettings). All boolean values are stored as "True" /
"False" strings per the established convention.

| Key | Default | Notes |
|---|---|---|
| `RfKit_Enabled` | `False` | Master toggle. Drives AppletVisibilityController availability id `"RfKit"`. |
| `RfKit_ManualIp` | `""` | Amp IP. Empty until first connect. |
| `RfKit_ManualPort` | `8080` | Confirmed via OPTIONS Allow probe on JJ's live amp. |
| `RfKit_AutoReconnect` | `True` | Exponential backoff on socket / 5xx. |
| `RfKit_PollIntervalMs` | `1000` | 1 Hz default; settable 250..5000. |
| `RfKit_Nickname` | `""` | Pre-populated from `/info.custom_device_name` on first connect. |
| `RfKit_Ant1_Label` | `""` | Operator-set; REST has no name field. |
| `RfKit_Ant2_Label` | `""` | |
| `RfKit_Ant3_Label` | `""` | |
| `RfKit_Ant4_Label` | `""` | |
| `RfKit_FaultHistory` | `[]` | JSON array of objects: `{ts, kind, fwd, swr, temp, band, antenna}` |

**TX interlock for the greyed buttons** inherits the existing PGXL keys:
`PGXL_TxInterlockMode` (Disabled / Warn / Block) and `PGXL_TxInterlockGraceMs`.
The key names are historical misnomers (interlock applies amp-agnostically);
a future rename to `Amp_TxInterlock*` can ride a cleanup epic.

---

## 9. Error handling

| Failure | Detection | UX |
|---|---|---|
| Wrong host/port at config time | First GET /info times out or 5xx | Toast on Test Connection: "Could not reach amp. Verify host/port and amp is online." Save still allowed. |
| Network drop mid-session | 3 consecutive poll failures | Status dot yellow, then red. Applet gauges grey out (held values dimmed). Exponential reconnect 1s..60s. |
| Amp reports `state=FAULT` in /power or /tuner | JSON status field non-empty | Status pill flashes red. Fault appended to `RfKit_FaultHistory` with timestamp + snapshot. Reset error button on RF2K-S tab posts to `/error/reset`. |
| Amp's `operational_interface` is not TCI | GET /operational-interface returns non-TCI | General tab live status shows the actual mode. "Set amp to TCI mode" button is the recovery. Not auto-fixed (operator may have intentionally set UNIV for RF-sensing). |
| Master toggle flipped OFF mid-session | RadioModel::rfKitEnabled = false | Rf2ksConnection::disconnect, clean TCP close. Applet hides via AppletVisibilityController. Settings preserved. |
| High SWR observed during TX | SWR gauge > 2.5 | Visual only; amp self-protects (fold-back). NereusSDR does not auto-bypass. |
| TUNE/BYPASS clicked | Buttons greyed | No click handler. Tooltip explains firmware limitation. |

---

## 10. Testing

New test executables (mirror `tst_pgxl_connection_*` pattern):

| Test | Coverage |
|---|---|
| `tst_rfkit_connection_parse` | JSON parsing for /power, /tuner, /antennas, /info, /data into snapshot structs. Round-trips fixture JSON captured from JJ's live amp. |
| `tst_rfkit_connection_poll` | 1 Hz timer fires, parallel GETs issued, snapshot signals emitted, RTT measured. |
| `tst_rfkit_connection_reconnect` | Simulated network drop (testForceDisconnect) triggers backoff schedule 1..60s. Recovery resets backoff. |
| `tst_rfkit_connection_control` | Verifies PUT frame format for setActiveAntenna, setOperateMode, setOperationalInterface, resetError. |
| `tst_rfkit_applet_layout` | Gauge binding, MOX gating disables tune/bypass buttons (greyed always; MOX gating belt-and-suspenders for if firmware ever supports them), button states. |
| `tst_rfkit_applet_context_menu` | Right-click items + emitted signals (navigationRequested, connectionToggleRequested, diagnosticsCopyRequested). |
| `tst_rfkit_page_master_gate` | Master toggle flip greys RF2K-S tab, hides applet via AppletVisibilityController, restores on flip-back. |
| `tst_smeter_widget_external_amp` | Generalized scale flip works for both PGXL and RF-Kit triggers. PGXL test (already passing) continues to pass; new test exercises RF-Kit driving the same signal path. |

Verification matrix lives at
`docs/architecture/phase-rfkit-verification/README.md` (new directory),
13-row bench matrix covering:

1. First-time setup (Setup -> RF-Kit -> enable -> host/port -> Test -> Set TCI -> Save)
2. Master toggle off/on (applet hide/show, menu grey/un-grey)
3. Antenna switch (click ANT 2, verify amp's active antenna changes)
4. OPERATE/STANDBY toggle
5. Set amp to TCI mode (PUT /operational-interface)
6. Band tracking via TCI (change band on NereusSDR, verify amp follows)
7. PTT band-edge / SMeterWidget 2 kW scale on OPERATE
8. Gauge accuracy (compare applet readings against amp's touchscreen)
9. Tuner status line under TUNE-initiated-on-front-panel
10. Network drop + reconnect
11. FAULT event capture (force high SWR, verify fault history populated)
12. Greyed TUNE/BYPASS buttons (tooltip displays, no action)
13. Antenna label round-trip (set, restart NereusSDR, labels persist)

---

## 11. Out of scope

* **TUNE / BYPASS network triggers.** Firmware does not expose, applet
  buttons stay greyed. Add the wire when firmware supports it.
* **External antennas (BCD-driven 1..16).** Configurable via amp's
  touchscreen; applet only shows the 4 internal antennas. Could add a Setup
  page section if anyone asks.
* **CAT or UDP operational-interface modes.** TCI is the path; UDP/CAT
  modes are detected and surfaced on the General tab but not driven from
  NereusSDR.
* **Multi-amp simultaneous operation (PGXL + RF2K-S at the same time).**
  SMeterWidget aggregator picks the first amp in OPERATE; if both are in
  OPERATE at once (rare), behavior is undefined. Cross when relevant.
* **Forking RF-Kit firmware.** Out of scope for this PR. See section 12.
* **SSH-into-amp for diagnostics.** Tempting but invasive. Out of scope.
* **mDNS / Bonjour amp discovery.** Manual host/port config only for v1.

---

## 12. Open follow-ups

1. **File a feature request with RF-Power** for `PUT /tuner` accepting
   `{"mode":"AUTO_TUNING"}` and `{"mode":"BYPASS"}`. The `Tuner.mode` enum
   already includes these as valid string values. The request is a small
   ask: add a write verb to an existing read endpoint. Email body draft
   lives in section 13.

2. **If RF-Power adds the verb**: un-grey the TUNE/BYPASS buttons in
   `Rf2ksApplet`, wire to new `Rf2ksConnection::triggerTune()` /
   `setTunerBypass(bool)` slots. About 30 lines of code. The greyed-button
   placeholder approach makes this a near-zero-touch follow-up.

3. **Private reference fork** of the amp's Python source pulled from JJ's
   live amp via SSH (port 22 is open). Useful for confirming the route
   table at firmware G200C267 hasn't grown. Not distributed publicly.
   Triggered only if RF-Power doesn't respond to (1) within ~2 months and
   we want to prototype the fix to attach to a stronger feature request.

4. **Multi-amp aggregation for SMeterWidget** when both PGXL and RF-Kit are
   online simultaneously. Add a "Source" right-click submenu on the meter.
   Defer until anyone reports owning both.

5. **External antenna (BCD) row** in the applet if any operator asks. Trivial
   add once we have a use case.

6. **`PGXL_TxInterlock*` rename to `Amp_TxInterlock*`** for accuracy.
   Touches both the just-shipped PGXL code and the new RF-Kit code; ride
   a cleanup epic, not this one.

---

## 13. Master-plan placement and feature-request draft

### Master-plan placement

This epic slots into the master plan as **Phase 3P-III: RF-Kit RF2K-S
integration**, after the just-shipped 3P-II (4O3A PGXL/TGXL) and before
the future 3F multi-panadapter work. CLAUDE.md will gain a row in the
"Phase" table once the implementation plan exists.

### Feature-request email draft to RF-Power

To: `info@rf-power.eu` (or via the rf-power.eu contact form)
Subject: RF2K-S REST API enhancement request: write verb for `/tuner`

```
Dear RF-Power team,

I am the maintainer of NereusSDR, an open-source cross-platform SDR
console for the OpenHPSDR family of radios. I have built integration for
the RF2K-S amplifier against your published OpenAPI 3.0 specification
(version 0.9.0). The integration works well for read-only telemetry plus
the antenna switch and operate-mode writes, and TCI band tracking lights
up the operator's UI within milliseconds of a frequency change.

There is one operator-facing gap: NereusSDR cannot initiate a tuner
autotune or a bypass toggle over the network. The /tuner endpoint is
GET-only, and there are no /tune, /autotune, or /tuner/tune routes. As a
result, my Rf2ksApplet has TUNE and BYPASS buttons that are visually
present but disabled with tooltip "Press TUNE / BYPASS on the amp's
front panel."

I would like to request that a future firmware release add a write verb
on the existing /tuner endpoint, accepting any of the existing
Tuner.mode enum values:

  PUT /tuner body {"mode": "AUTO_TUNING"}     starts an autotune cycle
  PUT /tuner body {"mode": "BYPASS"}          puts tuner in bypass
  PUT /tuner body {"mode": "AUTO"}            returns to AUTO

The enum values already exist in your published swagger.json. Adding the
write route would unlock remote tuner control for NereusSDR and any
other client built against your API. I would be glad to help test
candidate firmware.

Thank you for the excellent RF2K-S design and the well-documented REST
surface.

73,
J.J. Boyd (KG4VCF)
NereusSDR maintainer
https://github.com/boydsoftprez/NereusSDR
```

When the operator clicks the "Open feature-request email to RF-Power" button
on the General tab, NereusSDR builds a `mailto:` link with the body above
URL-encoded.

---

End of design.
