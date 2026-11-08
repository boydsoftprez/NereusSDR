// =================================================================
// PanClarityRegistry — NereusSDR-original GUI ownership, GPLv3.
// Modification history (NereusSDR):
// 2026-11-06 J.J. Boyd (KG4VCF), AI-assisted via Claude Code:
//   sourceForPanForTest(), issue 701.
// 2026-10-12 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   Requested Clarity is each pan's choice; setEnabled is a master broadcast.
// 2026-10-05 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-10 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//   Display pill plan Task 12, setHoldLengthProvider() and setClockForTest().
// =================================================================
#pragma once
#include <QObject>
#include <QHash>
#include <QSharedPointer>
#include <QString>
#include <functional>
#include <optional>
#include <QPointer>

namespace NereusSDR {
class RadioModel;
class SpectrumWidget;
class SpectrumOverlayPanel;
class PanadapterModel;
class SliceModel;
class FFTEngine;
class ClarityController;

class PanClarityRegistry final : public QObject {
    Q_OBJECT
public:
    struct SourceAssociation {
        int streamIndex = -1;
        quint64 streamEpoch = 0;
        double centreHz = 0;
        double sampleRateHz = 0;
        bool operator==(const SourceAssociation&) const = default;
    };
    struct RecipientToken {
        quint64 instance = 0;
        quint64 revision = 0;
        bool operator==(const RecipientToken&) const = default;
    };
    explicit PanClarityRegistry(RadioModel* model, QObject* parent = nullptr);
    ~PanClarityRegistry() override;
    void registerPan(const QString&, SpectrumWidget*, SpectrumOverlayPanel*, PanadapterModel* savedHintModel = nullptr);
    void retirePan(const QString&);
    void retireAll();
    ClarityController* controllerForPan(const QString&) const;
    struct ClarityStatus {
        bool requestedEnabled;
        bool active;
        bool paused;
        bool sourceAvailable;
        bool operator==(const ClarityStatus&) const = default;
    };
    /// Existing status flags only; no quantitative confidence is measured.
    std::optional<ClarityStatus> statusForPan(const QString&) const;
    void bindLocal(const QString&, SliceModel*, FFTEngine*, const SourceAssociation&);
    RecipientToken bindRemote(const QString&, SpectrumWidget*, SliceModel*, const SourceAssociation&, quint32 mediaEpoch, quint32 endpointId, quint32 contextGeneration);
    void feedRemoteFloor(const QString&, SpectrumWidget*, const RecipientToken&, float floorDbm, qint64 nowMs = -1);
    void setRemoteAvailable(const QString&, const RecipientToken&, bool);
    void invalidateRemoteSession();
    void invalidateAllSources();
    // An explicit master application, including at the same master value.
    // Registration and one pan's saved choice never broadcast through it.
    void setEnabled(bool);
    void setKeyed(bool);
    void retunePan(const QString&);
    // Read-only test seam: the stream, centre and rate this pan's Clarity is
    // bound to.
    std::optional<SourceAssociation> sourceForPanForTest(const QString&) const;
    // Read-only test seam; production reads the model's local MOX controller.
    void setLocalKeyedQueryForTest(std::function<bool()> query);
    // How long a pan holds its Clarity level after an un-key or a same-band
    // re-centre, in milliseconds. Asked per pan when a hold starts; with
    // none set, or an answer that is not positive, the hold is kMoxHoldMinMs
    // (core/spectrum/MoxDisplayHold.h).
    void setHoldLengthProvider(std::function<qint64(const QString& panId)> provider);
    // Replaces the wall clock in the hold, in the readings fed without a
    // time of their own, and in the per-band priming window.
    void setClockForTest(std::function<qint64()> nowMs);
signals:
    void panStatusChanged(const QString& panId);

private:
    struct Entry;
    QHash<QString, QSharedPointer<Entry>> m_entries;
    quint64 m_nextInstance = 0;
    QPointer<RadioModel> m_model;
    bool m_enabled = false;
    quint64 m_masterRevision = 0;
    bool m_keyed = false;
    std::function<bool()> m_localKeyedQueryForTest;
    std::function<qint64(const QString&)> m_holdLengthProvider;
    std::function<qint64()> m_clockForTest;
    enum class OperationAuthority { Recipient, OwnerControl };
    bool localKeyedNow() const;
    bool keyedNow() const;
    qint64 nowMs() const;
    qint64 holdLengthMs(const QString&) const;
    static float storedFloor(const QSharedPointer<Entry>&, int band);
    void unkeyed(const QString&, const QSharedPointer<Entry>&, ClarityController*);
    void dispatch(const QString&, const QSharedPointer<Entry>&, std::function<void(ClarityController*)>,
        OperationAuthority authority = OperationAuthority::Recipient, bool hint = false,
        std::optional<bool> keyedTransition = std::nullopt);
    bool current(const QString&, const QSharedPointer<Entry>&) const;
    bool canOutput(const QString&, const QSharedPointer<Entry>&) const;
    void refreshStatus(const QSharedPointer<Entry>&);
    void setPanEnabled(const QString&, const QSharedPointer<Entry>&, bool);
    void associate(const QString&, const QSharedPointer<Entry>&, SliceModel*, const SourceAssociation&);
    void bandChanged(const QString&, const QSharedPointer<Entry>&, int);
    void invalidate(const QSharedPointer<Entry>&);
};
}
