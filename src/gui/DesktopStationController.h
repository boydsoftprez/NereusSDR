#pragma once
// no-port-check: NereusSDR-original. Task 48 desktop hosting over one borrowed local model.

#include "core/station/StationHost.h"

#include <QObject>
#include <QPointer>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

class RadioModel;
class StationServer;

class DesktopStationController final : public QObject {
    Q_OBJECT
public:
    // TX badge take (JJ, 2026-09-30): Take takes transmit and keys nothing.
    // Fix wave (hosting 2-TONE parity): TwoTone starts the 2-tone test.
    enum class Key { Mox, Tune, Take, TwoTone };
    enum class RequestState { Refused, NoChange, Pending, Ask };

    struct TakeQuestion {
        Key key{Key::Mox};
        quint64 holderEpoch{0};
        bool holderKeyed{false};
        QString holderName;
        QString holderShortName;
        quint64 questionId{0};
    };
    struct RequestResult {
        RequestState state{RequestState::Refused};
        std::optional<TakeQuestion> question;
        QString reason;
        /// TX badge take: the take this result belongs to (0: none).
        /// takeFinished names it when the take ends later.
        quint64 takeId{0};
    };

    // The model, settings and chosen profile are caller-owned. Construction
    // never loads settings or station identity. start(true) is permitted only
    // after the caller has acquired exclusive ownership of that profile.
    DesktopStationController(RadioModel* localModel, StationHostOptions options,
                             QObject* parent = nullptr);
    ~DesktopStationController() override;

    bool start(bool profileOwnershipEstablished);
    void stop();
    StationHost* host() const { return m_host.get(); }
    StationServer* server() const;
    bool enabled() const;

signals:
    void hostingStateChanged(bool enabled);
    /// TX badge take: a take requestTakeTransmit started and that did not
    /// end within its call ended now. `held` is whether the station device
    /// holds transmit through it; false when the Core did not assign it
    /// (a stop not confirmed), and when another request, a stop or the end
    /// of hosting replaced it.
    void takeFinished(quint64 takeId, bool held);

public:
    RequestResult requestMox(bool on);
    RequestResult requestTune(bool on);
    /// Fix wave (hosting 2-TONE parity): the 2-tone test asks to take
    /// transmit as MOX and TUNE do. Off always stops the test, as the
    /// button did before.
    RequestResult requestTwoTone(bool on);
    /// TX badge take: the station device takes transmit through tx.take's
    /// rules (asked first while another device holds it) and keys nothing.
    /// NoChange when it already holds transmit.
    RequestResult requestTakeTransmit();
    RequestResult confirmTake(const TakeQuestion& shown);
    /// TX badge take: the take still waiting for its end (0: none).
    quint64 takeInFlight() const { return m_takeInFlight; }

#ifdef NEREUS_BUILD_TESTS
    // Forward the existing Host construction seam for in-process lifecycle tests.
    void setServerCreatedForTest(std::function<void(StationServer*)> callback)
    {
        m_serverCreatedForTest = std::move(callback);
    }
#endif

private:
    RequestResult request(Key key, bool on);
    RequestResult takeAndKey(Key key, std::optional<quint64> shownEpoch,
                             std::optional<bool> shownKeyed);
    RequestResult ask(Key key);
    void keyNow(Key key);
    bool stationHoldsTransmit() const;
    // TX badge take: ends the take in flight as not held (takeFinished).
    void supersedeTake();
    // TX badge take: what a take's call returns, and whether it ended there.
    RequestResult settleTake(quint64 takeId, RequestResult result);

    QPointer<RadioModel> m_model;
    StationHostOptions m_options;
    std::unique_ptr<StationHost> m_host;
    StationHost* m_startingHost{nullptr}; // owned by start() until that call returns
    std::optional<TakeQuestion> m_question;
    quint64 m_intentGeneration{0};
    quint64 m_nextQuestionId{0};
    quint64 m_takeInFlight{0};
    quint64 m_nextTakeId{0};
    bool m_moxRequested{false};
    bool m_tuneRequested{false};
    bool m_stopDuringStart{false};
    bool m_stopping{false};
#ifdef NEREUS_BUILD_TESTS
    std::function<void(StationServer*)> m_serverCreatedForTest;
#endif
};

} // namespace NereusSDR
