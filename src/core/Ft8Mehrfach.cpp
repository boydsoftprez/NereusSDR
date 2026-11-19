// =================================================================
// src/core/Ft8Mehrfach.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8Mehrfach.h fuer das Modell und die MSHV-Herkunft.
//
// Modification history (Longpath):
//   2026-10-24 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-17 — nimmVonHand(), raeumeAuf(); das Nachruecken waehlt den
//                 ersten Zug nach der Herkunft. Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Mehrfach.h"

#include <algorithm>

namespace Longpath {

void Ft8Mehrfach::setEigeneDaten(const QString& meinRuf,
                                 const QString& meinLocator)
{
    m_meinRuf     = meinRuf.trimmed().toUpper();
    m_meinLocator = meinLocator.trimmed().toUpper();
}

void Ft8Mehrfach::setPlaetze(int n)
{
    m_plaetze = std::clamp(n, 1, kMaxPlaetze);
}

void Ft8Mehrfach::setGrundtonHz(float hz)
{
    m_grundtonHz = hz;
}

bool Ft8Mehrfach::kennt(const QString& partner) const
{
    const QString p = partner.trimmed().toUpper();
    for (const Ft8Ablauf& a : m_laufend) {
        if (a.partner().toUpper() == p) { return true; }
    }
    for (const Wartend& w : m_warteschlange) {
        if (w.partner.toUpper() == p) { return true; }
    }
    return false;
}

int Ft8Mehrfach::aktive() const
{
    int n = 0;
    for (const Ft8Ablauf& a : m_laufend) {
        if (a.laeuft()) { ++n; }
    }
    return n;
}

bool Ft8Mehrfach::nimmAnrufer(const QString& partner, int snr,
                              const QString& seinLocator)
{
    return nimm(partner, snr, seinLocator, false);
}

bool Ft8Mehrfach::nimmVonHand(const QString& partner, int snr,
                              const QString& seinLocator)
{
    return nimm(partner, snr, seinLocator, true);
}

// ── Der erste Zug haengt daran, wer angefangen hat ───────────────────
//
// Er hat MICH gerufen: es beginnt bei Tx2, mit dem Rapport.
// Ich rufe IHN (Doppelklick auf sein CQ, „+ Queue"): es beginnt bei
// Tx1, mit meinem Locator.
//
// An EINER Stelle, weil es zwei Wege auf einen Platz gibt -- sofort
// und aus der Schlange -- und beide dasselbe tun muessen. Bis zum
// 2026-11-17 taten sie es nicht: `nimmAnrufer` nahm fuer den freien
// Platz `starteMitAnrufer`, `rueckeNach` fuer denselben Anrufer aus
// der Schlange `starteAlsAntworter`. Wer warten musste, bekam also
// meinen Locator statt seines Rapports und rief weiter.
Ft8Ablauf Ft8Mehrfach::beginne(const Wartend& w, const QString& meinRuf,
                               const QString& meinLocator)
{
    Ft8Ablauf a;
    if (w.vonHand) {
        a.starteAlsAntworter(w.partner, meinRuf, meinLocator,
                             w.snr, w.locator);
    } else {
        // Hier stand bis zum 2026-10-30 `starteAlsAntworter`, mit der
        // Begruendung, das sei der Einstieg bei Tx2. War es nicht:
        // jenes setzt `Zustand::RufeAn` (Tx1). Siehe
        // `Ft8Ablauf::starteMitAnrufer`.
        a.starteMitAnrufer(w.partner, meinRuf, meinLocator,
                           w.snr, w.locator);
    }
    return a;
}

bool Ft8Mehrfach::nimm(const QString& partner, int snr,
                       const QString& seinLocator, bool vonHand)
{
    const QString p = partner.trimmed().toUpper();
    if (p.isEmpty() || m_meinRuf.isEmpty()) { return false; }

    // Doppelte abweisen. Ohne das antwortete man demselben Rufzeichen
    // im selben Fenster zweimal, auf zwei Toenen -- und zaehlte es am
    // Ende als zwei QSOs.
    if (kennt(p)) { return false; }

    const Wartend w{p, snr, seinLocator.trimmed().toUpper(), vonHand};
    if (aktive() < m_plaetze) {
        m_laufend.push_back(beginne(w, m_meinRuf, m_meinLocator));
        m_laufend.back().setzeAnrufWiederholungen(m_anrufWiederholungen);
        return true;
    }

    m_warteschlange.push_back(w);
    return true;
}

int Ft8Mehrfach::verarbeite(const QString& nachricht, int snr)
{
    int vorangekommen = 0;
    for (Ft8Ablauf& a : m_laufend) {
        if (!a.laeuft()) { continue; }
        if (a.verarbeite(nachricht, snr)) { ++vorangekommen; }
    }
    return vorangekommen;
}

QVector<float> Ft8Mehrfach::tonleiter(int anzahl) const
{
    // Passt der volle Stapel unter die Grenze, bleibt es bei MSHV.
    const float oberster = m_grundtonHz
        + Ft8TxSource::kPlatzAbstandHz * static_cast<float>(m_plaetze - 1);
    if (m_plaetze <= 1 || oberster <= kPlatzObergrenzeHz) {
        return Ft8TxSource::tonleiter(m_grundtonHz, anzahl);
    }

    // Sonst nach unten, mit derselben Schrittweite und derselben
    // unteren Klammer wie die MSHV-Leiter.
    QVector<float> aus;
    if (anzahl <= 0) { return aus; }
    aus.reserve(anzahl);
    for (int i = 0; i < anzahl; ++i) {
        float f0 = m_grundtonHz
                   - Ft8TxSource::kPlatzAbstandHz * static_cast<float>(i);
        if (f0 < Ft8TxSource::kMinTonHz) { f0 = Ft8TxSource::kMinTonHz; }
        if (f0 > Ft8TxSource::kMaxTonHz) { f0 = Ft8TxSource::kMaxTonHz; }
        aus.append(f0);
    }
    return aus;
}

QVector<Ft8TxSource::Platz> Ft8Mehrfach::naechsteNachrichten() const
{
    QVector<Ft8TxSource::Platz> raus;
    const QVector<float> toene = tonleiter(m_laufend.size());

    for (int i = 0; i < m_laufend.size(); ++i) {
        if (!m_laufend[i].laeuft()) { continue; }
        const QString text = m_laufend[i].naechsteNachricht();
        if (text.isEmpty()) { continue; }
        Ft8TxSource::Platz p;
        p.text   = text;
        p.toneHz = (i < toene.size()) ? toene[i] : m_grundtonHz;
        raus.push_back(p);
    }
    return raus;
}

void Ft8Mehrfach::fensterVerstrichen()
{
    for (Ft8Ablauf& a : m_laufend) {
        if (a.laeuft()) { a.fensterVerstrichen(); }
    }

    raeumeAuf();
}

void Ft8Mehrfach::raeumeAuf()
{
    // Fertige beiseitelegen, Abgebrochene fallen lassen -- beides gibt
    // den Platz frei.
    QVector<Ft8Ablauf> bleibt;
    bleibt.reserve(m_laufend.size());
    for (const Ft8Ablauf& a : m_laufend) {
        if (a.zustand() == Ft8Ablauf::Zustand::Fertig) {
            m_fertig.push_back(a);
            continue;
        }
        if (a.zustand() == Ft8Ablauf::Zustand::Abgebrochen) {
            continue;
        }
        bleibt.push_back(a);
    }
    m_laufend = bleibt;

    rueckeNach();
}

void Ft8Mehrfach::rueckeNach()
{
    while (!m_warteschlange.isEmpty() && aktive() < m_plaetze) {
        const Wartend w = m_warteschlange.takeFirst();
        m_laufend.push_back(beginne(w, m_meinRuf, m_meinLocator));
        m_laufend.back().setzeAnrufWiederholungen(m_anrufWiederholungen);
    }
}

QVector<Ft8Ablauf> Ft8Mehrfach::nimmFertige()
{
    QVector<Ft8Ablauf> raus = m_fertig;
    m_fertig.clear();
    return raus;
}

QVector<Ft8Mehrfach::Belegung> Ft8Mehrfach::belegung() const
{
    QVector<Belegung> raus;
    const QVector<float> toene = tonleiter(m_laufend.size());
    for (int i = 0; i < m_laufend.size(); ++i) {
        Belegung b;
        b.partner = m_laufend[i].partner();
        b.zustand = m_laufend[i].zustand();
        b.toneHz  = (i < toene.size()) ? toene[i] : m_grundtonHz;
        raus.push_back(b);
    }
    return raus;
}

void Ft8Mehrfach::abbrechen()
{
    for (Ft8Ablauf& a : m_laufend) { a.abbrechen(); }
    m_laufend.clear();
    m_warteschlange.clear();
}

}  // namespace Longpath
