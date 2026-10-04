// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "core/cat/CatParser.h"
#include "core/cat/CatCommandRouter.h"
using namespace NereusSDR;
class TestCatParser : public QObject {
    Q_OBJECT
private slots:
    void validation() {
        CatParser parser;
        const CatValidation get = parser.validate("fa;");
        QVERIFY(get.request);
        QCOMPARE(get.request->code, QByteArray("FA"));
        QCOMPARE(get.request->form, CatForm::Get);
        QCOMPARE(parser.validate("FA00014074000;").request->form, CatForm::Set);
        for (const QByteArray& frame : {QByteArray("FA123;"), QByteArray("FAabcdefghijk;"), QByteArray("AN;"), QByteArray("FA"), QByteArray("QQ;")}) {
            const CatValidation result = parser.validate(frame);
            QVERIFY(!result.request);
            QCOMPARE(parser.formatValidationError(result, {1, 1}), QByteArray("?;"));
        }
        QVERIFY(parser.validate("FA00014Vv4000;").request);
        const QByteArray equalizer="zzEA01+001-002+003-004+005-006+007-00800;";
        const CatValidation eq=parser.validate(equalizer);
        QVERIFY(eq.request);
        QCOMPARE(eq.request->code,QByteArray("ZZEA"));
        QCOMPARE(eq.request->suffix,equalizer.mid(4,equalizer.size()-5));
        QVERIFY(parser.validate(";;;fa;").request);
        for (const QByteArray& frame : {QByteArray("ZZGA12345678-1234-1234-1234-123456789ABC;"), QByteArray("ZZKYMiXeD words and spaces   ;")}) {
            const CatValidation result = parser.validate(frame);
            QVERIFY(result.request);
            QCOMPARE(result.request->suffix, frame.mid(4, frame.size() - 5));
        }
    }
    void errors() {
        CatParser parser;
        CatSessionContext verbose{1, 1, true};
        const QList<QPair<QByteArray,QByteArray>> cases{{"f", "ZZEM:f:Bad Command Name;"}, {"AN;", "ZZEM:AN;:Inactive Command;"}, {"qq;", "ZZEM:qq;:Unknown Command;"}, {"faabcdefghijk;", "ZZEM:FA:Suffix Format Error;"}, {"fa123;", "ZZEM:FA123:Suffix Length Error;"}};
        for (const auto& pair : cases) {
            QCOMPARE(parser.formatValidationError(parser.validate(pair.first), verbose), pair.second);
        }
        CatCommandCatalog catalog;
        const CatRequest request{"FA", "", CatForm::Get};
        QCOMPARE(parser.format(*catalog.find("FA"), request, {CatResultKind::Error,"?;",7}, verbose), QByteArray("?;"));
        const CatRequest extended{"ZZFA","",CatForm::Get};
        QCOMPARE(parser.format(*catalog.find("ZZFA"),extended,{CatResultKind::Error,"?;",7},verbose),QByteArray("ZZEM:ZZFA:Feature Not Available;"));
        const QList<QByteArray> texts{"Undefined Error;","Bad Command Name;","Inactive Command;","Unknown Command;","Undefined Command Error;","Suffix Format Error;","Suffix Length Error;","Feature Not Available;","Form Must Be Open;","Value out of bounds;","Power must be on;"};
        for (int code=0; code<texts.size(); ++code) {
            QCOMPARE(parser.format(*catalog.find("ZZFA"),extended,{CatResultKind::Error,"?;",code},verbose),QByteArray("ZZEM:ZZFA:")+texts[code]);
        }
        for (const QByteArray& error : {QByteArray("?;"), QByteArray("E;"), QByteArray("O;")}) {
            QCOMPARE(parser.format(*catalog.find("FA"), request, {CatResultKind::Error,error}, {1,1}), error);
        }
    }
    void framing() {
        CatParser parser;
        CatCommandCatalog catalog;
        const CatSessionContext session{1,1};
        QCOMPARE(parser.format(*catalog.find("ID"), {"ID","",CatForm::Get}, {CatResultKind::Payload,"019"}, session), QByteArray("ID019;"));
        QCOMPARE(parser.format(*catalog.find("ID"), {"ID","",CatForm::Get}, {CatResultKind::Payload,"19"}, session), QByteArray("O;"));
        QCOMPARE(parser.format(*catalog.find("FA"), {"FA","00014074000",CatForm::Set}, {CatResultKind::Silence,{}}, session), QByteArray());
        QCOMPARE(parser.format(*catalog.find("FA"), {"FA","",CatForm::Get}, {CatResultKind::Wire,"FA00014074000;"}, session), QByteArray("FA00014074000;"));
        CatDescriptor descriptor; descriptor.code="ZZRM"; descriptor.answerWidth=4;
        QCOMPARE(parser.format(descriptor,{"ZZRM","0",CatForm::Get},{CatResultKind::Payload," 12 "},session),QByteArray("ZZRM012;"));
        descriptor.code="ZZML";
        QCOMPARE(parser.format(descriptor,{"ZZML","0",CatForm::Get},{CatResultKind::Payload," 12 "},session),QByteArray("ZZML0 12 ;"));
        descriptor.code="ZZMN";
        QCOMPARE(parser.format(descriptor,{"ZZMN","0",CatForm::Get},{CatResultKind::Payload," 12 "},session),QByteArray("ZZMN0 12 ;"));
    }
    void registryAndSessions() {
        CatParser parser;
        CatCommandRouter router;
        int calls=0;
        const auto handler=[&calls](const CatRequest&,CatSessionContext&) { ++calls; return CatCommandResult{CatResultKind::Payload,"019"}; };
        QVERIFY(router.registerHandler("ID",handler));
        QVERIFY(!router.registerHandler("ID",handler));
        QVERIFY(!router.registerHandler("AN",handler));
        QVERIFY(!router.registerHandler("QQ",handler));
        QVERIFY(!router.registerHandler("FA",{}));
        QVERIFY(router.registeredCodes().contains("ID"));
        CatSessionContext a{1,1}, b{2,1};
        for (const QByteArray& bad : {QByteArray("ID1;"),QByteArray("AN;")}) {
            const CatValidation validation=parser.validate(bad);
            if (validation.request) { router.execute(*validation.request,a); }
        }
        QCOMPARE(calls,0);
        QCOMPARE(router.execute(*parser.validate("ID;").request,a).data,QByteArray("019"));
        QCOMPARE(calls,1);
        router.execute(*parser.validate("ZZEM1;").request,a);
        QVERIFY(a.verboseErrors);
        QVERIFY(!b.verboseErrors);
        QCOMPARE(router.execute(*parser.validate("ZZEM;").request,a).data,QByteArray("1"));
        QCOMPARE(router.execute(*parser.validate("ZZEM;").request,b).data,QByteArray("0"));
        QCOMPARE(router.execute(*parser.validate("ZZEM2;").request,a).kind,CatResultKind::Error);
        const CatCommandResult missing=router.execute(*parser.validate("FA;").request,a);
        QCOMPARE(missing.kind,CatResultKind::Error);
        QCOMPARE(missing.verboseErrorCode,7);
        const CatCommandResult guid=router.execute(*parser.validate("ZZGA12345678-1234-1234-1234-123456789ABC;").request,a);
        QCOMPARE(guid.kind,CatResultKind::Wire);
        QCOMPARE(guid.data,QByteArray("ZZGA12345678-1234-1234-1234-123456789abc;"));
        QCOMPARE(router.execute(*parser.validate("ZZGAxxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx;").request,a).kind,CatResultKind::Error);
    }
    void requestFixtures() {
        CatParser parser;
        CatCommandCatalog catalog;
        QFile file(QFINDTESTDATA("data/cat/requests.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonArray fixtures=QJsonDocument::fromJson(file.readAll()).array();
        int accepted=0, rejected=0, framed=0, pending=0;
        for (const QJsonValue& value : fixtures) {
            const QJsonObject fixture=value.toObject();
            const QByteArray frame=fixture.value("request").toString().toLatin1();
            const CatDescriptor* descriptor=catalog.find(fixture.value("command").toString().toLatin1());
            QVERIFY(descriptor);
            const CatValidation validation=parser.validate(frame);
            if (!descriptor->active) {
                QVERIFY2(!validation.request, frame.constData());
                QCOMPARE(parser.formatValidationError(validation,{1,1}),QByteArray("?;"));
                ++rejected;
                continue;
            }
            QVERIFY2(validation.request.has_value(),frame.constData());
            ++accepted;
            QCOMPARE(validation.request->form,fixture.value("form").toString()=="Get" ? CatForm::Get : CatForm::Set);
            if (fixture.value("expectedReply").isNull()) { ++pending; continue; }
            const QByteArray wire=fixture.value("expectedReply").toString().toLatin1();
            CatCommandResult supplied{CatResultKind::Payload,{}};
            if (wire.isEmpty()) { supplied.kind=CatResultKind::Silence; }
            else if (wire=="?;") { supplied.kind=CatResultKind::Error; supplied.data=wire; }
            else if (descriptor->suffixKind==CatSuffixKind::Guid) { supplied.kind=CatResultKind::Wire; supplied.data=wire; }
            else {
                const qsizetype prefix=descriptor->code.size()+(descriptor->code.startsWith("ZZ") ? validation.request->suffix.size() : 0);
                supplied.data=wire.mid(prefix,wire.size()-prefix-1);
            }
            QCOMPARE(parser.format(*descriptor,*validation.request,supplied,{1,1}),wire);
            ++framed;
        }
        QCOMPARE(fixtures.size(),752);
        QCOMPARE(accepted+rejected,752);
        QCOMPARE(pending,186);
        QCOMPARE(framed+rejected,566);
        qInfo()<<"Validated requests:"<<accepted<<"active,"<<rejected<<"inactive; supplied result frames:"<<framed<<"; pending family fixtures:"<<pending;
    }
};
QTEST_GUILESS_MAIN(TestCatParser)
#include "tst_cat_parser.moc"
