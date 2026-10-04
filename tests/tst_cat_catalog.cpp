// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QSet>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "core/cat/CatCommandCatalog.h"
using namespace NereusSDR;
class TestCatCatalog : public QObject {
    Q_OBJECT
private slots:
    void catalogue() {
        CatCommandCatalog catalog;
        QVERIFY(catalog.isValid());
        QCOMPARE(catalog.descriptors().size(), 419);
        int active = 0;
        int standard = 0;
        QSet<QByteArray> codes;
        for (const CatDescriptor& descriptor : catalog.descriptors()) {
            QVERIFY(!codes.contains(descriptor.code));
            codes.insert(descriptor.code);
            if (descriptor.active) {
                ++active;
                if (descriptor.code.size() == 2) { ++standard; }
            }
        }
        QCOMPARE(active, 349);
        QCOMPARE(standard, 39);
        QCOMPARE(active - standard, 310);
        QCOMPARE(catalog.descriptors().size() - active, 70);
        QVERIFY(catalog.find("ZZHW")->active);
        QVERIFY(!catalog.find("AN")->active);
        QCOMPARE(catalog.find("FA")->setWidth, 11);
        QCOMPARE(catalog.find("FA")->getWidth, 0);
        QCOMPARE(catalog.find("FA")->answerWidth, 11);
        QCOMPARE(catalog.find("ZZEA")->setWidth, 36);
        QCOMPARE(catalog.find("ZZEA")->getWidth, 0);
        QCOMPARE(catalog.find("ZZEA")->answerWidth, 36);
        QCOMPARE(catalog.maximumRequestBytes(), qsizetype(41));
        QVERIFY(!catalog.find("NOPE"));
    }
    void fixtureCoverage() {
        CatCommandCatalog catalog;
        QVERIFY(catalog.isValid());
        QFile fixtures(QFINDTESTDATA("data/cat/requests.json"));
        QVERIFY(fixtures.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(fixtures.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(document.isArray());
        QSet<QString> ids;
        QSet<QByteArray> coveredCodes;
        for (const QJsonValue& value : document.array()) {
            const QJsonObject fixture = value.toObject();
            const QString id = fixture.value("id").toString();
            QVERIFY(!id.isEmpty());
            QVERIFY(!ids.contains(id));
            ids.insert(id);
            const QByteArray code = fixture.value("command").toString().toLatin1();
            const CatDescriptor* descriptor = catalog.find(code);
            QVERIFY(descriptor);
            coveredCodes.insert(code);
            const QByteArray request = fixture.value("request").toString().toLatin1();
            QVERIFY(request.startsWith(code));
            QVERIFY(request.endsWith(';'));
            QVERIFY(!fixture.value("source").toString().isEmpty());
            QVERIFY(!fixture.value("precondition").toString().isEmpty());
            const QString form = fixture.value("form").toString();
            QVERIFY(form == "Get" || form == "Set");
            if (descriptor->active) {
                const int width = form == "Get" ? descriptor->getWidth : descriptor->setWidth;
                QVERIFY(width >= 0);
                QCOMPARE(request.size(), code.size() + width + 1);
                QVERIFY(request.size() <= catalog.maximumRequestBytes());
            }
            if (!descriptor->active || descriptor->outcome == CatOutcome::Unavailable) {
                QCOMPARE(fixture.value("expectedReply").toString(), QStringLiteral("?;"));
                QCOMPARE(fixture.value("expectedMutation").toString(), QStringLiteral("none"));
            }
            if (fixture.value("expectedReply").isNull()) {
                QCOMPARE(fixture.value("fixtureStatus").toString(), QStringLiteral("pending-family-model"));
            }
        }
        QCOMPARE(coveredCodes.size(), catalog.descriptors().size());
        for (const CatDescriptor& descriptor : catalog.descriptors()) {
            if (!descriptor.active) { continue; }
            if (descriptor.getWidth >= 0 && !(descriptor.setWidth == descriptor.getWidth && descriptor.formPrecedence == CatFormPrecedence::SetFirst)) {
                QVERIFY(ids.contains(QString::fromLatin1(descriptor.code) + "-get"));
            }
            if (descriptor.setWidth >= 0 && !(descriptor.setWidth == descriptor.getWidth && descriptor.formPrecedence == CatFormPrecedence::GetFirst)) {
                QVERIFY(ids.contains(QString::fromLatin1(descriptor.code) + "-set"));
            }
        }
        QFile matrix(QFINDTESTDATA("data/cat/compatibility.csv"));
        QVERIFY(matrix.open(QIODevice::ReadOnly));
        QCOMPARE(matrix.readLine().trimmed(), QByteArray("Command,Active,Forms,Widths,SourceRange,Family,TargetApi,Scale,Outcome,ReadContract,SetContract,Fixture"));
        QSet<QByteArray> matrixCodes;
        while (!matrix.atEnd()) {
            const QByteArray row = matrix.readLine();
            const QByteArray code = row.left(row.indexOf(','));
            QVERIFY(catalog.find(code));
            QVERIFY(!matrixCodes.contains(code));
            matrixCodes.insert(code);
            QVERIFY(row.contains("requests.json#" + code + "-"));
        }
        QCOMPARE(matrixCodes, coveredCodes);
    }
    void invalidInput() {
        CatCommandCatalog missing(QStringLiteral(":/cat/missing.xml"));
        QVERIFY(!missing.isValid());
        QVERIFY(missing.descriptors().isEmpty());
        QCOMPARE(missing.maximumRequestBytes(), qsizetype(0));
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("<catstructs><catstruct code=\"FA\"><active>true</active></catstruct></catstructs>");
        file.flush();
        CatCommandCatalog malformed(file.fileName());
        QVERIFY(!malformed.isValid());
        QVERIFY(malformed.descriptors().isEmpty());
        QVERIFY(!malformed.errorString().isEmpty());
        QFile source(QStringLiteral(":/cat/CATStructs.xml"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QByteArray xml = source.readAll();
        for (const QByteArray& invalid : {QByteArray(xml).replace("code=\"AC\"", "code=\"AG\""),
                                       QByteArray(xml).replace("<active>false</active>", "<active>invalid</active>"),
                                       QByteArray(xml).replace("<nsetparms>3</nsetparms>", "<nsetparms>-2</nsetparms>")}) {
            file.resize(0);
            file.seek(0);
            QCOMPARE(file.write(invalid), invalid.size());
            file.flush();
            CatCommandCatalog invalidCatalog(file.fileName());
            QVERIFY(!invalidCatalog.isValid());
            QVERIFY(invalidCatalog.descriptors().isEmpty());
            QVERIFY(!invalidCatalog.errorString().isEmpty());
        }
    }
};
QTEST_GUILESS_MAIN(TestCatCatalog)
#include "tst_cat_catalog.moc"
