// no-port-check: NereusSDR-original production CAT registry and corpus verification.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-26 CI load round: the fixture rows in four ctest entries (84 s of
//            ctest's 120 s on a quiet Linux runner). J.J. Boyd (KG4VCF),
//            AI-assisted via Claude Code.
#include <QtTest>
#include "CatFixtureHarness.h"
#include "TestFunctionGroups.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/cat/CatService.h"
#include "models/RadioModel.h"
using namespace NereusSDR;
class TstCatCoverage : public QObject {
 Q_OBJECT
private slots:
 void init() { AppSettings::instance().clear(); }
 void cleanup() { AppSettings::instance().clear(); }
 void allCurrentFixtures_data() {
  QTest::addColumn<QJsonObject>("fixture"); QFile file(QFINDTESTDATA("data/cat/requests.json")); QVERIFY(file.open(QIODevice::ReadOnly));
  const QJsonArray records=QJsonDocument::fromJson(file.readAll()).array(); QVERIFY(!records.isEmpty());
  for(const QJsonValue& value:records) { const QJsonObject fixture=value.toObject(); QVERIFY(!fixture.value("expectedReply").isNull()); QTest::newRow(qPrintable(fixture.value("id").toString())) << fixture; }
 }
 void allCurrentFixtures() {
  QFETCH(QJsonObject,fixture);
  const QString owner=fixture.value("executionStatus").toString();
  if(owner == "executed-task-5") { CatRxFixtureHarness harness; harness.run(fixture); }
  else if(owner == "executed-task-6") { CatDspFixtureHarness harness; harness.run(fixture); }
  else { CatTask7FixtureHarness harness; harness.run(fixture); }
 }
 void registryExactlyMatchesActiveCatalogue() {
  RadioModel model; CatCommandCatalog catalog; QVERIFY(catalog.isValid()); QList<QByteArray> expected;
  for (const CatDescriptor& descriptor:catalog.descriptors()) { if (descriptor.active) { expected.append(descriptor.code); } }
  std::sort(expected.begin(),expected.end()); QCOMPARE(expected.size(),349); QCOMPARE(model.catService()->router().registeredCodes(),expected);
  QFile file(QFINDTESTDATA("data/cat/compatibility.csv")); QVERIFY(file.open(QIODevice::ReadOnly)); file.readLine(); QSet<QByteArray> rows; int active=0,inactive=0;
  while(!file.atEnd()) { const QByteArray line=file.readLine().trimmed(); if(line.isEmpty()) {continue;} const QList<QByteArray> cells=line.split(','); QVERIFY(cells.size()>2); const QByteArray code=cells[0]; QVERIFY(!rows.contains(code)); rows.insert(code); const CatDescriptor* descriptor=catalog.find(code); QVERIFY(descriptor); QCOMPARE(descriptor->active,cells[1]=="True"); if(descriptor->active) {++active;} else {++inactive; QVERIFY(!model.catService()->router().registeredCodes().contains(code));} }
  QCOMPARE(rows.size(),419); QCOMPARE(active,349); QCOMPARE(inactive,70);
 }
};
// CI load round, 2026-10-26: 783 fixture rows, each on a model of its own,
// took 84 s on a quiet Linux runner and timed out on a busy one.
// allCurrentFixtures's rows go in turn to 4 entries, _fixtures1 to
// _fixtures4, built from the same list its _data function reads, so
// a new fixture lands in one with no list to edit; the rest runs the other
// functions (tests/CMakeLists.txt).
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TstCatCoverage test;
    QTEST_SET_MAIN_SOURCE_PATH
    QFile file(QFINDTESTDATA("data/cat/requests.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical("tests/data/cat/requests.json not found");
        return 1;
    }
    constexpr int kFixtureEntries = 4;
    QList<NereusSDR::TestFunctionGroups::Group> groups{{QStringLiteral("fixtures1"), {}}, {QStringLiteral("fixtures2"), {}}, {QStringLiteral("fixtures3"), {}}, {QStringLiteral("fixtures4"), {}}};
    int index = 0;
    for (const QJsonValue& record : QJsonDocument::fromJson(file.readAll()).array()) {
        groups[index++ % kFixtureEntries].entries.append(
            QStringLiteral("allCurrentFixtures:") + record.toObject().value(QStringLiteral("id")).toString());
    }
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_CAT_COVERAGE_GROUP", groups);
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}
#include "tst_cat_coverage.moc"
