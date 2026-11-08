#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/LocalBandStackAccess.h  (NereusSDR)
// =================================================================
//
// The band stack in the window on the Core. With no invoker each call goes
// straight to BandStackController. While the desktop hosts, an invoker takes the
// call to the server as the station device, so a press that would move another
// device's slice asks first (design V15). The models cannot include the GUI, so
// the GUI hands the invoker in.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QJsonObject>
#include <QPointer>

#include <functional>

#include "core/NereusCoreExport.h"
#include "core/session/SessionMessages.h"
#include "models/bandstack/BandStackAccess.h"

namespace NereusSDR {

class RadioModel;
struct BandStackCommand;

class NEREUS_CORE_EXPORT LocalBandStackAccess final : public BandStackAccess {
    Q_OBJECT

public:
    explicit LocalBandStackAccess(RadioModel* model);

    /// Where calls go while hosting; empty sends them to the controller directly.
    void setInvoker(std::function<void(const SessionMessage&)> invoker);
    /// The server refused what the invoker carried.
    void reportInvokerRefusal(const QString& reason);

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
    /// The controller when the call is to run here; null when it was refused
    /// or went to the invoker.
    BandStackController* begin(const BandStackCommand& command);
    void finish(bool accepted, const QString& refusal);
    void onFacadeChanged();
    void onRecallReported(int sliceId, const QString& text);
    void refreshState();

    QPointer<RadioModel> m_model;
    std::function<void(const SessionMessage&)> m_invoker;
    QPointer<BandStackController> m_followed;
    QString m_json;
    QJsonObject m_document;
    State m_lastState{State::NotConnected};
};

} // namespace NereusSDR
