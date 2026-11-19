// =================================================================
// src/core/FlexLauschscheibe.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigen, siehe Kopf.

#include "core/FlexLauschscheibe.h"

#include "core/FlexRadioConnection.h"
#include "core/LogCategories.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace Longpath {

FlexLauschscheibe::FlexLauschscheibe(Sende sende, QObject* parent)
    : QObject(parent)
    , m_sende(std::move(sende))
{
}

// ── Zugaenge je Platz ────────────────────────────────────────────────

FlexLauschscheibe::Zustand FlexLauschscheibe::zustand(int platz) const
{
    return gueltig(platz) ? m_platz[platz].zustand : Zustand::Aus;
}

int FlexLauschscheibe::scheibe(int platz) const
{
    return gueltig(platz) ? m_platz[platz].scheibe : -1;
}

quint32 FlexLauschscheibe::strom(int platz) const
{
    return gueltig(platz) ? m_platz[platz].strom : 0;
}

double FlexLauschscheibe::gemeldeteFrequenzHz(int platz) const
{
    return gueltig(platz) ? m_platz[platz].gemeldetHz : 0.0;
}

double FlexLauschscheibe::gewuenschteFrequenzHz(int platz) const
{
    return gueltig(platz) ? m_platz[platz].gewuenschtHz : 0.0;
}

void FlexLauschscheibe::setPlaetze(int anzahl)
{
    m_plaetzeAnzahl = std::clamp(anzahl, 0, kMaxPlaetze);
}

// ── Was RadioModel liefert ───────────────────────────────────────────

void FlexLauschscheibe::setHauptscheibe(const QString& id)
{
    m_hauptId = id;
    // Was vorher als eigene, nicht sendende Scheibe gemeldet wurde, ist
    // jetzt entscheidbar.
    for (int n : QSet<int>(m_eigene)) {
        if (QString::number(n) == m_hauptId || istLauschscheibe(n)
            || m_uebrige.contains(n)) {
            continue;
        }
        const auto tx = m_sendetJe.constFind(n);
        if (tx != m_sendetJe.constEnd() && !tx.value()) {
            behandleUebrige(n);
        }
    }
    legeNaechstenAn();
}

void FlexLauschscheibe::setPanadapter(const QString& panId)
{
    m_panId = panId;
    legeNaechstenAn();
}

void FlexLauschscheibe::setPanMitteQuelle(std::function<double()> quelle)
{
    m_panMitte = std::move(quelle);
}

void FlexLauschscheibe::setEigenerHandle(quint32 handle)
{
    m_eigenerHandle = handle;
}

void FlexLauschscheibe::setHauptfrequenzHz(double hz)
{
    if (hz > 0.0) { m_hauptHz = hz; }
}

bool FlexLauschscheibe::bereit() const
{
    return m_sende && !m_hauptId.isEmpty() && !m_panId.isEmpty()
           && m_hauptHz > 0.0;
}

// ── Welche Scheibe ist wessen ────────────────────────────────────────

int FlexLauschscheibe::platzFuerScheibe(int sliceId) const
{
    if (sliceId < 0) { return -1; }
    for (int k = 0; k < kMaxPlaetze; ++k) {
        if (m_platz[k].scheibe == sliceId) { return k; }
    }
    // Waehrend des Anlegens kennt nur die Maschine die Nummer. Jede
    // Scheibe, die weder die Hauptscheibe noch eine bekannte Lausch- oder
    // Restscheibe ist, ist dann die neue.
    if (m_anlegePlatz >= 0 && !m_hauptId.isEmpty()
        && QString::number(sliceId) != m_hauptId
        && !m_uebrige.contains(sliceId)) {
        return m_anlegePlatz;
    }
    return -1;
}

bool FlexLauschscheibe::istLauschscheibe(int sliceId) const
{
    return platzFuerScheibe(sliceId) >= 0;
}

bool FlexLauschscheibe::istFremd(int sliceId) const
{
    return istLauschscheibe(sliceId) || m_uebrige.contains(sliceId);
}

bool FlexLauschscheibe::darfAnDenVfo(int sliceId) const
{
    if (istFremd(sliceId)) { return false; }
    if (!m_hauptId.isEmpty()) { return QString::number(sliceId) == m_hauptId; }
    // Vor der Anmeldung: keine, neben der eine andere nachweislich
    // sendet. Eine einzelne Scheibe ohne tx-Angabe darf (der Normalfall
    // mit EINER Scheibe); kommt die Sendescheibe danach, gewinnt sie,
    // und RadioModel reicht die Frequenz der Hauptscheibe nach, sobald
    // sie feststeht.
    return !istNichtHaupt(sliceId);
}

bool FlexLauschscheibe::istNichtHaupt(int sliceId) const
{
    if (istFremd(sliceId)) { return true; }
    const auto eigen = m_sendetJe.constFind(sliceId);
    if (eigen != m_sendetJe.constEnd() && eigen.value()) { return false; }
    for (auto it = m_sendetJe.constBegin(); it != m_sendetJe.constEnd(); ++it) {
        if (it.key() != sliceId && it.value()) { return true; }
    }
    return false;
}

int FlexLauschscheibe::platzFuerStrom(quint32 streamId) const
{
    if (streamId == 0) { return -1; }
    for (int k = 0; k < kMaxPlaetze; ++k) {
        if (m_platz[k].strom == streamId) { return k; }
    }
    return -1;
}

// ── Uebrig aus einer frueheren Sitzung ───────────────────────────────

bool FlexLauschscheibe::istUebrigeKandidat(int id,
                                           const QMap<QString, QString>& kv)
{
    if (kv.contains(QStringLiteral("tx"))) {
        m_sendetJe[id] = kv.value(QStringLiteral("tx")) == QLatin1String("1");
    }
    if (kv.value(QStringLiteral("in_use")) == QLatin1String("0")) {
        m_eigene.remove(id);
        m_sendetJe.remove(id);
        return m_uebrige.contains(id);
    }
    if (kv.contains(QStringLiteral("client_handle"))) {
        // Nur unsere eigene: die Scheibe eines anderen Clients (Maestro,
        // SmartSDR) ist nicht unsere Sache.
        QString roh = kv.value(QStringLiteral("client_handle")).trimmed();
        if (roh.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) { roh = roh.mid(2); }
        bool ok = false;
        const quint32 handle = roh.toUInt(&ok, 16);
        if (ok && m_eigenerHandle != 0 && handle == m_eigenerHandle) {
            m_eigene.insert(id);
        } else {
            m_eigene.remove(id);
        }
    }
    if (m_uebrige.contains(id)) { return true; }
    // Entschieden wird erst, wenn die Hauptscheibe feststeht -- sonst
    // koennte eine Hauptscheibe, die (noch) nicht sendet, fuer eine
    // uebrige gehalten und entfernt werden.
    if (m_hauptId.isEmpty() || QString::number(id) == m_hauptId) { return false; }
    if (!m_eigene.contains(id)) { return false; }
    // Und nur, wenn sie nachweislich NICHT sendet: die Sendescheibe ist
    // immer die des Betreibers.
    const auto tx = m_sendetJe.constFind(id);
    return tx != m_sendetJe.constEnd() && !tx.value();
}

void FlexLauschscheibe::behandleUebrige(int id)
{
    m_uebrige.insert(id);
    // Wird auf einem Platz gelauscht, der noch keine Nummer hat, und
    // legt gerade keiner an, uebernimmt er die uebrige -- statt eine
    // weitere anzulegen, die die Maschine ohnehin verweigern wuerde.
    if (!m_hauptId.isEmpty() && m_anlegePlatz < 0) {
        for (int k = 0; k < m_plaetzeAnzahl; ++k) {
            Platz& p = m_platz[k];
            if (p.gewuenschtHz > 0.0 && p.scheibe < 0
                && p.zustand == Zustand::Aus) {
                qCInfo(lcConnection) << "Flex-Lauschscheibe: Scheibe" << id
                                     << "aus einer frueheren Sitzung — uebernommen fuer Platz"
                                     << k;
                m_uebrige.remove(id);
                p.scheibe = id;
                setZustand(k, Zustand::Anlegen);
                richteEin(k);
                return;
            }
        }
    }
    if (m_entferntGeschickt.contains(id)) { return; }
    m_entferntGeschickt.insert(id);
    qCInfo(lcConnection) << "Flex-Lauschscheibe: Scheibe" << id
                         << "aus einer frueheren Sitzung — entfernt";
    m_sende(QStringLiteral("slice remove %1").arg(id), {});
}

// ── Ein- und Ausschalten ─────────────────────────────────────────────

void FlexLauschscheibe::setFrequenzHz(int platz, double hz)
{
    if (!gueltig(platz)) { return; }
    Platz& p = m_platz[platz];
    if (!(hz > 0.0)) {
        p.gewuenschtHz = 0.0;
        if (p.zustand == Zustand::Anlegen && p.scheibe < 0) {
            // Die Nummer ist noch nicht da; abgebaut wird, sobald sie
            // kommt. Sonst bliebe eine Scheibe auf der Maschine stehen.
            p.abbauNachAnlegen = true;
        } else if (p.zustand != Zustand::Aus) {
            baueAb(platz);
        }
        return;
    }

    p.gewuenschtHz = hz;
    p.abbauNachAnlegen = false;
    switch (p.zustand) {
    case Zustand::Aus:
        legeNaechstenAn();
        break;
    case Zustand::Anlegen:
        if (p.scheibe >= 0) { stimme(platz, hz); }   // sonst nimmt richteEin die letzte
        break;
    case Zustand::Laeuft:
        stimme(platz, hz);
        break;
    }
}

void FlexLauschscheibe::legeNaechstenAn()
{
    if (m_anlegePlatz >= 0 || !bereit()) { return; }
    for (int k = 0; k < m_plaetzeAnzahl; ++k) {
        const Platz& p = m_platz[k];
        if (p.zustand == Zustand::Aus && p.gewuenschtHz > 0.0) {
            legeAn(k);
            return;
        }
    }
}

void FlexLauschscheibe::legeAn(int platz)
{
    m_anlegePlatz = platz;
    setZustand(platz, Zustand::Anlegen);
    // Im Panadapter -- siehe Kopf. Seine Mitte, wenn bekannt; sonst die
    // Frequenz der Hauptscheibe.
    const double mitte = m_panMitte ? m_panMitte() : 0.0;
    const double anlegeHz = mitte > 0.0 ? mitte : m_hauptHz;
    const QString befehl =
        QStringLiteral("slice create pan=%1 freq=%2 mode=DIGU")
            .arg(m_panId)
            .arg(anlegeHz / 1.0e6, 0, 'f', 6);
    qCInfo(lcConnection) << "Flex-Lauschscheibe: lege an (Platz" << platz
                         << ") —" << befehl;
    m_sende(befehl, [this, platz](quint32 code, const QString& body) {
        if (m_anlegePlatz != platz) { return; }   // inzwischen vergessen
        m_anlegePlatz = -1;
        Platz& p = m_platz[platz];
        bool ok = false;
        const int nummer = body.trimmed().toInt(&ok);
        if (code != 0 || !ok || nummer < 0) {
            const QString grund =
                QStringLiteral("slice create abgelehnt, Code 0x%1 %2")
                    .arg(code, 0, 16).arg(body.trimmed());
            qCWarning(lcConnection) << "Flex-Lauschscheibe:" << grund;
            p.gewuenschtHz = 0.0;
            p.abbauNachAnlegen = false;
            setZustand(platz, Zustand::Aus);
            emit abgelehnt(platz, grund);
            legeNaechstenAn();
            return;
        }
        p.scheibe = nummer;
        if (p.abbauNachAnlegen) {
            p.abbauNachAnlegen = false;
            baueAb(platz);
        } else {
            richteEin(platz);
        }
        legeNaechstenAn();
    });
}

void FlexLauschscheibe::richteEin(int platz)
{
    Platz& p = m_platz[platz];
    const QString n = QString::number(p.scheibe);
    const int kanal = daxKanal(platz);
    // Erst stumm, dann weg: so hoert der Betreiber die neue Scheibe
    // nicht einmal kurz im Lautsprecher-Mix.
    m_sende(QStringLiteral("slice set %1 audio_mute=1").arg(n), {});
    m_sende(QStringLiteral("slice set %1 dax=%2").arg(n).arg(kanal), {});
    stimme(platz, p.gewuenschtHz);
    // Die Maschine macht die neue Scheibe zur AKTIVEN (gemessen am
    // 2026-11-19: `slice 1 … active=1`). Danach richten sich der Knopf
    // am Geraet und manches CAT-Programm -- also zurueck auf die
    // Scheibe des Betreibers.
    if (!m_hauptId.isEmpty()) {
        m_sende(QStringLiteral("slice set %1 active=1").arg(m_hauptId), {});
    }
    const int scheibeBeimAnlegen = p.scheibe;
    m_sende(QStringLiteral("stream create type=dax_rx dax_channel=%1").arg(kanal),
            [this, platz, scheibeBeimAnlegen](quint32 code, const QString& body) {
        Platz& q = m_platz[platz];
        if (q.scheibe < 0 || q.scheibe != scheibeBeimAnlegen) { return; }   // inzwischen abgebaut
        if (code != 0) {
            qCWarning(lcConnection).nospace()
                << "Flex-Lauschscheibe: DAX-Strom abgelehnt, Code 0x"
                << QString::number(code, 16) << " " << body.trimmed();
            emit abgelehnt(platz, QStringLiteral("stream create type=dax_rx abgelehnt"));
            return;
        }
        // Hexadezimal ohne Vorsatz, wie beim Tonstrom
        // (RadioModel::teardownFlexConnection, gemessen 2026-10-21).
        QString roh = body.trimmed();
        if (roh.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
            roh = roh.mid(2);
        }
        bool ok = false;
        const quint32 kennung = roh.toUInt(&ok, 16);
        if (!ok || kennung == 0) {
            qCWarning(lcConnection) << "Flex-Lauschscheibe: Antwort auf"
                                       " `stream create` ohne Kennung —" << body;
            return;
        }
        q.strom = kennung;
        qCInfo(lcConnection).nospace().noquote()
            << "Flex-Lauschscheibe: Platz " << platz << ", Scheibe "
            << q.scheibe << ", DAX-Kanal " << daxKanal(platz)
            << ", Strom 0x" << QString::number(q.strom, 16);
        setZustand(platz, Zustand::Laeuft);
    });
}

void FlexLauschscheibe::stimme(int platz, double hz)
{
    const Platz& p = m_platz[platz];
    if (p.scheibe < 0 || !(hz > 0.0)) { return; }
    m_sende(FlexCommandWire::tuneSliceCommand(QString::number(p.scheibe), hz), {});
}

void FlexLauschscheibe::baueAb(int platz)
{
    Platz& p = m_platz[platz];
    if (p.strom != 0) {
        m_sende(QStringLiteral("stream remove 0x%1")
                    .arg(QString::number(p.strom, 16)), {});
    }
    if (p.scheibe >= 0) {
        m_sende(QStringLiteral("slice remove %1").arg(p.scheibe), {});
    }
    if (p.zustand == Zustand::Anlegen && p.scheibe < 0) {
        // Die Antwort steht noch aus: dann raeumt sie selbst ab.
        p.abbauNachAnlegen = true;
        return;
    }
    if (p.scheibe >= 0 || p.strom != 0) {
        qCInfo(lcConnection) << "Flex-Lauschscheibe: Platz" << platz << "abgebaut";
    }
    p.scheibe = -1;
    p.strom = 0;
    p.gemeldetHz = 0.0;
    setZustand(platz, Zustand::Aus);
}

void FlexLauschscheibe::abbauen()
{
    for (int k = 0; k < kMaxPlaetze; ++k) {
        if (m_platz[k].zustand != Zustand::Aus || m_platz[k].scheibe >= 0) {
            baueAb(k);
        }
    }
}

void FlexLauschscheibe::vergiss()
{
    m_uebrige.clear();
    m_entferntGeschickt.clear();
    m_sendetJe.clear();
    m_eigene.clear();
    m_eigenerHandle = 0;
    m_anlegePlatz = -1;
    m_hauptId.clear();
    m_panId.clear();
    for (int k = 0; k < kMaxPlaetze; ++k) {
        Platz& p = m_platz[k];
        p.scheibe = -1;
        p.strom = 0;
        p.gemeldetHz = 0.0;
        p.abbauNachAnlegen = false;
        // Der Wunsch bleibt: beim naechsten Verbinden wird wieder gelauscht.
        setZustand(k, Zustand::Aus);
    }
}

// ── Ton und Zustand ──────────────────────────────────────────────────

void FlexLauschscheibe::tonAngekommen(int platz, const QVector<float>& werte)
{
    if (!gueltig(platz)) { return; }
    Platz& p = m_platz[platz];
    if (p.strom == 0) { return; }
    if (p.tonGemeldetFuer != p.strom) {
        p.tonGemeldetFuer = p.strom;
        qCInfo(lcConnection).nospace().noquote()
            << "Flex-Lauschscheibe: erster DAX-Ton auf Platz " << platz
            << ", Strom 0x" << QString::number(p.strom, 16) << " ("
            << werte.size() << " Werte) — die Scheibe ist stumm, DAX liefert trotzdem";
        p.pegelUhr.start();
        p.pegelSpitze = 0.0f;
        p.pegelQuadrate = 0.0;
        p.pegelWerte = 0;
    }
    for (float w : werte) {
        const float b = std::fabs(w);
        if (b > p.pegelSpitze) { p.pegelSpitze = b; }
        p.pegelQuadrate += double(w) * double(w);
    }
    p.pegelWerte += werte.size();

    constexpr qint64 kPegelAbstandMs = 60000;
    if (p.pegelUhr.isValid() && p.pegelUhr.elapsed() >= kPegelAbstandMs
        && p.pegelWerte > 0) {
        const auto dbfs = [](double x) {
            return x > 1e-12 ? 20.0 * std::log10(x) : -240.0;
        };
        const double effektiv = std::sqrt(p.pegelQuadrate / double(p.pegelWerte));
        qCInfo(lcConnection).nospace().noquote()
            << "Flex-Lauschscheibe: Platz " << platz
            << ", DAX-Pegel der letzten Minute — Spitze "
            << QString::number(dbfs(p.pegelSpitze), 'f', 1) << " dBFS, Mittel "
            << QString::number(dbfs(effektiv), 'f', 1) << " dBFS, "
            << p.pegelWerte << " Werte";
        p.pegelUhr.restart();
        p.pegelSpitze = 0.0f;
        p.pegelQuadrate = 0.0;
        p.pegelWerte = 0;
    }
}

bool FlexLauschscheibe::nimmStatus(const QString& object,
                                   const QMap<QString, QString>& kv)
{
    const int id = FlexCommandWire::sliceIdFromObject(object);
    if (id < 0) { return false; }
    int platz = platzFuerScheibe(id);
    if (platz < 0) {
        if (!istUebrigeKandidat(id, kv)) {
            // Vor der Anmeldung noch nicht entscheidbar -- aber eine
            // eigene Scheibe, neben der eine andere sendet, darf schon
            // jetzt keinen Weg der Hauptscheibe erreichen.
            return m_hauptId.isEmpty() && m_eigene.contains(id)
                   && istNichtHaupt(id);
        }
        if (kv.value(QStringLiteral("in_use")) == QLatin1String("0")) {
            m_uebrige.remove(id);   // weg ist weg
            return true;
        }
        behandleUebrige(id);
        platz = platzFuerScheibe(id);
        if (platz < 0) { return true; }   // entfernt
        // uebernommen: weiter unten wie jede Meldung einer Lauschscheibe
    }
    Platz& p = m_platz[platz];

    // Von aussen entfernt (SmartSDR, Maestro): vergessen, nicht wieder
    // anlegen -- das waere ein Kampf mit dem Betreiber.
    if (kv.value(QStringLiteral("in_use")) == QLatin1String("0") && p.scheibe == id) {
        qCInfo(lcConnection) << "Flex-Lauschscheibe: Scheibe" << id
                             << "(Platz" << platz << ") von der Maschine entfernt";
        p.scheibe = -1;
        p.strom = 0;
        p.gemeldetHz = 0.0;
        p.gewuenschtHz = 0.0;
        setZustand(platz, Zustand::Aus);
        return true;
    }

    if (const auto hz = FlexCommandWire::frequencyHzFromSliceKvs(kv)) {
        if (!qFuzzyCompare(*hz, p.gemeldetHz)) {
            p.gemeldetHz = *hz;
            qCInfo(lcConnection).nospace()
                << "Flex-Lauschscheibe: Platz " << platz << " steht auf "
                << QString::number(*hz / 1.0e6, 'f', 6) << " MHz";
            emit frequenzGemeldet(platz, *hz);
        }
    }
    return true;
}

void FlexLauschscheibe::setZustand(int platz, Zustand z)
{
    if (z == m_platz[platz].zustand) { return; }
    m_platz[platz].zustand = z;
    emit zustandGeaendert();
}

} // namespace Longpath
