// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe KiwiOwnSignal.h.
//
//   2026-11-24 — Neu angelegt.

#include "core/KiwiOwnSignal.h"

#include "core/Maidenhead.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Longpath {

namespace {

// "(48.3, 14.2)" -> lat/lon. Das Verzeichnis liefert den Standort so.
bool parseGps(const QString& gps, double& lat, double& lon)
{
    static const QRegularExpression re(
        QStringLiteral("\\(?\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*\\)?"));
    const QRegularExpressionMatch m = re.match(gps);
    if (!m.hasMatch()) { return false; }
    lat = m.captured(1).toDouble();
    lon = m.captured(2).toDouble();
    return std::abs(lat) <= 90.0 && std::abs(lon) <= 180.0;
}

// Grosskreisentfernung in km (Haversine, Erdradius 6371 km).
double greatCircleKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * kDeg;
    const double dLon = (lon2 - lon1) * kDeg;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
        + std::cos(lat1 * kDeg) * std::cos(lat2 * kDeg)
          * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * 6371.0 * std::asin(std::min(1.0, std::sqrt(a)));
}

double medianOf(QVector<double> v)
{
    if (v.isEmpty()) { return 0.0; }
    const int mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    return v[mid];
}

} // namespace

// ── Verzeichnis-Vorauswahl ───────────────────────────────────────────

bool kiwiReceiverCoversFrequency(const KiwiPublicReceiver& receiver,
                                 double frequencyHz)
{
    if (frequencyHz <= 0.0) { return true; }
    const QStringList parts = receiver.bands.split(QLatin1Char('-'));
    if (parts.size() != 2) { return true; }
    bool okLo = false;
    bool okHi = false;
    const double lo = parts[0].trimmed().toDouble(&okLo);
    const double hi = parts[1].trimmed().toDouble(&okHi);
    if (!okLo || !okHi || hi <= lo) { return true; }
    return frequencyHz >= lo && frequencyHz <= hi;
}

double kiwiReceiverDistanceKm(const KiwiPublicReceiver& receiver,
                              const QString& ownGrid)
{
    if (!isValidGridSquare(ownGrid)) { return -1.0; }
    if (isValidGridSquare(receiver.grid)) {
        return calculateDistanceKm(ownGrid, receiver.grid);
    }
    double lat = 0.0;
    double lon = 0.0;
    if (parseGps(receiver.gps, lat, lon)) {
        double ownLat = 0.0;
        double ownLon = 0.0;
        calculateLatLonFromGridSquare(ownGrid, ownLat, ownLon);
        return greatCircleKm(ownLat, ownLon, lat, lon);
    }
    return -1.0;
}

QVector<KiwiCandidate> kiwiPreselect(
    const QVector<KiwiPublicReceiver>& receivers,
    const QString& ownGrid, double frequencyHz, int maxCount)
{
    QVector<KiwiCandidate> out;
    for (const KiwiPublicReceiver& rx : receivers) {
        if (rx.offline) { continue; }
        if (!rx.mayConnectViaApi()) { continue; }
        if (rx.usersMax > 0 && rx.users >= rx.usersMax) { continue; }
        if (!kiwiReceiverCoversFrequency(rx, frequencyHz)) { continue; }
        KiwiCandidate c;
        c.receiver = rx;
        c.km = kiwiReceiverDistanceKm(rx, ownGrid);
        out.append(c);
    }
    if (isValidGridSquare(ownGrid)) {
        // stabil: gleiche Entfernung behaelt die Reihenfolge des Verzeichnisses
        std::stable_sort(out.begin(), out.end(),
                         [](const KiwiCandidate& a, const KiwiCandidate& b) {
            if ((a.km < 0) != (b.km < 0)) { return b.km < 0; }
            return a.km < b.km;
        });
    }
    if (maxCount > 0 && out.size() > maxCount) {
        out.resize(maxCount);
    }
    return out;
}

// ── "Hoert der Kiwi mich?" ───────────────────────────────────────────

void KiwiHearMeMeter::sUnitsFromDbm(double dbm, int& sUnits, int& overS9Db)
{
    // S9 = -73 dBm, 6 dB je S-Stufe (S1 = -121 dBm).
    if (dbm >= -73.0) {
        sUnits = 9;
        overS9Db = int(std::lround(dbm + 73.0));
        return;
    }
    overS9Db = 0;
    sUnits = std::clamp(int(std::lround((dbm + 127.0) / 6.0)), 0, 9);
}

QString kiwiSUnitsText(int sUnits, int overS9Db)
{
    if (sUnits >= 9 && overS9Db > 0) {
        return QStringLiteral("S9+%1").arg(overS9Db);
    }
    return QStringLiteral("S%1").arg(sUnits);
}

void KiwiHearMeMeter::reset()
{
    m_tx = false;
    m_everTransmitted = false;
    m_txStartMs = 0;
    m_txEndMs = 0;
    m_delayMeasured = false;
    m_delayMs = kAssumedDelayMs;
    m_preTxHot = false;
    m_lastRowHot = false;
    m_rows.clear();
}

void KiwiHearMeMeter::txChanged(bool transmitting, qint64 nowMs)
{
    if (transmitting == m_tx) { return; }
    m_tx = transmitting;
    if (transmitting) {
        // Eine neue Aussendung beginnt eine neue Messung. Den Verzug
        // behalten wir: er gehoert zur Strecke, nicht zur Sendung.
        m_everTransmitted = true;
        m_txStartMs = nowMs;
        m_txEndMs = 0;
        // Zeilen aus der Zeit davor gehoeren nicht zur Messung. Merken
        // nur, ob die Marke schon VOR der Sendung belegt war: dann sagt
        // ein Ausschlag nach dem Tasten nichts ueber den Verzug.
        m_preTxHot = m_lastRowHot;
        m_rows.clear();
    } else {
        m_txEndMs = nowMs;
    }
}

void KiwiHearMeMeter::feedRow(const QVector<float>& binsDbm, double lowHz,
                              double highHz, qint64 nowMs)
{
    RowSample s;
    s.atMs = nowMs;
    const int n = binsDbm.size();
    if (n < 4 || highHz <= lowHz || m_markHz <= 0.0) {
        // Ein unbrauchbares Bild zaehlt als "Marke nicht im Bild" -- es
        // wird nicht still weggeworfen, sonst waere es ein Loch.
        s.markInside = false;
    } else {
        s.markInside = m_markHz >= lowHz && m_markHz <= highHz;
        if (s.markInside) {
            const double binHz = (highHz - lowHz) / double(n);
            const double half = std::max(kMarkHalfWidthHz, binHz);
            const int first = std::clamp(
                int(std::floor((m_markHz - half - lowHz) / binHz)), 0, n - 1);
            const int last = std::clamp(
                int(std::ceil((m_markHz + half - lowHz) / binHz)), 0, n - 1);
            float peak = binsDbm[first];
            for (int i = first; i <= last; ++i) {
                peak = std::max(peak, binsDbm[i]);
            }
            QVector<double> all;
            all.reserve(n);
            for (float v : binsDbm) { all.append(double(v)); }
            s.peakDbm = double(peak);
            s.snrDb = double(peak) - medianOf(all);
        }
    }

    m_lastRowHot = s.markInside && s.snrDb >= kHearSnrDb;

    // Der Verzug wird an der ersten Zeile abgelesen, in der die Marke
    // nach dem Tasten aus dem Rauschen steigt -- aber nur, wenn sie vorher
    // frei war. Sonst bleibt der angenommene Wert.
    if (m_tx && !m_delayMeasured && !m_preTxHot && s.markInside
        && s.snrDb >= kHearSnrDb) {
        const qint64 d = nowMs - m_txStartMs;
        if (d >= kMinDelayMs && d <= kMaxDelayMs) {
            m_delayMeasured = true;
            m_delayMs = int(d);
        }
    }

    // Nur Zeilen ab dem Tasten merken (m_rows wurde dort geleert); davor
    // genuegt die letzte, damit m_preTxHot gesetzt werden kann.
    if (!m_tx && m_everTransmitted && nowMs > m_txEndMs + m_delayMs) {
        return;   // nach dem Fenster: nichts mehr aufnehmen
    }
    m_rows.append(s);
    if (m_rows.size() > kMaxKeptRows) {
        m_rows.remove(0, m_rows.size() - kMaxKeptRows);
    }
}

KiwiHearMeMeter::Result KiwiHearMeMeter::result() const
{
    Result r;
    r.delayMeasured = m_delayMeasured;
    r.delayMs = m_delayMs;
    if (!m_everTransmitted) {
        r.verdict = Verdict::Waiting;
        return r;
    }

    const qint64 winStart = m_txStartMs + m_delayMs;
    const qint64 winEnd = m_tx ? std::numeric_limits<qint64>::max()
                               : m_txEndMs + m_delayMs;
    QVector<double> snrs;
    QVector<double> hotPeaks;
    int notInside = 0;
    for (const RowSample& s : m_rows) {
        if (s.atMs < winStart || s.atMs > winEnd) { continue; }
        if (!s.markInside) { ++notInside; continue; }
        snrs.append(s.snrDb);
        if (s.snrDb >= kHearSnrDb) { hotPeaks.append(s.peakDbm); }
    }
    r.rows = snrs.size();
    if (!snrs.isEmpty()) { r.snrDb = medianOf(snrs); }

    // Sprachpausen: es genuegt, dass ein Viertel der Zeilen (mindestens
    // zwei) die Schwelle erreicht -- der Median waere bei SSB zu streng.
    const bool hears = hotPeaks.size() >= 2
                       && hotPeaks.size() * 4 >= snrs.size();
    if (hears) {
        r.verdict = Verdict::Hears;
        r.peakDbm = medianOf(hotPeaks);
        sUnitsFromDbm(r.peakDbm, r.sUnits, r.overS9Db);
        return r;
    }

    // Erst jetzt "hoert nichts" -- und nur mit Beweislast: genug Zeilen,
    // die Marke im Bild, und die Sendung lang genug, dass das Signal
    // den Stromverzug schon durchlaufen haben koennte.
    constexpr qint64 kSettleMs = 3000;
    const bool stillArriving = m_tx && (m_rows.isEmpty() ? true
        : m_rows.last().atMs < m_txStartMs + m_delayMs + kSettleMs);
    if (r.rows >= kMinRows && !stillArriving) {
        r.verdict = Verdict::Nothing;
        r.peakDbm = r.snrDb;   // unbenutzt, nur definiert
        return r;
    }
    r.verdict = Verdict::Unknown;
    return r;
}

} // namespace Longpath
