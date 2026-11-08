# hidapi Provenance - NereusSDR vendored-library inventory

This document catalogs hidapi as vendored into NereusSDR under
`third_party/hidapi/`. The desktop app reads USB tuning knobs and controllers
through it (`src/gui/hotkeys/devices/DeviceInput.cpp`,
`src/gui/hotkeys/devices/UsbPresence.cpp`). The platform's C file is compiled
straight into `NereusSDRLib` and its `NereusGui_LTO` twin, so there is no DLL and
no package step. It is never in `NereusCore` or `nereusd` (R-HK-18,
`tests/tst_core_has_no_gui_includes.cpp`, and a check in `release.yml`).

hidapi offers the GPLv3, a BSD-style license and its original license, at the
user's choice. NereusSDR uses the BSD-style license, which is compatible with its
GPLv3.

## Upstream

- **Project:** hidapi (libusb/hidapi team, originally Alan Ott, Signal 11 Software)
- **Repository:** https://github.com/libusb/hidapi
- **Tag:** `hidapi-0.15.0`
- **Commit:** `d6b2a974608dec3b76fb1e36c189f22b9cf3650c`
- **Archive:** https://codeload.github.com/libusb/hidapi/tar.gz/refs/tags/hidapi-0.15.0
- **Archive SHA-256:** `5d84dec684c27b97b921d2f3b73218cb773cf4ea915caee317ac8fc73cef8136`
- **Vendored:** 2026-11-06 (hot keys plan, Task 12)

The per-file hashes are in `third_party/hidapi/VERSION.txt`.

## License

The upstream `LICENSE-bsd.txt` is kept unchanged at
`third_party/hidapi/LICENSE-bsd.txt` and reproduced in
`packaging/third-party-licenses/hidapi.txt`.

## Files vendored

`hidapi/hidapi.h`, `mac/hid.c`, `mac/hidapi_darwin.h`, `linux/hid.c`, `windows/hid.c`,
the Windows headers it includes, `windows/hidapi_descriptor_reconstruct.c` (which
`windows/hid.c` includes at its line 1600), and `LICENSE-bsd.txt`. Left out: the libusb,
NetBSD and Android back ends, tests, documentation and upstream's build files.

## Local change

None.
