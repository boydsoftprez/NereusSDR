#pragma once

// =================================================================
// src/gui/meters/WebImageItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

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
