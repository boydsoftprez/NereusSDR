// =================================================================
// tests/tst_log_category_list.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Phone wire batch: radio's
// logCategoryList, every logging category the Support dialog lists with
// the label its checkbox shows (LogManager::categoryListJson).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-29 - radio's alexLpfBits is declared after logCategoryList.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - rxFilter0LowPassReason and rxFilter0LowPassSlice are
//                 declared last. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>

#include "core/LogCategories.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestLogCategoryList : public QObject {
    Q_OBJECT

private slots:
    void listIsTheSupportDialogs()
    {
        const LogManager& logs = LogManager::instance();
        const QJsonDocument doc =
            QJsonDocument::fromJson(logs.categoryListJson().toUtf8());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().keys(), QStringList{QStringLiteral("categories")});
        const QJsonArray list = doc.object().value(QStringLiteral("categories")).toArray();
        const QList<LogCategoryInfo> categories = logs.categories();
        QCOMPARE(list.size(), categories.size());
        for (int i = 0; i < categories.size(); ++i) {
            const QJsonObject entry = list.at(i).toObject();
            QCOMPARE(entry.keys(), (QStringList{QStringLiteral("id"), QStringLiteral("label")}));
            QCOMPARE(entry.value(QStringLiteral("id")).toString(), categories.at(i).id);
            QCOMPARE(entry.value(QStringLiteral("label")).toString(), categories.at(i).label);
        }
        // Pinned: the Support dialog's first two rows.
        QCOMPARE(list.at(0).toObject().value(QStringLiteral("id")).toString(),
                 QStringLiteral("nereus.discovery"));
        QCOMPARE(list.at(0).toObject().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Discovery"));
        QCOMPARE(list.at(1).toObject().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Connection"));
    }

    void radioCarriesItConstant()
    {
        const QMetaObject& meta = RadioModel::staticMetaObject;
        const int index = meta.indexOfProperty("logCategoryList");
        QVERIFY(index >= 0);
        QVERIFY(meta.property(index).isConstant());
        // Appended after every earlier property, so each keeps its wire
        // ordinal; it keeps its own (27) as later properties (txInhibitReason,
        // alexLpfBits, paTransmitBand, then the four Level Cal run
        // properties, then the shared-input low-pass reason and slice)
        // append after it.
        QCOMPARE(index - meta.propertyOffset(), 27);
        QCOMPARE(meta.indexOfProperty("txInhibitReason"), index + 1);
        QCOMPARE(meta.indexOfProperty("alexLpfBits"), index + 2);
        QCOMPARE(meta.indexOfProperty("paTransmitBand"), index + 3);
        QCOMPARE(meta.indexOfProperty("levelCalRunning"), index + 4);
        QCOMPARE(meta.indexOfProperty("levelCalSucceeded"), index + 7);
        QCOMPARE(meta.indexOfProperty("rxFilter0LowPassReason"), index + 8);
        QCOMPARE(meta.indexOfProperty("rxFilter0LowPassSlice"), index + 9);
        QCOMPARE(meta.indexOfProperty("diversityState"), index + 10);
        QCOMPARE(meta.indexOfProperty("diversityState") - meta.propertyOffset(), 37);
        QCOMPARE(index, meta.propertyCount() - 11);
        RadioModel model;
        QCOMPARE(model.logCategoryList(), LogManager::instance().categoryListJson());
    }
};

QTEST_MAIN(TestLogCategoryList)
#include "tst_log_category_list.moc"
