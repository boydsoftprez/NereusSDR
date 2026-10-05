// swift-tools-version:6.0
// NereusSDR for iOS: the NereusKit package, the app's modules below the screens
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import PackageDescription

// Mbed TLS's configuration. It must be the same for Mbed TLS and for every
// target that includes its headers, because its structures' layout depends
// on it. The default configuration leaves DTLS-SRTP out; libdatachannel
// needs it to key SRTP from the DTLS handshake.
let mbedTLSConfiguration: [CSetting] = [
    .define("MBEDTLS_SSL_DTLS_SRTP"),
]

// The vendored media libraries' files stay unchanged, so their compiler
// warnings are silenced on their own targets rather than fixed in place, and
// they compile as plain C, without Clang modules: their private headers
// (usrsctp's netinet copies above all) collide with the SDK's modules when
// the target's include directory is treated as one.
let upstreamFlags: CSetting = .unsafeFlags(["-w", "-fno-modules"])

// libsodium's configuration, the one cmake/NereusPairing.cmake gives the
// desktop on an Apple target: the portable C code (no assembly, no SIMD
// variants), 128-bit arithmetic on the 64-bit targets the app builds for,
// and the platform's functions. The iOS SDK has no sys/random.h and does
// not declare getentropy, so those two are macOS only; on iOS libsodium
// reads the system's random device, as its own configure build would.
// SODIUM_STATIC is public on the desktop; every target that includes
// sodium.h here sets it too.
let sodiumStatic: CSetting = .define("SODIUM_STATIC", to: "1")
let sodiumConfiguration: [CSetting] = [
    sodiumStatic,
    .define("CONFIGURED", to: "1"),
    .define("HAVE_INTTYPES_H", to: "1"),
    .define("HAVE_STDINT_H", to: "1"),
    .define("NATIVE_LITTLE_ENDIAN", to: "1"),
    .define("HAVE_TI_MODE", to: "1"),
    .define("TLS", to: "_Thread_local"),
    .define("HAVE_ARC4RANDOM", to: "1"),
    .define("HAVE_ARC4RANDOM_BUF", to: "1"),
    .define("HAVE_CATCHABLE_ABRT", to: "1"),
    .define("HAVE_CATCHABLE_SEGV", to: "1"),
    .define("HAVE_CLOCK_GETTIME", to: "1"),
    .define("HAVE_GETENTROPY", to: "1", .when(platforms: [.macOS])),
    .define("HAVE_GETPID", to: "1"),
    .define("HAVE_MADVISE", to: "1"),
    .define("HAVE_MEMSET_S", to: "1"),
    .define("HAVE_MLOCK", to: "1"),
    .define("HAVE_MMAP", to: "1"),
    .define("HAVE_MPROTECT", to: "1"),
    .define("HAVE_NANOSLEEP", to: "1"),
    .define("HAVE_POSIX_MEMALIGN", to: "1"),
    .define("HAVE_PTHREAD", to: "1"),
    .define("HAVE_PTHREAD_PRIO_INHERIT", to: "1"),
    .define("HAVE_RAISE", to: "1"),
    .define("HAVE_SYSCONF", to: "1"),
    .define("HAVE_SYS_MMAN_H", to: "1"),
    .define("HAVE_SYS_PARAM_H", to: "1"),
    .define("HAVE_SYS_RANDOM_H", to: "1", .when(platforms: [.macOS])),
    .define("HAVE_WEAK_SYMBOLS", to: "1"),
]

let package = Package(
    name: "NereusKit",
    platforms: [.iOS("17.4"), .macOS(.v14)],
    products: [
        // What the app links: every module below the screens.
        .library(name: "NereusKit", targets: ["NereusModels", "NereusLink", "NereusMirror", "NereusMedia", "NereusBand"]),
        // What the app's tests link, never the app itself: a fake Core
        // played from the link's conformance suite.
        .library(name: "NereusKitTesting", targets: ["NereusKitTesting"]),
    ],
    targets: [
        .target(
            name: "NereusModels",
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "NereusModelsTests",
            dependencies: ["NereusModels", "LinkTestSupport"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // Opus, vendored unchanged by ios/scripts/vendor-sources.sh opus: the
        // float build, without the deep-learning parts under dnn/.
        .target(
            name: "COpus",
            exclude: ["COPYING", "VENDORED.txt"],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("celt"),
                .headerSearchPath("silk"),
                .headerSearchPath("silk/float"),
                .define("OPUS_BUILD"),
                .define("USE_ALLOCA"),
                .define("HAVE_LRINTF"),
                .define("HAVE_LRINT"),
                .define("PACKAGE_VERSION", to: "\"940d4e5a\""),
                // The upstream files stay unchanged, so their compiler
                // warnings are silenced here rather than fixed in place.
                .unsafeFlags(["-w"]),
            ]
        ),
        .target(
            name: "COpusShim",
            dependencies: ["COpus"]
        ),
        // The media peer's libraries, vendored unchanged by
        // ios/scripts/vendor-sources.sh at the desktop's pins
        // (cmake/NereusRemoteMedia.cmake), with Mbed TLS for DTLS where the
        // desktop has OpenSSL. Each target's defines are the ones the
        // library's own CMake build sets for this configuration.
        .target(
            name: "CMbedTLS",
            exclude: ["LICENSE", "VENDORED.txt"],
            publicHeadersPath: "include",
            cSettings: mbedTLSConfiguration + [
                .headerSearchPath("library"),
                upstreamFlags,
            ]
        ),
        .target(
            name: "CSrtp",
            dependencies: ["CMbedTLS"],
            exclude: ["LICENSE", "VENDORED.txt"],
            publicHeadersPath: "include",
            cSettings: mbedTLSConfiguration + [
                .headerSearchPath("crypto/include"),
                // config/config.h, written by vendor-sources.sh, holds
                // config_in_cmake.h's values for this build.
                .headerSearchPath("config"),
                .define("HAVE_CONFIG_H"),
                upstreamFlags,
            ]
        ),
        .target(
            name: "CJuice",
            exclude: ["LICENSE", "VENDORED.txt"],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("include/juice"),
                .define("JUICE_STATIC"),
                .define("JUICE_EXPORTS"),
                .define("NO_SERVER"),
                .define("USE_NETTLE", to: "0"),
                .define("JUICE_TESTING", .when(configuration: .debug)),
                upstreamFlags,
            ]
        ),
        // Test-only access to libjuice's locked relay-entry state. This is
        // outside the app product and is never linked into a shipping build.
        .target(
            name: "CJuiceCloseTestSupport",
            dependencies: ["CJuice"],
            path: "Tests/CJuiceCloseTestSupport",
            publicHeadersPath: "include",
            cSettings: [.headerSearchPath("../../Sources/CJuice/include/juice")]
        ),
        .target(
            name: "CUsrsctp",
            exclude: ["LICENSE.md", "VENDORED.txt"],
            publicHeadersPath: "usrsctplib",
            cSettings: [
                .define("__Userspace__"),
                .define("SCTP_SIMPLE_ALLOCATOR"),
                .define("SCTP_PROCESS_LEVEL_LOCKS"),
                .define("SCTP_DEBUG"),
                .define("__APPLE_USE_RFC_2292"),
                .define("HAVE_SYS_QUEUE_H"),
                .define("HAVE_NETINET_IP_ICMP_H"),
                .define("HAVE_NET_ROUTE_H"),
                .define("HAVE_STDATOMIC_H"),
                .define("HAVE_SA_LEN"),
                .define("HAVE_SIN_LEN"),
                .define("HAVE_SIN6_LEN"),
                .define("HAVE_SCONN_LEN"),
                upstreamFlags,
            ]
        ),
        .target(
            name: "CDataChannel",
            dependencies: ["CMbedTLS", "CSrtp", "CJuice", "CUsrsctp"],
            exclude: ["LICENSE", "VENDORED.txt"],
            publicHeadersPath: "include",
            cxxSettings: [
                .define("MBEDTLS_SSL_DTLS_SRTP"),
                .headerSearchPath("include/rtc"),
                .headerSearchPath("src"),
                // plog is header-only, so it is a directory, not a target.
                .headerSearchPath("../CPlog/include"),
                .define("RTC_STATIC"),
                .define("RTC_EXPORTS"),
                .define("RTC_ENABLE_MEDIA", to: "1"),
                .define("RTC_ENABLE_WEBSOCKET", to: "0"),
                .define("RTC_SYSTEM_JUICE", to: "0"),
                .define("RTC_SYSTEM_SRTP", to: "0"),
                .define("USE_MBEDTLS", to: "1"),
                .define("USE_GNUTLS", to: "0"),
                .define("USE_NICE", to: "0"),
                .define("JUICE_STATIC"),
                .unsafeFlags(["-w"]),
            ]
        ),
        // Native teardown queue control used only by lifetime regressions.
        .target(
            name: "CPeerLifetimeTestSupport",
            dependencies: ["CDataChannel"],
            path: "Tests/CPeerLifetimeTestSupport",
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("../../Sources/CDataChannel/include/rtc"),
                .headerSearchPath("../../Sources/CDataChannel/src"),
                .headerSearchPath("../../Sources/CPlog/include"),
                .define("RTC_STATIC"),
                .define("RTC_ENABLE_MEDIA", to: "1"),
                .define("RTC_ENABLE_WEBSOCKET", to: "0"),
            ]
        ),
        // The pairing code's key exchange (link document section 3.6):
        // libsodium and spake2-ee, vendored unchanged by
        // ios/scripts/vendor-sources.sh at the desktop's pins
        // (cmake/NereusPairing.cmake). Swift sees sodium.h alone, through
        // the module map the script writes, and spake2-ee through the shim.
        .target(
            name: "CSodium",
            exclude: ["LICENSE", "VENDORED.txt"],
            publicHeadersPath: "src/libsodium/include",
            cSettings: sodiumConfiguration + [
                .headerSearchPath("src/libsodium/include/sodium"),
                .unsafeFlags(["-w", "-fno-modules", "-fno-strict-aliasing", "-fno-strict-overflow", "-fwrapv"]),
            ]
        ),
        .target(
            name: "CSpake2EE",
            dependencies: ["CSodium"],
            exclude: ["LICENSE", "VENDORED.txt"],
            publicHeadersPath: "src",
            cSettings: [
                sodiumStatic,
                upstreamFlags,
            ]
        ),
        // crypto_spake.h uses size_t with no includes of its own, so Swift
        // reaches it through this header, which includes what it needs first.
        .target(
            name: "CSpake2EEShim",
            dependencies: ["CSodium", "CSpake2EE"],
            cSettings: [sodiumStatic]
        ),
        // The playback path's lock-free ring and shared numbers, which the
        // audio render thread reads without locking or allocating.
        .target(
            name: "CAudioRing"
        ),
        .target(
            name: "NereusMedia",
            dependencies: ["COpus", "COpusShim", "CDataChannel", "CAudioRing", "NereusLink"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "NereusMediaTests",
            dependencies: ["NereusMedia", "NereusLink", "COpus", "CJuice", "CJuiceCloseTestSupport", "CPeerLifetimeTestSupport", "LinkTestSupport", "LinkSessionTestSupport"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // The session to the Core: the link's messages, the TLS WebSocket,
        // versions, sign-in, the heartbeat and reconnecting.
        .target(
            name: "NereusLink",
            dependencies: ["CSodium", "CSpake2EE", "CSpake2EEShim"],
            resources: [.copy("Resources/pairing-words-v1.txt")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "NereusLinkTests",
            dependencies: ["NereusLink", "LinkTestSupport", "LinkSessionTestSupport"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // The Core's state in the app: the mirrored objects, the settings
        // proxy, commands and telemetry, over a session.
        .target(
            name: "NereusMirror",
            dependencies: ["NereusLink", "NereusModels"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "NereusMirrorTests",
            dependencies: ["NereusMirror", "NereusModels", "NereusLink", "LinkTestSupport", "LinkSessionTestSupport"],
            resources: [.copy("Fixtures/DiversityProducerV1"), .copy("Fixtures/TxProfileWatch60")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // The band: the spectrum trace, the waterfall, the band-plan strip
        // and the dBm scale, drawn in Metal. The shaders ship as source and
        // compile at run time, so building needs no Metal toolchain.
        .target(
            name: "NereusBand",
            dependencies: ["NereusModels", "NereusMedia"],
            resources: [.copy("Shaders")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "NereusBandTests",
            dependencies: ["NereusBand", "NereusModels", "NereusMedia", "NereusLink", "LinkTestSupport"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // A fake Core for the app's tests: it plays a session fixture's
        // station side, then stays up and records what the app sends.
        // Never a dependency of the app.
        .target(
            name: "NereusKitTesting",
            dependencies: ["NereusLink", "NereusMedia", "LinkTestSupport", "LinkSessionTestSupport"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "NereusKitTestingTests",
            dependencies: ["NereusKitTesting", "NereusLink", "NereusMirror", "NereusMedia", "LinkTestSupport",
                           "LinkSessionTestSupport"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // Test support shared by the test targets: finds and reads the link's
        // conformance suite in the checkout. Never a dependency of the app.
        .target(
            name: "LinkTestSupport",
            path: "Tests/LinkTestSupport",
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        // Test support for the session's tests and the layers above it: a
        // scripted Core, a manual clock, an event recorder and the session
        // fixtures' runner. Never a dependency of the app.
        .target(
            name: "LinkSessionTestSupport",
            dependencies: ["NereusLink", "LinkTestSupport"],
            path: "Tests/LinkSessionTestSupport",
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ],
    cxxLanguageStandard: .cxx17
)
