# Mbed TLS — Longpath-Herkunftsnachweis

Eine Teilmenge von Mbed TLS unter `third_party/mbedtls/`, nur für die
Roundtable-Anmeldung (`src/core/roundtable/RoundtableKey.{h,cpp}`):
ECDSA über P-256 mit SHA-256, deterministisch nach RFC 6979.

Longpath steht unter GPLv3 (Wurzel-`LICENSE`). Mbed TLS ist
**Apache-2.0 OR GPL-2.0-or-later**; Longpath nimmt es unter Apache-2.0,
das mit GPLv3 verträglich ist. Der Lizenztext liegt unverändert in
`third_party/mbedtls/LICENSE`.

## Herkunft

- **Projekt:** Mbed TLS, https://github.com/Mbed-TLS/mbedtls
- **Fassung:** 3.6.7 (`MBEDTLS_VERSION_STRING "3.6.7"`)
- **Archiv:** `mbedtls-3.6.7.tar.bz2`, SHA-256
  `a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`
- **Weg zu uns:** über die Kopie, die das Upstream NereusSDR für seine
  iPhone-App führt (`ios/NereusKit/Sources/CMbedTLS` @ 9036ffc04, dort
  „272 files, copied unchanged from the archive", `VENDORED.txt`). Es
  wurde nichts aus dem Netz geholt; die Dateien kamen aus dem
  Upstream-Baum, den wir ohnehin führen.
- **Übernommen:** 2026-11-14

## Was übernommen wurde

Byte für Byte, geprüft mit `cmp` gegen die Upstream-Kopie:

- `LICENSE`
- `library/`: `asn1parse.c`, `asn1write.c`, `bignum.c`, `bignum_core.c`,
  `constant_time.c`, `ecdsa.c`, `ecp.c`, `ecp_curves.c`, `hmac_drbg.c`,
  `md.c`, `platform.c`, `platform_util.c`, `sha256.c` und die 12 internen
  Kopfdateien, die sie einbinden
- `include/mbedtls/`: die 24 Kopfdateien, die diese Teile einbinden

Ausgewählt mit `clang -MM` unter der Longpath-Konfiguration: genau die
Dateien, die übersetzt werden, und keine weiteren.

## Longpath-eigene Dateien

| Pfad | Zweck |
| --- | --- |
| `third_party/mbedtls/longpath/longpath_mbedtls_config.h` | Konfiguration (ersetzt `mbedtls_config.h` über `MBEDTLS_CONFIG_FILE`) |
| `third_party/mbedtls/CMakeLists.txt` | statische Bibliothek `longpath_mbedcrypto`, fremde Warnungen still |
| `third_party/mbedtls/VERSION` | Fassung, Archiv, Prüfsumme, Weg |
| `third_party/mbedtls/README-LONGPATH.md` | Verweis auf dieses Blatt |

## Örtliche Änderungen

Keine. Wer eine neuere Fassung will, kopiert dieselben Dateien neu aus dem
Archiv und fährt `tst_roundtable_key` und `tst_roundtable_wire`; beide
prüfen gegen Upstreams Konformitätsdaten, die OpenSSL erzeugt hat.
