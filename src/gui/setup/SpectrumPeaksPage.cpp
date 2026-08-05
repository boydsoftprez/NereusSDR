// =================================================================
// src/gui/setup/SpectrumPeaksPage.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// NereusSDR-original page structure.  Constants and property defaults
// reference Thetis display.cs:
//   display.cs:4395-4419  ShowPeakBlobs / NumberOfPeakBlobs [v2.10.3.13]
//   display.cs:4593-4714  BlobPeakHold / BlobPeakHoldMS / BlobPeakHoldDrop / PeakBlobFall [v2.10.3.13]
//   display.cs:8434-8435  m_bDX2_PeakBlob = OrangeRed, m_bDX2_PeakBlobText = Chartreuse [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Skeleton created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "SpectrumPeaksPage.h"
#include "gui/ColorSwatchButton.h"
#include "gui/SpectrumWidget.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

namespace NereusSDR {

namespace {

void applyDarkStyle(QWidget* w)
{
    w->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #8090a0; font-size: 11px;"
        "  border: 1px solid #203040; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #c8d8e8; }"
        "QSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    ));
}

} // anonymous namespace

SpectrumPeaksPage::SpectrumPeaksPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Spectrum Peaks"), model, parent)
{
    buildUI();

    // ── Load persisted values ─────────────────────────────────────────────────
    auto& s = AppSettings::instance();

    // Active Peak Hold
    m_aphEnable->setChecked(
        s.value(QStringLiteral("DisplayActivePeakHoldEnabled"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    m_aphDurationMs->setValue(
        s.value(QStringLiteral("DisplayActivePeakHoldDurationMs"), 2000).toInt());
    m_aphDropDbPerSec->setValue(
        s.value(QStringLiteral("DisplayActivePeakHoldDropDbPerSec"), 6).toInt());
    m_aphFill->setChecked(
        s.value(QStringLiteral("DisplayActivePeakHoldFill"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    m_aphOnTx->setChecked(
        s.value(QStringLiteral("DisplayActivePeakHoldOnTx"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    // NereusSDR-original — distinct trace colour. Default gold (#FFD700FF)
    // contrasts against typical clarity-blue spectrum and pure-white
    // Smooth-Defaults data line. Persisted format is "#RRGGBBAA".
    m_aphColor->setColor(ColorSwatchButton::colorFromHex(
        s.value(QStringLiteral("DisplayActivePeakHoldColor"),
                QStringLiteral("#FFD700FF")).toString()));

    // Peak Blobs
    // Thetis Display.cs:4395 [v2.10.3.13] ships m_bPeakBlobMaximums = true.
    // NereusSDR deviation: default OFF so first-launch is a clean panadapter.
    m_blobEnable->setChecked(
        s.value(QStringLiteral("DisplayPeakBlobsEnabled"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    // From Thetis Display.cs:4407 [v2.10.3.13] m_nNumberOfMaximums = 3
    m_blobCount->setValue(
        s.value(QStringLiteral("DisplayPeakBlobsCount"), 3).toInt());
    // From Thetis Display.cs:4401 [v2.10.3.13] m_bInsideFilterOnly = false
    m_blobInsideFilter->setChecked(
        s.value(QStringLiteral("DisplayPeakBlobsInsideFilterOnly"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    // From Thetis Display.cs:4593 [v2.10.3.13] m_bBlobPeakHold = false
    m_blobHoldEnable->setChecked(
        s.value(QStringLiteral("DisplayPeakBlobsHoldEnabled"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    // From Thetis Display.cs:4599 [v2.10.3.13] m_fBlobPeakHoldMS = 500
    m_blobHoldMs->setValue(
        s.value(QStringLiteral("DisplayPeakBlobsHoldMs"), 500).toInt());
    // From Thetis Display.cs:4605 [v2.10.3.13] m_bBlobPeakHoldDrop = false
    m_blobHoldDrop->setChecked(
        s.value(QStringLiteral("DisplayPeakBlobsHoldDrop"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    // From Thetis Display.cs:4697 [v2.10.3.13] m_dBmPerSecondPeakBlobFall = 6.0f
    m_blobFallDbPerSec->setValue(
        s.value(QStringLiteral("DisplayPeakBlobsFallDbPerSec"), 6).toInt());
    // From Thetis display.cs:8434 [v2.10.3.13] m_bDX2_PeakBlob = OrangeRed (#FF4500, fully opaque).
    // Upstream tags preserved: //MW0LGE (from cited display.cs:8429) [v2.10.3.15]
    // Persisted format is "#RRGGBBAA" via ColorSwatchButton::colorToHex; use the
    // matching colorFromHex helper so alpha lands in the right channel.
    m_blobColor->setColor(ColorSwatchButton::colorFromHex(
        s.value(QStringLiteral("DisplayPeakBlobColor"), QStringLiteral("#FF4500FF")).toString()));
    // From Thetis display.cs:8435 [v2.10.3.13] m_bDX2_PeakBlobText = Chartreuse (#7FFF00, fully opaque)
    m_blobTextColor->setColor(ColorSwatchButton::colorFromHex(
        s.value(QStringLiteral("DisplayPeakBlobTextColor"), QStringLiteral("#7FFF00FF")).toString()));

    // ── Apply persisted values to SpectrumWidget (Tasks 2.5 + 2.6) ─────────
    // Push loaded settings into the widget so Active Peak Hold and Peak Blobs
    // are active on first launch even before any control is touched.
    // Note: 'model' here is the constructor parameter (RadioModel*), not the
    // SetupPage::model() accessor (which is the same pointer but we must
    // disambiguate from the parameter name in this scope).
    if (model != nullptr) {
        if (auto* sw = model->spectrumWidget()) {
            // Active Peak Hold
            sw->setActivePeakHoldEnabled(m_aphEnable->isChecked());
            sw->setActivePeakHoldDurationMs(m_aphDurationMs->value());
            sw->setActivePeakHoldDropDbPerSec(static_cast<double>(m_aphDropDbPerSec->value()));
            sw->setActivePeakHoldFill(m_aphFill->isChecked());
            sw->setActivePeakHoldOnTx(m_aphOnTx->isChecked());
            sw->setActivePeakHoldColor(m_aphColor->color());

            // Peak Blobs
            sw->setPeakBlobsEnabled(m_blobEnable->isChecked());
            sw->setPeakBlobsCount(m_blobCount->value());
            sw->setPeakBlobsInsideFilterOnly(m_blobInsideFilter->isChecked());
            sw->setPeakBlobsHoldEnabled(m_blobHoldEnable->isChecked());
            sw->setPeakBlobsHoldMs(m_blobHoldMs->value());
            sw->setPeakBlobsHoldDrop(m_blobHoldDrop->isChecked());
            sw->setPeakBlobsFallDbPerSec(static_cast<double>(m_blobFallDbPerSec->value()));
            sw->setPeakBlobColor(m_blobColor->color());
            sw->setPeakBlobTextColor(m_blobTextColor->color());
        }
    }

    // ── Persist on change + live-wire to SpectrumWidget ──────────────────────
    // Each connect saves to AppSettings AND calls the matching SpectrumWidget
    // setter immediately so the display updates without reopening Setup.
    // The lambdas capture 'this' and call SetupPage::model() (not the
    // constructor parameter; that is out of scope after construction).

    connect(m_aphEnable, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayActivePeakHoldEnabled"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) {
                sw->setActivePeakHoldEnabled(v);
            }
        }
    });
    connect(m_aphDurationMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayActivePeakHoldDurationMs"), QString::number(v));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) {
                sw->setActivePeakHoldDurationMs(v);
            }
        }
    });
    connect(m_aphDropDbPerSec, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayActivePeakHoldDropDbPerSec"), QString::number(v));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) {
                sw->setActivePeakHoldDropDbPerSec(static_cast<double>(v));
            }
        }
    });
    connect(m_aphFill, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayActivePeakHoldFill"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) {
                sw->setActivePeakHoldFill(v);
            }
        }
    });
    connect(m_aphOnTx, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayActivePeakHoldOnTx"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) {
                sw->setActivePeakHoldOnTx(v);
            }
        }
    });
    connect(m_aphColor, &ColorSwatchButton::colorChanged, this, [this](const QColor& c) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayActivePeakHoldColor"),
            ColorSwatchButton::colorToHex(c));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setActivePeakHoldColor(c); }
        }
    });

    connect(m_blobEnable, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsEnabled"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobsEnabled(v); }
        }
    });
    connect(m_blobCount, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsCount"), QString::number(v));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobsCount(v); }
        }
    });
    connect(m_blobInsideFilter, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsInsideFilterOnly"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobsInsideFilterOnly(v); }
        }
    });
    connect(m_blobHoldEnable, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsHoldEnabled"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobsHoldEnabled(v); }
        }
    });
    connect(m_blobHoldMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsHoldMs"), QString::number(v));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobsHoldMs(v); }
        }
    });
    connect(m_blobHoldDrop, &QCheckBox::toggled, this, [this](bool v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsHoldDrop"),
            v ? QStringLiteral("True") : QStringLiteral("False"));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobsHoldDrop(v); }
        }
    });
    connect(m_blobFallDbPerSec, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobsFallDbPerSec"), QString::number(v));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) {
                sw->setPeakBlobsFallDbPerSec(static_cast<double>(v));
            }
        }
    });
    connect(m_blobColor, &ColorSwatchButton::colorChanged, this, [this](const QColor& c) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobColor"),
            ColorSwatchButton::colorToHex(c));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobColor(c); }
        }
    });
    connect(m_blobTextColor, &ColorSwatchButton::colorChanged, this, [this](const QColor& c) {
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakBlobTextColor"),
            ColorSwatchButton::colorToHex(c));
        if (auto* m = SetupPage::model()) {
            if (auto* sw = m->spectrumWidget()) { sw->setPeakBlobTextColor(c); }
        }
    });
}

void SpectrumPeaksPage::buildUI()
{
    applyDarkStyle(this);

    // ── Active Peak Hold ──────────────────────────────────────────────────────
    m_aphGroup = new QGroupBox(QStringLiteral("Active Peak Hold"), this);
    auto* aphForm = new QFormLayout(m_aphGroup);
    aphForm->setSpacing(6);

    m_aphEnable = new QCheckBox(
        QStringLiteral("Enable per-bin peak trace with decay"), m_aphGroup);
    m_aphEnable->setToolTip(QStringLiteral(
        "Display a secondary trace showing the highest level ever seen at each "
        "frequency bin. The trace decays downward at the configured rate once "
        "the hold duration elapses. Full implementation in Task 2.5."));
    aphForm->addRow(QString(), m_aphEnable);

    m_aphDurationMs = new QSpinBox(m_aphGroup);
    m_aphDurationMs->setRange(100, 60000);
    m_aphDurationMs->setSingleStep(100);
    m_aphDurationMs->setSuffix(QStringLiteral(" ms"));
    m_aphDurationMs->setToolTip(QStringLiteral(
        "How long (ms) a peak bin is held at its maximum before starting to decay."));
    aphForm->addRow(QStringLiteral("Hold duration:"), m_aphDurationMs);

    m_aphDropDbPerSec = new QSpinBox(m_aphGroup);
    m_aphDropDbPerSec->setRange(1, 60);
    m_aphDropDbPerSec->setSuffix(QStringLiteral(" dB/s"));
    m_aphDropDbPerSec->setToolTip(QStringLiteral(
        "Rate at which a held peak falls after the hold duration elapses."));
    aphForm->addRow(QStringLiteral("Drop rate:"), m_aphDropDbPerSec);

    m_aphFill = new QCheckBox(
        QStringLiteral("Fill area between peak trace and current trace"), m_aphGroup);
    m_aphFill->setToolTip(QStringLiteral(
        "Shade the region between the live spectrum and the peak-hold trace."));
    aphForm->addRow(QString(), m_aphFill);

    m_aphOnTx = new QCheckBox(
        QStringLiteral("Update during TX"), m_aphGroup);
    m_aphOnTx->setToolTip(QStringLiteral(
        "Continue updating the peak trace while transmitting. "
        "When off, the trace is frozen during TX."));
    aphForm->addRow(QString(), m_aphOnTx);

    // Placeholder colour; setColor() is called in the constructor after buildUI().
    // Gold default contrasts well against typical clarity-blue and white live traces.
    m_aphColor = new ColorSwatchButton(QColor(0xFF, 0xD7, 0x00, 0xFF), m_aphGroup);
    m_aphColor->setToolTip(QStringLiteral(
        "Color of the dashed Active Peak Hold trace. Set this to a hue "
        "different from the live data-line color so the peak trace stays "
        "visible (e.g. after Reset to Smooth Defaults paints the live "
        "trace white)."));
    aphForm->addRow(QStringLiteral("Trace color:"), m_aphColor);

    contentLayout()->addWidget(m_aphGroup);

    // ── Peak Blobs ────────────────────────────────────────────────────────────
    // From Thetis Display.cs:4395-4714 [v2.10.3.13] — ShowPeakBlobs / NumberOfPeakBlobs /
    //   ShowPeakBlobsInsideFilterOnly / BlobPeakHold / BlobPeakHoldMS /
    //   BlobPeakHoldDrop / PeakBlobFall
    m_blobGroup = new QGroupBox(QStringLiteral("Peak Blobs"), this);
    auto* blobForm = new QFormLayout(m_blobGroup);
    blobForm->setSpacing(6);

    m_blobEnable = new QCheckBox(
        QStringLiteral("Show top-N peak markers"), m_blobGroup);
    // From Thetis setup.cs chkShowPeakBlobMaximums [v2.10.3.13]
    m_blobEnable->setToolTip(QStringLiteral(
        "Display small circle markers at the top-N highest signal peaks in the spectrum."));
    blobForm->addRow(QString(), m_blobEnable);

    // From Thetis Display.cs:4407 [v2.10.3.13] range 1..m_nRX1Maximums.Length (=20)
    m_blobCount = new QSpinBox(m_blobGroup);
    m_blobCount->setRange(1, 20);
    m_blobCount->setToolTip(QStringLiteral(
        "Number of peak markers to display (1–20). "
        "From Thetis Display.cs:4407 [v2.10.3.13] — default 3, max 20."));
    blobForm->addRow(QStringLiteral("Number of peaks:"), m_blobCount);

    m_blobInsideFilter = new QCheckBox(
        QStringLiteral("Only show peaks inside the RX filter passband"), m_blobGroup);
    // From Thetis Display.cs:4401 [v2.10.3.13] ShowPeakBlobsInsideFilterOnly
    m_blobInsideFilter->setToolTip(QStringLiteral(
        "Restrict peak blobs to frequencies within the current RX filter passband."));
    blobForm->addRow(QString(), m_blobInsideFilter);

    m_blobHoldEnable = new QCheckBox(
        QStringLiteral("Hold peaks before decay"), m_blobGroup);
    // From Thetis Display.cs:4593 [v2.10.3.13] BlobPeakHold
    m_blobHoldEnable->setToolTip(QStringLiteral(
        "Keep each blob at its peak position for the hold duration before falling."));
    blobForm->addRow(QString(), m_blobHoldEnable);

    // From Thetis Display.cs:4599 [v2.10.3.13] m_fBlobPeakHoldMS = 500
    m_blobHoldMs = new QSpinBox(m_blobGroup);
    m_blobHoldMs->setRange(100, 60000);
    m_blobHoldMs->setSingleStep(100);
    m_blobHoldMs->setSuffix(QStringLiteral(" ms"));
    m_blobHoldMs->setToolTip(QStringLiteral(
        "How long (ms) a blob is held at its peak before falling. "
        "From Thetis Display.cs:4599 [v2.10.3.13] — default 500 ms."));
    blobForm->addRow(QStringLiteral("Hold duration:"), m_blobHoldMs);

    m_blobHoldDrop = new QCheckBox(
        QStringLiteral("Decay after hold (off = hard cut)"), m_blobGroup);
    // From Thetis Display.cs:4605 [v2.10.3.13] BlobPeakHoldDrop
    m_blobHoldDrop->setToolTip(QStringLiteral(
        "When on, blobs decay at the fall rate after the hold. "
        "When off, blobs disappear instantly after the hold duration."));
    blobForm->addRow(QString(), m_blobHoldDrop);

    // From Thetis Display.cs:4697 [v2.10.3.13] m_dBmPerSecondPeakBlobFall = 6.0f
    m_blobFallDbPerSec = new QSpinBox(m_blobGroup);
    m_blobFallDbPerSec->setRange(1, 60);
    m_blobFallDbPerSec->setSuffix(QStringLiteral(" dB/s"));
    m_blobFallDbPerSec->setToolTip(QStringLiteral(
        "Rate at which blobs fall after the hold duration. "
        "From Thetis Display.cs:4697 [v2.10.3.13] — default 6 dB/s."));
    blobForm->addRow(QStringLiteral("Fall rate:"), m_blobFallDbPerSec);

    // Colors
    // From Thetis display.cs:8434 [v2.10.3.13] m_bDX2_PeakBlob = Color.OrangeRed
    // Upstream tags preserved: //MW0LGE (from cited display.cs:8429) [v2.10.3.15]
    // Placeholder color; setColor() is called in the constructor after buildUI().
    m_blobColor = new ColorSwatchButton(QColor(0xFF, 0x45, 0x00, 0xFF), m_blobGroup);
    m_blobColor->setToolTip(QStringLiteral(
        "Color of the peak blob circles. "
        "From Thetis display.cs:8434 [v2.10.3.13] — default OrangeRed."));
    blobForm->addRow(QStringLiteral("Blob color:"), m_blobColor);

    // From Thetis display.cs:8435 [v2.10.3.13] m_bDX2_PeakBlobText = Color.Chartreuse
    // Placeholder color; setColor() is called in the constructor after buildUI().
    m_blobTextColor = new ColorSwatchButton(QColor(0x7F, 0xFF, 0x00, 0xFF), m_blobGroup);
    m_blobTextColor->setToolTip(QStringLiteral(
        "Color of the dBm readout text on each peak blob. "
        "From Thetis display.cs:8435 [v2.10.3.13] — default Chartreuse."));
    blobForm->addRow(QStringLiteral("Text color:"), m_blobTextColor);

    contentLayout()->addWidget(m_blobGroup);

    // ── Cross-link ────────────────────────────────────────────────────────────
    m_backBtn = new QPushButton(QStringLiteral("← Spectrum defaults"), this);
    m_backBtn->setToolTip(QStringLiteral(
        "Navigate back to the Spectrum Defaults page."));
    m_backBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a2a3a; color: #8aa8c0; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #203040; color: #c8d8e8; }"));
    connect(m_backBtn, &QPushButton::clicked,
            this, &SpectrumPeaksPage::backToSpectrumDefaultsRequested);
    contentLayout()->addWidget(m_backBtn, 0, Qt::AlignLeft);

    contentLayout()->addStretch();
}

}  // namespace NereusSDR
