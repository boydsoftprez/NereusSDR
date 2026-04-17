// =================================================================
// src/gui/widgets/VfoModeContainers.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/setup.designer.cs
//   Project Files/Source/Console/radio.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  Joe (WD5Y) — console.cs UI tweaks / mute / ForeColor
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
//                 Mode sub-widget containers lifted from AetherSDR
//                 `src/gui/VfoWidget.cpp:996-1300`.
// =================================================================

// NereusSDR native mode container widgets; Qt skeleton patterns informed
// by AetherSDR src/gui/VfoWidget.cpp:996-1300 (mode sub-widget blocks).
// DSP behavior bound to the S1.6 SliceModel stub API.
// No WDSP forwarding in this file — that is Stage 2.

#include "VfoModeContainers.h"
#include "GuardedComboBox.h"
#include "ScrollableLabel.h"
#include "TriBtn.h"
#include "VfoStyles.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

namespace NereusSDR {

// ── CTCSS tone list ────────────────────────────────────────────────────────
// 41-entry ITU standard CTCSS sub-tone table.
// AetherSDR VfoWidget.cpp uses the same list; values are an ITU/TIA standard
// so they are not invented here — they are authoritative reference values.
static const double kCtcssTones[] = {
     67.0,  71.9,  74.4,  77.0,  79.7,
     82.5,  85.4,  88.5,  91.5,  94.8,
     97.4, 100.0, 103.5, 107.2, 110.9,
    114.8, 118.8, 123.0, 127.3, 131.8,
    136.5, 141.3, 146.2, 151.4, 156.7,
    162.2, 167.9, 173.8, 179.9, 186.2,
    192.8, 203.5, 206.5, 210.7, 218.1,
    225.7, 229.1, 233.6, 241.8, 250.3,
    254.1
};
static constexpr int kCtcssCount = static_cast<int>(sizeof(kCtcssTones) / sizeof(kCtcssTones[0]));

// ── RttyMarkShiftContainer step constants ─────────────────────────────────
// AetherSDR VfoWidget.cpp uses 25 Hz step for Mark and 5 Hz step for Shift.
// These are UX choices, not DSP constants — native NereusSDR values.
static constexpr int kMarkStep  = 25;
static constexpr int kShiftStep = 5;
static constexpr int kDigStep   = 10;

// ══════════════════════════════════════════════════════════════════════════
//  FmOptContainer
// ══════════════════════════════════════════════════════════════════════════

FmOptContainer::FmOptContainer(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void FmOptContainer::buildUi()
{
    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // ── Row 1: tone mode combo + tone value combo ─────────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(4);

        m_toneModeCmb = new GuardedComboBox(this);
        m_toneModeCmb->setObjectName("toneModeCmb");
        m_toneModeCmb->addItem(QStringLiteral("Off"),          QVariant(0));
        m_toneModeCmb->addItem(QStringLiteral("CTCSS Encode"), QVariant(1));
        m_toneModeCmb->addItem(QStringLiteral("CTCSS Decode"), QVariant(2));
        m_toneModeCmb->addItem(QStringLiteral("CTCSS Enc+Dec"),QVariant(3));

        m_toneValueCmb = new GuardedComboBox(this);
        m_toneValueCmb->setObjectName("toneValueCmb");
        for (int i = 0; i < kCtcssCount; ++i) {
            const QString text = QString::number(kCtcssTones[i], 'f', 1);
            m_toneValueCmb->addItem(text, QVariant(text));
        }

        m_toneModeCmb->setToolTip(QStringLiteral("CTCSS tone mode (Off / Encode / Decode / Enc+Dec)"));
        m_toneValueCmb->setToolTip(QStringLiteral("CTCSS sub-audible tone frequency (Hz)"));
        row->addWidget(m_toneModeCmb, 1);
        row->addWidget(m_toneValueCmb, 1);
        vbox->addLayout(row);
    }

    // ── Row 2: offset label + spinbox ─────────────────────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(4);

        QLabel* offsetLbl = new QLabel(QStringLiteral("Offset:"), this);
        offsetLbl->setStyleSheet(kLabelStyle.toString());

        m_offsetKhzSpin = new QSpinBox(this);
        m_offsetKhzSpin->setObjectName("offsetKhzSpin");
        m_offsetKhzSpin->setRange(0, 10000);
        m_offsetKhzSpin->setSuffix(QStringLiteral(" kHz"));
        m_offsetKhzSpin->setSingleStep(50);

        row->addWidget(offsetLbl);
        row->addWidget(m_offsetKhzSpin, 1);
        vbox->addLayout(row);
    }

    // ── Row 3: TX direction buttons + Reverse toggle ──────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(4);

        m_txLowBtn   = new QPushButton(QStringLiteral("\u2212"), this);  // "−"
        m_simplexBtn = new QPushButton(QStringLiteral("Simplex"), this);
        m_txHighBtn  = new QPushButton(QStringLiteral("+"), this);
        m_revBtn     = new QPushButton(QStringLiteral("Rev"), this);

        m_txLowBtn->setObjectName("txLowBtn");
        m_txLowBtn->setToolTip(QStringLiteral("TX below RX (repeater Low offset) — Phase 3M-1"));
        m_simplexBtn->setObjectName("simplexBtn");
        m_simplexBtn->setToolTip(QStringLiteral("Simplex — TX on same frequency as RX"));
        m_txHighBtn->setObjectName("txHighBtn");
        m_txHighBtn->setToolTip(QStringLiteral("TX above RX (repeater High offset) — Phase 3M-1"));
        m_revBtn->setObjectName("revBtn");
        m_revBtn->setToolTip(QStringLiteral("Reverse — listen on the repeater output frequency"));

        for (QPushButton* btn : {m_txLowBtn, m_simplexBtn, m_txHighBtn, m_revBtn}) {
            btn->setCheckable(true);
            btn->setStyleSheet(kDspToggle.toString());
        }

        row->addWidget(m_txLowBtn);
        row->addWidget(m_simplexBtn);
        row->addWidget(m_txHighBtn);
        row->addWidget(m_revBtn);
        vbox->addLayout(row);
    }

    // ── Signal connections ────────────────────────────────────────────────

    // TODO Phase 3M-3: CTCSS decode requires a sub-audible tone detector
    // (bandpass at ctcssValueHz + threshold). WDSP FMSQ handles noise-level
    // squelch only, not tone-coded squelch. The SliceModel properties
    // (fmCtcssMode, fmCtcssValueHz) store the user's selection for when
    // the tone detector is implemented.

    connect(m_toneModeCmb,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (!m_slice) { return; }
        m_slice->setFmCtcssMode(m_toneModeCmb->itemData(idx).toInt());
    });

    connect(m_toneValueCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (!m_slice) { return; }
        const double hz = m_toneValueCmb->itemData(idx).toString().toDouble();
        m_slice->setFmCtcssValueHz(hz);
    });

    connect(m_offsetKhzSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int kHz) {
        if (!m_slice) { return; }
        m_slice->setFmOffsetHz(kHz * 1000);
    });

    // Phase 3M-1: wire TX CTCSS encode and keydown repeater shift
    connect(m_txLowBtn, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setFmTxMode(FmTxMode::Low);
        m_txLowBtn->setChecked(true);
        m_simplexBtn->setChecked(false);
        m_txHighBtn->setChecked(false);
    });

    // Phase 3M-1: wire TX CTCSS encode and keydown repeater shift
    connect(m_simplexBtn, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setFmTxMode(FmTxMode::Simplex);
        m_txLowBtn->setChecked(false);
        m_simplexBtn->setChecked(true);
        m_txHighBtn->setChecked(false);
        // Simplex: no repeater offset. The offset spinbox retains its last
        // non-simplex value (Thetis console.cs:40412 chkFMTXSimplex_CheckedChanged
        // does not zero udFMOffset). TX effects deferred to Phase 3M-1.
    });

    // Phase 3M-1: wire TX CTCSS encode and keydown repeater shift
    connect(m_txHighBtn, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setFmTxMode(FmTxMode::High);
        m_txLowBtn->setChecked(false);
        m_simplexBtn->setChecked(false);
        m_txHighBtn->setChecked(true);
    });

    // fmReverse is display-only in 3G-10. TX repeater listen reversal
    // (swapping TX/RX frequencies on keydown) is Phase 3M-1.
    connect(m_revBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_slice) { return; }
        m_slice->setFmReverse(checked);
    });
}

void FmOptContainer::setSlice(SliceModel* s)
{
    m_slice = s;
    syncFromSlice();
}

void FmOptContainer::syncFromSlice()
{
    if (!m_slice) { return; }

    const QSignalBlocker b1(m_toneModeCmb);
    const QSignalBlocker b2(m_toneValueCmb);
    const QSignalBlocker b3(m_offsetKhzSpin);

    // Tone mode: find the index whose itemData == m_slice->fmCtcssMode()
    const int mode = m_slice->fmCtcssMode();
    for (int i = 0; i < m_toneModeCmb->count(); ++i) {
        if (m_toneModeCmb->itemData(i).toInt() == mode) {
            m_toneModeCmb->setCurrentIndex(i);
            break;
        }
    }

    // Tone value: find the index whose text matches formatted fmCtcssValueHz
    const QString wantedTone = QString::number(m_slice->fmCtcssValueHz(), 'f', 1);
    const int toneIdx = m_toneValueCmb->findText(wantedTone);
    if (toneIdx >= 0) {
        m_toneValueCmb->setCurrentIndex(toneIdx);
    }

    // Offset: stored Hz → display kHz
    m_offsetKhzSpin->setValue(m_slice->fmOffsetHz() / 1000);

    // Direction buttons (mutually exclusive)
    const FmTxMode txMode = m_slice->fmTxMode();
    m_txLowBtn->setChecked(txMode == FmTxMode::Low);
    m_simplexBtn->setChecked(txMode == FmTxMode::Simplex);
    m_txHighBtn->setChecked(txMode == FmTxMode::High);

    // Reverse toggle (independent)
    m_revBtn->setChecked(m_slice->fmReverse());
}


// ══════════════════════════════════════════════════════════════════════════
//  DigOffsetContainer
// ══════════════════════════════════════════════════════════════════════════

DigOffsetContainer::DigOffsetContainer(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void DigOffsetContainer::buildUi()
{
    QHBoxLayout* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(4);

    m_minusBtn   = new TriBtn(TriBtn::Left, this);
    m_offsetLabel = new ScrollableLabel(this);
    m_plusBtn    = new TriBtn(TriBtn::Right, this);

    m_minusBtn->setObjectName("minusBtn");
    m_minusBtn->setToolTip(QStringLiteral("Decrease DIG offset"));
    m_offsetLabel->setObjectName("offsetLabel");
    m_plusBtn->setObjectName("plusBtn");
    m_plusBtn->setToolTip(QStringLiteral("Increase DIG offset"));

    m_offsetLabel->setRange(-10000, 10000);
    m_offsetLabel->setStep(kDigStep);
    m_offsetLabel->setValue(0);
    m_offsetLabel->setFormat([](int v) -> QString {
        if (v > 0) { return QStringLiteral("+%1 Hz").arg(v); }
        return QStringLiteral("%1 Hz").arg(v);
    });

    row->addWidget(m_minusBtn);
    row->addWidget(m_offsetLabel, 1);
    row->addWidget(m_plusBtn);

    // ── Signal connections ────────────────────────────────────────────────

    connect(m_minusBtn, &QPushButton::clicked, this, [this]() {
        applyOffset(currentOffsetHz() - kDigStep);
    });

    connect(m_plusBtn, &QPushButton::clicked, this, [this]() {
        applyOffset(currentOffsetHz() + kDigStep);
    });

    connect(m_offsetLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        applyOffset(hz);
    });
}

int DigOffsetContainer::currentOffsetHz() const
{
    if (!m_slice) { return 0; }
    if (m_slice->dspMode() == DSPMode::DIGL) { return m_slice->diglOffsetHz(); }
    return m_slice->diguOffsetHz();  // DIGU or fallback
}

void DigOffsetContainer::applyOffset(int hz)
{
    if (!m_slice) { return; }
    if (m_slice->dspMode() == DSPMode::DIGL) {
        m_slice->setDiglOffsetHz(hz);
    } else {
        m_slice->setDiguOffsetHz(hz);
    }
}

void DigOffsetContainer::setSlice(SliceModel* s)
{
    m_slice = s;
    syncFromSlice();
}

void DigOffsetContainer::syncFromSlice()
{
    if (!m_slice) { return; }
    const QSignalBlocker b(m_offsetLabel);
    m_offsetLabel->setValue(currentOffsetHz());
}


// ══════════════════════════════════════════════════════════════════════════
//  RttyMarkShiftContainer
// ══════════════════════════════════════════════════════════════════════════

RttyMarkShiftContainer::RttyMarkShiftContainer(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void RttyMarkShiftContainer::buildUi()
{
    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // ── Header row: "Mark" / "Shift" labels ──────────────────────────────
    {
        QHBoxLayout* hdr = new QHBoxLayout;
        hdr->setSpacing(4);

        QLabel* markHdr  = new QLabel(QStringLiteral("Mark"),  this);
        QLabel* shiftHdr = new QLabel(QStringLiteral("Shift"), this);
        markHdr->setStyleSheet(kLabelStyle.toString());
        shiftHdr->setStyleSheet(kLabelStyle.toString());
        markHdr->setAlignment(Qt::AlignCenter);
        shiftHdr->setAlignment(Qt::AlignCenter);

        hdr->addWidget(markHdr,  1);
        hdr->addWidget(shiftHdr, 1);
        vbox->addLayout(hdr);
    }

    // ── Control row: Mark sub-group + gap + Shift sub-group ──────────────
    {
        QHBoxLayout* ctrl = new QHBoxLayout;
        ctrl->setSpacing(4);

        // Mark sub-group
        m_markMinus = new TriBtn(TriBtn::Left, this);
        m_markLabel = new ScrollableLabel(this);
        m_markPlus  = new TriBtn(TriBtn::Right, this);

        m_markMinus->setObjectName("markMinus");
        m_markMinus->setToolTip(QStringLiteral("Decrease RTTY mark frequency"));
        m_markLabel->setObjectName("markLabel");
        m_markPlus->setObjectName("markPlus");
        m_markPlus->setToolTip(QStringLiteral("Increase RTTY mark frequency"));

        // From Thetis setup.designer.cs:40635 — RTTY mark default 2295 Hz
        m_markLabel->setRange(1000, 3500);
        m_markLabel->setStep(kMarkStep);
        m_markLabel->setValue(2295);

        ctrl->addWidget(m_markMinus);
        ctrl->addWidget(m_markLabel, 1);
        ctrl->addWidget(m_markPlus);

        ctrl->addSpacing(4);

        // Shift sub-group
        m_shiftMinus = new TriBtn(TriBtn::Left, this);
        m_shiftLabel = new ScrollableLabel(this);
        m_shiftPlus  = new TriBtn(TriBtn::Right, this);

        m_shiftMinus->setObjectName("shiftMinus");
        m_shiftMinus->setToolTip(QStringLiteral("Decrease RTTY shift spacing"));
        m_shiftLabel->setObjectName("shiftLabel");
        m_shiftPlus->setObjectName("shiftPlus");
        m_shiftPlus->setToolTip(QStringLiteral("Increase RTTY shift spacing"));

        // From Thetis radio.cs:2043-2044 — shift = mark(2295) − space(2125) = 170 Hz
        m_shiftLabel->setRange(50, 1000);
        m_shiftLabel->setStep(kShiftStep);
        m_shiftLabel->setValue(170);

        ctrl->addWidget(m_shiftMinus);
        ctrl->addWidget(m_shiftLabel, 1);
        ctrl->addWidget(m_shiftPlus);

        vbox->addLayout(ctrl);
    }

    // ── Signal connections ────────────────────────────────────────────────

    connect(m_markMinus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyMarkHz(m_slice->rttyMarkHz() - kMarkStep);
    });

    connect(m_markPlus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyMarkHz(m_slice->rttyMarkHz() + kMarkStep);
    });

    connect(m_markLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_slice) { return; }
        m_slice->setRttyMarkHz(hz);
    });

    connect(m_shiftMinus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyShiftHz(m_slice->rttyShiftHz() - kShiftStep);
    });

    connect(m_shiftPlus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyShiftHz(m_slice->rttyShiftHz() + kShiftStep);
    });

    connect(m_shiftLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_slice) { return; }
        m_slice->setRttyShiftHz(hz);
    });
}

void RttyMarkShiftContainer::setSlice(SliceModel* s)
{
    m_slice = s;
    syncFromSlice();
}

void RttyMarkShiftContainer::syncFromSlice()
{
    if (!m_slice) { return; }
    const QSignalBlocker b1(m_markLabel);
    const QSignalBlocker b2(m_shiftLabel);
    m_markLabel->setValue(m_slice->rttyMarkHz());
    m_shiftLabel->setValue(m_slice->rttyShiftHz());
}

// ── CW autotune (S2.10c) ──────────────────────────────────────────────────
//
// TODO (deferred — no WDSP API):
// CW autotune would require a pitch detection algorithm that identifies
// the received CW carrier frequency and applies a correction offset.
//
// Investigation of matchedCW.h (third_party/wdsp/src/matchedCW.h) shows
// that the "matched" module is a Gaussian partitioned-overlap-save filter
// with SetRXAMatchedRun/Freqs/Gain API — it is the APF "selection=1" type,
// not a CW tone detector or autotune controller.
//
// Implementing CW autotune natively would require:
//   1. A pitch detector (e.g., FFT peak-finder or autocorrelation) in the
//      audio-frequency range (~200–900 Hz for typical CW pitches)
//   2. A frequency error signal fed back to RxChannel::setShiftFrequency()
//      or slice->setRitHz() to center the tone on the APF frequency
//   3. A one-shot or polling mode (QTimer @ ~500ms per original plan)
//
// This is a substantial addition beyond the current phase scope.
// Deferred to a future phase with an explicit native design document.

}  // namespace NereusSDR
