// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// The plugins' files against the one action list, and what each plugin folder must carry.
//
// When the action list changes, this fails until regenerated:
//   NEREUS_WRITE_ACTIONS_JSON=1 ./build/tests/tst_deck_plugin_actions
//   python3 plugins/tools/gen-manifests.py
#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include "core/hotkeys/HotKeyActionList.h"

using namespace NereusSDR;

namespace {

const QString kPlugins = QStringLiteral(NEREUS_SOURCE_DIR "/plugins/");

QString gestureName(HotKeyGesture gesture)
{
    switch (gesture) {
    case HotKeyGesture::Press:
        return QStringLiteral("press");
    case HotKeyGesture::Hold:
        return QStringLiteral("hold");
    case HotKeyGesture::Turn:
        return QStringLiteral("turn");
    }
    return QString();
}

QJsonArray expectedActions()
{
    QJsonArray actions;
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        actions.append(QJsonObject{{QStringLiteral("id"), action.id},
                                   {QStringLiteral("name"), action.name},
                                   {QStringLiteral("group"), action.group},
                                   {QStringLiteral("gesture"), gestureName(action.gesture)},
                                   {QStringLiteral("transmits"), action.transmits}});
    }
    return actions;
}

QJsonDocument readJson(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()) : QJsonDocument();
}

} // namespace

class TestDeckPluginActions : public QObject {
    Q_OBJECT

private slots:
    void theActionListFileMatchesTheActions()
    {
        const QString path = kPlugins + QStringLiteral("common/actions.json");
        if (qEnvironmentVariableIsSet("NEREUS_WRITE_ACTIONS_JSON")) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write(QJsonDocument(expectedActions()).toJson(QJsonDocument::Indented));
        }
        const QJsonArray actual = readJson(path).array();
        const QJsonArray expected = expectedActions();
        QCOMPARE(actual.size(), expected.size());
        for (qsizetype i = 0; i < expected.size(); ++i) {
            QCOMPARE(actual.at(i).toObject(), expected.at(i).toObject());
        }
    }

    void theManifestsMatchTheActionList()
    {
        const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        if (python.isEmpty()) {
            QSKIP("python3 is not on the path, so gen-manifests.py --check can't run");
        }
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(python, {kPlugins + QStringLiteral("tools/gen-manifests.py"), QStringLiteral("--check")});
        QVERIFY(process.waitForFinished(60000));
        QVERIFY2(process.exitCode() == 0, process.readAll().constData());
    }

    void everyActionIsInBothManifests()
    {
        const QStringList manifests = {
            QStringLiteral("elgato-nereussdr/com.nereussdr.radio.sdPlugin/manifest.json"),
            QStringLiteral("ulanzi-nereussdr/com.nereussdr.radio.ulanziPlugin/manifest.json"),
        };
        for (const QString& manifest : manifests) {
            const QJsonArray listed = readJson(kPlugins + manifest).object().value(QStringLiteral("Actions")).toArray();
            QCOMPARE(listed.size(), HotKeyActionList::all().size());
            for (qsizetype i = 0; i < listed.size(); ++i) {
                const HotKeyAction& action = HotKeyActionList::all().at(i);
                const QJsonObject entry = listed.at(i).toObject();
                QVERIFY(entry.value(QStringLiteral("UUID")).toString().endsWith(QLatin1Char('.') + action.id));
                // A dial can't be held or key the radio, as the dispatcher's turn refuses.
                const bool onDials = entry.value(QStringLiteral("Controllers")).toArray().contains(QStringLiteral("Encoder"));
                QCOMPARE(onDials, action.gesture == HotKeyGesture::Turn);
                QVERIFY(!(onDials && action.transmits));
            }
        }
    }

    void everyPluginSaysUntestedAndCarriesItsLicenseNote_data()
    {
        QTest::addColumn<QString>("folder");
        QTest::addColumn<QString>("description");
        QTest::newRow("Elgato") << QStringLiteral("elgato-nereussdr/com.nereussdr.radio.sdPlugin")
                                << QStringLiteral("Description");
        QTest::newRow("Ulanzi") << QStringLiteral("ulanzi-nereussdr/com.nereussdr.radio.ulanziPlugin")
                                << QStringLiteral("Description");
        QTest::newRow("StreamController") << QStringLiteral("streamcontroller-nereussdr")
                                          << QStringLiteral("descriptions");
    }

    void everyPluginSaysUntestedAndCarriesItsLicenseNote()
    {
        QFETCH(QString, folder);
        QFETCH(QString, description);
        const QString root = kPlugins + folder + QLatin1Char('/');
        QVERIFY(QFile::exists(root + QStringLiteral("LICENSE-NOTE.md")));
        const QJsonValue value = readJson(root + QStringLiteral("manifest.json")).object().value(description);
        const QString text = value.isObject() ? value.toObject().value(QStringLiteral("en_US")).toString() : value.toString();
        QVERIFY2(text.contains(QStringLiteral("Untested")), qPrintable(text));
    }

    void noPluginCallsTci()
    {
        const QRegularExpression tci(QStringLiteral("\\btci\\b|50001|tci_"), QRegularExpression::CaseInsensitiveOption);
        QDirIterator files(kPlugins, QDir::Files, QDirIterator::Subdirectories);
        int seen = 0;
        while (files.hasNext()) {
            const QString path = files.next();
            if (path.contains(QStringLiteral("/node_modules/")) || path.contains(QStringLiteral("/libs/"))
                || path.endsWith(QStringLiteral(".png")) || path.endsWith(QStringLiteral(".md"))) {
                continue;
            }
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QString text = QString::fromUtf8(file.readAll());
            // The files that say they call none are the only ones allowed the word.
            const QString withoutClaims = QString(text).remove(QStringLiteral("calls no TCI"));
            QVERIFY2(!tci.match(withoutClaims).hasMatch(), qPrintable(path));
            ++seen;
        }
        QVERIFY(seen > 5);
    }

    void theElgatoFolderIsAtTheRootOfItsZip()
    {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QStringLiteral(NEREUS_CMAKE_COMMAND),
                      {QStringLiteral("-E"), QStringLiteral("tar"), QStringLiteral("tf"),
                       QStringLiteral(NEREUS_BUILD_DIR "/deck-plugins/NereusSDR.streamDeckPlugin")});
        QVERIFY(process.waitForFinished(60000));
        const QString output = QString::fromUtf8(process.readAll());
        QCOMPARE(process.exitCode(), 0);
        const QStringList entries = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY(!entries.isEmpty());
        for (const QString& entry : entries) {
            QVERIFY2(entry.startsWith(QStringLiteral("com.nereussdr.radio.sdPlugin/")), qPrintable(entry));
        }
        QVERIFY(entries.contains(QStringLiteral("com.nereussdr.radio.sdPlugin/manifest.json")));
        QVERIFY(entries.contains(QStringLiteral("com.nereussdr.radio.sdPlugin/node_modules/ws/index.js")));
    }
};

QTEST_GUILESS_MAIN(TestDeckPluginActions)
#include "tst_deck_plugin_actions.moc"
