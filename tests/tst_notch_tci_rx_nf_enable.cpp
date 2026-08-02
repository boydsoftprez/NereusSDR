// =================================================================
// tests/tst_notch_tci_rx_nf_enable.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. The Thetis
// citations below are rationale for the wire behaviour being asserted, not
// ported code.
//
// TNF section 6.4: rx_nf_enable is repointed off the m_tciStubRxNf[] array
// and onto the real NotchModel master enable, the handler's single-index
// push is dropped, and the both-index broadcast Thetis drives from
// TNFChangedHandlers is wired in TciServer::hookSliceBroadcasts().
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 6.4.
// Upstream (all [v2.10.3.15]):
//   TCIServer.cs:3384-3399   handleRxNfEnable (query + set branches)
//   TCIServer.cs:1954-1958   sendRxNfEnable wire format
//   TCIServer.cs:1315-1320   NfChanged -> sendRxNfEnable(0) AND (1)
//   TCIServer.cs:6771        console.TNFChangedHandlers += OnTnfChanged
//   TCIServer.cs:7686-7696   OnTnfChanged -> NfChanged on every listener
//   console.cs:40004         gates the handler fire on old_tnf != value
//   console.cs:52317-52330   GetMNF, a global flag behind a per-rx shape
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include <QtTest/QtTest>
#include <QStringList>

#include "core/AppSettings.h"
#include "core/TciProtocol.h"
#include "core/TciServer.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestNotchTciRxNfEnable : public QObject {
    Q_OBJECT

private:
    // Drain every frame the protocol has queued for broadcast. TciServer's
    // 5 ms drain timer only ticks while the event loop runs between start()
    // and stop(); these tests read the queue directly so nothing waits.
    static QStringList drain(TciProtocol* p)
    {
        QStringList out;
        while (p && p->hasPendingNotification()) {
            out << p->takePendingNotification();
        }
        return out;
    }

private slots:
    // NotchModel saves on mutate and restores in the RadioModel constructor,
    // so each slot starts from a clean settings sandbox.
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // -- Round-trip against the real master enable ------------------------

    void set_on_rx0_flips_the_master_enable()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());
        QVERIFY(!m.notchModel()->globalEnabled());

        p.handleCommand(QStringLiteral("rx_nf_enable:0,true;"));
        QVERIFY(m.notchModel()->globalEnabled());
    }

    void set_on_rx1_flips_the_same_master_enable()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        // TCI addresses receivers as rx 0|1 but the notch enable is global,
        // exactly as Thetis GetMNF is: console.cs:52317-52330 [v2.10.3.15]
        // returns TNFActive for either index.
        p.handleCommand(QStringLiteral("rx_nf_enable:1,true;"));
        QVERIFY(m.notchModel()->globalEnabled());

        p.handleCommand(QStringLiteral("rx_nf_enable:1,false;"));
        QVERIFY(!m.notchModel()->globalEnabled());
    }

    void query_reports_the_master_enable_on_both_indices()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        m.notchModel()->setGlobalEnabled(true);
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:0;")),
                 QStringLiteral("rx_nf_enable:0,true;"));
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:1;")),
                 QStringLiteral("rx_nf_enable:1,true;"));

        m.notchModel()->setGlobalEnabled(false);
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:0;")),
                 QStringLiteral("rx_nf_enable:0,false;"));
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:1;")),
                 QStringLiteral("rx_nf_enable:1,false;"));
    }

    void out_of_range_rx_index_leaves_the_master_enable_alone()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        // handleRxNfEnable rejects rx outside 0..1 (TCIServer.cs:3388
        // [v2.10.3.15]); GetMNF rejects its 1-based equivalent
        // (console.cs:52320 [v2.10.3.15]). The shim guards too, so a caller
        // that bypasses the handler cannot corrupt the flag.
        p.handleCommand(QStringLiteral("rx_nf_enable:2,true;"));
        QVERIFY(!m.notchModel()->globalEnabled());

        QMetaObject::invokeMethod(&m, "setRxNf", Qt::DirectConnection,
                                  Q_ARG(int, 7), Q_ARG(bool, true));
        QVERIFY(!m.notchModel()->globalEnabled());

        bool out = true;
        QMetaObject::invokeMethod(&m, "rxNf", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, out), Q_ARG(int, 7));
        QVERIFY(!out);
    }
};

QTEST_MAIN(TestNotchTciRxNfEnable)
#include "tst_notch_tci_rx_nf_enable.moc"

#else
// WebSockets not available: TciServer.h is #ifdef HAVE_WEBSOCKETS. The binary
// must still link so CTest doesn't report a missing executable.
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
