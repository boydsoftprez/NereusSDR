// =================================================================
// tests/tst_receiver_stop_notices.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. R3 receiver audio fix wave
// (R-R3-42, R-R3-44): one receiver-audio stop raises one plain notice,
// not one from TCI and another per VAX channel beside the window's own
// status.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio fix wave, and its
//                                    follow-up (an event is reason plus
//                                    slice). AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "gui/OperatorReasonText.h"
#include "gui/ReceiverStopNotices.h"
#include "gui/RemoteMediaController.h"
#include "OperatorWording.h"

using namespace NereusSDR;

class TstReceiverStopNotices : public QObject {
    Q_OBJECT

private slots:
    // The slice carried by TCI and two VAX channels is removed at the
    // Core: three consumers report it, one notice is raised, in plain
    // words and without a "TCI:" or "VAX N:" of its own.
    void oneStopFromThreeConsumersIsOneNotice()
    {
        ReceiverStopNotices notices;
        const QString reason = QStringLiteral("slice-removed");
        const QString first = notices.toastFor(reason, 1, 1000);    // TCI
        const QString second = notices.toastFor(reason, 1, 1003);   // VAX 1
        const QString third = notices.toastFor(reason, 1, 1004);    // VAX 2
        QCOMPARE(first, OperatorReasonText::forDisplay(reason));
        QVERIFY2(OperatorWording::isPlain(first), qPrintable(first));
        QVERIFY(second.isEmpty());
        QVERIFY(third.isEmpty());
        // A later, separate stop is news again.
        QCOMPARE(notices.toastFor(reason, 1, 1000 + ReceiverStopNotices::kSameEventMs),
                 OperatorReasonText::forDisplay(reason));
        // A different reason is a different event.
        QVERIFY(!notices.toastFor(QStringLiteral("receiver-limit"), 1, 1005).isEmpty());
    }

    // Two slices removed within the window are two events, two notices.
    void twoSlicesStoppingAreTwoNotices()
    {
        ReceiverStopNotices notices;
        const QString reason = QStringLiteral("slice-removed");
        QVERIFY(!notices.toastFor(reason, 0, 1000).isEmpty());
        QVERIFY(!notices.toastFor(reason, 1, 1500).isEmpty());
        // Each still one notice across its consumers.
        QVERIFY(notices.toastFor(reason, 0, 1600).isEmpty());
        QVERIFY(notices.toastFor(reason, 1, 1700).isEmpty());
    }

    // What the window's own status already says raises no toast at all.
    void whatTheWindowShowsRaisesNone()
    {
        ReceiverStopNotices notices;
        QVERIFY(notices.toastFor(QStringLiteral("radio-offline"), 0, 0).isEmpty());
        QVERIFY(notices.toastFor(QStringLiteral("media-not-ready"), 0, 0).isEmpty());
        QVERIFY(notices.toastFor(QStringLiteral("client-disabled"), 0, 0).isEmpty());
    }

    void knowsTheReceiverStops()
    {
        for (const char* wire : {"client-disabled", "media-not-ready", "radio-offline",
                                 "encoder-unavailable", "slice-removed", "receiver-limit"}) {
            QVERIFY2(ReceiverStopNotices::isReceiverStop(QString::fromLatin1(wire)), wire);
        }
        QVERIFY(ReceiverStopNotices::isReceiverStop(
            QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason)));
        // A TCI refusal is not a receiver stop: it keeps its own toast.
        QVERIFY(!ReceiverStopNotices::isReceiverStop(QStringLiteral("Apps cannot transmit through TCI from a remote window.")));
        // Every toast it can raise is in plain words.
        ReceiverStopNotices notices;
        for (const char* wire : {"encoder-unavailable", "slice-removed", "receiver-limit"}) {
            const QString text = notices.toastFor(QString::fromLatin1(wire), 0, 0);
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        const QString older = notices.toastFor(
            QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason), 0, 0);
        QVERIFY2(!older.isEmpty() && OperatorWording::isPlain(older), qPrintable(older));
    }
};

QTEST_MAIN(TstReceiverStopNotices)
#include "tst_receiver_stop_notices.moc"
