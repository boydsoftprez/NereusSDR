#pragma once

// =================================================================
// src/gui/meters/LedResultRegistry.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The process-wide table of LED results by 4-character
// ID, the stand-in for Thetis's _led_results dictionary that clsLed fills
// and clsTextOverlay reads to gate its lines.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created for the text overlay's LED gates. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QHash>
#include <QMutex>
#include <QString>

#include <optional>

namespace NereusSDR {

// Each live LED publishes isTrue() under its 4-character ID; a text
// overlay's RX / TX gate reads it. IDs carry the LED's guid, so LEDs in
// different windows do not collide. Preview and validation copies never
// publish. An LED that goes away clears its entry, so a stale result never
// gates an overlay.
class LedResultRegistry {
public:
    static LedResultRegistry& instance();

    // Publishes `value` under `id` (empty IDs are ignored).
    void set(const QString& id, bool value);
    // The last value published under `id`, or std::nullopt when no LED
    // holds that ID.
    std::optional<bool> get(const QString& id) const;

    // Registrant-aware forms used by LEDItem. `registrant` is the
    // publishing LED; it only identifies the entry and is never
    // dereferenced. remove() clears the entry only when `registrant` is the
    // one that published it last, so an LED destroyed after a replacement
    // with the same ID took over (an edited entry is rebuilt before the old
    // copy goes) leaves the replacement's result in place.
    void set(const QString& id, bool value, const void* registrant);
    void remove(const QString& id, const void* registrant);

    // Clears every entry (tests).
    void clear();

private:
    LedResultRegistry() = default;

    struct Entry {
        bool        value{false};
        const void* registrant{nullptr};
    };
    mutable QMutex         m_mutex;
    QHash<QString, Entry>  m_results;
};

} // namespace NereusSDR
