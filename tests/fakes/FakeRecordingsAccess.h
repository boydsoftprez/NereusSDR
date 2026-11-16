#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/fakes/FakeRecordingsAccess.h  (NereusSDR)
// =================================================================
//
// A RecordingsAccess for GUI tests and preview scenes: records every call and
// shows whatever state, document, list and playback it is given. It adds no
// signals, so it needs no moc; a test emits the inherited signals itself
// (kept(), refused()). requestList() answers at once from `entries`, to the
// reader that asked. With `appliesChanges`, rename and remove change
// `entries` and move the document's index revision, as the Core would.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 6.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: list readers,
//                 the slice and kind filters and appliesChanges.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 8: `settingsSent`
//                 keeps each setSettings change; `version` is the Core's
//                 recordingsVersion. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 11: the voice
//                 keyer's calls are recorded (sliceId carries the message
//                 number). AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 22: rewind, live
//                 and seekPast are recorded. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 23: the past
//                 picture's requests are recorded. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 24: with their
//                 moment, frequency, stretch and pan view. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QStringList>

#include <algorithm>

#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

class FakeRecordingsAccess : public RecordingsAccess {
public:
    struct Call {
        QString verb;
        int sliceId{-1};
        QString id;
        int offset{0};
        int count{0};
        int reader{0};
        std::optional<int> listSlice;
        std::optional<RecordingKind> listKind;
        // Task 24: the moment, frequency, stretch and pan view a DSR call
        // carried.
        qint64 utcNs{0};
        qint64 hz{0};
        qint64 fromUtcNs{0};
        DsrPanView view{};
    };

    explicit FakeRecordingsAccess(QObject* parent = nullptr) : RecordingsAccess(parent) {}

    QList<Call> calls;
    QList<RecordingEntry> entries;   // the Core's whole index, newest first
    bool coreHere = true;
    bool appliesChanges = false;   // rename and remove change `entries`
    QList<QJsonObject> settingsSent;   // each setSettings change, in order
    int version = 1;                   // the Core's recordingsVersion when it has one

    QStringList verbs() const
    {
        QStringList out;
        for (const Call& c : calls) {
            out.append(c.verb);
        }
        return out;
    }

    void setState(State state)
    {
        m_state = state;
        emit stateChanged();
    }

    void setDocument(const QJsonObject& document)
    {
        m_document = document;
        emit documentChanged();
    }

    void setPlayback(const std::optional<PlaybackState>& playback)
    {
        m_playback = playback;
        emit playbackChanged();
    }

    /// A document for `slices` (slice id to state, elapsed and myVoice) with
    /// the index at `revision`.
    static QJsonObject document(const QJsonArray& slices, qint64 revision, const QString& folder = {},
                                const QJsonObject& settings = {})
    {
        QJsonObject s = settings;
        if (!s.contains(QStringLiteral("keepMyVoice"))) {
            s.insert(QStringLiteral("keepMyVoice"), true);
        }
        if (!s.contains(QStringLiteral("quietMinutes"))) {
            s.insert(QStringLiteral("quietMinutes"), 2);
        }
        return QJsonObject{{QStringLiteral("version"), 1},
                           {QStringLiteral("slices"), slices},
                           {QStringLiteral("kept"), QJsonObject{{QStringLiteral("seq"), 0}}},
                           {QStringLiteral("index"), QJsonObject{{QStringLiteral("revision"), revision},
                                                                 {QStringLiteral("folder"), folder}}},
                           {QStringLiteral("settings"), s}};
    }

    static QJsonObject slice(int id, const QString& state, qint64 elapsedMs = 0, bool myVoice = false)
    {
        return QJsonObject{{QStringLiteral("slice"), id},
                           {QStringLiteral("state"), state},
                           {QStringLiteral("elapsedMs"), elapsedMs},
                           {QStringLiteral("myVoice"), myVoice}};
    }

    State state() const override { return m_state; }
    int coreVersion() const override { return m_state == State::NeedsNewerCore ? 0 : version; }
    QString unavailableReason(int needsVersion = 1) const override
    {
        if (m_state == State::NotConnected) {
            return notConnectedReason();
        }
        if (m_state == State::NeedsNewerCore || coreVersion() < needsVersion) {
            return needsNewerCoreReason();
        }
        return {};
    }
    QJsonObject document() const override { return m_document; }
    void start(int sliceId) override { calls.append({QStringLiteral("start"), sliceId}); }
    void stop(int sliceId) override { calls.append({QStringLiteral("stop"), sliceId}); }
    void requestList(int reader, const RecordingsListQuery& query) override
    {
        Call call{QStringLiteral("requestList"), -1, {}, query.offset, query.count, reader};
        call.listSlice = query.sliceId;
        call.listKind = query.kind;
        calls.append(call);
        const QList<RecordingEntry> matching = filterRecordings(entries, query.sliceId, query.kind);
        const qsizetype from = std::clamp<qsizetype>(query.offset, 0, matching.size());
        RecordingsListPage page;
        page.offset = static_cast<int>(from);
        page.total = static_cast<int>(matching.size());
        page.entries = matching.mid(from, std::max(0, query.count));
        m_pages.insert(reader, page);
        emit listChanged(reader);
    }
    RecordingsListPage listed(int reader) const override { return m_pages.value(reader); }
    void play(const QString& id) override { calls.append({QStringLiteral("play"), -1, id}); }
    void stopPlay() override { calls.append({QStringLiteral("stopPlay")}); }
    void seek(qint64 ms) override { calls.append({QStringLiteral("seek"), -1, QString::number(ms)}); }
    std::optional<PlaybackState> playback() const override { return m_playback; }
    void rename(const QString& id, const QString& name) override
    {
        calls.append({QStringLiteral("rename"), -1, id + QLatin1Char('=') + name});
        if (appliesChanges) {
            for (RecordingEntry& e : entries) {
                if (e.id == id) {
                    e.name = name;
                }
            }
            moveIndexRevision();
        }
    }
    void remove(const QStringList& ids) override
    {
        calls.append({QStringLiteral("remove"), -1, ids.join(QLatin1Char(','))});
        if (appliesChanges) {
            entries.removeIf([&ids](const RecordingEntry& e) { return ids.contains(e.id); });
            moveIndexRevision();
        }
    }
    /// The document's index revision moves by one, as the Core's does when
    /// its list changes.
    void moveIndexRevision()
    {
        QJsonObject index = m_document.value(QStringLiteral("index")).toObject();
        index.insert(QStringLiteral("revision"), index.value(QStringLiteral("revision")).toInteger() + 1);
        index.insert(QStringLiteral("count"), entries.size());
        m_document.insert(QStringLiteral("index"), index);
        emit documentChanged();
    }
    void download(const QString& id, const QString& toPath) override
    {
        calls.append({QStringLiteral("download"), -1, id + QLatin1Char('>') + toPath});
    }
    void setSettings(const QJsonObject& changed) override
    {
        calls.append({QStringLiteral("setSettings")});
        settingsSent.append(changed);
    }
    bool coreIsOnThisComputer() const override { return coreHere; }
    void sendVoice(int n) override { calls.append({QStringLiteral("sendVoice"), n}); }
    void stopVoice() override { calls.append({QStringLiteral("stopVoice")}); }
    void importVoice(int n, const QString& wavPath, const QString& name = QString()) override
    {
        calls.append({QStringLiteral("importVoice"), n, wavPath + QLatin1Char('=') + name});
    }
    void exportVoice(int n, const QString& toPath) override
    {
        calls.append({QStringLiteral("exportVoice"), n, toPath});
    }
    void renameVoice(int n, const QString& name) override
    {
        calls.append({QStringLiteral("renameVoice"), n, name});
    }
    void clearVoice(int n) override { calls.append({QStringLiteral("clearVoice"), n}); }
    void setVoiceRepeat(bool on, int delaySec) override
    {
        calls.append({QStringLiteral("setVoiceRepeat"), on ? 1 : 0, QString::number(delaySec)});
    }
    void listenVoice(int n) override { calls.append({QStringLiteral("listenVoice"), n}); }
    void rewind(int slice, qint64 utcNs, qint64 hz) override
    {
        Call call{QStringLiteral("rewind"), slice};
        call.utcNs = utcNs;
        call.hz = hz;
        calls.append(call);
    }
    void live(int slice) override { calls.append({QStringLiteral("live"), slice}); }
    void seekPast(int slice, qint64 utcNs) override
    {
        Call call{QStringLiteral("seekPast"), slice};
        call.utcNs = utcNs;
        calls.append(call);
    }
    void requestPastRows(int slice, const DsrPanView& view, qint64 fromUtcNs, qint64 toUtcNs,
                         int rowCount) override
    {
        Call call{QStringLiteral("requestPastRows"), slice, {}, 0, rowCount};
        call.fromUtcNs = fromUtcNs;
        call.utcNs = toUtcNs;
        call.view = view;
        calls.append(call);
    }
    void requestPastSpectrum(int slice, const DsrPanView& view, qint64 utcNs) override
    {
        Call call{QStringLiteral("requestPastSpectrum"), slice};
        call.utcNs = utcNs;
        call.view = view;
        calls.append(call);
    }
    void requestHours() override { calls.append({QStringLiteral("requestHours")}); }
    // Task 25: the stretch's ends in fromUtcNs and utcNs, whole pan as
    // sliceId 1 (sound only 0).
    void saveStretch(qint64 fromNs, qint64 toNs, bool wholePan) override
    {
        Call call{QStringLiteral("saveStretch"), wholePan ? 1 : 0};
        call.fromUtcNs = fromNs;
        call.utcNs = toNs;
        calls.append(call);
    }
    void saveSoundOnly(const QString& id) override { calls.append({QStringLiteral("saveSoundOnly"), -1, id}); }
    void openStretch(const QString& id) override { calls.append({QStringLiteral("openStretch"), -1, id}); }

private:
    State m_state = State::Ready;
    QJsonObject m_document;
    QHash<int, RecordingsListPage> m_pages;
    std::optional<PlaybackState> m_playback;
};

}  // namespace NereusSDR
