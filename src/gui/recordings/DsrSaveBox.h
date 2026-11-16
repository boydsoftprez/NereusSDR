#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/DsrSaveBox.h  (NereusSDR)
// =================================================================
//
// The box beside the hours map while SAVE's yellow bracket is up: the
// stretch's start and end (UTC), its length, about how big each choice
// would be, and "Save stretch (whole pan)", "Save sound only (MP3)" (WAV
// when the Files setting says WAV) and Cancel. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// section 3 (Saving a stretch); mockup dsr-rewind/hours.html #saveBox.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 25.
// =================================================================

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;

namespace NereusSDR {

class DsrSaveBox : public QFrame {
    Q_OBJECT

public:
    // The mockup's box: 244 px wide, 7 px left of the hours map.
    static constexpr int kWidth = 244;
    static constexpr int kGapPx = 7;
    // It sits with its top this far above the bracket's middle.
    static constexpr int kAboveMiddlePx = 55;

    explicit DsrSaveBox(QWidget* parent = nullptr);

    /// The stretch in the bracket, ms since 1970 UTC.
    void setStretch(qint64 fromUtcMs, qint64 toUtcMs);
    /// About how many bytes a millisecond of the whole pan takes on the
    /// Core (0 when not known yet), and whether sound only saves as WAV.
    void setEstimates(double panBytesPerMs, bool soundAsWav);

    qint64 fromUtcMs() const { return m_fromMs; }
    qint64 toUtcMs() const { return m_toMs; }
    QString headerText() const;
    QString detailText() const;
    /// "46 MB", or "1.2 GB" from 1000 MB up.
    static QString sizeText(double bytes);

signals:
    /// A save button was pressed: the stretch and which choice.
    void saveWanted(qint64 fromUtcMs, qint64 toUtcMs, bool wholePan);
    void cancelled();

private:
    void refresh();

    qint64 m_fromMs = 0;
    qint64 m_toMs = 0;
    double m_panBytesPerMs = 0.0;
    bool m_soundAsWav = false;
    QLabel* m_header = nullptr;
    QLabel* m_detail = nullptr;
    QPushButton* m_wholePan = nullptr;
    QPushButton* m_sound = nullptr;
};

} // namespace NereusSDR
