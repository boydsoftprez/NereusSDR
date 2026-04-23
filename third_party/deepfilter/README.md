# DeepFilterNet3 C API — vendor directory

Upstream: https://github.com/Rikorose/DeepFilterNet  
Pinned commit: d375b2d8309e0935d165700c91da9de862a99c31  
License: MIT OR Apache-2.0 (dual; see LICENSE-MIT / LICENSE-APACHE)

## What lives here

| Path | Contents |
|------|----------|
| `include/deep_filter.h` | C API header (vendored verbatim) |
| `LICENSE` / `LICENSE-MIT` / `LICENSE-APACHE` | License files |
| `COMMIT` | Pinned upstream commit hash |
| `models/` | Model tarball location (`DeepFilterNet3_onnx.tar.gz`); populated by setup script |
| `lib/<platform>/libdeepfilter.a` | Pre-built static lib; built locally per platform |

## Building

Run the setup script from the NereusSDR repo root:

```bash
./setup-deepfilter.sh
```

This will:
1. Try to download a pre-built `libdeepfilter.a` from the GitHub Release tagged `dfnr-libs`
2. Fall back to a source build via `cargo cbuild` if the pre-built binary is unavailable
3. Extract the `DeepFilterNet3_onnx.tar.gz` model tarball into `models/`

After the script succeeds, re-run CMake (`cmake -B build`) — CMake will detect
`lib/<platform>/libdeepfilter.a` and enable DFNR automatically.

Requires: Rust toolchain (`cargo`) — install via https://rustup.rs  
First source build takes ~5–10 min.

## Files NOT checked in (built locally)

- `lib/` — pre-built static libraries (per-platform; gitignored)
- `models/DeepFilterNet3_onnx.tar.gz` — model weights tarball (gitignored; fetched by setup script)
