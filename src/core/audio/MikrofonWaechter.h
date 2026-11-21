#pragma once

// =================================================================
// src/core/audio/MikrofonWaechter.h  (Longpath)
// =================================================================
//
// Merkt, wenn das PC-Mikrofon nichts mehr liefert.
//
// Anlass, 2026-11-20 13:00:24: das USB-Mikrofon verschwand vom Mac.
// PortAudio 19.7 meldet das nicht — der Aufnahmestrom bleibt „offen",
// nur ruft CoreAudio den Rückruf nicht mehr auf. Longpath hing am weg
// gewesenen Gerät, jeder Zug aus dem Mikrofonring kam leer zurück, und
// eine SSB-Sendung wäre stumm hinausgegangen. Zu sehen war es nur im
// Protokoll („PC-mic pulls ran short 3797 of 3797 times").
//
// Das Merkmal ist die Zeit seit dem letzten Aufnahme-Rückruf
// (`IAudioBus::nsSinceLastCallback`). Gemessen am selben Tag:
//
//   Normalbetrieb   höchstens 2 ms Lücke (Puffer ~53 ms)
//   Ausfall         4512 ms und mehr
//
// Drei Zehnerpotenzen dazwischen. Die Schwelle liegt bei einer Sekunde:
// zwanzigmal der Puffer, und trotzdem binnen gut einer Sekunde sichtbar.
//
// Zurück auf „liefert" geht es erst, wenn die Rückrufe DREI SEKUNDEN AM
// STÜCK wieder regelmäßig kommen. Ein Gerät, das nur ab und zu noch
// einen Block schickt, soll die Warnung nicht flackern lassen.
//
// Reine Logik ohne Qt-Objekt und ohne Uhr: der Aufrufer gibt Messwert
// und Zeit herein. So prüft `tests/tst_mikrofon_waechter.cpp` jeden
// Ablauf ohne Tongerät und ohne Warten.
// =================================================================
// Modification history (Longpath):
//   2026-11-20 — Angelegt. Longpath-eigen, keine Portierung (Thetis
//                erkennt den Fall nicht, siehe ivac.c:208 [@852bf0e]).
//                KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtGlobal>

namespace Longpath::Audio {

class MikrofonWaechter
{
public:
    /// Länger als das ohne Rückruf → ausgefallen.
    static constexpr qint64 kAusfallMs = 1000;
    /// Lücke, die als „wieder regelmäßig" gilt.
    static constexpr qint64 kRegelmaessigMs = 250;
    /// So lange muss es am Stück regelmäßig laufen, bis die Warnung geht.
    static constexpr qint64 kErholtNachMs = 3000;

    enum class Zustand {
        Unbeteiligt,   ///< PC-Mikrofon nicht gewählt oder Ton läuft nicht
        Liefert,
        Ausgefallen,
    };

    enum class Grund {
        Keiner,
        KeinGeraet,     ///< PC-Mikrofon gewählt, aber kein Eingang offen
        KeineRueckrufe, ///< Eingang offen, aber CoreAudio ruft nicht mehr
    };

    struct Probe {
        bool pcMikrofonGewaehlt{false};
        bool tonLaeuft{false};
        bool eingangOffen{false};
        /// -1 = noch nie ein Rückruf.
        qint64 nsSeitRueckruf{-1};
    };

    /// Einmal je Takt aufrufen. Gibt den neuen Zustand zurück.
    Zustand pruefe(const Probe& p, qint64 jetztMs);

    Zustand zustand() const noexcept { return m_zustand; }
    Grund grund() const noexcept { return m_grund; }
    /// Seit wann (in `jetztMs`-Zeit) ausgefallen; -1 sonst.
    qint64 ausgefallenSeitMs() const noexcept { return m_ausgefallenSeit; }

private:
    void setzeAus(Grund g, qint64 jetztMs);

    Zustand m_zustand{Zustand::Unbeteiligt};
    Grund m_grund{Grund::Keiner};
    qint64 m_ausgefallenSeit{-1};
    /// Seit wann der Eingang ohne jeden Rückruf offen steht.
    qint64 m_offenOhneRueckrufSeit{-1};
    /// Seit wann die Rückrufe wieder regelmäßig kommen.
    qint64 m_regelmaessigSeit{-1};
};

}  // namespace Longpath::Audio
