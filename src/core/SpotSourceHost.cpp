// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/SpotSourceHost.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR - SpotSourceHost implementation: starts, stops and follows the
// spot sources. See SpotSourceHost.h.
//
// The start calls and the settings they read are the ones the local
// window used before (RadioModel::restoreSpotClientAutoStartState and
// MainWindow::openSpotHub's per-tab wiring), moved here unchanged, so a
// window running its own radio behaves exactly as it did.
//
// Ported from freedv-gui src/main.cpp [@77e793a]: the PSK Reporter start
// and stop (restoreAutoStart's PSK Reporter block, startPskReporterWith,
// stopPskReporter, disconnectSource's PSK Reporter branch) follow
//   - main.cpp:2575-2597 (PskReporter added to m_reporters[] and the
//     5-minute m_pskReporterTimer started at audio start),
//   - main.cpp:1609-1616 (the ID_TIMER_PSKREPORTER tick sends every
//     reporter's in-progress packet),
//   - main.cpp:2694 (m_pskReporterTimer.Stop()),
// and src/reporting/pskreporter.cpp:148-169 [@77e793a] (the receiver's
// callsign, grid square and software set before anything is reported).
// These moved here from MainWindow.cpp and RadioModel.cpp in parity Task
// 19 with their cites, which the move dropped and this file restores.
// The rest of the file (the DX cluster, RBN, POTA, WSJT-X and
// SpotCollector starts, the forwarding to a Core and its state) is
// NereusSDR-original.
//
// FreeDV Reporter (iPhone plan Task 22 / parity Task 20): its start moved
// here from RadioModel::restoreSpotClientAutoStartState, and the Core's
// status message, QSY request and "hide my station" call the client's own
// ports of freedv-gui FreeDVReporter.cpp (updateMessage :122-130,
// requestQSY :104-119, hideFromView / showOurselves :167-185 [@77e793a];
// see FreeDVReporterClient.h). The QSY request names a callsign on the
// link; the Core looks up that station's session id in its own list, as
// the dialog's selected row gives it locally (freedv-gui
// src/gui/dialogs/freedv_reporter.cpp:1078-1090 OnSendQSY and :3266-3271
// requestQSY send the selected row's sid [@77e793a]). Ported from freedv-gui
// [@77e793a] for FreeDV Reporter: freedvHides (src/ongui.cpp:1523-1529 and
// OnToggleReporterVisibility :1930-1942: the reporter follows analog mode
// only while the operator has not hidden it) and setFreedvStationList
// (src/gui/dialogs/freedv_reporter.cpp:3280-3304: the list is cleared on
// connect and on disconnect).
//
// License (upstream):
//   - freedv-gui main.cpp carries the GPL v2.1 header reproduced verbatim
//     below per the upstream redistribution clause.
//   - freedv-gui carries an LGPLv2.1+ root license (`freedv-gui/COPYING`).
//     The specific `pskreporter.cpp` file carries a permissive
//     BSD-2-Clause-style file header (Copyright Mooneer Salem, no per-
//     file project copyright line); the BSD permission block is
//     reproduced verbatim below per the upstream redistribution clause.
//
//   - freedv-gui's src/ongui.cpp carries only the short comment header
//     reproduced verbatim below; src/gui/dialogs/freedv_reporter.cpp has
//     no per-file header. The root LGPLv2.1+ license applies to both.
//
// LGPL is upgrade-compatible to GPL-3 (LGPL §3 conversion clause); the
// BSD-2-Clause file-header carve-out is GPL-compatible by its own terms.
//
// --- From freedv-gui/src/main.cpp [@77e793a] (verbatim header) ---
//
// ==========================================================================
//  Name:            main.cpp
//
//  Purpose:         FreeDV main()
//  Created:         Apr. 9, 2012
//  Authors:         David Rowe, David Witten
//
//  License:
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2.1,
//   as published by the Free Software Foundation.  This program is
//   distributed in the hope that it will be useful, but WITHOUT ANY
//   WARRANTY; without even the implied warranty of MERCHANTABILITY or
//   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//   License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, see <http://www.gnu.org/licenses/>.
//
// ==========================================================================
//
// --- From freedv-gui src/reporting/pskreporter.cpp [@77e793a] (verbatim header) ---
//
// =========================================================================
//  Name:            pskreporter.cpp
//  Purpose:         Implementation of PSK Reporter support.
//
//  Authors:         Mooneer Salem
//  License:
//
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//  - Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  - Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// =========================================================================
//
// --- From freedv-gui src/ongui.cpp [@77e793a] (verbatim header) ---
//
// /*
//   ongui.cpp
//
//   The simpler GUI event handlers.
// */
//
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 19, R-IOS-25,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Fix wave after the review of parity
//                                    Tasks 19 and 21 (M7): the freedv-gui
//                                    header, cites and PROVENANCE row the
//                                    move from MainWindow and RadioModel
//                                    dropped are restored. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  iPhone plan Task 22 / parity Task 20
//                                    (R-IOS-26, R-R3-49): FreeDV Reporter
//                                    as a station source (start, stop,
//                                    state, message, QSY, hide); ported
//                                    freedvHides from freedv-gui
//                                    ongui.cpp:1523-1529, 1930-1942 and
//                                    setFreedvStationList from
//                                    freedv_reporter.cpp:3280-3304
//                                    [@77e793a], with ongui.cpp's header.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Spot resolved mode (R-IOS-25,
//                                    recordStreamVersion 2): each spot
//                                    record carries resolvedMode, the
//                                    desktop's own answer from
//                                    SpotModeResolver::dspModeForSpot
//                                    (ported from AetherSDR
//                                    src/core/SpotModeResolver.{h,cpp}
//                                    [@1e0718ad], Copyright (C) 2024-2026
//                                    Jeremy (KK7GWY) / AetherSDR
//                                    contributors, GPLv3), called here and
//                                    not copied. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "core/SpotSourceHost.h"

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "models/FreeDVStationModel.h"
#include "core/LogCategories.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/WsjtxClient.h"
#include "models/Band.h"
#include "models/SpotModeResolver.h"
#include "models/SpotModel.h"

#include <QDateTime>
#include <QTimeZone>
#include <cmath>

namespace NereusSDR {

const QString SpotSourceHost::kDxCluster = QStringLiteral("dxCluster");
const QString SpotSourceHost::kRbn = QStringLiteral("rbn");
const QString SpotSourceHost::kPota = QStringLiteral("pota");
const QString SpotSourceHost::kFreedvReporter = QStringLiteral("freedvReporter");
const QString SpotSourceHost::kPskReporter = QStringLiteral("pskReporter");
const QString SpotSourceHost::kWsjtx = QStringLiteral("wsjtx");
const QString SpotSourceHost::kSpotCollector = QStringLiteral("spotCollector");

const QString SpotSourceHost::kOff = QStringLiteral("off");
const QString SpotSourceHost::kConnecting = QStringLiteral("connecting");
const QString SpotSourceHost::kConnected = QStringLiteral("connected");
const QString SpotSourceHost::kError = QStringLiteral("error");

namespace {

QString versionString()
{
    return QStringLiteral("NereusSDR ") + QStringLiteral(NEREUSSDR_VERSION);
}

bool settingIsTrue(const QString& key)
{
    return AppSettings::instance().value(key, QStringLiteral("False")).toString()
        == QStringLiteral("True");
}

// The identity fall-back chain the Spot Hub's Settings tab set up: a
// source's own key, then the canonical User/Callsign.
QString resolveCall(const QString& perSourceKey)
{
    auto& s = AppSettings::instance();
    QString v = s.value(perSourceKey).toString();
    if (v.isEmpty()) {
        v = s.value(QStringLiteral("User/Callsign")).toString();
    }
    return v;
}

QString resolveGrid(const QString& perSourceKey)
{
    auto& s = AppSettings::instance();
    QString v = s.value(perSourceKey).toString();
    if (v.isEmpty()) {
        v = s.value(QStringLiteral("User/GridSquare")).toString();
    }
    return v;
}

} // namespace

QStringList SpotSourceHost::stationSources()
{
    return {kDxCluster, kRbn, kPota, kFreedvReporter, kPskReporter};
}

QStringList SpotSourceHost::recordStreamSources()
{
    return {kDxCluster, kRbn, kPota, kPskReporter};
}

QStringList SpotSourceHost::windowSources()
{
    return {kWsjtx, kSpotCollector};
}

bool SpotSourceHost::isStationSource(const QString& source)
{
    return stationSources().contains(source);
}

bool SpotSourceHost::isKnownSource(const QString& source)
{
    return isStationSource(source) || windowSources().contains(source);
}

QString SpotSourceHost::consoleStream(const QString& source)
{
    return QStringLiteral("spotConsole:") + source;
}

QJsonObject SpotSourceHost::spotRecordFields(const SpotData& spot, const DxccColorProvider* dxcc)
{
    // The heard-on frequency first, as the panadapter overlay places it.
    const double mhz = spot.rxFreqMhz > 0.0 ? spot.rxFreqMhz : spot.txFreqMhz;
    const qint64 hz = static_cast<qint64>(std::llround(mhz * 1.0e6));
    const QDateTime when = spot.timestamp.isValid()
        ? spot.timestamp
        : QDateTime::fromMSecsSinceEpoch(spot.addedMs, QTimeZone::UTC);
    QString colour;
    int priority = 0;
    if (dxcc != nullptr && dxcc->isEnabled() && !spot.callsign.isEmpty() && mhz > 0.0) {
        switch (dxcc->statusForSpot(spot.callsign, mhz, spot.mode)) {
        case DxccStatus::NewDxcc: priority = 4; break;
        case DxccStatus::NewBand: priority = 3; break;
        case DxccStatus::NewMode: priority = 2; break;
        case DxccStatus::Worked:  priority = 1; break;
        case DxccStatus::Unknown: priority = 0; break;
        }
        const QColor c = dxcc->colorForSpot(spot.callsign, mhz, spot.mode);
        if (c.isValid()) {
            colour = c.name();
        }
    }
    QJsonObject fields{
        {QStringLiteral("timeUtc"), when.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("frequencyHz"), static_cast<double>(hz)},
        {QStringLiteral("call"), spot.callsign},
        {QStringLiteral("mode"), spot.mode},
        {QStringLiteral("source"), spot.source},
        {QStringLiteral("spotter"), spot.spotterCallsign},
        {QStringLiteral("comment"), spot.comment},
        {QStringLiteral("band"), static_cast<int>(bandFromFrequency(static_cast<double>(hz)))},
        {QStringLiteral("dxccColour"), colour},
        {QStringLiteral("dxccPriority"), priority},
    };
    // Spot resolved mode (R-IOS-25, recordStreamVersion 2): the mode a click
    // on this spot puts a slice in, as the desktop's own resolver (ported
    // from AetherSDR SpotModeResolver [@1e0718ad]) picks it, as the slice's
    // dspMode number. Absent when it has none.
    if (const std::optional<DSPMode> mode = SpotModeResolver::dspModeForSpot(spot)) {
        fields.insert(QStringLiteral("resolvedMode"), static_cast<int>(*mode));
    }
    return fields;
}

QString SpotSourceHost::readOnlyReason()
{
    return QStringLiteral("The Core's spot sources change only from their Connect and Start "
                          "buttons.");
}

SpotSourceHost::SpotSourceHost(DxClusterClient* dxCluster, DxClusterClient* rbn,
                               WsjtxClient* wsjtx, SpotCollectorClient* spotCollector,
                               PotaClient* pota, PskReporterClient* pskReporter,
                               SpotModel* spots, QObject* parent)
    : QObject(parent)
    , m_dxCluster(dxCluster)
    , m_rbn(rbn)
    , m_wsjtx(wsjtx)
    , m_spotCollector(spotCollector)
    , m_pota(pota)
    , m_pskReporter(pskReporter)
    , m_spots(spots)
    , m_freedvHidden(settingIsTrue(QStringLiteral("FreeDvReporter/Hidden")))
{
    // Follow each client, so the state is what the client does, whoever
    // started it.
    const auto followCluster = [this](DxClusterClient* client, const QString& source) {
        if (client == nullptr) {
            return;
        }
        connect(client, &DxClusterClient::connected, this, [this, source]() {
            setSource(source, kConnected);
        });
        connect(client, &DxClusterClient::disconnected, this, [this, source]() {
            setSource(source, kOff);
        });
        connect(client, &DxClusterClient::connectionError, this,
                [this, source](const QString& error) { setSource(source, kError, error); });
        connect(client, &DxClusterClient::rawLineReceived, this,
                [this, source](const QString& line) { emit consoleLine(source, line); });
    };
    followCluster(m_dxCluster, kDxCluster);
    followCluster(m_rbn, kRbn);
    if (m_wsjtx) {
        connect(m_wsjtx, &WsjtxClient::listening, this, [this]() { setSource(kWsjtx, kConnected); });
        connect(m_wsjtx, &WsjtxClient::stopped, this, [this]() { setSource(kWsjtx, kOff); });
        connect(m_wsjtx, &WsjtxClient::rawLineReceived, this,
                [this](const QString& line) { emit consoleLine(kWsjtx, line); });
    }
    if (m_spotCollector) {
        connect(m_spotCollector, &SpotCollectorClient::listening, this,
                [this]() { setSource(kSpotCollector, kConnected); });
        connect(m_spotCollector, &SpotCollectorClient::stopped, this,
                [this]() { setSource(kSpotCollector, kOff); });
        connect(m_spotCollector, &SpotCollectorClient::rawLineReceived, this,
                [this](const QString& line) { emit consoleLine(kSpotCollector, line); });
    }
    if (m_pota) {
        connect(m_pota, &PotaClient::started, this,
                [this]() { setSource(kPota, kConnected, QStringLiteral("Polling api.pota.app")); });
        connect(m_pota, &PotaClient::stopped, this, [this]() { setSource(kPota, kOff); });
        connect(m_pota, &PotaClient::pollError, this,
                [this](const QString& error) { setSource(kPota, kError, error); });
        connect(m_pota, &PotaClient::rawLineReceived, this,
                [this](const QString& line) { emit consoleLine(kPota, line); });
    }
    if (m_pskReporter) {
        connect(m_pskReporter, &PskReporterClient::errorOccurred, this,
                [this](const QString& error) { setSource(kPskReporter, kError, error); });
    }
}

void SpotSourceHost::setFreedvReporter(FreeDVReporterClient* client)
{
    if (m_freedv) {
        disconnect(m_freedv, nullptr, this, nullptr);
    }
    m_freedv = client;
    if (client == nullptr) {
        return;
    }
    // Follow the client, whoever started it (the FreeDV tab, the Core's
    // restore, a window's spots.connect).
    connect(client, &FreeDVReporterClient::connected, this,
            [this]() { setSource(kFreedvReporter, kConnected); });
    connect(client, &FreeDVReporterClient::disconnected, this,
            [this]() { setSource(kFreedvReporter, kOff); });
    connect(client, &FreeDVReporterClient::connectionLost, this, [this](int retryInMs) {
        setSource(kFreedvReporter, kConnecting,
                  QStringLiteral("Lost the connection; trying again in %1 s")
                      .arg((retryInMs + 999) / 1000));
    });
    connect(client, &FreeDVReporterClient::connectionError, this,
            [this](const QString& error) { setSource(kFreedvReporter, kError, error); });
    connect(client, &FreeDVReporterClient::rawLineReceived, this,
            [this](const QString& line) { emit consoleLine(kFreedvReporter, line); });
}

void SpotSourceHost::setFreedvStationList(FreeDVStationModel* list)
{
    m_freedvList = list;
    if (m_freedv == nullptr || list == nullptr) {
        return;
    }
    // From freedv-gui src/gui/dialogs/freedv_reporter.cpp:3280-3304
    // [@77e793a]: onReporterConnect_ and onReporterDisconnect_ both call
    // clearAllEntries_(), so the list starts again on each connect and
    // empties when the connection ends, and a station the server stopped
    // telling this client about never lingers. (The client forgets its own
    // map at the same points: startConnection, stopConnection,
    // onWsDisconnected.)
    const auto clearAllEntries = [this]() {
        if (m_freedvList) {
            m_freedvList->clear();
        }
    };
    connect(m_freedv, &FreeDVReporterClient::connected, this, clearAllEntries);
    connect(m_freedv, &FreeDVReporterClient::disconnected, this, clearAllEntries);
    connect(m_freedv, &FreeDVReporterClient::connectionLost, this, clearAllEntries);
}

bool SpotSourceHost::freedvHides(bool listedSliceInRade) const
{
    // From freedv-gui src/ongui.cpp:1523-1529 [@77e793a]: an analog-mode
    // change reaches the shared reporter only while the operator has not
    // hidden the station,
    //     if (obj != wxGetApp().m_sharedReporterObject || !m_reporterHidden->GetValue())
    //     {
    //         obj->inAnalogMode(g_analog);
    //     }
    // and OnToggleReporterVisibility (ongui.cpp:1930-1942) hides or shows
    // it with the toggle. With RADE as the FreeDV mode and any other mode
    // as analog: hidden while "Hide my station" is on or the listed slice
    // is not in RADE.
    return freedvReporterHidden() || !listedSliceInRade;
}

void SpotSourceHost::restoreAutoStart(Placement placement)
{
    const bool station = placement != Placement::WindowSources;
    const bool window = placement != Placement::StationSources;
    auto& s = AppSettings::instance();

    // DxCluster
    if (station && m_dxCluster && settingIsTrue(QStringLiteral("DxClusterAutoConnect"))) {
        setSource(kDxCluster, kConnecting);
        m_dxCluster->connectToCluster(
            s.value(QStringLiteral("DxClusterHost"), QStringLiteral("dxc.nc7j.com")).toString(),
            static_cast<quint16>(s.value(QStringLiteral("DxClusterPort"), 7300).toInt()),
            resolveCall(QStringLiteral("DxClusterCallsign")));
    }

    // RBN (same DxClusterClient class, different keys / default host).
    if (station && m_rbn && settingIsTrue(QStringLiteral("RbnAutoConnect"))) {
        setSource(kRbn, kConnecting);
        m_rbn->connectToCluster(
            s.value(QStringLiteral("RbnHost"),
                    QStringLiteral("telnet.reversebeacon.net")).toString(),
            static_cast<quint16>(s.value(QStringLiteral("RbnPort"), 7000).toInt()),
            resolveCall(QStringLiteral("RbnCallsign")));
    }

    // WSJT-X (UDP bind on the configured address / port).
    if (window && m_wsjtx && settingIsTrue(QStringLiteral("WsjtxAutoStart"))) {
        m_wsjtx->startListening(
            s.value(QStringLiteral("WsjtxAddress"), QStringLiteral("224.0.0.1")).toString(),
            static_cast<quint16>(s.value(QStringLiteral("WsjtxPort"), 2237).toInt()));
    }

    // SpotCollector (UDP bind).
    if (window && m_spotCollector && settingIsTrue(QStringLiteral("SpotCollectorAutoStart"))) {
        m_spotCollector->startListening(
            static_cast<quint16>(s.value(QStringLiteral("SpotCollectorPort"), 9999).toInt()));
    }

    // POTA (HTTPS poll loop).
    if (station && m_pota && settingIsTrue(QStringLiteral("PotaAutoStart"))) {
        m_pota->startPolling(s.value(QStringLiteral("PotaPollInterval"), 30).toInt());
    }

    // FreeDV Reporter (WebSocket connect). Moved here from
    // RadioModel::restoreSpotClientAutoStartState (iPhone plan Task 22):
    // the identity is resolved from the saved settings before the connect,
    // and with none the start is skipped, as before, and says why.
    if (station && m_freedv && settingIsTrue(QStringLiteral("FreeDvAutoStart"))) {
        QString reason;
        if (!startFreedvWith(&reason)) {
            qCWarning(lcDsp) << "FreeDV Reporter auto-start skipped:" << reason;
            setSource(kFreedvReporter, kOff, reason);
        }
    }

    // PSK Reporter: send-only.  Identity refreshed from User/* fall-
    // back chain.  2026-05-12 bench fix: if PskReporterAutoStart is
    // True, arm the 5-minute auto-send timer now: source-first port
    // from freedv-gui main.cpp:2575-2597 [@77e793a] which adds
    // PskReporter to m_reporters[] AND starts m_pskReporterTimer at
    // audio start time.  Previously the AutoStart flag persisted but
    // had no effect (it only set identity), so users with auto-start
    // checked would never see any spots reach pskreporter.info.
    // [moved from RadioModel::restoreSpotClientAutoStartState in parity
    // Task 19; the station's placement only]
    if (station && m_pskReporter) {
        const QString pskCall = resolveCall(QStringLiteral("PskReporter/Callsign"));
        const QString pskGrid = resolveGrid(QStringLiteral("PskReporter/GridSquare"));
        if (!pskCall.isEmpty()) {
            m_pskReporter->setIdentity(pskCall, pskGrid, versionString());
            if (settingIsTrue(QStringLiteral("PskReporterAutoStart"))) {
                // Enable FreeDV Reporter timer (every 5 minutes).  [original inline comment from main.cpp:2594]
                m_pskReporter->setAutoSendIntervalSec(PskReporterClient::kReportingIntervalSec);
                setSource(kPskReporter, kConnected,
                          QStringLiteral("Reporting every 5 minutes"));
                qCInfo(lcDsp) << "PskReporter: auto-start armed (5-min interval)"
                              << "callsign=" << pskCall;
            }
        }
    }
}

bool SpotSourceHost::connectSource(const QString& source, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };
    if (!isKnownSource(source)) {
        return refuse(QStringLiteral("The Core does not run that spot source."));
    }
    if (!isStationSource(source)) {
        return refuse(QStringLiteral("WSJT-X and SpotCollector listen on each computer, not on "
                                     "the Core."));
    }
    auto& s = AppSettings::instance();
    if (source == kDxCluster || source == kRbn) {
        const bool cluster = source == kDxCluster;
        DxClusterClient* client = cluster ? m_dxCluster.data() : m_rbn.data();
        if (client == nullptr) {
            return refuse(QStringLiteral("The Core does not run that spot source."));
        }
        const QString call = resolveCall(cluster ? QStringLiteral("DxClusterCallsign")
                                                 : QStringLiteral("RbnCallsign"));
        if (call.isEmpty()) {
            return refuse(QStringLiteral("Enter your callsign in Spot Hub first."));
        }
        if (client->isConnected()) {
            return true;
        }
        const QString host = cluster
            ? s.value(QStringLiteral("DxClusterHost"), QStringLiteral("dxc.nc7j.com")).toString()
            : s.value(QStringLiteral("RbnHost"),
                      QStringLiteral("telnet.reversebeacon.net")).toString();
        const int port = cluster ? s.value(QStringLiteral("DxClusterPort"), 7300).toInt()
                                 : s.value(QStringLiteral("RbnPort"), 7000).toInt();
        setSource(source, kConnecting);
        client->connectToCluster(host, static_cast<quint16>(port), call);
        return true;
    }
    if (source == kPota) {
        if (m_pota == nullptr) {
            return refuse(QStringLiteral("The Core does not run that spot source."));
        }
        m_pota->startPolling(s.value(QStringLiteral("PotaPollInterval"), 30).toInt());
        return true;
    }
    if (source == kFreedvReporter) {
        return startFreedvWith(reason);
    }
    // PSK Reporter.
    if (m_pskReporter == nullptr) {
        return refuse(QStringLiteral("The Core does not run that spot source."));
    }
    const QString call = resolveCall(QStringLiteral("PskReporter/Callsign"));
    const QString grid = resolveGrid(QStringLiteral("PskReporter/GridSquare"));
    if (call.isEmpty() || grid.isEmpty()) {
        return refuse(QStringLiteral("Enter your callsign and grid square in Spot Hub first."));
    }
    startPskReporterWith(call, grid);
    return true;
}

bool SpotSourceHost::disconnectSource(const QString& source, QString* reason)
{
    if (!isStationSource(source)) {
        if (reason != nullptr) {
            *reason = isKnownSource(source)
                ? QStringLiteral("WSJT-X and SpotCollector listen on each computer, not on the "
                                 "Core.")
                : QStringLiteral("The Core does not run that spot source.");
        }
        return false;
    }
    if (source == kDxCluster && m_dxCluster) {
        m_dxCluster->disconnect();
        setSource(kDxCluster, kOff);
    } else if (source == kRbn && m_rbn) {
        m_rbn->disconnect();
        setSource(kRbn, kOff);
    } else if (source == kPota && m_pota) {
        m_pota->stopPolling();
        setSource(kPota, kOff);
    } else if (source == kFreedvReporter && m_freedv) {
        m_freedv->stopConnection();
        setSource(kFreedvReporter, kOff);
    } else if (source == kPskReporter && m_pskReporter) {
        // From freedv-gui main.cpp:2694 [@77e793a]:
        //   m_pskReporterTimer.Stop();
        // "Stop" = disarm the timer.
        m_pskReporter->setAutoSendIntervalSec(0);
        setSource(kPskReporter, kOff);
    }
    return true;
}

bool SpotSourceHost::sendCommand(const QString& source, const QString& text, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };
    DxClusterClient* client = source == kDxCluster ? m_dxCluster.data()
        : source == kRbn                            ? m_rbn.data()
                                                    : nullptr;
    if (client == nullptr) {
        return refuse(QStringLiteral("Only the DX cluster and the Reverse Beacon Network take "
                                     "typed commands."));
    }
    const QString command = text.trimmed();
    if (command.isEmpty()) {
        return refuse(QStringLiteral("Type a command first."));
    }
    if (!client->isConnected()) {
        return refuse(source == kDxCluster
                          ? QStringLiteral("The DX cluster is not connected.")
                          : QStringLiteral("The Reverse Beacon Network is not connected."));
    }
    client->sendCommand(command);
    // What the local console shows for a typed command, for every window.
    emit consoleLine(source, QStringLiteral("> ") + command);
    return true;
}

void SpotSourceHost::clearAll()
{
    if (m_spots) {
        m_spots->clear();
    }
}

// ── FreeDV Reporter (iPhone plan Task 22, stationFreedvVersion 1) ────────

QString SpotSourceHost::freedvCallsign()
{
    // R-IOS-26: a callsign, never the Core's label. The Spot Hub's own key
    // and its Settings tab's identity first, as before; the Core's
    // StationCallsign when neither is set.
    QString call = resolveCall(QStringLiteral("FreeDvReporter/Callsign"));
    if (call.isEmpty()) {
        call = AppSettings::instance().value(QStringLiteral("StationCallsign")).toString();
    }
    return call.trimmed();
}

QString SpotSourceHost::freedvGridSquare()
{
    return resolveGrid(QStringLiteral("FreeDvReporter/GridSquare")).trimmed();
}

bool SpotSourceHost::setFreedvMessage(const QString& text, QString* reason)
{
    if (m_freedv == nullptr) {
        if (reason != nullptr) {
            *reason = QStringLiteral("The Core does not run FreeDV Reporter.");
        }
        return false;
    }
    // From freedv-gui FreeDVReporter.cpp:122-130 [@77e793a] (updateMessage):
    // kept for the next connect when not connected now.
    m_freedv->updateMessage(text);
    return true;
}

bool SpotSourceHost::sendFreedvQsy(const QString& callsign, qint64 frequencyHz, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };
    if (m_freedv == nullptr) {
        return refuse(QStringLiteral("The Core does not run FreeDV Reporter."));
    }
    if (!m_freedv->isConnected()) {
        return refuse(QStringLiteral("FreeDV Reporter is not connected on the Core."));
    }
    if (frequencyHz <= 0) {
        return refuse(QStringLiteral("Enter a frequency for the QSY request first."));
    }
    const QString wanted = callsign.trimmed();
    if (wanted.isEmpty()) {
        return refuse(QStringLiteral("Pick a callsign for the QSY request first."));
    }
    // The station listed with that callsign; with more than one (the same
    // operator on two sessions), the one heard from last.
    QString sid;
    QDateTime newest;
    const QHash<QString, FreeDVStation> stations = m_freedv->stations();
    for (auto it = stations.cbegin(); it != stations.cend(); ++it) {
        if (it->callsign.compare(wanted, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (sid.isEmpty() || (it->lastUpdate.isValid() && it->lastUpdate > newest)) {
            sid = it.key();
            newest = it->lastUpdate;
        }
    }
    if (sid.isEmpty()) {
        return refuse(QStringLiteral("%1 is not on FreeDV Reporter now.").arg(wanted));
    }
    // From freedv-gui FreeDVReporter.cpp:104-119 [@77e793a] (requestQSY).
    m_freedv->requestQSY(sid, static_cast<quint64>(frequencyHz), QString());
    return true;
}

bool SpotSourceHost::setFreedvHidden(bool on, QString* reason)
{
    Q_UNUSED(reason);
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("FreeDvReporter/Hidden"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    s.save();
    if (m_freedvHidden == on) {
        return true;
    }
    m_freedvHidden = on;
    emit sourcesChanged();
    emit sourceChanged(kFreedvReporter);
    emit freedvHiddenChanged(on);
    return true;
}

bool SpotSourceHost::freedvReporterHidden() const
{
    return forwardsStationSources() ? m_stationFreedvHidden : m_freedvHidden;
}

QString SpotSourceHost::state(const QString& source) const
{
    const bool station = forwardsStationSources() && isStationSource(source);
    const auto& table = station ? m_station : m_local;
    const auto it = table.constFind(source);
    return it == table.cend() || it->state.isEmpty() ? kOff : it->state;
}

QString SpotSourceHost::text(const QString& source) const
{
    const bool station = forwardsStationSources() && isStationSource(source);
    const auto& table = station ? m_station : m_local;
    return table.value(source).text;
}

bool SpotSourceHost::isRunning(const QString& source) const
{
    const QString s = state(source);
    return s == kConnected || s == kConnecting;
}

void SpotSourceHost::setStationForwarder(StationForwarder forwarder)
{
    m_forwarder = std::move(forwarder);
    emit sourcesChanged();
    for (const QString& source : stationSources()) {
        emit sourceChanged(source);
    }
}

bool SpotSourceHost::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == "freedvReporterHidden") {
        if (value.typeId() != QMetaType::Bool) {
            return false;
        }
        if (m_stationFreedvHidden != value.toBool()) {
            m_stationFreedvHidden = value.toBool();
            emit sourcesChanged();
            emit sourceChanged(kFreedvReporter);
        }
        return true;
    }
    if (value.typeId() != QMetaType::QString) {
        return false;
    }
    for (const QString& source : stationSources()) {
        const QByteArray stateName = source.toUtf8() + "State";
        const QByteArray textName = source.toUtf8() + "Text";
        if (propertyName != stateName && propertyName != textName) {
            continue;
        }
        SourceState& entry = m_station[source];
        QString& field = propertyName == stateName ? entry.state : entry.text;
        if (field != value.toString()) {
            field = value.toString();
            emit sourcesChanged();
            emit sourceChanged(source);
        }
        return true;
    }
    return false;
}

void SpotSourceHost::clearStationValues()
{
    if (m_station.isEmpty() && !m_stationFreedvHidden) {
        return;
    }
    m_station.clear();
    m_stationFreedvHidden = false;
    emit sourcesChanged();
    for (const QString& source : stationSources()) {
        emit sourceChanged(source);
    }
}

void SpotSourceHost::appendStationConsole(const QString& source, const QStringList& lines,
                                          bool replace)
{
    if (replace) {
        emit consoleCleared(source);
    }
    for (const QString& line : lines) {
        emit consoleLine(source, line);
    }
}

void SpotSourceHost::reportStationRefusal(const QString& source, const QString& reason)
{
    emit sourceRefused(source, reason);
}

void SpotSourceHost::setFreedvForwarder(FreedvForwarder forwarder)
{
    m_freedvForwarder = std::move(forwarder);
}

// ── The Spot Hub's buttons ───────────────────────────────────────────────

void SpotSourceHost::connectCluster(const QString& host, quint16 port, const QString& callsign)
{
    if (forward("spots.connect", kDxCluster) || !m_dxCluster) {
        return;
    }
    setSource(kDxCluster, kConnecting);
    m_dxCluster->connectToCluster(host, port, callsign);
}

void SpotSourceHost::disconnectCluster()
{
    if (forward("spots.disconnect", kDxCluster) || !m_dxCluster) {
        return;
    }
    m_dxCluster->disconnect();
}

void SpotSourceHost::connectRbn(const QString& host, quint16 port, const QString& callsign)
{
    if (forward("spots.connect", kRbn) || !m_rbn) {
        return;
    }
    setSource(kRbn, kConnecting);
    m_rbn->connectToCluster(host, port, callsign);
}

void SpotSourceHost::disconnectRbn()
{
    if (forward("spots.disconnect", kRbn) || !m_rbn) {
        return;
    }
    m_rbn->disconnect();
}

void SpotSourceHost::startWsjtx(const QString& address, quint16 port)
{
    if (m_wsjtx) {
        m_wsjtx->startListening(address, port);
    }
}

void SpotSourceHost::stopWsjtx()
{
    if (m_wsjtx) {
        m_wsjtx->stopListening();
    }
}

void SpotSourceHost::startSpotCollector(quint16 port)
{
    if (m_spotCollector) {
        m_spotCollector->startListening(port);
    }
}

void SpotSourceHost::stopSpotCollector()
{
    if (m_spotCollector) {
        m_spotCollector->stopListening();
    }
}

void SpotSourceHost::startPota(int intervalSec)
{
    if (forward("spots.connect", kPota) || !m_pota) {
        return;
    }
    m_pota->startPolling(intervalSec);
}

void SpotSourceHost::stopPota()
{
    if (forward("spots.disconnect", kPota) || !m_pota) {
        return;
    }
    m_pota->stopPolling();
}

void SpotSourceHost::startPskReporter(const QString& callsign, const QString& gridSquare)
{
    if (forward("spots.connect", kPskReporter) || !m_pskReporter) {
        return;
    }
    startPskReporterWith(callsign, gridSquare);
}

void SpotSourceHost::stopPskReporter()
{
    if (forward("spots.disconnect", kPskReporter) || !m_pskReporter) {
        return;
    }
    // From freedv-gui main.cpp:2694 [@77e793a]:
    //   m_pskReporterTimer.Stop();
    // "Stop" = disarm the timer.  Any queued records flush on
    // ~PskReporterClient when the client tears down (mirrors
    // pskreporter.cpp:171-181 [@77e793a]).
    m_pskReporter->setAutoSendIntervalSec(0);
    setSource(kPskReporter, kOff);
}

void SpotSourceHost::startFreedvReporter()
{
    if (forward("spots.connect", kFreedvReporter) || !m_freedv) {
        return;
    }
    QString reason;
    if (!startFreedvWith(&reason)) {
        emit sourceRefused(kFreedvReporter, reason);
    }
}

void SpotSourceHost::stopFreedvReporter()
{
    if (forward("spots.disconnect", kFreedvReporter) || !m_freedv) {
        return;
    }
    disconnectSource(kFreedvReporter, nullptr);
}

void SpotSourceHost::sendFreedvMessage(const QString& text)
{
    if (forwardFreedv("freedv.setMessage", {{QStringLiteral("text"), text}})) {
        return;
    }
    QString reason;
    if (!setFreedvMessage(text, &reason)) {
        emit sourceRefused(kFreedvReporter, reason);
    }
}

void SpotSourceHost::requestFreedvQsy(const QString& callsign, qint64 frequencyHz)
{
    if (forwardFreedv("freedv.sendQsy", {{QStringLiteral("callsign"), callsign},
                                         {QStringLiteral("frequencyHz"), frequencyHz}})) {
        return;
    }
    QString reason;
    if (!sendFreedvQsy(callsign, frequencyHz, &reason)) {
        emit sourceRefused(kFreedvReporter, reason);
    }
}

void SpotSourceHost::hideFreedvStation(bool on)
{
    if (forwardFreedv("freedv.setHidden", {{QStringLiteral("on"), on}})) {
        return;
    }
    QString reason;
    if (!setFreedvHidden(on, &reason)) {
        emit sourceRefused(kFreedvReporter, reason);
    }
}

void SpotSourceHost::typeCommand(const QString& source, const QString& text)
{
    if (forward("spots.sendCommand", source, text)) {
        return;
    }
    QString reason;
    if (!sendCommand(source, text, &reason)) {
        emit sourceRefused(source, reason);
    }
}

void SpotSourceHost::clearAllSpots()
{
    if (m_forwarder) {
        QString reason;
        if (!m_forwarder("spots.clearAll", QString(), QString(), &reason)) {
            emit sourceRefused(QString(), reason);
        }
    }
    clearAll();
}

// ── Private ──────────────────────────────────────────────────────────────

void SpotSourceHost::setSource(const QString& source, const QString& state, const QString& text)
{
    SourceState& entry = m_local[source];
    const QString current = entry.state.isEmpty() ? kOff : entry.state;
    if (current == state && entry.text == text) {
        entry.state = state;
        return;
    }
    entry.state = state;
    entry.text = text;
    emit sourcesChanged();
    emit sourceChanged(source);
}

bool SpotSourceHost::forward(const QByteArray& verb, const QString& source, const QString& text)
{
    if (!m_forwarder || !isStationSource(source)) {
        return false;
    }
    QString reason;
    if (!m_forwarder(verb, source, text, &reason)) {
        emit sourceRefused(source, reason);
    }
    return true;
}

bool SpotSourceHost::forwardFreedv(const QByteArray& verb, const QVariantMap& args)
{
    if (!m_forwarder) {
        return false; // this window runs FreeDV Reporter itself
    }
    QString reason;
    if (!m_freedvForwarder) {
        reason = QStringLiteral("Not connected to the Core, so the FreeDV Reporter request was "
                                "not sent.");
    } else if (m_freedvForwarder(verb, args, &reason)) {
        return true;
    }
    emit sourceRefused(kFreedvReporter, reason);
    return true;
}

bool SpotSourceHost::startFreedvWith(QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };
    if (m_freedv == nullptr) {
        return refuse(QStringLiteral("The Core does not run that spot source."));
    }
    // Post-3J-2 UX fix [moved from RadioModel::restoreSpotClientAutoStartState
    // in iPhone plan Task 22]: re-resolve identity from the saved settings
    // and call setIdentity() before startConnection(); with no callsign or
    // grid the qso.freedv.org server would take the session as view-only
    // and never list the station, so the start is refused instead.
    const QString call = freedvCallsign();
    const QString grid = freedvGridSquare();
    if (call.isEmpty() || grid.isEmpty()) {
        return refuse(QStringLiteral("Enter your callsign and grid square in Spot Hub first."));
    }
    if (m_freedv->isConnected()) {
        return true;
    }
    const QString message =
        AppSettings::instance().value(QStringLiteral("FreeDvReporter/Message")).toString();
    qCInfo(lcDsp) << "FreeDVReporter: starting connection with identity"
                  << "callsign=" << call << "grid=" << grid << "msg=" << message
                  << "version=" << versionString();
    m_freedv->setIdentity(call, grid, message, versionString());
    setSource(kFreedvReporter, kConnecting);
    m_freedv->startConnection();
    return true;
}

void SpotSourceHost::startPskReporterWith(const QString& callsign, const QString& gridSquare)
{
    // 2026-05-12 bench fix: PSK Reporter Start button source-first
    // port from freedv-gui.  The dialog emitted pskStartRequested
    // but nothing in MainWindow handled it.
    //
    // From freedv-gui main.cpp:2594-2597 [@77e793a]:
    //   // Enable FreeDV Reporter timer (every 5 minutes).
    //   m_pskReporterTimer.Start(5 * 60 * 1000);
    // and main.cpp:1609-1616 [@77e793a]:
    //   if (timerId == ID_TIMER_PSKREPORTER) {
    //       // Reporter timer fired; send in-progress packet.
    //       for (auto& obj : wxGetApp().m_reporters) obj->send();
    //   }
    // PSK Reporter is a send-only IPFIX client (pskreporter.h:65-68
    // [@77e793a]: freqChange / transmit / inAnalogMode are no-ops).
    // "Start" = arm the 5-minute auto-send timer.
    //
    // 2026-05-12 bench fix (PR #238 review P2):
    // apply the freshly-validated identity to the
    // live client BEFORE arming the timer.  Without
    // this call, the client keeps the (often empty)
    // identity set at RadioModel construction time
    // and emits IPFIX datagrams with empty receiver
    // fields.  pskreporter.cpp:148-169 [@77e793a].
    // [moved from MainWindow::openSpotHub in parity Task 19]
    m_pskReporter->setIdentity(callsign, gridSquare, versionString());
    // Enable FreeDV Reporter timer (every 5 minutes).  [original inline comment from main.cpp:2594]
    // Reporter timer fired; send in-progress packet.  [original inline comment from main.cpp:1611;
    // the tick is PskReporterClient's own timer, armed here]
    m_pskReporter->setAutoSendIntervalSec(PskReporterClient::kReportingIntervalSec);
    setSource(kPskReporter, kConnected, QStringLiteral("Reporting every 5 minutes"));
}

} // namespace NereusSDR
