// no-port-check: NereusSDR-original ownership regressions, GPLv3.
// Modification history (NereusSDR):
// 2026-10-05 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/AppSettings.h"
#include "core/ClarityController.h"
#include "core/FFTEngine.h"
#include "gui/PanClarityRegistry.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpectrumOverlayPanel.h"
#include "models/RadioModel.h"
#include "models/PanadapterModel.h"
#include "models/SliceModel.h"
#include <memory>
using namespace NereusSDR;
class TestPanClarityRegistry : public QObject {
    Q_OBJECT
    using Source = PanClarityRegistry::SourceAssociation;
private slots:
    void init() { AppSettings::instance().setValue(QStringLiteral("ClarityEnabled"), QStringLiteral("True")); }
    void masterTransitionsSurviveSameSourceRebind_data()
    {
        QTest::addColumn<bool>("enableInsideCallback");
        QTest::newRow("on-after-unwind") << false;
        QTest::newRow("queued-off-and-on") << true;
    }
    void masterTransitionsSurviveSameSourceRebind()
    {
        QFETCH(bool, enableInsideCallback);
        PanClarityRegistry registry(nullptr); registry.setEnabled(true);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(0);
        const Source source{0,0,14225000,192000};
        registry.registerPan("p", &widget, nullptr);
        auto token = registry.bindRemote("p", &widget, &first, source,1,1,1);
        auto* controller = registry.controllerForPan("p");
        QSignalSpy thresholds(controller, &ClarityController::waterfallThresholdsChanged);
        bool changed = false;
        connect(controller, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            if (changed) { return; }
            changed = true;
            registry.setEnabled(false);
            token = registry.bindRemote("p", &widget, &second, source,1,2,2);
            if (enableInsideCallback) { registry.setEnabled(true); }
        });
        registry.feedRemoteFloor("p", &widget, token,-130,1000);
        QVERIFY(changed); QVERIFY(!widget.clarityActive());
        QCOMPARE(controller->isEnabled(), enableInsideCallback);
        if (!enableInsideCallback) { registry.setEnabled(true); }
        registry.feedRemoteFloor("p", &widget, token,-130,2000);
        QVERIFY(widget.clarityActive());
        QCOMPARE(thresholds.count(), 2);
        QCOMPARE(widget.wfActiveLowThreshold(), -135.0f);
        QCOMPARE(widget.wfActiveHighThreshold(), -75.0f);
    }
    void retuneSurvivesSameSourceRebind()
    {
        PanClarityRegistry registry(nullptr); registry.setEnabled(true);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(0);
        const Source source{0,0,14225000,192000};
        registry.registerPan("p", &widget, nullptr);
        auto token = registry.bindRemote("p", &widget, &first, source,1,1,1);
        auto* controller = registry.controllerForPan("p");
        bool changed = false;
        connect(controller, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            if (changed) { return; }
            changed = true;
            controller->notifyManualOverride();
            registry.retunePan("p");
            // The queued old recipient sample must still be rejected.
            registry.feedRemoteFloor("p", &widget, token,-50,2000);
            token = registry.bindRemote("p", &widget, &second, source,1,2,2);
        });
        registry.feedRemoteFloor("p", &widget, token,-130,1000);
        QVERIFY(changed); QVERIFY(!controller->isPaused());
        QVERIFY(!widget.clarityActive());
        QCOMPARE(controller->smoothedFloor(), -130.0f);
        registry.feedRemoteFloor("p", &widget, token,-90,2000);
        QCOMPARE(controller->smoothedFloor(), -90.0f);
        QCOMPARE(widget.wfActiveLowThreshold(), -95.0f);
    }
    void keyedTransitionsSurviveSameSourceRebind_data()
    {
        QTest::addColumn<bool>("unkeyInsideCallback");
        QTest::newRow("keyed-after-unwind") << false;
        QTest::newRow("queued-key-and-unkey") << true;
    }
    void keyedTransitionsSurviveSameSourceRebind()
    {
        QFETCH(bool, unkeyInsideCallback);
        PanClarityRegistry registry(nullptr); registry.setEnabled(true);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(0);
        const Source source{0,0,14225000,192000};
        registry.registerPan("p", &widget, nullptr);
        auto token = registry.bindRemote("p", &widget, &first, source,1,1,1);
        auto* controller = registry.controllerForPan("p");
        QSignalSpy paused(controller, &ClarityController::pausedChanged);
        bool changed = false;
        connect(controller, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            if (changed) { return; }
            changed = true;
            registry.setKeyed(true);
            token = registry.bindRemote("p", &widget, &second, source,1,2,2);
            if (unkeyInsideCallback) { registry.setKeyed(false); }
        });
        registry.feedRemoteFloor("p", &widget, token,-130,1000);
        QVERIFY(changed); QVERIFY(!widget.clarityActive());
        QCOMPARE(controller->isTransmitting(), !unkeyInsideCallback);
        QCOMPARE(paused.count(), unkeyInsideCallback ? 2 : 1);
        QCOMPARE(paused.at(0).at(0).toBool(), true);
        if (unkeyInsideCallback) { QCOMPARE(paused.at(1).at(0).toBool(), false); }
        else { registry.setKeyed(false); }
        registry.feedRemoteFloor("p", &widget, token,-90,2000);
        QVERIFY(widget.clarityActive());
    }
    void queuedUnkeyRetainsAuthoritativeLocalFence()
    {
        PanClarityRegistry registry(nullptr); registry.setEnabled(true);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(0);
        bool actualMox = false;
        registry.setLocalKeyedQueryForTest([&] { return actualMox; });
        registry.registerPan("p", &widget, nullptr);
        const Source source{0,0,14225000,192000};
        auto token = registry.bindRemote("p", &widget, &first, source,1,1,1);
        auto* controller = registry.controllerForPan("p");
        QSignalSpy paused(controller, &ClarityController::pausedChanged);
        bool changed = false;
        connect(controller, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            if (changed) { return; }
            changed = true;
            registry.setKeyed(true);
            token = registry.bindRemote("p", &widget, &second, source,1,2,2);
            actualMox = true;
            registry.setKeyed(false); // older display edge cannot unkey local truth
        });
        registry.feedRemoteFloor("p", &widget, token,-130,1000);
        QVERIFY(changed); QVERIFY(controller->isTransmitting());
        QVERIFY(!widget.clarityActive()); QCOMPARE(paused.count(), 1);
        registry.feedRemoteFloor("p", &widget, token,-90,2000);
        QCOMPARE(controller->smoothedFloor(), -130.0f);
        actualMox = false; registry.setKeyed(false);
        QVERIFY(!controller->isTransmitting()); QCOMPARE(paused.count(), 2);
        registry.feedRemoteFloor("p", &widget, token,-90,2000);
        QVERIFY(widget.clarityActive());
    }
    void retiredOwnerRejectsQueuedControls()
    {
        PanClarityRegistry registry(nullptr); registry.setEnabled(true);
        SpectrumWidget oldWidget, replacement; SliceModel slice; slice.setStreamIndex(0);
        registry.registerPan("p", &oldWidget, nullptr);
        const auto token = registry.bindRemote("p", &oldWidget, &slice,{0,0,1,2},1,1,1);
        QPointer<ClarityController> oldController(registry.controllerForPan("p"));
        bool changed = false;
        connect(oldController, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            changed = true;
            oldController->notifyManualOverride();
            registry.retunePan("p"); registry.setKeyed(true); registry.setKeyed(false);
            registry.setEnabled(false);
            registry.retirePan("p");
            registry.registerPan("p", &replacement, nullptr);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(oldController);
        });
        registry.feedRemoteFloor("p", &oldWidget, token,-130,1000);
        QVERIFY(changed); QVERIFY(oldController); QVERIFY(oldController->isPaused());
        QVERIFY(!oldController->isTransmitting()); QVERIFY(oldController->isEnabled());
        QVERIFY(!registry.controllerForPan("p")->isEnabled());
        QVERIFY(!registry.controllerForPan("p")->isPaused());
        QVERIFY(!registry.controllerForPan("p")->isTransmitting());
        QVERIFY(!replacement.clarityActive());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!oldController);
    }
    void differentSourceDuringFloorCannotCommitOldTail()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(1);
        registry.registerPan("p", &widget, nullptr);
        auto token=registry.bindRemote("p", &widget, &first, {0,0,14225000,192000},1,1,1);
        auto* owner=registry.controllerForPan("p");
        bool replaced=false;
        connect(owner, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            if (replaced) { return; }
            replaced=true;
            token=registry.bindRemote("p", &widget, &second,{1,0,7100000,192000},1,2,2);
            registry.feedRemoteFloor("p", &widget, token,-100,1500);
        });
        registry.feedRemoteFloor("p", &widget, token,-140,1000);
        QCOMPARE(widget.wfActiveLowThreshold(),-105.0f);
        QCOMPARE(owner->smoothedFloor(),-100.0f);
        ClarityController reference; reference.setEnabled(true);
        reference.feedNoiseFloor(-100,1500); reference.feedNoiseFloor(-90,2000);
        registry.feedRemoteFloor("p", &widget, token,-90,2000);
        QCOMPARE(owner->smoothedFloor(),reference.smoothedFloor());
        QCOMPARE(owner->lastLow(),reference.lastLow());
    }
    void rebindWithoutNestedInputResetsAfterUnwind()
    {
        PanClarityRegistry registry(nullptr); registry.setEnabled(true);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(1);
        registry.registerPan("p", &widget, nullptr);
        auto token = registry.bindRemote("p", &widget, &first, {0,0,14225000,192000},1,1,1);
        auto* controller = registry.controllerForPan("p");
        bool rebound = false;
        connect(controller, &ClarityController::noiseFloorChanged, &registry, [&](float) {
            if (rebound) { return; }
            rebound = true;
            token = registry.bindRemote("p", &widget, &second, {1,0,7100000,192000},1,2,2);
        });
        registry.feedRemoteFloor("p", &widget, token, -140, 1000);
        QVERIFY(!widget.clarityActive());
        registry.feedRemoteFloor("p", &widget, token, -100, 1100);
        QCOMPARE(controller->smoothedFloor(), -100.0f);
        QCOMPARE(widget.wfActiveLowThreshold(), -105.0f);
    }
    void executingControllerSurvivesRegistryDeletion()
    {
        SpectrumWidget widget; SliceModel slice; slice.setStreamIndex(0);
        auto registry=std::make_unique<PanClarityRegistry>(nullptr);
        registry->setEnabled(true); registry->registerPan("p", &widget,nullptr);
        const auto token=registry->bindRemote("p", &widget,&slice,{0,0,1,2},1,1,1);
        QPointer<ClarityController> owner(registry->controllerForPan("p"));
        bool survivedCallback=false;
        connect(owner,&ClarityController::noiseFloorChanged,&widget,[&](float) {
            registry.reset(); survivedCallback=bool(owner);
        });
        registry->feedRemoteFloor("p", &widget,token,-140,1000);
        QVERIFY(survivedCallback); QVERIFY(owner); QVERIFY(!widget.clarityActive());
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(owner.isNull());
    }
    void pausedOperationSurvivesOwnerDeletion()
    {
        QObject widgetOwner;
        SpectrumWidget widget; SliceModel slice;
        auto registry = std::make_unique<PanClarityRegistry>(nullptr);
        registry->setEnabled(true); registry->registerPan("p", &widget, nullptr);
        QPointer<ClarityController> controller(registry->controllerForPan("p"));
        controller->notifyManualOverride();
        bool alive = false;
        connect(controller, &ClarityController::pausedChanged, &widgetOwner, [&](bool paused) {
            if (!paused) { registry.reset(); alive = bool(controller); }
        });
        registry->retunePan("p");
        QVERIFY(alive); QVERIFY(controller);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!controller);
    }
    void hintOperationSurvivesContainingOwnerDeletion()
    {
        SpectrumWidget widget; SliceModel slice; slice.setStreamIndex(0);
        slice.setFrequency(14225000);
        PanadapterModel saved; saved.setBandNFEstimate(Band::Band40m, -100);
        auto containingOwner = std::make_unique<QObject>();
        QPointer<PanClarityRegistry> registry(new PanClarityRegistry(nullptr, containingOwner.get()));
        registry->setEnabled(true); registry->registerPan("p", &widget, nullptr, &saved);
        registry->bindRemote("p", &widget, &slice, {0,0,14225000,192000},1,1,1);
        QPointer<ClarityController> controller(registry->controllerForPan("p"));
        bool alive = false;
        connect(controller, &ClarityController::waterfallThresholdsChanged, &widget, [&](float, float) {
            containingOwner.reset(); alive = bool(controller);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(controller); // no deferred deletion while its call is active
        });
        slice.setFrequency(7100000);
        QVERIFY(!registry); QVERIFY(alive); QVERIFY(controller);
        QVERIFY(!widget.clarityActive());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!controller);
    }
    void hintRebindRetainsNewAvailability()
    {
        RadioModel model(RadioModel::Role::Remote);
        PanClarityRegistry registry(&model); SpectrumWidget widget;
        SliceModel first(0), second(1); first.setStreamIndex(0); second.setStreamIndex(1); first.setFrequency(14225000);
        PanadapterModel saved; saved.setBandNFEstimate(Band::Band40m, -100);
        registry.registerPan("p", &widget, nullptr, &saved);
        auto token = registry.bindRemote("p", &widget, &first, {0,0,14225000,192000},1,1,1);
        registry.setRemoteAvailable("p", token, false);
        bool rebound = false;
        connect(registry.controllerForPan("p"), &ClarityController::waterfallThresholdsChanged,
            &registry, [&](float, float) {
                if (rebound) { return; }
                rebound = true;
                token = registry.bindRemote("p", &widget, &second, {1,0,7100000,192000},1,2,2);
            });
        first.setFrequency(7100000);
        QVERIFY(rebound);
        registry.feedRemoteFloor("p", &widget, token, -120, 1000);
        QCOMPARE(widget.wfActiveLowThreshold(), -125.0f);
    }
    void localTruthPrecedesQueuedDisplayEdge()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        SpectrumWidget first, created; SliceModel slice; slice.setStreamIndex(0);
        bool actualMox=false;
        registry.setLocalKeyedQueryForTest([&] { return actualMox; });
        registry.registerPan("p",&first,nullptr);
        const auto token=registry.bindRemote("p",&first,&slice,{0,0,1,2},1,1,1);
        actualMox=true; // committed authoritative truth; display edge still queued
        registry.feedRemoteFloor("p",&first,token,-140,1000);
        QVERIFY(!first.clarityActive());
        emit registry.controllerForPan("p")->waterfallThresholdsChanged(-100,-40);
        QVERIFY(!first.clarityActive());
        registry.registerPan("new",&created,nullptr);
        QVERIFY(registry.controllerForPan("new")->isTransmitting());
        registry.setKeyed(true); actualMox=false; registry.setKeyed(false);
        QVERIFY(!registry.controllerForPan("new")->isTransmitting());
    }
    void independentFloorsAndRetune()
    {
        RadioModel model(RadioModel::Role::Remote);
        PanClarityRegistry registry(&model);
        std::vector<std::unique_ptr<SpectrumWidget>> widgets;
        std::vector<std::unique_ptr<SliceModel>> slices;
        std::vector<std::unique_ptr<ClarityController>> references;
        QList<PanClarityRegistry::RecipientToken> tokens;
        for (int i = 0; i < 4; ++i) {
            widgets.push_back(std::make_unique<SpectrumWidget>());
            slices.push_back(std::make_unique<SliceModel>(i));
            slices.back()->setStreamIndex(i % 2); slices.back()->setStreamEpoch(1);
            references.push_back(std::make_unique<ClarityController>()); references.back()->setEnabled(true);
            const QString id = QString::number(i);
            registry.registerPan(id, widgets.back().get(), nullptr);
            tokens.append(registry.bindRemote(id, widgets.back().get(), slices.back().get(), {i % 2, 1, 14225000, 192000}, 1, i + 1, 1));
        }
        for (qint64 now : {1000, 1500, 2500}) {
            for (int i = 0; i < 4; ++i) {
                const float floor = -150.0f + i * 15.0f + (now == 2500 ? 8.0f : 0.0f);
                references[i]->feedNoiseFloor(floor, now);
                registry.feedRemoteFloor(QString::number(i), widgets[i].get(), tokens[i], floor, now);
                QCOMPARE(widgets[i]->wfActiveLowThreshold(), references[i]->lastLow());
                QCOMPARE(widgets[i]->wfActiveHighThreshold(), references[i]->lastHigh());
            }
        }
        const float untouched = registry.controllerForPan("1")->smoothedFloor();
        registry.controllerForPan("0")->notifyManualOverride();
        registry.retunePan("0"); references[0]->retuneNow();
        registry.feedRemoteFloor("0", widgets[0].get(), tokens[0], -90, 3000);
        references[0]->feedNoiseFloor(-90, 3000);
        QCOMPARE(widgets[0]->wfActiveLowThreshold(), references[0]->lastLow());
        QCOMPARE(registry.controllerForPan("1")->smoothedFloor(), untouched);
    }
    void sameSourceRecipientReplacement()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        SpectrumWidget widget; SliceModel first(0), second(1);
        first.setStreamIndex(0); second.setStreamIndex(0);
        registry.registerPan("p", &widget, nullptr);
        const Source source{0, 0, 14225000, 192000};
        auto token = registry.bindRemote("p", &widget, &first, source, 1, 1, 1);
        auto* owner = registry.controllerForPan("p");
        registry.feedRemoteFloor("p", &widget, token, -130, 1000);
        const float pair = owner->lastLow();
        owner->notifyManualOverride();
        registry.setRemoteAvailable("p", token, false);
        auto replacement = registry.bindRemote("p", &widget, &second, source, 1, 2, 2);
        QVERIFY(owner->isPaused()); QCOMPARE(owner->lastLow(), pair);
        registry.retunePan("p");
        registry.feedRemoteFloor("p", &widget, token, -50, 2000);
        QCOMPARE(owner->lastLow(), pair);
        registry.feedRemoteFloor("p", &widget, replacement, -125, 2000);
        QCOMPARE(owner->smoothedFloor(), -125.0f);
        registry.setRemoteAvailable("p", replacement, false);
        registry.feedRemoteFloor("p", &widget, replacement, -50, 3000);
        QCOMPARE(owner->smoothedFloor(), -125.0f);
        replacement = registry.bindRemote("p", &widget, &first, source, 1, 3, 3);
        registry.feedRemoteFloor("p", &widget, replacement, -50, 2100);
        QCOMPARE(owner->smoothedFloor(), -125.0f); // existing cadence retained
        replacement = registry.bindRemote("p", &widget, &first, {0, 0, 7100000, 192000}, 1, 4, 4);
        registry.feedRemoteFloor("p", &widget, replacement, -100, 3000);
        QCOMPARE(owner->smoothedFloor(), -100.0f);
    }
    void retiredTokenCannotReachReusedId()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        SpectrumWidget oldWidget, widget; SliceModel slice; slice.setStreamIndex(0);
        registry.registerPan("p", &oldWidget, nullptr);
        auto old = registry.bindRemote("p", &oldWidget, &slice, {0,0,1,2},1,1,1);
        QPointer<ClarityController> oldOwner(registry.controllerForPan("p"));
        QMetaObject::invokeMethod(&registry, [&registry, &oldWidget, old]() {
            registry.feedRemoteFloor("p", &oldWidget, old, -50, 2000);
        }, Qt::QueuedConnection);
        registry.retirePan("p"); registry.registerPan("p", &widget, nullptr);
        auto token = registry.bindRemote("p", &widget, &slice, {0,0,1,2},1,2,1);
        registry.feedRemoteFloor("p", &widget, old, -50, 1000);
        QVERIFY(!widget.clarityActive());
        registry.feedRemoteFloor("p", &oldWidget, token, -50, 1000);
        QVERIFY(!widget.clarityActive());
        registry.feedRemoteFloor("p", &widget, token, -120, 1000);
        QCOMPARE(widget.wfActiveLowThreshold(), -125.0f);
        QCoreApplication::processEvents();
        QCOMPARE(widget.wfActiveLowThreshold(), -125.0f);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(oldOwner.isNull());
        // Retirement during a synchronous output callback cannot reach a reused widget.
        connect(registry.controllerForPan("p"), &ClarityController::noiseFloorChanged, &registry,
            [&] { registry.retirePan("p"); registry.registerPan("p", &oldWidget, nullptr); });
        registry.feedRemoteFloor("p", &widget, token, -80, 2000);
        QVERIFY(!oldWidget.clarityActive());
    }
    void localBinsKeepCurrentScale()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        SpectrumWidget first, second; SliceModel slice; slice.setStreamIndex(0); NereusSDR::FFTEngine engine(0), replacement(0);
        registry.registerPan("a", &first, nullptr); registry.registerPan("b", &second, nullptr);
        registry.bindLocal("a", &slice, &engine, {0,0,1,192000});
        registry.bindLocal("a", &slice, &engine, {0,0,1,192000});
        registry.bindLocal("b", &slice, &engine, {0,0,1,192000});
        ClarityController direct; direct.setEnabled(true); direct.setPollIntervalMs(0);
        for (const QString& id : {QString("a"), QString("b")}) { registry.controllerForPan(id)->setPollIntervalMs(0); }
        const QVector<float> bins(1024, -123.25f); direct.feedBins(bins);
        emit engine.fftReady(0, bins);
        QCOMPARE(first.wfActiveLowThreshold(), direct.lastLow()); QCOMPARE(second.wfActiveHighThreshold(), direct.lastHigh());
        registry.bindLocal("a", &slice, &replacement, {0,0,2,192000});
        registry.controllerForPan("b")->notifyManualOverride();
        emit engine.fftReady(0, QVector<float>(1024, -50));
        QCOMPARE(first.wfActiveLowThreshold(), direct.lastLow());
        emit replacement.fftReady(0, QVector<float>(1024, -100));
        QCOMPARE(first.wfActiveLowThreshold(), -105.0f);
        QCOMPARE(second.wfActiveHighThreshold(), direct.lastHigh());
    }
    void masterKeepsPauseAndSavedFields()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("DisplayWfLowLevel"), QStringLiteral("-137.1250"));
        settings.setValue(QStringLiteral("DisplayWfHighLevel"), QStringLiteral("-82.3750"));
        const QString savedLow = settings.value(QStringLiteral("DisplayWfLowLevel")).toString();
        const QString savedHigh = settings.value(QStringLiteral("DisplayWfHighLevel")).toString();
        SpectrumWidget widget; SliceModel slice; slice.setStreamIndex(0);
        registry.registerPan("p", &widget, nullptr);
        auto token=registry.bindRemote("p", &widget, &slice,{0,0,1,2},1,1,1);
        const float low=widget.wfLowThreshold(), high=widget.wfHighThreshold();
        registry.feedRemoteFloor("p", &widget, token, -130, 1000);
        auto* owner=registry.controllerForPan("p"); owner->notifyManualOverride();
        QSignalSpy changes(&model, &RadioModel::clarityEnabledChanged);
        model.setClarityEnabled(false); model.setClarityEnabled(false); model.setClarityEnabled(true);
        QCOMPARE(changes.size(), 2); QVERIFY(owner->isPaused()); QVERIFY(!widget.clarityActive());
        registry.retunePan("p"); registry.setKeyed(true);
        registry.feedRemoteFloor("p", &widget, token, -100, 2000); QVERIFY(!widget.clarityActive());
        emit owner->waterfallThresholdsChanged(-50,0); QVERIFY(!widget.clarityActive());
        registry.setKeyed(false); registry.feedRemoteFloor("p", &widget, token, -100, 3000);
        QCOMPARE(widget.wfLowThreshold(),low); QCOMPARE(widget.wfHighThreshold(),high);
        QCOMPARE(settings.value(QStringLiteral("DisplayWfLowLevel")).toString(), savedLow);
        QCOMPARE(settings.value(QStringLiteral("DisplayWfHighLevel")).toString(), savedHigh);
        registry.setKeyed(true); SpectrumWidget created; registry.registerPan("new",&created,nullptr);
        QVERIFY(registry.controllerForPan("new")->isTransmitting());
        registry.invalidateRemoteSession(); registry.feedRemoteFloor("p",&widget,token,-50,4000); QVERIFY(!widget.clarityActive());
    }
    void bandHintsAndSolePan0Writer()
    {
        RadioModel model(RadioModel::Role::Remote); PanClarityRegistry registry(&model);
        SpectrumWidget zero, other; PanadapterModel saved; SliceModel slice, otherSlice(1);
        slice.setStreamIndex(0); otherSlice.setStreamIndex(0); saved.setBand(Band::Band20m); saved.setBandNFEstimate(Band::Band40m,-135);
        registry.registerPan("pan-0",&zero,nullptr,&saved); registry.registerPan("pan-1",&other,nullptr);
        saved.setBand(Band::Band40m); QCOMPARE(zero.wfActiveLowThreshold(),-140.0f); QVERIFY(!other.clarityActive());
        registry.setKeyed(true); const float before=zero.wfActiveLowThreshold();
        saved.setBandNFEstimate(Band::Band20m,-100); saved.setBand(Band::Band20m); QCOMPARE(zero.wfActiveLowThreshold(),before);
        registry.setKeyed(false);
        auto first=registry.bindRemote("pan-0",&zero,&slice,{0,0,1,2},1,1,1);
        auto second=registry.bindRemote("pan-1",&other,&otherSlice,{0,0,1,2},1,2,1);
        registry.controllerForPan("pan-0")->setPollIntervalMs(0); registry.controllerForPan("pan-0")->setSmoothingTauSec(0);
        registry.controllerForPan("pan-1")->setPollIntervalMs(0);
        for(int i=0;i<30;++i) { registry.feedRemoteFloor("pan-0",&zero,first,-120,1000+i); registry.feedRemoteFloor("pan-1",&other,second,-70,1000+i); }
        QCOMPARE(saved.bandNFEstimate(Band::Band20m),-120.0f);
        const float ownRuntimeHint=registry.controllerForPan("pan-1")->smoothedFloor();
        otherSlice.setFrequency(7100000); otherSlice.setFrequency(14225000);
        QCOMPARE(other.wfActiveLowThreshold(),ownRuntimeHint-5);
        QCOMPARE(saved.bandNFEstimate(Band::Band20m),-120.0f);
        const float ownPair = zero.wfActiveLowThreshold();
        saved.setBand(Band::Band40m); // model follows a different selected slice
        QCOMPARE(zero.wfActiveLowThreshold(), ownPair);
        registry.feedRemoteFloor("pan-0", &zero, first, -120, 2100);
        QCOMPARE(saved.bandNFEstimate(Band::Band20m), -120.0f);
        QCOMPARE(saved.bandNFEstimate(Band::Band40m), -135.0f);
        for(int i=0;i<40;++i) { registry.feedRemoteFloor("pan-1",&other,second,-50,3000+i); }
        QCOMPARE(saved.bandNFEstimate(Band::Band20m),-120.0f);
    }
};
QTEST_MAIN(TestPanClarityRegistry)
#include "tst_pan_clarity_registry.moc"
