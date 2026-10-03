// no-port-check: NereusSDR-original desktop backup export tests.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsBackup.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

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

QByteArray coreXml(const QString& path)
{
    AppSettings core(path);
    core.setValue(QStringLiteral("CoreBackupSentinel"), QStringLiteral("core-only"));
    return core.exportLocalXml();
}

class BackupLink final : public IStationLink {
public:
    RadioModel* model {nullptr};
    bool ready {true};
    bool available {true};
    bool refuse {false};
    quint32 nextId {42};
    int requests {0};
    int cancels {0};
    quint32 lastCancelledId {0};
    quint32 activeId {0};
    QByteArray synchronousXml;
    QByteArray cancelCompletionXml;
    std::function<void()> duringRequest;

    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return ready; }
    bool settingsBackupExportAvailable() const override { return available; }
    CommandOutcome requestSettingsBackupExport() override
    {
        ++requests;
        if (refuse) { return {false, QStringLiteral("The Core is busy with another export.")}; }
        const quint32 id = nextId++;
        activeId = id;
        if (duringRequest) { duringRequest(); }
        if (!synchronousXml.isEmpty()) {
            emit model->stationSettingsBackupExportFinished(id, true, {}, synchronousXml);
        }
        return {true, {}, id};
    }
    void cancelSettingsBackupExport(quint32 operationId = 0) override
    {
        ++cancels;
        lastCancelledId = operationId;
        if (operationId == 0 || operationId == activeId) { activeId = 0; }
        if (!cancelCompletionXml.isEmpty()) {
            emit model->stationSettingsBackupExportFinished(operationId, true, {},
                                                            cancelCompletionXml);
        }
    }
};

class ProbePage final : public ExportImportConfigPage {
public:
    explicit ProbePage(RadioModel* model, QWidget* parent = nullptr)
        : ExportImportConfigPage(model, parent) {}
    QString destination;
    std::function<void()> whileChoosing;
    QList<QPair<bool, QString>> messages;
    QString radioPickerMac;

protected:
    QString chooseExportDestination(bool) override
    {
        if (whileChoosing) { whileChoosing(); }
        return destination;
    }
    QString chooseRadioExportDestination(const QString& mac) override
    {
        radioPickerMac = mac;
        if (whileChoosing) { whileChoosing(); }
        return destination;
    }
    void showExportResult(bool success, const QString& text) override
    {
        messages.append({success, text});
    }
};

QPushButton* exportButton(ExportImportConfigPage& page)
{
    return page.findChild<QPushButton*>(QStringLiteral("exportAllSettingsButton"));
}

QPushButton* radioButton(ExportImportConfigPage& page)
{
    return page.findChild<QPushButton*>(QStringLiteral("exportRadioButton"));
}

QString radioExplanation(ExportImportConfigPage& page)
{
    auto* label = page.findChild<QLabel*>(QStringLiteral("exportRadioExplanation"));
    return label ? label->text() : QString();
}

// The keys a saved settings file holds.
QStringList savedKeys(const QString& path)
{
    AppSettings saved(path);
    saved.load();
    QStringList keys = saved.allKeys();
    keys.sort();
    return keys;
}

// A remote window's settings proxy: it answers every hardware/ key from
// what the Core sent.
class CoreHardwareBackend final : public ISettingsBackend {
public:
    QMap<QString, QString> core;
    bool handlesKey(const QString& key) const override
    {
        return key.startsWith(QStringLiteral("hardware/"));
    }
    QVariant value(const QString& key, const QVariant& fallback) const override
    {
        return core.contains(key) ? QVariant(core.value(key)) : fallback;
    }
    void setValue(const QString& key, const QVariant& val) override
    {
        core.insert(key, val.toString());
    }
    bool contains(const QString& key) const override { return core.contains(key); }
    void remove(const QString& key) override { core.remove(key); }
    QStringList handledKeys() const override { return core.keys(); }
};

RadioInfo radioWithMac(const QString& mac)
{
    RadioInfo info;
    info.name = QStringLiteral("ANAN-G2");
    info.macAddress = mac;
    return info;
}

} // namespace

class TstSettingsBackupWindow : public QObject {
    Q_OBJECT
private slots:
    void remoteButtonWritesBothOwnersOnlyAfterMatchedResult()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings::instance().setValue(QStringLiteral("WindowBackupSentinel"),
                                         QStringLiteral("window-only"));
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QPushButton* const button = exportButton(page);
        QVERIFY(button && button->isEnabled());
        button->click();
        QCOMPARE(link.requests, 1);
        QVERIFY(fileBytes(page.destination).isEmpty());
        AppSettings::instance().setValue(QStringLiteral("WindowBackupSentinel"),
                                         QStringLiteral("window-later"));
        emit remote.stationSettingsBackupExportFinished(999, true, {}, coreXml(
            dir.filePath(QStringLiteral("core.xml"))));
        QVERIFY(fileBytes(page.destination).isEmpty());
        const QByteArray expectedCore = coreXml(dir.filePath(QStringLiteral("core.xml")));
        emit remote.stationSettingsBackupExportFinished(42, true, {}, expectedCore);
        SettingsBackup bundle;
        QString error;
        QVERIFY2(SettingsBackup::readFile(page.destination, &bundle, &error), qPrintable(error));
        QCOMPARE(bundle.coreXml, expectedCore);
        QVERIFY(bundle.windowXml.contains("WindowBackupSentinel"));
        QVERIFY(bundle.windowXml.contains("window-only"));
        QVERIFY(!bundle.windowXml.contains("window-later"));
        QVERIFY(!bundle.windowXml.contains("CoreBackupSentinel"));
        QCOMPARE(page.messages.size(), 1);
        QVERIFY(page.messages.front().first);
        QCOMPARE(link.cancels, 0);
        remote.detachStation();
    }

    void oldCoreAndBusyRequestNeverPublishOrCancelAnotherJob()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        link.available = false;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QPushButton* const button = exportButton(page);
        QVERIFY(button && !button->isEnabled());
        QVERIFY(page.findChild<QLabel*>(QStringLiteral("backupExportExplanation"))
                    ->text().contains(QStringLiteral("updated Core")));
        button->click();
        QCOMPARE(link.requests, 0);
        QVERIFY(fileBytes(page.destination).isEmpty());
        QVERIFY(!page.findChild<QPushButton*>(QStringLiteral("importAllSettingsButton"))
                     ->isEnabled());

        link.available = true;
        link.refuse = true;
        page.setStationSettingsAvailable(true, {});
        QVERIFY(button->isEnabled());
        button->click();
        QCOMPARE(link.requests, 1);
        QCOMPARE(link.cancels, 0);
        QVERIFY(fileBytes(page.destination).isEmpty());
        QVERIFY(!page.messages.isEmpty() && !page.messages.back().first);
        remote.detachStation();
    }

    void synchronousReplyAndFailurePreserveDestination()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QVERIFY(writeBytes(page.destination, "old destination"));
        link.synchronousXml = coreXml(dir.filePath(QStringLiteral("core.xml")));
        exportButton(page)->click();
        SettingsBackup bundle;
        QVERIFY(SettingsBackup::readFile(page.destination, &bundle));
        QCOMPARE(bundle.coreXml, link.synchronousXml);

        link.synchronousXml.clear();
        const QByteArray saved = fileBytes(page.destination);
        exportButton(page)->click();
        emit remote.stationSettingsBackupExportFinished(43, false,
            QStringLiteral("Core refused while on the air"), {});
        QCOMPARE(fileBytes(page.destination), saved);
        exportButton(page)->click();
        emit remote.stationSettingsBackupExportFinished(44, true, {}, QByteArray("<broken"));
        QCOMPARE(fileBytes(page.destination), saved);
        QCOMPARE(link.cancels, 0);
        remote.detachStation();
    }

    void cancellationAndReconnectIgnoreLateResult()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        const QString destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QVERIFY(writeBytes(destination, "old destination"));
        {
            auto page = std::make_unique<ProbePage>(&remote);
            page->setStationSettingsAvailable(true, {});
            page->destination = destination;
            exportButton(*page)->click();
            QCOMPARE(link.requests, 1);
        }
        QCOMPARE(link.cancels, 1);
        emit remote.stationSettingsBackupExportFinished(42, true, {}, coreXml(
            dir.filePath(QStringLiteral("core.xml"))));
        QCOMPARE(fileBytes(destination), QByteArray("old destination"));

        ProbePage next(&remote);
        next.setStationSettingsAvailable(true, {});
        next.destination = destination;
        exportButton(next)->click();
        link.ready = false;
        remote.reportStationLinkStateChanged();
        link.ready = true;
        remote.reportStationLinkStateChanged();
        emit remote.stationSettingsBackupExportFinished(43, true, {}, coreXml(
            dir.filePath(QStringLiteral("core.xml"))));
        QCOMPARE(fileBytes(destination), QByteArray("old destination"));
        QCOMPARE(link.cancels, 1);
        remote.detachStation();
    }

    void closingPageCancelsOnlyItsOwnRequest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        exportButton(page)->click();
        QCOMPARE(link.requests, 1);
        page.close();
        QCOMPARE(link.cancels, 1);
        QCOMPARE(link.lastCancelledId, quint32(42));
        emit remote.stationSettingsBackupExportFinished(42, true, {}, coreXml(
            dir.filePath(QStringLiteral("core.xml"))));
        QVERIFY(fileBytes(page.destination).isEmpty());
        remote.detachStation();
    }

    void parentHideRetiresJobBeforeSynchronousCancelCompletion()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        link.cancelCompletionXml = coreXml(dir.filePath(QStringLiteral("core.xml")));
        remote.attachStation(&link);
        QWidget parent;
        ProbePage page(&remote, &parent);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QVERIFY(writeBytes(page.destination, "old destination"));
        parent.show();
        page.show();
        exportButton(page)->click();
        QCOMPARE(link.requests, 1);
        parent.hide();
        QCOMPARE(link.cancels, 1);
        QCOMPARE(link.lastCancelledId, quint32(42));
        QCOMPARE(fileBytes(page.destination), QByteArray("old destination"));
        QVERIFY(page.messages.isEmpty());
        parent.show();
        page.show();
        exportButton(page)->click();
        QCOMPARE(link.requests, 2);
        parent.close();
        QCOMPARE(link.cancels, 2);
        QCOMPARE(link.lastCancelledId, quint32(43));
        QCOMPARE(fileBytes(page.destination), QByteArray("old destination"));
        remote.detachStation();
    }

    void latePageCloseCannotCancelNewerExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage old(&remote);
        old.setStationSettingsAvailable(true, {});
        old.destination = dir.filePath(QStringLiteral("old.nereus-settings"));
        exportButton(old)->click();
        QCOMPARE(link.activeId, quint32(42));
        ProbePage newer(&remote);
        newer.setStationSettingsAvailable(true, {});
        newer.destination = dir.filePath(QStringLiteral("new.nereus-settings"));
        exportButton(newer)->click();
        QCOMPARE(link.activeId, quint32(43));
        old.close();
        QCOMPARE(link.lastCancelledId, quint32(42));
        QCOMPARE(link.activeId, quint32(43));
        emit remote.stationSettingsBackupExportFinished(43, true, {}, coreXml(
            dir.filePath(QStringLiteral("core.xml"))));
        QVERIFY(!fileBytes(newer.destination).isEmpty());
        QVERIFY(fileBytes(old.destination).isEmpty());
        remote.detachStation();
    }

    void modelDestroyedInsideSynchronousRequestCannotPublish()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto remote = std::make_unique<RadioModel>(RadioModel::Role::Remote);
        BackupLink link;
        link.model = remote.get();
        remote->attachStation(&link);
        ProbePage page(remote.get());
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QVERIFY(writeBytes(page.destination, "old destination"));
        link.duringRequest = [&] { remote.reset(); };
        exportButton(page)->click();
        QCOMPARE(link.requests, 1);
        QCOMPARE(fileBytes(page.destination), QByteArray("old destination"));
        QCOMPARE(link.cancels, 0);
        QVERIFY(!page.messages.isEmpty() && !page.messages.back().first);
    }

    void destinationReplacedByDirectoryFailsWithoutChangingIt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        QVERIFY(writeBytes(page.destination, "old destination"));
        exportButton(page)->click();
        // The filesystem can change while the Core is sending its snapshot.
        // A directory collision fails on Windows and privileged Linux runners
        // too; removing write permissions does not fail when the runner is root.
        const QString savedBackup = dir.filePath(QStringLiteral("previous-backup"));
        QVERIFY(QFile::rename(page.destination, savedBackup));
        QVERIFY(QDir().mkdir(page.destination));
        const QString sentinel = QDir(page.destination).filePath(QStringLiteral("keep-me"));
        QVERIFY(writeBytes(sentinel, "existing directory contents"));
        emit remote.stationSettingsBackupExportFinished(42, true, {}, coreXml(
            dir.filePath(QStringLiteral("core.xml"))));
        QVERIFY(QFileInfo(page.destination).isDir());
        QCOMPARE(fileBytes(sentinel), QByteArray("existing directory contents"));
        QCOMPARE(fileBytes(savedBackup), QByteArray("old destination"));
        QVERIFY(!page.messages.isEmpty() && !page.messages.back().first);
        remote.detachStation();
    }

    void modalGateChangesAndLocalXmlExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        page.destination = dir.filePath(QStringLiteral("pair.nereus-settings"));
        page.whileChoosing = [&] { remote.transmitModel().setMox(true); };
        exportButton(page)->click();
        QCOMPARE(link.requests, 0);
        QVERIFY(fileBytes(page.destination).isEmpty());
        remote.transmitModel().setMox(false);
        page.whileChoosing = [&] { page.setStationSettingsAvailable(false,
            QStringLiteral("Core settings unavailable")); };
        exportButton(page)->click();
        QCOMPARE(link.requests, 0);
        page.setStationSettingsAvailable(true, {});
        remote.detachStation();

        // A picker can outlive the model or the connection it started on.
        auto gone = std::make_unique<RadioModel>(RadioModel::Role::Remote);
        BackupLink goneLink;
        goneLink.model = gone.get();
        gone->attachStation(&goneLink);
        ProbePage gonePage(gone.get());
        gonePage.setStationSettingsAvailable(true, {});
        gonePage.destination = dir.filePath(QStringLiteral("gone.nereus-settings"));
        gonePage.whileChoosing = [&] { gone.reset(); };
        exportButton(gonePage)->click();
        QCOMPARE(goneLink.requests, 0);
        QVERIFY(fileBytes(gonePage.destination).isEmpty());

        RadioModel changed(RadioModel::Role::Remote);
        BackupLink first;
        BackupLink replacement;
        first.model = &changed;
        replacement.model = &changed;
        changed.attachStation(&first);
        ProbePage changedPage(&changed);
        changedPage.setStationSettingsAvailable(true, {});
        changedPage.destination = dir.filePath(QStringLiteral("changed.nereus-settings"));
        changedPage.whileChoosing = [&] {
            changed.detachStation();
            changed.attachStation(&replacement);
        };
        exportButton(changedPage)->click();
        QCOMPARE(first.requests, 0);
        QCOMPARE(replacement.requests, 0);
        QVERIFY(fileBytes(changedPage.destination).isEmpty());
        changed.detachStation();

        AppSettings::instance().setValue(QStringLiteral("LocalBackupSentinel"),
                                         QStringLiteral("saved locally"));
        RadioModel local;
        ProbePage localPage(&local);
        localPage.destination = dir.filePath(QStringLiteral("local.xml"));
        QVERIFY(writeBytes(localPage.destination, "old destination"));
        exportButton(localPage)->click();
        const QByteArray xml = fileBytes(localPage.destination);
        QString error;
        QVERIFY2(AppSettings::validateLocalXml(xml, &error), qPrintable(error));
        QVERIFY(xml.contains("LocalBackupSentinel"));
        QVERIFY(xml.contains("saved locally"));
        QVERIFY(!xml.contains("windowXml"));
        localPage.destination.clear();
        exportButton(localPage)->click();
        QCOMPARE(fileBytes(dir.filePath(QStringLiteral("local.xml"))), xml);
    }

    // Export Connected Radio saves the connected radio's settings and
    // nothing else; with no radio it is disabled with the reason.
    void exportConnectedRadioSavesOnlyThatRadio()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mac = QStringLiteral("00:1C:C0:A2:10:5E");
        const QString other = QStringLiteral("00:1C:C0:A2:10:99");
        AppSettings& settings = AppSettings::instance();
        settings.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 192000);
        settings.setHardwareValue(mac, QStringLiteral("antennas/rx1"), 2);
        settings.setHardwareValue(other, QStringLiteral("radioInfo/sampleRate"), 48000);
        settings.setValue(QStringLiteral("RadioExportWindowSentinel"), QStringLiteral("window"));
        const auto cleanup = qScopeGuard([&] {
            settings.clearHardwareValues(mac);
            settings.clearHardwareValues(other);
            settings.remove(QStringLiteral("RadioExportWindowSentinel"));
        });

        RadioModel local;
        ProbePage page(&local);
        page.destination = dir.filePath(QStringLiteral("radio.nereus-radio"));
        QPushButton* const button = radioButton(page);
        QVERIFY(button != nullptr);
        QVERIFY(!button->isHidden());
        QVERIFY(!button->isEnabled());
        QCOMPARE(radioExplanation(page), QStringLiteral("Connect a radio to export its settings."));
        QCOMPARE(button->toolTip(), radioExplanation(page));

        local.setLastRadioInfoForTest(radioWithMac(mac));
        local.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(button->isEnabled());
        QVERIFY(radioExplanation(page).startsWith(
            QStringLiteral("Saves only the settings kept for the connected radio")));
        button->click();
        QCOMPARE(page.radioPickerMac, mac);
        QCOMPARE(page.messages.size(), 1);
        QVERIFY(page.messages.last().first);
        const QString prefix = QStringLiteral("hardware/%1/").arg(mac);
        QCOMPARE(savedKeys(page.destination),
                 (QStringList{prefix + QStringLiteral("antennas/rx1"),
                              prefix + QStringLiteral("radioInfo/sampleRate")}));
        {
            AppSettings saved(page.destination);
            saved.load();
            QCOMPARE(saved.hardwareValue(mac, QStringLiteral("radioInfo/sampleRate")).toInt(),
                     192000);
        }

        // The radio goes while the picker is open: nothing is written.
        const QString gonePath = dir.filePath(QStringLiteral("gone.nereus-radio"));
        page.destination = gonePath;
        page.whileChoosing = [&] { local.setConnectionStateForTest(ConnectionState::Disconnected); };
        button->click();
        page.whileChoosing = nullptr;
        QVERIFY(!QFileInfo::exists(gonePath));
        QVERIFY(!page.messages.last().first);
        QVERIFY(!button->isEnabled());
    }

    void remoteExportConnectedRadioSavesTheCoresValues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mac = QStringLiteral("00:1C:C0:A2:20:01");
        AppSettings& settings = AppSettings::instance();
        // A stale local copy the Core's values replace.
        settings.setValue(QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac),
                          QStringLiteral("48000"));
        CoreHardwareBackend backend;
        backend.core.insert(QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac),
                            QStringLiteral("384000"));
        backend.core.insert(QStringLiteral("hardware/%1/alex/master/rxAnt").arg(mac),
                            QStringLiteral("3"));
        settings.setRemoteBackend(&backend);
        const auto cleanup = qScopeGuard([&] {
            settings.setRemoteBackend(nullptr);
            settings.clearHardwareValues(mac);
        });

        RadioModel remote(RadioModel::Role::Remote);
        BackupLink link;
        link.model = &remote;
        remote.attachStation(&link);
        ProbePage page(&remote);
        page.setStationSettingsAvailable(true, {});
        QPushButton* const button = radioButton(page);
        QVERIFY(!button->isEnabled());
        QCOMPARE(radioExplanation(page),
                 QStringLiteral("The Core has no radio connected. Connect one to export its "
                                "settings."));

        remote.setLastRadioInfoForTest(radioWithMac(mac));
        remote.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(button->isEnabled());
        QVERIFY(radioExplanation(page).startsWith(
            QStringLiteral("Saves only the Core's settings for its connected radio")));

        // The Core's settings unavailable: disabled with that reason.
        page.setStationSettingsAvailable(false, QStringLiteral("The Core's settings are loading."));
        QVERIFY(!button->isEnabled());
        QCOMPARE(radioExplanation(page), QStringLiteral("The Core's settings are loading."));
        page.setStationSettingsAvailable(true, {});

        page.destination = dir.filePath(QStringLiteral("core-radio.nereus-radio"));
        button->click();
        QVERIFY(page.messages.last().first);
        QCOMPARE(link.requests, 0);  // no command to the Core
        AppSettings saved(page.destination);
        saved.load();
        QCOMPARE(saved.hardwareValue(mac, QStringLiteral("radioInfo/sampleRate")).toString(),
                 QStringLiteral("384000"));
        QCOMPARE(saved.hardwareValue(mac, QStringLiteral("alex/master/rxAnt")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(saved.allKeys().size(), 2);
        remote.detachStation();
    }
};

QTEST_MAIN(TstSettingsBackupWindow)
#include "tst_settings_backup_window.moc"
