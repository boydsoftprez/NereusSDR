// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/DsrRewindLink.cpp  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 24.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF): rewind plays on release (his design
//                 change); the past click is gone, refusals show on the
//                 waterfall. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF): any tune of the slice in the past
//                 plays from the top row (sliceTuned); playing, a frequency
//                 outside what was kept then says so on the waterfall.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/recordings/DsrRewindLink.h"

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/FFTEngine.h"
#include "core/recordings/RecordingService.h"
#include "gui/SpectrumWidget.h"
#include "models/recordings/RecordingsAccess.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr qint64 kNsPerMs = 1000000LL;
// A pan with no engine of its own yet asks at the display's default size.
constexpr int kFallbackFftSize = 4096;

} // namespace

DsrRewindLink::DsrRewindLink(SpectrumWidget* spectrum, std::function<int()> sliceForPan, QObject* parent)
    : QObject(parent)
    , m_spectrum(spectrum)
    , m_sliceForPan(std::move(sliceForPan))
{
    setObjectName(QStringLiteral("DsrRewindLink"));
    if (!spectrum) {
        return;
    }
    connect(spectrum, &SpectrumWidget::dsrPastRowsWanted, this, &DsrRewindLink::onRowsWanted);
    connect(spectrum, &SpectrumWidget::dsrPastSpectrumWanted, this, &DsrRewindLink::onSpectrumWanted);
    connect(spectrum, &SpectrumWidget::dsrPastPlayWanted, this, &DsrRewindLink::onPlayWanted);
    connect(spectrum, &SpectrumWidget::dsrPastSeekWanted, this, &DsrRewindLink::onSeekWanted);
    connect(spectrum, &SpectrumWidget::dsrLiveRequested, this, &DsrRewindLink::onLiveRequested);
    connect(spectrum, &SpectrumWidget::dsrSaveStretchWanted, this, &DsrRewindLink::onSaveWanted);
    connect(spectrum, &SpectrumWidget::dsrHoursWanted, this, [this] {
        if (m_access && unavailableReason().isEmpty()) {
            m_access->requestHours();
        }
    });
}

void DsrRewindLink::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : std::as_const(m_accessConnections)) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_rewoundSlice = -1;
    m_rewindAsked = false;
    m_liveAsked = false;
    m_rowsAsked = false;
    m_spectrumAsked = false;
    m_saveAsked = false;
    m_keptCentreHz = 0;
    m_keptSpanHz = 0;
    m_outsideKept = false;
    if (access != nullptr) {
        m_accessConnections = {
            connect(access, &RecordingsAccess::documentChanged, this, &DsrRewindLink::refresh),
            connect(access, &RecordingsAccess::stateChanged, this, &DsrRewindLink::refresh),
            connect(access, &RecordingsAccess::refused, this, &DsrRewindLink::onRefused),
            connect(access, &RecordingsAccess::pastRowsReady, this,
                    [this](const DsrPastFrames& rows) {
                        if (!m_rowsAsked || rows.slice != m_askedSlice || !m_spectrum) {
                            return;
                        }
                        if (rows.rowsLeft <= 0) {
                            m_rowsAsked = false;
                        }
                        m_spectrum->addDsrPastRows(rows);
                    }),
            connect(access, &RecordingsAccess::pastSpectrumReady, this,
                    [this](const DsrPastFrames& frame) {
                        if (!m_spectrumAsked || frame.slice != slice() || !m_spectrum) {
                            return;
                        }
                        m_spectrumAsked = false;
                        m_spectrum->setDsrPastSpectrum(frame);
                    }),
            connect(access, &RecordingsAccess::stretchSaving, this,
                    [this](const QString& note) {
                        if (m_saveAsked && m_spectrum) {
                            m_saveAsked = false;
                            m_spectrum->showDsrNote(note);
                        }
                    }),
            connect(access, &RecordingsAccess::hoursReady, this,
                    [this](const QList<DsrHour>& hours) {
                        if (m_spectrum) {
                            m_spectrum->setDsrHours(hours);
                        }
                    }),
        };
    }
    refresh();
}

int DsrRewindLink::slice() const
{
    return m_sliceForPan ? m_sliceForPan() : -1;
}

qint64 DsrRewindLink::sliceHz() const
{
    if (m_sliceHz) {
        return std::llround(m_sliceHz());
    }
    return m_spectrum ? std::llround(m_spectrum->vfoFrequency()) : 0;
}

void DsrRewindLink::sliceTuned()
{
    if (!m_spectrum || m_spectrum->wfLive()) {
        return;
    }
    if (!m_spectrum->dsrRewound()) {
        // In the past and not playing (a refusal, or the picture moved
        // without playing yet): play from the top row at the new frequency.
        m_spectrum->dsrPlayFromTop();
        return;
    }
    showWhetherKeptHere();
}

void DsrRewindLink::showWhetherKeptHere()
{
    if (!m_spectrum) {
        return;
    }
    // As the Core's rewind tests a receiver's span (RecordingService::rewind).
    const bool outside = m_rewoundSlice >= 0 && m_rewoundSlice == slice() && !m_rewindAsked
        && m_keptSpanHz > 0 && m_spectrum->dsrRewound()
        && std::llabs(sliceHz() - m_keptCentreHz) * 2 > m_keptSpanHz;
    if (outside) {
        m_spectrum->dsrPlayRefused(QString::fromLatin1(RecordingService::kRewindNoFrequencyReason));
    } else if (m_outsideKept) {
        m_spectrum->dsrPlayRefused(QString());
    }
    m_outsideKept = outside;
}

QString DsrRewindLink::unavailableReason() const
{
    if (!m_access) {
        return RecordingsAccess::notConnectedReason();
    }
    const QString version = m_access->unavailableReason(RecordingsAccess::kDsrVersion);
    if (!version.isEmpty()) {
        return version;
    }
    const QJsonObject dsr = m_access->document().value(QStringLiteral("dsr")).toObject();
    if (dsr.value(QStringLiteral("stretch")).isObject()) {
        return {};   // a saved stretch is open: it plays with the recorder off
    }
    if (!dsr.value(QStringLiteral("enabled")).toBool(false)) {
        return recorderOffReason();
    }
    const QString reason = dsr.value(QStringLiteral("reason")).toString();
    if (!reason.isEmpty()) {
        return reason;   // the recorder's own: an SD card, a missing disk
    }
    return {};
}

DsrPanView DsrRewindLink::panView() const
{
    DsrPanView view;
    if (!m_spectrum) {
        return view;
    }
    const int remote = m_spectrum->remoteGrantedFftSize();
    const int local = m_spectrum->localFftSize();
    int size = remote > 0 ? remote : local;
    if (size < DsrPastRenderer::kMinFftSize || size > DsrPastRenderer::kMaxFftSize || (size & (size - 1)) != 0) {
        size = kFallbackFftSize;
    }
    view.fftSize = size;
    // As the pan's display subscription takes its window
    // (RemoteMediaController requestFor).
    view.windowType = std::clamp(AppSettings::instance()
                                     .value(QLatin1String(ControlRanges::kDisplayFftWindowKey),
                                            QString::number(ControlRanges::kDisplayFftWindowDefault))
                                     .toString()
                                     .toInt(),
                                 0, int(WindowFunction::Count) - 1);
    view.centreHz = std::llround(m_spectrum->centerFrequency());
    view.spanHz = std::llround(m_spectrum->bandwidth());
    view.pixels = std::clamp(m_spectrum->width() - m_spectrum->reservedRightEdgeWidth(), 1,
                             DsrPastRenderer::kMaxPixels);
    return view;
}

void DsrRewindLink::refresh()
{
    if (!m_spectrum) {
        return;
    }
    const QString why = unavailableReason();
    const QJsonObject dsr = m_access ? m_access->document().value(QStringLiteral("dsr")).toObject()
                                     : QJsonObject();
    // A saved stretch that is open reaches as far back as it does.
    qint64 oldestMs = dsr.value(QStringLiteral("oldestUtcMs")).toInteger(0);
    const QJsonObject stretch = dsr.value(QStringLiteral("stretch")).toObject();
    const qint64 stretchFromMs = stretch.value(QStringLiteral("fromUtcNs")).toInteger(0) / kNsPerMs;
    if (stretchFromMs > 0) {
        oldestMs = oldestMs > 0 ? std::min(oldestMs, stretchFromMs) : stretchFromMs;
    }
    m_spectrum->setDsrRewindAvailable(why.isEmpty(), why, oldestMs);
    // The save box's sizes: what the recorder keeps per millisecond, and
    // the Files format for sound only.
    const qint64 keptMs = dsr.value(QStringLiteral("newestUtcMs")).toInteger(0)
        - dsr.value(QStringLiteral("oldestUtcMs")).toInteger(0);
    const double usedBytes = dsr.value(QStringLiteral("usedBytes")).toDouble(0.0);
    const QJsonObject settings = m_access ? m_access->document().value(QStringLiteral("settings")).toObject()
                                          : QJsonObject();
    m_spectrum->setDsrSaveEstimates(keptMs > 0 ? usedBytes / double(keptMs) : 0.0,
                                    settings.value(QStringLiteral("format")).toString()
                                        == QLatin1String("wav"));
    const int s = slice();
    bool listed = false;
    qint64 playedNs = 0;
    m_keptCentreHz = 0;
    m_keptSpanHz = 0;
    if (s >= 0) {
        for (const QJsonValue& v : dsr.value(QStringLiteral("rewound")).toArray()) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("slice")).toInt(-1) == s) {
                listed = true;
                playedNs = o.value(QStringLiteral("utcNs")).toInteger(0);
                m_keptCentreHz = o.value(QStringLiteral("keptCentreHz")).toInteger(0);
                m_keptSpanHz = o.value(QStringLiteral("keptSpanHz")).toInteger(0);
            }
        }
    }
    if (listed) {
        m_rewindAsked = false;
        m_liveAsked = false;
        m_rewoundSlice = s;
        m_spectrum->setDsrLiveReturnPending(false);
        m_spectrum->setDsrRewound(true, playedNs / kNsPerMs);
        // setDsrRewound clears the words on the waterfall; a frequency
        // outside what was kept then puts them back.
        m_outsideKept = false;
        showWhetherKeptHere();
        return;
    }
    m_outsideKept = false;
    if (m_rewoundSlice >= 0 && !m_rewindAsked) {
        // Rewound before, live now: LIVE, or PTT took it live on the
        // frequency played. After PTT, LIVE still returns to the frequency
        // before rewinding (the Core keeps it), so it stays red.
        const bool byLive = m_liveAsked;
        m_rewoundSlice = -1;
        m_liveAsked = false;
        m_spectrum->setDsrRewound(false);
        m_spectrum->returnWaterfallToLive();
        m_spectrum->setDsrLiveReturnPending(!byLive);
    }
}

void DsrRewindLink::onRowsWanted(qint64 fromUtcMs, qint64 toUtcMs, int rowCount)
{
    const int s = slice();
    if (!m_access || s < 0 || !unavailableReason().isEmpty()) {
        if (m_spectrum) {
            m_spectrum->dsrPastRowsRefused(unavailableReason());
        }
        return;
    }
    m_rowsAsked = true;
    m_askedSlice = s;
    m_access->requestPastRows(s, panView(), fromUtcMs * kNsPerMs, toUtcMs * kNsPerMs, rowCount);
}

void DsrRewindLink::onSpectrumWanted(qint64 utcMs)
{
    const int s = slice();
    if (!m_access || s < 0) {
        if (m_spectrum) {
            m_spectrum->dsrPastSpectrumRefused();
        }
        return;
    }
    m_spectrumAsked = true;
    m_access->requestPastSpectrum(s, panView(), utcMs * kNsPerMs);
}

void DsrRewindLink::onPlayWanted(qint64 utcMs)
{
    if (!m_spectrum) {
        return;
    }
    const int s = slice();
    if (!m_access || !unavailableReason().isEmpty()) {
        // The waterfall shows why already.
        m_spectrum->setDsrRewound(false);
        return;
    }
    const qint64 hz = sliceHz();
    if (s < 0 || hz <= 0) {
        m_spectrum->setDsrRewound(false);
        m_spectrum->dsrPlayRefused(noSliceReason());
        return;
    }
    m_rewindAsked = true;
    m_rewoundSlice = s;
    m_access->rewind(s, utcMs * kNsPerMs, hz);
}

void DsrRewindLink::onSeekWanted(qint64 utcMs)
{
    const int s = slice();
    if (m_access && s >= 0 && m_rewoundSlice == s) {
        m_access->seekPast(s, utcMs * kNsPerMs);
    }
}

void DsrRewindLink::onLiveRequested()
{
    const int s = slice();
    const bool pending = m_spectrum && m_spectrum->dsrLiveReturnPending();
    if (m_spectrum) {
        m_spectrum->setDsrLiveReturnPending(false);
    }
    if (!m_access || s < 0) {
        return;
    }
    if (m_rewoundSlice == s || pending) {
        m_liveAsked = true;
        m_rewindAsked = false;
        m_access->live(s);
    }
}

void DsrRewindLink::onSaveWanted(qint64 fromUtcMs, qint64 toUtcMs, bool wholePan)
{
    const QString why = unavailableReason();
    if (!m_access || !why.isEmpty()) {
        if (m_spectrum) {
            m_spectrum->showDsrNote(why);
        }
        return;
    }
    m_saveAsked = true;
    m_access->saveStretch(fromUtcMs * kNsPerMs, toUtcMs * kNsPerMs, wholePan);
}

void DsrRewindLink::onRefused(const QString& reason)
{
    if (!m_spectrum) {
        return;
    }
    if (m_saveAsked) {
        m_saveAsked = false;
        m_spectrum->showDsrNote(reason);
        return;
    }
    if (m_rewindAsked) {
        // It could not play: the picture stays where it is, in the past,
        // with why on the waterfall.
        m_rewindAsked = false;
        m_rewoundSlice = -1;
        m_spectrum->setDsrRewound(false);
        m_spectrum->dsrPlayRefused(reason);
        return;
    }
    if (m_rowsAsked) {
        m_rowsAsked = false;
        m_spectrum->dsrPastRowsRefused(reason);
        return;
    }
    if (m_spectrumAsked) {
        m_spectrumAsked = false;
        m_spectrum->dsrPastSpectrumRefused();
    }
}

} // namespace NereusSDR
