#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/bandstack/BandStackAccess.h  (NereusSDR)
// =================================================================
//
// What the GUI talks to for the band stack, so a panel does not care whether
// the Core is in this process. RadioModel::bandStack() returns the right one:
// LocalBandStackAccess on the Core and RemoteBandStackAccess in a remote window.
// Design: docs/architecture/2026-11-05-band-stack-design.md, sections 6.1, 6.4.
//
// Every refusal comes through refused() exactly once. A press held for a question
// is not a refusal; the question arrives the way any other held change's does.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

#include "core/NereusCoreExport.h"
#include "models/Band.h"
#include "models/bandstack/BandStackController.h"
#include "models/bandstack/BandStackStore.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT BandStackAccess : public QObject {
    Q_OBJECT

public:
    enum class State { Ready, NeedsNewerCore, NotConnected };

    explicit BandStackAccess(QObject* parent = nullptr) : QObject(parent) {}

    virtual State state() const = 0;
    /// Why the stack cannot be used now. Empty when Ready.
    virtual QString unavailableReason() const = 0;
    /// The document of design section 6.2. Empty until the Core knows a radio.
    virtual QJsonObject document() const = 0;

    virtual void press(int sliceId, Band band, BandStackDirection d, const BandStackView& v) = 0;
    virtual void step(int sliceId, BandStackDirection d, const BandStackView& v) = 0;
    virtual void goTo(int sliceId, const QString& entryId, const BandStackView& v) = 0;
    virtual void add(int sliceId, const BandStackView& v) = 0;
    virtual void saveHere(int sliceId, const QString& entryId, const BandStackView& v) = 0;
    virtual void edit(const QString& id, const std::optional<QString>& name,
                      std::optional<bool> locked) = 0;
    virtual void remove(const QString& id) = 0;
    virtual void reorder(Band band, const QStringList& ids) = 0;
    virtual void setReturn(Band band, BandStackReturnMode mode, const QString& entryId) = 0;
    virtual void clear(std::optional<Band> band) = 0;
    virtual void reseed() = 0;
    virtual void options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                         std::optional<int> dwellSeconds) = 0;
    virtual void restoreExpired(const QString& batchId) = 0;

    /// The words state() stands for.
    static QString needsNewerCoreReason()
    {
        return QStringLiteral("This Core does not have band stacks yet. Updating the Core adds them.");
    }
    static QString notConnectedReason()
    {
        return QStringLiteral("Connect to a radio to use band stacks.");
    }

signals:
    void documentChanged();
    void stateChanged();
    void refused(const QString& reason);
    /// A recall finished on a slice this device controls: "40m: 7.185 LSB, 5 of 6".
    void reported(const QString& text);
};

} // namespace NereusSDR
