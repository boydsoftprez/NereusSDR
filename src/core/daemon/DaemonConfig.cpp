// =================================================================
// src/core/daemon/DaemonConfig.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DaemonConfig.h for the on-disk
// format and the design rationale.
// =================================================================

#include "DaemonConfig.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QFile>
#include <QTextStream>

namespace NereusSDR {

DaemonConfig DaemonConfig::defaults()
{
    return DaemonConfig{};
}

DaemonConfig DaemonConfig::fromFile(const QString& path, QString* errorOut)
{
    DaemonConfig cfg = defaults();

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
        } else if (key == QLatin1String("log_level")) {
            cfg.logLevel = value;
        } else if (key == QLatin1String("sample_rate_hz")) {
            bool ok = false;
            const int v = value.toInt(&ok);
            if (ok) {
                cfg.sampleRateHz = v;
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
        } else {
            qCWarning(lcApp) << "nereusd.conf" << path << "line" << lineNo
                              << "unknown key, ignored:" << key;
        }
    }

    if (errorOut) {
        errorOut->clear();
    }
    return cfg;
}

bool DaemonConfig::validate(QString* errorOut) const
{
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

    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

QString resolveDaemonProfileArgument(const QString& requested, QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    if (requested.isEmpty()) {
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
