// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original local/remote station DSP asset contract.
#pragma once

#include "core/NereusCoreExport.h"
#include "DspAssetStore.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QVariantMap>

#include <array>
#include <functional>
#include <memory>

class QCryptographicHash;

namespace NereusSDR {

class AppSettings;

struct DspAssetServiceResult {
    bool accepted{false};
    QString reason;
    QVariantMap values;
};

class NEREUS_CORE_EXPORT DspAssetService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString nnrStandardAsset READ nnrStandardAsset NOTIFY selectionChanged)
    Q_PROPERTY(QString nnrPremiumAsset READ nnrPremiumAsset NOTIFY selectionChanged)
    Q_PROPERTY(bool nnrModelSelectionPending READ nnrModelSelectionPending NOTIFY selectionChanged)
    Q_PROPERTY(QString nnrModelStatus READ nnrModelStatus NOTIFY selectionChanged)
    Q_PROPERTY(quint32 selectionRevision READ selectionRevision NOTIFY selectionChanged)
    // R-R3-21 (dspAssetVersion 2): the one Core-wide NR3 model. The asset is
    // the chosen id (a bundled id or "sha256:..."); the status is plain text
    // about the model actually loaded.
    Q_PROPERTY(QString nr3ModelAsset READ nr3ModelAsset NOTIFY nr3SelectionChanged)
    Q_PROPERTY(QString nr3ModelStatus READ nr3ModelStatus NOTIFY nr3SelectionChanged)
    // Fix wave I3: false when no usable NR3 model file exists on the Core,
    // so NR3 cannot run (WDSP would pass audio through unchanged). A window
    // reads true until a Core says otherwise, so an older Core changes
    // nothing; nr3ModelStatus then carries the plain reason.
    Q_PROPERTY(bool nr3Runnable READ nr3Runnable NOTIFY nr3SelectionChanged)
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 3): the same pair for DFNR.
    // dfnrModelStatus is the plain reason DFNR cannot run (empty when it
    // can); dfnrRunnable is false when this build has no DFNR, the
    // DeepFilterNet model file is missing, or it failed to load at the
    // first selection. A window reads true until a Core says otherwise, so
    // an older Core changes nothing.
    Q_PROPERTY(QString dfnrModelStatus READ dfnrModelStatus NOTIFY dfnrAvailabilityChanged)
    Q_PROPERTY(bool dfnrRunnable READ dfnrRunnable NOTIFY dfnrAvailabilityChanged)
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 4): the same pair for MNR,
    // which runs only on a Mac. mnrStatus is the plain reason MNR cannot
    // run (empty when it can); mnrRunnable is false on a Core built without
    // MNR. A window reads true until a Core says otherwise, so an older
    // Core changes nothing.
    Q_PROPERTY(QString mnrStatus READ mnrStatus NOTIFY mnrAvailabilityChanged)
    Q_PROPERTY(bool mnrRunnable READ mnrRunnable NOTIFY mnrAvailabilityChanged)

public:
    using RemoteRequestHandler =
        std::function<quint32(const QByteArray&, const QVariantMap&)>;
    // Receives a model file path that has just passed the trial load. The
    // Core installs RNNRloadModel here; tests install a recorder.
    using Nr3ModelLoader = std::function<void(const QString&)>;

    // The two bundled rnnoise models, always selectable by these ids.
    static constexpr const char* kNr3BundledLargeId = "bundled:nr3-large";
    static constexpr const char* kNr3BundledSmallId = "bundled:nr3-small";
    static bool isBundledNr3Id(const QString& id);
    // Path of a bundled model file on this machine, or empty when the
    // install does not carry it.
    static QString bundledNr3ModelPath(const QString& id);
    // Test seam: where the bundled model files are, instead of the install.
    // An empty function restores the install lookup.
    static void setBundledNr3ModelPathsForTest(std::function<QString(const QString&)> resolver);

    explicit DspAssetService(AppSettings& settings, bool local,
                             QObject* parent = nullptr);
    ~DspAssetService() override;

    bool isLocal() const noexcept { return m_local; }
    DspAssetStore* store() const noexcept { return m_store.get(); }

    QString nnrStandardAsset() const { return m_selected[0]; }
    QString nnrPremiumAsset() const { return m_selected[1]; }
    std::array<QString, 2> desiredNnrModelAssets() const { return m_selected; }
    std::array<QString, 2> activeNnrModelAssets() const { return m_active; }
    bool nnrModelSelectionPending() const;
    QString nnrModelStatus() const { return m_status; }
    quint32 selectionRevision() const noexcept { return m_revision; }

    QString nr3ModelAsset() const { return m_nr3Selected; }
    QString nr3ModelStatus() const { return m_nr3Status; }
    // Local: the last resolve found a usable model file. Remote: mirrored.
    bool nr3Runnable() const { return m_nr3Runnable; }
    // Local: whether this Core can run DFNR, and why not. Remote: mirrored.
    bool dfnrRunnable() const { return m_dfnrRunnable; }
    QString dfnrModelStatus() const { return m_dfnrStatus; }
    // Local only (a remote window takes the Core's): what the Core found,
    // at start or when the model failed to load at the first selection.
    void setDfnrAvailability(bool runnable, const QString& status);
    // Local: whether this Core can run MNR, and why not. Remote: mirrored.
    bool mnrRunnable() const { return m_mnrRunnable; }
    QString mnrStatus() const { return m_mnrStatus; }
    // Local only (a remote window takes the Core's): set at start from
    // whether this build has MNR.
    void setMnrAvailability(bool runnable, const QString& status);
    // The id of the model last handed to the loader (local only).
    QString activeNr3ModelAsset() const { return m_nr3Active; }
    // Local: this build can load NR3 models. Remote: the Core advertised
    // dspAssetVersion 2 on a session that negotiated DSP control.
    bool nr3ModelsSupported() const;
    void setRemoteNr3ModelsSupported(bool supported);
    void setNr3ModelLoader(Nr3ModelLoader loader);
    // Resolves the chosen model to a file that passed the trial load,
    // falling back to the bundled large model and then the bundled small
    // one. Empty only when no usable model file exists at all; the loader
    // is never given an empty path.
    QString resolveNr3ModelPath(QString* resolvedId = nullptr, QString* reason = nullptr);
    // Resolves and hands the file to the loader. False (and no load) when
    // no usable model file exists.
    bool applyNr3Model(QString* reason = nullptr);

    void setRadioIdentity(const QString& mac);
    DspAssetServiceResult execute(const QByteArray& verb, const QVariantMap& args,
                                  const QString& owner);
    quint32 request(const QByteArray& verb, const QVariantMap& args);
    void setRemoteRequestHandler(RemoteRequestHandler handler);
    bool receiveRemoteResult(quint32 id, const QByteArray& verb, bool accepted,
                             const QString& reason, const QVariantMap& values);
    void resetSession();
    void cancelOwner(const QString& owner);

    std::array<QString, 2> resolveNnrModelPaths(QString* reason = nullptr);
    void markNnrModelsApplied();
    bool applyRemoteProperty(const QByteArray& name, const QVariant& value);

signals:
    void requestCompleted(quint32 id, bool accepted, QString reason, QVariantMap values);
    void selectionChanged();
    void nr3SelectionChanged();
    void dfnrAvailabilityChanged();
    void mnrAvailabilityChanged();
    void configurationChanged();

private:
    struct ActiveImport;
    struct PendingRequest {
        QByteArray verb;
    };

    quint32 allocateRequestId();
    void refreshSelectionStatus();
    bool setSelection(int slot, const QString& id, QString* reason);
    bool setNr3Selection(const QString& id, QString* reason);
    void importLegacyNr3ModelPath();
    void setNr3Status(const QString& status);
    void setNr3Runnable(bool runnable);
    DspAssetServiceResult reject(const QString& reason) const;
    // R-IOS-01: a store or validation message is sent as it is only when
    // it is written for the operator (DspAssetValidation::
    // isOperatorMessage); otherwise it goes to the log and `plain` is sent.
    DspAssetServiceResult rejectDetail(const QString& detail, const QString& plain) const;

    AppSettings& m_settings;
    bool m_local{false};
    std::unique_ptr<DspAssetStore> m_store;
    RemoteRequestHandler m_remoteRequest;
    QHash<QString, std::shared_ptr<ActiveImport>> m_imports;
    QHash<quint32, PendingRequest> m_pendingRequests;
    quint32 m_nextRequestId{1};
    std::array<QString, 2> m_selected;
    std::array<QString, 2> m_active;
    std::array<QString, 2> m_lastResolved;
    quint32 m_revision{1};
    QString m_status;
    QString m_radioIdentity;
    bool m_remotePending{false};
    QString m_nr3Selected;
    QString m_nr3Active;
    QString m_nr3Status;
    bool m_nr3Runnable{true};
    bool m_dfnrRunnable{true};
    QString m_dfnrStatus;
    bool m_mnrRunnable{true};
    QString m_mnrStatus;
    bool m_remoteNr3Supported{false};
    Nr3ModelLoader m_nr3Loader;
};

} // namespace NereusSDR
