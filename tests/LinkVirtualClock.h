#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/LinkVirtualClock.h  (NereusSDR)
// =================================================================
//
// The session fixture player's virtual clock (LinkFixtures::runSession),
// moved out of LinkFixtures.cpp so its own tests can reach it.
//
// A virtual clock over every QTimer under a root object (the station
// server's heartbeat and delta flush timers, its grace timer, and each
// peer's handshake deadline, parented to that peer's transport).
// advance() fires them in virtual time order by emitting their timeout
// directly, exactly as a real expiry would: a single-shot timer stops
// first, a repeating one runs on.
//
// Those timers still run in real time too, and the fixtures count on it
// for one of them: a delta the 50 ms flush sends is awaited, in real time,
// without an advanceMs. What the player does between two awaited messages
// takes no virtual time, though, so while it holds the clock (Hold: a
// drain, a client's connect, advance() itself) their real expiries are
// swallowed. Over a data channel a client's connect takes real time (a
// DTLS and SCTP handshake), and the 50 ms flush fired in the middle of the
// step and split one coalesced delta into two.
//
// A timer's virtual due time is its interval after the virtual instant it
// was started, whatever real time passes before the clock looks: virtual
// time stands still between two advances, so a timer is always started at
// the clock's now. A start is seen by the timer's active property, which
// notifies on every start (a restart while running included) and never on
// a repeating timer's own real expiry. The clock used to read the start
// from the timer's real remaining time instead, and so took real time into
// virtual time: a timer the clock first saw a millisecond after its start
// was due a millisecond early, and each real expiry of a repeating timer
// outside a hold moved its phase. session-tx-keepalive over a data channel
// then saw the meter pump poll at 999 ms instead of 1000 ms, too early for
// the time-out's 179, and the transmit watchdog's 1051 ms stop came first.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: moved from LinkFixtures.cpp; a restart is judged on the
//               real remaining time; real expiries are swallowed while the
//               clock is held (iPhone app plan Task 28 tail, R-IOS-16). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: a timer is due its interval after its start in virtual
//               time; starts are seen by the active property, never by
//               real remaining time (R-R3-49). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QProperty>
#include <QString>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace NereusSDR::Test {

class LinkVirtualClock : public QObject {
public:
    /// `drain` runs the queued work between timers (the player's drain()).
    LinkVirtualClock(QObject* root, std::function<void()> drain)
        : m_root(root)
        , m_drain(std::move(drain))
    {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            app->installEventFilter(this);
        }
    }

    ~LinkVirtualClock() override
    {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            app->removeEventFilter(this);
        }
    }

    qint64 now() const { return m_now; }

    /// While one is alive, the real expiries of the timers under the root
    /// are swallowed: what runs in between takes no virtual time.
    class Hold {
    public:
        explicit Hold(LinkVirtualClock& clock)
            : m_clock(clock)
        {
            ++m_clock.m_holding;
        }
        ~Hold() { --m_clock.m_holding; }
        Hold(const Hold&) = delete;
        Hold& operator=(const Hold&) = delete;

    private:
        LinkVirtualClock& m_clock;
    };

    void scan()
    {
        m_tracked.erase(std::remove_if(m_tracked.begin(), m_tracked.end(),
                                       [](const std::unique_ptr<Tracked>& t) {
                                           return t->timer.isNull() || !t->timer->isActive();
                                       }),
                        m_tracked.end());
        if (m_root.isNull()) {
            return;
        }
        const QList<QTimer*> timers = m_root->findChildren<QTimer*>();
        for (QTimer* timer : timers) {
            if (!timer->isActive()) {
                continue;
            }
            auto it = std::find_if(m_tracked.begin(), m_tracked.end(),
                                   [timer](const std::unique_ptr<Tracked>& t) {
                                       return t->timer == timer;
                                   });
            if (it == m_tracked.end()) {
                auto t = std::make_unique<Tracked>();
                t->timer = timer;
                t->due = m_now + std::max(0, timer->interval());
                Tracked* raw = t.get();
                t->started = timer->bindableActive().addNotifier([raw]() { raw->restarted = true; });
                m_tracked.push_back(std::move(t));
            } else if ((*it)->restarted) {
                (*it)->restarted = false;
                (*it)->due = m_now + std::max(0, timer->interval());
            }
        }
    }

    /// Empty on success; a description when a timeout could not be fired.
    QString advance(qint64 ms)
    {
        const Hold hold(*this);
        const qint64 target = m_now + ms;
        for (int guard = 0; guard < 100000; ++guard) {
            runQueued();
            scan();
            auto next = std::min_element(m_tracked.begin(), m_tracked.end(),
                                         [](const std::unique_ptr<Tracked>& a,
                                            const std::unique_ptr<Tracked>& b) {
                                             return a->due < b->due;
                                         });
            if (next == m_tracked.end() || (*next)->due > target) {
                break;
            }
            m_now = std::max(m_now, (*next)->due);
            QPointer<QTimer> timer = (*next)->timer;
            if (timer->isSingleShot()) {
                timer->stop();
                m_tracked.erase(next);
            } else {
                // The clock's own restart: its next period, not a start the
                // scan should read (a restart from the timeout still is).
                timer->start();
                (*next)->restarted = false;
                (*next)->due = m_now + std::max(1, timer->interval());
            }
            if (!QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection)) {
                return QStringLiteral("could not fire a timer's timeout");
            }
        }
        m_now = target;
        runQueued();
        scan();
        return QString();
    }

    /// The virtual time `timer` is due, or -1 when the clock does not track
    /// it (for the clock's own tests).
    qint64 dueForTest(const QTimer* timer) const
    {
        for (const auto& t : m_tracked) {
            if (t->timer == timer) {
                return t->due;
            }
        }
        return -1;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_holding == 0 || event->type() != QEvent::Timer) {
            return false;
        }
        auto* timer = qobject_cast<QTimer*>(watched);
        if (timer == nullptr || !isUnderRoot(timer)) {
            return false;
        }
        // A real expiry of a timer the clock fires. The underlying timer
        // runs on; its virtual due time is the clock's alone.
        ++m_swallowed;
        return true;
    }

public:
    /// How many real expiries were swallowed (for the clock's own tests).
    int swallowedForTest() const { return m_swallowed; }

private:
    struct Tracked {
        QPointer<QTimer> timer;
        qint64 due = 0;
        /// Set by the timer's active property on a start or a stop.
        bool restarted = false;
        QPropertyNotifier started;
    };

    bool isUnderRoot(const QObject* object) const
    {
        for (const QObject* o = object; o != nullptr; o = o->parent()) {
            if (o == m_root.data()) {
                return true;
            }
        }
        return false;
    }

    void runQueued()
    {
        if (m_drain) {
            m_drain();
        }
    }

    QPointer<QObject> m_root;
    std::function<void()> m_drain;
    qint64 m_now = 0;
    std::vector<std::unique_ptr<Tracked>> m_tracked;
    int m_swallowed = 0;
    int m_holding = 0;
};

} // namespace NereusSDR::Test
