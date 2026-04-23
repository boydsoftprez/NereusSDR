# LGPL-2.1 Compliance Notice (NereusSDR)

NereusSDR statically links one library licensed under LGPL-2.1:
**libspecbleach** (see `third_party/libspecbleach/`, used by the NR4
(SBNR) noise-reduction filter via `third_party/wdsp/src/sbnr.c`).

LGPL-2.1 Section 6 permits static linking with a GPL application on the
condition that we provide users with the means to relink NereusSDR
against a modified version of the LGPL library. NereusSDR satisfies
this via:

1. **Source pointer.** `third_party/libspecbleach/README.md` documents
   the pinned upstream commit and repo URL. The source is always
   retrievable from https://github.com/lucianodato/libspecbleach at
   the pinned SHA `d4cb7afff2c954b7938bd1e9bc77d81e788d08f0`.

2. **Rebuild pathway.** Users may modify `third_party/libspecbleach/
   CMakeLists.txt` to point `GIT_TAG` at a modified fork or local
   copy, then rebuild NereusSDR. The build produces `specbleach_static`
   which transitively links into `NereusSDR` — no prebuilt-library
   replacement required. Full build instructions are in `README.md` at
   the repository root.

3. **Source availability.** NereusSDR release artifacts (AppImage, DMG,
   NSIS installer) include this notice. The full source of
   libspecbleach at the pinned commit is available from the upstream
   repo at the SHA documented in `third_party/libspecbleach/README.md`.

No other dependencies in NereusSDR carry LGPL obligations at this time.

## Library details

| Field | Value |
|-------|-------|
| Library | libspecbleach |
| Author | Luciano Dato |
| License | LGPL-2.1 |
| Upstream | https://github.com/lucianodato/libspecbleach |
| Pinned SHA | `d4cb7afff2c954b7938bd1e9bc77d81e788d08f0` |
| NereusSDR usage | NR4 (SBNR) noise reduction — `third_party/wdsp/src/sbnr.c` |
| In-tree path | `third_party/libspecbleach/` |

## Change log

| Date | Change |
|------|--------|
| 2026-04-23 | Created — added libspecbleach (Task 6, Phase 3G-RX Epic C-1) |
