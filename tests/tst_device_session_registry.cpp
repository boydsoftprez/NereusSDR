// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_device_session_registry.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 71 (R-IOS-02): the record of who holds a place on the
// Core, over an injected clock (no sleeps).
//
// Refusals first: the fifth device while four places are taken, whether
// they are live, away or a hosting desktop's own window. Then who is let in
// without asking: a device that already holds a place (live, or away within
// its 180 s), which keeps its place and order and never ends anyone else.
// Then the away state and its end (the place freed at 180 s, not 179 s; the
// record that the time ran out, kept until the next admission or a revoke),
// token windows and leaving on purpose (no grace), activity measured at
// most once a minute, the one numbering rule for names and short names, and
// the redial arithmetic of the design's section 4.5 as a table.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 71 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8: each away period's generation
//               and isCurrentAbsence. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: a device with no name is never
//               numbered into one. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QSignalSpy>

#include <vector>

#include "core/security/DeviceStore.h"
#include "core/session/DeviceSessionRegistry.h"

using namespace NereusSDR;

namespace {

using Registry = DeviceSessionRegistry;

Registry::Entry paired(const char* id, const QString& name = QStringLiteral("iPhone"),
                       const QString& shortName = QString())
{
    Registry::Entry e;
    e.deviceId = QByteArray(id);
    e.kind = Registry::Kind::Paired;
    e.name = name;
    e.shortName = shortName;
    e.deviceKind = QStringLiteral("phone");
    return e;
}

// A session handle: any QObject the test owns.
struct Sessions {
    std::vector<std::unique_ptr<QObject>> objects;
    const QObject* make()
    {
        objects.push_back(std::make_unique<QObject>());
        return objects.back().get();
    }
};

} // namespace

class TstDeviceSessionRegistry : public QObject {
    Q_OBJECT

private slots:
    // ---- Refusals ----
    void aFifthDeviceIsRefusedWhileFourHoldPlaces();
    void anAwayDeviceKeepsItsPlaceAgainstAFifth();
    void aHostingDesktopTakesAPlace();

    // ---- Admission without a question ----
    void fourDevicesAreAdmittedInOrder();
    void theSameLiveDeviceReplacesItsOwnSessionOnly();
    void anAwayDeviceComesBackAt179Seconds();
    void anOldSessionEndingAfterItsReplacementChangesNothing();

    // ---- Away and its end ----
    void theGracePeriodEndsAt180Seconds();
    void anAdmissionExpiresAwayDevicesFirst();
    void theRecordThatTimeRanOutIsKeptUntilAdmissionOrRevoke();
    void aTokenWindowFreesItsPlaceAtOnce();
    void leavingOnPurposeFreesThePlaceAtOnce();
    void revokingAnAwayDeviceFreesItsPlaceAtOnce();
    void eachAbsenceHasItsOwnGeneration();

    // ---- Activity and change ----
    void activityIsReportedAtMostOnceAMinute();
    void changesAreSignalledAndTimeAloneIsNot();

    // ---- Names ----
    void namesAreNumberedByPairingOrder();
    void aDeviceWithNoNameIsNeverNumberedIntoOne();
    void shortNamesAreNumberedOnTheirOwnCollisions();
    void aNumberNeverRepeatsAnotherDevicesName();
    void anUnusableShortNameFallsBackToTheKindsWord();
    void aTokenWindowIsNamedByItsAddress();

    // ---- The redial arithmetic (design section 4.5) ----
    void redialsFallInsideTheGracePeriod();
    void replacementCandidatesUseActualActivityAndRetainRecords();
};

void TstDeviceSessionRegistry::aFifthDeviceIsRefusedWhileFourHoldPlaces()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    for (const char* id : {"a", "b", "c", "d"}) {
        QCOMPARE(registry.admit(paired(id), sessions.make()).admission,
                 Registry::Admission::Admitted);
    }
    QCOMPARE(registry.placesTaken(), 4);
    const Registry::AdmitResult fifth = registry.admit(paired("e"), sessions.make());
    QCOMPARE(fifth.admission, Registry::Admission::Full);
    QVERIFY(fifth.replacedSession == nullptr);
    QCOMPARE(registry.placesTaken(), 4);
    QVERIFY(!registry.entry("e"));
    // A token window meets the same full Core.
    Registry::Entry token;
    token.deviceId = registry.nextTokenDeviceId();
    token.kind = Registry::Kind::Token;
    token.deviceKind = QStringLiteral("computer");
    QCOMPARE(registry.admit(token, sessions.make()).admission, Registry::Admission::Full);
    // Nobody else's place moved.
    for (const char* id : {"a", "b", "c", "d"}) {
        QVERIFY(registry.entry(id));
        QCOMPARE(registry.entry(id)->state, Registry::State::Listening);
    }
}

void TstDeviceSessionRegistry::replacementCandidatesUseActualActivityAndRetainRecords()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* a = sessions.make();
    const QObject* b = sessions.make();
    const Registry::AdmitResult first = registry.admit(paired("a"), a);
    QVERIFY(!first.placeTaken);
    registry.admit(paired("b"), b);
    registry.admit(paired("c"), sessions.make());
    const Registry::AdmitResult returnWithoutTake = registry.admit(paired("c"), sessions.make());
    QVERIFY(!returnWithoutTake.placeTaken);
    registry.registerHostingDevice("desk", QStringLiteral("Desk"), QStringLiteral("Desk"));
    now = 1000;
    registry.noteActivity("a");
    now = 2000;
    registry.noteActivity("b");
    registry.sessionEnded("b", b, Registry::EndKind::Dropped);
    QList<Registry::Entry> candidates = registry.replacementCandidates("c");
    QCOMPARE(candidates.at(0).deviceId, QByteArray("b"));
    QCOMPARE(candidates.last().deviceId, QByteArray("c"));
    const quint32 before = registry.revision();
    now = 3000;
    registry.noteActivity("a");
    QVERIFY(registry.revision() != before);
    registry.replace("b", "new", QStringLiteral("Pat's iPad"));
    QVERIFY(!registry.entry("b"));
    QCOMPARE(registry.placeTakenBy("b")->byName, QStringLiteral("Pat's iPad"));
    QCOMPARE(registry.admit(paired("b"), sessions.make()).placeTaken->byId,
             QByteArray("new"));
    QVERIFY(!registry.placeTakenBy("b"));
    registry.remove("a");
    QVERIFY(!registry.entry("a"));
}

void TstDeviceSessionRegistry::anAwayDeviceKeepsItsPlaceAgainstAFifth()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* first = sessions.make();
    registry.admit(paired("a"), first);
    for (const char* id : {"b", "c", "d"}) {
        registry.admit(paired(id), sessions.make());
    }
    registry.sessionEnded("a", first, Registry::EndKind::Dropped);
    QCOMPARE(registry.entry("a")->state, Registry::State::Away);
    QCOMPARE(registry.placesTaken(), 4);
    now = Registry::kGraceMs - 1;
    QCOMPARE(registry.admit(paired("e"), sessions.make()).admission, Registry::Admission::Full);
}

void TstDeviceSessionRegistry::aHostingDesktopTakesAPlace()
{
    Registry registry;
    Sessions sessions;
    registry.registerHostingDevice("desk", QStringLiteral("Shack Mac"), QStringLiteral("Mac"));
    QCOMPARE(registry.placesTaken(), 1);
    const Registry::Entry hosting = *registry.entry("desk");
    QCOMPARE(hosting.kind, Registry::Kind::Hosting);
    QCOMPARE(hosting.deviceKind, QStringLiteral("station"));
    QVERIFY(hosting.session == nullptr);
    for (const char* id : {"a", "b", "c"}) {
        QCOMPARE(registry.admit(paired(id), sessions.make()).admission,
                 Registry::Admission::Admitted);
    }
    QCOMPARE(registry.admit(paired("d"), sessions.make()).admission, Registry::Admission::Full);
    registry.unregisterHostingDevice();
    QCOMPARE(registry.placesTaken(), 3);
    QCOMPARE(registry.admit(paired("d"), sessions.make()).admission,
             Registry::Admission::Admitted);
}

void TstDeviceSessionRegistry::fourDevicesAreAdmittedInOrder()
{
    Registry registry;
    qint64 now = 1000;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    for (const char* id : {"d", "b", "a", "c"}) {
        QCOMPARE(registry.admit(paired(id), sessions.make()).admission,
                 Registry::Admission::Admitted);
        now += 10;
    }
    QStringList order;
    for (const Registry::Entry& e : registry.entries()) {
        order.append(QString::fromLatin1(e.deviceId));
        QCOMPARE(e.state, Registry::State::Listening);
        QCOMPARE(e.reportedActivityMs, e.connectedSinceMs);
    }
    QCOMPARE(order, (QStringList{"d", "b", "a", "c"}));
}

void TstDeviceSessionRegistry::theSameLiveDeviceReplacesItsOwnSessionOnly()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* a1 = sessions.make();
    const QObject* b = sessions.make();
    registry.admit(paired("a", QStringLiteral("iPhone"), QStringLiteral("Old")), a1);
    registry.admit(paired("b"), b);
    registry.admit(paired("c"), sessions.make());
    registry.admit(paired("d"), sessions.make());
    now = 5000;
    // Full, yet the same device is let in at once, replacing only itself.
    const QObject* a2 = sessions.make();
    const Registry::AdmitResult again =
        registry.admit(paired("a", QStringLiteral("iPhone"), QStringLiteral("New")), a2);
    QCOMPARE(again.admission, Registry::Admission::SameDevice);
    QCOMPARE(again.replacedSession, a1);
    QCOMPARE(registry.placesTaken(), 4);
    const Registry::Entry a = *registry.entry("a");
    QCOMPARE(a.session, a2);
    QCOMPARE(a.order, quint64(1));
    QCOMPARE(a.connectedSinceMs, qint64(0));
    QCOMPARE(a.shortName, QStringLiteral("New"));
    QCOMPARE(registry.entries().first().deviceId, QByteArray("a"));
    QCOMPARE(registry.entry("b")->session, b);
    QCOMPARE(registry.entry("b")->state, Registry::State::Listening);
}

// Slice control plan Task 8: each away period has a generation, graceEnded
// carries it, and only the current absence is current.
void TstDeviceSessionRegistry::eachAbsenceHasItsOwnGeneration()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* first = sessions.make();
    registry.admit(paired("a"), first);
    registry.admit(paired("b"), sessions.make());
    QCOMPARE(registry.entry("a")->awayGeneration, quint64(0));
    QSignalSpy ended(&registry, &Registry::graceEnded);

    registry.sessionEnded("a", first, Registry::EndKind::Dropped);
    const quint64 g1 = registry.entry("a")->awayGeneration;
    QVERIFY(g1 != 0);
    QVERIFY(registry.isCurrentAbsence("a", g1));
    // Back: no absence is current.
    now = 170000;
    const QObject* second = sessions.make();
    registry.admit(paired("a"), second);
    QCOMPARE(registry.entry("a")->awayGeneration, quint64(0));
    QVERIFY(!registry.isCurrentAbsence("a", g1));
    // Away again: a new generation, the old one never current again.
    now = 175000;
    registry.sessionEnded("a", second, Registry::EndKind::Dropped);
    const quint64 g2 = registry.entry("a")->awayGeneration;
    QVERIFY(g2 != g1 && g2 != 0);
    QVERIFY(!registry.isCurrentAbsence("a", g1));
    QVERIFY(registry.isCurrentAbsence("a", g2));
    now = 180000;
    QVERIFY(registry.expireAway().isEmpty());
    QCOMPARE(ended.count(), 0);
    now = 175000 + Registry::kGraceMs;
    QCOMPARE(registry.expireAway(), QList<QByteArray>{QByteArray("a")});
    QCOMPARE(ended.count(), 1);
    QCOMPARE(ended.first().at(0).toByteArray(), QByteArray("a"));
    QCOMPARE(ended.first().at(1).value<quint64>(), g2);
    // No place held: the ended absence is still the current one.
    QVERIFY(registry.isCurrentAbsence("a", g2));
    // A device that never dropped holds no absence.
    QVERIFY(!registry.isCurrentAbsence("b", g2));
}

void TstDeviceSessionRegistry::anAwayDeviceComesBackAt179Seconds()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* first = sessions.make();
    registry.admit(paired("a"), first);
    registry.admit(paired("b"), sessions.make());
    now = 1000;
    registry.sessionEnded("a", first, Registry::EndKind::Dropped);
    QCOMPARE(registry.entry("a")->awaySinceMs, qint64(1000));
    now = 1000 + 179000;
    QVERIFY(registry.expireAway().isEmpty());
    const QObject* back = sessions.make();
    const Registry::AdmitResult result = registry.admit(paired("a"), back);
    QCOMPARE(result.admission, Registry::Admission::SameDevice);
    QVERIFY(result.replacedSession == nullptr);
    QVERIFY(!result.timeRanOutAtMs);
    QCOMPARE(registry.entry("a")->state, Registry::State::Listening);
    QCOMPARE(registry.entry("a")->session, back);
    QCOMPARE(registry.entries().first().deviceId, QByteArray("a"));
}

void TstDeviceSessionRegistry::anOldSessionEndingAfterItsReplacementChangesNothing()
{
    Registry registry;
    Sessions sessions;
    const QObject* a1 = sessions.make();
    const QObject* a2 = sessions.make();
    registry.admit(paired("a"), a1);
    registry.admit(paired("a"), a2);
    QSignalSpy changed(&registry, &Registry::changed);
    registry.sessionEnded("a", a1, Registry::EndKind::Dropped);
    QCOMPARE(registry.entry("a")->state, Registry::State::Listening);
    QCOMPARE(registry.entry("a")->session, a2);
    QCOMPARE(changed.count(), 0);
}

void TstDeviceSessionRegistry::theGracePeriodEndsAt180Seconds()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* a = sessions.make();
    registry.admit(paired("a"), a);
    now = 2000;
    registry.sessionEnded("a", a, Registry::EndKind::Dropped);
    QCOMPARE(registry.nextExpiryMs(), std::optional<qint64>(2000 + Registry::kGraceMs));
    now = 2000 + Registry::kGraceMs - 1;
    QVERIFY(registry.expireAway().isEmpty());
    QCOMPARE(registry.placesTaken(), 1);
    now = 2000 + Registry::kGraceMs;
    QCOMPARE(registry.expireAway(), (QList<QByteArray>{"a"}));
    QCOMPARE(registry.placesTaken(), 0);
    QVERIFY(!registry.entry("a"));
    QCOMPARE(registry.timeRanOutAtMs("a"), std::optional<qint64>(2000 + Registry::kGraceMs));
    QVERIFY(!registry.nextExpiryMs());
}

void TstDeviceSessionRegistry::anAdmissionExpiresAwayDevicesFirst()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* a = sessions.make();
    registry.admit(paired("a"), a);
    for (const char* id : {"b", "c", "d"}) {
        registry.admit(paired(id), sessions.make());
    }
    registry.sessionEnded("a", a, Registry::EndKind::Dropped);
    now = Registry::kGraceMs;
    // No expiry has run yet; the admission runs it.
    QCOMPARE(registry.admit(paired("e"), sessions.make()).admission,
             Registry::Admission::Admitted);
    QVERIFY(!registry.entry("a"));
    QVERIFY(registry.timeRanOutAtMs("a"));
}

void TstDeviceSessionRegistry::theRecordThatTimeRanOutIsKeptUntilAdmissionOrRevoke()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    const QObject* a = sessions.make();
    const QObject* b = sessions.make();
    registry.admit(paired("a"), a);
    registry.admit(paired("b"), b);
    registry.sessionEnded("a", a, Registry::EndKind::Dropped);
    registry.sessionEnded("b", b, Registry::EndKind::Dropped);
    now = Registry::kGraceMs;
    QCOMPARE(registry.expireAway().size(), 2);
    // Still recorded later on.
    now = Registry::kGraceMs * 10;
    QVERIFY(registry.timeRanOutAtMs("a"));
    // Its next admission reads it and clears it.
    const Registry::AdmitResult back = registry.admit(paired("a"), sessions.make());
    QCOMPARE(back.admission, Registry::Admission::Admitted);
    QCOMPARE(back.timeRanOutAtMs, std::optional<qint64>(Registry::kGraceMs));
    QVERIFY(!registry.timeRanOutAtMs("a"));
    // A revoke forgets it too.
    registry.remove("b");
    QVERIFY(!registry.timeRanOutAtMs("b"));
}

void TstDeviceSessionRegistry::aTokenWindowFreesItsPlaceAtOnce()
{
    Registry registry;
    Sessions sessions;
    const QByteArray first = registry.nextTokenDeviceId();
    const QByteArray second = registry.nextTokenDeviceId();
    QCOMPARE(first, QByteArray("token:1"));
    QCOMPARE(second, QByteArray("token:2"));
    Registry::Entry token;
    token.deviceId = first;
    token.kind = Registry::Kind::Token;
    token.deviceKind = QStringLiteral("computer");
    const QObject* session = sessions.make();
    QCOMPARE(registry.admit(token, session).admission, Registry::Admission::Admitted);
    registry.sessionEnded(first, session, Registry::EndKind::Dropped);
    QCOMPARE(registry.placesTaken(), 0);
    QVERIFY(!registry.timeRanOutAtMs(first));
}

void TstDeviceSessionRegistry::leavingOnPurposeFreesThePlaceAtOnce()
{
    Registry registry;
    Sessions sessions;
    const QObject* a = sessions.make();
    registry.admit(paired("a"), a);
    registry.sessionEnded("a", a, Registry::EndKind::Left);
    QCOMPARE(registry.placesTaken(), 0);
    QVERIFY(!registry.nextExpiryMs());
    QVERIFY(!registry.timeRanOutAtMs("a"));
}

void TstDeviceSessionRegistry::revokingAnAwayDeviceFreesItsPlaceAtOnce()
{
    Registry registry;
    Sessions sessions;
    const QObject* a = sessions.make();
    registry.admit(paired("a"), a);
    registry.sessionEnded("a", a, Registry::EndKind::Dropped);
    QCOMPARE(registry.placesTaken(), 1);
    registry.remove("a");
    QCOMPARE(registry.placesTaken(), 0);
    QVERIFY(!registry.nextExpiryMs());
}

void TstDeviceSessionRegistry::activityIsReportedAtMostOnceAMinute()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    registry.admit(paired("a"), sessions.make());
    QSignalSpy changed(&registry, &Registry::changed);
    now = 10000;
    registry.noteActivity("a");
    QCOMPARE(registry.entry("a")->lastActivityMs, qint64(10000));
    QCOMPARE(registry.entry("a")->reportedActivityMs, qint64(0));
    QCOMPARE(changed.count(), 0);
    now = Registry::kActivityResolutionMs;
    registry.noteActivity("a");
    QCOMPARE(registry.entry("a")->reportedActivityMs, Registry::kActivityResolutionMs);
    QCOMPARE(changed.count(), 1);
    now += 30000;
    registry.noteActivity("a");
    QCOMPARE(registry.entry("a")->reportedActivityMs, Registry::kActivityResolutionMs);
    QCOMPARE(changed.count(), 1);
    // An unknown device is nothing.
    registry.noteActivity("zz");
    QCOMPARE(changed.count(), 1);
}

void TstDeviceSessionRegistry::changesAreSignalledAndTimeAloneIsNot()
{
    Registry registry;
    qint64 now = 0;
    registry.setClock([&now]() { return now; });
    Sessions sessions;
    QSignalSpy changed(&registry, &Registry::changed);
    QSignalSpy places(&registry, &Registry::placesTakenChanged);
    const QObject* a = sessions.make();
    registry.admit(paired("a"), a);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(places.count(), 1);
    QCOMPARE(places.last().at(0).toInt(), 1);
    now = 100000;
    QVERIFY(registry.expireAway().isEmpty());
    QCOMPARE(changed.count(), 1);
    registry.sessionEnded("a", a, Registry::EndKind::Dropped);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(places.count(), 1);  // away keeps the place
    // A full Core's refusal changes nothing.
    for (const char* id : {"b", "c", "d"}) {
        registry.admit(paired(id), sessions.make());
    }
    const int before = changed.count();
    registry.admit(paired("e"), sessions.make());
    QCOMPARE(changed.count(), before);
}

void TstDeviceSessionRegistry::namesAreNumberedByPairingOrder()
{
    const auto numbered = Registry::numberNames({
        {"first", QStringLiteral("iPhone"), QStringLiteral("iPhone")},
        {"pad", QStringLiteral("iPad"), QStringLiteral("iPad")},
        {"second", QStringLiteral("iPhone"), QStringLiteral("iPhone")},
        {"third", QStringLiteral("iPhone"), QStringLiteral("iPhone")},
    });
    QCOMPARE(numbered.value("first").name, QStringLiteral("iPhone"));
    QCOMPARE(numbered.value("pad").name, QStringLiteral("iPad"));
    QCOMPARE(numbered.value("second").name, QStringLiteral("iPhone 2"));
    QCOMPARE(numbered.value("third").name, QStringLiteral("iPhone 3"));
    // The operator's own words are kept as they are.
    const auto grant = Registry::numberNames({{"g", QStringLiteral("Grant's iPhone"),
                                               QStringLiteral("Grant's")}});
    QCOMPARE(grant.value("g").name, QStringLiteral("Grant's iPhone"));
    QVERIFY(DeviceStore::isValidName(QStringLiteral("Grant's iPhone")));
}

// Slice control plan Task 17: two devices with no name stay nameless, so a
// refusal that names who holds a slice falls back to the kind's word, never
// to a bare number (" 2").
void TstDeviceSessionRegistry::aDeviceWithNoNameIsNeverNumberedIntoOne()
{
    const auto numbered = Registry::numberNames({
        {"a", QString(), QString()},
        {"b", QString(), QString()},
        {"c", QStringLiteral("iPhone"), QStringLiteral("Phone")},
    });
    QCOMPARE(numbered.value("a").name, QString());
    QCOMPARE(numbered.value("b").name, QString());
    QCOMPARE(numbered.value("a").shortName, QString());
    QCOMPARE(numbered.value("b").shortName, QString());
    QCOMPARE(numbered.value("c").name, QStringLiteral("iPhone"));
}

void TstDeviceSessionRegistry::shortNamesAreNumberedOnTheirOwnCollisions()
{
    // Different names, one short name; and two devices that both fall back
    // to "Phone".
    const auto numbered = Registry::numberNames({
        {"a", QStringLiteral("Shack iPhone"), QStringLiteral("Shack")},
        {"b", QStringLiteral("Car iPhone"), QStringLiteral("Shack")},
        {"c", QStringLiteral("Grant's iPhone"),
         Registry::usableShortName(QString(), QStringLiteral("phone"))},
        {"d", QStringLiteral("Hotel iPhone"),
         Registry::usableShortName(QString(), QStringLiteral("phone"))},
    });
    QCOMPARE(numbered.value("a").name, QStringLiteral("Shack iPhone"));
    QCOMPARE(numbered.value("b").name, QStringLiteral("Car iPhone"));
    QCOMPARE(numbered.value("a").shortName, QStringLiteral("Shack"));
    QCOMPARE(numbered.value("b").shortName, QStringLiteral("Shack 2"));
    QCOMPARE(numbered.value("c").shortName, QStringLiteral("Phone"));
    QCOMPARE(numbered.value("d").shortName, QStringLiteral("Phone 2"));
}

void TstDeviceSessionRegistry::aNumberNeverRepeatsAnotherDevicesName()
{
    // A device literally named "iPhone 2", paired after two "iPhone"s: the
    // second "iPhone" takes 3, so no two devices read the same.
    const auto numbered = Registry::numberNames({
        {"a", QStringLiteral("iPhone"), QStringLiteral("A")},
        {"b", QStringLiteral("iPhone"), QStringLiteral("B")},
        {"c", QStringLiteral("iPhone 2"), QStringLiteral("C")},
    });
    QCOMPARE(numbered.value("a").name, QStringLiteral("iPhone"));
    QCOMPARE(numbered.value("b").name, QStringLiteral("iPhone 3"));
    QCOMPARE(numbered.value("c").name, QStringLiteral("iPhone 2"));
}

void TstDeviceSessionRegistry::anUnusableShortNameFallsBackToTheKindsWord()
{
    const QString thirtyThree(33, QLatin1Char('x'));
    QVERIFY(!DeviceStore::isValidShortName(thirtyThree));
    QCOMPARE(Registry::usableShortName(thirtyThree, QStringLiteral("phone")),
             QStringLiteral("Phone"));
    QCOMPARE(Registry::usableShortName(QStringLiteral("Pad\x01"), QStringLiteral("tablet")),
             QStringLiteral("Tablet"));
    QCOMPARE(Registry::usableShortName(QStringLiteral("   "), QStringLiteral("computer")),
             QStringLiteral("Computer"));
    QCOMPARE(Registry::usableShortName(QString(32, QLatin1Char('y')), QStringLiteral("phone")),
             QString(32, QLatin1Char('y')));
    QCOMPARE(Registry::kindWord(QStringLiteral("phone")), QStringLiteral("Phone"));
    QCOMPARE(Registry::kindWord(QStringLiteral("tablet")), QStringLiteral("Tablet"));
    QCOMPARE(Registry::kindWord(QStringLiteral("computer")), QStringLiteral("Computer"));
}

void TstDeviceSessionRegistry::aTokenWindowIsNamedByItsAddress()
{
    QCOMPARE(Registry::tokenWindowName(QStringLiteral("192.168.1.20")),
             QStringLiteral("Computer at 192.168.1.20"));
    QCOMPARE(Registry::tokenWindowName(QStringLiteral("::ffff:192.168.1.20")),
             QStringLiteral("Computer at 192.168.1.20"));
    QCOMPARE(Registry::tokenWindowName(QStringLiteral("fe80::1%en0")),
             QStringLiteral("Computer at fe80::1"));
    QCOMPARE(Registry::tokenWindowName(QString()), QStringLiteral("Computer"));
}

void TstDeviceSessionRegistry::redialsFallInsideTheGracePeriod()
{
    // The Core's heartbeat: a ping every 20 s; at a tick where two pings are
    // still unanswered the session ends (StationServer's defaults). The
    // device's schedule: 1, 2, 5, 10, 30, 60 s, then every 60 s. The link is
    // lost between ticks; the Core's 180 s start when it ends the session.
    constexpr qint64 kTickMs = 20000;
    constexpr int kMissed = 2;
    const QList<qint64> backoffMs{1000, 2000, 5000, 10000, 30000, 60000};

    // When a heartbeat whose pings before `lostMs` were answered ends the
    // session: ticks at kTickMs, kTickMs*2, ...
    const auto heartbeatEnd = [&](qint64 lostMs, qint64 phaseMs) {
        int awaiting = 0;
        for (qint64 tick = phaseMs;; tick += kTickMs) {
            if (awaiting >= kMissed) {
                return tick;
            }
            // A ping sent before the loss is answered at once.
            awaiting = tick < lostMs ? 0 : awaiting + 1;
        }
    };
    const auto attempts = [&](qint64 noticedMs) {
        QList<qint64> out;
        qint64 at = noticedMs;
        for (int i = 0; i < 8; ++i) {
            at += backoffMs.at(std::min<int>(i, backoffMs.size() - 1));
            out.append(at);
        }
        return out;
    };

    struct Row {
        const char* name;
        qint64 coreLostMs;    // when the link is lost, on the Core's clock
        qint64 corePhaseMs;   // the Core's first tick
        qint64 devicePhaseMs; // the device's first tick
        QList<qint64> expectedSeconds;  // attempts, seconds into the Core's 180 s
        int inside;
    };
    const QList<Row> rows{
        // Both ends notice at the same tick.
        {"same tick", 20001, 20000, 20000, {1, 3, 8, 18, 48, 108, 168, 228}, 7},
        // The Core's last ping was lost with the link (it ends at 40 s); the
        // device's was answered just before (it notices at 60 s).
        {"device 20 s later", 20000, 20000, 19999, {21, 23, 28, 38, 68, 128, 188, 248}, 6},
    };

    for (const Row& row : rows) {
        const qint64 coreEnd = heartbeatEnd(row.coreLostMs, row.corePhaseMs);
        const qint64 deviceNoticed = heartbeatEnd(row.coreLostMs, row.devicePhaseMs);
        const QList<qint64> times = attempts(deviceNoticed);

        Registry registry;
        qint64 now = 0;
        registry.setClock([&now]() { return now; });
        Sessions sessions;
        const QObject* live = sessions.make();
        registry.admit(paired("a"), live);
        now = coreEnd;
        registry.sessionEnded("a", live, Registry::EndKind::Dropped);

        QList<qint64> seconds;
        int inside = 0;
        for (const qint64 at : times) {
            seconds.append((at - coreEnd + 500) / 1000);
            // Each attempt, on its own copy of the state: is it still the
            // device's own place, admitted with no question?
            Registry probe;
            qint64 probeNow = coreEnd;
            probe.setClock([&probeNow]() { return probeNow; });
            const QObject* probeLive = sessions.make();
            probeNow = 0;
            probe.admit(paired("a"), probeLive);
            probeNow = coreEnd;
            probe.sessionEnded("a", probeLive, Registry::EndKind::Dropped);
            for (const char* other : {"b", "c", "d"}) {
                probe.admit(paired(other), sessions.make());
            }
            probeNow = at;
            const Registry::Admission admission =
                probe.admit(paired("a"), sessions.make()).admission;
            if (admission == Registry::Admission::SameDevice) {
                ++inside;
                QVERIFY2(at - coreEnd < Registry::kGraceMs, row.name);
            } else {
                // The place was freed; with the three others it is admitted
                // again as a newcomer, never as its own.
                QCOMPARE(admission, Registry::Admission::Admitted);
                QVERIFY2(at - coreEnd >= Registry::kGraceMs, row.name);
            }
        }
        QVERIFY2(seconds == row.expectedSeconds,
                 qPrintable(QStringLiteral("%1: attempts at %2").arg(QLatin1String(row.name))
                                .arg([&seconds]() {
                                    QStringList s;
                                    for (const qint64 v : seconds) {
                                        s.append(QString::number(v));
                                    }
                                    return s.join(QLatin1Char(','));
                                }())));
        QCOMPARE(inside, row.inside);
        // Every attempt up to the sixth is inside the 180 s in both.
        for (int i = 0; i < 6; ++i) {
            QVERIFY(times.at(i) - coreEnd < Registry::kGraceMs);
        }
    }
}

QTEST_GUILESS_MAIN(TstDeviceSessionRegistry)
#include "tst_device_session_registry.moc"
