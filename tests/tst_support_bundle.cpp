// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_support_bundle.cpp  (NereusSDR)
// =================================================================
//
// Remote-window parity Task 22 / the iPhone app plan's Task 25 (R-R3-49,
// R-IOS-18; acceptance B6.4): the support bundle.
//
//   - The ZIP the Core sends reads back entry for entry; a damaged one is
//     refused.
//   - Neither the Core's bundle nor a window's (with the Core's under
//     core/) carries a key, a token, a pairing code or a device key, from
//     the settings, the logs or nereusd.conf; ordinary lines stay.
//   - The Core's bundle never passes 2 MiB, however large its logs, and
//     keeps the newest end of the newest log.
//   - A remote window's bundle holds the Core's files under core/ and its
//     radio-info.json describes the Core's radio; with no Core bundle it
//     says why.
//
// Files in a temporary directory only. No RF, no audio device.
//
//   cmake --build build --target tst_support_bundle
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_support_bundle$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: adversarial privacy, size, and concurrent archive cases
//               added with OpenAI Codex assistance.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include <future>

#include "core/SupportBundle.h"
#include "core/ZipArchive.h"
#include "core/security/PairingCode.h"

using namespace NereusSDR;

namespace {

// Planted secrets, each in a shape the Core or a window really holds.
const QString kToken = QStringLiteral("Qm9ndXNUb2tlbjAxMjM0NTY3ODlhYmNkZWZnaGlqa2xtbm8");
const QString kDeviceKey = QStringLiteral("MCowBQYDK2VwAyEA7x9y2mQ4r8Zk1LpWq3NvB6cD0eF5gH8iJ2kL");
const QString kKeyInOddKey = QStringLiteral("3f9a1c0b7e6d5a4f3e2d1c0b9a8f7e6d5c4b3a29");
const QString kPassword = QStringLiteral("hunter2-station-password");

QString pairingCode()
{
    const QStringList& words = PairingCode::wordList();
    return QStringLiteral("4821-%1-%2").arg(words.value(3), words.value(7));
}

QStringList secrets()
{
    return {kToken, kDeviceKey, kKeyInOddKey, kPassword, pairingCode(),
            pairingCode().toUpper().replace(QLatin1Char('-'), QLatin1Char(' '))};
}

void writeText(const QString& path, const QString& text)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(text.toUtf8());
}

// A Core's files with every secret planted, and ordinary lines beside them.
SupportBundle::Inputs plantedInputs(const QTemporaryDir& dir)
{
    const QString logDir = dir.filePath(QStringLiteral("logs"));
    QDir().mkpath(logDir);
    writeText(dir.filePath(QStringLiteral("NereusSDR.settings")),
              QStringLiteral("<?xml version=\"1.0\"?>\n<NereusSDR>\n"
                             "  <SampleRate>192000</SampleRate>\n"
                             "  <RemoteStationToken>%1</RemoteStationToken>\n"
                             "  <StationDeviceKey>%2</StationDeviceKey>\n"
                             "  <CustomNote>%3</CustomNote>\n"
                             "  <Password>%4</Password>\n"
                             "  <LastCode>%5</LastCode>\n"
                             "</NereusSDR>\n")
                  .arg(kToken, kDeviceKey, kKeyInOddKey, kPassword, pairingCode()));
    writeText(QDir(logDir).filePath(QStringLiteral("nereussdr-20260927-100000.log")),
              QStringLiteral("[10:00:00.000] INF: Radio connected\n"
                             "[10:00:01.000] INF: Enrolled device %1\n"
                             "[10:00:02.000] INF: pairing code %2 shown\n"
                             "[10:00:03.000] INF: Next code %3 on the console\n"
                             "[10:00:04.000] WRN: auth.request token=%4\n"
                             "[10:00:05.000] INF: Slice A on 14.074 MHz\n")
                  .arg(kDeviceKey, pairingCode(), pairingCode().toUpper(), kToken));
    writeText(dir.filePath(QStringLiteral("nereusd.conf")),
              QStringLiteral("radio_mac = \n"
                             "rendezvous_secret = %1\n"
                             "remote_port = 47910\n")
                  .arg(kToken));
    SupportBundle::Inputs inputs;
    inputs.system.appVersion = QStringLiteral("0.5.2");
    inputs.radio.model = QStringLiteral("Bench HL2");
    inputs.radio.connected = true;
    inputs.logDir = logDir;
    inputs.settingsPath = dir.filePath(QStringLiteral("NereusSDR.settings"));
    inputs.daemonConfigPath = dir.filePath(QStringLiteral("nereusd.conf"));
    inputs.telemetry = QJsonObject{{QStringLiteral("cpuPercent"), 12.5}};
    inputs.stamp = QDateTime(QDate(2026, 9, 27), QTime(10, 0));
    return inputs;
}

QString leakedSecret(const QString& text)
{
    for (const QString& secret : secrets()) {
        if (text.contains(secret, Qt::CaseInsensitive)) {
            return secret;
        }
    }
    return {};
}

QHash<QString, QByteArray> entriesOf(const QByteArray& zip)
{
    QHash<QString, QByteArray> out;
    const std::optional<QList<ZipEntry>> entries = ZipArchive::read(zip);
    if (entries) {
        for (const ZipEntry& entry : *entries) {
            out.insert(entry.name, entry.data);
        }
    }
    return out;
}

// Every file under `dir`, read.
QHash<QString, QByteArray> filesUnder(const QString& dir)
{
    QHash<QString, QByteArray> out;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            out.insert(QDir(dir).relativeFilePath(path), f.readAll());
        }
    }
    return out;
}

} // namespace

class TstSupportBundle : public QObject {
    Q_OBJECT

private slots:
    void aZipReadsBackEntryForEntry()
    {
        QByteArray random(70000, '\0');
        for (char& c : random) {
            c = static_cast<char>(QRandomGenerator::global()->bounded(256));
        }
        ZipWriter zip;
        zip.add(QStringLiteral("empty.txt"), QByteArray());
        zip.add(QStringLiteral("text.log"), QByteArray(200000, 'a'));
        zip.add(QStringLiteral("dir/random.bin"), random);
        const QByteArray archive = zip.finish();
        QCOMPARE(archive.size(), zip.finishedSize());
        const std::optional<QList<ZipEntry>> entries = ZipArchive::read(archive);
        QVERIFY(entries);
        QCOMPARE(entries->size(), 3);
        QCOMPARE(entries->at(0).name, QStringLiteral("empty.txt"));
        QVERIFY(entries->at(0).data.isEmpty());
        QCOMPARE(entries->at(1).data, QByteArray(200000, 'a'));
        QCOMPARE(entries->at(2).name, QStringLiteral("dir/random.bin"));
        QCOMPARE(entries->at(2).data, random);
        // The compressible entry was deflated.
        QVERIFY(archive.size() < 200000);

        // A damaged byte fails its check; a cut archive is refused.
        QByteArray damaged = archive;
        damaged[40] = static_cast<char>(damaged[40] ^ 0x5a);
        QVERIFY(!ZipArchive::read(damaged));
        QVERIFY(!ZipArchive::read(archive.left(archive.size() / 2)));
        QVERIFY(!ZipArchive::read(QByteArray("not a zip at all, not one byte of it")));
    }

    void theCoresBundleCarriesNoSecret()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray zip = SupportBundle::buildCoreBundle(plantedInputs(dir));
        QVERIFY(!zip.isEmpty());
        const QHash<QString, QByteArray> entries = entriesOf(zip);
        for (const char* name : {"system-info.json", "radio-info.json", "enabled-categories.txt",
                                 "telemetry.json", "settings.xml", "nereusd.conf",
                                 "nereussdr.log"}) {
            QVERIFY2(entries.contains(QLatin1String(name)), name);
        }
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            const QString leaked = leakedSecret(QString::fromUtf8(it.value()));
            QVERIFY2(leaked.isEmpty(), qPrintable(it.key() + QStringLiteral(": ") + leaked));
        }
        // The ordinary lines stay.
        QVERIFY(entries.value(QStringLiteral("settings.xml")).contains("<SampleRate>192000</SampleRate>"));
        QVERIFY(entries.value(QStringLiteral("nereussdr.log")).contains("Radio connected"));
        QVERIFY(entries.value(QStringLiteral("nereussdr.log")).contains("Slice A on 14.074 MHz"));
        QVERIFY(entries.value(QStringLiteral("nereusd.conf")).contains("remote_port = 47910"));
        QVERIFY(entries.value(QStringLiteral("telemetry.json")).contains("cpuPercent"));
        const QJsonObject radio =
            QJsonDocument::fromJson(entries.value(QStringLiteral("radio-info.json"))).object();
        QCOMPARE(radio.value(QStringLiteral("model")).toString(), QStringLiteral("Bench HL2"));
    }

    void theCoresBundleNeverPassesTwoMebibytes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SupportBundle::Inputs inputs = plantedInputs(dir);
        // Three large logs of barely compressible lines, the newest last.
        QRandomGenerator random(22);
        for (int file = 0; file < 3; ++file) {
            QByteArray text;
            text.reserve(12 * 1024 * 1024);
            int line = 0;
            while (text.size() < 12 * 1024 * 1024) {
                text += "[10:00:00.000] DBG: sample " + QByteArray::number(line++) + ' '
                    + QByteArray::number(random.generate()) + ' '
                    + QByteArray::number(random.generate()) + '\n';
            }
            text += "[23:59:59.999] INF: newest line of file " + QByteArray::number(file) + '\n';
            QFile f(QDir(inputs.logDir).filePath(
                QStringLiteral("nereussdr-20260927-1%10000.log").arg(file)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text);
        }
        const QByteArray zip = SupportBundle::buildCoreBundle(inputs);
        QVERIFY(!zip.isEmpty());
        QVERIFY2(zip.size() <= SupportBundle::kMaxCoreBundleBytes,
                 qPrintable(QString::number(zip.size())));
        const QHash<QString, QByteArray> entries = entriesOf(zip);
        QVERIFY(entries.contains(QStringLiteral("system-info.json")));
        QVERIFY(entries.contains(QStringLiteral("settings.xml")));
        // The newest log's newest end is the one kept.
        QVERIFY(entries.value(QStringLiteral("nereussdr.log")).contains("newest line of file 2"));
        QVERIFY(entries.value(QStringLiteral("nereussdr.log"))
                    .startsWith("[the older part of this log is left out]"));
    }

    void aWindowsBundleHoldsTheCoresBesideItsOwn()
    {
#ifdef Q_OS_WIN
        QSKIP("A window's bundle is read back with tar here; Windows writes a zip.");
#endif
        QTemporaryDir coreDir;
        QTemporaryDir windowDir;
        QVERIFY(coreDir.isValid() && windowDir.isValid());
        const QByteArray coreZip = SupportBundle::buildCoreBundle(plantedInputs(coreDir));
        SupportBundle::Inputs window = plantedInputs(windowDir);
        window.radio = SupportBundle::RadioDiagInfo{};   // a remote window runs no radio
        SupportBundle::CoreAttachment core;
        core.wanted = true;
        core.bundle = coreZip;
        const QString archive = SupportBundle::writeBundle(window, core);
        QVERIFY(!archive.isEmpty());
        QTemporaryDir out;
        QProcess tar;
        tar.start(QStringLiteral("tar"), {QStringLiteral("xzf"), archive, QStringLiteral("-C"),
                                          out.path()});
        QVERIFY(tar.waitForFinished(30000));
        QCOMPARE(tar.exitCode(), 0);
        QFile::remove(archive);
        const QStringList top = QDir(out.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QCOMPARE(top.size(), 1);
        const QHash<QString, QByteArray> files = filesUnder(QDir(out.path()).filePath(top.first()));
        for (const char* name : {"system-info.json", "radio-info.json", "nereussdr.log",
                                 "settings.xml", "core/nereussdr.log", "core/radio-info.json",
                                 "core/settings.xml", "core/telemetry.json"}) {
            QVERIFY2(files.contains(QLatin1String(name)), name);
        }
        // radio-info.json describes the Core's radio.
        const QJsonObject radio =
            QJsonDocument::fromJson(files.value(QStringLiteral("radio-info.json"))).object();
        QCOMPARE(radio.value(QStringLiteral("model")).toString(), QStringLiteral("Bench HL2"));
        QVERIFY(radio.value(QStringLiteral("connected")).toBool());
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            const QString leaked = leakedSecret(QString::fromUtf8(it.value()));
            QVERIFY2(leaked.isEmpty(), qPrintable(it.key() + QStringLiteral(": ") + leaked));
        }
    }

    void aWindowsBundleSaysWhyTheCoresIsMissing()
    {
#ifdef Q_OS_WIN
        QSKIP("A window's bundle is read back with tar here; Windows writes a zip.");
#endif
        QTemporaryDir windowDir;
        SupportBundle::CoreAttachment core;
        core.wanted = true;
        core.reason = QStringLiteral("The Core did not send its support bundle in time.");
        const QString archive = SupportBundle::writeBundle(plantedInputs(windowDir), core);
        QVERIFY(!archive.isEmpty());
        QTemporaryDir out;
        QProcess tar;
        tar.start(QStringLiteral("tar"), {QStringLiteral("xzf"), archive, QStringLiteral("-C"),
                                          out.path()});
        QVERIFY(tar.waitForFinished(30000));
        QFile::remove(archive);
        const QStringList top = QDir(out.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QCOMPARE(top.size(), 1);
        const QHash<QString, QByteArray> files = filesUnder(QDir(out.path()).filePath(top.first()));
        QCOMPARE(files.value(QStringLiteral("core/unavailable.txt")).trimmed(),
                 core.reason.toUtf8());
    }

    void simultaneousBundlesWithOneTimestampStayDistinct()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const SupportBundle::Inputs inputs = plantedInputs(dir);
        auto first = std::async(std::launch::async, [inputs]() {
            return SupportBundle::writeBundle(inputs, {});
        });
        auto second = std::async(std::launch::async, [inputs]() {
            return SupportBundle::writeBundle(inputs, {});
        });
        const QString one = first.get();
        const QString two = second.get();
        QVERIFY(!one.isEmpty() && !two.isEmpty());
        QVERIFY(one != two);
        QVERIFY(QFileInfo(one).size() > 0);
        QVERIFY(QFileInfo(two).size() > 0);
#ifndef Q_OS_WIN
        for (const QString& path : {one, two}) {
            QProcess tar;
            tar.start(QStringLiteral("tar"), {QStringLiteral("tzf"), path});
            QVERIFY(tar.waitForFinished(30000));
            QCOMPARE(tar.exitCode(), 0);
            QVERIFY(tar.readAllStandardOutput().contains("radio-info.json"));
        }
#endif
        QFile::remove(one);
        QFile::remove(two);
    }

    void sanitizingKeepsOrdinaryWords()
    {
        // A number and two words that are not both pairing words stay; a
        // long plain word stays; a path stays.
        const QString ordinary = QStringLiteral(
            "Slice 1 moved to 14074000 Hz on band 20m\n"
            "12 dBm and more\n"
            "WaterfallColourSchemeAutomaticLevelling\n"
            "/Users/op/.config/NereusSDR/nereussdr.log");
        QCOMPARE(SupportBundle::sanitizeText(ordinary), ordinary);
        QVERIFY(!SupportBundle::sanitizeText(pairingCode()).contains(pairingCode()));
    }

    void structuredConfigFailsClosedForShortAndUnknownValues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SupportBundle::Inputs inputs = plantedInputs(dir);
        const QString shortSecret = QStringLiteral("abc7");
        writeText(inputs.settingsPath,
                  QStringLiteral("<NereusSDR><SampleRate>192000</SampleRate>"
                                 "<CustomNote>%1</CustomNote><RemotePassword>%1</RemotePassword>"
                                 "<%1>1</%1>"
                                 "</NereusSDR>").arg(shortSecret));
        writeText(inputs.daemonConfigPath,
                  QStringLiteral("remote_port = 47910\nunknown_option = %1\n"
                                 "%1 = 1\n"
                                 "core_name = %1\n").arg(shortSecret));
        inputs.knownSecrets = {shortSecret};
        writeText(QDir(inputs.logDir).filePath(QStringLiteral("nereussdr-20260927-110000.log")),
                  QStringLiteral("[11:00] INF: value %1 from the Core\n").arg(shortSecret));
        const QHash<QString, QByteArray> entries = entriesOf(SupportBundle::buildCoreBundle(inputs));
        QVERIFY(!entries.isEmpty());
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            QVERIFY2(!it.value().contains(shortSecret.toUtf8()), qPrintable(it.key()));
        }
        QVERIFY(entries.value(QStringLiteral("settings.xml")).contains("<SampleRate>192000</SampleRate>"));
        QVERIFY(entries.value(QStringLiteral("nereusd.conf")).contains("remote_port = 47910"));
        QVERIFY(entries.value(QStringLiteral("nereusd.conf")).contains("unreviewed_setting = [REDACTED]"));
        QVERIFY(entries.value(QStringLiteral("nereussdr.log")).contains("[REDACTED]"));
    }

    void oversizedConfigurationIsOmittedAndReported()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SupportBundle::Inputs inputs = plantedInputs(dir);
        QFile settings(inputs.settingsPath);
        QVERIFY(settings.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(settings.write(QByteArray(SupportBundle::kMaxConfigInputBytes + 1, 'x')) > 0);
        settings.close();
        const QHash<QString, QByteArray> entries = entriesOf(SupportBundle::buildCoreBundle(inputs));
        QVERIFY(!entries.contains(QStringLiteral("settings.xml")));
        const QJsonObject limits = QJsonDocument::fromJson(
            entries.value(QStringLiteral("collection-limits.json"))).object();
        QVERIFY(limits.value(QStringLiteral("settingsOmittedBecauseOversized")).toBool());
    }

    void multilinePrivateKeyIsRemovedFromLog()
    {
        const QString pem = QStringLiteral("-----BEGIN PRIVATE KEY-----\nabc7\ndef8\n"
                                           "-----END PRIVATE KEY-----");
        const QString clean = SupportBundle::sanitizeText(
            QStringLiteral("Before\n%1\nAfter").arg(pem));
        QVERIFY(clean.contains(QStringLiteral("Before")));
        QVERIFY(clean.contains(QStringLiteral("After")));
        QVERIFY(!clean.contains(QStringLiteral("abc7")));
        QVERIFY(!clean.contains(QStringLiteral("def8")));
        const QString orphan = QStringLiteral("AAAABBBBCCCCDDDDEEEE\n"
                                              "-----END PRIVATE KEY-----\nAfter");
        const QString orphanClean = SupportBundle::sanitizeText(orphan);
        QVERIFY(!orphanClean.contains(QStringLiteral("AAAABBBB")));
        QVERIFY(orphanClean.contains(QStringLiteral("After")));
        const QString alphabeticBody(64, QLatin1Char('A'));
        QVERIFY(!SupportBundle::sanitizeText(alphabeticBody).contains(alphabeticBody));
    }

    void zipReaderRejectsUnsafeNamesAndExpansion()
    {
        ZipWriter traversal;
        traversal.add(QStringLiteral("../settings.xml"), QByteArrayLiteral("value"));
        QVERIFY(!ZipArchive::read(traversal.finish()));

        ZipWriter bomb;
        bomb.add(QStringLiteral("huge.log"), QByteArray(9 * 1024 * 1024, 'x'));
        QVERIFY(bomb.finishedSize() < SupportBundle::kMaxCoreBundleBytes);
        QVERIFY(!ZipArchive::read(bomb.finish()));
    }
};

QTEST_GUILESS_MAIN(TstSupportBundle)
#include "tst_support_bundle.moc"
