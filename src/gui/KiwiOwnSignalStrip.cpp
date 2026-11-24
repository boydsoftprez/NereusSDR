// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe KiwiOwnSignalStrip.h.
//
//   2026-11-24 — Neu angelegt.

#include "gui/KiwiOwnSignalStrip.h"

#include "gui/StyleConstants.h"
#include "gui/Zahlentext.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

constexpr int kHeaderH = 26;
constexpr int kAxisH = 18;
constexpr int kFooterH = 22;

QColor ink()    { return QColor(Style::role("text", Style::kTextPrimary)); }
QColor scale()  { return QColor(Style::role("text-scale", Style::kTextScale)); }
QColor border() { return QColor(Style::role("border", Style::kBorder)); }
QColor warm()   { return QColor(Style::role("trace", Style::kSpectrumTrace)); }
QColor markRed(){ return QColor(Style::kRedBorder); }
QColor okGreen(){ return QColor(Style::kGreenText); }
QColor warnAmber(){ return QColor(Style::kAmberWarn); }

// "38 km", "1,4 s" -- mit der Schreibweise der Oberflaeche.
QString kmText(double km)
{
    return QStringLiteral("%1 km").arg(anzeigeZahl(std::round(km), 0));
}

QString delayText(int ms, bool measured)
{
    const QString s = QStringLiteral("%1 s").arg(anzeigeZahl(ms / 1000.0, 1));
    return measured ? s : QStringLiteral("ca. ") + s;
}

// Schrittweite der Achsbeschriftung in Hz: 1-2-5, etwa fuenf bis zehn
// Marken ueber die Breite.
double niceStepHz(double spanHz)
{
    if (spanHz <= 0.0) { return 1000.0; }
    const double raw = spanHz / 8.0;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double n = raw / mag;
    const double f = n < 1.5 ? 1.0 : n < 3.5 ? 2.0 : n < 7.5 ? 5.0 : 10.0;
    return f * mag;
}

} // namespace

KiwiOwnSignalStrip::KiwiOwnSignalStrip(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("kiwiOwnSignalStrip"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void KiwiOwnSignalStrip::setKiwiName(const QString& name)
{
    m_name = name;
    update();
}

void KiwiOwnSignalStrip::setDistanceKm(double km)
{
    m_km = km;
    update();
}

void KiwiOwnSignalStrip::setDelay(int ms, bool measured)
{
    m_delayMs = ms;
    m_delayMeasured = measured;
    update();
}

void KiwiOwnSignalStrip::setView(double centerHz, double bandwidthHz)
{
    m_centerHz = centerHz;
    m_bandwidthHz = bandwidthHz;
    update();
}

void KiwiOwnSignalStrip::setMarkHz(double hz)
{
    if (qFuzzyCompare(m_markHz + 1.0, hz + 1.0)) { return; }
    m_markHz = hz;
    update();
}

void KiwiOwnSignalStrip::setPlotMargins(int left, int right)
{
    m_marginLeft = std::max(0, left);
    m_marginRight = std::max(0, right);
    update();
}

void KiwiOwnSignalStrip::pushRow(const QVector<float>& binsDbm, double lowHz,
                                 double highHz)
{
    if (binsDbm.size() < 2 || highHz <= lowHz) { return; }
    Row r;
    r.bins = binsDbm;
    r.lowHz = lowHz;
    r.highHz = highHz;
    m_rows.prepend(r);
    if (m_rows.size() > kRows) { m_rows.resize(kRows); }
    update();
}

void KiwiOwnSignalStrip::clearRows()
{
    m_rows.clear();
    update();
}

void KiwiOwnSignalStrip::setVerdict(const KiwiHearMeMeter::Result& result)
{
    m_verdict = result;
    update();
}

void KiwiOwnSignalStrip::setRecording(bool recording, qint64 elapsedMs,
                                      const QString& fileName)
{
    m_recording = recording;
    m_recordMs = elapsedMs;
    m_recordFile = fileName;
    update();
}

QString KiwiOwnSignalStrip::headerText() const
{
    QStringList parts;
    parts << (QStringLiteral("KIWI ")
              + (m_name.isEmpty() ? QStringLiteral("—") : m_name.toUpper()));
    if (m_km >= 0.0) { parts << kmText(m_km); }
    if (m_delayMs > 0) { parts << delayText(m_delayMs, m_delayMeasured); }
    return parts.join(QStringLiteral(" · "));
}

QString KiwiOwnSignalStrip::statusText() const
{
    using V = KiwiHearMeMeter::Verdict;
    switch (m_verdict.verdict) {
    case V::Hears:
        return tr("hört mich · %1")
            .arg(kiwiSUnitsText(m_verdict.sUnits, m_verdict.overS9Db));
    case V::Nothing:
        return tr("hört nichts");
    case V::Unknown:
        return tr("unbekannt");
    case V::Waiting:
        break;
    }
    return tr("wartet auf Sendung");
}

QColor KiwiOwnSignalStrip::statusColor() const
{
    using V = KiwiHearMeMeter::Verdict;
    switch (m_verdict.verdict) {
    case V::Hears:   return okGreen();
    case V::Nothing: return warnAmber();
    case V::Unknown:
    case V::Waiting: break;
    }
    return scale();
}

QString KiwiOwnSignalStrip::footerText() const
{
    if (!m_recording) { return tr("Aufnahme aus"); }
    const qint64 s = m_recordMs / 1000;
    const QString t = QStringLiteral("%1:%2")
        .arg(s / 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
    return tr("● Aufnahme %1 → %2").arg(t, m_recordFile);
}

QRect KiwiOwnSignalStrip::bodyRect() const
{
    return QRect(m_marginLeft, kHeaderH, width() - m_marginLeft - m_marginRight,
                 height() - kHeaderH - kAxisH - kFooterH);
}

int KiwiOwnSignalStrip::markX() const
{
    const QRect b = bodyRect();
    if (m_markHz <= 0.0 || m_bandwidthHz <= 0.0 || b.width() <= 0) { return -1; }
    const double lo = m_centerHz - m_bandwidthHz / 2.0;
    const double t = (m_markHz - lo) / m_bandwidthHz;
    if (t < 0.0 || t > 1.0) { return -1; }
    return b.left() + int(std::lround(t * (b.width() - 1)));
}

// Das Rauschen des Bildes: Mittel der Zeilen-Mediane. Folgt der Lage
// des Bandes, statt auf eine feste dBm-Spanne zu bauen -- der Kiwi liefert
// nicht auf jedem Band dieselbe Grundlinie.
double KiwiOwnSignalStrip::noiseFloorDbm() const
{
    double sum = 0.0;
    int n = 0;
    for (const Row& r : m_rows) {
        QVector<float> v = r.bins;
        const int mid = v.size() / 2;
        std::nth_element(v.begin(), v.begin() + mid, v.end());
        sum += double(v[mid]);
        ++n;
    }
    return n > 0 ? sum / n : -100.0;
}

void KiwiOwnSignalStrip::renderBody(QImage& img) const
{
    const int W = img.width();
    const int H = img.height();
    img.fill(QColor(Style::kInsetBg));
    if (m_rows.isEmpty() || W <= 0 || H <= 0 || m_bandwidthHz <= 0.0) { return; }

    // Grau-Rampe: Rauschen ein dunkles Grau, +35 dB ueber dem Rauschen weiss.
    const double floorDbm = noiseFloorDbm() - 4.0;
    constexpr double kSpanDb = 39.0;
    const double lo = m_centerHz - m_bandwidthHz / 2.0;

    for (int y = 0; y < H; ++y) {
        const int ri = std::min(int(std::int64_t(y) * kRows / H),
                                int(m_rows.size()) - 1);
        // Zeilen, die ueber den Verlauf hinaus liegen, leer lassen.
        if (std::int64_t(y) * kRows / H >= m_rows.size()) { continue; }
        const Row& r = m_rows[ri];
        const int N = r.bins.size();
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const double f = lo + (x + 0.5) / W * m_bandwidthHz;
            if (f < r.lowHz || f > r.highHz) { continue; }
            const int bi = std::clamp(
                int((f - r.lowHz) / (r.highHz - r.lowHz) * N), 0, N - 1);
            const double t = std::clamp(
                (double(r.bins[bi]) - floorDbm) / kSpanDb, 0.0, 1.0);
            const int g = 18 + int(t * 237.0);
            line[x] = qRgb(g, g, g);
        }
    }
}

QImage KiwiOwnSignalStrip::waterfallImage(int width) const
{
    if (m_rows.isEmpty()) { return QImage(); }
    QImage img(width, std::max(kRows, 120), QImage::Format_RGB32);
    renderBody(img);
    // Marke wie im Widget.
    if (m_markHz > 0.0 && m_bandwidthHz > 0.0) {
        const double lo = m_centerHz - m_bandwidthHz / 2.0;
        const double t = (m_markHz - lo) / m_bandwidthHz;
        if (t >= 0.0 && t <= 1.0) {
            const int x = int(std::lround(t * (width - 1)));
            QPainter p(&img);
            p.setPen(markRed());
            p.drawLine(x, 0, x, img.height() - 1);
        }
    }
    return img;
}

void KiwiOwnSignalStrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(Style::hexRole(Style::kAppBg)));

    const QRect body = bodyRect();

    // ── Kopfzeile: Balken in Warm, Name/Entfernung/Verzug, Aussage ─────
    const QRect head(m_marginLeft, 0, body.width(), kHeaderH - 2);
    p.fillRect(head, QColor(Style::hexRole(Style::kInsetBgTop)));
    p.fillRect(QRect(head.left(), head.top(), 3, head.height()), warm());
    p.setFont(Style::monoFont(font(), 11, QFont::DemiBold));
    p.setPen(ink());
    const QString status = statusText();
    const int statusW = p.fontMetrics().horizontalAdvance(status) + 24;
    p.drawText(head.adjusted(14, 0, -statusW, 0),
               Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
               p.fontMetrics().elidedText(headerText(), Qt::ElideRight,
                                          head.width() - statusW - 14));
    p.setFont(Style::monoFont(font(), 11, QFont::Normal));
    p.setPen(statusColor());
    p.drawText(head.adjusted(0, 0, -12, 0), Qt::AlignVCenter | Qt::AlignRight,
               status);

    // ── Wasserfall ─────────────────────────────────────────────────────
    if (body.width() > 0 && body.height() > 0) {
        QImage img(body.size(), QImage::Format_RGB32);
        renderBody(img);
        p.drawImage(body.topLeft(), img);
        if (m_rows.isEmpty()) {
            p.setFont(Style::monoFont(font(), 11, QFont::Normal));
            p.setPen(scale());
            p.drawText(body, Qt::AlignCenter, tr("wartet auf Bild vom Kiwi"));
        }
    }
    p.setPen(border());
    p.drawRect(body.adjusted(0, 0, -1, -1));

    // ── Marke auf der Sendefrequenz ────────────────────────────────────
    const int mx = markX();
    if (mx >= 0) {
        p.setPen(markRed());
        p.drawLine(mx, body.top(), mx, body.bottom());
        p.setBrush(markRed());
        const QPolygon tri({QPoint(mx - 5, body.top()), QPoint(mx + 5, body.top()),
                            QPoint(mx, body.top() + 7)});
        p.drawPolygon(tri);
        p.setBrush(Qt::NoBrush);
    }

    // ── Achse: dieselbe wie die des Panadapters darueber ───────────────
    if (m_bandwidthHz > 0.0 && body.width() > 0) {
        const double lo = m_centerHz - m_bandwidthHz / 2.0;
        const double step = niceStepHz(m_bandwidthHz);
        p.setFont(Style::monoFont(font(), 10, QFont::Normal));
        p.setPen(scale());
        const int axisTop = body.bottom() + 1;
        for (double f = std::ceil(lo / step) * step; f <= lo + m_bandwidthHz;
             f += step) {
            const int x = body.left()
                + int(std::lround((f - lo) / m_bandwidthHz * (body.width() - 1)));
            p.drawLine(x, axisTop, x, axisTop + 4);
            const QString t = anzeigeZahl(f / 1000.0, step < 1000.0 ? 1 : 0);
            const int tw = p.fontMetrics().horizontalAdvance(t);
            // Die aeusseren Beschriftungen bleiben im Widget stehen, statt
            // am Rand abgeschnitten zu werden.
            const int left = std::clamp(x - tw / 2 - 2, 0, std::max(0, width() - tw - 4));
            p.drawText(QRect(left, axisTop + 4, tw + 4, kAxisH - 4),
                       Qt::AlignCenter, t);
        }
    }

    // ── Fusszeile: Aufnahmestatus ──────────────────────────────────────
    p.setFont(Style::monoFont(font(), 11, QFont::Normal));
    p.setPen(m_recording ? ink() : scale());
    p.drawText(QRect(m_marginLeft, height() - kFooterH, body.width(), kFooterH),
               Qt::AlignVCenter | Qt::AlignLeft, footerText());
}

} // namespace Longpath
