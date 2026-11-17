// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/Ft8Warteschlange.cpp  (Longpath)
// =================================================================
// Siehe Ft8Warteschlange.h fuer die beiden Listen und die Messung,
// die sie noetig gemacht hat.
// =================================================================

#include "core/Ft8Warteschlange.h"

namespace Longpath {

bool Ft8Warteschlange::merkeAnruf(const QString& partner, int snr,
                                  const QString& locator, qint64 jetztMs,
                                  int tonHz)
{
    const QString k = schluessel(partner);
    if (k.isEmpty()) { return false; }

    // Wer gerade gearbeitet wurde, stellt sich nicht wieder an.
    if (inKarenz(k, jetztMs)) { return false; }
    // Und wen der Betreiber gestrichen hat, auch nicht -- eine Weile.
    if (abgewiesen(k, jetztMs)) { return false; }

    for (Wartend& w : m_schlange) {
        if (schluessel(w.partner) == k) {
            // Schon in der Schlange: Rapport und Zeit auffrischen,
            // aber die Reihenfolge nicht anfassen. Wer dreimal ruft,
            // draengt sich nicht vor -- sonst gewaenne der
            // Hartnaeckigste, nicht der Erste.
            w.snr    = snr;
            w.seitMs = jetztMs;
            if (!locator.isEmpty()) { w.locator = locator; }
            if (tonHz >= 0)         { w.tonHz = tonHz; }
            // Von Hand angestellt, und jetzt ruft er wirklich: dann
            // beginnt es mit dem Rapport, wie bei jedem Anrufer.
            w.vonHand = false;
            return false;
        }
    }

    Wartend neu;
    neu.partner = partner;
    neu.snr     = snr;
    neu.locator = locator;
    neu.seitMs  = jetztMs;
    neu.tonHz   = tonHz;
    m_schlange.append(neu);

    while (m_schlange.size() > kMaxWartende) { m_schlange.removeFirst(); }
    return true;
}

void Ft8Warteschlange::merkeGearbeitet(const QString& partner, qint64 jetztMs)
{
    const QString k = schluessel(partner);
    if (k.isEmpty()) { return; }
    m_gearbeitet.insert(k, jetztMs);
    vergiss(partner);
}

bool Ft8Warteschlange::inKarenz(const QString& partner, qint64 jetztMs) const
{
    const auto it = m_gearbeitet.constFind(schluessel(partner));
    if (it == m_gearbeitet.constEnd()) { return false; }
    return (jetztMs - it.value()) < kKarenzMs;
}

Ft8Warteschlange::Wartend Ft8Warteschlange::nimmNaechsten(qint64 jetztMs)
{
    while (!m_schlange.isEmpty()) {
        const Wartend w = m_schlange.first();
        m_schlange.removeFirst();
        if ((jetztMs - w.seitMs) >= kHaltbarkeitMs) { continue; }
        if (inKarenz(w.partner, jetztMs))           { continue; }
        return w;
    }
    return Wartend{};
}

int Ft8Warteschlange::wartende(qint64 jetztMs) const
{
    int n = 0;
    for (const Wartend& w : m_schlange) {
        if ((jetztMs - w.seitMs) >= kHaltbarkeitMs) { continue; }
        if (inKarenz(w.partner, jetztMs))           { continue; }
        ++n;
    }
    return n;
}

void Ft8Warteschlange::vergiss(const QString& partner)
{
    const QString k = schluessel(partner);
    for (int i = m_schlange.size() - 1; i >= 0; --i) {
        if (schluessel(m_schlange.at(i).partner) == k) {
            m_schlange.removeAt(i);
        }
    }
}

void Ft8Warteschlange::leeren()
{
    m_schlange.clear();
    m_gearbeitet.clear();
    m_abgewiesen.clear();
}

bool Ft8Warteschlange::gilt(const Wartend& w, qint64 jetztMs) const
{
    return (jetztMs - w.seitMs) < kHaltbarkeitMs
        && !inKarenz(w.partner, jetztMs);
}

void Ft8Warteschlange::raeume(qint64 jetztMs)
{
    for (int i = m_schlange.size() - 1; i >= 0; --i) {
        if (!gilt(m_schlange.at(i), jetztMs)) { m_schlange.removeAt(i); }
    }
}

int Ft8Warteschlange::stelle(const QString& partner) const
{
    const QString k = schluessel(partner);
    for (int i = 0; i < m_schlange.size(); ++i) {
        if (schluessel(m_schlange.at(i).partner) == k) { return i; }
    }
    return -1;
}

bool Ft8Warteschlange::stelleAn(const QString& partner, int snr,
                                const QString& locator, int tonHz,
                                qint64 jetztMs, bool vonHand)
{
    const QString k = schluessel(partner);
    if (k.isEmpty()) { return false; }
    if (inKarenz(k, jetztMs)) { return false; }
    raeume(jetztMs);
    if (stelle(k) >= 0) { return false; }

    // Wer von Hand dazugenommen wird, ist nicht mehr gestrichen.
    m_abgewiesen.remove(k);

    Wartend neu;
    neu.partner = partner;
    neu.snr     = snr;
    neu.locator = locator;
    neu.seitMs  = jetztMs;
    neu.tonHz   = tonHz;
    neu.vonHand = vonHand;
    m_schlange.append(neu);
    while (m_schlange.size() > kMaxWartende) { m_schlange.removeFirst(); }
    return true;
}

bool Ft8Warteschlange::ruecke(const QString& partner, int richtung,
                              qint64 jetztMs)
{
    raeume(jetztMs);
    const int i = stelle(partner);
    if (i < 0 || richtung == 0) { return false; }
    const int ziel = richtung < 0 ? i - 1 : i + 1;
    if (ziel < 0 || ziel >= m_schlange.size()) { return false; }
    m_schlange.swapItemsAt(i, ziel);
    return true;
}

bool Ft8Warteschlange::nachVorn(const QString& partner, qint64 jetztMs)
{
    raeume(jetztMs);
    const int i = stelle(partner);
    if (i <= 0) { return false; }
    m_schlange.move(i, 0);
    return true;
}

void Ft8Warteschlange::weiseAb(const QString& partner, qint64 jetztMs)
{
    const QString k = schluessel(partner);
    if (k.isEmpty()) { return; }
    m_abgewiesen.insert(k, jetztMs);
    vergiss(partner);
}

bool Ft8Warteschlange::abgewiesen(const QString& partner, qint64 jetztMs) const
{
    const auto it = m_abgewiesen.constFind(schluessel(partner));
    if (it == m_abgewiesen.constEnd()) { return false; }
    return (jetztMs - it.value()) < kAbweisungMs;
}

QVector<Ft8Warteschlange::Wartend> Ft8Warteschlange::gueltige(qint64 jetztMs) const
{
    QVector<Wartend> aus;
    for (const Wartend& w : m_schlange) {
        if (gilt(w, jetztMs)) { aus.append(w); }
    }
    return aus;
}

}  // namespace Longpath
