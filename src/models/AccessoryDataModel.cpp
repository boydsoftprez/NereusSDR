// no-port-check: NereusSDR-original. See AccessoryDataModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/AccessoryDataModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10): the RF-Kit's
//                                    connection counts. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "models/AccessoryDataModel.h"

#include <QtGlobal>

namespace NereusSDR {

QString AccessoryDataModel::readOnlyReason()
{
    return QStringLiteral("The Core keeps the amplifier and tuner records and settings. "
                          "Change them from this app's Setup pages.");
}

AccessoryDataModel::AccessoryDataModel(QObject* parent)
    : QObject(parent)
    , m_tgxlLabels(sized({}, kTgxlAntennas))
    , m_rfkitLabels(sized({}, kRfKitAntennas))
{
}

QString AccessoryDataModel::faultsFor(const QString& device) const
{
    if (device == QLatin1String("tgxl")) {
        return m_tgxlFaults;
    }
    if (device == QLatin1String("rfkit")) {
        return m_rfkitFaults;
    }
    return m_pgxlFaults;
}

// static
QStringList AccessoryDataModel::sized(const QStringList& labels, int count)
{
    QStringList out = labels.mid(0, count);
    while (out.size() < count) {
        out.append(QString());
    }
    return out;
}

void AccessoryDataModel::setFaults(const QString& pgxl, const QString& tgxl, const QString& rfkit)
{
    if (pgxl == m_pgxlFaults && tgxl == m_tgxlFaults && rfkit == m_rfkitFaults) {
        return;
    }
    m_pgxlFaults = pgxl;
    m_tgxlFaults = tgxl;
    m_rfkitFaults = rfkit;
    ++m_faultRevision;
    emit faultsChanged();
}

void AccessoryDataModel::setPgxlDiagnostics(const ConnectionDiagnostics::Counters& counters)
{
    if (counters == m_pgxl) {
        return;
    }
    m_pgxl = counters;
    emit pgxlDiagnosticsChanged();
}

void AccessoryDataModel::setTgxlDiagnostics(const ConnectionDiagnostics::Counters& counters)
{
    if (counters == m_tgxl) {
        return;
    }
    m_tgxl = counters;
    emit tgxlDiagnosticsChanged();
}

void AccessoryDataModel::setRfKitDiagnostics(const RfKitCounters& counters)
{
    if (counters == m_rfkit) {
        return;
    }
    m_rfkit = counters;
    emit rfkitDiagnosticsChanged();
}

void AccessoryDataModel::setInterlock(InterlockMode mode, int graceMs, bool swrGateEnabled,
                                      double swrGateMax)
{
    if (mode == m_interlockMode && graceMs == m_interlockGraceMs
        && swrGateEnabled == m_interlockSwrGateEnabled
        && qFuzzyCompare(swrGateMax, m_interlockSwrGateMax)) {
        return;
    }
    m_interlockMode = mode;
    m_interlockGraceMs = graceMs;
    m_interlockSwrGateEnabled = swrGateEnabled;
    m_interlockSwrGateMax = swrGateMax;
    emit interlockChanged();
}

void AccessoryDataModel::setPowerCap(const PowerCap& cap)
{
    if (cap == m_powerCap) {
        return;
    }
    m_powerCap = cap;
    emit powerCapChanged();
}

void AccessoryDataModel::setTuneMemory(const QString& json, bool autoRecall)
{
    if (json == m_tuneMemory && autoRecall == m_autoTuneMemoryRecall) {
        return;
    }
    m_tuneMemory = json;
    m_autoTuneMemoryRecall = autoRecall;
    emit tuneMemoryChanged();
}

void AccessoryDataModel::setLabels(const QStringList& tgxl, const QStringList& rfkit)
{
    const QStringList t = sized(tgxl, kTgxlAntennas);
    const QStringList r = sized(rfkit, kRfKitAntennas);
    if (t == m_tgxlLabels && r == m_rfkitLabels) {
        return;
    }
    m_tgxlLabels = t;
    m_rfkitLabels = r;
    emit labelsChanged();
}

// static
bool AccessoryDataModel::applyCounter(ConnectionDiagnostics::Counters* c, const QByteArray& name,
                                      const QVariant& value)
{
    const qint64 v = value.toLongLong();
    if (name == "ConnectedSinceMs") {
        c->connectedSinceMs = v;
    } else if (name == "LastRttMs") {
        c->lastRttMs = v;
    } else if (name == "KeepaliveMissed") {
        c->keepaliveMissed = value.toInt();
    } else if (name == "ReconnectCount") {
        c->reconnectCount = value.toInt();
    } else if (name == "FramesIn") {
        c->framesIn = v;
    } else if (name == "FramesOut") {
        c->framesOut = v;
    } else if (name == "BytesIn") {
        c->bytesIn = v;
    } else if (name == "BytesOut") {
        c->bytesOut = v;
    } else if (name == "LastFrameMs") {
        c->lastFrameMs = v;
    } else if (name == "FaultsSession") {
        c->faultsSession = value.toInt();
    } else {
        return false;
    }
    return true;
}

bool AccessoryDataModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    // A plain state apply for every property: nothing here is ever a
    // command to an accessory.
    if (propertyName == "faultRevision") {
        if (value.toLongLong() != m_faultRevision) {
            m_faultRevision = value.toLongLong();
            emit faultsChanged();
        }
        return true;
    }
    if (propertyName == "pgxlFaults" || propertyName == "tgxlFaults"
        || propertyName == "rfkitFaults") {
        QString* target = propertyName == "pgxlFaults" ? &m_pgxlFaults
            : propertyName == "tgxlFaults"             ? &m_tgxlFaults
                                                       : &m_rfkitFaults;
        if (*target != value.toString()) {
            *target = value.toString();
            emit faultsChanged();
        }
        return true;
    }
    if (propertyName.startsWith("pgxl")) {
        ConnectionDiagnostics::Counters next = m_pgxl;
        if (!applyCounter(&next, propertyName.mid(4), value)) {
            return false;
        }
        setPgxlDiagnostics(next);
        return true;
    }
    if (propertyName.startsWith("tgxlAntenna") || propertyName.startsWith("rfkitAntenna")) {
        const bool tgxl = propertyName.startsWith("tgxl");
        const QByteArray rest = propertyName.mid(tgxl ? 11 : 12);   // "<N>Label"
        const int index = rest.left(1).toInt() - 1;
        const int count = tgxl ? kTgxlAntennas : kRfKitAntennas;
        if (rest.mid(1) != "Label" || index < 0 || index >= count) {
            return false;
        }
        QStringList t = m_tgxlLabels;
        QStringList r = m_rfkitLabels;
        (tgxl ? t : r)[index] = value.toString();
        setLabels(t, r);
        return true;
    }
    if (propertyName.startsWith("rfkit")) {
        // R-R3-49 (parity Task 10): the RF-Kit's connection counts (its
        // faults and antenna names are handled above).
        RfKitCounters next = m_rfkit;
        if (propertyName == "rfkitConnectedSinceMs") {
            next.connectedSinceMs = value.toLongLong();
        } else if (propertyName == "rfkitPollsOk") {
            next.pollsOk = value.toInt();
        } else if (propertyName == "rfkitPollsFailed") {
            next.pollsFailed = value.toInt();
        } else if (propertyName == "rfkitReconnectCount") {
            next.reconnectCount = value.toInt();
        } else if (propertyName == "rfkitLastPollMs") {
            next.lastPollMs = value.toLongLong();
        } else if (propertyName == "rfkitRttAvgMs") {
            next.rttAvgMs = value.toInt();
        } else {
            return false;
        }
        setRfKitDiagnostics(next);
        return true;
    }
    if (propertyName.startsWith("tgxl")) {
        ConnectionDiagnostics::Counters next = m_tgxl;
        if (!applyCounter(&next, propertyName.mid(4), value)) {
            return false;
        }
        setTgxlDiagnostics(next);
        return true;
    }
    if (propertyName == "interlockMode") {
        const int raw = value.toInt();
        const InterlockMode mode = raw >= 0 && raw <= static_cast<int>(InterlockMode::Block)
            ? static_cast<InterlockMode>(raw) : InterlockMode::Disabled;
        setInterlock(mode, m_interlockGraceMs, m_interlockSwrGateEnabled, m_interlockSwrGateMax);
        return true;
    }
    if (propertyName == "interlockGraceMs") {
        setInterlock(m_interlockMode, value.toInt(), m_interlockSwrGateEnabled,
                     m_interlockSwrGateMax);
        return true;
    }
    if (propertyName == "interlockSwrGateEnabled") {
        setInterlock(m_interlockMode, m_interlockGraceMs, value.toBool(), m_interlockSwrGateMax);
        return true;
    }
    if (propertyName == "interlockSwrGateMax") {
        setInterlock(m_interlockMode, m_interlockGraceMs, m_interlockSwrGateEnabled,
                     value.toDouble());
        return true;
    }
    if (propertyName.startsWith("powerCap")) {
        PowerCap cap = m_powerCap;
        if (propertyName == "powerCapEnabled") {
            cap.enabled = value.toBool();
        } else if (propertyName == "powerCapW") {
            cap.watts = value.toInt();
        } else if (propertyName == "powerCapExceeded") {
            cap.exceeded = value.toBool();
        } else if (propertyName == "powerCapAlertCount") {
            cap.alertCount = value.toLongLong();
        } else if (propertyName == "powerCapAlertText") {
            cap.alertText = value.toString();
        } else {
            return false;
        }
        setPowerCap(cap);
        return true;
    }
    if (propertyName == "tuneMemory") {
        setTuneMemory(value.toString(), m_autoTuneMemoryRecall);
        return true;
    }
    if (propertyName == "autoTuneMemoryRecall") {
        setTuneMemory(m_tuneMemory, value.toBool());
        return true;
    }
    return false;
}

} // namespace NereusSDR
