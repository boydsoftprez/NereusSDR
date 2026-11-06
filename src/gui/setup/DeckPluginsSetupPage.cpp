// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 21.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/setup/DeckPluginsSetupPage.h"

#include "gui/hotkeys/DeckLink.h"
#include "gui/setup/HotKeySetupCommon.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

#include <utility>

namespace NereusSDR {

using namespace HotKeySetup;

namespace {

// A plugin picks its own name, so what the page shows of one is kept short.
constexpr int kLongestPluginName = 40;

const QString kUlanziFolderName = QStringLiteral("com.nereussdr.radio.ulanziPlugin");
// StreamController wants underscores: the id in the plugin's own manifest.json.
const QString kStreamControllerFolderName = QStringLiteral("com_nereussdr_radio");

QString checkBoxStyle()
{
    return QString::fromLatin1(Style::kCheckBoxStyle)
        + QStringLiteral("QCheckBox { background: transparent; }QCheckBox:disabled { color: #5a6a7a; }");
}

QString switchName()
{
    return QStringLiteral("Let deck plugins connect");
}

QString rowName(DeckPluginsSetupPage::Plugin plugin, const QString& what)
{
    return QStringLiteral("deckPlugin:%1:%2").arg(DeckPluginsSetupPage::helloName(plugin), what);
}

// Takes away a link and not what it points at, which removeRecursively would empty.
bool removePath(const QString& path)
{
    const QFileInfo info(path);
    if (info.isSymLink() || info.isFile()) {
        return QFile::remove(path);
    }
    return QDir(path).removeRecursively();
}

// What ships has no links, so one here means something is wrong: following it could
// copy far more than a plugin, or never finish.
bool copyFolder(const QString& from, const QString& to)
{
    if (!QDir().mkpath(to)) {
        return false;
    }
    const QFileInfoList entries = QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString target = QDir(to).filePath(entry.fileName());
        if (entry.isSymLink()) {
            return false;
        }
        if (entry.isDir() ? !copyFolder(entry.filePath(), target) : !QFile::copy(entry.filePath(), target)) {
            return false;
        }
    }
    return true;
}

QString copiedMessage(DeckPluginsSetupPage::Plugin plugin)
{
    return plugin == DeckPluginsSetupPage::Plugin::UlanziStudio
               ? QStringLiteral("Copied. Quit Ulanzi Studio and open it again to load the plugin.")
               : QStringLiteral("Copied. If StreamController is open, quit it and open it again to load the plugin.");
}

QString hostMissingMessage(DeckPluginsSetupPage::Plugin plugin)
{
    return plugin == DeckPluginsSetupPage::Plugin::UlanziStudio
               ? QStringLiteral("Ulanzi Studio isn't on this computer. Install it, open it once, and try again.")
               : QStringLiteral("StreamController's Flatpak isn't on this computer. Install it, open it once, and try again.");
}

QString notIncludedMessage(DeckPluginsSetupPage::Plugin plugin)
{
    return QStringLiteral("This copy of NereusSDR doesn't include the %1 plugin.").arg(DeckPluginsSetupPage::pluginName(plugin));
}

} // namespace

DeckPluginsSetupPage::DeckPluginsSetupPage(DeckLink* link, QWidget* parent)
    : SetupPage(QStringLiteral("Deck Plugins"), parent)
    , m_link(link)
    , m_platform(KnobsSetupPage::thisPlatform())
    , m_bundled(bundledFolder())
    , m_home(QDir::homePath())
    , m_appData(qEnvironmentVariable("APPDATA"))
    , m_opener([](const QUrl& url) { return QDesktopServices::openUrl(url); })
{
    auto* const statement = new QLabel(
        QStringLiteral("Deck plugins run on this computer and connect to this window, so a remote Core changes nothing here."));
    statement->setObjectName(QStringLiteral("deckPluginsThisComputer"));
    statement->setWordWrap(true);
    statement->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    addContent(statement);

    m_switch = new QCheckBox(switchName());
    m_switch->setObjectName(QStringLiteral("deckPluginsSwitch"));
    m_switch->setProperty("nereusSetupId", QStringLiteral("keys.deckPlugins.enabled"));
    m_switch->setStyleSheet(checkBoxStyle());
    m_switch->setChecked(DeckLink::savedEnabled());
    connect(m_switch, &QCheckBox::toggled, this, &DeckPluginsSetupPage::onSwitchToggled);
    addContent(m_switch);

    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("deckPluginsError"));
    m_error->setWordWrap(true);
    m_error->setTextFormat(Qt::PlainText);
    m_error->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #c8d8e8; }"));
    addContent(m_error);

    m_section = addSection(QStringLiteral("Plugins"));
    m_section->setObjectName(QStringLiteral("deckPluginsList"));
    auto* const sectionLayout = qobject_cast<QVBoxLayout*>(m_section->layout());
    auto* const grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(2, 1);
    int gridRow = 0;
    for (const Plugin plugin : plugins()) {
        auto* const name = new QLabel(pluginName(plugin), m_section);
        name->setObjectName(rowName(plugin, QStringLiteral("name")));
        grid->addWidget(name, gridRow, 0);

        auto* const systems = new QLabel(runsOn(plugin), m_section);
        systems->setObjectName(rowName(plugin, QStringLiteral("runsOn")));
        systems->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(systems, gridRow, 1);

        Row row;
        row.state = new QLabel(m_section);
        row.state->setObjectName(rowName(plugin, QStringLiteral("state")));
        grid->addWidget(row.state, gridRow, 2);

        row.install = new QPushButton(QStringLiteral("Install"), m_section);
        row.install->setObjectName(rowName(plugin, QStringLiteral("install")));
        row.install->setProperty("nereusSetupId", QStringLiteral("keys.deckPlugins.install.") + helloName(plugin));
        row.install->setAccessibleName(QStringLiteral("Install the %1 plugin").arg(pluginName(plugin)));
        row.install->setStyleSheet(smallButtonStyle());
        connect(row.install, &QPushButton::clicked, this, [this, plugin] { install(plugin); });
        grid->addWidget(row.install, gridRow, 3);

        m_rows.insert(plugin, row);
        ++gridRow;
    }
    sectionLayout->addLayout(grid);

    m_connected = new QLabel(m_section);
    m_connected->setObjectName(QStringLiteral("deckPluginsConnected"));
    m_connected->setWordWrap(true);
    m_connected->setTextFormat(Qt::PlainText);    // the names come from the plugins
    m_connected->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    sectionLayout->addWidget(m_connected);

    m_status = new QLabel(m_section);
    m_status->setObjectName(QStringLiteral("deckPluginsStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    sectionLayout->addWidget(m_status);
    setStatus({});

    if (m_link) {
        connect(m_link.data(), &DeckLink::pluginsChanged, this, &DeckPluginsSetupPage::refresh);
    }
    refresh();
}

DeckPluginsSetupPage::~DeckPluginsSetupPage() = default;

QList<DeckPluginsSetupPage::Plugin> DeckPluginsSetupPage::plugins()
{
    return {Plugin::StreamDeck, Plugin::StreamController, Plugin::UlanziStudio};
}

QString DeckPluginsSetupPage::pluginName(Plugin plugin)
{
    switch (plugin) {
    case Plugin::StreamDeck:
        return QStringLiteral("Stream Deck");
    case Plugin::StreamController:
        return QStringLiteral("StreamController");
    case Plugin::UlanziStudio:
        return QStringLiteral("Ulanzi Studio");
    }
    return {};
}

QString DeckPluginsSetupPage::helloName(Plugin plugin)
{
    // From plugins/README.md, the table's Hello name column.
    switch (plugin) {
    case Plugin::StreamDeck:
        return QStringLiteral("elgato");
    case Plugin::StreamController:
        return QStringLiteral("streamcontroller");
    case Plugin::UlanziStudio:
        return QStringLiteral("ulanzi");
    }
    return {};
}

QString DeckPluginsSetupPage::runsOn(Plugin plugin)
{
    return plugin == Plugin::StreamController ? QStringLiteral("Linux") : QStringLiteral("macOS, Windows");
}

bool DeckPluginsSetupPage::testedOnRealHost(Plugin plugin)
{
    Q_UNUSED(plugin);
    return false;
}

QString DeckPluginsSetupPage::unavailableReason(Plugin plugin, Platform platform)
{
    switch (plugin) {
    case Plugin::StreamDeck:
        return platform == Platform::Linux ? QStringLiteral("Elgato's app isn't on Linux.") : QString();
    case Plugin::UlanziStudio:
        return platform == Platform::Linux ? QStringLiteral("Ulanzi Studio isn't on Linux.") : QString();
    case Plugin::StreamController:
        return platform != Platform::Linux ? QStringLiteral("StreamController is only on Linux.") : QString();
    }
    return {};
}

QString DeckPluginsSetupPage::bundledFolder()
{
    // From CMakeLists.txt, NereusDeckPlugins and the install rule beneath it.
    const QDir app(QCoreApplication::applicationDirPath());
#if defined(Q_OS_MAC)
    return app.absoluteFilePath(QStringLiteral("../Resources/plugins"));
#elif defined(Q_OS_WIN)
    return app.absoluteFilePath(QStringLiteral("plugins"));
#else
    // Run from the build folder, nothing has been installed to share/ beside it.
    const QString installed = app.absoluteFilePath(QStringLiteral("../share/nereussdr/plugins"));
    const QString built = app.absoluteFilePath(QStringLiteral("deck-plugins"));
    return QFileInfo::exists(installed) || !QFileInfo::exists(built) ? installed : built;
#endif
}

QString DeckPluginsSetupPage::bundledName(Plugin plugin)
{
    switch (plugin) {
    case Plugin::StreamDeck:
        return QStringLiteral("NereusSDR.streamDeckPlugin");
    case Plugin::StreamController:
        return QStringLiteral("streamcontroller-nereussdr");
    case Plugin::UlanziStudio:
        return kUlanziFolderName;
    }
    return {};
}

QString DeckPluginsSetupPage::hostFolder(Plugin plugin, Platform platform, const QString& home, const QString& appData)
{
    if (!unavailableReason(plugin, platform).isEmpty()) {
        return {};
    }
    switch (plugin) {
    case Plugin::StreamDeck:
        return {};
    case Plugin::UlanziStudio:
        // From AetherSDR plugins/ulanzi-aethersdr/README.md:14-16 [@1e0718ad].
        return platform == Platform::Windows ? QDir(appData).filePath(QStringLiteral("Ulanzi/UlanziDeck"))
                                             : QDir(home).filePath(QStringLiteral("Library/Application Support/Ulanzi/UlanziDeck"));
    case Plugin::StreamController:
        // Flatpak's per-app folder for StreamController's id (its globals.py).
        return QDir(home).filePath(QStringLiteral(".var/app/com.core447.StreamController"));
    }
    return {};
}

QString DeckPluginsSetupPage::destinationFolder(Plugin plugin, Platform platform, const QString& home, const QString& appData)
{
    const QString host = hostFolder(plugin, platform, home, appData);
    if (host.isEmpty()) {
        return {};
    }
    return plugin == Plugin::UlanziStudio
               ? QDir(host).filePath(QStringLiteral("Plugins/") + kUlanziFolderName)
               : QDir(host).filePath(QStringLiteral("data/plugins/") + kStreamControllerFolderName);
}

DeckPluginsSetupPage::InstallResult DeckPluginsSetupPage::copyPlugin(Plugin plugin, const QString& source,
                                                                     const QString& hostFolder, const QString& destination)
{
    if (!QFileInfo(source).isDir()) {
        return {false, notIncludedMessage(plugin)};
    }
    if (!QFileInfo(hostFolder).isDir()) {
        return {false, hostMissingMessage(plugin)};
    }

    const InstallResult failed{false, QStringLiteral("Could not copy the plugin to %1.").arg(QDir::toNativeSeparators(destination))};
    // Built beside its place and moved there, so the host never loads half a plugin.
    const QString staging = destination + QStringLiteral(".installing");
    const QString previous = destination + QStringLiteral(".previous");
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !removePath(staging) || !removePath(previous)) {
        return failed;
    }
    if (!copyFolder(source, staging)) {
        removePath(staging);
        return failed;
    }
    const bool hadOne = QFileInfo(destination).exists() || QFileInfo(destination).isSymLink();
    if (hadOne && !QFile::rename(destination, previous)) {
        removePath(staging);
        return failed;
    }
    if (!QFile::rename(staging, destination)) {
        removePath(staging);
        if (hadOne) {
            QFile::rename(previous, destination);
        }
        return failed;
    }
    if (hadOne) {
        removePath(previous);
    }
    return {true, copiedMessage(plugin)};
}

DeckPluginsSetupPage::InstallResult DeckPluginsSetupPage::openPlugin(Plugin plugin, const QString& source, const Opener& opener)
{
    if (!QFileInfo(source).isFile()) {
        return {false, notIncludedMessage(plugin)};
    }
    if (!opener(QUrl::fromLocalFile(source))) {
        return {false, QStringLiteral("Nothing on this computer opened the plugin. Install Elgato's Stream Deck app and try again.")};
    }
    return {true, QStringLiteral("Handed to Elgato's app. Finish there if it asks anything.")};
}

void DeckPluginsSetupPage::install(Plugin plugin)
{
    if (!unavailableReason(plugin, m_platform).isEmpty()) {
        return;
    }
    const QString source = QDir(m_bundled).filePath(bundledName(plugin));
    const InstallResult result = plugin == Plugin::StreamDeck
        ? openPlugin(plugin, source, m_opener)
        : copyPlugin(plugin, source, hostFolder(plugin, m_platform, m_home, m_appData),
                     destinationFolder(plugin, m_platform, m_home, m_appData));
    setStatus(result.message);
}

void DeckPluginsSetupPage::onSwitchToggled(bool on)
{
    if (m_link) {
        m_link->setEnabled(on);     // on failure the choice stays saved, and refresh says why
    }
    refresh();
}

void DeckPluginsSetupPage::refresh()
{
    const bool on = m_switch->isChecked();
    const QString linkMissing = m_link ? QString() : QStringLiteral("Deck plugins belong to a radio window, and this page has none.");
    m_switch->setEnabled(m_link != nullptr);
    setReason(m_switch, linkMissing);

    const QString lastError = m_link ? m_link->lastError() : QString();
    m_error->setText(lastError.isEmpty() ? QString() : lastError + QStringLiteral(" Keys and knobs still work."));
    m_error->setVisible(!lastError.isEmpty());

    const QString offReason = QStringLiteral("%1 is off.").arg(switchName());
    m_section->setEnabled(on);
    setReason(m_section, on ? QString() : offReason);

    const QStringList connected = m_link ? QStringList(m_link->connectedPlugins()) : QStringList();
    QStringList shown;
    for (const QString& hello : connected) {
        QString name = hello.simplified().left(kLongestPluginName);
        for (const Plugin plugin : plugins()) {
            if (hello == helloName(plugin)) {
                name = pluginName(plugin);
            }
        }
        if (!shown.contains(name)) {
            shown.append(name);
        }
    }
    m_connected->setText(shown.isEmpty() ? QStringLiteral("No plugins are connected.")
                                         : QStringLiteral("Connected now: %1.").arg(shown.join(QStringLiteral(", "))));

    for (const Plugin plugin : plugins()) {
        const Row row = m_rows.value(plugin);
        const QString word = connected.contains(helloName(plugin)) ? QStringLiteral("Connected") : QStringLiteral("Not connected");
        const QString tested = testedOnRealHost(plugin) ? QStringLiteral("Tested") : QStringLiteral("Untested");
        row.state->setText(QStringLiteral("%1. %2.").arg(word, tested));

        const QString systemReason = unavailableReason(plugin, m_platform);
        const QString reason = !systemReason.isEmpty() ? systemReason : on ? QString() : offReason;
        row.install->setEnabled(reason.isEmpty());
        setReason(row.install, reason);
    }
}

void DeckPluginsSetupPage::setStatus(const QString& text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

void DeckPluginsSetupPage::setInstallPlacesForTest(const QString& bundled, const QString& home, const QString& appData)
{
    m_bundled = bundled;
    m_home = home;
    m_appData = appData;
}

void DeckPluginsSetupPage::setOpenerForTest(Opener opener)
{
    m_opener = std::move(opener);
}

void DeckPluginsSetupPage::setPlatformForTest(Platform platform)
{
    m_platform = platform;
    refresh();
}

} // namespace NereusSDR
