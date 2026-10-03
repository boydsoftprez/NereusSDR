// =================================================================
// tests/tst_capture_status_text.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Wording table for the PC
// microphone capture status (R-R3-36): every state and every failure
// reason maps to its exact operator text; no Thetis logic.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "gui/setup/CaptureStatusText.h"

using namespace NereusSDR;
using Status = CaptureSupervisor::Status;
using State = Status::State;
using Reason = Status::Reason;

Q_DECLARE_METATYPE(NereusSDR::CaptureSupervisor::Status::State)
Q_DECLARE_METATYPE(NereusSDR::CaptureSupervisor::Status::Reason)

class TstCaptureStatusText : public QObject {
    Q_OBJECT

private slots:
    void wording_data()
    {
        QTest::addColumn<State>("state");
        QTest::addColumn<Reason>("reason");
        QTest::addColumn<QString>("configured");
        QTest::addColumn<QString>("actual");
        QTest::addColumn<QString>("expected");

        const QString mic = QStringLiteral("USB Mic");

        QTest::newRow("closed") << State::Closed << Reason::None << mic << QString()
                                << QStringLiteral("Microphone not in use");
        QTest::newRow("permission") << State::PreparingPermission << Reason::None << mic << QString()
                                    << QStringLiteral("Waiting for microphone permission");
        QTest::newRow("opening") << State::Opening << Reason::None << mic << QString()
                                 << QStringLiteral("Preparing microphone");
        QTest::newRow("ready-named") << State::Ready << Reason::None << mic << QStringLiteral("USB Mic (2)")
                                     << QStringLiteral("Microphone ready: USB Mic (2)");
        QTest::newRow("ready-default") << State::Ready << Reason::None << QString()
                                       << QStringLiteral("MacBook Pro Microphone")
                                       << QStringLiteral("Microphone ready: MacBook Pro Microphone");
        QTest::newRow("ready-no-name") << State::Ready << Reason::None << QString() << QString()
                                       << QStringLiteral("Microphone ready: the system default microphone");
        QTest::newRow("stopping") << State::Stopping << Reason::None << mic << QString()
                                  << QStringLiteral("Stopping microphone");

        QTest::newRow("permission-denied") << State::Failed << Reason::PermissionDenied << mic << QString()
            << QStringLiteral("Microphone access is turned off for NereusSDR. "
                              "Allow it in System Settings, then retry.");
        QTest::newRow("device-not-found") << State::Failed << Reason::DeviceNotFound << mic << QString()
            << QStringLiteral("The selected microphone \"USB Mic\" is not available.");
        QTest::newRow("default-not-found") << State::Failed << Reason::DeviceNotFound << QString() << QString()
            << QStringLiteral("The system default microphone is not available.");
        QTest::newRow("open-failed") << State::Failed << Reason::OpenFailed << mic << QString()
            << QStringLiteral("The selected microphone could not be opened.");
        QTest::newRow("start-failed") << State::Failed << Reason::StartFailed << mic << QString()
            << QStringLiteral("The selected microphone could not be opened.");
        QTest::newRow("input-lost") << State::Failed << Reason::InputLost << mic << QString()
            << QStringLiteral("The microphone stopped sending audio.");
        QTest::newRow("timeout") << State::Failed << Reason::Timeout << mic << QString()
            << QStringLiteral("The microphone did not respond in time.");
        QTest::newRow("helper-missing") << State::Failed << Reason::HelperMissing << mic << QString()
            << QStringLiteral("Microphone support is missing from this installation.");
        QTest::newRow("helper-did-not-start") << State::Failed << Reason::HelperDidNotStart << mic << QString()
            << QStringLiteral("Microphone support stopped unexpectedly.");
        QTest::newRow("helper-exited") << State::Failed << Reason::HelperExited << mic << QString()
            << QStringLiteral("Microphone support stopped unexpectedly.");
        QTest::newRow("protocol-error") << State::Failed << Reason::ProtocolError << mic << QString()
            << QStringLiteral("Microphone support stopped unexpectedly.");
    }

    void wording()
    {
        QFETCH(State, state);
        QFETCH(Reason, reason);
        QFETCH(QString, configured);
        QFETCH(QString, actual);
        QFETCH(QString, expected);

        Status status;
        status.state = state;
        status.reason = reason;
        status.configuredDevice = configured;
        status.actualDevice = actual;
        status.generation = 7;
        QCOMPARE(captureStatusText(status), expected);
    }

    // Operator text never carries transport wording.
    void noTransportWording()
    {
        const QList<Reason> reasons = {
            Reason::None, Reason::PermissionDenied, Reason::DeviceNotFound, Reason::OpenFailed,
            Reason::StartFailed, Reason::InputLost, Reason::Timeout, Reason::HelperMissing,
            Reason::HelperDidNotStart, Reason::HelperExited, Reason::ProtocolError};
        const QStringList banned = {QStringLiteral("helper"), QStringLiteral("protocol"),
                                    QStringLiteral("generation"), QStringLiteral("pipe"),
                                    QStringLiteral("process")};
        for (Reason reason : reasons) {
            Status status;
            status.state = State::Failed;
            status.reason = reason;
            status.generation = 3;
            const QString text = captureStatusText(status).toLower();
            QVERIFY(!text.isEmpty());
            for (const QString& word : banned) {
                QVERIFY2(!text.contains(word), qPrintable(text));
            }
        }
    }
};

QTEST_APPLESS_MAIN(TstCaptureStatusText)
#include "tst_capture_status_text.moc"
