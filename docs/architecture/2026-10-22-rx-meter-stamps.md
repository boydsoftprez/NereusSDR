# RX meter stamps: remote meters held to the audio

Status: approved by JJ on 2026-10-22 ("your recommendation approved, this
should likely cover all rx meters, like anything that could drift"). The
minor calls are listed at the end for him to veto.

## The problem

In a remote window the spectrum and waterfall are held to the audio. The
Core stamps each frame with its capture time on the producer clock, and the
window shows the frame when the matching audio plays (RemoteDisplayPresenter,
`due = producerNs + map`, where the map is the window's display delay
follower). The RX meters are not held: each slice reading is applied the
moment its delta arrives. On JJ's Rock Core window (profile radxa_5c_r3)
the audio delay is about 200 to 250 ms. The display map follows it, and the
meters run 100 to 150 ms ahead of what he hears.

## The design

**Core: every RX meter reading carries the time it was measured.**

- `RxChannel::meterReadings()` returns all eight WDSP RX meters from **one**
  receive lane refresh, together with the steady-clock time the lane read
  them. That clock is the same one as `DaemonSpectrumSource::monotonicNowNs`
  and the audio capture clock (displayClockVersion 1). A sequence counter
  keeps the eight values and the time together. Before this, the pump read
  each meter separately, and the lane's refresh, which the first read asks
  for, often landed in the middle. The S-meter then came from one refresh
  and the ADC and AGC readings from the next, 100 ms apart.
- A read that finds the lane mid-write on all eight attempts returns the
  last whole read and logs one warning; it never returns a torn one. No
  test proves the mixed-refresh fix itself (the S-meter from one refresh
  and the ADC and AGC readings from the next): the lane's timing cannot be
  forced in a test. The tests check that the pump's readings and stamp
  match one snapshot.
- `SliceMeterPump::poll()` reads that snapshot and writes each slice's new
  `meterReadingNs` along with its readings. The no-reading clears (link not
  Connected, no channel, the cache not ready yet) are stamped with the
  pump's own time on the same clock. While transmitting, the pump writes
  nothing, as before.

**Wire: `meterReadingNs`, only for a peer that declared `rxMeterStamps` 1.**

- `SliceModel.meterReadingNs` is an `i64`, outbound with no WRITE, declared
  last, so every earlier ordinal keeps its place. It is a peer-only property
  (`fitPeerOnlyProperties`, `MirrorPolicy` feature gate) under the new hello
  feature `rxMeterStamps` 1. The Core advertises `rxMeterStampsVersion` 1
  after `waveformVersion` and before `coreBuildInfo`, only to that peer.
- The delta flush re-reads the live model, and the pump writes the stamp
  and the readings in the same tick on the main thread. A slice delta that
  carries meter readings therefore carries the stamp of the refresh they
  came from.
- Older peers: a window or phone that does not declare the feature gets the
  same schema, snapshot and deltas as today and shows readings on arrival.
  A new window on an older Core gets no stamps and shows readings on
  arrival.

**Window: hold the readings until their audio plays.**

- `StationClient::handleDelta` takes the RX meter readings out of a slice
  delta that carries `meterReadingNs` and queues them. Other properties in
  the same delta apply at once, as before. The held readings are:
  `signalStrengthDbm`, `signalPeakDbm`, `signalAverageDbm`, `adcPeakDbfs`,
  `adcAverageDbfs`, `agcGainDb`, `agcPeakDb` and `agcAverageDb`.
- A queued reading applies at `stamp + map`, the same rule and the same map
  the spectrum uses. StationClient is a core class (rule R1), so the map
  reaches it through a clock hook (`setRxMeterHoldClock`: now and the
  current map, both in the window's audio clock). RemoteMediaController
  installs the hook. It reads the follower's current map and does not
  update it, so the meters never move the spectrum's delay.
- A reading applies at once when there is no hook (a headless client, a
  test), when the map is not known yet, while no pan or mini display is
  open (the map is measured only as display frames are presented, so with
  every display closed it goes stale), when it is already due, or when it
  would wait more than 1.5 s (the presenter's `kMaxHoldNs`).
- Threads: StationClient is a child of MainWindow and applies deltas on
  the main thread; its hold timer is its own child. The follower's map is
  main-thread state, so the hook reads it there, with no lock or atomic.
- An unstamped reading for a slice that has readings queued goes in behind
  them, so readings never apply out of order. Without anything queued it
  applies at once.
- The queue is cleared when the session ends, and a reading for a slice
  that has gone is dropped.
- The applied readings go through the same apply path as before
  (`applyUpdates`, `SideEffectRule::Delta`). Only their timing changes.

**What does not change.** The local window, the values the meters show,
TX meters, the phone app, and the spectrum presenter and its hand-off.

## Calls settled here, for JJ's veto

1. Its own pull request, not folded into the waterfall row stamps. That
   one has no wire change; this one adds a hello feature, a capability and
   a slice property, and touches different files.
2. The names: hello feature `rxMeterStamps`, capability
   `rxMeterStampsVersion` 1, slice property `meterReadingNs`.
3. The stamp is the moment the receive lane read WDSP's meters, not the
   moment the pump published them. That is the closer time to the audio the
   reading describes. The pump's reads are now one consistent snapshot.
4. The longest hold is 1.5 s, the presenter's cap. A longer wait applies
   the reading at once.
5. With no map yet (no spectrum frame has been presented), or with no pan
   or mini display open, readings apply on arrival, as today. The map is
   measured only as display frames are presented, so with every display
   closed it may be stale. (Revised on the waterfall stutter session's
   review: the first draft kept using the last map with no pan showing.)
6. In Max Bin, `signalStrengthDbm` comes from the Core's spectrum, not WDSP.
   It is held with the same stamp as the other readings.
7. The station noise floor (`stationAutoAgcNoiseFloor*`) is not a meter and
   is not held.
