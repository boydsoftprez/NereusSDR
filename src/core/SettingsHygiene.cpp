// no-port-check: NereusSDR-original AppSettings validator.  Registered as
// NereusSDR-native in docs/attribution/THETIS-PROVENANCE.md.  The single
// inline upstream reference further down (BPF1-algorithm board family
// check for HermesC10 / ANAN-G2E) is a parity citation, not a port of
// upstream logic into this file.  Author tags preserved verbatim at the
// citation line.
//
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
#include "AlexSettingsKeys.h"
#include "AppSettings.h"
#include "codec/AlexFilterMap.h"

namespace NereusSDR {

namespace {

// hardware/<mac>/alex/bpf1/<slug>/<leaf>: the flat-map form, which is what
// AppSettings::contains / remove take.  The Setup tab writes the same key via
// setHardwareValue(mac, "alex/bpf1/<slug>/<leaf>", v), which prepends
// hardware/<mac>/ for you.
QString bpf1Key(const QString& mac, const char* slug, const char* leaf)
{
    return QStringLiteral("hardware/%1/%2/%3/%4")
        .arg(mac,
             QLatin1String(alexKeys::kAlex1Bpf1Prefix),
             QLatin1String(slug),
             QLatin1String(leaf));
}

} // namespace

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

    // Clear Saturn BPF1 settings if not a BPF1-algorithm board.
    // HermesC10 (ANAN-G2E) uses the BPF1 algorithm path alongside Saturn/SaturnMKII.
    // From Thetis console.cs:6829-6834 [v2.10.3.15] //N1GP G2E added (HermesC10) //DK1HLM
    //
    // Routed through the single canonical predicate rather than an open-coded
    // board list.  The list here used to omit OrionMKII (ANAN-7000DLE /
    // 8000DLE / Anvelina Pro 3 / Red Pitaya), which Thetis DOES dispatch to
    // setBPF1ForOrionIISaturn, so a 7000DLE owner's BPF1 band edges were being
    // discarded as if they belonged to a board that had no such filters.
    const bool usesBpf1 = codec::alex::usesBpf1Preselector(caps.board);
    if (!usesBpf1) {
        // Walk the rows the Setup tab actually writes.  This loop used to
        // iterate eleven ham-band names (160m, 80m, … 6m) that AntennaAlexAlex1Tab
        // has never written: it persists six crossover slugs (1_5MHz, 6_5MHz,
        // 9_5MHz, 13MHz, 20MHz, 6mBP).  The two name sets did not overlap, so
        // the removal was a silent no-op for every user.  Both sides now read
        // the slugs out of alexKeys so they cannot drift apart again.
        //
        // "enabled" is swept alongside the two edges.  The old loop skipped it,
        // which would have stranded the per-band bypass flag even on a slug it
        // did know.
        for (const char* slug : alexKeys::kPreselectorSlugs) {
            for (const char* leaf : alexKeys::kPreselectorLeaves) {
                s.remove(bpf1Key(mac, slug, leaf));
            }
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
    // Saturn BPF1 per-band edges are only meaningful on BPF1-algorithm boards.
    // HermesC10 (ANAN-G2E) uses the BPF1 algorithm alongside Saturn/SaturnMKII.
    // From Thetis console.cs:6829-6834 [v2.10.3.15] //N1GP G2E added (HermesC10) //DK1HLM
    //
    // Same canonical predicate as resetSettingsToDefaults. The open-coded
    // list this replaced omitted OrionMKII, so a 7000DLE's stored BPF1 edges
    // were reported as stray data for a board that does not use them.
    if (codec::alex::usesBpf1Preselector(caps.board)) { return; }

    auto& s = AppSettings::instance();

    // Probe every row the Setup tab can write, not just the first one.  This
    // used to test the single key hardware/<mac>/alex/bpf1/160m/start, which
    // AntennaAlexAlex1Tab never writes under that name.  It writes six
    // crossover slugs, and only for rows the operator actually touched.  So
    // the probe never fired, and stray BPF1 data was never reported.
    bool anyBpf1KeyExists = false;
    for (const char* slug : alexKeys::kPreselectorSlugs) {
        for (const char* leaf : alexKeys::kPreselectorLeaves) {
            if (s.contains(bpf1Key(mac, slug, leaf))) {
                anyBpf1KeyExists = true;
                break;
            }
        }
        if (anyBpf1KeyExists) { break; }
    }
    if (!anyBpf1KeyExists) { return; }

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
