# ASIO SDK Provenance - NereusSDR vendored-library inventory

This document catalogs the Steinberg ASIO SDK vendored under
`third_party/asiosdk/`. NereusSDR's own ASIO engine (Windows only) is
written against the SDK's headers and host helpers; the static library
`asiosdk_host` built from them is linked only into the mic helper
`nereus-audio-capture`.

NereusSDR is distributed under GPLv3 (root `LICENSE`, `LICENSE-NOTICE`).

## Upstream

- **Project:** Steinberg ASIO SDK
- **Publisher:** Steinberg Media Technologies GmbH
- **Version:** 2.3.4
- **Archive:** `ASIO-SDK_2.3.4_2025-10-15.zip`
- **Download URL:** https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip
  (reached from https://www.steinberg.net/asiosdk, which redirects there)
- **Downloaded:** 2026-10-09 (04:14 UTC), with JJ Boyd's approval
- **Archive size:** 8910208 bytes
- **Archive SHA-256:** `d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca`
- **Vendored:** 2026-10-09
- **Language:** C++ (Windows host helpers; the archive also holds a sample
  driver and a sample host, which NereusSDR does not compile)

## License

The SDK's `LICENSE.txt` (also `common/LICENSE.txt`, identical) states a
dual licence: the proprietary Steinberg ASIO License, or alternatively the
GNU General Public License version 3. Its `changes.txt` records that 2.3.4
added the dual licence; earlier releases were under the Steinberg licence
only, which is why the 2026-04-19 compliance review kept ASIO out
(`docs/architecture/2026-04-19-vax-design.md` section 8.5 keeps that
history).

NereusSDR takes the SDK under GPL version 3 (GPL-3.0-only: the SDK's grant
names version 3 and no later version), which matches NereusSDR's own
GPL-3.0 election in `LICENSE-NOTICE`. The verbatim licence ships as
`packaging/third-party-licenses/asiosdk.txt`, with the full GPLv3 text in
`packaging/third-party-licenses/GPLv3.txt`.

`LICENSE.txt` says it applies only to files that refer to it, and that
other SDK files carry their own embedded licence text. The host helper
files NereusSDR compiles (`host/asiodrivers.cpp`, `host/asiodrivers.h`,
`host/ginclude.h`, `host/pc/asiolist.cpp`, `host/pc/asiolist.h`) carry an
embedded BSD-style notice, "(c) 2025, Steinberg Media Technologies GmbH",
with three redistribution conditions. That notice is reproduced in
`packaging/third-party-licenses/asiosdk-notices.txt`, as its binary
redistribution condition requires. `common/asio.cpp`, `common/asio.h`,
`common/asiosys.h` and `common/iasiodrv.h` refer to `LICENSE.txt`.

The SDK's README and `Steinberg ASIO Usage Guidelines.pdf` set the rules
for the ASIO trademark and the ASIO compatible logo. They apply only if
NereusSDR shows the name as a trademark or the logo; the logo artwork in
`Steinberg ASIO Logo Artwork/` is vendored as part of the release and is
not used by NereusSDR.

## Files vendored from the SDK

Every file of the archive's top folder `ASIOSDK/` is copied byte for byte,
the PDFs and logo artwork included, with the `ASIOSDK/` prefix dropped.
Nothing is edited. Line endings are the archive's own (CRLF);
`.gitattributes` marks `third_party/asiosdk/**` as `-text` so no checkout
converts them.

| Archive file | NereusSDR path |
|---|---|
| `ASIOSDK/LICENSE.txt` | `third_party/asiosdk/LICENSE.txt` |
| `ASIOSDK/README.md` | `third_party/asiosdk/README.md` |
| `ASIOSDK/Steinberg ASIO Licensing Agreement.pdf` | `third_party/asiosdk/Steinberg ASIO Licensing Agreement.pdf` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.eps` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.eps` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.jpg` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.jpg` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.pdf` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.pdf` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.png` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.png` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.svg` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-TM-BW.svg` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-®-white-transparent-RGB.png` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-®-white-transparent-RGB.png` |
| `ASIOSDK/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-®-white-transparent-RGB.svg` | `third_party/asiosdk/Steinberg ASIO Logo Artwork/ASIO-compatible-logo-Steinberg-®-white-transparent-RGB.svg` |
| `ASIOSDK/Steinberg ASIO SDK 2.3.pdf` | `third_party/asiosdk/Steinberg ASIO SDK 2.3.pdf` |
| `ASIOSDK/Steinberg ASIO Usage Guidelines.pdf` | `third_party/asiosdk/Steinberg ASIO Usage Guidelines.pdf` |
| `ASIOSDK/asio/asio.dsw` | `third_party/asiosdk/asio/asio.dsw` |
| `ASIOSDK/asio/asio.opt` | `third_party/asiosdk/asio/asio.opt` |
| `ASIOSDK/changes.txt` | `third_party/asiosdk/changes.txt` |
| `ASIOSDK/common/LICENSE.txt` | `third_party/asiosdk/common/LICENSE.txt` |
| `ASIOSDK/common/asio.cpp` | `third_party/asiosdk/common/asio.cpp` |
| `ASIOSDK/common/asio.h` | `third_party/asiosdk/common/asio.h` |
| `ASIOSDK/common/asiodrvr.cpp` | `third_party/asiosdk/common/asiodrvr.cpp` |
| `ASIOSDK/common/asiodrvr.h` | `third_party/asiosdk/common/asiodrvr.h` |
| `ASIOSDK/common/asiosys.h` | `third_party/asiosdk/common/asiosys.h` |
| `ASIOSDK/common/combase.cpp` | `third_party/asiosdk/common/combase.cpp` |
| `ASIOSDK/common/combase.h` | `third_party/asiosdk/common/combase.h` |
| `ASIOSDK/common/debugmessage.cpp` | `third_party/asiosdk/common/debugmessage.cpp` |
| `ASIOSDK/common/dllentry.cpp` | `third_party/asiosdk/common/dllentry.cpp` |
| `ASIOSDK/common/iasiodrv.h` | `third_party/asiosdk/common/iasiodrv.h` |
| `ASIOSDK/common/register.cpp` | `third_party/asiosdk/common/register.cpp` |
| `ASIOSDK/common/wxdebug.h` | `third_party/asiosdk/common/wxdebug.h` |
| `ASIOSDK/driver/asiosample/asiosample.def` | `third_party/asiosdk/driver/asiosample/asiosample.def` |
| `ASIOSDK/driver/asiosample/asiosample.txt` | `third_party/asiosdk/driver/asiosample/asiosample.txt` |
| `ASIOSDK/driver/asiosample/asiosample/asiosample.dsp` | `third_party/asiosdk/driver/asiosample/asiosample/asiosample.dsp` |
| `ASIOSDK/driver/asiosample/asiosample/asiosample.vcproj` | `third_party/asiosdk/driver/asiosample/asiosample/asiosample.vcproj` |
| `ASIOSDK/driver/asiosample/asiosmpl.cpp` | `third_party/asiosdk/driver/asiosample/asiosmpl.cpp` |
| `ASIOSDK/driver/asiosample/asiosmpl.h` | `third_party/asiosdk/driver/asiosample/asiosmpl.h` |
| `ASIOSDK/driver/asiosample/macnanosecs.cpp` | `third_party/asiosdk/driver/asiosample/macnanosecs.cpp` |
| `ASIOSDK/driver/asiosample/mactimer.cpp` | `third_party/asiosdk/driver/asiosample/mactimer.cpp` |
| `ASIOSDK/driver/asiosample/makesamp.cpp` | `third_party/asiosdk/driver/asiosample/makesamp.cpp` |
| `ASIOSDK/driver/asiosample/wintimer.cpp` | `third_party/asiosdk/driver/asiosample/wintimer.cpp` |
| `ASIOSDK/host/ASIOConvertSamples.cpp` | `third_party/asiosdk/host/ASIOConvertSamples.cpp` |
| `ASIOSDK/host/ASIOConvertSamples.h` | `third_party/asiosdk/host/ASIOConvertSamples.h` |
| `ASIOSDK/host/asiodrivers.cpp` | `third_party/asiosdk/host/asiodrivers.cpp` |
| `ASIOSDK/host/asiodrivers.h` | `third_party/asiosdk/host/asiodrivers.h` |
| `ASIOSDK/host/ginclude.h` | `third_party/asiosdk/host/ginclude.h` |
| `ASIOSDK/host/pc/asiolist.cpp` | `third_party/asiosdk/host/pc/asiolist.cpp` |
| `ASIOSDK/host/pc/asiolist.h` | `third_party/asiosdk/host/pc/asiolist.h` |
| `ASIOSDK/host/sample/hostsample.cpp` | `third_party/asiosdk/host/sample/hostsample.cpp` |
| `ASIOSDK/host/sample/hostsample.dsp` | `third_party/asiosdk/host/sample/hostsample.dsp` |
| `ASIOSDK/host/sample/hostsample.vcproj` | `third_party/asiosdk/host/sample/hostsample.vcproj` |

## NereusSDR-added files

- `third_party/asiosdk/VERSION.txt`: the version, download URL, date and
  archive SHA-256.
- `third_party/asiosdk/CMakeLists.txt`: the Windows-only static library
  `asiosdk_host` (`common/asio.cpp`, `host/asiodrivers.cpp`,
  `host/pc/asiolist.cpp`; include folders `common`, `host`, `host/pc`;
  links `ole32` and `advapi32`). It returns early on any other platform.

## Build wiring

The root `CMakeLists.txt` adds `third_party/asiosdk` inside `if(WIN32)`,
next to the PortAudio block, so macOS and Linux configures never see the
target and compile no SDK file. PortAudio's own ASIO host API stays off
(`PA_USE_ASIO` OFF), because NereusSDR hosts ASIO itself. Warnings are
silenced on the `asiosdk_host` target only.

## Not used from the SDK

NereusSDR's ASIO host code is written against the SDK's own headers and
its sample host (`host/sample/hostsample.cpp`) as a reference. Thetis's
`hostsample.cpp` (Steinberg's sample with Thetis changes and no GPL
header) is read as a reference only and never copied. The sample driver
(`driver/`), the driver-side helpers in `common/` (`asiodrvr.*`,
`combase.*`, `dllentry.cpp`, `register.cpp`, `debugmessage.cpp`,
`wxdebug.h`), `host/ASIOConvertSamples.*` and the sample projects are not
compiled.

## Updating

1. Download the new release only after JJ's yes, and read its licence.
2. Delete every SDK file under `third_party/asiosdk/` except `VERSION.txt`
   and `CMakeLists.txt`, then copy every file of the archive's `ASIOSDK/`
   folder in byte for byte.
3. Check the copy: `unzip -q <archive> -d /tmp/asio && diff -r /tmp/asio/ASIOSDK third_party/asiosdk`
   (only the two NereusSDR-added files differ).
4. Update `VERSION.txt`, this file's Upstream section and file table,
   `packaging/third-party-licenses/asiosdk.txt` (copy of `LICENSE.txt`)
   and `asiosdk-notices.txt`, and the rows in
   `packaging/third-party-licenses/README.md`.
5. Run `python3 scripts/check-third-party-licenses.py`.

Do not edit the vendored files in place.
