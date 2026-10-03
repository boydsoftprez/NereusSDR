#!/bin/bash
set -euo pipefail
# Native arm64 Debian trixie container (rust:1.94.1-trixie), no prebuilt
# DeepFilterNet archive. The pinned source has Cargo.lock and the model.
cd /workspace
test "$(uname -m)" = aarch64
test "$(rustc --version)" = 'rustc 1.94.1 (e408947bf 2026-03-25)'
test "$(cargo --version)" = 'cargo 1.94.1 (29ea6fb6a 2026-03-24)'
commit=d375b2d8309e0935d165700c91da9de862a99c31
target=aarch64-unknown-linux-gnu
src=/tmp/nereus-dfnr-source
lib=/workspace/third_party/deepfilter/lib/linux-aarch64/libdeepfilter.a
model=/workspace/third_party/deepfilter/models/DeepFilterNet3_onnx.tar.gz

# cargo-c 0.10.21 uses Cargo 0.95, matching Rust 1.94. Build it from its
# published crate with its own lock file, then check the installed version.
cargo install cargo-c --version '0.10.21+cargo-0.95.0' --locked
test "$(cargo cbuild --version)" = 'cargo-c 0.10.21+cargo-0.95.0'

git init -q "$src"
git -C "$src" remote add origin https://github.com/Rikorose/DeepFilterNet.git
git -C "$src" fetch --depth 1 origin "$commit"
git -C "$src" checkout --detach FETCH_HEAD
test "$(git -C "$src" rev-parse HEAD)" = "$commit"
test -f "$src/Cargo.lock"
test -s "$src/models/DeepFilterNet3_onnx.tar.gz"
test "$(sha256sum "$src/Cargo.lock" | cut -d' ' -f1)" = \
    9d89af1892a255dbd5a0e66c2983e0c4d3893cfa301bd88278851562498473f0
test "$(sha256sum "$src/models/DeepFilterNet3_onnx.tar.gz" | cut -d' ' -f1)" = \
    c94d91f70911001c946e0fabb4aa9adc37045f45a03b56008cb0c8244cb63616

# The historical upstream lock no longer matches this commit's manifests.
# Cargo 1.94.1 refreshed it once; commit that exact result here so all image
# runs resolve the same crates without an unlocked update.
test "$(sha256sum packaging/station-image/DeepFilterNet-Cargo.lock | cut -d' ' -f1)" = \
    e89bdec912489ecda3f4df56d71c17ad2e94dc9b42821429cd96cfec2684b5bf
cp packaging/station-image/DeepFilterNet-Cargo.lock "$src/Cargo.lock"

export CFLAGS='-march=armv8-a' CXXFLAGS='-march=armv8-a'
export RUSTFLAGS='-C target-cpu=generic'
(cd "$src" && cargo cbuild --locked --release -p deep_filter \
    --features capi --target "$target")
built_lib="$src/target/$target/release/libdeepfilter.a"
test -s "$built_lib"
mkdir -p "$(dirname "$lib")" "$(dirname "$model")" /workspace/station-output
install -m 0644 "$built_lib" "$lib"
install -m 0644 "$src/models/DeepFilterNet3_onnx.tar.gz" "$model"

DFNR_COMMIT="$commit" DFNR_LIB="$lib" DFNR_MODEL="$model" python3 - <<'PY'
import hashlib
import json
import os
import subprocess
from pathlib import Path

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

data = {
    "source_sha": os.environ["DFNR_COMMIT"],
    "source_lock_sha256": digest("/tmp/nereus-dfnr-source/Cargo.lock"),
    "upstream_lock_sha256": "9d89af1892a255dbd5a0e66c2983e0c4d3893cfa301bd88278851562498473f0",
    "rustc": subprocess.check_output(["rustc", "--version"], text=True).strip(),
    "cargo": subprocess.check_output(["cargo", "--version"], text=True).strip(),
    "cargo_c": subprocess.check_output(["cargo", "cbuild", "--version"], text=True).strip(),
    "target": "aarch64-unknown-linux-gnu",
    "rustflags": os.environ["RUSTFLAGS"],
    "cflags": os.environ["CFLAGS"],
    "cxxflags": os.environ["CXXFLAGS"],
    "library_sha256": digest(os.environ["DFNR_LIB"]),
    "model_sha256": digest(os.environ["DFNR_MODEL"]),
}
Path("/workspace/station-output/dfnr-provenance.json").write_text(
    json.dumps(data, indent=2) + "\n"
)
PY
