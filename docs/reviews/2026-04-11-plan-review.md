# NereusSDR Master Plan Review

**Date:** 2026-04-11
**Reviewer:** Claude Code (automated review)
**Scope:** Full project plan, architecture docs, implementation plans, codebase audit
**Branch:** `claude/review-nereus-sdr-plan-uYj6c`

---

## Executive Summary

NereusSDR is in a strong position. The project has completed 10 phases (0, 1, 2,
3A-3E, 3G-1, 3G-2) with Phase 3G-3 in progress. The codebase is clean (61
source files, 15,601 LOC, only 1 TODO), CI is green on 3 platforms, and the core
RX path (radio -> I/Q -> WDSP -> audio + GPU spectrum) is verified working on
real hardware (ANAN-G2).

This review identifies **10 risks**, **7 documentation gaps**, and **5 actionable
recommendations** for plan improvement.

---

## 1. Project Health Dashboard

| Metric | Value | Assessment |
|--------|-------|------------|
| Source files | 61 (.h + .cpp) | Healthy |
| Total LOC | 15,601 | Moderate, well-structured |
| TODO/FIXME/HACK count | 1 | Excellent |
| CI platforms | 3 (Ubuntu, Windows, macOS) | Good |
| Phases complete | 10 of 23 | ~43% |
| Hardware verified | ANAN-G2 (Orion MkII, FW 27) | P2 only |
| Current phase | 3G-3 (Core Meter Groups) | In progress |

---

## 2. Phase Progress Audit

### Completed Phases (Verified)

| Phase | Name | Key Deliverable | Status |
|-------|------|-----------------|--------|
| 0 | Scaffolding | 69 files, CI green | Done |
| 1 | Analysis | 3 analysis docs (AetherSDR, Thetis, WDSP) | Done |
| 2 | Architecture Design | 6 architecture docs | Done |
| 3A | Radio Connection (P2) | P2RadioConnection, UDP discovery | Done |
| 3B | WDSP Integration | RxChannel, AudioEngine, wisdom caching | Done |
| 3C | macOS Build | Cross-platform WDSP, crash fix | Done |
| 3D | GPU Spectrum & Waterfall | FFTEngine, SpectrumWidget, 6 shaders | Done |
| 3E | VFO + Multi-RX Foundation | VfoWidget, SliceModel, CTUN, Alex filters | Done |
| 3G-1 | Container Infrastructure | ContainerWidget, FloatingContainer, ContainerManager | Done |
| 3G-2 | MeterWidget GPU Renderer | 3-pipeline QRhi renderer, MeterItem hierarchy | Done |

### In Progress

| Phase | Name | Status | Notes |
|-------|------|--------|-------|
| 3G-3 | Core Meter Groups | In progress | NeedleItem S-meter, preset factories, TX meter bindings |

### Pending (13 phases)

| Phase | Name | Dependencies | Risk Level |
|-------|------|--------------|------------|
| 3G-4 | Advanced Meter Items | 3G-3 | Low |
| 3G-5 | Interactive Meter Items | 3G-4 | Low |
| 3G-6 | Container Settings Dialog | 3G-5 | Low |
| **3I-1** | **Basic SSB TX** | 3G-3 | **High** (new subsystem) |
| 3I-2 | CW TX | 3I-1 | Medium |
| **3I-3** | **TX Processing + RX DSP** | 3I-2 | **High** (scope concern) |
| **3I-4** | **PureSignal** | 3I-3, **3F** | **High** (complex, cross-phase dependency) |
| **3F** | **Multi-Panadapter** | 3E | **High** (UpdateDDCs port) |
| 3H | Skins | 3F | Medium |
| 3J | TCI + Spots | - | Low |
| 3K | CAT/rigctld | - | Low |
| **3L** | **Protocol 1** | 3A | **Medium** (Hermes Lite 2 community) |
| 3M | Recording | 3B | Low |
| 3N | Packaging | All | Low |

---

## 3. Risk Register

### Risk 1: S-Meter Technical Debt (MEDIUM)

**Finding:** The current NeedleItem/MeterItem approach for S-meter rendering has
known GPU pipeline artifacts (texture-based text splitting, multi-pipeline
rendering seams). The CHANGELOG explicitly states: "S-meter will be
re-implemented as dedicated SMeterWidget (direct AetherSDR port) for
pixel-identical fidelity."

**Impact:** Current implementation works but is acknowledged as temporary. The
re-implementation is unscheduled.

**Recommendation:** Add SMeterWidget re-implementation as an explicit task in
Phase 3G-3 or create Phase 3G-3b for it. Don't let this become permanent
technical debt.

---

### Risk 2: UpdateDDCs() Not Ported (HIGH)

**Finding:** `adc-ddc-panadapter-mapping.md` Section 10 identifies
`UpdateDDCs()` (Thetis `console.cs:8186-8538`) as the single most important
unported function. Phase 3E is marked complete, but UpdateDDCs() is deferred to
Phase 3F. This function controls:
- DDC-to-ADC assignment (rx_adc_ctrl1/2 bitfields)
- Per-DDC sample rate management
- DDC enable/sync logic
- Diversity mode DDC assignment
- PureSignal DDC reservation

**Impact:** Without UpdateDDCs(), multi-receiver (Phase 3F), diversity RX, and
PureSignal (Phase 3I-4) cannot work. This is the critical path bottleneck.

**Recommendation:** Begin UpdateDDCs() analysis early. Consider starting a
partial port in Phase 3G timeframe to de-risk Phase 3F.

---

### Risk 3: PureSignal Cross-Phase Dependency (HIGH)

**Finding:** Phase 3I-4 (PureSignal) requires DDC0+DDC1 synchronized at 192kHz
for feedback capture. This depends on Phase 3F (UpdateDDCs) being complete. The
dependency is not explicitly documented in MASTER-PLAN.md.

**Impact:** If Phase 3F slips, PureSignal cannot start. PureSignal is a
must-have feature for serious operators with ANAN radios.

**Recommendation:** Add explicit dependency notation: `3I-4 requires 3F`. Create
a dependency DAG in MASTER-PLAN.md.

---

### Risk 4: Phase 3I-3 Scope Creep (MEDIUM)

**Finding:** Phase 3I-3 is named "TX Processing Chain" but includes both TX
enhancements AND RX DSP additions (SNB, spectral peak hold, histogram, RTTY
decoder). This mixes concerns and increases the phase's blast radius.

**Impact:** Could delay TX shipping if RX additions encounter issues.

**Recommendation:** Split Phase 3I-3 into:
- 3I-3a: TX Processing Chain (18-stage TXA)
- 3I-3b: RX DSP Additions (SNB, peak hold, histogram)

---

### Risk 5: No Release Timeline (MEDIUM)

**Finding:** MASTER-PLAN.md lists 13 pending phases but no target dates, sprint
durations, or milestone definitions. The CHANGELOG uses `[Unreleased]` with no
version target.

**Impact:** Hard to prioritize, hard for contributors to plan around, hard to
communicate project status externally.

**Recommendation:** Define at minimum:
- **Alpha:** TX working (SSB + CW) = post-3I-2
- **Beta:** Multi-receiver + PureSignal = post-3I-4
- **RC:** Protocol 1 + packaging = post-3N
- **1.0:** Feature parity with Thetis core RX/TX

---

### Risk 6: Testing Strategy Gaps (MEDIUM)

**Finding:** CONTRIBUTING.md says "Test your changes against a real OpenHPSDR
radio if possible." No mention of:
- Mock radio / pcap playback for CI
- Integration test fixtures
- Unit test coverage targets
- Regression test suite

**Impact:** Contributors without hardware can't verify changes. CI only validates
compilation, not behavior.

**Recommendation:** Create a test strategy document covering:
- pcap-based protocol replay for CI
- WDSP unit tests with known I/Q test vectors
- GUI smoke tests (if feasible with Qt Test)

---

### Risk 7: Protocol 1 Community Demand (LOW-MEDIUM)

**Finding:** Protocol 1 support (Phase 3L) is planned late in the roadmap, but
Hermes Lite 2 is one of the most popular OpenHPSDR radios (affordable, active
community). P1 is fully documented in `radio-abstraction.md` but not implemented.

**Impact:** Delaying P1 limits the user base to expensive ANAN radios only.

**Recommendation:** Consider promoting Phase 3L earlier (after 3I-2 or 3F) if
community demand warrants it. The architecture already supports P1 via
`RadioConnection` base class.

---

### Risk 8: Theme Hardcoding vs Skin System Conflict (LOW)

**Finding:** CONTRIBUTING.md hardcodes dark theme colors (`#0f0f1a` bg,
`#c8d8e8` text, `#00b4d8` accent). Phase 3H (Skin System) plans flexible
theming. These are inconsistent.

**Impact:** Low for now (Phase 3H is distant), but contributors may embed
hardcoded colors that later need migration.

**Recommendation:** Extract hardcoded colors to a `Theme` namespace or constants
file now. Reference these from CONTRIBUTING.md.

---

### Risk 9: Optional Dependency Degradation Untested (LOW)

**Finding:** CMakeLists.txt defines `HAVE_WDSP`, `HAVE_FFTW3`,
`HAVE_SERIALPORT`, `HAVE_WEBSOCKETS` feature flags. CONTRIBUTING.md requires
graceful degradation. No CI workflow tests builds with these flags disabled.

**Impact:** Degradation paths may be broken without anyone noticing.

**Recommendation:** Add a CI matrix job building with
`-DHAVE_WDSP=OFF -DHAVE_FFTW3=OFF` to verify the stub/fallback paths compile.

---

### Risk 10: Container Serialization Migration (LOW)

**Finding:** Phase 3G-1 introduced 24-field pipe-delimited container
serialization. Phase 3G-3 adds NEEDLE serialization format. No backward
compatibility or migration path is documented.

**Impact:** Low currently (no released version with old format), but will matter
post-1.0.

**Recommendation:** Document the serialization version in the format itself (add
a version field). Plan migration logic for future format changes.

---

## 4. Documentation Gaps

| # | Gap | Location | Severity |
|---|-----|----------|----------|
| 1 | `multi-panadapter.md` says max ~384 kHz per pan, but Phase 3E introduced 768k DDC sample rate doubling this | `docs/architecture/multi-panadapter.md` | Medium |
| 2 | `wdsp-integration.md` had incorrect default values (AGC top, NR rates) — corrected in code but doc note is easy to miss | `docs/architecture/wdsp-integration.md` | Low |
| 3 | MASTER-PLAN.md "Recommended Next Step" says Phase 3G-2, which is already complete | `docs/MASTER-PLAN.md` | Low |
| 4 | No dependency DAG between phases (3F->3I-4 dependency undocumented) | `docs/MASTER-PLAN.md` | Medium |
| 5 | No architecture doc for meter system (Phase 3G docs are in `docs/superpowers/plans/`, not `docs/architecture/`) | Doc organization | Low |
| 6 | `GuardedSlider` and `GuardedComboBox` referenced as "planned" in CONTRIBUTING.md but not in any phase plan | `CONTRIBUTING.md` | Low |
| 7 | No test strategy document exists | Missing file | Medium |

---

## 5. Architecture Assessment

### Strengths

1. **Clean separation of concerns** — core/gui/models directories with clear
   responsibilities. RadioModel as central hub follows AetherSDR pattern well.

2. **Thread architecture is sound** — 4 threads (main, connection, audio,
   spectrum) with auto-queued signals. No mutex in audio callback. Atomic
   parameters for cross-thread DSP.

3. **GPU rendering via QRhi** — Future-proof (Metal/Vulkan/D3D12). 3-pipeline
   meter renderer is well-designed.

4. **WDSP integration is faithful** — Thetis-sourced constants, correct
   fexchange2() usage, proper channel lifecycle.

5. **Protocol abstraction** — RadioConnection base class supports both P1 and
   P2 implementations cleanly.

### Areas for Improvement

1. **SpectrumWidget.cpp at 67.9 KB** — This is the largest file by far. Consider
   extracting overlay rendering, color scheme management, or mouse interaction
   into helper classes before it grows further.

2. **P2RadioConnection.cpp at 30.5 KB** — Large but appropriate for protocol
   complexity. Monitor growth during TX implementation.

3. **No formal model-view binding** — GUI sync uses manual signal/slot wiring
   with `m_updatingFromModel` guards. This works but is error-prone at scale.
   Consider a lightweight property-binding helper if feedback-loop bugs increase.

---

## 6. Critical Path Analysis

The shortest path to a useful release (basic RX + TX on P2):

```
Current ──► 3G-3 ──► 3I-1 ──► 3I-2 ──► 3I-3a ──► Alpha (SSB+CW TX)
                                                        │
                                                        ▼
                                                    3F (Multi-RX)
                                                        │
                                                        ▼
                                                    3I-4 (PureSignal)
                                                        │
                                                        ▼
                                                    Beta (full TX)
                                                        │
                                                        ▼
                                                3L (P1) + 3N (Packaging)
                                                        │
                                                        ▼
                                                      1.0 RC
```

**Key insight:** Phase 3G-4 through 3G-6 (advanced/interactive meters, settings
dialog) are nice-to-have and could be deferred past Alpha to accelerate TX
development. The current S-meter + Power/SWR + ALC meters are sufficient for
basic operation.

---

## 7. Recommendations Summary

| # | Recommendation | Priority | Effort |
|---|----------------|----------|--------|
| 1 | **Schedule SMeterWidget re-implementation** — don't let GPU artifact workaround become permanent | High | Medium |
| 2 | **Add phase dependency DAG** to MASTER-PLAN.md (especially 3F->3I-4) | High | Low |
| 3 | **Split Phase 3I-3** into TX-only (3I-3a) and RX additions (3I-3b) | Medium | Low |
| 4 | **Define release milestones** (Alpha/Beta/RC/1.0) with phase gates | Medium | Low |
| 5 | **Update multi-panadapter.md** for 768k DDC bandwidth | Medium | Low |
| 6 | **Consider promoting Phase 3L** (Protocol 1) if Hermes Lite 2 demand is high | Low | N/A (planning) |
| 7 | **Create test strategy document** with pcap replay and I/Q test vectors | Medium | Medium |

---

## 8. Conclusion

The NereusSDR project plan is **well-structured and executable**. The completed
phases demonstrate consistent delivery with high code quality. The architecture
is sound, following proven AetherSDR patterns with faithful Thetis logic porting.

The primary risks are around **multi-receiver DDC routing** (UpdateDDCs port),
**TX implementation scope**, and the **S-meter re-implementation**. None of these
are blockers for the current phase, but they should be addressed in planning
before Phase 3F begins.

The project would benefit most from:
1. Explicit phase dependencies
2. Release milestone definitions
3. A test strategy for CI without hardware

Overall assessment: **GREEN — on track with manageable risks**.

---

*Review generated from: MASTER-PLAN.md, CONTRIBUTING.md, CHANGELOG.md,
STYLEGUIDE.md, 7 architecture docs, 5 implementation plans, and full source
tree audit (61 files, 15,601 LOC).*
