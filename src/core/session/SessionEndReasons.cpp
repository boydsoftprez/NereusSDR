// =================================================================
// src/core/session/SessionEndReasons.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// See SessionEndReasons.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion carry, review finding
//                                    I1 (R-R3-21, R-R3-38, R-IOS-01): the
//                                    takeover and version reasons, formatted
//                                    and parsed in one place, worded with
//                                    "Core". AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Final review M1 (R-R3-38, R-IOS-01):
//                                    parse also reads an older Core's
//                                    takeover and version wordings.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 18 (R-IOS-08,
//                                    R-IOS-17): read() chooses by the end
//                                    code. AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/session/SessionEndReasons.h"

#include "core/session/SessionMessages.h"

#include <QHostAddress>
#include <QRegularExpression>

#include <algorithm>

namespace NereusSDR::SessionEndReasons {

namespace {

quint16 newest(const QList<quint16>& majors)
{
    return majors.isEmpty() ? quint16(0) : *std::max_element(majors.cbegin(), majors.cend());
}

// The address part of "address:port"; empty for anything that is not one.
QString addressOf(const QString& otherApp)
{
    static const QRegularExpression withPort(QStringLiteral("^(.+):([0-9]+)$"));
    const QRegularExpressionMatch parts = withPort.match(otherApp.trimmed());
    if (!parts.hasMatch()) {
        return {};
    }
    QHostAddress address(parts.captured(1));
    bool mapped = false;
    const quint32 v4 = address.toIPv4Address(&mapped);
    if (mapped) {
        address = QHostAddress(v4);
    }
    return address.isNull() ? QString() : address.toString();
}

} // namespace

QString takenOver(const QString& otherApp)
{
    return QStringLiteral("Another app at %1 connected to the Core and took over. "
                          "Connect again to take it back.")
        .arg(otherApp);
}

QString versionRefused(const QList<quint16>& coreMajors, const QList<quint16>& appMajors)
{
    const quint16 coreNewest = newest(coreMajors);
    const quint16 appNewest = newest(appMajors);
    // The side with the older newest version is the one to update.
    const QString update = coreNewest < appNewest ? QStringLiteral("Update the Core.")
                                                  : QStringLiteral("Update this app.");
    return QStringLiteral("This Core runs link version %1 and this app runs version %2. %3")
        .arg(coreNewest)
        .arg(appNewest)
        .arg(update);
}

Parsed parse(const QString& reason)
{
    // The fallback for an older Core, which sends no end code (read()).
    Parsed parsed;

    static const QRegularExpression takenOverPattern(
        QStringLiteral("^Another app at (.*) connected to the Core and took over\\. "
                       "Connect again to take it back\\.$"));
    if (const QRegularExpressionMatch match = takenOverPattern.match(reason);
        match.hasMatch()) {
        parsed.kind = Parsed::Kind::TakenOver;
        parsed.otherAppAddress = addressOf(match.captured(1));
        return parsed;
    }

    // Older-Core wording (StationServer::promoteToSession before the
    // wording moved here). The bench Cores still send it until they are
    // upgraded.
    static const QRegularExpression olderTakenOverPattern(
        QStringLiteral("^Displaced by a newer authenticated connection from (\\S+)$"));
    if (const QRegularExpressionMatch match = olderTakenOverPattern.match(reason);
        match.hasMatch()) {
        parsed.kind = Parsed::Kind::TakenOver;
        parsed.otherAppAddress = addressOf(match.captured(1));
        return parsed;
    }

    static const QRegularExpression versionPattern(
        QStringLiteral("^This Core runs link version ([0-9]+) and this app runs version "
                       "([0-9]+)\\. Update (the Core|this app)\\.$"));
    if (const QRegularExpressionMatch match = versionPattern.match(reason); match.hasMatch()) {
        parsed.kind = Parsed::Kind::VersionRefused;
        parsed.coreMajor = match.captured(1).toInt();
        parsed.appMajor = match.captured(2).toInt();
        return parsed;
    }

    // Older-Core wording (StationServer::handleHello before the wording
    // moved here): "station speaks major.minor, client speaks major.minor".
    static const QRegularExpression olderVersionPattern(QStringLiteral(
        "^Protocol major version mismatch: station speaks ([0-9]+)\\.[0-9]+, client speaks "
        "([0-9]+)\\.[0-9]+\\. A differing major means an incompatible wire contract\\.$"));
    if (const QRegularExpressionMatch match = olderVersionPattern.match(reason);
        match.hasMatch()) {
        parsed.kind = Parsed::Kind::VersionRefused;
        parsed.coreMajor = match.captured(1).toInt();
        parsed.appMajor = match.captured(2).toInt();
    }
    return parsed;
}

Parsed read(const QString& code, const QString& reason)
{
    if (code.isEmpty()) {
        return parse(reason);
    }
    const Parsed words = parse(reason);
    Parsed parsed;
    if (code == QLatin1String(SessionEndCode::kTakenOver)) {
        parsed = words.kind == Parsed::Kind::TakenOver ? words : Parsed{};
        parsed.kind = Parsed::Kind::TakenOver;
    } else if (code == QLatin1String(SessionEndCode::kLinkVersion)) {
        parsed = words.kind == Parsed::Kind::VersionRefused ? words : Parsed{};
        parsed.kind = Parsed::Kind::VersionRefused;
    } else if (code == QLatin1String(SessionEndCode::kDeviceRemoved)
               || code == QLatin1String(SessionEndCode::kDeviceNotPaired)) {
        parsed.kind = Parsed::Kind::DeviceRemoved;
    } else if (code == QLatin1String(SessionEndCode::kPairingRequired)) {
        parsed.kind = Parsed::Kind::PairingRequired;
    } else if (code == QLatin1String(SessionEndCode::kIdentityChanged)) {
        parsed.kind = Parsed::Kind::IdentityChanged;
    }
    return parsed;
}

} // namespace NereusSDR::SessionEndReasons
