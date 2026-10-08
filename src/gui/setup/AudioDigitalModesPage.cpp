// =================================================================
// src/gui/setup/AudioDigitalModesPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup > Audio > Digital modes page.
// See AudioDigitalModesPage.h for the full header.
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 11
//                (R-SPK-21, R-SPK-22, R-SPK-24). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "AudioDigitalModesPage.h"

#include "gui/setup/AudioTciPage.h"
#include "gui/setup/AudioVaxPage.h"

#include <QLabel>

namespace NereusSDR {

namespace {

static const char* kHeadingStyle =
    "QLabel { color: #c8d8e8; font-size: 13px; font-weight: bold; }";

} // namespace

AudioDigitalModesPage::AudioDigitalModesPage(RadioModel* model, AudioVaxPage* vax,
                                             AudioTciPage* tci, QWidget* parent)
    : SetupPage(QStringLiteral("Digital modes"), model, parent)
    , m_vax(vax)
    , m_tci(tci)
{
    buildPage();
}

AudioDigitalModesPage::AudioDigitalModesPage(RadioModel* model, QWidget* parent)
    : AudioDigitalModesPage(model, new AudioVaxPage(model), new AudioTciPage(model), parent)
{
}

QLabel* AudioDigitalModesPage::makeHeading(const QString& text, const QString& objectName)
{
    auto* heading = new QLabel(text, this);
    heading->setObjectName(objectName);
    heading->setStyleSheet(QLatin1String(kHeadingStyle));
    return heading;
}

void AudioDigitalModesPage::buildPage()
{
    addContent(makeHeading(tr("VAX"), QStringLiteral("digitalModesVaxHeading")));
    if (m_vax) {
        m_vax->setParent(this);
        addContent(m_vax);
    }
    addContent(makeHeading(tr("TCI"), QStringLiteral("digitalModesTciHeading")));
    if (m_tci) {
        m_tci->setParent(this);
        addContent(m_tci);
    }
}

void AudioDigitalModesPage::setReceiverAudioNote(RemoteReceiverAudioNote note)
{
    if (m_vax) {
        m_vax->setReceiverAudioNote(note);
    }
}

} // namespace NereusSDR
