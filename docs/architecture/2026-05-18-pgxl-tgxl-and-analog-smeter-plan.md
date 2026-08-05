# PGXL/TGXL Network Integration + Analog S-Meter Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire NereusSDR to FlexRadio/4O3A PGXL amplifier and TGXL tuner over Ethernet, with PGXL-aware analog S-Meter (Sig Avg + Max Bin ported from Thetis), full FlexRadio Ethernet API command surface (pairing + keepalive + ping + reconnect + save + ifconf), and a Setup `->` PGXL/TGXL Advanced device-config UI.

**Architecture:** AetherSDR 1:1 baseline (`PgxlConnection`, `TgxlConnection`, `TunerModel`, `AmpApplet`, `TunerApplet`, `RelayBar`, `SMeterWidget`) plus NereusSDR-native extensions for the FlexRadio API verbs AetherSDR skips, plus four new helper classes (`LanDiscovery`, `ConnectionDiagnostics`, `FaultLog`, `TuneMemoryStore`, `TxInterlockPolicy`). All main-thread Qt6/C++20 with QTcpSocket / QUdpSocket. Two Thetis S-Meter RX modes (Sig Avg via `GetRXAMeter(RXA_S_AV)`, Max Bin via `SetupDetectMaxBin` / `GetDetectMaxBin`) added through `WdspEngine` wrappers.

**Tech Stack:** Qt6 (`QTcpSocket`, `QUdpSocket`, `QTimer`, `QWidget` custom painting), C++20, WDSP (Thetis), AetherSDR upstream at `../AetherSDR/`, Thetis upstream at `../Thetis/` (version `v2.10.3.13-7-g501e3f51`), CMake/CTest, GPG-signed commits.

**Spec:** `docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md`. Re-read it before starting any task; the plan is bite-sized execution, not a re-derivation. Inline cite stamps use `[@501e3f5]` for Thetis (between releases) and `[@<aether_sha>]` for AetherSDR (capture at task time via `git -C ../AetherSDR rev-parse --short HEAD`).

**Attribution:** Every new file carries the verbatim AetherSDR or Thetis header per `docs/attribution/HOW-TO-PORT.md`. The `docs/attribution/aethersdr-contributor-index.md` entries at lines 229 + 247 (currently `(none yet)`) get updated to point at the new NereusSDR paths in the same commit that creates the ported file.

**Build commands:**
- Configure: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`
- Build: `cmake --build build -j$(nproc)`
- Test: `ctest --test-dir build --output-on-failure`
- Run: `./build/NereusSDR`

---

## File Structure

### New files (Phase 1: baseline)

| Path | Responsibility |
|---|---|
| `src/core/PgxlConnection.h` / `.cpp` | TCP client for PGXL on port 9008. Ports AetherSDR. |
| `src/core/TgxlConnection.h` / `.cpp` | TCP client for TGXL on port 9010. Ports AetherSDR. |
| `src/core/LanDiscovery.h` / `.cpp` | UDP listener on ports 9008 + 9010 for 4O3A announcements. NereusSDR-native. |
| `src/models/TunerModel.h` / `.cpp` | TGXL state model. Ports AetherSDR. |
| `src/gui/RelayBar.h` / `.cpp` | 0..255 relay bar widget with mousewheel step. Ports AetherSDR (inner class). |
| `src/gui/applets/AmpApplet.h` / `.cpp` | PGXL telemetry applet. Ports AetherSDR. |
| `src/gui/LanScanDialog.h` / `.cpp` | Modeless dialog wrapping `LanDiscovery` for the Scan LAN button. |
| `tests/PgxlConnectionParseTest.cpp` | Unit test for PGXL frame parsing. |
| `tests/TgxlConnectionParseTest.cpp` | Unit test for TGXL frame parsing. |
| `tests/TunerModelApplyStatusTest.cpp` | Unit test for TGXL kv-map application. |
| `tests/LanDiscoveryRegexTest.cpp` | Unit test for UDP discovery regex. |

### Modified files (Phase 1: baseline)

| Path | Change |
|---|---|
| `src/gui/applets/TunerApplet.h` / `.cpp` | Rewire: drop NyiOverlays, replace `QProgressBar` with `RelayBar`, replace 3-button group with single cycle button, wire to `TunerModel` + `TgxlConnection`. |
| `src/gui/setup/CatNetworkSetupPages.h` / `.cpp` | Add `PeripheralsPage` class for Setup `->` Network `->` Peripherals. |
| `src/models/RadioModel.h` / `.cpp` | Own `PgxlConnection`, `TgxlConnection`, `TunerModel`, `LanDiscovery`; expose `hasAmplifier()`, `ampOperate()`, `ampMetersChanged` signal. |
| `src/gui/MainWindow.h` / `.cpp` | Wire `onRadioConnected` auto-connect, build bottom-status TGXL chip, route AmpApplet `operateToggled` to PgxlConnection. |
| `src/gui/applets/AppletPanelWidget.h` / `.cpp` | Confirm AmpApplet placement in right-column container. |
| `CMakeLists.txt` | Register new source files + tests. |
| `docs/attribution/aethersdr-contributor-index.md` | Update lines 229 + 247 with new NereusSDR paths. |

### New files (Phase 2: S-Meter port)

| Path | Responsibility |
|---|---|
| `src/gui/SMeterWidget.h` / `.cpp` | Analog needle gauge with right-click context menu. Ports AetherSDR (134+701 LOC). |
| `tests/SMeterWidgetScaleTest.cpp` | Unit test for scale + setPowerScale. |
| `tests/SMeterWidgetPeakHoldTest.cpp` | Unit test for peak hold + decay. |
| `tests/SMeterWidgetContextMenuTest.cpp` | Unit test for right-click menu structure + action routing. |
| `tests/WdspEngineMaxBinTest.cpp` | Unit test for Max Bin detector wrappers. |

### Modified files (Phase 2)

| Path | Change |
|---|---|
| `src/core/wdsp/WdspEngine.h` / `.cpp` | Add `setupMaxBinDetector`, `getMaxBinDbm`, expose `getRxaMeter(channel, RXA_S_AV)` for Sig Avg. |
| `src/gui/applets/AppletPanelWidget.cpp` | Swap composite header S-Meter for `SMeterWidget`. Remove old inline settings strip. |
| `src/gui/MainWindow.cpp` | Wire standby-aware S-Meter feed switch (`ampOperate()` gate). |
| `src/models/SliceModel.cpp` | Emit `filterChanged` and `frequencyChanged` with rate-limited Max Bin reconfigure. |
| `src/gui/meters/MeterPoller.cpp` | Switch RX feed based on `SMeterWidget::rxMode()`. |

### New files (Phase 3: connection robustness)

| Path | Responsibility |
|---|---|
| `src/core/ConnectionDiagnostics.h` / `.cpp` | Per-device runtime metrics; 1 Hz coalesce. |
| `tests/PgxlConnectionPairingTest.cpp` | amplifier-create + flexradio-pair frame format + accept/reject. |
| `tests/PgxlConnectionKeepaliveTest.cpp` | keepalive timer + missed-pings detection. |
| `tests/PgxlConnectionReconnectTest.cpp` | Backoff schedule. |
| `tests/PgxlConnectionPingTest.cpp` | RTT measurement + timeout. |
| `tests/ConnectionDiagnosticsTest.cpp` | Metric aggregation + 1 Hz coalesce. |

### Modified files (Phase 3)

| Path | Change |
|---|---|
| `src/core/PgxlConnection.h` / `.cpp` | Add Tier 2 slots: amplifierCreate, flexradioPair, enableKeepalive, ping, interlockCreate, interlockDisable, readSetup, writeSetup, readIfconf, writeIfconf, save, setBand. Auto-reconnect timer + backoff. |
| `src/core/TgxlConnection.h` / `.cpp` | Parallel additions: enableKeepalive, ping, readSetup, writeSetup, readIfconf, writeIfconf, save. |
| `src/models/RadioModel.cpp` | Run pairing flow on PGXL `connected()`. Capture amp operate state + fault transitions. |
| `src/models/SliceModel.cpp` | Emit `bandChanged(Band)` and wire to PgxlConnection.setBand. |

### New files (Phase 4: Advanced UI + UX wins)

| Path | Responsibility |
|---|---|
| `src/core/FaultLog.h` / `.cpp` | Ring buffer of last 10 fault events, JSON-persisted. |
| `src/core/TuneMemoryStore.h` / `.cpp` | Per-(antenna, band) relay-position cache. |
| `src/core/TxInterlockPolicy.h` / `.cpp` | Disabled/Warn/Block TX gate policy. |
| `src/gui/setup/PgxlAdvancedPage.h` / `.cpp` | Setup `->` Network `->` PGXL Advanced. |
| `src/gui/setup/TgxlAdvancedPage.h` / `.cpp` | Setup `->` Network `->` TGXL Advanced. |
| `src/gui/setup/PgxlInterlockPage.h` / `.cpp` | Setup `->` Transmit `->` PGXL Interlock. |
| `src/gui/PgxlSaveRebootDialog.h` / `.cpp` | Save & Reboot confirmation modal. |
| `tests/FaultLogTest.cpp` | Ring buffer + JSON + likelyCause heuristic. |
| `tests/TuneMemoryStoreTest.cpp` | Recall/store/clear + persistence. |
| `tests/TxInterlockPolicyTest.cpp` | Three modes + grace period + SWR gate. |
| `tests/AmpAppletContextMenuTest.cpp` | Right-click navigation. |
| `tests/TunerAppletContextMenuTest.cpp` | Right-click navigation + memory shortcuts. |
| `tests/PgxlConnectionSetupTest.cpp` | readSetup / writeSetup. |
| `tests/PgxlConnectionIfconfTest.cpp` | readIfconf / writeIfconf. |
| `tests/PgxlConnectionSaveTest.cpp` | save + reboot lifecycle. |

### Modified files (Phase 4)

| Path | Change |
|---|---|
| `src/gui/applets/AmpApplet.h` / `.cpp` | Add `contextMenuEvent` for right-click menu. |
| `src/gui/applets/TunerApplet.h` / `.cpp` | Add `contextMenuEvent` + memory-row UI. |
| `src/gui/MainWindow.h` / `.cpp` | Add `openSetup(QString pageKey)` API for applet navigation. Add power-cap toast. |
| `src/core/MoxController.cpp` | Hook `TxInterlockPolicy::evaluateTxRequest` into `onTxRequested`. |
| `src/gui/setup/TransmitSetupPages.cpp` | Register PGXL Interlock sub-page. |
| `src/gui/setup/SetupDialog.cpp` | Register PGXL Advanced and TGXL Advanced; show conditionally on device presence. |
| `docs/MASTER-PLAN.md` | Add Phase 3P-II entry. |
| `CHANGELOG.md` | Add Phase 3P-II section. |

### Bench-verification artifacts

| Path | Responsibility |
|---|---|
| `docs/architecture/phase-pgxl-tgxl-smeter-verification/README.md` | 36-row bench matrix from spec §10.2. |

---

## Implementation Order

Four phases, executed in sequence within one PR. Each phase ends with green `ctest` and a coherent demo:

1. **Phase 1** ends with: PGXL telemetry visible in AmpApplet; TGXL relay control + auto-tune working; Peripherals page lets operator type IP and connect; LAN scan finds devices.
2. **Phase 2** ends with: analog S-Meter widget in the AppletPanelWidget header; right-click menu picks TX/RX mode + peak hold; Max Bin tracks the strongest carrier; Sig Avg gives averaged S-meter; PGXL-aware scale snaps to 2 kW when amp is OPERATE.
3. **Phase 3** ends with: PGXL pairing attempt on connect with graceful fallback; keepalive + ping live; auto-reconnect handles network blips; band notify on slice retune; Diagnostics shows live RTT / frame counts.
4. **Phase 4** ends with: full Setup `->` PGXL Advanced + TGXL Advanced device-config UI; right-click context menus on applets navigate to Advanced pages; fault history persists; tune memory recalls per band/antenna; optional TX interlock available.

Within each phase: classes before UI before integration. Test-first where the logic is exercisable in isolation (parsers, models, policies). UI tasks use build-and-run smoke checks since custom-painted widgets don't lend themselves to pure unit tests.

---

# Phase 1: PGXL/TGXL Baseline

Estimated: 30 tasks. Net new code: ~1200 LOC + 4 test files.

## Task 1: Capture AetherSDR upstream sha

**Files:**
- Read: `../AetherSDR/` (just `git rev-parse`)

- [ ] **Step 1: Capture the AetherSDR short sha for inline cites**

```bash
git -C ../AetherSDR rev-parse --short HEAD
```

Save the output (example: `abc1234`). Every inline cite in Phase 1-4 ports uses this sha as `[@abc1234]` unless that file was unchanged since a tag, in which case use the tag.

- [ ] **Step 2: Write the sha into a temporary scratch file for self-reference**

Create `.aether-sha` at repo root (gitignored):

```bash
echo "$(git -C ../AetherSDR rev-parse --short HEAD)" > .aether-sha
echo ".aether-sha" >> .gitignore
```

- [ ] **Step 3: Commit gitignore update only**

```bash
git add .gitignore
git commit -m "chore: gitignore .aether-sha scratch file for plan execution"
```

## Task 2: Add Phase 1 AppSettings keys

**Files:**
- Modify: `src/core/AppSettings.h` (default-value tables only, no logic change)
- Test: none (defaults are passive)

- [ ] **Step 1: Add Phase 1 keys to AppSettings header**

Append to the existing key documentation block in `src/core/AppSettings.h`:

```cpp
// PGXL/TGXL peripherals (Phase 3P-II baseline)
// Empty manualIp disables auto-connect; ports default per FlexRadio API
//   PGXL_ManualIp      string  ""     (default empty)
//   PGXL_ManualPort    int     9008
//   TGXL_ManualIp      string  ""
//   TGXL_ManualPort    int     9010
```

- [ ] **Step 2: Build to confirm no regression**

```bash
cmake --build build -j$(nproc)
```

Expected: build succeeds; no new warnings tied to this file.

- [ ] **Step 3: Commit**

```bash
git add src/core/AppSettings.h
git commit -m "docs(settings): document Phase 3P-II PGXL/TGXL Manual IP keys"
```

## Task 3: PgxlConnection header skeleton

**Files:**
- Create: `src/core/PgxlConnection.h`
- Test: none yet (header only)

- [ ] **Step 1: Create the header with verbatim AetherSDR attribution**

```cpp
// =================================================================
// src/core/PgxlConnection.h  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/core/PgxlConnection.{h,cpp} [@<sha>].
// =================================================================
#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QMap>
#include <QString>

namespace NereusSDR {

class PgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit PgxlConnection(QObject* parent = nullptr);

    bool    isConnected() const { return m_connected; }
    QString version()     const { return m_version; }
    QString peerAddress() const { return m_socket.peerAddress().toString(); }
    quint16 peerPort()    const { return m_socket.peerPort(); }

public slots:
    void connectToPgxl(const QString& host, quint16 port = 9008);
    void disconnect();
    quint32 sendCommand(const QString& cmd);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void statusUpdated(const QMap<QString, QString>& kvs);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError();
    void pollStatus();

private:
    void processLine(const QString& line);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    QString    m_version;
};

}  // namespace NereusSDR
```

- [ ] **Step 2: Build, expect "undefined reference" since .cpp doesn't exist yet**

```bash
cmake --build build -j$(nproc)
```

If the header is added to `CMakeLists.txt` AUTOMOC list, this builds the header alone. If not, defer the build verification to Task 4 when the .cpp lands.

- [ ] **Step 3: Stage but do not commit**

```bash
git add src/core/PgxlConnection.h
```

Commit happens at the end of Task 4 once the test passes.

## Task 4: PgxlConnection parse test (TDD red)

**Files:**
- Create: `tests/PgxlConnectionParseTest.cpp`
- Modify: `CMakeLists.txt` (register the new test)
- Test: `tests/PgxlConnectionParseTest.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// tests/PgxlConnectionParseTest.cpp
#include <QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionParseTest : public QObject {
    Q_OBJECT
private slots:
    void parsesVersionBanner();
    void parsesResponseFrameWithKvBody();
    void parsesUnsolicitedStatusPush();
};

void PgxlConnectionParseTest::parsesVersionBanner() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy connectedSpy(&conn, &NereusSDR::PgxlConnection::connected);

    // Inject a V frame by piping into the read buffer via a friend or
    // a test hook. The actual injection mechanism is implemented in Task 5.
    // For now, the test exists and fails to compile / link.
    QVERIFY(true);  // placeholder until injection wired in next task
}

void PgxlConnectionParseTest::parsesResponseFrameWithKvBody() {
    // Will be filled in Task 6 once the inject hook exists.
    QVERIFY(true);
}

void PgxlConnectionParseTest::parsesUnsolicitedStatusPush() {
    // Will be filled in Task 7 once the inject hook exists.
    QVERIFY(true);
}

QTEST_GUILESS_MAIN(PgxlConnectionParseTest)
#include "PgxlConnectionParseTest.moc"
```

- [ ] **Step 2: Register the test in CMakeLists.txt**

Add to the existing tests block:

```cmake
qt_add_executable(PgxlConnectionParseTest tests/PgxlConnectionParseTest.cpp)
target_link_libraries(PgxlConnectionParseTest PRIVATE NereusSDRLib Qt6::Test)
add_test(NAME PgxlConnectionParseTest COMMAND PgxlConnectionParseTest)
```

- [ ] **Step 3: Run the test, expect FAIL (link error)**

```bash
cmake --build build --target PgxlConnectionParseTest -j$(nproc)
```

Expected: link error referencing `PgxlConnection::PgxlConnection`. This is the red phase.

- [ ] **Step 4: Stage**

```bash
git add tests/PgxlConnectionParseTest.cpp CMakeLists.txt
```

## Task 5: PgxlConnection implementation (TDD green)

**Files:**
- Create: `src/core/PgxlConnection.cpp`
- Test: `tests/PgxlConnectionParseTest.cpp` (test hook added)

- [ ] **Step 1: Port AetherSDR's constructor + connect logic**

Open `../AetherSDR/src/core/PgxlConnection.cpp`. Port lines 1..130 verbatim with these Qt6/NereusSDR adjustments:

1. Wrap in `namespace NereusSDR { ... }`.
2. Replace `qCDebug(lcTuner)` with `qCDebug(lcPgxl)`. Declare `Q_LOGGING_CATEGORY(lcPgxl, "nereus.pgxl")` at the top of the .cpp.
3. Add the attribution header from Task 3 to the top of the .cpp too.
4. Add a friend declaration `friend class PgxlConnectionParseTest;` in the header, OR add a public `void injectFrameForTesting(const QByteArray& line)` test hook that calls `processLine` after appending a newline. Prefer the test hook approach to avoid build-config friend pollution.

Create `src/core/PgxlConnection.cpp`:

```cpp
// (attribution header from Task 3)
#include "PgxlConnection.h"
#include <QLoggingCategory>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcPgxl, "nereus.pgxl")

PgxlConnection::PgxlConnection(QObject* parent)
    : QObject(parent) {
    connect(&m_socket, &QTcpSocket::connected,    this, &PgxlConnection::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &PgxlConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,    this, &PgxlConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &PgxlConnection::onError);

    m_pollTimer.setInterval(200);  // 5 Hz per AetherSDR
    connect(&m_pollTimer, &QTimer::timeout, this, &PgxlConnection::pollStatus);
}

void PgxlConnection::connectToPgxl(const QString& host, quint16 port) {
    if (m_connected) disconnect();
    m_seq = 0;
    m_gotVersion = false;
    m_version.clear();
    m_readBuf.clear();
    qCDebug(lcPgxl) << "connecting to" << host << ":" << port;
    m_socket.connectToHost(host, port);
}

void PgxlConnection::disconnect() {
    m_pollTimer.stop();
    m_connected = false;
    m_socket.disconnectFromHost();
}

quint32 PgxlConnection::sendCommand(const QString& cmd) {
    quint32 seq = ++m_seq;
    QString line = QString("C%1|%2\n").arg(seq).arg(cmd);
    m_socket.write(line.toUtf8());
    qCDebug(lcPgxl) << "sent" << line.trimmed();
    return seq;
}

void PgxlConnection::onConnected() {
    qCDebug(lcPgxl) << "TCP connected";
}

void PgxlConnection::onDisconnected() {
    m_pollTimer.stop();
    m_connected = false;
    emit disconnected();
}

void PgxlConnection::onError() {
    QString err = m_socket.errorString();
    qCWarning(lcPgxl) << "socket error:" << err;
    emit connectionFailed(err);
}

void PgxlConnection::onReadyRead() {
    m_readBuf.append(m_socket.readAll());
    while (true) {
        int idx = m_readBuf.indexOf('\n');
        if (idx < 0) break;
        QString line = QString::fromUtf8(m_readBuf.left(idx)).trimmed();
        m_readBuf.remove(0, idx + 1);
        if (!line.isEmpty()) processLine(line);
    }
}

void PgxlConnection::pollStatus() {
    sendCommand("status");
}

void PgxlConnection::processLine(const QString& line) {
    // Implemented incrementally in Tasks 6, 7. Stub for now.
    qCDebug(lcPgxl) << "rx" << line;
}

}  // namespace NereusSDR
```

- [ ] **Step 2: Add a test injection hook to the header**

In `src/core/PgxlConnection.h`, inside the `public:` block, add:

```cpp
    // Test-only: feed a single line into processLine (no newline needed).
    // Used by PgxlConnectionParseTest. Production code never calls this.
    void injectLineForTesting(const QString& line) { processLine(line); }
```

- [ ] **Step 3: Build the test executable**

```bash
cmake --build build --target PgxlConnectionParseTest -j$(nproc)
```

Expected: builds clean.

- [ ] **Step 4: Run the test**

```bash
ctest --test-dir build -R PgxlConnectionParseTest --output-on-failure
```

Expected: PASS (the placeholders still pass).

- [ ] **Step 5: Commit Tasks 3+4+5 together**

```bash
git add src/core/PgxlConnection.h src/core/PgxlConnection.cpp tests/PgxlConnectionParseTest.cpp CMakeLists.txt
git commit -m "feat(pgxl): scaffold PgxlConnection class and parse test"
```

## Task 6: Parse version banner + R frames

**Files:**
- Modify: `src/core/PgxlConnection.cpp` (processLine)
- Modify: `tests/PgxlConnectionParseTest.cpp` (real assertions)

- [ ] **Step 1: Write the failing test for the version banner**

Replace `parsesVersionBanner()` in `tests/PgxlConnectionParseTest.cpp`:

```cpp
void PgxlConnectionParseTest::parsesVersionBanner() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy connectedSpy(&conn, &NereusSDR::PgxlConnection::connected);

    conn.injectLineForTesting("V3.8.9");

    QCOMPARE(conn.version(), QString("3.8.9"));
    QVERIFY(conn.isConnected());
    QCOMPARE(connectedSpy.count(), 1);
}
```

- [ ] **Step 2: Write the failing test for response frames**

Replace `parsesResponseFrameWithKvBody()`:

```cpp
void PgxlConnectionParseTest::parsesResponseFrameWithKvBody() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::PgxlConnection::statusUpdated);

    conn.injectLineForTesting("V3.8.9");  // need handshake first
    conn.injectLineForTesting("R1|0|state=OPERATE temp=42.5 vac=240 fwd=1480.0 swr=2.1");

    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("state"), QString("OPERATE"));
    QCOMPARE(kvs.value("temp"),  QString("42.5"));
    QCOMPARE(kvs.value("vac"),   QString("240"));
    QCOMPARE(kvs.value("fwd"),   QString("1480.0"));
    QCOMPARE(kvs.value("swr"),   QString("2.1"));
}
```

- [ ] **Step 3: Run the test, expect FAIL**

```bash
ctest --test-dir build -R PgxlConnectionParseTest --output-on-failure
```

Expected: fails because `processLine` is still a stub.

- [ ] **Step 4: Implement V and R parsing in processLine**

Replace `PgxlConnection::processLine` in `src/core/PgxlConnection.cpp` with the port from AetherSDR (the audit shows the full body):

```cpp
void PgxlConnection::processLine(const QString& line) {
    // Version: V3.8.9
    if (!m_gotVersion && line.startsWith('V')) {
        m_version = line.mid(1);
        m_gotVersion = true;
        qCInfo(lcPgxl) << "PGXL version" << m_version;
        sendCommand("info");
        sendCommand("status");
        m_connected = true;
        m_pollTimer.start();
        emit connected();
        return;
    }
    // Response: R<seq>|<hex>|<body>
    if (line.startsWith('R')) {
        int pipe1 = line.indexOf('|');
        int pipe2 = (pipe1 >= 0) ? line.indexOf('|', pipe1 + 1) : -1;
        if (pipe2 >= 0) {
            QString body = line.mid(pipe2 + 1).trimmed();
            if (!body.isEmpty()) {
                QMap<QString,QString> kvs;
                const auto parts = body.split(' ', Qt::SkipEmptyParts);
                for (const auto& p : parts) {
                    int eq = p.indexOf('=');
                    if (eq > 0) kvs.insert(p.left(eq), p.mid(eq + 1));
                }
                if (!kvs.isEmpty()) emit statusUpdated(kvs);
            }
        }
        return;
    }
    // S frames implemented in Task 7
}
```

- [ ] **Step 5: Run the test, expect PASS**

```bash
ctest --test-dir build -R PgxlConnectionParseTest --output-on-failure
```

Expected: PASS for the two implemented test cases.

- [ ] **Step 6: Commit**

```bash
git add src/core/PgxlConnection.cpp tests/PgxlConnectionParseTest.cpp
git commit -m "feat(pgxl): parse V banner and R frames with kv body"
```

## Task 7: Parse S frames (unsolicited status push)

**Files:**
- Modify: `src/core/PgxlConnection.cpp` (processLine)
- Modify: `tests/PgxlConnectionParseTest.cpp` (real assertion)

- [ ] **Step 1: Write the failing test**

Replace `parsesUnsolicitedStatusPush()`:

```cpp
void PgxlConnectionParseTest::parsesUnsolicitedStatusPush() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::PgxlConnection::statusUpdated);

    conn.injectLineForTesting("V3.8.9");
    conn.injectLineForTesting("S0|state=FAULT fwd=1820.0 swr=2.85 temp=78.0");

    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("state"), QString("FAULT"));
    QCOMPARE(kvs.value("swr"),   QString("2.85"));
}
```

- [ ] **Step 2: Run, expect FAIL**

```bash
ctest --test-dir build -R PgxlConnectionParseTest --output-on-failure
```

- [ ] **Step 3: Implement S parsing**

Append in `processLine` before the final closing brace:

```cpp
    // Status push: S0|state ... or S0|temp=... etc.
    if (line.startsWith('S')) {
        int pipe = line.indexOf('|');
        if (pipe < 0) return;
        QString rest = line.mid(pipe + 1);
        int firstEq = rest.indexOf('=');
        if (firstEq < 0) return;
        int lastSpaceBeforeEq = rest.lastIndexOf(' ', firstEq);
        QString kvString = (lastSpaceBeforeEq >= 0)
            ? rest.mid(lastSpaceBeforeEq + 1) : rest;
        QMap<QString,QString> kvs;
        const auto parts = kvString.split(' ', Qt::SkipEmptyParts);
        for (const auto& p : parts) {
            int eq = p.indexOf('=');
            if (eq > 0) kvs.insert(p.left(eq), p.mid(eq + 1));
        }
        if (!kvs.isEmpty()) emit statusUpdated(kvs);
    }
```

- [ ] **Step 4: Run, expect PASS**

```bash
ctest --test-dir build -R PgxlConnectionParseTest --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/core/PgxlConnection.cpp tests/PgxlConnectionParseTest.cpp
git commit -m "feat(pgxl): parse S unsolicited status push frames"
```

## Task 8: TgxlConnection header + skeleton

**Files:**
- Create: `src/core/TgxlConnection.h`
- Create: `src/core/TgxlConnection.cpp`

- [ ] **Step 1: Create the header**

Same shape as PgxlConnection with additional public API:

```cpp
// (attribution header)
#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QMap>

namespace NereusSDR {

class TgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit TgxlConnection(QObject* parent = nullptr);

    bool    isConnected() const { return m_connected; }
    QString version()     const { return m_version; }
    QString peerAddress() const { return m_socket.peerAddress().toString(); }
    quint16 peerPort()    const { return m_socket.peerPort(); }

public slots:
    void connectToTgxl(const QString& host, quint16 port = 9010);
    void disconnect();
    void adjustRelay(int relay, int direction);
    quint32 sendCommand(const QString& cmd);

    // Test injection
    void injectLineForTesting(const QString& line) { processLine(line); }

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void stateUpdated(const QMap<QString, QString>& kvs);
    void statusUpdated(const QMap<QString, QString>& kvs);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError();
    void pollStatus();

private:
    void processLine(const QString& line);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    QString    m_version;
};

}  // namespace NereusSDR
```

- [ ] **Step 2: Create the .cpp with the constructor + connect/disconnect/adjustRelay/sendCommand**

Port from `../AetherSDR/src/core/TgxlConnection.cpp` lines 1..170 with the same namespace + logging-category adjustments as PgxlConnection. `m_pollTimer.setInterval(1000)` (1 Hz). adjustRelay sends `tune relay=N move=+/-1`.

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc)
```

Expected: no errors.

- [ ] **Step 4: Stage but do not commit (test in Task 9)**

```bash
git add src/core/TgxlConnection.h src/core/TgxlConnection.cpp
```

## Task 9: TgxlConnection parse test

**Files:**
- Create: `tests/TgxlConnectionParseTest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the test**

```cpp
#include <QtTest>
#include "core/TgxlConnection.h"

class TgxlConnectionParseTest : public QObject {
    Q_OBJECT
private slots:
    void parsesVersionBanner();
    void parsesStateFrameUnsolicited();
    void parsesStatusPollResponse();
    void adjustRelayBuildsCorrectFrame();
};

void TgxlConnectionParseTest::parsesVersionBanner() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy connectedSpy(&conn, &NereusSDR::TgxlConnection::connected);
    conn.injectLineForTesting("V1.2.17");
    QCOMPARE(conn.version(), QString("1.2.17"));
    QVERIFY(conn.isConnected());
    QCOMPARE(connectedSpy.count(), 1);
}

void TgxlConnectionParseTest::parsesStateFrameUnsolicited() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy stateSpy(&conn, &NereusSDR::TgxlConnection::stateUpdated);
    conn.injectLineForTesting("V1.2.17");
    conn.injectLineForTesting("S0|state relayC1=42 relayL=199 relayC2=88 operate=1 bypass=0");
    QCOMPARE(stateSpy.count(), 1);
    auto kvs = stateSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("relayC1"), QString("42"));
    QCOMPARE(kvs.value("relayL"),  QString("199"));
    QCOMPARE(kvs.value("operate"), QString("1"));
}

void TgxlConnectionParseTest::parsesStatusPollResponse() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::TgxlConnection::statusUpdated);
    conn.injectLineForTesting("V1.2.17");
    conn.injectLineForTesting("S1|status fwd=12.5 swr=1.1 tuning=0");
    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("fwd"), QString("12.5"));
}

void TgxlConnectionParseTest::adjustRelayBuildsCorrectFrame() {
    // Verify the sendCommand string contains "tune relay=1 move=-1"
    // by attaching a QSignalSpy-like sniffer; the simplest test is to
    // pump the socket through a local QTcpServer. Implementation deferred
    // to a test infrastructure helper; for now just compile-test:
    NereusSDR::TgxlConnection conn;
    conn.adjustRelay(1, -1);  // should be a no-op when not connected
    QVERIFY(!conn.isConnected());
}

QTEST_GUILESS_MAIN(TgxlConnectionParseTest)
#include "TgxlConnectionParseTest.moc"
```

- [ ] **Step 2: Register in CMakeLists.txt + build + run**

```cmake
qt_add_executable(TgxlConnectionParseTest tests/TgxlConnectionParseTest.cpp)
target_link_libraries(TgxlConnectionParseTest PRIVATE NereusSDRLib Qt6::Test)
add_test(NAME TgxlConnectionParseTest COMMAND TgxlConnectionParseTest)
```

- [ ] **Step 3: Run, expect PASS (port already has the parsing)**

```bash
cmake --build build --target TgxlConnectionParseTest -j$(nproc)
ctest --test-dir build -R TgxlConnectionParseTest --output-on-failure
```

If FAIL: the `processLine` port from AetherSDR is incomplete; cross-check against `../AetherSDR/src/core/TgxlConnection.cpp` (especially the S-frame "state" vs "status" object discriminator).

- [ ] **Step 4: Commit Task 8 + 9 together**

```bash
git add src/core/TgxlConnection.h src/core/TgxlConnection.cpp tests/TgxlConnectionParseTest.cpp CMakeLists.txt
git commit -m "feat(tgxl): port TgxlConnection from AetherSDR with parse tests"
```

## Task 10: TunerModel header + skeleton

**Files:**
- Create: `src/models/TunerModel.h`
- Create: `src/models/TunerModel.cpp`

- [ ] **Step 1: Create header**

Port from `../AetherSDR/src/models/TunerModel.h`. The full Q_PROPERTY list in spec section 4.3. Key public surface:

```cpp
// (attribution header)
#pragma once
#include <QObject>
#include <QMap>

namespace NereusSDR {

class TgxlConnection;

class TunerModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int  relayC1 READ relayC1 NOTIFY relayChanged)
    Q_PROPERTY(int  relayL  READ relayL  NOTIFY relayChanged)
    Q_PROPERTY(int  relayC2 READ relayC2 NOTIFY relayChanged)
    Q_PROPERTY(bool isOperate READ isOperate NOTIFY stateChanged)
    Q_PROPERTY(bool isBypass  READ isBypass  NOTIFY stateChanged)
    Q_PROPERTY(bool isTuning  READ isTuning  NOTIFY tuningChanged)
    Q_PROPERTY(int  antennaA  READ antennaA  NOTIFY antennaAChanged)
    Q_PROPERTY(bool hasAntennaSwitch READ hasAntennaSwitch NOTIFY stateChanged)
    Q_PROPERTY(bool isPresent READ isPresent NOTIFY presenceChanged)
    Q_PROPERTY(bool hasDirectConnection READ hasDirectConnection NOTIFY directConnectionChanged)
    Q_PROPERTY(QString tgxlIp READ tgxlIp NOTIFY stateChanged)
    Q_PROPERTY(float fwdPower READ fwdPower NOTIFY metersChanged)
    Q_PROPERTY(float swr      READ swr      NOTIFY metersChanged)

public:
    explicit TunerModel(QObject* parent = nullptr);

    int  relayC1() const { return m_relayC1; }
    int  relayL()  const { return m_relayL;  }
    int  relayC2() const { return m_relayC2; }
    bool isOperate() const { return m_operate; }
    bool isBypass()  const { return m_bypass; }
    bool isTuning()  const { return m_tuning; }
    int  antennaA()  const { return m_antA;   }
    bool hasAntennaSwitch() const { return m_oneByThree; }
    bool isPresent() const { return m_present; }
    bool hasDirectConnection() const;
    QString tgxlIp() const { return m_ip; }
    float fwdPower() const { return m_fwd; }
    float swr()      const { return m_swr; }

    void bindConnection(TgxlConnection* conn);
    void applyStatus(const QMap<QString,QString>& kvs);

public slots:
    void autoTune();
    void adjustRelay(int relay, int dir);
    void setAntennaA(int antA);
    void setOperate(bool on);
    void setBypass(bool on);

signals:
    void relayChanged();
    void stateChanged();
    void tuningChanged(bool tuning);
    void antennaAChanged(int antA);
    void presenceChanged(bool present);
    void directConnectionChanged();
    void metersChanged(float fwd, float swr);

private:
    TgxlConnection* m_conn{nullptr};
    int  m_relayC1{0}, m_relayL{0}, m_relayC2{0};
    bool m_operate{false}, m_bypass{false}, m_tuning{false};
    int  m_antA{0};
    bool m_oneByThree{false};
    bool m_present{false};
    QString m_ip;
    QString m_serial;
    QString m_model;
    float m_fwd{0.0f}, m_swr{1.0f};
};

}  // namespace NereusSDR
```

- [ ] **Step 2: Create .cpp**

Port from `../AetherSDR/src/models/TunerModel.cpp` (233 lines). Key logic: `applyStatus` parses each known key and emits the matching changed signal; `autoTune` sends `tune start`; `adjustRelay` forwards to `m_conn->adjustRelay`; `setOperate`/`setBypass` send `operate=N`/`bypass=N`; `setAntennaA` sends `activate ant=N`.

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc)
```

- [ ] **Step 4: Stage**

```bash
git add src/models/TunerModel.h src/models/TunerModel.cpp
```

## Task 11: TunerModel applyStatus test (TDD)

**Files:**
- Create: `tests/TunerModelApplyStatusTest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the test**

```cpp
#include <QtTest>
#include "models/TunerModel.h"

class TunerModelApplyStatusTest : public QObject {
    Q_OBJECT
private slots:
    void appliesRelayValues();
    void appliesOperateAndBypass();
    void appliesAntennaSwitchModel();
    void appliesMeters();
    void emitsPresenceOnFirstStatus();
};

void TunerModelApplyStatusTest::appliesRelayValues() {
    NereusSDR::TunerModel m;
    QSignalSpy relaySpy(&m, &NereusSDR::TunerModel::relayChanged);

    m.applyStatus({{"relayC1","42"},{"relayL","199"},{"relayC2","88"}});

    QCOMPARE(m.relayC1(), 42);
    QCOMPARE(m.relayL(),  199);
    QCOMPARE(m.relayC2(), 88);
    QVERIFY(relaySpy.count() >= 1);
}

void TunerModelApplyStatusTest::appliesOperateAndBypass() {
    NereusSDR::TunerModel m;
    m.applyStatus({{"operate","1"},{"bypass","0"}});
    QVERIFY(m.isOperate());
    QVERIFY(!m.isBypass());

    m.applyStatus({{"bypass","1"}});
    QVERIFY(m.isBypass());
}

void TunerModelApplyStatusTest::appliesAntennaSwitchModel() {
    NereusSDR::TunerModel m;
    QVERIFY(!m.hasAntennaSwitch());
    m.applyStatus({{"one_by_three","1"},{"antA","1"}});
    QVERIFY(m.hasAntennaSwitch());
    QCOMPARE(m.antennaA(), 1);
}

void TunerModelApplyStatusTest::appliesMeters() {
    NereusSDR::TunerModel m;
    QSignalSpy metersSpy(&m, &NereusSDR::TunerModel::metersChanged);
    m.applyStatus({{"fwd","12.5"},{"swr","1.4"}});
    QCOMPARE(m.fwdPower(), 12.5f);
    QCOMPARE(m.swr(),      1.4f);
    QCOMPARE(metersSpy.count(), 1);
}

void TunerModelApplyStatusTest::emitsPresenceOnFirstStatus() {
    NereusSDR::TunerModel m;
    QSignalSpy presenceSpy(&m, &NereusSDR::TunerModel::presenceChanged);
    m.applyStatus({{"model","TunerGeniusXL"},{"serial_num","TGXL1234"}});
    QVERIFY(m.isPresent());
    QCOMPARE(presenceSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TunerModelApplyStatusTest)
#include "TunerModelApplyStatusTest.moc"
```

- [ ] **Step 2: Register in CMakeLists.txt + build + run**

Same pattern as Task 9. Run; expect PASS if the port from AetherSDR's `applyStatus` is faithful. If FAIL, cross-check against `../AetherSDR/src/models/TunerModel.cpp`.

- [ ] **Step 3: Commit Tasks 10 + 11**

```bash
git add src/models/TunerModel.h src/models/TunerModel.cpp tests/TunerModelApplyStatusTest.cpp CMakeLists.txt
git commit -m "feat(tgxl): port TunerModel with applyStatus tests"
```

## Task 12: RelayBar widget

**Files:**
- Create: `src/gui/RelayBar.h` / `.cpp`

- [ ] **Step 1: Port from AetherSDR's inner `RelayBar` class**

The AetherSDR `RelayBar` is an inner helper inside `TunerApplet.cpp` (~80 lines). Extract it into its own header + source files in NereusSDR. Public API:

```cpp
namespace NereusSDR {

class RelayBar : public QWidget {
    Q_OBJECT
public:
    explicit RelayBar(const QString& label, QWidget* parent = nullptr);
    void setValue(int byteValue);     // 0..255
    void setScrollEnabled(bool on);
signals:
    void relayAdjusted(int direction);  // +1 / -1
protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent* ev) override;
private:
    QString m_label;
    int     m_value{0};
    bool    m_scrollEnabled{false};
};

}  // namespace NereusSDR
```

`paintEvent` draws the bar background, a fill rect proportional to `m_value / 255.0f`, and the label. `wheelEvent` emits `relayAdjusted(ev->angleDelta().y() > 0 ? +1 : -1)` when `m_scrollEnabled`.

- [ ] **Step 2: Build and confirm no errors**

```bash
cmake --build build -j$(nproc)
```

- [ ] **Step 3: Commit**

```bash
git add src/gui/RelayBar.h src/gui/RelayBar.cpp CMakeLists.txt
git commit -m "feat(tgxl): extract RelayBar widget with mousewheel step support"
```

## Task 13: LanDiscovery class

**Files:**
- Create: `src/core/LanDiscovery.h` / `.cpp`
- Test: `tests/LanDiscoveryRegexTest.cpp`

- [ ] **Step 1: Create header**

```cpp
#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QSet>
#include <QString>

namespace NereusSDR {

class LanDiscovery : public QObject {
    Q_OBJECT
public:
    explicit LanDiscovery(QObject* parent = nullptr);

    void start(int timeoutMs = 3000);
    void stop();

    // Test hook: feed a raw datagram payload as if it arrived on UDP.
    void injectDatagramForTesting(const QString& payload);

signals:
    void deviceDiscovered(const QString& model,
                          const QString& ip,
                          quint16        port,
                          const QString& version,
                          const QString& serial,
                          const QString& nickname);
    void scanFinished();

private slots:
    void on9008Ready();
    void on9010Ready();
    void onTimeout();

private:
    void parseAnnouncement(const QString& payload, quint16 port);

    QUdpSocket m_sock9008;
    QUdpSocket m_sock9010;
    QTimer     m_timeout;
    QSet<QString> m_seenSerials;
};

}  // namespace NereusSDR
```

- [ ] **Step 2: Implement parseAnnouncement with the official regex**

In `.cpp`:

```cpp
#include "LanDiscovery.h"
#include <QRegularExpression>
#include <QLoggingCategory>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcLan, "nereus.lan")

LanDiscovery::LanDiscovery(QObject* parent) : QObject(parent) {
    connect(&m_sock9008, &QUdpSocket::readyRead, this, &LanDiscovery::on9008Ready);
    connect(&m_sock9010, &QUdpSocket::readyRead, this, &LanDiscovery::on9010Ready);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &LanDiscovery::onTimeout);
}

void LanDiscovery::start(int timeoutMs) {
    m_seenSerials.clear();
    m_sock9008.bind(QHostAddress::AnyIPv4, 9008,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_sock9010.bind(QHostAddress::AnyIPv4, 9010,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_timeout.start(timeoutMs);
}

void LanDiscovery::stop() {
    m_timeout.stop();
    m_sock9008.close();
    m_sock9010.close();
}

void LanDiscovery::on9008Ready() {
    while (m_sock9008.hasPendingDatagrams()) {
        QByteArray d(int(m_sock9008.pendingDatagramSize()), 0);
        m_sock9008.readDatagram(d.data(), d.size());
        parseAnnouncement(QString::fromUtf8(d).trimmed(), 9008);
    }
}

void LanDiscovery::on9010Ready() {
    while (m_sock9010.hasPendingDatagrams()) {
        QByteArray d(int(m_sock9010.pendingDatagramSize()), 0);
        m_sock9010.readDatagram(d.data(), d.size());
        parseAnnouncement(QString::fromUtf8(d).trimmed(), 9010);
    }
}

void LanDiscovery::onTimeout() {
    stop();
    emit scanFinished();
}

void LanDiscovery::injectDatagramForTesting(const QString& payload) {
    parseAnnouncement(payload, 9008);  // port arbitrary for tests
}

void LanDiscovery::parseAnnouncement(const QString& payload, quint16 port) {
    static const QRegularExpression rx(
        R"(^(?<model>\S+)\s+ip=(?<ip>\d+\.\d+\.\d+\.\d+)\s+v=(?<v>\S+)\s+serial=(?<serial>\S+)\s+nickname=(?<nick>\S+)$)");
    auto m = rx.match(payload);
    if (!m.hasMatch()) return;
    QString serial = m.captured("serial");
    if (m_seenSerials.contains(serial)) return;
    m_seenSerials.insert(serial);
    emit deviceDiscovered(m.captured("model"),
                          m.captured("ip"),
                          port,
                          m.captured("v"),
                          serial,
                          m.captured("nick"));
}

}  // namespace NereusSDR
```

- [ ] **Step 3: Stage**

```bash
git add src/core/LanDiscovery.h src/core/LanDiscovery.cpp
```

## Task 14: LanDiscovery regex test

**Files:**
- Create: `tests/LanDiscoveryRegexTest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the test**

```cpp
#include <QtTest>
#include "core/LanDiscovery.h"

class LanDiscoveryRegexTest : public QObject {
    Q_OBJECT
private slots:
    void parsesValidAnnouncement();
    void rejectsMalformedLine();
    void dedupsBySerial();
};

void LanDiscoveryRegexTest::parsesValidAnnouncement() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=PGXL5678 nickname=ShackAmp");
    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("PowerGeniusXL"));
    QCOMPARE(args.at(1).toString(), QString("192.168.1.43"));
    QCOMPARE(args.at(3).toString(), QString("3.8.9"));
    QCOMPARE(args.at(4).toString(), QString("PGXL5678"));
    QCOMPARE(args.at(5).toString(), QString("ShackAmp"));
}

void LanDiscoveryRegexTest::rejectsMalformedLine() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("garbage with no fields");
    d.injectDatagramForTesting("PowerGeniusXL ip=invalid v=3.8.9 serial=X nickname=Y");
    QCOMPARE(spy.count(), 0);
}

void LanDiscoveryRegexTest::dedupsBySerial() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    QString line = "PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=PGXL5678 nickname=ShackAmp";
    d.injectDatagramForTesting(line);
    d.injectDatagramForTesting(line);
    d.injectDatagramForTesting(line);
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(LanDiscoveryRegexTest)
#include "LanDiscoveryRegexTest.moc"
```

- [ ] **Step 2: Register, build, run, expect PASS**

Same CMakeLists pattern as before.

- [ ] **Step 3: Commit Task 13 + 14**

```bash
git add src/core/LanDiscovery.h src/core/LanDiscovery.cpp tests/LanDiscoveryRegexTest.cpp CMakeLists.txt
git commit -m "feat(lan): LanDiscovery UDP listener on 9008/9010 with regex parsing"
```

## Task 15: Port AmpApplet (new file)

**Files:**
- Create: `src/gui/applets/AmpApplet.h` / `.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Port from `../AetherSDR/src/gui/AmpApplet.{h,cpp}` (41 + 143 LOC)**

Apply these changes during the port:

1. Inherit from `NereusSDR::AppletWidget` (not `QWidget`) so we pick up the standard title bar + helpers. Pass `RadioModel*` to the constructor.
2. Use `HGauge` from NereusSDR (it has a different constructor than AetherSDR's): set range / yellow / red / title / unit through setters.
3. Add the attribution header verbatim from `../AetherSDR/src/gui/AmpApplet.h`.
4. Append the modification-history block per `docs/attribution/HOW-TO-PORT.md`.
5. Update `docs/attribution/aethersdr-contributor-index.md` line 229 to point at the new path.

The applet exposes:

```cpp
signals:
    void operateToggled(bool requestedOperate);
public slots:
    void setFwdPower(float w);
    void setSwr(float v);
    void setTemp(float c);
    void setDrainCurrent(float a);
    void setMainsVoltage(int v);
    void setState(const QString& state);
    void setMeff(const QString& meff);
```

- [ ] **Step 2: Build + smoke-run NereusSDR; verify AmpApplet placeholder shows in the right-column applet stack**

```bash
cmake --build build -j$(nproc)
./build/NereusSDR &
```

Disconnected state: gauges read zero, OPERATE button hidden/grey, telemetry labels empty.

- [ ] **Step 3: Commit**

```bash
git add src/gui/applets/AmpApplet.h src/gui/applets/AmpApplet.cpp CMakeLists.txt docs/attribution/aethersdr-contributor-index.md
git commit -m "feat(pgxl): port AmpApplet from AetherSDR with attribution"
```

## Task 16: Rewire existing TunerApplet (relay bars + cycle button)

**Files:**
- Modify: `src/gui/applets/TunerApplet.h` / `.cpp`

- [ ] **Step 1: Drop NyiOverlay calls**

Open `src/gui/applets/TunerApplet.cpp`. Remove every `NyiOverlay::markNyi(...)` call (lines 82, 91, 125, 154, 155, 156, 187, 188, 189 per the audit). Remove the `#include "NyiOverlay.h"` if no longer used.

- [ ] **Step 2: Replace QProgressBar with RelayBar**

In the header, change:

```cpp
QProgressBar* m_c1Bar = nullptr;
QProgressBar* m_lBar  = nullptr;
QProgressBar* m_c2Bar = nullptr;
```

to:

```cpp
RelayBar* m_c1Bar = nullptr;
RelayBar* m_lBar  = nullptr;
RelayBar* m_c2Bar = nullptr;
```

In the `.cpp`, replace the `makeRelayBar` local helper with construction of `new RelayBar("C1", this)` etc., and wire `RelayBar::relayAdjusted` to call `TunerModel::adjustRelay`.

- [ ] **Step 3: Refactor 3-button group to single cycle button**

Remove `m_bypassBtn`, `m_standbyBtn`, `m_modeGroup` from the header. Keep only `m_operateBtn`. In `buildUI`, replace the three-button block with one button and add a `cycleOperateState()` private slot that issues OPERATE -> BYPASS -> STANDBY transitions via `m_model->setOperate(...)` / `setBypass(...)`.

- [ ] **Step 4: Wire to TunerModel + TgxlConnection**

Constructor takes `TunerModel*` (passed by `RadioModel`). Subscribe to `relayChanged` -> `setValue` on each `RelayBar`; `stateChanged` -> updateOperateButton; `tuningChanged` -> tune button color + post-tune capture; `metersChanged` -> `HGauge::setValue` on Fwd/SWR; `directConnectionChanged` -> `RelayBar::setScrollEnabled` + antenna container visibility; `antennaAChanged` -> highlight selected ANT button.

Add the antenna container (3 buttons), gated visible only when `hasDirectConnection() && hasAntennaSwitch()`.

- [ ] **Step 5: Update header attribution block**

Add a new modification-history line dated 2026-05-18 noting the rewire (NyiOverlay removed, RelayBar swap, single cycle button, TunerModel wiring).

- [ ] **Step 6: Update label from "Aries ATU" to "Tuner Genius"**

Change `appletTitle()` return to `"Tuner Genius"`.

- [ ] **Step 7: Build + smoke-run**

```bash
cmake --build build -j$(nproc)
./build/NereusSDR &
```

Without a TGXL connected, the applet should show: gauges at zero, three RelayBar widgets at zero, TUNE button visible, OPERATE button visible labeled "STANDBY", antenna container hidden.

- [ ] **Step 8: Commit**

```bash
git add src/gui/applets/TunerApplet.h src/gui/applets/TunerApplet.cpp
git commit -m "feat(tgxl): rewire TunerApplet for TGXL (RelayBar + cycle button + model)"
```

## Task 17: PeripheralsPage in CatNetworkSetupPages

**Files:**
- Modify: `src/gui/setup/CatNetworkSetupPages.h` / `.cpp`

- [ ] **Step 1: Add `PeripheralsPage` class to the header**

```cpp
class PeripheralsPage : public QWidget {
    Q_OBJECT
public:
    explicit PeripheralsPage(RadioModel* model, QWidget* parent = nullptr);
private slots:
    void onScanLan(int rowIdx);
    void onConnect(int rowIdx);
private:
    void buildRow(int row, const QString& name,
                  const QString& ipKey, const QString& portKey,
                  quint16 defaultPort);
    RadioModel* m_model{nullptr};
    QGridLayout* m_grid{nullptr};
};
```

- [ ] **Step 2: Implement the grid layout per spec §5.3**

Two rows (TGXL, PGXL), six columns. Status label updates from `PgxlConnection::connected/disconnected/connectionFailed` and the parallel TGXL signals. Persist `*_ManualIp` / `*_ManualPort` in AppSettings on edit.

- [ ] **Step 3: Register the page in SetupDialog's Network category**

Wherever the existing TCI Server entry is registered, add `Peripherals` next to it.

- [ ] **Step 4: Build + smoke-run; navigate Setup > Network > Peripherals**

Without devices: both rows show "Disconnected"; clicking Scan LAN opens the Scan LAN dialog (next task).

- [ ] **Step 5: Commit**

```bash
git add src/gui/setup/CatNetworkSetupPages.h src/gui/setup/CatNetworkSetupPages.cpp
git commit -m "feat(setup): add Peripherals page for PGXL/TGXL configuration"
```

## Task 18: LanScanDialog

**Files:**
- Create: `src/gui/LanScanDialog.h` / `.cpp`

- [ ] **Step 1: Modeless dialog with a QTableWidget**

Columns: Model, IP, Port, Version, Serial, Nickname. Owns a `LanDiscovery` instance. On `deviceDiscovered`, append a row. On double-click, emit `deviceSelected(QString ip, quint16 port)` and close. Progress bar showing the 3 s window via a `QTimer::singleShot(3000, ...)`.

- [ ] **Step 2: Wire from PeripheralsPage Scan LAN buttons**

`PeripheralsPage::onScanLan` opens the dialog and on `deviceSelected` fills the Host + Port fields of the matching row.

- [ ] **Step 3: Build + smoke-run with two devices on the LAN (or hand-injected datagrams via the test hook for manual verification)**

```bash
./build/NereusSDR &
```

Click Scan LAN; expect a 3 s progress bar; table populates as devices announce.

- [ ] **Step 4: Commit**

```bash
git add src/gui/LanScanDialog.h src/gui/LanScanDialog.cpp src/gui/setup/CatNetworkSetupPages.cpp CMakeLists.txt
git commit -m "feat(lan): modeless Scan LAN dialog with double-click-to-fill"
```

## Task 19: RadioModel owns PGXL/TGXL/Tuner/Lan

**Files:**
- Modify: `src/models/RadioModel.h` / `.cpp`

- [ ] **Step 1: Add private members and accessors**

```cpp
private:
    PgxlConnection m_pgxlConnection;
    TgxlConnection m_tgxlConnection;
    TunerModel     m_tunerModel;
public:
    PgxlConnection* pgxlConnection() { return &m_pgxlConnection; }
    TgxlConnection* tgxlConnection() { return &m_tgxlConnection; }
    TunerModel*     tunerModel()     { return &m_tunerModel; }
    bool hasAmplifier() const { return m_hasAmplifier; }
    bool ampOperate()  const { return m_ampOperate; }
signals:
    void amplifierChanged(bool present);
    void ampStateChanged();
    void ampMetersChanged(float fwd, float swr);
private:
    bool m_hasAmplifier{false};
    bool m_ampOperate{false};
```

- [ ] **Step 2: In RadioModel constructor, wire the connections**

```cpp
m_tunerModel.bindConnection(&m_tgxlConnection);

connect(&m_pgxlConnection, &PgxlConnection::statusUpdated,
        this, &RadioModel::onPgxlStatus);
```

`onPgxlStatus(const QMap<QString,QString>&)` updates `m_hasAmplifier=true` on first call, parses `state` to set `m_ampOperate` (true when `state in {IDLE, OPERATE, TRANSMIT_A, TRANSMIT_B}`), and emits `ampMetersChanged(fwd, swr)` from `peakfwd` (convert dBm to watts) + `swr` (convert dB return-loss to ratio).

- [ ] **Step 3: Build**

```bash
cmake --build build -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp
git commit -m "feat(radio): own PGXL/TGXL connections and TunerModel"
```

## Task 20: MainWindow auto-connect on radio connect

**Files:**
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: In `onRadioConnected()`, auto-connect if Manual IP is set**

```cpp
auto& s = AppSettings::instance();

QString pgxlIp = s.value("PGXL_ManualIp", "").toString();
if (!pgxlIp.isEmpty() && !m_radioModel.pgxlConnection()->isConnected()) {
    quint16 p = quint16(s.value("PGXL_ManualPort", "9008").toInt());
    m_radioModel.pgxlConnection()->connectToPgxl(pgxlIp, p);
}

QString tgxlIp = s.value("TGXL_ManualIp", "").toString();
if (!tgxlIp.isEmpty() && !m_radioModel.tgxlConnection()->isConnected()) {
    quint16 p = quint16(s.value("TGXL_ManualPort", "9010").toInt());
    m_radioModel.tgxlConnection()->connectToTgxl(tgxlIp, p);
}
```

- [ ] **Step 2: Wire AmpApplet::operateToggled to PgxlConnection**

```cpp
connect(m_ampApplet, &AmpApplet::operateToggled, this, [this](bool wantOperate) {
    m_radioModel.pgxlConnection()->sendCommand(wantOperate ? "operate" : "standby");
});
```

- [ ] **Step 3: Subscribe AmpApplet to PgxlConnection statusUpdated**

```cpp
connect(m_radioModel.pgxlConnection(), &PgxlConnection::statusUpdated,
        this, [this](const QMap<QString,QString>& kvs) {
    bool ok=false;
    if (kvs.contains("temp"))   m_ampApplet->setTemp(kvs.value("temp").toFloat());
    if (kvs.contains("id"))     m_ampApplet->setDrainCurrent(kvs.value("id").toFloat());
    if (kvs.contains("vac"))    m_ampApplet->setMainsVoltage(kvs.value("vac").toInt());
    if (kvs.contains("state"))  m_ampApplet->setState(kvs.value("state"));
    if (kvs.contains("meffa"))  m_ampApplet->setMeff(kvs.value("meffa"));
    if (kvs.contains("peakfwd"))m_ampApplet->setFwdPower(kvs.value("peakfwd").toFloat());
    if (kvs.contains("swr"))    m_ampApplet->setSwr(kvs.value("swr").toFloat());
});
```

- [ ] **Step 4: Build + smoke-run with a real PGXL on the LAN**

If no hardware available, defer to bench-row 6 in §10.2.

- [ ] **Step 5: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "feat(pgxl): wire MainWindow auto-connect and AmpApplet signal routing"
```

## Task 21: MainWindow TGXL status bar chip

**Files:**
- Modify: `src/gui/MainWindow.cpp` (buildStatusBar)

- [ ] **Step 1: Add `m_tgxlChip` QLabel to MainWindow**

In `MainWindow::buildStatusBar()`, after the FPGA temperature chip:

```cpp
m_tgxlChip = new QLabel("TGXL", sb);
m_tgxlChip->setStyleSheet("QLabel { background:#1a3a5a; border:1px solid #205070; "
                          "padding:1px 8px; border-radius:3px; color:#88e0ff; }");
m_tgxlChip->setVisible(false);
sb->addPermanentWidget(m_tgxlChip);

connect(&m_radioModel.tunerModel(), &TunerModel::presenceChanged,
        m_tgxlChip, &QWidget::setVisible);
connect(&m_radioModel.tunerModel(), &TunerModel::stateChanged, this, [this]() {
    auto* t = m_radioModel.tunerModel();
    QString s = t->isOperate() ? (t->isBypass() ? "BYPS" : "OPER") : "SBY";
    m_tgxlChip->setText("TGXL " + s);
});
```

- [ ] **Step 2: Build + smoke-run; verify chip hidden without TGXL**

- [ ] **Step 3: Commit**

```bash
git add src/gui/MainWindow.cpp src/gui/MainWindow.h
git commit -m "feat(tgxl): bottom status bar TGXL presence chip"
```

## Task 22-29: Phase 1 integration polish (8 tasks)

Each ~5 min, single-purpose:

- [ ] **Task 22:** Wire TunerApplet to MainWindow's RadioModel `tunerModel()` accessor; verify SWR/FwdPower updates on synthetic injection.
- [ ] **Task 23:** Verify `connectionFailed` signal path: feed a wrong port; observe red "Error: Connection refused" in Peripherals row.
- [ ] **Task 24:** Verify Disconnect button state transition: click Disconnect; row reverts to "Disconnected"; AmpApplet greys out.
- [ ] **Task 25:** Run `ctest --test-dir build` and confirm all 4 new tests pass (PgxlConnectionParse, TgxlConnectionParse, TunerModelApplyStatus, LanDiscoveryRegex).
- [ ] **Task 26:** Verify no new compile warnings: `cmake --build build 2>&1 | grep -iE 'warning|error'` returns clean.
- [ ] **Task 27:** Run NereusSDR for 10 minutes connected to a radio without PGXL/TGXL; verify no spurious connect attempts (empty Manual IPs).
- [ ] **Task 28:** Update `docs/attribution/aethersdr-contributor-index.md` line 247 (TunerModel) and confirm any other entries pointing at "(none yet)" that this PR creates.
- [ ] **Task 29:** Run `scripts/verify-thetis-headers.py --all-kinds` (or whatever the AetherSDR-equivalent script is) to confirm header preservation. Commit each verification fix individually.

For each: build + smoke-run + commit:

```bash
git add <paths>
git commit -m "<terse subject>"
```

## Task 30: Phase 1 integration smoke check

**Files:**
- Modify: none (verification only)

- [ ] **Step 1: Build clean**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

- [ ] **Step 2: Run all tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all green including the 4 new tests.

- [ ] **Step 3: Smoke-run NereusSDR**

```bash
./build/NereusSDR &
```

Manual checks (15 min):

1. Open Setup > Network > Peripherals. See two rows. Type 192.168.1.42 for TGXL. Click Connect. (If no hardware: skip but verify Error state.)
2. Click Scan LAN. Dialog opens for 3 s. Table empty (no real devices) is acceptable here.
3. Confirm Peripherals row state updates.
4. With hardware: AmpApplet shows live telemetry; TunerApplet relay bars move; TUNE button triggers `tune start` (visible via wireshark/`tcpdump -i any -A port 9010`).
5. Bottom status bar TGXL chip appears on connect, disappears on disconnect.

- [ ] **Step 4: Tag the Phase 1 checkpoint commit**

No actual git tag, but the commit at this point is the Phase 1 baseline. Commit a CHANGELOG entry:

```bash
echo "" >> CHANGELOG.md
echo "### Phase 3P-II baseline (in progress)" >> CHANGELOG.md
echo "- PGXL telemetry + TGXL control over Ethernet (AetherSDR 1:1 baseline)" >> CHANGELOG.md
git add CHANGELOG.md
git commit -m "chore(changelog): Phase 3P-II baseline checkpoint"
```

---

# Phase 2: Analog S-Meter Port

Estimated: 25 tasks. Net new code: ~900 LOC + 4 test files.

## Task 31: WdspEngine RXA_S_AV wrapper

**Files:**
- Modify: `src/core/wdsp/WdspEngine.h` / `.cpp`

- [ ] **Step 1: Add the slot**

In header:

```cpp
// Returns averaged S-meter reading (dBm) from RXA_S_AV.
// Cite: Thetis console.cs:957 [@501e3f5]
double getRxaSignalAverage(int channel) const;
```

In .cpp:

```cpp
double WdspEngine::getRxaSignalAverage(int channel) const {
    return ::GetRXAMeter(channel, RXA_S_AV);
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build -j$(nproc)
```

- [ ] **Step 3: Commit (no separate test; covered by SMeterWidget MaxBin test path indirectly)**

```bash
git add src/core/wdsp/WdspEngine.h src/core/wdsp/WdspEngine.cpp
git commit -m "feat(wdsp): expose RXA_S_AV for Sig Avg S-meter mode"
```

## Task 32: WdspEngine setupMaxBinDetector wrapper

**Files:**
- Modify: `src/core/wdsp/WdspEngine.h` / `.cpp`

- [ ] **Step 1: Add slot per spec §5.4.3**

In header:

```cpp
// Cite: Thetis wdsp/analyzer.c:775 [@501e3f5]
void setupMaxBinDetector(int displayChannel,
                         int ss = 0, int LO = 0,
                         double rateHz = 192000.0,
                         double fLowHz = -3000.0,
                         double fHighHz = -300.0,
                         double tauSeconds = 0.5,
                         int frameRate = 60);

// Cite: Thetis wdsp/analyzer.c:830 [@501e3f5]
double getMaxBinDbm(int displayChannel) const;
```

In .cpp:

```cpp
void WdspEngine::setupMaxBinDetector(int disp, int ss, int LO,
                                     double rate, double fLow, double fHigh,
                                     double tau, int frameRate) {
    ::SetupDetectMaxBin(/*run=*/1, disp, ss, LO, rate, fLow, fHigh, tau, frameRate);
}

double WdspEngine::getMaxBinDbm(int disp) const {
    return ::GetDetectMaxBin(disp);
}
```

- [ ] **Step 2: Build**

- [ ] **Step 3: Commit**

```bash
git add src/core/wdsp/WdspEngine.h src/core/wdsp/WdspEngine.cpp
git commit -m "feat(wdsp): port Thetis SetupDetectMaxBin / GetDetectMaxBin wrappers"
```

## Task 33: WdspEngine Max Bin test

**Files:**
- Create: `tests/WdspEngineMaxBinTest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the test**

```cpp
#include <QtTest>
#include "core/wdsp/WdspEngine.h"

class WdspEngineMaxBinTest : public QObject {
    Q_OBJECT
private slots:
    void setupSucceedsWithDefaults();
    void getReturnsFiniteValue();
};

void WdspEngineMaxBinTest::setupSucceedsWithDefaults() {
    NereusSDR::WdspEngine engine;
    engine.initialize(/*display=*/0, /*rate=*/192000);
    engine.setupMaxBinDetector(0);  // defaults from spec
    QVERIFY(true);  // no crash + no exception
}

void WdspEngineMaxBinTest::getReturnsFiniteValue() {
    NereusSDR::WdspEngine engine;
    engine.initialize(0, 192000);
    engine.setupMaxBinDetector(0);
    double dbm = engine.getMaxBinDbm(0);
    QVERIFY(std::isfinite(dbm));
}

QTEST_GUILESS_MAIN(WdspEngineMaxBinTest)
#include "WdspEngineMaxBinTest.moc"
```

- [ ] **Step 2: Register, build, run; expect PASS**

```bash
ctest --test-dir build -R WdspEngineMaxBinTest --output-on-failure
```

- [ ] **Step 3: Commit**

```bash
git add tests/WdspEngineMaxBinTest.cpp CMakeLists.txt
git commit -m "test(wdsp): Max Bin detector wrappers smoke test"
```

## Task 34: SMeterWidget header skeleton

**Files:**
- Create: `src/gui/SMeterWidget.h`

- [ ] **Step 1: Port from `../AetherSDR/src/gui/SMeterWidget.h`**

Add the AetherSDR header + modification block. Update the RxMode enum to four entries:

```cpp
enum class RxMode { SMeter, SignalAverage, SMeterPeak, MaxBin };
```

Add `contextMenuEvent(QContextMenuEvent*)` declaration.

The full public API per spec §5.4.1.

- [ ] **Step 2: Stage**

```bash
git add src/gui/SMeterWidget.h
```

## Task 35: SMeterWidget skeleton + arc rendering

**Files:**
- Create: `src/gui/SMeterWidget.cpp`

- [ ] **Step 1: Port the constructor + paintEvent's arc/scale rendering from AetherSDR**

From `../AetherSDR/src/gui/SMeterWidget.cpp` lines 1..400 (constructor + paintEvent up through the tick rendering). Preserve all magic numbers verbatim: arc start/end 55/125 degrees, S0=-127 dBm, S9=-73 dBm.

- [ ] **Step 2: Build + smoke-run; expect to see an empty needle gauge in a test harness**

The AppletPanelWidget header still uses the old composite S-Meter at this point. To smoke-test the new widget, instantiate it temporarily in a scratch window or wait until Task 41 swaps the header.

- [ ] **Step 3: Stage**

```bash
git add src/gui/SMeterWidget.cpp
```

## Task 36: SMeterWidget scale test

**Files:**
- Create: `tests/SMeterWidgetScaleTest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Test setPowerScale across three brackets**

```cpp
#include <QtTest>
#include "gui/SMeterWidget.h"

class SMeterWidgetScaleTest : public QObject {
    Q_OBJECT
private slots:
    void powerScaleBarefootDefault();
    void powerScaleAuroraOverlord();
    void powerScalePgxlAmplifier();
};

void SMeterWidgetScaleTest::powerScaleBarefootDefault() {
    NereusSDR::SMeterWidget w;
    w.setPowerScale(100, false);  // barefoot
    // Internal m_powerScaleMax = 120, m_powerRedStart = 100
    // Verified via a public read-only accessor added for testing:
    QCOMPARE(w.testPowerScaleMax(), 120.0f);
    QCOMPARE(w.testPowerRedStart(), 100.0f);
}

void SMeterWidgetScaleTest::powerScaleAuroraOverlord() {
    NereusSDR::SMeterWidget w;
    w.setPowerScale(500, false);
    QCOMPARE(w.testPowerScaleMax(), 600.0f);
    QCOMPARE(w.testPowerRedStart(), 500.0f);
}

void SMeterWidgetScaleTest::powerScalePgxlAmplifier() {
    NereusSDR::SMeterWidget w;
    w.setPowerScale(0, true);
    QCOMPARE(w.testPowerScaleMax(), 2000.0f);
    QCOMPARE(w.testPowerRedStart(), 1500.0f);
}

QTEST_GUILESS_MAIN(SMeterWidgetScaleTest)
#include "SMeterWidgetScaleTest.moc"
```

- [ ] **Step 2: Add test accessors to SMeterWidget**

In the header, public section:

```cpp
// Test-only accessors. Production code uses paintEvent's internal logic.
float testPowerScaleMax()  const { return m_powerScaleMax; }
float testPowerRedStart()  const { return m_powerRedStart; }
```

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit Tasks 34-36 together**

```bash
git add src/gui/SMeterWidget.h src/gui/SMeterWidget.cpp tests/SMeterWidgetScaleTest.cpp CMakeLists.txt
git commit -m "feat(smeter): port SMeterWidget skeleton + scale test"
```

## Task 37: SMeterWidget peak hold test

**Files:**
- Create: `tests/SMeterWidgetPeakHoldTest.cpp`

- [ ] **Step 1: Test the three decay rates**

```cpp
void SMeterWidgetPeakHoldTest::fastDecayDropsAt20DbPerSec() {
    NereusSDR::SMeterWidget w;
    w.setPeakHoldEnabled(true);
    w.setPeakDecayRate(NereusSDR::SMeterWidget::Decay::Fast);
    w.setLevel(-50.0f);  // peak captured at -50
    // advance 1 s of synthetic time:
    w.testAdvanceTime(1000);
    float peak = w.testPeakLevel();
    QVERIFY(peak >= -71.0f);  // 20 dB/s -> peak around -70
    QVERIFY(peak <= -69.0f);
}
```

Similar for Medium (10 dB/s) and Slow (5 dB/s).

- [ ] **Step 2: Implement peak tracking + decay in SMeterWidget**

Port `m_peakDbm`, `m_peakHoldUntilMs`, and the decay-on-paint logic from AetherSDR. Add `testAdvanceTime(int ms)` and `testPeakLevel()` test accessors.

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/gui/SMeterWidget.h src/gui/SMeterWidget.cpp tests/SMeterWidgetPeakHoldTest.cpp CMakeLists.txt
git commit -m "feat(smeter): peak hold + decay rate logic"
```

## Task 38: SMeterWidget context menu (TX + RX + Peak Hold submenus)

**Files:**
- Modify: `src/gui/SMeterWidget.h` / `.cpp`

- [ ] **Step 1: Override contextMenuEvent**

```cpp
void SMeterWidget::contextMenuEvent(QContextMenuEvent* ev) {
    QMenu menu(this);

    QMenu* txMenu = menu.addMenu("TX Mode");
    auto* txGroup = new QActionGroup(&menu);
    txGroup->setExclusive(true);
    for (auto [label, mode] : {std::pair{"Power", TxMode::Power},
                                {"SWR", TxMode::SWR},
                                {"Level", TxMode::Level},
                                {"Compression", TxMode::Compression}}) {
        auto* a = txMenu->addAction(label);
        a->setCheckable(true);
        a->setChecked(m_txMode == mode);
        txGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode]() {
            setTxMode(modeToLabel(mode));
        });
    }

    QMenu* rxMenu = menu.addMenu("RX Mode");
    auto* rxGroup = new QActionGroup(&menu);
    rxGroup->setExclusive(true);
    for (auto [label, mode] : {std::pair{"Signal", RxMode::SMeter},
                                {"Sig Avg", RxMode::SignalAverage},
                                {"Signal Peak", RxMode::SMeterPeak},
                                {"Max Bin", RxMode::MaxBin}}) {
        auto* a = rxMenu->addAction(label);
        a->setCheckable(true);
        a->setChecked(m_rxMode == mode);
        rxGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode]() {
            setRxMode(modeToLabel(mode));
        });
    }

    QMenu* peakMenu = menu.addMenu("Peak Hold");
    auto* enabledA = peakMenu->addAction("Enabled");
    enabledA->setCheckable(true);
    enabledA->setChecked(m_peakHoldEnabled);
    connect(enabledA, &QAction::triggered, this,
            [this](bool checked) { setPeakHoldEnabled(checked); });
    QMenu* decayMenu = peakMenu->addMenu("Decay");
    // ... three exclusive actions for Fast/Medium/Slow
    peakMenu->addSeparator();
    auto* resetA = peakMenu->addAction("Reset");
    connect(resetA, &QAction::triggered, this, &SMeterWidget::resetPeak);

    menu.exec(ev->globalPos());
}
```

- [ ] **Step 2: Build + smoke-run; right-click on the test harness widget**

Verify menu structure matches the mockup.

- [ ] **Step 3: Commit**

```bash
git add src/gui/SMeterWidget.h src/gui/SMeterWidget.cpp
git commit -m "feat(smeter): right-click context menu for TX/RX/Peak settings"
```

## Task 39: SMeterWidget context menu test

**Files:**
- Create: `tests/SMeterWidgetContextMenuTest.cpp`

- [ ] **Step 1: Test the menu structure programmatically**

```cpp
void SMeterWidgetContextMenuTest::menuHasFourRxModes() {
    NereusSDR::SMeterWidget w;
    QMenu* menu = w.buildContextMenuForTesting();
    auto rxMenu = menu->actions().at(1)->menu();  // index 1 = RX Mode
    QStringList labels;
    for (auto* a : rxMenu->actions()) labels << a->text();
    QVERIFY(labels.contains("Signal"));
    QVERIFY(labels.contains("Sig Avg"));
    QVERIFY(labels.contains("Signal Peak"));
    QVERIFY(labels.contains("Max Bin"));
}

void SMeterWidgetContextMenuTest::settingTxModePersistsToAppSettings() {
    AppSettings::instance().clear();
    NereusSDR::SMeterWidget w;
    w.setTxMode("SWR");
    QCOMPARE(AppSettings::instance().value("SMeter_TxSelect", -1).toInt(), 1);
}
```

- [ ] **Step 2: Add `buildContextMenuForTesting()` test hook**

Refactor contextMenuEvent to delegate to `buildContextMenu()` which returns the QMenu*.

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/gui/SMeterWidget.h src/gui/SMeterWidget.cpp tests/SMeterWidgetContextMenuTest.cpp CMakeLists.txt
git commit -m "test(smeter): context menu structure + AppSettings round-trip"
```

## Task 40: AppletPanelWidget header swap

**Files:**
- Modify: `src/gui/applets/AppletPanelWidget.cpp`

- [ ] **Step 1: Replace the composite header MeterWidget with `SMeterWidget`**

Find where AppletPanelWidget instantiates the header (the composite ItemGroup S-Meter); replace with:

```cpp
m_sMeter = new SMeterWidget(this);
setHeaderWidget(m_sMeter, "S-Meter", /*aspectRatio=*/2.0f);
```

Remove the old composite S-Meter construction code; if it's no longer referenced elsewhere, the dead code stays available for Container #1 use (don't delete the underlying MeterWidget; just don't use it for the header).

- [ ] **Step 2: Remove the AppletPanel inline settings strip (TX/RX combos + peak hold checkbox + decay combo + reset button)**

Right-click is the only entry point now.

- [ ] **Step 3: Build + smoke-run; verify the new analog needle gauge is in the fixed header**

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/AppletPanelWidget.cpp src/gui/applets/AppletPanelWidget.h
git commit -m "feat(smeter): swap composite header for SMeterWidget; drop inline settings"
```

## Task 41: MeterPoller RX feed switch

**Files:**
- Modify: `src/gui/meters/MeterPoller.h` / `.cpp`

- [ ] **Step 1: Branch on SMeterWidget rxMode in MeterPoller's poll loop**

```cpp
void MeterPoller::pollSMeter() {
    if (!m_sMeter || !m_wdsp) return;
    int ch = m_activeRxChannel;
    int disp = m_activeDisplayChannel;

    float dbm = 0.0f;
    switch (m_sMeter->rxMode()) {
    case SMeterWidget::RxMode::SMeter:
    case SMeterWidget::RxMode::SMeterPeak:
        dbm = float(m_wdsp->getRxaMeter(ch, RXA_S_PK));
        break;
    case SMeterWidget::RxMode::SignalAverage:
        dbm = float(m_wdsp->getRxaSignalAverage(ch));
        break;
    case SMeterWidget::RxMode::MaxBin:
        dbm = float(m_wdsp->getMaxBinDbm(disp));
        break;
    }
    m_sMeter->setLevel(dbm);
}
```

- [ ] **Step 2: Build + smoke-run; right-click S-Meter, switch modes, verify needle source changes**

- [ ] **Step 3: Commit**

```bash
git add src/gui/meters/MeterPoller.h src/gui/meters/MeterPoller.cpp
git commit -m "feat(smeter): MeterPoller branches on RX mode (Signal/SigAvg/Peak/MaxBin)"
```

## Task 42: SliceModel filterChanged + Max Bin reconfigure

**Files:**
- Modify: `src/models/SliceModel.h` / `.cpp`
- Modify: `src/gui/MainWindow.cpp` (wire it)

- [ ] **Step 1: Emit filterChanged/frequencyChanged with debounce**

In SliceModel, add `filterChanged(int lowCutHz, int highCutHz)` signal emitted when the IF filter changes. Add a 100 ms `QTimer::singleShot` debounce for `frequencyChanged(qint64 hz)`.

- [ ] **Step 2: In MainWindow, wire to setupMaxBinDetector**

```cpp
connect(&activeSlice, &SliceModel::filterChanged, this,
        [this](int lowCut, int highCut) {
    m_wdsp.setupMaxBinDetector(displayChannelFor(activeSlice),
                               /*ss=*/0, /*LO=*/0,
                               m_displayRateHz,
                               lowCut, highCut,
                               0.5, 60);
});
```

- [ ] **Step 3: Build + smoke-run; change filter; verify Max Bin reading follows the new window**

- [ ] **Step 4: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp src/gui/MainWindow.cpp
git commit -m "feat(smeter): slice-aware Max Bin detector reconfigure"
```

## Task 43: MainWindow PGXL-aware S-Meter feed switch

**Files:**
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Wire ampMetersChanged + txMetersChanged with gating**

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
connect(&m_radioModel, &RadioModel::amplifierChanged, this, [this](bool present) {
    m_sMeter->setPowerScale(0, present);
});
connect(&m_radioModel, &RadioModel::ampStateChanged, this, [this]() {
    m_sMeter->setPowerScale(0, m_radioModel.hasAmplifier());
});
```

- [ ] **Step 2: Build + smoke-run with PGXL: connect amp, observe scale snap to 2 kW; STANDBY, scale reverts to barefoot**

- [ ] **Step 3: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "feat(smeter): PGXL-aware power scale + standby feed switch"
```

## Tasks 44-54: Phase 2 polish (11 tasks)

Single-action items:

- [ ] **44:** AppSettings persist `SMeter_TxSelect` on setTxMode.
- [ ] **45:** AppSettings persist `SMeter_RxSelect` on setRxMode (range 0..3).
- [ ] **46:** AppSettings persist `PeakHoldEnabled` and `PeakDecayRate`.
- [ ] **47:** Load AppSettings on SMeterWidget construction; apply mode + peak hold.
- [ ] **48:** Add `Reset Peak` action + verify it zeros m_peakDbm.
- [ ] **49:** Verify mode persists across restart (run, change mode, kill, rerun, verify).
- [ ] **50:** Run all 4 new tests: `ctest -R 'SMeter|WdspEngineMaxBin'`.
- [ ] **51:** Verify no compile warnings.
- [ ] **52:** Update `docs/attribution/aethersdr-contributor-index.md` SMeterWidget entry.
- [ ] **53:** Update CHANGELOG.md Phase 3P-II section.
- [ ] **54:** Final Phase 2 commit:

```bash
git add CHANGELOG.md docs/attribution/aethersdr-contributor-index.md
git commit -m "chore(changelog): Phase 3P-II S-Meter checkpoint"
```

---

# Phase 3: Connection Robustness

Estimated: 20 tasks. Net new code: ~600 LOC + 5 test files.

## Task 55: PgxlConnection extended slots (header)

**Files:**
- Modify: `src/core/PgxlConnection.h`

- [ ] **Step 1: Add the Tier 2 slots and state per spec §4.1.1**

```cpp
public slots:
    quint32 amplifierCreate(const QString& ourSerial,
                            const QString& ourModel = "NereusSDR",
                            const QString& antMap = "ANT1:PORTA,ANT2:PORTB");
    quint32 flexradioPair(QChar ampSlice,
                          const QString& radioSerial,
                          const QString& txAnt,
                          bool pttOverLan = true,
                          bool active = true);
    quint32 enableKeepalive();
    quint32 ping(const QString& tag = "");
    quint32 interlockCreate(const QString& validAntennas,
                            const QString& name,
                            const QString& serial);
    quint32 interlockDisable(int interlockId);
    quint32 readSetup();
    quint32 writeSetup(const QMap<QString,QString>& fields);
    quint32 readIfconf();
    quint32 writeIfconf(const QString& ip, const QString& netmask,
                        const QString& gateway, bool dhcp);
    quint32 save();
    quint32 setBand(int bandHz);

signals:
    void pongReceived(quint32 seq, qint64 rttMs, const QString& tag);
    void pingTimedOut(quint32 seq);
    void faultCaptured(/*const FaultEvent& ev*/);  // forward-declared until Phase 4
    void setupResponse(const QMap<QString,QString>& fields);
    void ifconfResponse(const QMap<QString,QString>& fields);
    void pairingResult(bool succeeded, const QString& detail);
    void saveAcknowledged();
    void reconnectAttempt(int attemptNumber, int backoffMs);

private:
    QTimer m_keepaliveTimer;
    QTimer m_pingTimer;
    QTimer m_reconnectTimer;
    int    m_reconnectAttempts{0};
    int    m_keepaliveMissed{0};
    struct PendingPing { quint32 seq; qint64 sentMs; QString tag; };
    QHash<quint32, PendingPing> m_pendingPings;
    qint64  m_lastFrameMs{0};
    qint64  m_connectedSinceMs{0};
    quint64 m_framesIn{0}, m_framesOut{0}, m_bytesIn{0}, m_bytesOut{0};
```

- [ ] **Step 2: Stage**

```bash
git add src/core/PgxlConnection.h
```

## Task 56: amplifierCreate + test

**Files:**
- Modify: `src/core/PgxlConnection.cpp`
- Create: `tests/PgxlConnectionPairingTest.cpp`

- [ ] **Step 1: Implement amplifierCreate**

```cpp
quint32 PgxlConnection::amplifierCreate(const QString& serial,
                                       const QString& model,
                                       const QString& antMap) {
    return sendCommand(QString("amplifier create ip=%1 port=%2 model=%3 serial_num=%4 ant=%5")
        .arg(QHostInfo::localHostName())   // or our own LAN IP
        .arg(9008)
        .arg(model)
        .arg(serial)
        .arg(antMap));
}
```

- [ ] **Step 2: Write the failing test**

```cpp
void PgxlConnectionPairingTest::amplifierCreateEmitsExpectedFrame() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    conn.amplifierCreate("NereusSDR-AA:BB:CC", "NereusSDR", "ANT1:PORTA,ANT2:PORTB");
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains("amplifier create"));
    QVERIFY(frame.contains("model=NereusSDR"));
    QVERIFY(frame.contains("serial_num=NereusSDR-AA:BB:CC"));
    QVERIFY(frame.contains("ant=ANT1:PORTA,ANT2:PORTB"));
}
```

Add a test signal `testFrameWrittenForTesting(QString)` emitted from `sendCommand` (guarded by `#ifdef NEREUS_TESTING` or always-on test signal).

- [ ] **Step 3: Run, expect FAIL then PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/PgxlConnection.h src/core/PgxlConnection.cpp tests/PgxlConnectionPairingTest.cpp CMakeLists.txt
git commit -m "feat(pgxl): amplifier create slot + frame format test"
```

## Task 57: flexradioPair with accept/reject

**Files:**
- Modify: `src/core/PgxlConnection.cpp`
- Modify: `tests/PgxlConnectionPairingTest.cpp`

- [ ] **Step 1: Implement flexradioPair**

```cpp
quint32 PgxlConnection::flexradioPair(QChar slice, const QString& radioSerial,
                                     const QString& txAnt, bool ptt, bool active) {
    quint32 seq = sendCommand(QString("flexradio ampslice=%1 serial=%2 txant=%3 ptt=%4 active=%5")
        .arg(slice).arg(radioSerial).arg(txAnt)
        .arg(ptt ? "LAN" : "NONE")
        .arg(active ? 1 : 0));
    m_pendingPairingSeq = seq;
    return seq;
}
```

In processLine's R-frame handler, if `seq == m_pendingPairingSeq`, parse the hex code: emit `pairingResult(hex == "0", body)`.

- [ ] **Step 2: Add accept + reject tests**

```cpp
void PgxlConnectionPairingTest::pairingSuccessEmitsPairingResultTrue() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy resultSpy(&conn, &NereusSDR::PgxlConnection::pairingResult);
    conn.injectLineForTesting("V3.8.9");
    quint32 seq = conn.flexradioPair('A', "NereusSDR-AA:BB", "ANT1");
    conn.injectLineForTesting(QString("R%1|0|serial=NereusSDR-AA:BB txant=ANT1").arg(seq));
    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.takeFirst().at(0).toBool(), true);
}

void PgxlConnectionPairingTest::pairingFailureEmitsPairingResultFalse() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy resultSpy(&conn, &NereusSDR::PgxlConnection::pairingResult);
    conn.injectLineForTesting("V3.8.9");
    quint32 seq = conn.flexradioPair('A', "NereusSDR-AA:BB", "ANT1");
    conn.injectLineForTesting(QString("R%1|2|invalid serial format").arg(seq));
    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.takeFirst().at(0).toBool(), false);
}
```

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/PgxlConnection.h src/core/PgxlConnection.cpp tests/PgxlConnectionPairingTest.cpp
git commit -m "feat(pgxl): flexradio pairing with success/failure detection"
```

## Task 58: keepalive + ping

**Files:**
- Modify: `src/core/PgxlConnection.cpp`
- Create: `tests/PgxlConnectionKeepaliveTest.cpp`
- Create: `tests/PgxlConnectionPingTest.cpp`

- [ ] **Step 1: Implement enableKeepalive**

```cpp
quint32 PgxlConnection::enableKeepalive() {
    quint32 seq = sendCommand("keepalive enable");
    auto& s = AppSettings::instance();
    m_keepaliveTimer.start(s.value("PGXL_KeepaliveSec", "30").toInt() * 1000);
    return seq;
}
```

In the constructor, connect `m_keepaliveTimer` to a slot that issues a `status` poke.

- [ ] **Step 2: Implement ping + pong correlation**

```cpp
quint32 PgxlConnection::ping(const QString& tag) {
    quint32 seq = sendCommand("ping");
    m_pendingPings.insert(seq, {seq, QDateTime::currentMSecsSinceEpoch(), tag});
    return seq;
}
```

In processLine's R-frame handler, if `seq` is in `m_pendingPings`, compute RTT and emit `pongReceived(seq, rttMs, tag)`; then erase from map.

A separate `m_pingTimeoutTimer` (5 s) checks for stale entries and emits `pingTimedOut`.

- [ ] **Step 3: Write tests for both**

(Pattern matches Tasks 56/57.)

- [ ] **Step 4: Run, expect PASS**

- [ ] **Step 5: Commit**

```bash
git add src/core/PgxlConnection.h src/core/PgxlConnection.cpp tests/PgxlConnectionKeepaliveTest.cpp tests/PgxlConnectionPingTest.cpp CMakeLists.txt
git commit -m "feat(pgxl): keepalive timer + ping with RTT measurement"
```

## Task 59: Auto-reconnect backoff

**Files:**
- Modify: `src/core/PgxlConnection.cpp`
- Create: `tests/PgxlConnectionReconnectTest.cpp`

- [ ] **Step 1: Implement reconnect schedule**

In `onDisconnected`, if `AppSettings PGXL_AutoReconnect == "True"`, schedule reconnect with backoff (1, 2, 5, 10, 30, 60, 60... seconds).

```cpp
static const int kBackoffSec[] = {1, 2, 5, 10, 30, 60};
int idx = std::min(m_reconnectAttempts, int(std::size(kBackoffSec)) - 1);
int delayMs = kBackoffSec[idx] * 1000;
emit reconnectAttempt(m_reconnectAttempts + 1, delayMs);
m_reconnectTimer.singleShot(delayMs, this, [this] {
    ++m_reconnectAttempts;
    m_socket.connectToHost(m_lastHost, m_lastPort);
});
```

- [ ] **Step 2: Test the schedule**

```cpp
void PgxlConnectionReconnectTest::backoffSequence() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy spy(&conn, &NereusSDR::PgxlConnection::reconnectAttempt);
    AppSettings::instance().setValue("PGXL_AutoReconnect", "True");

    for (int i = 0; i < 8; ++i) conn.testForceDisconnect();

    QVector<int> delays;
    while (spy.count()) {
        delays << spy.takeFirst().at(1).toInt();
    }
    QCOMPARE(delays.value(0), 1000);
    QCOMPARE(delays.value(1), 2000);
    QCOMPARE(delays.value(2), 5000);
    QCOMPARE(delays.value(3), 10000);
    QCOMPARE(delays.value(4), 30000);
    QCOMPARE(delays.value(5), 60000);
    QCOMPARE(delays.value(6), 60000);
    QCOMPARE(delays.value(7), 60000);
}
```

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/PgxlConnection.h src/core/PgxlConnection.cpp tests/PgxlConnectionReconnectTest.cpp CMakeLists.txt
git commit -m "feat(pgxl): auto-reconnect with exponential backoff (1/2/5/10/30/60s)"
```

## Task 60: TgxlConnection parallel additions

**Files:**
- Modify: `src/core/TgxlConnection.h` / `.cpp`

- [ ] **Step 1: Add parallel slots: enableKeepalive, ping, readSetup, writeSetup, readIfconf, writeIfconf, save**

Same shapes as PgxlConnection but without `amplifierCreate` / `flexradioPair` (TGXL doesn't pair the same way).

- [ ] **Step 2: Reuse the same auto-reconnect logic**

(Consider extracting into a `ReconnectStrategy` helper if duplication grows; for now inline duplication is fine and clearer.)

- [ ] **Step 3: Build**

- [ ] **Step 4: Commit**

```bash
git add src/core/TgxlConnection.h src/core/TgxlConnection.cpp
git commit -m "feat(tgxl): parallel keepalive/ping/setup/ifconf/save additions"
```

## Task 61: ConnectionDiagnostics class

**Files:**
- Create: `src/core/ConnectionDiagnostics.h` / `.cpp`
- Create: `tests/ConnectionDiagnosticsTest.cpp`

- [ ] **Step 1: Implement per spec §4.6**

Header has 10 Q_PROPERTYs + `bindTo(PgxlConnection*)` overload + `bindTo(TgxlConnection*)`. .cpp tracks frames/bytes via signal-driven counters; uptime via `QDateTime::currentMSecsSinceEpoch() - m_connectedSinceMs`; 1 Hz coalesce QTimer emits `changed()`.

- [ ] **Step 2: Test the metric aggregation**

```cpp
void ConnectionDiagnosticsTest::tracksFrameCount() {
    NereusSDR::PgxlConnection conn;
    NereusSDR::ConnectionDiagnostics diag;
    diag.bindTo(&conn);
    conn.injectLineForTesting("V3.8.9");
    conn.injectLineForTesting("R1|0|state=OPERATE");
    diag.testFlushCoalesceTimer();
    QCOMPARE(diag.framesIn(), quint64(2));
}
```

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/ConnectionDiagnostics.h src/core/ConnectionDiagnostics.cpp tests/ConnectionDiagnosticsTest.cpp CMakeLists.txt
git commit -m "feat(diag): ConnectionDiagnostics with 1 Hz coalesced metrics"
```

## Tasks 62-74: Phase 3 polish (13 tasks)

- [ ] **62:** Implement RadioModel pairing-flow runner (calls amplifierCreate + flexradioPair + enableKeepalive on PGXL connect).
- [ ] **63:** Update PeripheralsPage Status string to reflect pairing result.
- [ ] **64:** SliceModel emit bandChanged(Band) (uses existing Band enum).
- [ ] **65:** MainWindow connect SliceModel.bandChanged -> PgxlConnection.setBand.
- [ ] **66:** Implement PgxlConnection.setBand (sends `flexradio ampslice=A serial=... band=<freq>` if paired, else no-op).
- [ ] **67:** TgxlConnection pong correlation parallel to PgxlConnection.
- [ ] **68:** AppSettings persist PGXL_AutoReconnect, _KeepaliveSec, _PingSec, _PairAttempt, _FlexAmpSlice, _TxAnt.
- [ ] **69:** AppSettings persist TGXL_AutoReconnect, _KeepaliveSec, _PingSec.
- [ ] **70:** ConnectionDiagnostics binding for TgxlConnection.
- [ ] **71:** Run `ctest -R 'PgxlConnection|TgxlConnection|ConnectionDiagnostics'`; expect all green.
- [ ] **72:** Smoke-run NereusSDR with PGXL: connect, observe Diagnostics live RTT in PGXL Advanced (pending until Phase 4) - for now check via debug log `qCDebug(lcPgxl) << "rtt" << rttMs;`.
- [ ] **73:** Update CHANGELOG.md Phase 3P-II section.
- [ ] **74:** Phase 3 checkpoint commit.

```bash
git add CHANGELOG.md
git commit -m "chore(changelog): Phase 3P-II connection robustness checkpoint"
```

---

# Phase 4: Advanced UI + UX Wins

Estimated: 25 tasks. Net new code: ~700 LOC + 6 test files.

## Task 75: FaultLog class

**Files:**
- Create: `src/core/FaultLog.h` / `.cpp`
- Create: `tests/FaultLogTest.cpp`

- [ ] **Step 1: Implement per spec §4.7**

`FaultEvent` struct + `FaultLog` class with ring buffer of 10, JSON serialize/deserialize, `capture(ev)`, `clear()`, `events()`.

- [ ] **Step 2: likelyCause heuristic**

```cpp
QString FaultLog::likelyCauseFor(float fwd, float swr, float temp) {
    if (swr > 2.5f)      return "SWR trip";
    if (temp > 85.0f)    return "Overtemp";
    if (fwd > 1900.0f)   return "Drive too high";
    return "Unknown";
}
```

- [ ] **Step 3: Test the ring buffer + heuristic + persistence**

```cpp
void FaultLogTest::ringBufferKeepsNewestTen() {
    NereusSDR::FaultLog log("PGXL_FaultHistory");
    log.clear();
    for (int i = 0; i < 12; ++i) {
        log.capture({i * 1000, "FAULT", 1500.0f, 1.5f, 70.0f, "X"});
    }
    QCOMPARE(log.events().size(), 10);
    QCOMPARE(log.events().first().whenMs, qint64(11000));  // newest first
}

void FaultLogTest::likelyCauseSwrTrip() {
    auto cause = NereusSDR::FaultLog::likelyCauseFor(1500.0f, 3.0f, 60.0f);
    QCOMPARE(cause, QString("SWR trip"));
}
```

- [ ] **Step 4: Run, expect PASS**

- [ ] **Step 5: Commit**

```bash
git add src/core/FaultLog.h src/core/FaultLog.cpp tests/FaultLogTest.cpp CMakeLists.txt
git commit -m "feat(pgxl): FaultLog ring buffer with likelyCause heuristic"
```

## Task 76: TuneMemoryStore class

**Files:**
- Create: `src/core/TuneMemoryStore.h` / `.cpp`
- Create: `tests/TuneMemoryStoreTest.cpp`

- [ ] **Step 1: Implement per spec §4.8**

- [ ] **Step 2: Tests for recall/store/clear/persistence**

```cpp
void TuneMemoryStoreTest::recallReturnsStoredValue() {
    NereusSDR::TuneMemoryStore store;
    store.store({1, NereusSDR::Band::Band20m, 42, 199, 88, 12345000});
    auto rec = store.recall(1, NereusSDR::Band::Band20m);
    QVERIFY(rec.has_value());
    QCOMPARE(rec->c1, 42);
    QCOMPARE(rec->l, 199);
}
```

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/TuneMemoryStore.h src/core/TuneMemoryStore.cpp tests/TuneMemoryStoreTest.cpp CMakeLists.txt
git commit -m "feat(tgxl): TuneMemoryStore for per-(antenna,band) relay positions"
```

## Task 77: TxInterlockPolicy class

**Files:**
- Create: `src/core/TxInterlockPolicy.h` / `.cpp`
- Create: `tests/TxInterlockPolicyTest.cpp`

- [ ] **Step 1: Implement per spec §4.9**

Three modes, grace period, optional SWR gate. `evaluateTxRequest(ampPresent, ampInOperate, swr)` returns bool + emits warned/denied.

- [ ] **Step 2: Tests for all three modes**

```cpp
void TxInterlockPolicyTest::disabledAlwaysAllows() {
    NereusSDR::TxInterlockPolicy p;
    p.setMode(NereusSDR::TxInterlockPolicy::Disabled);
    QVERIFY(p.evaluateTxRequest(false, false, 99.0f));
}

void TxInterlockPolicyTest::blockDeniesWhenAmpStandby() {
    NereusSDR::TxInterlockPolicy p;
    p.setMode(NereusSDR::TxInterlockPolicy::Block);
    QSignalSpy spy(&p, &NereusSDR::TxInterlockPolicy::denied);
    QVERIFY(!p.evaluateTxRequest(true, false, 1.5f));
    QCOMPARE(spy.count(), 1);
}

void TxInterlockPolicyTest::warnAllowsButEmits() {
    NereusSDR::TxInterlockPolicy p;
    p.setMode(NereusSDR::TxInterlockPolicy::Warn);
    QSignalSpy spy(&p, &NereusSDR::TxInterlockPolicy::warned);
    QVERIFY(p.evaluateTxRequest(true, false, 1.5f));
    QCOMPARE(spy.count(), 1);
}
```

- [ ] **Step 3: Run, expect PASS**

- [ ] **Step 4: Commit**

```bash
git add src/core/TxInterlockPolicy.h src/core/TxInterlockPolicy.cpp tests/TxInterlockPolicyTest.cpp CMakeLists.txt
git commit -m "feat(tx): TxInterlockPolicy with Disabled/Warn/Block modes"
```

## Task 78: Setup PGXL Advanced page (skeleton + Identity)

**Files:**
- Create: `src/gui/setup/PgxlAdvancedPage.h` / `.cpp`

- [ ] **Step 1: Header with section method stubs**

```cpp
class PgxlAdvancedPage : public QWidget {
    Q_OBJECT
public:
    explicit PgxlAdvancedPage(RadioModel* model, QWidget* parent = nullptr);
private:
    void buildIdentitySection();
    void buildHardwareSection();
    void buildNetworkSection();
    void buildPairingSection();
    void buildDiagnosticsSection();
    void buildFaultHistorySection();
    void buildFooter();

    RadioModel* m_model{nullptr};
    QLineEdit*  m_nickname{nullptr};
    QLabel*     m_firmwareVersion{nullptr};
    // ... etc per spec §5.6
};
```

- [ ] **Step 2: Implement Identity & Status section**

Editable nickname, read-only firmware version + serial + current state badge.

- [ ] **Step 3: Wire to RadioModel + PgxlConnection signals**

- [ ] **Step 4: Build + smoke-run; navigate Setup > Network > PGXL Advanced (if visible); verify section renders**

- [ ] **Step 5: Stage**

```bash
git add src/gui/setup/PgxlAdvancedPage.h src/gui/setup/PgxlAdvancedPage.cpp
```

## Task 79-83: PGXL Advanced sections (5 tasks)

Each builds one section per spec §5.6:

- [ ] **79:** Hardware section: bias mode toggle, fan mode combo, LED slider, TX power cap. Calls `writeSetup` on change but marks pending Save & Reboot.
- [ ] **80:** Network section: DHCP toggle, IP/netmask/gateway fields. Calls `writeIfconf` on change.
- [ ] **81:** Pairing & band source: 3-option dropdown + TX antenna + slice binding toggles.
- [ ] **82:** Diagnostics section: 8-cell grid bound to ConnectionDiagnostics Q_PROPERTYs.
- [ ] **83:** Fault history: QTableView bound to FaultLog::events() + Clear All button.

Each task ends with a commit:

```bash
git add src/gui/setup/PgxlAdvancedPage.cpp
git commit -m "feat(setup-pgxl): <section name>"
```

## Task 84: Save & Reboot modal

**Files:**
- Create: `src/gui/PgxlSaveRebootDialog.h` / `.cpp`

- [ ] **Step 1: Modal QDialog with explanatory text + Cancel/Save buttons**

Per spec §5.6 footer. On confirm: emit `save()` via PgxlConnection, set page status to "Rebooting...", auto-reconnect with 20 s initial backoff handles the rest.

- [ ] **Step 2: Wire into PgxlAdvancedPage footer**

- [ ] **Step 3: Build + smoke-run**

- [ ] **Step 4: Commit**

```bash
git add src/gui/PgxlSaveRebootDialog.h src/gui/PgxlSaveRebootDialog.cpp src/gui/setup/PgxlAdvancedPage.cpp CMakeLists.txt
git commit -m "feat(setup-pgxl): Save & Reboot confirmation modal"
```

## Task 85: Setup TGXL Advanced (parallel)

**Files:**
- Create: `src/gui/setup/TgxlAdvancedPage.h` / `.cpp`

- [ ] **Step 1: Parallel implementation of spec §5.7**

5 sections: Identity, Antenna labels (TGXL-specific), Network, Tune memory management (bound to TuneMemoryStore::listAll()), Diagnostics, Fault history. Footer with Save.

- [ ] **Step 2: Build + smoke-run**

- [ ] **Step 3: Commit**

```bash
git add src/gui/setup/TgxlAdvancedPage.h src/gui/setup/TgxlAdvancedPage.cpp CMakeLists.txt
git commit -m "feat(setup-tgxl): TGXL Advanced page parallel to PGXL Advanced"
```

## Task 86: Setup PGXL Interlock (under Transmit)

**Files:**
- Create: `src/gui/setup/PgxlInterlockPage.h` / `.cpp`

- [ ] **Step 1: Three controls per spec §5.8**

Mode combo + grace spinbox + SWR gate checkbox/spinbox. Persists to AppSettings, mirrors live in TxInterlockPolicy.

- [ ] **Step 2: Register under Setup > Transmit category**

- [ ] **Step 3: Build + smoke-run**

- [ ] **Step 4: Commit**

```bash
git add src/gui/setup/PgxlInterlockPage.h src/gui/setup/PgxlInterlockPage.cpp src/gui/setup/TransmitSetupPages.cpp CMakeLists.txt
git commit -m "feat(setup-tx): PGXL Interlock policy page"
```

## Task 87: MoxController interlock hook

**Files:**
- Modify: `src/core/MoxController.cpp`

- [ ] **Step 1: Wire TxInterlockPolicy::evaluateTxRequest into onTxRequested**

```cpp
bool MoxController::onTxRequested() {
    bool ampOK = m_radioModel->hasAmplifier() && m_radioModel->ampOperate();
    float swr = m_radioModel->currentSwr();
    if (!m_interlockPolicy.evaluateTxRequest(m_radioModel->hasAmplifier(), ampOK, swr)) {
        return false;  // denied
    }
    // ... existing TX logic
}
```

- [ ] **Step 2: Build + smoke-run; verify with Block mode + STANDBY amp**

- [ ] **Step 3: Commit**

```bash
git add src/core/MoxController.cpp
git commit -m "feat(tx): wire TxInterlockPolicy into MoxController gate"
```

## Task 88: AmpApplet right-click menu

**Files:**
- Modify: `src/gui/applets/AmpApplet.h` / `.cpp`
- Create: `tests/AmpAppletContextMenuTest.cpp`

- [ ] **Step 1: Override contextMenuEvent**

```cpp
void AmpApplet::contextMenuEvent(QContextMenuEvent* ev) {
    QMenu menu(this);
    auto* openAdv = menu.addAction("Open PGXL Advanced...");
    connect(openAdv, &QAction::triggered, this, [this]() {
        emit navigationRequested("pgxlAdvanced");
    });
    menu.addSeparator();
    auto* disc = menu.addAction(m_connected ? "Disconnect" : "Reconnect");
    connect(disc, &QAction::triggered, this, [this]() {
        emit connectionToggleRequested();
    });
    auto* copy = menu.addAction("Copy diagnostics to clipboard");
    connect(copy, &QAction::triggered, this, [this]() {
        emit diagnosticsCopyRequested();
    });
    menu.exec(ev->globalPos());
}
```

- [ ] **Step 2: Test menu structure**

```cpp
void AmpAppletContextMenuTest::menuOpensPgxlAdvanced() {
    NereusSDR::AmpApplet a(nullptr);
    QSignalSpy spy(&a, &NereusSDR::AmpApplet::navigationRequested);
    QMenu* m = a.buildContextMenuForTesting();
    m->actions().first()->trigger();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QString("pgxlAdvanced"));
}
```

- [ ] **Step 3: Wire MainWindow to consume navigationRequested**

```cpp
connect(m_ampApplet, &AmpApplet::navigationRequested, this, [this](QString key) {
    openSetup(key);
});
```

- [ ] **Step 4: Run, expect PASS**

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/AmpApplet.h src/gui/applets/AmpApplet.cpp tests/AmpAppletContextMenuTest.cpp CMakeLists.txt
git commit -m "feat(pgxl): right-click AmpApplet opens PGXL Advanced"
```

## Task 89: TunerApplet right-click menu

**Files:**
- Modify: `src/gui/applets/TunerApplet.h` / `.cpp`
- Create: `tests/TunerAppletContextMenuTest.cpp`

- [ ] **Step 1: Override contextMenuEvent with tune-memory actions**

Menu structure per spec §5.9 TunerApplet. Save current tune memory calls `m_tuneMemoryStore->store(currentMem())`.

- [ ] **Step 2: Test**

```cpp
void TunerAppletContextMenuTest::saveCurrentMemoryStoresSlot() {
    NereusSDR::TuneMemoryStore store;
    NereusSDR::TunerApplet a(&store, nullptr);
    a.testSetCurrentBandAndAntenna(NereusSDR::Band::Band20m, 1);
    a.testSetRelayValues(42, 199, 88);
    QMenu* m = a.buildContextMenuForTesting();
    // Find and trigger "Save current tune memory"
    for (auto* act : m->actions()) {
        if (act->text().contains("Save current tune memory")) {
            act->trigger();
            break;
        }
    }
    auto rec = store.recall(1, NereusSDR::Band::Band20m);
    QVERIFY(rec.has_value());
    QCOMPARE(rec->c1, 42);
}
```

- [ ] **Step 3: Wire MainWindow**

- [ ] **Step 4: Run, expect PASS; Commit**

```bash
git add src/gui/applets/TunerApplet.h src/gui/applets/TunerApplet.cpp tests/TunerAppletContextMenuTest.cpp CMakeLists.txt
git commit -m "feat(tgxl): right-click TunerApplet opens TGXL Advanced + memory shortcuts"
```

## Task 90: MainWindow openSetup navigation API

**Files:**
- Modify: `src/gui/MainWindow.h` / `.cpp`

- [ ] **Step 1: Add `openSetup(QString pageKey)` method**

```cpp
void MainWindow::openSetup(const QString& pageKey) {
    if (!m_setupDialog) m_setupDialog = new SetupDialog(this);
    m_setupDialog->showPage(pageKey);  // pageKey: "pgxlAdvanced", "tgxlAdvanced", "pgxlInterlock", "peripherals"
    m_setupDialog->show();
    m_setupDialog->raise();
}
```

- [ ] **Step 2: SetupDialog::showPage maps key -> page index**

Add a `QHash<QString, QWidget*> m_pagesByKey` populated at construction.

- [ ] **Step 3: Build + smoke-run; right-click AmpApplet -> "Open PGXL Advanced..." -> dialog opens at right page**

- [ ] **Step 4: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp src/gui/setup/SetupDialog.h src/gui/setup/SetupDialog.cpp
git commit -m "feat(setup): openSetup(pageKey) navigation API for applet right-click"
```

## Tasks 91-100: Phase 4 polish + final integration (10 tasks)

- [ ] **91:** PgxlConnection.readSetup/writeSetup + tests (PgxlConnectionSetupTest).
- [ ] **92:** PgxlConnection.readIfconf/writeIfconf + tests (PgxlConnectionIfconfTest).
- [ ] **93:** PgxlConnection.save + tests (PgxlConnectionSaveTest).
- [ ] **94:** Hook FaultLog::capture into RadioModel's PGXL status handler when state begins with `FAULT`.
- [ ] **95:** Antenna label persistence: TGXL_Ant1_Label/_Ant2_Label/_Ant3_Label round-trip + TunerApplet button text update.
- [ ] **96:** TGXL tune memory auto-recall on bandChanged (per spec; absolute-write verb bench-confirmable, fall back to `tune start`).
- [ ] **97:** Power cap soft-alert toast in MainWindow when peakfwd > `PGXL_PowerCapW`.
- [ ] **98:** Bench verification matrix README scaffold at `docs/architecture/phase-pgxl-tgxl-smeter-verification/README.md` with all 36 rows from spec §10.2.
- [ ] **99:** Update `docs/MASTER-PLAN.md` to add Phase 3P-II entry per spec §13.
- [ ] **100:** Final CHANGELOG.md entry + PR description draft (in `docs/superpowers/pr-drafts/<branch>.md` if that convention exists, else inline).

```bash
git add docs/MASTER-PLAN.md CHANGELOG.md docs/architecture/phase-pgxl-tgxl-smeter-verification/README.md
git commit -m "docs: Phase 3P-II completion notes + bench verification matrix"
```

## Task 101: Final integration verification

**Files:**
- Modify: none (verification only)

- [ ] **Step 1: Full clean build**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

Expected: no errors, no new warnings.

- [ ] **Step 2: Full test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all green, including the 14 new tests.

- [ ] **Step 3: Smoke-run with hardware (if available) - walk through 5 representative bench rows**

Manually drive rows 6, 11, 19, 22, 29 from the bench matrix as confidence checks.

- [ ] **Step 4: PR readiness**

```bash
git log --oneline origin/main..HEAD | wc -l   # commit count
git diff --stat origin/main..HEAD             # diff size
```

Expected: ~100 commits, ~3000 lines insertions.

- [ ] **Step 5: Self-review against spec**

Walk every spec section (1-13). For each requirement, point to the task that implements it. Any gaps go back into the plan as additional tasks before opening the PR.

---

## Plan Self-Review Checklist

Use this checklist after completing the plan but before starting Task 1. Run through it once; fix issues inline.

**Spec coverage:**

- [ ] §4.1 PgxlConnection -> Tasks 3-7 (skeleton + parse) and 55-59 (extended slots).
- [ ] §4.2 TgxlConnection -> Tasks 8-9 (skeleton + parse) and 60 (extended slots).
- [ ] §4.3 TunerModel -> Tasks 10-11.
- [ ] §4.4 RelayBar -> Task 12.
- [ ] §4.5 LanDiscovery -> Tasks 13-14.
- [ ] §4.6 ConnectionDiagnostics -> Task 61.
- [ ] §4.7 FaultLog -> Task 75.
- [ ] §4.8 TuneMemoryStore -> Task 76.
- [ ] §4.9 TxInterlockPolicy -> Task 77.
- [ ] §5.1 AmpApplet -> Task 15.
- [ ] §5.2 TunerApplet rewire -> Task 16.
- [ ] §5.3 Peripherals page -> Task 17.
- [ ] §5.4 SMeterWidget + right-click + Sig Avg + Max Bin -> Tasks 31-43.
- [ ] §5.5 Status bar TGXL chip -> Task 21.
- [ ] §5.6 PGXL Advanced -> Tasks 78-84.
- [ ] §5.7 TGXL Advanced -> Task 85.
- [ ] §5.8 PGXL Interlock -> Task 86.
- [ ] §5.9 Right-click menus -> Tasks 88-90.
- [ ] §6.4 Extended command reference -> Tasks 55-59, 91-93.
- [ ] §7 Data flow -> Cross-cutting; covered by Tasks 19-20, 41-43, 62-66, 87.
- [ ] §8 Persistence -> Tasks 2, 44-46, 68-69, 91-95.
- [ ] §10 Testing -> 14 unit tests (Tasks 4-11, 33, 36-37, 39, 56-58, 61, 75-77, 88-89, 91-93).
- [ ] §10.2 Bench matrix -> Task 98 (README scaffold) + Task 101 (representative rows).

**Type consistency:**

- [ ] `PgxlConnection::sendCommand` returns `quint32` (seq).
- [ ] `PgxlConnection::pong/ping` use `quint32 seq` consistently.
- [ ] `FaultEvent::whenMs` is `qint64` (matches `QDateTime::currentMSecsSinceEpoch()`).
- [ ] `TuneMemory::antenna` is `int` (1..3, not 0..2).
- [ ] `Band` enum is the existing `src/models/Band.h` enum.
- [ ] `SMeterWidget::RxMode` has exactly four entries: `SMeter, SignalAverage, SMeterPeak, MaxBin`.
- [ ] `SMeter_RxSelect` integer maps to enum: 0=SMeter, 1=SignalAverage, 2=SMeterPeak, 3=MaxBin.

**Placeholder scan:**

- [ ] No "TBD", "TODO", "FIXME" in plan text.
- [ ] Every code-changing step has a code block.
- [ ] Cross-task references are by task number, not "Similar to earlier".

If any gap surfaces, add the task inline. The plan is the contract for execution; treat it as such.

---

## Execution Handoff

Plan complete and saved to `docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md`. Two execution options:

1. **Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
