// no-port-check: NereusSDR-original. Task 24 bounded Settings Hygiene reply.
// J.J. Boyd / KG4VCF, 2026-09-27. AI-assisted implementation via Codex.
#include "core/session/SettingsHygieneWire.h"

#include "core/AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace NereusSDR {
namespace {
constexpr int kMaxIssues = 32;
constexpr int kMaxJsonBytes = 64 * 1024;

bool bounded(const QString& text, int maxBytes)
{
    return text.toUtf8().size() <= maxBytes;
}

bool canonicalMac(const QString& mac)
{
    return !mac.isEmpty() && AppSettings::normalizedRadioMac(mac) == mac;
}

QString severityName(SettingsHygiene::Severity value)
{
    switch (value) {
    case SettingsHygiene::Severity::Info: return QStringLiteral("info");
    case SettingsHygiene::Severity::Warning: return QStringLiteral("warning");
    case SettingsHygiene::Severity::Critical: return QStringLiteral("critical");
    }
    return {};
}

std::optional<SettingsHygiene::Severity> severityValue(const QString& value)
{
    if (value == QStringLiteral("info")) { return SettingsHygiene::Severity::Info; }
    if (value == QStringLiteral("warning")) { return SettingsHygiene::Severity::Warning; }
    if (value == QStringLiteral("critical")) { return SettingsHygiene::Severity::Critical; }
    return std::nullopt;
}

bool boundedIssue(const SettingsHygiene::Issue& issue)
{
    return !severityName(issue.severity).isEmpty() && bounded(issue.key, 256)
        && bounded(issue.summary, 256) && bounded(issue.detail, 1024)
        && bounded(issue.fixActionId, 64);
}
} // namespace

std::optional<QList<MirrorUpdate>> SettingsHygieneWire::encode(const SettingsHygieneReply& reply)
{
    if (!canonicalMac(reply.mac) || reply.issues.size() > kMaxIssues) { return std::nullopt; }
    QJsonArray issues;
    for (const auto& issue : reply.issues) {
        if (!boundedIssue(issue)) { return std::nullopt; }
        issues.append(QJsonObject{{QStringLiteral("severity"), severityName(issue.severity)},
                                  {QStringLiteral("key"), issue.key},
                                  {QStringLiteral("summary"), issue.summary},
                                  {QStringLiteral("detail"), issue.detail},
                                  {QStringLiteral("fixActionId"), issue.fixActionId}});
    }
    const QByteArray json = QJsonDocument(issues).toJson(QJsonDocument::Compact);
    if (json.size() > kMaxJsonBytes) { return std::nullopt; }
    return QList<MirrorUpdate>{
        {0, "mac", MirrorWireKind::Utf8, reply.mac},
        {0, "issuesJson", MirrorWireKind::Utf8, QString::fromUtf8(json)}};
}

std::optional<SettingsHygieneReply> SettingsHygieneWire::decode(
    const QList<MirrorUpdate>& values)
{
    if (values.size() != 2) { return std::nullopt; }
    const MirrorUpdate* macValue = nullptr;
    const MirrorUpdate* issuesValue = nullptr;
    for (const auto& value : values) {
        if (value.ordinal != 0 || value.kind != MirrorWireKind::Utf8
            || value.value.typeId() != QMetaType::QString) {
            return std::nullopt;
        }
        if (value.name == "mac" && !macValue) { macValue = &value; }
        else if (value.name == "issuesJson" && !issuesValue) { issuesValue = &value; }
        else { return std::nullopt; }
    }
    if (!macValue || !issuesValue) { return std::nullopt; }
    SettingsHygieneReply reply;
    reply.mac = macValue->value.toString();
    const QByteArray json = issuesValue->value.toString().toUtf8();
    if (!canonicalMac(reply.mac) || json.size() > kMaxJsonBytes) { return std::nullopt; }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()
        || document.array().size() > kMaxIssues) { return std::nullopt; }
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) { return std::nullopt; }
        const QJsonObject object = value.toObject();
        if (object.size() != 5 || !object.value(QStringLiteral("severity")).isString()
            || !object.value(QStringLiteral("key")).isString()
            || !object.value(QStringLiteral("summary")).isString()
            || !object.value(QStringLiteral("detail")).isString()
            || !object.value(QStringLiteral("fixActionId")).isString()) {
            return std::nullopt;
        }
        const auto severity = severityValue(object.value(QStringLiteral("severity")).toString());
        if (!severity) { return std::nullopt; }
        SettingsHygiene::Issue issue{*severity,
            object.value(QStringLiteral("key")).toString(),
            object.value(QStringLiteral("summary")).toString(),
            object.value(QStringLiteral("detail")).toString(),
            object.value(QStringLiteral("fixActionId")).toString()};
        if (!boundedIssue(issue)) { return std::nullopt; }
        reply.issues.append(issue);
    }
    return reply;
}
} // namespace NereusSDR
