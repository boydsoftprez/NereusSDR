// no-port-check: NereusSDR-original test diagnostics.
// Authored for NereusSDR by J.J. Boyd (KG4VCF), with OpenAI Codex assistance.
#pragma once

#include "DataChannelPair.h"
#include "core/session/DataChannelLibraryLogTestHook.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QStringList>

#include <memory>
#if !defined(Q_OS_WIN)
#include <stdlib.h>
#endif

namespace NereusSDR::Test {

inline QString safeRtcStage(const QString& line)
{
    const auto state = [&line](QStringView marker, QStringView label) -> QString {
        const qsizetype at = line.indexOf(marker);
        if (at < 0) return {};
        QString value = line.mid(at + marker.size()).trimmed().split(QLatin1Char(' ')).first().toLower();
        static const QSet<QString> allowed{
            QStringLiteral("new"), QStringLiteral("checking"), QStringLiteral("connecting"),
            QStringLiteral("connected"), QStringLiteral("completed"), QStringLiteral("failed"),
            QStringLiteral("disconnected"), QStringLiteral("closed"), QStringLiteral("stable"),
            QStringLiteral("gathering"), QStringLiteral("inprogress"),
            QStringLiteral("have-local-offer"), QStringLiteral("have-remote-offer")};
        if (!allowed.contains(value)) value = QStringLiteral("other");
        return label.toString() + QStringLiteral(":") + value;
    };
    for (const auto& [marker, label] : {
             std::pair{QStringView(u"Changed ICE state to"), QStringView(u"ice")},
             std::pair{QStringView(u"Changed gathering state to"), QStringView(u"gathering")},
             std::pair{QStringView(u"Changed signaling state to"), QStringView(u"signaling")},
             std::pair{QStringView(u"Changed state to"), QStringView(u"peer")}}) {
        if (const QString phase = state(marker, label); !phase.isEmpty()) return phase;
    }
    for (const auto& [marker, label] : {
             std::pair{QStringView(u"Remote description kept before the ICE agent takes it"), QStringView(u"remote-description-kept")},
             std::pair{QStringView(u"candidates from remote description"), QStringView(u"remote-candidates-consumed")},
             std::pair{QStringView(u"Starting ICE transport"), QStringView(u"ice-start")},
             std::pair{QStringView(u"Starting DTLS transport"), QStringView(u"dtls-start")},
             std::pair{QStringView(u"before incoming records are taken"), QStringView(u"dtls-mtu-set")},
             std::pair{QStringView(u"Registering incoming callback"), QStringView(u"dtls-incoming-ready")},
             std::pair{QStringView(u"DTLS handshake finished"), QStringView(u"dtls-handshake-finished")},
             std::pair{QStringView(u"Starting SCTP transport"), QStringView(u"sctp-start")},
             std::pair{QStringView(u"ICE timeout"), QStringView(u"ice-timeout")},
             std::pair{QStringView(u"Handshake failed"), QStringView(u"dtls-handshake-failed")}}) {
        if (line.contains(marker)) return label.toString();
    }
    return {};
}

// Process-wide library capture, scoped to one synchronous fixture join.
// Only fixed stage words and structural pair events enter this buffer.
class DataChannelStartupEvidence {
public:
    DataChannelStartupEvidence() : m_state(std::make_shared<State>())
    {
        m_state->elapsed.start();
        testhooks::setDataChannelLibraryLog(
            [state = m_state](quintptr thread, const QString& line) {
                const QString stage = safeRtcStage(line);
                if (stage.isEmpty()) { return; }
                const QMutexLocker lock(&state->mutex);
                if (state->libraryStages.size() == 512) { state->libraryStages.removeFirst(); }
                state->libraryStages.append(QStringLiteral("%1 ms thread %2 %3")
                    .arg(state->elapsed.elapsed()).arg(thread).arg(stage));
            });
    }

    ~DataChannelStartupEvidence() { testhooks::setDataChannelLibraryLog({}); }
    DataChannelStartupEvidence(const DataChannelStartupEvidence&) = delete;
    DataChannelStartupEvidence& operator=(const DataChannelStartupEvidence&) = delete;

    DataChannelPairObserver observer() const
    {
        return [state = m_state](DataChannelPairEvent event) {
            const QMutexLocker lock(&state->mutex);
            const int sideIndex = event.side == DataChannelPairEvent::Side::Offerer ? 0 : 1;
            const QString side = sideIndex == 0 ? QStringLiteral("offerer")
                                                : QStringLiteral("answerer");
            QString detail;
            switch (event.kind) {
            case DataChannelPairEvent::Kind::DescriptionEmitted:
                detail = QStringLiteral("description emitted count=%1")
                    .arg(++state->descriptionsEmitted[sideIndex]);
                break;
            case DataChannelPairEvent::Kind::DescriptionAccepted:
                detail = QStringLiteral("description accepted=%1").arg(int(event.accepted));
                break;
            case DataChannelPairEvent::Kind::CandidateEmitted:
                detail = QStringLiteral("candidate emitted count=%1")
                    .arg(++state->candidatesEmitted[sideIndex]);
                break;
            case DataChannelPairEvent::Kind::CandidateAccepted:
                detail = QStringLiteral("candidate accepted=%1").arg(int(event.accepted));
                break;
            case DataChannelPairEvent::Kind::GatheringComplete:
                detail = QStringLiteral("gathering complete");
                break;
            case DataChannelPairEvent::Kind::Opened:
                detail = QStringLiteral("opened");
                break;
            case DataChannelPairEvent::Kind::Failed:
                detail = QStringLiteral("failed");
                break;
            }
            if (state->pairEvents.size() == 256) { state->pairEvents.removeFirst(); }
            state->pairEvents.append(QStringLiteral("%1 ms %2 %3")
                .arg(state->elapsed.elapsed()).arg(side, detail));
        };
    }

    QString diagnostic() const
    {
        QString load = QStringLiteral("load average unavailable");
#if !defined(Q_OS_WIN)
        double values[3]{};
        if (getloadavg(values, 3) == 3) {
            load = QStringLiteral("load average %1 %2 %3")
                .arg(values[0], 0, 'f', 2).arg(values[1], 0, 'f', 2).arg(values[2], 0, 'f', 2);
        }
#endif
        const QMutexLocker lock(&m_state->mutex);
        return load + QLatin1Char('\n') + m_state->pairEvents.join(QLatin1Char('\n'))
            + QLatin1Char('\n') + m_state->libraryStages.join(QLatin1Char('\n'));
    }

    /// Load findings 2: the last library stage recorded ("ice:checking",
    /// "dtls-start", ...), or "none", so a stalled open says where.
    QString lastLibraryStage() const
    {
        const QMutexLocker lock(&m_state->mutex);
        if (m_state->libraryStages.isEmpty()) { return QStringLiteral("none"); }
        return m_state->libraryStages.constLast().section(QLatin1Char(' '), 4);
    }

private:
    struct State {
        QElapsedTimer elapsed;
        QMutex mutex;
        QStringList libraryStages;
        QStringList pairEvents;
        int descriptionsEmitted[2]{};
        int candidatesEmitted[2]{};
    };
    const std::shared_ptr<State> m_state;
};

} // namespace NereusSDR::Test
