// =================================================================
// src/core/audio/MikrofonWaechter.cpp  (Longpath)
// =================================================================
// Siehe MikrofonWaechter.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-20 — Angelegt. Longpath-eigen, keine Portierung.
//                KI-gestützt via Anthropic Claude Code.
// =================================================================

#include "core/audio/MikrofonWaechter.h"

namespace Longpath::Audio {

void MikrofonWaechter::setzeAus(Grund g, qint64 jetztMs)
{
    if (m_zustand != Zustand::Ausgefallen) {
        m_ausgefallenSeit = jetztMs;
    }
    m_zustand = Zustand::Ausgefallen;
    m_grund = g;
    m_regelmaessigSeit = -1;
}

MikrofonWaechter::Zustand MikrofonWaechter::pruefe(const Probe& p, qint64 jetztMs)
{
    // Mikrofon an der Maschine, DAX/VAX oder kein Ton: geht uns nichts an.
    if (!p.pcMikrofonGewaehlt || !p.tonLaeuft) {
        m_zustand = Zustand::Unbeteiligt;
        m_grund = Grund::Keiner;
        m_ausgefallenSeit = -1;
        m_offenOhneRueckrufSeit = -1;
        m_regelmaessigSeit = -1;
        return m_zustand;
    }

    // Gewählt, aber nichts offen: gesendet würde Stille.
    if (!p.eingangOffen) {
        m_offenOhneRueckrufSeit = -1;
        setzeAus(Grund::KeinGeraet, jetztMs);
        return m_zustand;
    }

    // Offen, aber noch nie ein Rückruf: dem Gerät die Anlaufzeit lassen,
    // gemessen ab dem ersten Takt, in dem wir es offen gesehen haben.
    qint64 lueckeMs = 0;
    if (p.nsSeitRueckruf < 0) {
        if (m_offenOhneRueckrufSeit < 0) {
            m_offenOhneRueckrufSeit = jetztMs;
        }
        lueckeMs = jetztMs - m_offenOhneRueckrufSeit;
    } else {
        m_offenOhneRueckrufSeit = -1;
        lueckeMs = p.nsSeitRueckruf / 1000000;
    }

    if (lueckeMs > kAusfallMs) {
        setzeAus(Grund::KeineRueckrufe, jetztMs);
        return m_zustand;
    }

    if (m_zustand != Zustand::Ausgefallen) {
        m_zustand = Zustand::Liefert;
        m_grund = Grund::Keiner;
        return m_zustand;
    }

    // Ausgefallen und gerade wieder ein Rückruf: erst nach drei Sekunden
    // ohne neue Lücke zurück auf „liefert".
    if (lueckeMs > kRegelmaessigMs || p.nsSeitRueckruf < 0) {
        m_regelmaessigSeit = -1;
        return m_zustand;
    }
    if (m_regelmaessigSeit < 0) {
        m_regelmaessigSeit = jetztMs;
    }
    if (jetztMs - m_regelmaessigSeit >= kErholtNachMs) {
        m_zustand = Zustand::Liefert;
        m_grund = Grund::Keiner;
        m_ausgefallenSeit = -1;
        m_regelmaessigSeit = -1;
    }
    return m_zustand;
}

}  // namespace Longpath::Audio
