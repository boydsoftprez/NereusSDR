// =================================================================
// src/core/AnvelinaDxOutputs.cpp  (NereusSDR)
// =================================================================
//
// Independently implemented from "Specification: Open_Collector_Anvelina_DX
// for Thetis" (Yuri EU2AV, 2026-05-21) and the Anvelina PRO3 gateware
// (n1gp-Anvelina_PROIII Orion.v:920-923 [@8e86a61]).
//
// no-port-check: NereusSDR-original model; see AnvelinaDxOutputs.h.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created: DX OUT 7..10 state, names and wire byte, per MAC
//                under hardware/<mac>/oc/dx/ (J.J. Boyd KG4VCF,
//                AI-assisted: Claude Code).
// =================================================================

#include "AnvelinaDxOutputs.h"

#include "AppSettings.h"
#include "codec/CodecContext.h"

namespace NereusSDR {

AnvelinaDxOutputs::AnvelinaDxOutputs(QObject* parent) : QObject(parent) {}

bool AnvelinaDxOutputs::outputOn(int out) const
{
    if (!validOutput(out)) {
        return false;
    }
    return (m_bits.load() & bitFor(out)) != 0;
}

void AnvelinaDxOutputs::setOutputOn(int out, bool on)
{
    if (!validOutput(out)) {
        return;
    }
    const quint8 before = m_bits.load();
    const quint8 after = on ? quint8(before | bitFor(out)) : quint8(before & ~bitFor(out));
    if (after == before) {
        return;
    }
    m_bits.store(after);
    save();
    emit changed();
}

void AnvelinaDxOutputs::setAll(bool on)
{
    const quint8 after = on ? kDxWireMask : quint8(0);
    if (m_bits.load() == after) {
        return;
    }
    m_bits.store(after);
    save();
    emit changed();
}

QString AnvelinaDxOutputs::label(int out) const
{
    if (!validOutput(out)) {
        return {};
    }
    return m_labels[static_cast<size_t>(out - kFirstOutput)];
}

void AnvelinaDxOutputs::setLabel(int out, const QString& label)
{
    if (!validOutput(out)) {
        return;
    }
    QString& slot = m_labels[static_cast<size_t>(out - kFirstOutput)];
    if (slot == label) {
        return;
    }
    slot = label;
    save();
    emit changed();
}

quint8 AnvelinaDxOutputs::wireByte() const
{
    return static_cast<quint8>(m_bits.load() & kDxWireMask);
}

void AnvelinaDxOutputs::setMacAddress(const QString& mac)
{
    m_mac = mac;
}

QString AnvelinaDxOutputs::keyFor(int out, const char* field) const
{
    return QStringLiteral("hardware/%1/oc/dx/out%2/%3")
        .arg(m_mac)
        .arg(out)
        .arg(QLatin1String(field));
}

void AnvelinaDxOutputs::load()
{
    if (m_mac.isEmpty()) {
        return;
    }
    auto& s = AppSettings::instance();
    quint8 bits = 0;
    for (int out = kFirstOutput; out <= kLastOutput; ++out) {
        if (s.value(keyFor(out, "enabled"), QStringLiteral("False")).toString()
            == QStringLiteral("True")) {
            bits = quint8(bits | bitFor(out));
        }
        m_labels[static_cast<size_t>(out - kFirstOutput)] =
            s.value(keyFor(out, "label"), QString()).toString();
    }
    m_bits.store(bits);
    emit changed();
}

void AnvelinaDxOutputs::save()
{
    if (m_mac.isEmpty()) {
        return;
    }
    auto& s = AppSettings::instance();
    // Only the keys whose saved value differs, read with load()'s own
    // defaults (as OcMatrix::save does), so one click in a remote window is
    // one write to the Core.
    const auto store = [&s](const QString& key, const QString& value, const QString& fallback) {
        if (s.value(key, fallback).toString() != value) {
            s.setValue(key, value);
        }
    };
    const QString kTrue = QStringLiteral("True");
    const QString kFalse = QStringLiteral("False");
    for (int out = kFirstOutput; out <= kLastOutput; ++out) {
        store(keyFor(out, "enabled"), outputOn(out) ? kTrue : kFalse, kFalse);
        store(keyFor(out, "label"), m_labels[static_cast<size_t>(out - kFirstOutput)], QString());
    }
}

} // namespace NereusSDR
