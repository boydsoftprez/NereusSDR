// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 21.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Setup > Keys and controllers > Deck Plugins: the switch for the private link, and the
// three plugins with Install, their connection state and Untested (R-HK-17, R-HK-19). The
// link stops at this window, so a remote Core changes nothing here.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/SetupPage.h"
#include "gui/setup/KnobsSetupPage.h"

#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>

class QCheckBox;
class QGroupBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class DeckLink;

class NEREUS_GUI_EXPORT DeckPluginsSetupPage : public SetupPage {
    Q_OBJECT

public:
    using Platform = KnobsSetupPage::Platform;
    using Opener = std::function<bool(const QUrl&)>;

    enum class Plugin { StreamDeck, StreamController, UlanziStudio };

    struct InstallResult {
        bool ok{false};
        QString message;
    };

    // A null link leaves the switch disabled, with the reason.
    explicit DeckPluginsSetupPage(DeckLink* link, QWidget* parent = nullptr);
    ~DeckPluginsSetupPage() override;

    static QList<Plugin> plugins();
    static QString pluginName(Plugin plugin);
    // The name the plugin gives the link in its hello.
    static QString helloName(Plugin plugin);
    static QString runsOn(Plugin plugin);
    // R-HK-19: false until a real host has run the plugin and the code is changed to say so.
    static bool testedOnRealHost(Plugin plugin);

    // Why it can't be installed on this system; empty when it can. Free of the running
    // system so a test on one computer can read all three.
    static QString unavailableReason(Plugin plugin, Platform platform);

    // Where the build lays out the plugins (plugins/README.md, "Where they end up").
    static QString bundledFolder();
    static QString bundledName(Plugin plugin);

    // The host's own folder, there once it has been installed and opened, and the folder
    // the plugin goes in. Both empty for Elgato's app, which installs its own.
    static QString hostFolder(Plugin plugin, Platform platform, const QString& home, const QString& appData);
    static QString destinationFolder(Plugin plugin, Platform platform, const QString& home, const QString& appData);

    // Copies source to destination, replacing what's there. It makes no folder unless
    // hostFolder is there, and a copy that fails leaves what was there before.
    static InstallResult copyPlugin(Plugin plugin, const QString& source, const QString& hostFolder,
                                    const QString& destination);
    // Hands the file to the app that opens it.
    static InstallResult openPlugin(Plugin plugin, const QString& source, const Opener& opener);

    // A test never writes a real plugin folder, so it says where they are.
    void setInstallPlacesForTest(const QString& bundled, const QString& home, const QString& appData);
    void setOpenerForTest(Opener opener);
    void setPlatformForTest(Platform platform);

private:
    struct Row {
        QLabel* state{nullptr};
        QPushButton* install{nullptr};
    };

    void refresh();
    void install(Plugin plugin);
    void onSwitchToggled(bool on);
    void setStatus(const QString& text);

    QPointer<DeckLink> m_link;
    Platform m_platform;
    QString m_bundled;
    QString m_home;
    QString m_appData;
    Opener m_opener;

    QCheckBox* m_switch{nullptr};
    QLabel* m_error{nullptr};
    QGroupBox* m_section{nullptr};
    QHash<Plugin, Row> m_rows;
    QLabel* m_connected{nullptr};
    QLabel* m_status{nullptr};
};

} // namespace NereusSDR
