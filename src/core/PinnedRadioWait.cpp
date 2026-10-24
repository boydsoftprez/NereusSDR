// =================================================================
// src/core/PinnedRadioWait.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original: no Thetis or AetherSDR upstream.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - A scan started by a request the quiet period held back
//                 counts as joined, never stopped by the backstop. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/PinnedRadioWait.h"

#include "core/LogCategories.h"

#include <QLoggingCategory>

#include <limits>

namespace NereusSDR {

PinnedRadioWait::PinnedRadioWait(RadioDiscovery* discovery, const QString& mac, QObject* parent)
    : QObject(parent)
    , m_discovery(discovery)
    , m_mac(mac)
{
    m_backstop.setSingleShot(true);
    connect(&m_backstop, &QTimer::timeout, this, &PinnedRadioWait::onBackstop);
}

void PinnedRadioWait::start()
{
    if (!m_discovery) {
        m_done = true;
        emit gaveUp();
        return;
    }

    // Already listed: a reply the running scan handled before this wait
    // began. Connect at once.
    const QList<RadioInfo> listed = m_discovery->discoveredRadios();
    for (const RadioInfo& info : listed) {
        if (info.macAddress == m_mac) {
            m_done = true;
            emit found(info);
            return;
        }
    }

    // A radio already listed by an earlier scan comes back as radioUpdated,
    // a new one as radioDiscovered.
    connect(m_discovery, &RadioDiscovery::radioDiscovered, this, &PinnedRadioWait::onRadio);
    connect(m_discovery, &RadioDiscovery::radioUpdated, this, &PinnedRadioWait::onRadio);
    connect(m_discovery, &RadioDiscovery::discoveryFinished, this, &PinnedRadioWait::onScanFinished);
    // A scan deferred by the post-disconnect quiet period arms the backstop
    // when it starts.
    connect(m_discovery, &RadioDiscovery::discoveryStarted, this, &PinnedRadioWait::armBackstop);

    // The wait owns a scan only when its own request started it here. A
    // request held back by the quiet period may later run, or may join a
    // scan someone else started first; which one is not known, so such a
    // scan is treated as joined and never stopped by the backstop.
    if (!m_discovery->isScanRunning()) {
        m_discovery->startDiscovery();
        m_startedScan = !m_done && m_discovery && m_discovery->isScanRunning();
    }
    if (!m_done && m_discovery && m_discovery->isScanRunning()) {
        armBackstop();
    }
}

void PinnedRadioWait::onRadio(const RadioInfo& info)
{
    if (m_done || m_waitingForScanEndOnly || info.macAddress != m_mac) {
        return;
    }
    finish();
    emit found(info);
}

void PinnedRadioWait::onScanFinished()
{
    if (m_done) {
        return;
    }
    finish();
    emit gaveUp();
}

void PinnedRadioWait::onBackstop()
{
    if (m_done || !m_discovery) {
        return;
    }
    // A quiet period armed during the scan holds its attempts back; that is
    // a scan behaving, so wait for it.
    if (m_discovery->isScanRunning() && m_discovery->holdOffRemainingMs() > 0) {
        armBackstop();
        return;
    }
    if (m_startedScan || !m_discovery->isScanRunning()) {
        qCInfo(lcConnection) << "Auto-reconnect: the scan outlived its backstop; stopping it";
        finish();
        if (m_startedScan) {
            m_discovery->stopDiscovery();
        }
        emit gaveUp();
        return;
    }
    // A joined scan keeps running for the radios on later adapters. The
    // wait stops taking the radio and gives up when that scan ends.
    qCInfo(lcConnection) << "Auto-reconnect: the joined scan outlived its backstop;"
                         << "giving up when it ends";
    m_waitingForScanEndOnly = true;
}

void PinnedRadioWait::armBackstop()
{
    if (m_done || !m_discovery) {
        return;
    }
    const qint64 boundMs = m_discovery->walkBoundMs();
    const qint64 ms = m_backstopOverrideMs.has_value()
        ? qint64(*m_backstopOverrideMs)
        : qint64(kBackstopFactor) * boundMs + kBackstopSlackMs;
    m_backstop.start(int(qMin<qint64>(ms, std::numeric_limits<int>::max())));
}

void PinnedRadioWait::finish()
{
    m_done = true;
    m_backstop.stop();
    if (m_discovery) {
        disconnect(m_discovery, nullptr, this, nullptr);
    }
}

} // namespace NereusSDR
