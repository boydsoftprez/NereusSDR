# 4O3A LAN PTT Chain Pcap-Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align NereusSDR's SmartSDR API listener wire format with canonical FlexRadio behavior so that 4O3A PowerGenius XL and Tuner Genius XL accessories complete the Ethernet Interlock handshake reliably and stop reporting "no PTT in" / "high SWR" during LAN-driven TUNE and MOX cycles.

**Architecture:** Six independent changes (C1 to C6) to `src/core/SmartSdrApiListener.cpp` that each modify one piece of the handshake or PTT chain. Order matters: C1 establishes a synthetic local-client handle that C2/C3/C5 reference; C4 inserts a 30 ms settle delay; C5 rewrites the un-key path; C6 retires the explicit `pttA=` pushes. Each change ships with a new unit test exercising the wire format, drives one focused commit, and feeds into a final 10-cycle manual bench matrix that is the acceptance gate.

**Tech Stack:** C++20, Qt6 (QTcpServer + QTcpSocket + QTimer + QtTest), CMake, ctest, GPG-signed git commits. Pre-commit hooks enforce Thetis header attribution, inline cite preservation, and compliance inventory.

**Source of truth:** `docs/architecture/4o3a-lan-ptt-pcap-divergence.md` (commit 559890a2) is the authoritative design spec. §8 defines C1 to C8; this plan implements C1 to C6 only. C7 and C8 are deferred to a follow-up after bench-green.

**Standing rules (apply to every commit in this plan):**
- GPG-sign every commit. Never `--no-gpg-sign`.
- No em-dashes in any drafted text (commit messages, code comments, doc edits). Use periods, colons, semicolons, parens, commas; hyphens and arrows are fine.
- No `Co-Authored-By: Claude` git trailer.
- Run only the new test 2x per commit (flake-check). Pre-commit hooks already run the verifiers. Full ctest runs once at the epic gate (Task 7) before bench.
- Inline cite for every code change: `// 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C<n>: <short reason>`.
- Stage specific files only when committing. Never `git add -A`.

---

## File Structure

| File | Responsibility | Status |
|---|---|---|
| `src/core/SmartSdrApiListener.h` | Class interface. Gains `m_localClientHandle` member, `localClientHandle()` const accessor for tests, `m_lastTuneInitiator` for C2, `start(QHostAddress, quint16)` overload for tests. | Modify |
| `src/core/SmartSdrApiListener.cpp` | Handshake + PTT chain logic. Modified in 6 commits, one per change. | Modify |
| `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` | New test file. Grows from a smoke test (Task 0) through one or more functions per task. | Create + extend |
| `tests/CMakeLists.txt` | Register the new test executable. | Modify (once, Task 0) |

`SmartSdrApiListener.cpp` is ~1034 lines today. We do NOT split it as part of this plan: the changes are concentrated in `setInterlockTransmitting`, `advanceToTransmittingIfReady`, `onPttAckTimeout`, and the existing `dispatchLine` `transmit tune` branch. A future epic can revisit the file's size if the LAN PTT subsystem grows further.

The new test file uses a real loopback `QTcpServer` (already inside the listener) on an ephemeral port. Tests connect a `QTcpSocket` from inside the test process, send canned C-frames as if it were a real amp, and read back the listener's S-frames. This matches what Wireshark on the bench would record, so frame-format assertions translate directly to bench expectations. Tests cannot prove TGXL parses the frames correctly. That is what the §9 10-cycle bench matrix is for. Unit tests prove frame format only.

---

## Task 0: Test Harness Foundation

**Goal:** Add the minimum machinery needed for subsequent tasks to drive the listener from a unit test. No behavior change to production code.

**Files:**
- Modify: `src/core/SmartSdrApiListener.h:53` (add `start(QHostAddress, quint16)` overload)
- Modify: `src/core/SmartSdrApiListener.cpp:39-57` (refactor `start()` to delegate)
- Create: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp`
- Modify: `tests/CMakeLists.txt` (add `nereus_add_test(tst_smart_sdr_api_listener_ptt_chain)` near the existing PTT-adjacent tests, e.g. after `tst_tx_interlock_policy`)

- [ ] **Step 1: Add the new `start(addr, port)` overload to the header.**

Edit `src/core/SmartSdrApiListener.h` so the `start()` declaration becomes a pair:

```cpp
    // Production entry point: binds to AnyIPv4:4992. Equivalent to
    // start(QHostAddress::AnyIPv4, 4992).
    bool start();

    // Test seam: bind to a caller-chosen address and port. Used by the
    // PTT-chain unit tests to drive a listener on loopback + ephemeral port
    // so multiple test cases can run concurrently and the production 4992
    // bind is not required. Production code never calls this overload.
    bool start(QHostAddress bindAddr, quint16 port);
    void stop();
    bool isListening() const;
```

- [ ] **Step 2: Refactor the existing `start()` to delegate.**

In `src/core/SmartSdrApiListener.cpp` lines 39-57, replace the body with:

```cpp
bool SmartSdrApiListener::start()
{
    // AnyIPv4 (not Any) because Qt's default Any binds IPv6-only on macOS,
    // which silently blocks IPv4 clients like Windows PowerGeniusDesktop.
    return start(QHostAddress::AnyIPv4, 4992);
}

bool SmartSdrApiListener::start(QHostAddress bindAddr, quint16 port)
{
    if (m_server.isListening()) {
        m_server.close();
    }
    bool ok = m_server.listen(bindAddr, port);
    if (!ok) {
        qCWarning(lcSmartSdr) << "failed to bind"
                               << bindAddr.toString() << ":" << port
                               << ":" << m_server.errorString();
        return false;
    }
    m_periodicTimer.start();
    qCInfo(lcSmartSdr) << "SmartSDR API listener listening on"
                       << m_server.serverAddress().toString()
                       << ":" << m_server.serverPort();
    return true;
}
```

- [ ] **Step 3: Add an accessor for the bound port (test-only convenience).**

In `SmartSdrApiListener.h`, alongside `isListening() const`:

```cpp
    // Test-only convenience: return the actual port the server bound to.
    // When start() picked an ephemeral port (port=0), this is the kernel-
    // assigned value tests need to know in order to connect a client.
    quint16 serverPort() const { return m_server.serverPort(); }
```

- [ ] **Step 4: Create the new test file with the harness and one smoke test.**

Write `tests/tst_smart_sdr_api_listener_ptt_chain.cpp`:

```cpp
// =================================================================
// tests/tst_smart_sdr_api_listener_ptt_chain.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream port. Covers C1 to C6 from the
// approved design doc docs/architecture/4o3a-lan-ptt-pcap-divergence.md
// (commit 559890a2).
// =================================================================
// Modification history (NereusSDR):
//   2026-05-21  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Smoke test (Task 0 foundation).
// =================================================================

#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QHostAddress>

#include "core/SmartSdrApiListener.h"

using NereusSDR::SmartSdrApiListener;

namespace {

// Helper: wait until `pred()` returns true or `timeoutMs` elapses, pumping
// the Qt event loop. Used because the listener does its work over queued
// signals + async TCP, so blocking sleeps would deadlock.
template<typename Pred>
bool waitFor(Pred pred, int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    while (!pred() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return pred();
}

// Drain everything readable on `sock` into a QByteArray, with up to
// `timeoutMs` to let bytes arrive. Test-side mirror of what Wireshark
// would record for one TCP-4992 client.
QByteArray drain(QTcpSocket* sock, int timeoutMs = 200)
{
    QByteArray out;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (sock->bytesAvailable() > 0) {
            out.append(sock->readAll());
        }
    }
    return out;
}

}  // namespace

class SmartSdrApiListenerPttChainTest : public QObject
{
    Q_OBJECT

private slots:
    void smoke_listenerAcceptsClientAndSendsBanner();
};

// Task 0 smoke test: prove the harness machinery works end-to-end.
// Start the listener on loopback + ephemeral port, connect a QTcpSocket,
// and confirm the V/H banner pair arrives. If this passes, every later
// test in this file has a working foundation.
void SmartSdrApiListenerPttChainTest::smoke_listenerAcceptsClientAndSendsBanner()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();
    QVERIFY(port > 0);

    QTcpSocket fakeAmp;
    fakeAmp.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(fakeAmp.waitForConnected(1000));

    const QByteArray bytes = drain(&fakeAmp);
    const QString text = QString::fromUtf8(bytes);
    QVERIFY2(text.startsWith(QStringLiteral("V1.4.0.0\n")),
             qPrintable(QStringLiteral("expected V<ver>\\n prefix, got: ") + text));
    QVERIFY2(text.contains(QStringLiteral("\nH")),
             qPrintable(QStringLiteral("expected H<handle>\\n line, got: ") + text));
}

QTEST_MAIN(SmartSdrApiListenerPttChainTest)
#include "tst_smart_sdr_api_listener_ptt_chain.moc"
```

- [ ] **Step 5: Register the test in `tests/CMakeLists.txt`.**

Find the line `nereus_add_test(tst_tx_interlock_policy)` and add immediately after it:

```cmake
# 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8: PTT chain pcap-alignment.
# Drives SmartSdrApiListener on loopback + ephemeral port and asserts the
# wire format of the handshake, PTT_REQUESTED, TRANSMITTING, UNKEY_REQUESTED,
# and READY frames. Companion design doc commit: 559890a2.
nereus_add_test(tst_smart_sdr_api_listener_ptt_chain)
```

- [ ] **Step 6: Configure the build to pick up the new files.**

Run from the worktree root:

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -30
```

Expected: target builds, no warnings on the new code, executable lands in `build/tests/`.

If you see "cannot find <core/SmartSdrApiListener.h>", run `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo` first to regenerate the build system with the new test source listed.

- [ ] **Step 7: Run the smoke test 2x (flake check).**

```bash
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: PASS twice. If either fails, fix before proceeding. Common failure: the test process is already binding 4992 from an earlier RadioModel session in another worktree. The harness uses ephemeral port 0 so this should not happen, but if it does, kill stray test processes (`pkill -f tst_smart_sdr`).

- [ ] **Step 8: Commit.**

```bash
git add src/core/SmartSdrApiListener.h \
        src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp \
        tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(amp): add SmartSdrApiListener PTT-chain test harness (Task 0)

Foundation for the C1 to C6 work that follows the approved design doc at
docs/architecture/4o3a-lan-ptt-pcap-divergence.md (commit 559890a2).

Adds a start(QHostAddress, quint16) overload so unit tests can bind the
TCP server to loopback + ephemeral port without colliding with the
production 4992 bind. Production start() now delegates to the new
overload with the previous AnyIPv4:4992 defaults; behavior unchanged.

Adds tests/tst_smart_sdr_api_listener_ptt_chain.cpp with a single smoke
test that connects a real QTcpSocket and asserts the V<ver> + H<handle>
banner pair arrives. Subsequent commits in this branch extend this file
one test function at a time per change C1 to C6.

No behavior change to production code. No regression risk.
EOF
)"
```

Expected: pre-commit hooks pass (header verifier, attribution check, inline cite gate). Commit lands GPG-signed.

---

## Task 1 (C1): Synthetic Local-Client Handle

**Goal:** Stop using each amp's own banner handle as `tx_client_handle` in interlock S-frames. Use a single synthetic handle generated at listener boot.

**Why:** Design doc row D2. Hypothesized root cause of TGXL never sending `interlock ready`. Bench evidence: TGXL ACKs in 17 ms in pcap (where `tx_client_handle` is the SmartSDR-Win PC client, distinct from any amp), but never ACKs in our bench (where we use the amp's own banner).

**Files:**
- Modify: `src/core/SmartSdrApiListener.h` (add `m_localClientHandle` member + accessor)
- Modify: `src/core/SmartSdrApiListener.cpp` (generate in `start()`; consume in `setInterlockTransmitting` key + unkey + `advanceToTransmittingIfReady` + `onPttAckTimeout`)
- Modify: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (add test function)

- [ ] **Step 1: Write the failing test for the C1 contract.**

Append to `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (above the `QTEST_MAIN` line) a new private slot declaration and definition. First add to the `private slots:` block:

```cpp
    void c1_localClientHandleIsStableAndDistinctFromBanners();
```

Then append this test function above `QTEST_MAIN`:

```cpp
// Task 1 (C1): After start(), the listener owns a synthetic
// local-client handle that (a) is 8-hex, (b) is the same value for the
// lifetime of the listener, and (c) is distinct from any client's banner
// handle assigned at accept time.
void SmartSdrApiListenerPttChainTest::c1_localClientHandleIsStableAndDistinctFromBanners()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));

    const QString localHandle = listener.localClientHandle();
    QCOMPARE(localHandle.size(), 8);
    // Hex digits only.
    for (QChar c : localHandle) {
        QVERIFY2(c.isDigit() || (c.toLatin1() >= 'A' && c.toLatin1() <= 'F'),
                 qPrintable(QStringLiteral("non-hex character in handle: ") + localHandle));
    }
    // Stable across reads.
    QCOMPARE(listener.localClientHandle(), localHandle);

    // Connect two clients and confirm the banner-assigned handles differ
    // from the local-client handle.
    QTcpSocket a, b;
    a.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(a.waitForConnected(1000));
    b.connectToHost(QHostAddress::LocalHost, listener.serverPort());
    QVERIFY(b.waitForConnected(1000));

    auto bannerOf = [](const QByteArray& bytes) -> QString {
        const QString text = QString::fromUtf8(bytes);
        const int hIdx = text.indexOf(QStringLiteral("\nH"));
        if (hIdx < 0) { return QString(); }
        const int nlIdx = text.indexOf(QLatin1Char('\n'), hIdx + 2);
        return text.mid(hIdx + 2, (nlIdx - hIdx - 2));
    };

    const QString bannerA = bannerOf(drain(&a));
    const QString bannerB = bannerOf(drain(&b));
    QVERIFY(!bannerA.isEmpty());
    QVERIFY(!bannerB.isEmpty());
    QVERIFY2(bannerA != localHandle,
             qPrintable(QStringLiteral("client A banner ") + bannerA
                        + QStringLiteral(" collides with local handle ")
                        + localHandle));
    QVERIFY2(bannerB != localHandle,
             qPrintable(QStringLiteral("client B banner ") + bannerB
                        + QStringLiteral(" collides with local handle ")
                        + localHandle));
}
```

- [ ] **Step 2: Run the test, expect FAIL on the `localClientHandle()` call.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -10
```

Expected: build FAILS with `error: 'class NereusSDR::SmartSdrApiListener' has no member named 'localClientHandle'`. That confirms the test is exercising the right contract.

- [ ] **Step 3: Add the `m_localClientHandle` member and accessor to the header.**

In `src/core/SmartSdrApiListener.h`, alongside other private members near `m_pttPendingSource`:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: synthetic local-client
    // handle used as tx_client_handle= in every PTT_REQUESTED / TRANSMITTING /
    // UNKEY_REQUESTED / READY S-frame. Generated once per start() so it is
    // stable for the listener's lifetime and distinct from every amp's banner
    // handle. NereusSDR equivalent of the SmartSDR-Win PC client's 0x66B137B7
    // in the canonical pcap (flex-tgxl-direct-CONTROL.pcapng @ T+167.678).
    QString m_localClientHandle;
```

And expose a const accessor in the `public:` block, just above the `signals:` section (search for `bool hasInterlockedAmp() const;`):

```cpp
    // Test-only accessor for the synthetic local-client handle (C1).
    // Production code does not call this; the value is consumed internally
    // by every interlock S-frame builder.
    QString localClientHandle() const { return m_localClientHandle; }
```

- [ ] **Step 4: Generate the handle in `start(QHostAddress, quint16)`.**

In `src/core/SmartSdrApiListener.cpp` inside the new `start(addr, port)` body, immediately after the successful `m_server.listen(...)` call and before `m_periodicTimer.start()`:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: generate synthetic
    // local-client handle once per listener boot. Stable for the lifetime
    // of this start() call. Consumed by every interlock S-frame builder.
    m_localClientHandle = generateHandle();
    qCInfo(lcSmartSdr) << "local-client handle:" << m_localClientHandle;
```

- [ ] **Step 5: Run the test, expect PASS.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes.

- [ ] **Step 6: Wire the new handle into every interlock S-frame builder.**

There are five existing call sites in `src/core/SmartSdrApiListener.cpp` that currently use either `it->handle` (the amp's banner) or `"0x00000000"` as `tx_client_handle=`. Replace each with `m_localClientHandle`:

**Site A: PTT_REQUESTED per-amp builder, around lines 213-219 inside `setInterlockTransmitting` key branch.**

Find:

```cpp
                const QString body =
                    QStringLiteral("interlock tx_client_handle=0x%1"
                                   " state=PTT_REQUESTED reason="
                                   " source=%2 tx_allowed=1 amplifier=0x%3")
                        .arg(it->handle)        // non-zero per pcap
                        .arg(wireSource)
                        .arg(it->ampHandle);
```

Replace with:

```cpp
                // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1:
                // tx_client_handle is the synthetic local-client handle,
                // not the amp's own banner. (C2 will collapse this loop
                // to a single frame.)
                const QString body =
                    QStringLiteral("interlock tx_client_handle=0x%1"
                                   " state=PTT_REQUESTED reason="
                                   " source=%2 tx_allowed=1 amplifier=0x%3")
                        .arg(m_localClientHandle)
                        .arg(wireSource)
                        .arg(it->ampHandle);
```

**Site B: TRANSMITTING no-amps fallback, around lines 255-259 inside the same function.**

Find:

```cpp
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x00000000"
                               " state=TRANSMITTING reason="
                               " source=%1 tx_allowed=1 amplifier=")
                    .arg(wireSource);
```

Replace with:

```cpp
            // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: use the
            // synthetic local-client handle even on the no-amps fallback
            // path. (C3 will keep this as a single frame.)
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=TRANSMITTING reason="
                               " source=%2 tx_allowed=1 amplifier=")
                    .arg(m_localClientHandle)
                    .arg(wireSource);
```

**Site C: READY per-amp builder, around lines 323-327 inside the un-key branch.**

Find:

```cpp
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=READY reason=AMP:%2"
                               " source= tx_allowed=1 amplifier=")
                    .arg(it->handle).arg(it->interlockName);
```

Replace with:

```cpp
            // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1: same
            // synthetic handle on the un-key READY frames. (C5 will
            // rewrite this branch to UNKEY_REQUESTED + 2-frame READY.)
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=READY reason=AMP:%2"
                               " source= tx_allowed=1 amplifier=")
                    .arg(m_localClientHandle).arg(it->interlockName);
```

**Site D: READY no-amps fallback, around lines 338-341 inside the un-key branch.**

Find:

```cpp
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x00000000"
                               " state=READY reason= source= tx_allowed=1"
                               " amplifier=");
```

Replace with:

```cpp
            // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1.
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=READY reason= source= tx_allowed=1"
                               " amplifier=")
                    .arg(m_localClientHandle);
```

**Site E: TRANSMITTING per-amp builder, around lines 383-387 inside `advanceToTransmittingIfReady`.**

Find:

```cpp
        const QString body =
            QStringLiteral("interlock tx_client_handle=0x%1"
                           " state=TRANSMITTING reason= source=%2"
                           " tx_allowed=1 amplifier=0x%3")
                .arg(it->handle).arg(source).arg(it->ampHandle);
```

Replace with:

```cpp
        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C1:
        // tx_client_handle is the synthetic local-client handle. (C3
        // will collapse this loop to a single frame.)
        const QString body =
            QStringLiteral("interlock tx_client_handle=0x%1"
                           " state=TRANSMITTING reason= source=%2"
                           " tx_allowed=1 amplifier=0x%3")
                .arg(m_localClientHandle).arg(source).arg(it->ampHandle);
```

- [ ] **Step 7: Rebuild and re-run the test 2x.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes.

- [ ] **Step 8: Commit.**

```bash
git add src/core/SmartSdrApiListener.h \
        src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp
git commit -m "$(cat <<'EOF'
fix(amp): synthetic local-client handle for interlock S-frames (C1)

Implements C1 from docs/architecture/4o3a-lan-ptt-pcap-divergence.md
(commit 559890a2). Replaces the amp's own banner handle in
tx_client_handle= with a stable synthetic 8-hex value generated once at
start(). NereusSDR equivalent of SmartSDR-Win's 0x66B137B7 in the pcap.

Hypothesized root cause of TGXL never sending interlock ready: canonical
pcap shows TGXL ACKs in 17 ms when tx_client_handle is the GUI client
handle (distinct from TGXL's banner). Our previous behavior put TGXL's
own banner there, and TGXL appears to reject self-referencing frames.

Adds tst_smart_sdr_api_listener_ptt_chain c1 case proving the handle is
stable across reads and distinct from connected clients' banner handles.

C2 to C5 follow in subsequent commits and consume this handle in the
canonical PTT_REQUESTED / TRANSMITTING / UNKEY_REQUESTED / READY frames.
EOF
)"
```

---

## Task 2 (C2): Single PTT_REQUESTED Frame with Canonical Fields

**Goal:** Replace the per-amp loop in `setInterlockTransmitting` key branch with one `S0|interlock` frame. Fields: `tx_client_handle=<synthetic>`, `state=PTT_REQUESTED`, `reason=AMP:<initiating-amp-name>`, `source=TUNE|MIC`, `tx_allowed=1`, `amplifier=` empty.

**Why:** Rows D1, D3, D4. The current frame storm (N frames, one per amp) plus the wrong `reason=` (empty) and wrong `amplifier=` (self) is the format-mismatch cluster that probably accounts for TGXL's slow / never ACK.

**Initiating-amp resolution (per design doc §8 Definitions):**
- `source=TUNE`: the amp that most recently sent `C<n>|transmit tune on`. Track via a new `m_lastTuneInitiator` member set in `dispatchLine` when the tune-on command is received.
- `source=MIC` (Thetis maps MOX to wire-MIC): first amp registered with `ampModel == "PowerGeniusXL"`. If none, use empty string.

**Files:**
- Modify: `src/core/SmartSdrApiListener.h` (add `m_lastTuneInitiator` member; private helper `initiatingAmpName(source)`)
- Modify: `src/core/SmartSdrApiListener.cpp` (track tune initiator in `dispatchLine`; refactor PTT_REQUESTED loop to single frame)
- Modify: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (add test function)

- [ ] **Step 1: Write the failing test for C2 wire format.**

Add to `private slots:`:

```cpp
    void c2_pttRequestedIsOneFrameWithCanonicalFields();
```

Add this helper above the test class (alongside `drain` and `waitFor`):

```cpp
// Send a canned `interlock create` from `sock`. Returns when the listener
// has assigned an interlock id (i.e. it->interlockId != 0 inside the
// listener). The fake amp ALSO needs an `amplifier create` first so the
// listener tracks ampHandle / ampModel / interlockName correctly.
void registerFakeAmp(QTcpSocket* sock,
                     const QString& model,
                     const QString& interlockName,
                     const QString& serial = QStringLiteral("TEST-1"))
{
    QByteArray cmd;
    cmd.append(QStringLiteral("C1|amplifier create ip=127.0.0.1 port=9999"
                              " model=%1 serial_num=%2 ant=ANT1\n")
                   .arg(model).arg(serial)
                   .toUtf8());
    cmd.append(QStringLiteral("C2|interlock create type=AMP name=%1"
                              " serial=%2 valid_antennas=ANT1\n")
                   .arg(interlockName).arg(serial)
                   .toUtf8());
    sock->write(cmd);
    sock->flush();
    // Pump the event loop so the listener processes the lines.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 200) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
}

// Find all `S0|interlock state=<state>` frames in a captured byte stream.
// Returns the body portion after `|` so test assertions can substring-match.
QStringList findS0InterlockFrames(const QByteArray& bytes,
                                  const QString& state)
{
    QStringList out;
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("S0|interlock "))
            && line.contains(QStringLiteral("state=") + state)) {
            out << line;
        }
    }
    return out;
}
```

Then add the test function:

```cpp
// Task 2 (C2): PTT_REQUESTED is exactly one frame broadcast to all
// subscribers, with canonical fields. Verified against pcap T+167.678
// from flex-tgxl-direct-CONTROL.pcapng.
void SmartSdrApiListenerPttChainTest::c2_pttRequestedIsOneFrameWithCanonicalFields()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();
    const QString localHandle = listener.localClientHandle();

    // Two amps: TGXL initiates TUNE, PGXL participates.
    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);  // banner + initial status

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);  // R-frames for the create commands

    // Simulate TGXL pressing its hardware TUNE button: it would send
    // `C<n>|transmit tune on`. The listener records TG as the initiator.
    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    QVERIFY(waitFor([&]() { return drain(&tgxl, 50).contains("R9|0"); }, 500));

    // RadioModel side: setInterlockTransmitting(true, "TUNE").
    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));

    // Both subscribers should see the same one PTT_REQUESTED frame.
    const QByteArray tgxlBytes = drain(&tgxl);
    const QByteArray pgxlBytes = drain(&pgxl);
    const QStringList tgxlFrames =
        findS0InterlockFrames(tgxlBytes, QStringLiteral("PTT_REQUESTED"));
    const QStringList pgxlFrames =
        findS0InterlockFrames(pgxlBytes, QStringLiteral("PTT_REQUESTED"));
    QCOMPARE(tgxlFrames.size(), 1);
    QCOMPARE(pgxlFrames.size(), 1);

    const QString frame = tgxlFrames.first();
    QVERIFY2(frame.contains(QStringLiteral("tx_client_handle=0x") + localHandle),
             qPrintable(QStringLiteral("expected synthetic tx_client_handle in: ") + frame));
    QVERIFY2(frame.contains(QStringLiteral("reason=AMP:TG ")),
             qPrintable(QStringLiteral("expected reason=AMP:TG in: ") + frame));
    QVERIFY2(frame.contains(QStringLiteral("source=TUNE")),
             qPrintable(QStringLiteral("expected source=TUNE in: ") + frame));
    QVERIFY2(frame.contains(QStringLiteral("tx_allowed=1")),
             qPrintable(QStringLiteral("expected tx_allowed=1 in: ") + frame));
    QVERIFY2(frame.endsWith(QStringLiteral("amplifier=")),
             qPrintable(QStringLiteral("expected empty amplifier= in: ") + frame));
}
```

- [ ] **Step 2: Run the test, expect FAIL on the frame count or the field content.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$'
```

Expected: FAIL with `Compared values are not the same` on `QCOMPARE(tgxlFrames.size(), 1)` (current code emits 2 frames), or on the reason= assertion (current code emits empty reason).

- [ ] **Step 3: Add `m_lastTuneInitiator` to the header.**

In `src/core/SmartSdrApiListener.h`, alongside `m_localClientHandle`:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: name= of the amp
    // that most recently sent `transmit tune on`. Used as reason=AMP:<name>
    // in the PTT_REQUESTED frame when source=TUNE. Cleared on
    // tuneRequested(false) or on next setInterlockTransmitting(false).
    QString m_lastTuneInitiator;

    // Private helper: resolve reason=AMP:<name> for the canonical
    // PTT_REQUESTED frame. wireSource is "TUNE" or "MIC".
    QString initiatingAmpName(const QString& wireSource) const;
```

Add the declaration to the existing `private:` block alongside other helpers like `generateHandle`.

- [ ] **Step 4: Track the tune initiator in `dispatchLine`.**

In `src/core/SmartSdrApiListener.cpp`, find the existing `transmit tune` handling. Search for `tuneRequested(true)` (the emit). Add immediately before the emit:

```cpp
            // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: record
            // which amp sent the tune-on so PTT_REQUESTED can carry
            // reason=AMP:<name>. Falls back to that amp's interlockName
            // (set when it sent `interlock create`) so the wire matches
            // pcap T+167.678 where reason=AMP:TG names the TGXL initiator.
            auto initIt = m_clients.find(sock);
            if (initIt != m_clients.end() && !initIt->interlockName.isEmpty()) {
                m_lastTuneInitiator = initIt->interlockName;
            }
```

And on the tune-off branch (search for `tuneRequested(false)`), clear it:

```cpp
            // C2: clear the recorded initiator. Next TUNE cycle re-records.
            m_lastTuneInitiator.clear();
```

- [ ] **Step 5: Implement `initiatingAmpName(source)`.**

Add to `src/core/SmartSdrApiListener.cpp` near the existing `generateHandle` definition (search for `QString SmartSdrApiListener::generateHandle`):

```cpp
QString SmartSdrApiListener::initiatingAmpName(const QString& wireSource) const
{
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2:
    // - source=TUNE: name of the amp that sent `transmit tune on`. Falls
    //   back to empty if no tune initiator recorded.
    // - source=MIC : first amp registered with model=PowerGeniusXL (the
    //   power amp in the chain). Falls back to empty if no PGXL-class amp
    //   is connected, which is acceptable per the design doc Definitions.
    if (wireSource == QStringLiteral("TUNE")) {
        return m_lastTuneInitiator;
    }
    if (wireSource == QStringLiteral("MIC")) {
        for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
            if (it->ampModel == QStringLiteral("PowerGeniusXL")
                && !it->interlockName.isEmpty()) {
                return it->interlockName;
            }
        }
    }
    return QString();
}
```

- [ ] **Step 6: Replace the PTT_REQUESTED per-amp loop with a single frame.**

In `setInterlockTransmitting` key branch (around lines 210-232 after the C1 edits), replace the entire `if (anyInterlockedAmp) { for (auto it = m_clients.cbegin(); ...) { ... } ... }` block's inner loop with one frame. Find the block that begins with the `for` loop building the PTT_REQUESTED body:

```cpp
        if (anyInterlockedAmp) {
            for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
                if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
                // ... PTT_REQUESTED body builder ...
                for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                    QTcpSocket* sock = jt.key();
                    if (sock && sock->isOpen()) { sock->write(frame); }
                }
                qCInfo(lcSmartSdr) << "TX S0|interlock state=PTT_REQUESTED" ...
            }
            // ... arm timeout ...
        }
```

Replace with:

```cpp
        if (anyInterlockedAmp) {
            // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C2: ONE
            // PTT_REQUESTED frame broadcast to every subscriber, with
            // canonical fields. reason=AMP:<initiating-amp-name>;
            // amplifier= empty; tx_client_handle is the synthetic local
            // handle. Matches pcap T+167.678.
            const QString reasonName = initiatingAmpName(wireSource);
            const QString reasonField = reasonName.isEmpty()
                ? QString()
                : QStringLiteral("AMP:") + reasonName;
            const QString body =
                QStringLiteral("interlock tx_client_handle=0x%1"
                               " state=PTT_REQUESTED reason=%2"
                               " source=%3 tx_allowed=1 amplifier=")
                    .arg(m_localClientHandle)
                    .arg(reasonField)
                    .arg(wireSource);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|interlock state=PTT_REQUESTED"
                               << "tx_client_handle=0x" << m_localClientHandle
                               << "reason=" << reasonField
                               << "source=" << wireSource
                               << "(canonical single-frame; waiting for interlock ready"
                                  " ACKs from" << /*count via iteration*/ 0
                               << "enabled amps)";

            // Arm the 500 ms wiki-spec timeout (unchanged from C1 state).
            if (!m_pttAckTimeout.isActive()) {
                m_pttAckTimeout.setSingleShot(true);
                m_pttAckTimeout.setInterval(500);
                disconnect(&m_pttAckTimeout, &QTimer::timeout, nullptr, nullptr);
                connect(&m_pttAckTimeout, &QTimer::timeout,
                        this, &SmartSdrApiListener::onPttAckTimeout);
            }
            m_pttAckTimeout.start();
            QTimer::singleShot(0, this,
                               &SmartSdrApiListener::advanceToTransmittingIfReady);
        }
```

(The trailing `else { /* no-amps fallback */ }` block stays as-is; C3 will revisit it.)

- [ ] **Step 7: Rebuild and run the test 2x.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes (smoke + c1 + c2 all green).

- [ ] **Step 8: Commit.**

```bash
git add src/core/SmartSdrApiListener.h \
        src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp
git commit -m "$(cat <<'EOF'
fix(amp): single canonical PTT_REQUESTED frame (C2)

Implements C2 from docs/architecture/4o3a-lan-ptt-pcap-divergence.md.
Collapses the per-amp PTT_REQUESTED loop into a single S0|interlock
frame broadcast to all subscribers. Fields now match pcap T+167.678
exactly: tx_client_handle is the synthetic local-client handle from C1,
reason=AMP:<initiating-amp-name>, source=TUNE|MIC, amplifier= empty.

Adds m_lastTuneInitiator tracking in dispatchLine so a TGXL hardware
TUNE press records reason=AMP:TG; the new initiatingAmpName(source)
helper resolves the name for both TUNE and MIC paths (MIC picks the
first PowerGeniusXL-class amp).

Test asserts exactly one PTT_REQUESTED frame reaches each subscriber
with the canonical field values.
EOF
)"
```

---

## Task 3 (C3): Single TRANSMITTING Frame with Comma-Separated Amplifier List

**Goal:** Replace the per-amp loop in `advanceToTransmittingIfReady` with one `S0|interlock state=TRANSMITTING` frame whose `amplifier=` carries a comma-separated list of all keyed amp handles.

**Why:** Rows D8, D9. Pcap T+167.734 shows `amplifier=0x096016A4,0x22E8213A` (TGXL,PGXL) in a single frame. Amps that watch for the multi-amp list to compute their own role in the chain miss it when we send per-amp frames.

**Files:**
- Modify: `src/core/SmartSdrApiListener.cpp` (`advanceToTransmittingIfReady` only)
- Modify: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (add test function)

- [ ] **Step 1: Write the failing test for C3.**

Add to `private slots:`:

```cpp
    void c3_transmittingIsOneFrameWithCommaSeparatedAmpList();
```

Append:

```cpp
// Task 3 (C3): TRANSMITTING is exactly one frame with
// amplifier=0x<h1>,0x<h2>,... (comma-separated list of every keyed amp).
// Matches pcap T+167.734.
void SmartSdrApiListenerPttChainTest::c3_transmittingIsOneFrameWithCommaSeparatedAmpList()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);

    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    drain(&tgxl);

    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));
    drain(&tgxl); drain(&pgxl);  // PTT_REQUESTED frames

    // Both amps ACK. The interlock ids are 1 and 2 by registration order.
    tgxl.write("C10|interlock ready 1\n");
    tgxl.flush();
    pgxl.write("C10|interlock ready 2\n");
    pgxl.flush();

    const QByteArray tgxlBytes = drain(&tgxl, 300);
    const QStringList frames =
        findS0InterlockFrames(tgxlBytes, QStringLiteral("TRANSMITTING"));
    QCOMPARE(frames.size(), 1);

    const QString frame = frames.first();
    QVERIFY2(frame.contains(QStringLiteral("source=TUNE")),
             qPrintable(frame));
    QVERIFY2(frame.contains(QStringLiteral("amplifier=0x")),
             qPrintable(frame));
    QVERIFY2(frame.count(QLatin1Char(',')) == 1,
             qPrintable(QStringLiteral("expected exactly one comma in amplifier=, got: ") + frame));
}
```

- [ ] **Step 2: Run, expect FAIL (current code emits 2 frames).**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$'
```

Expected: FAIL on `QCOMPARE(frames.size(), 1)`.

- [ ] **Step 3: Refactor `advanceToTransmittingIfReady` to single frame.**

In `src/core/SmartSdrApiListener.cpp`, replace the existing per-amp TRANSMITTING loop (around lines 381-398, the block that begins `for (auto it = m_clients.cbegin(); ...) { if (it->interlockId == 0 ...) continue; const QString body = ...TRANSMITTING...; ... write(frame); ... }`) with:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C3: ONE TRANSMITTING
    // frame with amplifier=<comma-separated list of every keyed amp's
    // handle>. Matches pcap T+167.734.
    QStringList ampHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            ampHandles << (QStringLiteral("0x") + it->ampHandle);
        }
    }
    const QString body =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=TRANSMITTING reason= source=%2"
                       " tx_allowed=1 amplifier=%3")
            .arg(m_localClientHandle)
            .arg(source)
            .arg(ampHandles.join(QLatin1Char(',')));
    const QByteArray frame =
        QStringLiteral("S0|%1\n").arg(body).toUtf8();
    for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
        QTcpSocket* sock = jt.key();
        if (sock && sock->isOpen()) { sock->write(frame); }
    }
    qCInfo(lcSmartSdr) << "TX S0|interlock state=TRANSMITTING"
                       << "source=" << source
                       << "amplifier=" << ampHandles.join(QLatin1Char(','))
                       << "(canonical single-frame; all amps acked)";
```

(The `pttA=1` push block at lines 408-419 stays for now; C6 retires it.)

- [ ] **Step 4: Rebuild and run the test 2x.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes.

- [ ] **Step 5: Commit.**

```bash
git add src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp
git commit -m "$(cat <<'EOF'
fix(amp): single canonical TRANSMITTING frame with comma-separated amps (C3)

Implements C3 from docs/architecture/4o3a-lan-ptt-pcap-divergence.md.
Replaces the per-amp TRANSMITTING loop in advanceToTransmittingIfReady
with one S0|interlock frame whose amplifier=<list> carries every keyed
amp's handle, comma-separated. Matches pcap T+167.734 exactly.

Amps that watch the multi-amp list to compute their own role in the
chain (versus their pair partner) can now do so. Single-frame emission
also halves the wire traffic on the TX-key path.

Test asserts exactly one TRANSMITTING frame is broadcast and the
amplifier= field contains exactly one comma when two amps are keyed.
EOF
)"
```

---

## Task 4 (C4): 30 ms Settle Delay Across Success and Timeout Paths

**Goal:** Defer the TRANSMITTING broadcast + RF-flow gate release by 30 ms after the last ACK arrives (success path) OR after the 500 ms ACK timeout fires (fallback path). Both paths share the same delay.

**Why:** Row D7. Direct cause of key-down high-SWR. Pcap T+167.704 (last ACK) → T+167.734 (TRANSMITTING) shows a 30 ms quiet window. Without it, PGXL has registered carrier before TGXL relays have settled to TRANSMIT position.

**Files:**
- Modify: `src/core/SmartSdrApiListener.h` (add private helper `broadcastTransmitting(source)`)
- Modify: `src/core/SmartSdrApiListener.cpp` (extract the broadcast into the helper; wrap call site in `QTimer::singleShot(30, ...)`)
- Modify: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (add test function with timing assertion)

- [ ] **Step 1: Write the failing test for C4 timing.**

Add to `private slots:`:

```cpp
    void c4_transmittingIsDelayed30msAfterLastAck();
```

Append:

```cpp
// Task 4 (C4): TRANSMITTING is emitted no sooner than ~30 ms after the
// last interlock ready ACK arrives. Matches pcap T+167.704 to T+167.734.
void SmartSdrApiListenerPttChainTest::c4_transmittingIsDelayed30msAfterLastAck()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);

    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    drain(&tgxl);

    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));
    drain(&tgxl); drain(&pgxl);

    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);

    // Both amps ACK back to back.
    QElapsedTimer timer;
    timer.start();
    tgxl.write("C10|interlock ready 1\n");
    tgxl.flush();
    pgxl.write("C10|interlock ready 2\n");
    pgxl.flush();

    // Wait for interlockGranted; record elapsed time from the second ACK
    // write to the signal fire. Should be >= 25 ms (allow 5 ms jitter
    // below the nominal 30 ms target).
    QVERIFY(grantSpy.wait(500));
    const qint64 elapsedMs = timer.elapsed();
    QVERIFY2(elapsedMs >= 25,
             qPrintable(QStringLiteral("expected >= 25 ms settle, got %1 ms")
                            .arg(elapsedMs)));
    // Sanity upper bound (the 500 ms ACK timeout would be a regression).
    QVERIFY2(elapsedMs < 200,
             qPrintable(QStringLiteral("settle too long: %1 ms").arg(elapsedMs)));
}
```

- [ ] **Step 2: Run, expect FAIL (current code fires the signal in <5 ms).**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$'
```

Expected: FAIL on the `elapsedMs >= 25` assertion (current path is synchronous).

- [ ] **Step 3: Extract the broadcast into a private helper.**

In `src/core/SmartSdrApiListener.h`, add to the `private:` declarations:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C4: actual emission
    // of the canonical TRANSMITTING S-frame + RF-flow gate release.
    // Wrapped by both the success path (advanceToTransmittingIfReady)
    // and the timeout path (onPttAckTimeout) inside a 30 ms QTimer to
    // match pcap T+167.704 -> T+167.734 settle window.
    void broadcastTransmitting(const QString& source);
```

In `src/core/SmartSdrApiListener.cpp`, define the helper. Place it right after `advanceToTransmittingIfReady`:

```cpp
void SmartSdrApiListener::broadcastTransmitting(const QString& source)
{
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C3 + C4:
    // single S0|interlock state=TRANSMITTING frame with comma-separated
    // amplifier list, emitted 30 ms after the last ACK arrives.
    QStringList ampHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            ampHandles << (QStringLiteral("0x") + it->ampHandle);
        }
    }
    const QString body =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=TRANSMITTING reason= source=%2"
                       " tx_allowed=1 amplifier=%3")
            .arg(m_localClientHandle)
            .arg(source)
            .arg(ampHandles.join(QLatin1Char(',')));
    const QByteArray frame =
        QStringLiteral("S0|%1\n").arg(body).toUtf8();
    for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
        QTcpSocket* sock = jt.key();
        if (sock && sock->isOpen()) { sock->write(frame); }
    }
    qCInfo(lcSmartSdr) << "TX S0|interlock state=TRANSMITTING"
                       << "source=" << source
                       << "amplifier=" << ampHandles.join(QLatin1Char(','))
                       << "(post 30 ms settle)";

    // Per-amp pttA=1 push (unchanged from C3 state; C6 retires it).
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->ampHandle.isEmpty()) { continue; }
        const QString ampBody = QStringLiteral("amplifier 0x%1 pttA=1")
                                    .arg(it->ampHandle);
        const QByteArray ampFrame =
            QStringLiteral("S%1|%2\n").arg(it->handle).arg(ampBody).toUtf8();
        QTcpSocket* sock = it.key();
        if (sock && sock->isOpen()) { sock->write(ampFrame); }
    }
    qCInfo(lcSmartSdr) << "TX S<h>|amplifier pttA=1 to all subscribed amps";

    emit interlockGranted(source);
}
```

- [ ] **Step 4: Rewrite `advanceToTransmittingIfReady` to call the helper after 30 ms.**

Replace the body (lines ~353-425) so the success path becomes:

```cpp
void SmartSdrApiListener::advanceToTransmittingIfReady()
{
    if (m_pttPendingSource.isEmpty()) { return; }

    int totalInterlocks = 0;
    int readyCount = 0;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        ++totalInterlocks;
        if (it->ackReady) { ++readyCount; }
    }
    if (totalInterlocks == 0) { return; }
    if (readyCount < totalInterlocks) { return; }

    // All registered+enabled amps have ACKed. Stop the timeout, capture
    // the source, then schedule broadcastTransmitting() 30 ms in the
    // future per pcap T+167.704 -> T+167.734.
    m_pttAckTimeout.stop();
    const QString source = m_pttPendingSource;
    m_pttPendingSource.clear();

    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C4: 30 ms settle
    // before TRANSMITTING + RF-flow gate release so TGXL relays have
    // time to switch to TRANSMIT position before PGXL sees carrier.
    QTimer::singleShot(30, this, [this, source]() {
        broadcastTransmitting(source);
    });
}
```

- [ ] **Step 5: Mirror the 30 ms settle on the timeout path.**

Find `onPttAckTimeout` (line ~427). Replace its trailing `advanceToTransmittingIfReady();` call (line ~512 today) and the surrounding block:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C4: timeout path
    // shares the same 30 ms settle as the success path. Set every
    // enabled amp's ackReady so the next call to broadcastTransmitting()
    // has a consistent view. Then capture + clear m_pttPendingSource and
    // schedule the broadcast.
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it->interlockId != 0 && it->interlockEnabled) {
            it->ackReady = true;
        }
    }
    if (m_pttPendingSource.isEmpty()) { return; }
    const QString source = m_pttPendingSource;
    m_pttPendingSource.clear();
    QTimer::singleShot(30, this, [this, source]() {
        broadcastTransmitting(source);
    });
```

(The existing `qCWarning` / `qCInfo` lines that summarize laggards stay in place above this block.)

- [ ] **Step 6: Rebuild and run the test 2x.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes.

- [ ] **Step 7: Commit.**

```bash
git add src/core/SmartSdrApiListener.h \
        src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp
git commit -m "$(cat <<'EOF'
fix(amp): 30 ms settle delay before TRANSMITTING + RF gate release (C4)

Implements C4 from docs/architecture/4o3a-lan-ptt-pcap-divergence.md.
Extracts the TRANSMITTING emission + pttA=1 push + interlockGranted
into broadcastTransmitting(source) and schedules it 30 ms after both
the success path (all amps ACKed) and the timeout fallback. Matches
pcap T+167.704 to T+167.734 settle window.

Hypothesized fix for the key-down high-SWR pattern (5/10 cycles before
C4). PGXL was registering carrier before TGXL relays had switched to
TRANSMIT position. The settle gives TGXL the same window canonical
FLEX gives it.

Test asserts interlockGranted fires no sooner than 25 ms after the
second ACK write (allowing 5 ms jitter under the 30 ms target) and
no later than 200 ms (sanity bound against the 500 ms timeout).
EOF
)"
```

---

## Task 5 (C5): Canonical Un-Key Sequence (UNKEY_REQUESTED + Two READY Frames)

**Goal:** Replace the un-key branch in `setInterlockTransmitting` with a three-frame canonical sequence: `UNKEY_REQUESTED`, then ~2 ms later `READY (empty reason)`, then ~0.5 ms later `READY (with reason=AMP:<initiator>)`.

**Why:** Rows D11 + D12. Today we skip `UNKEY_REQUESTED` entirely and emit one `READY` per amp instead of the canonical two. The missing `UNKEY_REQUESTED` is the direct cause of the un-key high-SWR flash (10/10 cycles): TGXL has no signal to start relay switch-back before PGXL drops carrier.

**Files:**
- Modify: `src/core/SmartSdrApiListener.h` (add private helper `broadcastUnkeySequence(initiatorName)`)
- Modify: `src/core/SmartSdrApiListener.cpp` (replace un-key branch body)
- Modify: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (add test function)

- [ ] **Step 1: Write the failing test for C5.**

Add to `private slots:`:

```cpp
    void c5_unkeyEmitsUnkeyRequestedThenTwoReadyFrames();
```

Append:

```cpp
// Task 5 (C5): unkey emits UNKEY_REQUESTED, then READY (empty reason),
// then READY (reason=AMP:<initiator>). Matches pcap T+168.874 to T+168.877.
void SmartSdrApiListenerPttChainTest::c5_unkeyEmitsUnkeyRequestedThenTwoReadyFrames()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);

    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    drain(&tgxl);

    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));
    drain(&tgxl); drain(&pgxl);
    tgxl.write("C10|interlock ready 1\n"); tgxl.flush();
    pgxl.write("C10|interlock ready 2\n"); pgxl.flush();
    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);
    QVERIFY(grantSpy.wait(500));
    drain(&tgxl); drain(&pgxl);  // TRANSMITTING + pttA=1 chatter

    // Trigger un-key.
    listener.setInterlockTransmitting(false, QStringLiteral("TUNE"));

    // Capture frames across ~50 ms so the 2 ms + 0.5 ms gaps land.
    const QByteArray bytes = drain(&tgxl, 100);
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));

    // Filter to the S0|interlock lines in arrival order.
    QStringList interlockLines;
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("S0|interlock"))) {
            interlockLines << line;
        }
    }
    QVERIFY2(interlockLines.size() >= 3,
             qPrintable(QStringLiteral("expected at least 3 interlock lines, got: ")
                            + interlockLines.join(QStringLiteral(" || "))));
    QVERIFY2(interlockLines[0].contains(QStringLiteral("state=UNKEY_REQUESTED")),
             qPrintable(interlockLines[0]));
    QVERIFY2(interlockLines[0].contains(QStringLiteral("reason=AMP:TG")),
             qPrintable(interlockLines[0]));
    QVERIFY2(interlockLines[1].contains(QStringLiteral("state=READY")),
             qPrintable(interlockLines[1]));
    QVERIFY2(interlockLines[1].contains(QStringLiteral(" reason= ")),
             qPrintable(QStringLiteral("expected empty reason in first READY: ") + interlockLines[1]));
    QVERIFY2(interlockLines[2].contains(QStringLiteral("state=READY")),
             qPrintable(interlockLines[2]));
    QVERIFY2(interlockLines[2].contains(QStringLiteral("reason=AMP:TG")),
             qPrintable(interlockLines[2]));
}
```

- [ ] **Step 2: Run, expect FAIL (current code emits no UNKEY_REQUESTED).**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$'
```

Expected: FAIL with the UNKEY_REQUESTED line absent.

- [ ] **Step 3: Add the unkey helper to the header.**

In `src/core/SmartSdrApiListener.h` private declarations:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C5: canonical
    // un-key sequence. Emits UNKEY_REQUESTED reason=AMP:<initiator>,
    // then READY (empty reason) 2 ms later, then READY reason=AMP:<...>
    // 0.5 ms after that. Matches pcap T+168.874 -> T+168.877.
    void broadcastUnkeySequence(const QString& initiatorName);
```

- [ ] **Step 4: Implement the helper.**

In `src/core/SmartSdrApiListener.cpp`, place the definition immediately after `broadcastTransmitting`:

```cpp
void SmartSdrApiListener::broadcastUnkeySequence(const QString& initiatorName)
{
    const QString reasonField = initiatorName.isEmpty()
        ? QString()
        : QStringLiteral("AMP:") + initiatorName;

    auto broadcast = [this](const QString& body) {
        const QByteArray frame =
            QStringLiteral("S0|%1\n").arg(body).toUtf8();
        for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
            QTcpSocket* sock = jt.key();
            if (sock && sock->isOpen()) { sock->write(frame); }
        }
    };

    // Frame 1: UNKEY_REQUESTED reason=AMP:<initiator>.
    const QString unkeyBody =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=UNKEY_REQUESTED reason=%2"
                       " source= tx_allowed=1 amplifier=")
            .arg(m_localClientHandle).arg(reasonField);
    broadcast(unkeyBody);
    qCInfo(lcSmartSdr) << "TX S0|interlock state=UNKEY_REQUESTED"
                       << "reason=" << reasonField;

    // Frame 2: READY (empty reason), 2 ms later.
    const QString readyEmptyBody =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=READY reason= source= tx_allowed=1"
                       " amplifier=")
            .arg(m_localClientHandle);
    QTimer::singleShot(2, this, [broadcast, readyEmptyBody]() {
        broadcast(readyEmptyBody);
    });

    // Frame 3: READY reason=AMP:<initiator>, ~0.5 ms after frame 2 (round
    // up to 3 ms total post-UNKEY since QTimer resolution on most
    // platforms is 1 ms minimum).
    const QString readyNamedBody =
        QStringLiteral("interlock tx_client_handle=0x%1"
                       " state=READY reason=%2 source= tx_allowed=1"
                       " amplifier=")
            .arg(m_localClientHandle).arg(reasonField);
    QTimer::singleShot(3, this, [broadcast, readyNamedBody]() {
        broadcast(readyNamedBody);
    });
}
```

- [ ] **Step 5: Replace the un-key branch body.**

In `setInterlockTransmitting`, find the `} else {` branch starting around line 288. Replace the entire branch body (lines 288-350) with:

```cpp
    } else {
        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C5: canonical
        // un-key sequence (UNKEY_REQUESTED then 2-frame READY pair). The
        // initiator name is the one recorded for the in-flight TX (TUNE
        // path) or the first PGXL-class amp's name (MIC/MOX path).
        m_pttAckTimeout.stop();
        const QString initiator = m_lastTuneInitiator.isEmpty()
            ? initiatingAmpName(QStringLiteral("MIC"))
            : m_lastTuneInitiator;
        m_pttPendingSource.clear();

        // Preserve the existing per-amp pttA=0 push for now; C6 retires it.
        for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
            if (it->ampHandle.isEmpty()) { continue; }
            const QString body = QStringLiteral("amplifier 0x%1 pttA=0")
                                     .arg(it->ampHandle);
            const QByteArray frame =
                QStringLiteral("S%1|%2\n").arg(it->handle).arg(body).toUtf8();
            QTcpSocket* sock = it.key();
            if (sock && sock->isOpen()) { sock->write(frame); }
        }
        qCInfo(lcSmartSdr) << "TX S<h>|amplifier pttA=0 to all subscribed amps"
                            << "(retires in C6)";

        broadcastUnkeySequence(initiator);
    }
```

- [ ] **Step 6: Rebuild and run the test 2x.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes.

- [ ] **Step 7: Commit.**

```bash
git add src/core/SmartSdrApiListener.h \
        src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp
git commit -m "$(cat <<'EOF'
fix(amp): canonical unkey sequence UNKEY_REQUESTED + READY pair (C5)

Implements C5 from docs/architecture/4o3a-lan-ptt-pcap-divergence.md.
Replaces the un-key branch's per-amp READY loop with the canonical
three-frame sequence:

  1. S0|interlock state=UNKEY_REQUESTED reason=AMP:<initiator>
  2. ~2 ms later: state=READY reason= empty
  3. ~3 ms total: state=READY reason=AMP:<initiator>

Matches pcap T+168.874 -> T+168.876 -> T+168.877.

Hypothesized fix for the 10/10 un-key high-SWR flash: previously TGXL
had no signal to start relay switch-back before PGXL dropped carrier.
UNKEY_REQUESTED now signals that intent before any PGXL state change.

The explicit S<h>|amplifier pttA=0 push is preserved here (still
emitted before broadcastUnkeySequence). C6 retires it.

Test asserts the three frames arrive in order with the expected reason
fields.
EOF
)"
```

---

## Task 6 (C6): Retire Explicit pttA Pushes; Add Canonical Amplifier-State Broadcasts

**Goal:** Stop pushing `S<h>|amplifier 0x<h> pttA=1` on key-down and `S<h>|amplifier 0x<h> pttA=0` on un-key. Instead emit `S0|amplifier 0x<h> state=TRANSMIT_A` ~5 ms after TRANSMITTING (per keyed amp) and `S0|amplifier 0x<h> state=IDLE` ~5 ms after the second READY frame from C5 (per keyed amp).

**Why:** Rows D10 + D13. Canonical FLEX never sends explicit `pttA=` pushes. Amps derive PTT-in display from `S0|interlock state=...` plus `S0|amplifier state=...`. The current pttA=0 timing race contributes to the un-key high-SWR flash even after C5.

**Files:**
- Modify: `src/core/SmartSdrApiListener.cpp` (`broadcastTransmitting` and `setInterlockTransmitting` un-key branch + `broadcastUnkeySequence`)
- Modify: `tests/tst_smart_sdr_api_listener_ptt_chain.cpp` (add test function)

- [ ] **Step 1: Write the failing test for C6.**

Add to `private slots:`:

```cpp
    void c6_pttAPushesAreReplacedWithAmplifierStateBroadcasts();
```

Append:

```cpp
// Task 6 (C6): on key-down, no S<h>|amplifier 0x<h> pttA=1; instead
// S0|amplifier 0x<h> state=TRANSMIT_A for each keyed amp. On un-key,
// no pttA=0; instead state=IDLE. Matches pcap T+167.740 (state=TRANSMIT_A
// ~5 ms after TRANSMITTING) and T+168.881 (state=IDLE ~5 ms after the
// second READY).
void SmartSdrApiListenerPttChainTest::c6_pttAPushesAreReplacedWithAmplifierStateBroadcasts()
{
    SmartSdrApiListener listener;
    QVERIFY(listener.start(QHostAddress::LocalHost, 0));
    const quint16 port = listener.serverPort();

    QTcpSocket tgxl, pgxl;
    tgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(tgxl.waitForConnected(1000));
    pgxl.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(pgxl.waitForConnected(1000));
    drain(&tgxl); drain(&pgxl);

    registerFakeAmp(&tgxl, QStringLiteral("TunerGeniusXL"), QStringLiteral("TG"));
    registerFakeAmp(&pgxl, QStringLiteral("PowerGeniusXL"), QStringLiteral("PG-XL"));
    drain(&tgxl); drain(&pgxl);

    tgxl.write("C9|transmit tune on\n");
    tgxl.flush();
    drain(&tgxl);

    // Key-down.
    listener.setInterlockTransmitting(true, QStringLiteral("TUNE"));
    drain(&tgxl); drain(&pgxl);
    tgxl.write("C10|interlock ready 1\n"); tgxl.flush();
    pgxl.write("C10|interlock ready 2\n"); pgxl.flush();
    QSignalSpy grantSpy(&listener, &SmartSdrApiListener::interlockGranted);
    QVERIFY(grantSpy.wait(500));

    // Capture key-down chatter for ~50 ms past the grant.
    const QByteArray keyDownBytes = drain(&tgxl, 50);
    const QString keyDownText = QString::fromUtf8(keyDownBytes);
    QVERIFY2(!keyDownText.contains(QStringLiteral("pttA=1")),
             qPrintable(QStringLiteral("expected no pttA=1 push, got: ") + keyDownText));
    QVERIFY2(keyDownText.contains(QStringLiteral("state=TRANSMIT_A")),
             qPrintable(QStringLiteral("expected state=TRANSMIT_A broadcast, got: ") + keyDownText));

    // Un-key.
    drain(&pgxl);  // clear pgxl buffer
    listener.setInterlockTransmitting(false, QStringLiteral("TUNE"));
    const QByteArray unKeyBytes = drain(&tgxl, 100);
    const QString unKeyText = QString::fromUtf8(unKeyBytes);
    QVERIFY2(!unKeyText.contains(QStringLiteral("pttA=0")),
             qPrintable(QStringLiteral("expected no pttA=0 push, got: ") + unKeyText));
    QVERIFY2(unKeyText.contains(QStringLiteral("state=IDLE")),
             qPrintable(QStringLiteral("expected state=IDLE broadcast, got: ") + unKeyText));
}
```

- [ ] **Step 2: Run, expect FAIL.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$'
```

Expected: FAIL because current code emits `pttA=1` / `pttA=0` and does not emit `state=TRANSMIT_A` / `state=IDLE`.

- [ ] **Step 3: Replace the key-down pttA=1 push with state=TRANSMIT_A.**

In `src/core/SmartSdrApiListener.cpp`, inside `broadcastTransmitting` (the helper introduced in C4), find this block (the trailing per-amp `pttA=1` push):

```cpp
    // Per-amp pttA=1 push (unchanged from C3 state; C6 retires it).
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->ampHandle.isEmpty()) { continue; }
        const QString ampBody = QStringLiteral("amplifier 0x%1 pttA=1")
                                    .arg(it->ampHandle);
        const QByteArray ampFrame =
            QStringLiteral("S%1|%2\n").arg(it->handle).arg(ampBody).toUtf8();
        QTcpSocket* sock = it.key();
        if (sock && sock->isOpen()) { sock->write(ampFrame); }
    }
    qCInfo(lcSmartSdr) << "TX S<h>|amplifier pttA=1 to all subscribed amps";
```

Replace with:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: replace the
    // explicit pttA=1 push with canonical state=TRANSMIT_A broadcasts,
    // emitted ~5 ms after TRANSMITTING (pcap T+167.734 -> T+167.740).
    // Per keyed amp, broadcast to every subscriber. Amps derive their
    // PTT-in display from state= now; pttA= is no longer needed.
    QStringList keyedHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            keyedHandles << it->ampHandle;
        }
    }
    QTimer::singleShot(5, this, [this, keyedHandles]() {
        for (const QString& h : keyedHandles) {
            const QString body = QStringLiteral("amplifier 0x%1 state=TRANSMIT_A").arg(h);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|amplifier 0x" << h << "state=TRANSMIT_A";
        }
    });
```

- [ ] **Step 4: Remove the pttA=0 push from the un-key branch and add state=IDLE.**

In `setInterlockTransmitting` un-key branch (post-C5), find:

```cpp
        // Preserve the existing per-amp pttA=0 push for now; C6 retires it.
        for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
            if (it->ampHandle.isEmpty()) { continue; }
            const QString body = QStringLiteral("amplifier 0x%1 pttA=0")
                                     .arg(it->ampHandle);
            const QByteArray frame =
                QStringLiteral("S%1|%2\n").arg(it->handle).arg(body).toUtf8();
            QTcpSocket* sock = it.key();
            if (sock && sock->isOpen()) { sock->write(frame); }
        }
        qCInfo(lcSmartSdr) << "TX S<h>|amplifier pttA=0 to all subscribed amps"
                            << "(retires in C6)";

        broadcastUnkeySequence(initiator);
```

Replace with:

```cpp
        // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: no explicit
        // pttA=0 push. Canonical FLEX never sends it. broadcastUnkey-
        // Sequence handles the state=IDLE broadcast 5 ms after the second
        // READY frame so it lands at pcap T+168.881 timing.
        broadcastUnkeySequence(initiator);
```

- [ ] **Step 5: Extend `broadcastUnkeySequence` to emit state=IDLE 5 ms after the second READY.**

Find the helper definition in `src/core/SmartSdrApiListener.cpp`. After the `QTimer::singleShot(3, ...)` block for frame 3, append:

```cpp
    // 2026-05-21 4o3a-lan-ptt-pcap-divergence.md §8 C6: state=IDLE
    // broadcast ~5 ms after the second READY frame (pcap T+168.881).
    // One per amp that was keyed.
    QStringList keyedHandles;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it->interlockId == 0 || !it->interlockEnabled) { continue; }
        if (!it->ampHandle.isEmpty()) {
            keyedHandles << it->ampHandle;
        }
    }
    QTimer::singleShot(8, this, [this, keyedHandles]() {
        for (const QString& h : keyedHandles) {
            const QString body = QStringLiteral("amplifier 0x%1 state=IDLE").arg(h);
            const QByteArray frame =
                QStringLiteral("S0|%1\n").arg(body).toUtf8();
            for (auto jt = m_clients.cbegin(); jt != m_clients.cend(); ++jt) {
                QTcpSocket* sock = jt.key();
                if (sock && sock->isOpen()) { sock->write(frame); }
            }
            qCInfo(lcSmartSdr) << "TX S0|amplifier 0x" << h << "state=IDLE";
        }
    });
```

(8 ms = 3 ms to second READY + 5 ms gap. Matches pcap T+168.881 relative to T+168.874 UNKEY_REQUESTED.)

- [ ] **Step 6: Rebuild and run the test 2x.**

```bash
cmake --build build --target tst_smart_sdr_api_listener_ptt_chain 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R '^tst_smart_sdr_api_listener_ptt_chain$' --repeat until-pass:2
```

Expected: build clean, two consecutive PASSes. All six test functions (smoke + c1 + c2 + c3 + c4 + c5 + c6) green.

- [ ] **Step 7: Commit.**

```bash
git add src/core/SmartSdrApiListener.cpp \
        tests/tst_smart_sdr_api_listener_ptt_chain.cpp
git commit -m "$(cat <<'EOF'
fix(amp): retire pttA pushes, broadcast canonical amplifier states (C6)

Implements C6 from docs/architecture/4o3a-lan-ptt-pcap-divergence.md.
Stops emitting S<h>|amplifier 0x<h> pttA=1 on key-down and pttA=0 on
un-key (NereusSDR-specific divergence; canonical FLEX never does this).

Replaces them with S0|amplifier 0x<h> state=TRANSMIT_A 5 ms after
TRANSMITTING (per keyed amp) and S0|amplifier 0x<h> state=IDLE 5 ms
after the second READY frame. Matches pcap T+167.740 and T+168.881.

Amps now derive their PTT-in / TRANSMIT_A display from the canonical
state= sequence: S0|interlock state=PTT_REQUESTED then state=TRANSMITTING
then S0|amplifier 0x<h> state=TRANSMIT_A. Eliminates the pttA=0 timing
race that contributed to the un-key high-SWR flash even after C5.

Test asserts neither pttA=1 nor pttA=0 appears on the wire, and the
state=TRANSMIT_A / state=IDLE broadcasts do appear at the expected
points in the cycle.
EOF
)"
```

---

## Task 7: Epic Gate (Full Test Suite + Manual Bench Matrix)

**Goal:** Prove the branch ships without regressions before handing off to bench. Run the full ctest suite, build a clean release binary, and execute the §9 10-cycle manual bench matrix from the design doc.

**Files:**
- Modify: `docs/architecture/4o3a-lan-ptt-pcap-divergence.md` (append §13 results table)

- [ ] **Step 1: Run the full ctest suite.**

```bash
ctest --test-dir build --output-on-failure -j 4 2>&1 | tail -40
```

Expected: 100% pass rate. Pre-existing failures must NOT be ignored per JJ's standing rule (`feedback_failed_tests_never_ignored.md`). If anything regresses, diagnose before continuing.

- [ ] **Step 2: Clean build the release binary.**

```bash
cmake --build build --target NereusSDR 2>&1 | tail -10
```

Expected: zero warnings on the touched files; binary produced at `build/NereusSDR`.

- [ ] **Step 3: Kill any running NereusSDR instance, then launch the freshly built one.**

```bash
pkill -f 'NereusSDR$' || true
./build/NereusSDR &
```

Expected: app opens, listener bound to 4992 (if FourO3A_Enabled=True), TGXL and PGXL auto-connect within ~10 seconds.

- [ ] **Step 4: Execute the §9 10-cycle bench matrix.**

For each of the ten rows in `docs/architecture/4o3a-lan-ptt-pcap-divergence.md` §9:

1. Power-cycle TGXL and PGXL (10 seconds off, then back on).
2. Wait for both amps to reconnect and reach OPERATE / IDLE state.
3. Execute the row's steps.
4. Inspect the bench log at `/tmp/nereus-maxbin-debug/log.txt` (or wherever JJ has it configured) for the expected canonical sequence:
   - PTT_REQUESTED has `tx_client_handle=0x<synthetic>` and `reason=AMP:<TG|PG-XL>`.
   - Both amps ACK within ~50 ms.
   - 30 ms settle before TRANSMITTING.
   - State=TRANSMIT_A broadcast ~5 ms after TRANSMITTING.
   - On un-key: UNKEY_REQUESTED then two READYs then state=IDLE.
5. Record pass / fail in the §13 table you will add in Step 5.

If any row fails, STOP. Diagnose. Return to the design doc and document what divergence the failure exposes. Do not iterate-and-hope.

- [ ] **Step 5: Append a §13 Bench Results table to the design doc.**

Add at the end of `docs/architecture/4o3a-lan-ptt-pcap-divergence.md`:

```markdown
## 13. Bench Results (2026-05-DD)

Bench run after C1 to C6 landed on branch claude/jolly-golick-11c3c3.

| Row | Scenario | Result | Notes |
|---|---|---|---|
| 1 | TUNE x OPERATE clean | PASS / FAIL | TGXL ACK: <ms>, PGXL ACK: <ms>, settle: <ms>, unkey: clean / flash |
| 2 | TUNE x STANDBY | PASS / FAIL |  |
| 3 | MOX-LSB x OPERATE | PASS / FAIL |  |
| 4 | MOX-LSB x STANDBY | PASS / FAIL |  |
| 5 | MOX-CW x OPERATE | PASS / FAIL |  |
| 6 | Rapid-fire 5 tunes/3s | PASS / FAIL |  |
| 7 | Amp power cycle mid-TX | PASS / FAIL |  |
| 8 | DDC pan during TX | PASS / FAIL |  |
| 9 | 4O3A toggle during TX | PASS / FAIL |  |
| 10 | Fresh-boot first TUNE | PASS / FAIL |  |

**Conclusion:** <pcap-aligned / regression detected at row N>
```

Replace placeholders with real measurements from the bench session.

- [ ] **Step 6: Commit the bench results.**

```bash
git add docs/architecture/4o3a-lan-ptt-pcap-divergence.md
git commit -m "$(cat <<'EOF'
docs(amp): record 10-cycle bench results for C1-C6 pcap-alignment

Bench session captured all ten rows of the §9 verification matrix
following C1-C6 implementation. See §13 for the row-by-row table.
EOF
)"
```

- [ ] **Step 7: Open the PR.**

```bash
git push -u origin HEAD
gh pr create --title "fix(amp,tuner): pcap-align 4O3A LAN PTT chain (C1-C6)" \
  --body "$(cat <<'EOF'
## Summary
- Aligns the SmartSDR API listener's PTT key-down and un-key wire format with canonical FlexRadio behavior captured in `flex-tgxl-direct-CONTROL.pcapng`.
- Implements C1 to C6 from the approved design at [docs/architecture/4o3a-lan-ptt-pcap-divergence.md](docs/architecture/4o3a-lan-ptt-pcap-divergence.md) (commit 559890a2).
- New unit test `tst_smart_sdr_api_listener_ptt_chain` covers each change with a wire-format assertion run against a real loopback TCP harness.

## Changes
- C1: synthetic local-client handle replaces amp banner handles in `tx_client_handle=`.
- C2: single `PTT_REQUESTED` frame with `reason=AMP:<initiator>` and empty `amplifier=`.
- C3: single `TRANSMITTING` frame with comma-separated `amplifier=` list.
- C4: 30 ms settle delay shared by success + timeout paths before `TRANSMITTING` and RF-flow gate release.
- C5: canonical un-key sequence `UNKEY_REQUESTED` then `READY` (empty reason) then `READY` (named reason).
- C6: explicit `pttA=` pushes retired; replaced with canonical `S0|amplifier state=TRANSMIT_A` / `state=IDLE` broadcasts.

## Test plan
- [x] `ctest --output-on-failure --repeat until-pass:2 -R tst_smart_sdr_api_listener_ptt_chain` passes
- [x] Full `ctest -j 4` suite green
- [x] §9 10-cycle bench matrix (see §13 in design doc)
EOF
)"
```

Expected: PR URL returned. Paste into the standing project notes.

---

## Self-Review

**Spec coverage:**
- C1 (synthetic local-client handle): Task 1 covers full surface area; tests in Task 1 prove stability and distinctness from banner handles. All five existing wire-format sites updated.
- C2 (single PTT_REQUESTED): Task 2 covers frame count + field content. `m_lastTuneInitiator` introduced + `initiatingAmpName(source)` helper covers both TUNE and MIC reason= paths.
- C3 (single TRANSMITTING with comma-separated amp list): Task 3 covers it.
- C4 (30 ms settle on success + timeout): Task 4 extracts `broadcastTransmitting` helper and gates both call sites on `QTimer::singleShot(30, ...)`.
- C5 (canonical unkey sequence): Task 5 introduces `broadcastUnkeySequence` and rewrites the un-key branch.
- C6 (retire pttA + add state= broadcasts): Task 6 covers key-down (`state=TRANSMIT_A`) and un-key (`state=IDLE`).
- Bench acceptance gate: Task 7 runs the full §9 matrix and records results.

No unaddressed requirements from §8 C1 to C6.

**Placeholder scan:** Every code step shows the actual code; no TBD / TODO. Every test step shows the assertion. Every commit step shows the exact `git commit -m` HEREDOC. Every cmake step shows the exact command.

**Type and signature consistency:**
- `m_localClientHandle` is `QString`; accessor `localClientHandle()` returns `QString`. Used consistently across all five rewrite sites and in tests.
- `m_lastTuneInitiator` is `QString`; consumed only via `initiatingAmpName(QString)`.
- `broadcastTransmitting(const QString& source)`, `broadcastUnkeySequence(const QString& initiatorName)`, and `initiatingAmpName(const QString& wireSource) const` all use `const QString&` parameters.
- `start(QHostAddress, quint16)` overload signature matches the QTcpServer::listen signature exactly.
- Test helpers `drain(QTcpSocket*, int)`, `waitFor(Pred, int)`, `registerFakeAmp(QTcpSocket*, QString, QString, QString)`, `findS0InterlockFrames(QByteArray, QString)` are referenced consistently.

**Ambiguity check:**
- C2 reason= for local MOX with no PGXL-class amp: explicit fallback to empty string, matches design doc Definitions.
- C4 timer interaction with QTimer::singleShot(0, advanceToTransmittingIfReady): the existing 0 ms re-entry call still fires; the 30 ms settle is layered on top inside `advanceToTransmittingIfReady`'s success branch only, not on the 0 ms no-amps-yet check.
- C5 timing values 2 / 3 ms are explicit (and 8 ms total for C6's state=IDLE 5 ms after the second READY). All offsets relative to the helper entry point.

Plan complete and saved to `docs/superpowers/plans/2026-05-21-4o3a-lan-ptt-chain-pcap-alignment.md`.
