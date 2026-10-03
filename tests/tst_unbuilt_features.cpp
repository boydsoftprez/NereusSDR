// =================================================================
// tests/tst_unbuilt_features.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives real local and remote
// MainWindows, Setup pages, dialogs, applets and containers; no upstream
// logic is ported here.
//
// R3 unfinished controls, Task 1 (R-R3-49, R-R3-21): one list names every
// feature that is not built yet (UnbuiltFeatures), and every surface that
// fronts one is hidden through it, in local and remote windows.
//   - The test enumerates the list: every entry has surface checks here, so
//     a feature added to the list without its surfaces fails.
//   - Nothing on the list shows in a local or a remote window.
//   - Marking one feature built makes each of its surfaces appear, and no
//     other feature's surface.
//   - Hidden is not removed: saved values for hidden controls come back
//     unchanged after a start and a save.
//   - A saved container holding a Voice Rec/Play control loads, does not
//     show or list it, does not offer a new one, and saves it back.
//
// R3 unfinished controls, Task 2 (R-R3-49, R-R3-21): the controls the
// operator removed are built in no window, local or remote; the values
// users saved for them stay in the settings file; a saved Discord control
// is dropped on load with one log line; and no Setup page or category with
// nothing to show is offered or found (Logging & Performance, once its
// Performance checkboxes went).
//
// R3 unfinished controls, Task 3 (R-R3-49, R-R3-21): the container function
// buttons with nothing behind them (RX2, SUB, SWAP, AVG, filter Var1 and
// Var2, antenna Rx/Tx: "macro-buttons"; DUP, PLAY, REC, XPA, the display
// modes, the antenna and band XVTR under their features' entries) are
// surfaces of the list like any other.
//
// R3 unfinished controls, Task 4 (R-R3-49, R-R3-21): filter Var1 and Var2
// ("variable-filters") and antenna Rx/Tx ("antenna-rx-tx") have entries of
// their own, so marking the macro buttons built does not show them.
//
// R3 unfinished controls fix wave (R-R3-49, R-R3-21): the macro buttons
// ("macro-buttons"), AVG ("display-averaging") and RX2, SUB and SWAP
// ("two-receiver-layout") are three entries, so marking one built shows
// only its own buttons.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, Task 1.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, Task 3.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, Task 4.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, fix wave.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Remote-window parity Task 13: the
//                                    Alex-1 TX filter options and HL2 TX
//                                    timings. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  R-R3-49 load round: one row per
//                                    feature for marking one built, and
//                                    four ctest entries (main()).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextEdit>
#include <QWidget>

#include <functional>
#include <memory>

#include "OperatorWording.h"
#include "TestFunctionGroups.h"
#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/RadioDiscovery.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/SetupDialog.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpotHubDialog.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/widgets/FilterPolicyDialog.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/ItemGroup.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/ClickBoxItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

using F = UnbuiltFeature;

StationStartupSelection remoteCore()
{
    // Never dialled: replace(..., false) builds the remote window without
    // starting its connection.
    return {{QStringLiteral("ws://127.0.0.1:4433"), {}, {}, true}, QStringLiteral("core")};
}

// True when `w` would be on screen once `root` is: nothing between them is
// hidden, except by a stacked widget choosing another page (a tab not
// selected is still offered).
bool shownWithin(const QWidget* w, const QWidget* root)
{
    if (w == nullptr) { return false; }
    for (const QWidget* p = w; p != nullptr && p != root; p = p->parentWidget()) {
        const bool stackPage = qobject_cast<const QStackedWidget*>(p->parentWidget()) != nullptr;
        if (p->isHidden() && !stackPage) { return false; }
    }
    return true;
}

template <typename T>
bool namedShown(const QWidget* root, const QString& name)
{
    if (root == nullptr) { return false; }
    const T* w = root->findChild<T*>(name);
    return shownWithin(w, root);
}

// JJ's rule "disabled, never hidden": a control named `name`, shown and
// usable. An unbuilt one may be in view, disabled with its reason.
bool usableShown(const QWidget* root, const QString& name)
{
    if (root == nullptr) { return false; }
    const QWidget* w = root->findChild<QWidget*>(name);
    return w != nullptr && shownWithin(w, root) && w->isEnabled();
}

// A label or button with exactly this text, shown.
bool textShown(const QWidget* root, const QString& text)
{
    if (root == nullptr) { return false; }
    for (const QLabel* label : root->findChildren<QLabel*>()) {
        if (label->text() == text && shownWithin(label, root)) { return true; }
    }
    for (const QAbstractButton* button : root->findChildren<QAbstractButton*>()) {
        if (button->text() == text && shownWithin(button, root)) { return true; }
    }
    return false;
}

bool groupShown(const QWidget* root, const QString& title)
{
    if (root == nullptr) { return false; }
    for (const QGroupBox* group : root->findChildren<QGroupBox*>()) {
        if (group->title() == title && shownWithin(group, root)) { return true; }
    }
    return false;
}

// A tab with this text, offered in any tab widget under `root`.
bool tabShown(const QWidget* root, const QString& text)
{
    if (root == nullptr) { return false; }
    for (const QTabWidget* tabs : root->findChildren<QTabWidget*>()) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == text && tabs->isTabVisible(i)
                && shownWithin(tabs, root)) {
                return true;
            }
        }
    }
    return false;
}

bool actionShown(const QWidget* root, const QString& text)
{
    for (const QAction* action : root->findChildren<QAction*>()) {
        if (action->text() == text && action->isVisible()) { return true; }
    }
    return false;
}

// R-R3-49 removals: true when anything under `root` carries this text at
// all, shown or hidden (a removed control is not built, not merely hidden).
bool textBuilt(const QWidget* root, const QString& text)
{
    if (root == nullptr) { return false; }
    for (const QLabel* label : root->findChildren<QLabel*>()) {
        if (label->text() == text) { return true; }
    }
    for (const QAbstractButton* button : root->findChildren<QAbstractButton*>()) {
        if (button->text() == text) { return true; }
    }
    for (const QGroupBox* group : root->findChildren<QGroupBox*>()) {
        if (group->title() == text) { return true; }
    }
    for (const QTabWidget* tabs : root->findChildren<QTabWidget*>()) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == text) { return true; }
        }
    }
    for (const QAction* action : root->findChildren<QAction*>()) {
        if (action->text() == text) { return true; }
    }
    return false;
}

// True when a Setup page shows something besides its title: a control, a
// group, a view or a line of text. A page with none of these is empty.
bool pageHasShownContent(const QWidget* page, const QString& title)
{
    for (const QWidget* w : page->findChildren<QWidget*>()) {
        if (!shownWithin(w, page)) { continue; }
        if (qobject_cast<const QScrollBar*>(w) != nullptr) { continue; }
        if (const auto* label = qobject_cast<const QLabel*>(w)) {
            if (label->text() != title && !label->text().isEmpty()) { return true; }
            continue;
        }
        if (qobject_cast<const QAbstractButton*>(w) || qobject_cast<const QComboBox*>(w)
            || qobject_cast<const QAbstractSpinBox*>(w) || qobject_cast<const QAbstractSlider*>(w)
            || qobject_cast<const QLineEdit*>(w) || qobject_cast<const QTextEdit*>(w)
            || qobject_cast<const QPlainTextEdit*>(w) || qobject_cast<const QGroupBox*>(w)
            || qobject_cast<const QTabWidget*>(w)
            || (qobject_cast<const QAbstractItemView*>(w) != nullptr)) {
            return true;
        }
    }
    return false;
}

// A captured log line, for the Discord drop's one line.
QStringList& capturedLog()
{
    static QStringList lines;
    return lines;
}

void captureLog(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    capturedLog() << QStringLiteral("%1 %2").arg(int(type)).arg(message);
}

// Everything the surface checks build, once each, for one set of marks in
// the list. A fresh set is built after the list changes, because every
// surface reads the list when it is built.
class Hosts {
public:
    Hosts(GuiSessionCoordinator& sessions, bool remote)
        : m_sessions(sessions), m_remote(remote) {}

    ~Hosts()
    {
        m_filterPolicy.reset();
        m_containerDialog.reset();
        m_container.reset();
        m_flagHost.reset();
        m_rfKit.reset();
        m_phoneCw.reset();
    }

    bool remote() const { return m_remote; }

    MainWindow* window()
    {
        if (m_window == nullptr) {
            const bool ok = m_remote ? m_sessions.replace(remoteCore(), false)
                                     : m_sessions.replace({}, false);
            m_window = ok ? m_sessions.window() : nullptr;
            if (m_window != nullptr) {
                // Wide enough that the status bar folds nothing away.
                m_window->resize(4000, 1000);
                m_window->show();
                QCoreApplication::processEvents();
            }
        }
        return m_window;
    }

    SetupDialog* setup()
    {
        if (m_setup == nullptr && window() != nullptr) {
            // The dialog MainWindow::createSetupDialog() builds, on the
            // window's own model (that slot is private).
            m_setup = new SetupDialog(m_window->radioModel(), m_window);
        }
        return m_setup;
    }

    bool pageRegistered(const QString& label)
    {
        return setup() != nullptr && m_setup->pageLabelsForTest().contains(label);
    }

    bool categoryShown(const QString& label)
    {
        if (setup() == nullptr) { return false; }
        const auto* tree = m_setup->findChild<QTreeWidget*>();
        for (int i = 0; tree != nullptr && i < tree->topLevelItemCount(); ++i) {
            if (tree->topLevelItem(i)->text(0) == label && !tree->topLevelItem(i)->isHidden()) {
                return true;
            }
        }
        return false;
    }

    QWidget* page(const QString& label)
    {
        if (!pageRegistered(label)) { return nullptr; }
        return m_setup->realizePageForTest(label);
    }

    SpotHubDialog* spotHub()
    {
        if (m_spotHub == nullptr && window() != nullptr) {
            QMetaObject::invokeMethod(m_window, "openSpotHub", Qt::DirectConnection);
            m_spotHub = m_window->findChild<SpotHubDialog*>();
        }
        return m_spotHub;
    }

    NetworkDiagnosticsDialog* networkDiagnostics()
    {
        if (m_netDiag == nullptr && window() != nullptr) {
            m_netDiag = new NetworkDiagnosticsDialog(m_window->radioModel(), nullptr, m_window);
        }
        return m_netDiag;
    }

    PhoneCwApplet* phoneCw()
    {
        if (window() == nullptr) { return nullptr; }
        if (auto* applet = m_window->findChild<PhoneCwApplet*>()) { return applet; }
        if (!m_phoneCw) {
            m_phoneCw = std::make_unique<PhoneCwApplet>(m_window->radioModel());
        }
        return m_phoneCw.get();
    }

    Rf2ksApplet* rfKit()
    {
        if (window() == nullptr) { return nullptr; }
        if (auto* applet = m_window->findChild<Rf2ksApplet*>()) { return applet; }
        if (!m_rfKit) {
            m_rfKit = std::make_unique<Rf2ksApplet>(m_window->radioModel());
        }
        return m_rfKit.get();
    }

    // A slice flag on a shown panadapter, positioned once so its floating
    // buttons exist.
    VfoWidget* flag()
    {
        if (!m_flagHost) {
            m_flagHost = std::make_unique<SpectrumWidget>();
            m_flagHost->resize(1200, 500);
            m_flagHost->setSampleRate(192000.0);
            m_flagHost->setDdcCenterFrequency(14200000.0);
            m_flagHost->setFrequencyRange(14200000.0, 192000.0);
            m_flag = m_flagHost->addVfoWidget(0);
            m_flag->setFrequency(14200000.0);
            m_flagHost->show();
            QCoreApplication::processEvents();
            m_flagHost->updateVfoPositions();
        }
        return m_flag;
    }

    // A container holding a Voice Rec/Play control, and its settings
    // dialog with the Add menu opened once.
    ContainerSettingsDialog* containerDialog()
    {
        if (!m_containerDialog) {
            m_container = std::make_unique<ContainerWidget>();
            m_meter = new MeterWidget();
            m_meter->addItem(new TextItem());
            m_meter->addItem(new VoiceRecordPlayItem());
            m_meter->addItem(new FilterDisplayItem());
            m_meter->addItem(new ClickBoxItem());
            auto* pbSnr = new BarItem();
            pbSnr->setBindingId(MeterBinding::PbSnr);
            m_meter->addItem(pbSnr);
            m_container->setContent(m_meter);
            m_containerDialog = std::make_unique<ContainerSettingsDialog>(m_container.get());
            for (QPushButton* button : m_containerDialog->findChildren<QPushButton*>()) {
                if (button->text() == QStringLiteral("+")) { button->click(); }
            }
        }
        return m_containerDialog.get();
    }

    MeterWidget* containerMeter()
    {
        containerDialog();
        return m_meter;
    }

    // R3 Task 3: the container function, filter, antenna and band buttons,
    // built when first asked for (after the list is set for the pass).
    OtherButtonItem* otherButtons()
    {
        if (!m_otherButtons) { m_otherButtons = std::make_unique<OtherButtonItem>(); }
        return m_otherButtons.get();
    }
    FilterButtonItem* filterButtons()
    {
        if (!m_filterButtons) { m_filterButtons = std::make_unique<FilterButtonItem>(); }
        return m_filterButtons.get();
    }
    AntennaButtonItem* antennaButtons()
    {
        if (!m_antennaButtons) { m_antennaButtons = std::make_unique<AntennaButtonItem>(); }
        return m_antennaButtons.get();
    }
    BandButtonItem* bandButtons()
    {
        if (!m_bandButtons) { m_bandButtons = std::make_unique<BandButtonItem>(); }
        return m_bandButtons.get();
    }
    // The pan's filter policy dialog for chain 0, on the window's model.
    FilterPolicyDialog* filterPolicy()
    {
        if (!m_filterPolicy && window() != nullptr) {
            m_filterPolicy = std::make_unique<FilterPolicyDialog>(0, m_window->radioModel());
        }
        return m_filterPolicy.get();
    }

private:
    GuiSessionCoordinator& m_sessions;
    bool m_remote{false};
    MainWindow* m_window{nullptr};
    SetupDialog* m_setup{nullptr};
    SpotHubDialog* m_spotHub{nullptr};
    NetworkDiagnosticsDialog* m_netDiag{nullptr};
    std::unique_ptr<PhoneCwApplet> m_phoneCw;
    std::unique_ptr<Rf2ksApplet> m_rfKit;
    std::unique_ptr<SpectrumWidget> m_flagHost;
    VfoWidget* m_flag{nullptr};
    std::unique_ptr<ContainerWidget> m_container;
    MeterWidget* m_meter{nullptr};
    std::unique_ptr<ContainerSettingsDialog> m_containerDialog;
    std::unique_ptr<OtherButtonItem> m_otherButtons;
    std::unique_ptr<FilterButtonItem> m_filterButtons;
    std::unique_ptr<AntennaButtonItem> m_antennaButtons;
    std::unique_ptr<BandButtonItem> m_bandButtons;
    std::unique_ptr<FilterPolicyDialog> m_filterPolicy;
};

// The hosts a surface lives on. The per-feature pass builds only the hosts
// its feature's surfaces need, and checks every surface on those hosts.
enum class Host { Window, Setup, SpotHub, NetDiag, Applets, Flag, Container, FilterPolicy };

struct Surface {
    QString name;
    Host host;
    std::function<bool(Hosts&)> shown;
};

// One entry per feature on the list: every surface that fronts it.
QMap<F, QList<Surface>> surfaces()
{
    const auto menu = [](const QString& text) {
        return Surface{QStringLiteral("menu ") + text, Host::Window,
                       [text](Hosts& h) { return h.window() && actionShown(h.window(), text); }};
    };
    const auto status = [](const QString& name) {
        return Surface{QStringLiteral("status bar ") + name, Host::Window, [name](Hosts& h) {
                           return h.window() && namedShown<QWidget>(h.window(), name);
                       }};
    };
    const auto setupPage = [](const QString& label) {
        return Surface{QStringLiteral("Setup page ") + label, Host::Setup,
                       [label](Hosts& h) { return h.pageRegistered(label); }};
    };
    const auto onPage = [](const QString& label, const QString& what,
                           std::function<bool(QWidget*)> check) {
        return Surface{QStringLiteral("Setup ") + label + QStringLiteral(": ") + what, Host::Setup,
                       [label, check](Hosts& h) {
                           QWidget* page = h.page(label);
                           return page != nullptr && check(page);
                       }};
    };
    const auto named = [](const QString& name) {
        return [name](QWidget* root) { return namedShown<QWidget>(root, name); };
    };
    const auto text = [](const QString& t) {
        return [t](QWidget* root) { return textShown(root, t); };
    };
    const auto tab = [](const QString& t) {
        return [t](QWidget* root) { return tabShown(root, t); };
    };
    const auto spot = [](const QString& name) {
        return Surface{QStringLiteral("Spot Hub ") + name, Host::SpotHub, [name](Hosts& h) {
                           return namedShown<QWidget>(h.spotHub(), name);
                       }};
    };
    // R3 Task 3: one container function button, by id.
    const auto functionButton = [](const QString& caption, OtherButtonItem::ButtonId id) {
        return Surface{QStringLiteral("container button ") + caption, Host::Container,
                       [id](Hosts& h) { return h.otherButtons()->isButtonShown(id); }};
    };
    const auto boxButton = [](const QString& what,
                              std::function<ButtonBoxItem*(Hosts&)> box, int index) {
        return Surface{QStringLiteral("container ") + what, Host::Container,
                       [box, index](Hosts& h) { return box(h)->isButtonShown(index); }};
    };
    const auto applet = [](const QString& what, std::function<bool(Hosts&)> check) {
        return Surface{what, Host::Applets, std::move(check)};
    };
    const auto phoneButton = [](const QString& t) {
        return [t](Hosts& h) { return textShown(h.phoneCw(), t); };
    };
    // showPage() lands on the page only when it is built.
    const auto phonePage = [](int index) {
        return [index](Hosts& h) {
            PhoneCwApplet* applet = h.phoneCw();
            auto* stack = applet ? applet->findChild<QStackedWidget*>() : nullptr;
            if (stack == nullptr) { return false; }
            applet->showPage(index);
            const bool landed = stack->currentIndex() == index;
            applet->showPage(0);
            return landed;
        };
    };

    QMap<F, QList<Surface>> map;
    using B = OtherButtonItem::ButtonId;
    const auto filters = [](Hosts& h) -> ButtonBoxItem* { return h.filterButtons(); };
    const auto antennas = [](Hosts& h) -> ButtonBoxItem* { return h.antennaButtons(); };
    const auto bands = [](Hosts& h) -> ButtonBoxItem* { return h.bandButtons(); };
    map[F::DisplayMode] = {menu(QStringLiteral("&Display Mode")),
                           functionButton(QStringLiteral("SPEC"), B::Spectrum),
                           functionButton(QStringLiteral("PAN"), B::Panadapter),
                           functionButton(QStringLiteral("SCP"), B::Scope),
                           functionButton(QStringLiteral("SCP2"), B::Scope2),
                           functionButton(QStringLiteral("PHS"), B::Phase),
                           functionButton(QStringLiteral("WF"), B::Waterfall),
                           functionButton(QStringLiteral("HIST"), B::Histogram),
                           functionButton(QStringLiteral("PANF"), B::Panafall),
                           functionButton(QStringLiteral("PANS"), B::Panascope),
                           functionButton(QStringLiteral("SPCS"), B::Spectrascope),
                           functionButton(QStringLiteral("OFF"), B::DisplayOff)};
    map[F::UiScale] = {menu(QStringLiteral("&UI Scale")), setupPage(QStringLiteral("UI Scale & Theme"))};
    map[F::MinimalMode] = {menu(QStringLiteral("&Minimal Mode")),
                           setupPage(QStringLiteral("Collapsible Display"))};
    map[F::Keyboard] = {
        menu(QStringLiteral("&Keyboard Shortcuts...")), setupPage(QStringLiteral("Shortcuts")),
        Surface{QStringLiteral("Setup category Keyboard"), Host::Setup,
                [](Hosts& h) { return h.categoryShown(QStringLiteral("Keyboard")); }}};
    map[F::Equalizer] = {menu(QStringLiteral("&Equalizer..."))};
    map[F::Transverters] = {
        boxButton(QStringLiteral("antenna XVTR"), antennas, 5),
        boxButton(QStringLiteral("band XVTR"), bands, uiIndexFromBand(Band::XVTR)),
        menu(QStringLiteral("Trans&verters…")), menu(QStringLiteral("&VHF")),
        onPage(QStringLiteral("Hardware Config"), QStringLiteral("XVTR tab"), tab(QStringLiteral("XVTR"))),
        onPage(QStringLiteral("Hardware Config"), QStringLiteral("OC VHF tab"), tab(QStringLiteral("VHF")))};
    map[F::BandStack] = {menu(QStringLiteral("Band &Stacking...")),
                         status(QStringLiteral("statusBandStackDots"))};
    map[F::Cwx] = {
        menu(QStringLiteral("C&WX...")), status(QStringLiteral("statusCwxLabel")),
        applet(QStringLiteral("Phone/CW CW tab"), phoneButton(QStringLiteral("CW"))),
        applet(QStringLiteral("Phone/CW CW page"), phonePage(1)),
        onPage(QStringLiteral("CW"), QStringLiteral("keyer group"), named(QStringLiteral("cwKeyerGroup"))),
        onPage(QStringLiteral("CW"), QStringLiteral("timing group"), named(QStringLiteral("cwTimingGroup")))};
    map[F::Memories] = {menu(QStringLiteral("&Memory Manager...")),
                        spot(QStringLiteral("displayMemoriesToggle"))};
    map[F::Cat] = {menu(QStringLiteral("&CAT Control...")), setupPage(QStringLiteral("Serial Ports")),
                   setupPage(QStringLiteral("TCP/IP CAT")), status(QStringLiteral("statusCatIndicator"))};
    map[F::Midi] = {menu(QStringLiteral("&MIDI Mapping...")), setupPage(QStringLiteral("MIDI Control"))};
    map[F::Help] = {menu(QStringLiteral("&Getting Started")), menu(QStringLiteral("&NereusSDR Help")),
                    menu(QStringLiteral("Understanding &Data Modes"))};
    map[F::Acc] = {
        applet(QStringLiteral("Phone/CW +ACC"), phoneButton(QStringLiteral("+ACC"))),
        applet(QStringLiteral("Phone/CW microphone source ACC"), [](Hosts& h) {
            PhoneCwApplet* applet = h.phoneCw();
            for (QComboBox* combo : applet ? applet->findChildren<QComboBox*>() : QList<QComboBox*>{}) {
                if (combo->accessibleName() != QStringLiteral("Microphone source")) { continue; }
                auto* list = qobject_cast<QListView*>(combo->view());
                return list != nullptr
                    && !list->isRowHidden(static_cast<int>(PhoneCwApplet::MicInput::Accessory));
            }
            return false;
        })};
    map[F::PhoneMon] = {applet(QStringLiteral("Phone/CW MON"), phoneButton(QStringLiteral("MON"))),
                        applet(QStringLiteral("Phone/CW monitor level"), [](Hosts& h) {
                            for (QSlider* s : h.phoneCw()->findChildren<QSlider*>()) {
                                if (s->accessibleName() == QStringLiteral("Monitor level")) {
                                    return shownWithin(s, h.phoneCw());
                                }
                            }
                            return false;
                        })};
    map[F::FmPage] = {applet(QStringLiteral("Phone/CW FM page"), phonePage(2))};
    // Fix wave I1: the macro buttons, AVG and the two-receiver layout each
    // front a feature of their own.
    map[F::MacroButtons] = {
        Surface{QStringLiteral("container macro buttons"), Host::Container, [](Hosts& h) {
                    // Macro buttons follow the 34 core buttons; none is saved
                    // visible by default, so "not hidden until built" is the
                    // surface here.
                    OtherButtonItem* item = h.otherButtons();
                    for (int i = 34; i < item->buttonCount(); ++i) {
                        if (item->button(i).hiddenUntilBuilt) { return false; }
                    }
                    return item->buttonCount() > 34;
                }}};
    map[F::DisplayAveraging] = {functionButton(QStringLiteral("AVG"), B::Avg)};
    map[F::TwoReceiverLayout] = {
        functionButton(QStringLiteral("RX2"), B::Rx2),
        functionButton(QStringLiteral("SUB"), B::SubRx),
        functionButton(QStringLiteral("SWAP"), B::PanSwap)};
    // Task 4 carried finding: Var1, Var2 and Rx/Tx front features of their
    // own, so marking the macro buttons built does not show them.
    map[F::VariableFilters] = {boxButton(QStringLiteral("filter Var1"), filters, 10),
                               boxButton(QStringLiteral("filter Var2"), filters, 11)};
    map[F::AntennaRxTxSplit] = {boxButton(QStringLiteral("antenna Rx/Tx"), antennas, 9)};
    map[F::RfkitTune] = {
        applet(QStringLiteral("RF-Kit TUNE"),
               [](Hosts& h) { return textShown(h.rfKit(), QStringLiteral("TUNE")); }),
        applet(QStringLiteral("RF-Kit BYPASS"),
               [](Hosts& h) { return textShown(h.rfKit(), QStringLiteral("BYPASS")); })};
    map[F::Voice] = {
        functionButton(QStringLiteral("PLAY"), B::Play),
        functionButton(QStringLiteral("REC"), B::Rec),
        status(QStringLiteral("statusDvkLabel")),
        Surface{QStringLiteral("slice flag record"), Host::Flag, [](Hosts& h) {
                    QPushButton* b = h.flag()->recordButtonForTest();
                    return b != nullptr && !b->isHidden();
                }},
        Surface{QStringLiteral("slice flag play"), Host::Flag, [](Hosts& h) {
                    QPushButton* b = h.flag()->playButtonForTest();
                    return b != nullptr && !b->isHidden();
                }},
        Surface{QStringLiteral("container Add > Voice Rec/Play"), Host::Container, [](Hosts& h) {
                    return actionShown(h.containerDialog(), QStringLiteral("Voice Rec/Play"));
                }},
        Surface{QStringLiteral("container item list Voice Rec/Play"), Host::Container, [](Hosts& h) {
                    for (QListWidget* list : h.containerDialog()->findChildren<QListWidget*>()) {
                        for (int i = 0; i < list->count(); ++i) {
                            if (list->item(i)->text().startsWith(QStringLiteral("Voice Rec/Play"))
                                && !list->item(i)->isHidden()) {
                                return true;
                            }
                        }
                    }
                    return false;
                }},
        Surface{QStringLiteral("container draws Voice Rec/Play"), Host::Container, [](Hosts& h) {
                    MeterWidget* meter = h.containerMeter();
                    for (MeterItem* item : meter->items()) {
                        if (qobject_cast<VoiceRecordPlayItem*>(item) && meter->shouldRender(item)) {
                            return true;
                        }
                    }
                    return false;
                }}};
    // Parity Task 31: the container DUP button is display duplex, built;
    // FDX (full duplex) is the status bar label only.
    map[F::Fdx] = {status(QStringLiteral("statusFdxLabel"))};
    map[F::Navigation] = {setupPage(QStringLiteral("Navigation"))};
    map[F::Sam] = {onPage(QStringLiteral("AM/SAM"), QStringLiteral("SAM group"), named(QStringLiteral("samGroup")))};
    map[F::Skins] = {setupPage(QStringLiteral("Skins"))};
    map[F::TxProfilesLeaf] = {setupPage(QStringLiteral("TX Profiles"))};
    map[F::BandwidthMonitor] = {onPage(QStringLiteral("Hardware Config"), QStringLiteral("Bandwidth Monitor tab"),
                                       tab(QStringLiteral("Bandwidth Monitor")))};
    map[F::Hl2SecondI2cBus] = {onPage(QStringLiteral("Hardware Config"), QStringLiteral("I2C bus 0"),
                                      named(QStringLiteral("hl2I2cBus0")))};
    map[F::ConnectionHistory] = {onPage(QStringLiteral("Connection Quality"), QStringLiteral("60 s history"),
                                        named(QStringLiteral("connectionHistoryGroup")))};
    map[F::Logging] = {
        onPage(QStringLiteral("Logging & Performance"), QStringLiteral("log group"), named(QStringLiteral("diagLogGroup"))),
        onPage(QStringLiteral("Logging & Performance"), QStringLiteral("categories"),
               named(QStringLiteral("diagCategoriesGroup")))};
    map[F::SignalGenerator] = {setupPage(QStringLiteral("Signal Generator")),
                               setupPage(QStringLiteral("Hardware Tests"))};
    map[F::DspRate] = {onPage(QStringLiteral("Advanced"), QStringLiteral("DSP group"),
                              named(QStringLiteral("audioAdvancedDspGroup")))};
    map[F::IqToVax] = {onPage(QStringLiteral("Advanced"), QStringLiteral("Send IQ to VAX"), text(QStringLiteral("Send IQ to VAX"))),
                       onPage(QStringLiteral("Advanced"), QStringLiteral("TX Monitor to VAX"),
                              text(QStringLiteral("TX Monitor to VAX")))};
    map[F::MuteVaxDuringTx] = {onPage(QStringLiteral("Advanced"), QStringLiteral("Mute VAX during TX"),
                                      text(QStringLiteral("Mute VAX during TX on other slice")))};
    map[F::AntennaConflict] = {onPage(QStringLiteral("Hardware Config"), QStringLiteral("conflict policy"),
                                      named(QStringLiteral("antennaConflictPolicyGroup")))};
    map[F::OcExtras] = {
        functionButton(QStringLiteral("XPA"), B::Xpa),
        onPage(QStringLiteral("Hardware Config"), QStringLiteral("hot switching"), named(QStringLiteral("ocAllowHotSwitching"))),
        onPage(QStringLiteral("Hardware Config"), QStringLiteral("USB BCD"), named(QStringLiteral("ocUsbBcdGroup"))),
        onPage(QStringLiteral("Hardware Config"), QStringLiteral("external PA"), named(QStringLiteral("ocExternalPaGroup")))};
    map[F::MultimeterHolds] = {
        onPage(QStringLiteral("Multimeter"), QStringLiteral("peak hold"), text(QStringLiteral("Peak hold time:"))),
        onPage(QStringLiteral("Multimeter"), QStringLiteral("text hold"), text(QStringLiteral("Text hold time:"))),
        onPage(QStringLiteral("Multimeter"), QStringLiteral("digital delay"), text(QStringLiteral("Digital delay:"))),
        onPage(QStringLiteral("Multimeter"), QStringLiteral("history enable"),
               text(QStringLiteral("Enable signal history graph")))};
    map[F::WsjtxFilters] = {spot(QStringLiteral("wsjtxFilterCQ")), spot(QStringLiteral("wsjtxFilterPOTA")),
                            spot(QStringLiteral("wsjtxFilterCallingMe"))};
    map[F::RbnRateLimit] = {spot(QStringLiteral("rbnRateSpin")),
                            Surface{QStringLiteral("Spot Hub RBN Rate Limit label"), Host::SpotHub,
                                    [](Hosts& h) { return textShown(h.spotHub(), QStringLiteral("Rate Limit:")); }}};
    map[F::FreeDvToPsk] = {spot(QStringLiteral("freedvReportToPskChk"))};
    map[F::SmallFilter] = {onPage(QStringLiteral("Meter Styles"), QStringLiteral("small filter display"),
                                  named(QStringLiteral("appearanceVfoFlagGroup")))};
    map[F::ApfParams] = {onPage(QStringLiteral("CW"), QStringLiteral("APF bandwidth"), text(QStringLiteral("Bandwidth"))),
                         onPage(QStringLiteral("CW"), QStringLiteral("APF gain"), text(QStringLiteral("Gain")))};
    map[F::AmSquelchTail] = {onPage(QStringLiteral("AM/SAM"), QStringLiteral("max tail"), text(QStringLiteral("Max Tail")))};
    map[F::FmDeviation] = {onPage(QStringLiteral("FM"), QStringLiteral("deviation"),
                                  named(QStringLiteral("fmRxDeviationCombo"))),
                           onPage(QStringLiteral("FM"), QStringLiteral("de-emphasis"),
                                  named(QStringLiteral("fmDeEmphasisButton")))};
    // Fix wave I3: the review's unfinished controls (plan rows fm-tx,
    // fm-repeater and ddc-routing; export-radio is built since 2026-09-28).
    const auto flagButton = [](const QString& name) {
        // The FM controls sit on the flag's FM mode page, not shown in USB:
        // the control's own hidden flag is the surface.
        return Surface{QStringLiteral("slice flag ") + name, Host::Flag, [name](Hosts& h) {
                           const auto* b = h.flag()->findChild<QWidget*>(name);
                           return b != nullptr && !b->isHidden();
                       }};
    };
    map[F::FmTransmit] = {onPage(QStringLiteral("FM"), QStringLiteral("FM transmit group"),
                                 named(QStringLiteral("fmTxGroup"))),
                          flagButton(QStringLiteral("txLowBtn")),
                          flagButton(QStringLiteral("simplexBtn")),
                          flagButton(QStringLiteral("txHighBtn")),
                          // Plan row fm-flag: the transmit shift and Rev.
                          flagButton(QStringLiteral("fmOffsetLabel")),
                          flagButton(QStringLiteral("offsetKhzSpin")),
                          flagButton(QStringLiteral("revBtn"))};
    // Plan row fm-flag: the CTCSS tone choices (no tone encoder or detector).
    map[F::FmTones] = {flagButton(QStringLiteral("toneModeCmb")),
                       flagButton(QStringLiteral("toneValueCmb"))};
    map[F::DdcRouting] = {setupPage(QStringLiteral("DDC Routing"))};
    // Plan rows hpf-bcast and freq-cal (added 2026-09-24): nothing reads the
    // one or handles the other.
    map[F::HpfBroadcastReject] = {
        Surface{QStringLiteral("filter policy HPF checkbox"), Host::FilterPolicy, [](Hosts& h) {
                    FilterPolicyDialog* d = h.filterPolicy();
                    const auto* b = d ? d->findChild<QCheckBox*>(QStringLiteral("filterPolicyHpfCheck"))
                                      : nullptr;
                    return b != nullptr && !b->isHidden();
                }}};
    map[F::FrequencyCalibration] = {onPage(QStringLiteral("Hardware Config"),
                                           QStringLiteral("frequency calibration Start"),
                                           named(QStringLiteral("freqCalStartButton")))};
    map[F::GanymedeTrip] = {status(QStringLiteral("paStatusBadge"))};
    map[F::PbSnr] = {Surface{QStringLiteral("container PB SNR render"), Host::Container,
                            [](Hosts& h) {
                                for (const MeterItem* item : h.containerMeter()->items()) {
                                    if (item->bindingId() == MeterBinding::PbSnr) {
                                        return h.containerMeter()->shouldRender(item);
                                    }
                                }
                                return false;
                            }}};
    map[F::ContainerClickBox] = {
        Surface{QStringLiteral("container Click Box render"), Host::Container,
                [](Hosts& h) {
                    for (const MeterItem* item : h.containerMeter()->items()) {
                        if (qobject_cast<const ClickBoxItem*>(item)) {
                            return h.containerMeter()->shouldRender(item);
                        }
                    }
                    return false;
                }},
        Surface{QStringLiteral("container Click Box Add"), Host::Container,
                [](Hosts& h) { return actionShown(h.containerDialog(), QStringLiteral("Click Box")); }}};
    map[F::AudioBitDepth] = {onPage(QStringLiteral("Devices"), QStringLiteral("bit depth"),
                                   text(QStringLiteral("Bit depth:")))};
    map[F::AudioAutoMatch] = {onPage(QStringLiteral("Devices"), QStringLiteral("auto match"),
                                    text(QStringLiteral("Auto-match")))};
    map[F::AudioMonitorTxInput] = {onPage(QStringLiteral("Devices"), QStringLiteral("monitor TX input"),
                                         text(QStringLiteral("Monitor TX input during transmit")))};
    map[F::AudioToneCheck] = {onPage(QStringLiteral("Devices"), QStringLiteral("tone check"),
                                    text(QStringLiteral("Enable tone check (A-440 Hz burst on PTT)")))};
    map[F::WaterfallLowColor] = {onPage(QStringLiteral("Colors & Theme"), QStringLiteral("low color"),
                                      text(QStringLiteral("Low Level Color:")))};
    map[F::MultimeterAveraging] = {onPage(QStringLiteral("Multimeter"), QStringLiteral("averaging"),
                                         text(QStringLiteral("Averaging window:")))};
    map[F::TxGridScale] = {onPage(QStringLiteral("TX Display"), QStringLiteral("grid scale"),
                                 [](QWidget* p) { return groupShown(p, QStringLiteral("TX Grid Scale")); })};
    return map;
}

// "surface: shown" lines for every surface of `feature` that is shown.
QStringList shownSurfaces(const QList<Surface>& list, Hosts& hosts,
                          const QSet<Host>* onlyHosts = nullptr)
{
    QStringList shown;
    for (const Surface& s : list) {
        if (onlyHosts != nullptr && !onlyHosts->contains(s.host)) { continue; }
        if (s.shown(hosts)) { shown << s.name; }
    }
    return shown;
}

} // namespace

class TstUnbuiltFeatures : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Windows save their settings; this run keeps a file of its own.
        AppSettings::setProfileOverride(QStringLiteral("unbuilt-features-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        UnbuiltFeatures::resetForTest();
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        // No VAX first-run dialog and no discovery broadcast onto the LAN
        // from the local windows.
        AppSettings::instance().setValue(QStringLiteral("audio/FirstRunComplete"),
                                         QStringLiteral("True"));
        AppSettings::instance().ensureSettingsAtVersion(6);
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
        BuildIdentity::setBuildTag(QString());
    }

    void cleanup()
    {
        UnbuiltFeatures::resetForTest();
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // The list: named entries, none built, every one with surface checks
    // here. A feature added to the list without its surfaces fails this.
    void everyListedFeatureHasItsSurfacesChecked()
    {
        const QList<UnbuiltFeatures::Entry>& list = UnbuiltFeatures::all();
        QVERIFY(!list.isEmpty());
        const QMap<F, QList<Surface>> map = surfaces();
        QSet<QString> keys;
        for (const UnbuiltFeatures::Entry& entry : list) {
            QVERIFY2(!entry.key.isEmpty(), qPrintable(entry.description));
            QVERIFY2(!keys.contains(entry.key), qPrintable(entry.key));
            keys.insert(entry.key);
            QCOMPARE(UnbuiltFeatures::key(entry.feature), entry.key);
            QVERIFY2(!UnbuiltFeatures::isBuilt(entry.feature), qPrintable(entry.key));
            QVERIFY2(map.contains(entry.feature) && !map.value(entry.feature).isEmpty(),
                     qPrintable(QStringLiteral("%1 has no surface checks").arg(entry.key)));
        }
        QCOMPARE(map.size(), list.size());
    }

    void builtFilterDisplayIsVisibleInLocalAndRemoteContainers()
    {
        QVERIFY(UnbuiltFeatures::isBuilt(F::ContainerFilterDisplay));
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            Hosts hosts(sessions, remote);
            bool renders = false;
            for (const MeterItem* item : hosts.containerMeter()->items()) {
                if (qobject_cast<const FilterDisplayItem*>(item)) {
                    renders = hosts.containerMeter()->shouldRender(item);
                }
            }
            QVERIFY(renders);
            QVERIFY(actionShown(hosts.containerDialog(), QStringLiteral("Filter Display")));
        }
    }

    // The stream channels setting is built (codex/tci-settings-real): the
    // Channels control on Setup > Audio > TCI is shown and usable, in a local
    // window and a remote one.
    void builtTciStreamChannelsControlIsUsable()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            Hosts hosts(sessions, remote);
            QWidget* page = hosts.page(QStringLiteral("TCI"));
            QVERIFY2(page != nullptr, remote ? "remote" : "local");
            QVERIFY2(usableShown(page, QStringLiteral("tciStreamChannelsCombo")),
                     remote ? "remote" : "local");
        }
        QVERIFY(sessions.replace({}, false));
    }

    // The three RX2 VFO options are built (codex/tci-rx2-quirks): shown and
    // usable on Setup > Network > TCI Server in a local window and a remote
    // one, with Forget usable only while Duplicate is on, as in Thetis.
    void builtTciRx2VfoOptionsAreUsable()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            const char* where = remote ? "remote" : "local";
            Hosts hosts(sessions, remote);
            QWidget* page = hosts.page(QStringLiteral("TCI Server"));
            QVERIFY2(page != nullptr, where);
            auto* copy = page->findChild<QCheckBox*>(QStringLiteral("tciCopyRx2VfobToVfoaCheck"));
            QVERIFY2(copy != nullptr, where);
            copy->setChecked(true);
            for (const char* name : {"tciForgetRx2VfoBCheck", "tciUseRx1VfoaForRx2VfoaCheck",
                                     "tciCopyRx2VfobToVfoaCheck"}) {
                QVERIFY2(usableShown(page, QLatin1String(name)), where);
            }
            copy->setChecked(false);
            QVERIFY2(!usableShown(page, QStringLiteral("tciForgetRx2VfoBCheck")), where);
            QVERIFY2(usableShown(page, QStringLiteral("tciUseRx1VfoaForRx2VfoaCheck")), where);
            copy->setChecked(true);
            QVERIFY2(usableShown(page, QStringLiteral("tciForgetRx2VfoBCheck")), where);
        }
        QVERIFY(sessions.replace({}, false));
    }

    // Nothing on the list shows, in a local window or a remote one.
    void noUnbuiltSurfaceShowsInLocalOrRemoteWindows()
    {
        const QMap<F, QList<Surface>> map = surfaces();
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            Hosts hosts(sessions, remote);
            QVERIFY(hosts.window() != nullptr);
            QCOMPARE(hosts.window()->radioModel()->ownsLocalDsp(), !remote);
            QStringList shown;
            for (const UnbuiltFeatures::Entry& entry : UnbuiltFeatures::all()) {
                for (const QString& s : shownSurfaces(map.value(entry.feature), hosts)) {
                    shown << entry.key + QStringLiteral(": ") + s;
                }
            }
            QVERIFY2(shown.isEmpty(),
                     qPrintable((remote ? QStringLiteral("remote: ") : QStringLiteral("local: "))
                                + shown.join(QStringLiteral("; "))));
        }
        QVERIFY(sessions.replace({}, false));
    }

    // Marking one feature built brings every one of its surfaces back and
    // no other feature's. One row per listed feature (R-R3-49 load round):
    // main() spreads the rows over ctest entries from the same list.
    void markingOneFeatureBuiltShowsItsSurfacesOnly_data()
    {
        QTest::addColumn<int>("index");
        const QList<UnbuiltFeatures::Entry>& list = UnbuiltFeatures::all();
        for (int i = 0; i < list.size(); ++i) {
            QTest::newRow(qPrintable(list.at(i).key)) << i;
        }
    }

    void markingOneFeatureBuiltShowsItsSurfacesOnly()
    {
        QFETCH(int, index);
        const UnbuiltFeatures::Entry& entry = UnbuiltFeatures::all().at(index);
        const QMap<F, QList<Surface>> map = surfaces();
        GuiSessionCoordinator sessions;
        {
            UnbuiltFeatures::resetForTest();
            UnbuiltFeatures::setBuiltForTest(entry.feature, true);
            Hosts hosts(sessions, false);
            QSet<Host> used;
            for (const Surface& s : map.value(entry.feature)) { used.insert(s.host); }

            QStringList missing;
            for (const Surface& s : map.value(entry.feature)) {
                if (!s.shown(hosts)) { missing << s.name; }
            }
            QVERIFY2(missing.isEmpty(),
                     qPrintable(entry.key + QStringLiteral(" built, not shown: ")
                                + missing.join(QStringLiteral("; "))));

            QStringList others;
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                if (it.key() == entry.feature) { continue; }
                for (const QString& s : shownSurfaces(it.value(), hosts, &used)) {
                    others << UnbuiltFeatures::key(it.key()) + QStringLiteral(": ") + s;
                }
            }
            QVERIFY2(others.isEmpty(),
                     qPrintable(entry.key + QStringLiteral(" built, also shown: ")
                                + others.join(QStringLiteral("; "))));
        }
        UnbuiltFeatures::resetForTest();
        QVERIFY(sessions.replace({}, false));
    }

    // Parity Task 31 (A11): View > Display duplex (DUP) in both windows,
    // off by default; a window running its own DSP changes the saved
    // setting, a remote window whose Core is below txDisplayVersion 3 shows
    // it disabled with the reason.
    void theDisplayDuplexMenuItemIsInBothWindows()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            Hosts hosts(sessions, remote);
            QAction* action =
                hosts.window()->findChild<QAction*>(QStringLiteral("actionDisplayDuplex"));
            QVERIFY(action != nullptr);
            QVERIFY(action->isVisible());
            QVERIFY(action->isCheckable());
            QCOMPARE(action->text(), QStringLiteral("Display duplex (DUP)"));
            QVERIFY(!action->isChecked());
            QVERIFY(OperatorWording::isPlain(action->toolTip()));
            if (remote) {
                QVERIFY(!action->isEnabled());
                QCOMPARE(action->toolTip(),
                         QStringLiteral("This Core does not show the receiver while "
                                        "transmitting for this app. Updating the Core may "
                                        "help."));
            } else {
                QVERIFY(action->isEnabled());
                action->trigger();
                QVERIFY(action->isChecked());
                QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayDuplex")).toString(),
                         QStringLiteral("True"));
                action->trigger();
                QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayDuplex")).toString(),
                         QStringLiteral("False"));
            }
        }
        QVERIFY(sessions.replace({}, false));
    }

    // Hidden is not removed: the values saved for hidden controls come back
    // unchanged after a start (every Setup page and the Spot Hub built) and
    // a save.
    void savedValuesOfHiddenControlsSurviveAStartAndASave()
    {
        const QMap<QString, QString> seeded = {
            {QStringLiteral("TciCwBecomesCwuAbove10mhz"), QStringLiteral("True")},
            {QStringLiteral("TciTxChannel"), QStringLiteral("Left")},
            {QStringLiteral("TciRxSensorIntervalMs"), QStringLiteral("450")},
            {QStringLiteral("TciTxSensorIntervalMs"), QStringLiteral("550")},
            {QStringLiteral("TciForgetRx2VfoBOnDisconnect"), QStringLiteral("True")},
            {QStringLiteral("TciUseRx1VfoaForRx2Vfoa"), QStringLiteral("True")},
            {QStringLiteral("TciCopyRx2VfobToVfoa"), QStringLiteral("True")},
            {QStringLiteral("TciAudioStreamChannels"), QStringLiteral("1")},
            {QStringLiteral("audio/DspRate"), QStringLiteral("96000")},
            {QStringLiteral("audio/DspBlockSize"), QStringLiteral("256")},
            {QStringLiteral("audio/SendIqToVax"), QStringLiteral("True")},
            {QStringLiteral("audio/TxMonitorToVax"), QStringLiteral("True")},
            {QStringLiteral("audio/MuteVaxDuringTxOnOtherSlice"), QStringLiteral("True")},
            {QStringLiteral("MultimeterPeakHoldMs"), QStringLiteral("900")},
            {QStringLiteral("MultimeterTextHoldMs"), QStringLiteral("800")},
            {QStringLiteral("MultimeterDigitalDelayMs"), QStringLiteral("300")},
            {QStringLiteral("MultimeterSignalHistoryEnabled"), QStringLiteral("True")},
            {QStringLiteral("Antenna_ConflictPolicy"), QStringLiteral("2")},
            {QStringLiteral("AppearanceSmallModeFilterOnVfos"), QStringLiteral("True")},
            {QStringLiteral("IsMemorySpotsEnabled"), QStringLiteral("True")},
            {QStringLiteral("WsjtxFilterCQ"), QStringLiteral("False")},
            {QStringLiteral("WsjtxFilterPOTA"), QStringLiteral("False")},
            {QStringLiteral("WsjtxFilterCallingMe"), QStringLiteral("False")},
            {QStringLiteral("RbnRateLimit"), QStringLiteral("33")},
            {QStringLiteral("FreeDvReporter/ReportToPsk"), QStringLiteral("True")},
        };
        auto& settings = AppSettings::instance();
        for (auto it = seeded.constBegin(); it != seeded.constEnd(); ++it) {
            settings.setValue(it.key(), it.value());
        }
        QVERIFY(settings.save());

        {
            GuiSessionCoordinator sessions;
            Hosts hosts(sessions, false);
            QVERIFY(hosts.window() != nullptr);
            SetupDialog* setup = hosts.setup();
            QVERIFY(setup != nullptr);
            setup->realizeAllPagesForTest();
            QVERIFY(hosts.spotHub() != nullptr);
            QCoreApplication::processEvents();
            QVERIFY(settings.save());
            QVERIFY(sessions.replace({}, false));
        }
        QVERIFY(settings.save());

        AppSettings reread(settings.filePath());
        reread.load();
        for (auto it = seeded.constBegin(); it != seeded.constEnd(); ++it) {
            QCOMPARE(reread.value(it.key()).toString(), it.value());
        }
    }

    // A saved container holding a Voice Rec/Play control loads, keeps it
    // without drawing or listing it, offers no new one, and saves it back.
    void savedVoiceControlLoadsHiddenAndSavesBack()
    {
        const auto clearContainers = [] {
            auto& s = AppSettings::instance();
            for (const QString& k : s.allKeys()) {
                if (k.startsWith(QStringLiteral("Container"))) { s.remove(k); }
            }
        };
        clearContainers();
        const auto cleanupKeys = qScopeGuard(clearContainers);

        const auto voiceCount = [](MeterWidget* meter) {
            int n = 0;
            for (MeterItem* item : meter->items()) {
                if (qobject_cast<VoiceRecordPlayItem*>(item)) { ++n; }
            }
            return n;
        };

        QWidget dockParent;
        QSplitter splitter;
        QString savedId;
        {
            ContainerManager mgr(&dockParent, &splitter);
            ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
            QVERIFY(c);
            savedId = c->id();
            auto* meter = new MeterWidget();
            c->setContent(meter);
            meter->addItem(new TextItem());
            meter->addItem(new VoiceRecordPlayItem());
            mgr.saveState();
        }

        for (int pass = 0; pass < 2; ++pass) {
            ContainerManager mgr(&dockParent, &splitter);
            mgr.restoreState();
            ContainerWidget* c = mgr.container(savedId);
            QVERIFY(c != nullptr);
            auto* meter = qobject_cast<MeterWidget*>(c->content());
            QVERIFY(meter != nullptr);
            QCOMPARE(meter->items().size(), 2);
            QCOMPARE(voiceCount(meter), 1);
            for (MeterItem* item : meter->items()) {
                QCOMPARE(meter->shouldRender(item), !qobject_cast<VoiceRecordPlayItem*>(item));
            }

            {
                // The settings dialog lists only the text item, and its
                // Add menu offers no Voice Rec/Play. Applying keeps it.
                ContainerSettingsDialog dialog(c, nullptr, &mgr);
                int listed = 0;
                for (QListWidget* list : dialog.findChildren<QListWidget*>()) {
                    for (int i = 0; i < list->count(); ++i) {
                        if (list->item(i)->text().startsWith(QStringLiteral("Voice Rec/Play"))) {
                            QVERIFY(list->item(i)->isHidden());
                            ++listed;
                        }
                    }
                }
                QCOMPARE(listed, 1);
                for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
                    if (button->text() == QStringLiteral("+")) { button->click(); }
                }
                QVERIFY(!actionShown(&dialog, QStringLiteral("Voice Rec/Play")));
                for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
                    if (button->text() == QStringLiteral("Apply")) { button->click(); }
                }
            }
            meter = qobject_cast<MeterWidget*>(c->content());
            QVERIFY(meter != nullptr);
            QCOMPARE(voiceCount(meter), 1);
            QVERIFY(meter->serializeItems().contains(QStringLiteral("VOICERECPLAY")));
            mgr.saveState();
        }
    }

    // R-R3-49 (Task 2): the controls the operator removed are built in no
    // window, local or remote.
    void removedControlsAppearInNoWindow()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            const QString where = remote ? QStringLiteral("remote: ") : QStringLiteral("local: ");
            Hosts hosts(sessions, remote);
            MainWindow* window = hosts.window();
            QVERIFY(window != nullptr);
            QStringList found;
            const auto check = [&found](const QString& what, bool present) {
                if (present) { found << what; }
            };

            // View > Dark Theme, Tools > Macro Buttons.
            check(QStringLiteral("menu Dark Theme"), textBuilt(window, QStringLiteral("&Dark Theme")));
            check(QStringLiteral("menu Macro Buttons"),
                  textBuilt(window, QStringLiteral("Macro &Buttons...")));

            // Pan overlay: RF Gain, WNB, IQ channel (the flyouts are the
            // spectrum's children; search the whole window).
            QVERIFY(!window->findChildren<SpectrumOverlayPanel*>().isEmpty());
            check(QStringLiteral("overlay RF Gain"), textBuilt(window, QStringLiteral("RF Gain:")));
            check(QStringLiteral("overlay WNB"), textBuilt(window, QStringLiteral("WNB")));
            check(QStringLiteral("overlay IQ Ch"), textBuilt(window, QStringLiteral("IQ Ch")));
            check(QStringLiteral("overlay IQ combo"),
                  window->findChild<QComboBox*>(QStringLiteral("vaxIqCombo")) != nullptr);

            // Setup: every page built, then nothing removed anywhere in it.
            SetupDialog* setup = hosts.setup();
            QVERIFY(setup != nullptr);
            setup->realizeAllPagesForTest();
            for (const QString& page : {QStringLiteral("RX2 Display"), QStringLiteral("Gradients")}) {
                check(QStringLiteral("Setup page ") + page, hosts.pageRegistered(page));
            }
            for (const QString& text : {
                     // Startup & Preferences, all but auto-connect, callsign, grid.
                     QStringLiteral("Restore last frequency on connect"),
                     QStringLiteral("Application"),
                     QStringLiteral("Show splash screen at startup"),
                     QStringLiteral("Check for updates on startup"),
                     QStringLiteral("FFTW Wisdom"), QStringLiteral("Regenerate"),
                     QStringLiteral("Process Priority"),
                     // RX2 Display, Gradients.
                     QStringLiteral("RX2 Spectrum"), QStringLiteral("RX2 Waterfall"),
                     QStringLiteral("Waterfall Gradient"),
                     // Hardware: Radio Info RX2 rate, Diversity tab.
                     QStringLiteral("RX2 sample rate (Hz):"), QStringLiteral("Diversity"),
                     // Audio > Advanced VAX feedback tuning.
                     QStringLiteral("VAC Feedback-Loop Tuning"), QStringLiteral("Target VAX Channel"),
                     // Diagnostics Performance checkboxes.
                     QStringLiteral("Performance"),
                     QStringLiteral("Warn when spectrum render delay exceeds threshold"),
                     QStringLiteral("Warn on long pixel-fetch operations"),
                     QStringLiteral("Purge FFT buffers periodically (debugging)"),
                     // Audio > TCI Slice B rate.
                     QStringLiteral("Slice B rate:")}) {
                check(QStringLiteral("Setup ") + text, textBuilt(setup, text));
            }
            // The three Startup & Preferences controls that work stay.
            QVERIFY(hosts.page(QStringLiteral("Startup & Preferences"))
                        ->findChild<QWidget*>(QStringLiteral("startupAutoConnect")) != nullptr);

            // Spot Hub: the automatic background colour option.
            QVERIFY(hosts.spotHub() != nullptr);
            check(QStringLiteral("Spot Hub auto background"),
                  hosts.spotHub()->findChild<QPushButton*>(
                      QStringLiteral("displayOverrideBgAutoToggle")) != nullptr);

            // Containers: no Discord control offered.
            check(QStringLiteral("container Add Discord Buttons"),
                  textBuilt(hosts.containerDialog(), QStringLiteral("Discord Buttons")));

            QVERIFY2(found.isEmpty(), qPrintable(where + found.join(QStringLiteral("; "))));
        }
        QVERIFY(sessions.replace({}, false));
    }

    // Removed is not migrated: what users saved for removed controls stays
    // in the settings file after a start (every Setup page and the Spot Hub
    // built) and a save.
    void savedValuesOfRemovedControlsSurviveAStartAndASave()
    {
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:49");
        const QMap<QString, QString> seeded = {
            {QStringLiteral("audio/VacFeedback/1/Gain"), QStringLiteral("1.2500")},
            {QStringLiteral("audio/VacFeedback/1/SlewTimeMs"), QStringLiteral("9")},
            {QStringLiteral("audio/VacFeedback/2/PropRing"), QStringLiteral("4")},
            {QStringLiteral("audio/VacFeedback/3/FfRing"), QStringLiteral("6")},
            {QStringLiteral("DiagnosticsSpecWarningLedRenderDelay"), QStringLiteral("True")},
            {QStringLiteral("DiagnosticsSpecWarningLedGetPixels"), QStringLiteral("True")},
            {QStringLiteral("DiagnosticsPurgeBuffers"), QStringLiteral("True")},
            {QStringLiteral("IsSpotsOverrideToAutoBackgroundColorEnabled"), QStringLiteral("False")},
            {QStringLiteral("TciSliceB_OutputSampleRate"), QStringLiteral("96000")},
        };
        const QMap<QString, QString> seededHardware = {
            {QStringLiteral("diversity/enabled"), QStringLiteral("true")},
            {QStringLiteral("diversity/referenceAdc"), QStringLiteral("1")},
            {QStringLiteral("diversity/phaseDeg"), QStringLiteral("45")},
            {QStringLiteral("diversity/gainDb"), QStringLiteral("-12")},
        };
        auto& settings = AppSettings::instance();
        for (auto it = seeded.constBegin(); it != seeded.constEnd(); ++it) {
            settings.setValue(it.key(), it.value());
        }
        for (auto it = seededHardware.constBegin(); it != seededHardware.constEnd(); ++it) {
            settings.setHardwareValue(mac, it.key(), it.value());
        }
        QVERIFY(settings.save());

        {
            GuiSessionCoordinator sessions;
            Hosts hosts(sessions, false);
            QVERIFY(hosts.window() != nullptr);
            SetupDialog* setup = hosts.setup();
            QVERIFY(setup != nullptr);
            setup->realizeAllPagesForTest();
            QVERIFY(hosts.spotHub() != nullptr);
            QCoreApplication::processEvents();
            QVERIFY(settings.save());
            QVERIFY(sessions.replace({}, false));
        }
        QVERIFY(settings.save());

        AppSettings reread(settings.filePath());
        reread.load();
        for (auto it = seeded.constBegin(); it != seeded.constEnd(); ++it) {
            QCOMPARE(reread.value(it.key()).toString(), it.value());
        }
        for (auto it = seededHardware.constBegin(); it != seededHardware.constEnd(); ++it) {
            QCOMPARE(reread.hardwareValue(mac, it.key()).toString(), it.value());
        }
    }

    // A saved container holding a Discord control loads without error and
    // without it, logs one line, and the rest of it loads and saves.
    void savedDiscordControlIsDroppedOnLoad()
    {
        const auto clearContainers = [] {
            auto& s = AppSettings::instance();
            for (const QString& k : s.allKeys()) {
                if (k.startsWith(QStringLiteral("Container"))) { s.remove(k); }
            }
        };
        clearContainers();
        const auto cleanupKeys = qScopeGuard(clearContainers);
        const QString discordLine = QStringLiteral("DISCORDBTNS|0|0.5|1|0.2|0|10|3");

        QWidget dockParent;
        QSplitter splitter;
        QString savedId;
        {
            ContainerManager mgr(&dockParent, &splitter);
            ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
            QVERIFY(c);
            savedId = c->id();
            auto* meter = new MeterWidget();
            c->setContent(meter);
            auto* first = new TextItem();
            first->setLabel(QStringLiteral("before"));
            auto* second = new TextItem();
            second->setLabel(QStringLiteral("after"));
            meter->addItem(first);
            meter->addItem(second);
            mgr.saveState();
        }
        // The Discord control sits between the two, as a saved container
        // from before the removal would hold it.
        auto& s = AppSettings::instance();
        const QString key = QStringLiteral("ContainerItems_%1").arg(savedId);
        QStringList lines = s.value(key).toString().split(QLatin1Char('\n'));
        QCOMPARE(lines.size(), 2);
        lines.insert(1, discordLine);
        s.setValue(key, lines.join(QLatin1Char('\n')));

        capturedLog().clear();
        const QtMessageHandler previous = qInstallMessageHandler(captureLog);
        {
            ContainerManager mgr(&dockParent, &splitter);
            mgr.restoreState();
            qInstallMessageHandler(previous);
            ContainerWidget* c = mgr.container(savedId);
            QVERIFY(c != nullptr);
            auto* meter = qobject_cast<MeterWidget*>(c->content());
            QVERIFY(meter != nullptr);
            QCOMPARE(meter->items().size(), 2);
            QVERIFY(!meter->serializeItems().contains(QStringLiteral("DISCORDBTNS")));
            mgr.saveState();
        }
        qInstallMessageHandler(previous);
        const QStringList discordLines = capturedLog().filter(QStringLiteral("Discord"));
        QCOMPARE(discordLines.size(), 1);
        QVERIFY2(discordLines.first().startsWith(QStringLiteral("%1 ").arg(int(QtInfoMsg))),
                 qPrintable(discordLines.first()));
        QVERIFY2(capturedLog().filter(QStringLiteral("%1 ").arg(int(QtCriticalMsg))).isEmpty(),
                 qPrintable(capturedLog().join(QStringLiteral(" | "))));
        const QStringList saved = s.value(key).toString().split(QLatin1Char('\n'));
        QCOMPARE(saved.size(), 2);

        // A meter group holding one drops it the same way.
        capturedLog().clear();
        TextItem groupText;
        const QString group = QStringList{QStringLiteral("GROUP"), QStringLiteral("g"),
                                          QStringLiteral("0"), QStringLiteral("0"),
                                          QStringLiteral("1"), QStringLiteral("1"),
                                          QStringLiteral("2"), discordLine,
                                          groupText.serialize()}
                                  .join(QLatin1Char('\n'));
        qInstallMessageHandler(captureLog);
        std::unique_ptr<ItemGroup> loaded(ItemGroup::deserialize(group));
        qInstallMessageHandler(previous);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->items().size(), 1);
        QCOMPARE(capturedLog().filter(QStringLiteral("Discord")).size(), 1);
    }

    // An emptied Setup page is not offered: Logging & Performance, once its
    // Performance checkboxes went, holds only the logging groups, hidden
    // until logging is built. It is not in the tree, selectPage() does not
    // find it, and no registered page or category is empty, local or remote.
    void emptiedSetupPageIsNotShownOrFound()
    {
        const QString logging = QStringLiteral("Logging & Performance");
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            const QString where = remote ? QStringLiteral("remote: ") : QStringLiteral("local: ");
            Hosts hosts(sessions, remote);
            SetupDialog* setup = hosts.setup();
            QVERIFY(setup != nullptr);
            QVERIFY2(!hosts.pageRegistered(logging), qPrintable(where + logging));
            QVERIFY(hosts.categoryShown(QStringLiteral("Diagnostics")));

            auto* tree = setup->findChild<QTreeWidget*>();
            QVERIFY(tree != nullptr);
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                QVERIFY2((*it)->text(0) != logging, qPrintable(where + logging));
            }
            QTreeWidgetItem* before = tree->currentItem();
            setup->selectPage(logging);
            QCOMPARE(tree->currentItem(), before);

            // No category without pages.
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                QVERIFY2(tree->topLevelItem(i)->childCount() > 0,
                         qPrintable(where + tree->topLevelItem(i)->text(0)));
            }

            // No page the tree offers has nothing to show. (A leaf or a
            // category the tree hides is not offered.)
            QStringList empty;
            int offered = 0;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                QTreeWidgetItem* category = tree->topLevelItem(i);
                if (category->isHidden()) { continue; }
                for (int j = 0; j < category->childCount(); ++j) {
                    QTreeWidgetItem* leaf = category->child(j);
                    if (leaf->isHidden()) { continue; }
                    const QVariant index = leaf->data(0, Qt::UserRole);
                    QVERIFY(index.isValid());
                    QWidget* page = setup->realizePageAtForTest(index.toInt());
                    // A Core page in a remote window before the Core's
                    // settings arrive is a stand-in under the dialog's
                    // reason line, not an empty page.
                    if (page != nullptr
                        && page->objectName() == QStringLiteral("setupStationPlaceholder")) {
                        continue;
                    }
                    // A PA page on a radio without power amplifier settings
                    // (no radio here) is shown disabled under the dialog's
                    // reason line (Task 16 fix wave 2: never hidden), not
                    // offered as a page to use.
                    if (page != nullptr && category->text(0) == QStringLiteral("PA")
                        && !page->isEnabled() && !leaf->toolTip(0).isEmpty()) {
                        continue;
                    }
                    ++offered;
                    if (page == nullptr || !pageHasShownContent(page, leaf->text(0))) {
                        empty << category->text(0) + QStringLiteral(" > ") + leaf->text(0);
                    }
                }
            }
            QVERIFY(offered > 10);
            QVERIFY2(empty.isEmpty(), qPrintable(where + empty.join(QStringLiteral("; "))));
        }

        // Once logging is built, the page is offered again with its groups.
        UnbuiltFeatures::setBuiltForTest(F::Logging, true);
        {
            Hosts hosts(sessions, false);
            QWidget* page = hosts.page(logging);
            QVERIFY(page != nullptr);
            QVERIFY(pageHasShownContent(page, logging));
        }
        UnbuiltFeatures::resetForTest();
        QVERIFY(sessions.replace({}, false));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstUnbuiltFeatures test;
    QTEST_SET_MAIN_SOURCE_PATH
    // R-R3-49 load round: 24 s quiet, up to 87 s at load 80 to 110 beside
    // two more of itself, and past the 120 s limit in a suite run at load
    // 50 to 110. Two thirds of it is markingOneFeatureBuiltShowsItsSurfacesOnly,
    // a fresh window and its Setup pages for each listed feature (115 s
    // alone at load 64 to 177). Its rows, one per feature and built here
    // from the list its _data function reads, run as three ctest entries,
    // tst_unbuilt_features_each1 to _each3, every third feature each; the
    // rest as tst_unbuilt_features (tests/CMakeLists.txt).
    constexpr int kEachEntries = 3;
    QList<NereusSDR::TestFunctionGroups::Group> groups;
    for (int k = 0; k < kEachEntries; ++k) {
        groups.append({QStringLiteral("each%1").arg(k + 1), {}});
    }
    const QList<NereusSDR::UnbuiltFeatures::Entry>& list = NereusSDR::UnbuiltFeatures::all();
    for (int i = 0; i < list.size(); ++i) {
        groups[i % kEachEntries].entries.append(
            QStringLiteral("markingOneFeatureBuiltShowsItsSurfacesOnly:") + list.at(i).key);
    }
    // Load findings 4: the rest passed 120 s above load 330 (148 s of it
    // at load 360 to 900: emptiedSetupPageIsNotShownOrFound, which builds
    // every Setup page, 34 s, and four cases that build windows 101 s), so
    // those run as entries of their own: tst_unbuilt_features_setup,
    // _windows1 and _windows2.
    groups.append({QStringLiteral("setup"),
                   {QStringLiteral("emptiedSetupPageIsNotShownOrFound")}});
    groups.append({QStringLiteral("windows1"),
                   {QStringLiteral("removedControlsAppearInNoWindow"),
                    QStringLiteral("savedValuesOfHiddenControlsSurviveAStartAndASave")}});
    groups.append({QStringLiteral("windows2"),
                   {QStringLiteral("savedValuesOfRemovedControlsSurviveAStartAndASave"),
                    QStringLiteral("noUnbuiltSurfaceShowsInLocalOrRemoteWindows")}});
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_UNBUILT_FEATURES_GROUP", groups);
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}

#include "tst_unbuilt_features.moc"
