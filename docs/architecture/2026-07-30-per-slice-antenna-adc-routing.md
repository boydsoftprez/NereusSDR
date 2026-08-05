# Per-slice antenna versus per-slice ADC routing

Found on a live ANAN-G2 (Saturn, fw 27) on 2026-07-30 by KG4VCF, on branch
`feature/phase3f-sub-epic-a-foundation` at `daec8f8b`.

This is the design tension recorded as item 5 of
`2026-07-28-phase3f-session-state.md` ("the antenna is per-band-global in
AlexController while ADC routing is per-slice in the codec, so the two models
do not fully compose on a multi-slice radio. It works for the current operator
flow"). It does not work. This document is the diagnosis.

---

## 1. Symptom

Two pans, two slices, both on 80m, both on the same dial frequency
(3.916.400). Slice B's RX antenna is set to EXT1.

Expected: slice B moves to the second filter chain, its pan reads `CH 1`, and
the bottom bar reads `CH 0: 80m` and `CH 1: 80m`.

Observed: both pans read `CH 0` and the bottom bar reads `CH 1 ... 80m (idle)`.
The flag keeps showing EXT1, so the model holds the operator's intent, but no
routing follows it.

The `CH` pill is honest. The routing genuinely is not moving.

---

## 2. Evidence

From the application log, at the moment EXT1 was selected:

```
11:08:48.290  applyAlexAntennaForBand("80m" isTx=false) → rxOnly=2 trxAnt=2 txAnt=1 rxOut=true
11:08:48.290  P2::setAntennaRouting rxAnt=2 txAnt=1 rxOnlyAnt=2 rxOut=true running=true
11:08:48.290  P2: applyDdcAssignment — ddcEnable=4 rate[2]=48000 nDdc=1
11:08:48.290  P2::setAlexRxBpf adc0=-1 adc1=8 running=true
```

The selection is applied. Note `adc0=-1`: chain 0 is left with no slice at
all, and chain 1 takes the 80m filter. Both slices moved, not one.

Then, 2.6 seconds later, with no operator action in between:

```
11:08:50.881  applyAlexAntennaForBand("80m" isTx=false) → rxOnly=2 trxAnt=1 txAnt=1 rxOut=true
11:08:50.881  P2::setAntennaRouting rxAnt=1 txAnt=1 rxOnlyAnt=2 rxOut=true running=true
11:08:50.881  applyAlexAntennaForBand("80m" isTx=false) → rxOnly=0 trxAnt=1 txAnt=1 rxOut=false
11:08:50.881  P2::setAntennaRouting rxAnt=1 txAnt=1 rxOnlyAnt=0 rxOut=false running=true
11:08:50.882  P2::setAlexRxBpf adc0=8 adc1=-1 running=true
```

Two applies land in the same millisecond and the second one clears
`rxOnlyAnt` to 0, putting everything back on chain 0. `nDdc=1` throughout.

---

## 3. Root cause 1: the antenna is stored per band, not per slice

`RadioModel::applyAlexAntennaForBand(Band band, bool isTx)`
(`RadioModel.cpp:9777`) resolves the whole routing from the band alone:

```cpp
const int txAnt   = m_alexController.txAnt(band);
      rxOnlyAnt   = m_alexController.rxOnlyAnt(band);
```

There is no slice parameter in that function, and none in the
`AlexController` accessors it calls. Two slices on 80m therefore share one
`rxOnlyAnt(80m)` value. There is no such thing as "slice B's antenna" for the
router to act on; there is only "80m's antenna".

The revert follows directly from that, via the per-slice handler at
`RadioModel.cpp:9509`:

```cpp
if (ant.startsWith("ANT")) {
    m_alexController.setRxAnt(band, antNum);
    m_alexController.setRxOnlyAnt(band, 0);  // issue #257 — release bypass mux
}
```

Any slice sitting on ANT1/2/3 clears the rx-only mux **for the entire band**.
Slice A is on ANT1, so whenever its antenna handler re-fires it wipes slice
B's EXT1. That clear is correct for a single-slice radio, where the antenna
popup is one mutually-exclusive selection and picking ANT1 must release a
prior EXT1. It is wrong once a band can host more than one slice.

---

## 4. Root cause 2: stream allocation ignores the antenna

`SliceStreamAllocator` places slices on streams by frequency and nothing
else (`SliceStreamAllocator.h:63`, `:87`):

```cpp
Placement placeSlice(double frequencyHz) const;
Placement retuneSlice(int currentStream, bool soleOccupant,
                      bool ddcPinned, double frequencyHz) const;
```

Slices whose frequencies fall in one window co-host on one stream, and one
stream is one DDC. A DDC has exactly one ADC. So two co-hosted slices
**cannot** sit on different ADCs, whatever the antenna model says.

That is why `nDdc=1` is not itself a bug here: with both slices on the same
frequency, co-hosting them on one DDC is the allocator working as designed.
The defect is that the antenna is an ADC-selecting input which the allocator
never sees, so it will happily co-host two slices whose antennas have just
made co-hosting impossible.

---

## 5. Why the bench matrix did not catch this

Row 15 of `2026-05-26-phase3f-verification/g2-results.md` sets up two slices
on **different** bands (40m and 20m) before selecting RX2 / EXT1. On different
bands, the per-band antenna store gives each slice its own slot, and the
different frequencies put them on separate streams. Both defects are hidden by
the same coincidence.

Same band is the case that breaks it, and no row covers it.

---

## 6. What a fix has to satisfy

Both defects have to move for the operator's expectation to hold. Fixing
either alone leaves the symptom:

- Per-slice antenna without allocator awareness: slice B holds EXT1, the
  allocator still co-hosts it with slice A, and one DDC still cannot serve
  two ADCs.
- Allocator awareness without per-slice antenna: there is no per-slice
  antenna for the allocator to group on.

Constraints any design must respect:

1. **One ADC per DDC.** Slices needing different ADCs must occupy different
   streams. This is a hardware fact, not a policy choice.
2. **ADC count is not filter-chain count.** ANAN-100D and 200D report two
   ADCs and are absent from Thetis's `setAlex2HPF` model list. Use
   `rxFilterChainCount`, not `adcCount` and not `hasAlex2`.
3. **Single-ADC SKUs.** HL2 and Atlas have one ADC and hide antenna UI
   entirely (`!caps.hasAlex || antennaInputCount < 3`). The per-slice model
   must degrade to today's behaviour there, not merely be hidden.
4. **DDC exhaustion.** Splitting co-hosted slices consumes a DDC that may not
   be free. The operator needs to be told, not silently ignored.
5. **Persistence.** `AlexController` state is persisted per band per MAC.
   Moving to per-slice changes the key shape and needs a migration, the same
   shape the NB per-slice work used at `994177c2`.
6. **Thetis has no answer here.** Thetis is a two-receiver console whose
   antenna is per band by construction (`Alex.cs:310-413`). Beyond the
   existing port there is no upstream to follow, so this is NereusSDR-original
   design and must be marked as such rather than cited.

---

## 7. Status

Diagnosis complete and verified in code. No fix implemented yet; the design
choices in section 6 need a decision first, and `CLAUDE.md` places
architecture changes outside what an agent settles alone.
