# App: Desktop-Layout (Blatt, 2026-11-21)

Ein Blatt, 1400 × 900, nach Martins Foto von Mac-Longpath. Nur Daten, die die
Seite heute schon bekommt (TCI); Platzhalter sind weggelassen.

* **Pillenleiste oben:** Band, Betriebsart, Filter, Schritt, NR/NB/ANF/SQL.
* **Mitte:** Panadapter und Wasserfall groß (echte Auflösung).
* **Links:** S-Meter analog, SWR (zeigt einen Wert nur beim Senden), Log
  (letzte Verbindungen aus `log_last`, Knopf „QSO eintragen").
* **Rechts, „Combined":** Frequenz, Filterbreite, Schritt, AF und PWR, Rotor.
* **Unten:** Sendeleiste (SENDEN/TUNE als Haltetasten, ab Werk gesperrt) und
  Fußzeile mit echten Zuständen (gekoppelt, Bild kB/s, Ton, ON AIR).

Weggelassen, weil die Seite es nicht bekommt: RIT/XIT, SWR-Verlauf, TNF/MOX/VOX.
Die Schritt-Chips 10 Hz / 100 Hz / 1 kHz sind neu (heute nur 100 Hz ±).

## Umsetzung (Bilder der echten Seite)

`umsetzung-vorher-desktop.png` / `umsetzung-nachher-desktop.png` (1400 × 900) und
`umsetzung-vorher-iphone.png` / `umsetzung-nachher-iphone.png` (393 × 852, 2x).
Das iPhone-Bild ist vorher und nachher gleich: unter 900 px bleibt „Ein Schirm".
Ab 900 px sorgt `desktop.js` für das Raster; die Elemente und ihre
Ereignishörer bleiben dieselben.
