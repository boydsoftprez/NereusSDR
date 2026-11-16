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
//   Past click   rewind(slice, moment, frequency): the slice moves there and
//                plays from that moment.
//   Moving       while rewound a jump on the strip, the hours map or a
//                typed time moves the sound too (seekPast).
//   LIVE         live(slice): live sound on the frequency before rewinding.
//   PTT          the Core takes a rewound slice live on the frequency played
//                when it keys; the picture goes live and LIVE stays red
//                until pressed, still returning to the old frequency.
//   Not usable   an older Core, no connection or the recorder off: the
//                waterfall keeps its own minutes and the hours map and the
//                past click are disabled with the reason.
//
// None of it keys the radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 24.
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

private:
    void refresh();
    void onRowsWanted(qint64 fromUtcMs, qint64 toUtcMs, int rowCount);
    void onSpectrumWanted(qint64 utcMs);
    void onSignalClicked(qint64 utcMs, double hz);
    void onSeekWanted(qint64 utcMs);
    void onLiveRequested();
    void onRefused(const QString& reason);
    int slice() const;

    QPointer<SpectrumWidget> m_spectrum;
    std::function<int()> m_sliceForPan;
    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    int m_rewoundSlice = -1;       // the slice the document says is rewound, -1 none
    bool m_rewindAsked = false;    // rewind sent, the document not yet showing it
    bool m_liveAsked = false;      // live sent
    bool m_rowsAsked = false;
    bool m_spectrumAsked = false;
    int m_askedSlice = -1;
};

} // namespace NereusSDR
