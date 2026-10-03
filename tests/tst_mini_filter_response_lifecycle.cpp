// no-port-check: NereusSDR-original direct-signal FIR lifetime regressions.
#include <QtTest/QtTest>
#include <QPointer>

#include "models/RadioModel.h"

namespace NereusSDR {

class TestMiniFilterResponseLifecycle final : public QObject {
    Q_OBJECT
private slots:
    void requestListenerCanRemoveItsOwnDemand()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int sliceId = model.addSliceWithStationId(0);
        QVERIFY(sliceId >= 0);
        connect(&model, &RadioModel::miniFilterResponseChanged, &model,
                [&model](int) { model.setMiniFilterResponseSlices({}); },
                Qt::DirectConnection);
        model.setMiniFilterResponseSlices({sliceId});
        QVERIFY(model.m_miniFilterResponses.isEmpty());
    }

    void requestListenerCanReplaceTheWholeSetDuringInitialSweep()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int a = model.addSliceWithStationId(0);
        const int b = model.addSliceWithStationId(1);
        QVERIFY(a >= 0 && b >= 0);
        bool cleared = false;
        connect(&model, &RadioModel::miniFilterResponseChanged, &model,
                [&model, &cleared](int) {
            if (cleared) { return; }
            cleared = true;
            model.setMiniFilterResponseSlices({});
        }, Qt::DirectConnection);
        model.setMiniFilterResponseSlices({a, b});
        QVERIFY(cleared);
        QVERIFY(model.m_miniFilterResponses.isEmpty());
    }

    void responseListenerCanReplaceDemandWithoutRevivingOldRequest()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int sliceId = model.addSliceWithStationId(0);
        QVERIFY(sliceId >= 0);
        model.setMiniFilterResponseSlices({sliceId});
        auto old = model.m_miniFilterResponses.find(sliceId);
        QVERIFY(old != model.m_miniFilterResponses.end());
        const quint64 oldSerial = old->serial;
        old->command = 42;
        old->dirty = true;
        bool replaced = false;
        connect(&model, &RadioModel::miniFilterResponseChanged, &model,
                [&model, &replaced, sliceId](int id) {
            if (id != sliceId || replaced) { return; }
            replaced = true;
            model.setMiniFilterResponseSlices({});
            model.setMiniFilterResponseSlices({sliceId});
        }, Qt::DirectConnection);
        model.reportStationFilterResponse(42, false, {}, 0.0, 0.0, {});
        QVERIFY(replaced);
        const auto current = model.m_miniFilterResponses.constFind(sliceId);
        QVERIFY(current != model.m_miniFilterResponses.cend());
        QVERIFY(current->serial > oldSerial);
        QCOMPARE(current->command, quint32{0});
        QVERIFY(current->response.magnitudesDb.isEmpty());
    }

    void failureListenerCanRemoveAnotherDemandWhileIterating()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int first = model.addSliceWithStationId(0);
        const int second = model.addSliceWithStationId(1);
        QVERIFY(first >= 0 && second >= 0);
        model.setMiniFilterResponseSlices({first, second});
        connect(&model, &RadioModel::miniFilterResponseChanged, &model,
                [&model](int) { model.setMiniFilterResponseSlices({}); },
                Qt::DirectConnection);
        model.failStationFilterResponse();
        QVERIFY(model.m_miniFilterResponses.isEmpty());
    }

    void failureListenerCannotClearReplacementOfLaterDemand()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int a = model.addSliceWithStationId(0);
        const int b = model.addSliceWithStationId(1);
        QVERIFY(a >= 0 && b >= 0);
        model.setMiniFilterResponseSlices({a, b});
        const int first = model.m_miniFilterResponses.cbegin().key();
        const int later = first == a ? b : a;
        const quint64 oldSerial = model.m_miniFilterResponses[later].serial;
        model.m_miniFilterResponses[later].command = 99;
        bool replaced = false;
        connect(&model, &RadioModel::miniFilterResponseChanged, &model,
                [&model, &replaced, first, later](int id) {
            if (id != first || replaced) { return; }
            replaced = true;
            model.setMiniFilterResponseSlices({first});
            model.setMiniFilterResponseSlices({first, later});
            model.m_miniFilterResponses[later].command = 123;
        }, Qt::DirectConnection);
        model.failStationFilterResponse();
        QVERIFY(replaced);
        QVERIFY(model.m_miniFilterResponses[later].serial > oldSerial);
        QCOMPARE(model.m_miniFilterResponses[later].command, quint32{123});
    }

    void versionSweepCannotClearReplacementOfLaterDemand()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int a = model.addSliceWithStationId(0);
        const int b = model.addSliceWithStationId(1);
        QVERIFY(a >= 0 && b >= 0);
        model.setMiniFilterResponseSlices({a, b});
        const int first = model.m_miniFilterResponses.cbegin().key();
        const int later = first == a ? b : a;
        const quint64 oldSerial = model.m_miniFilterResponses[later].serial;
        bool replaced = false;
        connect(&model, &RadioModel::miniFilterResponseChanged, &model,
                [&model, &replaced, first, later](int id) {
            if (id != first || replaced) { return; }
            replaced = true;
            model.setMiniFilterResponseSlices({first});
            model.setMiniFilterResponseSlices({first, later});
            model.m_miniFilterResponses[later].response.magnitudesDb = {7.0};
        }, Qt::DirectConnection);
        model.setStationDspInfoVersion(1);
        QVERIFY(replaced);
        QVERIFY(model.m_miniFilterResponses[later].serial > oldSerial);
        QCOMPARE(model.m_miniFilterResponses[later].response.magnitudesDb,
                 QVector<double>({7.0}));
    }

    void requestListenerCanDestroyModel()
    {
        auto* model = new RadioModel(RadioModel::Role::Remote);
        const int sliceId = model->addSliceWithStationId(0);
        QVERIFY(sliceId >= 0);
        QPointer<RadioModel> held(model);
        connect(model, &RadioModel::miniFilterResponseChanged, model,
                [model](int) { delete model; }, Qt::DirectConnection);
        model->setMiniFilterResponseSlices({sliceId});
        QVERIFY(held.isNull());
    }
};

} // namespace NereusSDR

QTEST_MAIN(NereusSDR::TestMiniFilterResponseLifecycle)
#include "tst_mini_filter_response_lifecycle.moc"
