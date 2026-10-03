// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_data_channel_linger_teardown.cpp  (NereusSDR)
// =================================================================
//
// A lingering close (DataChannelTransport's closeLink on an open channel)
// holds its peer on a context object that the application's teardown
// deletes. This test runs three applications, one after another, in one
// process, as the teardown is only reached by destroying one:
//
//   1. A connection closed while the teardown runs (from a post routine
//      that runs after the transport's own) still lingers on an object
//      that the same teardown deletes.
//   2. Once that application has gone, nothing is left; a second
//      application's lingering close has its own object again.
//   3. Once the second application has gone, nothing is left either.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>

#include <memory>

#include "core/session/DataChannelTransport.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"

#include "fakes/DataChannelPair.h"

using namespace NereusSDR;
using NereusSDR::Test::startDataChannelPair;
using NereusSDR::Test::waitFor;

namespace {

constexpr quint64 kStationCap = StationServer::kMaxIncomingMessageBytes;
constexpr quint64 kClientCap = StationClient::kMaxIncomingMessageBytes;

struct OpenPair {
    std::unique_ptr<DataChannelTransport> offerer = std::make_unique<DataChannelTransport>();
    std::unique_ptr<DataChannelTransport> answerer = std::make_unique<DataChannelTransport>();

    bool open()
    {
        if (!startDataChannelPair(offerer.get(), answerer.get(), kClientCap, kStationCap,
                                  QString(), QString())) {
            return false;
        }
        return waitFor([this] { return offerer->isOpen() && answerer->isOpen(); }, 15000);
    }
};

// The connection the first application's teardown closes.
std::unique_ptr<OpenPair> g_closedInTeardown;
bool g_teardownCloseRan = false;

// Registered before any lingering close, so it runs after the transport's
// own post routine (Qt runs the last one added first).
void closeDuringTeardown()
{
    if (!g_closedInTeardown) {
        return;
    }
    g_teardownCloseRan = true;
    g_closedInTeardown->answerer->closeLink(QStringLiteral("test teardown"));
    g_closedInTeardown->offerer->closeLink(QStringLiteral("test teardown"));
    g_closedInTeardown.reset();
}

} // namespace

class TstLingerFirstApplication : public QObject {
    Q_OBJECT

private slots:
    void aCloseDuringTheTeardownStillLingers()
    {
        qAddPostRoutine(&closeDuringTeardown);
        QVERIFY(!DataChannelTransport::lingerTargetExistsForTest());
        OpenPair pair;
        QVERIFY(pair.open());
        pair.answerer->closeLink(QStringLiteral("test"));
        QVERIFY(DataChannelTransport::lingerTargetExistsForTest());
        g_closedInTeardown = std::make_unique<OpenPair>();
        QVERIFY(g_closedInTeardown->open());
    }
};

class TstLingerSecondApplication : public QObject {
    Q_OBJECT

private slots:
    void theFirstTeardownLeftNothing()
    {
        QVERIFY(g_teardownCloseRan);
        QVERIFY(!DataChannelTransport::lingerTargetExistsForTest());
    }

    void aSecondApplicationLingersAgain()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        pair.answerer->closeLink(QStringLiteral("test"));
        QVERIFY(DataChannelTransport::lingerTargetExistsForTest());
    }
};

class TstLingerThirdApplication : public QObject {
    Q_OBJECT

private slots:
    void theSecondTeardownLeftNothing()
    {
        QVERIFY(!DataChannelTransport::lingerTargetExistsForTest());
    }
};

int main(int argc, char** argv)
{
    int status = 0;
    {
        QCoreApplication app(argc, argv);
        TstLingerFirstApplication first;
        status |= QTest::qExec(&first, argc, argv);
    }
    {
        QCoreApplication app(argc, argv);
        TstLingerSecondApplication second;
        status |= QTest::qExec(&second, argc, argv);
    }
    {
        QCoreApplication app(argc, argv);
        TstLingerThirdApplication third;
        status |= QTest::qExec(&third, argc, argv);
    }
    return status;
}

#include "tst_data_channel_linger_teardown.moc"
