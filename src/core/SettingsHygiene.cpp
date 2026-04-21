// =================================================================
// src/core/SettingsHygiene.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Validates persisted AppSettings against the
// connected board's BoardCapabilities. No Thetis port at this layer.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include "SettingsHygiene.h"
#include "AppSettings.h"

namespace NereusSDR {

SettingsHygiene::SettingsHygiene(QObject* parent)
    : QObject(parent)
{
}

// ── Public API ─────────────────────────────────────────────────────────────

void SettingsHygiene::validate(const QString& mac, const BoardCapabilities& caps)
{
    m_issues.clear();

    checkStepAtt(mac, caps);
    checkSaturnBpf1(mac, caps);
    checkN2adrFilter(mac, caps);
    checkApolloSettings(mac, caps);
    checkAlexAntenna(mac, caps);

    emit issuesChanged();
}

QVector<SettingsHygiene::Issue> SettingsHygiene::issues() const
{
    return m_issues;
}

int SettingsHygiene::issueCount() const
{
    return m_issues.size();
}

bool SettingsHygiene::hasIssues() const
{
    return !m_issues.isEmpty();
}

void SettingsHygiene::resetSettingsToDefaults(const QString& mac,
                                               const BoardCapabilities& caps)
{
    auto& s = AppSettings::instance();

    // Clamp S-ATT to board max.
    const QString attKey = QStringLiteral("hardware/%1/sAtt").arg(mac);
    if (s.contains(attKey)) {
        int persisted = s.value(attKey, 0).toInt();
        if (persisted > caps.attenuator.maxDb) {
            s.setValue(attKey, caps.attenuator.maxDb);
        }
    }

    // Clear Saturn BPF1 settings if not a Saturn board.
    if (caps.board != HPSDRHW::Saturn) {
        const QString bpf1Base = QStringLiteral("hardware/%1/alex/bpf1").arg(mac);
        // Walk known band keys and remove.
        const QStringList bands = {
            QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
            QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"),
            QStringLiteral("17m"), QStringLiteral("15m"), QStringLiteral("12m"),
            QStringLiteral("10m"), QStringLiteral("6m")
        };
        for (const QString& band : bands) {
            s.remove(QStringLiteral("%1/%2/start").arg(bpf1Base, band));
            s.remove(QStringLiteral("%1/%2/end").arg(bpf1Base, band));
        }
    }

    // Clear Apollo settings if board doesn't have Apollo.
    if (!caps.hasApollo) {
        s.remove(QStringLiteral("hardware/%1/apollo/enabled").arg(mac));
        s.remove(QStringLiteral("hardware/%1/apollo/filter").arg(mac));
        s.remove(QStringLiteral("hardware/%1/apollo/tuner").arg(mac));
    }

    s.save();

    // Re-validate to clear any issues that were fixed.
    validate(mac, caps);
}

void SettingsHygiene::forgetRadio(const QString& mac)
{
    auto& s = AppSettings::instance();

    // Remove all keys under hardware/<mac>/ by collecting them first.
    const QString prefix = QStringLiteral("hardware/%1/").arg(mac);
    const QStringList all = s.allKeys();
    for (const QString& key : all) {
        if (key.startsWith(prefix)) {
            s.remove(key);
        }
    }
    // Also remove hardware/<mac> root key if any.
    s.remove(QStringLiteral("hardware/%1").arg(mac));
    s.save();

    m_issues.clear();
    emit issuesChanged();
}

// ── Private validation rules ───────────────────────────────────────────────

void SettingsHygiene::checkStepAtt(const QString& mac,
                                    const BoardCapabilities& caps)
{
    if (!caps.attenuator.present) { return; }

    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/sAtt").arg(mac);
    if (!s.contains(key)) { return; }

    int persisted = s.value(key, 0).toInt();
    if (persisted > caps.attenuator.maxDb) {
        addAttClampIssue(mac, persisted, caps.attenuator.maxDb);
    }
}

void SettingsHygiene::checkSaturnBpf1(const QString& mac,
                                       const BoardCapabilities& caps)
{
    // Saturn BPF1 per-band edges are only meaningful on Saturn boards.
    // If we find any stored for a non-Saturn board, report it as Info.
    if (caps.board == HPSDRHW::Saturn) { return; }

    auto& s = AppSettings::instance();
    const QString bpf1Key = QStringLiteral("hardware/%1/alex/bpf1/160m/start").arg(mac);
    if (!s.contains(bpf1Key)) { return; }

    // no-port-check: NereusSDR-original rule.
    Issue issue;
    issue.severity = Severity::Info;
    issue.key      = QStringLiteral("hardware/%1/alex/bpf1").arg(mac);
    issue.summary  = QStringLiteral("Saturn BPF1 settings stored for non-Saturn board");
    issue.detail   = QStringLiteral(
        "AppSettings contains Saturn BPF1 band-edge data for radio %1, "
        "but this board does not use BPF1 overrides. "
        "These settings are harmless but can be removed via 'Forget Radio'."
    ).arg(mac);
    issue.fixActionId = QStringLiteral("forgetRadio");
    m_issues.append(issue);
}

void SettingsHygiene::checkN2adrFilter(const QString& mac,
                                        const BoardCapabilities& caps)
{
    // N2ADR filter is an HL2-only hardware option.
    if (caps.hasIoBoardHl2) { return; }

    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/n2adr/filterEnabled").arg(mac);
    if (!s.contains(key)) { return; }

    bool enabled = (s.value(key, QStringLiteral("False")).toString() ==
                    QStringLiteral("True"));
    if (!enabled) { return; }

    // no-port-check: NereusSDR-original rule.
    Issue issue;
    issue.severity = Severity::Warning;
    issue.key      = key;
    issue.summary  = QStringLiteral("N2ADR filter enabled on non-HL2 board");
    issue.detail   = QStringLiteral(
        "The N2ADR filter setting is enabled in AppSettings for radio %1, "
        "but this board type does not have an HL2 I/O board. "
        "The setting is ignored at runtime but may confuse future users."
    ).arg(mac);
    issue.fixActionId = QStringLiteral("resetToDefaults");
    m_issues.append(issue);
}

void SettingsHygiene::checkApolloSettings(const QString& mac,
                                           const BoardCapabilities& caps)
{
    if (caps.hasApollo) { return; }

    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/apollo/enabled").arg(mac);
    if (!s.contains(key)) { return; }

    bool enabled = (s.value(key, QStringLiteral("False")).toString() ==
                    QStringLiteral("True"));
    if (!enabled) { return; }

    // no-port-check: NereusSDR-original rule.
    Issue issue;
    issue.severity = Severity::Warning;
    issue.key      = key;
    issue.summary  = QStringLiteral("Apollo tuner enabled on non-Apollo board");
    issue.detail   = QStringLiteral(
        "Apollo tuner/filter settings are persisted for radio %1, "
        "but this board does not support Apollo hardware. "
        "The settings are ignored at runtime."
    ).arg(mac);
    issue.fixActionId = QStringLiteral("resetToDefaults");
    m_issues.append(issue);
}

void SettingsHygiene::checkAlexAntenna(const QString& mac,
                                        const BoardCapabilities& caps)
{
    // Only report on boards with Alex TX routing capability.
    if (!caps.hasAlexTxRouting) { return; }

    auto& s = AppSettings::instance();

    // Only report if the user has started persisting some alex/antenna keys
    // but has left the 20m TX antenna unset.  If no antenna keys exist at all
    // the settings are freshly provisioned and the default (Ant 1) is correct;
    // surfacing an issue in that case would be noise.
    const QString anyKeyProbe =
        QStringLiteral("hardware/%1/alex/antenna").arg(mac);

    bool anyAntennaKeyExists = false;
    const QStringList all = s.allKeys();
    for (const QString& key : all) {
        if (key.startsWith(anyKeyProbe)) {
            anyAntennaKeyExists = true;
            break;
        }
    }
    if (!anyAntennaKeyExists) { return; }  // fresh state — no issue

    const QString txKey =
        QStringLiteral("hardware/%1/alex/antenna/20m/tx").arg(mac);
    if (s.contains(txKey)) { return; }  // 20m TX is set — OK

    // no-port-check: NereusSDR-original rule.
    Issue issue;
    issue.severity = Severity::Info;
    issue.key      = QStringLiteral("hardware/%1/alex/antenna").arg(mac);
    issue.summary  = QStringLiteral("Alex TX antenna not configured for all bands");
    issue.detail   = QStringLiteral(
        "No Alex TX antenna has been saved for radio %1 on 20m. "
        "Default Antenna 1 will be used. Visit Hardware → Antenna Control "
        "to configure per-band antenna routing."
    ).arg(mac);
    issue.fixActionId = QString();  // user-action only
    m_issues.append(issue);
}

void SettingsHygiene::addAttClampIssue(const QString& mac,
                                        int persisted, int maxDb)
{
    Issue issue;
    issue.severity = Severity::Warning;
    issue.key      = QStringLiteral("hardware/%1/sAtt").arg(mac);
    issue.summary  = QStringLiteral("S-ATT setting clamped");
    issue.detail   = QStringLiteral(
        "Persisted attenuator value %1 dB exceeds this radio's 0–%2 dB range "
        "— will be clamped to %2 dB on next connect. "
        "Use Reset to Defaults to fix."
    ).arg(persisted).arg(maxDb);
    issue.fixActionId = QStringLiteral("resetToDefaults");
    m_issues.append(issue);
}

} // namespace NereusSDR
