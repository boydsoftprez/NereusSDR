// Remote Daemon R2, Task 15 -- SettingsProxy / SettingsProxyServer.
//
// Task 13 gave AppSettings a single funnel (value/setValue/contains/
// remove/allKeys) and a change hook that fires from inside it. Task 14
// gave the tree a pure function, classifySettingsKey(), that decides
// whether an AppSettings key belongs to the operator's own machine
// (OperatorLocal) or to the station (Station, must round-trip over the
// wire). This task is where those two land: AppSettings gains a
// non-owning ISettingsBackend* delegation seam (setRemoteBackend(),
// nullptr default, one-branch guard in each of the five funnel methods),
// SettingsProxy is the client-side implementation of that interface (an
// in-memory cache, installed on a remote-mode GUI's AppSettings
// singleton), and SettingsProxyServer is the daemon-side counterpart
// that WRAPS AppSettings from the outside (through its existing public
// API plus the Task 13 hook) to build connect-time snapshots and apply
// inbound writes.
//
// The hazard this whole task exists to avoid, named by Task 13's own
// review before this task was written: an inbound remote write lands via
// AppSettings::setValue(), which fires the Task 13 change hook, which
// ships the SAME change straight back out to every connected client --
// including, redundantly, the one that sent it. AppSettings.h:187-198's
// contract is explicit that the fix is a suppression flag in the
// CONSUMER of the hook (SettingsProxyServer), never inside AppSettings
// itself. serverSuppressesEchoOnInboundApply below is the test that
// proves the fix; the task report records a sabotage-and-revert pass
// against the SAME test (temporarily removing SettingsProxyServer's
// m_applyingInboundWrite guard) to prove it is not vacuous.
//
// This file also pins two invariants the controller notes call out by
// name as things later tasks depend on:
//   - reads are synchronous and never touch the network
//     (valueNeverBlocksOrSpinsEventLoop);
//   - setRemoteBackend(nullptr) leaves today's local path byte-identical
//     (nullBackendLeavesLocalPathByteIdentical).

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/session/SessionMessages.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsProxyServer.h"
#include "core/settings/SettingsScope.h"

using namespace NereusSDR;

namespace {

// A deliberately NARROW fake, matching the R2 Task 15 brief's own Step 1
// wording ("a fake backend handling only hardware/") rather than a full
// classifySettingsKey()-driven implementation: this file's job is to
// prove AppSettings's delegation SEAM works (asks handlesKey(), honours
// the answer, falls through when it says no), not to re-verify Task 14's
// classification rules, which tst_settings_scope.cpp already covers at
// length.
class FakeHardwareOnlyBackend : public ISettingsBackend {
public:
    bool handlesKey(const QString& key) const override
    {
        handlesKeyCalls.append(key);
        return key.startsWith(QStringLiteral("hardware/"));
    }

    QVariant value(const QString& key, const QVariant& defaultValue) const override
    {
        valueCalls.append(key);
        auto it = store.constFind(key);
        if (it != store.constEnd()) {
            return QVariant(it.value());
        }
        return defaultValue;
    }

    void setValue(const QString& key, const QVariant& val) override
    {
        setValueCalls.append(key);
        store.insert(key, val.toString());
    }

    bool contains(const QString& key) const override
    {
        containsCalls.append(key);
        return store.contains(key);
    }

    void remove(const QString& key) override
    {
        removeCalls.append(key);
        store.remove(key);
    }

    QStringList handledKeys() const override
    {
        return store.keys();
    }

    QMap<QString, QString> store;
    mutable QStringList handlesKeyCalls;
    mutable QStringList valueCalls;
    QStringList setValueCalls;
    mutable QStringList containsCalls;
    QStringList removeCalls;
};

QStringList g_capturedLogLines;

void captureMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    g_capturedLogLines.append(msg);
}

// Fix round 1 (review, Minor 9). valueNeverBlocksOrSpinsEventLoop()'s
// timing proof would not catch a value() that called
// QCoreApplication::processEvents() on an otherwise-empty queue -- that
// returns near-instantly, so it would not blow the 200ms budget.
// valueDoesNotProcessQueuedEvents() (below) targets that gap using this
// marker: post a queued-connection invocation, call value(), then check
// it is STILL undelivered.
//
// DISCLOSED LIMITATION, found while building this fix: this file runs
// under QTEST_APPLESS_MAIN, which constructs no QCoreApplication at all.
// Empirically verified (temporarily inserting each into value() and
// reverting) that BOTH QCoreApplication::processEvents() (silently
// no-ops -- the function checks QCoreApplication::instance() and returns
// immediately when it is null) and QEventLoop::processEvents()/exec()
// (also refuses, with a "QEventLoop: Cannot be used without
// QCoreApplication" warning) are unable to deliver ANYTHING in this
// specific test binary's environment -- so this test cannot actually
// distinguish "value() correctly does nothing" from "value() tried to
// pump the event queue and Qt silently declined" here. Kept anyway
// because it exercises the real production code path and would catch a
// regression in the one context that matters (a real GUI process, which
// always has a live QCoreApplication/QApplication) -- restructuring this
// whole file to QTEST_MAIN to close this gap for good would touch every
// one of the other 50+ tests in it and was judged out of scope for a
// Minor.
class QueuedEventMarker : public QObject {
    Q_OBJECT
public:
    bool delivered = false;
public slots:
    void mark() { delivered = true; }
};

} // namespace

class TstSettingsProxy : public QObject {
    Q_OBJECT

private slots:

    // ── Step 1: the routing test (AppSettings <-> ISettingsBackend seam) ──

    void routingReachesBackendForStationKeyOnly()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        FakeHardwareOnlyBackend fake;
        s.setRemoteBackend(&fake);

        // A hardware/ key: setValue/value/contains all reach the fake.
        s.setValue(QStringLiteral("hardware/aa:bb/x/y"), QStringLiteral("42"));
        QCOMPARE(fake.setValueCalls, QStringList{QStringLiteral("hardware/aa:bb/x/y")});
        QCOMPARE(fake.store.value(QStringLiteral("hardware/aa:bb/x/y")), QStringLiteral("42"));
        QVERIFY(s.contains(QStringLiteral("hardware/aa:bb/x/y")));
        QCOMPARE(s.value(QStringLiteral("hardware/aa:bb/x/y")).toString(), QStringLiteral("42"));
        QVERIFY(fake.valueCalls.contains(QStringLiteral("hardware/aa:bb/x/y")));

        // hardwareValue()/setHardwareValue() reach it too -- Task 13
        // already routes both through value()/setValue(), so this is
        // "for free" once the delegation branch exists in those two.
        s.setHardwareValue(QStringLiteral("aa:bb"), QStringLiteral("z"), QStringLiteral("99"));
        QCOMPARE(fake.store.value(QStringLiteral("hardware/aa:bb/z")), QStringLiteral("99"));
        QCOMPARE(s.hardwareValue(QStringLiteral("aa:bb"), QStringLiteral("z")).toString(),
                 QStringLiteral("99"));

        // An OperatorLocal-shaped key (no "hardware/" prefix) never
        // reaches the fake's storage or read/write methods at all -- it
        // stays entirely on AppSettings's own local m_settings map.
        fake.setValueCalls.clear();
        fake.valueCalls.clear();
        fake.containsCalls.clear();
        s.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#112233"));
        QVERIFY(fake.setValueCalls.isEmpty());
        QVERIFY(!fake.store.contains(QStringLiteral("DisplayNoiseFloorColor")));
        QCOMPARE(s.value(QStringLiteral("DisplayNoiseFloorColor")).toString(),
                 QStringLiteral("#112233"));
        QVERIFY(fake.valueCalls.isEmpty());
        QVERIFY(s.contains(QStringLiteral("DisplayNoiseFloorColor")));
        QVERIFY(fake.containsCalls.isEmpty());
    }

    void removeRoutesThroughBackendForClaimedKeyOnly()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        FakeHardwareOnlyBackend fake;
        s.setRemoteBackend(&fake);

        s.setValue(QStringLiteral("hardware/aa:bb/x"), QStringLiteral("1"));
        s.remove(QStringLiteral("hardware/aa:bb/x"));
        QCOMPARE(fake.removeCalls, QStringList{QStringLiteral("hardware/aa:bb/x")});
        QVERIFY(!fake.store.contains(QStringLiteral("hardware/aa:bb/x")));

        s.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#000"));
        s.remove(QStringLiteral("DisplayNoiseFloorColor"));
        QVERIFY(fake.removeCalls.size() == 1); // unchanged -- the local remove never reached the fake
        QVERIFY(!s.contains(QStringLiteral("DisplayNoiseFloorColor")));
    }

    void allKeysMergesLocalAndBackendKeys()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("x"));

        FakeHardwareOnlyBackend fake;
        fake.store.insert(QStringLiteral("hardware/aa:bb/y"), QStringLiteral("z"));
        s.setRemoteBackend(&fake);

        const QStringList keys = s.allKeys();
        QCOMPARE(keys.size(), 2);
        QVERIFY(keys.contains(QStringLiteral("DisplayNoiseFloorColor")));
        QVERIFY(keys.contains(QStringLiteral("hardware/aa:bb/y")));
    }

    // ── setRemoteBackend(nullptr) leaves today's path byte-identical ──────

    void nullBackendLeavesLocalPathByteIdentical()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(s.remoteBackend() == nullptr); // default, before this task's seam is ever touched

        s.setValue(QStringLiteral("hardware/aa:bb/x"), QStringLiteral("1"));
        s.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#000000"));
        QCOMPARE(s.value(QStringLiteral("hardware/aa:bb/x")).toString(), QStringLiteral("1"));
        QCOMPARE(s.allKeys().size(), 2);
        QVERIFY(s.contains(QStringLiteral("hardware/aa:bb/x")));
        s.remove(QStringLiteral("hardware/aa:bb/x"));
        QVERIFY(!s.contains(QStringLiteral("hardware/aa:bb/x")));
        QCOMPARE(s.allKeys().size(), 1);

        // Installing, then immediately uninstalling, a backend must leave
        // AppSettings in EXACTLY the state it would be in had
        // setRemoteBackend() never been called at all.
        FakeHardwareOnlyBackend fake;
        s.setRemoteBackend(&fake);
        s.setRemoteBackend(nullptr);
        QVERIFY(s.remoteBackend() == nullptr);

        s.setValue(QStringLiteral("hardware/aa:bb/x"), QStringLiteral("2"));
        QCOMPARE(s.value(QStringLiteral("hardware/aa:bb/x")).toString(), QStringLiteral("2"));
        QVERIFY2(fake.store.isEmpty(), "the fake must never have been consulted once uninstalled");
        QCOMPARE(s.allKeys().size(), 2);
    }

    // ── Fix round 1 (review, Important 1): allKeys()/contains()/value() ───
    // ── must agree, and clearHardwareValues() must actually delete a ──────
    // ── stale local entry, not merely hide it ──────────────────────────────

    void preExistingLocalHardwareKeyIsHiddenAndClearHardwareValuesActuallyDeletesIt()
    {
        // Reproduces the review's exact scenario: an operator who has used
        // this radio LOCALLY before (so hardware/<mac>/* already sits in
        // m_settings) then goes remote. Before this fix, allKeys() kept
        // listing that entry forever (a pure union that only ever added)
        // while contains()/value() already delegated it away and reported
        // it absent -- a broken invariant hardwareValues() and
        // clearHardwareValues() both depended on. This test fails against
        // the pre-fix allKeys() (lists a key contains() denies) AND
        // against the pre-fix remove() (clearHardwareValues() leaves the
        // stale entry on disk, invisible but not deleted).
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:ff");
        const QString fullKey = QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(mac);

        // "Operator has used this radio locally before": written with NO
        // backend installed yet, straight into m_settings.
        s.setHardwareValue(mac, QStringLiteral("radioInfo/sampleRate"), 192000);
        QVERIFY(s.allKeys().contains(fullKey));

        // Now go remote: install a backend that claims the same key
        // family but starts with nothing of its own cached.
        SettingsProxy proxy;
        s.setRemoteBackend(&proxy);

        QVERIFY2(!s.allKeys().contains(fullKey),
                 "a local entry the backend now claims must not appear in allKeys()");
        QVERIFY2(!s.contains(fullKey), "contains() already correctly delegated and denied this");
        QVERIFY2(!s.value(fullKey).isValid(),
                 "value() with no explicit default must come back invalid/absent, not the "
                 "stale local number");

        // "Forget radio" must ACTUALLY delete the stale local entry, not
        // just leave it hidden while a backend happens to be installed.
        s.clearHardwareValues(mac);

        // Uninstall the backend and confirm it is REALLY gone -- if
        // remove() had only delegated (the pre-fix behavior), the stale
        // entry would reappear here.
        s.setRemoteBackend(nullptr);
        QVERIFY2(!s.allKeys().contains(fullKey),
                 "clearHardwareValues() must actually delete the stale local m_settings "
                 "entry, not merely leave it hidden while a backend happens to be installed");
        QVERIFY(!s.contains(fullKey));
    }

    void singleKeyRemoveAlsoDeletesStaleLocalEntryDirectly()
    {
        // Narrower companion to the test above: remove() itself (not
        // just clearHardwareValues(), which calls it in a loop) must
        // clean up a stale local leftover for a backend-claimed key.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));

        // The real backend, not the "hardware/"-only fake: handlesKey()
        // uses classifySettingsKey(), and "Slice0/Locked" classifies
        // Station, so this exercises a non-hardware/ Station family too.
        SettingsProxy proxy;
        s.setRemoteBackend(&proxy);
        QVERIFY(!s.contains(QStringLiteral("Slice0/Locked"))); // already hidden (Important 1)

        s.remove(QStringLiteral("Slice0/Locked"));

        s.setRemoteBackend(nullptr);
        QVERIFY2(!s.contains(QStringLiteral("Slice0/Locked")),
                 "remove() must delete the stale local entry even for a single direct call, "
                 "not only when reached via clearHardwareValues()'s loop");
    }

    // ── ISettingsBackend.h's synchronous-read invariant ────────────────────

    void valueNeverBlocksOrSpinsEventLoop()
    {
        SettingsProxy proxy;
        QMap<QString, QString> data;
        for (int i = 0; i < 2000; ++i) {
            data.insert(QStringLiteral("hardware/aa:bb/k%1").arg(i), QStringLiteral("v%1").arg(i));
        }
        proxy.applySnapshot(data);

        // logProxiedRead() (Step 9) is unconditional and this loop makes
        // 4000 calls -- silenced for the DURATION of this timing loop
        // only (restored immediately after) so it does not trip QTest's
        // default max-message count (2000) and swallow output from every
        // later test slot in this same process, and so the measured
        // time reflects the cache lookup itself rather than 4000 log
        // lines' worth of string formatting.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.settingsproxy.debug=false"));

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 2000; ++i) {
            const QVariant v = proxy.value(QStringLiteral("hardware/aa:bb/k%1").arg(i), QVariant());
            QCOMPARE(v.toString(), QStringLiteral("v%1").arg(i));
        }
        for (int i = 0; i < 2000; ++i) {
            const QVariant v = proxy.value(QStringLiteral("hardware/aa:bb/missing%1").arg(i),
                                           QStringLiteral("default"));
            QCOMPARE(v.toString(), QStringLiteral("default"));
        }
        const qint64 elapsedMs = timer.elapsed();
        QLoggingCategory::setFilterRules(QString());
        // This cannot PROVE the absence of a blocking call (see
        // ISettingsBackend.h's invariant paragraph for why that is
        // fundamentally a code-review, not a unit-test, property) -- but
        // 4000 synchronous in-memory QMap lookups complete in low single-
        // digit milliseconds; a single hidden socket wait or sleep would
        // blow this budget by two to three orders of magnitude. 200ms
        // leaves a wide, non-flaky margin while still catching that.
        QVERIFY2(elapsedMs < 200,
                 qPrintable(QStringLiteral("4000 SettingsProxy::value() calls took %1 ms -- "
                                           "reads must be synchronous/cache-only")
                                .arg(elapsedMs)));
    }

    void valueDoesNotProcessQueuedEvents()
    {
        // Fix round 1 (review, Minor 9). See QueuedEventMarker's own
        // comment for why the timing proof above cannot catch a
        // processEvents() call on an empty queue, and for this test's
        // own disclosed limitation in the QTEST_APPLESS_MAIN environment.
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k"), QStringLiteral("v")}});

        QueuedEventMarker marker;
        QMetaObject::invokeMethod(&marker, "mark", Qt::QueuedConnection);
        QVERIFY2(!marker.delivered,
                 "sanity: nothing has run this thread's event loop yet");

        const QVariant v = proxy.value(QStringLiteral("hardware/aa:bb/k"), QVariant());
        Q_UNUSED(v);

        QVERIFY2(!marker.delivered,
                 "value() must not process queued events -- a call that did would "
                 "silently deliver this marker without an event loop ever running");
    }

    // ── Step 3: AppSettings::snapshot() -- fully-qualified extraction ─────

    void snapshotReturnsFullyQualifiedKeysUnlikeHardwareValues()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setHardwareValue(QStringLiteral("aa:bb"), QStringLiteral("radioInfo/sampleRate"), 192000);

        const QMap<QString, QString> snap = s.snapshot({QStringLiteral("hardware/aa:bb/")});
        QCOMPARE(snap.size(), 1);
        QVERIFY2(snap.contains(QStringLiteral("hardware/aa:bb/radioInfo/sampleRate")),
                 "snapshot() must return the FULLY QUALIFIED key, unlike hardwareValues()");
        QCOMPARE(snap.value(QStringLiteral("hardware/aa:bb/radioInfo/sampleRate")),
                 QStringLiteral("192000"));

        // hardwareValues() strips the prefix -- the two methods are
        // deliberately different shapes; confirm both on the same data.
        const QMap<QString, QVariant> stripped = s.hardwareValues(QStringLiteral("aa:bb"));
        QVERIFY(stripped.contains(QStringLiteral("radioInfo/sampleRate")));
        QVERIFY(!stripped.contains(QStringLiteral("hardware/aa:bb/radioInfo/sampleRate")));
    }

    void snapshotScopesToGivenPrefixesOnly()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setHardwareValue(QStringLiteral("aa:bb"), QStringLiteral("k"), QStringLiteral("1"));
        s.setHardwareValue(QStringLiteral("cc:dd"), QStringLiteral("k"), QStringLiteral("2"));
        s.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));
        s.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("x"));

        const QMap<QString, QString> snap =
            s.snapshot({QStringLiteral("hardware/aa:bb/"), QStringLiteral("Slice")});
        QCOMPARE(snap.size(), 2);
        QVERIFY(snap.contains(QStringLiteral("hardware/aa:bb/k")));
        QVERIFY(snap.contains(QStringLiteral("Slice0/Locked")));
        QVERIFY(!snap.contains(QStringLiteral("hardware/cc:dd/k")));
        QVERIFY(!snap.contains(QStringLiteral("DisplayNoiseFloorColor")));
    }

    void snapshotWithNoMatchingPrefixesReturnsEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        s.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));

        const QMap<QString, QString> snap = s.snapshot({QStringLiteral("Tci")});
        QVERIFY(snap.isEmpty());
    }

    // ── Step 3: SettingsProxy::applySnapshot() merges, never replaces ─────

    void proxyApplySnapshotMergesNotReplaces()
    {
        SettingsProxy proxy;
        QMap<QString, QString> first;
        first.insert(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("v1"));
        proxy.applySnapshot(first);
        QCOMPARE(proxy.cacheSize(), 1);

        QMap<QString, QString> second;
        second.insert(QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("v2"));
        proxy.applySnapshot(second);

        QCOMPARE(proxy.cacheSize(), 2);
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k1"), QVariant()).toString(),
                 QStringLiteral("v1"));
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k2"), QVariant()).toString(),
                 QStringLiteral("v2"));
    }

    void proxyApplySnapshotOverwritesUpdatedKeysWithinScope()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("old")}});
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("new")}});

        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k1"), QVariant()).toString(),
                 QStringLiteral("new"));
        QCOMPARE(proxy.cacheSize(), 1);
    }

    // ── Step 4: absent keys stay absent, before and after a snapshot ──────

    void unwrittenStationKeyReturnsCallerDefaultBeforeAnySnapshot()
    {
        SettingsProxy proxy;
        QVERIFY(!proxy.hasReceivedSnapshot());
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/never"), QStringLiteral("fallback")).toString(),
                 QStringLiteral("fallback"));
        QVERIFY2(proxy.provenUnsetKeys().isEmpty(),
                 "nothing is PROVEN unset before any snapshot has ever been applied");
    }

    void unwrittenStationKeyReturnsCallerDefaultAfterSnapshot()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("v1")}});

        // The controller notes' own worked example: a Setup page reading
        // Region and expecting "United States" back when the key was
        // never set. Reproduced here against the STATION path instead of
        // AppSettings's local path (Task 13 already pins the local one).
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/never"), QStringLiteral("fallback")).toString(),
                 QStringLiteral("fallback"));
        QVERIFY(proxy.provenUnsetKeys().contains(QStringLiteral("hardware/aa:bb/never")));
        // Still doesn't show up as a real cached key.
        QVERIFY(!proxy.handledKeys().contains(QStringLiteral("hardware/aa:bb/never")));
        QVERIFY(!proxy.contains(QStringLiteral("hardware/aa:bb/never")));
    }

    void handledKeysReturnsOnlyRealCachedValuesNotProvenUnsetOnes()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("v1")}});
        proxy.value(QStringLiteral("hardware/aa:bb/never"), QVariant());

        const QStringList keys = proxy.handledKeys();
        QCOMPARE(keys.size(), 1);
        QVERIFY(keys.contains(QStringLiteral("hardware/aa:bb/k1")));
        QVERIFY(!keys.contains(QStringLiteral("hardware/aa:bb/never")));
    }

    void removeBeforeAnySnapshotDoesNotMarkProvenUnset()
    {
        // Fix round 1 (review, Minor 8). remove() used to mark
        // PROVEN-unset unconditionally, even before this client had ever
        // heard from the daemon at all -- violating the class comment's
        // own "three-state read" definition (PROVEN UNSET requires at
        // least one snapshot to have landed) and making Step 9's log
        // ambiguous about what "ProvenUnset" actually means.
        SettingsProxy proxy;
        QVERIFY(!proxy.hasReceivedSnapshot());

        proxy.remove(QStringLiteral("hardware/aa:bb/never-snapshotted"));

        QVERIFY2(proxy.provenUnsetKeys().isEmpty(),
                 "remove() before any snapshot must not claim PROVEN unset -- it is the "
                 "NO-SNAPSHOT-YET state, exactly like an equivalent value() call");
    }

    void removeAfterSnapshotDoesMarkProvenUnset()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("v1")}});

        proxy.remove(QStringLiteral("hardware/aa:bb/k1"));

        QVERIFY(proxy.provenUnsetKeys().contains(QStringLiteral("hardware/aa:bb/k1")));
    }

    // ── Step 9: the proxied-read log ───────────────────────────────────────

    void proxiedReadLogHasScannableShape()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("v1")}});

        g_capturedLogLines.clear();
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.settingsproxy.debug=true"));
        QtMessageHandler previous = qInstallMessageHandler(captureMessageHandler);

        const QVariant hit = proxy.value(QStringLiteral("hardware/aa:bb/k1"), QVariant());
        Q_UNUSED(hit);
        const QVariant beforeSnapshot = SettingsProxy().value(QStringLiteral("hardware/aa:bb/x"),
                                                               QStringLiteral("def"));
        Q_UNUSED(beforeSnapshot);
        const QVariant provenUnset = proxy.value(QStringLiteral("hardware/aa:bb/missing"),
                                                  QStringLiteral("def"));
        Q_UNUSED(provenUnset);

        qInstallMessageHandler(previous);
        QLoggingCategory::setFilterRules(QString());

        bool sawHit = false;
        bool sawProvenUnset = false;
        bool sawNoSnapshotYet = false;
        for (const QString& line : std::as_const(g_capturedLogLines)) {
            if (!line.contains(QStringLiteral("proxied-read"))) {
                continue;
            }
            if (line.contains(QStringLiteral("hardware/aa:bb/k1")) && line.contains(QStringLiteral("CacheHit"))) {
                sawHit = true;
            }
            if (line.contains(QStringLiteral("hardware/aa:bb/missing")) &&
                line.contains(QStringLiteral("ProvenUnset"))) {
                sawProvenUnset = true;
            }
            if (line.contains(QStringLiteral("hardware/aa:bb/x")) &&
                line.contains(QStringLiteral("NoSnapshotYet"))) {
                sawNoSnapshotYet = true;
            }
        }
        QVERIFY2(sawHit, "expected a 'proxied-read ... CacheHit' log line for a real cached value");
        QVERIFY2(sawProvenUnset, "expected a 'proxied-read ... ProvenUnset' log line");
        QVERIFY2(sawNoSnapshotYet, "expected a 'proxied-read ... NoSnapshotYet' log line");
    }

    // ── Step 6: optimistic writes, origin tags, rejection ──────────────────

    void optimisticWriteUpdatesCacheAndEmitsOutboundWhileReady()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        QSignalSpy spy(&proxy, &SettingsProxy::outboundWriteRequested);

        proxy.setValue(QStringLiteral("hardware/aa:bb/AgcThreshold"), 10);

        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/AgcThreshold"), QVariant()).toInt(), 10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("hardware/aa:bb/AgcThreshold"));
        QCOMPARE(spy.at(0).at(1).toInt(), 10);
    }

    void writeDroppedNotQueuedWhileNotReady()
    {
        SettingsProxy proxy;
        QVERIFY(!proxy.ready());
        QSignalSpy spy(&proxy, &SettingsProxy::outboundWriteRequested);

        proxy.setValue(QStringLiteral("hardware/aa:bb/AgcThreshold"), 10);

        QCOMPARE(spy.count(), 0); // dropped, not queued -- no signal at all
        // The cache still reflects the optimistic write (UI stays
        // interactive/consistent during an outage -- see class comment).
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/AgcThreshold"), QVariant()).toInt(), 10);

        // Becoming ready afterward does not retroactively flush anything
        // -- there is no queue.
        proxy.setReady(true);
        QCOMPARE(spy.count(), 0);
    }

    void removeDroppedNotQueuedWhileNotReady()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k"), QStringLiteral("v")}});
        QSignalSpy spy(&proxy, &SettingsProxy::outboundRemoveRequested);

        proxy.remove(QStringLiteral("hardware/aa:bb/k"));

        QCOMPARE(spy.count(), 0);
        QVERIFY(!proxy.contains(QStringLiteral("hardware/aa:bb/k")));
    }

    // ── Fix round 1 (review, Important 2): offline-drop tracking ───────────

    void offlineWritesAndRemovesAreTrackedAsDropped()
    {
        SettingsProxy proxy;
        QVERIFY(!proxy.ready());
        QVERIFY(proxy.droppedWhileOffline().isEmpty());

        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        proxy.remove(QStringLiteral("hardware/aa:bb/k2"));

        const QSet<QString> dropped = proxy.droppedWhileOffline();
        QCOMPARE(dropped.size(), 2);
        QVERIFY(dropped.contains(QStringLiteral("hardware/aa:bb/k1")));
        QVERIFY(dropped.contains(QStringLiteral("hardware/aa:bb/k2")));
    }

    void onlineWritesAndRemovesAreNotTrackedAsDropped()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        proxy.remove(QStringLiteral("hardware/aa:bb/k1"));

        QVERIFY(proxy.droppedWhileOffline().isEmpty());
    }

    void reconnectSnapshotContradictsOverlappingOfflineEditsAndClearsWholeSet()
    {
        // Fix round 1 (review, Important 2). Before this fix, Step 8's
        // "dropped, not queued" destroyed the information Task 19 needs
        // at the moment of the drop -- nothing recorded WHICH keys had a
        // pending edit while offline, so a reconnect could not
        // distinguish "your offline edit was overwritten" from "the
        // daemon changed this key while you were away".
        SettingsProxy proxy;
        QVERIFY(!proxy.ready());
        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        proxy.setValue(QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("mine-too"));
        QCOMPARE(proxy.droppedWhileOffline().size(), 2);
        QVERIFY(proxy.keysContradictedByLastSnapshot().isEmpty());

        // Reconnect: the fresh snapshot reports its OWN value for k1 (a
        // genuine contradiction -- the daemon never saw this client's
        // offline edit) and says nothing about k2 at all.
        QMap<QString, QString> snap;
        snap.insert(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("daemon-truth"));
        proxy.setReady(true);
        proxy.applySnapshot(snap);

        QCOMPARE(proxy.keysContradictedByLastSnapshot(),
                 QSet<QString>{QStringLiteral("hardware/aa:bb/k1")});
        QVERIFY2(proxy.droppedWhileOffline().isEmpty(),
                 "a fresh full snapshot clears the WHOLE pending set, not just the "
                 "contradicted subset -- see the class comment for why");
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k1"), QVariant()).toString(),
                 QStringLiteral("daemon-truth"));
    }

    // ── Whole-branch review, Important 2: the bookkeeping gets a consumer ──
    //
    // keysContradictedByLastSnapshot() was written, tested, and read by
    // nothing in production: the operator was told the LINK dropped
    // (MainWindow's "Link to the Core lost" toast) and never that a specific
    // EDIT of theirs did not stick. value() keeps returning the offline
    // value, so the control reads back as applied, which is the worst
    // shape this can take. The signal below is what a GUI hangs a notice
    // on; these two slots pin that it fires when and only when there is
    // something to say.
    void contradictedOfflineEditsAreAnnouncedOnceWithTheirKeys()
    {
        SettingsProxy proxy;
        QSignalSpy announced(&proxy, &SettingsProxy::offlineEditsSuperseded);

        QVERIFY(!proxy.ready());
        proxy.setValue(QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("mine-too"));
        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        proxy.setValue(QStringLiteral("hardware/aa:bb/k3"), QStringLiteral("uncovered"));
        QCOMPARE(announced.count(), 0);

        QMap<QString, QString> snap;
        snap.insert(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("daemon-truth"));
        snap.insert(QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("daemon-truth-2"));
        proxy.setReady(true);
        proxy.applySnapshot(snap);

        QCOMPARE(announced.count(), 1);
        // SORTED, and carrying only the contradicted subset: k3 is an
        // offline edit the snapshot had no opinion about, which
        // SettingsProxy.h documents as deliberately unreported.
        const QStringList keys = announced.first().first().toStringList();
        QCOMPARE(keys, (QStringList{QStringLiteral("hardware/aa:bb/k1"),
                                    QStringLiteral("hardware/aa:bb/k2")}));
    }

    void aSnapshotThatContradictsNothingAnnouncesNothing()
    {
        SettingsProxy proxy;
        QSignalSpy announced(&proxy, &SettingsProxy::offlineEditsSuperseded);

        // An ONLINE write is not a dropped one, so a later snapshot
        // covering it is an ordinary update, not a superseded edit.
        proxy.setReady(true);
        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("daemon-truth")}});
        QCOMPARE(announced.count(), 0);

        // ...and neither is an offline edit whose key the snapshot never
        // mentions. Nothing was overwritten, so there is nothing to say.
        proxy.setReady(false);
        proxy.setValue(QStringLiteral("hardware/aa:bb/k9"), QStringLiteral("mine"));
        proxy.setReady(true);
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("daemon-truth-2")}});
        QCOMPARE(announced.count(), 0);
    }

    // ── Whole-branch review, Minor 5 ──────────────────────────────────────
    //
    // hasNonEmptySnapshot()'s doc says "once at least one snapshot has
    // been applied AND it carried at least one key besides the seed
    // marker". The body only ever inspected m_cache, so this client's own
    // optimistic offline writes satisfied it too, and the "a snapshot has
    // landed" half of the sentence was decoration. Inert today (the
    // setupDialogAllowed() gate also requires ready(), which only the
    // handshake sets, and the handshake applies a snapshot first), which
    // is exactly why it needs a test rather than a reader noticing.
    void hasNonEmptySnapshotStaysFalseUntilASnapshotHasActuallyLanded()
    {
        SettingsProxy proxy;
        QVERIFY(!proxy.hasNonEmptySnapshot());

        // The client's OWN write, before it has ever heard from a station.
        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        QVERIFY(proxy.contains(QStringLiteral("hardware/aa:bb/k1")));
        QVERIFY2(!proxy.hasNonEmptySnapshot(),
                 "an optimistic local write is not a snapshot from the station");

        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("daemon-truth")}});
        QVERIFY(proxy.hasNonEmptySnapshot());
    }

    // The same contract AFTER a snapshot has landed. An empty snapshot, or
    // one carrying only the seed marker, followed by this window's own
    // write must still read as "the station sent nothing real": the write
    // came from here, not from the Core.
    void hasNonEmptySnapshotIgnoresThisWindowsOwnWritesAfterASnapshot()
    {
        SettingsProxy empty;
        empty.setReady(true);
        empty.applySnapshot(QMap<QString, QString>{});
        empty.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        QVERIFY2(!empty.hasNonEmptySnapshot(),
                 "this window's own write is not station content");
        QVERIFY2(!empty.setupDialogAllowed(),
                 "an own write must not open Setup over an empty station snapshot");

        SettingsProxy seeded;
        QMap<QString, QString> marker;
        marker.insert(QLatin1String(AppSettings::kDaemonProfileSeededKey), QStringLiteral("True"));
        seeded.applySnapshot(marker);
        seeded.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        QVERIFY2(!seeded.hasNonEmptySnapshot(),
                 "the seed marker plus an own write is still not station content");

        // A value the Core pushes later does count.
        seeded.applyRemoteValue(QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("core"),
                                QStringLiteral("other"));
        QVERIFY(seeded.hasNonEmptySnapshot());

        // A Core-asserted removal takes it back out.
        seeded.applyRemoteRemoval(QStringLiteral("hardware/aa:bb/k2"));
        QVERIFY(!seeded.hasNonEmptySnapshot());
    }

    // ── Whole-branch review, Minor 6 ──────────────────────────────────────
    //
    // ISettingsBackend's contract is handledKeys() subset-of handlesKey():
    // every key this backend reports holding is one it claims. Every
    // writer into m_cache honoured that by construction except
    // applySnapshot(), which merged whatever the daemon sent. At one
    // version the two classifiers agree and nothing can go wrong; across a
    // version skew the daemon can send a key this build classifies
    // OperatorLocal, and the invariant became conventional rather than
    // structural. Filtering costs nothing and changes no read: a key this
    // client does not claim was never routed here by AppSettings anyway.
    void applySnapshotOnlyCachesKeysThisClientClaims()
    {
        SettingsProxy proxy;
        QMap<QString, QString> snap;
        snap.insert(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("station"));
        // Classifies OperatorLocal: a hypothetical newer daemon deciding
        // trace colour belongs to the station.
        snap.insert(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#112233"));
        proxy.applySnapshot(snap);

        QVERIFY(proxy.contains(QStringLiteral("hardware/aa:bb/k1")));
        QVERIFY2(!proxy.contains(QStringLiteral("DisplayNoiseFloorColor")),
                 "a snapshot key this client does not claim was cached anyway, "
                 "breaking handledKeys() subset-of handlesKey()");

        // The invariant itself, asserted over the whole cache rather than
        // over the one key this slot happened to plant.
        for (const QString& key : proxy.handledKeys()) {
            QVERIFY2(proxy.handlesKey(key), qPrintable(key));
        }

        // The seed marker survives, because handlesKey() claims it
        // explicitly even though it classifies OperatorLocal. Without that
        // carve-out this filter would silently disarm the Setup gate on a
        // freshly reserved daemon profile.
        SettingsProxy seeded;
        seeded.applySnapshot(QMap<QString, QString>{
            {QString::fromLatin1(AppSettings::kDaemonProfileSeededKey), QStringLiteral("1")}});
        QVERIFY(seeded.contains(QString::fromLatin1(AppSettings::kDaemonProfileSeededKey)));
        seeded.setReady(true);
        QVERIFY(seeded.setupDialogAllowed());
    }

    void keysContradictedByLastSnapshotIsRecomputedNotAccumulated()
    {
        SettingsProxy proxy;
        proxy.setValue(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("mine"));
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("daemon-1")}});
        QCOMPARE(proxy.keysContradictedByLastSnapshot().size(), 1);

        // A SECOND snapshot, with nothing newly dropped in between, must
        // report an EMPTY contradiction set -- not the first snapshot's
        // stale result carried forward.
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("daemon-2")}});
        QVERIFY(proxy.keysContradictedByLastSnapshot().isEmpty());
    }

    void removeRoutesToBackendAndEmitsOutboundWhileReady()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxy proxy;
        s.setRemoteBackend(&proxy);
        proxy.setReady(true);
        proxy.applySnapshot(QMap<QString, QString>{
            {QStringLiteral("hardware/aa:bb/k"), QStringLiteral("v")}});
        QVERIFY(s.contains(QStringLiteral("hardware/aa:bb/k")));

        QSignalSpy spy(&proxy, &SettingsProxy::outboundRemoveRequested);
        s.remove(QStringLiteral("hardware/aa:bb/k"));

        QVERIFY(!s.contains(QStringLiteral("hardware/aa:bb/k")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("hardware/aa:bb/k"));
    }

    void applyRemoteValueUpdatesCacheRegardlessOfOriginTag()
    {
        SettingsProxy proxy;
        proxy.setLocalOriginTag(QStringLiteral("my-session"));

        // A genuine third-party/daemon-local change: empty origin tag.
        proxy.applyRemoteValue(QStringLiteral("hardware/aa:bb/k"), QStringLiteral("fromDaemon"), QString());
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k"), QVariant()).toString(),
                 QStringLiteral("fromDaemon"));

        // The echo of this client's OWN write: tag matches localOriginTag().
        proxy.applyRemoteValue(QStringLiteral("hardware/aa:bb/k"), QStringLiteral("myOwnEcho"),
                               QStringLiteral("my-session"));
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k"), QVariant()).toString(),
                 QStringLiteral("myOwnEcho"));
    }

    void rejectionRevertsCacheToRestoredValueAndEmitsSignal()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.setValue(QStringLiteral("hardware/aa:bb/k"), QStringLiteral("optimistic"));
        QSignalSpy spy(&proxy, &SettingsProxy::valueRejected);

        proxy.applyRejection(QStringLiteral("hardware/aa:bb/k"), QStringLiteral("daemon-truth"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("hardware/aa:bb/k"));
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("daemon-truth"));
        QCOMPARE(proxy.value(QStringLiteral("hardware/aa:bb/k"), QVariant()).toString(),
                 QStringLiteral("daemon-truth"));
    }

    void rejectionToProvenUnsetWhenDaemonHasNothingEither()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.setValue(QStringLiteral("hardware/aa:bb/k"), QStringLiteral("optimistic"));
        QVERIFY(proxy.contains(QStringLiteral("hardware/aa:bb/k")));

        proxy.applyRejection(QStringLiteral("hardware/aa:bb/k"), QVariant()); // invalid = daemon has nothing

        QVERIFY(!proxy.contains(QStringLiteral("hardware/aa:bb/k")));
        QVERIFY(proxy.provenUnsetKeys().contains(QStringLiteral("hardware/aa:bb/k")));
    }

    void snapshotAppliedSignalCarriesThisCallsKeyCount()
    {
        SettingsProxy proxy;
        QSignalSpy spy(&proxy, &SettingsProxy::snapshotApplied);

        QMap<QString, QString> data;
        data.insert(QStringLiteral("hardware/aa:bb/k1"), QStringLiteral("v1"));
        data.insert(QStringLiteral("hardware/aa:bb/k2"), QStringLiteral("v2"));
        proxy.applySnapshot(data);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }

    // ── Step 7: the Setup-dialog gate ───────────────────────────────────────

    void setupGateDeniesWithoutReadyEvenWithNonEmptySnapshot()
    {
        SettingsProxy proxy;
        proxy.applySnapshot(QMap<QString, QString>{{QStringLiteral("Slice0/Locked"), QStringLiteral("True")}});
        QVERIFY(!proxy.ready());
        QVERIFY(!proxy.setupDialogAllowed());
    }

    void setupGateAllowsOnReadyWithNonEmptySnapshot()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.applySnapshot(QMap<QString, QString>{{QStringLiteral("Slice0/Locked"), QStringLiteral("True")}});
        QVERIFY(proxy.setupDialogAllowed());
    }

    void setupGateDeniesOnReadyWithNoSnapshotAtAll()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        QVERIFY(!proxy.hasReceivedSnapshot());
        QVERIFY(!proxy.setupDialogAllowed());
    }

    void setupGateDeniesOnReadyWithEmptySnapshotAndNoSeedMarker()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.applySnapshot(QMap<QString, QString>{});
        QVERIFY(proxy.hasReceivedSnapshot());
        QVERIFY(!proxy.hasNonEmptySnapshot());
        QVERIFY2(!proxy.setupDialogAllowed(),
                 "ready() alone must not be sufficient -- an empty snapshot with no seed marker "
                 "must not open Setup (187 widget constructors would bake ship defaults in)");
    }

    void setupGateAllowsOnReadyWithSeedMarkerAloneEvenIfSnapshotOtherwiseEmpty()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        QMap<QString, QString> data;
        data.insert(QLatin1String(AppSettings::kDaemonProfileSeededKey), QStringLiteral("True"));
        proxy.applySnapshot(data);

        QVERIFY(proxy.hasReceivedSnapshot());
        QVERIFY2(!proxy.hasNonEmptySnapshot(),
                 "the seed marker alone must not count as real station content");
        QVERIFY2(proxy.setupDialogAllowed(),
                 "the seed marker alone must still open the gate -- a legitimately fresh "
                 "daemon profile must not be indistinguishable from something broken");
    }

    // ── The seed-marker special case in handlesKey() ────────────────────────

    void seedMarkerRoutesThroughBackendAndSurvivesSnapshotDespiteNotClassifyingStation()
    {
        // This test would be meaningless if the marker DID classify
        // Station -- confirm it genuinely does not, so the special case
        // in SettingsProxy::handlesKey() is doing real work.
        QCOMPARE(classifySettingsKey(QString::fromLatin1(AppSettings::kDaemonProfileSeededKey)),
                 SettingsScope::OperatorLocal);

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxy proxy;
        s.setRemoteBackend(&proxy);

        QVERIFY(!s.contains(QLatin1String(AppSettings::kDaemonProfileSeededKey)));

        QMap<QString, QString> data;
        data.insert(QLatin1String(AppSettings::kDaemonProfileSeededKey), QStringLiteral("True"));
        proxy.applySnapshot(data);

        QVERIFY2(s.contains(QLatin1String(AppSettings::kDaemonProfileSeededKey)),
                 "the seed marker must reach AppSettings::contains() through the proxy even "
                 "though classifySettingsKey() has no rule for it");
        QCOMPARE(s.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)).toString(),
                 QStringLiteral("True"));
    }

    // ── SettingsProxyServer: buildSnapshot() scope (Step 5) ─────────────────

    void serverBuildSnapshotScopesToConnectedMacPlusGlobalStationKeys()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        daemon.setHardwareValue(QStringLiteral("aa:bb"), QStringLiteral("radioInfo/sampleRate"), 192000);
        daemon.setHardwareValue(QStringLiteral("cc:dd"), QStringLiteral("radioInfo/sampleRate"), 96000);
        daemon.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));
        daemon.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#000"));

        SettingsProxyServer server(daemon);
        const QMap<QString, QString> snap = server.buildSnapshot(QStringLiteral("aa:bb"));

        QVERIFY(snap.contains(QStringLiteral("hardware/aa:bb/radioInfo/sampleRate")));
        QVERIFY2(!snap.contains(QStringLiteral("hardware/cc:dd/radioInfo/sampleRate")),
                 "hardware/ is per-MAC -- a different saved radio's subtree must not leak in");
        QVERIFY(snap.contains(QStringLiteral("Slice0/Locked")));
        QVERIFY(!snap.contains(QStringLiteral("DisplayNoiseFloorColor")));
    }

    void serverBuildSnapshotIncludesHardwareOcLiteralSegmentRegardlessOfConnectedMac()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        daemon.setHardwareValue(QStringLiteral("oc"), QStringLiteral("pennyExtCtrl"), QStringLiteral("True"));

        SettingsProxyServer server(daemon);
        const QMap<QString, QString> snap = server.buildSnapshot(QStringLiteral("aa:bb"));

        QVERIFY2(snap.contains(QStringLiteral("hardware/oc/pennyExtCtrl")),
                 "'oc' is a literal segment, not a MAC (Task 14's own canonical proof), and must "
                 "be included regardless of which MAC is connected");
    }

    void serverBuildSnapshotIncludesSeedMarkerWhenPresent()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        daemon.seedDaemonProfileMarker();

        SettingsProxyServer server(daemon);
        const QMap<QString, QString> snap = server.buildSnapshot(QString());

        QVERIFY(snap.contains(QLatin1String(AppSettings::kDaemonProfileSeededKey)));
        QCOMPARE(snap.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)), QStringLiteral("True"));
    }

    void serverBuildSnapshotOmitsSeedMarkerWhenAbsent()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        SettingsProxyServer server(daemon);
        const QMap<QString, QString> snap = server.buildSnapshot(QString());

        QVERIFY(!snap.contains(QLatin1String(AppSettings::kDaemonProfileSeededKey)));
    }

    void serverBuildSnapshotMeasuredSizeOnSyntheticRealisticFixture()
    {
        // The R2 design addendum section 8 measured a REAL settings file
        // at 15,201 keys / 3,186,789 bytes across five MACs (13,970 under
        // hardware/, 11,564 of those under hardware/<mac>/tx/profile/*).
        // That file is not available in this environment, so this test
        // builds a SCALED-DOWN synthetic fixture with the same shape --
        // five MACs, tx/profile/* dominating each one's hardware/ subtree,
        // a small global Station set, a larger OperatorLocal set -- and
        // measures buildSnapshot() for ONE connected MAC against it. The
        // task report quotes these numbers verbatim; they approximate,
        // they do not reproduce, the real file.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        const QStringList macs = {
            QStringLiteral("aa:aa:aa:aa:aa:aa"), QStringLiteral("bb:bb:bb:bb:bb:bb"),
            QStringLiteral("cc:cc:cc:cc:cc:cc"), QStringLiteral("dd:dd:dd:dd:dd:dd"),
            QStringLiteral("ee:ee:ee:ee:ee:ee"),
        };
        constexpr int kProfileEntriesPerMac = 550; // approximates 11,564 / 5, scaled down 4x
        constexpr int kOtherHardwareKeysPerMac = 30;
        for (const QString& mac : macs) {
            for (int p = 0; p < kProfileEntriesPerMac; ++p) {
                daemon.setHardwareValue(mac, QStringLiteral("tx/profile/P%1/field").arg(p),
                                        QStringLiteral("v"));
            }
            for (int k = 0; k < kOtherHardwareKeysPerMac; ++k) {
                daemon.setHardwareValue(mac, QStringLiteral("radioInfo/k%1").arg(k), QStringLiteral("v"));
            }
        }
        constexpr int kGlobalStationKeys = 40;
        for (int i = 0; i < kGlobalStationKeys; ++i) {
            daemon.setValue(QStringLiteral("Slice%1/Locked").arg(i % 4), QStringLiteral("True"));
        }
        constexpr int kOperatorLocalKeys = 200;
        for (int i = 0; i < kOperatorLocalKeys; ++i) {
            daemon.setValue(QStringLiteral("DisplaySomeCosmeticKey%1").arg(i), QStringLiteral("x"));
        }

        SettingsProxyServer server(daemon);
        const QMap<QString, QString> snap = server.buildSnapshot(macs.at(0));

        qint64 approxBytes = 0;
        for (auto it = snap.constBegin(); it != snap.constEnd(); ++it) {
            approxBytes += it.key().size() + it.value().size();
        }

        qInfo() << "SettingsProxyServer synthetic snapshot measurement:"
                << "totalKeysInStore=" << daemon.allKeys().size()
                << "snapshotKeyCount=" << snap.size()
                << "approxSnapshotBytes=" << approxBytes;

        const int expectedOneMacShare = kProfileEntriesPerMac + kOtherHardwareKeysPerMac; // 580
        QVERIFY2(snap.size() >= expectedOneMacShare,
                 "must carry at least the connected MAC's own hardware/ subtree");
        QVERIFY2(snap.size() <= expectedOneMacShare + kGlobalStationKeys,
                 "must not carry more than one MAC's hardware/ subtree plus the global Station set");
        QVERIFY2(snap.size() < daemon.allKeys().size(),
                 "the snapshot must be smaller than the whole store, never a full copy");
    }

    // ── The hazard: inbound-apply echo suppression (Steps 6+10 root cause) ─

    void serverSuppressesEchoOnInboundApply()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);

        const SettingsApplyResult result = server.applyInboundWrite(
            QStringLiteral("hardware/aa:bb/radioInfo/sampleRate"), 192000, QStringLiteral("client-42"));

        QVERIFY(result.accepted);
        QVERIFY(result.reason.isEmpty());
        QCOMPARE(daemon.value(QStringLiteral("hardware/aa:bb/radioInfo/sampleRate")).toInt(), 192000);

        // Exactly ONE broadcast -- the explicit, tagged emission
        // applyInboundWrite() makes itself. If the generic change-hook
        // path (onLocalAppSettingsChange) were not suppressed for the
        // duration of that call, this would be 2: the explicit one plus
        // a redundant, untagged second one reacting to the identical
        // underlying AppSettings::setValue(). This is the exact hazard
        // Task 13's review named by shape before this task was written.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("hardware/aa:bb/radioInfo/sampleRate"));
        QCOMPARE(spy.at(0).at(1).toInt(), 192000);
        QCOMPARE(spy.at(0).at(2).toString(), QStringLiteral("client-42"));
    }

    void bothBroadcastPathsEmitTheSameQVariantType()
    {
        // Fix round 1 (review, Minor 6). Before this fix,
        // applyInboundWrite() emitted the CALLER's raw QVariant (an int,
        // for a typical Task 18 decoder) while onLocalAppSettingsChange()
        // emitted m_appSettings.value(key) (always QVariant(QString),
        // since setValue() collapses everything to QString on the way
        // in). Task 18 serialises this onto the wire, where 192000 and
        // "192000" are different JSON -- the two paths must agree on
        // what a client receives for the identical underlying change.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);

        // Path 1: applyInboundWrite(), called with an int QVariant --
        // exactly what a real JSON-decoding Task 18 would pass for a
        // numeric field.
        server.applyInboundWrite(QStringLiteral("hardware/aa:bb/radioInfo/sampleRate"), 192000,
                                 QStringLiteral("client-1"));
        QCOMPARE(spy.count(), 1);
        const QVariant fromInboundPath = spy.at(0).at(1);
        spy.clear();

        // Path 2: a genuine local change, same underlying AppSettings
        // storage, different code path (onLocalAppSettingsChange).
        daemon.setValue(QStringLiteral("hardware/aa:bb/radioInfo/otherField"), 192000);
        QCOMPARE(spy.count(), 1);
        const QVariant fromLocalPath = spy.at(0).at(1);

        QCOMPARE(fromInboundPath.typeId(), fromLocalPath.typeId());
        QVERIFY2(fromInboundPath.typeId() != QMetaType::Int,
                 "the broadcast must carry QVariant(QString) -- what AppSettings::value() "
                 "actually returns -- not the caller's original int");
        QCOMPARE(fromInboundPath.toString(), QStringLiteral("192000"));
        QCOMPARE(fromInboundPath.toString(), fromLocalPath.toString());
    }

    // ── Whole-branch review, Minor 7 ──────────────────────────────────────
    //
    // SwrProtectionLimit is Station-scoped and reaches a PA-protection
    // gate. RadioModel's construction reads it and hands it straight to
    // SwrProtectionController::setLimit(), which stores without clamping,
    // while the only UI that writes it is a QDoubleSpinBox clamped to
    // 1.0..5.0 (TransmitSetupPages.cpp). Over the wire there was nothing
    // between the socket and the applied limit. Hardening rather than a
    // live defect: it needs an authenticated client and a daemon restart,
    // and it is the same "ungated inbound" class the R2 plan documents.
    // The bound here is the spinbox's, exactly, so the wire cannot express
    // a limit the operator's own control cannot.
    void serverRejectsSwrProtectionLimitOutsideTheSpinboxRange()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        daemon.setValue(QStringLiteral("SwrProtectionLimit"), QStringLiteral("2.0"));
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);
        const QString key = QStringLiteral("SwrProtectionLimit");

        // Disabling protection by asking for an unreachable SWR is the
        // shape that matters, not merely a malformed number.
        const SettingsApplyResult tooHigh =
            server.applyInboundWrite(key, 99.0, QStringLiteral("client-1"));
        QVERIFY(!tooHigh.accepted);
        QVERIFY(!tooHigh.reason.isEmpty());
        QCOMPARE(tooHigh.restoredValue.toString(), QStringLiteral("2.0"));

        // ...and below 1.0 is not physical: SWR cannot be under 1:1, and a
        // limit there trips the PA gate permanently.
        QVERIFY(!server.applyInboundWrite(key, 0.5, QStringLiteral("client-1")).accepted);
        QVERIFY(!server.applyInboundWrite(key, QStringLiteral("not a number"),
                                          QStringLiteral("client-1")).accepted);

        QCOMPARE(daemon.value(key).toString(), QStringLiteral("2.0"));
        QCOMPARE(spy.count(), 0);

        // Both boundaries themselves are legal, not just the interior.
        QVERIFY(server.applyInboundWrite(key, 1.0, QStringLiteral("client-1")).accepted);
        QVERIFY(server.applyInboundWrite(key, 5.0, QStringLiteral("client-1")).accepted);
        QVERIFY(server.applyInboundWrite(key, 2.5, QStringLiteral("client-1")).accepted);
        QCOMPARE(daemon.value(key).toDouble(), 2.5);
    }

    // R-R3-46 / R-R3-11: the Core's controller owns every step attenuator
    // and preamp key (options/stepAtt, options/autoAtt, options/preamp) and
    // saves them itself, so a raw write is refused whatever its value, in
    // range or not, with the plain "update this app" reason; the Core's own
    // value is handed back. This replaces the fix-round range tests, which
    // accepted in-range raw writes the controller then overwrote, and (R3
    // remote radio hardware Task 3) the out-of-range one: 999, -999 and a
    // value that is not a number are refused the same way, before any
    // range is read, and nothing is broadcast.
    void serverRefusesEveryStepAttenuatorAndPreampKeyWithThePlainReason()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        daemon.setHardwareValue(QStringLiteral("aa:bb"), QStringLiteral("options/stepAtt/rx1Band/20m"), 12);
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);
        const QString reason = QStringLiteral(
            "This Core keeps its own attenuator and preamp settings. Update this app to change them.");

        const QStringList keys = {
            QStringLiteral("hardware/aa:bb/options/stepAtt/rx1Value"),
            QStringLiteral("hardware/aa:bb/options/stepAtt/rx1Band/20m"),
            QStringLiteral("hardware/aa:bb/options/stepAtt/txBand/20m"),
            QStringLiteral("hardware/aa:bb/options/stepAtt/rx1Enabled"),
            QStringLiteral("hardware/aa:bb/options/stepAtt/attOnTxEnabled"),
            QStringLiteral("hardware/aa:bb/options/autoAtt/rx1Mode"),
            QStringLiteral("hardware/aa:bb/options/autoAtt/rx1HoldSeconds"),
            QStringLiteral("hardware/aa:bb/options/preamp/rx1Band/20m"),
            // Case does not open a way around it.
            QStringLiteral("HARDWARE/aa:bb/Options/StepAtt/rx1Value"),
        };
        for (const QString& key : keys) {
            QVERIFY2(isModelOwnedStepAttenuatorSettingsKey(key), qPrintable(key));
            for (const QVariant& dB : {QVariant(-999), QVariant(-28), QVariant(0), QVariant(40),
                                       QVariant(61), QVariant(999),
                                       QVariant(QStringLiteral("not a number"))}) {
                const SettingsApplyResult result =
                    server.applyInboundWrite(key, dB, QStringLiteral("client-1"));
                QVERIFY2(!result.accepted, qPrintable(key));
                QCOMPARE(result.reason, reason);
            }
        }
        const SettingsApplyResult band = server.applyInboundWrite(
            keys.at(1), 40, QStringLiteral("client-1"));
        QCOMPARE(band.restoredValue.toInt(), 12); // the Core's own value, untouched
        QCOMPARE(daemon.hardwareValue(QStringLiteral("aa:bb"),
                                      QStringLiteral("options/stepAtt/rx1Band/20m")).toInt(), 12);
        QVERIFY(!daemon.contains(keys.at(0)));
        QCOMPARE(spy.count(), 0);
    }

    void serverStepAttenuatorOwnershipIsScopedToThoseThreeFamilies()
    {
        // Other hardware keys, and names that merely look alike, still land.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxyServer server(daemon);
        for (const QString& key : {
                 QStringLiteral("hardware/aa:bb/radioInfo/sampleRate"),
                 QStringLiteral("hardware/aa:bb/options/stepAttLabel"),
                 QStringLiteral("hardware/aa:bb/antennaAlex/stepAtt/rx1Value"),
             }) {
            QVERIFY2(!isModelOwnedStepAttenuatorSettingsKey(key), qPrintable(key));
            QVERIFY2(server.applyInboundWrite(key, 192000, QStringLiteral("client-1")).accepted,
                     qPrintable(key));
        }
    }

    void serverRejectsInboundWriteForNonStationKey()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);

        const SettingsApplyResult result = server.applyInboundWrite(
            QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#ff0000"), QStringLiteral("client-42"));

        QVERIFY(!result.accepted);
        QVERIFY(!result.reason.isEmpty());
        QVERIFY2(!daemon.contains(QStringLiteral("DisplayNoiseFloorColor")),
                 "a rejected write must change nothing");
        QCOMPARE(spy.count(), 0);
    }

    void serverForwardsGenuineLocalChangeWithEmptyOriginTag()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);

        // A DIRECT AppSettings::setValue() call -- exactly what a
        // migration, or a Setup page open on the daemon's own console (if
        // this build ever grows one), makes. NOT through
        // applyInboundWrite().
        daemon.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Slice0/Locked"));
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("True"));
        QVERIFY2(spy.at(0).at(2).toString().isEmpty(),
                 "a genuine local change carries an EMPTY origin tag -- it is nobody's echo");
    }

    void serverIgnoresLocalOperatorLocalKeyChange()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        SettingsProxyServer server(daemon);
        QSignalSpy spy(&server, &SettingsProxyServer::outboundValueChanged);

        daemon.setValue(QStringLiteral("DisplayNoiseFloorColor"), QStringLiteral("#000000"));

        QCOMPARE(spy.count(), 0);
    }

    // ── Whole-branch review, Important 4 ─────────────────────────────────
    //
    // The two units behind the end-to-end case in tst_station_session
    // (aRemovedStationSettingReachesTheClientAsAbsenceNotAnEmptyString).
    // The daemon's change hook fires identically for a set and a remove,
    // and value() on an absent key returns an INVALID QVariant that the
    // relay used to flatten into "", so a removal reached every client as
    // "set to empty string".
    void serverReportsARemovalAsRemovalNotAsAnEmptyValue()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        daemon.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));

        SettingsProxyServer server(daemon);
        QSignalSpy changed(&server, &SettingsProxyServer::outboundValueChanged);
        QSignalSpy removed(&server, &SettingsProxyServer::outboundValueRemoved);

        daemon.remove(QStringLiteral("Slice0/Locked"));

        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(0).toString(), QStringLiteral("Slice0/Locked"));
        QVERIFY2(changed.count() == 0,
                 "a removal must not also be announced as a value change");

        // An ordinary write to the SAME key still takes the value path,
        // so this is a branch and not a blanket reclassification.
        daemon.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("False"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(1).toString(), QStringLiteral("False"));
        QCOMPARE(removed.count(), 1);

        // A genuine empty string is a VALUE, not an absence.
        daemon.setValue(QStringLiteral("Slice0/Locked"), QString());
        QCOMPARE(changed.count(), 2);
        QVERIFY(changed.at(1).at(1).toString().isEmpty());
        QCOMPARE(removed.count(), 1);
    }

    void applyRemoteRemovalLeavesTheKeyAbsentAndProvenUnset()
    {
        SettingsProxy proxy;
        proxy.setReady(true);
        proxy.applySnapshot({ { QStringLiteral("Slice0/Locked"), QStringLiteral("True") } });
        QVERIFY(proxy.contains(QStringLiteral("Slice0/Locked")));

        QSignalSpy outboundRemove(&proxy, &SettingsProxy::outboundRemoveRequested);
        proxy.applyRemoteRemoval(QStringLiteral("Slice0/Locked"));

        QVERIFY2(!proxy.contains(QStringLiteral("Slice0/Locked")),
                 "the daemon said the key is gone, so contains() must say so too");
        QCOMPARE(proxy.value(QStringLiteral("Slice0/Locked"), QStringLiteral("fallback"))
                     .toString(),
                 QStringLiteral("fallback"));
        QVERIFY2(!proxy.handledKeys().contains(QStringLiteral("Slice0/Locked")),
                 "a removed key must not still be listed");
        QVERIFY(proxy.provenUnsetKeys().contains(QStringLiteral("Slice0/Locked")));
        QVERIFY2(outboundRemove.count() == 0,
                 "applying the daemon's own report must not send it back a removal");
    }

    // The wire shape, so the absence encoding survives a real encode and
    // decode rather than only existing as a pair of C++ calls. Same
    // convention settingsReject already uses: no entry at all, never an
    // entry carrying an empty string.
    void anAbsentSettingsValueEncodesWithNoEntryAndSurvivesTheRoundTrip()
    {
        const QByteArray wire = SessionMessages::encode(
            SessionMessages::settingsValueAbsent(QStringLiteral("Slice0/Locked"), QString()));
        QVERIFY2(!wire.contains("\"value\""), wire.constData());

        SessionMessage back;
        QVERIFY2(SessionMessages::decode(wire, &back), wire.constData());
        QCOMPARE(back.kind, SessionMessageKind::SettingsValue);
        QCOMPARE(back.objectKey, QByteArray("Slice0/Locked"));
        QVERIFY2(back.updates.isEmpty(),
                 "an entry-less settings.value is what marks the key absent");

        // The distinguishing case: a genuine empty-string VALUE still
        // carries an entry, so the two cannot be confused on the wire.
        SessionMessage empty;
        QVERIFY(SessionMessages::decode(
            SessionMessages::encode(SessionMessages::settingsValue(
                QStringLiteral("Slice0/Locked"), QString(), QStringLiteral("client-1"))),
            &empty));
        QCOMPARE(empty.updates.size(), 1);
        QVERIFY(empty.updates.first().value.toString().isEmpty());
    }

    void serverDestructorClearsChangeHookAndSubsequentWritesDoNotCrash()
    {
        // AppSettings::instance()-shaped lifetime mismatch guard: a
        // SettingsProxyServer that outlives its own usefulness must not
        // leave a dangling `this` captured in AppSettings's change hook.
        // This cannot deterministically PROVE the hook was cleared
        // without a getter AppSettings deliberately does not expose (see
        // the task report) -- it is a smoke test relying on a sanitizer
        // build to flag a real dangling-pointer bug, not a self-contained
        // proof. What it does directly confirm: after the server is
        // destroyed, further AppSettings mutations complete normally and
        // round-trip correctly, which a live dangling std::function call
        // would put at serious risk of not doing.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings daemon(tmp.filePath(QStringLiteral("NereusSDR.settings")));
        {
            SettingsProxyServer server(daemon);
            Q_UNUSED(server);
        }
        daemon.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));
        QCOMPARE(daemon.value(QStringLiteral("Slice0/Locked")).toString(), QStringLiteral("True"));
    }
};

QTEST_APPLESS_MAIN(TstSettingsProxy)
#include "tst_settings_proxy.moc"
