// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 19.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// The deck plugin link, through a real QLocalSocket client on a real socket (R-HK-16): the
// action list, press and release, state and display, holds dropping with the connection
// however it ends, no network listener, other users refused. Every wait is on what the
// link did, with a long bound. A line that should do nothing is shown to have done nothing
// by a later line on the same connection, which runs after it.
#include <QtTest>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/DeckLink.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyStateFeed.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/UsbPresence.h"
#include "models/SliceModel.h"

#if !defined(Q_OS_WIN)
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using namespace NereusSDR;

namespace {

constexpr int kWaitMs = 30000;

// The same bytes tst_device_output uses for the simulated Stream Deck+.
constexpr quint16 kDeckVid = 0x0FD9;
constexpr quint16 kDeckPid = 0x0084;

const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};

QByteArray sourceFile(const char* relative)
{
    QFile file(QStringLiteral(NEREUS_SOURCE_DIR "/") + QLatin1String(relative));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct SliceCall {
    QString id;
    SliceModel* slice;
};

struct TuneCall {
    SliceModel* slice;
    double hz;
};

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    QHash<QChar, SliceModel*> open;
    QList<SliceCall> sliceActions;
    QList<TuneCall> tunes;
    QList<bool> keyHolds;
    QList<QPair<QString, bool>> cwKeys;
    std::function<void()> duringKeyDown;

    SliceModel* windowSlice() const override { return window; }
    SliceModel* sliceByLetter(QChar letter) const override { return open.value(letter, nullptr); }
    QChar letterOf(const SliceModel* slice) const override { return open.key(const_cast<SliceModel*>(slice)); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel* slice, double hz) override
    {
        tunes.append({slice, hz});
        return true;
    }
    void runSliceAction(const QString& id, SliceModel* slice, int) override { sliceActions.append({id, slice}); }
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool down) override
    {
        keyHolds.append(down);
        if (down && duringKeyDown) {
            duringKeyDown();
        }
        return true;
    }
    void runTransmitAction(const QString&) override {}
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString& id, bool down) override { cwKeys.append({id, down}); }
    void toast(const QString&) override {}
    qint64 nowMs() const override { return 1000; }
};

class FakeSource : public HotKeyStateSource {
public:
    SliceModel* slice{nullptr};
    QHash<QString, bool> transmit;

    SliceModel* windowSlice() const override { return slice; }
    bool transmitOn(const QString& id) const override { return transmit.value(id, false); }
    bool muted(const SliceModel* s) const override { return s->muted(); }
    bool radioConnected() const override { return true; }
    bool radioTransmitting() const override { return false; }
};

// A simulated Stream Deck+, on the bus and opening whenever asked.
struct FakeBus {
    QMutex lock;
    QSet<quint32> present;
    int closes{0};
};

class FakeHandle : public HidDeviceHandle {
public:
    explicit FakeHandle(std::shared_ptr<FakeBus> bus)
        : m_bus(std::move(bus))
    {
    }
    ~FakeHandle() override
    {
        const QMutexLocker locker(&m_bus->lock);
        ++m_bus->closes;
    }
    int read(char*, int) override { return 0; }
    int write(const QByteArray& report) override { return static_cast<int>(report.size()); }

private:
    std::shared_ptr<FakeBus> m_bus;
};

class FakeLayer : public HidLayer {
public:
    explicit FakeLayer(std::shared_ptr<FakeBus> bus)
        : m_bus(std::move(bus))
    {
    }
    QSet<quint32> present() override
    {
        const QMutexLocker locker(&m_bus->lock);
        return m_bus->present;
    }
    std::unique_ptr<HidDeviceHandle> open(quint16, quint16, QString*) override
    {
        return std::make_unique<FakeHandle>(m_bus);
    }

private:
    std::shared_ptr<FakeBus> m_bus;
};

// What a plugin is: a socket, and the JSON lines that have come in on it.
class Client {
public:
    std::unique_ptr<QLocalSocket> socket = std::make_unique<QLocalSocket>();
    QList<QJsonObject> messages;

    explicit Client(const QString& path)
    {
        QObject::connect(socket.get(), &QLocalSocket::readyRead, socket.get(), [this] { read(); });
        socket->connectToServer(path);
        socket->waitForConnected(kWaitMs);
    }

    bool connected() const { return socket != nullptr && socket->state() == QLocalSocket::ConnectedState; }

    void send(const QByteArray& line)
    {
        socket->write(line);
        socket->flush();
    }

    bool hello(const QByteArray& plugin = "test")
    {
        send("{\"type\":\"hello\",\"plugin\":\"" + plugin + "\",\"version\":\"1.0.0\"}\n");
        return waitForType(QStringLiteral("display"));
    }

    bool waitForDrop()
    {
        return QTest::qWaitFor([this] { return !connected(); }, kWaitMs);
    }

    bool waitForType(const QString& type)
    {
        return QTest::qWaitFor([this, type] { return !ofType(type).isEmpty(); }, kWaitMs);
    }

    bool waitFor(const std::function<bool(const QJsonObject&)>& match)
    {
        return QTest::qWaitFor([this, &match] {
            return std::any_of(messages.cbegin(), messages.cend(), match);
        }, kWaitMs);
    }

    QList<QJsonObject> ofType(const QString& type) const
    {
        QList<QJsonObject> found;
        for (const QJsonObject& message : messages) {
            if (message.value(QStringLiteral("type")).toString() == type) {
                found.append(message);
            }
        }
        return found;
    }

private:
    void read()
    {
        m_buffer += socket->readAll();
        qsizetype end = 0;
        while ((end = m_buffer.indexOf('\n')) >= 0) {
            messages.append(QJsonDocument::fromJson(m_buffer.left(end)).object());
            m_buffer.remove(0, end + 1);
        }
    }

    QByteArray m_buffer;
};

} // namespace

class TestDeckLink : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_home;
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<FakeTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<SliceModel> m_a;
    std::unique_ptr<SliceModel> m_b;
    std::unique_ptr<FakeSource> m_source;
    std::unique_ptr<HotKeyStateFeed> m_feed;
    std::unique_ptr<DeckLink> m_link;

    static HotKeySource deckSource(int connection)
    {
        return HotKeySource{HotKeySourceKind::Deck, QString::number(connection)};
    }

    // Runs after every line sent ahead of it on this connection.
    bool sentinel(Client& client)
    {
        const qsizetype before = m_targets->sliceActions.size();
        client.send("{\"type\":\"press\",\"id\":\"mute\"}\n");
        return QTest::qWaitFor([this, before] { return m_targets->sliceActions.size() > before; }, kWaitMs);
    }

    static void useHome(const QString& home)
    {
        const QByteArray path = QFile::encodeName(home);
        qputenv("HOME", path);
        qputenv("CFFIXED_USER_HOME", path);
        qputenv("NEREUS_TEST_HOME", path);
    }

    QStringList deckFiles() const
    {
        return QDir(DeckLink::deckFolder()).entryList(QDir::Files | QDir::System | QDir::Hidden);
    }

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        // A short home: a local socket's name has a short limit, and ctest's own test
        // home is too deep for it. The marker moves with it, before anything resolves a
        // settings path, and every path below comes from it.
#if defined(Q_OS_WIN)
        m_home = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/tdl-XXXXXX"));
#else
        m_home = std::make_unique<QTemporaryDir>(QStringLiteral("/tmp/tdl-XXXXXX"));
#endif
        QVERIFY(m_home->isValid());
        useHome(m_home->path());
    }

    void init()
    {
        useHome(m_home->path());
        QVERIFY(QDir(QDir(m_home->path()).filePath(QStringLiteral(".nereussdr"))).removeRecursively());
        AppSettings::instance().clear();

        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<FakeTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_a = std::make_unique<SliceModel>(0);
        m_b = std::make_unique<SliceModel>(1);
        m_a->setFrequency(14200000.0);
        m_a->setStepHz(100);
        m_b->setFrequency(7100000.0);
        m_b->setStepHz(100);
        m_targets->window = m_a.get();
        m_targets->open.insert(QLatin1Char('A'), m_a.get());
        m_targets->open.insert(QLatin1Char('B'), m_b.get());
        m_source = std::make_unique<FakeSource>();
        m_source->slice = m_a.get();
        m_feed = std::make_unique<HotKeyStateFeed>(m_source.get());
        m_link = std::make_unique<DeckLink>(m_dispatcher.get(), m_feed.get());
    }

    void cleanup()
    {
        m_link.reset();     // ahead of the home moving back, so it removes what it made
        useHome(m_home->path());
        m_feed.reset();
        m_source.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_b.reset();
        m_a.reset();
        m_bindings.reset();
        DeviceInput::setLayerFactoryForTest(nullptr);
        AppSettings::instance().clear();
    }

    // ---- a test run stays away from the real home ----

    void theFolderIsUnderTheTestHome()
    {
        QCOMPARE(DeckLink::deckFolder(), QDir(m_home->path()).filePath(QStringLiteral(".nereussdr/deck")));
#if !defined(Q_OS_WIN)
        const passwd* const account = getpwuid(getuid());
        QVERIFY(account != nullptr);
        const QString realHome = QString::fromLocal8Bit(account->pw_dir);
        QVERIFY(!DeckLink::deckFolder().startsWith(realHome + QLatin1Char('/')));
        QVERIFY(!m_link->socketPath().startsWith(realHome + QLatin1Char('/')));
#endif
    }

    void nothingListensUntilAsked()
    {
        QVERIFY(!m_link->isListening());
        QVERIFY(!QFileInfo::exists(DeckLink::deckFolder()));
        QVERIFY(!DeviceInput::realDevicesAllowed());
    }

    void aWindowOpensItOnlyInTheApplication()
    {
        const QByteArray window = sourceFile("src/gui/MainWindow.cpp");
        const qsizetype gate = window.indexOf("DeviceInput::realDevicesAllowed() && DeckLink::savedEnabled()");
        QVERIFY(gate >= 0);
        QCOMPARE(window.count("m_deckLink->setEnabled("), 1);
        QVERIFY(window.indexOf("m_deckLink->setEnabled(") > gate);
    }

    // ---- hello ----

    void helloGetsEveryActionTheStateAndTheDisplay()
    {
        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());

        const QJsonObject hello = client.messages.first();
        QCOMPARE(hello.value(QStringLiteral("type")).toString(), QStringLiteral("hello"));
        QCOMPARE(hello.value(QStringLiteral("protocol")).toInt(), DeckLink::kProtocolVersion);
        QHash<QString, QJsonObject> wire;
        for (const QJsonValue& value : hello.value(QStringLiteral("actions")).toArray()) {
            wire.insert(value.toObject().value(QStringLiteral("id")).toString(), value.toObject());
        }
        QCOMPARE(wire.size(), HotKeyActionList::all().size());
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            QVERIFY2(wire.contains(action.id), qPrintable(action.id));
            const QJsonObject sent = wire.value(action.id);
            QCOMPARE(sent.value(QStringLiteral("name")).toString(), action.name);
            QCOMPARE(sent.value(QStringLiteral("group")).toString(), action.group);
            QCOMPARE(sent.value(QStringLiteral("transmits")).toBool(), action.transmits);
            const QString gesture = sent.value(QStringLiteral("gesture")).toString();
            QCOMPARE(gesture, action.gesture == HotKeyGesture::Press ? QStringLiteral("press")
                              : action.gesture == HotKeyGesture::Hold ? QStringLiteral("hold")
                                                                      : QStringLiteral("turn"));
        }

        // A key lit before it connected is lit on its deck, and so is one that's off.
        QVERIFY(client.waitFor([](const QJsonObject& m) {
            return m.value(QStringLiteral("type")).toString() == QLatin1String("state")
                && m.value(QStringLiteral("id")).toString() == QLatin1String("tx.mox")
                && m.value(QStringLiteral("on")).toBool();
        }));
        QCOMPARE(client.ofType(QStringLiteral("state")).size(), HotKeyStateFeed::actionIds().size());

        const QJsonObject display = client.ofType(QStringLiteral("display")).first();
        QCOMPARE(display.value(QStringLiteral("frequency")).toString(), m_feed->display().frequency);
        QCOMPARE(display.value(QStringLiteral("mode")).toString(), m_feed->display().mode);
        QCOMPARE(display.value(QStringLiteral("filter")).toString(), m_feed->display().filter);
        QCOMPARE(display.value(QStringLiteral("slice")).toString(), QStringLiteral("A"));
        QCOMPARE(m_link->connectedPlugins(), QList<QString>{QStringLiteral("test")});
    }

    void aStateChangeAndADisplayChangeArrive()
    {
        QVERIFY(m_link->setEnabled(true));
        Client quiet(m_link->socketPath());     // no hello: it hears nothing
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        client.messages.clear();

        m_source->transmit.insert(QStringLiteral("tx.mox"), true);
        m_feed->refresh();
        QVERIFY(client.waitFor([](const QJsonObject& m) {
            return m.value(QStringLiteral("type")).toString() == QLatin1String("state")
                && m.value(QStringLiteral("id")).toString() == QLatin1String("tx.mox")
                && m.value(QStringLiteral("on")).toBool();
        }));

        m_a->setFrequency(14300000.0);
        QVERIFY(client.waitFor([](const QJsonObject& m) {
            return m.value(QStringLiteral("type")).toString() == QLatin1String("display")
                && m.value(QStringLiteral("frequency")).toString() == QLatin1String("14.300.000");
        }));
        QVERIFY(quiet.messages.isEmpty());
    }

    // ---- what a plugin sends ----

    void aPressReachesTheDispatcherWithItsSourceAndPin()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());

        client.send("{\"type\":\"press\",\"id\":\"mute\",\"pin\":\"B\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->sliceActions.size() == 1; }, kWaitMs));
        QCOMPARE(m_targets->sliceActions.at(0).id, QStringLiteral("mute"));
        QCOMPARE(m_targets->sliceActions.at(0).slice, m_b.get());

        client.send("{\"type\":\"press\",\"id\":\"mute\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->sliceActions.size() == 2; }, kWaitMs));
        QCOMPARE(m_targets->sliceActions.at(1).slice, m_a.get());

        // The ledger says whose hold it is. This is the link's first connection.
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 1; }, kWaitMs));
        QVERIFY(m_dispatcher->holdsTransmit(deckSource(1)));
        QVERIFY(!m_dispatcher->holdsTransmit(deckSource(2)));

        client.send("{\"type\":\"release\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 2; }, kWaitMs));
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
    }

    void aTurnReachesTheDispatcherWithItsPin()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());

        client.send("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\",\"steps\":-2,\"pin\":\"B\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return !m_targets->tunes.isEmpty(); }, kWaitMs));
        QCOMPARE(m_targets->tunes.at(0).slice, m_b.get());
        QCOMPARE(m_targets->tunes.at(0).hz, 7100000.0 - 200.0);
    }

    void aReleaseWithABadPinStillLetsGo()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 1; }, kWaitMs));
        client.send("{\"type\":\"release\",\"id\":\"tx.hold\",\"pin\":\"nonsense\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 2; }, kWaitMs));
        QCOMPARE(m_targets->keyHolds.at(1), false);
    }

    void aBadLineIsIgnoredAndTheConnectionStays_data()
    {
        QTest::addColumn<QByteArray>("line");
        QTest::addColumn<QString>("warning");

        const QString notAnObject = QStringLiteral("isn't a JSON object");
        const QString unknownAction = QStringLiteral("unknown action");
        const QString badSlice = QStringLiteral("bad slice");
        QTest::newRow("not JSON") << QByteArray("{not json") << notAnObject;
        QTest::newRow("an array") << QByteArray("[1,2]") << notAnObject;
        QTest::newRow("no type") << QByteArray("{\"id\":\"tx.hold\"}") << QStringLiteral("unknown type");
        QTest::newRow("unknown type") << QByteArray("{\"type\":\"reboot\"}") << QStringLiteral("unknown type");
        QTest::newRow("a window's message") << QByteArray("{\"type\":\"state\",\"id\":\"tx.mox\",\"on\":true}") << QStringLiteral("unknown type");
        QTest::newRow("hello with no name") << QByteArray("{\"type\":\"hello\"}") << QStringLiteral("no plugin name");
        QTest::newRow("press unknown id") << QByteArray("{\"type\":\"press\",\"id\":\"tx.nothing\"}") << unknownAction;
        QTest::newRow("press with no id") << QByteArray("{\"type\":\"press\"}") << unknownAction;
        QTest::newRow("release unknown id") << QByteArray("{\"type\":\"release\",\"id\":\"tx.nothing\"}") << unknownAction;
        QTest::newRow("pin out of range") << QByteArray("{\"type\":\"press\",\"id\":\"tx.hold\",\"pin\":\"E\"}") << badSlice;
        QTest::newRow("pin in lower case") << QByteArray("{\"type\":\"press\",\"id\":\"tx.hold\",\"pin\":\"b\"}") << badSlice;
        QTest::newRow("pin too long") << QByteArray("{\"type\":\"press\",\"id\":\"tx.hold\",\"pin\":\"AB\"}") << badSlice;
        QTest::newRow("pin not text") << QByteArray("{\"type\":\"press\",\"id\":\"tx.hold\",\"pin\":1}") << badSlice;
        QTest::newRow("pin empty") << QByteArray("{\"type\":\"press\",\"id\":\"tx.hold\",\"pin\":\"\"}") << badSlice;
        QTest::newRow("turn unknown up") << QByteArray("{\"type\":\"turn\",\"idUp\":\"x\",\"idDown\":\"tune.down\",\"steps\":1}") << unknownAction;
        QTest::newRow("turn unknown down") << QByteArray("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"x\",\"steps\":1}") << unknownAction;
        QTest::newRow("turn steps text") << QByteArray("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\",\"steps\":\"1\"}") << QStringLiteral("bad steps");
        QTest::newRow("turn steps part") << QByteArray("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\",\"steps\":1.5}") << QStringLiteral("bad steps");
        QTest::newRow("turn steps huge") << QByteArray("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\",\"steps\":1e12}") << QStringLiteral("bad steps");
        QTest::newRow("turn no steps") << QByteArray("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\"}") << QStringLiteral("bad steps");
        QTest::newRow("turn bad pin") << QByteArray("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\",\"steps\":1,\"pin\":\"Z\"}") << badSlice;
    }

    void aBadLineIsIgnoredAndTheConnectionStays()
    {
        QFETCH(QByteArray, line);
        QFETCH(QString, warning);
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QRegularExpression::escape(warning)));
        client.send(line + '\n');
        QVERIFY(sentinel(client));

        QVERIFY(client.connected());
        QCOMPARE(m_targets->sliceActions.size(), 1);
        QVERIFY(m_targets->keyHolds.isEmpty());
        QVERIFY(m_targets->tunes.isEmpty());
    }

    void anActionBeforeHelloIsIgnored()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("before its hello")));
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(client.hello());
        QVERIFY(sentinel(client));
        QVERIFY(m_targets->keyHolds.isEmpty());
    }

    void aLineOverTheLimitClosesOnlyThatConnection()
    {
        QVERIFY(m_link->setEnabled(true));
        Client other(m_link->socketPath());
        QVERIFY(other.hello());
        Client client(m_link->socketPath());
        QVERIFY(client.hello());

        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 1; }, kWaitMs));

        // Exactly the limit is a line, here a bad one. It's the next byte that closes.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("isn't a JSON object")));
        client.send(QByteArray(DeckLink::kMaxLineBytes, 'x') + '\n');
        QVERIFY(sentinel(client));
        QVERIFY(client.connected());

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("went over")));
        client.send(QByteArray(DeckLink::kMaxLineBytes + 1, 'x'));  // no newline needed
        QVERIFY(client.waitForDrop());
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
        QVERIFY(other.connected());
        QVERIFY(sentinel(other));
    }

    void partLinesWaitForTheRest()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());

        const QByteArray line = "{\"type\":\"press\",\"id\":\"mute\",\"pin\":\"B\"}\n";
        client.send(line.left(10));
        // Whether or not this half has been read when the rest goes, one action runs.
        QCoreApplication::processEvents();
        QVERIFY(m_targets->sliceActions.isEmpty());
        client.send(line.mid(10));
        QVERIFY(QTest::qWaitFor([this] { return m_targets->sliceActions.size() == 1; }, kWaitMs));
        QCOMPARE(m_targets->sliceActions.at(0).slice, m_b.get());

        // And two in one read, with a carriage return on the first.
        client.send("{\"type\":\"press\",\"id\":\"mute\"}\r\n{\"type\":\"press\",\"id\":\"mute\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->sliceActions.size() == 3; }, kWaitMs));
    }

    // ---- a hold never outlives its connection ----

    void aHoldDropsHoweverTheConnectionEnds_data()
    {
        QTest::addColumn<QString>("how");
        QTest::newRow("plugin closes") << QStringLiteral("close");
        QTest::newRow("plugin aborts") << QStringLiteral("abort");
        QTest::newRow("socket destroyed") << QStringLiteral("destroy");
        QTest::newRow("line over the limit") << QStringLiteral("overlong");
        QTest::newRow("link switched off") << QStringLiteral("off");
        QTest::newRow("link destroyed") << QStringLiteral("destroyLink");
    }

    void aHoldDropsHoweverTheConnectionEnds()
    {
        QFETCH(QString, how);
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        client.send("{\"type\":\"press\",\"id\":\"cw.dot\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->cwKeys.size() == 1; }, kWaitMs));
        QCOMPARE(m_targets->keyHolds, QList<bool>{true});
        QVERIFY(m_dispatcher->holdsTransmit(deckSource(1)));

        if (how == QLatin1String("close")) {
            client.socket->disconnectFromServer();
        } else if (how == QLatin1String("abort")) {
            client.socket->abort();
        } else if (how == QLatin1String("destroy")) {
            client.socket.reset();
        } else if (how == QLatin1String("overlong")) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("went over")));
            client.send(QByteArray(DeckLink::kMaxLineBytes + 1, 'x'));
        } else if (how == QLatin1String("off")) {
            QVERIFY(m_link->setEnabled(false));
        } else {
            m_link.reset();
        }
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() >= 2; }, kWaitMs));
        QVERIFY(QTest::qWaitFor([this] { return m_targets->cwKeys.size() >= 2; }, kWaitMs));

        // Once, and only once: nothing still queued calls it again.
        if (m_link != nullptr && m_link->isListening()) {
            Client witness(m_link->socketPath());
            QVERIFY(witness.hello());
            QVERIFY(sentinel(witness));
        }
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
        QCOMPARE(m_targets->cwKeys.at(1), (QPair<QString, bool>{QStringLiteral("cw.dot"), false}));
        QVERIFY(!m_dispatcher->holdsTransmit(deckSource(1)));
        QVERIFY(!m_dispatcher->transmitHoldOn());
    }

    void anotherSourcesHoldStays()
    {
        QVERIFY(m_link->setEnabled(true));
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QCOMPARE(m_targets->keyHolds, QList<bool>{true});

        Client first(m_link->socketPath());
        QVERIFY(first.hello());
        Client second(m_link->socketPath());
        QVERIFY(second.hello());
        first.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        second.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] {
            return m_dispatcher->holdsTransmit(deckSource(1)) && m_dispatcher->holdsTransmit(deckSource(2));
        }, kWaitMs));

        first.socket->abort();
        QVERIFY(QTest::qWaitFor([this] { return !m_dispatcher->holdsTransmit(deckSource(1)); }, kWaitMs));
        QVERIFY(m_dispatcher->holdsTransmit(deckSource(2)));
        QVERIFY(m_dispatcher->holdsTransmit(kKeyboard));

        second.socket->abort();
        QVERIFY(QTest::qWaitFor([this] { return !m_dispatcher->holdsTransmit(deckSource(2)); }, kWaitMs));
        QVERIFY(m_dispatcher->holdsTransmit(kKeyboard));
        QCOMPARE(m_targets->keyHolds, QList<bool>{true});   // still keyed, by the keyboard

        m_dispatcher->release(QStringLiteral("tx.hold"), kKeyboard);
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
    }

    void aConnectionThatEndsInsideItsPressStillLetsGo()
    {
        // A press can spin the event loop. Ending there leaves nothing to deliver the
        // release, and the ledger took the hold only after the drop looked.
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        m_targets->duringKeyDown = [this, &client] {
            client.socket->abort();
            QVERIFY(QTest::qWaitFor([this] { return m_link->connectedPlugins().isEmpty(); }, kWaitMs));
        };
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 2; }, kWaitMs));
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
        QVERIFY(!m_dispatcher->holdsTransmit(deckSource(1)));
    }

    void aPluginThatStopsReadingIsClosedAndItsHoldDrops()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 1; }, kWaitMs));

        // It reads nothing more, so the system's buffer fills and then the link's does.
        client.socket->setReadBufferSize(1);
        QObject::disconnect(client.socket.get(), &QLocalSocket::readyRead, nullptr, nullptr);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("stopped reading")));
        double hz = 14000000.0;
        for (int i = 0; i < 2000000 && m_targets->keyHolds.size() == 1; ++i) {
            m_a->setFrequency(hz += 1.0);
            if (i % 64 == 0) {
                QCoreApplication::processEvents();
            }
        }
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
        QVERIFY(m_link->connectedPlugins().isEmpty());
    }

    // ---- no network, and only this user ----

    void theClassUsesOnlyALocalSocket()
    {
        const QByteArray source = sourceFile("src/gui/hotkeys/DeckLink.cpp") + sourceFile("src/gui/hotkeys/DeckLink.h");
        QVERIFY(source.contains("QLocalServer"));
        for (const char* const forbidden : {"QTcp", "QUdp", "QSsl", "QWebSocket", "QHostAddress", "QNetwork",
                                            "QAbstractSocket", "QDtls", "QSctp", "AbstractNamespace"}) {
            QVERIFY2(!source.contains(forbidden), forbidden);
        }
        QVERIFY(source.contains("QLocalServer::UserAccessOption"));
        QVERIFY(!source.contains("WorldAccessOption"));
    }

    void onlyThisUserCanReachIt()
    {
#if defined(Q_OS_WIN)
        QSKIP("Windows has no mode bits. The source check above covers UserAccessOption there.");
#else
        const QFileDevice::Permissions kOwner = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
        const QFileDevice::Permissions kOthers = QFileDevice::ReadGroup | QFileDevice::WriteGroup
            | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;

        // A folder someone left open is closed up again.
        QVERIFY(QDir().mkpath(DeckLink::deckFolder()));
        QVERIFY(QFile::setPermissions(DeckLink::deckFolder(), kOwner | kOthers));
        QVERIFY(m_link->setEnabled(true));

        const QFileDevice::Permissions folder = QFile::permissions(DeckLink::deckFolder());
        QCOMPARE(folder & kOthers, QFileDevice::Permissions());
        QCOMPARE(folder & kOwner, kOwner);

        const QFileDevice::Permissions socket = QFile::permissions(m_link->socketPath());
        QVERIFY(socket != QFileDevice::Permissions());
        QCOMPARE(socket & kOthers, QFileDevice::Permissions());

        const QString discovery = QDir(DeckLink::deckFolder()).filePath(
            QFileInfo(m_link->socketPath()).completeBaseName() + QStringLiteral(".json"));
        const QFileDevice::Permissions file = QFile::permissions(discovery);
        QVERIFY(file != QFileDevice::Permissions());
        QCOMPARE(file & kOthers, QFileDevice::Permissions());
#endif
    }

    // ---- the name and the discovery file ----

    void theNameComesFromTheSettingsFile()
    {
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(AppSettings::instance().filePath().toUtf8(), QCryptographicHash::Sha256)
                .toHex()
                .left(12));
#if defined(Q_OS_WIN)
        QCOMPARE(m_link->socketPath(), QStringLiteral("\\\\.\\pipe\\nereussdr-deck-") + hash);
#else
        QCOMPARE(m_link->socketPath(), QDir(DeckLink::deckFolder()).filePath(hash + QStringLiteral(".sock")));
#endif
    }

    void theDiscoveryFileSaysWhereAndGoesWithTheLink()
    {
        QVERIFY(m_link->setEnabled(true));
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(AppSettings::instance().filePath().toUtf8(), QCryptographicHash::Sha256)
                .toHex()
                .left(12));
        const QString path = QDir(DeckLink::deckFolder()).filePath(hash + QStringLiteral(".json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject content = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(content.value(QStringLiteral("protocol")).toInt(), 1);
        QCOMPARE(content.value(QStringLiteral("path")).toString(), m_link->socketPath());
        QCOMPARE(qint64(content.value(QStringLiteral("pid")).toDouble()), QCoreApplication::applicationPid());
        QCOMPARE(content.value(QStringLiteral("profile")).toString(), AppSettings::profileOverride());
        QVERIFY(content.contains(QStringLiteral("profile")));

        // Nothing half-written is left beside it for a plugin to find.
        const QStringList jsonFiles = QDir(DeckLink::deckFolder()).entryList({QStringLiteral("*")}, QDir::Files | QDir::Hidden);
        QCOMPARE(jsonFiles.filter(QStringLiteral(".json")), QStringList{hash + QStringLiteral(".json")});

        // What the file names does answer.
        Client client(content.value(QStringLiteral("path")).toString());
        QVERIFY(client.hello());

        QVERIFY(m_link->setEnabled(false));
        QVERIFY(!QFileInfo::exists(path));
#if !defined(Q_OS_WIN)
        QVERIFY(!QFileInfo::exists(m_link->socketPath()));
#endif
    }

    // ---- the off switches ----

    void theLinkSwitchClosesEverythingAndIsSaved()
    {
        QVERIFY(DeckLink::savedEnabled());     // default on
        QVERIFY(m_link->setEnabled(true));
        QVERIFY(m_link->isListening());
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        QSignalSpy plugins(m_link.get(), &DeckLink::pluginsChanged);

        QVERIFY(m_link->setEnabled(false));
        QVERIFY(!m_link->isListening());
        QVERIFY(deckFiles().isEmpty());
        QVERIFY(m_link->connectedPlugins().isEmpty());
        QCOMPARE(plugins.size(), 1);
        QVERIFY(client.waitForDrop());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/DeckLink/Enabled")).toString(), QStringLiteral("False"));
        QVERIFY(!DeckLink::savedEnabled());

        Client refused(m_link->socketPath());
        QVERIFY(QTest::qWaitFor([&refused] { return refused.socket->state() == QLocalSocket::UnconnectedState; }, kWaitMs));

        QVERIFY(m_link->setEnabled(true));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/DeckLink/Enabled")).toString(), QStringLiteral("True"));
        Client again(m_link->socketPath());
        QVERIFY(again.hello());
    }

    void theGlobalSwitchMeansMessagesReachNothing()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        QVERIFY(QTest::qWaitFor([this] { return m_targets->keyHolds.size() == 1; }, kWaitMs));

        m_bindings->setEnabled(false);
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));

        client.send("{\"type\":\"press\",\"id\":\"tx.hold\"}\n");
        client.send("{\"type\":\"press\",\"id\":\"mute\"}\n");
        client.send("{\"type\":\"turn\",\"idUp\":\"tune.up\",\"idDown\":\"tune.down\",\"steps\":1}\n");
        // The connection stays: a second hello gets its answer, which comes after those lines.
        const qsizetype heard = client.ofType(QStringLiteral("hello")).size();
        client.send("{\"type\":\"hello\",\"plugin\":\"test\"}\n");
        QVERIFY(QTest::qWaitFor([&client, heard] { return client.ofType(QStringLiteral("hello")).size() > heard; }, kWaitMs));
        QCOMPARE(m_targets->keyHolds, (QList<bool>{true, false}));
        QVERIFY(m_targets->sliceActions.isEmpty());
        QVERIFY(m_targets->tunes.isEmpty());
        QVERIFY(client.connected());
    }

    // ---- socket in use, and left behind ----

    void aSecondLinkReportsInUseAndLeavesTheFirstAlone()
    {
        QVERIFY(m_link->setEnabled(true));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
        const QStringList before = deckFiles();

        {
            DeckLink second(m_dispatcher.get(), m_feed.get());
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Deck link not open")));
            QVERIFY(!second.setEnabled(true));
            QVERIFY(!second.isListening());
            QVERIFY(second.lastError().contains(QStringLiteral("already has the deck plugin connection")));
            QCOMPARE(deckFiles(), before);
            // Going off and going away touch nothing of the first's.
            QVERIFY(second.setEnabled(false));
            QVERIFY(second.lastError().isEmpty());
            QCOMPARE(deckFiles(), before);
        }
        QCOMPARE(deckFiles(), before);
        QVERIFY(m_link->isListening());
        QVERIFY(m_link->lastError().isEmpty());

        // The first still takes old and new connections, and keys still work.
        QVERIFY(sentinel(client));
        Client fresh(m_link->socketPath());
        QVERIFY(fresh.hello());
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QCOMPARE(m_targets->keyHolds, QList<bool>{true});
    }

    void whatAStoppedLinkLeftBehindIsReplaced()
    {
#if defined(Q_OS_WIN)
        QSKIP("A pipe leaves no file behind.");
#else
        QVERIFY(QDir().mkpath(DeckLink::deckFolder()));
        const QString path = m_link->socketPath();

        // A real socket file nobody answers on, as a crash leaves.
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(fd >= 0);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const QByteArray encoded = QFile::encodeName(path);
        QVERIFY(static_cast<size_t>(encoded.size()) < sizeof(address.sun_path));
        std::memcpy(address.sun_path, encoded.constData(), static_cast<size_t>(encoded.size()) + 1);
        QCOMPARE(::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);
        ::close(fd);
        QVERIFY(QFileInfo::exists(path));

        const QString discovery = QDir(DeckLink::deckFolder()).filePath(
            QFileInfo(path).completeBaseName() + QStringLiteral(".json"));
        QFile stale(discovery);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.write("{\"protocol\":1,\"path\":\"/nowhere\",\"pid\":1,\"profile\":\"\"}");
        stale.close();

        QVERIFY2(m_link->setEnabled(true), qPrintable(m_link->lastError()));
        Client client(path);
        QVERIFY(client.hello());
        QFile fresh(discovery);
        QVERIFY(fresh.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(fresh.readAll()).object().value(QStringLiteral("path")).toString(), path);
#endif
    }

    void aPathTooLongSaysSoAndMakesNothing()
    {
#if defined(Q_OS_WIN)
        QSKIP("A pipe's name doesn't depend on the home folder.");
#else
        // Named, never made, and past any limit a socket can have.
        const QString deep = QDir(m_home->path()).filePath(QString(120, QLatin1Char('d')));
        useHome(deep);
        QVERIFY(QFile::encodeName(m_link->socketPath()).size() > 103);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Deck link not open")));
        QVERIFY(!m_link->setEnabled(true));
        QVERIFY(m_link->lastError().contains(QStringLiteral("too long")));
        QVERIFY(!QFileInfo::exists(deep));
#endif
    }

    void aPathAtTheLimitOpens()
    {
#if defined(Q_OS_WIN)
        QSKIP("A pipe's name doesn't depend on the home folder.");
#else
        const qsizetype now = QFile::encodeName(m_link->socketPath()).size();
        useHome(QDir(m_home->path()).filePath(QString(103 - now - 1, QLatin1Char('d'))));
        QCOMPARE(QFile::encodeName(m_link->socketPath()).size(), 103);
        QVERIFY2(m_link->setEnabled(true), qPrintable(m_link->lastError()));
        Client client(m_link->socketPath());
        QVERIFY(client.hello());
#endif
    }

    // ---- the Stream Deck+ goes to Elgato's plugin ----

    void anElgatoHelloIsReportedUntilTheLastGoes()
    {
        QVERIFY(m_link->setEnabled(true));
        QSignalSpy elgato(m_link.get(), &DeckLink::elgatoConnectionChanged);
        QSignalSpy plugins(m_link.get(), &DeckLink::pluginsChanged);

        Client streamController(m_link->socketPath());
        QVERIFY(streamController.hello("streamcontroller"));
        QCOMPARE(elgato.size(), 0);
        QCOMPARE(plugins.size(), 1);

        Client first(m_link->socketPath());
        QVERIFY(first.hello("elgato"));
        QCOMPARE(elgato.size(), 1);
        QCOMPARE(elgato.at(0).at(0).toBool(), true);

        Client second(m_link->socketPath());
        QVERIFY(second.hello("elgato"));
        QCOMPARE(elgato.size(), 1);
        QCOMPARE(m_link->connectedPlugins(),
                 (QList<QString>{QStringLiteral("elgato"), QStringLiteral("elgato"), QStringLiteral("streamcontroller")}));

        first.socket->abort();
        QVERIFY(QTest::qWaitFor([this] { return m_link->connectedPlugins().size() == 2; }, kWaitMs));
        QCOMPARE(elgato.size(), 1);

        second.socket->abort();
        QVERIFY(elgato.size() == 2 || elgato.wait(kWaitMs));
        QCOMPARE(elgato.at(1).at(0).toBool(), false);
        QCOMPARE(m_link->connectedPlugins(), QList<QString>{QStringLiteral("streamcontroller")});
    }

    void anElgatoPluginTakesTheStreamDeckAndGivesItBack()
    {
        const auto bus = std::make_shared<FakeBus>();
        bus->present.insert(UsbPresence::key(kDeckVid, kDeckPid));
        DeviceInput::setLayerFactoryForTest([bus] { return std::make_unique<FakeLayer>(bus); });
        ControllerHost host(m_dispatcher.get(), m_bindings.get());
        connect(m_link.get(), &DeckLink::elgatoConnectionChanged, &host, &ControllerHost::setStreamDeckClaimedByPlugin);
        QVERIFY(QTest::qWaitFor([&host] { return host.deviceConnected(DeviceId::StreamDeckPlus); }, kWaitMs));

        QVERIFY(m_link->setEnabled(true));
        {
            Client other(m_link->socketPath());
            QVERIFY(other.hello("streamcontroller"));
            QVERIFY(sentinel(other));
            QVERIFY(host.deviceConnected(DeviceId::StreamDeckPlus));
        }

        Client client(m_link->socketPath());
        QVERIFY(client.hello("elgato"));
        QVERIFY(QTest::qWaitFor([&host] { return !host.deviceConnected(DeviceId::StreamDeckPlus); }, kWaitMs));
        QCOMPARE(host.deviceReason(DeviceId::StreamDeckPlus), QStringLiteral("A deck plugin is using it."));
        {
            const QMutexLocker locker(&bus->lock);
            QCOMPARE(bus->closes, 1);
        }

        client.socket->abort();
        QVERIFY(QTest::qWaitFor([&host] { return host.deviceConnected(DeviceId::StreamDeckPlus); }, kWaitMs));
    }
};

QTEST_MAIN(TestDeckLink)
#include "tst_deck_link.moc"
