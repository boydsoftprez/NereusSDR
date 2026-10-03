// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ConfirmStep.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-30): the questions the Core has asked and
// the notices it keeps. See ConfirmStep.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over parity: forgetTakeBacks(). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/ConfirmStep.h"

namespace NereusSDR {

void ConfirmStep::ask(const Question& question)
{
    m_questions.insert(question.device, question);
    m_droppedAsChanged.remove(question.device);
}

QList<QByteArray> ConfirmStep::dropQuestionsNaming(int sliceId)
{
    QList<QByteArray> devices;
    for (auto it = m_questions.begin(); it != m_questions.end();) {
        if (it->namedSlices.contains(sliceId)) {
            devices.append(it.key());
            m_droppedAsChanged.insert(it.key(), qMakePair(it->id, it->kind));
            it = m_questions.erase(it);
        } else {
            ++it;
        }
    }
    return devices;
}

std::optional<QString> ConfirmStep::takeDroppedAsChanged(const QByteArray& device, qint64 id)
{
    const auto it = m_droppedAsChanged.constFind(device);
    if (it == m_droppedAsChanged.cend() || it->first != id) {
        return std::nullopt;
    }
    const QString kind = it->second;
    m_droppedAsChanged.remove(device);
    return kind;
}

std::optional<ConfirmStep::Question> ConfirmStep::answer(const QByteArray& device, qint64 id)
{
    const auto it = m_questions.find(device);
    if (it == m_questions.end() || it->id != id) {
        return std::nullopt;
    }
    Question question = *it;
    m_questions.erase(it);
    return question;
}

const ConfirmStep::Question* ConfirmStep::openQuestion(const QByteArray& device) const
{
    const auto it = m_questions.constFind(device);
    return it == m_questions.cend() ? nullptr : &(*it);
}

void ConfirmStep::dropQuestion(const QByteArray& device)
{
    m_questions.remove(device);
    m_droppedAsChanged.remove(device);
}

void ConfirmStep::keepTakeBack(const Notice& notice)
{
    m_takeBack[notice.device].append(notice);
}

std::optional<ConfirmStep::Notice> ConfirmStep::takeBackRecord(const QByteArray& device,
                                                               qint64 id) const
{
    for (const Notice& notice : m_takeBack.value(device)) {
        if (notice.id == id) {
            return notice;
        }
    }
    return std::nullopt;
}

void ConfirmStep::forgetTakeBack(const QByteArray& device, qint64 id)
{
    auto it = m_takeBack.find(device);
    if (it == m_takeBack.end()) {
        return;
    }
    it->removeIf([id](const Notice& notice) { return notice.id == id; });
    if (it->isEmpty()) {
        m_takeBack.erase(it);
    }
}

void ConfirmStep::forgetTakeBacks(const QByteArray& device,
                                  const std::function<bool(const Notice&)>& which)
{
    auto it = m_takeBack.find(device);
    if (it == m_takeBack.end() || !which) {
        return;
    }
    it->removeIf(which);
    if (it->isEmpty()) {
        m_takeBack.erase(it);
    }
}

void ConfirmStep::keepPending(const Notice& notice)
{
    m_pending[notice.device].append(notice);
}

QList<ConfirmStep::Notice> ConfirmStep::takePending(const QByteArray& device)
{
    return m_pending.take(device);
}

QList<ConfirmStep::Notice> ConfirmStep::endTakeBacks(const QByteArray& device)
{
    // Every record: a waiting notice's and a delivered one's alike (a
    // waiting notice that offers Take it back always has its record,
    // StationServer::tellDevice).
    const QList<Notice> ended = m_takeBack.take(device);
    const auto it = m_pending.find(device);
    if (it != m_pending.end()) {
        for (Notice& notice : *it) {
            notice.prompt.takeBack = false;
        }
    }
    return ended;
}

int ConfirmStep::pendingCount(const QByteArray& device) const
{
    return static_cast<int>(m_pending.value(device).size());
}

void ConfirmStep::forgetDevice(const QByteArray& device)
{
    m_questions.remove(device);
    m_droppedAsChanged.remove(device);
    m_takeBack.remove(device);
    m_pending.remove(device);
}

} // namespace NereusSDR
