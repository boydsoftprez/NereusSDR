// no-port-check: NereusSDR-original settings import tests.
//
// Setup > Diagnostics > Import All Settings used to copy the chosen file
// over the settings file and ask for a restart, while the running window
// kept its own values and wrote them back when it quit, so the restart
// found the old settings. The import is now saved as the file for the next
// launch, and the running store's later saves write nothing.

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

QByteArray fileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

// A settings file holding `values`, written as Export writes one.
bool writeSettingsFile(const QString& path, const QMap<QString, QString>& values)
{
    AppSettings file(path);
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        file.setValue(it.key(), it.value());
    }
    return file.save();
}

// What the next launch reads for `key` from the file at `path`.
QString nextLaunchValue(const QString& path, const QString& key)
{
    AppSettings nextLaunch(path);
    nextLaunch.load();
    return nextLaunch.value(key).toString();
}

class ImportProbePage final : public ExportImportConfigPage {
public:
    explicit ImportProbePage(RadioModel* model) : ExportImportConfigPage(model) {}
    QString source;
    bool confirm {true};
    int questions {0};
    QList<QPair<bool, QString>> messages;

protected:
    QString chooseImportSource() override { return source; }
    bool confirmImport(const QString&) override
    {
        ++questions;
        return confirm;
    }
    void showImportResult(bool success, const QString& text) override
    {
        messages.append({success, text});
    }
};

} // namespace

class TstSettingsImport : public QObject {
    Q_OBJECT
private:
    QString m_profileDir;

private slots:
    void initTestCase()
    {
        // The test sandbox's settings folder is shared by every test
        // binary; this process's own profile keeps parallel runs apart.
        const QString profile = QStringLiteral("tst_settings_import_%1")
            .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        const QString path = AppSettings::instance().filePath();
        QVERIFY2(path.contains(QStringLiteral("/profiles/%1/").arg(profile)), qPrintable(path));
        m_profileDir = QFileInfo(path).absolutePath();
    }

    void cleanupTestCase()
    {
        if (!m_profileDir.isEmpty()) {
            QDir(m_profileDir).removeRecursively();
        }
    }

    // The imported file is what the next launch reads, whatever the
    // running store saves afterwards; the store's own values stay in use.
    void importHoldsLaterSavesUntilRestart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("NereusSDR.settings"));
        AppSettings store(path);
        store.setValue(QStringLiteral("ImportSentinel"), QStringLiteral("running"));
        QVERIFY(store.save());

        const QString imported = dir.filePath(QStringLiteral("imported.xml"));
        QVERIFY(writeSettingsFile(imported, {{QStringLiteral("ImportSentinel"),
                                              QStringLiteral("imported")},
                                             {QStringLiteral("ImportedOnly"),
                                              QStringLiteral("yes")}}));
        QString error;
        QVERIFY2(store.importFileForNextLaunch(imported, &error), qPrintable(error));
        QVERIFY(error.isEmpty());
        QVERIFY(store.savesHeldUntilRestart());
        QCOMPARE(store.value(QStringLiteral("ImportSentinel")).toString(),
                 QStringLiteral("running"));
        QVERIFY(!store.contains(QStringLiteral("ImportedOnly")));

        // What quit does: the models write their state, then the store saves.
        store.setValue(QStringLiteral("ImportSentinel"), QStringLiteral("written at quit"));
        QVERIFY(store.save(&error));
        QVERIFY(error.isEmpty());
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportSentinel")),
                 QStringLiteral("imported"));
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportedOnly")), QStringLiteral("yes"));
        // The settings the import replaced are kept as the backup.
        QCOMPARE(nextLaunchValue(path + QStringLiteral(".bak"), QStringLiteral("ImportSentinel")),
                 QStringLiteral("running"));

        // A second import still replaces the file.
        const QString second = dir.filePath(QStringLiteral("second.xml"));
        QVERIFY(writeSettingsFile(second, {{QStringLiteral("ImportSentinel"),
                                            QStringLiteral("second")}}));
        QVERIFY2(store.importFileForNextLaunch(second, &error), qPrintable(error));
        QVERIFY(store.save());
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportSentinel")),
                 QStringLiteral("second"));
        QVERIFY(nextLaunchValue(path, QStringLiteral("ImportedOnly")).isEmpty());
    }

    // A refused file changes nothing: the store's file is as it was and
    // its saves still write.
    void refusedFileChangesNothing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("NereusSDR.settings"));
        AppSettings store(path);
        store.setValue(QStringLiteral("ImportSentinel"), QStringLiteral("before"));
        QVERIFY(store.save());
        const QByteArray before = fileBytes(path);

        const QString notSettings = dir.filePath(QStringLiteral("notes.xml"));
        QVERIFY(writeBytes(notSettings, "<html><body>not settings</body></html>"));
        const QString empty = dir.filePath(QStringLiteral("empty.xml"));
        QVERIFY(writeBytes(empty, {}));
        const QString oversized = dir.filePath(QStringLiteral("oversized.xml"));
        QVERIFY(writeBytes(oversized, QByteArray(16 * 1024 * 1024 + 1, ' ')));
        const QString missing = dir.filePath(QStringLiteral("missing.xml"));
        for (const QString& source : {notSettings, empty, oversized, missing}) {
            QString error;
            QVERIFY2(!store.importFileForNextLaunch(source, &error), qPrintable(source));
            QVERIFY2(!error.isEmpty(), qPrintable(source));
            QCOMPARE(fileBytes(path), before);
            QVERIFY(!store.savesHeldUntilRestart());
        }

        store.setValue(QStringLiteral("ImportSentinel"), QStringLiteral("after"));
        QVERIFY(store.save());
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportSentinel")),
                 QStringLiteral("after"));
    }

    // A file from a build before 2026-04-30, with element names it did not
    // escape, imports as load() reads it.
    void olderUnescapedFileImportsAsLoadReadsIt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString old = dir.filePath(QStringLiteral("old.settings"));
        QVERIFY(writeBytes(old, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                "<NereusSDR>\n"
                                "    <Profiles__s__D-104+CPDR>kept</Profiles__s__D-104+CPDR>\n"
                                "</NereusSDR>\n"));
        const QString key = QStringLiteral("Profiles/D-104+CPDR");
        QCOMPARE(nextLaunchValue(old, key), QStringLiteral("kept"));

        const QString path = dir.filePath(QStringLiteral("NereusSDR.settings"));
        AppSettings store(path);
        QString error;
        QVERIFY2(store.importFileForNextLaunch(old, &error), qPrintable(error));
        QCOMPARE(nextLaunchValue(path, key), QStringLiteral("kept"));
    }

    // The reported bug, through the Import button and the settings store
    // the window uses: after an import, the save quit runs (MainWindow's
    // aboutToQuit flushes the models into the store, then saves it) must
    // leave the imported file for the restart.
    void importButtonSurvivesTheQuitSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("ImportSentinel"), QStringLiteral("running"));
        settings.setValue(QStringLiteral("RunningOnly"), QStringLiteral("running"));
        QVERIFY(settings.save());
        const QString path = settings.filePath();

        RadioModel local;
        ImportProbePage page(&local);
        auto* const button = page.findChild<QPushButton*>(
            QStringLiteral("importAllSettingsButton"));
        QVERIFY(button && button->isEnabled());

        // The picker cancelled, then No to the question: nothing changes.
        button->click();
        QCOMPARE(page.questions, 0);
        const QString imported = dir.filePath(QStringLiteral("imported.xml"));
        QVERIFY(writeSettingsFile(imported, {{QStringLiteral("ImportSentinel"),
                                              QStringLiteral("imported")},
                                             {QStringLiteral("ImportedOnly"),
                                              QStringLiteral("imported")}}));
        page.source = imported;
        page.confirm = false;
        button->click();
        QCOMPARE(page.questions, 1);
        QVERIFY(page.messages.isEmpty());
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportSentinel")),
                 QStringLiteral("running"));

        page.confirm = true;
        button->click();
        QCOMPARE(page.messages.size(), 1);
        QVERIFY(page.messages.last().first);
        QCOMPARE(page.messages.last().second,
                 QStringLiteral("Settings imported. Please restart NereusSDR."));

        settings.setValue(QStringLiteral("ImportSentinel"), QStringLiteral("written at quit"));
        QVERIFY(settings.save());
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportSentinel")),
                 QStringLiteral("imported"));
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportedOnly")),
                 QStringLiteral("imported"));
        QVERIFY(nextLaunchValue(path, QStringLiteral("RunningOnly")).isEmpty());

        // A file that is not settings is refused and the import stays.
        const QString notSettings = dir.filePath(QStringLiteral("notes.txt"));
        QVERIFY(writeBytes(notSettings, "not settings"));
        page.source = notSettings;
        button->click();
        QCOMPARE(page.messages.size(), 2);
        QVERIFY(!page.messages.last().first);
        QVERIFY2(page.messages.last().second.contains(notSettings),
                 qPrintable(page.messages.last().second));
        QCOMPARE(nextLaunchValue(path, QStringLiteral("ImportSentinel")),
                 QStringLiteral("imported"));
    }
};

QTEST_MAIN(TstSettingsImport)
#include "tst_settings_import.moc"
