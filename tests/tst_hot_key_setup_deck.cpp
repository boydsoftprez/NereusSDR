// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 21.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Setup > Keys and controllers > Deck Plugins. The plugins connect here through a real
// socket, under the test's own home. Installs go to a scratch folder and an opener that
// only records: no real plugin folder is written and no real app is opened. No plugin
// host has run any of this (R-HK-19).
#include <QtTest>

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocalSocket>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/SetupDialog.h"
#include "gui/hotkeys/DeckLink.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/setup/DeckPluginsSetupPage.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using Page = DeckPluginsSetupPage;
using Plugin = DeckPluginsSetupPage::Plugin;
using Platform = DeckPluginsSetupPage::Platform;

namespace {

constexpr int kWaitMs = 30000;

class NoTargets : public HotKeyTargets {
public:
    SliceModel* windowSlice() const override { return nullptr; }
    SliceModel* sliceByLetter(QChar) const override { return nullptr; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel*, double) override { return true; }
    void runSliceAction(const QString&, SliceModel*, int) override {}
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool) override { return true; }
    void runTransmitAction(const QString&) override {}
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return 1000; }
};

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return QDir().mkpath(QFileInfo(path).absolutePath()) && file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList everythingUnder(const QString& folder)
{
    QStringList found;
    const QDir root(folder);
    const QFileInfoList entries = root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        found.append(entry.fileName());
    }
    return found;
}

} // namespace

class TestHotKeySetupDeck : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<NoTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<DeckLink> m_link;
    std::unique_ptr<Page> m_page;
    std::unique_ptr<QTemporaryDir> m_shortHome;
    std::unique_ptr<QTemporaryDir> m_scratch;
    QList<QUrl> m_opened;
    bool m_openerAnswer{true};

    QString scratch(const QString& relative) const { return QDir(m_scratch->path()).filePath(relative); }
    QString home() const { return scratch(QStringLiteral("home")); }
    QString appData() const { return scratch(QStringLiteral("appdata")); }
    QString bundled() const { return scratch(QStringLiteral("bundled")); }

    Page& open(Platform platform, DeckLink* link)
    {
        m_page = std::make_unique<Page>(link);
        m_page->setInstallPlacesForTest(bundled(), home(), appData());
        m_page->setOpenerForTest([this](const QUrl& url) {
            m_opened.append(url);
            return m_openerAnswer;
        });
        m_page->setPlatformForTest(platform);
        return *m_page;
    }

    DeckLink* makeLink()
    {
        m_link = std::make_unique<DeckLink>(m_dispatcher.get(), nullptr);
        return m_link.get();
    }

    template <typename T>
    T* named(const QString& name)
    {
        return m_page->findChild<T*>(name);
    }

    QPushButton* installOf(Plugin plugin)
    {
        return named<QPushButton>(QStringLiteral("deckPlugin:%1:install").arg(Page::helloName(plugin)));
    }
    QString stateOf(Plugin plugin)
    {
        return named<QLabel>(QStringLiteral("deckPlugin:%1:state").arg(Page::helloName(plugin)))->text();
    }
    QCheckBox* theSwitch() { return named<QCheckBox>(QStringLiteral("deckPluginsSwitch")); }
    QLabel* errorLine() { return named<QLabel>(QStringLiteral("deckPluginsError")); }
    QLabel* connectedLine() { return named<QLabel>(QStringLiteral("deckPluginsConnected")); }
    QString status() { return named<QLabel>(QStringLiteral("deckPluginsStatus"))->text(); }

    void fillBundle()
    {
        QVERIFY(writeFile(scratch(QStringLiteral("bundled/NereusSDR.streamDeckPlugin")), "zip"));
        for (const Plugin plugin : {Plugin::UlanziStudio, Plugin::StreamController}) {
            const QDir folder(QDir(bundled()).filePath(Page::bundledName(plugin)));
            QVERIFY(writeFile(folder.filePath(QStringLiteral("manifest.json")), "new"));
            QVERIFY(writeFile(folder.filePath(QStringLiteral("plugin/app.js")), "code"));
            QVERIFY(writeFile(folder.filePath(QStringLiteral(".hidden")), "dot"));
        }
    }

private slots:
    void initTestCase()
    {
        // A short home: a local socket's name has a short limit, and ctest's own test
        // home is too deep for it. As tst_deck_link does, before anything resolves a
        // settings path.
#if defined(Q_OS_WIN)
        m_shortHome = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/thd-XXXXXX"));
#else
        m_shortHome = std::make_unique<QTemporaryDir>(QStringLiteral("/tmp/thd-XXXXXX"));
#endif
        QVERIFY(m_shortHome->isValid());
        const QByteArray path = QFile::encodeName(m_shortHome->path());
        qputenv("HOME", path);
        qputenv("CFFIXED_USER_HOME", path);
        qputenv("NEREUS_TEST_HOME", path);
    }

    void init()
    {
        AppSettings::instance().clear();
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<NoTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_scratch = std::make_unique<QTemporaryDir>();
        QVERIFY(m_scratch->isValid());
        QVERIFY(QDir().mkpath(home()));
        QVERIFY(QDir().mkpath(appData()));
        m_opened.clear();
        m_openerAnswer = true;
    }

    void cleanup()
    {
        m_page.reset();
        m_link.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_bindings.reset();
        m_scratch.reset();
        AppSettings::instance().clear();
    }

    // ---- The rows ----

    void threePluginsEachWithInstallStateAndUntested()
    {
        open(Platform::Mac, makeLink());
        QCOMPARE(Page::plugins().size(), 3);
        for (const Plugin plugin : Page::plugins()) {
            QVERIFY(!Page::testedOnRealHost(plugin));
            QCOMPARE(stateOf(plugin), QStringLiteral("Not connected. Untested."));
            QPushButton* const install = installOf(plugin);
            QVERIFY(install != nullptr);
            QCOMPARE(install->text(), QStringLiteral("Install"));
        }
        QCOMPARE(theSwitch()->text(), QStringLiteral("Let deck plugins connect"));
        QCOMPARE(connectedLine()->text(), QStringLiteral("No plugins are connected."));
    }

    void thePageSaysARemoteCoreChangesNothing()
    {
        open(Platform::Mac, makeLink());
        const QString text = named<QLabel>(QStringLiteral("deckPluginsThisComputer"))->text();
        QVERIFY2(text.contains(QStringLiteral("a remote Core changes nothing here")), qPrintable(text));
    }

    void aSystemAPluginDoesntRunOnKeepsItsRowDisabledWithTheReason_data()
    {
        QTest::addColumn<Plugin>("plugin");
        QTest::addColumn<Platform>("platform");
        QTest::addColumn<QString>("reason");
        const QString noElgato = QStringLiteral("Elgato's app isn't on Linux.");
        const QString noUlanzi = QStringLiteral("Ulanzi Studio isn't on Linux.");
        const QString onlyLinux = QStringLiteral("StreamController is only on Linux.");
        QTest::newRow("elgato mac") << Plugin::StreamDeck << Platform::Mac << QString();
        QTest::newRow("elgato windows") << Plugin::StreamDeck << Platform::Windows << QString();
        QTest::newRow("elgato linux") << Plugin::StreamDeck << Platform::Linux << noElgato;
        QTest::newRow("ulanzi mac") << Plugin::UlanziStudio << Platform::Mac << QString();
        QTest::newRow("ulanzi windows") << Plugin::UlanziStudio << Platform::Windows << QString();
        QTest::newRow("ulanzi linux") << Plugin::UlanziStudio << Platform::Linux << noUlanzi;
        QTest::newRow("streamcontroller mac") << Plugin::StreamController << Platform::Mac << onlyLinux;
        QTest::newRow("streamcontroller windows") << Plugin::StreamController << Platform::Windows << onlyLinux;
        QTest::newRow("streamcontroller linux") << Plugin::StreamController << Platform::Linux << QString();
    }

    void aSystemAPluginDoesntRunOnKeepsItsRowDisabledWithTheReason()
    {
        QFETCH(Plugin, plugin);
        QFETCH(Platform, platform);
        QFETCH(QString, reason);
        open(platform, makeLink());
        QPushButton* const install = installOf(plugin);
        QCOMPARE(install->isEnabled(), reason.isEmpty());
        QCOMPARE(install->toolTip(), reason);
        QCOMPARE(install->accessibleDescription(), reason);
    }

    // ---- The switch ----

    void theSwitchWritesTheSettingAndOpensAndClosesTheLink()
    {
        open(Platform::Mac, makeLink());
        QVERIFY(theSwitch()->isChecked());     // on by default

        theSwitch()->setChecked(false);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/DeckLink/Enabled")).toString(), QStringLiteral("False"));
        QVERIFY(!m_link->isListening());

        theSwitch()->setChecked(true);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/DeckLink/Enabled")).toString(), QStringLiteral("True"));
        QVERIFY(m_link->isListening());
        QVERIFY(errorLine()->isHidden());
    }

    void withTheSwitchOffInstallIsDisabledWithTheReason()
    {
        open(Platform::Mac, makeLink());
        theSwitch()->setChecked(false);
        QVERIFY(!installOf(Plugin::UlanziStudio)->isEnabled());
        QCOMPARE(installOf(Plugin::UlanziStudio)->toolTip(), QStringLiteral("Let deck plugins connect is off."));
        // The row's own reason is still the true one.
        QCOMPARE(installOf(Plugin::StreamController)->toolTip(), QStringLiteral("StreamController is only on Linux."));

        theSwitch()->setChecked(true);
        QVERIFY(installOf(Plugin::UlanziStudio)->isEnabled());
        QVERIFY(installOf(Plugin::UlanziStudio)->toolTip().isEmpty());
    }

    void aSavedOffOpensOff()
    {
        AppSettings::instance().setValue(QStringLiteral("HotKeys/DeckLink/Enabled"), QStringLiteral("False"));
        open(Platform::Mac, makeLink());
        QVERIFY(!theSwitch()->isChecked());
        QVERIFY(!installOf(Plugin::StreamDeck)->isEnabled());
    }

    void aSocketInUseSaysWhyAndLeavesTheChoiceAlone()
    {
        DeckLink holder(m_dispatcher.get(), nullptr);
        QVERIFY(holder.setEnabled(true));

        // Off, so turning the switch on is what finds the socket taken.
        AppSettings::instance().setValue(QStringLiteral("HotKeys/DeckLink/Enabled"), QStringLiteral("False"));
        open(Platform::Mac, makeLink());
        QVERIFY(errorLine()->isHidden());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Deck link not open")));
        theSwitch()->setChecked(true);

        QVERIFY(!m_link->lastError().isEmpty());
        QVERIFY(!errorLine()->isHidden());
        QVERIFY2(errorLine()->text().startsWith(m_link->lastError()), qPrintable(errorLine()->text()));
        QVERIFY2(errorLine()->text().endsWith(QStringLiteral("Keys and knobs still work.")), qPrintable(errorLine()->text()));
        QVERIFY(theSwitch()->isChecked());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/DeckLink/Enabled")).toString(), QStringLiteral("True"));

        // It failed before the page opened, too.
        m_page.reset();
        open(Platform::Mac, m_link.get());
        QVERIFY(!errorLine()->isHidden());

        // Turning it off clears what no longer applies.
        theSwitch()->setChecked(false);
        QVERIFY(errorLine()->isHidden());
    }

    void withNoLinkTheSwitchIsDisabledWithTheReason()
    {
        open(Platform::Mac, nullptr);
        QVERIFY(!theSwitch()->isEnabled());
        QVERIFY2(theSwitch()->toolTip().contains(QStringLiteral("radio window")), qPrintable(theSwitch()->toolTip()));
        QCOMPARE(stateOf(Plugin::StreamDeck), QStringLiteral("Not connected. Untested."));
    }

    // ---- Connected plugins ----

    void connectedPluginsShowTheirNames()
    {
        open(Platform::Mac, makeLink());
        QVERIFY(m_link->setEnabled(true));
        QSignalSpy changed(m_link.get(), &DeckLink::pluginsChanged);

        QLocalSocket ulanzi;
        ulanzi.connectToServer(m_link->socketPath());
        QVERIFY(ulanzi.waitForConnected(kWaitMs));
        ulanzi.write("{\"type\":\"hello\",\"plugin\":\"ulanzi\",\"version\":\"1.0.0\"}\n");
        QTRY_VERIFY_WITH_TIMEOUT(changed.size() >= 1, kWaitMs);
        QCOMPARE(stateOf(Plugin::UlanziStudio), QStringLiteral("Connected. Untested."));
        QCOMPARE(stateOf(Plugin::StreamDeck), QStringLiteral("Not connected. Untested."));
        QCOMPARE(connectedLine()->text(), QStringLiteral("Connected now: Ulanzi Studio."));

        // A plugin we don't ship shows the name it gave, as text and not as markup.
        changed.clear();
        QLocalSocket other;
        other.connectToServer(m_link->socketPath());
        QVERIFY(other.waitForConnected(kWaitMs));
        other.write("{\"type\":\"hello\",\"plugin\":\"<b>mine</b>\",\"version\":\"1.0.0\"}\n");
        QTRY_VERIFY_WITH_TIMEOUT(changed.size() >= 1, kWaitMs);
        QVERIFY2(connectedLine()->text().contains(QStringLiteral("<b>mine</b>")), qPrintable(connectedLine()->text()));
        QVERIFY2(connectedLine()->text().contains(QStringLiteral("Ulanzi Studio")), qPrintable(connectedLine()->text()));
        QCOMPARE(connectedLine()->textFormat(), Qt::PlainText);

        changed.clear();
        ulanzi.disconnectFromServer();
        other.disconnectFromServer();
        QTRY_COMPARE_WITH_TIMEOUT(connectedLine()->text(), QStringLiteral("No plugins are connected."), kWaitMs);
        QCOMPARE(stateOf(Plugin::UlanziStudio), QStringLiteral("Not connected. Untested."));
    }

    // ---- Where they go ----

    void theDestinationsAreTheHostsOwn()
    {
        const QString h = QStringLiteral("/h");
        const QString a = QStringLiteral("/a");
        QCOMPARE(Page::destinationFolder(Plugin::UlanziStudio, Platform::Mac, h, a),
                 QStringLiteral("/h/Library/Application Support/Ulanzi/UlanziDeck/Plugins/com.nereussdr.radio.ulanziPlugin"));
        QCOMPARE(Page::destinationFolder(Plugin::UlanziStudio, Platform::Windows, h, a),
                 QStringLiteral("/a/Ulanzi/UlanziDeck/Plugins/com.nereussdr.radio.ulanziPlugin"));
        QCOMPARE(Page::destinationFolder(Plugin::StreamController, Platform::Linux, h, a),
                 QStringLiteral("/h/.var/app/com.core447.StreamController/data/plugins/com_nereussdr_radio"));
        QVERIFY(Page::destinationFolder(Plugin::UlanziStudio, Platform::Linux, h, a).isEmpty());
        QVERIFY(Page::destinationFolder(Plugin::StreamController, Platform::Mac, h, a).isEmpty());
        QVERIFY(Page::destinationFolder(Plugin::StreamDeck, Platform::Mac, h, a).isEmpty());
    }

    void theNamesMatchWhatTheBuildLaysOut()
    {
        const QByteArray cmake = readFile(QStringLiteral(NEREUS_SOURCE_DIR "/CMakeLists.txt"));
        for (const Plugin plugin : Page::plugins()) {
            const QByteArray laidOut = "${NEREUS_DECK_PLUGINS_DIR}/" + Page::bundledName(plugin).toUtf8();
            QVERIFY2(cmake.contains(laidOut), laidOut.constData());
        }
        // StreamController finds a plugin by this id.
        const QJsonObject manifest = QJsonDocument::fromJson(
            readFile(QStringLiteral(NEREUS_SOURCE_DIR "/plugins/streamcontroller-nereussdr/manifest.json"))).object();
        const QString destination = Page::destinationFolder(Plugin::StreamController, Platform::Linux, QStringLiteral("/h"), {});
        QCOMPARE(QFileInfo(destination).fileName(), manifest.value(QStringLiteral("id")).toString());
    }

    // ---- Install: Elgato ----

    void elgatoGoesToTheOsAsAFile()
    {
        fillBundle();
        open(Platform::Mac, makeLink());
        installOf(Plugin::StreamDeck)->click();
        QCOMPARE(m_opened.size(), 1);
        QVERIFY(m_opened.first().isLocalFile());
        QCOMPARE(m_opened.first().toLocalFile(), scratch(QStringLiteral("bundled/NereusSDR.streamDeckPlugin")));
        QVERIFY2(status().contains(QStringLiteral("Elgato's app")), qPrintable(status()));
        QVERIFY(everythingUnder(home()).isEmpty());
    }

    void whenNothingOpensItThePageSaysSo()
    {
        fillBundle();
        m_openerAnswer = false;
        open(Platform::Windows, makeLink());
        installOf(Plugin::StreamDeck)->click();
        QCOMPARE(m_opened.size(), 1);
        QVERIFY2(status().contains(QStringLiteral("Install Elgato's Stream Deck app")), qPrintable(status()));
    }

    void elgatoOnLinuxOpensNothing()
    {
        fillBundle();
        open(Platform::Linux, makeLink());
        installOf(Plugin::StreamDeck)->click();
        QVERIFY(m_opened.isEmpty());
    }

    // ---- Install: the two that copy ----

    void ulanziCopiesAndSaysToReopen()
    {
        fillBundle();
        const QString host = Page::hostFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData());
        QVERIFY(QDir().mkpath(host));
        open(Platform::Mac, makeLink());
        installOf(Plugin::UlanziStudio)->click();

        const QDir placed(Page::destinationFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData()));
        QCOMPARE(readFile(placed.filePath(QStringLiteral("manifest.json"))), QByteArray("new"));
        QCOMPARE(readFile(placed.filePath(QStringLiteral("plugin/app.js"))), QByteArray("code"));
        QCOMPARE(readFile(placed.filePath(QStringLiteral(".hidden"))), QByteArray("dot"));
        QCOMPARE(status(), QStringLiteral("Copied. Quit Ulanzi Studio and open it again to load the plugin."));
    }

    void ulanziOnWindowsGoesUnderAppData()
    {
        fillBundle();
        QVERIFY(QDir().mkpath(Page::hostFolder(Plugin::UlanziStudio, Platform::Windows, home(), appData())));
        open(Platform::Windows, makeLink());
        installOf(Plugin::UlanziStudio)->click();
        const QDir placed(Page::destinationFolder(Plugin::UlanziStudio, Platform::Windows, home(), appData()));
        QCOMPARE(readFile(placed.filePath(QStringLiteral("manifest.json"))), QByteArray("new"));
        QVERIFY(everythingUnder(home()).isEmpty());
    }

    void streamControllerCopiesToItsUnderscoredName()
    {
        fillBundle();
        QVERIFY(QDir().mkpath(Page::hostFolder(Plugin::StreamController, Platform::Linux, home(), appData())));
        open(Platform::Linux, makeLink());
        installOf(Plugin::StreamController)->click();
        const QString placed = QDir(home()).filePath(
            QStringLiteral(".var/app/com.core447.StreamController/data/plugins/com_nereussdr_radio/manifest.json"));
        QCOMPARE(readFile(placed), QByteArray("new"));
        QVERIFY2(status().startsWith(QStringLiteral("Copied.")), qPrintable(status()));
    }

    void anAppThatIsntThereGetsThePlainReasonAndNoFolders_data()
    {
        QTest::addColumn<Plugin>("plugin");
        QTest::addColumn<Platform>("platform");
        QTest::addColumn<QString>("reason");
        QTest::newRow("ulanzi") << Plugin::UlanziStudio << Platform::Mac
                                << QStringLiteral("Ulanzi Studio isn't on this computer. Install it, open it once, and try again.");
        QTest::newRow("streamcontroller") << Plugin::StreamController << Platform::Linux
                                          << QStringLiteral("StreamController's Flatpak isn't on this computer. Install it, open it once, and try again.");
    }

    void anAppThatIsntThereGetsThePlainReasonAndNoFolders()
    {
        QFETCH(Plugin, plugin);
        QFETCH(Platform, platform);
        QFETCH(QString, reason);
        fillBundle();
        open(platform, makeLink());
        installOf(plugin)->click();
        QCOMPARE(status(), reason);
        QVERIFY(everythingUnder(home()).isEmpty());
        QVERIFY(everythingUnder(appData()).isEmpty());
    }

    void aCopyWithoutThePluginSaysSoAndMakesNoFolders()
    {
        QVERIFY(QDir().mkpath(Page::hostFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData())));
        open(Platform::Mac, makeLink());
        installOf(Plugin::UlanziStudio)->click();
        QCOMPARE(status(), QStringLiteral("This copy of NereusSDR doesn't include the Ulanzi Studio plugin."));
        QVERIFY(!QFileInfo::exists(QFileInfo(Page::destinationFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData())).absolutePath()));

        installOf(Plugin::StreamDeck)->click();
        QCOMPARE(status(), QStringLiteral("This copy of NereusSDR doesn't include the Stream Deck plugin."));
        QVERIFY(m_opened.isEmpty());
    }

    void installingAgainReplacesWhatWasThere()
    {
        fillBundle();
        const QString host = Page::hostFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData());
        const QDir placed(Page::destinationFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData()));
        QVERIFY(writeFile(placed.filePath(QStringLiteral("old-file.js")), "old"));
        QVERIFY(writeFile(placed.filePath(QStringLiteral("manifest.json")), "old"));

        const Page::InstallResult result = Page::copyPlugin(Plugin::UlanziStudio, QDir(bundled()).filePath(Page::bundledName(Plugin::UlanziStudio)),
                                                            host, placed.path());
        QVERIFY(result.ok);
        QCOMPARE(readFile(placed.filePath(QStringLiteral("manifest.json"))), QByteArray("new"));
        QVERIFY(!QFileInfo::exists(placed.filePath(QStringLiteral("old-file.js"))));
        QCOMPARE(everythingUnder(QFileInfo(placed.path()).absolutePath()), QStringList{placed.dirName()});
    }

#if !defined(Q_OS_WIN)
    // Symbolic links need a privilege on Windows that a test run doesn't have.
    void aFailedCopyLeavesWhatWasThereAndNothingElse()
    {
        fillBundle();
        const QString source = QDir(bundled()).filePath(Page::bundledName(Plugin::UlanziStudio));
        QVERIFY(QFile::link(scratch(QStringLiteral("nowhere")), QDir(source).filePath(QStringLiteral("zz-link"))));
        const QString host = Page::hostFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData());
        const QDir placed(Page::destinationFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData()));
        QVERIFY(writeFile(placed.filePath(QStringLiteral("manifest.json")), "old"));

        const Page::InstallResult result = Page::copyPlugin(Plugin::UlanziStudio, source, host, placed.path());
        QVERIFY(!result.ok);
        QVERIFY2(result.message.startsWith(QStringLiteral("Could not copy the plugin")), qPrintable(result.message));
        QCOMPARE(readFile(placed.filePath(QStringLiteral("manifest.json"))), QByteArray("old"));
        QCOMPARE(everythingUnder(QFileInfo(placed.path()).absolutePath()), QStringList{placed.dirName()});
    }

    // Someone who linked the folder to a working copy keeps the working copy.
    void replacingALinkLeavesWhatItPointedAt()
    {
        fillBundle();
        const QString workingCopy = scratch(QStringLiteral("working-copy"));
        QVERIFY(writeFile(QDir(workingCopy).filePath(QStringLiteral("mine.js")), "mine"));
        const QString host = Page::hostFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData());
        const QString destination = Page::destinationFolder(Plugin::UlanziStudio, Platform::Mac, home(), appData());
        QVERIFY(QDir().mkpath(QFileInfo(destination).absolutePath()));
        QVERIFY(QFile::link(workingCopy, destination));

        const Page::InstallResult result = Page::copyPlugin(Plugin::UlanziStudio, QDir(bundled()).filePath(Page::bundledName(Plugin::UlanziStudio)),
                                                            host, destination);
        QVERIFY(result.ok);
        QVERIFY(!QFileInfo(destination).isSymLink());
        QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("manifest.json"))), QByteArray("new"));
        QCOMPARE(readFile(QDir(workingCopy).filePath(QStringLiteral("mine.js"))), QByteArray("mine"));
    }
#endif

    // ---- In Setup ----

    void theSetupDialogBuildsThePageAsThisComputerWithItsLink()
    {
        makeLink();
        RadioModel model;
        SetupDialog dialog(&model);
        dialog.setDeckLink(m_link.get());
        const QStringList labels = dialog.pageLabelsForTest();
        QCOMPARE(dialog.pageScopeAtForTest(static_cast<int>(labels.indexOf(QStringLiteral("Deck Plugins")))),
                 SetupScope::ThisComputer);
        dialog.selectPage(QStringLiteral("Deck Plugins"));
        auto* const page = dialog.findChild<Page*>();
        QVERIFY(page != nullptr);
        auto* const box = page->findChild<QCheckBox*>(QStringLiteral("deckPluginsSwitch"));
        QVERIFY(box != nullptr);
        QVERIFY(box->isEnabled());     // it has the link it was given
    }
};

QTEST_MAIN(TestHotKeySetupDeck)
#include "tst_hot_key_setup_deck.moc"
