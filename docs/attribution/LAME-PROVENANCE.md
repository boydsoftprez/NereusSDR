# LAME and minimp3 Provenance - NereusSDR recording files

The Core writes contact recordings, voice keyer messages and spectrum
recorder audio as WAV or MP3 (`src/core/recordings/AudioFileWriter.cpp`) and
reads them back for playback (`src/core/recordings/AudioFileReader.cpp`).
MP3 is encoded with LAME and decoded with minimp3. Neither is ported: both
are called through their public interfaces, and no upstream logic is copied
into NereusSDR files.

## LAME (encoder)

- **Project:** LAME (LAME Ain't an MP3 Encoder), https://lame.sourceforge.io/
- **Library:** libmp3lame, linked as a shared library, unchanged
- **Licence:** GNU Library General Public License version 2 or later (the
  notice in `include/lame/lame.h`; `COPYING` is the LGPL version 2 text).
  NereusSDR takes it under LGPL-2.1. Notice: `packaging/third-party-licenses/lame.txt`.
- **Interface used:** `lame_init`, `lame_set_in_samplerate`,
  `lame_set_out_samplerate`, `lame_set_num_channels`, `lame_set_mode`,
  `lame_set_VBR`, `lame_set_brate`, `lame_set_bWriteVbrTag`,
  `lame_set_write_id3tag_automatic`, `lame_init_params`,
  `lame_encode_buffer_ieee_float`, `lame_encode_buffer_interleaved_ieee_float`,
  `lame_encode_flush`, `lame_get_lametag_frame`, `lame_close`, as declared
  in `lame.h` of LAME 3.100.
- **Optional:** CMake's `NEREUS_WITH_LAME` (default ON) looks for
  `lame/lame.h` and `mp3lame`; when found the Core gets `NEREUS_HAVE_LAME`.
  Without it `AudioFileWriter::mp3Available()` is false and a request for
  MP3 fails with a reason, so the caller records WAV.

| Build | Where LAME comes from |
| --- | --- |
| macOS (CI, Apple Silicon release) | Homebrew `lame` |
| macOS Intel release | LAME 3.100 built from source for x86_64 by `.github/workflows/release.yml` (the Apple Silicon runner's Homebrew has no x86_64 LAME): the SourceForge `lame-3.100.tar.gz`, SHA-256 checked against the value Homebrew's formula pins, with Homebrew's one fix (`lame_init_old` taken out of `include/libmp3lame.sym`), shared only; `libmp3lame.0.dylib` is bundled in `Contents/Frameworks` |
| Linux CI, self-hosted CI image, AppImage, .deb | `libmp3lame-dev`; runtime `libmp3lame0` (dpkg-shlibdeps adds it to a .deb's Depends) |
| Debian 13 arm64 Core package (the Pi image) | `libmp3lame-dev` in `packaging/station-image/build-trixie-deb.sh` |
| Windows CI (MinGW) | MSYS2 `mingw-w64-x86_64-lame`, `LAME_ROOT` = the MINGW64 folder; its `libmpg123-0.dll` dependency is staged for the tests by `scripts/ci/test-windows-audio.ps1` (CI only, not shipped) |
| Windows release (MSVC) | LAME 3.100 built from source by `.github/workflows/release.yml` with the MSYS2 MINGW64 toolchain: the same SourceForge tarball and SHA-256 as the macOS Intel row, shared only, `--disable-frontend --disable-decoder`, with `lame_init_old` and the decoder's `hip_*` and `lame_decode*` names taken out of `include/libmp3lame.sym` (undefined without the decoder). MSYS2's own `mingw-w64-x86_64-lame` (4.0) imports `libmpg123-0.dll`, which the package would otherwise have to ship. Staged in `third_party/lame` with an MSVC import library made from the DLL's exports; the step fails if the DLL imports anything but Windows' own DLLs; `libmp3lame-0.dll` is copied beside `NereusSDR.exe` |

LGPL: the library stays a separate, replaceable shared library on every
platform (`find_library` asks for the shared names first), which meets
LGPL-2.1 section 6(b). See `docs/attribution/LGPL-COMPLIANCE.md`.

## minimp3 (decoder)

- **Project:** minimp3, https://github.com/lieff/minimp3
- **Commit:** `a2ad968c94819fb98589cca8026e0a57958c963c` (2026-11-15)
- **Vendored:** 2026-11-15, `third_party/minimp3/` (`minimp3.h`,
  `minimp3_ex.h`, `LICENSE`), unchanged; per-file SHA-256 in
  `third_party/minimp3/VERSION.txt`
- **Licence:** CC0 1.0 Universal. Notice:
  `packaging/third-party-licenses/minimp3.txt`.
- **Use:** `AudioFileReader.cpp` compiles the implementation once with
  `MINIMP3_FLOAT_OUTPUT` and `MINIMP3_NO_STDIO`, and reads through
  `mp3dec_ex_open_cb` over a `QFile` (so a non-ASCII Windows path works),
  with `MP3D_SEEK_TO_SAMPLE`. `mp3dec_ex` reads LAME's Info tag and trims
  the encoder delay and padding.

## Decoder check (V10), 2026-11-15

The plan asked whether the LAME each build gets exports LAME's own decoder
(`hip_decode_init`, `hip_decode`), and to use it only if every platform has
it, else to vendor minimp3 for one decode path everywhere.

| Platform | Evidence | Result |
| --- | --- | --- |
| macOS, this Mac (Homebrew lame 3.100) | `nm -gU /opt/homebrew/opt/lame/lib/libmp3lame.dylib` lists `T _hip_decode_init`, `T _hip_decode`, `_hip_decode1`, `_hip_decode_headers`, `_hip_decode_exit` | has them |
| macOS CI (`brew install lame`) | `brew info lame` on this Mac: "3.100 -> stable 4.0"; CI installs 4.0, whose decoder exports nobody here has seen | not established |
| Windows (MSYS2 `mingw-w64-x86_64-lame`) | the package's PKGBUILD (MINGW-packages master) is `pkgver=4.0`, configured `--enable-shared --enable-static [--enable-nasm]` with no `--disable-decoder`; LAME 4.0's release notes were not reachable | not established |
| Linux CI and the self-hosted image (Ubuntu 24.04 `libmp3lame0`), Pi image (Debian 13 arm64 `libmp3lame0`) | sources.debian.org returned HTTP 503 for the package's symbols file; no Linux machine was used | not established |

Three of four could not be established before the controller's push, so
by the plan's rule minimp3 is the decoder on every platform and LAME is
used for encoding only. CI prints each platform's answer anyway: the
"LAME exports" lines in `.github/workflows/ci.yml` (Linux, macOS,
Windows) run `nm -D`, `nm -gU` and `objdump -p` on the library the leg
links and list any `hip_decode*` and `lame_encode*` exports, so a later
change can revisit the choice with evidence.

## Modification history

| Date | Change |
| --- | --- |
| 2026-11-15 | Created: LAME for MP3 recordings, minimp3 vendored for decoding (recording, voice keyer and spectrum recorder plan, Task 1). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. |
| 2026-11-16 | Windows release builds LAME 3.100 from source without its decoder instead of shipping MSYS2's package, which imports libmpg123. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. |
