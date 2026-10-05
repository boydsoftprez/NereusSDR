// no-port-check: NereusSDR-original actual-dialog native scroll characterization.
// Modification history (NereusSDR):
//   2026-10-04 — Capture native preview scrolling before diagnostic redraws by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryDir>
#include <QWindow>
#include <memory>

#include "core/AppSettings.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {
constexpr int kFrameTimeoutMs = 3000;

QJsonArray rectJson(const QRect& rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}

QString pointerId(const void* pointer)
{
    return QString::number(reinterpret_cast<quintptr>(pointer), 16);
}

QRect dialogRect(QWidget* widget, QWidget* dialog)
{
    // Some logged widgets are siblings of the viewport rather than its
    // descendants. Global mapping handles those without assuming ancestry.
    return QRect(widget->mapToGlobal(QPoint()) - dialog->mapToGlobal(QPoint()), widget->size());
}

// Reads existing handles only. QWidget::winId() would create native widgets and
// could change the very ancestor/sibling promotion being investigated.
QJsonObject widgetTopology(QWidget* widget, QWidget* dialog, QWidget* viewport)
{
    QJsonObject result{
        {"class", QString::fromLatin1(widget->metaObject()->className())},
        {"objectName", widget->objectName()},
        {"widget", pointerId(widget)},
        {"parentWidget", pointerId(widget->parentWidget())},
        {"visible", widget->isVisible()},
        {"nativeAttribute", widget->testAttribute(Qt::WA_NativeWindow)},
        {"dontCreateNativeAncestors", widget->testAttribute(Qt::WA_DontCreateNativeAncestors)},
        {"internalWinId", QString::number(qulonglong(widget->internalWinId()))},
        {"widgetGeometry", rectJson(widget->geometry())},
        {"mappedDialogRect", rectJson(dialogRect(widget, dialog))},
        {"mappedViewportRect", rectJson(dialogRect(widget, viewport))}
    };
    QRect clip = dialogRect(widget, dialog).intersected(dialogRect(viewport, dialog));
    for (QWidget* ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
        clip = clip.intersected(dialogRect(ancestor, dialog));
        if (ancestor == dialog) {
            break;
        }
    }
    result["viewportAncestorIntersection"] = rectJson(clip);
    QJsonArray visibleRects;
    for (const QRect& rect : widget->visibleRegion()) {
        visibleRects.append(rectJson(rect.translated(widget->mapTo(dialog, QPoint()))));
    }
    result["widgetVisibleRegionInDialog"] = visibleRects;
    QWidget* nativeParent = widget->nativeParentWidget();
    result["nativeParentWidget"] = pointerId(nativeParent);
    if (nativeParent) {
        result["expectedNativeGeometry"] = rectJson(dialogRect(widget, nativeParent));
    }
    if (QWindow* window = widget->windowHandle()) {
        result["windowHandle"] = pointerId(window);
        result["windowParent"] = pointerId(window->parent());
        result["windowGeometry"] = rectJson(window->geometry());
        result["windowExposed"] = window->isExposed();
        result["windowSurfaceType"] = int(window->surfaceType());
        result["windowDpr"] = window->devicePixelRatio();
    }
#ifdef NEREUS_GPU_SPECTRUM
    if (MeterWidget* meter = qobject_cast<MeterWidget*>(widget)) {
        result["requestedRhiApi"] = int(meter->api());
    }
#endif
    return result;
}

bool saveTopology(const QString& directory, const QString& stage, QWidget& dialog,
                  QScrollArea& scroll, const QList<MeterWidget*>& meters)
{
    QJsonArray widgets;
    widgets.append(widgetTopology(&dialog, &dialog, scroll.viewport()));
    for (QWidget* widget : dialog.findChildren<QWidget*>()) {
        widgets.append(widgetTopology(widget, &dialog, scroll.viewport()));
    }
    QJsonArray rows;
    for (MeterWidget* meter : meters) {
        rows.append(widgetTopology(meter, &dialog, scroll.viewport()));
    }
    QJsonObject record{
        {"stage", stage}, {"qtRuntime", QString::fromLatin1(qVersion())},
        {"platform", QGuiApplication::platformName()},
        {"executable", QCoreApplication::applicationFilePath()},
        {"sourceRevision", qEnvironmentVariable("PREVIEW_SCROLL_SOURCE_REVISION")},
        {"noFastMove", qEnvironmentVariable("QT_NO_FAST_MOVE")},
        {"nativeSiblingIsolation", QCoreApplication::testAttribute(Qt::AA_DontCreateNativeWidgetSiblings)},
        {"scrollValue", scroll.verticalScrollBar()->value()},
        {"scrollMaximum", scroll.verticalScrollBar()->maximum()},
        {"viewportRect", rectJson(dialogRect(scroll.viewport(), &dialog))},
        {"capturePrecedesFreshGrab", true}, {"widgets", widgets}, {"meterRows", rows}
    };
#ifdef NEREUS_GPU_SPECTRUM
    record["renderBuild"] = "QRhiWidget";
#else
    record["renderBuild"] = "CPU QWidget (cannot establish Metal acceptance)";
#endif
    QFile file(directory + "/" + stage + "-topology.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes = QJsonDocument(record).toJson(QJsonDocument::Indented);
    return file.write(bytes) == bytes.size();
}

QImage nativeWindowCapture(QWidget& dialog)
{
    QWindow* window = dialog.windowHandle();
    if (!window || !window->screen() || !window->isExposed() || !dialog.internalWinId()) {
        return {};
    }
    // Nonzero, already-existing fixture window only; never grabWindow(0), which
    // would capture the operator desktop. No foreground activation is requested.
    return window->screen()->grabWindow(dialog.internalWinId(), 0, 0,
                                        dialog.width(), dialog.height()).toImage();
}

QRect pixelRect(const QRect& logical, const QImage& image, const QSize& logicalSize)
{
    const qreal xScale = qreal(image.width()) / logicalSize.width();
    const qreal yScale = qreal(image.height()) / logicalSize.height();
    return QRect(qRound(logical.x() * xScale), qRound(logical.y() * yScale),
                 qRound(logical.width() * xScale), qRound(logical.height() * yScale));
}

bool containsSentinel(const QImage& image, const QLabel& sentinel, const QSize& dialogSize)
{
    if (image.isNull()) {
        return false;
    }
    const QRect pixels = pixelRect(sentinel.geometry(), image, dialogSize);
    if (!image.rect().contains(pixels)) {
        return false;
    }
    return image.pixelColor(pixels.center()) == QColor(sentinel.property("sentinelColor").toString());
}

qsizetype changedPixels(const QImage& baseline, const QImage& current,
                       const QRegion& region, const QSize& dialogSize)
{
    qsizetype count = 0;
    for (const QRect& logical : region) {
        const QRect pixels = pixelRect(logical, baseline, dialogSize).intersected(baseline.rect());
        for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
            for (int x = pixels.left(); x <= pixels.right(); ++x) {
                if (baseline.pixelColor(x, y) != current.pixelColor(x, y)) {
                    ++count;
                }
            }
        }
    }
    return count;
}
} // namespace

class TstContainerPreviewScrollNative : public QObject {
    Q_OBJECT
private slots:
    void scrollingPreservesActualDialogPixelsOutsidePreview()
    {
        // The break this catches is preview scrolling copying/painting editor
        // bands outside its viewport. Geometry-only or freshly rendered images
        // cannot prove that the native display preserves those bands.
        if (QGuiApplication::platformName() == QStringLiteral("offscreen")
            || QGuiApplication::platformName() == QStringLiteral("minimal")) {
            QSKIP("Requires NATIVE_WINDOW registration and actual own-window capture.");
        }
        QTemporaryDir settingsDirectory;
        QVERIFY(settingsDirectory.isValid());
        const QString captureDirectory = qEnvironmentVariable("PREVIEW_SCROLL_NATIVE_CAPTURE_DIR");
        if (captureDirectory.isEmpty()) {
            QSKIP("Set PREVIEW_SCROLL_NATIVE_CAPTURE_DIR to preserve native captures and topology evidence.");
        }
        QVERIFY(QDir().mkpath(captureDirectory));
        AppSettings settings(settingsDirectory.filePath("settings"));
        ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry;
        MeterPoller poller;
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        manager.setPreviewPoller(&poller);
        WorkspaceDocument document;
        document.mainContainerId = "scroll-fixture";
        ContainerDocument container;
        container.id = document.mainContainerId;
        container.name = "Preview scroll native regression";
        container.layout = ContentLayout::VerticalStack;
        const QStringList types{
            "meter.signalText", "meter.comp", "meter.eq", "meter.leveler",
            "meter.cfc", "meter.cfcGain", "meter.alcGain"
        };
        for (const QString& type : types) {
            ContentEntry entry = registry.makeEntry(type);
            QJsonObject properties = entry.config.value("properties").toObject();
            if (type == QStringLiteral("meter.signalText")) {
                properties["fontSize"] = 56;
                properties["faceHeight"] = 120;
            } else {
                properties["rowHeight"] = 96;
            }
            entry.config["properties"] = properties;
            QVERIFY2(registry.validateEntry(entry).isEmpty(), qPrintable(type));
            container.contents.append(entry);
        }
        document.containers = {container};
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);
        const WorkspaceDocument originalStore = store.snapshot();
        ContainerSettingsDialog dialog(manager.container(container.id), nullptr, &manager);
        dialog.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
        dialog.setAttribute(Qt::WA_ShowWithoutActivating);
        dialog.resize(1200, 800);
        QListWidget* contents = dialog.findChild<QListWidget*>("containerDraftContents");
        QVERIFY(contents);
        // A bar's actual property editor includes the History color row visible
        // in the reported duplicated bands; select it without native user input.
        contents->setCurrentRow(1);
        QVERIFY(dialog.findChild<QWidget*>("historyColor"));
        ContainerPreviewWidget* preview = dialog.findChild<ContainerPreviewWidget*>();
        QVERIFY(preview);
        QScrollArea* scroll = nullptr;
        for (QScrollArea* candidate : dialog.findChildren<QScrollArea*>()) {
            if (candidate->widget() == preview) {
                scroll = candidate;
                break;
            }
        }
        QVERIFY(scroll);
        const QList<MeterWidget*> meters = preview->findChildren<MeterWidget*>();
        QCOMPARE(meters.size(), types.size());
        const WorkspaceDocument originalDraft = dialog.editSession()->draft();
        QLabel topSentinel(&dialog), bottomSentinel(&dialog);
        topSentinel.setGeometry(2, 2, 16, 4);
        bottomSentinel.setGeometry(2, dialog.height() - 6, 16, 4);
        topSentinel.setProperty("sentinelColor", "#23b6d5");
        bottomSentinel.setProperty("sentinelColor", "#d59323");
        for (QLabel* sentinel : {&topSentinel, &bottomSentinel}) {
            sentinel->setAttribute(Qt::WA_TransparentForMouseEvents);
            sentinel->setStyleSheet("background:" + sentinel->property("sentinelColor").toString() + ";");
        }
#ifdef NEREUS_GPU_SPECTRUM
        QList<std::shared_ptr<QSignalSpy>> initialFrames;
        for (MeterWidget* meter : meters) {
            initialFrames.append(std::make_shared<QSignalSpy>(meter, &QRhiWidget::frameSubmitted));
        }
#endif
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTRY_VERIFY_WITH_TIMEOUT(scroll->verticalScrollBar()->maximum() > 0, kFrameTimeoutMs);
#ifdef NEREUS_GPU_SPECTRUM
        for (qsizetype i = 0; i < meters.size(); ++i) {
            if (dialogRect(meters[i], scroll->viewport()).intersects(scroll->viewport()->rect())) {
                QTRY_VERIFY_WITH_TIMEOUT(initialFrames[i]->count() > 0, kFrameTimeoutMs);
            }
#if defined(Q_OS_MAC)
            QCOMPARE(meters[i]->api(), QRhiWidget::Api::Metal);
#endif
        }
#endif
        QCoreApplication::processEvents();
        QVERIFY(saveTopology(captureDirectory, "initial-before-native-capture", dialog, *scroll, meters));
        const QImage baseline = nativeWindowCapture(dialog);
        if (!baseline.isNull()) {
            QVERIFY(baseline.save(captureDirectory + "/initial-native.png"));
        }
        if (!containsSentinel(baseline, topSentinel, dialog.size())
            || !containsSentinel(baseline, bottomSentinel, dialog.size())) {
            QSKIP("Own-window QScreen capture unavailable, denied, incorrectly cropped, or lacks fixture pixels; topology is diagnostic only.");
        }

        // Cover whole static header/properties/action bands, including blank
        // raster between controls where the screenshot showed duplicated rows.
        // Exclude the entire scroll frame and text caret interiors, which can
        // blink independently. No animated meter pixels form the oracle.
        QRegion staticRegions(dialog.rect().adjusted(1, 1, -1, -1));
        staticRegions -= dialogRect(scroll, &dialog).adjusted(-1, -1, 1, 1);
        for (QLineEdit* edit : dialog.findChildren<QLineEdit*>()) {
            staticRegions -= dialogRect(edit, &dialog).adjusted(-2, -2, 2, 2);
        }
        QVERIFY(!staticRegions.isEmpty());
        QScrollBar* bar = scroll->verticalScrollBar();
        const int maximum = bar->maximum();
        const QList<int> offsets{
            1, 19, maximum / 3, maximum, maximum - 17, maximum / 2, 0,
            31, maximum, maximum / 3, 3, 0
        };
        QStringList pixelFailures;
        for (qsizetype step = 0; step < offsets.size(); ++step) {
            const QString stage = QString("step-%1-offset-%2").arg(step, 2, 10, QLatin1Char('0')).arg(offsets[step]);
            QVERIFY(saveTopology(captureDirectory, stage + "-before-scroll", dialog, *scroll, meters));
            bar->setValue(offsets[step]);
            QCOMPARE(bar->value(), offsets[step]);
            QCoreApplication::processEvents();
            QVERIFY(saveTopology(captureDirectory, stage + "-after-scroll", dialog, *scroll, meters));
            // This capture is the primary oracle and precedes explicit update,
            // QWidget::grab(), and QRhiWidget::grabFramebuffer().
            const QImage native = nativeWindowCapture(dialog);
            QVERIFY2(!native.isNull(), qPrintable(stage + ": native capture became unavailable"));
            QVERIFY(native.save(captureDirectory + "/" + stage + "-native.png"));
            QCOMPARE(native.size(), baseline.size());
            QVERIFY2(containsSentinel(native, topSentinel, dialog.size())
                     && containsSentinel(native, bottomSentinel, dialog.size()),
                     "Capture lost fixture sentinels; cannot interpret this as a compositor regression.");
            const qsizetype differences = changedPixels(baseline, native, staticRegions, dialog.size());
            if (differences) {
                pixelFailures.append(stage + ": " + QString::number(differences) + " outside-preview pixels changed");
            }
            qInfo().noquote() << stage << "native static-region changed pixels:" << differences;
            QCOMPARE(dialog.editSession()->draft(), originalDraft);
            QCOMPARE(store.snapshot(), originalStore);
            QCOMPARE(poller.targetCountForTest(), 0);
        }

#ifdef NEREUS_GPU_SPECTRUM
        // Defer all forced rendering until every primary scroll capture is
        // saved, so a diagnostic redraw cannot repair accumulating corruption
        // between offsets. A clean fresh frame cannot erase native failures.
        for (MeterWidget* meter : meters) {
            if (!dialogRect(meter, scroll->viewport()).intersects(scroll->viewport()->rect())) {
                continue;
            }
            QSignalSpy frames(meter, &QRhiWidget::frameSubmitted);
            QSignalSpy failures(meter, &QRhiWidget::renderFailed);
            meter->update();
            QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 0 || failures.count() > 0, kFrameTimeoutMs);
            QCOMPARE(failures.count(), 0);
            QVERIFY(frames.count() > 0);
        }
        QVERIFY(saveTopology(captureDirectory, "final-after-frame-before-fresh-grab", dialog, *scroll, meters));
        const QImage submitted = nativeWindowCapture(dialog);
        QVERIFY(!submitted.isNull());
        QVERIFY(submitted.save(captureDirectory + "/final-after-frame-native.png"));
#endif
        // Fresh QWidget pixels are diagnostic only; the native display oracle
        // above always precedes QWidget::grab or QRhiWidget::grabFramebuffer.
        QVERIFY(saveTopology(captureDirectory, "final-before-fresh-widget-grab", dialog, *scroll, meters));
        QVERIFY(dialog.grab().save(captureDirectory + "/final-fresh-widget.png"));
#ifdef NEREUS_GPU_SPECTRUM
        for (qsizetype i = 0; i < meters.size(); ++i) {
            if (!dialogRect(meters[i], scroll->viewport()).intersects(scroll->viewport()->rect())) {
                continue;
            }
            QVERIFY(saveTopology(captureDirectory, QString("final-before-framebuffer-%1").arg(i), dialog, *scroll, meters));
            const QImage fresh = meters[i]->grabFramebuffer();
            QVERIFY(!fresh.isNull());
            QVERIFY(fresh.save(captureDirectory + QString("/final-fresh-framebuffer-%1.png").arg(i)));
        }
#endif
        QCOMPARE(dialog.editSession()->draft(), originalDraft);
        dialog.reject();
        QCOMPARE(store.snapshot(), originalStore);
        QCOMPARE(poller.targetCountForTest(), 0);
        QVERIFY2(pixelFailures.isEmpty(), qPrintable(pixelFailures.join('\n')));
    }
};

QTEST_MAIN(TstContainerPreviewScrollNative)
#include "tst_container_preview_scroll_native.moc"
