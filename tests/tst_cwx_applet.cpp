// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_cwx_applet.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-44): the CWX applet on RadioModel's CWX sender. Send
// queues the text, Stop aborts, Key holds while pressed; M1 to M8 send
// their memory and a right-click edits it; Speed, Rpt and its delay write
// the settings; KB to CW sends as typed; the sent characters dim and the
// current one is marked from characterStarted. In the window: Tools > CWX,
// the status-bar label and CAT ZZKO show and hide the applet.
//
// Nothing here keys a radio: the connection is a fake with no socket.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, created (R-CW-44, R-CW-34 ZZKO host).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QCoreApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTextEdit>

#include "core/AppSettings.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/SliceOwnership.h"
#include "core/cat/CatService.h"
#include "core/cat/CatSession.h"
#include "core/session/RemoteTransmitClient.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/applets/CwxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class CwxAppletConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwxAppletConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

RadioInfo radioInfo(ProtocolVersion protocol, HPSDRHW board, int firmware)
{
    RadioInfo info;
    info.protocol = protocol;
    info.boardType = board;
    info.firmwareVersion = firmware;
    return info;
}

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// A G2 on a fake connection, slice A in CWU, Semi break-in (as the CAT CW
// tests set it up), so a CWX request passes the request gate.
struct Rig {
    RadioModel model;
    CwxAppletConnection conn;
    SliceModel* slice{nullptr};
    std::unique_ptr<CwxApplet> applet;

    explicit Rig(int wpm = 60)
    {
        AppSettings::instance().clear();
        model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 22));
        model.injectConnectionForTest(&conn);
        model.wireCwKeyerConnectionForTest();
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        slice = model.sliceById(model.addSlice());
        model.sliceOwnership()->setOwner(slice->sliceIndex(), SliceOwnership::stationDevice());
        slice->setDspMode(DSPMode::CWU);
        slice->setFrequency(14'050'000.0);
        model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        model.transmitModel().setCwxSpeed(wpm);
        applet = std::make_unique<CwxApplet>(&model);
        applet->resize(292, 240);
        applet->show();
        pump();
    }
    ~Rig()
    {
        model.cwxSender()->stop();
        applet.reset();
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    CwxSender* cwx() { return model.cwxSender(); }
    TransmitModel& tx() { return model.transmitModel(); }
    template <typename T> T* child(const char* name)
    {
        return applet->findChild<T*>(QString::fromLatin1(name));
    }
    QTextEdit* text() { return child<QTextEdit>("cwxText"); }
    QPushButton* button(const char* name) { return child<QPushButton>(name); }
};

// The marks the field shows: the current character's position, or -1.
int currentMark(QTextEdit* edit)
{
    for (const QTextEdit::ExtraSelection& mark : edit->extraSelections()) {
        if (mark.format.background().color() == QColor(QStringLiteral("#2f6146"))) {
            return mark.cursor.selectionStart();
        }
    }
    return -1;
}

// The sent range [start, end), or {-1, -1}.
QPair<int, int> sentMark(QTextEdit* edit)
{
    for (const QTextEdit::ExtraSelection& mark : edit->extraSelections()) {
        if (mark.format.background().style() == Qt::NoBrush
            && mark.format.foreground().color().alphaF() < 0.5) {
            return {mark.cursor.selectionStart(), mark.cursor.selectionEnd()};
        }
    }
    return {-1, -1};
}

} // namespace

class TestCwxApplet : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void sendQueuesTheTextAndStopAborts()
    {
        Rig rig;
        rig.text()->setPlainText(QStringLiteral("CQ TEST"));
        QTest::mouseClick(rig.button("cwxSend"), Qt::LeftButton);
        QVERIFY(rig.cwx()->isSending());
        QVERIFY(rig.cwx()->queuedCharacters() > 0);
        QTRY_VERIFY(rig.button("cwxSend")->isChecked());

        QTest::mouseClick(rig.button("cwxStop"), Qt::LeftButton);
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        QTRY_VERIFY(!rig.button("cwxSend")->isChecked());
        QCOMPARE(currentMark(rig.text()), -1);
    }

    void escapeInTheTextStops()
    {
        Rig rig;
        rig.text()->setPlainText(QStringLiteral("CQ TEST"));
        QTest::mouseClick(rig.button("cwxSend"), Qt::LeftButton);
        QVERIFY(rig.cwx()->isSending());
        QTest::keyClick(rig.text(), Qt::Key_Escape);
        QVERIFY(!rig.cwx()->isSending());
    }

    void keyHoldsWhilePressed()
    {
        Rig rig;
        QPushButton* key = rig.button("cwxKey");
        QTest::mousePress(key, Qt::LeftButton);
        QVERIFY(rig.cwx()->isKeyHeld());
        QTRY_VERIFY(key->isChecked());
        QTest::mouseRelease(key, Qt::LeftButton);
        QVERIFY(!rig.cwx()->isKeyHeld());
        QTRY_VERIFY(!key->isChecked());
    }

    void aMemoryButtonSendsItsMemory()
    {
        Rig rig;
        rig.tx().setCwxMemory3(QStringLiteral("TU 73"));
        pump();
        QPushButton* m3 = rig.button("cwxMemory3");
        QVERIFY(m3->toolTip().startsWith(QStringLiteral("TU 73")));
        QTest::mouseClick(m3, Qt::LeftButton);
        QVERIFY(rig.cwx()->isSending());
        // The memory's text is in the field, where it is marked as it goes.
        QCOMPARE(rig.text()->toPlainText(), QStringLiteral("TU 73"));
        rig.cwx()->stop();

        // An empty memory is refused with words.
        QSignalSpy refused(rig.applet.get(), &CwxApplet::requestRefused);
        QTest::mouseClick(rig.button("cwxMemory5"), Qt::LeftButton);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("Memory 5 is empty."));
    }

    void rightClickingAMemoryEditsIt()
    {
        Rig rig;
        rig.tx().setCwxMemory2(QStringLiteral("OLD"));
        QPushButton* m2 = rig.button("cwxMemory2");
        QCOMPARE(m2->contextMenuPolicy(), Qt::CustomContextMenu);
        emit m2->customContextMenuRequested(QPoint(4, 4));
        auto* editor = rig.applet->findChild<QInputDialog*>(QStringLiteral("cwxMemoryEditor"));
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->textValue(), QStringLiteral("OLD"));
        editor->setTextValue(QStringLiteral("CQ CQ DE KG4VCF K"));
        editor->accept();
        QCOMPARE(rig.tx().cwxMemory2(), QStringLiteral("CQ CQ DE KG4VCF K"));
        pump();
        QVERIFY(m2->toolTip().startsWith(QStringLiteral("CQ CQ DE KG4VCF K")));
    }

    void speedRepeatAndItsDelayWriteTheSettings()
    {
        Rig rig;
        auto* speed = rig.child<QSlider>("cwxSpeedSlider");
        auto* delay = rig.child<QSlider>("cwxRepeatDelaySlider");
        QCOMPARE(speed->minimum(), 5);
        QCOMPARE(speed->maximum(), 60);
        QCOMPARE(delay->minimum(), 1);
        QCOMPARE(delay->maximum(), 60);
        speed->setValue(30);
        QCOMPARE(rig.tx().cwxSpeed(), 30);
        QTest::mouseClick(rig.button("cwxRepeat"), Qt::LeftButton);
        QVERIFY(rig.tx().cwxRepeat());
        delay->setValue(9);
        QCOMPARE(rig.tx().cwxRepeatDelayS(), 9);

        // And the applet follows the settings.
        rig.tx().setCwxSpeed(41);
        rig.tx().setCwxRepeat(false);
        rig.tx().setCwxRepeatDelayS(12);
        QCOMPARE(speed->value(), 41);
        QVERIFY(!rig.button("cwxRepeat")->isChecked());
        QCOMPARE(delay->value(), 12);
    }

    void kbToCwSendsAsTyped()
    {
        // 5 WPM, so the typed character is still being sent when looked at.
        Rig rig(5);
        rig.text()->setPlainText(QStringLiteral("OLD "));
        QTest::mouseClick(rig.button("cwxKeyboard"), Qt::LeftButton);
        QVERIFY(rig.button("cwxKeyboard")->isChecked());
        // Send is greyed with the reason while typing sends.
        QVERIFY(!rig.button("cwxSend")->isEnabled());
        QVERIFY(!rig.button("cwxSend")->toolTip().isEmpty());
        QVERIFY(!rig.cwx()->isSending());

        QTextCursor end = rig.text()->textCursor();
        end.movePosition(QTextCursor::End);
        rig.text()->setTextCursor(end);
        QTest::keyClicks(rig.text(), QStringLiteral("K"));
        QVERIFY(rig.cwx()->isSending());
        // Only the typed character went, not the text already there: the
        // sender marks it where it was typed, with nothing sent before it.
        QTRY_COMPARE(currentMark(rig.text()), 4);
        QCOMPARE(sentMark(rig.text()), qMakePair(-1, -1));
    }

    void theSentCharactersDimAndTheCurrentOneIsMarked()
    {
        // 5 WPM: 'C' takes seconds, so the next characterStarted is ours.
        Rig rig(5);
        rig.text()->setPlainText(QStringLiteral("CQ TEST"));
        QTest::mouseClick(rig.button("cwxSend"), Qt::LeftButton);
        QTRY_COMPARE(currentMark(rig.text()), 0);
        QCOMPARE(sentMark(rig.text()), qMakePair(-1, -1));

        emit rig.cwx()->characterStarted(3);
        QCOMPARE(rig.applet->markedCurrentForTest(), 3);
        QCOMPARE(currentMark(rig.text()), 3);
        QCOMPARE(sentMark(rig.text()), qMakePair(0, 3));

        rig.cwx()->stop();
        QTRY_COMPARE(currentMark(rig.text()), -1);
        QCOMPARE(sentMark(rig.text()), qMakePair(-1, -1));
        // The text itself is unchanged by the marks.
        QCOMPARE(rig.text()->toPlainText(), QStringLiteral("CQ TEST"));
    }

    void aRefusedSendSaysWhy()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        rig.text()->setPlainText(QStringLiteral("CQ"));
        QSignalSpy refused(rig.applet.get(), &CwxApplet::requestRefused);
        QTest::mouseClick(rig.button("cwxSend"), Qt::LeftButton);
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(refused.count(), 1);
        QVERIFY(!refused.at(0).at(0).toString().isEmpty());
        QVERIFY(!rig.button("cwxSend")->isChecked());
    }

    void remoteSenderDistinguishesUnsupportedCwFromDisconnection_data()
    {
        QTest::addColumn<bool>("connected");
        QTest::addColumn<int>("cwVersion");
        QTest::addColumn<QString>("startReason");
        QTest::addColumn<QString>("releaseReason");
        QTest::addColumn<QStringList>("expectedVerbs");
        const QString newer = QStringLiteral("Sending CW from a remote window needs a newer Core.");
        const QString noLink = QString::fromLatin1(RemoteTransmitClient::kNoLinkReason);
        QTest::newRow("disconnected") << false << 2 << noLink << noLink << QStringList{};
        QTest::newRow("connected without CW") << true << 0 << newer << newer << QStringList{};
        QTest::newRow("v1 releases only") << true << 1 << newer << QString()
            << QStringList{QStringLiteral("cw.stop"), QStringLiteral("cw.key")};
        QTest::newRow("v2 starts and releases") << true << 2 << QString() << QString()
            << QStringList{QStringLiteral("cw.send"), QStringLiteral("cw.memory"),
                           QStringLiteral("cw.stop"), QStringLiteral("cw.key"),
                           QStringLiteral("cw.key")};
    }

    void remoteSenderDistinguishesUnsupportedCwFromDisconnection()
    {
        QFETCH(bool, connected);
        QFETCH(int, cwVersion);
        QFETCH(QString, startReason);
        QFETCH(QString, releaseReason);
        QFETCH(QStringList, expectedVerbs);
        QStringList verbs;
        QList<MirrorUpdate> lastArguments;
        quint32 nextId = 0;
        RemoteTransmitClient transmit([&](const QByteArray& verb,
                                          const QList<MirrorUpdate>& arguments) {
            verbs.append(QString::fromLatin1(verb));
            lastArguments = arguments;
            return ++nextId;
        });
        transmit.setAvailable(connected);
        transmit.setCwAvailable(cwVersion >= 2);
        transmit.setCwReleaseAvailable(cwVersion >= 1);
        const CwxApplet::RemoteSender remote = CwxApplet::remoteSenderFor(
            [&transmit]() { return &transmit; },
            QStringLiteral("Sending CW from a remote window needs a newer Core."));

        QCOMPARE(remote.sendText(QStringLiteral("CQ")), startReason);
        QCOMPARE(remote.sendMemory(1), startReason);
        QCOMPARE(remote.stop(), releaseReason);
        QCOMPARE(remote.setKey(true), startReason);
        QCOMPARE(remote.setKey(false), releaseReason);
        QCOMPARE(verbs, expectedVerbs);
        if (!verbs.isEmpty()) {
            QCOMPARE(lastArguments.size(), 1);
            QCOMPARE(lastArguments.first().name, QByteArray("down"));
            QCOMPARE(lastArguments.first().kind, MirrorWireKind::Bool);
            QVERIFY(!lastArguments.first().value.toBool());
        }
    }

    void sendNotPermittedGreysTheSendControlsWithTheReason()
    {
        Rig rig;
        rig.applet->setSendPermitted(false, QStringLiteral("Reason words."));
        for (const char* name : {"cwxSend", "cwxStop", "cwxKey", "cwxKeyboard", "cwxMemory1"}) {
            QPushButton* b = rig.button(name);
            QVERIFY2(!b->isEnabled(), name);
            QVERIFY2(!b->isHidden(), name);
            QCOMPARE(b->toolTip(), QStringLiteral("Reason words."));
        }
        rig.applet->setCwxSettingsPermitted(false, QStringLiteral("Settings words."));
        QVERIFY(!rig.child<QSlider>("cwxSpeedSlider")->isEnabled());
        QCOMPARE(rig.child<QSlider>("cwxSpeedSlider")->toolTip(), QStringLiteral("Settings words."));
        QVERIFY(!rig.button("cwxRepeat")->isEnabled());
        rig.applet->setSendPermitted(true);
        rig.applet->setCwxSettingsPermitted(true);
        QVERIFY(rig.button("cwxSend")->isEnabled());
        QVERIFY(rig.child<QSlider>("cwxSpeedSlider")->isEnabled());
    }

    // In the window: CAT ZZKO reads and sets whether the applet shows, and
    // Tools > CWX and the status-bar CWX label show it.
    void zzkoToolsCwxAndTheStatusLabelShowTheApplet()
    {
        Test::markAudioFirstRunDone();
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.resize(1280, 800);
        window.show();
        pump();
        RadioModel* model = window.radioModel();
        QVERIFY(model->hasCwxAppletHost());
        QVERIFY(window.findChild<CwxApplet*>() != nullptr);

        const int sliceId = model->addSlice();
        QVERIFY(sliceId >= 0);
        model->sliceOwnership()->setOwner(sliceId, SliceOwnership::stationDevice());
        CatEndpointConfig config;
        config.binding.primarySliceId = sliceId;
        QVERIFY(model->catService()->applyChannelConfig(1, config));
        model->catService()->startConfigured();
        const quint64 session = model->catService()->openSession(1, CatTransportKind::Tester);
        QVERIFY(session != 0);
        const auto cat = [&](const QByteArray& frame) {
            return model->catService()->processFrame(session, frame);
        };

        // Hidden until asked for, as Thetis's CWX form is closed.
        QCOMPARE(cat("ZZKO;"), QByteArray("ZZKO0;"));
        QCOMPARE(cat("ZZKO1;"), QByteArray());
        QVERIFY(model->cwxAppletShown());
        QCOMPARE(cat("ZZKO;"), QByteArray("ZZKO1;"));
        QCOMPARE(cat("ZZKO0;"), QByteArray());
        QVERIFY(!model->cwxAppletShown());
        QCOMPARE(cat("ZZKO;"), QByteArray("ZZKO0;"));

        auto* action = window.findChild<QAction*>(QStringLiteral("cwxAction"));
        QVERIFY(action != nullptr);
        QVERIFY(action->isEnabled());
        action->trigger();
        QVERIFY(model->cwxAppletShown());
        QCOMPARE(cat("ZZKO;"), QByteArray("ZZKO1;"));

        auto* label = window.findChild<QLabel*>(QStringLiteral("statusCwxLabel"));
        QVERIFY(label != nullptr);
        QTest::mouseClick(label, Qt::LeftButton);
        QVERIFY(!model->cwxAppletShown());
        QTest::mouseClick(label, Qt::LeftButton);
        QVERIFY(model->cwxAppletShown());
    }
};

QTEST_MAIN(TestCwxApplet)
#include "tst_cwx_applet.moc"
