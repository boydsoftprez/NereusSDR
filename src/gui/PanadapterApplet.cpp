// no-port-check: AetherSDR-derived NereusSDR file. Per-pan container
// (SpectrumWidget host, slice association) is adapted structurally from
// AetherSDR src/gui/PanadapterApplet.{h,cpp} [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterApplet.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterApplet.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/SliceModel.h"

#include <QResizeEvent>
#include <QVBoxLayout>

namespace NereusSDR {

PanadapterApplet::PanadapterApplet(const QString& panId, QWidget* parent)
    : QWidget(parent)
    , m_panId(panId)
    , m_spectrum(new SpectrumWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_spectrum);

    // Phase 3F Sub-Epic E Task 2: per-pan status overlay in top-right.
    // Positioned manually in resizeEvent so the spectrum host owns the full
    // applet area underneath. Parented to `this`, not m_spectrum, so it sits
    // above the SpectrumWidget's QRhi surface without becoming a child of it.
    m_statusOverlay = new SpectrumStatusOverlay(this);
    m_statusOverlay->raise();

    connect(m_statusOverlay, &SpectrumStatusOverlay::txBadgeClicked, this,
            [this]() { emit txBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::wideBadgeClicked, this,
            [this]() { emit wideBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::chainTagClicked, this,
            &PanadapterApplet::chainTagClicked);
}

PanadapterApplet::~PanadapterApplet() = default;

void PanadapterApplet::addSlice(int sliceIndex)
{
    m_associatedSlices.insert(sliceIndex);
    if (m_activeSliceIndex == -1) {
        setActiveSliceIndex(sliceIndex);
    }
}

void PanadapterApplet::removeSlice(int sliceIndex)
{
    m_associatedSlices.remove(sliceIndex);
    if (m_activeSliceIndex == sliceIndex) {
        m_activeSliceIndex = m_associatedSlices.isEmpty() ? -1 : *m_associatedSlices.begin();
        emit activeSliceChanged(m_panId, m_activeSliceIndex);
    }
}

void PanadapterApplet::setActiveSliceIndex(int sliceIndex)
{
    if (m_activeSliceIndex == sliceIndex) { return; }
    m_activeSliceIndex = sliceIndex;
    emit activeSliceChanged(m_panId, sliceIndex);
}

void PanadapterApplet::setCenterMhz(double mhz) { m_centerMhz = mhz; }
void PanadapterApplet::setBandwidthMhz(double bw) { m_bandwidthMhz = bw; }

void PanadapterApplet::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_statusOverlay) {
        const QSize hint = m_statusOverlay->sizeHint();
        // 8px inset from top + right edge.
        m_statusOverlay->setGeometry(width() - hint.width() - 8, 8,
                                     hint.width(), hint.height());
    }
}

void PanadapterApplet::updateStatusOverlay(SliceModel* slice)
{
    if (!m_statusOverlay || !slice) { return; }
    m_statusOverlay->setSliceLetter(slice->sliceLetter());
    // SliceModel::frequency() returns double Hz (default 14225000.0 = 14.225 MHz).
    // Cast directly; no MHz->Hz conversion.
    m_statusOverlay->setFrequencyHz(static_cast<qint64>(slice->frequency()));
    m_statusOverlay->setMode(SliceModel::modeName(slice->dspMode()));
    m_statusOverlay->setChainIndex(slice->chainIndex());
    m_statusOverlay->setTxBound(slice->isTxSlice());
    m_statusOverlay->setDiversityActive(slice->diversityEnabled());
    m_statusOverlay->setPsPaused(slice->psPaused());
}

} // namespace NereusSDR
