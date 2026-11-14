# Mbed TLS in Longpath

Eine Teilmenge von Mbed TLS 3.6.7: genau das, was die Roundtable-Anmeldung
braucht — ECDSA über P-256 mit SHA-256 (RFC 6979, deterministisch).

- `include/`, `library/`: Byte für Byte aus dem offiziellen Archiv, siehe `VERSION`.
  Nicht verändern; bei einem Wechsel der Fassung neu kopieren.
- `longpath/longpath_mbedtls_config.h`: unsere Konfiguration (ersetzt `mbedtls_config.h`).
- `CMakeLists.txt`: das Ziel `longpath_mbedcrypto`.

Benutzt nur von `src/core/roundtable/RoundtableKey.{h,cpp}`.
Herkunft und Dateiliste: `docs/attribution/MBEDTLS-PROVENANCE.md`.
