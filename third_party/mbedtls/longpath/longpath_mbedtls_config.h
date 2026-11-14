/*
 * third_party/mbedtls/longpath/longpath_mbedtls_config.h  (Longpath)
 *
 * Longpath-eigene Konfiguration fuer die mbedTLS-Teilmenge in diesem
 * Ordner. Sie ersetzt die volle mbedtls_config.h (MBEDTLS_CONFIG_FILE)
 * und schaltet nur ein, was die Roundtable-Anmeldung braucht:
 * ECDSA ueber P-256 mit SHA-256, Signatur deterministisch nach RFC 6979.
 * Kein TLS, kein X.509, kein PSA, keine Zufallsquelle von mbedTLS selbst
 * (die kommt aus QRandomGenerator::system()).
 *
 * Modification history (Longpath):
 *   2026-11-14 — Angelegt fuer Longpath von Martin Fischer,
 *                KI-gestuetzt ueber Anthropic Claude.
 *   2026-11-14 — MBEDTLS_PLATFORM_C: unter MinGW verlangt check_config.h
 *                es (snprintf-Ersatz fuer Windows), die Windows-CI brach ab.
 *                Ueberall an, damit alle drei Systeme denselben Weg nehmen.
 */
#ifndef LONGPATH_MBEDTLS_CONFIG_H
#define LONGPATH_MBEDTLS_CONFIG_H

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_DETERMINISTIC
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PLATFORM_C

#endif /* LONGPATH_MBEDTLS_CONFIG_H */
