// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// The plugins' link clients as real child processes, on a real DeckLink socket (R-HK-16 "I"):
// they find it through the discovery file, press and release tx.hold and turn tune.up, and
// killing a client while it holds releases the hold. Every wait is on what the link did.
#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/DeckLink.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kWaitMs = 30000;

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    QList<bool> keyHolds;
    QList<double> tunes;

    SliceModel* windowSlice() const override { return window; }
    SliceModel* sliceByLetter(QChar) const override { return window; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel*, double hz) override
    {
        tunes.append(hz);
        return true;
    }
    void runSliceAction(const QString&, SliceModel*, int) override {}
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool down) override
    {
        keyHolds.append(down);
        return true;
    }
    void runTransmitAction(const QString&) override {}
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return 1000; }
};

} // namespace

class TestDeckPluginClients : public QObject {
    Q_OBJECT

    static void useHome(const QString& home)
    {
        const QByteArray path = QFile::encodeName(home);
        qputenv("HOME", path);
        qputenv("CFFIXED_USER_HOME", path);
        qputenv("NEREUS_TEST_HOME", path);
    }

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        // A short home: a local socket's name has a short limit, and ctest's own test home
        // is too deep for it. The marker moves with it, before anything resolves a settings
        // path.
#if defined(Q_OS_WIN)
        m_home = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/tdp-XXXXXX"));
#else
        m_home = std::make_unique<QTemporaryDir>(QStringLiteral("/tmp/tdp-XXXXXX"));
#endif
        QVERIFY(m_home->isValid());
        useHome(m_home->path());
    }

    void aRealClientDrivesTheLinkAndKillingItReleases_data()
    {
        QTest::addColumn<QString>("program");
        QTest::addColumn<QString>("script");
        QTest::addColumn<QString>("plugin");
        QTest::newRow("node") << QStringLiteral("node") << QStringLiteral("elgato-nereussdr/link.test.js")
                              << QStringLiteral("elgato");
#if !defined(Q_OS_WIN)
        QTest::newRow("python3") << QStringLiteral("python3") << QStringLiteral("streamcontroller-nereussdr/test_link.py")
                                 << QStringLiteral("streamcontroller");
#endif
    }

    void aRealClientDrivesTheLinkAndKillingItReleases()
    {
        QFETCH(QString, program);
        QFETCH(QString, script);
        QFETCH(QString, plugin);
        const QString executable = QStandardPaths::findExecutable(program);
        if (executable.isEmpty()) {
            QSKIP(qPrintable(program + QStringLiteral(" is not on the path, so this client can't run")));
        }

        AppSettings::instance().clear();
        HotKeyBindings bindings;
        FakeTargets targets;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        SliceModel slice(0);
        slice.setFrequency(14200000.0);
        slice.setStepHz(100);
        targets.window = &slice;
        DeckLink link(&dispatcher, nullptr);
        QVERIFY2(link.setEnabled(true), qPrintable(link.lastError()));
        const HotKeySource source{HotKeySourceKind::Deck, QStringLiteral("1")};

        QProcess client;
        client.setProcessChannelMode(QProcess::ForwardedChannels);
        client.start(executable, {QStringLiteral(NEREUS_SOURCE_DIR "/plugins/") + script,
                                  QStringLiteral("--integration"), m_home->path()});
        QVERIFY2(client.waitForStarted(kWaitMs), qPrintable(client.errorString()));
        QVERIFY(QTest::qWaitFor([&] { return link.connectedPlugins().contains(plugin); }, kWaitMs));

        client.write("press tx.hold\n");
        QVERIFY(QTest::qWaitFor([&] { return targets.keyHolds.size() == 1; }, kWaitMs));
        QCOMPARE(targets.keyHolds, QList<bool>{true});
        QVERIFY(dispatcher.holdsTransmit(source));

        client.write("release tx.hold\n");
        QVERIFY(QTest::qWaitFor([&] { return targets.keyHolds.size() == 2; }, kWaitMs));
        QCOMPARE(targets.keyHolds, (QList<bool>{true, false}));
        QVERIFY(!dispatcher.holdsTransmit(source));

        client.write("turn tune.up 2\n");
        QVERIFY(QTest::qWaitFor([&] { return !targets.tunes.isEmpty(); }, kWaitMs));
        QCOMPARE(targets.tunes.at(0), 14200000.0 + 200.0);

        client.write("press tx.hold\n");
        QVERIFY(QTest::qWaitFor([&] { return targets.keyHolds.size() == 3; }, kWaitMs));
        QVERIFY(dispatcher.holdsTransmit(source));

        client.kill();
        QVERIFY(QTest::qWaitFor([&] { return targets.keyHolds.size() == 4; }, kWaitMs));
        QCOMPARE(targets.keyHolds, (QList<bool>{true, false, true, false}));
        QVERIFY(!dispatcher.holdsTransmit(source));
        QVERIFY(QTest::qWaitFor([&] { return link.connectedPlugins().isEmpty(); }, kWaitMs));
        client.waitForFinished(kWaitMs);
    }

private:
    std::unique_ptr<QTemporaryDir> m_home;
};

QTEST_MAIN(TestDeckPluginClients)
#include "tst_deck_plugin_clients.moc"
