// =================================================================
// src/core/TxInterlockPolicy.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native TX interlock policy. See TxInterlockPolicy.h for
// full design notes.
//
// AI tooling: Anthropic Claude Code.

#include "core/TxInterlockPolicy.h"
#include "core/AppSettings.h"

namespace NereusSDR {

TxInterlockPolicy::TxInterlockPolicy(QObject* parent)
    : QObject(parent)
{
    auto& s = AppSettings::instance();

    // Load Mode from string.
    const QString modeStr = s.value("PGXL_TxInterlockMode", "Disabled").toString();
    if (modeStr == "Warn") {
        m_mode = Warn;
    } else if (modeStr == "Block") {
        m_mode = Block;
    } else {
        m_mode = Disabled;
    }

    m_graceMs        = s.value("PGXL_TxInterlockGraceMs", 3000).toInt();
    m_swrGateEnabled = s.value("PGXL_TxSwrGate", "False").toString() == "True";
    m_swrGateMax     = s.value("PGXL_TxSwrGateMax", "3.0").toString().toFloat();
}

bool TxInterlockPolicy::evaluateTxRequest(bool ampPresent, bool ampInOperate, float currentSwr)
{
    if (m_mode == Disabled) {
        return true;
    }

    // Check 1: amplifier present but not in OPERATE.
    if (ampPresent && !ampInOperate) {
        const QString reason = "Amplifier present but not in OPERATE";
        if (m_mode == Block) {
            emit denied(reason);
            return false;
        }
        emit warned(reason);
        return true;
    }

    // Check 2: SWR gate.
    if (m_swrGateEnabled && currentSwr > m_swrGateMax) {
        const QString reason = QString("SWR %1 above limit %2")
                                   .arg(static_cast<double>(currentSwr))
                                   .arg(static_cast<double>(m_swrGateMax));
        if (m_mode == Block) {
            emit denied(reason);
            return false;
        }
        emit warned(reason);
        return true;
    }

    return true;
}

void TxInterlockPolicy::setMode(Mode m)
{
    if (m_mode == m) { return; }
    m_mode = m;
    QString modeStr;
    switch (m) {
    case Warn:  modeStr = "Warn";  break;
    case Block: modeStr = "Block"; break;
    default:    modeStr = "Disabled"; break;
    }
    AppSettings::instance().setValue("PGXL_TxInterlockMode", modeStr);
    emit changed();
}

void TxInterlockPolicy::setGraceMs(int ms)
{
    if (m_graceMs == ms) { return; }
    m_graceMs = ms;
    AppSettings::instance().setValue("PGXL_TxInterlockGraceMs", ms);
    emit changed();
}

void TxInterlockPolicy::setSwrGateEnabled(bool on)
{
    if (m_swrGateEnabled == on) { return; }
    m_swrGateEnabled = on;
    AppSettings::instance().setValue("PGXL_TxSwrGate", on ? "True" : "False");
    emit changed();
}

void TxInterlockPolicy::setSwrGateMax(float x)
{
    if (qFuzzyCompare(m_swrGateMax, x)) { return; }
    m_swrGateMax = x;
    AppSettings::instance().setValue("PGXL_TxSwrGateMax", QString::number(static_cast<double>(x)));
    emit changed();
}

}  // namespace NereusSDR
