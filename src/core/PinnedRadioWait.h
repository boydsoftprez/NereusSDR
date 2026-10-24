// =================================================================
// src/core/PinnedRadioWait.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original: no Thetis or AetherSDR upstream.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code. The
//                 wait MainWindow::tryAutoReconnect() used to run inline
//                 with a fixed 3 s timeout, now driven by the scan's own
//                 end (review of PR 600, Important 1).
// =================================================================

#pragma once

#include "core/RadioDiscovery.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <optional>

namespace NereusSDR {

// Waits for one MAC (the pinned auto-connect radio) on a RadioDiscovery.
//
// start() checks the radios already listed first, so a reply handled before
// the wait began is not missed. Otherwise it listens to radioDiscovered and
// radioUpdated for the MAC, joins a running scan or starts one, and waits
// for that scan to finish: the scan's own end decides, not a fixed time.
// A backstop sized from the scan's own timing (RadioDiscovery::walkBoundMs())
// only guards a scan that never ends.
//
// Exactly one of found() or gaveUp() is emitted, once.
//   found(info)  the radio was listed or replied (inside start() when it was
//                already listed).
//   gaveUp()     the scan waited on finished without the radio. When the
//                backstop fires on a scan this wait started, it stops that
//                scan and gives up. When the backstop fires on a scan it
//                joined (the startup scan), that scan keeps running, so
//                radios on later adapters still reach the list; the wait
//                stops taking the radio and gives up when that scan ends.
class PinnedRadioWait : public QObject {
    Q_OBJECT
public:
    // The backstop is kBackstopFactor times the scan's longest remaining
    // time plus kBackstopSlackMs, so it never ends a scan that is behaving.
    static constexpr int kBackstopFactor = 2;
    static constexpr int kBackstopSlackMs = 2000;

    PinnedRadioWait(RadioDiscovery* discovery, const QString& mac, QObject* parent = nullptr);

    void start();

    // True when start() found no scan running and its own request started
    // one at once (not one held back by the post-disconnect quiet period).
    bool startedScan() const { return m_startedScan; }

#ifdef NEREUS_BUILD_TESTS
    void setBackstopForTest(int ms) { m_backstopOverrideMs = ms; }
#endif

signals:
    void found(const NereusSDR::RadioInfo& info);
    void gaveUp();

private:
    void onRadio(const NereusSDR::RadioInfo& info);
    void onScanFinished();
    void onBackstop();
    void armBackstop();
    void finish();

    QPointer<RadioDiscovery> m_discovery;
    QString m_mac;
    QTimer m_backstop;
    bool m_startedScan{false};
    bool m_done{false};
    bool m_waitingForScanEndOnly{false};
    std::optional<int> m_backstopOverrideMs;   // tests only
};

} // namespace NereusSDR
