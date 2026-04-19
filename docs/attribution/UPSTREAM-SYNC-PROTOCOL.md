# Upstream Sync Protocol

How NereusSDR pulls from the upstream Thetis / mi0bot-Thetis-HL2 /
AetherSDR / WDSP repositories, refreshes its inline cite version
stamps, and reconciles drift. Compliance Plan Task 17
(`docs/architecture/2026-04-18-gpl-compliance-full-audit-plan.md`).

The point of this doc is that upstream drift is inevitable — Thetis
ships new releases on its own cadence, WDSP gets point updates, and
AetherSDR occasionally refactors the skeleton we templated off.
Without a defined cadence, our inline cites (`// From Thetis
console.cs:4821 [v2.10.3.13]`) silently rot against whatever we
pulled in 2026-04. This protocol keeps the rot bounded.

---

## When to sync

**Mandatory:**

1. **Before cutting any minor release** (x.Y.0). This is the hard gate;
   never ship a minor without at least checking for upstream drift.
2. **When Thetis ships a new tagged release.** Watch
   <https://github.com/ramdor/Thetis/releases> (or subscribe to the
   release feed); open a sync issue within one week.

**Optional / ad-hoc:**

3. Before starting work on a Phase that touches a heavily-ported
   subsystem (e.g. a new DSP feature → sync Thetis first so the cites
   match current upstream; a new P1 feature → sync mi0bot).
4. When a contributor reports a cite that no longer resolves to the
   line it claims (Thetis re-wrapped the file, line numbers shifted).

---

## Procedure

### 1 — Fetch every upstream

```bash
git -C ../Thetis fetch --tags
git -C ../mi0bot-Thetis fetch --tags   # if you have the fork clone
git -C ../AetherSDR fetch              # AetherSDR doesn't tag; use main
# WDSP is vendored — update via the WDSP import procedure below, not git fetch
```

### 2 — Record the new version stamps

For each upstream, record the current verification stamp:

```bash
git -C ../Thetis describe --tags          # e.g. "v2.10.4.1"
git -C ../Thetis rev-parse --short HEAD   # e.g. "a1b2c3d"
git -C ../mi0bot-Thetis rev-parse --short HEAD
git -C ../AetherSDR rev-parse --short HEAD
```

These are the stamps to use in new `// From Thetis ...` cites for the
rest of the session / PR — either the tag (`[v2.10.4.1]`) or the
short SHA (`[@a1b2c3d]`) per the grammar in
[`HOW-TO-PORT.md`](HOW-TO-PORT.md) §Inline cite versioning.

### 3 — Surface stale cites

```bash
python3 scripts/verify-inline-cites.py --format=json > /tmp/cites.json
```

The result groups findings by kind (`missing-stamp`,
`malformed-stamp`). At this point every cite in the tree already
carries some stamp — the relevant question is now "do those stamps
point at a release the upstream still has?" That's a human judgement:
stamps older than N major releases should probably be refreshed; a
stamp that matches a tag the upstream has since retired should be
re-verified against current HEAD.

### 4 — Re-verify ports against new upstream

Pick the PROVENANCE rows whose cites are >1 minor version old OR whose
upstream lines could plausibly have moved (i.e. rows that cite
`console.cs` / `setup.cs` / any file under active upstream
maintenance). For each:

1. Open the upstream file at the new HEAD and at the old stamp.
2. Diff the cited line range:
   ```bash
   git -C ../Thetis diff <old-stamp>..<new-stamp> -- 'Project Files/Source/Console/console.cs'
   ```
3. If the cited range moved but the logic is unchanged, update the
   cite to the new line number and refresh the stamp.
4. If the logic itself changed, open an issue against NereusSDR and
   decide: port the change, document the intentional divergence in
   the source file's Modification-history block, or both.

### 5 — Refresh WDSP (separate cadence)

WDSP is vendored, not fetched live:

1. Re-run `scripts/audit-wdsp-headers.py` against the current tree
   (drift detection).
2. If `TAPR/OpenHPSDR-wdsp` has a new tag since our last import:
   - Diff the new upstream tree against `third_party/wdsp/src/`.
   - Import unchanged files as-is; for changed files preserve the
     verbatim upstream header.
   - Re-run `scripts/audit-wdsp-headers.py`; if the census counts
     drift, update the EXPECTED dict in the script (in a separate
     commit, with reasoning in the message).
   - Run `scripts/verify-thetis-headers.py --kind=wdsp` to confirm
     the exemption set is still accurate.

### 6 — Log the sync

Append a new dated entry to
[`REMEDIATION-LOG.md`](REMEDIATION-LOG.md) covering:

- Which upstreams were synced (Thetis / mi0bot / AetherSDR / WDSP).
- Old and new stamps.
- How many cites were refreshed, how many files re-verified.
- Any drift documented as intentional divergence.
- Commit SHAs for the sync work.

The log entry closes the loop: future reviewers can see we sync on
a cadence and can reproduce exactly what we compared against.

---

## What "stale" means

| Stamp age | Treatment |
|---|---|
| Same tag | No action |
| Same minor (e.g. v2.10.3.X vs v2.10.3.Y) | Advisory — refresh opportunistically |
| One minor behind (v2.10.3 vs v2.10.4) | Refresh before next NereusSDR minor release |
| Two+ minors behind | Mandatory refresh before any touch on the cited file |
| Upstream retired the tag | Immediate re-verify against current HEAD |

"Refresh" means: open the upstream file at the new revision, confirm
the cited logic is still where the cite says it is, update the line
numbers + stamp if it moved, document divergence if the logic changed.

---

## Non-goals

- This protocol does **not** require mirroring every upstream change.
  NereusSDR is a port, not a downstream fork — we pull upstream
  changes when they affect logic we already ported, and we explicitly
  *skip* upstream changes we don't want (with a documented "divergence"
  note in the source file's Modification history).
- This protocol does **not** require daily fetching. Quarterly-ish +
  release-triggered is enough given Thetis's cadence.

---

## Tooling status

Implemented:

- `scripts/verify-inline-cites.py` (tree-wide stamp presence audit)
- `scripts/verify-thetis-headers.py --all-kinds` (per-kind header markers)
- `scripts/audit-wdsp-headers.py` (WDSP census)
- `scripts/verify-provenance-sync.py` (PROVENANCE-vs-tree orphan check)

Not yet implemented (future work):

- Automatic "which stamps are >N releases old" reporter. Today the
  judgement is manual per step 3 above; a follow-up script could parse
  every `[vX.Y.Z.W]` and compare against a configured "oldest
  acceptable" threshold.
- CI job that runs the sync check on a schedule (cron workflow). Plan
  Task 15 lands the on-every-PR gate; the scheduled variant is a
  follow-on.
