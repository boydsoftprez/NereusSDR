# NereusSDR Roundtable — Longpath-Herkunftsnachweis

Dieses Blatt verzeichnet, was Longpath aus NereusSDRs Roundtable-Arbeit
übernommen hat, und in welcher Form. Vorbild ist
`NEREUS-RENDEZVOUS-PROVENANCE.md`.

Longpath **ist** ein Fork von NereusSDR (`boydsoftprez/NereusSDR`, © J.J.
Boyd, KG4VCF), siehe `CLAUDE.md`. Diese Übernahme ist darum kein Port aus
einem fremden Projekt, sondern das Nachziehen aus unserem eigenen
Upstream — dieselbe Lizenz (GPLv3). Laut Upstreams Entwurf
(`docs/architecture/2026-10-23-roundtable-design.md`) ist Roundtable
NereusSDR-eigener Code, ohne Thetis- oder AetherSDR-Anteile.

## Was übernommen wurde

| Pfad | Stand | Form |
| --- | --- | --- |
| `roundtable/README.md`, `roundtable/conformance/`, `roundtable/server/`, `roundtable/tests/` (292 Dateien: Referenzserver, seine Tests, die Konformitätssuite v1) | `upstream/main` @ 9036ffc04 | **unverändert, Byte für Byte** |
| `src/core/roundtable/RoundtableWire.{h,cpp}` | `upstream/main` @ 9036ffc04 | übernommen; Namensraum `Longpath`, kein Export-Makro, Longpath-Kopf (Upstream-Verlauf wörtlich behalten) |
| `tests/tst_roundtable_wire.cpp` | `upstream/main` @ 9036ffc04 | übernommen; Signieren/Prüfen über `RoundtableKey` statt `StationIdentity` |

Nicht übernommen: `roundtable/deploy/`, `roundtable/deploy.sh` (der Betrieb
von JJs öffentlichem Server ist nicht unsere Sache).

Longpath-eigen (kein Upstream-Gegenstück in dieser Form):
`src/core/roundtable/RoundtableKey.{h,cpp}` und `tests/tst_roundtable_key.cpp`.
Upstream signiert mit OpenSSL (`src/core/security/StationIdentity.cpp`);
Longpath hat kein OpenSSL und rechnet mit einer Teilmenge von Mbed TLS,
siehe `MBEDTLS-PROVENANCE.md`.

## Warum die Protokollnamen bleiben

Roundtable ist ein **Protokoll**. Seine Transkripte tragen feste Präfixe
(`"NereusSDR roundtable hello v1\n"`, `"NereusSDR roundtable id v1\n"`),
die in jede Signatur und jede Stations-Id eingerechnet sind. Wer sie
umbenennt, spricht ein anderes Protokoll und kann sich bei keinem
Roundtable-Server mehr anmelden. Sie sind darum keine Verweise auf den
Programmnamen im Sinne von `CLAUDE.md`. Nach außen meldet Longpath
`software: "Longpath"` und seine eigene Fassung.

## Wie aktualisiert wird

```
git fetch upstream
git checkout upstream/main -- roundtable/README.md roundtable/conformance roundtable/server roundtable/tests
python3 -m pytest roundtable/tests -q -p no:cacheprovider
ctest --test-dir build -R '^tst_roundtable_' --output-on-failure
```

Schlägt `tst_roundtable_wire` danach fehl, hat sich das Protokoll
geändert: zuerst `docs/architecture/2026-10-23-roundtable-v1.md` im
Upstream lesen, dann `RoundtableWire` nachziehen.

## Lizenz

NereusSDR und Longpath stehen beide unter GPLv3 (Wurzel-`LICENSE`). Die
Verlaufsvermerke in den übernommenen Dateien bleiben wörtlich stehen.
