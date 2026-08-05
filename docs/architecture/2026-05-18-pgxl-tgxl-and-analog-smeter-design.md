# PGXL / TGXL Network Integration + Analog S-Meter Forward Port

Status: Design, pending implementation plan.
Author: J.J. Boyd (KG4VCF), with AI-assisted drafting via Claude Code.
Date: 2026-05-18.

---

## 1. Goal

Wire NereusSDR up to two FlexRadio / 4O3A Ethernet accessories that operators
already have at their stations:

1. **Power Genius XL** (PGXL): legal-limit HF/6 m linear amplifier with forward
   power, SWR, drain current, temperature, mains voltage telemetry over LAN.
2. **Tuner Genius XL** (TGXL): network-controlled antenna tuner with C1/L/C2
   relay positions, OPERATE/BYPASS/STANDBY mode, optional 3x1 antenna switch,
   and tune-cycle initiation.

Plus a related UI parity item the user pulled into the same epic:

3. **Forward-port AetherSDR's analog S-Meter widget**, including its peak-hold
   settings parity and PGXL-aware scale switching, so the existing composite
   S-Meter in `AppletPanelWidget`'s fixed header is replaced by the AetherSDR
   needle gauge that auto-scales to 2 kW when the amplifier is detected and
   operating.

Operator-visible success criteria:

* Amplifier forward power, SWR, temperature, and OPERATE/STANDBY state are
  visible on the right-column applet stack any time the PGXL is online.
* **Right-clicking the AmpApplet or TunerApplet opens that device's
  Setup &rarr; Network &rarr; \<Device\> Advanced page** (the second
  NereusSDR-native UX divergence, in addition to the S-Meter right-click
  pattern in section 5.4.2).
* Tuner relay positions are visible and click-tunable. Antenna 1/2/3 buttons
  appear when a TGXL 3x1 is online. The TUNE button starts a tune cycle and
  the final settled SWR is captured into the button label. **Per-antenna
  labels** are operator-editable ("80 m dipole", "vertical"). **Frequency-keyed
  tune memory** stores relay positions per band per antenna so changing bands
  recalls the last known good tune without a fresh cycle.
* **PGXL is paired with NereusSDR as its exciter at connect time** via
  `amplifier create` + an attempt at `flexradio` pairing (graceful fallback
  if PGXL rejects an ANAN serial). The pairing path is operator-selectable
  in Setup &rarr; PGXL Advanced.
* **Connection is robust**: `keepalive enable` is sent at connect; `ping` is
  issued at a configurable cadence; missed pings or socket drops trigger
  exponential-backoff auto-reconnect, all silent to the operator unless
  retries exhaust.
* **Setup &rarr; Network &rarr; PGXL Advanced** lets the operator read +
  edit nickname / bias mode / fan mode / LED intensity / TX power cap /
  pairing path / network config (DHCP, IP, netmask, gateway), see live
  connection diagnostics (uptime, RTT, keepalive misses, frame counts),
  and review a persistent **fault history** (last 10 `state=FAULT` events
  with FWD/SWR/Temp at fault and likely-cause hint). A
  **Save &amp; Reboot** action sends the `save` command after operator
  confirmation. **Setup &rarr; Network &rarr; TGXL Advanced** mirrors the
  structure for the tuner (nickname, antenna labels, tune-memory
  management, network config, diagnostics, faults).
* **Optional TX interlock** under Setup &rarr; Transmit: three modes
  (Disabled / Warn / Block) with a configurable grace period. Disabled by
  default to match AetherSDR's behavior; operator opt-in.
* The fixed-header S-Meter behaves like AetherSDR's needle gauge with two
  NereusSDR-native additions: a shallow-arc needle with S0..S9+60 scale,
  configurable peak hold + decay, TX mode (Power / SWR / Level / Compression),
  RX mode (Signal / **Sig Avg** / Signal Peak / **Max Bin**, the last two ported
  from Thetis: **Sig Avg** uses WDSP's `GetRXAMeter(RXA_S_AV)` for the averaged
  S-meter reading, and **Max Bin** uses WDSP's `SetupDetectMaxBin` /
  `GetDetectMaxBin` for the strongest-bin-in-passband reading), and the
  power scale snaps to 2 kW (red zone at 1.5 kW) the
  moment PGXL is in OPERATE. **All S-Meter settings are reached via
  right-click context menu on the widget itself**, not an always-visible
  settings strip; this is a deliberate divergence from AetherSDR's UX.
* `Setup -> Network -> Peripherals` has a single page with rows for both
  devices: enable, host, port, scan LAN, connect/disconnect, status. Scan LAN
  is UDP-native per the published 4O3A protocol; no TCP probe sweep.

Everything ships in **one combined PR** per the brainstorm decision.

---

## 2. Source provenance and licensing

This epic is structured as **AetherSDR 1:1 baseline + NereusSDR-native
additions** for the PGXL/TGXL networking and the analog S-Meter widget,
plus Thetis for two S-Meter RX modes. The AetherSDR command set
(`info`, `status`, `tune`, `activate ant`, `operate=`, `bypass=`) is the
verbatim baseline; the additional FlexRadio API verbs that AetherSDR does
not call (`amplifier create`, `flexradio`, `keepalive enable`, `ping`,
`setup read` / write, `ifconf read` / write, `interlock`, `save`) are
ported from the published FlexRadio wiki and added because NereusSDR is
the **exciter** in this topology, not a peer client like AetherSDR.
Protocol details
**Thetis is an additional upstream** for two SMeterWidget RX-mode options
added in section 5.4.3:

* **Sig Avg**: averaged S-meter reading via WDSP `GetRXAMeter(channel,
  RXA_S_AV)`, paired with the standard `RXA_S_PK` path that AetherSDR
  already uses for `Signal`. Thetis selector lives in
  `console.cs:957`.
* **Max Bin**: strongest-FFT-bin reading via WDSP `SetupDetectMaxBin` /
  `GetDetectMaxBin`. Thetis selector at `console.cs:954` (alongside
  Signal/Sig Avg), full DSP in `wdsp/analyzer.c:688..830`.

Both are Thetis-native with no AetherSDR equivalent. Thetis has no
PGXL/TGXL implementation; AetherSDR is the sole upstream for those.

### Upstream files to port

| File | LOC (AetherSDR) | NereusSDR target |
|---|---|---|
| `src/core/PgxlConnection.{h,cpp}` | 55 + 149 | `src/core/PgxlConnection.{h,cpp}` |
| `src/core/TgxlConnection.{h,cpp}` | 68 + 177 | `src/core/TgxlConnection.{h,cpp}` |
| `src/models/TunerModel.{h,cpp}` | 93 + 233 | `src/models/TunerModel.{h,cpp}` |
| `src/gui/AmpApplet.{h,cpp}` | 41 + 143 | `src/gui/applets/AmpApplet.{h,cpp}` |
| `src/gui/TunerApplet.{h,cpp}` | 86 + 352 | rewire existing `src/gui/applets/TunerApplet.{h,cpp}` |
| `src/gui/SMeterWidget.{h,cpp}` | 134 + 701 | `src/gui/SMeterWidget.{h,cpp}` |
| `src/gui/AppletPanel.cpp` (lines 237..328 only, the S-Meter header controls) | 92 | wire into `src/gui/applets/AppletPanelWidget.{h,cpp}` |
| `RelayBar` inner class from `src/gui/TunerApplet.cpp` | ~80 | `src/gui/RelayBar.{h,cpp}` |

Approximate total: **2,373 LOC** of upstream code, of which 326 LOC
(`TunerModel`) and 184 LOC (`AmpApplet`) and 835 LOC (`SMeterWidget`) are
entirely new files in NereusSDR; the rest rewires existing skeletons.

### Thetis upstream for Max Bin meter (section 5.4.3)

Verified against Thetis `v2.10.3.13-7-g501e3f51`, abbreviated `[@501e3f5]`
in inline cites:

| Thetis file | Lines | What we port |
|---|---|---|
| `Project Files/Source/wdsp/analyzer.c` | 688..830 | `Init_DetectMaxBin`, `SetupDetectMaxBin`, `DetectMaxBin`, `GetDetectMaxBin` (Max Bin DSP). |
| `Project Files/Source/Console/dsp.cs` | 387..388, 846..850 | P/Invoke declarations: `GetRXAMeter(channel, rxaMeterType)` (used for both `RXA_S_PK` aka Signal and `RXA_S_AV` aka Sig Avg) plus `SetupDetectMaxBin` / `GetDetectMaxBin` for Max Bin. Ported into `WdspEngine` as C++ wrappers. |
| `Project Files/Source/Console/MeterManager.cs` | 1912, 21469, 21507..21550 | `clsItemGroup.MaxBin` property, `MeterType.SIGNAL_MAX_BIN`, `AddSMeterBarMaxBin`. We port the constants and the semantics, not the meter-group UI (NereusSDR uses its own meter system). |
| `Project Files/Source/Console/console.cs` | 954, 957 | `GetRXAMeter(channel, RXA_S_PK)` for Signal and `GetRXAMeter(channel, RXA_S_AV)` for Sig Avg. The two are otherwise identical paths through the existing meter pump. |
| `Project Files/Source/Console/ucSignalSelect.cs` | 93, 121, 130 | `radSigMaxBin.Checked` UI selector. We translate to a QAction in the right-click menu. |

No mi0bot-Thetis variant needed (not HL2-specific).

### Protocol reference

For PGXL specifically the C/R/S/V frame format and ports are also documented
on FlexRadio's official wiki:

* [smartsdr-api-docs / PowerGenius Ethernet API](https://github.com/flexradio/smartsdr-api-docs/wiki/PowerGenius-Ethernet-API)
* PDF copy at [`https://edge.flexradio.com/.../PGXL-Amplifier-to-Radio-API-Documentation.pdf`](https://edge.flexradio.com/www/offload/20240326085158/PGXL-Amplifier-to-Radio-API-Documentation.pdf)

4O3A officially defers protocol questions to that wiki (per the 4O3A forum
thread "Power Genius XL API commands available?" where Dragisa from 4O3A
points developers to it). TGXL has no separate wiki page but uses the same
C/R/S/V family on port 9010 per AetherSDR's reverse-engineered driver, and
the same UDP discovery pattern by analogy.

### Attribution rules

Every ported file gets:

1. The AetherSDR header preserved byte-for-byte from upstream
   (`Copyright (C) 2024-2026 Jeremy (KK7GWY) / AetherSDR contributors` plus
   the GPLv3 permission block, per `docs/attribution/HOW-TO-PORT.md`
   templates and the existing `aethersdr-contributor-index.md` entries).
2. A `Modification history (NereusSDR)` block with the port date and AI
   tooling disclosure.
3. The `docs/attribution/aethersdr-contributor-index.md` entries at
   lines 229 and 247 (currently "(none yet)") get updated to point at the
   new NereusSDR paths.
4. Inline upstream cites use the existing `// From AetherSDR <path>:N
   [@<shortsha>]` grammar per `feedback_inline_cite_versioning`.

No Thetis attribution applies (no Thetis source involved).

---

## 3. Architecture overview

```
+------------------+         +----------------------+
| Radio (ANAN/HL2) |         | PGXL on LAN          |
| OpenHPSDR P1/P2  |         | TCP 9008 + UDP 9008  |
+--------+---------+         +-----------+----------+
         |                               |
         | OpenHPSDR                     | C/R/S/V text
         | UDP                           | TCP
         v                               v
+--------+----------------------+   +----+------------+
| RadioConnection (worker)      |   | PgxlConnection  |
+-------------------------------+   | (main thread)   |
         |                          +--------+--------+
         |  signals (queued)                 |
         v                                   | statusUpdated()
+--------+----------------------+            v
| RadioModel (main thread)      |   +--------+--------+
|                               |   | AmpApplet       |
|  m_pgxlConnection ------------+-->| (right column)  |
|  m_tgxlConnection ----+       |   +-----------------+
|  m_tunerModel <-------+       |
|                       |       |
|  m_lanDiscovery       |       |
|                       v       |   +-----------------+
|                  +----+----+  |   | TunerApplet     |
|                  |TgxlConn |--+-->| (right column)  |
|                  +----+----+  |   +--------+--------+
|                       |       |            ^
|                       +-------+------------+
|                                            |
|  hasAmplifier() -------> SMeterWidget <----+
|  ampOperate() --------->                   |
|  txMetersChanged ----->                    |
|  ampMetersChanged ---->                    |
+--------------------------------------------+
```

Six new (or rewired) main-thread objects, no thread crossings inside the
new code. The radio connection already lives on a worker thread; all signals
into RadioModel auto-queue to main exactly like the existing P1/P2 path.

---

## 4. Network classes

### 4.1 `PgxlConnection`

`src/core/PgxlConnection.{h,cpp}`. Port verbatim from AetherSDR.

**Threading.** Main thread. `QTcpSocket` is non-blocking by nature; readyRead
is delivered to the main event loop.

**Public API.**

```cpp
class PgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit PgxlConnection(QObject* parent = nullptr);

    bool    isConnected() const;
    QString version()     const;
    QString peerAddress() const;
    quint16 peerPort()    const;

public slots:
    void connectToPgxl(const QString& host, quint16 port = 9008);
    void disconnect();
    quint32 sendCommand(const QString& cmd);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);  // new vs AetherSDR
    void statusUpdated(const QMap<QString, QString>& kvs);
};
```

The `connectionFailed` signal is added to match the equivalent on
`TgxlConnection` so that Peripherals row error UI behaves identically for
both devices. AetherSDR did not expose this; the Setup row picks the error
message off the socket error. Adding the signal is a tiny divergence that
does not change behavior of any existing AetherSDR consumer; it is purely a
new emit on the same `onError()` path.

**State.**

```cpp
QTcpSocket m_socket;
QTimer     m_pollTimer;        // 200 ms (5 Hz)
QByteArray m_readBuf;
quint32    m_seq{0};
bool       m_connected{false};
bool       m_gotVersion{false};
QString    m_version;
```

**Poll cadence.** `m_pollTimer.setInterval(200)` -> 5 Hz `status` poll.
This is intentional: PGXL FWD power and SWR drive the S-Meter needle, which
needs responsive metering. TGXL polls at 1 Hz because relay positions and
state changes happen on human timescales.

**Read-loop logic.** Newline-delimited frame extraction from `m_readBuf`,
then per-line dispatch into `processLine`. See section 6 for the frame
grammar.

#### 4.1.1 Extended PGXL commands (NereusSDR-native)

Beyond the AetherSDR baseline, PgxlConnection exposes additional slots
for the NereusSDR-specific exciter-pairing and lifecycle responsibilities.
Each maps to a verb from the FlexRadio PowerGenius Ethernet API wiki.

```cpp
public slots:
    // Lifecycle: called automatically on connect
    quint32 amplifierCreate(const QString& ourSerial,
                            const QString& ourModel = "NereusSDR",
                            const QString& antMap = "ANT1:PORTA,ANT2:PORTB");

    // Pairing: attempt FLEX-style pairing; falls back gracefully on reject
    quint32 flexradioPair(QChar ampSlice,           // 'A' or 'B'
                          const QString& radioSerial,
                          const QString& txAnt,     // "ANT1" or "ANT2"
                          bool pttOverLan = true,
                          bool active = true);

    // Liveness
    quint32 enableKeepalive();                       // sent once on connect
    quint32 ping(const QString& tag = "");           // returns seq; correlate via pongReceived

    // Antenna validity gating (operator-toggled)
    quint32 interlockCreate(const QString& validAntennas,
                            const QString& name,
                            const QString& serial);
    quint32 interlockDisable(int interlockId);

    // Configuration read / write
    quint32 readSetup();                             // -> setupResponse(QMap)
    quint32 writeSetup(const QMap<QString,QString>& fields);  // nickname, fan, led, bias
    quint32 readIfconf();                            // -> ifconfResponse(QMap)
    quint32 writeIfconf(const QString& ip,
                        const QString& netmask,
                        const QString& gateway,
                        bool dhcp);

    // Persist and reboot (gated by operator confirmation modal)
    quint32 save();

    // Band notify: send the active slice's band to the amp
    quint32 setBand(int bandHz);                     // sends band via paired path

signals:
    void pongReceived(quint32 seq, qint64 rttMs, const QString& tag);
    void pingTimedOut(quint32 seq);
    void faultCaptured(const FaultEvent& ev);
    void setupResponse(const QMap<QString,QString>& fields);
    void ifconfResponse(const QMap<QString,QString>& fields);
    void pairingResult(bool succeeded, const QString& detail);
    void saveAcknowledged();                         // before the reboot drops the connection
    void reconnectAttempt(int attemptNumber, int backoffMs);
```

**New internal state** (added to the AetherSDR-baseline members):

```cpp
QTimer  m_keepaliveTimer;       // default 30 s (PGXL_KeepaliveSec)
QTimer  m_pingTimer;            // default 60 s (PGXL_PingSec); off when 0
QTimer  m_reconnectTimer;       // single-shot with backoff
int     m_reconnectAttempts{0};
int     m_keepaliveMissed{0};
struct PendingPing { quint32 seq; qint64 sentMs; QString tag; };
QHash<quint32, PendingPing> m_pendingPings;
qint64  m_lastFrameMs{0};       // for diagnostics
qint64  m_connectedSinceMs{0};
quint64 m_framesIn{0}, m_framesOut{0}, m_bytesIn{0}, m_bytesOut{0};
```

**Auto-reconnect schedule.** Backoff sequence by attempt number: 1 s,
2 s, 5 s, 10 s, 30 s, then 60 s indefinitely. AppSettings
`PGXL_AutoReconnect` (default `True`) gates the entire reconnect timer;
operator-initiated Disconnect cancels any in-flight backoff.

**Pairing flow.** On TCP connect, after the version banner:

1. Send `amplifierCreate` with the operator-configured `PGXL_FlexAmpSlice`
   / `PGXL_TxAnt` and a NereusSDR-specific serial format
   (`NereusSDR-<radio_mac>`).
2. If `PGXL_PairAttempt == True` (default), send
   `flexradioPair(slice, radioSerial, txAnt, ptt=true, active=true)`.
3. Wait up to 2 s for the response. If `R<seq>|0|...` (success), emit
   `pairingResult(true, ...)`. If non-zero hex (failure) or timeout,
   emit `pairingResult(false, errorString)` and log the rejection. The
   operator's Setup row Status label reflects the outcome:
   "Connected; LAN PTT active" vs "Connected; PTT via BCD/CAT only".
4. Send `enableKeepalive`.
5. Send `readSetup` and `readIfconf` to populate the Advanced page on
   first connect (lazy: only if the page has ever been opened).
6. Start polling.

### 4.2 `TgxlConnection`

`src/core/TgxlConnection.{h,cpp}`. Same shape as `PgxlConnection`, with
additional convenience methods:

```cpp
public slots:
    void connectToTgxl(const QString& host, quint16 port = 9010);
    void disconnect();
    void adjustRelay(int relay, int direction);  // relay 0..2, direction -1|+1
    quint32 sendCommand(const QString& cmd);

signals:
    void stateUpdated(const QMap<QString, QString>& kvs);   // from "S0|state ..."
    void statusUpdated(const QMap<QString, QString>& kvs);  // from "S<seq>|status ..."
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
```

`adjustRelay(int relay, int dir)` issues `tune relay=N move=+/-1` to step a
single relay by one tick. Used by the mousewheel handler on the relay bars
to support manual tune-up without the TGXL auto-tune cycle.

Poll cadence: 1 Hz (`m_pollTimer.setInterval(1000)`).

#### 4.2.1 Extended TGXL commands (NereusSDR-native)

TgxlConnection gets a parallel set of slots: `enableKeepalive()`,
`ping()`, `readSetup()`, `writeSetup()`, `readIfconf()`, `writeIfconf()`,
`save()`. **`amplifierCreate` and `flexradioPair` are not part of the
TGXL API** (TGXL is a tuner, not an amplifier; band/PTT pairing happens
differently). Band notification on TGXL is implicit through the
`operate=` / `bypass=` commands and the auto-tune trigger; no separate
`setBand` slot is needed.

Same internal state additions as PGXL: keepalive timer, ping timer,
reconnect timer with the same backoff schedule, frame/byte counters,
pending pings map, last-frame timestamp.

**Bench-verification caveat.** TGXL has no FlexRadio wiki page; its
response to `keepalive`, `ping`, `setup read`, `ifconf read`, and `save`
is inferred from the shared C/R/S/V protocol family but not documented.
Implementation will probe each verb on a real TGXL during bench testing
and the spec flags any rejection as a follow-up issue rather than a
shipping blocker. Worst case: the verb gets an `R<seq>|<non-zero>|<msg>`
response and that surface is read-only or disabled in the UI.

### 4.3 `TunerModel`

`src/models/TunerModel.{h,cpp}`. New file. Mediates between `TgxlConnection`
and the rest of NereusSDR. Equivalent to AetherSDR's `TunerModel`.

Public Q_PROPERTYs (selection of the important ones):

```cpp
Q_PROPERTY(int  relayC1            READ relayC1            NOTIFY relayChanged)
Q_PROPERTY(int  relayL             READ relayL             NOTIFY relayChanged)
Q_PROPERTY(int  relayC2            READ relayC2            NOTIFY relayChanged)
Q_PROPERTY(bool isOperate          READ isOperate          NOTIFY stateChanged)
Q_PROPERTY(bool isBypass           READ isBypass           NOTIFY stateChanged)
Q_PROPERTY(bool isTuning           READ isTuning           NOTIFY tuningChanged)
Q_PROPERTY(int  antennaA           READ antennaA           NOTIFY antennaAChanged)
Q_PROPERTY(bool hasAntennaSwitch   READ hasAntennaSwitch   NOTIFY stateChanged)
Q_PROPERTY(bool isPresent          READ isPresent          NOTIFY presenceChanged)
Q_PROPERTY(bool hasDirectConnection READ hasDirectConnection NOTIFY directConnectionChanged)
Q_PROPERTY(QString tgxlIp          READ tgxlIp             NOTIFY stateChanged)
Q_PROPERTY(float fwdPower          READ fwdPower           NOTIFY metersChanged)
Q_PROPERTY(float swr               READ swr                NOTIFY metersChanged)
```

Public slots called from `TunerApplet`:

```cpp
void autoTune();                       // sends "tune start"
void adjustRelay(int relay, int dir);  // forwards to TgxlConnection::adjustRelay
void setAntennaA(int antA);            // sends "activate ant=N" (1..3)
void setOperate(bool on);              // sends "operate=1|0"
void setBypass(bool on);               // sends "bypass=1|0"
```

`applyStatus(const QMap<QString,QString>&)` accepts the kv map emitted by
`TgxlConnection::stateUpdated` and `TgxlConnection::statusUpdated` and
updates the Q_PROPERTYs (relayC1, relayL, relayC2, operate, bypass, tuning,
antA, one_by_three, model, serial_num, ip, fwd, swr) atomically.

### 4.4 `RelayBar` widget

`src/gui/RelayBar.{h,cpp}`. Custom horizontal bar for the C1/L/C2 relay
positions. Differs from a plain `QProgressBar` because of:

* 0..255 range (TGXL relay values are byte-wide).
* Mousewheel handler that emits `relayAdjusted(int direction)` (`+1` for
  wheel up, `-1` for wheel down) when scrolling is enabled.
* Scrolling enabled only when the direct TGXL TCP connection is active
  (`TunerModel::hasDirectConnection()`); during radio-only mode the bars
  are read-only.
* Visual style matches the existing NereusSDR widget palette
  (`StyleConstants.h` `kPanelBg`, `kBorderSubtle`, `kAccent`).

### 4.5 `LanDiscovery`

`src/core/LanDiscovery.{h,cpp}`. New, no AetherSDR equivalent (AetherSDR
relies on the FlexRadio's TCP `amplifier <handle> ip=...` push to learn IPs,
which OpenHPSDR radios do not emit). Implements 4O3A-native UDP discovery
per the published FlexRadio wiki spec.

**Mechanism.**

* Open two `QUdpSocket`s bound to `QHostAddress::AnyIPv4` on ports 9008 and
  9010 in `ShareAddress | ReuseAddressHint` mode. PGXL and TGXL announce on
  the local subnet's directed broadcast address (typically
  `192.168.x.255`) periodically.
* Each incoming datagram is decoded as UTF-8 text and matched against the
  official announcement regex from the FlexRadio wiki:

  ```
  ^(?<model>\S+)\s+ip=(?<ip>\d+\.\d+\.\d+\.\d+)\s+v=(?<v>\S+)\s+serial=(?<serial>\S+)\s+nickname=(?<nick>\S+)$
  ```

* Matches are deduplicated by `serial` and emitted as
  `deviceDiscovered(QString model, QString ip, quint16 port, QString
  version, QString serial, QString nickname)`.
* Listening window: bounded by `start(int timeoutMs)`. Default 3000 ms when
  triggered from the `Scan LAN` button.

**Threading.** Main thread. `QUdpSocket::readyRead` integrates with the
event loop the same way `QTcpSocket` does.

**Why not a TCP probe sweep.** The original brainstorm proposed a TCP probe
sweep of the local /24 as a protocol-agnostic fallback. The official wiki
makes that unnecessary: PGXL (and by analogy TGXL) actively broadcast their
presence on the same port number they accept TCP commands on. The UDP
listener is faster, quieter, and uses the documented protocol. The TCP
probe sweep idea is dropped from the design.

### 4.6 ConnectionDiagnostics

`src/core/ConnectionDiagnostics.{h,cpp}`. Lightweight QObject per device
that the Setup &rarr; \<Device\> Advanced page binds to. Aggregates the
runtime metrics tracked by PgxlConnection / TgxlConnection.

```cpp
class ConnectionDiagnostics : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64  uptimeMs        READ uptimeMs        NOTIFY changed)
    Q_PROPERTY(qint64  lastRttMs       READ lastRttMs       NOTIFY changed)
    Q_PROPERTY(int     keepaliveMissed READ keepaliveMissed NOTIFY changed)
    Q_PROPERTY(int     reconnectCount  READ reconnectCount  NOTIFY changed)
    Q_PROPERTY(quint64 framesIn        READ framesIn        NOTIFY changed)
    Q_PROPERTY(quint64 framesOut       READ framesOut       NOTIFY changed)
    Q_PROPERTY(quint64 bytesIn         READ bytesIn         NOTIFY changed)
    Q_PROPERTY(quint64 bytesOut        READ bytesOut        NOTIFY changed)
    Q_PROPERTY(qint64  lastFrameMs     READ lastFrameMs     NOTIFY changed)
    Q_PROPERTY(int     faultsSession   READ faultsSession   NOTIFY changed)
public:
    void bindTo(PgxlConnection* c);
    void bindTo(TgxlConnection* c);
};
```

Update cadence: `changed` is coalesced at 1 Hz via QTimer so the
diagnostics grid doesn't repaint at every telemetry frame.

### 4.7 FaultLog

`src/core/FaultLog.{h,cpp}`. Ring buffer of last 10 fault events per
device, persisted to AppSettings as a JSON array.

```cpp
struct FaultEvent {
    qint64  whenMs;            // epoch ms
    QString state;             // "FAULT" or any state starting with "FAULT_"
    float   fwdAtFaultW;       // forward power at fault transition
    float   swrAtFault;
    float   tempAtFaultC;
    QString likelyCause;       // heuristic: "SWR trip", "Overtemp", "Drive too high", "Unknown"
};

class FaultLog : public QObject {
    Q_OBJECT
public:
    explicit FaultLog(const QString& deviceKey, QObject* parent = nullptr);
    QVector<FaultEvent> events() const;        // newest first
    void capture(const FaultEvent& ev);        // append + evict oldest beyond 10
    void clear();
signals:
    void changed();
};
```

Heuristic for `likelyCause` at capture time:

```
if (swrAtFault > 2.5)         -> "SWR trip"
else if (tempAtFaultC > 85)   -> "Overtemp"
else if (fwdAtFaultW > 1900)  -> "Drive too high"
else                          -> "Unknown"
```

Captured by `RadioModel`'s PGXL status handler whenever the `state` key
in the response transitions to a value beginning with `FAULT`. TGXL has
no documented FAULT taxonomy; bench testing will determine whether to
capture non-OPERATE / non-BYPASS / non-STANDBY states as faults for the
tuner.

AppSettings: `PGXL_FaultHistory`, `TGXL_FaultHistory` (JSON arrays).

### 4.8 TuneMemoryStore

`src/core/TuneMemoryStore.{h,cpp}`. Frequency-keyed (band-keyed) per-antenna
cache of TGXL relay positions, plus an optional auto-recall-on-band-change
flow.

```cpp
struct TuneMemory {
    int    antenna;          // 1..3
    Band   band;             // enum from src/models/Band.h
    int    c1, l, c2;        // relay byte values 0..255
    qint64 savedAtMs;
};

class TuneMemoryStore : public QObject {
    Q_OBJECT
public:
    std::optional<TuneMemory> recall(int antenna, Band band) const;
    void store(const TuneMemory& mem);
    void clear(int antenna, Band band);
    void clearAll();
    QVector<TuneMemory> listAll() const;       // sorted by band, then antenna
signals:
    void changed();
};
```

AppSettings keys: `TGXL_TuneMemory_Ant<N>_Band<M>` (JSON object per
slot: `{ "c1": 42, "l": 199, "c2": 88, "savedAt": <epoch_ms> }`).

**Recall flow.** `SliceModel` emits `bandChanged(Band)`. If the active
TGXL antenna and the new band have a stored memory slot AND
`TGXL_AutoTuneMemoryRecall == True`, attempt to write the stored relay
positions to TGXL.

**Bench-verification caveat.** The TGXL absolute-relay-position write
verb is not in AetherSDR's command set (AetherSDR only steps relays
+/-1 via `tune relay=N move=+/-1`). Bench testing on a real
TGXL will probe for an absolute-write verb (likely `tune relay=N pos=V`).
If TGXL does not accept absolute writes, recall falls back to issuing a
fresh `tune start` (auto-tune cycle); the memory is then a UX
hint ("we've seen good tune here before") rather than a direct restore.
Either path satisfies the operator-facing requirement.

### 4.9 TxInterlockPolicy

`src/core/TxInterlockPolicy.{h,cpp}`. Operator-toggled policy hooked into
`MoxController::onTxRequested` to gate TX based on PGXL state.

```cpp
class TxInterlockPolicy : public QObject {
    Q_OBJECT
public:
    enum Mode { Disabled, Warn, Block };
    Q_ENUM(Mode)

    Q_PROPERTY(Mode mode           READ mode           WRITE setMode           NOTIFY changed)
    Q_PROPERTY(int  graceMs        READ graceMs        WRITE setGraceMs        NOTIFY changed)
    Q_PROPERTY(bool swrGateEnabled READ swrGateEnabled WRITE setSwrGateEnabled NOTIFY changed)
    Q_PROPERTY(float swrGateMax    READ swrGateMax     WRITE setSwrGateMax     NOTIFY changed)

    // Returns true if TX should proceed; emits warning / denial signals
    // for the toast and Setup-page status surface.
    bool evaluateTxRequest(bool ampPresent, bool ampInOperate, float currentSwr);

signals:
    void warned(const QString& reason);
    void denied(const QString& reason);
};
```

AppSettings keys: `PGXL_TxInterlockMode` (string, default
`"Disabled"`), `PGXL_TxInterlockGraceMs` (int, default 3000),
`PGXL_TxSwrGate` (bool, default `False`), `PGXL_TxSwrGateMax` (float,
default 3.0).

Default mode is `Disabled` so out-of-box TX behavior matches AetherSDR
exactly; the gating is opt-in.

---

## 5. UI surfaces

### 5.1 `AmpApplet` (new)

`src/gui/applets/AmpApplet.{h,cpp}`. Port from AetherSDR
`src/gui/AmpApplet.{h,cpp}`. Inherits the NereusSDR `AppletWidget` base
class to pick up the standard title-bar styling and helper factories
(`styledButton`, etc.).

Layout (vertical):

1. `Fwd Pwr` `HGauge`, range 0..2000 W (yellow + red threshold at 1500 W).
2. `SWR` `HGauge`, range 1.0..3.0 (yellow + red at 2.5).
3. `Temp` `HGauge`, range 0..100 degC (yellow + red at 80).
4. Spacer.
5. Telemetry row: `Volts: NNN  Amps: N.N` + `MEffA: XXX` labels stacked
   left; OPERATE / STANDBY button right (fills remaining width).

The OPERATE button is a single cycle button per AetherSDR (not three
exclusive buttons). Its label and color reflect device state:

* `IDLE`, `OPERATE`, `TRANSMIT_A`, `TRANSMIT_B` -> green OPERATE button.
* `STANDBY`, `POWERUP`, `FAULT` -> blue STANDBY button.

Click handler:

```cpp
emit operateToggled(currentLabelIsOperate ? false : true);
```

`MainWindow` wires `AmpApplet::operateToggled(bool on)` to
`PgxlConnection::sendCommand("operate")` or `sendCommand("standby")`. The
AetherSDR audit found a small gap where this connection has to be made in
MainWindow explicitly; we will replicate that pattern.

### 5.2 `TunerApplet` (rewire existing)

`src/gui/applets/TunerApplet.{h,cpp}`. Existing skeleton at
`src/gui/applets/TunerApplet.h:36` is layout-equivalent to AetherSDR's; we
keep the gauges as-is and:

1. Drop the "Aries ATU" header comments. Retitle to "Tuner Genius".
2. Remove every `NyiOverlay::markNyi(...)` call (lines 82, 91, 125, 154,
   155, 156, 187, 188, 189).
3. Replace the three `QProgressBar` relay bars with `RelayBar` instances.
4. Replace the 3-button `QButtonGroup` (m_operateBtn / m_bypassBtn /
   m_standbyBtn / m_modeGroup) with a single cycling button per AetherSDR.
   Delete `m_bypassBtn`, `m_standbyBtn`, `m_modeGroup`. `cycleOperateState()`
   slot drives `TunerModel::setOperate(...)` and `setBypass(...)` in the
   AetherSDR sequence: OPERATE -> BYPASS -> STANDBY -> OPERATE.
5. Add an `m_antContainer` widget holding 3 antenna buttons; visibility
   gated on `TunerModel::hasDirectConnection() && hasAntennaSwitch()`.
6. Repurpose the existing fwd/swr `HGauge`s to receive updates from
   `TunerModel::metersChanged(float fwd, float swr)`. Add amp-aware scale
   switching via a new `setPowerScale(int maxWatts, bool hasAmplifier)`
   slot that mirrors `SMeterWidget::setPowerScale`.
7. Add the post-tune SWR capture: 400 ms timer started on
   `tuning->false`, captures any SWR value > 1.01 into the TUNE button
   label as `"SWR x.xx"`.

The header attribution block stays (this file is already attributed). The
modification history note gets a new line for the 3M-/3J-style port date.

### 5.3 `Setup -> Network -> Peripherals` page

New page added inside `src/gui/setup/CatNetworkSetupPages.{h,cpp}`, which is
where TCI Server already lives. Single page (not two), with rows for
TGXL and PGXL. Header label: "Peripherals".

Grid layout (one row per device):

| Column | Widget | Width hint |
|---|---|---|
| Name | `QLabel` ("Tuner Genius XL" / "Power Genius XL") | 160 |
| Host | `QLineEdit` (placeholder "192.168.1.42") | 180 |
| Port | `QSpinBox` (defaults 9010 / 9008, range 1..65535) | 80 |
| Scan | `QPushButton "Scan LAN"` | 80 |
| Action | `QPushButton "Connect"` / `"Disconnect"` (toggles label) | 100 |
| Status | `QLabel` (live: "Disconnected", "Connecting...", "Connected to 192.168.1.42:9010", "Error: Connection refused", etc.) | 280 |

Trailing footnote text:

> Configure peripherals on your local network. Devices auto-connect on app
> launch if a Host is configured. The Scan LAN button passively listens for
> 4O3A device announcements on UDP 9008 / 9010 for 3 seconds.

The Scan LAN button opens a modeless `LanScanDialog` (modal would block the
listener) which:

* Shows a 3-second progress bar.
* Populates a `QTableWidget` (columns: Model, IP, Port, Version, Serial,
  Nickname) as `LanDiscovery::deviceDiscovered` signals fire.
* On double-click of a row, fills the Host and Port fields of the matching
  Peripherals row, then closes.

### 5.4 `SMeterWidget` (new) + `AppletPanelWidget` header refactor

`src/gui/SMeterWidget.{h,cpp}`. Port verbatim from AetherSDR's
`src/gui/SMeterWidget.{h,cpp}` (134 + 701 LOC). The widget is a custom
`QWidget` that paints its own analog needle gauge.

Two NereusSDR-native divergences from AetherSDR (both explicit
operator-requested):

1. **Settings UI moves from an inline header strip to a right-click
   context menu on the widget itself** (section 5.4.2). AetherSDR's
   `AppletPanel.cpp:237..328` always-visible settings row is dropped.
2. **Two additional RX modes, `SignalAverage` and `MaxBin`, are added**
   (section 5.4.3) ported from Thetis. `SignalAverage` uses
   `GetRXAMeter(RXA_S_AV)`; `MaxBin` uses the `SetupDetectMaxBin` /
   `GetDetectMaxBin` detector and `MeterType.SIGNAL_MAX_BIN`. AetherSDR
   has neither.

#### 5.4.1 Public API and scale

Properties (extended from AetherSDR with the two new RX modes):

```cpp
Q_PROPERTY(float levelDbm READ levelDbm)

enum class TxMode  { Power, SWR, Level, Compression };
enum class RxMode  { SMeter, SignalAverage, SMeterPeak, MaxBin };  // SignalAverage + MaxBin: new
enum class Decay   { Fast, Medium, Slow };

void setLevel(float dbm);                                    // shared by all RX modes
void setTxMeters(float fwdPower, float swr);
void setMicMeters(float micLevel, float compLevel, float micPeak, float compPeak);
void setTransmitting(bool tx);
void setTxMode(const QString& mode);
void setRxMode(const QString& mode);                         // accepts "Sig Avg" and "Max Bin"
void setPowerScale(int maxWatts, bool hasAmplifier);
void setPeakHoldEnabled(bool on);
void setPeakHoldTimeMs(int ms);
void setPeakDecayRate(Decay rate);
void resetPeak();

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;   // new: builds settings menu
```

Scale (verbatim from AetherSDR):

* S0 = -127 dBm, S1 = -121 dBm, ..., S9 = -73 dBm (6 dB per S-unit).
* S9+10/20/40/60 = -63 / -53 / -33 / -13 dBm.
* Arc 55 deg .. 125 deg around a center below the widget.
* Needle: 45 ms attack, 180 ms release.
* Peak hold: orange triangle at peak, optional red dashed line at hold value.

PGXL-aware power scale (verbatim from `setPowerScale`):

* `hasAmplifier == true` -> max 2000 W, red zone start 1500 W. Ticks every
  100 W, labels every 500 W.
* `maxWatts > 100` -> max 600 W (Aurora), red zone start 500 W. Ticks every
  50 W, labels every 100 W.
* Else -> max 120 W (barefoot), red zone start 100 W. Ticks every 10 W,
  labels every 40 W.

#### 5.4.2 Right-click context menu (replaces AetherSDR inline settings)

`SMeterWidget::contextMenuEvent` builds a `QMenu` with the following
structure. All actions are checkable where the underlying setting is
binary or enum-valued, and exclusive within their group via `QActionGroup`
so the menu doubles as a state readout:

```
S-Meter Settings (right-click)
+- TX Mode  (exclusive action group, current ticked)
|  +- Power
|  +- SWR
|  +- Level
|  +- Compression
+- RX Mode  (exclusive action group, current ticked; Thetis order)
|  +- Signal              (SMeter, AetherSDR + Thetis "Signal")
|  +- Sig Avg             (SignalAverage, new; Thetis "Sig Avg")
|  +- Signal Peak         (SMeterPeak, AetherSDR-only)
|  +- Max Bin             (MaxBin, new; Thetis "SIGNAL_MAX_BIN")
+- Peak Hold
|  +- Enabled             (checkable toggle)
|  +- Decay
|  |  +- Fast             (exclusive within Decay)
|  |  +- Medium
|  |  +- Slow
|  +- Reset               (transient action, fires resetPeak())
```

Action handlers route to the same `setTxMode` / `setRxMode` /
`setPeakHoldEnabled` / `setPeakDecayRate` / `resetPeak` slots that the
former inline widgets used, so persistence is unchanged. The AetherSDR
header strip is removed from the AppletPanelWidget header layout; the
fixed header now contains only the SMeterWidget itself.

Settings widgets persisted to AppSettings:

1. **TX Mode** (Power / SWR / Level / Compression). Key
   `SMeter_TxSelect` (int, 0..3, default 0).
2. **RX Mode** (Signal / Sig Avg / Signal Peak / Max Bin). Key
   `SMeter_RxSelect` (int, 0..3, default 0). Range widened from AetherSDR's
   0..1 to accommodate Sig Avg (Thetis) and Max Bin (Thetis).
3. **Peak Hold Enabled**. Key `PeakHoldEnabled` (string "True" / "False",
   default "False").
4. **Peak Decay Rate** (Fast / Medium / Slow). Key `PeakDecayRate` (string,
   default "Medium").
5. **Reset Peak**: transient, not persisted.

Decay-rate mapping (verbatim from AetherSDR `AppletPanel.cpp:306`):

| Setting | Decay (dB/s) | Hold (ms) |
|---|---:|---:|
| Fast   | 20 | 200 |
| Medium | 10 | 500 |
| Slow   |  5 | 1000 |

#### 5.4.3 Max Bin RX mode (ported from Thetis)

When RX Mode is `MaxBin`, `SMeterWidget::setLevel(dbm)` is driven by the
WDSP `GetDetectMaxBin` poller instead of the demodulator level meter.
Behavior reads as "dBm of the strongest FFT bin in a narrow band around
the slice's passband", smoothed with the Thetis-default tau of 0.5 s at
60 fps.

**New `WdspEngine` wrappers** (`src/core/wdsp/WdspEngine.{h,cpp}`):

```cpp
// From Thetis wdsp/analyzer.c:775 [@501e3f5]
// SetupDetectMaxBin(int run, int disp, int ss, int LO, double rate,
//                   double fLow, double fHigh, double tau, int frame_rate)
void setupMaxBinDetector(int displayChannel,
                         int ss = 0,
                         int LO = 0,
                         double rateHz = 192000.0,
                         double fLowHz = -3000.0,
                         double fHighHz = -300.0,
                         double tauSeconds = 0.5,
                         int frameRate = 60);

// From Thetis wdsp/analyzer.c:830 [@501e3f5]
// double GetDetectMaxBin(int disp)
double getMaxBinDbm(int displayChannel);
```

Construction parameters track Thetis defaults verbatim. The
`fLow`/`fHigh` window is driven from the active slice's filter low/high
cut (re-applied through `setupMaxBinDetector` whenever the filter changes,
the slice retunes by more than ~half the filter width, or the rate
changes). Rate is set from the active panadapter's display sample rate.

**MeterPoller integration** (`src/gui/meters/MeterPoller.{h,cpp}`):

The existing 100 ms / 10 fps `MeterPoller` already polls WDSP meters for
the composite MeterWidget. We add one new path:

* When `SMeterWidget::rxMode() == MaxBin` and the slice is RX (not TX),
  the poller calls `wdspEngine->getMaxBinDbm(displayChannel)` and feeds
  it to `SMeterWidget::setLevel(dbm)`.
* When RX mode changes back to Signal or Signal Peak, the poller
  reverts to the normal level-meter feed.
* The MaxBin path runs only when the widget is visible and a radio is
  connected; otherwise `setupMaxBinDetector(..., run=0, ...)` (drop the
  detector) to save FFT work.

**Slice-aware reconfiguration**:

`SliceModel::filterChanged` and `SliceModel::frequencyChanged` (when the
delta exceeds the half-bandwidth) re-call `setupMaxBinDetector` with the
new fLow/fHigh window. Reconfiguration is rate-limited to one call per
100 ms via a `QTimer::singleShot` debounce so a fast tune sweep does not
hammer the WDSP detector.

**Why Max Bin and not just SMeter+Peak?**

Quoting the Thetis Setup tooltip phrasing from `setup.cs:25178..25446`
where users toggle `igs.MaxBin`: max-bin reads the **strongest signal in
the passband**, whereas SMeter reads the **demodulator output level**
(which is mode-dependent: AM detects the envelope, SSB detects audio
post-filter, etc.). For DXing or signal hunting the max-bin reading is
often more informative because it tracks the loudest carrier directly,
independent of demodulator quirks.

### 5.5 Bottom status bar

A small TGXL presence chip is added to `MainWindow::buildStatusBar()`,
visible when `TunerModel::isPresent()` returns true. Matches AetherSDR's
`m_tgxlIndicator`. **No PGXL chip** is added; AetherSDR has none, and
PGXL state is already visible inside `AmpApplet`.

### 5.6 Setup &rarr; Network &rarr; PGXL Advanced (new page)

New page inside `src/gui/setup/CatNetworkSetupPages.{h,cpp}`. Hidden from
the Setup sidebar until a PGXL is connected (or has connected during the
current session). Six sections in a single scrolling page:

**5.6.1 Identity &amp; Status.** Read-only firmware version + serial,
editable nickname (via `writeSetup(nickname=...)`), current state badge
(driven by `PgxlConnection::statusUpdated`), MeFFA string.

**5.6.2 Hardware.** Section flagged "requires Save &amp; Reboot":

* Bias mode toggle (Class A vs Class AB). Maps to a `writeSetup(bias=...)`
  field; exact key name confirmed by bench testing of `readSetup`.
* Fan mode dropdown (Auto / Quiet / Continuous). Maps to
  `writeSetup(fan=...)`.
* LED intensity slider (0..100). Maps to `writeSetup(led=...)`.
* TX power cap: optional checkbox + watts spinbox. **Soft alert only**;
  PGXL does not enforce, NereusSDR shows a toast when peak forward
  exceeds the cap.

**5.6.3 Network.** DHCP toggle + IP / netmask / gateway QLineEdits. Maps
to `writeIfconf(...)`. Warning text: "PGXL must be unicast-reachable
from this host after the change; if you lose connection, use Scan LAN to
rediscover."

**5.6.4 Pairing &amp; band source.** Three-option dropdown:

* `flexradio (LAN PTT & band)` (default). Sends `flexradioPair(...)` on
  connect with graceful fallback.
* `amplifier create (no PTT, manual band)`. Sends `amplifierCreate` only.
  Operator must use BCD / CAT for band info.
* `None`. Skip both; operator uses front-panel / BCD only.

Plus TX antenna toggle (ANT1 / ANT2) and slice binding toggle (A / B).
Both persist independently of the dropdown so changing pairing mode
doesn't reset the antenna mapping.

**5.6.5 Diagnostics.** 8-cell grid bound to `ConnectionDiagnostics`
Q_PROPERTYs (section 4.6). Updates at 1 Hz.

**5.6.6 Fault history.** Table bound to `FaultLog::events()` (section
4.7), newest first. Columns: When (local time), State, FWD/SWR/Temp
snapshot, Likely cause. Clear All button at the bottom.

**Footer.** Revert + Save &amp; Reboot Amp buttons. Save &amp; Reboot
opens a confirmation modal:

> Sending `save` will persist your configuration to flash and reboot the
> PGXL. The amplifier will be offline for approximately 20 seconds.
> NereusSDR will auto-reconnect when it returns. Do not transmit during
> reboot.

On confirm: `save()` is sent, the connection drops (PGXL reboots),
auto-reconnect handles re-establishment, status label reflects the
in-progress reboot.

### 5.7 Setup &rarr; Network &rarr; TGXL Advanced (new page, parallel)

Same skeleton as PGXL Advanced, smaller because TGXL has fewer settings.
Five sections:

* **Identity &amp; Status.** Editable nickname, firmware version, serial,
  current state (OPERATE / BYPASS / STANDBY), model variant (3x1 vs 1x1).
* **Antenna labels** (TGXL-specific; PGXL has no equivalent). Three text
  fields for ANT 1 / ANT 2 / ANT 3 user-defined names ("80 m dipole",
  "vertical", "beverage"). Persisted in AppSettings keys
  `TGXL_Ant1_Label` / `_Ant2_Label` / `_Ant3_Label`. Labels propagate to
  TunerApplet button text and to the Peripherals status string.
* **Network.** Same DHCP / IP / netmask / gateway pattern as PGXL.
* **Tune memory management.** Table bound to `TuneMemoryStore::listAll()`
  (section 4.8). Columns: Band, Antenna, C1, L, C2, Saved at. Per-row
  Clear button + bulk Clear All button. Toggle for
  `TGXL_AutoTuneMemoryRecall`.
* **Diagnostics.** Same 8-cell grid as PGXL.
* **Fault history.** Same shape as PGXL.

**Footer.** Revert + Save buttons. Save behavior: bench testing confirms
whether TGXL accepts the `save` verb; if yes, same Save &amp; Reboot
modal applies; if no, the button is hidden.

### 5.8 Setup &rarr; Transmit &rarr; PGXL Interlock (new section)

Lives under the existing Setup &rarr; Transmit category because it's a
TX policy rather than a PGXL device-config setting. Three controls bound
to `TxInterlockPolicy`:

* Mode combo: Disabled / Warn / Block.
* Grace period spinbox (ms, default 3000, range 0..10000). Window after
  PTT during which a non-OPERATE PGXL state does not yet fire the
  policy.
* Optional SWR gate: checkbox + max-SWR spinbox (default 3.0). When
  enabled, denies / warns TX if current SWR exceeds the threshold.

### 5.9 Right-click context menus on applets

Both `AmpApplet` and `TunerApplet` override `contextMenuEvent` to expose
quick navigation to the corresponding Advanced page plus device-specific
quick actions. NereusSDR-native UX divergence from AetherSDR (which has
no equivalent).

**`AmpApplet::contextMenuEvent` menu:**

```
PGXL
+- Open PGXL Advanced...        (navigates to Setup -> Network -> PGXL Advanced)
+- ----
+- Disconnect                   (or Reconnect when offline)
+- Copy diagnostics to clipboard
+- ----
+- View FlexRadio API reference (opens wiki URL in default browser)
```

**`TunerApplet::contextMenuEvent` menu:**

```
TGXL
+- Open TGXL Advanced...        (navigates to Setup -> Network -> TGXL Advanced)
+- ----
+- Save current tune memory     (current band + active antenna; calls TuneMemoryStore::store)
+- Clear current tune memory
+- ----
+- Disconnect                   (or Reconnect when offline)
+- Copy diagnostics to clipboard
```

Navigation is implemented by emitting a signal that `MainWindow` catches
and uses to open `SetupDialog` at the right page key, e.g. `mw->openSetup("pgxlAdvanced")`.

---

## 6. Protocol details

All four extracted from FlexRadio's [PowerGenius Ethernet API wiki](https://github.com/flexradio/smartsdr-api-docs/wiki/PowerGenius-Ethernet-API)
and cross-validated against AetherSDR's parser implementation.

### 6.1 Frame format

UTF-8 text, line-delimited:

* Commands (client to device): `C<seq>|<cmd>\r` where `\r` is 0x0D.
  Sequence number 1..255 wraps.
* Responses (device to client): `R<seq>|<hex>|<body>\n`. `<hex> == 0`
  means success; non-zero is failure. Body is space-separated `key=value`
  pairs.
* Async status push (device to client): `S0|<object> <key>=<value> ...\n`.
  TGXL uses `S0|state ...` for state changes and `S<seq>|status ...` for
  status poll responses.
* Connect prologue (device to client): `V<a.b.c> [AUTH]\n`. AUTH is only
  required for WAN connections; on the local LAN we never see it.

### 6.2 Command and response keys

PGXL response keys consumed by AmpApplet:

| Key | Unit | Description |
|---|---|---|
| `temp` | degC (float) | Heat-sink temperature. |
| `id` | A (float) | Drain current. |
| `vac` | V (int) | Mains voltage. |
| `state` | enum | One of: IDLE, OPERATE, STANDBY, POWERUP, FAULT, TRANSMIT_A, TRANSMIT_B. |
| `meffa` | label | Mechanical efficiency string. |
| `peakfwd` | dBm (float) | Peak forward power. |
| `swr` | dB (float, signed) | Return loss; convert: `swr_ratio = pow(10, -rl_db/20)`. |
| `fwd` | floor (float) | Continuous forward power floor. |

TGXL response keys consumed by TunerModel:

| Key | Unit | Description |
|---|---|---|
| `relayC1`, `relayL`, `relayC2` | int 0..255 | Capacitor/inductor relay byte. |
| `operate` | "0"/"1" | OPERATE state. |
| `bypass` | "0"/"1" | BYPASS state. |
| `tuning` | "0"/"1" | Tune cycle active. |
| `antA` | int 0..2 | Active antenna for 3x1 model (0-indexed). |
| `one_by_three` | "0"/"1" | Indicates TGXL 3x1 (antenna switch present). |
| `serial_num` | string | Device serial. |
| `model` | string | e.g. "TunerGeniusXL". |
| `ip` | string | Reported IP address. |
| `fwd`, `swr` | float | Tuner meter readings. |

PGXL command verbs (subset used by NereusSDR):

* `info` -> initial inventory.
* `status` -> poll meters.
* `operate` -> request OPERATE.
* `standby` -> request STANDBY.

TGXL command verbs (subset used by NereusSDR):

* `info`, `status` -> as above.
* `tune start` -> initiate full auto-tune cycle.
* `tune relay=N move=+/-1` -> manual single-step.
* `operate=1` / `operate=0` -> toggle OPERATE.
* `bypass=1` / `bypass=0` -> toggle BYPASS.
* `activate ant=N` -> antenna select (3x1 model only).

### 6.3 UDP discovery

Both PGXL and TGXL announce themselves periodically on the local subnet's
directed broadcast address. Datagrams arrive on the same port number that
each device uses for TCP commands.

* PGXL: UDP 9008, period ~1 sec.
* TGXL: UDP 9010, period ~1 sec (by analogy; bench-confirmable).

Announcement format (text, single line; official regex from FlexRadio wiki):

```
TunerGeniusXL ip=192.168.1.42 v=1.2.17 serial=1234 nickname=ShackTuner
PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=5678 nickname=ShackAmp
```

```regex
^(?<model>\S+)\s+ip=(?<ip>\d+\.\d+\.\d+\.\d+)\s+v=(?<v>\S+)\s+serial=(?<serial>\S+)\s+nickname=(?<nick>\S+)$
```

Parsed and deduplicated by `LanDiscovery` (section 4.5).

### 6.4 Extended PGXL command reference (NereusSDR-native)

Verbatim from the [FlexRadio PowerGenius Ethernet API wiki](https://github.com/flexradio/smartsdr-api-docs/wiki/PowerGenius-Ethernet-API).
Implementation lives in `PgxlConnection` (section 4.1.1) and is exercised
at connect time and via the Setup &rarr; PGXL Advanced page.

**`amplifier create`** registers the exciter pairing.

```
C<seq>|amplifier create ip=<ip> port=<port> model=<model> serial_num=<serial> ant=<map>
```

Example NereusSDR sends:

```
C1|amplifier create ip=192.168.1.43 port=9008 model=NereusSDR serial_num=NereusSDR-AA:BB:CC:DD:EE:FF ant=ANT1:PORTA,ANT2:PORTB
```

Response: `R<seq>|<hex>|<message>`; hex 0 = success.

**`flexradio ampslice=...`** claims FLEX-style pairing for LAN PTT and band.

```
C<seq>|flexradio ampslice=A serial=<radio_serial> txant=ANT1 ptt=LAN active=1
```

Response on success: `R<seq>|0|serial=... txant=ANT1 ptt=LAN active=1`. On
failure (PGXL rejects the serial format or pairing slot is busy):
`R<seq>|<nonzero>|<error>`. NereusSDR treats any non-zero as "pairing
failed" and falls back to no-LAN-PTT mode.

**`keepalive enable`** starts the amp-side watchdog feeder.

```
C<seq>|keepalive enable
```

Response: success ack. After this, the amp expects periodic communication;
NereusSDR's own keepalive timer issues a `status` every 30 s (configurable
via `PGXL_KeepaliveSec`) to satisfy it.

**`ping`** is a no-op roundtrip for RTT measurement.

```
C<seq>|ping
```

Response: `R<seq>|0|`. NereusSDR records `(now - sentMs)` as the RTT in
`ConnectionDiagnostics::lastRttMs`.

**`interlock create`** / **`interlock disable`** define antenna validity.

```
C<seq>|interlock create type=AMP valid_antennas=ANT1,ANT2 name=PG-XL serial=<serial>
C<seq>|interlock disable <id>
```

Only used when the operator toggles "Restrict TX to declared antennas" in
Setup &rarr; PGXL Advanced. Not the same as `TxInterlockPolicy` (section
4.9), which is an entirely-client-side TX gate.

**`status`** is unchanged from the AetherSDR baseline (5 Hz poll).

**`setup read`** returns nickname, fan mode, MeFFA state, LED intensity.

```
C<seq>|setup read
R<seq>|0|nickname=ShackAmp fan=auto meffa=off led=65
```

The companion write is not formally specified by the wiki; bench testing
will confirm the exact syntax (likely `setup nickname=... fan=... led=...`
mirroring the read's kv shape).

**`ifconf read`** / **`ifconf address=...`** for network configuration.

```
C<seq>|ifconf read
R<seq>|0|address=192.168.1.43 netmask=255.255.255.0 gateway=192.168.1.1 dhcp=false

C<seq>|ifconf address=192.168.1.43 netmask=255.255.255.0 gateway=192.168.1.1 dhcp=false
```

**`save`** persists config to flash and **reboots the amp**.

```
C<seq>|save
R<seq>|0|saving
S|state=REBOOT
```

The async `S|state=REBOOT` is the last frame before TCP drops. NereusSDR
expects the drop, marks the device as rebooting, and starts the auto-reconnect
backoff sequence at attempt 0 with a 20 s initial wait (longer than the
default 1 s because the amp is rebooting).

**`message severity=info code=... "text"`** is documented but not used by
NereusSDR (out of scope per section 11).

### 6.5 Connection lifecycle sequence

```
Client (NereusSDR)                          PGXL
       |                                      |
       |--- TCP connect to ip:9008 ---------->|
       |                                      |
       |<------------------ V3.8.9 ---------- |  (version banner)
       |                                      |
       |--- C1|amplifier create ... --------->|
       |<------------ R1|0|<details> -------- |
       |                                      |
       |--- C2|flexradio ampslice=A ... ----->|  (Tier 2 addition)
       |<-- R2|0|... ------------------------ |  OR
       |<-- R2|<nonzero>|<err> -------------- |  (fall back to no-PTT)
       |                                      |
       |--- C3|keepalive enable ------------->|  (Tier 2 addition)
       |<------------ R3|0| ----------------- |
       |                                      |
       |--- C4|status ----------------------->|
       |<-- R4|0|state=OPERATE temp=42 ... -- |
       |                                      |
       |--- (every 200 ms) C<n>|status ------>|
       |<-- R<n>|0|... ---------------------- |
       |                                      |
       |--- (every 30 s) C<n>|status -------->|  (keepalive cadence)
       |--- (every 60 s) C<n>|ping ---------->|  (Tier 2 addition; RTT)
       |<-- R<n>|0| ------------------------- |
       |                                      |
       |     . . . (eventually a fault) . . . |
       |<-- S0|state=FAULT fwd=1850 swr=2.8 - |  (async push)
       |     . . . (FaultLog::capture)        |
```

---

## 7. Data flow / signal wiring

```
PgxlConnection ──statusUpdated(kvs)─────────────► RadioModel (caches)
                                          └────► AmpApplet (gauges + label)
AmpApplet ──operateToggled(bool)─────────► PgxlConnection.sendCommand("operate"|"standby")

TgxlConnection ──stateUpdated(kvs)──────► TunerModel.applyStatus()
              ──statusUpdated(kvs)──────► TunerModel.applyStatus()

TunerModel ──stateChanged─────────────► TunerApplet.syncFromModel()
          ──metersChanged(fwd,swr)────► TunerApplet.updateMeters()
          ──directConnectionChanged──► TunerApplet relay scroll + ANT row visibility
          ──tuningChanged(bool)──────► TunerApplet tune button + post-tune capture

TunerApplet ──autoTune───► TunerModel.autoTune() ──► TgxlConnection.sendCommand("tune start")
           ──adjustRelay─► TunerModel.adjustRelay() ─► TgxlConnection.adjustRelay()
           ──setAntennaA─► TunerModel.setAntennaA() ─► TgxlConnection.sendCommand("activate ant=N")
           ──cycleOperateState──► TunerModel.setOperate/setBypass ─► TgxlConnection.sendCommand("operate=N"|"bypass=N")

PgxlConnection ──statusUpdated────► RadioModel.setHasAmplifier(true) once
                                  └► RadioModel.setAmpOperate(state ∈ {IDLE,OPERATE,TRANSMIT_*})

RadioModel ──hasAmplifierChanged──► SMeterWidget.setPowerScale(maxWatts, hasAmp)
          ──ampStateChanged──────► SMeterWidget.setPowerScale (refresh)
          ──ampMetersChanged─────► SMeterWidget.setTxMeters (gated by ampOperate)
          ──txMetersChanged──────► SMeterWidget.setTxMeters (fallback)

MeterPoller (10 fps) ──► if SMeterWidget.rxMode() == SMeter | SMeterPeak:
                          WdspEngine.getRxaMeter(ch, RXA_S_PK) ──► SMeterWidget.setLevel
                        else if rxMode() == SignalAverage:
                          WdspEngine.getRxaMeter(ch, RXA_S_AV) ──► SMeterWidget.setLevel
                        else if rxMode() == MaxBin:
                          WdspEngine.getMaxBinDbm(displayChan) ──► SMeterWidget.setLevel

SliceModel ──filterChanged─────► WdspEngine.setupMaxBinDetector(fLow=lowCut, fHigh=highCut, ...)
          ──frequencyChanged──► (debounced 100ms) same setupMaxBinDetector call

SMeterWidget.contextMenuEvent ──► builds QMenu, action triggers route to
                                  setTxMode / setRxMode / setPeakHoldEnabled /
                                  setPeakDecayRate / resetPeak slots; AppSettings
                                  keys updated by the same setters as before.
```

The S-Meter `setTxMeters` feed switch is gated in MainWindow:

```cpp
connect(&m_radioModel, &RadioModel::ampMetersChanged, this,
        [this](float fwd, float swr) {
    if (m_radioModel.hasAmplifier() && m_radioModel.ampOperate())
        m_sMeter->setTxMeters(fwd, swr);
});
connect(&m_radioModel, &RadioModel::txMetersChanged, this,
        [this](float fwd, float swr) {
    if (!m_radioModel.hasAmplifier() || !m_radioModel.ampOperate())
        m_sMeter->setTxMeters(fwd, swr);
});
```

This is the AetherSDR pattern (commit f971244 per the source audit) ported
unchanged.

### 7.1 Additional flows for the NereusSDR-native scope

```
PgxlConnection ──connected()──────► RadioModel.runPgxlPairing()
                                   ├► amplifierCreate(ourSerial, "NereusSDR", antMap)
                                   ├► if PGXL_PairAttempt: flexradioPair(slice, radioSerial, txAnt)
                                   ├► enableKeepalive()
                                   └► readSetup() + readIfconf() (lazy)

PgxlConnection ──pairingResult(ok, detail)──► PeripheralsPage.updateStatus("LAN PTT active" or "PTT via BCD/CAT only")

PgxlConnection ──pongReceived(seq, rttMs, tag)──► ConnectionDiagnostics.setLastRttMs(rttMs)
              ──pingTimedOut(seq)──────────────► ConnectionDiagnostics.incrementMissedPings()
                                                    if missed >= 3: trigger reconnect

PgxlConnection ──disconnected()──► if PGXL_AutoReconnect:
                                     m_reconnectTimer.start(backoff[m_reconnectAttempts])
                                     emit reconnectAttempt(attemptNumber, backoffMs)

PgxlConnection ──faultCaptured(ev)──► FaultLog.capture(ev) [PGXL_FaultHistory]
                                    └► ConnectionDiagnostics.incrementFaultsSession()

SliceModel ──bandChanged(Band)──► PgxlConnection.setBand(slice.frequencyHz)  (if pairing succeeded)
                                  TuneMemoryStore.recall(activeAnt, band)
                                  └► if hit and PGXL_AutoTuneMemoryRecall:
                                        TgxlConnection.writeRelays(c1, l, c2)  [bench-confirmable]
                                     else:
                                        TunerApplet.memoryRow.showHint("no memory for this slot")

MoxController ──txRequested()──► TxInterlockPolicy.evaluateTxRequest(ampPresent, ampOperate, swr)
                                  ├► if Mode == Disabled: allow
                                  ├► if Mode == Warn: allow + emit warned(reason); toast
                                  └► if Mode == Block: deny + emit denied(reason); toast

AmpApplet ──contextMenuEvent──► QMenu actions:
                                "Open PGXL Advanced..." → MainWindow.openSetup("pgxlAdvanced")
                                "Disconnect" / "Reconnect"
                                "Copy diagnostics" → QGuiApplication.clipboard()

TunerApplet ──contextMenuEvent──► QMenu actions:
                                  "Open TGXL Advanced..." → MainWindow.openSetup("tgxlAdvanced")
                                  "Save current tune memory" → TuneMemoryStore.store(...)
                                  "Clear current tune memory" → TuneMemoryStore.clear(...)
                                  "Disconnect" / "Reconnect"
                                  "Copy diagnostics" → clipboard
```

The `save` command path (Setup &rarr; PGXL Advanced &rarr; Save &amp;
Reboot) does an extra dance:

```
SetupPage button clicked
       │
       ▼
Confirmation modal: "20s offline, NereusSDR will auto-reconnect"
       │
       ▼ (operator confirms)
PgxlConnection.save()
       │
       ▼
PGXL: R<seq>|0|saving  then  S|state=REBOOT  then  TCP drop
       │
       ▼
PgxlConnection.disconnected() → reconnectTimer.start(20_000)  // 20s initial
       │
       ▼
After ~20 s: TCP reconnect attempts begin
       │
       ▼
PGXL back up: version banner; re-runs pairing flow from 7.1
       │
       ▼
SetupPage shows "Reboot complete; pairing succeeded"
```

---

## 8. Persistence (AppSettings)

All flat global keys, not per-MAC. Rationale: PGXL/TGXL and the S-Meter
config are station-level, not radio-level. Matches AetherSDR. Per-MAC
scoping would be future work if a real multi-radio-multi-station user case
emerges.

| Key | Type | Default | Notes |
|---|---|---|---|
| `TGXL_ManualIp` | string | "" | Empty disables auto-connect. |
| `TGXL_ManualPort` | int | 9010 | |
| `PGXL_ManualIp` | string | "" | |
| `PGXL_ManualPort` | int | 9008 | |
| `SMeter_TxSelect` | int (0..3) | 0 | Power=0, SWR=1, Level=2, Compression=3. |
| `SMeter_RxSelect` | int (0..3) | 0 | Signal=0, SignalAverage=1, SignalPeak=2, MaxBin=3. Range widened from AetherSDR's 0..1 to accommodate the two Thetis-ported modes (section 5.4.3). |
| `PeakHoldEnabled` | "True"/"False" | "False" | |
| `PeakDecayRate` | "Fast"/"Medium"/"Slow" | "Medium" | |

**Tier 2 (NereusSDR-native) keys:**

| Key | Type | Default | Notes |
|---|---|---|---|
| `PGXL_AutoReconnect` | "True"/"False" | "True" | Gates the reconnect timer entirely. |
| `PGXL_KeepaliveSec` | int | 30 | Cadence for `keepalive enable`-driven status pokes. |
| `PGXL_PingSec` | int | 60 | Ping cadence. 0 disables pings. |
| `PGXL_PairAttempt` | "True"/"False" | "True" | Whether to try `flexradio` pairing on connect. |
| `PGXL_PairingMode` | string | "flexradio" | "flexradio", "amplifier-create", or "none". |
| `PGXL_FlexAmpSlice` | string ("A"/"B") | "A" | Slice binding for FLEX pairing. |
| `PGXL_TxAnt` | string | "ANT1" | "ANT1" or "ANT2". |
| `TGXL_AutoReconnect` | "True"/"False" | "True" | Same shape as PGXL. |
| `TGXL_KeepaliveSec` | int | 30 | |
| `TGXL_PingSec` | int | 60 | |

**Tier 3 (UX wins) keys:**

| Key | Type | Default | Notes |
|---|---|---|---|
| `TGXL_Ant1_Label` | string | "ANT 1" | Operator-defined antenna name; propagates to TunerApplet buttons. |
| `TGXL_Ant2_Label` | string | "ANT 2" | |
| `TGXL_Ant3_Label` | string | "ANT 3" | |
| `TGXL_AutoTuneMemoryRecall` | "True"/"False" | "False" | Auto-restore relay positions on `bandChanged`. |
| `TGXL_TuneMemory_Ant<N>_Band<M>` | JSON object | not set | One key per (antenna, band) slot. `{ "c1": int, "l": int, "c2": int, "savedAt": epoch_ms }`. |
| `PGXL_FaultHistory` | JSON array | `[]` | Last 10 `FaultEvent` records. |
| `TGXL_FaultHistory` | JSON array | `[]` | |
| `PGXL_TxInterlockMode` | string | "Disabled" | "Disabled", "Warn", "Block". |
| `PGXL_TxInterlockGraceMs` | int | 3000 | |
| `PGXL_TxSwrGate` | "True"/"False" | "False" | |
| `PGXL_TxSwrGateMax` | float | 3.0 | |
| `PGXL_PowerCapW` | int | 0 | 0 = disabled; otherwise soft-alert when peak forward exceeds. |

**Tier 4 (Advanced UI / device-config) keys:**

| Key | Type | Default | Notes |
|---|---|---|---|
| `PGXL_Nickname` | string | "" | Cache of last-read nickname (writes go through `writeSetup`). |
| `PGXL_BiasMode` | string | "AB" | "A" or "AB". |
| `PGXL_FanMode` | string | "Quiet" | "Auto", "Quiet", "Continuous". |
| `PGXL_LedIntensity` | int (0..100) | 65 | |
| `PGXL_StaticIp` | string | "" | Empty = DHCP. |
| `PGXL_StaticNetmask` | string | "" | |
| `PGXL_StaticGateway` | string | "" | |
| `TGXL_Nickname` | string | "" | |
| `TGXL_StaticIp` / `TGXL_StaticNetmask` / `TGXL_StaticGateway` | string | "" | |

Auto-connect on launch: in `MainWindow::onRadioConnected()`, for each of
PGXL and TGXL, if `_ManualIp` is non-empty and the connection is not yet
up, call `connectToPgxl()` / `connectToTgxl()`. Verbatim from AetherSDR
`MainWindow.cpp:5293`.

---

## 9. Error handling

| Case | Behavior |
|---|---|
| TCP connect failed (wrong host / wrong port / device off) | `connectionFailed(QString)` signal; Peripherals row Status label turns red with the error text. AmpApplet / TunerApplet show their disconnected state. No auto-retry; user clicks Connect again. |
| TCP connection dropped after being up | `disconnected()` signal; Status label "Disconnected". No auto-reconnect (matches AetherSDR). |
| Malformed frame received | Logged via `qCWarning(lcTuner)`; frame discarded; reading continues. |
| Unknown response key | Logged via `qCDebug(lcTuner)`; ignored in `applyStatus`. |
| PGXL FAULT state | `state=FAULT` arrives; AmpApplet button shows "STANDBY" with non-green styling. Operator addresses fault at the device. |
| PGXL not in OPERATE | S-Meter scale falls back to barefoot (120 W / 600 W); TX feed reverts to radio exciter FWDPWR. |
| LAN scan finds nothing | LanScanDialog stays open for the 3-second window; table stays empty; "No devices found on this subnet" label appears at the end. |
| LAN scan finds duplicate devices | Dedup by `serial` in `LanDiscovery`; only first announcement per serial is emitted. |
| User clicks Scan LAN while UDP port 9008 / 9010 is already in use | Bind fails with `AddressInUseError`; dialog shows "Port 9008 in use; close any other 4O3A utility and retry." |
| `flexradio` pairing rejected by PGXL | `pairingResult(false, errorString)` emitted. Setup status: "Connected; LAN PTT unavailable (pairing rejected: \<error\>)". AmpApplet keeps showing telemetry; band/PTT must go via BCD or CAT. No retry. |
| `keepalive` pings missed (3 consecutive) | Connection considered stale; reconnect triggered. Setup status: "Reconnecting (attempt N)..." Counter visible in Diagnostics. |
| Reconnect attempts exhausted | After 6 failed attempts with exponential backoff, reconnect timer slows to 60 s indefinitely. Setup status: "Disconnected (auto-retry every 60 s)". |
| `save` command sent | Connection drops as expected. Reconnect timer starts with 20 s initial wait (longer than the default 1 s). AmpApplet shows "Rebooting" badge. Auto-reconnect re-runs the pairing flow on return. |
| `writeSetup` / `writeIfconf` rejected | Response hex non-zero. Field rolled back to last-known value; toast: "Setup write rejected: \<error\>." |
| TX interlock denied TX (Block mode) | `denied(reason)` signal; MOX request not honored; toast: "TX blocked: PGXL not in OPERATE." Operator must change mode or address the amp state. |
| TX interlock warned TX (Warn mode) | `warned(reason)` signal; MOX proceeds; toast: "TX proceeding; PGXL is in STANDBY." |
| PGXL FAULT captured | `state=FAULT` arrives; `FaultLog::capture` appends; AmpApplet badge turns red; `ConnectionDiagnostics::faultsSession` increments. Persistent across sessions via `PGXL_FaultHistory`. |
| Tune-memory recall rejected by TGXL (absolute write verb unsupported) | Fall back to `tune start` (fresh auto-tune cycle); TunerApplet memory row shows "memory hint only; running fresh tune." |

PGXL retains its own SWR-trip and overtemp auto-STANDBY safety internally.
NereusSDR's optional `TxInterlockPolicy` adds a **client-side TX gate**
layered on top (default Disabled to match AetherSDR), so operators who
want a software safety net can opt in without changing AetherSDR-style
out-of-box behavior.

---

## 10. Testing

### 10.1 Unit tests

| File | Coverage |
|---|---|
| `tests/PgxlConnectionParseTest.cpp` | Feed synthetic `V`, `R`, `S0` frames; assert correct signals fire with correct kv maps and seq numbers. |
| `tests/TgxlConnectionParseTest.cpp` | Same, plus `stateUpdated` vs `statusUpdated` routing (S0|state vs S<seq>|status). |
| `tests/TunerModelApplyStatusTest.cpp` | Table-driven test for every key (relayC1, relayL, relayC2, operate, bypass, tuning, antA, one_by_three, model, serial_num, ip, fwd, swr). Assert Q_PROPERTY notifications fire. |
| `tests/LanDiscoveryRegexTest.cpp` | Feed sample announcement strings; assert correct model/ip/v/serial/nick parsed; assert malformed lines are rejected; assert duplicate-serial dedup. |
| `tests/SMeterWidgetScaleTest.cpp` | Call `setPowerScale` with the three brackets; assert internal `m_powerScaleMax` / `m_powerRedStart` match the AetherSDR table; assert needle target dBm-to-fraction mapping is preserved across S0..S9+60. |
| `tests/SMeterWidgetPeakHoldTest.cpp` | Set decay rate; feed levels; advance time; assert peak decays at the expected dB/s; assert reset zeroes the peak. |
| `tests/SMeterWidgetContextMenuTest.cpp` | Trigger `contextMenuEvent` programmatically; assert the menu has the expected structure (TX Mode / RX Mode / Peak Hold submenus); fire each action and assert it routes to the correct setter and updates the corresponding AppSettings key. |
| `tests/WdspEngineMaxBinTest.cpp` | Spin up a WdspEngine with a mock display channel, call `setupMaxBinDetector` with the Thetis defaults, feed FFT bins through the analyzer, assert `getMaxBinDbm` returns the expected dBm; assert the smoothing tau decays correctly (10*log10 per Thetis `dmb_max_dB` update rule); assert `run=0` stops emission. |
| `tests/PgxlConnectionPairingTest.cpp` | Synthesize the connect-time sequence: `amplifier create` + `flexradio ampslice=A serial=... txant=ANT1 ptt=LAN active=1` + `keepalive enable`. Assert the exact frames emitted. Feed back `R|0|...` (success) and `R|<nonzero>|<err>` (failure) and assert `pairingResult(true/false, ...)` fires correctly. |
| `tests/PgxlConnectionKeepaliveTest.cpp` | Set `PGXL_KeepaliveSec=1` for the test; verify a status frame is emitted at the 1 s mark; simulate 3 missed responses; assert reconnect timer starts. |
| `tests/PgxlConnectionReconnectTest.cpp` | Force `disconnected()` 7 times; assert reconnect schedule honored (1, 2, 5, 10, 30, 60, 60 s). Assert `reconnectAttempt(N, backoffMs)` signals fire with the expected sequence. |
| `tests/PgxlConnectionPingTest.cpp` | Issue `ping("test1")`; feed back `R|0|` after a synthetic delay; assert `pongReceived(seq, ~delayMs, "test1")` fires; assert `pingTimedOut(seq)` fires after the 5 s timeout. |
| `tests/PgxlConnectionSetupTest.cpp` | Issue `readSetup`; feed back the kv response; assert `setupResponse` map contains the expected fields. Issue `writeSetup({{nickname,"X"}})`; assert correct frame emitted. |
| `tests/PgxlConnectionIfconfTest.cpp` | Same shape as setup test, for `ifconf read` / `ifconf address=`. |
| `tests/PgxlConnectionSaveTest.cpp` | Issue `save`; feed back `R|0|saving` + `S|state=REBOOT`; assert `saveAcknowledged` fires; assert the connection transitions to "rebooting"; assert the 20 s reconnect timer starts. |
| `tests/ConnectionDiagnosticsTest.cpp` | Bind to a mock PgxlConnection; simulate frames in/out; advance time; assert uptime, last RTT, byte/frame counts, last-frame age update correctly; assert the 1 Hz coalesce timer. |
| `tests/FaultLogTest.cpp` | Capture 12 fault events; assert the ring buffer keeps newest 10; assert JSON serialization round-trips correctly; assert `likelyCause` heuristic matches the four cases. |
| `tests/TuneMemoryStoreTest.cpp` | Store memory for (Ant 1, 20m); recall returns the same values. Store for a different slot; recall the first slot is unchanged. Clear; recall returns std::nullopt. Persist + reload from AppSettings. |
| `tests/TxInterlockPolicyTest.cpp` | All three modes; verify Disabled always allows; Warn always allows + emits `warned`; Block denies when amp absent or not in OPERATE; grace period suppresses the policy for `graceMs` after PTT; SWR gate kicks in when enabled. |
| `tests/AmpAppletContextMenuTest.cpp` | Programmatically trigger `contextMenuEvent`; assert menu has "Open PGXL Advanced..." action; fire it and assert it emits the navigation signal with `"pgxlAdvanced"` key. |
| `tests/TunerAppletContextMenuTest.cpp` | Same shape; assert "Save current tune memory" action calls `TuneMemoryStore::store(...)` with the current band+antenna. |

### 10.2 Bench verification

Bench matrix at `docs/architecture/phase-pgxl-tgxl-smeter-verification/README.md` (created with the implementation PR).
Rows cover both ANAN-G2 and HL2 (HL2 row gated on the open ATT/filter audit, matching the
3R precedent).

| Row | Scenario |
|---|---|
| 1 | Scan LAN happy path: PGXL + TGXL on subnet; Scan finds both within 3 s. |
| 2 | Scan LAN empty subnet: dialog reports "no devices found" cleanly. |
| 3 | Manual IP, correct host + port: Connect succeeds; Peripherals row turns green; AmpApplet / TunerApplet populate. |
| 4 | Manual IP, wrong port: Connect fails; row status shows "Error: Connection refused". |
| 5 | Manual IP, wrong host: Connect fails after socket timeout; row status shows "Error: Network unreachable" or similar. |
| 6 | PGXL telemetry live: keying the radio shows the AmpApplet FwdPower/SWR/Temp gauges moving; OPERATE button green. |
| 7 | TGXL telemetry live: TUNE button starts tune cycle; relay bars move; post-tune SWR captured into button. |
| 8 | OPERATE / BYPASS / STANDBY cycle on TGXL via single cycle button: state changes round-trip through TunerModel and persist on the device. |
| 9 | Manual relay step via mousewheel: each wheel tick advances the relay by 1 in the indicated direction. |
| 10 | Antenna 1/2/3 switch on TGXL 3x1: clicking each button activates the corresponding antenna on-device and updates the green-highlight on the UI. |
| 11 | S-Meter PGXL-aware scaling: with PGXL absent, scale is 120 W barefoot; with PGXL in STANDBY, scale stays barefoot and feed comes from radio FWDPWR; with PGXL in OPERATE, scale snaps to 2 kW and feed switches to PGXL peakfwd. |
| 12 | S-Meter peak hold: Fast/Medium/Slow decay rates produce the expected dB/s falloff; Reset zeroes the peak. |
| 13 | Auto-connect on launch: with Manual IPs configured, launching NereusSDR auto-connects to PGXL and TGXL within ~2 seconds of `MainWindow::onRadioConnected`. |
| 14 | TX mode / RX mode (Signal / Signal Peak / Max Bin) and peak-hold settings persist across app restart. |
| 15 | Right-click context menu: right-click on S-Meter shows the four submenus; current TX/RX/Decay selections are ticked; firing an action updates the widget behavior and the AppSettings key in one step. |
| 16 | Max Bin RX mode: tune to a slice with a clearly-strongest carrier inside the passband; switch RX mode to Max Bin; needle tracks the carrier level (often higher than the SMeter reading). Changing the filter low/high cut updates the detector window within ~100 ms; the reading follows the new window. |
| 17 | Max Bin smoothing: when the strongest carrier is interrupted, the reading decays at the Thetis tau=0.5s rate, not instantly. |
| 18 | HL2 row: same as rows 6, 7, 11, 16 but on HL2. Gated on HL2 ATT/filter audit closure. |
| 19 | PGXL pairing: `flexradio ampslice=A serial=NereusSDR-... txant=ANT1 ptt=LAN active=1` is sent on connect. Assert PGXL response (accept or reject) is logged and reflected in the Peripherals row status string. |
| 20 | PGXL keepalive: simulate a network blip (unplug ethernet for 10 s); observe NereusSDR detects within `3 * PGXL_KeepaliveSec`, marks disconnected, and auto-reconnects when network returns. |
| 21 | PGXL ping RTT: live PGXL connection shows non-zero RTT in PGXL Advanced &rarr; Diagnostics; pull the cable and observe `pingTimedOut` increments `keepaliveMissed`. |
| 22 | PGXL save &amp; reboot: trigger Save &amp; Reboot from the Advanced page; confirm the modal; observe the connection drops, the amp reboots, NereusSDR auto-reconnects within 30 s, and the pairing flow re-runs cleanly. |
| 23 | PGXL bias mode / fan mode / LED / nickname edits: change each in PGXL Advanced, click Save &amp; Reboot, observe the new values persist post-reboot via `readSetup`. |
| 24 | PGXL `ifconf address` write: change the PGXL static IP from `.43` to `.44`, click Save &amp; Reboot, observe NereusSDR uses Scan LAN to rediscover and reconnect. |
| 25 | PGXL fault history: drive the amp into a synthetic FAULT (high-SWR test); observe a row appended to Fault history with correct `likelyCause`; Clear All works; entries persist across app restart. |
| 26 | PGXL TX power cap: set cap to 1000 W; key the radio at >1000 W; toast appears; TX continues (soft alert only). |
| 27 | TX interlock Disabled: out-of-box default; TX proceeds regardless of PGXL state. |
| 28 | TX interlock Warn: PGXL in STANDBY; key the radio; toast appears; TX proceeds. |
| 29 | TX interlock Block: PGXL in STANDBY; key the radio; toast appears; TX is denied; MOX does not engage. |
| 30 | TX interlock grace period: set to 5000 ms; key the radio during the grace window; policy does not fire even if PGXL not in OPERATE. |
| 31 | TX interlock SWR gate: enable, max=2.0; key into a high-SWR load; policy denies (Block) or warns (Warn) accordingly. |
| 32 | TGXL antenna labels: set ANT 1 label to "80 m dipole"; observe TunerApplet button text updates; persists across restart. |
| 33 | TGXL tune memory save: tune up on 20 m / ANT 1; click "Save current tune memory" in the TunerApplet right-click; observe TuneMemoryStore entry; restart NereusSDR and observe persistence. |
| 34 | TGXL tune memory auto-recall: enable `TGXL_AutoTuneMemoryRecall`; switch from 20 m to 40 m and back; observe the stored 20 m relay positions get restored (or a fresh tune triggers if absolute-write isn't supported). |
| 35 | Right-click AmpApplet &rarr; "Open PGXL Advanced..." navigates to the Setup dialog at the right page. Same for TunerApplet &rarr; "Open TGXL Advanced...". |
| 36 | Right-click on either applet &rarr; "Copy diagnostics" puts a JSON blob on the clipboard with all the ConnectionDiagnostics fields. |

---

## 11. Out of scope

* OpenHPSDR-native amp/tuner abstraction layer (MASTER-PLAN.md:1067; future
  epic).
* Per-MAC scoping of PGXL/TGXL settings.
* PGXL chip in the bottom status bar; AetherSDR has none.
* Antenna Genius (AG) integration; same protocol family, separate epic.
* SO2R PGXL configuration (3WAY variant); deferred.
* Recording / scope / spectrum integration of PGXL telemetry beyond the
  S-Meter feed switch.
* Replacing NereusSDR's MeterWidget/MeterItem system with SMeterWidget for
  Container #1 and elsewhere; only the AppletPanelWidget header slot is
  swapped.
* **CAT-serial pairing path** (`catradio` command verb); requires NereusSDR
  rigctld which is Phase 3K. PGXL operators who need band/PTT via CAT
  serial can configure the PGXL front panel directly. NereusSDR's pairing
  attempt is `flexradio`-only this epic.
* **PGXL `message` verb**; debug-only, no operator-facing value.
* **Discovery of Antenna Genius devices** even though `LanDiscovery` could
  catch them on UDP 9007; AG support is its own epic.

---

## 12. Open follow-ups

* If a Wireshark capture of `OPS Manager` is obtained, decode the active
  `OPS Manager probe` packet (if any) for a faster non-passive discovery
  mode. The current 3-second passive listen window relies on the device's
  own broadcast cadence; an active probe could cut latency to one round
  trip.
* Other Thetis-native meter modes are easy adds once the WDSP plumbing for
  Max Bin lands: `Sig Avg` (long-time-average S-meter), `ADC L` / `ADC R`
  (raw ADC level for hardware level-set), `ADC2 L` / `ADC2 R` (second-ADC
  variants for diversity / dual-input boards). Each would be one new
  RxMode enum entry and one new WdspEngine wrapper. Scoped out of this
  epic to keep it focused on PGXL/TGXL plus the operator-requested Max Bin.
* The S-Meter port replaces only the fixed-header S-Meter. The composite
  MeterWidget/MeterItem system stays for Container #1 power meters, ALC,
  etc. If a future epic ports more AetherSDR-style needle gauges, the
  shared painting code in `SMeterWidget.cpp` could be refactored into a
  reusable `NeedleGauge` primitive.
* `LanDiscovery` is generic enough to also catch Antenna Genius
  announcements on UDP 9007 (the AG announcement port per AetherSDR's
  `AntennaGeniusModel`). Wiring AG support is a separate epic; the class
  shape should not preclude it.

---

## 13. Master-plan placement

This epic does not have a `3X` phase number yet. Suggest adding to
`docs/MASTER-PLAN.md` as **Phase 3P-II: External RF accessories (PGXL +
TGXL) + analog S-Meter port (with Thetis Max Bin / Sig Avg)**, slotting
after the 3M-3 TX-processing tail and the 3J-2 / 3R closeout, before
3M-2 CW TX. Two upstreams (AetherSDR for the PGXL/TGXL baseline + analog
needle widget; Thetis for Max Bin + Sig Avg) drive the phase; both
attribution chains are preserved per section 2. The FlexRadio API wiki
is the reference for the Tier 2-4 NereusSDR-native command additions; not
an attribution source, because the wiki is documentation rather than
source code, but cited as the spec-of-truth for every new verb.

Estimated scope: **~3000 LOC of production code plus ~30 new unit tests
and ~20 new bench-matrix rows**. Single PR per the operator's decision;
expect the branch to live for 2-3 weeks of iteration before merge. CI
must stay green across every push so the diff doesn't accumulate
unrelated regressions.

---

End of design.
