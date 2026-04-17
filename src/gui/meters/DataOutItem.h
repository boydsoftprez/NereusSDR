#pragma once
// =================================================================
// src/gui/meters/DataOutItem.h  (NereusSDR)
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
#include <QString>

namespace NereusSDR {
// External data output bridge (MMIO). From Thetis clsDataOut (MeterManager.cs:16047+).
class DataOutItem : public MeterItem {
    Q_OBJECT
public:
    enum class OutputFormat { Json, Xml, Raw };
    enum class TransportMode { Udp, Tcp, Serial, TcpClient };

    explicit DataOutItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setMmioGuid(const QString& guid) { m_mmioGuid = guid; }
    QString mmioGuid() const { return m_mmioGuid; }
    void setMmioVariable(const QString& var) { m_mmioVariable = var; }
    QString mmioVariable() const { return m_mmioVariable; }
    void setOutputFormat(OutputFormat fmt) { m_outputFormat = fmt; }
    OutputFormat outputFormat() const { return m_outputFormat; }
    void setTransportMode(TransportMode mode) { m_transportMode = mode; }
    TransportMode transportMode() const { return m_transportMode; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QString m_mmioGuid;
    QString m_mmioVariable;
    OutputFormat m_outputFormat{OutputFormat::Json};
    TransportMode m_transportMode{TransportMode::Udp};
};
} // namespace NereusSDR
