#pragma once

// =================================================================
// src/core/AnvelinaDxOutputs.h  (NereusSDR)
// =================================================================
//
// The four Anvelina PRO3 DX open collector outputs (USEROUT7 .. USEROUT10),
// their on/off state and a name for each, saved per radio.
//
// Independently implemented from "Specification: Open_Collector_Anvelina_DX
// for Thetis" (Yuri EU2AV, 2026-05-21) and the Anvelina PRO3 gateware
// (n1gp-Anvelina_PROIII Orion.v:920-923 [@8e86a61]). No upstream code is
// ported here: upstream Thetis carries the wire byte only (the codecs cite
// it), with no state, names or controls behind it.
//
// no-port-check: NereusSDR-original model. Comments name the spec and the
// gateware as the source of the bit layout; the Thetis wire byte is ported
// in P2CodecOrionMkII.cpp and P1CodecAnvelinaPro3.cpp.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created: DX OUT 7..10 state, names and wire byte, per MAC
//                under hardware/<mac>/oc/dx/ (J.J. Boyd KG4VCF,
//                AI-assisted: Claude Code).
// =================================================================

#include "core/NereusCoreExport.h"

#include <QObject>
#include <QString>

#include <array>
#include <atomic>

namespace NereusSDR {

// The DX outputs are their own state, separate from the band OC byte
// (OcMatrix): nothing switches them by band or by transmit. The radio
// drives each output while it runs (Orion.v: USEROUTn = run ? bit : 0), so
// the byte goes out unconditionally and the radio does the gating.
//
// Wire byte (Spec, page 2, "Structure of Byte 1397"):
//   bit 1 -> USEROUT7, bit 2 -> USEROUT8, bit 3 -> USEROUT9,
//   bit 4 -> USEROUT10; bit 0 (internal) and bits 7:5 always 0.
//
// Threads: wireByte() may be read from any thread (the connection reads it
// while composing); everything else is the main thread's.
class NEREUS_CORE_EXPORT AnvelinaDxOutputs : public QObject {
    Q_OBJECT

public:
    // The outputs as the radio's panel names them: DX OUT 7 .. DX OUT 10.
    static constexpr int kFirstOutput = 7;
    static constexpr int kLastOutput  = 10;
    static constexpr int kOutputCount = kLastOutput - kFirstOutput + 1;

    explicit AnvelinaDxOutputs(QObject* parent = nullptr);

    // out is 7..10; any other value reads false / empty and writes nothing.
    bool outputOn(int out) const;
    void setOutputOn(int out, bool on);
    // All ON / All OFF.
    void setAll(bool on);

    // The operator's name for an output (e.g. "ANT Switch"), empty by default.
    QString label(int out) const;
    void setLabel(int out, const QString& label);

    // The byte for P2 high-priority byte 1397 and P1 bank 17 C1, masked with
    // kDxWireMask (codec/CodecContext.h).
    quint8 wireByte() const;

    // ── Persistence: hardware/<mac>/oc/dx/out<N>/{enabled,label} ──────────
    void setMacAddress(const QString& mac);
    void load();   // hydrate from AppSettings; emits changed()
    void save();   // write the keys whose saved value differs

signals:
    void changed();  // fires on any state change; UI re-reads

private:
    static bool validOutput(int out) noexcept
    {
        return out >= kFirstOutput && out <= kLastOutput;
    }
    static quint8 bitFor(int out) noexcept
    {
        return static_cast<quint8>(1U << (out - kFirstOutput + 1));
    }
    QString keyFor(int out, const char* field) const;

    // The outputs as wire bits (bit 1 = DX OUT 7 .. bit 4 = DX OUT 10).
    std::atomic<quint8> m_bits{0};
    std::array<QString, kOutputCount> m_labels{};
    QString m_mac;
};

} // namespace NereusSDR
