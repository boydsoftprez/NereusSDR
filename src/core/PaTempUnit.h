// =================================================================
// src/core/PaTempUnit.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.  PA temperature unit selection
// (°C / °F) is not a Thetis behaviour — Thetis hard-codes the °C
// format string at console.cs:26760 [v2.10.3.13-beta2 @c26a8a4]
// (`String.Format("{0:#0.0}C", _MKIIHL2Temp)`).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-08 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Added alongside the HL2 PA-temperature surface in the
//                 status-bar bottom banner.  The toggle widget lives on
//                 the banner; PaValuesPage and RadioStatusPage subscribe
//                 to PaTempUnitNotifier::unitChanged so a single click
//                 reformats every surface app-wide.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QString>

namespace NereusSDR {

enum class PaTempUnit {
    Celsius,
    Fahrenheit,
};

// PaTempUnitNotifier — singleton QObject whose `unitChanged` signal fires
// whenever the user flips the PA-temperature unit preference.
//
// The current value lives in AppSettings under the key "PaTempUnit"
// ("C" / "F"); this class is the read/write/notify gateway around it.
// Surfaces that display PA temperature subscribe to `unitChanged` and
// re-format their labels.
//
// The singleton is created lazily on first instance() call and lives
// for the application lifetime (via Q_GLOBAL_STATIC).
class NEREUS_CORE_EXPORT PaTempUnitNotifier : public QObject {
    Q_OBJECT

public:
    static PaTempUnitNotifier& instance();

    // Read the persisted unit preference.  Defaults to Celsius when the
    // AppSettings key is missing or holds an unrecognised string.
    static PaTempUnit currentUnit();

    // Persist `unit` to AppSettings and emit `unitChanged(unit)` if the
    // value actually changed.  Idempotent: writing the same value again
    // emits nothing.
    static void setUnit(PaTempUnit unit);

    // Format a temperature value (always passed in degrees Celsius —
    // RadioStatus is the canonical °C source) to a user-facing string
    // using the current unit pref, with one decimal of precision.
    //
    // Examples (current = Celsius):       "25.0°C"
    // Examples (current = Fahrenheit):    "77.0°F"
    static QString format(double celsius);

    // Convert °C → °F using the standard formula F = C * 9/5 + 32.
    // Exposed for tests and for any callsite that needs the numeric
    // value in the user's preferred unit before formatting (e.g. a
    // chart axis label).
    static double celsiusToFahrenheit(double celsius) noexcept;

    // Constructor must be public for Q_GLOBAL_STATIC; instance() is the
    // intended access path so direct construction outside the global
    // static is a logic error (caller will get a fresh, unsignalled
    // QObject).  Q_DISABLE_COPY forbids the easy mistakes.
    PaTempUnitNotifier();
    ~PaTempUnitNotifier() override = default;
    Q_DISABLE_COPY(PaTempUnitNotifier)

signals:
    // Emitted when setUnit() changes the persisted value.  Surfaces
    // displaying PA temperature should reformat on receipt.
    void unitChanged(NereusSDR::PaTempUnit unit);
};

}  // namespace NereusSDR
