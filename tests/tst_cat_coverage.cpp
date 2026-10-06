// no-port-check: NereusSDR-original production CAT registry and corpus verification.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "CatFixtureHarness.h"
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
QTEST_GUILESS_MAIN(TstCatCoverage)
#include "tst_cat_coverage.moc"
