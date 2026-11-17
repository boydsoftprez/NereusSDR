#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/DsrRewindLink.h  (NereusSDR)
// =================================================================
//
// Rewind on one pan's waterfall: joins its SpectrumWidget to the recordings
// access (RadioModel::recordings()) for the slice the pan tunes. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md, section
// 3 (Rewind sound, Rewind picture, PTT while rewound, Moving through hours),
// 6.7, A11, A12; mockups dsr-rewind/.
//
//   Past rows    the waterfall's dsrPastRowsWanted asks the Core with the
//                pan's own FFT size, window, centre, span and pixel width
//                (requestPastRows); the answers come back to the waterfall.
//   Release      letting go of the picture in the past (strip drag, hours
//                map, typed time) plays the slice from the top row at the
//                frequency it is on: rewind(slice, moment, frequency). A
//                click in the past tunes as live; when the slice is not
//                playing the past yet it then plays from the top row too.
//   Moving       while rewound a jump on the strip, the hours map or a
//                typed time moves the sound too (seekPast).
//   LIVE         live(slice): live sound on the frequency before rewinding.
//   PTT          the Core takes a rewound slice live on the frequency played
//                when it keys; the picture goes live and LIVE stays red
//                until pressed, still returning to the old frequency.
//   SAVE         the bracketed stretch is saved on the Core
//                (saveStretch, recordingsVersion 4); what the Core says
//                about it shows on the waterfall for a few seconds.
//   Saved        a saved whole-pan stretch opened on the waterfall plays
//                like rewind even with the recorder off.
//   Not usable   an older Core, no connection or the recorder off: the
//                waterfall keeps its own minutes, nothing plays, and the
//                hours map is disabled with the reason, which the
//                waterfall shows too.
//
// None of it keys the radio.
//
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
// =================================================================

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <functional>

#include "core/recordings/dsr/DsrReplay.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

class SpectrumWidget;

class DsrRewindLink : public QObject {
    Q_OBJECT

public:
    /// `sliceForPan` names the slice this pan tunes (-1 for none), asked
    /// each time it is needed.
    DsrRewindLink(SpectrumWidget* spectrum, std::function<int()> sliceForPan, QObject* parent = nullptr);

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const { return m_access; }

    /// The pan's own view, as its display subscription sends it.
    DsrPanView panView() const;

    /// Why rewind cannot be used now, or empty.
    QString unavailableReason() const;

    static QString recorderOffReason()
    {
        return QStringLiteral("The Digital Spectrum Recorder is off. Turn it on in Setup.");
    }
    static QString noSliceReason() { return QStringLiteral("No slice on this pan to play."); }

    /// The frequency of the slice this pan tunes, in Hz, asked each time a
    /// rewind goes out. Without one the pan's own VFO is used.
    void setSliceFrequency(std::function<double()> sliceHz) { m_sliceHz = std::move(sliceHz); }

private:
    void refresh();
    void onRowsWanted(qint64 fromUtcMs, qint64 toUtcMs, int rowCount);
    void onSpectrumWanted(qint64 utcMs);
    void onPlayWanted(qint64 utcMs);
    void onSeekWanted(qint64 utcMs);
    void onLiveRequested();
    void onRefused(const QString& reason);
    void onSaveWanted(qint64 fromUtcMs, qint64 toUtcMs, bool wholePan);
    int slice() const;

    QPointer<SpectrumWidget> m_spectrum;
    std::function<int()> m_sliceForPan;
    std::function<double()> m_sliceHz;
    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    int m_rewoundSlice = -1;       // the slice the document says is rewound, -1 none
    bool m_rewindAsked = false;    // rewind sent, the document not yet showing it
    bool m_liveAsked = false;      // live sent
    bool m_rowsAsked = false;
    bool m_spectrumAsked = false;
    int m_askedSlice = -1;
    bool m_saveAsked = false;      // saveStretch sent, not yet answered
};

} // namespace NereusSDR
