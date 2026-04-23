# Contributing to NereusSDR

Thanks for your interest in NereusSDR! We're building a native, cross-platform
SDR console for OpenHPSDR radios. Community contributions are welcome.

---

## Quick Start

1. Browse [open issues](https://github.com/boydsoftprez/NereusSDR/issues) —
   issues labeled `good first issue` are great starting points.
2. Fork the repo and create a feature branch from `main`.
3. Implement the fix or feature (one issue per PR).
4. Open a pull request referencing the issue number (`Fixes #42`).

---

## Reporting Bugs

- Open a [GitHub issue](https://github.com/boydsoftprez/NereusSDR/issues/new) directly.
- Include: OS, NereusSDR version, radio model, protocol version (P1 or P2), firmware version.
- Attach logs (`~/.config/NereusSDR/nereussdr.log`) if available.
- Check existing issues first to avoid duplicates.

## Suggesting Features

- Open a GitHub issue describing the problem you're solving, not just the solution.
- Reference Thetis behavior where applicable — screenshots help.
- One feature per issue.

---

## Submitting Code

**Development tool:** NereusSDR is developed using [Claude Code](https://claude.com/claude-code)
as the primary development environment. We **strongly encourage all contributors to use
Claude Code** — it has full codebase context via `CLAUDE.md` and naturally produces code
that matches our conventions.

1. **Fork the repo** and create a feature branch from `main`.
2. **One issue per PR.** Keep changes focused and reviewable.
3. **Follow the coding conventions** below.
4. **Test your changes** against a real OpenHPSDR radio if possible.
5. **Sign your commits** with GPG when contributing back to this repository's `main` branch (required by this repo's branch protection — not a license obligation; downstream forks and redistributions are not required to sign).
6. **Open a pull request** against `main` with a clear description.

---

## Project Architecture

The full architecture is documented in [CLAUDE.md](CLAUDE.md) including
the data pipeline, thread architecture, protocol specification, and
implementation patterns. Read it before making changes.

### Key Patterns

- **Model → Radio**: Model setters emit signals → `RadioConnection` sends
  protocol commands via UDP (both P1 and P2).
- **Radio → Model**: Protocol responses → `RadioModel` routes to model's
  `applyStatus()`.
- **Model → GUI**: Models emit signals → GUI widgets update via slots.
- **GUI → Model**: GUI widgets call model setters. Use `QSignalBlocker` or
  `m_updatingFromModel` guards to prevent echo loops.
- **Settings**: Use `AppSettings`, **never** `QSettings`. Keys are PascalCase.
  Booleans are `"True"` / `"False"` strings.
- **DSP**: All signal processing is client-side via WDSP. Each receiver owns
  its own WDSP channel with independent DSP state.

### Thread Architecture

| Thread | Components |
|--------|-----------|
| **Main** | GUI rendering, RadioModel, all sub-models, user input |
| **Connection** | RadioConnection (UDP I/O, protocol framing) |
| **Audio** | AudioEngine + WdspEngine (I/Q processing, DSP, audio output) |
| **Spectrum** | FFT computation, waterfall data generation |

Cross-thread communication uses auto-queued signals exclusively.

---

## OpenHPSDR Protocol Reference

Two protocol versions are supported:

- **Protocol 1:** UDP-only on port 1024. 1032-byte Metis frames with C&C control
  bytes and 24-bit I/Q samples. Used by Metis, Hermes, Angelia, Orion boards.
- **Protocol 2:** UDP-only on multiple dedicated ports. Structured command
  packets to ports 1024-1027, per-DDC I/Q data on ports 1035-1041.
  Used by Orion MkII, Saturn (ANAN-G2).

See `docs/protocols/` for detailed protocol documentation.

---

## Coding Conventions

### C++ Style

- **C++20 / Qt6** — modern idioms (`std::ranges`, `auto`, structured bindings).
- **RAII everywhere.** No naked `new`/`delete`. Use Qt parent-child ownership.
- **Qt signals/slots** for cross-object communication.
- **`QSignalBlocker`** to prevent feedback loops.
- **Keep classes small** and single-responsibility.

### Naming

- Classes: `PascalCase` (`SliceModel`, `SpectrumWidget`)
- Methods: `camelCase` (`setFrequency()`, `applyStatus()`)
- Members: `m_camelCase` (`m_frequency`, `m_sliceId`)
- Signals: past tense (`frequencyChanged`, `commandReady`)
- AppSettings keys: `PascalCase` (`LastConnectedRadioMac`)

### Widget Guidelines

- All GUI follows the dark theme: `#0f0f1a` background, `#c8d8e8` text,
  `#00b4d8` accent, `#203040` borders.
- In scrollable areas, install an event filter on `QSlider` and `QComboBox` to
  block wheel events from leaking to parent widgets. Dedicated `GuardedSlider`
  and `GuardedComboBox` convenience wrappers will be added in a future phase.
- Disable `autoDefault` on QPushButtons inside QDialogs.

### Optional Dependencies

Features gated behind compile-time flags:

| Flag | Package | Feature |
|------|---------|---------|
| `HAVE_WDSP` | WDSP library | Full DSP engine (demod, AGC, NR, NB, ANF, PureSignal) |
| `HAVE_FFTW3` | libfftw3 | Client-side FFT for spectrum/waterfall |
| `HAVE_SERIALPORT` | `Qt6::SerialPort` | Serial PTT/CW keying |
| `HAVE_WEBSOCKETS` | `Qt6::WebSockets` | TCI server |
| `HAVE_DFNR` | Rust toolchain + DeepFilterNet3 (libdf via `setup-deepfilter.{sh,ps1}`) | DFNR DeepFilterNet3 noise reduction (Sub-epic C-1) |

**DFNR — DeepFilterNet3:** building the DFNR runtime requires Rust.
Install via `https://rustup.rs` (or your platform package manager),
then run `./setup-deepfilter.sh` (POSIX) or
`./setup-deepfilter.ps1` (Windows) from the repo root before
`cmake -B build`. The script downloads a pre-built libdf if
available, or builds from source via `cargo cbuild` if not. If you
skip this step, `ENABLE_DFNR` auto-falls-back to OFF and the build
proceeds without DFNR.

Use `#ifdef HAVE_*` guards. Features must degrade gracefully when unavailable.

### Commit Messages

- Imperative mood: "Add band stacking" not "Added band stacking".
- First line under 72 characters.
- Reference issues: `Fixes #42` or `Closes #42`.

### Commit Signing

All commits merged to **this repository's** `main` branch must be GPG-signed (this is a project-policy branch-protection rule for this upstream repo only — it is not a GPL contribution prerequisite, and forks/downstream redistributions are not required to sign). Setup:

```bash
# Generate key
gpg --quick-gen-key "Your Name <email@example.com>" ed25519 sign 0

# Add to GitHub: Settings → SSH and GPG keys → New GPG key
gpg --armor --export <KEY_ID>

# Configure git
git config --global user.signingkey <KEY_ID>
git config --global commit.gpgsign true
```

---

## What We Will Not Accept

- **Wine/Crossover workarounds.** The goal is fully native cross-platform.
- **Copied proprietary code.** Clean-room implementations from observed
  protocol behavior and public specifications are fine.
- **Changes that break the core RX path.** Test: discovery → connect →
  I/Q reception → WDSP processing → audio output.
- **Large reformatting PRs.** Fix style only in files you're modifying.

---

## Notes for AI Agents

Read [CLAUDE.md](CLAUDE.md) first — it is the authoritative project context.

### Quick reference

| Task | Start here |
|------|-----------|
| New slice property | `SliceModel.h/.cpp` — getter/setter/signal, parse in `applyStatus()` |
| New TX property | `TransmitModel.h/.cpp` — same pattern |
| New DSP parameter | `WdspEngine.h/.cpp` — add setter, wire to WDSP API |
| New GUI control | Follow existing applet patterns |
| Protocol command | Check OpenHPSDR protocol spec, test with radio |

### AI-to-AI coordination

If your AI agent hits an issue requiring maintainer coordination, open a
GitHub issue with: your analysis, relevant log output, code references,
and proposed fix.

---

## License Preservation on Derived Code

Any PR that ports, translates, or materially adapts Thetis source code
into NereusSDR must preserve the original license header, copyright
lines, and dual-licensing notices from the Thetis source file, and append
a dated modification note to the NereusSDR file. See
[docs/attribution/HOW-TO-PORT.md](docs/attribution/HOW-TO-PORT.md)
for templates and [docs/attribution/THETIS-PROVENANCE.md](docs/attribution/THETIS-PROVENANCE.md)
for the existing provenance inventory.

Pulling a newer Thetis / mi0bot / AetherSDR / WDSP revision into
NereusSDR? Follow
[docs/attribution/UPSTREAM-SYNC-PROTOCOL.md](docs/attribution/UPSTREAM-SYNC-PROTOCOL.md)
— it covers version-stamp refresh, cite re-verification, and how to
log the sync in `REMEDIATION-LOG.md`.

This is a merge-blocking requirement, not a style preference.

### Install the attribution pre-commit hook

After cloning, run once:

```bash
bash scripts/install-hooks.sh
```

This points `git config core.hooksPath` at `scripts/git-hooks/` so the
attribution verifiers (`verify-thetis-headers.py` + `check-new-ports.py`)
run before every commit. Same scripts CI runs, but locally — catches
missing headers and unregistered ports before push, not after a 7-minute
CI cycle. Do not bypass the hook with `--no-verify`; fix the attribution
instead.

## Code of Conduct

Be respectful, constructive, and patient. Ham radio has a long tradition
of helping each other learn — bring that spirit here.

73 de KG4VCF

## License

By contributing to NereusSDR, you agree that your contributions will be
licensed under the [GNU General Public License v3.0](LICENSE).
