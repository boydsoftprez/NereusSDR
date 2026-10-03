#include "SettingsBackup.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

namespace NereusSDR {
namespace {

bool fail(QString* error, const QString& reason)
{
    if (error) {
        *error = reason;
    }
    return false;
}

bool validXmlPart(const QByteArray& xml, const QString& name, QString* error)
{
    if (xml.isEmpty()) {
        return fail(error, name + QStringLiteral(" XML is empty"));
    }
    if (xml.size() > SettingsBackup::kMaxXmlBytes) {
        return fail(error, name + QStringLiteral(" XML exceeds 16 MiB"));
    }
    const QString text = QString::fromUtf8(xml);
    if (text.toUtf8() != xml) {
        return fail(error, name + QStringLiteral(" XML is not valid UTF-8"));
    }
    return true;
}

} // namespace

bool SettingsBackup::encode(const SettingsBackup& backup, QByteArray* output,
                            QString* error)
{
    if (!output) {
        return fail(error, QStringLiteral("Missing backup output"));
    }
    if (!validXmlPart(backup.windowXml, QStringLiteral("Window"), error)
        || !validXmlPart(backup.coreXml, QStringLiteral("Core"), error)) {
        return false;
    }
    const QJsonObject object{
        {QStringLiteral("format"), QStringLiteral("nereus-settings-backup")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("windowXml"), QString::fromUtf8(backup.windowXml)},
        {QStringLiteral("coreXml"), QString::fromUtf8(backup.coreXml)}
    };
    const QByteArray encoded = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (encoded.size() > kMaxFileBytes) {
        return fail(error, QStringLiteral("Settings backup exceeds 64 MiB"));
    }
    *output = encoded;
    if (error) {
        error->clear();
    }
    return true;
}

bool SettingsBackup::decode(const QByteArray& input, SettingsBackup* output,
                            QString* error)
{
    if (!output) {
        return fail(error, QStringLiteral("Missing backup output"));
    }
    if (input.isEmpty() || input.size() > kMaxFileBytes) {
        return fail(error, QStringLiteral("Settings backup is empty or exceeds 64 MiB"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error, QStringLiteral("Settings backup JSON is invalid: %1")
                    .arg(parseError.errorString()));
    }
    const QJsonObject object = document.object();
    if (object.size() != 4) {
        return fail(error, QStringLiteral("Settings backup has unexpected fields"));
    }
    if (object.value(QStringLiteral("format")).toString()
        != QStringLiteral("nereus-settings-backup")) {
        return fail(error, QStringLiteral("Unknown settings backup format"));
    }
    const QJsonValue version = object.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != 1) {
        return fail(error, QStringLiteral("Unsupported settings backup version"));
    }
    const QJsonValue window = object.value(QStringLiteral("windowXml"));
    const QJsonValue core = object.value(QStringLiteral("coreXml"));
    if (!window.isString() || !core.isString()) {
        return fail(error, QStringLiteral("Settings backup requires both XML strings"));
    }
    SettingsBackup candidate{window.toString().toUtf8(), core.toString().toUtf8()};
    if (!validXmlPart(candidate.windowXml, QStringLiteral("Window"), error)
        || !validXmlPart(candidate.coreXml, QStringLiteral("Core"), error)) {
        return false;
    }
    *output = std::move(candidate);
    if (error) {
        error->clear();
    }
    return true;
}

bool SettingsBackup::readFile(const QString& path, SettingsBackup* output,
                              QString* error)
{
    if (!output) {
        return fail(error, QStringLiteral("Missing backup output"));
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("Could not open settings backup: %1")
                    .arg(file.errorString()));
    }
    if (file.size() > kMaxFileBytes) {
        return fail(error, QStringLiteral("Settings backup exceeds 64 MiB"));
    }
    return decode(file.read(kMaxFileBytes + 1), output, error);
}

bool SettingsBackup::writeFile(const QString& path, const SettingsBackup& backup,
                               QString* error)
{
    QByteArray encoded;
    if (!encode(backup, &encoded, error)) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, QStringLiteral("Could not open settings backup destination: %1")
                    .arg(file.errorString()));
    }
    if (file.write(encoded) != encoded.size() || !file.commit()) {
        return fail(error, QStringLiteral("Could not save settings backup: %1")
                    .arg(file.errorString()));
    }
    if (error) {
        error->clear();
    }
    return true;
}

} // namespace NereusSDR
