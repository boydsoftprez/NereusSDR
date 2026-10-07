// =================================================================
// PanClarityRegistry — NereusSDR-original GUI ownership, GPLv3.
// Modification history (NereusSDR):
// 2026-10-05 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
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
    void bindLocal(const QString&, SliceModel*, FFTEngine*, const SourceAssociation&);
    RecipientToken bindRemote(const QString&, SpectrumWidget*, SliceModel*, const SourceAssociation&, quint32 mediaEpoch, quint32 endpointId, quint32 contextGeneration);
    void feedRemoteFloor(const QString&, SpectrumWidget*, const RecipientToken&, float floorDbm, qint64 nowMs = -1);
    void setRemoteAvailable(const QString&, const RecipientToken&, bool);
    void invalidateRemoteSession();
    void invalidateAllSources();
    void setEnabled(bool);
    void setKeyed(bool);
    void retunePan(const QString&);
    // Read-only test seam; production reads the model's local MOX controller.
    void setLocalKeyedQueryForTest(std::function<bool()> query);
private:
    struct Entry;
    QHash<QString, QSharedPointer<Entry>> m_entries;
    quint64 m_nextInstance = 0;
    QPointer<RadioModel> m_model;
    bool m_enabled = false;
    bool m_keyed = false;
    std::function<bool()> m_localKeyedQueryForTest;
    enum class OperationAuthority { Recipient, OwnerControl };
    bool localKeyedNow() const;
    bool keyedNow() const;
    void dispatch(const QString&, const QSharedPointer<Entry>&, std::function<void(ClarityController*)>,
        OperationAuthority authority = OperationAuthority::Recipient, bool hint = false,
        std::optional<bool> keyedTransition = std::nullopt);
    bool current(const QString&, const QSharedPointer<Entry>&) const;
    bool canOutput(const QString&, const QSharedPointer<Entry>&) const;
    void refreshStatus(const QSharedPointer<Entry>&);
    void associate(const QString&, const QSharedPointer<Entry>&, SliceModel*, const SourceAssociation&);
    void bandChanged(const QString&, const QSharedPointer<Entry>&, int);
    void invalidate(const QSharedPointer<Entry>&);
};
}
