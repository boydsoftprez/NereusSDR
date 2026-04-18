#pragma once

// =================================================================
// src/core/mmio/SerialEndpointWorker.h  (NereusSDR)
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

#include "ITransportWorker.h"

#ifdef HAVE_SERIALPORT

#include <QByteArray>

class QSerialPort;

namespace NereusSDR {

class MmioEndpoint;

// Phase 3G-6 block 5 — Serial transport worker for MMIO endpoints.
// Opens a QSerialPort on the endpoint's configured device and baud rate,
// accumulates newline-delimited messages from the port's readyRead signal,
// dispatches each complete line through the endpoint's configured
// FormatParser, and emits valueBatchReceived() with the resulting
// key->value map.
//
// Modelled after Thetis MeterManager.cs SerialPortHandler (lines
// 40589-40816), which uses the DataReceived event — the Qt equivalent
// is QSerialPort::readyRead.
//
// Runs entirely on ExternalVariableEngine::m_workerThread. All slots
// execute on that thread; no cross-thread synchronisation is needed.
class SerialEndpointWorker : public ITransportWorker {
    Q_OBJECT

public:
    explicit SerialEndpointWorker(MmioEndpoint* endpoint, QObject* parent = nullptr);
    ~SerialEndpointWorker() override;

public slots:
    void start() override;
    void stop() override;

private slots:
    void onReadyRead();
    void onErrorOccurred();

private:
    MmioEndpoint* m_endpoint;
    QSerialPort*  m_port{nullptr};
    QByteArray    m_lineBuffer;
};

} // namespace NereusSDR

#endif // HAVE_SERIALPORT
