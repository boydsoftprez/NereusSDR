# App: Panadapter und Wasserfall in echter Auflösung (2026-11-21)

Martin: die Seite ist „sehr unscharf". Ursache: `#pan` (373 × 96) und `#wf`
(373 × 160) hatten feste Pixelgrößen und wurden auf die Anzeigebreite
gestreckt. Jetzt: Zeichenfläche = Anzeigegröße × `devicePixelRatio`.

| Blatt | |
| --- | --- |
| `iphone-393-vorher.png` / `iphone-393-nachher.png` | iPhone-Breite 393, doppelte Auflösung |
| `iphone-393-*-bildausschnitt.png` | nur Panadapter/Wasserfall |
| `desktop-1200-vorher.png` / `desktop-1200-nachher.png` | Fensterbreite 1200 (Vorschau 800 px) |

Daten und Raten unverändert (373 Punkte, 12 Bilder/s angefordert).
