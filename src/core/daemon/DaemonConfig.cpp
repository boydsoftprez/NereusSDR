// =================================================================
// src/core/daemon/DaemonConfig.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DaemonConfig.h for the on-disk
// format and the design rationale. 2026-09-24: station_bind (R-R3-22 /
// R-R3-47), J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: iPhone app Task 12 (R-IOS-08): the listener defaults and
// pairing_lan_click, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
// Code.
// 2026-09-24: iPhone app Task 17 (R-IOS-08): status_page, status_port and
// state_directory, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
// Code.
// 2026-09-25: iPhone app plan Task 34 (R-IOS-02): remote_transmit, J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-26: iPhone app plan Task 27 (R-IOS-08, R-IOS-16):
// rendezvous_servers and relay, J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
// =================================================================

#include "DaemonConfig.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/session/RendezvousClient.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QRegularExpression>
#include <QTextStream>

namespace NereusSDR {

DaemonConfig DaemonConfig::defaults()
{
    return DaemonConfig{};
}

DaemonConfig DaemonConfig::fromFile(const QString& path, QString* errorOut)
{
    DaemonConfig cfg = defaults();
    cfg.sourcePath = path;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("could not open \"%1\": %2")
                            .arg(path, file.errorString());
        }
        return cfg;
    }

    QTextStream in(&file);
    int lineNo = 0;
    QString olderStationBind; // station_tci_bind, read when station_bind is empty
    bool sawRemotePort = false;
    bool sawRemoteBind = false;
    while (!in.atEnd()) {
        ++lineNo;
        QString line = in.readLine();

        // '#' starts a comment, whether it is the whole line or trails a
        // value; truncate before trimming so "key = value  # note" and
        // "# note" both work.
        const int hashIdx = line.indexOf(QLatin1Char('#'));
        if (hashIdx >= 0) {
            line.truncate(hashIdx);
        }
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const int eqIdx = line.indexOf(QLatin1Char('='));
        if (eqIdx < 0) {
            qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                              << "has no '=', ignored:" << line;
            continue;
        }

        const QString key = line.left(eqIdx).trimmed();
        const QString value = line.mid(eqIdx + 1).trimmed();

        // Unknown keys warn rather than fail (see DaemonConfig.h): a
        // config file written for a newer nereusd must still start an
        // older one instead of refusing to boot.
        if (key == QLatin1String("radio_mac")) {
            cfg.radioMac = value;
        } else if (key == QLatin1String("audio_device")) {
            cfg.audioDevice = value;
        } else if (key == QLatin1String("sample_rate_hz")) {
            bool ok = false;
            const int v = value.toInt(&ok);
            if (ok) {
                cfg.sampleRateHz = v;
                cfg.sampleRateExplicit = true;
            } else {
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "sample_rate_hz is not a number, keeping"
                                  << cfg.sampleRateHz << ":" << value;
            }
        } else if (key == QLatin1String("slice_count")) {
            bool ok = false;
            const int v = value.toInt(&ok);
            if (ok) {
                cfg.sliceCount = v;
            } else {
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "slice_count is not a number, keeping"
                                  << cfg.sliceCount << ":" << value;
            }
        } else if (key == QLatin1String("remote_port")) {
            sawRemotePort = true;
            bool ok = false;
            const int v = value.toInt(&ok);
            if (ok) {
                cfg.remotePort = v;
            } else {
                // iPhone app Task 12: a remote_port line that is not a
                // number leaves the listener off, as it did when off was
                // the default, rather than falling back to listening on
                // every interface because of a typo.
                cfg.remotePort = kExplicitConfigRemotePort;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "remote_port is not a number, the listener stays off:"
                                  << value;
            }
        } else if (key == QLatin1String("audio_bitrate")) {
            bool ok = false;
            const int v = value.toInt(&ok);
            if (ok && (v == kWidebandAudioBitrate || v == kFullbandAudioBitrate)) {
                cfg.audioBitrate = v;
            } else {
                cfg.audioBitrate = kDefaultAudioBitrate;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "audio_bitrate must be 24000 or 48000, keeping"
                                  << cfg.audioBitrate << ":" << value;
            }
        } else if (key == QLatin1String("thread_placement")) {
            if (value == QLatin1String("auto")) {
                cfg.threadPlacement = true;
            } else if (value == QLatin1String("off")) {
                cfg.threadPlacement = false;
            } else {
                cfg.threadPlacement = true;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "thread_placement must be auto or off, keeping auto:"
                                  << value;
            }
        } else if (key == QLatin1String("display_adaptive")) {
            if (value == QLatin1String("on")) {
                cfg.displayAdaptive = true;
            } else if (value == QLatin1String("off")) {
                cfg.displayAdaptive = false;
            } else {
                cfg.displayAdaptive = true;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "display_adaptive must be on or off, keeping on:"
                                  << value;
            }
        } else if (key == QLatin1String("audio_lossless")) {
            if (value.compare(QLatin1String("allow"), Qt::CaseInsensitive) == 0) {
                cfg.audioLosslessAllowed = true;
            } else if (value.compare(QLatin1String("deny"), Qt::CaseInsensitive) == 0) {
                cfg.audioLosslessAllowed = false;
            } else {
                cfg.audioLosslessAllowed = true;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "audio_lossless must be allow or deny, keeping allow :"
                                  << value;
            }
        } else if (key == QLatin1String("core_name")) {
            cfg.coreName = value;
        } else if (key == QLatin1String("remote_bind")) {
            sawRemoteBind = true;
            cfg.remoteBind = value;
        } else if (key == QLatin1String("pairing_lan_click")) {
            if (value.compare(QLatin1String("allow"), Qt::CaseInsensitive) == 0) {
                cfg.pairingLanClickAllowed = true;
            } else if (value.compare(QLatin1String("deny"), Qt::CaseInsensitive) == 0) {
                cfg.pairingLanClickAllowed = false;
            } else {
                cfg.pairingLanClickAllowed = true;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "pairing_lan_click must be allow or deny, keeping allow:"
                                  << value;
            }
        } else if (key == QLatin1String("rendezvous_servers")) {
            // iPhone app plan Task 27: an ordered list; empty names none.
            QStringList entries;
            for (const QString& entry :
                 value.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts)) {
                QStringList rejected;
                if (RendezvousClient::serverUrls({entry}, &rejected).isEmpty()) {
                    qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                      << "rendezvous_servers entry is not a server address, "
                                         "skipped:"
                                      << entry;
                    continue;
                }
                entries.append(entry);
            }
            cfg.rendezvousServers = entries;
        } else if (key == QLatin1String("relay")) {
            if (value.compare(QLatin1String("allow"), Qt::CaseInsensitive) == 0) {
                cfg.relayAllowed = true;
            } else if (value.compare(QLatin1String("deny"), Qt::CaseInsensitive) == 0) {
                cfg.relayAllowed = false;
            } else {
                cfg.relayAllowed = true;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "relay must be allow or deny, keeping allow:" << value;
            }
        } else if (key == QLatin1String("remote_transmit")) {
            // iPhone app plan Task 34: allow or deny; anything else denies.
            if (value.compare(QLatin1String("allow"), Qt::CaseInsensitive) == 0) {
                cfg.remoteTransmitAllowed = true;
            } else if (value.compare(QLatin1String("deny"), Qt::CaseInsensitive) == 0) {
                cfg.remoteTransmitAllowed = false;
            } else {
                cfg.remoteTransmitAllowed = false;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "remote_transmit must be allow or deny, denying:"
                                  << value;
            }
        } else if (key == QLatin1String("status_page")) {
            if (value.compare(QLatin1String("on"), Qt::CaseInsensitive) == 0) {
                cfg.statusPage = true;
            } else if (value.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0) {
                cfg.statusPage = false;
            } else {
                cfg.statusPage = true;
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "status_page must be on or off, keeping on:" << value;
            }
        } else if (key == QLatin1String("status_port")) {
            bool ok = false;
            const int v = value.toInt(&ok);
            if (ok) {
                cfg.statusPort = v;
            } else {
                qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                                  << "status_port is not a number, keeping"
                                  << cfg.statusPort << ":" << value;
            }
        } else if (key == QLatin1String("state_directory")) {
            cfg.stateDirectory = value;
        } else if (key == QLatin1String("station_bind")) {
            cfg.stationBind = value;
        } else if (key == QLatin1String("station_tci_bind")) {
            // The older name (TCI only), now every station listener's.
            olderStationBind = value;
        } else if (key == QLatin1String("display_application_bytes_per_second")
                   || key == QLatin1String("spectrum_sample_units_per_second")) {
            bool ok = false;
            const quint64 parsed = value.toULongLong(&ok, 10);
            const quint64 limit = ok && !value.startsWith(QLatin1Char('-')) ? parsed : 0;
            if (key == QLatin1String("display_application_bytes_per_second")) {
                cfg.displayApplicationBytesPerSecond = limit;
            } else {
                cfg.spectrumSampleUnitsPerSecond = limit;
            }
        } else {
            qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                              << "unknown key, ignored:" << key;
        }
    }

    if (cfg.stationBind.isEmpty() && !olderStationBind.isEmpty()) {
        cfg.stationBind = olderStationBind;
    }

    // iPhone app Task 12: a file that sets either listener key keeps the
    // meaning it had before the listener was on by default; the key it
    // leaves out takes the earlier default (DaemonConfig.h).
    if (sawRemotePort || sawRemoteBind) {
        if (!sawRemotePort) {
            cfg.remotePort = kExplicitConfigRemotePort;
        }
        if (!sawRemoteBind) {
            cfg.remoteBind = QString::fromLatin1(kExplicitConfigRemoteBind);
        }
    }

    if (errorOut) {
        errorOut->clear();
    }
    return cfg;
}

bool DaemonConfig::validate(QString* errorOut) const
{
    static const QRegularExpression radioMacPattern(QStringLiteral("\\A(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\\z"));
    if (!radioMac.isEmpty() && !radioMacPattern.match(radioMac).hasMatch()) {
        if (errorOut) { *errorOut = QStringLiteral("radio_mac must be empty or six colon-separated hexadecimal pairs"); }
        return false;
    }
    const QByteArray nameBytes = coreName.toUtf8();
    bool invalidName = nameBytes.size() > 128 || QString::fromUtf8(nameBytes) != coreName;
    for (QChar character : coreName) {
        invalidName = invalidName || character.category() == QChar::Other_Control;
    }
    if (invalidName) {
        if (errorOut) { *errorOut = QStringLiteral("core_name must be valid UTF-8 without control characters, at most 128 bytes"); }
        return false;
    }
    if ((displayApplicationBytesPerSecond || spectrumSampleUnitsPerSecond)
        && !displayBudgetLimits()) {
        if (errorOut) {
            *errorOut = QStringLiteral("display_application_bytes_per_second and "
                "spectrum_sample_units_per_second must both be positive integers "
                "no greater than 9007199254740991");
        }
        return false;
    }
    if (!remoteBind.isEmpty() && QHostAddress(remoteBind).isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("remote_bind must be empty or an IP address, got %1")
                            .arg(remoteBind);
        }
        return false;
    }
    if (!stationBind.isEmpty() && QHostAddress(stationBind).isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("station_bind must be empty or an IP address, got %1")
                            .arg(stationBind);
        }
        return false;
    }
    if (sliceCount < 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("slice_count must be at least 1, got %1")
                            .arg(sliceCount);
        }
        return false;
    }
    if (sampleRateHz <= 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("sample_rate_hz must be positive, got %1")
                            .arg(sampleRateHz);
        }
        return false;
    }
    if (audioBitrate != kWidebandAudioBitrate && audioBitrate != kFullbandAudioBitrate) {
        if (errorOut) {
            *errorOut = QStringLiteral("audio_bitrate must be 24000 or 48000, got %1")
                            .arg(audioBitrate);
        }
        return false;
    }
    // 0 is the documented "do not listen" value, so only a genuinely
    // impossible port is rejected. Refusing at parse time rather than
    // letting bind() fail later means the operator is told which line of
    // their config is wrong.
    if (remotePort < 0 || remotePort > 65535) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                            "remote_port must be 0 (disabled) or 1-65535, got %1")
                            .arg(remotePort);
        }
        return false;
    }
    if (statusPort < 1 || statusPort > 65535) {
        if (errorOut) {
            *errorOut = QStringLiteral("status_port must be 1-65535, got %1").arg(statusPort);
        }
        return false;
    }
    if (!stateDirectory.isEmpty() && !QDir::isAbsolutePath(stateDirectory)) {
        if (errorOut) {
            *errorOut = QStringLiteral("state_directory must be empty or an absolute path, got %1")
                            .arg(stateDirectory);
        }
        return false;
    }

    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

std::optional<DisplayBudgetLimits> DaemonConfig::displayBudgetLimits() const
{
    if (!displayApplicationBytesPerSecond || !spectrumSampleUnitsPerSecond) {
        return std::nullopt;
    }
    const DisplayBudgetLimits limits{*displayApplicationBytesPerSecond,
                                    *spectrumSampleUnitsPerSecond, 1};
    return limits.isValid() ? std::optional{limits} : std::nullopt;
}

QHostAddress DaemonConfig::listenAddressFor(const QString& bind)
{
    const QHostAddress parsed(bind);
    if (parsed == QHostAddress::AnyIPv6) {
        return QHostAddress(QHostAddress::Any);
    }
    return parsed;
}

QString resolveDaemonProfileArgument(const QString& requested, bool wasSet, QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    if (!wasSet) {
        // No --profile on the command line at all: reserve nereusd's own
        // profile instead of silently sharing the GUI client's directory.
        // See this function's declaration in DaemonConfig.h for the full
        // rationale (Remote Daemon R2, Task 1).
        return QString(AppSettings::kDaemonProfileName);
    }
    if (requested.isEmpty()) {
        // Explicit --profile "": the deliberate escape hatch back to the
        // shared default directory.
        return {};
    }
    if (!AppSettings::isValidProfileName(requested)) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "invalid --profile \"%1\" (allowed: [A-Za-z0-9_-]+)").arg(requested);
        }
        return {};
    }
    return requested;
}

} // namespace NereusSDR
