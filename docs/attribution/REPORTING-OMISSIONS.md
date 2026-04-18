# Reporting a Missed Attribution

If you believe NereusSDR has failed to credit a contributor in a file
derived from your work (or from work you care about — Thetis,
mi0bot/Thetis-HL2, WDSP, AetherSDR, PowerSDR, or any other upstream),
this document explains how to tell us and what happens next.

This process applies whether you are:
- The missed contributor yourself
- A maintainer of an upstream project (Thetis, mi0bot, etc.)
- A downstream user, researcher, or compliance reviewer
- Another NereusSDR contributor who noticed the gap

All reports are taken seriously. Omissions are cured, not argued.

---

## How to report

**Preferred: open a GitHub issue** on this repository with the label
`compliance`. Use the title pattern:

```
Missed attribution: <contributor name or callsign> in <file path>
```

Helpful body contents (all optional — a one-line notice is enough if
that's all you have time for):

1. **File(s)** — which NereusSDR file(s) are under-attributed. Repo-
   relative paths are best.
2. **Contributor** — the name, callsign, or other identifier of the
   party whose attribution is missing.
3. **Upstream source evidence** — which upstream source file contains
   the attribution that should have propagated. A link or path is
   enough; we will verify from the source.
4. **Suggested correction** — if you have a preferred wording, share
   it. Otherwise we will use the form that appears in the upstream
   source.
5. **Your role** — if you are the missed contributor, please say so
   (no need for legal identity, just enough that we know whose voice
   we're hearing).

**Private alternative:** if you'd rather not file a public issue,
email the NereusSDR maintainer at `jj@skyrunner.net` with the same
information. A private report receives the same process; public
documentation follows the cure (see below), not the report itself.

**Language:** plain technical description is ideal. Formal legal
notices are accepted but not required — we prefer to resolve
omissions before either party needs counsel.

---

## What happens after you report

### Within 72 hours
- The report is acknowledged (issue comment or email reply).
- The maintainer reads the upstream source to verify the claim.
- If the claim is straightforwardly correct, a fix branch is cut.

### Within 7 days (typical)
- Correction lands on `main` as a GPG-signed commit:
  `fix(compliance): restore missed attribution for <name> in <file>`
- The commit body cites the upstream source evidence and includes a
  root-cause note (why the first-pass audit missed it).
- An entry is appended to
  [`docs/attribution/REMEDIATION-LOG.md`](REMEDIATION-LOG.md).
- The GitHub issue is closed with a link to the fix commit.
- If you reported privately, you receive a reply with the commit SHA
  and a short thank-you note.

### Complications that can extend the timeline
- The upstream source isn't directly accessible (private fork, removed
  file) — we coordinate with the upstream maintainer.
- Multiple contributors are affected and the correct form needs
  discussion — we loop the affected parties into the issue.
- The omission is entangled with a pending release — we still cure
  but may need to sequence the fix alongside release timing.

If any of these apply, we will tell you within the initial 72-hour
acknowledgment and keep you updated at least weekly.

---

## What we will *not* do

- Argue that an omission is "small enough not to matter." Omissions are
  cured. Size is irrelevant.
- Delay a fix while waiting for formal escalation. If the claim is
  evidence-backed, we cure first and discuss later.
- Quietly fix and hope nobody notices. Every cure is publicly logged
  in `REMEDIATION-LOG.md`.
- Strip your attribution if you ask us to. Contributors can request
  that their credit be adjusted (different name form, added email, year
  range correction) but removal of attribution is not a cure we offer —
  GPL-preserved notices stay preserved.

---

## If you want your contribution acknowledged in more ways

- **Author-page listing** — we are happy to add contributors to an
  `AUTHORS.md` or About-dialog listing if you ask.
- **Year-range adjustments** — if our conservative year range for your
  work is wrong, tell us the correct span.
- **Name form** — if you'd prefer a specific name/callsign pairing
  (with or without email), we will use it.

---

## Our commitment

NereusSDR descends from Thetis, mi0bot/Thetis-HL2, WDSP, AetherSDR, and
the broader OpenHPSDR / FlexRadio PowerSDR lineage. Those contributor
chains are the foundation of the project. Preserving them accurately —
and fixing mistakes when we find them — is not a burden; it is the
project's baseline posture.

Thank you for helping us keep the record straight.

— J.J. Boyd (KG4VCF), NereusSDR maintainer
