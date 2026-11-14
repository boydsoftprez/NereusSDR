# Mbed TLS 3.6.7 -- nur ECDSA P-256 + SHA-256, fuer die Roundtable-Anmeldung
# (src/core/roundtable/RoundtableKey). Lizenz: Apache-2.0 OR GPL-2.0-or-later
# (LICENSE in diesem Ordner); Longpath nimmt sie unter Apache-2.0, das zu
# GPLv3 passt.
#
# Die Dateien unter include/ und library/ sind Byte fuer Byte aus dem
# offiziellen Archiv mbedtls-3.6.7.tar.bz2 (SHA-256 a7e8bcbe...d11f6),
# ueber die Kopie, die das Upstream NereusSDR fuer seine iPhone-App
# fuehrt (ios/NereusKit/Sources/CMbedTLS @ 9036ffc04). Nur die dreizehn
# Uebersetzungseinheiten und 36 Kopfdateien, die die Konfiguration in
# longpath/ wirklich braucht. Siehe README-LONGPATH.md und
# docs/attribution/MBEDTLS-PROVENANCE.md.
#
# STATIC (nicht OBJECT): Pruefstaende linken LongpathObjs, und eine
# OBJECT-Bibliothek nimmt die Objekte einer zweiten nicht mit.

add_library(longpath_mbedcrypto STATIC
    library/asn1parse.c
    library/asn1write.c
    library/bignum.c
    library/bignum_core.c
    library/constant_time.c
    library/ecdsa.c
    library/ecp.c
    library/ecp_curves.c
    library/hmac_drbg.c
    library/md.c
    library/platform.c
    library/platform_util.c
    library/sha256.c
)
target_include_directories(longpath_mbedcrypto
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/longpath
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library
)
target_compile_definitions(longpath_mbedcrypto
    PUBLIC MBEDTLS_CONFIG_FILE="longpath_mbedtls_config.h")
set_target_properties(longpath_mbedcrypto PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 99
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF)

# Fremder Code, fremde Warnungen: aus dem Longpath-Baulog heraushalten.
if(MSVC)
    target_compile_options(longpath_mbedcrypto PRIVATE /w)
else()
    target_compile_options(longpath_mbedcrypto PRIVATE -w)
endif()
