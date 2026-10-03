// no-port-check: NereusSDR-original.
// Task 56: a Core-owned route stays alive for the real ICE transport and
// leaves only after its libjuice agent has retired. No radio or audio device
// is opened by this test.
// Modification history (NereusSDR):
//   2026-09-27: original implementation by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via OpenAI Codex.

#include <QtTest>

#include "impl/icetransport.hpp"

#include <memory>

class TstIceTransportLifetime : public QObject {
    Q_OBJECT

private slots:
    void ownerSurvivesRetainedIceReference()
    {
        auto owner = std::make_shared<int>(42);
        std::weak_ptr<void> weak = owner;
        rtc::Configuration config;
        config.iceTransportLifetime = owner;
        auto ice = std::make_shared<rtc::impl::IceTransport>(
            config,
            [](const rtc::Candidate&) {},
            [](rtc::impl::Transport::State) {},
            [](rtc::impl::IceTransport::GatheringState) {});
        config.iceTransportLifetime.reset();
        owner.reset();
        QVERIFY(!weak.expired());

        // A peer may drop its reference while another transport user still
        // owns ICE. The route must follow the actual ICE object's lifetime.
        auto retained = ice;
        ice.reset();
        QVERIFY(!weak.expired());
        retained.reset();
        QTRY_VERIFY_WITH_TIMEOUT(weak.expired(), 5000);
    }

    void emptyOwnerKeepsOrdinaryClose()
    {
        rtc::Configuration config;
        QVERIFY(!config.iceTransportLifetime);
        auto ice = std::make_shared<rtc::impl::IceTransport>(
            config,
            [](const rtc::Candidate&) {},
            [](rtc::impl::Transport::State) {},
            [](rtc::impl::IceTransport::GatheringState) {});
        ice.reset();
    }
};

QTEST_APPLESS_MAIN(TstIceTransportLifetime)
#include "tst_ice_transport_lifetime.moc"
