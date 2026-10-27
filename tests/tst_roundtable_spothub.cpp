// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_roundtable_spothub.cpp  (NereusSDR)
// =================================================================
//
// Roundtable plan Task 12 (R-RT-14 desktop sheet, R-RT-16 Spot Hub tab and
// first run; design 11.2, C15, D25, U5):
//   - the Spot Hub's Roundtable tab comes after PSK Reporter, only where a
//     RoundtableModel is given;
//   - each Core-side control writes its Roundtable/* key through the model
//     and shows the Core's value when another device changes it;
//   - Hide my station, Unmute and the first-run buttons call the model's
//     actions and never write Roundtable/Hidden, MuteList or
//     FirstRunAnswer themselves;
//   - Open Archive asks for the sign-in link and opens it; Delete my
//     archive asks only after its confirm;
//   - this computer's two choices are written here;
//   - the first-run sheet's words are the design's, the switch moves the
//     main button, either button answers, and another device's answer
//     gives a one-time note;
//   - against a fake Roundtable server on 127.0.0.1 (as in
//     tst_roundtable_client): nothing about the station is sent before Go
//     live, and hello then presence after it.
//
// Known gap (Task 11): the Core's client does not pick up a live
// Roundtable/Callsign write; the override test checks that the key is
// written and shown, not that the server hears it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 12).
// =================================================================

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableState.h"
#include "core/roundtable/RoundtableWire.h"
#include "core/security/StationIdentity.h"
#include "core/session/IStationLink.h"
#include "gui/SpotHubDialog.h"
#include "gui/roundtable/RoundtableFirstRunDialog.h"
#include "models/RoundtableModel.h"
#include "models/SpotTableModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest/QtTest>

#include <memory>
#include <vector>

using namespace NereusSDR;
namespace Wire = NereusSDR::RoundtableWire;

namespace {

const QString kSuite = QStringLiteral(NEREUS_ROUNDTABLE_CONFORMANCE_DIR);
constexpr qint64 kWallStart = 1800000000000LL;
constexpr qint64 kMonoStart = 5000000;
constexpr int kWaitMs = 5000;

// The keys only Roundtable's actions change (Task 11's settings proxy
// refuses a device's write of them).
const QStringList kActionKeys{QStringLiteral("Roundtable/FirstRunAnswer"),
                              QStringLiteral("Roundtable/Hidden"),
                              QStringLiteral("Roundtable/MuteList"),
                              QStringLiteral("Roundtable/MentionsSeenMs")};

QJsonObject fixtureWire(const QString& name)
{
    QFile file(kSuite + QStringLiteral("/control/") + name + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("wire")).toObject();
}

// One connection the fake server accepted (tst_roundtable_client's, cut
// down to the sign-in).
struct FakeConnection {
    QWebSocket* socket = nullptr;
    QString path;
    QList<QJsonObject> frames;
    QByteArray nonce;
    bool allFramesValid = true;

    QStringList types() const
    {
        QStringList out;
        for (const QJsonObject& frame : frames) {
            out.append(frame.value(QStringLiteral("type")).toString());
        }
        return out;
    }

    void send(const QJsonObject& message) const
    {
        socket->sendTextMessage(
            QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }
};

class FakeServer {
public:
    FakeServer()
        : m_server(QStringLiteral("fake-roundtable"), QWebSocketServer::NonSecureMode)
    {
        QObject::connect(&m_server, &QWebSocketServer::newConnection, &m_context,
                         [this] { accept(); });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QUrl stationUrl() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/station").arg(m_server.serverPort()));
    }

    std::vector<std::unique_ptr<FakeConnection>> connections;

    int count(const QString& path) const
    {
        int n = 0;
        for (const auto& c : connections) {
            if (c->path == path) {
                ++n;
            }
        }
        return n;
    }

    FakeConnection* last(const QString& path) const
    {
        for (auto it = connections.rbegin(); it != connections.rend(); ++it) {
            if ((*it)->path == path) {
                return it->get();
            }
        }
        return nullptr;
    }

private:
    void accept()
    {
        while (m_server.hasPendingConnections()) {
            QWebSocket* socket = m_server.nextPendingConnection();
            socket->setParent(&m_context);
            auto connection = std::make_unique<FakeConnection>();
            FakeConnection* c = connection.get();
            c->socket = socket;
            c->path = socket->requestUrl().path();
            connections.push_back(std::move(connection));
            QObject::connect(socket, &QWebSocket::textMessageReceived, &m_context,
                             [c](const QString& text) { received(c, text); });
            if (c->path == QLatin1String("/v1/station")) {
                c->nonce.resize(32);
                QRandomGenerator::global()->fillRange(
                    reinterpret_cast<quint32*>(c->nonce.data()), 8);
                c->send(QJsonObject{{QStringLiteral("type"), QStringLiteral("challenge")},
                                    {QStringLiteral("nonce"), Wire::toBase64Url(c->nonce)}});
            }
        }
    }

    static void received(FakeConnection* c, const QString& text)
    {
        const bool station = c->path == QLatin1String("/v1/station");
        const Wire::Decoded decoded = Wire::decode(
            text, station ? Wire::Direction::StationToServer : Wire::Direction::LiveToServer);
        if (!decoded.ok) {
            c->allFramesValid = false;
            return;
        }
        c->frames.append(decoded.message);
        if (decoded.type == QLatin1String("hello")) {
            bool ok = false;
            const QByteArray spki = Wire::fromBase64Url(
                decoded.message.value(QStringLiteral("spki")).toString(), &ok);
            QJsonObject welcome = fixtureWire(QStringLiteral("server-station-welcome-core"));
            welcome.insert(QStringLiteral("station"), Wire::roundtableId(spki));
            c->send(welcome);
        }
    }

    QWebSocketServer m_server;
    QObject m_context;  // last: the sockets go first
};

// A window's link to a Core that offers Roundtable, recording the verbs.
class RecordingLink : public IStationLink {
public:
    QStringList calls;
    CommandOutcome answer{true, QString()};

    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }

    bool roundtableAvailable() const override { return true; }
    CommandOutcome requestRoundtableSetHidden(bool hidden) override
    {
        return note(QStringLiteral("setHidden %1").arg(hidden));
    }
    CommandOutcome requestRoundtableAnswerFirstRun(bool live) override
    {
        return note(QStringLiteral("answerFirstRun %1").arg(live));
    }
    CommandOutcome requestRoundtableUnmute(const QString& callsign) override
    {
        return note(QStringLiteral("unmute ") + callsign);
    }
    CommandOutcome requestRoundtableWebSignIn() override
    {
        return note(QStringLiteral("webSignIn"));
    }
    CommandOutcome requestRoundtableDeleteArchive(const QString& confirm) override
    {
        return note(QStringLiteral("deleteArchive ") + confirm);
    }

private:
    CommandOutcome note(const QString& call)
    {
        calls.append(call);
        return answer;
    }
};

// A window's Roundtable: the Core's `roundtable` object and the link.
struct Window {
    RecordingLink link;
    RoundtableState state;
    RoundtableModel model;

    Window() { model.attachStation(&link, &state); }
};

// The Spot List's table (the Spot List tab connects to it); made in
// initTestCase.
std::unique_ptr<SpotTableModel> g_table;

std::unique_ptr<SpotHubDialog> makeHub(RoundtableModel* model)
{
    return std::make_unique<SpotHubDialog>(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                           nullptr, nullptr, g_table.get(), nullptr, nullptr,
                                           model);
}

QString settingOf(const QString& key)
{
    return AppSettings::instance().value(key).toString();
}

QStringList labelTexts(const QWidget* root)
{
    QStringList texts;
    for (const QLabel* label : root->findChildren<QLabel*>()) {
        texts.append(label->text());
    }
    return texts;
}

bool anyContains(const QStringList& texts, const QString& needle)
{
    for (const QString& text : texts) {
        if (text.contains(needle)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TstRoundtableSpotHub : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { g_table = std::make_unique<SpotTableModel>(); }
    void cleanupTestCase() { g_table.reset(); }
    void init();

    void tabComesAfterPskReporterWithAModel();
    void coreControlsWriteTheirKeysThroughTheModel();
    void overridesAreCheckedThenWrittenAndShown();
    void showsTheCoresValueWhenAnotherDeviceChangesIt();
    void hideGoesThroughTheModelAndFollowsTheCore();
    void muteListShowsTheCoresAndUnmuteAsksTheCore();
    void archiveAsksForTheLinkAndOpensIt();
    void deleteAsksOnlyAfterTheConfirm();
    void thisComputersChoicesAreWrittenHere();
    void statusLineAndStartFollowTheLink();
    void openingTheTabSaysSo();

    void firstRunWordsAreTheDesigns();
    void switchMovesTheMainButtonAndEitherButtonAnswers();
    void refusedAnswerShowsTheReason();
    void anotherDevicesAnswerGivesTheNoteOnce();
    void answerFromAnotherDeviceWhileAskingTurnsIntoTheNote();
    void goLiveSendsNothingBeforeTheAnswerAndPresenceAfter();
};

void TstRoundtableSpotHub::init()
{
    auto& settings = AppSettings::instance();
    for (const QString& key : settings.allKeys()) {
        if (key.startsWith(QLatin1String("Roundtable/"))) {
            settings.remove(key);
        }
    }
    settings.setValue(QStringLiteral("User/Callsign"), QStringLiteral("KG4VCF"));
    settings.setValue(QStringLiteral("User/GridSquare"), QStringLiteral("EM73tu"));
}

void TstRoundtableSpotHub::tabComesAfterPskReporterWithAModel()
{
    Window w;
    auto hub = makeHub(&w.model);
    auto* tabs = hub->findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 11);
    int psk = -1;
    for (int i = 0; i < tabs->count(); ++i) {
        if (tabs->tabText(i) == QLatin1String("PSK Reporter")) {
            psk = i;
        }
    }
    QVERIFY(psk >= 0);
    QCOMPARE(tabs->tabText(psk + 1), QStringLiteral("Roundtable"));

    auto without = makeHub(nullptr);
    QCOMPARE(without->findChild<QTabWidget*>()->count(), 10);
    QVERIFY(without->findChild<QWidget*>(QStringLiteral("roundtableTab")) == nullptr);
}

void TstRoundtableSpotHub::coreControlsWriteTheirKeysThroughTheModel()
{
    Window w;
    auto hub = makeHub(&w.model);
    QSignalSpy changed(&w.model, &RoundtableModel::settingChanged);

    auto* autoStart = hub->findChild<QPushButton*>(QStringLiteral("roundtableAutoStartBtn"));
    QVERIFY(autoStart != nullptr);
    QVERIFY(autoStart->isChecked());  // default "True"
    QCOMPARE(autoStart->text(), QStringLiteral("Auto-Start: ON"));
    autoStart->click();
    QCOMPARE(settingOf(QStringLiteral("Roundtable/AutoStart")), QStringLiteral("False"));
    QCOMPARE(autoStart->text(), QStringLiteral("Auto-Start: OFF"));

    auto* away = hub->findChild<QComboBox*>(QStringLiteral("roundtableAwayCombo"));
    QVERIFY(away != nullptr);
    QCOMPARE(away->currentText(), QStringLiteral("Keep reporting, marked AWAY"));
    away->setCurrentIndex(1);
    QCOMPARE(away->currentText(), QStringLiteral("Stop reporting"));
    QCOMPARE(settingOf(QStringLiteral("Roundtable/AwayChoice")), QStringLiteral("Stop"));

    auto* highlight = hub->findChild<QCheckBox*>(QStringLiteral("roundtableHighlightChk"));
    QVERIFY(highlight != nullptr);
    QVERIFY(highlight->isChecked());  // default on (C15)
    QCOMPARE(highlight->text(), QStringLiteral("Highlight messages that name my callsign"));
    highlight->click();
    QCOMPARE(settingOf(QStringLiteral("Roundtable/HighlightMentions")), QStringLiteral("False"));

    QVERIFY(changed.count() >= 3);
    for (const QString& key : kActionKeys) {
        QVERIFY2(!AppSettings::instance().contains(key), qPrintable(key));
    }
}

void TstRoundtableSpotHub::overridesAreCheckedThenWrittenAndShown()
{
    Window w;
    auto hub = makeHub(&w.model);
    auto* call = hub->findChild<QLineEdit*>(QStringLiteral("roundtableCallEdit"));
    auto* grid = hub->findChild<QLineEdit*>(QStringLiteral("roundtableGridEdit"));
    auto* notice = hub->findChild<QLabel*>(QStringLiteral("roundtableStationNotice"));
    QVERIFY(call != nullptr && grid != nullptr && notice != nullptr);
    QCOMPARE(call->text(), QString());
    QCOMPARE(call->placeholderText(), QStringLiteral("Using KG4VCF"));

    call->setText(QStringLiteral("not a call"));
    emit call->editingFinished();
    QVERIFY(!AppSettings::instance().contains(QStringLiteral("Roundtable/Callsign")));
    QVERIFY(!notice->isHidden());
    QVERIFY(notice->text().contains(QStringLiteral("is not a callsign")));

    // The key is written and shown; the Core's client does not pick it up
    // live (Task 11's known gap), which this test does not claim.
    call->setText(QStringLiteral("w1aw"));
    emit call->editingFinished();
    QCOMPARE(settingOf(QStringLiteral("Roundtable/Callsign")), QStringLiteral("W1AW"));
    QCOMPARE(call->text(), QStringLiteral("W1AW"));
    QVERIFY(notice->isHidden());

    grid->setText(QStringLiteral("FN4"));
    emit grid->editingFinished();
    QVERIFY(!AppSettings::instance().contains(QStringLiteral("Roundtable/GridSquare")));
    QVERIFY(!notice->isHidden());
    grid->setText(QStringLiteral("FN42"));
    emit grid->editingFinished();
    QCOMPARE(settingOf(QStringLiteral("Roundtable/GridSquare")), QStringLiteral("FN42"));

    // Emptied: back to the Settings tab's.
    call->clear();
    emit call->editingFinished();
    QCOMPARE(settingOf(QStringLiteral("Roundtable/Callsign")), QString());
}

void TstRoundtableSpotHub::showsTheCoresValueWhenAnotherDeviceChangesIt()
{
    Window w;
    auto hub = makeHub(&w.model);
    auto* autoStart = hub->findChild<QPushButton*>(QStringLiteral("roundtableAutoStartBtn"));
    auto* away = hub->findChild<QComboBox*>(QStringLiteral("roundtableAwayCombo"));
    auto* highlight = hub->findChild<QCheckBox*>(QStringLiteral("roundtableHighlightChk"));
    auto* call = hub->findChild<QLineEdit*>(QStringLiteral("roundtableCallEdit"));
    auto* grid = hub->findChild<QLineEdit*>(QStringLiteral("roundtableGridEdit"));

    // The phone changed the Core's settings: the window's settings hold the
    // Core's values and RadioModel says so (reportStationSettingChanged).
    auto& settings = AppSettings::instance();
    const auto coreSets = [&](const QString& key, const QString& value) {
        settings.setValue(key, value);
        w.model.noteSettingChanged(key);
    };
    coreSets(QStringLiteral("Roundtable/AutoStart"), QStringLiteral("False"));
    coreSets(QStringLiteral("Roundtable/AwayChoice"), QStringLiteral("Stop"));
    coreSets(QStringLiteral("Roundtable/HighlightMentions"), QStringLiteral("False"));
    coreSets(QStringLiteral("Roundtable/Callsign"), QStringLiteral("K4ABC"));
    coreSets(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("FN42"));
    QVERIFY(!autoStart->isChecked());
    QCOMPARE(autoStart->text(), QStringLiteral("Auto-Start: OFF"));
    QCOMPARE(away->currentText(), QStringLiteral("Stop reporting"));
    QVERIFY(!highlight->isChecked());
    QCOMPARE(call->text(), QStringLiteral("K4ABC"));
    QCOMPARE(grid->text(), QStringLiteral("FN42"));

    // Showing the Core's value writes nothing back.
    QSignalSpy changed(&w.model, &RoundtableModel::settingChanged);
    w.model.noteSettingChanged(QString());  // a whole snapshot
    QCOMPARE(changed.count(), 1);
    QCOMPARE(settingOf(QStringLiteral("Roundtable/AwayChoice")), QStringLiteral("Stop"));

    // A key outside Roundtable/ is not the tab's.
    w.model.noteSettingChanged(QStringLiteral("User/Callsign"));
    QCOMPARE(changed.count(), 1);
}

void TstRoundtableSpotHub::hideGoesThroughTheModelAndFollowsTheCore()
{
    Window w;
    QVERIFY(w.state.applyStationValue("firstRunAnswered", true));
    auto hub = makeHub(&w.model);
    auto* hide = hub->findChild<QCheckBox*>(QStringLiteral("roundtableHideChk"));
    QVERIFY(hide != nullptr);
    QCOMPARE(hide->text(), QStringLiteral("Hide my station from Roundtable"));
    QVERIFY(!hide->isChecked());

    hide->click();
    QCOMPARE(w.link.calls, QStringList{QStringLiteral("setHidden 1")});
    QVERIFY(!AppSettings::instance().contains(QStringLiteral("Roundtable/Hidden")));

    // The Core's answer.
    QVERIFY(w.state.applyStationValue("hidden", true));
    QVERIFY(hide->isChecked());
    QVERIFY(w.state.applyStationValue("hidden", false));
    QVERIFY(!hide->isChecked());

    // Refused: the reason, and the box back to what the Core holds.
    w.link.answer = {false, QStringLiteral("The Core has no Roundtable connection right now.")};
    hide->click();
    auto* notice = hub->findChild<QLabel*>(QStringLiteral("roundtableStationNotice"));
    QVERIFY(!notice->isHidden());
    QCOMPARE(notice->text(), QStringLiteral("The Core has no Roundtable connection right now."));
    QVERIFY(!hide->isChecked());
    for (const QString& key : kActionKeys) {
        QVERIFY2(!AppSettings::instance().contains(key), qPrintable(key));
    }
}

void TstRoundtableSpotHub::muteListShowsTheCoresAndUnmuteAsksTheCore()
{
    Window w;
    auto hub = makeHub(&w.model);
    QVERIFY(hub->findChild<QLabel*>(QStringLiteral("roundtableMuteEmpty")) != nullptr);
    QCOMPARE(hub->findChild<QLabel*>(QStringLiteral("roundtableMuteEmpty"))->text(),
             QStringLiteral("Nobody muted. Mute from any message's ⋯ menu."));

    // The Core's list, as the window's settings share it.
    AppSettings::instance().setValue(QStringLiteral("Roundtable/MuteList"),
                                     QStringLiteral("N9JNK,KD9QRT"));
    w.model.noteSettingChanged(QStringLiteral("Roundtable/MuteList"));
    QVERIFY(hub->findChild<QLabel*>(QStringLiteral("roundtableMuteEmpty")) == nullptr);
    QStringList calls;
    for (const QLabel* label : hub->findChildren<QLabel*>(QStringLiteral("roundtableMuteCall"))) {
        calls.append(label->text());
    }
    QCOMPARE(calls, (QStringList{QStringLiteral("N9JNK"), QStringLiteral("KD9QRT")}));

    auto* unmute = hub->findChild<QPushButton*>(QStringLiteral("roundtableUnmute_N9JNK"));
    QVERIFY(unmute != nullptr);
    unmute->click();
    QCOMPARE(w.link.calls, QStringList{QStringLiteral("unmute N9JNK")});
    // The list changes when the Core's does, not from the click.
    QCOMPARE(settingOf(QStringLiteral("Roundtable/MuteList")), QStringLiteral("N9JNK,KD9QRT"));

    w.link.answer = {false, QStringLiteral("Not muted.")};
    hub->findChild<QPushButton*>(QStringLiteral("roundtableUnmute_KD9QRT"))->click();
    auto* notice = hub->findChild<QLabel*>(QStringLiteral("roundtableMuteNotice"));
    QVERIFY(!notice->isHidden());
    QCOMPARE(notice->text(), QStringLiteral("Not muted."));
}

void TstRoundtableSpotHub::archiveAsksForTheLinkAndOpensIt()
{
    Window w;
    auto hub = makeHub(&w.model);
    QList<QUrl> opened;
    hub->setRoundtableUrlOpener([&opened](const QUrl& url) {
        opened.append(url);
        return true;
    });

    // A link nobody here asked for is not opened.
    w.model.applyWebLink(QUrl(QStringLiteral("https://roundtable.nereussdr.com/k/unasked")));
    QVERIFY(opened.isEmpty());

    auto* archive = hub->findChild<QPushButton*>(QStringLiteral("roundtableArchiveBtn"));
    QVERIFY(archive != nullptr);
    QCOMPARE(archive->text(), QStringLiteral("Open Archive"));
    archive->click();
    QCOMPARE(w.link.calls, QStringList{QStringLiteral("webSignIn")});
    QVERIFY(opened.isEmpty());
    const QUrl link(QStringLiteral("https://roundtable.nereussdr.com/k/one-time"));
    w.model.applyWebLink(link);
    QCOMPARE(opened, QList<QUrl>{link});
    // Once.
    w.model.applyWebLink(link);
    QCOMPARE(opened.size(), 1);

    // Refused: the reason.
    w.link.answer = {false, QStringLiteral("The Core has no Roundtable connection right now.")};
    archive->click();
    auto* notice = hub->findChild<QLabel*>(QStringLiteral("roundtableArchiveNotice"));
    QCOMPARE(notice->text(), QStringLiteral("The Core has no Roundtable connection right now."));

    // The public page.
    hub->findChild<QPushButton*>(QStringLiteral("roundtablePublicPageBtn"))->click();
    QCOMPARE(opened.last(), QUrl(QStringLiteral("https://roundtable.nereussdr.com/")));
}

void TstRoundtableSpotHub::deleteAsksOnlyAfterTheConfirm()
{
    Window w;
    auto hub = makeHub(&w.model);
    auto* del = hub->findChild<QPushButton*>(QStringLiteral("roundtableDeleteBtn"));
    auto* confirm = hub->findChild<QFrame*>(QStringLiteral("roundtableDeleteConfirm"));
    QVERIFY(del != nullptr && confirm != nullptr);
    QCOMPARE(del->text(), QStringLiteral("Delete My Archive…"));
    QVERIFY(confirm->isHidden());

    del->click();
    QVERIFY(w.link.calls.isEmpty());
    QVERIFY(!confirm->isHidden());
    const QString text =
        hub->findChild<QLabel*>(QStringLiteral("roundtableDeleteConfirmText"))->text();
    QVERIFY(text.contains(QStringLiteral("This can't be undone.")));
    QVERIFY(text.contains(QStringLiteral("Backup copies age out within 7 days.")));

    hub->findChild<QPushButton*>(QStringLiteral("roundtableDeleteCancelBtn"))->click();
    QVERIFY(confirm->isHidden());
    QVERIFY(w.link.calls.isEmpty());

    del->click();
    hub->findChild<QPushButton*>(QStringLiteral("roundtableDeleteEverythingBtn"))->click();
    QCOMPARE(w.link.calls, QStringList{QStringLiteral("deleteArchive delete my archive")});
    QVERIFY(confirm->isHidden());
}

void TstRoundtableSpotHub::thisComputersChoicesAreWrittenHere()
{
    AppSettings::instance().setValue(QStringLiteral("Roundtable/PanelOpensOn"),
                                     QStringLiteral("Messages"));
    Window w;
    auto hub = makeHub(&w.model);
    auto* earlier = hub->findChild<QComboBox*>(QStringLiteral("roundtableEarlierTurnsCombo"));
    auto* opensOn = hub->findChild<QComboBox*>(QStringLiteral("roundtablePanelOpensOnCombo"));
    QVERIFY(earlier != nullptr && opensOn != nullptr);
    QCOMPARE(earlier->currentData().toString(), QStringLiteral("All"));
    QCOMPARE(opensOn->currentData().toString(), QStringLiteral("Messages"));

    earlier->setCurrentIndex(earlier->findData(QStringLiteral("10")));
    QCOMPARE(settingOf(QStringLiteral("Roundtable/EarlierTurnsShown")), QStringLiteral("10"));
    opensOn->setCurrentIndex(opensOn->findData(QStringLiteral("Readings")));
    QCOMPARE(settingOf(QStringLiteral("Roundtable/PanelOpensOn")), QStringLiteral("Readings"));
    QVERIFY(w.link.calls.isEmpty());
}

void TstRoundtableSpotHub::statusLineAndStartFollowTheLink()
{
    Window w;
    auto hub = makeHub(&w.model);
    auto* status = hub->findChild<QLabel*>(QStringLiteral("roundtableStatusLabel"));
    auto* start = hub->findChild<QPushButton*>(QStringLiteral("roundtableStartBtn"));
    QVERIFY(status != nullptr && start != nullptr);
    QVERIFY(status->text().contains(QStringLiteral("Stopped")));
    QCOMPARE(start->text(), QStringLiteral("Start"));
    QSignalSpy starts(hub.get(), &SpotHubDialog::roundtableStartRequested);
    QSignalSpy stops(hub.get(), &SpotHubDialog::roundtableStopRequested);
    start->click();
    QCOMPARE(starts.count(), 1);

    QVERIFY(w.state.applyStationValue(
        "linkState", static_cast<int>(RoundtableModel::LinkState::Hidden)));
    QVERIFY(status->text().contains(QStringLiteral("Reading only, until someone chooses Go live")));
    QCOMPARE(start->text(), QStringLiteral("Stop"));

    QVERIFY(w.state.applyStationValue("firstRunAnswered", true));
    QVERIFY(w.state.applyStationValue("hidden", true));
    QVERIFY(status->text().contains(QStringLiteral("Reading only, your station is hidden")));
    auto* preview = hub->findChild<QLabel*>(QStringLiteral("roundtablePreviewText"));
    QVERIFY(preview->text().contains(QStringLiteral("You don't appear")));

    QVERIFY(w.state.applyStationValue("hidden", false));
    QVERIFY(w.state.applyStationValue(
        "linkState", static_cast<int>(RoundtableModel::LinkState::Live)));
    QVERIFY(status->text().contains(QStringLiteral("Connected as KG4VCF")));
    start->click();
    QCOMPARE(stops.count(), 1);

    QVERIFY(w.state.applyStationValue(
        "linkState", static_cast<int>(RoundtableModel::LinkState::Offline)));
    QVERIFY(status->text().contains(QStringLiteral("Can't reach the server, trying again")));

    // A Core without Roundtable: the reason, and the actions disabled.
    hub->setStationRoundtableAvailable(false, IStationLink::roundtableUnavailableReason());
    QVERIFY(status->text().contains(QStringLiteral("This Core cannot share Roundtable")));
    QVERIFY(!start->isEnabled());
    QVERIFY(!hub->findChild<QCheckBox*>(QStringLiteral("roundtableHideChk"))->isEnabled());
    QVERIFY(!hub->findChild<QComboBox*>(QStringLiteral("roundtableAwayCombo"))->isEnabled());
    QVERIFY(hub->findChild<QComboBox*>(QStringLiteral("roundtableEarlierTurnsCombo"))->isEnabled());
    hub->setStationRoundtableAvailable(true, QString());
    QVERIFY(start->isEnabled());
}

void TstRoundtableSpotHub::openingTheTabSaysSo()
{
    Window w;
    auto hub = makeHub(&w.model);
    QSignalSpy opened(hub.get(), &SpotHubDialog::roundtableOpened);
    auto* tabs = hub->findChild<QTabWidget*>();
    tabs->setCurrentIndex(0);
    QCOMPARE(opened.count(), 0);
    for (int i = 0; i < tabs->count(); ++i) {
        if (tabs->tabText(i) == QLatin1String("Roundtable")) {
            tabs->setCurrentIndex(i);
        }
    }
    QCOMPARE(opened.count(), 1);
}

void TstRoundtableSpotHub::firstRunWordsAreTheDesigns()
{
    Window w;
    QVERIFY(RoundtableFirstRunDialog::shouldShow(&w.model));
    RoundtableFirstRunDialog dialog(&w.model);
    QCOMPARE(dialog.mode(), RoundtableFirstRunDialog::Mode::Question);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Roundtable is public"));
    const QStringList texts = labelTexts(&dialog);
    QVERIFY(texts.contains(QStringLiteral("Roundtable is public")));
    QVERIFY(texts.contains(QStringLiteral(
        "Roundtable shows who hears whom, live, to everyone on it and on its web page. "
        "With your station on, they see:")));
    QVERIFY(anyContains(texts, QStringLiteral("Your callsign <b")));
    QVERIFY(anyContains(texts, QStringLiteral(">KG4VCF</b> and grid <b")));
    QVERIFY(anyContains(texts, QStringLiteral(">EM73tu</b>")));
    QVERIFY(texts.contains(QStringLiteral(
        "Your radio's reading of each talker, and how far you are from them")));
    QVERIFY(texts.contains(QStringLiteral("Every station's reading of you while you talk")));
    QVERIFY(texts.contains(QStringLiteral("Messages you post on your frequency")));
    QVERIFY(texts.contains(QStringLiteral("Your archive of who heard you stays private to you.")));
    QVERIFY(texts.contains(QStringLiteral("Hide my station")));
    QVERIFY(texts.contains(QStringLiteral(
        "You still see everything. Your station and your readings are not sent. "
        "Others can still hear you, and may name you when you talk.")));
    QVERIFY(texts.contains(QStringLiteral(
        "Asked once for this station, on any of its devices. Change it any time in "
        "Roundtable settings.")));
    QCOMPARE(dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunStayHidden"))->text(),
             QStringLiteral("Stay hidden"));
    QCOMPARE(dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunGoLive"))->text(),
             QStringLiteral("Go live"));
    // Asking marks this computer as asked.
    QCOMPARE(settingOf(QString::fromLatin1(RoundtableFirstRunDialog::kSeenKey)),
             QStringLiteral("True"));
}

void TstRoundtableSpotHub::switchMovesTheMainButtonAndEitherButtonAnswers()
{
    {
        Window w;
        RoundtableFirstRunDialog dialog(&w.model);
        auto* live = dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunGoLive"));
        auto* stay = dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunStayHidden"));
        auto* sw = dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunHideSwitch"));
        QVERIFY(!dialog.hideSwitchOn());
        QCOMPARE(sw->text(), QStringLiteral("Off"));
        QVERIFY(live->isDefault());
        QVERIFY(!stay->isDefault());
        sw->click();
        QVERIFY(dialog.hideSwitchOn());
        QCOMPARE(sw->text(), QStringLiteral("On"));
        QVERIFY(stay->isDefault());
        QVERIFY(!live->isDefault());

        // Either button answers whatever the switch says.
        live->click();
        QCOMPARE(w.link.calls, QStringList{QStringLiteral("answerFirstRun 1")});
        QVERIFY(!live->isEnabled());
        // The Core's answer closes the sheet.
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        QVERIFY(w.state.applyStationValue("firstRunAnswered", true));
        QCOMPARE(accepted.count(), 1);
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Roundtable/FirstRunAnswer")));
    }
    {
        Window w;
        RoundtableFirstRunDialog dialog(&w.model);
        dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunStayHidden"))->click();
        QCOMPARE(w.link.calls, QStringList{QStringLiteral("answerFirstRun 0")});
    }
}

void TstRoundtableSpotHub::refusedAnswerShowsTheReason()
{
    Window w;
    w.link.answer = {false, QStringLiteral("The Core has no Roundtable connection right now.")};
    RoundtableFirstRunDialog dialog(&w.model);
    auto* live = dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunGoLive"));
    live->click();
    auto* error = dialog.findChild<QLabel*>(QStringLiteral("roundtableFirstRunError"));
    QVERIFY(!error->isHidden());
    QCOMPARE(error->text(), QStringLiteral("The Core has no Roundtable connection right now."));
    QVERIFY(live->isEnabled());
    QCOMPARE(dialog.mode(), RoundtableFirstRunDialog::Mode::Question);
}

void TstRoundtableSpotHub::anotherDevicesAnswerGivesTheNoteOnce()
{
    Window w;
    QVERIFY(w.state.applyStationValue("firstRunAnswered", true));
    QVERIFY(RoundtableFirstRunDialog::shouldShow(&w.model));
    {
        RoundtableFirstRunDialog dialog(&w.model);
        QCOMPARE(dialog.mode(), RoundtableFirstRunDialog::Mode::Note);
        const QStringList texts = labelTexts(&dialog);
        QVERIFY(anyContains(texts, QStringLiteral("Your station went live from another device.")));
        QVERIFY(texts.contains(QStringLiteral(
            "Asked once per station, so this computer doesn't ask again. Hide my station is "
            "in Roundtable settings.")));
    }
    QVERIFY(!RoundtableFirstRunDialog::shouldShow(&w.model));

    // Hidden on the other device.
    AppSettings::instance().remove(QString::fromLatin1(RoundtableFirstRunDialog::kSeenKey));
    QVERIFY(w.state.applyStationValue("hidden", true));
    RoundtableFirstRunDialog hidden(&w.model);
    QVERIFY(anyContains(labelTexts(&hidden),
                        QStringLiteral("Your station chose Stay hidden on another device.")));

    // No Roundtable: nothing to ask.
    RoundtableModel detached;
    QVERIFY(!RoundtableFirstRunDialog::shouldShow(&detached));
    QVERIFY(!RoundtableFirstRunDialog::shouldShow(nullptr));
}

void TstRoundtableSpotHub::answerFromAnotherDeviceWhileAskingTurnsIntoTheNote()
{
    Window w;
    RoundtableFirstRunDialog dialog(&w.model);
    QCOMPARE(dialog.mode(), RoundtableFirstRunDialog::Mode::Question);
    const int asking = dialog.sizeHint().height();
    QVERIFY(w.state.applyStationValue("firstRunAnswered", true));
    QCOMPARE(dialog.mode(), RoundtableFirstRunDialog::Mode::Note);
    QVERIFY(w.link.calls.isEmpty());
    // The note fits its own words, not the question's height.
    QVERIFY2(dialog.height() < asking / 2,
             qPrintable(QStringLiteral("%1 of %2").arg(dialog.height()).arg(asking)));

    // Opened as the note, it is as short.
    RoundtableFirstRunDialog note(&w.model);
    QCOMPARE(note.mode(), RoundtableFirstRunDialog::Mode::Note);
    QVERIFY2(note.height() < asking / 2,
             qPrintable(QStringLiteral("%1 of %2").arg(note.height()).arg(asking)));
}

void TstRoundtableSpotHub::goLiveSendsNothingBeforeTheAnswerAndPresenceAfter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
    QVERIFY(identity.isValid());
    FakeServer server;
    QVERIFY(server.listen());

    qint64 wall = kWallStart;
    qint64 mono = kMonoStart;
    RoundtableClient client;
    client.setClocksForTest([&wall] { return wall; }, [&mono] { return mono; });
    client.setServerUrl(server.stationUrl());
    client.setIdentity(&identity);
    client.setStationInfo(QStringLiteral("KG4VCF"), QStringLiteral("EM73tu"));
    RoundtableSliceState slice;
    slice.id = 0;
    slice.hz = 14212000;
    slice.mode = QStringLiteral("USB");
    slice.filterHz = 2700;
    client.setSlices({slice});

    // The Everything placement: the Core's own client, state and model.
    RoundtableState state;
    state.bindClient(&client, &AppSettings::instance());
    RoundtableModel model;
    model.attachLocal(&client, &state);
    QVERIFY(model.available());
    QVERIFY(!model.firstRunAnswered());

    client.start();
    QVERIFY(QTest::qWaitFor([&server] { return server.count(QStringLiteral("/v1/live")) >= 1; },
                            kWaitMs));
    FakeConnection* live = server.last(QStringLiteral("/v1/live"));
    QVERIFY(QTest::qWaitFor([live] { return !live->frames.isEmpty(); }, kWaitMs));
    QCOMPARE(live->types(), QStringList{QStringLiteral("watchBand")});

    RoundtableFirstRunDialog dialog(&model);
    QCOMPARE(dialog.mode(), RoundtableFirstRunDialog::Mode::Question);
    QTest::qWait(200);
    QCOMPARE(server.count(QStringLiteral("/v1/station")), 0);

    QSignalSpy accepted(&dialog, &QDialog::accepted);
    dialog.findChild<QPushButton*>(QStringLiteral("roundtableFirstRunGoLive"))->click();
    QCOMPARE(accepted.count(), 1);
    QVERIFY(model.firstRunAnswered());
    QVERIFY(!model.hidden());
    QCOMPARE(settingOf(QStringLiteral("Roundtable/FirstRunAnswer")), QStringLiteral("Live"));

    QVERIFY(QTest::qWaitFor(
        [&server] { return server.count(QStringLiteral("/v1/station")) >= 1; }, kWaitMs));
    FakeConnection* station = server.last(QStringLiteral("/v1/station"));
    QVERIFY(QTest::qWaitFor([station] { return station->frames.size() >= 2; }, kWaitMs));
    QCOMPARE(station->types().mid(0, 2),
             (QStringList{QStringLiteral("hello"), QStringLiteral("presence")}));
    QCOMPARE(station->frames.at(1).value(QStringLiteral("callsign")).toString(),
             QStringLiteral("KG4VCF"));
    QVERIFY(station->allFramesValid);
    QVERIFY(live->allFramesValid);
    client.stop();
}

QTEST_MAIN(TstRoundtableSpotHub)
#include "tst_roundtable_spothub.moc"
