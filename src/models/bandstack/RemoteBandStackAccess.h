#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/RemoteBandStackAccess.h  (NereusSDR)
// =================================================================
//
// The band stack in a remote window: the document comes from the mirror and
// each call goes out as a bandStack.* verb through the window's station link. A
// press held for a question is not a refusal; it takes the window's existing
// question path.
//
// A remote window has no controller, so reported() is built from the document
// when lastRecall.seq moves for a slice this window controls.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QJsonObject>
#include <QPointer>
#include <QtGlobal>

#include <optional>

#include "core/NereusCoreExport.h"
#include "models/bandstack/BandStackAccess.h"

namespace NereusSDR {

class RadioModel;
struct BandStackCommand;

class NEREUS_CORE_EXPORT RemoteBandStackAccess final : public BandStackAccess {
    Q_OBJECT

public:
    explicit RemoteBandStackAccess(RadioModel* model);

    State state() const override;
    QString unavailableReason() const override;
    QJsonObject document() const override { return m_document; }

    void press(int sliceId, Band band, BandStackDirection d, const BandStackView& v) override;
    void step(int sliceId, BandStackDirection d, const BandStackView& v) override;
    void goTo(int sliceId, const QString& entryId, const BandStackView& v) override;
    void add(int sliceId, const BandStackView& v) override;
    void saveHere(int sliceId, const QString& entryId, const BandStackView& v) override;
    void edit(const QString& id, const std::optional<QString>& name,
              std::optional<bool> locked) override;
    void remove(const QString& id) override;
    void reorder(Band band, const QStringList& ids) override;
    void setReturn(Band band, BandStackReturnMode mode, const QString& entryId) override;
    void clear(std::optional<Band> band) override;
    void reseed() override;
    void options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                 std::optional<int> dwellSeconds) override;
    void restoreExpired(const QString& batchId) override;

private:
    void send(const BandStackCommand& command);
    void onFacadeChanged();
    void reportRecall(const QJsonObject& before);
    QString recallText(const QJsonObject& before, int sliceId) const;
    void refreshState();

    QPointer<RadioModel> m_model;
    QString m_json;
    QJsonObject m_document;
    // The first document of a session reports nothing: that recall is old.
    std::optional<qint64> m_seenRecall;
    State m_lastState{State::NotConnected};
};

} // namespace NereusSDR
