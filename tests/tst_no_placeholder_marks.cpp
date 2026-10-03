// =================================================================
// tests/tst_no_placeholder_marks.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It builds real local and remote
// MainWindows, every Setup page, the Spot Hub, Network Diagnostics and a
// slice flag; no upstream logic is ported here.
//
// R3 unfinished controls, Task 4 (R-R3-49, R-R3-21): no control a user can
// reach carries a not-yet-implemented mark, overlay or tooltip. Every
// widget and menu item the app creates, local and remote, is swept (hidden
// ones too, since a hidden control comes back when its feature is built),
// once with the unbuilt list as it is and once with every feature marked
// built, so the surfaces hidden today are built and swept as well. The
// check itself is proven on a control marked the old way, so a mark added
// anywhere fails this test.
//
// Fix wave: roadmap wording too (a numbered phase, deferred, follow-up,
// will appear here, will add), field placeholder text, tree, table and
// list items, and the filter policy and container settings dialogs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, Task 4.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, fix wave.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  No reachable text says "yet"; the
//                                    NYI tooltip's new words. AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QListWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QWidget>

#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/RadioDiscovery.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/SetupDialog.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpotHubDialog.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/applets/NyiOverlay.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/widgets/FilterPolicyDialog.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

StationStartupSelection remoteCore()
{
    // Never dialled: replace(..., false) builds the remote window without
    // starting its connection.
    return {{QStringLiteral("ws://127.0.0.1:4433"), {}, {}, true}, QStringLiteral("core")};
}

// The words of a not-yet-implemented mark: the NYI badge, the old overlay's
// and markNyi's tooltips, and the "Phase X" roadmap placeholder. Fix wave
// I2: also roadmap wording, which promises later work instead of saying
// what a control does: a numbered phase ("Phase 3M-1"), "deferred",
// "follow-up", "will appear here" and "will add". JJ's rule for what the
// phone and the app show (2026-09-29): "yet" promises a future, so no text
// a user can reach says it; and markNyi's own tooltip, "This control is
// not built."
const QRegularExpression& placeholderWording()
{
    static const QRegularExpression pattern(
        QStringLiteral("\\bNYI\\b|not yet implemented|\\bPhase X\\b|Available in Phase"
                       "|\\bPhase [0-9]|\\bdeferred\\b|\\bfollow-up\\b|\\bfollow up\\b"
                       "|will appear here|\\bwill add\\b|control is not built"
                       "|\\byet\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

QString describe(const QObject* object)
{
    QStringList chain;
    for (const QObject* o = object; o != nullptr; o = o->parent()) {
        const QString name = o->objectName();
        chain.prepend(QString::fromLatin1(o->metaObject()->className())
                      + (name.isEmpty() ? QString() : QStringLiteral("#") + name));
        if (chain.size() >= 4) { break; }
    }
    return chain.join(QStringLiteral(" > "));
}

// Every text a widget or a menu item can show a user: its captions, tab
// and list items, and its tooltip, status tip and "What's This".
QStringList textsOf(const QWidget* w)
{
    QStringList texts{w->toolTip(), w->statusTip(), w->whatsThis(),
                      w->accessibleName(), w->accessibleDescription()};
    if (const auto* edit = qobject_cast<const QLineEdit*>(w)) {
        texts << edit->placeholderText();
    } else if (const auto* plain = qobject_cast<const QPlainTextEdit*>(w)) {
        texts << plain->placeholderText();
    } else if (const auto* rich = qobject_cast<const QTextEdit*>(w)) {
        texts << rich->placeholderText();
    }
    if (const auto* label = qobject_cast<const QLabel*>(w)) {
        texts << label->text();
    } else if (const auto* button = qobject_cast<const QAbstractButton*>(w)) {
        texts << button->text();
    } else if (const auto* group = qobject_cast<const QGroupBox*>(w)) {
        texts << group->title();
    } else if (const auto* combo = qobject_cast<const QComboBox*>(w)) {
        for (int i = 0; i < combo->count(); ++i) {
            texts << combo->itemText(i) << combo->itemData(i, Qt::ToolTipRole).toString();
        }
    } else if (const auto* tabs = qobject_cast<const QTabWidget*>(w)) {
        for (int i = 0; i < tabs->count(); ++i) {
            texts << tabs->tabText(i) << tabs->tabToolTip(i);
        }
    } else if (const auto* menu = qobject_cast<const QMenu*>(w)) {
        texts << menu->title();
    } else if (const auto* tree = qobject_cast<const QTreeWidget*>(w)) {
        // Fix wave follow-up: every item's text and tooltip, every column.
        std::function<void(const QTreeWidgetItem*)> walk = [&](const QTreeWidgetItem* item) {
            for (int c = 0; c < item->columnCount(); ++c) {
                texts << item->text(c) << item->toolTip(c) << item->statusTip(c);
            }
            for (int i = 0; i < item->childCount(); ++i) { walk(item->child(i)); }
        };
        if (const QTreeWidgetItem* header = tree->headerItem()) { walk(header); }
        for (int i = 0; i < tree->topLevelItemCount(); ++i) { walk(tree->topLevelItem(i)); }
    } else if (const auto* table = qobject_cast<const QTableWidget*>(w)) {
        for (int r = 0; r < table->rowCount(); ++r) {
            for (int c = 0; c < table->columnCount(); ++c) {
                if (const QTableWidgetItem* item = table->item(r, c)) {
                    texts << item->text() << item->toolTip() << item->statusTip();
                }
            }
        }
        for (int c = 0; c < table->columnCount(); ++c) {
            if (const QTableWidgetItem* item = table->horizontalHeaderItem(c)) {
                texts << item->text() << item->toolTip();
            }
        }
        for (int r = 0; r < table->rowCount(); ++r) {
            if (const QTableWidgetItem* item = table->verticalHeaderItem(r)) {
                texts << item->text() << item->toolTip();
            }
        }
    } else if (const auto* list = qobject_cast<const QListWidget*>(w)) {
        for (int i = 0; i < list->count(); ++i) {
            texts << list->item(i)->text() << list->item(i)->toolTip();
        }
    }
    return texts;
}

QStringList textsOf(const QAction* a)
{
    return {a->text(), a->toolTip(), a->statusTip(), a->whatsThis(), a->iconText()};
}

// Every placeholder mark on the widgets alive now, hidden ones included.
QStringList marksInApp()
{
    QStringList found;
    QSet<const QAction*> actions;
    for (const QWidget* w : QApplication::allWidgets()) {
        if (qobject_cast<const NyiOverlay*>(w) != nullptr) {
            found << QStringLiteral("NYI badge on ") + describe(w->parent());
            continue;
        }
        for (const QString& text : textsOf(w)) {
            if (placeholderWording().match(text).hasMatch()) {
                found << describe(w) + QStringLiteral(": ") + text;
            }
        }
        for (const QAction* a : w->actions()) { actions.insert(a); }
        for (const QAction* a : w->findChildren<QAction*>(Qt::FindDirectChildrenOnly)) {
            actions.insert(a);
        }
    }
    for (const QAction* a : actions) {
        for (const QString& text : textsOf(a)) {
            if (placeholderWording().match(text).hasMatch()) {
                found << QStringLiteral("menu item ") + describe(a) + QStringLiteral(": ") + text;
            }
        }
    }
    found.removeDuplicates();
    return found;
}

// Builds everything the app creates for one window: the window (its menus,
// status bar, applets, pan overlays and containers), the Setup dialog with
// every registered page realized, the Spot Hub, Network Diagnostics and a
// slice flag.
QStringList marksInWindow(GuiSessionCoordinator& sessions, bool remote)
{
    const bool ok = remote ? sessions.replace(remoteCore(), false) : sessions.replace({}, false);
    MainWindow* window = ok ? sessions.window() : nullptr;
    if (window == nullptr) { return {QStringLiteral("no window")}; }
    if (window->radioModel()->ownsLocalDsp() == remote) {
        return {QStringLiteral("wrong window kind")};
    }
    window->resize(4000, 1000);
    window->show();
    QCoreApplication::processEvents();

    auto* setup = new SetupDialog(window->radioModel(), window);
    setup->realizeAllPagesForTest();
    QMetaObject::invokeMethod(window, "openSpotHub", Qt::DirectConnection);
    auto* netDiag = new NetworkDiagnosticsDialog(window->radioModel(), nullptr, window);
    Q_UNUSED(netDiag);

    // Fix wave follow-up: the filter policy dialog (both chains) and a
    // container's settings dialog with its Add menu opened.
    auto* policy0 = new FilterPolicyDialog(0, window->radioModel(), window);
    auto* policy1 = new FilterPolicyDialog(1, window->radioModel(), window);
    Q_UNUSED(policy0);
    Q_UNUSED(policy1);
    auto* container = new ContainerWidget(window);
    auto* meter = new MeterWidget();
    meter->addItem(new TextItem());
    meter->addItem(new OtherButtonItem());
    container->setContent(meter);
    auto* containerDialog = new ContainerSettingsDialog(container, window);
    for (QPushButton* button : containerDialog->findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("+")) { button->click(); }
    }

    auto flagHost = std::make_unique<SpectrumWidget>();
    flagHost->resize(1200, 500);
    flagHost->setSampleRate(192000.0);
    flagHost->setDdcCenterFrequency(14200000.0);
    flagHost->setFrequencyRange(14200000.0, 192000.0);
    VfoWidget* flag = flagHost->addVfoWidget(0);
    flag->setFrequency(14200000.0);
    flagHost->show();
    QCoreApplication::processEvents();
    flagHost->updateVfoPositions();

    return marksInApp();
}

} // namespace

class TstNoPlaceholderMarks : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Windows save their settings; this run keeps a file of its own.
        AppSettings::setProfileOverride(QStringLiteral("no-placeholder-marks-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        UnbuiltFeatures::resetForTest();
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        // No VAX first-run dialog and no discovery broadcast onto the LAN.
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

    // The check finds a control marked the old way (overlay tooltip and
    // badge) and a menu item with a placeholder tooltip, so adding a mark
    // anywhere the app builds fails the sweep below.
    void theSweepFindsAMark()
    {
        QVERIFY(marksInApp().isEmpty());
        {
            QWidget host;
            auto* button = new QPushButton(QStringLiteral("Try"), &host);
            NyiOverlay::markNyi(button, QStringLiteral("Phase 9"));
            QVERIFY2(!marksInApp().isEmpty(), "a markNyi tooltip was not found");
        }
        {
            QWidget host;
            auto* badge = new NyiOverlay(QStringLiteral("Phase 9"), &host);
            badge->setToolTip(QString());
            QVERIFY2(!marksInApp().isEmpty(), "an NYI badge was not found");
        }
        {
            QMenu menu;
            QAction* item = menu.addAction(QStringLiteral("Something"));
            item->setToolTip(QStringLiteral("NYI - Phase X"));
            QVERIFY2(!marksInApp().isEmpty(), "a menu item's placeholder tooltip was not found");
        }
        // Fix wave I2: roadmap wording in a label, a tooltip or a field's
        // placeholder text is found too.
        const QStringList roadmap{
            QStringLiteral("TX above RX (repeater High offset), Phase 3M-1"),
            QStringLiteral("A later phase will add a per-band override."),
            QStringLiteral("Deferred to a later release"),
            QStringLiteral("Follow-up work"),
            QStringLiteral("Profiles will appear here"),
            // JJ's rule: "yet" promises a future.
            QStringLiteral("Band stacking is not ready yet."),
        };
        for (const QString& text : roadmap) {
            {
                QWidget host;
                new QLabel(text, &host);
                QVERIFY2(!marksInApp().isEmpty(), qPrintable(QStringLiteral("label: ") + text));
            }
            {
                QWidget host;
                auto* button = new QPushButton(QStringLiteral("Try"), &host);
                button->setToolTip(text);
                QVERIFY2(!marksInApp().isEmpty(), qPrintable(QStringLiteral("tooltip: ") + text));
            }
            {
                QWidget host;
                auto* edit = new QLineEdit(&host);
                edit->setPlaceholderText(text);
                QVERIFY2(!marksInApp().isEmpty(),
                         qPrintable(QStringLiteral("placeholder text: ") + text));
            }
        }
        // Fix wave follow-up: tree, table and list items, text and tooltip.
        {
            QTreeWidget tree;
            auto* item = new QTreeWidgetItem(&tree, {QStringLiteral("Fine")});
            item->setToolTip(0, QStringLiteral("Coming in Phase 3M"));
            QVERIFY2(!marksInApp().isEmpty(), "a tree item's tooltip was not found");
        }
        {
            QTreeWidget tree;
            new QTreeWidgetItem(&tree, {QStringLiteral("Deferred")});
            QVERIFY2(!marksInApp().isEmpty(), "a tree item's text was not found");
        }
        {
            QTableWidget table(1, 1);
            auto* item = new QTableWidgetItem(QStringLiteral("Fine"));
            item->setToolTip(QStringLiteral("NYI"));
            table.setItem(0, 0, item);
            QVERIFY2(!marksInApp().isEmpty(), "a table item's tooltip was not found");
        }
        {
            QTableWidget table(1, 1);
            table.setItem(0, 0, new QTableWidgetItem(QStringLiteral("Rows will appear here")));
            QVERIFY2(!marksInApp().isEmpty(), "a table item's text was not found");
        }
        QVERIFY(marksInApp().isEmpty());
    }

    // Nothing the app creates carries a mark, in a local or a remote window.
    void noPlaceholderMarkInLocalOrRemoteWindows()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            const QStringList marks = marksInWindow(sessions, remote);
            // One line per mark: a test failure message is cut short.
            for (const QString& mark : marks) { qWarning().noquote() << "mark:" << mark; }
            QVERIFY2(marks.isEmpty(),
                     qPrintable((remote ? QStringLiteral("remote: ") : QStringLiteral("local: "))
                                + marks.join(QStringLiteral("; "))));
        }
        QVERIFY(sessions.replace({}, false));
    }

    // With every feature on the unbuilt list marked built, the surfaces
    // hidden today (menus, Setup pages, applet pages) are built too; none of
    // them carries a mark either, so none comes back with one.
    void noPlaceholderMarkOnSurfacesHiddenUntilBuilt()
    {
        for (const UnbuiltFeatures::Entry& entry : UnbuiltFeatures::all()) {
            UnbuiltFeatures::setBuiltForTest(entry.feature, true);
        }
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            const QStringList marks = marksInWindow(sessions, remote);
            // One line per mark: a test failure message is cut short.
            for (const QString& mark : marks) { qWarning().noquote() << "mark:" << mark; }
            QVERIFY2(marks.isEmpty(),
                     qPrintable((remote ? QStringLiteral("remote, all built: ")
                                        : QStringLiteral("local, all built: "))
                                + marks.join(QStringLiteral("; "))));
        }
        QVERIFY(sessions.replace({}, false));
    }
};

QTEST_MAIN(TstNoPlaceholderMarks)
#include "tst_no_placeholder_marks.moc"
