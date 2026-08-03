# Bottom Banner Cleanup + Pan Menu — Bench Verification Matrix

> **Status:** Matrix drafted. The seven rows below are transcribed
> verbatim from design doc §9.2; automated coverage (unit tests over
> `ChromeFoldPlan` / `ChromeBarController` / `PanLayoutDialog` /
> `PanadapterStack`) is in place and green, but the rows themselves need
> a live G2, a live HL2, and (row 6) a 2-slice SKU on the bench. Pending
> that hardware pass.

Design: [2026-08-02-bottom-banner-and-pan-menu-design.md](../2026-08-02-bottom-banner-and-pan-menu-design.md) §9.2
Plan: [2026-08-02-bottom-banner-and-pan-menu-plan.md](../2026-08-02-bottom-banner-and-pan-menu-plan.md)

## How to use

1. Build and launch: `cmake --build build --target NereusSDR && open build/NereusSDR.app`
   (or the platform equivalent).
2. Work through the rows in order; each names the radio(s) it needs.
3. Record PASS / FAIL plus a one-line note per row. Screenshots are
   welcome but not required — most of these are layout/timing behaviors
   better shown with a short screen recording than a still.
4. Reference this matrix from the PR description once all rows the
   available bench can reach are checked off.

---

## Matrix (7 rows, design doc §9.2)

| # | Radio(s) | Scenario | Steps | Expected result |
|---|---|---|---|---|
| 1 | G2 | Maximised, nothing folded | Connect, maximise the window at 1512 logical points (the reference Retina width from design §3.1) | Nothing folded, no overlap anywhere on the strip |
| 2 | G2 | Narrow-drag fold order | Drag the window narrower slowly, from maximised down toward the 746 px floor (design §6) | Rungs fire in the designed order (system tile, TGXL, CAT+TCI, chain tags, RX pills one at a time, then placeholders), no flicker at any boundary, `OverflowChip` lists exactly what went |
| 3 | G2 or HL2 | Alarm invariance under TX | Trigger an ADC overload while transmitting | The TX badge does not move; the ADC OVL badge lights inside its own reserved 50 px slot |
| 4 | HL2 | HL2 system tile + unit toggle | Connect an HL2, observe the system tile; click the temperature row | System tile shows `PA T`, not `PA` (HL2 has no PA-volts telemetry); °C / °F toggle still works and reformats live |
| 5 | G2 (or any multi-slice board) | Multi-pan dashboard follows the active slice | Open a second pan, add Slice B, activate it | The RX pills follow Slice B's state and the slice tag reads `B`, not `A` |
| 6 | G2, HL2, and a 2-slice SKU if one is on the bench | Layout grid gating | Open the pan layout dialog (`+PAN` icon or View > Pan Layout…, Ctrl+L) on each board | Hidden-tile count and footer text match each board's DDC-derived ceiling (`qMin(maxSlices, userDdcCount)`, finding 2 of the final fix wave) — HL2 (5 slices, 2 DDCs) must show only the 1 and 2-pan layouts, not all nine |
| 7 | G2 (needs 5 independent DDCs for `3h2`) | New layouts allocate real DDCs | Apply each of the four new layouts (`2h1`, `3v`, `4v`, `3h2`) in turn | Slices land on every pan in each layout and DDCs allocate correctly (no coupled/shared-window pans where an independent one was expected) |

---

## Known deferrals (no live bench available)

- Rows 1, 2, 3, 5, 7 need a live G2.
- Row 4 needs a live HL2.
- Row 6's 2-slice-SKU leg (HermesII) is optional — HL2 already exercises
  the interesting case (`maxSlices != userDdcCount`) on hardware that is
  likely to be on the bench regardless.
- Everything else in the epic (fold math, controller wiring, dialog
  gating logic, per-pan menu routing, layout template geometry) has
  automated coverage under `tests/tst_chrome_fold_plan.cpp`,
  `tests/tst_chrome_bar_controller.cpp`, `tests/tst_chrome_bar_items.cpp`,
  `tests/tst_pan_layout_dialog_gating.cpp`, `tests/tst_pan_menu_routing.cpp`,
  `tests/tst_panadapter_stack_layouts.cpp`, and siblings — see
  `ctest --test-dir build -R "chrome|pan_layout|pan_menu|panadapter_stack"`.
