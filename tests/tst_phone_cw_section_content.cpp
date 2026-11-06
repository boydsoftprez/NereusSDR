// no-port-check: NereusSDR-original test file.
//
// A container can hold a copy of the Phone/CW panel. The copy hides each
// row, and the Phone and CW buttons, from its entry. The docked panel is
// unchanged.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - The Phone/CW panel copy's hide toggles, page choice and
//                 live wiring. Brian Sutton, AI-assisted via xAI Grok.
//   2026-11-06 - PR 757 corrections: every row's hide, show and page switch
//                 asked of the row's own wrapper, heights as rows hide, the
//                 docked panel held still, Advanced bindings, the compression
//                 gauge's reason, the DEXP marker, the iambic memory and the
//                 remote window's gates. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAbstractSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QJsonObject>
#include <QLabel>
#include <QLoggingCategory>
#include <QMap>
#include <QPushButton>
#include <QSlider>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <memory>

#include "core/AppSettings.h"
#include "core/CwTypes.h"
#include "gui/HGauge.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/PhoneCwSectionContent.h"
#include "gui/applets/SectionParts.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterPoller.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

QPushButton* buttonNamed(QWidget* root, const QString& text)
{
    for (QPushButton* button : root->findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

HGauge* gaugeNamed(QWidget* root, const QString& name)
{
    for (HGauge* gauge : root->findChildren<HGauge*>()) {
        if (gauge->accessibleName() == name) {
            return gauge;
        }
    }
    return nullptr;
}

const QStringList kPhoneRows = {
    QStringLiteral("level"), QStringLiteral("compression"), QStringLiteral("profile"),
    QStringLiteral("mic"), QStringLiteral("proc"), QStringLiteral("dexp"),
    QStringLiteral("amCarrier")};
const QStringList kCwRows = {
    QStringLiteral("cwDelay"), QStringLiteral("cwSpeed"), QStringLiteral("cwSidetone"),
    QStringLiteral("cwPan"), QStringLiteral("cwKeying"), QStringLiteral("cwLamps"),
    QStringLiteral("cwStatus")};
const QString kPageButtons = QStringLiteral("pageButtons");

// A toggle hides the row's wrapper, not the controls inside it, and
// isHidden() answers only for the widget it is asked of.
QWidget* rowOf(const PhoneCwApplet* copy, const QString& id)
{
    const QList<QWidget*> widgets = copy->containerParts()->widgets(id);
    return widgets.size() == 1 ? widgets.first() : nullptr;
}

QJsonObject hiding(const QStringList& ids, const QString& page = QString())
{
    QJsonObject config;
    for (const QString& id : ids) {
        config.insert(SectionParts::hideKey(id), true);
    }
    if (!page.isEmpty()) {
        config.insert(QStringLiteral("page"), page);
    }
    return config;
}

QStringList allBut(const QStringList& rows, const QString& kept)
{
    QStringList others = rows;
    others.removeAll(kept);
    return others;
}

QStackedWidget* stackOf(QWidget* applet)
{
    return applet->findChild<QStackedWidget*>();
}

// A window that lays the panel out the way a container's stack does: one
// column, the panel at its own height, spare room under it.
struct Column {
    QWidget window;
    QVBoxLayout* layout = nullptr;

    Column()
    {
        window.resize(320, 900);
        layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    void hold(QWidget* panel)
    {
        layout->addWidget(panel);
        layout->addStretch();
        window.show();
        settle();
    }

    void settle()
    {
        // Each level's layout request reaches its parent on the next pass.
        for (int pass = 0; pass < 6; ++pass) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            layout->activate();
        }
    }
};

void settleCopy(PhoneCwApplet* copy)
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        copy->layout()->activate();
        stackOf(copy)->currentWidget()->layout()->activate();
    }
}

// Every row showing has the height it wants, inside the copy's rect.
bool everyRowFits(PhoneCwApplet* copy, QString* problem)
{
    const QStringList rows = QStringList{kPageButtons} + kPhoneRows + kCwRows;
    for (const QString& id : rows) {
        QWidget* row = rowOf(copy, id);
        if (!row->isVisible()) {
            continue;
        }
        const int bottom = row->mapTo(copy, QPoint(0, row->height())).y();
        const int wanted = row->hasHeightForWidth() ? row->heightForWidth(row->width())
                                                    : row->sizeHint().height();
        if (bottom > copy->height() || row->height() < wanted) {
            *problem = QStringLiteral("%1 is %2 px of %3 wanted, ending at %4 in a %5 px rect")
                           .arg(id).arg(row->height()).arg(wanted).arg(bottom).arg(copy->height());
            return false;
        }
    }
    return true;
}

// Every control a person can reach, by its accessible name.
struct ControlState {
    bool enabled = false;
    QString toolTip;
    bool operator==(const ControlState& other) const
    {
        return enabled == other.enabled && toolTip == other.toolTip;
    }
};

QMap<QString, ControlState> controlsOf(QWidget* root)
{
    QMap<QString, ControlState> controls;
    const auto record = [&controls](QWidget* control) {
        if (!control->accessibleName().isEmpty()) {
            controls.insert(control->accessibleName(), {control->isEnabled(), control->toolTip()});
        }
    };
    for (QPushButton* control : root->findChildren<QPushButton*>()) { record(control); }
    for (QAbstractSlider* control : root->findChildren<QAbstractSlider*>()) { record(control); }
    for (QComboBox* control : root->findChildren<QComboBox*>()) { record(control); }
    return controls;
}

bool sameGates(QWidget* docked, QWidget* copy, QString* difference)
{
    const QMap<QString, ControlState> expected = controlsOf(docked);
    const QMap<QString, ControlState> actual = controlsOf(copy);
    if (expected.keys() != actual.keys()) {
        *difference = QStringLiteral("different controls");
        return false;
    }
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        if (!(actual.value(it.key()) == it.value())) {
            *difference = it.key() + QStringLiteral(": docked ")
                + (it.value().enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                + QStringLiteral(" \"") + it.value().toolTip + QStringLiteral("\", copy ")
                + (actual.value(it.key()).enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                + QStringLiteral(" \"") + actual.value(it.key()).toolTip + QStringLiteral("\"");
            return false;
        }
    }
    return true;
}

} // namespace

class TstPhoneCwSectionContent : public QObject {
    Q_OBJECT
    static inline QtMessageHandler s_previousHandler = nullptr;

private slots:
    void initTestCase()
    {
        // A slice made with no radio connected says so; these tests have none.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
        // The offscreen platform answers every shown window's size hints with
        // this one line. It says nothing about the panel.
        s_previousHandler = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& context, const QString& message) {
                if (message != QLatin1String("This plugin does not support propagateSizeHints()")
                    && s_previousHandler) {
                    s_previousHandler(type, context, message);
                }
            });
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(s_previousHandler);
    }

    void init()
    {
        AppSettings::instance().remove(QStringLiteral("PhoneCw/DexpThresholdMarkerDb"));
    }

    void copyHidesRowsTheAppletKeeps()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* copy = applet.createPanelCopy(&host, false);
        QVERIFY(copy);
        QVERIFY(copy != &applet);

        QJsonObject config;
        config.insert(SectionParts::hideKey(QStringLiteral("level")), true);
        config.insert(SectionParts::hideKey(QStringLiteral("pageButtons")), true);
        config.insert(QStringLiteral("page"), QStringLiteral("cw"));
        copy->applyContainerConfig(config);

        HGauge* copyLevel = gaugeNamed(copy, QStringLiteral("Microphone level gauge"));
        HGauge* appletLevel = gaugeNamed(&applet, QStringLiteral("Microphone level gauge"));
        QVERIFY(copyLevel);
        QVERIFY(appletLevel);
        // The toggle hides the row's wrapper around the gauge.
        QVERIFY(copyLevel->parentWidget()->isHidden());
        QVERIFY(!copyLevel->isHidden());
        QVERIFY(!appletLevel->isHidden());

        QPushButton* copyPhone = buttonNamed(copy, QStringLiteral("Phone"));
        QPushButton* appletPhone = buttonNamed(&applet, QStringLiteral("Phone"));
        QVERIFY(copyPhone);
        QVERIFY(appletPhone);
        QVERIFY(copyPhone->parentWidget()->isHidden());
        QVERIFY(!appletPhone->isHidden());

        auto* delay = copy->findChild<QSlider*>(QStringLiteral("cwDelaySlider"));
        QVERIFY(delay);
        QVERIFY(!delay->isHidden());
        QCOMPARE(copy->findChild<QStackedWidget*>()->currentIndex(), 1);

        // MON is not built, so the copy keeps it hidden and offers no toggle.
        QPushButton* mon = buttonNamed(copy, QStringLiteral("MON"));
        QVERIFY(mon);
        QVERIFY(mon->isHidden());
    }

    void liveCopySetsTheModelAndPreviewDoesNot()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* copy = applet.createPanelCopy(&host, false);
        auto* proc = copy->findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(proc);
        QVERIFY(proc->isEnabled());
        proc->setChecked(true);
        QVERIFY(radio.transmitModel().cpdrOn());
        auto* appletProc = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(appletProc->isChecked());

        const std::unique_ptr<PhoneCwApplet> preview(applet.createPanelCopy(nullptr, true));
        QVERIFY(preview);
        QVERIFY(!preview->isEnabled());
        auto* previewProc = preview->findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(previewProc);
        previewProc->setChecked(true);
        previewProc->setChecked(false);
        QVERIFY(radio.transmitModel().cpdrOn());
    }

    void propertiesNameThePageAndEachHideToggle()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        const ContentDescriptor descriptor = [&] {
            for (const ContentDescriptor& item : registry.descriptors()) {
                if (item.typeId == QLatin1String("phcw.panel")) {
                    return item;
                }
            }
            return ContentDescriptor{};
        }();
        QCOMPARE(descriptor.title, QStringLiteral("Phone / CW sections"));
        QVERIFY(descriptor.group == ContentGroup::PanelSections);

        const QVector<ContentProperty> properties = registry.declaredProperties(QStringLiteral("phcw.panel"));
        QStringList labels;
        for (const ContentProperty& property : properties) {
            labels << property.label;
            QVERIFY(property.kind != ContentProperty::Kind::Order);
        }
        QCOMPARE(labels.first(), QStringLiteral("Page"));
        QVERIFY(labels.contains(QStringLiteral("Hide page buttons")));
        QVERIFY(labels.contains(QStringLiteral("Hide level gauge")));
        QVERIFY(labels.contains(QStringLiteral("Hide compression gauge")));
        QVERIFY(labels.contains(QStringLiteral("Hide mic profile")));
        QVERIFY(labels.contains(QStringLiteral("Hide mic source and gain")));
        // The PROC row also holds VAX, so the toggle says so.
        QVERIFY(labels.contains(QStringLiteral("Hide PROC and VAX")));
        QVERIFY(!labels.contains(QStringLiteral("Hide PROC")));
        QVERIFY(labels.contains(QStringLiteral("Hide DEXP")));
        QVERIFY(labels.contains(QStringLiteral("Hide AM carrier")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW delay")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW speed")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW sidetone")));
        QVERIFY(labels.contains(QStringLiteral("Hide CW pan")));
        QVERIFY(labels.contains(QStringLiteral("Hide break-in, Iambic and pitch")));
        QVERIFY(labels.contains(QStringLiteral("Hide KEY, TX and keyer lamps")));
        QVERIFY(labels.contains(QStringLiteral("Hide keyer status")));
        QVERIFY(!labels.contains(QStringLiteral("Hide MON")));
    }

    // The rows the copy offers, each with its one wrapper.
    void theCopyNamesEachRow()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* copy = applet.createPanelCopy(&host, false);
        QCOMPARE(copy->containerParts()->ids(),
                 QStringList{kPageButtons} + kPhoneRows + kCwRows);
        for (const QString& id : copy->containerParts()->ids()) {
            QVERIFY2(rowOf(copy, id), qPrintable(id));
        }
        // The docked panel has no rows to hide.
        QVERIFY(!applet.containerParts());
    }

    void eachHideKeyHidesExactlyItsRow()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* plain = applet.createPanelCopy(&host, false);
        const QStringList ids = plain->containerParts()->ids();
        for (const QString& id : ids) {
            auto* copy = applet.createPanelCopy(&host, false);
            copy->applyContainerConfig(hiding({id}));
            const SectionParts* parts = copy->containerParts();
            for (const QString& other : ids) {
                QCOMPARE(parts->isHidden(other), other == id);
                const QList<QWidget*> mine = parts->widgets(other);
                const QList<QWidget*> theirs = plain->containerParts()->widgets(other);
                QCOMPARE(mine.size(), theirs.size());
                QVERIFY(!mine.isEmpty());
                for (int i = 0; i < mine.size(); ++i) {
                    if (other == id) {
                        QVERIFY2(mine.at(i)->isHidden(), qPrintable(id));
                    } else {
                        QCOMPARE(mine.at(i)->isHidden(), theirs.at(i)->isHidden());
                    }
                }
            }
        }
    }

    // Through the factory, a saved entry's config brings each hide and the
    // page back.
    void aSavedEntryRestoresItsHidesAndPage()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        QWidget host;
        for (const QString& id : QStringList{kPageButtons} + kPhoneRows + kCwRows) {
            ContentEntry entry = registry.makeEntry(QStringLiteral("phcw.panel"));
            entry.config.insert(SectionParts::hideKey(id), true);
            entry.config.insert(QStringLiteral("page"), QStringLiteral("cw"));
            auto* copy = qobject_cast<PhoneCwApplet*>(registry.createWidget(entry, &host));
            QVERIFY2(copy, qPrintable(id));
            QVERIFY2(rowOf(copy, id)->isHidden(), qPrintable(id));
            QCOMPARE(stackOf(copy)->currentIndex(), 1);
        }
        auto* plain = qobject_cast<PhoneCwApplet*>(
            registry.createWidget(registry.makeEntry(QStringLiteral("phcw.panel")), &host));
        QVERIFY(plain);
        QCOMPARE(stackOf(plain)->currentIndex(), 0);
    }

    // A hidden row stays hidden through a page switch, the docked panel's
    // mode change, a sync and the same config again; clearing the key shows
    // it; the copy stays on its page.
    void aHiddenRowStaysHiddenAndAClearedOneShows_data()
    {
        QTest::addColumn<QString>("row");
        QTest::addColumn<QString>("page");
        QTest::addColumn<int>("pageIndex");
        QTest::newRow("pageButtons on phone") << kPageButtons << QStringLiteral("phone") << 0;
        QTest::newRow("pageButtons on cw") << kPageButtons << QStringLiteral("cw") << 1;
        for (const QString& id : kPhoneRows) {
            QTest::newRow(qPrintable(id)) << id << QStringLiteral("phone") << 0;
        }
        for (const QString& id : kCwRows) {
            QTest::newRow(qPrintable(id)) << id << QStringLiteral("cw") << 1;
        }
    }

    void aHiddenRowStaysHiddenAndAClearedOneShows()
    {
        QFETCH(QString, row);
        QFETCH(QString, page);
        QFETCH(int, pageIndex);

        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        PhoneCwApplet applet(&radio);
        Column column;
        auto* copy = applet.createPanelCopy(&column.window, false);
        const QJsonObject config = hiding({row}, page);
        copy->applyContainerConfig(config);
        column.hold(copy);

        const auto rowsShowAsConfigured = [&]() {
            const QStringList rows = QStringList{kPageButtons} + kPhoneRows + kCwRows;
            for (const QString& id : rows) {
                if (rowOf(copy, id)->isHidden() != (id == row)) {
                    return false;
                }
            }
            return true;
        };
        QVERIFY(rowsShowAsConfigured());
        QVERIFY(!rowOf(copy, row)->isVisible());
        QCOMPARE(stackOf(copy)->currentIndex(), pageIndex);

        // What MainWindow does on a slice mode change reaches the docked
        // panel only. The copy keeps its page and its hides.
        for (DSPMode mode : {DSPMode::CWU, DSPMode::USB, DSPMode::FM, DSPMode::CWL, DSPMode::LSB}) {
            slice->setDspMode(mode);
            applet.showPage(mode == DSPMode::CWU || mode == DSPMode::CWL ? 1
                            : mode == DSPMode::FM ? 2 : 0);
            applet.syncFromModel();
            column.settle();
            QVERIFY(rowsShowAsConfigured());
            QCOMPARE(stackOf(copy)->currentIndex(), pageIndex);
        }

        // Its own page buttons still turn the page, and no hide moves.
        auto* other = buttonNamed(copy, pageIndex == 0 ? QStringLiteral("CW") : QStringLiteral("Phone"));
        QVERIFY(other);
        other->click();
        column.settle();
        QCOMPARE(stackOf(copy)->currentIndex(), pageIndex == 0 ? 1 : 0);
        QVERIFY(rowsShowAsConfigured());
        QVERIFY(!rowOf(copy, row)->isVisible());
        copy->showPage(pageIndex);
        column.settle();
        QVERIFY(rowsShowAsConfigured());

        // The config again: nothing changes.
        copy->applyContainerConfig(config);
        column.settle();
        QVERIFY(rowsShowAsConfigured());
        QCOMPARE(stackOf(copy)->currentIndex(), pageIndex);

        // A config without the key shows the row on its page.
        QJsonObject shown;
        shown.insert(QStringLiteral("page"), page);
        copy->applyContainerConfig(shown);
        column.settle();
        QVERIFY(!rowOf(copy, row)->isHidden());
        QVERIFY(rowOf(copy, row)->isVisible());
    }

    // ── Heights ──────────────────────────────────────────────────────────

    void aCopyGetsShorterAsRowsHide_data()
    {
        QTest::addColumn<QString>("page");
        QTest::addColumn<QStringList>("rows");
        QTest::newRow("phone") << QStringLiteral("phone") << kPhoneRows;
        QTest::newRow("cw") << QStringLiteral("cw") << kCwRows;
    }

    void aCopyGetsShorterAsRowsHide()
    {
        QFETCH(QString, page);
        QFETCH(QStringList, rows);

        RadioModel radio;
        PhoneCwApplet applet(&radio);
        Column column;
        auto* copy = applet.createPanelCopy(&column.window, false);
        column.hold(copy);
        copy->applyContainerConfig(hiding({}, page));
        column.settle();

        QStringList hidden;
        int height = copy->height();
        for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
            hidden << *it;
            copy->applyContainerConfig(hiding(hidden, page));
            column.settle();
            QVERIFY2(copy->height() < height,
                     qPrintable(QStringLiteral("hiding %1 left %2 px, was %3 px")
                                    .arg(*it).arg(copy->height()).arg(height)));
            height = copy->height();
        }
    }

    // One row left, page buttons gone too: the block is that row and its
    // page's margins, with no gap above or below.
    void aLoneRowLeavesNoGaps_data()
    {
        QTest::addColumn<QString>("row");
        QTest::addColumn<QString>("page");
        for (const QString& id : kPhoneRows) {
            QTest::newRow(qPrintable(id)) << id << QStringLiteral("phone");
        }
        for (const QString& id : kCwRows) {
            QTest::newRow(qPrintable(id)) << id << QStringLiteral("cw");
        }
    }

    void aLoneRowLeavesNoGaps()
    {
        QFETCH(QString, row);
        QFETCH(QString, page);

        RadioModel radio;
        PhoneCwApplet applet(&radio);
        Column column;
        auto* copy = applet.createPanelCopy(&column.window, false);
        column.hold(copy);
        const QStringList rows = page == QLatin1String("phone") ? kPhoneRows : kCwRows;
        copy->applyContainerConfig(hiding(QStringList{kPageButtons} + allBut(rows, row), page));
        column.settle();

        QWidget* kept = rowOf(copy, row);
        QVERIFY(kept->isVisible());
        const QMargins margins = kept->parentWidget()->layout()->contentsMargins();
        QCOMPARE(kept->y(), margins.top());
        QCOMPARE(copy->height(), margins.top() + kept->height() + margins.bottom());
    }

    // With the page buttons kept, the block is the buttons and the page.
    void thePageButtonsAddOnlyTheirOwnHeight()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        Column column;
        auto* copy = applet.createPanelCopy(&column.window, false);
        column.hold(copy);
        copy->applyContainerConfig(hiding(allBut(kPhoneRows, QStringLiteral("mic"))));
        column.settle();

        QWidget* mic = rowOf(copy, QStringLiteral("mic"));
        const QMargins margins = mic->parentWidget()->layout()->contentsMargins();
        QCOMPARE(copy->height(), rowOf(copy, kPageButtons)->height()
                                     + margins.top() + mic->height() + margins.bottom());
    }

    // The gap under the compression gauge goes with the gauge.
    void theGapAfterCompressionGoesWithIt()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        Column column;
        auto* copy = applet.createPanelCopy(&column.window, false);
        column.hold(copy);
        QWidget* level = rowOf(copy, QStringLiteral("level"));
        QWidget* profile = rowOf(copy, QStringLiteral("profile"));
        const auto* page = level->parentWidget();
        const int spacing = qobject_cast<QBoxLayout*>(page->layout())->spacing();

        // All shown: the gauges sit 4 px further from the controls, as the
        // docked panel draws them.
        auto* gauge = gaugeNamed(copy, QStringLiteral("Compression gauge"));
        const int gaugeBottom = gauge->mapTo(page, QPoint(0, gauge->height())).y();
        QCOMPARE(profile->y() - gaugeBottom, spacing + 4);

        copy->applyContainerConfig(hiding({QStringLiteral("compression")}));
        column.settle();
        QCOMPARE(profile->y() - (level->y() + level->height()), spacing);

        copy->applyContainerConfig(hiding({QStringLiteral("level"), QStringLiteral("compression")}));
        column.settle();
        QCOMPARE(profile->y(), page->layout()->contentsMargins().top());
    }

    // A free canvas can give the copy more than it asks for. The rows stay
    // together at the top, on either page.
    void spareRoomGoesBelowTheRows_data()
    {
        QTest::addColumn<QString>("page");
        QTest::addColumn<QStringList>("rows");
        QTest::newRow("phone") << QStringLiteral("phone") << kPhoneRows;
        QTest::newRow("cw") << QStringLiteral("cw") << kCwRows;
    }

    void spareRoomGoesBelowTheRows()
    {
        QFETCH(QString, page);
        QFETCH(QStringList, rows);

        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        host.resize(320, 900);
        auto* copy = applet.createPanelCopy(&host, false);
        copy->applyContainerConfig(hiding({rows.at(1)}, page));
        copy->resize(320, copy->sizeHint().height());
        host.show();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        copy->layout()->activate();
        stackOf(copy)->currentWidget()->layout()->activate();

        QMap<QString, int> tops;
        for (const QString& id : rows) {
            tops.insert(id, rowOf(copy, id)->y());
        }
        copy->resize(320, copy->height() + 150);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        copy->layout()->activate();
        stackOf(copy)->currentWidget()->layout()->activate();
        for (const QString& id : rows) {
            if (id != rows.at(1)) {
                QCOMPARE(rowOf(copy, id)->y(), tops.value(id));
            }
        }
    }

    // The block follows the page its own buttons turn to.
    void theHeightFollowsThePage()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        Column column;
        auto* copy = applet.createPanelCopy(&column.window, false);
        column.hold(copy);
        copy->applyContainerConfig(hiding(allBut(kPhoneRows, QStringLiteral("mic"))));
        column.settle();
        const int phone = copy->height();

        buttonNamed(copy, QStringLiteral("CW"))->click();
        column.settle();
        const int cw = copy->height();
        QVERIFY2(cw > phone, qPrintable(QStringLiteral("phone %1 px, cw %2 px").arg(phone).arg(cw)));
        QVERIFY(rowOf(copy, QStringLiteral("cwDelay"))->isVisible());

        buttonNamed(copy, QStringLiteral("Phone"))->click();
        column.settle();
        QCOMPARE(copy->height(), phone);
    }

    // The docked panel keeps every row and the layout it always had: its
    // height holds on every page, whatever a copy does, and none of the
    // copy's layout changes reach it.
    void theDockedPanelsLayoutDoesNotChange()
    {
        RadioModel radio;
        Column column;
        auto* applet = new PhoneCwApplet(&radio, &column.window);
        column.hold(applet);
        QStackedWidget* stack = stackOf(applet);
        QCOMPARE(stack->count(), 3);
        const int height = applet->height();

        // The height is the buttons row plus the tallest page, FM included,
        // so drift that stays equal across pages shows here too.
        QPushButton* phoneButton = buttonNamed(applet, QStringLiteral("Phone"));
        const int buttonsRow = 2 * phoneButton->y() + phoneButton->height();
        int tallestPage = 0;
        for (int i = 0; i < stack->count(); ++i) {
            tallestPage = qMax(tallestPage, stack->widget(i)->sizeHint().height());
        }
        QCOMPARE(height, buttonsRow + tallestPage);

        const auto pagesKeepTheirPolicy = [stack]() {
            for (int i = 0; i < stack->count(); ++i) {
                if (stack->widget(i)->sizePolicy().verticalPolicy() != QSizePolicy::Preferred) {
                    return false;
                }
            }
            return true;
        };
        QVERIFY(pagesKeepTheirPolicy());
        for (int index : {1, 0}) {
            applet->showPage(index);
            column.settle();
            QCOMPARE(applet->height(), height);
            QVERIFY(pagesKeepTheirPolicy());
        }

        // The rows sit directly on the page, unwrapped, with the 4 px spacer
        // after the gauges and no stretch below the last row.
        QWidget* phonePage = stack->widget(0);
        QCOMPARE(gaugeNamed(applet, QStringLiteral("Compression gauge"))->parentWidget(), phonePage);
        QLayout* phoneColumn = phonePage->layout();
        int spacers = 0;
        for (int i = 0; i < phoneColumn->count(); ++i) {
            if (QSpacerItem* spacer = phoneColumn->itemAt(i)->spacerItem()) {
                ++spacers;
                QCOMPARE(spacer->sizeHint().height(), 4);
                QVERIFY(i < phoneColumn->count() - 1);
            }
        }
        QCOMPARE(spacers, 1);

        auto* copy = applet->createPanelCopy(&column.window, false);
        copy->applyContainerConfig(hiding(kPhoneRows));
        copy->show();
        column.settle();
        QCOMPARE(applet->height(), height);
        QVERIFY(!gaugeNamed(applet, QStringLiteral("Compression gauge"))->isHidden());
        QVERIFY(pagesKeepTheirPolicy());
    }

    // ── A free canvas ────────────────────────────────────────────────────

    // A free canvas sizes an entry once and a page click doesn't resize it,
    // so with page buttons showing the rect must hold either page. Through
    // the real host: build the copy, turn the page, read that every row fits.
    void aFreeCanvasCopyFitsEitherPage_data()
    {
        QTest::addColumn<QString>("page");
        QTest::addColumn<QStringList>("hidden");
        QTest::newRow("phone, mic only") << QStringLiteral("phone")
                                         << allBut(kPhoneRows, QStringLiteral("mic"));
        // A guard: this one passes without the fix too.
        QTest::newRow("guard: phone, nothing hidden") << QStringLiteral("phone") << QStringList{};
        QTest::newRow("cw, nothing hidden") << QStringLiteral("cw") << QStringList{};
        QTest::newRow("cw, delay only") << QStringLiteral("cw")
                                        << allBut(kCwRows, QStringLiteral("cwDelay"));
        // A guard as well.
        QTest::newRow("guard: one row on each") << QStringLiteral("phone")
                                         << allBut(kPhoneRows, QStringLiteral("level"))
                                                + allBut(kCwRows, QStringLiteral("cwPan"));
    }

    void aFreeCanvasCopyFitsEitherPage()
    {
        QFETCH(QString, page);
        QFETCH(QStringList, hidden);

        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        ContainerContentHost host(registry);
        host.resize(500, 900);
        host.show();

        ContainerDocument document;
        document.id = QStringLiteral("main");
        document.layout = ContentLayout::FreeCanvas;
        ContentEntry entry = registry.makeEntry(QStringLiteral("phcw.panel"));
        entry.id = QStringLiteral("copy");
        entry.config = hiding(hidden, page);
        document.contents = {entry};
        host.reconcile(document);

        auto* copy = qobject_cast<PhoneCwApplet*>(host.entryRows().first().widget.data());
        QVERIFY(copy);
        QString problem;
        settleCopy(copy);
        QVERIFY2(everyRowFits(copy, &problem), qPrintable(problem));
        const int rectHeight = copy->height();

        const QString other = page == QLatin1String("phone") ? QStringLiteral("CW") : QStringLiteral("Phone");
        const QString back = page == QLatin1String("phone") ? QStringLiteral("Phone") : QStringLiteral("CW");
        buttonNamed(copy, other)->click();
        settleCopy(copy);
        QVERIFY(rowOf(copy, kPageButtons)->isVisible());
        QVERIFY2(everyRowFits(copy, &problem), qPrintable(problem));
        QCOMPARE(copy->height(), rectHeight);

        buttonNamed(copy, back)->click();
        settleCopy(copy);
        QVERIFY2(everyRowFits(copy, &problem), qPrintable(problem));
    }

    // Page buttons hidden, Page set to CW: the rect holds the CW page.
    void aFreeCanvasCopyWithoutButtonsFitsItsPage()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        ContainerContentHost host(registry);
        host.resize(500, 900);
        host.show();
        ContainerDocument document;
        document.id = QStringLiteral("main");
        document.layout = ContentLayout::FreeCanvas;
        ContentEntry entry = registry.makeEntry(QStringLiteral("phcw.panel"));
        entry.id = QStringLiteral("copy");
        entry.config = hiding({kPageButtons}, QStringLiteral("cw"));
        document.contents = {entry};
        host.reconcile(document);
        auto* copy = qobject_cast<PhoneCwApplet*>(host.entryRows().first().widget.data());
        QVERIFY(copy);
        settleCopy(copy);
        QCOMPARE(stackOf(copy)->currentIndex(), 1);
        QVERIFY(!rowOf(copy, kPageButtons)->isVisible());
        QString problem;
        QVERIFY2(everyRowFits(copy, &problem), qPrintable(problem));
        QVERIFY(rowOf(copy, QStringLiteral("cwDelay"))->isVisible());
    }

    // The way a person adds one: the settings dialog saves a rect straight
    // away, which a free canvas prefers over any hint. The rect comes from
    // the copy's reported height; other content keeps the usual 160 px.
    void anAddedCopyFitsAndItsRectSurvivesAReload()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        ContainerWorkspaceStore store(settings);
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        registry.registerWidgetFactory(
            QStringLiteral("test.plain"),
            {QStringLiteral("test.plain"), QStringLiteral("Plain"), false, true, {}, ContentGroup::PanelSections},
            [](const ContentEntry&, QWidget* parent, ContentRenderMode) -> QWidget* {
                return new QLabel(QStringLiteral("plain"), parent);
            });
        MeterPoller poller;
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        manager.setPreviewPoller(&poller);
        ContainerDocument container;
        container.id = QStringLiteral("A");
        container.layout = ContentLayout::FreeCanvas;
        WorkspaceDocument workspace;
        workspace.mainContainerId = container.id;
        workspace.containers = {container};
        QCOMPARE(manager.commitWorkspace(workspace, 0).status, CommitStatus::Saved);
        root.show();

        ContainerSettingsDialog dialog(manager.container(container.id), nullptr, &manager);
        dialog.resize(1300, 1000);
        dialog.show();
        QCoreApplication::processEvents();
        auto* available = dialog.findChild<QListWidget*>(QStringLiteral("containerAvailableContents"));
        QVERIFY(available);
        const auto add = [available](const QString& type) {
            for (int i = 0; i < available->count(); ++i) {
                QListWidgetItem* item = available->item(i);
                if (item->data(Qt::UserRole).toString() == type) {
                    available->setCurrentItem(item);
                    emit available->itemDoubleClicked(item);
                    return true;
                }
            }
            return false;
        };
        QVERIFY(add(QStringLiteral("phcw.panel")));
        QVERIFY(add(QStringLiteral("test.plain")));
        QVERIFY(add(QStringLiteral("phcw.panel")));
        WorkspaceDocument draft = dialog.editSession()->draft();
        QVector<ContentEntry>& added = draft.containers[0].contents;
        QCOMPARE(added.size(), 3);
        const QRectF first = added[0].freeCanvasRect().value();
        const QRectF plain = added[1].freeCanvasRect().value();
        const QRectF second = added[2].freeCanvasRect().value();
        QCOMPARE(plain.height(), 160.0);
        QVERIFY(first.height() > 160.0);
        // Each rect is saved, so the next add starts below it.
        QVERIFY(plain.top() >= first.bottom());
        QVERIFY(second.top() >= plain.bottom());
        QCOMPARE(second.height(), first.height());

        // The second copy hides all Phone rows but mic and keeps its rect.
        added[2].config = hiding(allBut(kPhoneRows, QStringLiteral("mic")));
        dialog.reject();
        draft.revision = store.snapshot().revision;
        QCOMPARE(manager.commitWorkspace(draft, draft.revision).status, CommitStatus::Saved);

        auto* host = manager.contentHost(container.id);
        QVERIFY(host);
        for (int index : {0, 2}) {
            auto* copy = qobject_cast<PhoneCwApplet*>(host->entryRows().at(index).widget.data());
            QVERIFY(copy);
            // What the rect was sized from is what the live copy reports.
            if (index == 0) {
                QCOMPARE(qreal(copy->property("freeCanvasHint").toSize().height()), first.height());
            }
            QString problem;
            for (const QString& page : {QStringLiteral("CW"), QStringLiteral("Phone"), QStringLiteral("CW")}) {
                buttonNamed(copy, page)->click();
                settleCopy(copy);
                QCOMPARE(qreal(copy->height()), first.height());
                QVERIFY2(everyRowFits(copy, &problem),
                         qPrintable(QStringLiteral("copy %1 on %2: ").arg(index).arg(page) + problem));
            }
        }

        // A rect the person sized stays as they sized it, through a save and
        // a reload, even when it's too small for the copy.
        WorkspaceDocument sized = store.snapshot();
        const QRectF chosen(10, 30, 340, 100);
        sized.containers[0].contents[0].setFreeCanvasRect(chosen);
        QCOMPARE(manager.commitWorkspace(sized, sized.revision).status, CommitStatus::Saved);
        const DocumentResult reloaded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(store.snapshot()));
        QCOMPARE(reloaded.document.containers[0].contents[0].freeCanvasRect().value(), chosen);
        QCOMPARE(reloaded.document.containers[0].contents[2].freeCanvasRect().value(), second);
        auto* resized = qobject_cast<PhoneCwApplet*>(host->entryRows().first().widget.data());
        QVERIFY(resized);
        settleCopy(resized);
        QCOMPARE(resized->height(), 100);
    }

    // With the page buttons hidden the page can't change, so the copy asks
    // for what it shows. Its minimum follows the same rule. This reads the
    // settings preview's copy, which has no CW status text: its hint can be
    // a line short of a live copy's when CW is the taller page, so the rect
    // an add saves is measured with the status text (see the test above).
    void aFreeCanvasCopyReportsTheSizeItNeeds()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);
        QWidget host;

        ContentEntry noButtons = registry.makeEntry(QStringLiteral("phcw.panel"));
        noButtons.config = hiding(QStringList{kPageButtons} + allBut(kPhoneRows, QStringLiteral("mic")));
        const std::unique_ptr<QWidget> shortView(registry.createPreview(noButtons, &host));
        QVERIFY(shortView);
        QCOMPARE(shortView->property("freeCanvasHint").toSize().height(), shortView->sizeHint().height());
        // Exactly the row and its page's margins, read before the copy is
        // ever shown: the MON row's stale height doesn't count.
        QWidget* mic = rowOf(qobject_cast<PhoneCwApplet*>(shortView.get()), QStringLiteral("mic"));
        const QMargins margins = mic->parentWidget()->layout()->contentsMargins();
        QCOMPARE(shortView->property("freeCanvasHint").toSize().height(),
                 margins.top() + mic->sizeHint().height() + margins.bottom());
        QCOMPARE(shortView->property("freeCanvasMinimum").toSizeF().height(),
                 qreal(shortView->minimumSizeHint().height()));

        ContentEntry withButtons = registry.makeEntry(QStringLiteral("phcw.panel"));
        withButtons.config = hiding(allBut(kPhoneRows, QStringLiteral("mic")));
        const std::unique_ptr<QWidget> tallView(registry.createPreview(withButtons, &host));
        QVERIFY(tallView);
        auto* copy = qobject_cast<PhoneCwApplet*>(tallView.get());
        QVERIFY(copy);
        QStackedWidget* stack = stackOf(copy);
        const int buttons = rowOf(copy, kPageButtons)->sizeHint().height();
        QCOMPARE(tallView->property("freeCanvasHint").toSize().height(),
                 buttons + stack->widget(1)->sizeHint().height());
        QCOMPARE(tallView->property("freeCanvasMinimum").toSizeF().height(),
                 qreal(rowOf(copy, kPageButtons)->minimumSizeHint().height()
                       + qMax(stack->widget(0)->minimumSizeHint().height(),
                              stack->widget(1)->minimumSizeHint().height())));
        // The stack path is not touched: it still follows the page shown.
        QVERIFY(tallView->sizeHint().height() < tallView->property("freeCanvasHint").toSize().height());
    }

    // ── Editor ───────────────────────────────────────────────────────────

    void advancedBindingsSaysTheCopyHasNoSource()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        ContainerContentRegistry registry;
        registerPhoneCwSectionContent(registry, &applet);

        ContentPropertyEditor editor(registry);
        editor.setEntry(registry.makeEntry(QStringLiteral("phcw.panel")));
        QGroupBox* advanced = nullptr;
        for (QGroupBox* group : editor.findChildren<QGroupBox*>()) {
            if (group->title() == QStringLiteral("Advanced bindings")) {
                advanced = group;
            }
        }
        QVERIFY(advanced);
        QVERIFY(!advanced->isEnabled());
        QCOMPARE(advanced->toolTip(), QStringLiteral(
            "These controls follow the Phone/CW panel; they have no source of their own."));
        auto* source = editor.findChild<QComboBox*>(QStringLiteral("contentSlice"));
        QVERIFY(source);
        QVERIFY(!source->isEnabled());

        // The toggles and the page stay live beside it.
        auto* toggle = editor.findChild<QCheckBox*>(
            QStringLiteral("property_") + SectionParts::hideKey(QStringLiteral("level")));
        QVERIFY(toggle);
        QVERIFY(toggle->isEnabled());
    }

    // ── Staying in step with the docked panel ────────────────────────────

    // A reading must not wipe the docked gauge's reason: a copy made after
    // MOX off on an older Core would draw live with no reason.
    void aReadingKeepsTheCompressionReason()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        const QString reason = QStringLiteral("This Core does not send this reading.");
        applet.setCompressionUnavailable(reason);
        radio.transmitModel().setMox(true);
        radio.transmitModel().setMox(false);
        applet.setCompressionReading(-30.0);

        HGauge* docked = gaugeNamed(&applet, QStringLiteral("Compression gauge"));
        QVERIFY(docked->isUnavailable());

        auto* copy = applet.createPanelCopy(&host, false);
        HGauge* gauge = gaugeNamed(copy, QStringLiteral("Compression gauge"));
        QVERIFY(gauge->isUnavailable());
        QCOMPARE(gauge->toolTip(), reason);
        QCOMPARE(gauge->accessibleDescription(), reason);

        // Made available again, the next copy is live with its own tooltip.
        applet.setCompressionUnavailable(QString());
        QVERIFY(!gauge->isUnavailable());
        auto* next = applet.createPanelCopy(&host, false);
        HGauge* nextGauge = gaugeNamed(next, QStringLiteral("Compression gauge"));
        QVERIFY(!nextGauge->isUnavailable());
        QCOMPARE(nextGauge->toolTip(), docked->toolTip());
    }

    // The DEXP marker lives in no model, so the panels pass it on directly.
    void theDexpMarkerStaysInStepEverywhere()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* first = applet.createPanelCopy(&host, false);
        std::unique_ptr<PhoneCwApplet> second(applet.createPanelCopy(nullptr, false));
        const QString name = QStringLiteral("PhoneCwDexpThresholdSlider");
        auto* docked = applet.findChild<QSlider*>(name);
        auto* one = first->findChild<QSlider*>(name);
        auto* two = second->findChild<QSlider*>(name);
        QVERIFY(docked);
        QVERIFY(one);
        QVERIFY(two);
        const QString key = QStringLiteral("PhoneCw/DexpThresholdMarkerDb");

        one->setValue(-77);
        QCOMPARE(docked->value(), -77);
        QCOMPARE(two->value(), -77);
        QCOMPARE(AppSettings::instance().value(key).toInt(), -77);

        docked->setValue(-33);
        QCOMPARE(one->value(), -33);
        QCOMPARE(two->value(), -33);
        QCOMPARE(AppSettings::instance().value(key).toInt(), -33);

        // A copy made now starts at the same value; a removed copy is out
        // of the chain.
        auto* third = applet.createPanelCopy(&host, false);
        auto* three = third->findChild<QSlider*>(name);
        QCOMPARE(three->value(), -33);
        second.reset();
        three->setValue(-12);
        QCOMPARE(docked->value(), -12);
        QCOMPARE(one->value(), -12);
        QCOMPARE(AppSettings::instance().value(key).toInt(), -12);
    }

    // A copy built in straight-key mode brings back the iambic letter the
    // docked panel would.
    void aCopyRestoresTheDockedPanelsIambicMode()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        TransmitModel& tx = radio.transmitModel();
        tx.setCwKeyerMode(static_cast<int>(CwKeyerMode::IambicA));
        tx.setCwKeyerMode(static_cast<int>(CwKeyerMode::Straight));

        auto* copy = applet.createPanelCopy(&host, false);
        auto* iambic = copy->findChild<QPushButton*>(QStringLiteral("cwIambicButton"));
        QVERIFY(iambic);
        QVERIFY(!iambic->isChecked());
        iambic->click();
        QCOMPARE(tx.cwKeyerMode(), static_cast<int>(CwKeyerMode::IambicA));
        auto* dockedIambic = applet.findChild<QPushButton*>(QStringLiteral("cwIambicButton"));
        QVERIFY(dockedIambic->isChecked());
    }

    // Removing a copy while the setting moves: the other copy still follows.
    void removingACopyLeavesTheOtherFollowing()
    {
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QWidget host;
        auto* kept = applet.createPanelCopy(&host, false);
        std::unique_ptr<PhoneCwApplet> removed(applet.createPanelCopy(nullptr, false));
        const QString name = QStringLiteral("Microphone gain");
        const auto gainOf = [&name](QWidget* panel) -> QSlider* {
            for (QSlider* slider : panel->findChildren<QSlider*>()) {
                if (slider->accessibleName() == name) {
                    return slider;
                }
            }
            return nullptr;
        };
        TransmitModel& tx = radio.transmitModel();
        tx.setMicGainDb(-3);
        QCOMPARE(gainOf(kept)->value(), gainOf(&applet)->value());
        QCOMPARE(gainOf(removed.get())->value(), gainOf(&applet)->value());

        removed.reset();
        tx.setMicGainDb(5);
        QCOMPARE(gainOf(kept)->value(), gainOf(&applet)->value());
        applet.setCompressionReading(-10.0);
        applet.setCompressionUnavailable(QStringLiteral("no reading"));
        QVERIFY(gaugeNamed(kept, QStringLiteral("Compression gauge"))->isUnavailable());
        applet.syncFromModel();
    }

    // ── A remote window ──────────────────────────────────────────────────

    void aCopyOnARemoteWindowIsGatedLikeTheDockedPanel()
    {
        RadioModel remote(RadioModel::Role::Remote);
        PhoneCwApplet applet(&remote);
        QWidget host;
        QString difference;

        // As the window opens: everything the Core has not allowed is shut.
        auto* opening = applet.createPanelCopy(&host, false);
        QVERIFY2(sameGates(&applet, opening, &difference), qPrintable(difference));
        const QString proc = QStringLiteral("PhoneCwProcButton");
        QVERIFY(!applet.findChild<QPushButton*>(proc)->isEnabled());
        QVERIFY(!opening->findChild<QPushButton*>(proc)->isEnabled());
        QVERIFY(!opening->findChild<QSlider*>(QStringLiteral("cwSpeedSlider"))->isEnabled());

        // The docked panel's reasons reach a copy already there and a copy
        // made afterward.
        const QString transmitWhy = QStringLiteral("transmit reason");
        const QString settingsWhy = QStringLiteral("settings reason");
        const QString profileWhy = QStringLiteral("profile reason");
        const QString cwWhy = QStringLiteral("CW reason");
        applet.setTransmitPermitted(false, transmitWhy);
        applet.setTransmitSettingsPermitted(false, settingsWhy);
        applet.setTxProfilePermitted(false, profileWhy);
        applet.setCwSettingsPermitted(false, cwWhy);
        QVERIFY2(sameGates(&applet, opening, &difference), qPrintable(difference));
        auto* later = applet.createPanelCopy(&host, false);
        QVERIFY2(sameGates(&applet, later, &difference), qPrintable(difference));
        QCOMPARE(later->findChild<QPushButton*>(proc)->toolTip(), settingsWhy);
        QCOMPARE(later->findChild<QSlider*>(QStringLiteral("cwSpeedSlider"))->toolTip(), cwWhy);

        // Opened by the Core: the same again, and a new copy opens with it.
        applet.setTransmitPermitted(true);
        applet.setTransmitSettingsPermitted(true);
        applet.setTxProfilePermitted(true);
        applet.setCwSettingsPermitted(true);
        QVERIFY(applet.findChild<QPushButton*>(proc)->isEnabled());
        QVERIFY2(sameGates(&applet, opening, &difference), qPrintable(difference));
        QVERIFY2(sameGates(&applet, later, &difference), qPrintable(difference));
        auto* opened = applet.createPanelCopy(&host, false);
        QVERIFY2(sameGates(&applet, opened, &difference), qPrintable(difference));
        QVERIFY(opened->findChild<QPushButton*>(proc)->isEnabled());
    }
};

QTEST_MAIN(TstPhoneCwSectionContent)
#include "tst_phone_cw_section_content.moc"
