// 2026-09-27: bounded collection and structured privacy policy refined with
// OpenAI Codex assistance; original support implementation retained.
#include "SupportBundle.h"
#include "LogCategories.h"
#include "AppSettings.h"
#include "RadioConnection.h"
#include "models/RadioModel.h"

#include <QCoreApplication>
#include <QSysInfo>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QPointer>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>
#include <QTemporaryDir>
#include <QUuid>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "ZipArchive.h"
#include "core/security/PairingCode.h"

#include <memory>
#include <optional>
#include <algorithm>

namespace NereusSDR {

SupportBundle::SystemInfo SupportBundle::collectSystemInfo()
{
    SystemInfo sys;
    sys.appVersion = QCoreApplication::applicationVersion();
    sys.qtVersion = QString::fromLatin1(qVersion());
    sys.osName = QSysInfo::prettyProductName();
    sys.kernelVersion = QSysInfo::kernelVersion();
    sys.cpuArch = QSysInfo::currentCpuArchitecture();
    sys.buildDate = QString::fromLatin1(__DATE__);
    return sys;
}

SupportBundle::RadioDiagInfo SupportBundle::collectRadioInfo(const RadioModel* model)
{
    RadioDiagInfo info;
    if (!model || !model->isConnected()) {
        info.connected = false;
        return info;
    }

    // The connection's own record, or the model's (the radio a Core or a
    // window last connected, when no connection object is held).
    const RadioConnection* conn = model->connection();
    const RadioInfo& ri = conn != nullptr ? conn->radioInfo() : model->currentRadioInfo();
    info.connected = true;
    info.model = ri.displayName();
    info.firmware = QString::number(ri.firmwareVersion);
    info.protocol = static_cast<int>(ri.protocol);

    // Redact MAC — keep only last segment
    if (ri.macAddress.length() > 3) {
        info.macAddress = QStringLiteral("**:**:**:**:**:") + ri.macAddress.right(2);
    }

    // Redact IP — keep only last octet
    QString ip = ri.address.toString();
    int lastDot = ip.lastIndexOf(QLatin1Char('.'));
    if (lastDot > 0) {
        info.ipAddress = QStringLiteral("*.*.*. ") + ip.mid(lastDot + 1);
    }

    return info;
}

QString SupportBundle::bundleDirPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/NereusSDR/support");
}

SupportBundle::Inputs SupportBundle::gatherInputs(const RadioModel* model)
{
    Inputs inputs;
    inputs.system = collectSystemInfo();
    inputs.radio = collectRadioInfo(model);
    inputs.categories = LogManager::instance().categories();
    inputs.logDir = LogManager::instance().logDirPath();
    inputs.settingsPath = AppSettings::instance().filePath();
    inputs.stamp = QDateTime::currentDateTime();
    return inputs;
}

QString SupportBundle::createBundle(const RadioModel* model)
{
    return writeBundle(gatherInputs(model), CoreAttachment{});
}

namespace {

QByteArray readPrefix(const QString& path, qint64 limit, bool* truncated = nullptr)
{
    if (truncated) { *truncated = false; }
    QFile f(path);
    if (path.isEmpty() || !f.open(QIODevice::ReadOnly)) {
        return {};
    }
    if (truncated) { *truncated = f.size() > limit; }
    return f.read(limit);
}

QByteArray readTail(const QString& path, qint64 limit)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    const qint64 size = f.size();
    const qint64 start = std::max<qint64>(0, size - limit);
    if (!f.seek(start)) { return {}; }
    QByteArray tail = f.read(limit);
    if (start > 0) {
        const qsizetype newline = tail.indexOf('\n');
        if (newline >= 0) { tail.remove(0, newline + 1); }
        tail.prepend("[the older part of this log is left out]\n");
    }
    return tail;
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return f.write(data) == data.size();
}

// The newest `bytes` of a file, starting at a whole line.
QByteArray tailOf(const QByteArray& data, qint64 bytes)
{
    if (data.size() <= bytes) {
        return data;
    }
    QByteArray tail = data.right(bytes);
    const qsizetype newline = tail.indexOf('\n');
    if (newline >= 0) {
        tail.remove(0, newline + 1);
    }
    return QByteArrayLiteral("[the older part of this log is left out]\n") + tail;
}

// A ZIP entry name that stays inside the folder it is written to.
QString safeEntryName(const QString& name)
{
    const QStringList parts = QDir::fromNativeSeparators(name).split(QLatin1Char('/'),
                                                                     Qt::SkipEmptyParts);
    QStringList kept;
    for (const QString& part : parts) {
        if (part != QLatin1String(".") && part != QLatin1String("..")
            && !part.contains(QLatin1Char(':'))) {
            kept.append(part);
        }
    }
    return kept.join(QLatin1Char('/'));
}

bool isSensitiveName(const QString& name)
{
    const QString lower = name.toLower();
    for (const char* word : {"token", "password", "passwd", "secret", "auth",
                              "credential", "pairing", "private", "apikey", "devicekey"}) {
        if (lower.contains(QLatin1String(word))) { return true; }
    }
    return false;
}

bool isSafeSetting(const QString& key, const QString& value)
{
    if (isSensitiveName(key)) { return false; }
    // Explicit diagnostic settings whose values have a closed grammar.
    static const QSet<QString> numericKeys{
        QStringLiteral("SampleRate"), QStringLiteral("audio/DspRate"),
        QStringLiteral("audio/DspBlockSize"), QStringLiteral("DisplayFftSize"),
        QStringLiteral("DisplaySpectrumFps")};
    if (numericKeys.contains(key)) {
        bool ok = false;
        value.toDouble(&ok);
        return ok;
    }
    return false;
}

QByteArray sanitizedSettings(const QByteArray& xmlBytes)
{
    QXmlStreamReader reader(xmlBytes);
    QByteArray out;
    QXmlStreamWriter writer(&out);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("NereusSDR"));
    int depth = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            ++depth;
            if (depth == 1) {
                if (reader.name() != QLatin1String("NereusSDR")) { return {}; }
            } else if (depth == 2 && reader.attributes().value(QStringLiteral("type"))
                                      == QLatin1String("station")) {
                writer.writeStartElement(QStringLiteral("Station"));
                writer.writeAttribute(QStringLiteral("type"), QStringLiteral("station"));
            } else if (depth == 2 || depth == 3) {
                const QString key = reader.name().toString();
                const QString value = reader.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement);
                const bool safe = isSafeSetting(key, value);
                writer.writeTextElement(safe ? key : QStringLiteral("RedactedSetting"),
                                        safe ? value : QStringLiteral("[REDACTED]"));
                --depth;
            } else { return {}; }
        } else if (reader.isEndElement()) {
            if (depth == 2) { writer.writeEndElement(); }
            --depth;
        }
    }
    if (reader.hasError() || depth != 0) { return {}; }
    writer.writeEndElement();
    writer.writeEndDocument();
    return out;
}

QByteArray sanitizedDaemonConfig(const QByteArray& raw)
{
    static const QSet<QString> numeric{
        QStringLiteral("sample_rate_hz"), QStringLiteral("slice_count"),
        QStringLiteral("remote_port"), QStringLiteral("audio_bitrate"),
        QStringLiteral("status_port"),
        QStringLiteral("display_application_bytes_per_second")};
    static const QHash<QString, QStringList> enums{
        {QStringLiteral("thread_placement"), {QStringLiteral("auto"), QStringLiteral("off")}},
        {QStringLiteral("display_adaptive"), {QStringLiteral("on"), QStringLiteral("off")}},
        {QStringLiteral("audio_lossless"), {QStringLiteral("allow"), QStringLiteral("deny")}},
        {QStringLiteral("pairing_lan_click"), {QStringLiteral("allow"), QStringLiteral("deny")}},
        {QStringLiteral("relay"), {QStringLiteral("allow"), QStringLiteral("deny")}},
        {QStringLiteral("remote_transmit"), {QStringLiteral("allow"), QStringLiteral("deny")}},
        {QStringLiteral("status_page"), {QStringLiteral("on"), QStringLiteral("off")}}};
    QByteArray out;
    for (const QString& rawLine : QString::fromUtf8(raw).split(QLatin1Char('\n'))) {
        const QString line = rawLine.section(QLatin1Char('#'), 0, 0).trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) { continue; }
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (!QRegularExpression(QStringLiteral("^[a-z][a-z0-9_]*$")).match(key).hasMatch()) {
            continue;
        }
        bool ok = false;
        value.toLongLong(&ok);
        const bool safe = !isSensitiveName(key)
            && ((numeric.contains(key) && ok)
                || (enums.contains(key) && enums.value(key).contains(value)));
        out += (safe ? key.toUtf8() : QByteArray("unreviewed_setting"))
            + " = " + (safe ? value.toUtf8() : QByteArray("[REDACTED]")) + '\n';
    }
    return out;
}

QJsonValue sanitizedJsonValue(const QJsonValue& value, const QStringList& knownSecrets)
{
    if (value.isObject()) {
        QJsonObject out;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            bool sensitiveKey = isSensitiveName(it.key());
            for (const QString& secret : knownSecrets) {
                if (!secret.isEmpty() && it.key().contains(secret)) { sensitiveKey = true; }
            }
            out.insert(sensitiveKey ? QStringLiteral("redactedField") : it.key(),
                       sensitiveKey ? QJsonValue(QStringLiteral("[REDACTED]"))
                                    : sanitizedJsonValue(it.value(), knownSecrets));
        }
        return out;
    }
    if (value.isArray()) {
        QJsonArray out;
        for (const QJsonValue& item : value.toArray()) {
            out.append(sanitizedJsonValue(item, knownSecrets));
        }
        return out;
    }
    return value.isString() ? QJsonValue(QStringLiteral("[REDACTED]")) : value;
}

// Runs `work` on a thread of its own, then `done` on `context`'s thread.
template <typename Result>
void runOnWorker(QObject* context, std::function<Result()> work,
                 std::function<void(Result)> done)
{
    auto result = std::make_shared<Result>();
    QThread* worker = QThread::create([work = std::move(work), result]() { *result = work(); });
    worker->setObjectName(QStringLiteral("SupportBundle"));
    const QPointer<QObject> guard(context);
    QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(worker, &QThread::finished, context,
                     [result, guard, done = std::move(done)]() {
        if (guard) {
            done(std::move(*result));
        }
    });
    // Normal priority on purpose: on a loaded computer a low-priority thread
    // (macOS runs it at a throttled quality of service) can wait many
    // seconds for its turn, and the window waits for the answer. The work is
    // short and never touches the radio's threads.
    worker->start();
}

} // namespace

QByteArray SupportBundle::systemInfoJson(const SystemInfo& sys)
{
    QJsonObject obj;
    obj[QStringLiteral("appVersion")] = sys.appVersion;
    obj[QStringLiteral("qtVersion")] = sys.qtVersion;
    obj[QStringLiteral("os")] = sys.osName;
    obj[QStringLiteral("kernel")] = sys.kernelVersion;
    obj[QStringLiteral("cpu")] = sys.cpuArch;
    obj[QStringLiteral("buildDate")] = sys.buildDate;
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

QByteArray SupportBundle::radioInfoJson(const RadioDiagInfo& radio)
{
    QJsonObject obj;
    obj[QStringLiteral("connected")] = radio.connected;
    obj[QStringLiteral("model")] = radio.model;
    obj[QStringLiteral("mac")] = radio.macAddress;
    obj[QStringLiteral("firmware")] = radio.firmware;
    obj[QStringLiteral("ip")] = radio.ipAddress;
    obj[QStringLiteral("protocol")] = radio.protocol;
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

QByteArray SupportBundle::enabledCategoriesText(const QList<LogCategoryInfo>& categories)
{
    QByteArray out;
    for (const auto& cat : categories) {
        out += cat.id.toUtf8() + ": " + (cat.enabled ? "ENABLED" : "disabled") + '\n';
    }
    return out;
}

QStringList SupportBundle::recentLogFiles(const QString& logDir)
{
    QDir d(logDir);
    const QStringList logs = d.entryList({QStringLiteral("nereussdr-*.log")},
                                         QDir::Files, QDir::Name);
    QStringList out;
    for (qsizetype i = logs.size() - 1; i >= 0 && out.size() < kMaxLogFiles; --i) {
        const QString path = d.absoluteFilePath(logs[i]);
        if (QFileInfo(path).size() < 50) {
            continue;  // Skip near-empty files
        }
        out.append(path);
    }
    return out;
}

QByteArray SupportBundle::buildCoreBundle(const Inputs& inputs)
{
    ZipWriter zip(inputs.stamp.isValid() ? inputs.stamp : QDateTime::currentDateTime());
    RadioDiagInfo safeRadio = inputs.radio;
    safeRadio.model = sanitizeText(safeRadio.model, inputs.knownSecrets);
    zip.add(QStringLiteral("system-info.json"), systemInfoJson(inputs.system));
    zip.add(QStringLiteral("radio-info.json"), radioInfoJson(safeRadio));
    zip.add(QStringLiteral("enabled-categories.txt"), enabledCategoriesText(inputs.categories));
    QJsonObject telemetry = inputs.telemetry;
    if (telemetry.isEmpty()) {
        telemetry.insert(QStringLiteral("note"),
                         QStringLiteral("The Core had measured no telemetry."));
    }
    zip.add(QStringLiteral("telemetry.json"),
            QJsonDocument(sanitizedJsonValue(telemetry, inputs.knownSecrets).toObject())
                .toJson(QJsonDocument::Indented));
    bool settingsTruncated = false;
    const QByteArray settings = readPrefix(inputs.settingsPath, kMaxConfigInputBytes,
                                           &settingsTruncated);
    QByteArray cleanSettings;
    if (!settings.isEmpty() && !settingsTruncated) {
        cleanSettings = sanitizedSettings(settings);
    }
    if (!cleanSettings.isEmpty()) {
        ZipWriter trial = zip;
        trial.add(QStringLiteral("settings.xml"), cleanSettings);
        if (trial.finishedSize() <= kMaxCoreBundleBytes) {
            zip = trial;
        }
    }
    bool configTruncated = false;
    const QByteArray config = readPrefix(inputs.daemonConfigPath, kMaxConfigInputBytes,
                                         &configTruncated);
    if (!config.isEmpty()) {
        zip.add(QStringLiteral("nereusd.conf"), configTruncated
            ? QByteArray("[configuration omitted: input exceeded 1 MiB]\n")
            : sanitizedDaemonConfig(config));
    }
    QJsonObject limits{{QStringLiteral("settingsOmittedBecauseOversized"), settingsTruncated},
                       {QStringLiteral("settingsOmittedBecauseInvalid"),
                        !settings.isEmpty() && !settingsTruncated && cleanSettings.isEmpty()},
                       {QStringLiteral("daemonConfigOmittedBecauseOversized"), configTruncated},
                       {QStringLiteral("logTailLimitBytes"), static_cast<double>(kMaxLogTailBytes)}};
    zip.add(QStringLiteral("collection-limits.json"), QJsonDocument(limits).toJson());

    // The logs, newest first, each as much of its end as still fits. Each
    // log's newest kMaxLogTailBytes are cleaned of secrets once; a shorter
    // end of the clean text is tried while it does not fit.
    constexpr qint64 kMinLogTailBytes = 4096;
    int count = 0;
    for (const QString& path : recentLogFiles(inputs.logDir)) {
        const QByteArray clean = sanitizeText(QString::fromUtf8(
            readTail(path, kMaxLogTailBytes)), inputs.knownSecrets).toUtf8();
        const QString name = count == 0 ? QStringLiteral("nereussdr.log")
                                        : QStringLiteral("nereussdr-%1.log").arg(count);
        for (qint64 tail = clean.size();; tail /= 2) {
            ZipWriter trial = zip;
            trial.add(name, tailOf(clean, tail));
            if (trial.finishedSize() <= kMaxCoreBundleBytes) {
                zip = trial;
                ++count;
                break;
            }
            if (tail < kMinLogTailBytes) {
                break;   // not even its newest lines fit
            }
        }
        if (zip.finishedSize() >= kMaxCoreBundleBytes - kMinLogTailBytes) {
            break;
        }
    }
    QByteArray archive = zip.finish();
    if (archive.size() > kMaxCoreBundleBytes) {
        return {};
    }
    return archive;
}

void SupportBundle::buildCoreBundleAsync(QObject* context, Inputs inputs,
                                         std::function<void(QByteArray)> done)
{
    runOnWorker<QByteArray>(context, [inputs = std::move(inputs)]() {
        return buildCoreBundle(inputs);
    }, std::move(done));
}

QString SupportBundle::writeBundle(const Inputs& inputs, const CoreAttachment& core)
{
    // Create timestamped temp directory
    const QDateTime stamp = inputs.stamp.isValid() ? inputs.stamp : QDateTime::currentDateTime();
    const QString timestamp = stamp.toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QTemporaryDir staging(QDir::tempPath() + QStringLiteral("/nereussdr-support-XXXXXX"));
    if (!staging.isValid()) { return {}; }
    const QString tempDir = staging.path();

    writeFile(tempDir + QStringLiteral("/system-info.json"), systemInfoJson(inputs.system));
    RadioDiagInfo safeRadio = inputs.radio;
    safeRadio.model = sanitizeText(safeRadio.model, inputs.knownSecrets);
    QByteArray radioInfo = radioInfoJson(safeRadio);

    // This computer's logs, secrets removed.
    int count = 0;
    for (const QString& path : recentLogFiles(inputs.logDir)) {
        const QString destName = count == 0 ? QStringLiteral("nereussdr.log")
                                            : QStringLiteral("nereussdr-%1.log").arg(count);
        writeFile(tempDir + QLatin1Char('/') + destName,
                  sanitizeText(QString::fromUtf8(readTail(path, kMaxLogTailBytes)),
                               inputs.knownSecrets).toUtf8());
        ++count;
    }
    bool settingsTruncated = false;
    const QByteArray settings = readPrefix(inputs.settingsPath, kMaxConfigInputBytes,
                                           &settingsTruncated);
    if (!settings.isEmpty()) {
        const QByteArray clean = settingsTruncated ? QByteArray() : sanitizedSettings(settings);
        writeFile(tempDir + QStringLiteral("/settings.xml"),
                  settingsTruncated ? QByteArray("[settings omitted: input exceeded 1 MiB]\n")
                                    : (clean.isEmpty()
                                        ? QByteArray("[settings omitted: invalid XML]\n") : clean));
    }
    writeFile(tempDir + QStringLiteral("/enabled-categories.txt"),
              enabledCategoriesText(inputs.categories));

    // A remote window: the Core's bundle beside this computer's, and the
    // radio this window runs is the Core's.
    if (core.wanted) {
        const QString coreDir = tempDir + QStringLiteral("/core");
        QDir().mkpath(coreDir);
        const std::optional<QList<ZipEntry>> entries = core.bundle.isEmpty()
            ? std::nullopt : ZipArchive::read(core.bundle);
        if (entries) {
            for (const ZipEntry& entry : *entries) {
                const QString name = safeEntryName(entry.name);
                if (name.isEmpty()) {
                    continue;
                }
                const QString target = coreDir + QLatin1Char('/') + name;
                QDir().mkpath(QFileInfo(target).absolutePath());
                writeFile(target, entry.data);
                if (name == QLatin1String("radio-info.json")) {
                    radioInfo = entry.data;
                }
            }
        } else {
            const QString why = core.reason.isEmpty()
                ? QStringLiteral("The Core's bundle could not be read.") : core.reason;
            writeFile(coreDir + QStringLiteral("/unavailable.txt"), why.toUtf8() + '\n');
        }
    }
    writeFile(tempDir + QStringLiteral("/radio-info.json"), radioInfo);

    // Create archive
    QDir().mkpath(bundleDirPath());
    const QString archiveName = QStringLiteral("support-bundle-") + timestamp
        + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString archivePath = createArchive(tempDir, archiveName);
    return archivePath;
}

void SupportBundle::writeBundleAsync(QObject* context, Inputs inputs, CoreAttachment core,
                                     std::function<void(QString)> done)
{
    runOnWorker<QString>(context, [inputs = std::move(inputs), core = std::move(core)]() {
        return writeBundle(inputs, core);
    }, std::move(done));
}

void SupportBundle::openBundleFolder()
{
    QString dir = bundleDirPath();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

QString SupportBundle::sanitizeLine(const QString& line)
{
    // Redact lines that may contain sensitive data
    static const char* const kWords[] = {
        "token", "password", "passwd", "passphrase", "secret", "auth", "credential",
        "privatekey", "private_key", "private key", "pairingcode", "pairing_code",
        "pairing code", "devicekey", "device_key", "device key", "apikey", "api_key",
        "api key",
    };
    const QString lower = line.toLower();
    for (const char* word : kWords) {
        if (lower.contains(QLatin1String(word))) {
            return line.trimmed().startsWith(QLatin1Char('<'))
                ? QStringLiteral("<!-- [REDACTED] -->") : QStringLiteral("[REDACTED]");
        }
    }
    return line;
}

QString SupportBundle::sanitizeText(const QString& text, const QStringList& knownSecrets)
{
    QString scrubbed = text;
    // Replace credentials the Core already holds in memory. No credential
    // file is opened for this purpose. A short value is still a secret.
    for (const QString& secret : knownSecrets) {
        if (!secret.isEmpty()) {
            scrubbed.replace(secret, QStringLiteral("[REDACTED]"), Qt::CaseSensitive);
        }
    }
    // A private key may span many lines and its base64 body has no key word.
    const QRegularExpression pem(QStringLiteral(
        "-----BEGIN [A-Z ]*PRIVATE KEY-----[\\s\\S]*?-----END [A-Z ]*PRIVATE KEY-----"));
    scrubbed.replace(pem, QStringLiteral("[REDACTED PRIVATE KEY]"));
    // A bounded tail can begin inside a PEM block. If its first marker is
    // END, everything before that marker is withheld. A block with BEGIN
    // but no END in the captured tail is withheld to the end.
    const QRegularExpression beginPem(QStringLiteral("-----BEGIN [A-Z ]*PRIVATE KEY-----"));
    const QRegularExpression endPem(QStringLiteral("-----END [A-Z ]*PRIVATE KEY-----"));
    const QRegularExpressionMatch firstEnd = endPem.match(scrubbed);
    const QRegularExpressionMatch firstBegin = beginPem.match(scrubbed);
    if (firstEnd.hasMatch()
        && (!firstBegin.hasMatch() || firstEnd.capturedStart() < firstBegin.capturedStart())) {
        scrubbed.replace(0, firstEnd.capturedEnd(), QStringLiteral("[REDACTED PRIVATE KEY]"));
    }
    const QRegularExpressionMatch openBegin = beginPem.match(scrubbed);
    if (openBegin.hasMatch() && !endPem.match(scrubbed, openBegin.capturedEnd()).hasMatch()) {
        scrubbed.replace(openBegin.capturedStart(), scrubbed.size() - openBegin.capturedStart(),
                         QStringLiteral("[REDACTED PRIVATE KEY]"));
    }
    // A long run of key or token characters holding both letters and
    // digits: a key, a token, a certificate or a hash, in any encoding
    // (base64, base64url, hex).
    const QRegularExpression longRun(QStringLiteral("[A-Za-z0-9+/=_-]{32,}"));
    const QRegularExpression letter(QStringLiteral("[A-Za-z]"));
    const QRegularExpression digit(QStringLiteral("[0-9]"));
    // A pairing code, <number>-<word>-<word> (PairingCode.h), however it
    // was typed.
    const QStringList& words = PairingCode::wordList();
    const QRegularExpression code(
        QStringLiteral("(?<![A-Za-z0-9])[0-9]{1,6}[\\s.-]+([A-Za-z]+)[\\s.-]+([A-Za-z]+)"
                       "(?![A-Za-z0-9])"));

    QStringList lines = scrubbed.split(QLatin1Char('\n'));
    for (QString& line : lines) {
        line = sanitizeLine(line);
        QRegularExpressionMatchIterator runs = longRun.globalMatch(line);
        QList<QRegularExpressionMatch> found;
        while (runs.hasNext()) {
            found.append(runs.next());
        }
        for (qsizetype i = found.size() - 1; i >= 0; --i) {
            const QString run = found[i].captured(0);
            if ((run.contains(letter) && run.contains(digit)) || run.size() >= 48) {
                line.replace(found[i].capturedStart(0), found[i].capturedLength(0),
                             QStringLiteral("[REDACTED]"));
            }
        }
        QRegularExpressionMatchIterator codes = code.globalMatch(line);
        found.clear();
        while (codes.hasNext()) {
            found.append(codes.next());
        }
        for (qsizetype i = found.size() - 1; i >= 0; --i) {
            const QString first = found[i].captured(1).toLower();
            const QString second = found[i].captured(2).toLower();
            if (words.isEmpty() || (words.contains(first) && words.contains(second))) {
                line.replace(found[i].capturedStart(0), found[i].capturedLength(0),
                             QStringLiteral("[REDACTED]"));
            }
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString SupportBundle::createArchive(const QString& sourceDir, const QString& archiveName)
{
    QString destDir = bundleDirPath();

#ifdef Q_OS_WIN
    // Windows: use PowerShell Compress-Archive
    QString zipPath = destDir + QLatin1Char('/') + archiveName + QStringLiteral(".zip");
    QProcess proc;
    proc.setProgram(QStringLiteral("powershell"));
    proc.setArguments({
        QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
        QStringLiteral("Compress-Archive -Path '%1/*' -DestinationPath '%2' -Force")
            .arg(QDir::toNativeSeparators(sourceDir),
                 QDir::toNativeSeparators(zipPath))
    });
    proc.start();
    proc.waitForFinished(10000);

    if (QFile::exists(zipPath)) {
        return zipPath;
    }
    return {};
#else
    // Unix/macOS: use tar
    QString tarPath = destDir + QLatin1Char('/') + archiveName + QStringLiteral(".tar.gz");
    QProcess proc;
    proc.setWorkingDirectory(QFileInfo(sourceDir).absolutePath());
    proc.setProgram(QStringLiteral("tar"));
    proc.setArguments({
        QStringLiteral("czf"), tarPath,
        QStringLiteral("-C"), QFileInfo(sourceDir).absolutePath(),
        QFileInfo(sourceDir).fileName()
    });
    proc.start();
    proc.waitForFinished(10000);

    if (QFile::exists(tarPath)) {
        return tarPath;
    }
    return {};
#endif
}

} // namespace NereusSDR
