// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/DsrSaveBox.cpp  (NereusSDR)
// =================================================================
//
// See DsrSaveBox.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 25.
// =================================================================

#include "gui/recordings/DsrSaveBox.h"

#include "core/recordings/ContactRecorder.h"

#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr double kBytesPerMb = 1.0e6;
constexpr double kMbPerGb = 1000.0;
constexpr double kMsPerMinute = 60000.0;
// Sound only is saved as Contact recordings are: 24 kHz mono, 16-bit WAV
// or 64 kbit/s MP3.
constexpr double kWavBytesPerMs = ContactRecorder::kFileRateHz * 2.0 / 1000.0;
constexpr double kMp3BytesPerMs = ContactRecorder::kMonoKbitPerSec / 8.0;

QString hm(qint64 utcMs)
{
    return QDateTime::fromMSecsSinceEpoch(utcMs, QTimeZone::utc()).toString(QStringLiteral("HH:mm"));
}

} // namespace

DsrSaveBox::DsrSaveBox(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("DsrSaveBox"));
    setFixedWidth(kWidth);
    // hours.html #saveBox.
    setStyleSheet(QStringLiteral(
        "#DsrSaveBox { background: #1b1b1f; border: 1px solid #3d3d41; border-radius: 4px; }"
        "#DsrSaveBox QLabel { color: #e8e8ec; background: transparent; border: none; font-size: 12px; }"
        "#DsrSaveBox QLabel#DsrSaveDetail { color: #a6a6ac; }"
        "#DsrSaveBox QPushButton { min-height: 22px; max-height: 22px; border-radius: 6px;"
        " border: 1px solid #2c2c31; background: #1f1f24; color: #f0f0f2; font-weight: 700;"
        " font-size: 11.5px; }"
        "#DsrSaveBox QPushButton#DsrSaveWholePan { background: #264e78; border-color: #4a7ba8;"
        " color: #ffffff; }"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("DsrSaveHeader"));
    QFont bold = m_header->font();
    bold.setBold(true);
    m_header->setFont(bold);
    layout->addWidget(m_header);
    m_detail = new QLabel(this);
    m_detail->setObjectName(QStringLiteral("DsrSaveDetail"));
    m_detail->setWordWrap(true);
    layout->addWidget(m_detail);
    layout->addSpacing(3);
    m_wholePan = new QPushButton(QStringLiteral("Save stretch (whole pan)"), this);
    m_wholePan->setObjectName(QStringLiteral("DsrSaveWholePan"));
    layout->addWidget(m_wholePan);
    m_sound = new QPushButton(this);
    m_sound->setObjectName(QStringLiteral("DsrSaveSound"));
    layout->addWidget(m_sound);
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    cancel->setObjectName(QStringLiteral("DsrSaveCancel"));
    layout->addWidget(cancel);
    connect(m_wholePan, &QPushButton::clicked, this, [this] { emit saveWanted(m_fromMs, m_toMs, true); });
    connect(m_sound, &QPushButton::clicked, this, [this] { emit saveWanted(m_fromMs, m_toMs, false); });
    connect(cancel, &QPushButton::clicked, this, &DsrSaveBox::cancelled);
    refresh();
}

void DsrSaveBox::setStretch(qint64 fromUtcMs, qint64 toUtcMs)
{
    m_fromMs = fromUtcMs;
    m_toMs = toUtcMs;
    refresh();
}

void DsrSaveBox::setEstimates(double panBytesPerMs, bool soundAsWav)
{
    m_panBytesPerMs = std::max(0.0, panBytesPerMs);
    m_soundAsWav = soundAsWav;
    refresh();
}

QString DsrSaveBox::headerText() const
{
    return QStringLiteral("Save %1 to %2 UTC").arg(hm(m_fromMs), hm(m_toMs));
}

QString DsrSaveBox::sizeText(double bytes)
{
    const double mb = bytes / kBytesPerMb;
    if (mb >= kMbPerGb) {
        return QStringLiteral("%1 GB").arg(mb / kMbPerGb, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB").arg(std::max<qint64>(1, std::llround(mb)));
}

QString DsrSaveBox::detailText() const
{
    const double ms = double(std::max<qint64>(0, m_toMs - m_fromMs));
    const qint64 minutes = std::max<qint64>(1, std::llround(ms / kMsPerMinute));
    const QString sound = sizeText(ms * (m_soundAsWav ? kWavBytesPerMs : kMp3BytesPerMs));
    if (m_panBytesPerMs <= 0.0) {
        return QStringLiteral("%1 min · sound only about %2").arg(minutes).arg(sound);
    }
    return QStringLiteral("%1 min · whole pan about %2, sound only about %3")
        .arg(minutes)
        .arg(sizeText(ms * m_panBytesPerMs), sound);
}

void DsrSaveBox::refresh()
{
    m_header->setText(headerText());
    m_detail->setText(detailText());
    m_sound->setText(m_soundAsWav ? QStringLiteral("Save sound only (WAV)") : QStringLiteral("Save sound only (MP3)"));
    adjustSize();
}

} // namespace NereusSDR
