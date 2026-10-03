// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/ClientDeviceIdentity.cpp  (NereusSDR)
// =================================================================
//
// See ClientDeviceIdentity.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8b: a window run with a profile
//               other than the default carries the profile in its name and
//               short name. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/security/ClientDeviceIdentity.h"

#include "core/AppSettings.h"

#include <QMutex>
#include <QMutexLocker>
#include <QSysInfo>

namespace NereusSDR {

ClientDeviceIdentity ClientDeviceIdentity::loadOrCreate(const QString& profileDir)
{
    ClientDeviceIdentity identity;
    identity.m_key = StationIdentity::loadOrCreateKeyFile(
        profileDir, QString::fromLatin1(kKeyFileName), QStringLiteral("This computer's"));
    return identity;
}

std::shared_ptr<const ClientDeviceIdentity> ClientDeviceIdentity::forThisProfile()
{
    // One key per process and profile. The GUI thread asks; the mutex only
    // keeps a second window's first ask from creating the file twice.
    static QMutex mutex;
    static std::shared_ptr<const ClientDeviceIdentity> identity;
    QMutexLocker lock(&mutex);
    if (!identity) {
        identity = std::make_shared<const ClientDeviceIdentity>(loadOrCreate(
            AppSettings::resolveConfigDir(AppSettings::profileOverride())));
    }
    return identity;
}

QString ClientDeviceIdentity::machineName()
{
    return deviceNameFrom(QSysInfo::machineHostName());
}

QString ClientDeviceIdentity::deviceNameFrom(const QString& hostName)
{
    return cleanedName(hostName, kMaxNameBytes);
}

QString ClientDeviceIdentity::machineShortName()
{
    return shortNameFrom(QSysInfo::machineHostName());
}

QString ClientDeviceIdentity::shortNameFrom(const QString& hostName)
{
    return cleanedName(hostName.trimmed().section(QLatin1Char('.'), 0, 0), kMaxShortNameBytes);
}

QString ClientDeviceIdentity::machineName(const QString& profile)
{
    return deviceNameFrom(QSysInfo::machineHostName(), profile);
}

QString ClientDeviceIdentity::machineShortName(const QString& profile)
{
    return shortNameFrom(QSysInfo::machineHostName(), profile);
}

QString ClientDeviceIdentity::deviceNameFrom(const QString& hostName, const QString& profile)
{
    return withProfile(hostName, profile, kMaxNameBytes, /*shortName=*/false);
}

QString ClientDeviceIdentity::shortNameFrom(const QString& hostName, const QString& profile)
{
    return withProfile(hostName, profile, kMaxShortNameBytes, /*shortName=*/true);
}

QString ClientDeviceIdentity::withProfile(const QString& hostName, const QString& profile,
                                          int maxBytes, bool shortName)
{
    const auto plain = [&hostName, shortName](int bytes) {
        return shortName ? cleanedName(hostName.trimmed().section(QLatin1Char('.'), 0, 0), bytes)
                         : cleanedName(hostName, bytes);
    };
    if (profile.trimmed().isEmpty()) {
        return plain(maxBytes);
    }
    // The profile keeps at most half the room, so the computer's own name
    // stays readable beside it.
    const QString label = cleanedName(profile, maxBytes / 2 - 3);
    const QString suffix = QStringLiteral(" (%1)").arg(label);
    return plain(maxBytes - static_cast<int>(suffix.toUtf8().size())) + suffix;
}

QString ClientDeviceIdentity::cleanedName(const QString& text, int maxBytes)
{
    QString name;
    name.reserve(text.size());
    for (const QChar c : text) {
        // What DeviceStore::isValidName refuses.
        const QChar::Category category = c.category();
        if (c.isNull() || category == QChar::Other_Control || category == QChar::Other_Format
            || category == QChar::Separator_Line || category == QChar::Separator_Paragraph) {
            continue;
        }
        name.append(c);
    }
    // A lone surrogate does not survive the UTF-8 the Core stores.
    name = QString::fromUtf8(name.toUtf8()).remove(QChar::ReplacementCharacter).trimmed();
    if (name.endsWith(QLatin1String(".local"), Qt::CaseInsensitive)) {
        name.chop(6);
        name = name.trimmed();
    }
    // Cut at a character boundary so the UTF-8 form fits.
    while (!name.isEmpty() && name.toUtf8().size() > maxBytes) {
        name.chop(1);
        if (!name.isEmpty() && name.back().isHighSurrogate()) {
            name.chop(1);
        }
    }
    name = name.trimmed();
    return name.isEmpty() ? QStringLiteral("Computer") : name;
}

} // namespace NereusSDR
