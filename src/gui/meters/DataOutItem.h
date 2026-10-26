#pragma once

// =================================================================
// src/gui/meters/DataOutItem.h  (NereusSDR)
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
//   2026-10-26 - Data out sends on a timer (100-5000 ms; new items 500,
//                 saved items 1000) to the endpoint chosen by its 4-char
//                 ID, with the hosting meter's readings named as Thetis
//                 names them and the entry's slice and radio. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - setSendEnabled(false) keeps a preview copy inert: no timer
//                 and nothing sent. The timer runs only while sending is
//                 enabled and an endpoint is chosen. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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
#include <QChar>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QUuid>
#include <QVariant>

namespace NereusSDR {

class MeterWidget;

// Send interval of a new data out item: Thetis stores 500 when the item
// has no endpoint (as a new one has none) and shows 500 for it.
// From Thetis setup.cs:25283 [v2.10.3.15] (igs.UpdateInterval = 500;)
// and setup.cs:26017 [v2.10.3.15] (nudDataOutNode_sendinterval.Value = 500;)
constexpr int kDataOutNewIntervalMs = 500;
// Send interval of an item whose saved record has none: the class default.
// From Thetis MeterManager.cs:17374 [v2.10.3.15] (clsDataOut: UpdateInterval = 1000;)
constexpr int kDataOutClassIntervalMs = 1000;
// From Thetis setup.designer.cs:64049-64053 [v2.10.3.15] (nudDataOutNode_sendinterval.Minimum)
constexpr int kDataOutMinMs = 100;
// From Thetis setup.designer.cs:64044-64048 [v2.10.3.15] (nudDataOutNode_sendinterval.Maximum)
constexpr int kDataOutMaxMs = 5000;
// From Thetis setup.designer.cs:64038-64042 [v2.10.3.15] (nudDataOutNode_sendinterval.Increment)
constexpr int kDataOutStepMs = 10;

// External data output bridge (MMIO). From Thetis clsDataOut (MeterManager.cs:16047+).
class DataOutItem : public MeterItem {
    Q_OBJECT
public:
    enum class OutputFormat { Json, Xml, Raw };
    enum class TransportMode { Udp, Tcp, Serial, TcpClient };

    explicit DataOutItem(QObject* parent = nullptr);

    void setMmioGuid(const QString& guid) { m_mmioGuid = guid; }
    QString mmioGuid() const { return m_mmioGuid; }
    void setMmioVariable(const QString& var) { m_mmioVariable = var; }
    QString mmioVariable() const { return m_mmioVariable; }
    void setOutputFormat(OutputFormat fmt) { m_outputFormat = fmt; }
    OutputFormat outputFormat() const { return m_outputFormat; }
    void setTransportMode(TransportMode mode) { m_transportMode = mode; }
    TransportMode transportMode() const { return m_transportMode; }

    // Send interval in ms, kept within kDataOutMinMs..kDataOutMaxMs.
    void setSendIntervalMs(int ms);
    int sendIntervalMs() const { return m_sendIntervalMs; }

    // The MMIO endpoint the item sends to (Thetis MMIOGuid); null sends
    // nothing. The editor picks it by the endpoint's 4-char ID.
    void setEndpointGuid(const QUuid& guid);
    QUuid endpointGuid() const { return m_endpointGuid; }

    // False for a copy that only shows the entry (ContentRenderMode Preview
    // or Validation): it runs no timer and sendNow() sends nothing. True by
    // default. The send timer runs only while this is true and an endpoint
    // is chosen.
    void setSendEnabled(bool enabled);
    bool sendEnabled() const { return m_sendEnabled; }
    bool sendTimerActive() const { return m_sendTimer.isActive(); }

    // Set by MainWindow from the entry's Source and the session: the slice
    // letter (null when the Source resolves to no slice) and the radio id
    // (empty when there is no session).
    void setSliceLetter(QChar letter) { m_sliceLetter = letter; }
    QChar sliceLetter() const { return m_sliceLetter; }
    void setRadioId(const QString& id) { m_radioId = id; }
    QString radioId() const { return m_radioId; }

    // The message one send carries, in this item's output format.
    // readings are MeterBinding id -> value (named with thetisReadingName;
    // ids with no Thetis name are left out); published are named readings
    // other items publish. Pure: the timer and the tests call it.
    QString buildPayload(const QHash<int, double>& readings,
                         const QHash<QString, QVariant>& published,
                         QChar sliceLetter, const QString& radioId) const;

    // One send now: the hosting MeterWidget's latest readings plus what its
    // other items publish, queued on the endpoint through
    // ExternalVariableEngine::enqueueOutbound. The timer calls it.
    void sendNow();

    void applyNewItemDefaults() override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    MeterWidget* hostMeter() const;
    void updateSendTimer();

    QString m_mmioGuid;
    QString m_mmioVariable;
    OutputFormat m_outputFormat{OutputFormat::Json};
    TransportMode m_transportMode{TransportMode::Udp};
    int m_sendIntervalMs{kDataOutClassIntervalMs};
    QUuid m_endpointGuid;
    QChar m_sliceLetter;
    QString m_radioId;
    bool m_sendEnabled{true};
    QTimer m_sendTimer;
};
} // namespace NereusSDR
