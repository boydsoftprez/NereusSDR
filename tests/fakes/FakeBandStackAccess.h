#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/fakes/FakeBandStackAccess.h  (NereusSDR)
// =================================================================
//
// A BandStackAccess for GUI tests: records every call and can show any state or
// document. The signals are public, so a test emits refused() and reported()
// itself. Add this header to the test's sources so it is moc'ed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QList>

#include "models/bandstack/BandStackAccess.h"

namespace NereusSDR {

class FakeBandStackAccess : public BandStackAccess {
    Q_OBJECT

public:
    struct Call {
        QString verb;
        int sliceId{-1};
        std::optional<Band> band;
        BandStackDirection direction{BandStackDirection::Next};
        QString entryId;
        BandStackView view;
        std::optional<QString> name;
        std::optional<bool> locked;
        QStringList ids;
        BandStackReturnMode mode{BandStackReturnMode::Current};
        std::optional<bool> dupes;
        std::optional<int> expiryMinutes;
        std::optional<int> dwellSeconds;
    };

    explicit FakeBandStackAccess(QObject* parent = nullptr) : BandStackAccess(parent) {}

    QList<Call> calls;

    void setDocument(const QJsonObject& document)
    {
        m_document = document;
        emit documentChanged();
    }

    void setState(State state, const QString& reason = {})
    {
        m_state = state;
        m_reason = reason;
        emit stateChanged();
    }

    State state() const override { return m_state; }
    QString unavailableReason() const override { return m_reason; }
    QJsonObject document() const override { return m_document; }

    void press(int sliceId, Band band, BandStackDirection d, const BandStackView& v) override
    {
        Call call;
        call.verb = QStringLiteral("press");
        call.sliceId = sliceId;
        call.band = band;
        call.direction = d;
        call.view = v;
        calls.append(call);
    }
    void step(int sliceId, BandStackDirection d, const BandStackView& v) override
    {
        Call call;
        call.verb = QStringLiteral("step");
        call.sliceId = sliceId;
        call.direction = d;
        call.view = v;
        calls.append(call);
    }
    void goTo(int sliceId, const QString& entryId, const BandStackView& v) override
    {
        Call call;
        call.verb = QStringLiteral("goTo");
        call.sliceId = sliceId;
        call.entryId = entryId;
        call.view = v;
        calls.append(call);
    }
    void add(int sliceId, const BandStackView& v) override
    {
        Call call;
        call.verb = QStringLiteral("add");
        call.sliceId = sliceId;
        call.view = v;
        calls.append(call);
    }
    void saveHere(int sliceId, const QString& entryId, const BandStackView& v) override
    {
        Call call;
        call.verb = QStringLiteral("saveHere");
        call.sliceId = sliceId;
        call.entryId = entryId;
        call.view = v;
        calls.append(call);
    }
    void edit(const QString& id, const std::optional<QString>& name,
              std::optional<bool> locked) override
    {
        Call call;
        call.verb = QStringLiteral("edit");
        call.entryId = id;
        call.name = name;
        call.locked = locked;
        calls.append(call);
    }
    void remove(const QString& id) override
    {
        Call call;
        call.verb = QStringLiteral("remove");
        call.entryId = id;
        calls.append(call);
    }
    void reorder(Band band, const QStringList& ids) override
    {
        Call call;
        call.verb = QStringLiteral("reorder");
        call.band = band;
        call.ids = ids;
        calls.append(call);
    }
    void setReturn(Band band, BandStackReturnMode mode, const QString& entryId) override
    {
        Call call;
        call.verb = QStringLiteral("setReturn");
        call.band = band;
        call.mode = mode;
        call.entryId = entryId;
        calls.append(call);
    }
    void clear(std::optional<Band> band) override
    {
        Call call;
        call.verb = QStringLiteral("clear");
        call.band = band;
        calls.append(call);
    }
    void reseed() override
    {
        Call call;
        call.verb = QStringLiteral("reseed");
        calls.append(call);
    }
    void options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                 std::optional<int> dwellSeconds) override
    {
        Call call;
        call.verb = QStringLiteral("options");
        call.dupes = dupes;
        call.expiryMinutes = expiryMinutes;
        call.dwellSeconds = dwellSeconds;
        calls.append(call);
    }
    void restoreExpired(const QString& batchId) override
    {
        Call call;
        call.verb = QStringLiteral("restoreExpired");
        call.entryId = batchId;
        calls.append(call);
    }

private:
    State m_state{State::Ready};
    QString m_reason;
    QJsonObject m_document;
};

} // namespace NereusSDR
