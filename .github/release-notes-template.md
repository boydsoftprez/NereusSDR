---

## Installation

### Linux

**x86_64:** download `NereusSDR-vX.Y.Z-x86_64.AppImage`
**aarch64:** download `NereusSDR-vX.Y.Z-aarch64.AppImage`

```bash
chmod +x NereusSDR-vX.Y.Z-*.AppImage
./NereusSDR-vX.Y.Z-*.AppImage
```

### macOS

**Apple Silicon (M1/M2/M3/M4):** `NereusSDR-vX.Y.Z-macOS-apple-silicon.dmg`

Intel Macs are not currently built on CI (GitHub Actions has retired the
`macos-13` runner label). Intel Mac users should build from source for now.

This build is **ad-hoc signed** (no Apple Developer ID yet). On first launch:
1. Open the DMG, drag NereusSDR to Applications.
2. **Right-click** NereusSDR.app → **Open** → **Open** in the dialog.
3. After the first launch, double-clicking works normally.

### Windows

Two options — pick one:

- **Installer (recommended):** `NereusSDR-vX.Y.Z-Windows-x64-setup.exe` — installs
  to `Program Files`, adds Start Menu shortcut, registers an uninstaller.
- **Portable:** `NereusSDR-vX.Y.Z-Windows-x64-portable.zip` — extract and run
  `NereusSDR.exe`. No install footprint.

Both are unsigned at the moment. SmartScreen may flag the binary on first run
(no Authenticode signature yet); click **More info → Run anyway**.

## Verification

All artifacts are GPG-signed by `KG4VCF`. To verify:

```bash
gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
```

## Reporting Issues

This is an alpha build for debuggers/testers. Please report issues at
<https://github.com/boydsoftprez/NereusSDR/issues> with: OS, radio model,
protocol version, and log file (`~/.config/NereusSDR/nereussdr.log`).
