#pragma once

// =================================================================
// src/core/mmio/ExternalVariableEngine.h  (NereusSDR)
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

#include <QObject>
#include <QUuid>
#include <QMap>
#include <QString>
#include <QHash>
#include <QVariant>
#include <QReadWriteLock>

class QThread;

namespace NereusSDR {

class MmioEndpoint;
class ITransportWorker;

// Phase 3G-6 block 5 — singleton that owns the MMIO endpoint
// registry and manages per-endpoint transport workers on a dedicated
// background thread. Matches Thetis's MultiMeterIO static registry
// from MeterManager.cs:40831-40834 but with a Qt-native signal/slot
// transport model.
//
// Threading:
// - The engine itself and every worker live on m_workerThread.
// - MmioEndpoint objects are owned by the engine and shared with the
//   main thread via read-locked value() / variableNames() calls.
// - Persistence (load/save) happens on the main thread against
//   AppSettings and is cheap enough to block briefly.
class ExternalVariableEngine : public QObject {
    Q_OBJECT

public:
    static ExternalVariableEngine& instance();

    // Read endpoints from AppSettings and start workers. Call once
    // at app startup after AppSettings is available.
    void init();

    // Stop all workers and drop the registry. Called at shutdown.
    void shutdown();

    // Registry access — safe to call from any thread.
    QList<MmioEndpoint*> endpoints() const;
    MmioEndpoint* endpoint(const QUuid& guid) const;

    // Add / update / remove an endpoint. The engine takes ownership
    // of the passed MmioEndpoint and persists the updated registry
    // immediately.
    void addEndpoint(MmioEndpoint* ep);
    void removeEndpoint(const QUuid& guid);
    void updateEndpoint(MmioEndpoint* ep);

signals:
    void endpointAdded(const QUuid& guid);
    void endpointRemoved(const QUuid& guid);
    void endpointChanged(const QUuid& guid);

private slots:
    void onValueBatchReceived(const QHash<QString, QVariant>& batch);

private:
    ExternalVariableEngine();
    ~ExternalVariableEngine() override;
    ExternalVariableEngine(const ExternalVariableEngine&) = delete;
    ExternalVariableEngine& operator=(const ExternalVariableEngine&) = delete;

    // Create a concrete ITransportWorker subclass appropriate for
    // the endpoint's transport kind. Returns nullptr if the
    // transport isn't available (e.g. serial without HAVE_SERIALPORT).
    ITransportWorker* makeWorker(MmioEndpoint* ep);

    void startWorker(MmioEndpoint* ep);
    void stopWorker(const QUuid& guid);

    void loadFromSettings();
    void saveEndpointToSettings(const MmioEndpoint* ep);
    void removeEndpointFromSettings(const QUuid& guid);

    mutable QReadWriteLock m_registryLock;
    QMap<QUuid, MmioEndpoint*> m_endpoints;
    QMap<QUuid, ITransportWorker*> m_workers;

    QThread* m_workerThread{nullptr};
};

} // namespace NereusSDR
