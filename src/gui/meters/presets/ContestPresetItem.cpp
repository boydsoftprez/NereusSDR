// =================================================================
// src/gui/meters/presets/ContestPresetItem.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1574-1599 (createContestPreset
// helper). Ships under the project's GPLv3 terms (root LICENSE);
// no per-file verbatim upstream header required because no Thetis
// source code was consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. Paints five stacked strips (backdrop,
//                VFO, band placeholders, mode placeholders, clock)
//                in fixed 30/25/20/25 proportions matching the
//                factory's layout rects.
// =================================================================

#include "gui/meters/presets/ContestPresetItem.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace NereusSDR {

ContestPresetItem::ContestPresetItem(QObject* parent)
    : MeterItem(parent)
{
}

void ContestPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }
    p.save();
    p.fillRect(rect, m_backdropColor);

    // Layout fractions mirror createContestPreset
    // (ItemGroup.cpp:1574-1599):
    //   VFO       : 0.00 -> 0.30
    //   BandBtns  : 0.30 -> 0.55
    //   ModeBtns  : 0.55 -> 0.75
    //   Clock     : 0.75 -> 1.00
    const int y0 = rect.y();
    const int h  = rect.height();
    const QRect vfoRect  (rect.x(), y0,                rect.width(), h * 30 / 100);
    const QRect bandRect (rect.x(), y0 + h * 30 / 100, rect.width(), h * 25 / 100);
    const QRect modeRect (rect.x(), y0 + h * 55 / 100, rect.width(), h * 20 / 100);
    const QRect clockRect(rect.x(), y0 + h * 75 / 100, rect.width(), h - h * 75 / 100);

    // VFO strip — band/mode labels + large freq digits.
    {
        const int labelH = vfoRect.height() * 30 / 100;
        const QRect labelRect(vfoRect.x(), vfoRect.y(),
                              vfoRect.width(), labelH);
        p.setPen(m_labelColor);
        QFont lf = p.font();
        lf.setPointSizeF(qMax(8.0, labelH * 0.55));
        p.setFont(lf);
        p.drawText(labelRect.adjusted(4, 0, 0, 0),
                   Qt::AlignLeft | Qt::AlignVCenter, m_bandLabel);
        p.drawText(labelRect.adjusted(0, 0, -4, 0),
                   Qt::AlignRight | Qt::AlignVCenter, m_modeLabel);
        const QRect digitRect(vfoRect.x(), vfoRect.y() + labelH,
                              vfoRect.width(), vfoRect.height() - labelH);
        const double mhz = m_freqHz / 1.0e6;
        p.setPen(m_digitColor);
        QFont df = p.font();
        df.setBold(true);
        df.setPointSizeF(qMax(12.0, digitRect.height() * 0.7));
        p.setFont(df);
        p.drawText(digitRect, Qt::AlignCenter,
                   QStringLiteral("%1 MHz").arg(mhz, 0, 'f', 3));
    }

    // Band button placeholder strip — horizontally tiled cells.
    {
        const int N = 7;
        const int cellW = bandRect.width() / N;
        QFont f = p.font();
        f.setPointSizeF(qMax(7.0, bandRect.height() * 0.3));
        f.setBold(false);
        p.setFont(f);
        static const char* const kLabels[N] = {
            "160", "80", "40", "20", "15", "10", "6"
        };
        for (int i = 0; i < N; ++i) {
            const QRect cell(bandRect.x() + i * cellW + 2,
                             bandRect.y() + 2,
                             cellW - 4,
                             bandRect.height() - 4);
            p.fillRect(cell, m_buttonColor);
            p.setPen(m_labelColor);
            p.drawText(cell, Qt::AlignCenter, QString::fromLatin1(kLabels[i]));
        }
    }

    // Mode button placeholder strip.
    {
        const int N = 5;
        const int cellW = modeRect.width() / N;
        static const char* const kLabels[N] = {
            "USB", "LSB", "CW", "AM", "FM"
        };
        for (int i = 0; i < N; ++i) {
            const QRect cell(modeRect.x() + i * cellW + 2,
                             modeRect.y() + 2,
                             cellW - 4,
                             modeRect.height() - 4);
            p.fillRect(cell, m_buttonColor);
            p.setPen(m_labelColor);
            p.drawText(cell, Qt::AlignCenter, QString::fromLatin1(kLabels[i]));
        }
    }

    // Clock strip — UTC time.
    {
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
        p.setPen(m_clockColor);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, clockRect.height() * 0.5));
        p.setFont(f);
        p.drawText(clockRect, Qt::AlignCenter,
                   nowUtc.toString(QStringLiteral("HH:mm:ss")) + QStringLiteral(" UTC"));
    }
    p.restore();
}

QString ContestPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),      QStringLiteral("ContestPreset"));
    o.insert(QStringLiteral("x"),         static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),         static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),         static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),         static_cast<double>(m_h));
    o.insert(QStringLiteral("freqHz"),    m_freqHz);
    o.insert(QStringLiteral("bandLabel"), m_bandLabel);
    o.insert(QStringLiteral("modeLabel"), m_modeLabel);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool ContestPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("ContestPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    m_freqHz    = o.value(QStringLiteral("freqHz")).toDouble(m_freqHz);
    m_bandLabel = o.value(QStringLiteral("bandLabel")).toString(m_bandLabel);
    m_modeLabel = o.value(QStringLiteral("modeLabel")).toString(m_modeLabel);
    return true;
}

} // namespace NereusSDR
