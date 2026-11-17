// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original contact recorder.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; one slice's contact recorder (spec 6.1,
//             6.2, V2) and the call taken from Roundtable. AI tooling:
//             Anthropic Claude Code.

#include "ContactRecorder.h"

#include "core/LogCategories.h"
#include "core/Resampler.h"
#include "core/recordings/AudioFileWriter.h"
#include "core/roundtable/RoundtableWire.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

struct CallTally {
    qint64 talkerMs = 0;
    qint64 nameMs = 0;
};

// The base callsign of `text`, or empty when it is not a callsign.
QString baseOf(const QString& text)
{
    const QString call = text.trimmed().toUpper();
    if (!RoundtableWire::isCallsign(call)) {
        return QString();
    }
    return RoundtableWire::baseCallsign(call);
}

QString longest(const QHash<QString, CallTally>& tally, qint64 CallTally::*field)
{
    QString best;
    qint64 bestMs = 0;
    for (auto it = tally.cbegin(); it != tally.cend(); ++it) {
        const qint64 ms = it.value().*field;
        if (ms > bestMs || (ms == bestMs && ms > 0 && it.key() < best)) {
            best = it.key();
            bestMs = ms;
        }
    }
    return best;
}

}  // namespace

QString callForRecording(const QJsonObject& room, qint64 startMs, qint64 endMs, const QString& ownCall)
{
    if (room.isEmpty() || endMs <= startMs) {
        return QString();
    }
    const QString ownBase = baseOf(ownCall);
    QHash<QString, CallTally> tally;
    // RoundtableModel::room(): turns still going in "now" (end null), ended
    // ones in "earlier".
    for (const QString& list : {QStringLiteral("now"), QStringLiteral("earlier")}) {
        for (const QJsonValue& value : room.value(list).toArray()) {
            const QJsonObject turn = value.toObject();
            const qint64 turnStart = turn.value(QStringLiteral("start")).toInteger();
            const QJsonValue endValue = turn.value(QStringLiteral("end"));
            const bool open = endValue.isNull() || endValue.isUndefined();
            const qint64 turnEnd = open ? endMs : endValue.toInteger();
            const qint64 overlap = std::min(turnEnd, endMs) - std::max(turnStart, startMs);
            if (overlap <= 0) {
                continue;
            }
            QSet<QString> talkers;
            for (const QJsonValue& t : turn.value(QStringLiteral("talkers")).toArray()) {
                const QString base = baseOf(t.toObject().value(QStringLiteral("callsign")).toString());
                if (!base.isEmpty() && base != ownBase) {
                    talkers.insert(base);
                }
            }
            QSet<QString> names;
            for (const QJsonValue& n : turn.value(QStringLiteral("names")).toArray()) {
                const QString base = baseOf(n.toObject().value(QStringLiteral("name")).toString());
                if (!base.isEmpty() && base != ownBase) {
                    names.insert(base);
                }
            }
            for (const QString& base : std::as_const(talkers)) {
                tally[base].talkerMs += overlap;
            }
            for (const QString& base : std::as_const(names)) {
                tally[base].nameMs += overlap;
            }
        }
    }
    const QString talker = longest(tally, &CallTally::talkerMs);
    if (!talker.isEmpty()) {
        return talker;
    }
    return longest(tally, &CallTally::nameMs);
}

ContactRecorder::ContactRecorder(int sliceId)
    : m_sliceId(sliceId)
{
    m_scratch.resize(static_cast<size_t>(kChunkFrames) * 2);
    m_mono.resize(static_cast<size_t>(kChunkFrames));
    m_resampled.resize(static_cast<size_t>(kChunkFrames) + 256);
    m_monitorPending.reserve(static_cast<size_t>(kMonitorMaxFrames) + kChunkFrames);
    m_out.resize((static_cast<size_t>(kChunkFrames) + 256) * 2);
    m_silence.assign(static_cast<size_t>(kChunkFrames), 0.0f);
}

ContactRecorder::~ContactRecorder() = default;

void ContactRecorder::begin(std::unique_ptr<AudioFileWriter> writer, bool sidesApart)
{
    m_writer = std::move(writer);
    m_sidesApart = sidesApart;
    m_failed = false;
    m_framesWritten = 0;
    m_monitorPending.clear();
    if (m_receiveResampler) {
        m_receiveResampler->clear();
    }
    if (m_monitorResampler) {
        m_monitorResampler->clear();
    }
}

int ContactRecorder::resampleInto(std::unique_ptr<Resampler>& resampler, int& resamplerRate,
                                  int rateHz, const float* in, int frames)
{
    if (rateHz == kFileRateHz) {
        std::copy(in, in + frames, m_resampled.begin());
        return frames;
    }
    if (!resampler || resamplerRate != rateHz) {
        resampler = std::make_unique<Resampler>(static_cast<double>(rateHz),
                                                static_cast<double>(kFileRateHz), kChunkFrames);
        resamplerRate = rateHz;
    }
    // Only ever down to 24 kHz (48 kHz receive, 48 or 192 kHz monitor), so
    // a chunk's output fits in a chunk's room.
    return resampler->processInto(in, frames, m_resampled.data(),
                                  static_cast<int>(m_resampled.size()));
}

bool ContactRecorder::drain()
{
    if (!m_writer) {
        return !m_failed;
    }
    // Your voice first, so the receive frames below can carry it.
    for (;;) {
        const int n = m_monitorRing.pop(m_scratch.data(), kChunkFrames);
        if (n <= 0) {
            break;
        }
        const int rate = m_monitorRateHz.load(std::memory_order_relaxed);
        const int out = resampleInto(m_monitorResampler, m_monitorResamplerRate, rate,
                                     m_scratch.data(), n);
        m_monitorPending.insert(m_monitorPending.end(), m_resampled.begin(),
                                m_resampled.begin() + out);
    }
    if (m_monitorPending.size() > static_cast<size_t>(kMonitorMaxFrames)) {
        const size_t excess = m_monitorPending.size() - static_cast<size_t>(kMonitorMaxFrames);
        m_monitorPending.erase(m_monitorPending.begin(),
                               m_monitorPending.begin() + static_cast<std::ptrdiff_t>(excess));
    }

    for (;;) {
        const int n = m_receiveRing.pop(m_scratch.data(), kChunkFrames * 2);
        if (n <= 0) {
            break;
        }
        const int frames = n / 2;
        bool heard = false;
        for (int i = 0; i < frames; ++i) {
            const float l = m_scratch[static_cast<size_t>(2 * i)];
            const float r = m_scratch[static_cast<size_t>(2 * i + 1)];
            m_mono[static_cast<size_t>(i)] = 0.5f * (l + r);
            if (std::fabs(l) > kSquelchOpenLevel || std::fabs(r) > kSquelchOpenLevel) {
                heard = true;
            }
        }
        if (heard) {
            m_heardSound.store(true, std::memory_order_release);
        }
        const int rate = m_receiveRateHz.load(std::memory_order_relaxed);
        const int out = resampleInto(m_receiveResampler, m_receiveResamplerRate, rate,
                                     m_mono.data(), frames);
        if (out > 0 && !writeFrames(m_resampled.data(), out)) {
            return fail();
        }
    }

    // A receiver that stopped while keyed: your voice goes on by itself.
    while (m_monitorPending.size() > static_cast<size_t>(kMonitorLeadFrames)) {
        const int frames = static_cast<int>(std::min(
            m_monitorPending.size() - static_cast<size_t>(kMonitorLeadFrames),
            static_cast<size_t>(kChunkFrames)));
        if (!writeFrames(m_silence.data(), frames)) {
            return fail();
        }
    }
    return true;
}

bool ContactRecorder::writeFrames(const float* receive, int frames)
{
    const int channels = m_sidesApart ? 2 : 1;
    const size_t need = static_cast<size_t>(frames) * static_cast<size_t>(channels);
    if (m_out.size() < need) {
        m_out.resize(need);
    }
    const int voice = static_cast<int>(std::min(m_monitorPending.size(), static_cast<size_t>(frames)));
    for (int i = 0; i < frames; ++i) {
        const float rx = receive[i];
        const float me = i < voice ? m_monitorPending[static_cast<size_t>(i)] : 0.0f;
        if (m_sidesApart) {
            m_out[static_cast<size_t>(2 * i)] = rx;
            m_out[static_cast<size_t>(2 * i + 1)] = me;
        } else {
            m_out[static_cast<size_t>(i)] = std::clamp(rx + me, -1.0f, 1.0f);
        }
    }
    if (voice > 0) {
        m_monitorPending.erase(m_monitorPending.begin(), m_monitorPending.begin() + voice);
    }
    if (!m_writer->write(m_out.data(), frames)) {
        return false;
    }
    m_framesWritten += frames;
    return true;
}

bool ContactRecorder::fail()
{
    qCWarning(lcRecordings) << "recording on slice" << m_sliceId << "stopped: the disk refused a write after"
                            << m_framesWritten << "frames";
    m_failed = true;
    // Keep what was written: the WAV header or the MP3 tail goes on now.
    if (m_writer) {
        m_writer->finish();
        m_writer.reset();
    }
    m_monitorPending.clear();
    return false;
}

bool ContactRecorder::finish()
{
    if (!m_writer) {
        return !m_failed;
    }
    if (!drain()) {
        return false;
    }
    const bool ok = m_writer->finish();
    if (!ok) {
        qCWarning(lcRecordings) << "recording on slice" << m_sliceId << "could not be finished";
        m_failed = true;
    }
    m_writer.reset();
    m_monitorPending.clear();
    return ok;
}

}  // namespace NereusSDR
