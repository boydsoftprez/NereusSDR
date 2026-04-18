#pragma once

// =================================================================
// src/gui/meters/WebImageItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "MeterItem.h"
#include <QColor>
#include <QImage>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;

namespace NereusSDR {

// From Thetis clsWebImage (MeterManager.cs:14165+)
// Displays an image fetched from a URL with periodic refresh.
class WebImageItem : public MeterItem {
    Q_OBJECT

public:
    explicit WebImageItem(QObject* parent = nullptr);
    ~WebImageItem() override;

    void setUrl(const QString& url);
    QString url() const { return m_url; }

    void setRefreshInterval(int seconds);
    int refreshInterval() const { return m_refreshInterval; }

    void setFallbackColor(const QColor& c) { m_fallbackColor = c; }
    QColor fallbackColor() const { return m_fallbackColor; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private slots:
    void fetchImage();
    void onFetchFinished(QNetworkReply* reply);

private:
    QString m_url;
    int     m_refreshInterval{300}; // seconds
    QColor  m_fallbackColor{0x20, 0x20, 0x20};
    QImage  m_image;
    bool    m_fetchInProgress{false};

    QNetworkAccessManager* m_nam{nullptr};
    QTimer m_refreshTimer;
};

} // namespace NereusSDR
