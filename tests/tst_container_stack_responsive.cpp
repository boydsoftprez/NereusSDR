// no-port-check: Observe production stack allocations and Qt lettering offline.
// Modification history (NereusSDR):
//   2026-10-04 — Live stack resize regressions by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFontMetricsF>
#include <QJsonDocument>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"

using namespace NereusSDR;

namespace {
class HintedApplet final : public QWidget {
public:
    explicit HintedApplet(QWidget* parent) : QWidget(parent) {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    QSize sizeHint() const override { return {180, 48}; }
};

// This fixture exercises the manager's real floating shell and arrange margins.
// It has no MainWindow, discovery, RadioModel, poller or settings singleton.
struct StackFixture {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath("stack-settings.xml")};
    ContainerWorkspaceStore store{settings};
    QWidget appletOwner;
    HintedApplet applet{&appletOwner};
    QWidget dock;
    QSplitter splitter;
    ContainerContentRegistry registry;
    ContainerManager manager{&dock, &splitter};

    StackFixture() {
        dock.resize(1200, 1000);
        registry.attachSingleton("applet:rx", &applet);
        manager.setWorkspaceAdapter(&store, &registry);
    }
    WorkspaceDocument document(bool autoHeight = false, bool mixed = false) {
        WorkspaceDocument result;
        result.mainContainerId = "responsive-stack";
        ContainerDocument container;
        container.id = result.mainContainerId;
        container.name = "Synthetic offline Slice A stack";
        container.layout = ContentLayout::VerticalStack;
        container.dockMode = DockMode::Floating;
        container.header = HeaderMode::Always;
        container.geometry = QRect(40, 40, 640, 450);
        container.autoHeight = autoHeight;
        const QStringList variants{"signalMaxBin", "comp", "eq", "leveler", "cfc",
                                   "cfcGain", "levelerGain", "alcGain", "alcGroup"};
        for (int index = 0; index < variants.size(); ++index) {
            ContentEntry entry = registry.makeEntry("meter." + variants[index]);
            entry.id = variants[index];
            entry.context["sliceId"] = mixed && index >= 6 ? 1 : 0;
            entry.canvasRect = QRectF(.13, .24, .61, .18);
            QJsonObject properties = entry.config["properties"].toObject();
            // Unknown/imported typography must survive presentation resizing.
            properties["importedFont"] = QJsonObject{{"family", "Arial"},
                {"pointSize", 17.25}, {"weight", 600}, {"italic", true}};
            properties["futureSetting"] = QJsonObject{{"retained", "verbatim"}};
            if (index == 0) { properties["rowHeight"] = 120; }
            entry.config["properties"] = properties;
            entry.config["legacyRecord"] = QStringLiteral(" \n") +
                QString::fromUtf8(QJsonDocument(properties).toJson(QJsonDocument::Compact)) +
                QStringLiteral("\n ");
            entry.extensions["futureEntrySetting"] = "retained";
            container.contents.append(entry);
            if (mixed && index == 5) {
                ContentEntry control = registry.makeEntry("control.monitor");
                control.id = "compact-monitor";
                control.context["sliceId"] = 0;
                container.contents.append(control);
                ContentEntry borrowed = registry.makeEntry("applet:rx");
                borrowed.id = "hinted-rx";
                container.contents.append(borrowed);
            }
        }
        result.containers.append(container);
        return result;
    }
    ContainerContentHost* host() { return manager.contentHost("responsive-stack"); }
    QWidget* shell() { return manager.container("responsive-stack")->window(); }
};

void settleLayouts(QWidget* shell) {
    // Drain queued layout/grip projection, without sleeping or waiting on a
    // radio cadence. All resizing below uses actual post-layout dimensions.
    for (int pass = 0; pass < 8; ++pass) {
        if (shell->layout()) { shell->layout()->activate(); }
        for (QWidget* child : shell->findChildren<QWidget*>()) {
            if (child->layout()) { child->layout()->activate(); }
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

struct Lettering {
    QString value;
    QFont font;
    QRectF ink;
};
class FontObserverEngine final : public QPaintEngine {
public:
    explicit FontObserverEngine(int dpr) : QPaintEngine(AllFeatures), m_dpr(dpr) {}
    QVector<Lettering> letters;
    QRectF cachedScaleInk;
    bool begin(QPaintDevice* device) override {
        setPaintDevice(device); setActive(true); return true;
    }
    bool end() override { setActive(false); return true; }
    void updateState(const QPaintEngineState&) override {}
    Type type() const override { return User; }
    void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override {}
    void drawImage(const QRectF& destination, const QImage& image, const QRectF&, Qt::ImageConversionFlags) override {
        // Bar scales arrive as a cached image. Observe its actual endpoint/tick
        // pixels as well as top-level text, in the same leaf logical space.
        int left = image.width(), top = image.height(), right = -1, bottom = -1;
        for (int y = 0; y < image.height(); ++y) {
            const QRgb* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(pixels[x]) > 64) {
                    left = qMin(left, x); right = qMax(right, x);
                    top = qMin(top, y); bottom = qMax(bottom, y);
                }
            }
        }
        if (right >= left && bottom >= top) {
            const QRectF logical(destination.left() + left / image.devicePixelRatioF(),
                                 destination.top() + top / image.devicePixelRatioF(),
                                 (right - left + 1) / image.devicePixelRatioF(),
                                 (bottom - top + 1) / image.devicePixelRatioF());
            const QRectF physical = state->transform().mapRect(logical);
            cachedScaleInk = QRectF(physical.topLeft() / m_dpr, physical.size() / m_dpr);
        }
    }
    void drawPath(const QPainterPath&) override {}
    void drawPolygon(const QPointF*, int, PolygonDrawMode) override {}
    void drawTextItem(const QPointF& point, const QTextItem& item) override {
        QPainterPath path;
        path.addText(point, item.font(), item.text());
        const QRectF physical = state->transform().mapRect(path.boundingRect());
        letters.append({item.text(), item.font(),
            QRectF(physical.topLeft() / m_dpr, physical.size() / m_dpr)});
    }
private:
    int m_dpr;
};
class FontObserverDevice final : public QPaintDevice {
public:
    FontObserverDevice(QSize size, int dpr) : m_size(size), m_dpr(dpr), m_engine(dpr) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QVector<Lettering>& letters() const { return m_engine.letters; }
    QRectF cachedScaleInk() const { return m_engine.cachedScaleInk; }
protected:
    int metric(PaintDeviceMetric metric) const override {
        switch (metric) {
        case PdmWidth: return m_size.width() * m_dpr;
        case PdmHeight: return m_size.height() * m_dpr;
        case PdmDpiX: case PdmDpiY: case PdmPhysicalDpiX: case PdmPhysicalDpiY: return 96;
        case PdmDepth: return 32;
        case PdmDevicePixelRatio: return m_dpr;
        case PdmDevicePixelRatioScaled: return m_dpr * devicePixelRatioFScale();
        default: return QPaintDevice::metric(metric);
        }
    }
private:
    QSize m_size;
    int m_dpr;
    mutable FontObserverEngine m_engine;
};

struct RowObservation {
    QString id;
    QSize allocation;
    int preferredHeight = 0;
    int titlePixels = -1;
    QRectF titleInk;
    QRectF paintedRow;
    QRectF cachedScaleInk;
    QImage raster;
};
QPoint mapBetweenWidgets(const QWidget* source, const QWidget* target, const QPoint& point) {
    // mapTo/mapFrom(QWidget*) require an ancestor. The host and its viewport
    // need a common coordinate space, including after the body has scrolled.
    return source->mapToGlobal(point) - target->mapToGlobal(QPoint(0, 0));
}
QString rectDescription(const QRectF& rect) {
    return QStringLiteral("(%1,%2 %3x%4)")
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}
QString rowDescription(const RowObservation& row) {
    // Allocations, painted row bounds and observed ink are all logical pixels;
    // paintedRow/titleInk share the leaf canvas origin, including for DPR2.
    return QStringLiteral("%1 allocation=%2x%3 title=%4 base=%5 row=%6 ink=%7 scaleInk=%8")
        .arg(row.id).arg(row.allocation.width()).arg(row.allocation.height())
        .arg(row.titlePixels).arg(row.preferredHeight)
        .arg(rectDescription(row.paintedRow)).arg(rectDescription(row.titleInk))
        .arg(rectDescription(row.cachedScaleInk));
}
void logRowPairs(const QVector<RowObservation>& first, const QVector<RowObservation>& second,
                 const QString& firstLabel, const QString& secondLabel) {
    // Log every family before a failing assertion returns from the test slot.
    for (int index = 0; index < qMin(first.size(), second.size()); ++index) {
        qInfo().noquote() << firstLabel + " " + rowDescription(first[index]) +
            " " + secondLabel + " " + rowDescription(second[index]);
    }
}
QVector<RowObservation> observeBars(ContainerContentHost* host, int dpr, const QString& stage = {}) {
    QVector<RowObservation> result;
    for (const auto& row : host->entryRows()) {
        auto* face = qobject_cast<BarPresetItem*>(row.item.data());
        if (!face) { continue; }
        const QSize canvas = row.widget->size();
        FontObserverDevice device(canvas, dpr);
        QPainter observer(&device);
        QFont owning("Arial");
        owning.setItalic(true);
        observer.setFont(owning);
        face->paint(observer, canvas.width(), canvas.height());
        observer.end();
        RowObservation observation;
        observation.id = row.entryId;
        observation.allocation = host->entryBoundary(row.entryId).size();
        observation.paintedRow = QRectF(mapBetweenWidgets(host, row.widget,
            host->entryBoundary(row.entryId).topLeft()), observation.allocation);
        observation.cachedScaleInk = device.cachedScaleInk();
        observation.preferredHeight = face->preferredRowHeight();
        const QString title = face->configuration()["label"].toString();
        for (const auto& letter : device.letters()) {
            if (letter.value == title) {
                observation.titlePixels = letter.font.pixelSize();
                observation.titleInk = letter.ink;
            }
        }
        observation.raster = QImage(canvas * dpr, QImage::Format_ARGB32_Premultiplied);
        observation.raster.setDevicePixelRatio(dpr);
        observation.raster.fill(Qt::transparent);
        QPainter raster(&observation.raster);
        raster.setFont(owning);
        face->paint(raster, canvas.width(), canvas.height());
        raster.end();
        const QString capturePath = qEnvironmentVariable("NEREUS_STACK_CAPTURE_DIR");
        if (!capturePath.isEmpty() && !stage.isEmpty()) {
            QDir capture(capturePath);
            const QString name = QStringLiteral("%1-%2-%3-%4x%5-DPR%6.png")
                .arg(QString::fromLatin1(QTest::currentTestFunction())).arg(stage).arg(observation.id)
                .arg(observation.allocation.width()).arg(observation.allocation.height()).arg(dpr);
            const QRect physicalRow(qRound(observation.paintedRow.x() * dpr),
                                    qRound(observation.paintedRow.y() * dpr),
                                    observation.allocation.width() * dpr,
                                    observation.allocation.height() * dpr);
            if (!capture.mkpath(".") || !observation.raster.copy(physicalRow).save(capture.filePath(name))) {
                QTest::qFail(qPrintable("Could not capture fixture raster: " + capture.filePath(name)), __FILE__, __LINE__);
            }
        }
        result.append(observation);
    }
    return result;
}

QString geometryDescription(StackFixture& fixture, QScrollArea* scroll) {
    QString description = QStringLiteral("shell=%1x%2 host=%3x%4 viewport=%5x%6 body=%7x%8 vbar=%9")
        .arg(fixture.shell()->width()).arg(fixture.shell()->height())
        .arg(fixture.host()->width()).arg(fixture.host()->height())
        .arg(scroll->viewport()->width()).arg(scroll->viewport()->height())
        .arg(scroll->widget()->width()).arg(scroll->widget()->height())
        .arg(scroll->verticalScrollBar()->isVisible());
    description += QStringLiteral(" hostInShell=%1 viewportInShell=%2")
        .arg(rectDescription(QRectF(mapBetweenWidgets(fixture.host(), fixture.shell(), QPoint()),
                                   fixture.host()->size())))
        .arg(rectDescription(QRectF(mapBetweenWidgets(scroll->viewport(), fixture.shell(), QPoint()),
                                   scroll->viewport()->size())));
    for (MeterWidget* meter : fixture.host()->meterSurfaces()) {
        description += QStringLiteral(" leaf=%1x%2 minWidth=%3")
            .arg(meter->width()).arg(meter->height()).arg(meter->minimumWidth());
    }
    return description;
}
} // namespace

class TstContainerStackResponsive : public QObject {
    Q_OBJECT
private:
    void verifyObservedInk(const QVector<RowObservation>& rows) {
        for (const auto& row : rows) {
            const QString details = rowDescription(row);
            QVERIFY2(row.titlePixels > 0 && !row.titleInk.isEmpty(), qPrintable(details));
            QVERIFY2(!row.cachedScaleInk.isEmpty(), qPrintable(details));
            QVERIFY2(row.paintedRow.adjusted(-1, -1, 1, 1).contains(row.titleInk), qPrintable(details));
            QVERIFY2(row.paintedRow.adjusted(-1, -1, 1, 1).contains(row.cachedScaleInk), qPrintable(details));
        }
    }
    void verifyRetainedContents(StackFixture& fixture, const QVector<ContentEntry>& original) {
        // Outer geometry/revision may legitimately persist after a shell resize.
        // Compare content bytes/values, rather than whole WorkspaceDocument.
        QCOMPARE(fixture.store.snapshot().containers.first().contents, original);
        const auto captured = fixture.host()->captureDocument().contents;
        QCOMPARE(captured.size(), original.size());
        for (int index = 0; index < original.size(); ++index) {
            QCOMPARE(captured[index].config, original[index].config);
            QCOMPARE(captured[index].canvasRect, original[index].canvasRect);
            QCOMPARE(captured[index].extensions, original[index].extensions);
        }
    }
    void verifyHorizontalContainment(StackFixture& fixture, int leftMargin = 20) {
        auto* scroll = fixture.host()->findChild<QScrollArea*>();
        QVERIFY(scroll);
        qInfo().noquote() << geometryDescription(fixture, scroll);
        const QRect viewport = scroll->viewport()->rect();
        for (const auto& row : fixture.host()->entryRows()) {
            if (!row.item) { continue; }
            const QRect boundary = fixture.host()->entryBoundary(row.entryId);
            const QPoint origin = mapBetweenWidgets(fixture.host(), scroll->viewport(), boundary.topLeft());
            const QRect mapped(origin, boundary.size());
            const QString details = row.entryId + " hostRow=" + rectDescription(boundary) +
                " viewportRow=" + rectDescription(mapped) + " " + geometryDescription(fixture, scroll);
            qInfo().noquote() << details;
            // Manager installs an arrange controller:20 left grip,8 right margin.
            // Rows may scroll vertically; no row may be swallowed horizontally.
            QVERIFY2(mapped.left() >= leftMargin, qPrintable(details));
            QVERIFY2(mapped.right() <= viewport.right() - 8, qPrintable(details));
        }
    }
private slots:
    void narrowFloatingStackKeepsItsRightEdge() {
        StackFixture fixture;
        QVERIFY(fixture.directory.isValid());
        const auto document = fixture.document();
        const auto original = document.containers.first().contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        QVERIFY(fixture.host());
        fixture.shell()->show();
        for (int width : {640, 320, 260, 640}) {
            fixture.shell()->resize(width, 180);
            settleLayouts(fixture.shell());
            auto* scroll = fixture.host()->findChild<QScrollArea*>();
            QVERIFY(scroll);
            QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
            verifyHorizontalContainment(fixture);
            verifyRetainedContents(fixture, original);
        }
        // Remove the need for a vertical scrollbar at the final wide allocation.
        fixture.shell()->resize(640, 1000);
        settleLayouts(fixture.shell());
        verifyHorizontalContainment(fixture);
        verifyRetainedContents(fixture, original);
    }
    void manualStackAllocatesReachableResponsiveArea_data() {
        QTest::addColumn<int>("dpr");
        QTest::newRow("logical-DPR1") << 1;
        QTest::newRow("logical-DPR2") << 2;
    }
    void manualStackAllocatesReachableResponsiveArea() {
        QFETCH(int, dpr);
        StackFixture fixture;
        QVERIFY(fixture.directory.isValid());
        const auto document = fixture.document();
        const auto original = document.containers.first().contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        fixture.shell()->show();
        fixture.shell()->resize(640, 900);
        settleLayouts(fixture.shell());
        const auto large = observeBars(fixture.host(), dpr, "large");
        QCOMPARE(large.size(), 9);
        fixture.shell()->resize(320, 450);
        settleLayouts(fixture.shell());
        const auto small = observeBars(fixture.host(), dpr, "small");
        QCOMPARE(small.size(), large.size());
        logRowPairs(large, small, "large", "small");
        verifyObservedInk(large);
        verifyObservedInk(small);
        for (int index = 0; index < large.size(); ++index) {
            const auto& before = large[index];
            const auto& after = small[index];
            QCOMPARE(before.id, after.id);
            QVERIFY(before.titlePixels > 0);
            QVERIFY(after.titlePixels > 0);
            const QString details = QStringLiteral("%1 large=%2x%3 title=%4 small=%5x%6 title=%7")
                .arg(before.id).arg(before.allocation.width()).arg(before.allocation.height())
                .arg(before.titlePixels).arg(after.allocation.width()).arg(after.allocation.height())
                .arg(after.titlePixels);
            // Both available dimensions changed. This deliberately avoids
            // choosing a minimum font or a manual short-height shrink policy.
            QVERIFY2(before.allocation.width() > after.allocation.width(), qPrintable(details));
            QVERIFY2(before.allocation.height() > after.allocation.height(), qPrintable(details));
            QVERIFY2(before.titlePixels > after.titlePixels, qPrintable(details));
            QVERIFY2(before.paintedRow.adjusted(-1, -1, 1, 1).contains(before.titleInk), qPrintable(details));
            QVERIFY2(after.paintedRow.adjusted(-1, -1, 1, 1).contains(after.titleInk), qPrintable(details));
            QCOMPARE(before.preferredHeight, after.preferredHeight);
        }
        verifyRetainedContents(fixture, original);
        fixture.shell()->resize(640, 900);
        settleLayouts(fixture.shell());
        const auto returned = observeBars(fixture.host(), dpr, "returned");
        QCOMPARE(returned.size(), large.size());
        for (int index = 0; index < large.size(); ++index) {
            QCOMPARE(returned[index].allocation, large[index].allocation);
            QCOMPARE(returned[index].titlePixels, large[index].titlePixels);
            QCOMPARE(returned[index].raster, large[index].raster);
        }
        verifyHorizontalContainment(fixture);
        verifyRetainedContents(fixture, original);
    }
    void autoHeightFollowsNaturalResponsiveRows() {
        StackFixture fixture;
        QVERIFY(fixture.directory.isValid());
        const auto document = fixture.document(true);
        const auto original = document.containers.first().contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        fixture.shell()->show();
        fixture.shell()->resize(320, fixture.shell()->height());
        settleLayouts(fixture.shell());
        const auto narrow = observeBars(fixture.host(), 1, "narrow");
        const int narrowPreferred = fixture.host()->preferredContentHeight();
        fixture.shell()->resize(640, fixture.shell()->height());
        settleLayouts(fixture.shell());
        const auto wide = observeBars(fixture.host(), 1, "wide");
        QCOMPARE(wide.size(), 9);
        QCOMPARE(narrow.size(), wide.size());
        logRowPairs(narrow, wide, "natural narrow", "wide");
        verifyObservedInk(narrow);
        verifyObservedInk(wide);
        for (int index = 0; index < wide.size(); ++index) {
            const QString details = QStringLiteral("%1 natural narrow=%2x%3 wide=%4x%5 base=%6")
                .arg(wide[index].id).arg(narrow[index].allocation.width())
                .arg(narrow[index].allocation.height()).arg(wide[index].allocation.width())
                .arg(wide[index].allocation.height()).arg(wide[index].preferredHeight);
            QVERIFY2(wide[index].allocation.height() > narrow[index].allocation.height(), qPrintable(details));
            QVERIFY2(wide[index].titlePixels > narrow[index].titlePixels, qPrintable(details));
            QCOMPARE(wide[index].preferredHeight, index == 0 ? 120 : 72);
        }
        QVERIFY(fixture.host()->preferredContentHeight() > narrowPreferred);
        // At auto-height, content chooses natural height. Extra manually supplied
        // height must not feed back into ever-growing natural row/font sizes.
        const int natural = fixture.host()->preferredContentHeight();
        QCOMPARE(fixture.shell()->height(), natural + ContainerWidget::kTitleBarHeight);
        settleLayouts(fixture.shell());
        QCOMPARE(fixture.host()->preferredContentHeight(), natural);
        verifyHorizontalContainment(fixture);
        verifyRetainedContents(fixture, original);
    }
    void heightOnlyGrowthAndScrollbarTransition_data() {
        QTest::addColumn<int>("dpr");
        QTest::addColumn<bool>("arrange");
        QTest::newRow("DPR1-arrange") << 1 << true;
        QTest::newRow("DPR2-arrange") << 2 << true;
        QTest::newRow("DPR1-no-arrange") << 1 << false;
        QTest::newRow("DPR2-no-arrange") << 2 << false;
    }
    void heightOnlyGrowthAndScrollbarTransition() {
        // Fixed run heights would keep the titles unchanged when only the
        // shell height grows. An unreserved scrollbar would crop narrow rows.
        QFETCH(int, dpr);
        QFETCH(bool, arrange);
        StackFixture fixture;
        const auto document = fixture.document();
        const auto original = document.containers.first().contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        if (!arrange) { fixture.host()->setArrangeController(nullptr); }
        fixture.shell()->show();
        fixture.shell()->resize(640, 180);
        settleLayouts(fixture.shell());
        auto* scroll = fixture.host()->findChild<QScrollArea*>();
        QVERIFY(scroll);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        const auto shortRows = observeBars(fixture.host(), dpr, "short");
        QCOMPARE(shortRows.size(), 9);
        verifyHorizontalContainment(fixture, arrange ? 20 : 0);
        fixture.shell()->resize(640, 900);
        settleLayouts(fixture.shell());
        const auto tallerRows = observeBars(fixture.host(), dpr, "taller");
        QCOMPARE(tallerRows.size(), shortRows.size());
        logRowPairs(shortRows, tallerRows, "height-only short", "taller");
        verifyObservedInk(shortRows);
        verifyObservedInk(tallerRows);
        for (int index = 0; index < shortRows.size(); ++index) {
            QVERIFY2(tallerRows[index].allocation.height() > shortRows[index].allocation.height(),
                     qPrintable(rowDescription(tallerRows[index])));
            // The long custom-height SignalMaxBin title can be width-limited;
            // short default-height labels must grow with available height.
            if (index > 0) {
                QVERIFY2(tallerRows[index].titlePixels > shortRows[index].titlePixels,
                         qPrintable(rowDescription(tallerRows[index])));
            }
        }
        fixture.shell()->resize(640, 1800);
        settleLayouts(fixture.shell());
        QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        verifyObservedInk(observeBars(fixture.host(), dpr, "tall"));
        verifyHorizontalContainment(fixture, arrange ? 20 : 0);
        verifyRetainedContents(fixture, original);
        fixture.shell()->resize(260, 180);
        settleLayouts(fixture.shell());
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        verifyObservedInk(observeBars(fixture.host(), dpr, "narrow"));
        verifyHorizontalContainment(fixture, arrange ? 20 : 0);
        fixture.shell()->resize(640, 180);
        settleLayouts(fixture.shell());
        const auto returned = observeBars(fixture.host(), dpr, "returned");
        QCOMPARE(returned.size(), shortRows.size());
        for (int index = 0; index < returned.size(); ++index) {
            QCOMPARE(returned[index].allocation, shortRows[index].allocation);
            QCOMPARE(returned[index].titlePixels, shortRows[index].titlePixels);
            QCOMPARE(returned[index].raster, shortRows[index].raster);
        }
        verifyRetainedContents(fixture, original);
    }
    void mixedRunsKeepCompactControlsAndBorrowedAppletHints() {
        StackFixture fixture;
        QVERIFY(fixture.directory.isValid());
        const auto document = fixture.document(false, true);
        const auto original = document.containers.first().contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        fixture.shell()->show();
        fixture.shell()->resize(320, 450);
        settleLayouts(fixture.shell());
        const auto narrow = observeBars(fixture.host(), 1);
        const int appletHeight = fixture.applet.height();
        int compactHeight = -1;
        for (const auto& row : fixture.host()->entryRows()) {
            if (row.entryId == "compact-monitor") { compactHeight = row.height; }
        }
        QVERIFY(compactHeight > 0);
        QCOMPARE(fixture.host()->meterSurfaces().size(), 2);
        QCOMPARE(fixture.host()->sourceContext(fixture.host()->meterSurfaces()[0])["sliceId"].toInt(), 0);
        QCOMPARE(fixture.host()->sourceContext(fixture.host()->meterSurfaces()[1])["sliceId"].toInt(), 1);
        fixture.shell()->resize(640, 900);
        settleLayouts(fixture.shell());
        const auto wide = observeBars(fixture.host(), 1);
        QCOMPARE(wide.size(), narrow.size());
        logRowPairs(narrow, wide, "mixed narrow", "wide");
        for (int index = 0; index < wide.size(); ++index) {
            QVERIFY2(wide[index].allocation.height() > narrow[index].allocation.height(), qPrintable(wide[index].id));
        }
        QCOMPARE(fixture.applet.height(), appletHeight);
        QVERIFY(fixture.applet.height() <= fixture.applet.sizeHint().height());
        for (const auto& row : fixture.host()->entryRows()) {
            if (row.entryId == "compact-monitor") { QCOMPARE(row.height, compactHeight); }
        }
        verifyHorizontalContainment(fixture);
        verifyRetainedContents(fixture, original);
    }
    void visiblePlaceholderReservesItsRow_data() {
        QTest::addColumn<QString>("typeId");
        QTest::addColumn<bool>("available");
        QTest::addColumn<bool>("visible");
        QTest::addColumn<bool>("autoHeight");
        QTest::addColumn<bool>("longReason");
        QTest::newRow("unavailable-meter-manual") << QString("meter.offlineFuture") << false << true << false << false;
        QTest::newRow("unavailable-applet-manual") << QString("applet:s_meter") << false << true << false << false;
        QTest::newRow("waiting-applet-manual") << QString("applet:s_meter") << true << true << false << false;
        QTest::newRow("hidden-unavailable-meter") << QString("meter.offlineFuture") << false << false << false << false;
        QTest::newRow("hidden-unavailable-applet") << QString("applet:s_meter") << false << false << false << false;
        QTest::newRow("unavailable-meter-auto-height") << QString("meter.offlineFuture") << false << true << true << false;
        QTest::newRow("long-reason-narrow-wrap") << QString("meter.alc") << false << true << false << true;
    }
    void visiblePlaceholderReservesItsRow() {
        // Omitting the visible placeholder from fixed space would assign the
        // whole650px viewport to five bars, then overflow/collapse the label.
        QFETCH(QString, typeId);
        QFETCH(bool, available);
        QFETCH(bool, visible);
        QFETCH(bool, autoHeight);
        QFETCH(bool, longReason);
        StackFixture fixture;
        const QString reason = longReason ? QStringLiteral("Unavailable offline fixture with a retained explanation that must wrap fully at the allocated width. ").repeated(8)
            : QStringLiteral("Unavailable offline fixture");
        if (!available) { fixture.registry.setAvailable(typeId, false, reason); }
        auto document = fixture.document(autoHeight);
        auto& contents = document.containers.first().contents;
        contents = contents.mid(1, 5); // five default72px native rows; base sum360
        ContentEntry placeholder = fixture.registry.makeEntry(typeId);
        placeholder.id = "offline-placeholder";
        placeholder.name = "Offline";
        placeholder.visible = visible;
        placeholder.extensions["futurePlaceholder"] = QJsonObject{{"opaque", "preserve"}};
        contents.append(placeholder);
        const auto original = contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        fixture.shell()->show();
        fixture.shell()->resize(548, 672); //520 usable width and650 manual height
        settleLayouts(fixture.shell());
        auto* scroll = fixture.host()->findChild<QScrollArea*>();
        QVERIFY(scroll);
        QLabel* label = nullptr;
        for (const auto& row : fixture.host()->entryRows()) {
            if (row.entryId == placeholder.id) { label = qobject_cast<QLabel*>(row.widget.data()); }
        }
        QVERIFY(label);
        const auto tall = observeBars(fixture.host(), 1, "placeholder-tall");
        QCOMPARE(tall.size(), 5);
        int barHeight = 0;
        for (const auto& row : tall) { barHeight += row.allocation.height(); }
        const int preferred = fixture.host()->preferredContentHeight();
        qInfo().noquote() << geometryDescription(fixture, scroll)
            << "visible=" << visible << "autoHeight=" << autoHeight
            << "placeholder=" << label->size() << "hint=" << label->sizeHint()
            << "bars=" << barHeight << "preferred=" << preferred;
        QCOMPARE(label->isVisible(), visible);
        const int tallLabelHeight = label->height();
        if (visible) {
            const int requiredHeight = label->hasHeightForWidth() ? label->heightForWidth(label->width()) : label->sizeHint().height();
            QVERIFY(label->height() >= requiredHeight);
            QVERIFY(preferred > barHeight);
            QCOMPARE(preferred, barHeight + label->height());
        } else {
            QCOMPARE(preferred, barHeight);
        }
        QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        verifyHorizontalContainment(fixture);
        verifyRetainedContents(fixture, original);
        const QSize settledShell = fixture.shell()->size();
        settleLayouts(fixture.shell());
        QCOMPARE(fixture.shell()->size(), settledShell);
        QCOMPARE(fixture.host()->preferredContentHeight(), preferred);
        if (!autoHeight) {
            fixture.shell()->resize(longReason ? 260 : 548, 180);
            settleLayouts(fixture.shell());
            QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
            if (visible) {
                const int requiredHeight = label->hasHeightForWidth() ? label->heightForWidth(label->width()) : label->sizeHint().height();
                qInfo() << "placeholder narrow" << label->size() << "required height" << requiredHeight;
                QVERIFY(label->height() >= requiredHeight);
                if (longReason) { QVERIFY(label->height() > tallLabelHeight); }
            }
            fixture.shell()->resize(548, 672);
            settleLayouts(fixture.shell());
            const auto returned = observeBars(fixture.host(), 1, "placeholder-returned");
            QCOMPARE(returned.size(), tall.size());
            for (int index = 0; index < returned.size(); ++index) {
                QCOMPARE(returned[index].allocation, tall[index].allocation);
                QCOMPARE(returned[index].raster, tall[index].raster);
            }
            QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        }
        verifyRetainedContents(fixture, original);
    }
    void projectedBodyMinimumIsReleasedForCanvasLayouts() {
        // A stale responsive minimum would make an empty canvas scroll through
        // the former stack height after its views and queued work were released.
        StackFixture fixture;
        const auto document = fixture.document();
        const auto original = document.containers.first().contents;
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        fixture.shell()->show();
        auto* scroll = fixture.host()->findChild<QScrollArea*>();
        QVERIFY(scroll);
        for (ContentLayout layout : {ContentLayout::LegacyCanvas, ContentLayout::FreeCanvas}) {
            fixture.host()->reconcile(document.containers.first());
            fixture.shell()->resize(640, 1800);
            settleLayouts(fixture.shell());
            QVERIFY(scroll->widget()->minimumHeight() > 1000);
            // Resize queues a projection, then replace the rows before it runs.
            fixture.shell()->resize(640, 180);
            auto empty = document.containers.first();
            empty.layout = layout;
            empty.contents.clear();
            fixture.host()->reconcile(empty);
            settleLayouts(fixture.shell());
            QCOMPARE(fixture.host()->meterSurfaces().size(), 0);
            QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
            QVERIFY(scroll->widget()->minimumHeight() < 180);
        }
        fixture.host()->reconcile(document.containers.first());
        settleLayouts(fixture.shell());
        QCOMPARE(fixture.host()->meterSurfaces().size(), 1);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        verifyRetainedContents(fixture, original);
    }
};

QTEST_MAIN(TstContainerStackResponsive)
#include "tst_container_stack_responsive.moc"
