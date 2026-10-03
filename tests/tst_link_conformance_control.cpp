// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_conformance_control.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 3 (R-IOS-01): the station runs the link's control
// fixtures (tests/data/link/v1/control/*.json). Each holds one wire message
// and whether it decodes; the station's decoder must agree, and a message
// that decodes must encode back to the same object. The app runs the same
// files against its own codec.
//
// Also checks manifest.json itself (LinkFixtures::checkManifest), that the
// fixtures cover every message kind in each direction it travels, and that
// an altered fixture fails with a message that says what differs.
//
//   cmake --build build --target tst_link_conformance_control
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build \
//       -R '^tst_link_conformance_control$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): control
//                                    conformance runner. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): runs once per link major in
//                                    the manifest.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    linkMajors read against the
//                                    station's supported majors.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08): the
//                                    pair.* kinds in both directions.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 74 (R-IOS-30):
//                                    confirm.request and notice, from the
//                                    station. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25): record.batch
//                                    placed as a station kind. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  iPhone app plan Task 29 (R-IOS-16):
//                                    path.join and path.switch from the
//                                    client, path.switch from the station.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include "core/session/LinkVersion.h"
#include "core/session/SessionMessages.h"

#include "LinkFixtures.h"

using namespace NereusSDR;
using NereusSDR::Test::LinkFixtures;

namespace {

// The kinds each end sends, from the dispatch switches that accept them:
// StationServer::onTransportText for the client's, StationClient's inbound
// switch for the station's. A kind listed on both sides travels both ways.
const QStringList& clientKinds()
{
    static const QStringList kinds{
        QStringLiteral("hello"),          QStringLiteral("auth.request"),
        QStringLiteral("command.invoke"), QStringLiteral("media.control"),
        QStringLiteral("property.write"), QStringLiteral("settings.write"),
        QStringLiteral("settings.remove"),
        // iPhone app Task 14: pairing (pair.fail when the device's own
        // step 3 fails).
        QStringLiteral("pair.start"), QStringLiteral("pair.spake"),
        QStringLiteral("pair.confirm"), QStringLiteral("pair.fail"),
        // iPhone app plan Task 29: moving a session to another connection.
        QStringLiteral("path.join"), QStringLiteral("path.switch"),
        QStringLiteral("session.takeover"),
    };
    return kinds;
}

const QStringList& stationKinds()
{
    static const QStringList kinds{
        QStringLiteral("hello"),           QStringLiteral("auth.result"),
        QStringLiteral("capabilities"),    QStringLiteral("settings.snapshot"),
        QStringLiteral("schema"),          QStringLiteral("object.create"),
        QStringLiteral("object.destroy"),  QStringLiteral("delta"),
        QStringLiteral("property.result"), QStringLiteral("snapshot.complete"),
        QStringLiteral("command.result"),  QStringLiteral("settings.value"),
        QStringLiteral("settings.reject"), QStringLiteral("session.end"),
        QStringLiteral("media.control"),   QStringLiteral("station.metrics.v1"),
        // iPhone app Task 14: pairing.
        QStringLiteral("pair.accept"),     QStringLiteral("pair.spake"),
        QStringLiteral("pair.confirm"),    QStringLiteral("pair.fail"),
        // iPhone app Task 74: asking first, telling afterwards.
        QStringLiteral("confirm.request"), QStringLiteral("notice"),
        // Parity Task 19: a record stream's changes.
        QStringLiteral("record.batch"),
        // iPhone app plan Task 29: the barrier of a move.
        QStringLiteral("path.switch"),
        QStringLiteral("session.held"),
    };
    return kinds;
}

} // namespace

class TstLinkConformanceControl : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void manifestIsWellFormed();
    void directionsCoverEveryKind();
    void everyKindIsCoveredInEachDirection();
    void controlFixtures_data();
    void controlFixtures();
    void alteredFixturesFailReadably();

private:
    QJsonObject fixture(const QString& id);
    QJsonObject m_manifest;
};

void TstLinkConformanceControl::initTestCase()
{
    QString error;
    m_manifest = LinkFixtures::readObject(
        QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("manifest.json")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

QJsonObject TstLinkConformanceControl::fixture(const QString& id)
{
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("control"))) {
        if (entry.id == id) {
            QString error;
            const QJsonObject o = LinkFixtures::readObject(
                QDir(LinkFixtures::dataDirectory()).filePath(entry.file), &error);
            return error.isEmpty() ? o : QJsonObject{};
        }
    }
    return {};
}

void TstLinkConformanceControl::manifestIsWellFormed()
{
    const QString problem =
        LinkFixtures::checkManifest(m_manifest, LinkFixtures::dataDirectory());
    QVERIFY2(problem.isEmpty(), qPrintable(problem));

    // The checker itself refuses a broken manifest, with a reason.
    QJsonObject broken = m_manifest;
    QJsonArray fixtures = broken.value(QStringLiteral("fixtures")).toArray();
    QJsonObject first = fixtures.at(0).toObject();
    first.insert(QStringLiteral("file"), QStringLiteral("control/no-such-fixture.json"));
    fixtures.replace(0, first);
    broken.insert(QStringLiteral("fixtures"), fixtures);
    const QString refused = LinkFixtures::checkManifest(broken, LinkFixtures::dataDirectory());
    QVERIFY2(refused.contains(QStringLiteral("no-such-fixture.json")), qPrintable(refused));

    // linkMajors is read against the majors this station supports.
    QJsonObject unsupported = m_manifest;
    QJsonArray majors;
    for (const quint16 major : LinkVersion::supportedMajors()) {
        majors.append(int(major));
    }
    majors.append(int(LinkVersion::supportedMajors().last()) + 1);
    unsupported.insert(QStringLiteral("linkMajors"), majors);
    const QString beyond =
        LinkFixtures::checkManifest(unsupported, LinkFixtures::dataDirectory());
    QVERIFY2(beyond.contains(QStringLiteral("does not support")), qPrintable(beyond));
}

void TstLinkConformanceControl::directionsCoverEveryKind()
{
    // A kind added to SessionMessageKind must be placed in a direction here,
    // and so get fixtures.
    QSet<QString> placed;
    for (const QString& kind : clientKinds()) {
        placed.insert(kind);
    }
    for (const QString& kind : stationKinds()) {
        placed.insert(kind);
    }
    for (const SessionMessageKind kind : SessionMessages::allKinds()) {
        const QString name = QString::fromUtf8(SessionMessages::kindName(kind));
        QVERIFY2(placed.remove(name), qPrintable(name + QStringLiteral(" has no direction")));
    }
    QVERIFY2(placed.isEmpty(), qPrintable(QStringList(placed.values()).join(QLatin1Char(' '))));
}

void TstLinkConformanceControl::everyKindIsCoveredInEachDirection()
{
    QSet<QString> covered;
    for (const LinkFixtures::Entry& entry :
         LinkFixtures::entries(m_manifest, QStringLiteral("control"))) {
        QString error;
        const QJsonObject o = LinkFixtures::readObject(
            QDir(LinkFixtures::dataDirectory()).filePath(entry.file), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        if (!o.value(QStringLiteral("decodes")).toBool()) {
            continue;
        }
        covered.insert(o.value(QStringLiteral("from")).toString() + QLatin1Char(' ')
                       + o.value(QStringLiteral("wire")).toObject().value(QStringLiteral("type"))
                             .toString());
    }
    for (const QString& kind : clientKinds()) {
        QVERIFY2(covered.contains(QStringLiteral("client ") + kind),
                 qPrintable(QStringLiteral("no decoding fixture for client ") + kind));
    }
    for (const QString& kind : stationKinds()) {
        QVERIFY2(covered.contains(QStringLiteral("station ") + kind),
                 qPrintable(QStringLiteral("no decoding fixture for station ") + kind));
    }
}

void TstLinkConformanceControl::controlFixtures_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("major");
    const QList<LinkFixtures::Entry> entries =
        LinkFixtures::entries(m_manifest, QStringLiteral("control"));
    QVERIFY(!entries.isEmpty());
    // Once per link major the suite covers (manifest linkMajors).
    for (const quint16 major : LinkFixtures::linkMajors(m_manifest)) {
        for (const LinkFixtures::Entry& entry : entries) {
            QTest::newRow(qPrintable(QStringLiteral("%1 link %2").arg(entry.id).arg(major)))
                << entry.file << int(major);
        }
    }
}

void TstLinkConformanceControl::controlFixtures()
{
    QFETCH(QString, file);
    QFETCH(int, major);
    QVERIFY2(LinkVersion::supportedMajors().contains(quint16(major)),
             qPrintable(QStringLiteral("this station does not offer link major %1").arg(major)));
    QString error;
    const QJsonObject o =
        LinkFixtures::readObject(QDir(LinkFixtures::dataDirectory()).filePath(file), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QString failure = LinkFixtures::runControl(o);
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
}

void TstLinkConformanceControl::alteredFixturesFailReadably()
{
    // A decoding fixture whose expected verdict is flipped.
    QJsonObject flipped = fixture(QStringLiteral("control-client-hello"));
    QVERIFY(!flipped.isEmpty());
    flipped.insert(QStringLiteral("decodes"), false);
    QString failure = LinkFixtures::runControl(flipped);
    QVERIFY2(failure.contains(QStringLiteral("accepted a message the fixture says it refuses")),
             qPrintable(failure));

    // A refusal fixture whose expected verdict is flipped.
    QJsonObject refused = fixture(QStringLiteral("control-unknown-type"));
    QVERIFY(!refused.isEmpty());
    refused.insert(QStringLiteral("decodes"), true);
    failure = LinkFixtures::runControl(refused);
    QVERIFY2(failure.contains(QStringLiteral("refused a message the fixture says decodes")),
             qPrintable(failure));

    // A key the encoder does not write back: the difference names its path.
    QJsonObject extra = fixture(QStringLiteral("control-station-auth-result"));
    QVERIFY(!extra.isEmpty());
    QJsonObject wire = extra.value(QStringLiteral("wire")).toObject();
    wire.insert(QStringLiteral("conformanceExtra"), 1);
    extra.insert(QStringLiteral("wire"), wire);
    failure = LinkFixtures::runControl(extra);
    QVERIFY2(failure.contains(QStringLiteral("$.conformanceExtra")), qPrintable(failure));
}

QTEST_MAIN(TstLinkConformanceControl)
#include "tst_link_conformance_control.moc"
