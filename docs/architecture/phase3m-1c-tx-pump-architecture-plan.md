# Phase 3M-1c — TX Pump Architecture Redesign Plan

**Status:** v3 SHIPPED 2026-04-29 (supersedes v2 below).
**Date:** 2026-04-29.
**Branch:** `feature/phase3m-1c-polish-persistence`.
**Worktree:** `/Users/j.j.boyd/NereusSDR/.claude/worktrees/adoring-pare-b0402e`.
**Trigger:** post-3M-1c bench regression report (JJ, 2026-04-29). TUN
worked after a band-aid fix; SSB voice at low mic gain was "distorted
and gravelly". Investigation found the root cause is architectural,
not a tuning issue — the entire TX pump model adopted in this session
(D.1 / E.1 / L.4) is built on a misread of Thetis upstream.

---

## v3 supersession (2026-04-29)

**v2** (the plan body that follows below) shipped a QTimer-driven 5 ms
polling worker pulling 256-sample blocks from PortAudio mic via
`AudioEngine::pullTxMic`.  Block size 256 was chosen to satisfy the WDSP
r2-ring divisibility constraint (`2048 % 256 == 0`); fexchange2 (separate
float Iin/Qin/Iout/Qout) was kept from the v1 implementation.  At the
bench this still produced periodic stutter — the 5 ms × 256 = 51.2 kHz
pull rate is 6.67 % faster than the 48 kHz mic delivery rate, leading to
recurring zero-fill ticks every ~75 ticks.

**v3** (the implementation that landed in commits a344921..b5da0af)
replaces the polling layer with a Thetis-faithful `cm_main` semaphore
loop:

1. **Cadence source = radio mic frames.**  P1 EP6 mic16 byte zone +
   P2 UDP port 1026 are decoded on the network thread and pushed
   into a new `TxMicSource` ring (Thetis `Inbound()` port).
2. **Worker = pure semaphore wait** (`waitForBlock` → `drainBlock` →
   `fexchange0`).  No QTimer.  Mirrors Thetis cmbuffs.c:151-168
   one-to-one.
3. **PC mic = override path** (mirrors Thetis cmaster.c:379 `asioIN`).
   When `MicSource::Pc` AND `m_txInputBus` is open, the worker
   overlays PC mic samples on top of the radio mic samples in `m_in`
   before fexchange0.  HL2 `setMicSourceLocked(true)` keeps the
   override on regardless of UI state (HL2 has no mic jack).
4. **Block size = 64** (Thetis getbuffsize(48000) exact).
5. **WDSP API = fexchange0** (interleaved double I/Q, mirrors
   cmaster.c:389 callsite exactly).
6. **LOS = 3000 ms** (Thetis network.c:656 exact) — onWatchdogTick /
   onKeepAliveTick inject one zero block of `kBlockFrames=64` per
   `kMicLosTimeoutMs` window when no mic frame has arrived.
7. **VOX defensive guards on all 5 DEXP-touching setters** —
   `pdexp[ch]==nullptr` null-check in `setVoxRun`,
   `setVoxAttackThreshold`, `setVoxHangTime`, `setAntiVoxRun`,
   `setAntiVoxGain`.  Required because NereusSDR ports OpenChannel
   but not (yet) `create_dexp`, so the existing `txa.rsmpin.p` guard
   alone does not imply pdexp is allocated.  Functional VOX waits
   for the create_dexp port (separate follow-up).

### v3 commit chain

| # | SHA | Subject |
|---|-----|---------|
| 1 | a344921 | feat(3m-1c): TxMicSource — Thetis Inbound/cm_main port |
| 2 | a20f2b9 | feat(3m-1c): P1 EP6 mic16 extraction → TxMicSource |
| 3 | 287f673 | feat(3m-1c): P2 port 1026 mic frame parsing → TxMicSource |
| 4 | e4d3dc8 | refactor(3m-1c): TxChannel fexchange2→fexchange0 + interleaved double + 64-block |
| 5 | 33de2a3 | refactor(3m-1c): TxWorkerThread semaphore-wake (no QTimer) |
| 6 | 07e2508 | feat(3m-1c): VOX defensive guards on all 5 DEXP-touching setters |
| 7 | b5da0af | feat(3m-1c): RadioModel TxMicSource lifecycle + AudioEngine PC override gate |

### v3 test surface (added)

* `tst_tx_mic_source` — 8 cases: ring lifecycle, semaphore wake,
  partial-block accumulation, wrap-aware writes, stop-while-waiting,
  concurrent SPSC.
* `tst_p1_mic_extraction` — 3 cases: parseEp6Frame mic-aware overload
  (normal samples, HL2-style zeros, backward-compat with 3-arg).
* `tst_p2_mic_frame` — 3 cases: decodeMicFrame132 (normal, all-zero,
  wrong-size).
* `tst_tx_worker_thread` — rewritten 9 cases: lifecycle + semaphore
  drive + PC mic override path + cross-thread setter races.
* `tst_radio_model_3m1b_ownership` — +1 case
  (`txMicSourceNullBeforeConnect`).

Full ctest run: 238/238 passing (235 baseline + 3 net new — the QTimer
cadence test from v2 was dropped).

### v3 deferred follow-ups

* `create_dexp` port (full DEXP/VOX functional path).  Without it
  VOX is null-guarded but functionally inert.
* Full `xcmaster` pipeline beyond `xdexp`/`fexchange0`: `xpipe`,
  `xMixAudio`, `xtxgain`, `xeer`, `xilv`, `xsidetone`.
* PortAudio frames-per-buffer wiring for the dead
  `pcMicBufferSamples` slider.
* Radio-mic seq# error reporting UI (P1/P2 mic_in_seq_err).

---

## Original v2 plan body (historical context)

This plan documents:

- The actual Thetis + deskhpsdr TX-pump architectures
- Why pre-code review §0.5's lock of "720 samples for source-first
  parity" was a misread of `cmInboundSize[5]`
- The cascade of consequences: D.1 → E.1 → L.4 → silence-drive thrash
  → bench regression
- The recommended fix (Option B in the architectural review): worker
  thread + ring + matching block size, mirroring Thetis's
  `cmaster.c` / `cmbuffs.c` exactly
- A complete subagent dispatch prompt the next session can use directly

---

## 1. Thetis upstream — the authoritative reference

The TX pipeline lives in **`Project Files/Source/ChannelMaster/cmaster.c`
+ `cmbuffs.c`**. Native C, called from the C# layer via P/Invoke.
Architecture is **callback-driven via a per-stream worker thread**.

### 1.1 Audio source → ring buffer (`Inbound`)

Audio source (PortAudio, IVAC, TCI) calls `Inbound(stream, nsamples,
samples)` each time new mic samples arrive. From `cmbuffs.c:89-121
[v2.10.3.13]`:

```c
PORT
void Inbound (int id, int nsamples, double* in)
{
    int n;
    int first, second;
    CMB a = pcm->pebuff[id];

    if (_InterlockedAnd (&a->accept, 1))
    {
        EnterCriticalSection (&a->csIN);
        if (nsamples > (a->r1_active_buffsize - a->r1_inidx))
        {
            first = a->r1_active_buffsize - a->r1_inidx;
            second = nsamples - first;
        }
        else
        {
            first = nsamples;
            second = 0;
        }
        memcpy (a->r1_baseptr + 2 * a->r1_inidx, in,
                first  * sizeof (complex));
        memcpy (a->r1_baseptr,                   in + 2 * first,
                second * sizeof (complex));

        if ((a->r1_unqueuedsamps += nsamples) >= a->r1_outsize)
        {
            n = a->r1_unqueuedsamps / a->r1_outsize;
            ReleaseSemaphore (a->Sem_BuffReady, n, 0);
            a->r1_unqueuedsamps -= n * a->r1_outsize;
        }
        if ((a->r1_inidx += nsamples) >= a->r1_active_buffsize)
            a->r1_inidx -= a->r1_active_buffsize;
        LeaveCriticalSection (&a->csIN);
    }
}
```

Two key behaviors:

1. **Ring write**: appends `nsamples` complex samples to the ring at
   `r1_inidx`, with wrap-around handling.
2. **Semaphore release**: if `r1_unqueuedsamps` accumulates to one or
   more `r1_outsize`-blocks, releases that many semaphores. **One
   semaphore = one block ready for DSP.**

### 1.2 Worker thread (`cm_main`)

Started via `_beginthread` per stream. From `cmbuffs.c:151-168
[v2.10.3.13]`:

```c
void cm_main (void *pargs)
{
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"),
                                                 &taskIndex);
    if (hTask != 0) AvSetMmThreadPriority(hTask, 2);
    else SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    int id = (int)pargs;
    CMB a = pcm->pdbuff[id];

    while (_InterlockedAnd (&a->run, 1))
    {
        WaitForSingleObject(a->Sem_BuffReady, INFINITE);
        cmdata (id, pcm->in[id]);
        xcmaster(id);
    }
    _endthread();
}
```

**Pattern:**

1. Promote thread to "Pro Audio" priority class on Windows.
2. Loop forever:
   - Block on `Sem_BuffReady` semaphore.
   - When released: drain one `r1_outsize`-block from ring into
     `pcm->in[id]` via `cmdata()`.
   - Call `xcmaster(id)` — runs `fexchange0` plus surrounding DSP
     stages.

**No timer. No polling.** The semaphore is signaled by `Inbound()` when
data is ready; the worker wakes, drains exactly one block, runs DSP.

### 1.3 `xcmaster` (the DSP dispatcher)

From `cmaster.c:340-405 [v2.10.3.13]`. The TX branch (`case 1`):

```c
case 1:  // standard transmitter
    tx = txid (stream);
    asioIN(pcm->in[stream]);
    if (_InterlockedAnd (&pcm->xmtr[tx].use_tci_audio, 1))
    {
        if (pcm->InboundTCITxAudio)
            (*pcm->InboundTCITxAudio)(pcm->xcm_insize[stream],
                                       pcm->in[stream]);
        else
            memset (pcm->in[stream], 0,
                    pcm->xcm_insize[stream] * sizeof (complex));
    }
    xpipe (stream, 0, pcm->in);
    xdexp (tx);                                  // VOX/DEXP
    fexchange0 (chid (stream, 0), pcm->in[stream],
                pcm->xmtr[tx].out[0], &error);   // ← SINGLE fexchange0
    xsidetone(tx);
    xpipe (stream, 1, pcm->xmtr[tx].out);
    xMixAudio (0, 0, chid (stream, 0),
               pcm->xmtr[tx].out[2]);            // monitor mix
    xtxgain (pcm->xmtr[tx].pgain);
    xeer (pcm->xmtr[tx].peer);                   // EER
    xilv (pcm->xmtr[tx].pilv,
          pcm->xmtr[tx].out);                    // → Outbound() to UDP
    break;
```

**One `fexchange0` per worker tick. One worker tick per
`r1_outsize`-block delivered by `Inbound()`.**

### 1.4 The block-size invariant

From `cmaster.c:460-487 [v2.10.3.13]`:

```c
pcm->xcm_insize[in_id] = getbuffsize (rate);
SetCMRingOutsize(in_id, pcm->xcm_insize[in_id]);
// ...
SetInputBuffsize (chid (in_id, 0), pcm->xcm_insize[in_id]);
SetSiphonInsize  (rx, pcm->xcm_insize[in_id]);
SetIVACiqSizeAndRate (rx, pcm->xcm_insize[in_id], pcm->xcm_inrate[in_id]);
SetDEXPSize (tx, pcm->xcm_insize[in_id]);
```

**Three sizes are forced equal:**

- `pcm->xcm_insize[in_id]` — the value passed into `fexchange0`.
- `r1_outsize` (the ring drain size) — set via `SetCMRingOutsize(...)`.
- WDSP channel `in_size` — set via `SetInputBuffsize(...)`.

All three = `getbuffsize(rate)`. For rate = 48000, this is **64**
samples (per `cmsetup.c:106-110 [v2.10.3.13]`):

```c
int getbuffsize (int rate)
{
    return (int)(64 * rate / 48000);
}
```

So Thetis's TX `fexchange0` runs **once per 64 samples**, every
**1.33 ms** at 48 kHz. Block size is uniform end-to-end. No
re-blocking, no accumulator with mismatched cadence.

### 1.5 The 720 number — what it actually is

`cmaster.cs:495 [v2.10.3.13]`:

```csharp
int[] cmInboundSize = new int[8] { 240, 240, 240, 240, 240, 720, 240, 240 };
```

`cmInboundSize` is the **inbound network block size from the radio for
each of 8 streams**. Index 5 (mic input) is 720 samples. This is the
hardware/network frame size — what arrives in a P1 EP6 mic byte zone or
P2 mic packet from the radio's ADC, multiplexed onto the Ethernet
stream.

`Inbound()` in cmaster receives this 720-sample chunk, writes it into
the ring, and the ring drains in `r1_outsize` (= 64) chunks via
`cm_main`. **`cmaster` re-chunks 720 → 64 internally before fexchange.**

**Pre-code review §0.5 misread `cmInboundSize[5]=720` as the DSP block
size.** It is not. It is the network arrival block size, and `cmaster`
absorbs it into the ring on the network side; the DSP side runs at 64.

## 2. deskhpsdr — the simpler reference

deskhpsdr (a piHPSDR fork) doesn't have a separate ChannelMaster
component. The TX pipeline is inline in `transmitter.c` and runs
**synchronously on the audio callback thread**.

From `transmitter.c:2021-2028 [@120188f]`:

```c
tx->mic_input_buffer[tx->samples * 2]     = mic_sample_double;  // I = mic
tx->mic_input_buffer[(tx->samples * 2)+1] = 0.0;                // Q = 0
tx->samples++;

if (tx->samples == tx->buffer_size) {
    tx_full_buffer(tx);   // ← calls fexchange0 synchronously
    tx->samples = 0;
}
```

Each individual mic sample is appended to `tx->mic_input_buffer` (one
sample at a time, interleaved with Q=0). When the buffer reaches
`tx->buffer_size`, `tx_full_buffer(tx)` runs — which calls `fexchange0`
inline on the audio callback thread.

This is the simplest possible model: **one fexchange0 per
`tx->buffer_size` samples accumulated**. Same uniform-block-size
invariant as Thetis, but inline rather than worker-thread.

The downside is that `fexchange0` runs on the audio callback thread.
For desktop hardware with plenty of CPU this is fine; for slower
hardware (e.g., Pi-class, multi-channel work) it risks audio xruns.

## 3. NereusSDR's 3M-1c design — what we built and why it's wrong

| Layer | Thetis / deskhpsdr | NereusSDR 3M-1c |
|---|---|---|
| Pump driver | Audio data arrival (semaphore / inline) | `QTimer` in AudioEngine (5 ms) |
| Accumulator size | `r1_outsize == xcm_insize` (64 in Thetis, `tx->buffer_size` in deskhpsdr) | 720 (`kMicBlockFrames`, D.1) |
| DSP block size | Same as accumulator | 256 (`m_inputBufferSize`) |
| Re-blocker | None | `MicReBlocker` (L.4) — bridges 720 → 256 |
| Silence drive | None (audio source delivers zeros) | `TxChannel::onSilenceTimer` (commit 418e155) |
| Cadence | 1.33 ms (Thetis) / `tx->buffer_size`/sample_rate (deskhpsdr) | Bursty: 2-3 fexchange calls in <1 ms, then 14 ms idle |

The cascade of bugs:

1. **Pre-code review §0.5** misread `cmInboundSize[5]=720` as the DSP
   block size and locked the AudioEngine accumulator at 720.
2. **D.1** built a 720-sample accumulator in AudioEngine that emits
   `micBlockReady` when full.
3. **E.1** push-driven refactor of TxChannel made `driveOneTxBlock` a
   slot accepting `(samples, frames)` — but TxChannel's
   `m_inputBufferSize` is 256 (because 256 divides the WDSP r2-ring's
   2048; 720 doesn't). E.1 spec compliance reviewer flagged this real
   constraint, locked the contract guard at 256, and surfaced "Phase L
   needs a 720→256 re-blocker" as a new task.
4. **L.4** added `MicReBlocker` to bridge 720→256. Re-chunking creates
   a burst pattern: per `micBlockReady` emit (every ~15 ms),
   `MicReBlocker` fires `driveOneTxBlock` 2-3 times in immediate
   succession (within <1 ms), then idles for ~14 ms.
5. **Bench regression**: TxChannel's QTimer was removed in E.1 but
   nothing replaced the production-side pump for `pullTxMic`. `cm_main`
   in Thetis is driven by `Inbound()` calling `ReleaseSemaphore`; in
   NereusSDR, no equivalent driver existed. Result: `pullTxMic` was
   never called → accumulator never filled → `micBlockReady` never
   fired → `fexchange2` never ran. **TUN and SSB voice TX both silent
   in production**, despite all 236 tests passing (tests called
   `driveOneTxBlock` directly with synthetic samples, never exercising
   the production pump).
6. **Bench-fix attempt A** (commit 418e155): added a 5 ms `QTimer` in
   AudioEngine that calls `pullTxMic`. Restores TUN.
7. **Bench-fix attempt B** (commit 418e155): added a 5 ms silence-drive
   timer in TxChannel for the no-mic case.
8. **Bench regression #2** (JJ at the bench, 2026-04-29): SSB voice at
   low mic gain sounds "distorted and gravelly". Diagnosis: silence
   timer fires during the natural mic-burst gap (between
   `MicReBlocker` bursts, every ~15 ms), adding extra `fexchange2`
   calls (~4 per 15 ms instead of ~2.81). Net output rate runs ~42% over
   the radio's expected 192 kHz → SPSC ring overflows and interleaved
   silence frames → audible distortion.
9. **Bench-fix attempt C** (commit 03ec584): bumped silence-drive
   threshold from 8 ms to 20 ms. Stops mid-burst-gap firing but does
   NOT address the root architectural mismatch.
10. **JJ pause + architectural review (this doc)**: the entire pump
    model is fighting Thetis's design. Time to redesign.

## 4. Recommendation: Option B — Thetis-style worker thread + ring

(Option A: revert to pre-3M-1c QTimer-on-main-thread pump — rejected
because pre-3M-1c was also flawed. Option C: deskhpsdr inline — rejected
because running fexchange on the audio callback thread risks xruns on
slower hardware.)

### 4.1 Architecture

```
┌─ Audio thread (PortAudio / CoreAudio HAL callback) ────────────────┐
│                                                                    │
│  mic samples → write into AudioEngine bus SPSC ring                │
│  (existing today; already RT-safe — no DSP, no allocs, no logging) │
│                                                                    │
│  CHANGE FROM CURRENT: ring fill remains as-is.  The pump moves     │
│  off the main thread to TxWorkerThread.                            │
└────────────────────────────────────────────────────────────────────┘
                              ▼
┌─ TxWorkerThread (NEW: dedicated QThread) ──────────────────────────┐
│                                                                    │
│  QTimer @ ~5 ms, lives on TxWorkerThread (started via              │
│  moveToThread + start()):                                          │
│                                                                    │
│  void onPumpTick():                                                │
│      static thread_local std::array<float, 256> buffer;            │
│      const int got = audioEngine->pullTxMic(buffer.data(), 256);   │
│      if (got < 256) {                                              │
│          // Bus empty / null / partial — zero-fill the gap.        │
│          // Silence path falls out for free; no separate timer.    │
│          std::fill(buffer.begin() + got, buffer.end(), 0.0f);      │
│      }                                                             │
│      txChannel->driveOneTxBlock(buffer.data(), 256);               │
│        └→ runs fexchange2 HERE (off main, off audio thread)        │
│        └→ pushes I/Q to connection's SPSC ring (existing)          │
│                                                                    │
│  Future polish: replace QTimer with a `QSemaphore`-based wake from │
│  the audio thread when the bus has >= 256 samples ready (closer    │
│  to Thetis's `Inbound() → ReleaseSemaphore` pattern).  Functionally│
│  equivalent; QTimer is a polling approximation that's simpler.     │
└────────────────────────────────────────────────────────────────────┘
                              ▼
                          Connection's SPSC ring
                          (drained by P2 / P1 connection thread,
                          existing infrastructure)
                              ▼
                          Radio
```

### 4.2 Block-size invariant (matching Thetis)

```
m_inputBufferSize == 256  (TxChannel, dictated by WDSP r2-ring)
                ||
worker tick block size == 256  (single uniform end-to-end size)
                ||
fexchange2 in_size == 256  (already configured this way)
```

**One pull, one fexchange2, one block.** Exactly the
`r1_outsize == xcm_insize == in_size` invariant from Thetis
`cmaster.c:460-474`.

### 4.3 What this solves

- **Steady cadence.** Worker timer ticks every ~5 ms (matching the
  natural PC mic 256-samples-per-5.33-ms delivery rate), one fexchange2
  per tick. Output rate ≈ 192 kHz steady, matches the radio's
  expectation.
- **Silence path falls out for free.** When PC mic is missing or open
  failed, `pullTxMic` returns 0, the worker zero-fills the rest of the
  buffer, and `driveOneTxBlock(zeros, 256)` runs. fexchange2 with zero
  input still produces PostGen TUNE-tone output (TUN works) and silent
  SSB (correct, no mic = no modulation). **No separate silence-drive
  timer needed.**
- **Off the main thread.** Qt UI stays responsive during TX.
  fexchange2 + sendTxIq run on the worker. Main thread does zero DSP
  work.
- **Off the audio thread.** Audio callback thread continues to do only
  bus ring-fill (RT-safe). No fexchange2 on the OS audio callback.
- **No re-blocker.** `MicReBlocker` is deleted entirely.
- **No 720-sample accumulator.** `kMicBlockFrames`, `m_micBlockBuffer`,
  `m_micBlockFill`, `clearMicBuffer`, and `micBlockReady` are all
  deleted from AudioEngine.
- **Future-ready.** When 3M-4 (PureSignal) lands its feedback DDC +
  IQC engine, the same worker-thread-per-channel pattern composes:
  add a `RxWorkerThread` for PS feedback, etc.

### 4.4 Cross-thread parameter mutation

Every existing TxChannel setter that's called from the GUI thread
(setMicPreamp, setVoxAttackThreshold, setMicMute, the 12 TXPostGen
wrappers, etc.) needs a thread-safety story now that fexchange2 runs
on TxWorkerThread.

**Recommended pattern:** all TxChannel public setters become Qt slots,
and GUI-thread connections to them become `Qt::QueuedConnection` (or
`Qt::AutoConnection`, which Qt auto-resolves to Queued when the
emitter is on a different thread than the receiver). The setter call
gets posted to TxWorkerThread's event loop, which runs it between
`onPumpTick` invocations.

This is the standard Qt pattern for cross-thread method invocation.
No mutexes needed in TxChannel for setter state.

For the few hot setters that should run synchronously without queuing
(e.g., `setMicPreamp` which is called per-mic-block), atomics work —
already done for `setMicPreamp` in 3M-1b.

The implementation should audit each TxChannel setter and decide:
either Qt slot + Queued, or atomic. Document the choice inline.

## 5. Implementation plan

### 5.1 Files to ADD

#### `src/core/TxWorkerThread.h` + `.cpp`

A minimal `QThread` subclass that owns its own QTimer (the pump tick)
and the lifetime of TxChannel + its connection to AudioEngine.

```cpp
class TxWorkerThread : public QThread {
    Q_OBJECT
public:
    explicit TxWorkerThread(QObject* parent = nullptr);
    ~TxWorkerThread() override;

    // Set/clear the components the worker drives.  Called from main
    // thread before the thread starts (or after stop).
    void setTxChannel(TxChannel* ch);
    void setAudioEngine(AudioEngine* engine);

    // Start the pump (creates QTimer on this thread, starts it).
    // Stop drains pending events and exits the event loop.
    void startPump();
    void stopPump();

protected:
    void run() override;   // QThread::run — sets up event loop + timer

private slots:
    void onPumpTick();     // Pump-driven 5 ms tick

private:
    QTimer*       m_pumpTimer{nullptr};   // owned by worker thread
    TxChannel*    m_txChannel{nullptr};   // not owned
    AudioEngine*  m_audioEngine{nullptr}; // not owned
    static constexpr int kPumpIntervalMs = 5;
    static constexpr int kBlockFrames    = 256;  // matches WDSP r2-ring
};
```

#### `tests/tst_tx_worker_thread.cpp`

Unit tests:

- Worker constructs / starts / stops cleanly
- `startPump()` actually fires `onPumpTick` at ~5 ms cadence (use
  `QSignalSpy` + `QElapsedTimer` to measure; tolerance ±2 ms)
- `onPumpTick` calls `pullTxMic` exactly once per tick
- `onPumpTick` calls `driveOneTxBlock(samples, 256)` when bus has data
- `onPumpTick` calls `driveOneTxBlock(zeros, 256)` when bus is null
  / partial
- Cross-thread setter dispatch: setting a TxChannel parameter from the
  main thread queues correctly to the worker (verify via signal-slot
  spy)
- Lifecycle: stopPump() while a tick is in flight completes the
  current tick before exiting

### 5.2 Files to MODIFY

#### `src/core/TxChannel.h` + `.cpp`

- **Drop** the silence-drive timer (commit 418e155 changes) —
  `m_silenceTimer`, `m_lastDriveTimer`, `kSilenceTimerIntervalMs`,
  `kSilenceStaleThresholdMs`, `onSilenceTimer()` slot, the include of
  `QElapsedTimer`.
- **Drop** the `m_lastDriveTimer.restart()` call at the end of
  `driveOneTxBlock`.
- **Drop** the silence-timer wire-up in `setRunning(true/false)`.
- **Keep** `driveOneTxBlock(const float* samples, int frames)` slot
  signature — TxWorkerThread is the new caller.
- **Audit** all public setters for cross-thread safety. Mark each as
  one of: (a) Qt slot, will be invoked via Qt::QueuedConnection from
  main thread; (b) atomic, lock-free. Document inline.

#### `src/core/AudioEngine.h` + `.cpp`

- **Drop** D.1 entirely:
  - `static constexpr int kMicBlockFrames = 720`
  - `std::array<float, kMicBlockFrames> m_micBlockBuffer`
  - `int m_micBlockFill = 0`
  - `void micBlockReady(const float* samples, int frames)` signal
  - The accumulator-and-emit lines inside `pullTxMic` (the for-loop at
    lines ~1014-1024)
- **Drop** D.2: `clearMicBuffer()` method.
- **Drop** the bench-fix-A `pumpMic` timer:
  - `m_micPumpTimer`, `kMicPumpIntervalMs`
  - `pumpMic()` method
  - The constructor wire-up
  - The `<QTimer>` include if no longer needed
- **Keep** `pullTxMic(float* dst, int n)` as the canonical bus drain.
  TxWorkerThread calls it.
- **Add** thread-safety doc-comments on `pullTxMic` saying it MUST be
  called from TxWorkerThread (not main thread, not audio callback).

#### `src/models/RadioModel.h` + `.cpp`

- **Add** an `std::unique_ptr<TxWorkerThread> m_txWorker` member.
- **In `connectToRadio()`**:
  - Construct `m_txWorker` after TxChannel is created.
  - Call `m_txWorker->setTxChannel(m_txChannel)` and
    `m_txWorker->setAudioEngine(m_audioEngine)`.
  - Move TxChannel to the worker thread:
    `m_txChannel->moveToThread(m_txWorker.get())`.
  - Call `m_txWorker->start()` (which internally sets up the QTimer
    and starts the event loop).
- **In `teardownConnection()`**:
  - `m_txWorker->stopPump()` first.
  - `m_txWorker->quit()` + `m_txWorker->wait()`.
  - `m_txChannel->moveToThread(this->thread())` before destruction
    (move back to main thread so TxChannel destructor runs cleanly).
  - `m_txWorker.reset()`.
- **Drop** the L.4 `MicReBlocker` construction, lambda, and connection.
  Replace with the worker-thread setup above.
- **Update** the L.2 fixup connect lambdas: each connect from
  TransmitModel two-tone signals to TxChannel TXPostGen setters now
  dispatches via `Qt::AutoConnection` (which Qt auto-resolves to
  Queued when receiver is on TxWorkerThread). Verify each setter has
  the `Q_INVOKABLE` or slot annotation needed for QueuedConnection.

#### `src/core/audio/MicReBlocker.h` + `.cpp` — DELETE

The class is no longer needed. The single-block-size invariant
eliminates the re-block requirement.

#### `src/core/MoxController.h` + `.cpp`

The 3-arg `moxChanged(int rx, bool oldMox, bool newMox)` signal stays.
The connections to TxChannel slots (e.g., `setMicMute` via the
TwoToneController and existing wires) need to be Queued if TxChannel
is now on TxWorkerThread. Audit.

### 5.3 Tests to DROP

- `tests/tst_audio_engine_mic_block_ready.cpp` — D.1 signal no longer
  exists.
- `tests/tst_mic_re_blocker.cpp` — class deleted.
- The "silence timer fires" test in `tst_tx_channel_push_driven.cpp`
  (the `silenceTimer_drivesFexchange2_whenNoPushArrives` slot, which
  was added in commit 418e155 and refined in 03ec584).

### 5.4 Tests to ADD

- `tests/tst_tx_worker_thread.cpp` (described in §5.1).
- A bench-style integration test if feasible: construct
  RadioModel + AudioEngine + TxChannel + TxWorkerThread + a fake
  AudioBus delivering known samples; verify TxChannel actually emits
  TX I/Q at the correct cadence via
  `MockConnection::sendTxIq` call counter + sample-content check.
- Cross-thread setter race test: from main thread, rapidly call
  `setMicPreamp` while the worker is pumping; verify no crash, and
  that the setter eventually takes effect (visible via WDSP state
  read or via a `lastMicPreampForTest` getter).

### 5.5 Tests to UPDATE

- `tst_tx_channel_push_driven.cpp` — drop the silence-timer assertion;
  keep the rest as a synchronous-driveOneTxBlock contract test.
- `tst_radio_model_3m1b_ownership.cpp` — verify the new
  TxWorkerThread is constructed/destroyed during connect/teardown.

### 5.6 Doc updates

- **`phase3m-1c-post-code-review.md`** — append a section "Phase 3M-1c
  bench regression + architectural pivot (2026-04-29 amendment)"
  documenting:
  - The bench regression report (TUN broken initially, SSB gravelly
    after first fix attempt).
  - The architectural review that surfaced the `cmInboundSize=720`
    misread.
  - The decision to redesign per Option B.
  - Cross-reference to this plan doc.
- **`phase3m-1c-thetis-pre-code-review.md`** — add a correction note
  at §0.5 "Open items" lock #4: the 720-sample accumulator decision was
  based on a misread of `cmInboundSize[5]`. The corrected
  understanding is that `cmInboundSize` is the network arrival block
  size; the DSP block size in Thetis is `xcm_insize = getbuffsize(rate)
  = 64`. NereusSDR's 256 (matching WDSP r2-ring divisibility) is the
  correct end-to-end block size.
- **`phase3m-0-verification/README.md`** — update the 3M-1c rows that
  reference D.1 / E.1 / L.4 with notes that the architecture was
  redesigned post-3M-1c-execution per
  `phase3m-1c-tx-pump-architecture-plan.md`.

## 6. Subagent dispatch prompt (for the next session)

Use this prompt verbatim (or with minor edits) to dispatch a focused
subagent for the implementation:

````
You are an implementer subagent for the NereusSDR project executing
**Phase 3M-1c TX pump architecture redesign** — replacing the D.1
(`micBlockReady` accumulator) + E.1 (push-driven slot) + L.4
(`MicReBlocker`) + post-3M-1c bench-fix-A (AudioEngine pump) +
bench-fix-B (TxChannel silence-drive) chain with a Thetis-style
worker-thread + matching-block-size architecture.

## Working environment

- Worktree: `/Users/j.j.boyd/NereusSDR/.claude/worktrees/adoring-pare-b0402e`
- Branch: `feature/phase3m-1c-polish-persistence`
- Currently 28 commits ahead of `origin/main`. Do not switch branches.
  Do not rebase.
- `git log --oneline -3` should show: `<commit-of-the-handoff-doc>`,
  `03ec584 fix(3m-1c): bump silence-drive threshold ...`,
  `418e155 fix(3m-1c): E.1 bench regression ...`. If it doesn't, STOP
  and report BLOCKED.
- Tests baseline: 236/236 passing.
- Required env var before every commit:
  `export NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`

## Context — read before starting

`docs/architecture/phase3m-1c-tx-pump-architecture-plan.md` has the
complete spec including:

- Thetis `cm_main` worker-thread pattern (cmbuffs.c:151-168 and
  cmaster.c:340-405 [v2.10.3.13])
- Why pre-code review §0.5's lock of 720 was a misread of
  `cmInboundSize[5]` (network block, not DSP block)
- The full implementation plan (files to add, modify, delete)
- The single block-size invariant: 256 samples end-to-end
- Cross-thread setter safety guidance

Read that document fully before writing any code. Then read:

- `src/core/TxChannel.{h,cpp}` (full file)
- `src/core/AudioEngine.{h,cpp}` (full file)
- `src/models/RadioModel.{h,cpp}` (around the TX wiring blocks)
- `tests/tst_tx_channel_push_driven.cpp`,
  `tests/tst_audio_engine_mic_block_ready.cpp`,
  `tests/tst_mic_re_blocker.cpp` — these are the tests that change.

Then do the work per the plan. Use TDD where it helps (worker thread
unit tests, cross-thread setter race test).

## Discipline

- GPG-signed commits only. Never `--no-gpg-sign` or `--no-verify`.
- Pre-commit hook chain (5 verifiers) must pass on every commit.
- Two-stage review per task per `superpowers:subagent-driven-development`.
- Auto-launch the app after build (kill + relaunch from this worktree).
- Verify the running binary path matches this worktree before claiming
  bench-ready.

## Out of scope

- Don't reopen the architectural decisions documented in the plan.
- Don't add VAC / VAX integration for the worker thread (that's 3M-3
  scope when those backends ship).
- Don't change the existing connection-side SPSC ring or the
  P1/P2 transmit packetization — they're fine.
- Don't touch the TwoToneController (Phase I) — only its connections
  to TxChannel may need to become Queued. The class itself doesn't
  change.

## Reporting

Report ONE of: DONE / DONE_WITH_CONCERNS / NEEDS_CONTEXT / BLOCKED.
Be specific about commit SHA, ctest count (was 236/236; will likely
drop a few obsolete tests + add a few new ones), and any architectural
choices you made within the plan's allowance.
````

## 7. Estimated work + risk

**Estimated session work:** 1 substantial session — likely 3-5 hours
for a focused subagent + 2-stage review. Comparable in scope to the F
(MicProfileManager) or I (TwoToneController) phases.

**Risk profile: Medium-High.** Cross-thread Qt is subtle and easy to
get subtly wrong. Specific risks:

| Risk | Mitigation |
|---|---|
| Race on TxChannel setter from GUI thread vs worker thread fexchange | Audit + document each setter as Qt-slot + Queued OR atomic. Add a cross-thread setter race test. |
| Lifecycle: TxChannel destructor running on worker thread | Move TxChannel back to main thread before `~RadioModel`. Document in teardownConnection. |
| Worker thread doesn't stop cleanly on disconnect | `quit()` + `wait()` pattern. Test the disconnect path. |
| QTimer on worker thread doesn't tick at expected cadence | Use `Qt::PreciseTimer`. Validate cadence in tst_tx_worker_thread. |
| WDSP setter calls (e.g., from TwoToneController) hit thread issues | Audit Phase I's connect lines. May need to add Q_INVOKABLE to wrappers, or wrap setters in slots. |
| Bench: TUN works but SSB still has artifacts | The plan's silence-falls-out-for-free design directly addresses the gravelly distortion. If artifacts persist, would suggest a deeper WDSP issue — escalate. |

## 8. Verification

### 8.1 Unit tests

- All 236 existing pre-redesign tests must still pass (those that
  weren't slated for deletion in §5.3).
- New `tst_tx_worker_thread.cpp` ~6-8 cases.
- New cross-thread setter test ~2 cases.
- Net delta: ~233-235 tests passing post-redesign (drop 3-4, add ~8).

### 8.2 Bench rows

These bench rows from `phase3m-0-verification/README.md` are the
acceptance criteria for this redesign:

- `[3M-1c-bench]` TUN produces clean carrier on dummy load (was the
  bench regression that triggered this).
- `[3M-1c-bench-G2]` SSB voice TX at low mic gain — clean, not
  gravelly.
- `[3M-1c-bench-G2]` SSB voice TX at normal mic gain — clean speech
  reproduction.
- `[3M-1c-bench]` Mic-mute / no-mic / TUN-without-mic — TUN tone
  still produces clean carrier.
- `[3M-1c-bench-G2]` 30-minute SSB transmission — no zero-filled
  frames, no audible glitches, no SPSC ring overflows.

### 8.3 Source-first audit

- `xcm_insize == r1_outsize == in_size` invariant (Thetis cmaster.c:460
  / 474) reflected in the redesign's "256 end-to-end" structure.
- `cm_main` worker-thread pattern (Thetis cmbuffs.c:151-168) reflected
  in `TxWorkerThread::run()` + QTimer-on-worker.
- `cmInboundSize=720` correction documented in the post-code review
  amendment.

---

## 9. Out-of-scope (deferred)

These items are explicitly NOT part of this redesign:

- **Replace QTimer with QSemaphore-based wake** (closer to Thetis's
  `Inbound() → ReleaseSemaphore`). Functionally equivalent to QTimer
  but avoids polling overhead. Future polish.
- **Reduce block size from 256 to 64** to match Thetis exactly. Would
  need to verify WDSP r2-ring still divides cleanly (2048 % 64 = 0,
  yes). Lower latency but more fexchange calls per second. Future
  optimization.
- **Multi-channel TX support** (PureSignal feedback DDC, etc.). Phase
  3M-4 owns this; the worker-thread pattern composes naturally when
  that lands.
- **Move `pullTxMic` to live on the worker thread directly** rather
  than being called via cross-thread method invocation. Could be done
  via `pullTxMic` being made `Q_INVOKABLE` and the worker calling it
  via `QMetaObject::invokeMethod(Qt::DirectConnection)` — but if
  AudioEngine has thread affinity `main`, that's wrong. Likely the
  right answer is to make `pullTxMic` lock-free (it already accesses
  the SPSC ring which is lock-free) and document that callers may be
  on any thread. Out of scope for this redesign; verify in
  implementation that the existing locking discipline is OK.

---

End of plan.
