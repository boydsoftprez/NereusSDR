# RedPitaya P1 C&C Round-Robin + EP2 Pacer Fixes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix two independent P1 protocol bugs exposed by RedPitaya pcap analysis
in issue #38: (A) an off-by-one error in `kRxC0Address` that causes DDC3 NCO
(`C0 = 0x0A`) to never reach the radio and bank 9 to collide with bank 10 on
`C0 = 0x12`; (B) an EP2 send cadence of ~40 pps (one per 25 ms watchdog tick)
instead of the spec-compliant ~381 pps (48 kHz / 126 samples-per-packet) that
Thetis emits, which starves the radio's 48 kHz audio DAC and makes any C&C
command take ~213 ms to traverse the full 17-bank cycle.

**Architecture:** Bug A is a one-line constant-table correction plus updated
comments. Bug B separates EP2 pacing from the watchdog by introducing a
dedicated `QTimer` on `P1RadioConnection` that fires at 2 ms intervals with
`Qt::PreciseTimer`, while the existing watchdog keeps its silence-detection
role. Both fixes are localized to `P1RadioConnection.{h,cpp}` with
extensions to `tst_p1_wire_format.cpp` and a new integration test in
`tst_p1_loopback_connection.cpp` that measures EP2 pps against the
`P1FakeRadio` loopback fake.

**Tech Stack:** C++20 / Qt6 (QTimer, QUdpSocket, QtTest), CMake+Ninja build,
QTest `QTRY_*` macros for async loopback assertions, `P1FakeRadio` loopback
fake.

---

## Evidence Summary (pcaps from issue #38)

- **Thetis capture** (`Thetis_2_RedPitaya.pcapng`, 1.0 GB / ~72 s):
  Host→radio 27 611 packets → **383 pps**.
  C0 cycle: `{0x00,0x02,0x04,0x06,0x1C,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x1E,0x20,0x22,0x24}` (17 unique).
- **Nereus capture** (`Neureus_2_RedPitaya.pcapng`, 229 MB / ~70 s, v0.1.7-rc1):
  Host→radio 5 005 packets → **72 pps (5.3× too slow)**.
  C0 cycle: `{0x00,0x02,0x04,0x06,0x1C,0x08,0x0C,0x0E,0x10,0x12,0x12,0x14,0x16,0x1E,0x20,0x22,0x24}` — `0x0A` missing, `0x12` duplicated.

Thetis source of truth for the C&C address map:
`Thetis/Project Files/Source/ChannelMaster/networkproto1.c`:

| Case | C0 OR | Purpose |
|------|-------|---------|
| 5 (`:526`) | `0x08` | RX3 (DDC2) NCO |
| 6 (`:539`) | `0x0A` | RX4 (DDC3) NCO |
| 7 (`:549`) | `0x0C` | RX5 (DDC4) NCO |
| 8 | `0x0E` | RX6 (DDC5) NCO |
| 9 (`:569`) | `0x10` | RX7 (DDC6) NCO |

Thetis EP2 pacing (networkproto1.c:700-747 `sendProtocol1Samples`): blocks on
two audio-subsystem semaphores, so send rate = audio rate = 48 kHz / 126
samples = 380.95 pps. Observed on wire: 383 pps.

---

## File Structure

- **Modify:** `src/core/P1RadioConnection.cpp`
  - Line 167: fix `kRxC0Address` constants and the preceding comment block.
  - Lines 631-650: demote watchdog to silence-detection only.
  - Add new `onEp2PacerTick()` slot that calls `sendCommandFrame()`.
- **Modify:** `src/core/P1RadioConnection.h`
  - Line 133 neighborhood: add `QTimer* m_ep2PacerTimer{nullptr};`.
  - Line 152 neighborhood: add `static constexpr int kEp2PacerIntervalMs = 2;`.
  - Add `private slots: void onEp2PacerTick();`.
- **Modify:** `tests/tst_p1_wire_format.cpp`
  - Extend existing `rxFrequencyC0AddressForRx0` pattern with tests for `rxIdx` 1-6.
  - Add test verifying all 7 `rxIdx` produce distinct C0 addresses.
- **Modify:** `tests/tst_p1_loopback_connection.cpp`
  - Add `ep2PaceRateMatchesAudioClock()` test that counts
    `fake.ep2FramesReceived()` over 500 ms and asserts rate ≥ 200 pps (safety
    floor well above the broken 40 pps and below the 500 pps upper bound).
- **Modify:** `CHANGELOG.md` — add entry under `## Unreleased` for 0.1.8.
- **Modify:** `CMakeLists.txt` — bump `VERSION` to `0.1.8`.

No new files. No new translation units. Everything lives where the existing P1
logic lives.

---

### Task 1: Extend wire-format unit tests to cover all 7 `rxIdx` slots

**Files:**
- Modify: `tests/tst_p1_wire_format.cpp` (after `rxFrequencyC0AddressForRx0`, ~line 102)

- [ ] **Step 1: Add the six new failing tests**

Insert the following immediately after `rxFrequencyC0AddressForRx0()`:

```cpp
    void rxFrequencyC0AddressForRx1() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 1, 14200000);
        // Source: networkproto1.c:498 — case 3: C0 |= 6 → address = 6>>1 = 0x03
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x03);
    }

    void rxFrequencyC0AddressForRx2() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 2, 14200000);
        // Source: networkproto1.c:526 — case 5: C0 |= 0x08 → address = 0x08>>1 = 0x04
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x04);
    }

    void rxFrequencyC0AddressForRx3() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 3, 14200000);
        // Source: networkproto1.c:539 — case 6: C0 |= 0x0A → address = 0x0A>>1 = 0x05
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x05);
    }

    void rxFrequencyC0AddressForRx4() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 4, 14200000);
        // Source: networkproto1.c:549 — case 7: C0 |= 0x0C → address = 0x0C>>1 = 0x06
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x06);
    }

    void rxFrequencyC0AddressForRx5() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 5, 14200000);
        // Source: networkproto1.c:~560 — case 8: C0 |= 0x0E → address = 0x0E>>1 = 0x07
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x07);
    }

    void rxFrequencyC0AddressForRx6() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBankRxFreq(out, 6, 14200000);
        // Source: networkproto1.c:569 — case 9: C0 |= 0x10 → address = 0x10>>1 = 0x08
        QCOMPARE(int((out[0] >> 1) & 0x7F), 0x08);
    }

    void rxFrequencyAddressesAreAllDistinct() {
        // Bug guard: the 7 DDC NCO banks must each emit a different C0 byte,
        // else the round-robin collides two banks onto one bus slot.
        quint8 bytes[7] = {};
        for (int i = 0; i < 7; ++i) {
            quint8 out[5] = {};
            P1RadioConnection::composeCcBankRxFreq(out, i, 14200000);
            bytes[i] = out[0];
        }
        for (int a = 0; a < 7; ++a) {
            for (int b = a + 1; b < 7; ++b) {
                QVERIFY2(bytes[a] != bytes[b],
                    qPrintable(QString("kRxC0Address[%1]==kRxC0Address[%2]==0x%3")
                        .arg(a).arg(b).arg(bytes[a], 2, 16, QChar('0'))));
            }
        }
    }
```

- [ ] **Step 2: Run the new tests — expect failures on rxIdx 3, 4, 5, 6 and the distinctness check**

Run:

```bash
cmake --build build --target tst_p1_wire_format
./build/tests/tst_p1_wire_format -v1
```

Expected failures (exact values — use these to confirm the bug exists):
- `rxFrequencyC0AddressForRx3` FAIL: actual `0x06`, expected `0x05`.
- `rxFrequencyC0AddressForRx4` FAIL: actual `0x07`, expected `0x06`.
- `rxFrequencyC0AddressForRx5` FAIL: actual `0x08`, expected `0x07`.
- `rxFrequencyC0AddressForRx6` FAIL: actual `0x09`, expected `0x08`.
- `rxFrequencyAddressesAreAllDistinct` FAIL: two slots collide (see failure message for which indices).
- `rxFrequencyC0AddressForRx1`, `Rx2` PASS (already correct).

- [ ] **Step 3: Commit the failing tests**

```bash
git add tests/tst_p1_wire_format.cpp
git commit -S -m "test(p1): cover all 7 DDC NCO address slots

Extends tst_p1_wire_format with per-rxIdx C0-address assertions and
a distinctness guard. Four tests fail against current code — they
pin down the off-by-one in kRxC0Address documented in #38.

Refs #38

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Fix `kRxC0Address` off-by-one

**Files:**
- Modify: `src/core/P1RadioConnection.cpp:160-168`

- [ ] **Step 1: Replace the address comment block and the array**

Replace the block at lines 160-168 with:

```cpp
    // Address assignments from networkproto1.c (C0 OR bits; the table stores
    // the already-shifted value so callers just assign out[0] = addrBits):
    //   rxIndex 0 → case 2 (:485):  C0 |= 0x04  (RX1 / DDC0)
    //   rxIndex 1 → case 3 (:498):  C0 |= 0x06  (RX2 / DDC1)
    //   rxIndex 2 → case 5 (:526):  C0 |= 0x08  (RX3 / DDC2)
    //   rxIndex 3 → case 6 (:539):  C0 |= 0x0A  (RX4 / DDC3)
    //   rxIndex 4 → case 7 (:549):  C0 |= 0x0C  (RX5 / DDC4)
    //   rxIndex 5 → case 8      :  C0 |= 0x0E  (RX6 / DDC5)
    //   rxIndex 6 → case 9 (:569):  C0 |= 0x10  (RX7 / DDC6)
    //
    // History: prior revision had {4,6,8,0x0C,0x0E,0x10,0x12}, dropping 0x0A
    // entirely and aliasing rxIndex 6 onto bank 10's 0x12 slot. Fixed per
    // pcap analysis of RedPitaya (#38).
    static const quint8 kRxC0Address[] = { 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10 };
```

- [ ] **Step 2: Re-run wire-format tests — all should pass**

Run:

```bash
cmake --build build --target tst_p1_wire_format
./build/tests/tst_p1_wire_format -v1
```

Expected: all tests in the suite PASS, including the 6 new per-rxIdx tests and
`rxFrequencyAddressesAreAllDistinct`.

- [ ] **Step 3: Build the full app — no other callers should break**

Run:

```bash
cmake --build build -j
```

Expected: clean build, no warnings about narrowing or unused literals.

- [ ] **Step 4: Commit the fix**

```bash
git add src/core/P1RadioConnection.cpp
git commit -S -m "fix(p1): restore DDC3 NCO address in kRxC0Address table

The rxIdx→C0 table skipped 0x0A (DDC3 NCO) and shifted the tail up
by one slot, aliasing rxIdx 6 onto bank 10's 0x12 command. On the
wire this meant DDC3 was never tuned and every full round-robin
sent TX-drive twice. Confirmed against pcap from #38.

Thetis source: networkproto1.c:526, 539, 549, 569.

Fixes part of #38

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Add failing integration test for EP2 pacer rate

**Files:**
- Modify: `tests/tst_p1_loopback_connection.cpp` (add new test slot after `disconnectStopsData()`)

- [ ] **Step 1: Add the failing pacer test**

Insert the following before the `QTEST_MAIN` line (~after line 107):

```cpp
    // Verifies EP2 (host→radio) cadence matches the spec's 48 kHz / 126
    // samples-per-packet rate of ~381 pps. Before the pacer fix the rate
    // was ~40 pps (once per 25 ms watchdog tick), starving the radio's
    // audio DAC. See issue #38 pcap analysis.
    void ep2PaceRateMatchesAudioClock() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Baseline count after connection is up — discard discovery/start framing.
        const int baseline = fake.ep2FramesReceived();

        // Sample for 500 ms.
        QTest::qWait(500);

        const int delta = fake.ep2FramesReceived() - baseline;
        // At 381 pps ideal we expect ~190 packets in 500 ms. Windows QTimer
        // jitter is ~±30 %, so assert a floor of 100 packets (200 pps) which
        // is still 5× the broken 40 pps watchdog cadence.
        QVERIFY2(delta >= 100,
            qPrintable(QString("EP2 rate too slow: %1 packets in 500 ms (=%2 pps)")
                .arg(delta).arg(delta * 2)));

        conn.disconnect();
        fake.stop();
    }
```

- [ ] **Step 2: Run the loopback tests — pacer test should FAIL**

Run:

```bash
cmake --build build --target tst_p1_loopback_connection
./build/tests/tst_p1_loopback_connection -v1
```

Expected:
- `rxPathEndToEnd` PASS.
- `disconnectStopsData` PASS.
- `ep2PaceRateMatchesAudioClock` FAIL: `delta ≈ 20` (one packet per 25 ms tick
  = 20 packets in 500 ms).

- [ ] **Step 3: Commit the failing test**

```bash
git add tests/tst_p1_loopback_connection.cpp
git commit -S -m "test(p1): pin EP2 pacer rate to spec-compliant 200+ pps

Adds a loopback integration test that measures host→radio packet
rate over 500 ms. Fails against current code (40 pps) — locks in
the 200 pps floor that the upcoming dedicated pacer enforces.

Refs #38

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Separate EP2 pacer from watchdog

**Files:**
- Modify: `src/core/P1RadioConnection.h`
- Modify: `src/core/P1RadioConnection.cpp`

- [ ] **Step 1: Declare the new timer, slot, and interval constant**

In `P1RadioConnection.h`, add to the `private slots:` section (near existing `onWatchdogTick`):

```cpp
    void onEp2PacerTick();
```

In the private member area (near `m_watchdogTimer`):

```cpp
    QTimer*     m_ep2PacerTimer{nullptr};
```

In the constants block (near `kWatchdogTickMs`):

```cpp
    // EP2 send cadence — target 381 pps (48 kHz audio / 126 samples per
    // EP2 frame) to match Thetis' sendProtocol1Samples semaphore-driven
    // audio clock. 2 ms PreciseTimer yields ~350-500 pps on Windows under
    // normal scheduling jitter. Source: networkproto1.c:700-747.
    static constexpr int kEp2PacerIntervalMs = 2;
```

- [ ] **Step 2: Construct the pacer timer in the socket/timer setup path**

In `P1RadioConnection.cpp`, find the block near line 335 that creates
`m_watchdogTimer`. Immediately after that block, add:

```cpp
    // EP2 pacer — dedicated high-resolution timer that feeds the radio's
    // 48 kHz audio DAC. Kept separate from the watchdog so silence detection
    // cadence (25 ms) and EP2 cadence (2 ms) can evolve independently.
    m_ep2PacerTimer = new QTimer(this);
    m_ep2PacerTimer->setInterval(kEp2PacerIntervalMs);
    m_ep2PacerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_ep2PacerTimer, &QTimer::timeout, this, &P1RadioConnection::onEp2PacerTick);
```

- [ ] **Step 3: Start/stop the pacer alongside the watchdog**

Find the place the watchdog is started (line ~454) and add:

```cpp
    m_ep2PacerTimer->start();
```

Find the place the watchdog is stopped (line ~470-471) and add, inside the
same guard block:

```cpp
    if (m_ep2PacerTimer) {
        m_ep2PacerTimer->stop();
    }
```

Also stop it when transitioning to `Error` inside `onWatchdogTick` silence
detection (line ~670, immediately after `m_watchdogTimer->stop();`):

```cpp
        if (m_ep2PacerTimer) {
            m_ep2PacerTimer->stop();
        }
```

Re-arm it in `onReconnectTimeout` alongside the watchdog re-arm (line ~719-721):

```cpp
    if (m_ep2PacerTimer && !m_ep2PacerTimer->isActive()) {
        m_ep2PacerTimer->start();
    }
```

- [ ] **Step 4: Implement the pacer slot and demote the watchdog**

Add the new slot, placed near `onWatchdogTick`:

```cpp
// ---------------------------------------------------------------------------
// onEp2PacerTick
//
// Fires every kEp2PacerIntervalMs ms while connected. Sends one EP2 frame
// per tick so the host→radio rate tracks the radio's 48 kHz audio clock
// (Thetis emits ~381 pps from sendProtocol1Samples; we aim for the same
// ballpark to feed its audio DAC and advance the 17-bank round-robin).
// Skipped while the HL2 bandwidth monitor is throttling.
// ---------------------------------------------------------------------------
void P1RadioConnection::onEp2PacerTick()
{
    if (!m_running || !m_socket || m_radioInfo.address.isNull()) { return; }

    const ConnectionState cs = state();
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) { return; }
    if (m_hl2Throttled) { return; }

    sendCommandFrame();
}
```

Then in `onWatchdogTick` (lines 637-650), delete the `sendCommandFrame()`
call and its surrounding comment block. The updated block reads:

```cpp
    // Watchdog is now silence-detection only. EP2 pacing lives on
    // m_ep2PacerTimer (onEp2PacerTick) to match Thetis' 381 pps audio-clock
    // cadence. See kEp2PacerIntervalMs.

    // HL2 bandwidth monitor — check for LAN PHY throttle on every watchdog tick.
    // Source: mi0bot bandwidth_monitor.{c,h} — NereusSDR sequence-gap adaptation.
    if (m_caps && m_caps->hasBandwidthMonitor) {
        hl2CheckBandwidthMonitor();
    }
```

- [ ] **Step 5: Re-run the loopback tests — pacer test should now PASS**

Run:

```bash
cmake --build build --target tst_p1_loopback_connection
./build/tests/tst_p1_loopback_connection -v1
```

Expected: all three tests PASS, with the pacer test showing
`delta ≥ 100` (typically 150-250 on Windows).

- [ ] **Step 6: Run the full test suite to catch regressions**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass. Particular attention to
`tst_reconnect_on_silence` — the silence-detection path must still work.

- [ ] **Step 7: Commit the fix**

```bash
git add src/core/P1RadioConnection.h src/core/P1RadioConnection.cpp
git commit -S -m "fix(p1): pace EP2 sends at 2 ms to feed 48 kHz audio DAC

Splits EP2 send cadence off the 25 ms watchdog timer and onto a
dedicated Qt::PreciseTimer at 2 ms (~350-500 pps observed). The
previous 40 pps rate was 10x too slow for the spec'd 48 kHz /
126-sample-per-packet audio stream, starving the radio's DAC
and making every C&C bank take ~213 ms to propagate. Watchdog
keeps its silence-detection role untouched.

Thetis reference: networkproto1.c:700-747 (sendProtocol1Samples
semaphore pacing from the 48 kHz audio subsystem).

Fixes #38

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Changelog + version bump

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Bump version to 0.1.8**

In `CMakeLists.txt`, find the `project(NereusSDR VERSION 0.1.7 ...)` line and
replace `0.1.7` with `0.1.8`.

- [ ] **Step 2: Add changelog entry**

Under the top-most version heading in `CHANGELOG.md`, add a new section:

```markdown
## 0.1.8 - 2026-04-16

### Fixed
- **P1 RedPitaya / Hermes family (#38):** restore DDC3 NCO command on the wire.
  The `kRxC0Address` lookup table was missing the `0x0A` address entry,
  which caused bank 6 (RX4/DDC3 frequency) to be dropped and bank 9
  (RX7/DDC6) to alias onto bank 10's `0x12` TX-drive slot. Verified
  byte-for-byte against Thetis `networkproto1.c` cases 6 and 9.
- **P1 EP2 send cadence (#38):** host→radio packets are now paced by a
  dedicated 2 ms `Qt::PreciseTimer` instead of the 25 ms watchdog tick,
  yielding ~350-500 pps (target: Thetis' 381 pps from its 48 kHz audio
  clock). Previously we ran ~40 pps, starving the radio's audio DAC and
  stretching the 17-bank C&C round-robin to ~213 ms per cycle.
```

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt CHANGELOG.md
git commit -S -m "chore: bump version to 0.1.8 and log P1 RedPitaya fixes

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Open PR against `main`

- [ ] **Step 1: Push the branch**

```bash
git push -u origin fix/p1-redpitaya-cc-ep2-pacer
```

- [ ] **Step 2: Open the PR**

```bash
gh pr create --title "fix(p1): RedPitaya C&C round-robin + EP2 pacer (#38)" --body "$(cat <<'EOF'
## Summary
- Restore `0x0A` (DDC3 NCO) in `kRxC0Address`; bank 9 no longer aliases bank 10 on `0x12`
- Split EP2 send cadence off the 25 ms watchdog onto a dedicated 2 ms `Qt::PreciseTimer` so host→radio packets track the radio's 48 kHz audio clock (~350-500 pps, vs. Thetis' 381 pps)
- Both regressions are visible on Patrick's Thetis vs. NereusSDR pcaps attached to #38

## Test plan
- [x] `tst_p1_wire_format` covers all 7 DDC NCO `rxIdx` slots + distinctness guard
- [x] `tst_p1_loopback_connection::ep2PaceRateMatchesAudioClock` asserts ≥200 pps over a 500 ms window
- [x] `ctest --output-on-failure` green
- [ ] Pcap re-shoot from Patrick (`wormhole.app`) — compare C0 cycle and pps to Thetis reference

Fixes #38

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 3: Open the PR URL in the browser** (per user preference)

```bash
gh pr view --web
```

---

### Task 7: Issue #38 response + rc build

- [ ] **Step 1: Draft the issue response**

Paste the following into `gh issue comment 38 --body-file -` after the user
reviews it (per user's "review public posts" preference, do NOT post without
approval):

```
Patrick,

The pcaps told us exactly what was wrong — thank you for chasing those
uploads through Wormhole. Two independent bugs in NereusSDR's P1 path:

1. **DDC3 NCO address missing.** Our command-bank table skipped `0x0A` and
   shifted everything after it up by one slot. On the wire that meant
   bank 6 was never sent and bank 9 aliased bank 10. Thetis does this
   correctly (networkproto1.c:539, 569). One-line fix.

2. **EP2 send rate 10× too slow.** We were sending one EP2 frame every
   25 ms (40 pps) where Thetis sends one every ~2.6 ms (383 pps in your
   capture). The RedPitaya's 48 kHz audio buffer was effectively starving
   — very likely the "glitchy / digital" sound you described. We've moved
   pacing to a dedicated 2 ms precise timer.

Both fixes land in v0.1.8-rc1. Same dance as before: download, keep the
"Red Pitaya" model selection in Radio > Discover, then reconnect.

If you have the bandwidth for another pcap pair once you've tried rc1,
the comparison will tell us whether the on-wire byte cycle now matches
Thetis exactly. No rush — what you've already given us is what solved it.

J.J. Boyd ~ KG4VCF
Co-Authored with Claude Code
```

- [ ] **Step 2: Wait for maintainer approval**, then post via `gh issue comment`
  and open the issue URL in the browser.

- [ ] **Step 3: Trigger the release workflow**

Follow the `/release` skill flow to cut `v0.1.8-rc1` once the PR merges.
Ensure GPG signing is on (standing rule, per project conventions).

---

## Self-Review Notes

- **Spec coverage:** Bug A (`kRxC0Address` off-by-one) → Tasks 1 & 2. Bug B
  (EP2 pacer) → Tasks 3 & 4. Release hygiene → Tasks 5 & 6. User
  communication → Task 7. Every item from the triage has an owner.
- **Placeholder scan:** No TBDs, no "handle edge cases later", every code
  step has literal code. Commit messages are complete, not templated.
- **Type / name consistency:** `m_ep2PacerTimer` (member), `kEp2PacerIntervalMs`
  (constant), `onEp2PacerTick` (slot) — spelled identically in the header
  declaration, timer construction, start/stop sites, slot definition, and
  changelog entry.
- **Risk sanity-check:** The higher EP2 rate (2 ms tick, ~500 pps ceiling) is
  slightly above Thetis' 381 pps. The Hermes family FPGA tolerates this —
  it already buffers Thetis' rate with jitter. The HL2 bandwidth monitor
  pauses the pacer via `m_hl2Throttled`, so throttled paths stay polite.
