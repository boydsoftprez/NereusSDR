#!/bin/sh
# NereusSDR for iOS: vendors a third-party library's pinned sources into NereusKit
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
#
# Usage:
#   ios/scripts/vendor-sources.sh <library>            write the committed copy
#   ios/scripts/vendor-sources.sh <library> --out DIR  write into DIR instead
#   ios/scripts/vendor-sources.sh <library> --verify   write into a temporary
#       directory and compare it with the committed copy, byte for byte
#
# Libraries: opus libdatachannel libjuice libsrtp usrsctp plog mbedtls libsodium
#            spake2ee
#
# The script downloads the library's pinned archive (cached under
# ios/.build/vendor-cache, or $NEREUS_VENDOR_CACHE), refuses it unless its
# SHA-256 matches the pin, extracts the listed files into
# ios/NereusKit/Sources/<target>/, and prints the library's ios/THIRD-PARTY.md
# row. The copy is exactly the upstream files plus VENDORED.txt, which this
# script writes (and, for libdatachannel, include/module.modulemap, which
# exposes only the C API to Swift, and for libsrtp, config/config.h, the
# values its config_in_cmake.h takes in this build), so running it again
# reproduces the committed files.
#
# A library may carry patches under ios/patches/<library>/, applied in name
# order after the copy (patch -p1 from the archive's top). They are
# committed, so the result is still reproducible byte for byte; each one is
# named in VENDORED.txt and in the library's provenance file.
#
# The five media libraries (libdatachannel and the four it builds on) use
# the archives and SHA-256 values the desktop pins in
# cmake/NereusRemoteMedia.cmake. Mbed TLS is the app's DTLS library in place
# of the desktop's OpenSSL, pinned to its newest 3.6 LTS release.
#
# The pairing code's key exchange (libsodium and spake2-ee) uses the
# archives and SHA-256 values the desktop pins in cmake/NereusPairing.cmake.
# libsodium's copy also carries sodium/version.h (the archive's
# builds/msvc/version.h, as that CMake file takes it, since the archive
# ships only version.h.in) and include/module.modulemap, which exposes
# sodium.h alone to Swift; spake2-ee's carries src/module.modulemap, which
# marks its two headers textual.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
ios_dir=$(cd "$here/.." && pwd)

usage() {
    echo "usage: $0 <library> [--out DIR | --verify]" >&2
    echo "libraries: opus libdatachannel libjuice libsrtp usrsctp plog mbedtls libsodium spake2ee" >&2
    exit 2
}

[ $# -ge 1 ] || usage
library=$1
shift

mode=write
out_dir=
while [ $# -gt 0 ]; do
    case $1 in
        --out)
            [ $# -ge 2 ] || usage
            out_dir=$2
            shift 2
            ;;
        --verify)
            mode=verify
            shift
            ;;
        *)
            usage
            ;;
    esac
done

# Per library: its name, pin, archive, the archive's top directory, the
# target directory under NereusKit/Sources, its licence and its licence file.
archive_ext=tar.gz
licence_file=LICENSE
case $library in
    opus)
        display_name=Opus
        commit=940d4e5af64351ca8ba8390df3f555484c567fbb
        url="https://github.com/xiph/opus/archive/$commit.zip"
        sha256=20e37f9079ac2b80e3235cd8ce2547829e147bf44a2f5dd28a888fa3e9c24341
        top="opus-$commit"
        target=COpus
        licence=BSD-3-Clause
        licence_file=COPYING
        archive_ext=zip
        ;;
    libdatachannel)
        display_name=libdatachannel
        commit=v0.24.5
        url="https://codeload.github.com/paullouisageneau/libdatachannel/tar.gz/refs/tags/$commit"
        sha256=454537c3cd526bed935d847bb2dff4046f266eef84d43b2a5f2f2f293c0026f4
        top="libdatachannel-0.24.5"
        target=CDataChannel
        licence=MPL-2.0
        ;;
    libjuice)
        display_name=libjuice
        commit=3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6
        url="https://codeload.github.com/paullouisageneau/libjuice/tar.gz/$commit"
        sha256=a6b1d55338ea12adc0177eaafd9521ac0101b6a8716c71024a668f4896fd6b7c
        top="libjuice-$commit"
        target=CJuice
        licence=MPL-2.0
        ;;
    libsrtp)
        display_name=libsrtp
        commit=24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5
        url="https://codeload.github.com/cisco/libsrtp/tar.gz/$commit"
        sha256=063478e368d7cd13d04a908d152a46f90bfa728c4b324be6d413b0b92728207c
        top="libsrtp-$commit"
        target=CSrtp
        licence=BSD-3-Clause
        ;;
    usrsctp)
        display_name=usrsctp
        commit=fec583d54493f879d2ae44a743423bf8a04371ab
        url="https://codeload.github.com/paullouisageneau/usrsctp/tar.gz/$commit"
        sha256=e5c114afe73c9a0ec419fab5f5b3f63f3ce57b09b90d06ea85e63dca8aed3e7c
        top="usrsctp-$commit"
        target=CUsrsctp
        licence=BSD-3-Clause
        licence_file=LICENSE.md
        ;;
    plog)
        display_name=plog
        commit=94899e0b926ac1b0f4750bfbd495167b4a6ae9ef
        url="https://codeload.github.com/SergiusTheBest/plog/tar.gz/$commit"
        sha256=92a08bce559b5f28aa88d3fd9071567414b9f43a83fe2a05a6dd14f1da536072
        top="plog-$commit"
        target=CPlog
        licence=MIT
        ;;
    mbedtls)
        display_name="Mbed TLS"
        commit=v3.6.7
        url="https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2"
        sha256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
        top="mbedtls-3.6.7"
        target=CMbedTLS
        # Mbed TLS is dual licensed Apache-2.0 OR GPL-2.0-or-later; the app
        # takes it under Apache-2.0.
        licence=Apache-2.0
        archive_ext=tar.bz2
        ;;
    libsodium)
        display_name=libsodium
        commit=1.0.22-RELEASE
        url="https://github.com/jedisct1/libsodium/releases/download/1.0.22-RELEASE/libsodium-1.0.22.tar.gz"
        sha256=adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349
        top="libsodium-1.0.22"
        target=CSodium
        licence=ISC
        ;;
    spake2ee)
        display_name=spake2-ee
        commit=fd3ea61f27a75ff63b0f192c9e619b5a494d048e
        url="https://codeload.github.com/jedisct1/spake2-ee/tar.gz/$commit"
        sha256=20d63587c1191b952e98b9a4d8bd557c8a6c4f6bfbac0b9fb77b28854c244bb6
        top="spake2-ee-$commit"
        target=CSpake2EE
        licence=BSD-2-Clause
        ;;
    *)
        echo "$0: unknown library \"$library\"" >&2
        usage
        ;;
esac

target_rel="NereusKit/Sources/$target"
committed="$ios_dir/$target_rel"

sha256_of() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d ' ' -f 1
    else
        sha256sum "$1" | cut -d ' ' -f 1
    fi
}

cache_dir=${NEREUS_VENDOR_CACHE:-"$ios_dir/.build/vendor-cache"}
mkdir -p "$cache_dir"
archive="$cache_dir/$library-$commit.$archive_ext"

if [ ! -f "$archive" ] || [ "$(sha256_of "$archive")" != "$sha256" ]; then
    rm -f "$archive"
    echo "Downloading $url" >&2
    curl -sSLf -o "$archive.part" "$url"
    mv "$archive.part" "$archive"
fi
actual=$(sha256_of "$archive")
if [ "$actual" != "$sha256" ]; then
    echo "$0: $archive has SHA-256 $actual, expected $sha256" >&2
    rm -f "$archive"
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/nereus-vendor.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
mkdir -p "$work/archive"
case $archive_ext in
    zip) unzip -q "$archive" -d "$work/archive" ;;
    tar.gz) tar -xzf "$archive" -C "$work/archive" ;;
    tar.bz2) tar -xjf "$archive" -C "$work/archive" ;;
esac
src="$work/archive/$top"

# Prints the files named by make variables in one of the library's .mk
# lists, one per line.
mk_files() {
    mk=$1
    shift
    awk -v wanted=" $* " '
        function take(line) {
            gsub(/\\/, "", line)
            n = split(line, parts, /[ \t]+/)
            for (i = 1; i <= n; i++) {
                if (parts[i] != "") {
                    print parts[i]
                }
            }
        }
        {
            if (collecting) {
                more = ($0 ~ /\\[ \t]*$/)
                take($0)
                collecting = more
                next
            }
            if (match($0, /^[A-Z0-9_]+[ \t]*=/)) {
                name = $0
                sub(/[ \t]*=.*/, "", name)
                if (index(wanted, " " name " ") > 0) {
                    rest = $0
                    sub(/^[^=]*=/, "", rest)
                    more = ($0 ~ /\\[ \t]*$/)
                    take(rest)
                    collecting = more
                }
            }
        }
    ' "$src/$mk"
}

list_opus_files() {
    # The float build of the codec: CELT, SILK with its float analysis,
    # and the Opus layer. The deep-learning parts (dnn/: deep PLC, DRED,
    # OSCE) and every processor-specific file (x86, arm, mips) stay out;
    # the build defines none of the switches that would reach them.
    {
        mk_files celt_sources.mk CELT_SOURCES
        mk_files silk_sources.mk SILK_SOURCES SILK_SOURCES_FLOAT
        mk_files opus_sources.mk OPUS_SOURCES OPUS_SOURCES_FLOAT
        mk_files celt_headers.mk CELT_HEAD
        mk_files silk_headers.mk SILK_HEAD
        mk_files opus_headers.mk OPUS_HEAD
        for header in "$src"/include/*.h; do
            echo "include/$(basename "$header")"
        done
        echo COPYING
    } | grep -v -E '(^|/)(x86|arm|mips|fixed)/' | LC_ALL=C sort -u
}

# Prints the archive's files under directory $1 whose names match any of
# the patterns that follow, relative to the archive's top, one per line.
files_under() {
    dir=$1
    shift
    for pattern in "$@"; do
        (cd "$src" && find "$dir" -type f -name "$pattern")
    done
}

list_libdatachannel_files() {
    # The whole library as its CMakeLists.txt builds it: the public
    # headers (the C API rtc/rtc.h among them), the sources and the
    # implementation. The WebSocket, TLS and HTTP files compile to nothing
    # with RTC_ENABLE_WEBSOCKET=0. Examples, tests and deps/ stay out; the
    # dependencies are vendored as their own targets.
    {
        files_under include/rtc '*.h' '*.hpp'
        files_under src '*.cpp' '*.hpp'
        echo LICENSE
    } | LC_ALL=C sort -u
}

list_libjuice_files() {
    # LIBJUICE_SOURCES and the headers beside them, and the public header.
    {
        files_under src '*.c' '*.h'
        echo include/juice/juice.h
        echo LICENSE
    } | LC_ALL=C sort -u
}

list_libsrtp_files() {
    # The CMakeLists.txt source lists for the Mbed TLS crypto engine
    # (ENABLE_MBEDTLS): SOURCES_C, CIPHERS_SOURCES_C, HASHES_SOURCES_C,
    # KERNEL_SOURCES_C, MATH_SOURCES_C and REPLAY_SOURCES_C, and the
    # headers. The OpenSSL, NSS and built-in AES and SHA-1 files stay out.
    {
        echo srtp/srtp.c
        for file in cipher.c cipher_test_cases.c cipher_test_cases.h \
                null_cipher.c aes_icm_mbedtls.c aes_gcm_mbedtls.c; do
            echo "crypto/cipher/$file"
        done
        for file in auth.c auth_test_cases.c auth_test_cases.h null_auth.c \
                hmac_mbedtls.c; do
            echo "crypto/hash/$file"
        done
        for file in alloc.c crypto_kernel.c err.c key.c; do
            echo "crypto/kernel/$file"
        done
        echo crypto/math/datatypes.c
        echo crypto/replay/rdb.c
        echo crypto/replay/rdbx.c
        files_under crypto/include '*.h'
        echo include/srtp.h
        echo include/srtp_priv.h
        echo include/stream_list_priv.h
        echo LICENSE
    } | LC_ALL=C sort -u
}

list_usrsctp_files() {
    # usrsctplib/: the sources its CMakeLists.txt builds and their headers.
    {
        files_under usrsctplib '*.c' '*.h'
        echo LICENSE.md
    } | LC_ALL=C sort -u
}

list_plog_files() {
    # plog is header-only: its include/ tree.
    {
        files_under include '*.h'
        echo LICENSE
    } | LC_ALL=C sort -u
}

list_mbedtls_files() {
    # library/ (the crypto, X.509 and TLS sources its CMakeLists.txt
    # builds, with their private headers) and the public include/mbedtls
    # and include/psa headers. 3rdparty/ (Everest and p256-m, off in the
    # default configuration), programs, tests and the framework stay out.
    {
        files_under library '*.c' '*.h'
        files_under include/mbedtls '*.h'
        files_under include/psa '*.h'
        echo LICENSE
    } | LC_ALL=C sort -u
}

list_libsodium_files() {
    # The whole library, not the minimal build (spake2-ee uses the ed25519
    # core functions): every .c and .h file under src/libsodium, as
    # cmake/NereusPairing.cmake compiles them. The hand-written assembly
    # (.S) stays out, since SwiftPM would compile it; the build defines
    # none of the switches that would reach it or the SIMD variants.
    {
        files_under src/libsodium '*.c' '*.h'
        echo LICENSE
    } | LC_ALL=C sort -u
}

list_spake2ee_files() {
    # The exchange itself and its licence; the tests stay out.
    {
        echo src/crypto_spake.c
        echo src/crypto_spake.h
        echo src/pushpop.h
        echo LICENSE
    } | LC_ALL=C sort -u
}

# libsodium's sodium/version.h, the archive's builds/msvc/version.h (the
# archive ships only version.h.in, which configure fills), and the module
# map that exposes sodium.h, and nothing else, to Swift.
write_libsodium_extras() {
    include="$1/src/libsodium/include"
    cp "$src/builds/msvc/version.h" "$include/sodium/version.h"
    {
        echo "module CSodium {"
        echo "    header \"sodium.h\""
        echo "    export *"
        echo "}"
    } > "$include/module.modulemap"
}

# spake2-ee's module map. Its headers use size_t and uint16_t with no
# includes of their own, so neither can be compiled alone as a Clang
# module: both are textual, and Swift reaches crypto_spake.h through the
# CSpake2EEShim target's header, which includes what it needs first.
write_spake2ee_modulemap() {
    {
        echo "module CSpake2EE {"
        echo "    textual header \"crypto_spake.h\""
        echo "    textual header \"pushpop.h\""
        echo "}"
    } > "$1/src/module.modulemap"
}

# libsrtp's config.h: what its CMakeLists.txt writes from config_in_cmake.h
# for an Apple target with the Mbed TLS crypto engine (MBEDTLS and GCM in
# place of the desktop's OPENSSL). Every header and function it records is
# present on macOS and iOS alike; both are 64-bit little-endian.
write_libsrtp_config() {
    mkdir -p "$1/config"
    {
        echo "/* libsrtp config.h for NereusSDR for iOS, written by"
        echo "   ios/scripts/vendor-sources.sh libsrtp from config_in_cmake.h. */"
        for define in \
                'PACKAGE_VERSION "2.8.0"' \
                'PACKAGE_STRING "libsrtp2 2.8.0"' \
                'MBEDTLS 1' 'GCM 1' 'CPU_CISC 1' \
                'HAVE_ARPA_INET_H 1' 'HAVE_INTTYPES_H 1' \
                'HAVE_MACHINE_TYPES_H 1' 'HAVE_NETINET_IN_H 1' \
                'HAVE_STDINT_H 1' 'HAVE_STDLIB_H 1' 'HAVE_SYS_SOCKET_H 1' \
                'HAVE_SYS_TYPES_H 1' 'HAVE_UNISTD_H 1' 'HAVE_INET_ATON 1' \
                'HAVE_INET_PTON 1' 'HAVE_SIGACTION 1' 'HAVE_USLEEP 1' \
                'HAVE_UINT8_T 1' 'HAVE_UINT16_T 1' 'HAVE_UINT32_T 1' \
                'HAVE_UINT64_T 1' 'HAVE_INT32_T 1' \
                'SIZEOF_UNSIGNED_LONG 8' 'SIZEOF_UNSIGNED_LONG_LONG 8' \
                'HAVE_INLINE 1'; do
            echo "#define $define"
        done
    } > "$1/config/config.h"
}

# The module map that exposes libdatachannel's C API, and nothing of its
# C++ headers, to Swift.
write_libdatachannel_modulemap() {
    {
        echo "module CDataChannel {"
        echo "    header \"rtc/rtc.h\""
        echo "    export *"
        echo "}"
    } > "$1/include/module.modulemap"
}

# Writes the library's files and VENDORED.txt into $1.
write_copy() {
    dest=$1
    rm -rf "$dest"
    mkdir -p "$dest"
    "list_${library}_files" > "$work/files"
    while IFS= read -r file; do
        if [ ! -f "$src/$file" ]; then
            echo "$0: the archive has no $file" >&2
            exit 1
        fi
        mkdir -p "$dest/$(dirname "$file")"
        cp "$src/$file" "$dest/$file"
    done < "$work/files"
    count=$(wc -l < "$work/files" | tr -d ' ')
    patches=
    if [ -d "$ios_dir/patches/$library" ]; then
        for patch_file in "$ios_dir/patches/$library"/*.patch; do
            [ -f "$patch_file" ] || continue
            if ! patch -p1 -s -N -d "$dest" < "$patch_file"; then
                echo "$0: $patch_file does not apply" >&2
                exit 1
            fi
            patches="$patches ${patch_file#"$ios_dir/"}"
        done
    fi
    {
        echo "$display_name, vendored into NereusSDR for iOS"
        echo
        case $commit in
            v* | *-RELEASE) echo "Tag:      $commit" ;;
            *) echo "Commit:   $commit" ;;
        esac
        echo "Archive:  $url"
        echo "SHA-256:  $sha256"
        echo "Licence:  $licence (see $licence_file)"
        if [ -n "$patches" ]; then
            echo "Files:    $count, copied from the archive, then patched by"
            for patch_file in $patches; do
                echo "          ios/$patch_file"
            done
        else
            echo "Files:    $count, copied unchanged from the archive"
        fi
        case $library in
            libdatachannel)
                echo "          and include/module.modulemap, written by the script"
                ;;
            libsrtp)
                echo "          and config/config.h, written by the script"
                ;;
            libsodium)
                echo "          and src/libsodium/include/sodium/version.h (a copy of"
                echo "          builds/msvc/version.h) and src/libsodium/include/module.modulemap,"
                echo "          written by the script"
                ;;
            spake2ee)
                echo "          and src/module.modulemap, written by the script"
                ;;
        esac
        echo
        echo "Written by ios/scripts/vendor-sources.sh $library. Do not edit"
        if [ -n "$patches" ]; then
            echo "these files; change the pin or the patches and run it again."
        else
            echo "these files; change the pin in that script and run it again."
        fi
    } > "$dest/VENDORED.txt"
    case $library in
        libdatachannel) write_libdatachannel_modulemap "$dest" ;;
        libsrtp) write_libsrtp_config "$dest" ;;
        libsodium) write_libsodium_extras "$dest" ;;
        spake2ee) write_spake2ee_modulemap "$dest" ;;
    esac
}

row() {
    version=$commit
    if [ -d "$ios_dir/patches/$library" ]; then
        version="$commit, patched (ios/patches/$library)"
    fi
    echo "| $display_name | $version | $url | $sha256 | $licence | $target_rel |"
}

case $mode in
    write)
        dest=${out_dir:-$committed}
        write_copy "$dest"
        echo "Wrote $display_name into $dest" >&2
        echo "ios/THIRD-PARTY.md row:" >&2
        row
        ;;
    verify)
        write_copy "$work/copy"
        if [ ! -d "$committed" ]; then
            echo "$0: $committed is missing" >&2
            exit 1
        fi
        if diff -r "$work/copy" "$committed" >&2; then
            echo "$display_name: the committed files match the pinned archive" >&2
        else
            echo "$0: $target_rel differs from what the pinned archive gives" >&2
            exit 1
        fi
        ;;
esac
