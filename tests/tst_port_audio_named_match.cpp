// =================================================================
// tests/tst_port_audio_named_match.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Named-device resolution for
// PortAudioBus (R-R3-36): the capture helper opens a named microphone in
// strict mode, where only an exact name may match, so a missing
// "USB Mic" never opens "USB Mic 2" on the same or another host API.
// Non-strict callers keep the substring fallback.  Pure logic, no
// PortAudio device is touched.
//
// Native audio plan bug 1 (R-AUD-06): only the saved host API is searched,
// so a name found only under another host API never matches.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-06): matches stay within
//               the saved host API. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/PortAudioBus.h"

using namespace NereusSDR;
using Candidate = PortAudioBus::NamedDeviceCandidate;
Q_DECLARE_METATYPE(Candidate)

class TstPortAudioNamedMatch : public QObject {
    Q_OBJECT

private slots:
    void strictAcceptsExactNamesOnly_data()
    {
        QTest::addColumn<QVector<Candidate>>("candidates");
        QTest::addColumn<QString>("wanted");
        QTest::addColumn<int>("hostApi");
        QTest::addColumn<int>("strictResult");
        QTest::addColumn<int>("lenientResult");

        const QVector<Candidate> twoOnly = {{QStringLiteral("USB Mic 2"), 0}};
        QTest::newRow("same-api substring") << twoOnly << "USB Mic" << 0 << -1 << 0;

        const QVector<Candidate> crossOnly = {{QStringLiteral("Built-in"), 0},
                                              {QStringLiteral("USB Mic 2"), 1}};
        QTest::newRow("cross-api substring") << crossOnly << "USB Mic" << 0 << -1 << -1;

        const QVector<Candidate> both = {{QStringLiteral("USB Mic 2"), 0},
                                         {QStringLiteral("usb mic"), 0}};
        QTest::newRow("exact wins, case-insensitive") << both << " USB Mic " << 0 << 1 << 1;

        const QVector<Candidate> crossExact = {{QStringLiteral("USB Mic 2"), 0},
                                               {QStringLiteral("USB Mic"), 1}};
        QTest::newRow("cross-api exact") << crossExact << "USB Mic" << 0 << -1 << 0;

        // A "Windows WASAPI" (index 3 here) device name that exists only
        // under MME (index 2) does not open on MME.
        const QVector<Candidate> mmeOnly = {{QStringLiteral("Speakers (USB Audio)"), 2}};
        QTest::newRow("wasapi name only under mme") << mmeOnly << "Speakers (USB Audio)" << 3
                                                    << -1 << -1;

        const QVector<Candidate> anyApi = {{QStringLiteral("USB Mic 2"), 3},
                                           {QStringLiteral("USB Mic"), 4}};
        QTest::newRow("any api exact") << anyApi << "USB Mic" << -1 << 1 << 1;

        const QVector<Candidate> none = {{QStringLiteral("Built-in Microphone"), 0}};
        QTest::newRow("no match") << none << "USB Mic" << 0 << -1 << -1;
    }
    void strictAcceptsExactNamesOnly()
    {
        QFETCH(QVector<Candidate>, candidates);
        QFETCH(QString, wanted);
        QFETCH(int, hostApi);
        QFETCH(int, strictResult);
        QFETCH(int, lenientResult);
        QCOMPARE(PortAudioBus::matchNamedDevice(candidates, wanted, hostApi, true), strictResult);
        QCOMPARE(PortAudioBus::matchNamedDevice(candidates, wanted, hostApi, false), lenientResult);
    }
};

QTEST_GUILESS_MAIN(TstPortAudioNamedMatch)
#include "tst_port_audio_named_match.moc"
