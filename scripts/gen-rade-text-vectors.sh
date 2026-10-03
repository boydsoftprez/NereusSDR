#!/usr/bin/env bash
# Regenerate tests/data/rade_text/freedv_backend_vectors.h from a local
# freedv-backend clone (nothing downloads). The vectors are what FreeDV's own
# rade_text code writes and reads, so tst_rade_text_codec proves a NereusSDR
# station and a FreeDV station decode each other's end-of-over callsigns.
#
#   FREEDV_BACKEND_DIR=/path/to/freedv-backend scripts/gen-rade-text-vectors.sh
#
# Default clone: ../freedv-backend beside the repository, else
# ~/freedv-backend.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
FB="${FREEDV_BACKEND_DIR:-}"
if [[ -z "$FB" ]]; then
    for c in "$REPO/../freedv-backend" "$HOME/freedv-backend"; do
        if [[ -d "$c/src/pipeline" ]]; then FB="$c"; break; fi
    done
fi
if [[ -z "$FB" || ! -f "$FB/src/pipeline/rade_text.cpp" ]]; then
    echo "freedv-backend clone not found; set FREEDV_BACKEND_DIR" >&2
    exit 2
fi
SHA="$(git -C "$FB" rev-parse --short HEAD)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# The upstream LDPC code needs only RADE_COMP from rade_api.h; the vendored
# rade_api.h pulls in librade's internals, so a one-type shim stands in.
cat > "$WORK/rade_api.h" <<'SHIM'
#pragma once
typedef struct { float real; float imag; } RADE_COMP;
SHIM

CXX="${CXX:-c++}"
CC="${CC:-cc}"
"$CC" -O1 -c "$FB/src/util/logging/ulog.c" -o "$WORK/ulog.o"
"$CXX" -std=c++20 -O1 \
    -I "$WORK" -I "$FB/src/pipeline" \
    "$REPO/tests/tools/rade_text_vectors/gen_rade_text_vectors.cpp" \
    "$FB/src/pipeline/rade_text.cpp" \
    "$FB/src/pipeline/ldpc_encode.cpp" \
    "$FB/src/pipeline/ldpc_decode.cpp" \
    "$WORK/ulog.o" -o "$WORK/gen"
"$WORK/gen" "$WORK/vectors.h" "$SHA" 2>/dev/null
# No trailing blanks in the committed header.
sed -e 's/[[:space:]]*$//' "$WORK/vectors.h" > "$REPO/tests/data/rade_text/freedv_backend_vectors.h"
echo "wrote tests/data/rade_text/freedv_backend_vectors.h from freedv-backend @$SHA"
