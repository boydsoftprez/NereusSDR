// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe KiwiRecording.h.
//
//   2026-11-24 — Neu angelegt.

#include "core/KiwiRecording.h"

#include <QDir>
#include <QFileInfo>

namespace Longpath {

KiwiRecording::KiwiRecording(QObject* parent) : QObject(parent) {}

KiwiRecording::~KiwiRecording()
{
    m_writer.close();
}

QString KiwiRecording::fileBaseName(const QString& kiwiName,
                                    const QDateTime& when)
{
    QString clean;
    for (const QChar c : kiwiName.trimmed()) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-')
            || c == QLatin1Char('_')) {
            clean.append(c);
        } else if ((c.isSpace() || c == QLatin1Char('.') || c == QLatin1Char('/'))
                   && !clean.isEmpty() && !clean.endsWith(QLatin1Char('-'))) {
            clean.append(QLatin1Char('-'));
        }
    }
    if (clean.size() > 32) { clean.truncate(32); }
    while (clean.endsWith(QLatin1Char('-'))) { clean.chop(1); }
    QString base = QStringLiteral("Mein-Signal");
    if (!clean.isEmpty()) { base += QLatin1Char('_') + clean; }
    return base + QLatin1Char('_')
           + when.toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
}

bool KiwiRecording::start(const QString& folder, const QString& kiwiName,
                          const QDateTime& when, QString* error)
{
    if (m_writer.isOpen()) {
        if (error) { *error = tr("Es läuft schon eine Aufnahme."); }
        return false;
    }
    if (!QDir().mkpath(folder)) {
        if (error) { *error = tr("Ordner nicht anlegbar: %1").arg(folder); }
        return false;
    }
    const QString base = QDir(folder).filePath(fileBaseName(kiwiName, when));
    m_wavPath = base + QStringLiteral(".wav");
    m_imagePath.clear();
    m_stoppedByLimit = false;
    m_writeFailed = false;
    m_finalized = false;
    QString err;
    if (!m_writer.open(m_wavPath, kSampleRate,
                       WavStreamWriter::Format::Float32Stereo,
                       /*dither=*/false, &err)) {
        if (error) { *error = err; }
        m_wavPath.clear();
        return false;
    }
    return true;
}

void KiwiRecording::feedAudio(const QByteArray& pcm24kStereoFloat)
{
    if (!m_writer.isOpen()) { return; }
    const int frames = pcm24kStereoFloat.size() / int(sizeof(float) * 2);
    if (frames <= 0) { return; }
    if (!m_writer.writeInterleaved(
            reinterpret_cast<const float*>(pcm24kStereoFloat.constData()),
            frames)) {
        m_writeFailed = true;
        m_writer.close();
        emit aborted(tr("Schreiben fehlgeschlagen — Platte voll?"));
        return;
    }
    if (m_writer.framesWritten() >= kMaxSeconds * kSampleRate) {
        m_stoppedByLimit = true;
        m_writer.close();
        emit aborted(tr("Obergrenze von zwei Stunden erreicht."));
    }
}

qint64 KiwiRecording::elapsedMs() const
{
    return m_writer.framesWritten() * 1000 / kSampleRate;
}

QString KiwiRecording::stop(const QImage& waterfall)
{
    if (m_wavPath.isEmpty() || m_finalized) { return QString(); }
    m_finalized = true;
    m_writer.close();
    const QString wav = m_wavPath;
    if (!waterfall.isNull()) {
        const QString png = QFileInfo(wav).path() + QLatin1Char('/')
                            + QFileInfo(wav).completeBaseName()
                            + QStringLiteral(".png");
        if (waterfall.save(png)) { m_imagePath = png; }
    }
    return wav;
}

} // namespace Longpath
