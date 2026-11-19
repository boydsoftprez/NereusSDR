// =================================================================
// src/core/FlexLauschscheibe.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigen, siehe Kopf.

#include "core/FlexLauschscheibe.h"

#include "core/FlexRadioConnection.h"
#include "core/LogCategories.h"

#include <QLoggingCategory>

namespace Longpath {

FlexLauschscheibe::FlexLauschscheibe(Sende sende, QObject* parent)
    : QObject(parent)
    , m_sende(std::move(sende))
{
}

void FlexLauschscheibe::setHauptscheibe(const QString& id)
{
    m_hauptId = id;
    if (m_zustand == Zustand::Aus && m_gewuenschtHz > 0.0 && bereit()) {
        legeAn();
    }
}

void FlexLauschscheibe::setPanadapter(const QString& panId)
{
    m_panId = panId;
    if (m_zustand == Zustand::Aus && m_gewuenschtHz > 0.0 && bereit()) {
        legeAn();
    }
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

void FlexLauschscheibe::setFrequenzHz(double hz)
{
    if (!(hz > 0.0)) {
        m_gewuenschtHz = 0.0;
        if (m_zustand == Zustand::Anlegen) {
            // Die Nummer ist noch nicht da; abgebaut wird, sobald sie
            // kommt. Sonst bliebe eine Scheibe auf der Maschine stehen.
            m_abbauNachAnlegen = true;
        } else if (m_zustand == Zustand::Laeuft) {
            abbauen();
        }
        return;
    }

    m_gewuenschtHz = hz;
    m_abbauNachAnlegen = false;
    switch (m_zustand) {
    case Zustand::Aus:
        if (bereit()) { legeAn(); }
        break;
    case Zustand::Anlegen:
        break;   // richteEin() nimmt die zuletzt gewuenschte
    case Zustand::Laeuft:
        stimme(hz);
        break;
    }
}

void FlexLauschscheibe::legeAn()
{
    setZustand(Zustand::Anlegen);
    // Auf der Frequenz der Hauptscheibe, im Panadapter -- siehe Kopf.
    const QString befehl =
        QStringLiteral("slice create pan=%1 freq=%2 mode=DIGU")
            .arg(m_panId)
            .arg(m_hauptHz / 1.0e6, 0, 'f', 6);
    qCInfo(lcConnection) << "Flex-Lauschscheibe: lege an —" << befehl;
    m_sende(befehl, [this](quint32 code, const QString& body) {
        if (m_zustand != Zustand::Anlegen) { return; }   // inzwischen vergessen
        bool ok = false;
        const int nummer = body.trimmed().toInt(&ok);
        if (code != 0 || !ok || nummer < 0) {
            const QString grund =
                QStringLiteral("slice create abgelehnt, Code 0x%1 %2")
                    .arg(code, 0, 16).arg(body.trimmed());
            qCWarning(lcConnection) << "Flex-Lauschscheibe:" << grund;
            m_gewuenschtHz = 0.0;
            m_abbauNachAnlegen = false;
            setZustand(Zustand::Aus);
            emit abgelehnt(grund);
            return;
        }
        m_scheibe = nummer;
        if (m_abbauNachAnlegen) {
            m_abbauNachAnlegen = false;
            abbauen();
            return;
        }
        richteEin();
    });
}

void FlexLauschscheibe::richteEin()
{
    const QString n = QString::number(m_scheibe);
    // Erst stumm, dann weg: so hoert der Betreiber die neue Scheibe
    // nicht einmal kurz im Lautsprecher-Mix.
    m_sende(QStringLiteral("slice set %1 audio_mute=1").arg(n), {});
    m_sende(QStringLiteral("slice set %1 dax=%2").arg(n).arg(kDaxKanal), {});
    stimme(m_gewuenschtHz);
    m_sende(QStringLiteral("stream create type=dax_rx dax_channel=%1")
                .arg(kDaxKanal),
            [this](quint32 code, const QString& body) {
        if (m_scheibe < 0) { return; }   // inzwischen abgebaut
        if (code != 0) {
            qCWarning(lcConnection).nospace()
                << "Flex-Lauschscheibe: DAX-Strom abgelehnt, Code 0x"
                << QString::number(code, 16) << " " << body.trimmed();
            emit abgelehnt(QStringLiteral("stream create type=dax_rx abgelehnt"));
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
        m_strom = kennung;
        qCInfo(lcConnection).nospace()
            << "Flex-Lauschscheibe: Scheibe " << m_scheibe
            << ", DAX-Strom 0x" << QString::number(m_strom, 16);
        setZustand(Zustand::Laeuft);
    });
}

void FlexLauschscheibe::stimme(double hz)
{
    if (m_scheibe < 0 || !(hz > 0.0)) { return; }
    m_sende(FlexCommandWire::tuneSliceCommand(QString::number(m_scheibe), hz), {});
}

void FlexLauschscheibe::abbauen()
{
    if (m_strom != 0) {
        m_sende(QStringLiteral("stream remove 0x%1")
                    .arg(QString::number(m_strom, 16)), {});
    }
    if (m_scheibe >= 0) {
        m_sende(QStringLiteral("slice remove %1").arg(m_scheibe), {});
    }
    if (m_zustand == Zustand::Anlegen && m_scheibe < 0) {
        // Die Antwort steht noch aus: dann raeumt sie selbst ab.
        m_abbauNachAnlegen = true;
        return;
    }
    if (m_scheibe >= 0 || m_strom != 0) {
        qCInfo(lcConnection) << "Flex-Lauschscheibe: abgebaut";
    }
    m_scheibe = -1;
    m_strom = 0;
    m_gemeldetHz = 0.0;
    setZustand(Zustand::Aus);
}

void FlexLauschscheibe::vergiss()
{
    m_scheibe = -1;
    m_strom = 0;
    m_gemeldetHz = 0.0;
    m_abbauNachAnlegen = false;
    m_hauptId.clear();
    m_panId.clear();
    // Der Wunsch bleibt: beim naechsten Verbinden wird wieder gelauscht.
    setZustand(Zustand::Aus);
}

bool FlexLauschscheibe::istLauschscheibe(int sliceId) const
{
    if (sliceId < 0) { return false; }
    if (m_scheibe >= 0) { return sliceId == m_scheibe; }
    // Waehrend des Anlegens kennt nur die Maschine die Nummer. Jede
    // Scheibe, die nicht die Hauptscheibe ist, ist dann die neue.
    if (m_zustand == Zustand::Anlegen && !m_hauptId.isEmpty()) {
        return QString::number(sliceId) != m_hauptId;
    }
    return false;
}

bool FlexLauschscheibe::istLauschstrom(quint32 streamId) const
{
    return m_strom != 0 && streamId == m_strom;
}

void FlexLauschscheibe::tonAngekommen(int werte)
{
    if (m_strom == 0 || m_tonGemeldetFuer == m_strom) { return; }
    m_tonGemeldetFuer = m_strom;
    qCInfo(lcConnection).nospace()
        << "Flex-Lauschscheibe: erster DAX-Ton auf Strom 0x"
        << QString::number(m_strom, 16) << " (" << werte
        << " Werte) — die Scheibe ist stumm, DAX liefert trotzdem";
}

bool FlexLauschscheibe::nimmStatus(const QString& object,
                                   const QMap<QString, QString>& kv)
{
    const int id = FlexCommandWire::sliceIdFromObject(object);
    if (!istLauschscheibe(id)) { return false; }

    // Von aussen entfernt (SmartSDR, Maestro): vergessen, nicht wieder
    // anlegen -- das waere ein Kampf mit dem Betreiber.
    if (kv.value(QStringLiteral("in_use")) == QLatin1String("0")
        && m_scheibe == id) {
        qCInfo(lcConnection) << "Flex-Lauschscheibe: Scheibe" << id
                             << "von der Maschine entfernt";
        m_scheibe = -1;
        m_strom = 0;
        m_gemeldetHz = 0.0;
        m_gewuenschtHz = 0.0;
        setZustand(Zustand::Aus);
        return true;
    }

    if (const auto hz = FlexCommandWire::frequencyHzFromSliceKvs(kv)) {
        if (!qFuzzyCompare(*hz, m_gemeldetHz)) {
            m_gemeldetHz = *hz;
            emit frequenzGemeldet(*hz);
        }
    }
    return true;
}

void FlexLauschscheibe::setZustand(Zustand z)
{
    if (z == m_zustand) { return; }
    m_zustand = z;
    emit zustandGeaendert();
}

} // namespace Longpath
